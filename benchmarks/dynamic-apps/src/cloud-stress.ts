import { pathToFileURL } from "node:url";
import {
	type LoadConfig,
	type LoadResult,
	readLoadConfig,
	runLoadTest,
} from "./load.js";

type StressMode = "ramp" | "soak" | "both";

interface StressCase {
	name: "warm" | "snapshot" | "actor";
	path: string;
	concurrency: number;
	echoRequestId: boolean;
}

interface StageResult {
	name: string;
	concurrency: number;
	startedAt: string;
	finishedAt: string;
	passed: boolean;
	cases: Record<string, LoadResult>;
}

interface RampResult {
	name: string;
	stages: StageResult[];
	firstFailingConcurrency?: number;
	lastPassingConcurrency?: number;
	peakRequestsPerSecond: number;
	observedInstances: string[];
}

interface CloudStressResult {
	baseUrl: string;
	mode: StressMode;
	startedAt: string;
	finishedAt?: string;
	setup: unknown;
	ramps: RampResult[];
	soak?: {
		requestedDurationSeconds: number;
		completedDurationSeconds: number;
		concurrency: number;
		windows: StageResult[];
		observedInstances: string[];
		completed: number;
		minimumSuccessRate: number;
	};
	diagnostics: { before: unknown; after?: unknown };
}

interface StressConfig {
	baseUrl: string;
	mode: StressMode;
	rampConcurrencies: number[];
	rampStageSeconds: number;
	soakDurationSeconds: number;
	soakWindowSeconds: number;
	soakConcurrency?: number;
	timeoutMs: number;
	minimumSuccessRate: number;
	maxSamples: number;
}

const DEFAULT_RAMP_CONCURRENCIES = [1, 4, 16, 32, 64, 128, 256, 512, 1_000];

export async function runCloudStress(
	config: StressConfig,
): Promise<CloudStressResult> {
	const baseUrl = config.baseUrl.replace(/\/$/u, "");
	const result: CloudStressResult = {
		baseUrl,
		mode: config.mode,
		startedAt: new Date().toISOString(),
		setup: await setup(baseUrl),
		ramps: [],
		diagnostics: { before: await readJson(baseUrl, "/bench/info") },
	};
	progress("cloud-stress-ready", {
		baseUrl,
		mode: config.mode,
		rampConcurrencies: config.rampConcurrencies,
	});

	try {
		if (config.mode === "ramp" || config.mode === "both") {
			result.ramps.push(
				await runRamp(config, "warm", (concurrency) => [
					{
						name: "warm",
						path: "/bench/warm",
						concurrency,
						echoRequestId: true,
					},
				]),
			);
			result.ramps.push(await runRamp(config, "mixed", mixedCases));
		}

		if (config.mode === "soak" || config.mode === "both") {
			const sustainable = lastPassingConcurrency(result.ramps);
			const soakConcurrency =
				config.soakConcurrency ?? Math.max(1, Math.floor(sustainable * 0.7));
			result.soak = await runSoak(config, soakConcurrency);
		}
	} finally {
		result.diagnostics.after = await readJson(baseUrl, "/bench/info").catch(
			(error: unknown) => ({ error: errorMessage(error) }),
		);
		result.finishedAt = new Date().toISOString();
	}

	return result;
}

async function runRamp(
	config: StressConfig,
	name: string,
	cases: (concurrency: number) => StressCase[],
): Promise<RampResult> {
	const result: RampResult = {
		name,
		stages: [],
		peakRequestsPerSecond: 0,
		observedInstances: [],
	};
	const instances = new Set<string>();
	for (const concurrency of config.rampConcurrencies) {
		const stage = await runStage(
			config,
			`${name}-ramp-c${concurrency}`,
			concurrency,
			config.rampStageSeconds,
			cases(concurrency),
		);
		result.stages.push(stage);
		observeStage(stage, instances);
		result.peakRequestsPerSecond = Math.max(
			result.peakRequestsPerSecond,
			totalRequestsPerSecond(stage),
		);
		if (!stage.passed) {
			result.firstFailingConcurrency = concurrency;
			break;
		}
		result.lastPassingConcurrency = concurrency;
	}
	result.observedInstances = [...instances].sort();
	if (result.lastPassingConcurrency === undefined) {
		throw new Error(`${name} ramp failed at its first stage`);
	}
	return result;
}

async function runSoak(
	config: StressConfig,
	concurrency: number,
): Promise<NonNullable<CloudStressResult["soak"]>> {
	const startedAt = Date.now();
	const deadline = startedAt + config.soakDurationSeconds * 1_000;
	const windows: StageResult[] = [];
	const instances = new Set<string>();
	let completed = 0;
	let minimumSuccessRate = 1;
	for (let window = 1; Date.now() < deadline; window += 1) {
		const remainingSeconds = Math.max(
			1,
			Math.ceil((deadline - Date.now()) / 1_000),
		);
		const durationSeconds = Math.min(
			config.soakWindowSeconds,
			remainingSeconds,
		);
		const stage = await runStage(
			config,
			`soak-${window}`,
			concurrency,
			durationSeconds,
			mixedCases(concurrency),
		);
		windows.push(stage);
		observeStage(stage, instances);
		for (const value of Object.values(stage.cases)) {
			completed += value.completed;
			minimumSuccessRate = Math.min(minimumSuccessRate, value.successRate);
		}
		if (!stage.passed) {
			throw new Error(`soak failed in window ${window}; aborting early`);
		}
	}
	return {
		requestedDurationSeconds: config.soakDurationSeconds,
		completedDurationSeconds: Math.round((Date.now() - startedAt) / 1_000),
		concurrency,
		windows,
		observedInstances: [...instances].sort(),
		completed,
		minimumSuccessRate,
	};
}

