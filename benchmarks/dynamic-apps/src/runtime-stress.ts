import { strict as assert } from "node:assert";
import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { performance } from "node:perf_hooks";
import { promisify } from "node:util";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { DynamicActorRuntime } from "../../../packages/dynamic-apps/src/actor-runtime.js";
import {
	DynamicAppsExecutor,
	readExecutorConfig,
} from "../../../packages/dynamic-apps/src/executor.js";
import {
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
} from "../../../packages/dynamic-apps/src/runtime.js";

const execFileAsync = promisify(execFile);

interface Artifact {
	bytes: Uint8Array;
	hash: string;
	release: string;
	marker: string;
	dispose(): Promise<void>;
}

interface AppState {
	artifact: Artifact;
	revision: number;
}

interface StressResult {
	config: Record<string, number>;
	cases: Record<string, unknown>;
	memory: {
		baseline: ReturnType<typeof memorySnapshot>;
		final: ReturnType<typeof memorySnapshot>;
	};
}

const STRESS_CASES = [
	"multiApp",
	"payloadBurst",
	"invalidation",
	"coldFanout",
	"admission",
	"actorOversize",
	"actorStartup",
] as const;

type StressCase = (typeof STRESS_CASES)[number];

class FakeStatePlane {
	readonly apps = new Map<string, AppState>();
	readonly releases = new Map<string, Map<string, Artifact>>();
	readonly listeners = new Map<string, Set<(event: unknown) => void>>();
	readonly calls = { resolve: 0, manifest: 0, chunk: 0, connect: 0 };

	readonly client = {
		agentOSAppsApp: {
			getOrCreate: (key: string[]) => this.#handle(key[0] ?? ""),
		},
	};

	set(appId: string, artifact: Artifact): void {
		this.apps.set(appId, { artifact, revision: 1 });
		this.#retain(appId, artifact);
	}

	activate(appId: string, artifact: Artifact): void {
		const current = this.#state(appId);
		const next = { artifact, revision: current.revision + 1 };
		this.apps.set(appId, next);
		this.#retain(appId, artifact);
		for (const listener of this.listeners.get(appId) ?? []) {
			listener({
				revision: next.revision,
				release: artifact.release,
				artifactHash: artifact.hash,
				activatedAt: Date.now(),
			});
		}
	}

	#state(appId: string): AppState {
		const state = this.apps.get(appId);
		if (!state) throw new Error(`unknown stress app ${appId}`);
		return state;
	}

	#handle(appId: string) {
		return {
			resolveDeployment: async () => {
				this.calls.resolve += 1;
				const state = this.#state(appId);
				return resolution(appId, state);
			},
			getArtifactManifest: async (release: string) => {
				this.calls.manifest += 1;
				const artifact = this.#artifactForRelease(appId, release);
				return {
					format: DIRECT_RUNTIME_FORMAT,
					hash: artifact.hash,
					bytes: artifact.bytes.byteLength,
					chunks: 1,
					chunkBytes: artifact.bytes.byteLength,
				};
			},
			readArtifactChunk: async (release: string) => {
				this.calls.chunk += 1;
				return this.#artifactForRelease(appId, release).bytes;
			},
			connect: () => {
				this.calls.connect += 1;
				return {
					ready: Promise.resolve(),
					on: (_name: string, listener: (event: unknown) => void) => {
						const listeners = this.listeners.get(appId) ?? new Set();
						listeners.add(listener);
						this.listeners.set(appId, listeners);
						return () => listeners.delete(listener);
					},
					onOpen: () => () => {},
					onClose: () => () => {},
					dispose: async () => {},
				};
			},
		};
	}

	#artifactForRelease(appId: string, release: string): Artifact {
		const artifact = this.releases.get(appId)?.get(release);
		if (!artifact) throw new Error(`unknown release ${release} for ${appId}`);
		return artifact;
	}

	#retain(appId: string, artifact: Artifact): void {
		const releases = this.releases.get(appId) ?? new Map();
		releases.set(artifact.release, artifact);
		this.releases.set(appId, releases);
	}
}

