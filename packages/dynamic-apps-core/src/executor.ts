import { createHash, randomUUID } from "node:crypto";
import { chmod, mkdtemp, rm, writeFile } from "node:fs/promises";
import { availableParallelism, tmpdir } from "node:os";
import { join } from "node:path";
import { AgentOs } from "@rivet-dev/agentos-core";
import { DynamicAppsError } from "./errors.js";
import { DynamicAppsLogLineDecoder, emitDynamicAppsLog } from "./logging.js";
import { capConcurrencyForMemory, readCgroupMemory } from "./memory.js";
import { DIRECT_BUNDLE_PATH, DIRECT_RUNTIME_FORMAT } from "./runtime.js";
import { validateAppId } from "./source.js";
import type {
	ActiveRelease,
	ReleaseInvalidation,
	ReleaseLoadContext,
	Unsubscribe,
} from "./types.js";

const MAX_URL_BYTES = 16 * 1024;
const MAX_METHOD_BYTES = 256;
const MAX_HEADER_PAIRS = 256;
const MAX_HEADER_BYTES = 64 * 1024;
const MAX_RESPONSE_STATUS_TEXT_BYTES = 1024;
const MAX_REQUEST_BODY_BYTES = 1024 * 1024;
const MAX_RESPONSE_BODY_BYTES = 4 * 1024 * 1024;
const AGENTOS_VM_OVERHEAD_MB = 64;

const HOP_BY_HOP_HEADERS = [
	"connection",
	"keep-alive",
	"proxy-authenticate",
	"proxy-authorization",
	"te",
	"trailer",
	"transfer-encoding",
	"upgrade",
] as const;

export type ExecutionMode = "ephemeral" | "pooled";

export interface ExecutorConfig {
	executionMode: ExecutionMode;
	contextPoolSize: number;
	contextPoolMaxTotal: number;
	contextIdleTtlMs: number;
	contextHeapLimitMb: number;
	runtimeCacheMaxEntries: number;
	runtimeCacheMaxBytes: number;
	runtimeCacheIdleTtlMs: number;
	memoryHighWaterPercent: number;
	executionConcurrency: number;
	executionQueueSize: number;
	executionQueueWaitMs: number;
	executionTimeoutMs: number;
	timingHeaders: boolean;
	logRequests: boolean;
}

export interface ExecutorReleaseSource {
	loadActiveRelease(
		appId: string,
		context: ReleaseLoadContext,
	): Promise<ActiveRelease | undefined>;
	watchActiveRelease(
		appId: string,
		invalidate: ReleaseInvalidation,
	): Promise<Unsubscribe>;
}

interface RequestEnvelope {
	url: string;
	method: string;
	headers: Array<[string, string]>;
	bodyBase64?: string;
}

interface ResponseEnvelope {
	status: number;
	statusText: string;
	headers: Array<[string, string]>;
	bodyBase64: string;
	timing?: Record<string, number>;
}

interface RequestTrace {
	appId: string;
	requestId: string;
	startedAt: number;
	phases: Map<string, number>;
	cacheOutcome: string;
	executionMode: ExecutionMode;
	release?: string;
}

interface ContextSlot {
	id: string;
	pooled: boolean;
	lastUsedAt: number;
}

interface PreparedRuntime {
	key: string;
	appId: string;
	release: string;
	artifactHash: string;
	artifactBytes: number;
	artifact: Uint8Array;
	directory: string;
	artifactPath: string;
	vm: AgentOs;
	cleanContexts: ContextSlot[];
	inUse: number;
	refilling: number;
	refs: number;
	stale: boolean;
	disposing: boolean;
	lastUsedAt: number;
	backgroundTasks: Set<Promise<unknown>>;
	activeControllers: Set<AbortController>;
	activeEvaluations: Set<Promise<unknown>>;
	disposePromise?: Promise<void>;
	vmCreates: number;
	vmDisposes: number;
	contextCreates: number;
	contextDisposes: number;
	contextResetFailures: number;
	contextOverflowEvaluations: number;
	lastContextResetError?: string;
	evaluations: number;
}

interface AppMapping {
	resolution: ActiveRelease;
	runtime: PreparedRuntime;
}

interface AppCacheEntry {
	appId: string;
	subscription: Promise<Unsubscribe>;
	unsubscribe?: Unsubscribe;
	mapping?: AppMapping;
	resolvePromise?: Promise<AppMapping>;
	epoch: number;
	lastUsedAt: number;
	refs: number;
}

interface EvaluationResult<T> {
	outcome: "succeeded" | "failed" | "cancelled" | "timed_out";
	value?: T;
	error?: { message?: string };
}

export function readExecutorConfig(
	env: NodeJS.ProcessEnv = process.env,
): ExecutorConfig {
	const executionMode = env.DYNAMIC_APPS_EXECUTION_MODE ?? "pooled";
	if (executionMode !== "ephemeral" && executionMode !== "pooled") {
		throw new DynamicAppsError(
			"agentos_apps_invalid_config",
			"DYNAMIC_APPS_EXECUTION_MODE must be ephemeral or pooled",
		);
	}
	const requestedPoolSize = integerEnv(
		env,
		"DYNAMIC_APPS_CONTEXT_POOL_SIZE",
		2,
		0,
		128,
	);
	const contextHeapLimitMb = integerEnv(
		env,
		"DYNAMIC_APPS_CONTEXT_HEAP_LIMIT_MB",
		64,
		8,
		2_048,
	);
	const memoryHighWaterPercent = integerEnv(
		env,
		"DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT",
		70,
		10,
		95,
	);
	const requestedExecutionConcurrency = integerEnv(
		env,
		"DYNAMIC_APPS_EXECUTION_CONCURRENCY",
		Math.max(1, availableParallelism()),
		1,
		1_024,
	);
	const requestedPoolMaxTotal = integerEnv(
		env,
		"DYNAMIC_APPS_CONTEXT_POOL_MAX_TOTAL",
		8,
		0,
		1_024,
	);
	const cgroupMemory = readCgroupMemory();
	const memoryContextCap = cgroupMemory
		? capConcurrencyForMemory({
				requested: 1_024,
				contextAndVmLimitMb: contextHeapLimitMb + AGENTOS_VM_OVERHEAD_MB,
				memoryHighWaterPercent,
				currentBytes: cgroupMemory.currentBytes,
				maxBytes: cgroupMemory.maxBytes,
			})
		: undefined;
	return {
		executionMode,
		contextPoolSize: Math.min(
			requestedPoolSize,
			memoryContextCap ?? requestedPoolSize,
		),
		contextPoolMaxTotal: Math.min(
			requestedPoolMaxTotal,
			memoryContextCap ?? requestedPoolMaxTotal,
		),
		contextIdleTtlMs: integerEnv(
			env,
			"DYNAMIC_APPS_CONTEXT_IDLE_TTL_MS",
			30_000,
			1_000,
			60 * 60_000,
		),
		contextHeapLimitMb,
		runtimeCacheMaxEntries: integerEnv(
			env,
			"DYNAMIC_APPS_RUNTIME_CACHE_MAX_ENTRIES",
			16,
			1,
			1_024,
		),
		runtimeCacheMaxBytes: integerEnv(
			env,
			"DYNAMIC_APPS_RUNTIME_CACHE_MAX_BYTES",
			256 * 1024 * 1024,
			1024 * 1024,
			16 * 1024 * 1024 * 1024,
		),
		runtimeCacheIdleTtlMs: integerEnv(
			env,
			"DYNAMIC_APPS_RUNTIME_CACHE_IDLE_TTL_MS",
			15 * 60_000,
			1_000,
			24 * 60 * 60_000,
		),
		memoryHighWaterPercent,
		executionConcurrency: Math.min(
			requestedExecutionConcurrency,
			memoryContextCap ?? requestedExecutionConcurrency,
		),
		executionQueueSize: integerEnv(
			env,
			"DYNAMIC_APPS_EXECUTION_QUEUE_SIZE",
			64,
			0,
			100_000,
		),
		executionQueueWaitMs: integerEnv(
			env,
			"DYNAMIC_APPS_EXECUTION_QUEUE_WAIT_MS",
			5_000,
			1,
			60_000,
		),
		executionTimeoutMs: integerEnv(
			env,
			"DYNAMIC_APPS_EXECUTION_TIMEOUT_MS",
			30_000,
			1,
			5 * 60_000,
		),
		timingHeaders: env.DYNAMIC_APPS_TIMING_HEADERS === "1",
		logRequests: env.DYNAMIC_APPS_LOG_REQUESTS === "1",
	};
}

