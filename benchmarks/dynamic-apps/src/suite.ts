import { pathToFileURL } from "node:url";
import { type LoadResult, readLoadConfig, runLoadTest } from "./load.js";

export interface BenchmarkSuite {
	baseUrl: string;
	initialization: Record<string, LoadResult>;
	warmup: Record<string, { attempts: number; warm: boolean }>;
	cases: Record<string, LoadResult>;
	diagnostics: { before: unknown; after: unknown };
}

interface CaseDefinition {
	name: string;
	path: string;
	concurrency: number;
	requests: number;
	timeoutMs?: number;
}

export async function runBenchmarkSuite(
	baseUrl: string,
): Promise<BenchmarkSuite> {
	const normalized = baseUrl.replace(/\/$/, "");
	const requestedCases = new Set(
		(process.env.BENCH_CASES ?? "")
			.split(",")
			.map((value) => value.trim())
			.filter(Boolean),
	);
	const definitions = suiteDefinitions(
		process.env.BENCH_PROFILE ?? "full",
	).filter(
		(definition) =>
			requestedCases.size === 0 || requestedCases.has(definition.name),
	);
	if (definitions.length === 0)
		throw new Error("BENCH_CASES selected no cases");
	if (
		definitions.some((definition) =>
			definition.path.startsWith("/bench/actor-app/"),
		)
	) {
		const setup = await fetch(`${normalized}/bench/actor-app/setup`, {
			method: "POST",
			signal: AbortSignal.timeout(15 * 60_000),
		});
		if (!setup.ok) {
			throw new Error(
				`actor application setup failed with HTTP ${setup.status}: ${await setup.text()}`,
			);
		}
	}
	const before = await readDiagnostics(normalized);
	const warmup: Record<string, { attempts: number; warm: boolean }> = {};
	const initialization: Record<string, LoadResult> = {};
	for (const architecture of ["pooled", "ephemeral"] as const) {
		if (
			definitions.some((definition) =>
				definition.path.startsWith(`/bench/${architecture}`),
			)
		) {
			const defaults = readLoadConfig({});
			initialization[architecture] = await runLoadTest({
				...defaults,
				target: `${normalized}/bench/${architecture}`,
				concurrency: 1,
				durationSeconds: 3_600,
				timeoutMs: 60_000,
				maxRequests: 1,
				maxSamples: 1,
				minSuccessRate: 1,
			});
			warmup[architecture] = await warmActor(
				`${normalized}/bench/${architecture}`,
			);
		}
	}
	const cases: Record<string, LoadResult> = {};

	for (const definition of definitions) {
		console.error(
			`benchmark ${definition.name}: c=${definition.concurrency} n=${definition.requests}`,
		);
		const defaults = readLoadConfig({});
		const result = await runLoadTest({
			...defaults,
			target: `${normalized}${definition.path}`,
			concurrency: definition.concurrency,
			durationSeconds: 3_600,
			timeoutMs: definition.timeoutMs ?? 60_000,
			maxRequests: definition.requests,
			maxSamples: definition.requests,
			maxReplicaSeries: 512,
			minSuccessRate: 1,
		});
		cases[definition.name] = result;
		console.error(
			`benchmark ${definition.name}: success=${result.successRate} p50=${result.latencyMs.p50}ms`,
		);
	}

	return {
		baseUrl: normalized,
		initialization,
		warmup,
		cases,
		diagnostics: { before, after: await readDiagnostics(normalized) },
	};
}

async function readDiagnostics(baseUrl: string): Promise<unknown> {
	const response = await fetch(`${baseUrl}/bench/info`, {
		signal: AbortSignal.timeout(10_000),
	});
	if (!response.ok)
		throw new Error(
			`benchmark diagnostics failed with HTTP ${response.status}`,
		);
	return response.json();
}