async function main(): Promise<void> {
	const selectedCases = parseCases(process.env.STRESS_CASES);
	const appCount = integerEnv("STRESS_APP_COUNT", 24, 2, 256);
	const requests = integerEnv("STRESS_REQUESTS", 10_000, 100, 10_000_000);
	const concurrency = integerEnv("STRESS_CONCURRENCY", 64, 1, 1_024);
	const poolMaxTotal = integerEnv("STRESS_POOL_MAX_TOTAL", 8, 0, 256);
	const poolSize = integerEnv("STRESS_POOL_SIZE", 2, 0, 128);
	const requestBytes = integerEnv(
		"STRESS_REQUEST_BYTES",
		256 * 1024,
		0,
		1024 * 1024,
	);
	const responseBytes = integerEnv(
		"STRESS_RESPONSE_BYTES",
		128 * 1024,
		0,
		4 * 1024 * 1024,
	);
	const coldFanout = integerEnv("STRESS_COLD_FANOUT", 16, 1, 128);
	const baselineMemory = memorySnapshot();
	const artifacts = await Promise.all(
		Array.from({ length: appCount + 3 }, (_, index) =>
			createDirectArtifact(`stress-${index}`),
		),
	);
	const actorArtifact = await createActorArtifact("while (true) {}");
	const result: StressResult = {
		config: {
			appCount,
			requests,
			concurrency,
			poolMaxTotal,
			poolSize,
			requestBytes,
			responseBytes,
			coldFanout,
		},
		cases: {},
		memory: { baseline: baselineMemory, final: baselineMemory },
	};

	try {
		await runStressCase(result, selectedCases, "multiApp", () =>
			multiAppStress(
				artifacts.slice(0, appCount),
				requests,
				concurrency,
				poolMaxTotal,
				poolSize,
			),
		);
		await runStressCase(result, selectedCases, "payloadBurst", () =>
			payloadBurstStress(
				artifacts[appCount] as Artifact,
				requests,
				concurrency,
				poolMaxTotal,
				poolSize,
				requestBytes,
				responseBytes,
			),
		);
		await runStressCase(result, selectedCases, "invalidation", () =>
			invalidationStress(
				artifacts[appCount + 1] as Artifact,
				artifacts[appCount + 2] as Artifact,
				requests,
				concurrency,
				poolMaxTotal,
				poolSize,
			),
		);
		await runStressCase(result, selectedCases, "coldFanout", () =>
			coldFanoutStress(artifacts[0] as Artifact, coldFanout),
		);
		await runStressCase(result, selectedCases, "admission", () =>
			admissionStress(artifacts[1] as Artifact, concurrency),
		);
		await runStressCase(result, selectedCases, "actorOversize", () =>
			actorOversizeStress(requests, concurrency),
		);
		await runStressCase(result, selectedCases, "actorStartup", () =>
			actorStartupStress(actorArtifact, Math.min(concurrency, 16)),
		);
	} finally {
		await Promise.allSettled([
			...artifacts.map((artifact) => artifact.dispose()),
			actorArtifact.dispose(),
		]);
	}

	await settleMemory();
	result.memory.final = memorySnapshot();
	console.log(JSON.stringify(result, null, 2));
}

async function runStressCase(
	result: StressResult,
	selectedCases: ReadonlySet<StressCase>,
	name: StressCase,
	task: () => Promise<unknown>,
): Promise<void> {
	if (!selectedCases.has(name)) return;
	const before = memorySnapshot();
	const value = await task();
	const afterDispose = memorySnapshot();
	await settleMemory();
	result.cases[name] = {
		...(value as Record<string, unknown>),
		memory: { before, afterDispose, afterGc: memorySnapshot() },
	};
}

function parseCases(value: string | undefined): Set<StressCase> {
	const selected = new Set(
		(value ? value.split(",") : STRESS_CASES).map((item) => item.trim()),
	);
	for (const name of selected) {
		if (!(STRESS_CASES as readonly string[]).includes(name)) {
			throw new Error(`unknown STRESS_CASES entry ${name}`);
		}
	}
	return selected as Set<StressCase>;
}