async function runStage(
	config: StressConfig,
	name: string,
	concurrency: number,
	durationSeconds: number,
	cases: StressCase[],
): Promise<StageResult> {
	const startedAt = new Date().toISOString();
	progress("stage-start", { name, concurrency, durationSeconds, cases });
	const entries = await Promise.all(
		cases.map(async (definition) => {
			const defaults = readLoadConfig({});
			const loadConfig: LoadConfig = {
				...defaults,
				target: `${config.baseUrl.replace(/\/$/u, "")}${definition.path}`,
				concurrency: definition.concurrency,
				durationSeconds,
				timeoutMs: config.timeoutMs,
				maxRequests: 10_000_000,
				maxSamples: config.maxSamples,
				maxResponseBytes: 16 * 1_024,
				maxReplicaSeries: 5_000,
				validateJsonOk: true,
				echoRequestId: definition.echoRequestId,
			};
			return [definition.name, await runLoadTest(loadConfig)] as const;
		}),
	);
	const casesByName = Object.fromEntries(entries);
	const passed = Object.values(casesByName).every(
		(value) => value.successRate >= config.minimumSuccessRate,
	);
	const stage: StageResult = {
		name,
		concurrency,
		startedAt,
		finishedAt: new Date().toISOString(),
		passed,
		cases: casesByName,
	};
	progress("stage-finish", summarizeStage(stage));
	return stage;
}

function mixedCases(concurrency: number): StressCase[] {
	const warm = Math.max(1, Math.floor(concurrency * 0.8));
	const snapshot = Math.max(1, Math.floor(concurrency * 0.1));
	const actor = Math.max(1, concurrency - warm - snapshot);
	return [
		{
			name: "warm",
			path: "/bench/warm",
			concurrency: warm,
			echoRequestId: true,
		},
		{
			name: "snapshot",
			path: "/bench/snapshot",
			concurrency: snapshot,
			echoRequestId: true,
		},
		{
			name: "actor",
			path: "/bench/actor-app/action",
			concurrency: actor,
			echoRequestId: false,
		},
	];
}

function lastPassingConcurrency(ramps: RampResult[]): number {
	const mixed = ramps.find((ramp) => ramp.name === "mixed");
	if (mixed?.lastPassingConcurrency !== undefined) {
		return mixed.lastPassingConcurrency;
	}
	const passing = ramps.flatMap((ramp) =>
		ramp.lastPassingConcurrency === undefined
			? []
			: [ramp.lastPassingConcurrency],
	);
	return passing.length > 0 ? Math.min(...passing) : 1;
}

function observeStage(stage: StageResult, instances: Set<string>): void {
	for (const result of Object.values(stage.cases)) {
		for (const instance of Object.keys(result.benchmarkInstances)) {
			instances.add(instance);
		}
	}
}

function totalRequestsPerSecond(stage: StageResult): number {
	return Object.values(stage.cases).reduce(
		(total, result) => total + result.requestsPerSecond,
		0,
	);
}

function summarizeStage(stage: StageResult): unknown {
	return {
		name: stage.name,
		concurrency: stage.concurrency,
		passed: stage.passed,
		totalRequestsPerSecond: totalRequestsPerSecond(stage),
		cases: Object.fromEntries(
			Object.entries(stage.cases).map(([name, result]) => [
				name,
				{
					completed: result.completed,
					requestsPerSecond: result.requestsPerSecond,
					successRate: result.successRate,
					latencyMs: result.latencyMs,
					statuses: result.statuses,
					instances: Object.keys(result.benchmarkInstances).length,
				},
			]),
		),
	};
}

async function setup(baseUrl: string): Promise<unknown> {
	await postJson(baseUrl, "/bench/setup", 15 * 60_000);
	await postJson(baseUrl, "/bench/actor-app/setup", 15 * 60_000);
	const verification = await postJson(
		baseUrl,
		"/bench/actor-app/verify",
		15 * 60_000,
	);
	for (let index = 0; index < 10; index += 1) {
		const response = await fetch(
			`${baseUrl}/bench/warm?requestId=warmup-${index}`,
			{
				signal: AbortSignal.timeout(60_000),
			},
		);
		const body = (await response.json()) as {
			ok?: unknown;
			requestId?: unknown;
		};
		if (
			!response.ok ||
			body.ok !== true ||
			body.requestId !== `warmup-${index}`
		) {
			throw new Error(
				`direct warmup ${index} failed with HTTP ${response.status}`,
			);
		}
	}
	return verification;
}