function suiteDefinitions(profile: string): CaseDefinition[] {
	const sequentialRequests = integerEnv(
		"BENCH_SEQUENTIAL_REQUESTS",
		200,
		1,
		100_000,
	);
	const pooledRequests = integerEnv("BENCH_POOLED_REQUESTS", 80, 1, 100_000);
	const pooledConcurrentRequests = integerEnv(
		"BENCH_POOLED_CONCURRENT_REQUESTS",
		32,
		1,
		100_000,
	);
	const ephemeralRequests = integerEnv(
		"BENCH_EPHEMERAL_REQUESTS",
		80,
		1,
		100_000,
	);
	if (profile === "routing") {
		return actorRoutingDefinitions(sequentialRequests);
	}
	if (profile === "actors") return actorApplicationDefinitions();
	if (profile === "smoke") {
		return [
			{
				name: "noopSequential",
				path: "/bench/noop",
				concurrency: 1,
				requests: 4,
			},
			{
				name: "pooledSequential",
				path: "/bench/pooled",
				concurrency: 1,
				requests: 4,
			},
			{
				name: "ephemeralSequential",
				path: "/bench/ephemeral",
				concurrency: 1,
				requests: 4,
			},
		];
	}
	if (profile === "stability") {
		return [
			{
				name: "pooledStability",
				path: "/bench/pooled",
				concurrency: integerEnv("BENCH_STABILITY_CONCURRENCY", 8, 1, 1_000),
				requests: integerEnv("BENCH_STABILITY_REQUESTS", 10_000, 1, 10_000_000),
				timeoutMs: 60_000,
			},
		];
	}
	if (profile !== "full") throw new Error(`unknown BENCH_PROFILE ${profile}`);
	return [
		...actorRoutingDefinitions(sequentialRequests),
		...actorApplicationDefinitions(),
		{
			name: "noopSequential",
			path: "/bench/noop",
			concurrency: 1,
			requests: sequentialRequests,
		},
		{
			name: "noopConcurrent",
			path: "/bench/noop",
			concurrency: 8,
			requests: 800,
		},
		{
			name: "pooledSequential",
			path: "/bench/pooled",
			concurrency: 1,
			requests: pooledRequests,
		},
		{
			name: "pooledConcurrent",
			path: "/bench/pooled",
			concurrency: 8,
			requests: pooledConcurrentRequests,
			timeoutMs: 10_000,
		},
		{
			name: "ephemeralSequential",
			path: "/bench/ephemeral",
			concurrency: 1,
			requests: ephemeralRequests,
		},
		{
			name: "ephemeralConcurrent",
			path: "/bench/ephemeral",
			concurrency: 8,
			requests: Math.min(ephemeralRequests, 64),
		},
		{
			name: "pooledConcurrent32",
			path: "/bench/pooled",
			concurrency: 32,
			requests: 128,
			timeoutMs: 60_000,
		},
	];
}

function actorApplicationDefinitions(): CaseDefinition[] {
	return [
		{
			name: "actor-app-sequential",
			path: "/bench/actor-app/action",
			concurrency: 1,
			requests: 100,
		},
		{
			name: "actor-app-concurrent",
			path: "/bench/actor-app/action",
			concurrency: 16,
			requests: 256,
			timeoutMs: 60_000,
		},
	];
}

function integerEnv(
	name: string,
	fallback: number,
	minimum: number,
	maximum: number,
): number {
	const value = Number(process.env[name] ?? fallback);
	if (!Number.isInteger(value) || value < minimum || value > maximum) {
		throw new Error(`${name} must be an integer from ${minimum} to ${maximum}`);
	}
	return value;
}

function actorRoutingDefinitions(requests: number): CaseDefinition[] {
	return ["resolve", "action"].map((variant) => ({
		name: `actor-${variant}`,
		path: `/bench/actor/${variant}`,
		concurrency: 1,
		requests,
	}));
}

async function warmActor(
	target: string,
): Promise<{ attempts: number; warm: boolean }> {
	for (let attempts = 1; attempts <= 50; attempts += 1) {
		const response = await fetch(target, {
			signal: AbortSignal.timeout(60_000),
		});
		await response.arrayBuffer();
		if (!response.ok) {
			throw new Error(`warmup failed with HTTP ${response.status}`);
		}
		if (response.headers.get("x-agentos-app-cold-start") === "0") {
			// Exercise connection reuse and exclude initialization transients.
			for (let index = 0; index < 9; index += 1) {
				const extra = await fetch(target, {
					signal: AbortSignal.timeout(60_000),
				});
				await extra.arrayBuffer();
				if (!extra.ok)
					throw new Error(`warmup failed with HTTP ${extra.status}`);
			}
			await new Promise((resolve) => setTimeout(resolve, 500));
			return { attempts: attempts + 9, warm: true };
		}
		await new Promise((resolve) => setTimeout(resolve, 100));
	}
	return { attempts: 50, warm: false };
}

async function main(): Promise<void> {
	const baseUrl = process.env.BENCH_BASE_URL;
	if (!baseUrl) throw new Error("BENCH_BASE_URL is required");
	const suite = await runBenchmarkSuite(baseUrl);
	console.log(JSON.stringify(suite, null, 2));
	if (Object.values(suite.cases).some((result) => result.successRate < 1)) {
		console.error("one or more benchmark cases had unsuccessful requests");
		process.exitCode = 1;
	}
}

const entrypoint = process.argv[1];
if (entrypoint && import.meta.url === pathToFileURL(entrypoint).href) {
	await main();
}