function memorySnapshot() {
	const usage = process.memoryUsage();
	return {
		rssBytes: usage.rss,
		heapUsedBytes: usage.heapUsed,
		externalBytes: usage.external,
		arrayBuffersBytes: usage.arrayBuffers,
	};
}

async function settleMemory(): Promise<void> {
	const gc = (globalThis as { gc?: () => void }).gc;
	for (let index = 0; index < 3; index += 1) {
		gc?.();
		await new Promise<void>((resolve) => setImmediate(resolve));
	}
}

async function multiAppStress(
	artifacts: Artifact[],
	requests: number,
	concurrency: number,
	poolMaxTotal: number,
	poolSize: number,
): Promise<unknown> {
	const plane = new FakeStatePlane();
	for (const [index, artifact] of artifacts.entries()) {
		plane.set(`stress-app-${index}`, artifact);
	}
	const executor = new DynamicAppsExecutor(
		executorConfig({
			appEntries: artifacts.length,
			concurrency,
			poolMaxTotal,
			poolSize,
		}),
		plane.client as never,
	);
	const rss = rssSampler();
	const startedAt = performance.now();
	try {
		await runConcurrent(requests, concurrency, async (index) => {
			const appIndex = index % artifacts.length;
			const response = await executor.request(
				`stress-app-${appIndex}`,
				new Request(`http://stress.test/request/${index}`),
			);
			assert.equal(response.status, 200);
			const body = (await response.json()) as {
				marker: string;
				counter: number;
			};
			assert.equal(body.marker, artifacts[appIndex]?.marker);
			assert.equal(body.counter, 1);
		});
		const diagnostics = executor.diagnostics() as Record<string, number>;
		assert.equal(diagnostics.activeEvaluations, 0);
		assert.equal(diagnostics.queuedEvaluations, 0);
		assert.equal(diagnostics.inUseIsolates, 0);
		assert.equal(diagnostics.contextResetFailures, 0);
		assert(diagnostics.cleanIsolates <= poolMaxTotal);
		assert(diagnostics.pooledIsolates <= poolMaxTotal);
		return {
			elapsedMs: round(performance.now() - startedAt),
			requestsPerSecond: round(
				requests / ((performance.now() - startedAt) / 1_000),
			),
			peakRssBytes: rss.peak(),
			stateCalls: plane.calls,
			diagnostics,
		};
	} finally {
		rss.stop();
		await executor.dispose();
	}
}

async function payloadBurstStress(
	artifact: Artifact,
	requests: number,
	concurrency: number,
	poolMaxTotal: number,
	poolSize: number,
	requestBytes: number,
	responseBytes: number,
): Promise<unknown> {
	const plane = new FakeStatePlane();
	plane.set("payload", artifact);
	const executor = new DynamicAppsExecutor(
		executorConfig({ appEntries: 2, concurrency, poolMaxTotal, poolSize }),
		plane.client as never,
	);
	const latencies: number[] = [];
	const rss = rssSampler();
	const startedAt = performance.now();
	try {
		await executor.request("payload", new Request("http://stress.test/warm"));
		await runConcurrent(requests, concurrency, async (index) => {
			const requestStartedAt = performance.now();
			const response = await executor.request(
				"payload",
				new Request(
					`http://stress.test/payload/${index}?responseBytes=${responseBytes}`,
					{
						method: "POST",
						body: new Uint8Array(requestBytes),
					},
				),
			);
			assert.equal(response.status, 200);
			assert.equal((await response.arrayBuffer()).byteLength, responseBytes);
			latencies.push(performance.now() - requestStartedAt);
		});
		const diagnostics = executor.diagnostics() as Record<string, number>;
		assert.equal(diagnostics.activeEvaluations, 0);
		assert.equal(diagnostics.queuedEvaluations, 0);
		assert.equal(diagnostics.inUseIsolates, 0);
		assert.equal(diagnostics.contextResetFailures, 0);
		return {
			elapsedMs: round(performance.now() - startedAt),
			latencyMs: summarize(latencies),
			peakRssBytes: rss.peak(),
			diagnostics,
		};
	} finally {
		rss.stop();
		await executor.dispose();
	}
}