export function resolveExecutorConfig(
	base: ExecutorConfig,
	overrides: Partial<ExecutorConfig> = {},
): ExecutorConfig {
	const config = { ...base, ...overrides };
	if (
		config.executionMode !== "ephemeral" &&
		config.executionMode !== "pooled"
	) {
		throw invalidExecutorConfig("executionMode");
	}
	const limits: Array<[keyof ExecutorConfig, number, number]> = [
		["contextPoolSize", 0, 128],
		["contextPoolMaxTotal", 0, 1_024],
		["contextIdleTtlMs", 1_000, 60 * 60_000],
		["contextHeapLimitMb", 8, 2_048],
		["runtimeCacheMaxEntries", 1, 1_024],
		["runtimeCacheMaxBytes", 1024 * 1024, 16 * 1024 * 1024 * 1024],
		["runtimeCacheIdleTtlMs", 1_000, 24 * 60 * 60_000],
		["memoryHighWaterPercent", 10, 95],
		["executionConcurrency", 1, 1_024],
		["executionQueueSize", 0, 100_000],
		["executionQueueWaitMs", 1, 60_000],
		["executionTimeoutMs", 1, 5 * 60_000],
	];
	for (const [name, minimum, maximum] of limits) {
		const value = config[name];
		if (
			typeof value !== "number" ||
			!Number.isSafeInteger(value) ||
			value < minimum ||
			value > maximum
		) {
			throw invalidExecutorConfig(String(name));
		}
	}
	if (
		typeof config.timingHeaders !== "boolean" ||
		typeof config.logRequests !== "boolean"
	) {
		throw invalidExecutorConfig("timingHeaders/logRequests");
	}
	return config;
}

function invalidExecutorConfig(name: string): DynamicAppsError {
	return new DynamicAppsError(
		"agentos_apps_invalid_config",
		`invalid Dynamic Apps executor config ${name}`,
	);
}

export class DynamicAppsExecutor {
	readonly config: ExecutorConfig;
	readonly #source: ExecutorReleaseSource;
	readonly #semaphore: Semaphore;
	readonly #apps = new Map<string, AppCacheEntry>();
	readonly #runtimes = new Map<string, PreparedRuntime>();
	readonly #runtimePromises = new Map<string, Promise<PreparedRuntime>>();
	readonly #cleanupTimer: ReturnType<typeof setInterval>;
	#pooledContexts = 0;
	#poolReservations = 0;
	#disposed = false;
	#disposePromise?: Promise<void>;