async function postJson(
	baseUrl: string,
	path: string,
	timeoutMs: number,
): Promise<unknown> {
	const response = await fetch(`${baseUrl}${path}`, {
		method: "POST",
		signal: AbortSignal.timeout(timeoutMs),
	});
	const body = await response.text();
	if (!response.ok) {
		throw new Error(
			`${path} failed with HTTP ${response.status}: ${body.slice(0, 512)}`,
		);
	}
	return body ? JSON.parse(body) : null;
}

async function readJson(baseUrl: string, path: string): Promise<unknown> {
	const response = await fetch(`${baseUrl}${path}`, {
		signal: AbortSignal.timeout(60_000),
	});
	if (!response.ok)
		throw new Error(`${path} failed with HTTP ${response.status}`);
	return response.json();
}

function readStressConfig(env: NodeJS.ProcessEnv = process.env): StressConfig {
	const baseUrl = env.BENCH_BASE_URL;
	if (!baseUrl) throw new Error("BENCH_BASE_URL is required");
	const mode = env.CLOUD_STRESS_MODE ?? "both";
	if (mode !== "ramp" && mode !== "soak" && mode !== "both") {
		throw new Error("CLOUD_STRESS_MODE must be ramp, soak, or both");
	}
	return {
		baseUrl,
		mode,
		rampConcurrencies: integerListEnv(
			env.CLOUD_STRESS_RAMP_CONCURRENCIES,
			DEFAULT_RAMP_CONCURRENCIES,
		),
		rampStageSeconds: integerEnv(
			env,
			"CLOUD_STRESS_RAMP_STAGE_SECONDS",
			60,
			1,
			3_600,
		),
		soakDurationSeconds: integerEnv(
			env,
			"CLOUD_STRESS_SOAK_DURATION_SECONDS",
			3_600,
			1,
			86_400,
		),
		soakWindowSeconds: integerEnv(
			env,
			"CLOUD_STRESS_SOAK_WINDOW_SECONDS",
			60,
			1,
			600,
		),
		soakConcurrency: optionalIntegerEnv(
			env,
			"CLOUD_STRESS_SOAK_CONCURRENCY",
			1,
			10_000,
		),
		timeoutMs: integerEnv(env, "CLOUD_STRESS_TIMEOUT_MS", 60_000, 1, 300_000),
		minimumSuccessRate: numberEnv(
			env,
			"CLOUD_STRESS_MIN_SUCCESS_RATE",
			0.999,
			0,
			1,
		),
		maxSamples: integerEnv(
			env,
			"CLOUD_STRESS_MAX_SAMPLES",
			250_000,
			1,
			1_000_000,
		),
	};
}

function integerListEnv(
	value: string | undefined,
	fallback: number[],
): number[] {
	if (value === undefined) return fallback;
	const parsed = value.split(",").map((entry) => Number(entry.trim()));
	if (
		parsed.length === 0 ||
		parsed.some(
			(entry) => !Number.isInteger(entry) || entry < 1 || entry > 10_000,
		)
	) {
		throw new Error(
			"CLOUD_STRESS_RAMP_CONCURRENCIES must contain integers from 1 to 10000",
		);
	}
	return parsed;
}

function integerEnv(
	env: NodeJS.ProcessEnv,
	name: string,
	fallback: number,
	minimum: number,
	maximum: number,
): number {
	const value = Number(env[name] ?? fallback);
	if (!Number.isInteger(value) || value < minimum || value > maximum) {
		throw new Error(`${name} must be an integer from ${minimum} to ${maximum}`);
	}
	return value;
}

function optionalIntegerEnv(
	env: NodeJS.ProcessEnv,
	name: string,
	minimum: number,
	maximum: number,
): number | undefined {
	if (env[name] === undefined) return undefined;
	return integerEnv(env, name, minimum, minimum, maximum);
}

function numberEnv(
	env: NodeJS.ProcessEnv,
	name: string,
	fallback: number,
	minimum: number,
	maximum: number,
): number {
	const value = Number(env[name] ?? fallback);
	if (!Number.isFinite(value) || value < minimum || value > maximum) {
		throw new Error(`${name} must be a number from ${minimum} to ${maximum}`);
	}
	return value;
}

function progress(event: string, details: unknown): void {
	console.error(
		JSON.stringify({ event, at: new Date().toISOString(), details }),
	);
}

function errorMessage(error: unknown): string {
	return error instanceof Error ? error.message : String(error);
}

async function main(): Promise<void> {
	const config = readStressConfig();
	let result: CloudStressResult | undefined;
	try {
		result = await runCloudStress(config);
		console.log(JSON.stringify(result, null, 2));
	} catch (error) {
		progress("cloud-stress-failed", { message: errorMessage(error), result });
		throw error;
	}
}

const entrypoint = process.argv[1];
if (entrypoint && import.meta.url === pathToFileURL(entrypoint).href) {
	await main();
}