async function invalidationStress(
	before: Artifact,
	after: Artifact,
	requests: number,
	concurrency: number,
	poolMaxTotal: number,
	poolSize: number,
): Promise<unknown> {
	const plane = new FakeStatePlane();
	plane.set("invalidate", before);
	const executor = new DynamicAppsExecutor(
		executorConfig({ appEntries: 2, concurrency, poolMaxTotal, poolSize }),
		plane.client as never,
	);
	let activated = false;
	let oldResponsesAfterActivation = 0;
	const activationIndex = Math.floor(requests / 3);
	try {
		await runConcurrent(requests, concurrency, async (index) => {
			if (!activated && index >= activationIndex) {
				plane.activate("invalidate", after);
				activated = true;
			}
			const startedAfterActivation = activated;
			const response = await executor.request(
				"invalidate",
				new Request(`http://stress.test/invalidation/${index}`),
			);
			const body = (await response.json()) as {
				marker: string;
				counter: number;
			};
			assert.equal(body.counter, 1);
			if (startedAfterActivation && body.marker !== after.marker) {
				oldResponsesAfterActivation += 1;
			}
		});
		assert.equal(oldResponsesAfterActivation, 0);
		return {
			activationIndex,
			oldResponsesAfterActivation,
			stateCalls: plane.calls,
			diagnostics: executor.diagnostics(),
		};
	} finally {
		await executor.dispose();
	}
}

async function coldFanoutStress(
	artifact: Artifact,
	count: number,
): Promise<unknown> {
	const plane = new FakeStatePlane();
	plane.set("cold", artifact);
	const executors = Array.from(
		{ length: count },
		() =>
			new DynamicAppsExecutor(
				executorConfig({ appEntries: 1, concurrency: 1, poolMaxTotal: 0 }),
				plane.client as never,
			),
	);
	const startedAt = performance.now();
	try {
		await Promise.all(
			executors.map(async (executor, index) => {
				const response = await executor.request(
					"cold",
					new Request(`http://stress.test/cold/${index}`),
				);
				assert.equal(response.status, 200);
				await response.arrayBuffer();
			}),
		);
		return {
			fanout: count,
			elapsedMs: round(performance.now() - startedAt),
			stateCalls: plane.calls,
		};
	} finally {
		await Promise.all(executors.map((executor) => executor.dispose()));
	}
}

async function admissionStress(
	artifact: Artifact,
	concurrency: number,
): Promise<unknown> {
	const plane = new FakeStatePlane();
	plane.set("admission", artifact);
	const active = Math.min(4, concurrency);
	const queued = Math.min(8, Math.max(0, concurrency - active));
	const executor = new DynamicAppsExecutor(
		{
			...executorConfig({
				appEntries: 1,
				concurrency: active,
				poolMaxTotal: 2,
			}),
			executionQueueSize: queued,
		},
		plane.client as never,
	);
	let release = () => {};
	const gate = new Promise<void>((resolve) => {
		release = resolve;
	});
	let pulls = 0;
	const total = Math.max(concurrency * 2, active + queued + 1);
	try {
		await executor.request("admission", new Request("http://stress.test/warm"));
		const outcomes = Array.from({ length: total }, (_, index) =>
			executor
				.request(
					"admission",
					streamingRequest(`http://stress.test/admission/${index}`, {
						async pull(controller) {
							pulls += 1;
							await gate;
							controller.close();
						},
					}),
				)
				.then(
					() => "ok" as const,
					(error: unknown) =>
						isErrorCode(error, "agentos_apps_no_capacity")
							? ("rejected" as const)
							: Promise.reject(error),
				),
		);
		await waitFor(() => pulls === active);
		assert.equal(pulls, active);
		const admittedBodyPullsBeforeRelease = pulls;
		release();
		const settled = await Promise.all(outcomes);
		return {
			total,
			admittedBodyPullsBeforeRelease,
			completed: settled.filter((value) => value === "ok").length,
			rejected: settled.filter((value) => value === "rejected").length,
		};
	} finally {
		release();
		await executor.dispose();
	}
}