	constructor(source: ExecutorReleaseSource, config: ExecutorConfig) {
		this.config = config;
		this.#source = source;
		this.#semaphore = new Semaphore(
			config.executionConcurrency,
			config.executionQueueSize,
			config.executionQueueWaitMs,
		);
		this.#cleanupTimer = setInterval(
			() => void this.#pruneCaches(true),
			Math.min(config.contextIdleTtlMs, config.runtimeCacheIdleTtlMs, 30_000),
		);
		this.#cleanupTimer.unref?.();
	}

	async request(
		appId: string,
		request: Request,
		requestId: string = randomUUID(),
	): Promise<Response> {
		if (this.#disposed) {
			throw new DynamicAppsError(
				"agentos_apps_executor_disposed",
				"Dynamic Apps executor is shutting down",
			);
		}
		const trace: RequestTrace = {
			appId,
			requestId,
			startedAt: performance.now(),
			phases: new Map(),
			cacheOutcome: "app-hit",
			executionMode: this.config.executionMode,
		};
		let admitted = false;
		try {
			await measure(trace, "execution-queue", () => this.#semaphore.acquire());
			admitted = true;
			const envelope = await measure(trace, "request-buffer", () =>
				serializeRequest(request),
			);
			const requestedRegion =
				request.headers.get("x-agentos-app-region") ?? undefined;
			const { entry, hit } = this.#appEntry(appId);
			if (!hit) trace.cacheOutcome = "app-miss";
			entry.refs += 1;
			entry.lastUsedAt = Date.now();
			try {
				const mapping = entry.mapping
					? entry.mapping
					: await this.#resolveAndPrepare(entry, trace);
				if (
					requestedRegion &&
					!mapping.resolution.regions.includes(requestedRegion)
				) {
					throw new DynamicAppsError(
						"agentos_apps_region_not_deployed",
						`app is not deployed in requested region ${requestedRegion}`,
						{ requestedRegion, regions: mapping.resolution.regions },
					);
				}
				trace.release = mapping.resolution.release;
				mapping.runtime.refs += 1;
				mapping.runtime.lastUsedAt = Date.now();
				try {
					const response = await this.#execute(
						mapping.runtime,
						envelope,
						trace,
						request.signal,
					);
					this.#finishTrace(response.headers, trace);
					return response;
				} finally {
					mapping.runtime.refs -= 1;
					void this.#maybeDisposeRuntime(mapping.runtime);
				}
			} finally {
				entry.refs -= 1;
				void this.#pruneCaches();
			}
		} finally {
			if (admitted) this.#semaphore.release();
		}
	}

	diagnostics(): Record<string, unknown> {
		const runtimes = [...this.#runtimes.values()];
		return {
			apps: this.#apps.size,
			executionConcurrency: this.config.executionConcurrency,
			runtimes: runtimes.length,
			artifactBytes: runtimes.reduce(
				(sum, item) => sum + item.artifactBytes,
				0,
			),
			activeEvaluations: this.#semaphore.active,
			queuedEvaluations: this.#semaphore.queued,
			cleanContexts: runtimes.reduce(
				(sum, item) => sum + item.cleanContexts.length,
				0,
			),
			pooledContexts: this.#pooledContexts,
			poolReservations: this.#poolReservations,
			contextPoolMaxTotal: this.config.contextPoolMaxTotal,
			inUseContexts: runtimes.reduce((sum, item) => sum + item.inUse, 0),
			refillingContexts: runtimes.reduce(
				(sum, item) => sum + item.refilling,
				0,
			),
			rssBytes: process.memoryUsage().rss,
			vmCreates: runtimes.reduce((sum, item) => sum + item.vmCreates, 0),
			vmDisposes: runtimes.reduce((sum, item) => sum + item.vmDisposes, 0),
			contextCreates: runtimes.reduce(
				(sum, item) => sum + item.contextCreates,
				0,
			),
			contextDisposes: runtimes.reduce(
				(sum, item) => sum + item.contextDisposes,
				0,
			),
			contextResetFailures: runtimes.reduce(
				(sum, item) => sum + item.contextResetFailures,
				0,
			),
			contextOverflowEvaluations: runtimes.reduce(
				(sum, item) => sum + item.contextOverflowEvaluations,
				0,
			),
			lastContextResetError: runtimes
				.filter((item) => item.lastContextResetError !== undefined)
				.at(-1)?.lastContextResetError,
			evaluations: runtimes.reduce((sum, item) => sum + item.evaluations, 0),
		};
	}

	async dispose(): Promise<void> {
		if (this.#disposePromise !== undefined) return this.#disposePromise;
		this.#disposed = true;
		clearInterval(this.#cleanupTimer);
		this.#semaphore.dispose();
		this.#disposePromise = this.#finishDispose();
		return this.#disposePromise;
	}

	async #finishDispose(): Promise<void> {
		const entries = [...this.#apps.values()];
		this.#apps.clear();
		await Promise.allSettled([
			...entries.map((entry) => this.#releaseEntry(entry)),
			...this.#runtimePromises.values(),
		]);
		for (const runtime of this.#runtimes.values()) runtime.stale = true;
		await Promise.allSettled(
			[...this.#runtimes.values()].map((runtime) =>
				this.#disposeRuntime(runtime),
			),
		);
		this.#runtimes.clear();
	}

	#appEntry(appId: string): { entry: AppCacheEntry; hit: boolean } {
		const existing = this.#apps.get(appId);
		if (existing) return { entry: existing, hit: true };
		const entry: AppCacheEntry = {
			appId,
			subscription: undefined as unknown as Promise<Unsubscribe>,
			epoch: 0,
			lastUsedAt: Date.now(),
			refs: 0,
		};
		this.#apps.set(appId, entry);
		entry.subscription = Promise.resolve()
			.then(() =>
				this.#source.watchActiveRelease(appId, () => this.invalidate(appId)),
			)
			.then(async (unsubscribe) => {
				if (typeof unsubscribe !== "function") {
					throw new DynamicAppsError(
						"agentos_apps_invalid_subscription",
						"watchActiveRelease must resolve to an unsubscribe function",
					);
				}
				const once = onceUnsubscribe(unsubscribe);
				if (this.#disposed || this.#apps.get(appId) !== entry) {
					await once();
				} else {
					entry.unsubscribe = once;
				}
				return once;
			})
			.catch((error) => {
				if (this.#apps.get(appId) === entry) this.#apps.delete(appId);
				throw error;
			});
		return { entry, hit: false };
	}

	invalidate(appId: string): void {
		const entry = this.#apps.get(appId);
		if (!entry || this.#disposed) return;
		const active = entry.mapping !== undefined || entry.refs > 0;
		entry.epoch += 1;
		entry.mapping = undefined;
		if (active) void this.#resolveAndPrepare(entry).catch(() => {});
	}

	#invalidateRuntime(runtime: PreparedRuntime): void {
		runtime.stale = true;
		if (this.#runtimes.get(runtime.key) === runtime) {
			this.#runtimes.delete(runtime.key);
		}
		void this.#maybeDisposeRuntime(runtime);
	}

	async #resolveAndPrepare(
		entry: AppCacheEntry,
		trace?: RequestTrace,
	): Promise<AppMapping> {
		if (entry.mapping) return entry.mapping;
		if (entry.resolvePromise !== undefined) return entry.resolvePromise;
		const promise = (async () => {
			for (;;) {
				await entry.subscription;
				const epoch = entry.epoch;
				const resolution = await measureOptional(trace, "release-load", () =>
					this.#source.loadActiveRelease(
						entry.appId,
						createReleaseLoadContext(trace),
					),
				);
				if (entry.epoch !== epoch) continue;
				if (!resolution) {
					throw new DynamicAppsError(
						"agentos_apps_not_deployed",
						"app has no active direct release; call deployApp() first",
					);
				}
				if (resolution.appId !== entry.appId) {
					throw new DynamicAppsError(
						"agentos_apps_active_release_invalid",
						"loadActiveRelease returned a release for a different app",
					);
				}
				const verifiedResolution = await measureOptional(
					trace,
					"artifact-verify",
					async () => verifyActiveRelease(resolution),
				);
				const runtime = await this.#prepareRuntime(verifiedResolution, trace);
				if (entry.epoch !== epoch) continue;
				if (this.#disposed || this.#apps.get(entry.appId) !== entry) {
					throw disposedError();
				}
				const mapping = { resolution: verifiedResolution, runtime };
				entry.mapping = mapping;
				return mapping;
			}
		})();
		entry.resolvePromise = promise;
		try {
			return await promise;
		} finally {
			if (entry.resolvePromise === promise) entry.resolvePromise = undefined;
		}
	}

	async #prepareRuntime(
		resolution: ActiveRelease,
		trace?: RequestTrace,
	): Promise<PreparedRuntime> {
		if (this.#disposed) {
			throw new DynamicAppsError(
				"agentos_apps_executor_disposed",
				"Dynamic Apps executor is shutting down",
			);
		}
		const key = `${resolution.appId}:${resolution.release}:${resolution.artifact.hash}:${DIRECT_RUNTIME_FORMAT}`;
		const existing = this.#runtimes.get(key);
		if (existing && !existing.stale) {
			existing.lastUsedAt = Date.now();
			return existing;
		}
		const pending = this.#runtimePromises.get(key);
		if (pending) return pending;
		const promise = this.#createRuntime(key, resolution, trace);
		this.#runtimePromises.set(key, promise);
		try {
			return await promise;
		} finally {
			if (this.#runtimePromises.get(key) === promise) {
				this.#runtimePromises.delete(key);
			}
		}
	}

	async #createRuntime(
		key: string,
		resolution: ActiveRelease,
		trace?: RequestTrace,
	): Promise<PreparedRuntime> {
		const artifact = resolution.artifact.bytes;
		await this.#pruneCaches(true, artifact.byteLength);
		if (artifact.byteLength > this.config.runtimeCacheMaxBytes) {
			throw new DynamicAppsError(
				"agentos_apps_artifact_cache_limit",
				"application artifact is larger than the configured runtime cache",
			);
		}
		const directory = await mkdtemp(join(tmpdir(), "dynamic-app-runtime-"));
		const artifactPath = join(directory, "release.aospkg");
		let vm: AgentOs | undefined;
		try {
			await chmod(directory, 0o700);
			await writeFile(artifactPath, artifact, { mode: 0o600 });
			vm = await measureOptional(trace, "vm-prepare", () =>
				AgentOs.create({
					sidecar: { kind: "shared", pool: "dynamic-apps-direct" },
					defaultSoftware: false,
					mounts: [
						{
							path: "/app",
							readOnly: true,
							plugin: {
								id: "agentos_packages",
								config: {
									kind: "tar",
									tarPath: artifactPath,
									root: "/",
									readOnly: true,
								},
							},
						},
					],
					permissions: {
						fs: "allow",
						childProcess: "allow",
						process: "allow",
						env: "allow",
						network: "allow",
					},
					limits: {
						jsRuntime: { v8HeapLimitMb: this.config.contextHeapLimitMb },
					},
				}),
			);
			const runtime: PreparedRuntime = {
				key,
				appId: resolution.appId,
				release: resolution.release,
				artifactHash: resolution.artifact.hash,
				artifactBytes: artifact.byteLength,
				artifact,
				directory,
				artifactPath,
				vm,
				cleanContexts: [],
				inUse: 0,
				refilling: 0,
				refs: 0,
				stale: false,
				disposing: false,
				lastUsedAt: Date.now(),
				backgroundTasks: new Set(),
				activeControllers: new Set(),
				activeEvaluations: new Set(),
				vmCreates: 1,
				vmDisposes: 0,
				contextCreates: 0,
				contextDisposes: 0,
				contextResetFailures: 0,
				contextOverflowEvaluations: 0,
				evaluations: 0,
			};
			this.#runtimes.set(key, runtime);
			if (this.config.executionMode === "pooled") {
				await measureOptional(trace, "context-prewarm", () =>
					this.#fillPool(runtime),
				);
			}
			if (this.#disposed) {
				throw new DynamicAppsError(
					"agentos_apps_executor_disposed",
					"Dynamic Apps executor is shutting down",
				);
			}
			emitDynamicAppsLog({
				level: "debug",
				source: "runtime",
				message: "Dynamic Apps release runtime prepared",
				appId: resolution.appId,
				release: resolution.release,
			});
			return runtime;
		} catch (error) {
			if (vm) await vm.dispose().catch(() => {});
			await rm(directory, { recursive: true, force: true }).catch(() => {});
			this.#runtimes.delete(key);
			emitDynamicAppsLog({
				level: "error",
				source: "runtime",
				message: "Dynamic Apps release runtime preparation failed",
				appId: resolution.appId,
				release: resolution.release,
			});
			throw error;
		}
	}

	async #execute(
		runtime: PreparedRuntime,
		envelope: RequestEnvelope,
		trace: RequestTrace,
		requestSignal: AbortSignal,
	): Promise<Response> {
		let slot: ContextSlot | undefined;
		let pooledActive = false;
		let completedSuccessfully = false;
		try {
			if (this.config.executionMode === "pooled") {
				slot = await measure(trace, "context-lease", () =>
					this.#acquireContext(runtime),
				);
				pooledActive = true;
				if (!slot) runtime.contextOverflowEvaluations += 1;
			}
			const expression = slot
				? "await globalThis.__dynamicAppsDispatch(inputs.request)"
				: `await (await import("/app/${DIRECT_BUNDLE_PATH}")).dispatch(inputs.request)`;
			const result = await measure(trace, "evaluation", () =>
				this.#evaluate<ResponseEnvelope>(runtime, expression, {
					...(slot ? { contextId: slot.id } : {}),
					inputs: { request: envelope as never },
					trace,
					signal: requestSignal,
				}),
			);
			const output = evaluationValue(result, this.config.executionTimeoutMs);
			runtime.evaluations += 1;
			if (output.timing) {
				for (const [name, value] of Object.entries(output.timing)) {
					if (Number.isFinite(value))
						trace.phases.set(camelToKebab(name), value);
				}
			}
			const response = responseFromEnvelope(output, envelope.method);
			completedSuccessfully = true;
			return response;
		} finally {
			if (pooledActive) {
				if (slot) {
					const startedAt = performance.now();
					if (completedSuccessfully && !runtime.stale && !this.#disposed) {
						try {
							await runtime.vm.contexts.reset(slot.id);
							await this.#initializeContext(runtime, slot, trace);
							this.#offerContext(runtime, slot);
						} catch (error) {
							runtime.contextResetFailures += 1;
							runtime.lastContextResetError = errorMessage(error);
							emitDynamicAppsLog({
								level: "error",
								source: "runtime",
								message: "Dynamic Apps context reset failed",
								appId: runtime.appId,
								release: runtime.release,
								requestId: trace.requestId,
							});
							await this.#deleteContext(runtime, slot);
						}
					} else {
						await this.#deleteContext(runtime, slot);
					}
					trace.phases.set("context-reset", performance.now() - startedAt);
				}
				runtime.inUse = Math.max(0, runtime.inUse - 1);
				this.#ensurePool(runtime);
			}
		}
	}

	async #evaluate<T>(
		runtime: PreparedRuntime,
		expression: string,
		options: {
			contextId?: string;
			inputs?: Record<string, never>;
			trace?: RequestTrace;
			signal?: AbortSignal;
		},
	): Promise<EvaluationResult<T>> {
		const controller = new AbortController();
		runtime.activeControllers.add(controller);
		const signal = options.signal
			? AbortSignal.any([options.signal, controller.signal])
			: controller.signal;
		const stdout = this.#executionLogDecoder(runtime, options.trace, "stdout");
		const stderr = this.#executionLogDecoder(runtime, options.trace, "stderr");
		const evaluation = runtime.vm.javascript.evaluate<T>(expression, {
			...(options.contextId ? { contextId: options.contextId } : {}),
			...(options.inputs ? { inputs: options.inputs } : {}),
			format: "module",
			timeoutMs: this.config.executionTimeoutMs,
			signal,
			onStdout: (chunk) => stdout.write(chunk),
			onStderr: (chunk) => stderr.write(chunk),
		}) as Promise<EvaluationResult<T>>;
		runtime.activeEvaluations.add(evaluation);
		try {
			return await evaluation;
		} catch (error) {
			if (error instanceof Error && error.name === "AbortError") {
				throw new DynamicAppsError(
					"agentos_apps_execution_cancelled",
					"application execution was cancelled",
				);
			}
			throw error;
		} finally {
			stdout.end();
			stderr.end();
			runtime.activeEvaluations.delete(evaluation);
			runtime.activeControllers.delete(controller);
		}
	}

	#executionLogDecoder(
		runtime: PreparedRuntime,
		trace: RequestTrace | undefined,
		stream: "stdout" | "stderr",
	): DynamicAppsLogLineDecoder {
		return new DynamicAppsLogLineDecoder((message, truncated) => {
			const startedAt = performance.now();
			emitDynamicAppsLog({
				level: stream === "stdout" ? "info" : "error",
				source: "application",
				message,
				appId: runtime.appId,
				release: runtime.release,
				...(trace ? { requestId: trace.requestId } : {}),
				stream,
				...(truncated ? { metadata: { truncated: true } } : {}),
			});
			if (trace) {
				trace.phases.set(
					"log-dispatch",
					(trace.phases.get("log-dispatch") ?? 0) +
						(performance.now() - startedAt),
				);
			}
		});
	}

	async #initializeContext(
		runtime: PreparedRuntime,
		slot: ContextSlot,
		trace?: RequestTrace,
	): Promise<void> {
		const result = await this.#evaluate<boolean>(
			runtime,
			`await import("/app/${DIRECT_BUNDLE_PATH}").then((module) => { globalThis.__dynamicAppsDispatch = module.dispatch; return typeof module.dispatch === "function"; })`,
			{ contextId: slot.id, trace },
		);
		if (evaluationValue(result, this.config.executionTimeoutMs) !== true) {
			throw new Error("application bundle did not export a dispatcher");
		}
	}

	async #createContext(runtime: PreparedRuntime): Promise<ContextSlot> {
		const slot: ContextSlot = {
			id: randomUUID(),
			pooled: false,
			lastUsedAt: Date.now(),
		};
		await runtime.vm.createContext(slot.id);
		try {
			await this.#initializeContext(runtime, slot);
			runtime.contextCreates += 1;
			return slot;
		} catch (error) {
			await runtime.vm.contexts.delete(slot.id).catch(() => {});
			throw error;
		}
	}

	async #fillPool(runtime: PreparedRuntime): Promise<void> {
		if (await this.#memoryPressure()) return;
		const reserved = this.#reservePoolContexts(this.config.contextPoolSize);
		const outcomes = await Promise.allSettled(
			Array.from({ length: reserved }, () => this.#createContext(runtime)),
		);
		const failed = outcomes.find(
			(outcome): outcome is PromiseRejectedResult =>
				outcome.status === "rejected",
		);
		for (const outcome of outcomes) {
			this.#poolReservations = Math.max(0, this.#poolReservations - 1);
			if (outcome.status === "fulfilled") {
				if (failed) await this.#deleteContext(runtime, outcome.value);
				else this.#offerContext(runtime, outcome.value);
			}
		}
		if (failed) throw failed.reason;
	}

	#ensurePool(runtime: PreparedRuntime): void {
		if (
			this.config.executionMode !== "pooled" ||
			runtime.stale ||
			this.#disposed
		)
			return;
		const missingForRuntime =
			this.config.contextPoolSize -
			(runtime.cleanContexts.length + runtime.inUse + runtime.refilling);
		const missing = this.#reservePoolContexts(missingForRuntime);
		for (let index = 0; index < missing; index += 1) {
			runtime.refilling += 1;
			let reserved = true;
			const task = this.#memoryPressure()
				.then(async (pressure) => {
					if (pressure || runtime.stale) return;
					const slot = await this.#createContext(runtime);
					this.#poolReservations = Math.max(0, this.#poolReservations - 1);
					reserved = false;
					this.#offerContext(runtime, slot);
				})
				.catch((error) => {
					emitDynamicAppsLog({
						level: "error",
						source: "runtime",
						message: "Dynamic Apps context replenishment failed",
						appId: runtime.appId,
						release: runtime.release,
						metadata: { error: errorMessage(error) },
					});
				})
				.finally(() => {
					if (reserved) {
						this.#poolReservations = Math.max(0, this.#poolReservations - 1);
					}
					runtime.refilling -= 1;
					void this.#maybeDisposeRuntime(runtime);
				});
			this.#track(runtime, task);
		}
	}

	async #acquireContext(
		runtime: PreparedRuntime,
	): Promise<ContextSlot | undefined> {
		const slot = runtime.cleanContexts.shift();
		runtime.inUse += 1;
		if (slot) slot.lastUsedAt = Date.now();
		return slot;
	}

	#offerContext(runtime: PreparedRuntime, slot: ContextSlot): void {
		if (runtime.stale || this.#disposed) {
			void this.#deleteContext(runtime, slot);
			return;
		}
		if (!slot.pooled) {
			if (
				this.#pooledContexts + this.#poolReservations >=
				this.config.contextPoolMaxTotal
			) {
				void this.#deleteContext(runtime, slot);
				return;
			}
			slot.pooled = true;
			this.#pooledContexts += 1;
		}
		slot.lastUsedAt = Date.now();
		runtime.cleanContexts.push(slot);
	}

	async #deleteContext(
		runtime: PreparedRuntime,
		slot: ContextSlot,
	): Promise<void> {
		if (slot.pooled) {
			slot.pooled = false;
			this.#pooledContexts = Math.max(0, this.#pooledContexts - 1);
		}
		try {
			await runtime.vm.contexts.delete(slot.id);
		} catch (error) {
			emitDynamicAppsLog({
				level: "error",
				source: "runtime",
				message: "Dynamic Apps context disposal failed",
				appId: runtime.appId,
				release: runtime.release,
				metadata: { error: errorMessage(error) },
			});
		} finally {
			runtime.contextDisposes += 1;
		}
	}

	#reservePoolContexts(requested: number): number {
		const available = Math.max(
			0,
			this.config.contextPoolMaxTotal -
				this.#pooledContexts -
				this.#poolReservations,
		);
		const reserved = Math.max(0, Math.min(requested, available));
		this.#poolReservations += reserved;
		return reserved;
	}

	async #pruneCaches(force = false, incomingBytes = 0): Promise<void> {
		if (this.#disposed) return;
		const now = Date.now();
		for (const runtime of this.#runtimes.values()) {
			const keep: ContextSlot[] = [];
			for (const slot of runtime.cleanContexts) {
				if (now - slot.lastUsedAt >= this.config.contextIdleTtlMs) {
					await this.#deleteContext(runtime, slot);
				} else {
					keep.push(slot);
				}
			}
			runtime.cleanContexts = keep;
		}
		for (const entry of [...this.#apps.values()].sort(
			(a, b) => a.lastUsedAt - b.lastUsedAt,
		)) {
			if (
				entry.refs === 0 &&
				(now - entry.lastUsedAt >= this.config.runtimeCacheIdleTtlMs ||
					this.#apps.size > this.config.runtimeCacheMaxEntries)
			) {
				this.#apps.delete(entry.appId);
				void this.#releaseEntry(entry).catch(() => {});
			}
		}
		let bytes = [...this.#runtimes.values()].reduce(
			(sum, item) => sum + item.artifactBytes,
			0,
		);
		const pressure = force && (await this.#memoryPressure());
		for (const runtime of [...this.#runtimes.values()].sort(
			(a, b) => a.lastUsedAt - b.lastUsedAt,
		)) {
			const over =
				this.#runtimes.size + (incomingBytes > 0 ? 1 : 0) >
					this.config.runtimeCacheMaxEntries ||
				bytes + incomingBytes > this.config.runtimeCacheMaxBytes ||
				pressure;
			if (
				runtime.refs === 0 &&
				(now - runtime.lastUsedAt >= this.config.runtimeCacheIdleTtlMs || over)
			) {
				this.#invalidateRuntime(runtime);
				for (const app of this.#apps.values()) {
					if (app.mapping?.runtime === runtime) app.mapping = undefined;
				}
				bytes -= runtime.artifactBytes;
			}
		}
	}

	async #releaseEntry(entry: AppCacheEntry): Promise<void> {
		try {
			const unsubscribe = entry.unsubscribe ?? (await entry.subscription);
			await unsubscribe();
		} catch {
			// A failed watcher must not prevent executor shutdown or cache eviction.
		}
	}

	async #maybeDisposeRuntime(runtime: PreparedRuntime): Promise<void> {
		if (
			!runtime.stale ||
			runtime.refs > 0 ||
			runtime.inUse > 0 ||
			runtime.refilling > 0 ||
			runtime.disposing
		)
			return;
		await this.#disposeRuntime(runtime);
	}

	async #disposeRuntime(runtime: PreparedRuntime): Promise<void> {
		if (runtime.disposePromise !== undefined) return runtime.disposePromise;
		runtime.disposing = true;
		runtime.stale = true;
		runtime.disposePromise = (async () => {
			for (const controller of runtime.activeControllers) controller.abort();
			await Promise.allSettled([...runtime.activeEvaluations]);
			await Promise.allSettled([...runtime.backgroundTasks]);
			await Promise.allSettled(
				runtime.cleanContexts
					.splice(0)
					.map((slot) => this.#deleteContext(runtime, slot)),
			);
			try {
				await runtime.vm.dispose();
				runtime.vmDisposes += 1;
			} catch (error) {
				emitDynamicAppsLog({
					level: "error",
					source: "runtime",
					message: "Dynamic Apps VM disposal failed",
					appId: runtime.appId,
					release: runtime.release,
					metadata: { error: errorMessage(error) },
				});
			}
			await rm(runtime.directory, { recursive: true, force: true }).catch(
				(error) =>
					emitDynamicAppsLog({
						level: "error",
						source: "runtime",
						message: "Dynamic Apps artifact cleanup failed",
						appId: runtime.appId,
						release: runtime.release,
						metadata: { error: errorMessage(error) },
					}),
			);
			runtime.artifact = new Uint8Array();
		})();
		return runtime.disposePromise;
	}

	#track(runtime: PreparedRuntime, task: Promise<unknown>): void {
		runtime.backgroundTasks.add(task);
		void task
			.finally(() => runtime.backgroundTasks.delete(task))
			.catch(() => {});
	}

	async #memoryPressure(): Promise<boolean> {
		const memory = readCgroupMemory();
		return memory
			? (memory.currentBytes / memory.maxBytes) * 100 >=
					this.config.memoryHighWaterPercent
			: false;
	}

	#finishTrace(headers: Headers, trace: RequestTrace): void {
		const totalMs = performance.now() - trace.startedAt;
		headers.set("x-agentos-app-release", trace.release ?? "unknown");
		headers.set(
			"x-agentos-app-cold-start",
			trace.cacheOutcome === "app-hit" ? "0" : "1",
		);
		if (this.config.timingHeaders) {
			headers.set("x-agentos-app-cache", trace.cacheOutcome);
			headers.set("x-agentos-app-execution-mode", trace.executionMode);
			for (const [name, value] of trace.phases) {
				headers.set(`x-agentos-bench-${name}-ms`, value.toFixed(2));
			}
			headers.set("x-agentos-bench-server-total-ms", totalMs.toFixed(2));
		}
		if (this.config.logRequests) {
			emitDynamicAppsLog({
				level: "info",
				source: "runtime",
				message: "Dynamic Apps request completed",
				appId: trace.appId,
				release: trace.release,
				requestId: trace.requestId,
				metadata: {
					cache: trace.cacheOutcome,
					executionMode: trace.executionMode,
					totalMs,
				},
			});
		}
	}
}

