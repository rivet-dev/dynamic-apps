import { pathToFileURL } from "node:url";
import { type LoadResult, readLoadConfig, runLoadTest } from "./load.js";

export interface BenchmarkSuite {
	baseUrl: string;
	warmup: { attempts: number; warm: boolean };
	cases: Record<string, LoadResult>;
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
	const needsAppWarmup = definitions.some(
		(definition) =>
			definition.path.startsWith("/bench/warm") ||
			definition.path.startsWith("/bench/cold"),
	);
	const warmup = needsAppWarmup
		? await warmActor(`${normalized}/bench/warm`)
		: { attempts: 0, warm: false };
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

	return { baseUrl: normalized, warmup, cases };
}

function suiteDefinitions(profile: string): CaseDefinition[] {
	const sequentialRequests = integerEnv(
		"BENCH_SEQUENTIAL_REQUESTS",
		200,
		1,
		100_000,
	);
	const warmRequests = integerEnv("BENCH_WARM_REQUESTS", 80, 1, 100_000);
	const warmConcurrentRequests = integerEnv(
		"BENCH_WARM_CONCURRENT_REQUESTS",
		32,
		1,
		100_000,
	);
	const coldRequests = integerEnv("BENCH_COLD_REQUESTS", 30, 1, 100_000);
	if (profile === "routing") {
		return actorRoutingDefinitions(sequentialRequests);
	}
	if (profile === "smoke") {
		return [
			{
				name: "noopSequential",
				path: "/bench/noop",
				concurrency: 1,
				requests: 4,
			},
			{
				name: "warmSequential",
				path: "/bench/warm",
				concurrency: 1,
				requests: 4,
			},
			{
				name: "coldSequential",
				path: "/bench/cold",
				concurrency: 1,
				requests: 2,
			},
		];
	}
	if (profile !== "full") throw new Error(`unknown BENCH_PROFILE ${profile}`);
	return [
		...actorRoutingDefinitions(sequentialRequests),
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
			name: "warmSequential",
			path: "/bench/warm",
			concurrency: 1,
			requests: warmRequests,
		},
		{
			name: "warmDirectSequential",
			path: "/bench/warm-direct",
			concurrency: 1,
			requests: warmRequests,
		},
		{
			name: "warmConcurrent",
			path: "/bench/warm",
			concurrency: 8,
			requests: warmConcurrentRequests,
			timeoutMs: 10_000,
		},
		{
			name: "coldSequential",
			path: "/bench/cold",
			concurrency: 1,
			requests: coldRequests,
		},
		{
			name: "coldConcurrent",
			path: "/bench/cold",
			concurrency: 8,
			requests: 16,
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
	return [
		"resolve",
		"action-direct",
		"action-direct-skip",
		"action-query",
		"action-query-skip",
		"fetch-direct",
		"fetch-direct-skip",
		"fetch-query",
		"fetch-query-skip",
		"nested",
	].map((variant) => ({
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