async function actorOversizeStress(
	requests: number,
	concurrency: number,
): Promise<unknown> {
	const runtime = new DynamicActorRuntime({
		DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES: "1024",
	});
	let pulls = 0;
	let cancellations = 0;
	try {
		await runConcurrent(requests, concurrency, async (index) => {
			let requestPulls = 0;
			const response = await runtime.request({
				key: `oversize-${index}`,
				loadArtifact: async () => {
					throw new Error("oversized callback loaded an artifact");
				},
				endpoint: "http://stress.test",
				namespace: "stress",
				pool: "default",
				request: streamingRequest(`http://stress.test/actor/${index}`, {
					pull(controller) {
						requestPulls += 1;
						pulls += 1;
						controller.enqueue(new Uint8Array(1024));
						if (requestPulls === 64) controller.close();
					},
					cancel() {
						cancellations += 1;
					},
				}),
			});
			assert.equal(response.status, 413);
			assert(requestPulls <= 2);
		});
		assert.equal(cancellations, requests);
		return { requests, pulls, cancellations };
	} finally {
		await runtime.dispose();
	}
}

async function actorStartupStress(
	artifact: Artifact,
	concurrency: number,
): Promise<unknown> {
	const runtime = new DynamicActorRuntime({
		DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES: String(concurrency),
		DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS: "50",
	});
	const startedAt = performance.now();
	try {
		await runConcurrent(concurrency, concurrency, async (index) => {
			await assert.rejects(
				runtime.request({
					key: `stalled-${index}`,
					loadArtifact: async () => artifact.bytes,
					endpoint: "http://stress.test",
					namespace: "stress",
					pool: "default",
					request: new Request(`http://stress.test/start/${index}`, {
						method: "POST",
					}),
				}),
				/startup exceeded/,
			);
		});
		return {
			workers: concurrency,
			elapsedMs: round(performance.now() - startedAt),
		};
	} finally {
		await runtime.dispose();
	}
}

function executorConfig(input: {
	appEntries: number;
	concurrency: number;
	poolMaxTotal: number;
	poolSize?: number;
}) {
	return readExecutorConfig({
		DYNAMIC_APPS_ISOLATE_MODE: "prewarm",
		DYNAMIC_APPS_ISOLATE_POOL_SIZE: String(input.poolSize ?? 2),
		DYNAMIC_APPS_ISOLATE_POOL_MAX_TOTAL: String(input.poolMaxTotal),
		DYNAMIC_APPS_ISOLATE_HEAP_LIMIT_MB: "64",
		DYNAMIC_APPS_RUNTIME_CACHE_MAX_ENTRIES: String(input.appEntries),
		DYNAMIC_APPS_RUNTIME_CACHE_MAX_BYTES: String(512 * 1024 * 1024),
		DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT: "95",
		DYNAMIC_APPS_EXECUTION_CONCURRENCY: String(Math.min(input.concurrency, 32)),
		DYNAMIC_APPS_EXECUTION_QUEUE_SIZE: String(
			Math.max(input.concurrency * 2, 64),
		),
		DYNAMIC_APPS_EXECUTION_QUEUE_WAIT_MS: "30000",
		DYNAMIC_APPS_EXECUTION_TIMEOUT_MS: "30000",
	});
}

function resolution(appId: string, state: AppState) {
	return {
		appId,
		release: state.artifact.release,
		region: "local",
		regions: ["local"],
		revision: state.revision,
		artifactHash: state.artifact.hash,
		artifactBytes: state.artifact.bytes.byteLength,
		entrypoint: DIRECT_ENTRYPOINT,
		namespace: "stress",
		scaling: { minReplicas: 0, maxReplicas: 1, targetConcurrency: 32 },
		maxRequestBytes: 1024 * 1024,
		maxResponseBytes: 4 * 1024 * 1024,
	};
}