export class ApplicationHandlerError extends Error {}

function evaluationValue<T>(result: EvaluationResult<T>, timeoutMs: number): T {
	if (result.outcome === "succeeded" && result.value !== undefined) {
		return result.value;
	}
	if (result.outcome === "timed_out") {
		throw new DynamicAppsError(
			"agentos_apps_execution_timeout",
			`application execution exceeded ${timeoutMs}ms`,
		);
	}
	if (result.outcome === "cancelled") {
		throw new DynamicAppsError(
			"agentos_apps_execution_cancelled",
			"application execution was cancelled",
		);
	}
	throw new ApplicationHandlerError(
		result.error?.message ?? "application evaluation failed",
	);
}

function disposedError(): DynamicAppsError {
	return new DynamicAppsError(
		"agentos_apps_executor_disposed",
		"Dynamic Apps executor is shutting down",
	);
}

function onceUnsubscribe(unsubscribe: Unsubscribe): Unsubscribe {
	let promise: Promise<void> | undefined;
	return () => {
		promise ??= Promise.resolve().then(unsubscribe);
		return promise;
	};
}

function createReleaseLoadContext(
	trace: RequestTrace | undefined,
): ReleaseLoadContext {
	return {
		recordTiming(name, durationMs) {
			if (!Number.isFinite(durationMs) || durationMs < 0) {
				throw new DynamicAppsError(
					"agentos_apps_invalid_timing",
					"release load timing duration must be finite and non-negative",
				);
			}
			if (typeof name !== "string" || Buffer.byteLength(name) > 64) {
				throw new DynamicAppsError(
					"agentos_apps_invalid_timing",
					"release load timing name must be at most 64 bytes",
				);
			}
			const normalized = name
				.toLowerCase()
				.replace(/[^a-z0-9]+/g, "-")
				.replace(/^-|-$/g, "");
			if (!normalized) {
				throw new DynamicAppsError(
					"agentos_apps_invalid_timing",
					"release load timing name must contain ASCII letters or digits",
				);
			}
			trace?.phases.set(`store-${normalized}`, durationMs);
		},
	};
}