async function createDirectArtifact(marker: string): Promise<Artifact> {
	const source = `
let counter = 0;
globalThis.__dynamicAppDispatch = async function(inputJson) {
  const input = JSON.parse(inputJson);
  counter += 1;
  const url = new URL(input.url);
  const responseBytes = Math.max(0, Math.min(4194304, Number(url.searchParams.get("responseBytes") || 0)));
  const requestBody = input.bodyBase64
    ? globalThis.__dynamicAppsBase64Decode(input.bodyBase64)
    : new Uint8Array();
  const body = responseBytes > 0
    ? new Uint8Array(responseBytes).fill(120)
    : new TextEncoder().encode(JSON.stringify({
        marker: ${JSON.stringify(marker)},
        counter,
        requestBytes: requestBody.byteLength,
      }));
  return JSON.stringify({
    status: 200,
    statusText: "OK",
    headers: [["content-type", responseBytes > 0 ? "application/octet-stream" : "application/json"]],
    bodyBase64: globalThis.__dynamicAppsBase64Encode(body),
  });
};
`;
	return createArtifact(marker, "direct", source);
}

function createActorArtifact(source: string): Promise<Artifact> {
	return createArtifact("actor-stall", "actor", source);
}

async function createArtifact(
	marker: string,
	directoryName: "direct" | "actor",
	source: string,
): Promise<Artifact> {
	const directory = await mkdtemp(
		join(tmpdir(), "dynamic-apps-runtime-stress-"),
	);
	const archive = join(directory, "app.tar");
	await mkdir(join(directory, directoryName));
	await writeFile(join(directory, directoryName, "main.mjs"), source);
	await writeFile(
		join(directory, "agentos-package.json"),
		JSON.stringify({ name: `dynamic-apps-${marker}`, version: "1.0.0" }),
	);
	await execFileAsync(
		"tar",
		["-cf", archive, directoryName, "agentos-package.json"],
		{ cwd: directory },
	);
	const bytes = new Uint8Array(
		packAospkgFromTarBytes(await readFile(archive)).bytes,
	);
	return {
		bytes,
		hash: createHash("sha256").update(bytes).digest("hex"),
		release: `release-${marker}`,
		marker,
		dispose: () => rm(directory, { recursive: true, force: true }),
	};
}

async function runConcurrent(
	total: number,
	concurrency: number,
	task: (index: number) => Promise<void>,
): Promise<void> {
	let next = 0;
	await Promise.all(
		Array.from({ length: Math.min(total, concurrency) }, async () => {
			for (;;) {
				const index = next;
				next += 1;
				if (index >= total) return;
				await task(index);
			}
		}),
	);
}

function streamingRequest(
	url: string,
	source: UnderlyingDefaultSource<Uint8Array>,
): Request {
	return new Request(url, {
		method: "POST",
		body: new ReadableStream(source, { highWaterMark: 0 }),
		duplex: "half",
	} as RequestInit & { duplex: "half" });
}

function rssSampler(): { peak(): number; stop(): void } {
	let peak = process.memoryUsage().rss;
	const timer = setInterval(() => {
		peak = Math.max(peak, process.memoryUsage().rss);
	}, 5);
	timer.unref?.();
	return {
		peak: () => peak,
		stop: () => clearInterval(timer),
	};
}

async function waitFor(predicate: () => boolean): Promise<void> {
	const deadline = Date.now() + 5_000;
	while (Date.now() < deadline) {
		if (predicate()) return;
		await new Promise((resolve) => setTimeout(resolve, 1));
	}
	throw new Error("stress condition did not become true");
}

function isErrorCode(error: unknown, code: string): boolean {
	return (
		typeof error === "object" &&
		error !== null &&
		"code" in error &&
		error.code === code
	);
}

function summarize(values: number[]): Record<string, number> {
	values.sort((a, b) => a - b);
	return {
		p50: round(percentile(values, 0.5)),
		p95: round(percentile(values, 0.95)),
		p99: round(percentile(values, 0.99)),
		max: round(values.at(-1) ?? 0),
	};
}

function percentile(values: number[], quantile: number): number {
	if (values.length === 0) return 0;
	return (
		values[
			Math.min(values.length - 1, Math.ceil(values.length * quantile) - 1)
		] ?? 0
	);
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

function round(value: number): number {
	return Math.round(value * 100) / 100;
}

await main();