function verifyActiveRelease(input: ActiveRelease): ActiveRelease {
	if (!input || typeof input !== "object") {
		throw new DynamicAppsError(
			"agentos_apps_active_release_invalid",
			"loadActiveRelease returned an invalid release",
		);
	}
	validateAppId(input.appId);
	if (
		typeof input.release !== "string" ||
		Buffer.byteLength(input.release) < 1 ||
		Buffer.byteLength(input.release) > 256 ||
		/[\0-\x1f\x7f]/.test(input.release) ||
		!Array.isArray(input.regions) ||
		input.regions.length === 0 ||
		input.regions.length > 128 ||
		input.regions.some(
			(region) =>
				typeof region !== "string" || !/^[a-z0-9][a-z0-9-]{0,62}$/.test(region),
		) ||
		!validScaling(input.scaling) ||
		!Number.isSafeInteger(input.maxRequestBytes) ||
		input.maxRequestBytes < 1 ||
		!Number.isSafeInteger(input.maxResponseBytes) ||
		input.maxResponseBytes < 1
	) {
		throw new DynamicAppsError(
			"agentos_apps_active_release_invalid",
			"loadActiveRelease returned invalid release metadata",
		);
	}
	const artifact = input.artifact;
	if (
		!artifact ||
		artifact.format !== DIRECT_RUNTIME_FORMAT ||
		artifact.entrypoint !== "direct-v2/main.mjs" ||
		!/^[a-f0-9]{64}$/.test(artifact.hash) ||
		!(artifact.bytes instanceof Uint8Array) ||
		!Number.isSafeInteger(artifact.byteLength) ||
		artifact.byteLength < 1 ||
		artifact.byteLength !== artifact.bytes.byteLength ||
		typeof artifact.usesRivetKit !== "boolean"
	) {
		throw new DynamicAppsError(
			"agentos_apps_artifact_manifest_mismatch",
			"loadActiveRelease returned invalid artifact metadata",
		);
	}
	const bytes = new Uint8Array(artifact.bytes);
	if (createHash("sha256").update(bytes).digest("hex") !== artifact.hash) {
		throw new DynamicAppsError(
			"agentos_apps_artifact_hash_mismatch",
			"loaded artifact failed size or hash verification",
		);
	}
	return {
		...input,
		regions: [...input.regions],
		scaling: { ...input.scaling },
		artifact: { ...artifact, bytes },
	};
}

function validScaling(value: ActiveRelease["scaling"]): boolean {
	return (
		value !== null &&
		typeof value === "object" &&
		Number.isInteger(value.minReplicas) &&
		value.minReplicas >= 0 &&
		Number.isInteger(value.maxReplicas) &&
		value.maxReplicas >= 1 &&
		value.maxReplicas <= 128 &&
		value.minReplicas <= value.maxReplicas &&
		Number.isInteger(value.targetConcurrency) &&
		value.targetConcurrency >= 1 &&
		value.targetConcurrency <= 1_024
	);
}

async function serializeRequest(request: Request): Promise<RequestEnvelope> {
	if (Buffer.byteLength(request.url) > MAX_URL_BYTES) {
		throw new DynamicAppsError(
			"agentos_apps_request_limit",
			"request URL exceeds limit",
		);
	}
	if (
		Buffer.byteLength(request.method) > MAX_METHOD_BYTES ||
		!/^[!#$%&'*+\-.^_`|~0-9A-Za-z]+$/.test(request.method)
	) {
		throw new DynamicAppsError(
			"agentos_apps_request_limit",
			"request method is invalid or exceeds limit",
		);
	}
	const headers = new Headers(request.headers);
	const connectionTokens = (headers.get("connection") ?? "")
		.split(",")
		.map((value) => value.trim().toLowerCase())
		.filter(Boolean);
	for (const name of [...HOP_BY_HOP_HEADERS, ...connectionTokens]) {
		headers.delete(name);
	}
	for (const name of [
		"x-rivet-token",
		"x-agentos-app-region",
		"x-agentos-app-registry-dispatch",
	]) {
		headers.delete(name);
	}
	const pairs = [...headers.entries()];
	validateHeaderPairs(pairs, "request");
	let body: Uint8Array | undefined;
	if (request.method !== "GET" && request.method !== "HEAD" && request.body) {
		body = await readBoundedBody(request.body, MAX_REQUEST_BODY_BYTES);
	}
	return {
		url: request.url,
		method: request.method,
		headers: pairs,
		bodyBase64:
			body && body.byteLength > 0
				? Buffer.from(body).toString("base64")
				: undefined,
	};
}

function responseFromEnvelope(
	envelope: ResponseEnvelope,
	method: string,
): Response {
	if (
		!envelope ||
		typeof envelope !== "object" ||
		!Number.isInteger(envelope.status) ||
		envelope.status < 200 ||
		envelope.status > 599 ||
		typeof envelope.statusText !== "string" ||
		Buffer.byteLength(envelope.statusText) > MAX_RESPONSE_STATUS_TEXT_BYTES ||
		!Array.isArray(envelope.headers) ||
		typeof envelope.bodyBase64 !== "string"
	) {
		throw new DynamicAppsError(
			"agentos_apps_invalid_response",
			"application returned an invalid response envelope",
		);
	}
	validateHeaderPairs(envelope.headers, "response");
	const headers = new Headers();
	for (const [name, value] of envelope.headers) headers.append(name, value);
	for (const name of HOP_BY_HOP_HEADERS) headers.delete(name);
	const body = Buffer.from(envelope.bodyBase64, "base64");
	if (body.byteLength > MAX_RESPONSE_BODY_BYTES) {
		throw new DynamicAppsError(
			"agentos_apps_response_limit",
			"application response exceeds limit",
		);
	}
	return new Response(
		method !== "HEAD" && ![204, 205, 304].includes(envelope.status)
			? body
			: null,
		{ status: envelope.status, statusText: envelope.statusText, headers },
	);
}

function validateHeaderPairs(
	pairs: Array<[string, string]>,
	kind: "request" | "response",
): void {
	if (pairs.length > MAX_HEADER_PAIRS) {
		throw new DynamicAppsError(
			kind === "request"
				? "agentos_apps_request_limit"
				: "agentos_apps_response_header_limit",
			`${kind} headers exceed pair limit`,
		);
	}
	let bytes = 0;
	for (const pair of pairs) {
		if (
			!Array.isArray(pair) ||
			pair.length !== 2 ||
			typeof pair[0] !== "string" ||
			typeof pair[1] !== "string"
		) {
			throw new DynamicAppsError(
				"agentos_apps_invalid_response",
				`${kind} contains an invalid header pair`,
			);
		}
		bytes += Buffer.byteLength(pair[0]) + Buffer.byteLength(pair[1]);
	}
	if (bytes > MAX_HEADER_BYTES) {
		throw new DynamicAppsError(
			kind === "request"
				? "agentos_apps_request_limit"
				: "agentos_apps_response_header_limit",
			`${kind} headers exceed byte limit`,
		);
	}
}

async function readBoundedBody(
	body: ReadableStream<Uint8Array>,
	limit: number,
): Promise<Uint8Array> {
	const reader = body.getReader();
	const chunks: Uint8Array[] = [];
	let bytes = 0;
	try {
		for (;;) {
			const { value, done } = await reader.read();
			if (done) break;
			bytes += value.byteLength;
			if (bytes > limit) {
				await reader.cancel();
				throw new DynamicAppsError(
					"agentos_apps_request_limit",
					"request body exceeds limit",
				);
			}
			chunks.push(value);
		}
	} finally {
		reader.releaseLock();
	}
	return new Uint8Array(Buffer.concat(chunks, bytes));
}

async function measure<T>(
	trace: RequestTrace,
	name: string,
	operation: () => Promise<T>,
): Promise<T> {
	const startedAt = performance.now();
	try {
		return await operation();
	} finally {
		trace.phases.set(name, performance.now() - startedAt);
	}
}

function measureOptional<T>(
	trace: RequestTrace | undefined,
	name: string,
	operation: () => Promise<T>,
): Promise<T> {
	return trace ? measure(trace, name, operation) : operation();
}

function camelToKebab(value: string): string {
	return value.replace(/[A-Z]/g, (letter) => `-${letter.toLowerCase()}`);
}

function integerEnv(
	env: NodeJS.ProcessEnv,
	name: string,
	fallback: number,
	minimum: number,
	maximum: number,
): number {
	const value = Number(env[name] ?? fallback);
	if (!Number.isSafeInteger(value) || value < minimum || value > maximum) {
		throw new DynamicAppsError(
			"agentos_apps_invalid_config",
			`${name} must be an integer between ${minimum} and ${maximum}`,
		);
	}
	return value;
}

function errorMessage(error: unknown): string {
	return error instanceof Error ? error.message : String(error);
}

/** @internal Computes the safe active-context cap for a finite cgroup. */
export function capExecutionConcurrencyForMemory(input: {
	requested: number;
	contextHeapLimitMb: number;
	memoryHighWaterPercent: number;
	currentBytes: number;
	maxBytes: number;
}): number {
	return capConcurrencyForMemory({
		requested: input.requested,
		contextAndVmLimitMb: input.contextHeapLimitMb + AGENTOS_VM_OVERHEAD_MB,
		memoryHighWaterPercent: input.memoryHighWaterPercent,
		currentBytes: input.currentBytes,
		maxBytes: input.maxBytes,
	});
}

class Semaphore {
	readonly capacity: number;
	readonly #maxQueued: number;
	readonly #waitMs: number;
	#active = 0;
	#disposed = false;
	#queue: Array<{ resolve(): void; reject(error: unknown): void }> = [];

	constructor(capacity: number, maxQueued: number, waitMs: number) {
		this.capacity = capacity;
		this.#maxQueued = maxQueued;
		this.#waitMs = waitMs;
	}

	get active(): number {
		return this.#active;
	}

	get queued(): number {
		return this.#queue.length;
	}

	async acquire(): Promise<void> {
		if (this.#disposed) throw new Error("semaphore disposed");
		if (this.#active < this.capacity) {
			this.#active += 1;
			return;
		}
		if (this.#queue.length >= this.#maxQueued) {
			throw new DynamicAppsError(
				"agentos_apps_no_capacity",
				"Dynamic Apps execution queue is full",
			);
		}
		await new Promise<void>((resolve, reject) => {
			let settled = false;
			const item = {
				resolve: () => {
					if (settled) return;
					settled = true;
					clearTimeout(timer);
					this.#active += 1;
					resolve();
				},
				reject: (error: unknown) => {
					if (settled) return;
					settled = true;
					clearTimeout(timer);
					reject(error);
				},
			};
			const timer = setTimeout(() => {
				const offset = this.#queue.indexOf(item);
				if (offset >= 0) this.#queue.splice(offset, 1);
				item.reject(
					new DynamicAppsError(
						"agentos_apps_no_capacity",
						`Dynamic Apps execution queue exceeded ${this.#waitMs}ms`,
					),
				);
			}, this.#waitMs);
			this.#queue.push(item);
		});
	}

	release(): void {
		if (this.#active <= 0) return;
		this.#active -= 1;
		if (this.#active < this.capacity) this.#queue.shift()?.resolve();
	}

	dispose(): void {
		this.#disposed = true;
		for (const item of this.#queue.splice(0)) {
			item.reject(new Error("disposed"));
		}
	}
}
