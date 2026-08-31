import { createHash, randomUUID } from "node:crypto";
import { readFile } from "node:fs/promises";
import { availableParallelism } from "node:os";
import ivm from "isolated-vm";
import { createClient } from "rivetkit/client";
import type { AppRouteResolution } from "./actors.js";
import { DynamicAppsError } from "./errors.js";
import { ensurePrivateAppsRegistry } from "./registry.js";
import { DIRECT_BUNDLE_PATH, DIRECT_RUNTIME_FORMAT } from "./runtime.js";

const MAX_URL_BYTES = 16 * 1024;
const MAX_METHOD_BYTES = 256;
const MAX_HEADER_PAIRS = 256;
const MAX_HEADER_BYTES = 64 * 1024;
const MAX_RESPONSE_STATUS_TEXT_BYTES = 1024;
const MAX_REQUEST_BODY_BYTES = 1024 * 1024;
const MAX_RESPONSE_BODY_BYTES = 4 * 1024 * 1024;

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

export type IsolateMode = "fresh" | "snapshot" | "prewarm";

export interface ExecutorConfig {
	isolateMode: IsolateMode;
	isolatePoolSize: number;
	isolatePoolMaxTotal: number;
	isolateIdleTtlMs: number;
	isolateHeapLimitMb: number;
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

interface ArtifactManifest {
	format: string;
	hash: string;
	bytes: number;
	chunks: number;
	chunkBytes: number;
}

interface ReleaseActivatedEvent {
	revision: number;
	release: string;
	artifactHash: string;
	activatedAt: number;
}

interface AppConnection {
	ready: Promise<void>;
	on(
		name: string,
		callback: (event: ReleaseActivatedEvent) => void,
	): () => void;
	onOpen(callback: () => void): () => void;
	onClose(callback: () => void): () => void;
	dispose(): Promise<void>;
}

interface AppHandle {
	resolveDeployment(): Promise<AppRouteResolution>;
	getArtifactManifest(release: string): Promise<ArtifactManifest>;
	readArtifactChunk(release: string, index: number): Promise<Uint8Array>;
	connect(): AppConnection;
}

interface StateClient {
	agentOSAppsApp: { getOrCreate(key: string[]): AppHandle };
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
	startedAt: number;
	phases: Map<string, number>;
	cacheOutcome: string;
	isolateMode: IsolateMode;
	release?: string;
}

interface IsolateSlot {
	isolate: ivm.Isolate;
	pooled: boolean;
	context?: ivm.Context;
	dispatch?: ivm.Reference<(input: string) => Promise<string>>;
	lastUsedAt: number;
}

interface PreparedRuntime {
	key: string;
	release: string;
	artifactHash: string;
	artifactBytes: number;
	source: string;
	snapshot?: ivm.ExternalCopy<ArrayBuffer>;
	cleanSlots: IsolateSlot[];
	inUse: number;
	refilling: number;
	refs: number;
	stale: boolean;
	disposing: boolean;
	lastUsedAt: number;
	backgroundTasks: Set<Promise<unknown>>;
	isolateCreates: number;
	isolateDisposes: number;
	contextCreates: number;
	contextDisposes: number;
	contextResetFailures: number;
	prewarmOverflowCreates: number;
	lastContextResetError?: string;
	dispatches: number;
}

interface AppMapping {
	resolution: AppRouteResolution;
	runtime: PreparedRuntime;
}

interface AppCacheEntry {
	appId: string;
	handle: AppHandle;
	connection: AppConnection;
	ready: Promise<void>;
	mapping?: AppMapping;
	resolvePromise?: Promise<AppMapping>;
	epoch: number;
	highestRevision: number;
	lastUsedAt: number;
	refs: number;
}

export function readExecutorConfig(
	env: NodeJS.ProcessEnv = process.env,
): ExecutorConfig {
	const mode = env.DYNAMIC_APPS_ISOLATE_MODE ?? "prewarm";
	if (mode !== "fresh" && mode !== "snapshot" && mode !== "prewarm") {
		throw new DynamicAppsError(
			"agentos_apps_invalid_config",
			"DYNAMIC_APPS_ISOLATE_MODE must be fresh, snapshot, or prewarm",
		);
	}
	const poolSize = integerEnv(env, "DYNAMIC_APPS_ISOLATE_POOL_SIZE", 2, 0, 128);
	return {
		isolateMode: mode === "prewarm" && poolSize === 0 ? "snapshot" : mode,
		isolatePoolSize: poolSize,
		isolatePoolMaxTotal: integerEnv(
			env,
			"DYNAMIC_APPS_ISOLATE_POOL_MAX_TOTAL",
			8,
			0,
			1_024,
		),
		isolateIdleTtlMs: integerEnv(
			env,
			"DYNAMIC_APPS_ISOLATE_IDLE_TTL_MS",
			30_000,
			1_000,
			60 * 60_000,
		),
		isolateHeapLimitMb: integerEnv(
			env,
			"DYNAMIC_APPS_ISOLATE_HEAP_LIMIT_MB",
			64,
			8,
			2_048,
		),
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
		memoryHighWaterPercent: integerEnv(
			env,
			"DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT",
			70,
			10,
			95,
		),
		executionConcurrency: integerEnv(
			env,
			"DYNAMIC_APPS_EXECUTION_CONCURRENCY",
			Math.max(1, availableParallelism()),
			1,
			1_024,
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

export class DynamicAppsExecutor {
	readonly config: ExecutorConfig;
	readonly #client: StateClient;
	readonly #ensureRegistry: boolean;
	readonly #semaphore: Semaphore;
	readonly #apps = new Map<string, AppCacheEntry>();
	readonly #runtimes = new Map<string, PreparedRuntime>();
	readonly #runtimePromises = new Map<string, Promise<PreparedRuntime>>();
	readonly #cleanupTimer: ReturnType<typeof setInterval>;
	#pooledIsolates = 0;
	#poolReservations = 0;
	#disposed = false;
	#disposePromise?: Promise<void>;

	constructor(
		config: ExecutorConfig = readExecutorConfig(),
		client?: StateClient,
	) {
		this.config = config;
		this.#ensureRegistry = client === undefined;
		this.#client = client ?? (createClient() as unknown as StateClient);
		this.#semaphore = new Semaphore(
			config.executionConcurrency,
			config.executionQueueSize,
			config.executionQueueWaitMs,
		);
		this.#cleanupTimer = setInterval(
			() => void this.#pruneCaches(true),
			Math.min(config.isolateIdleTtlMs, config.runtimeCacheIdleTtlMs, 30_000),
		);
		this.#cleanupTimer.unref?.();
	}

	async request(appId: string, request: Request): Promise<Response> {
		if (this.#disposed) {
			throw new DynamicAppsError(
				"agentos_apps_executor_disposed",
				"Dynamic Apps executor is shutting down",
			);
		}
		const trace: RequestTrace = {
			startedAt: performance.now(),
			phases: new Map(),
			cacheOutcome: "app-hit",
			isolateMode: this.config.isolateMode,
		};
		let admitted = false;
		try {
			await measure(trace, "execution-queue", () => this.#semaphore.acquire());
			admitted = true;
			const envelope = await measure(trace, "request-buffer", () =>
				serializeRequest(request),
			);
			if (this.#ensureRegistry) {
				await measure(trace, "registry-ready", ensurePrivateAppsRegistry);
			}
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
			runtimes: runtimes.length,
			artifactBytes: runtimes.reduce(
				(sum, item) => sum + item.artifactBytes,
				0,
			),
			activeEvaluations: this.#semaphore.active,
			queuedEvaluations: this.#semaphore.queued,
			cleanIsolates: runtimes.reduce(
				(sum, item) => sum + item.cleanSlots.length,
				0,
			),
			pooledIsolates: this.#pooledIsolates,
			poolReservations: this.#poolReservations,
			isolatePoolMaxTotal: this.config.isolatePoolMaxTotal,
			inUseIsolates: runtimes.reduce((sum, item) => sum + item.inUse, 0),
			refillingIsolates: runtimes.reduce(
				(sum, item) => sum + item.refilling,
				0,
			),
			rssBytes: process.memoryUsage().rss,
			isolatedVmExternalBytes: ivm.ExternalCopy.totalExternalSize,
			isolateCreates: runtimes.reduce(
				(sum, item) => sum + item.isolateCreates,
				0,
			),
			isolateDisposes: runtimes.reduce(
				(sum, item) => sum + item.isolateDisposes,
				0,
			),
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
			prewarmOverflowCreates: runtimes.reduce(
				(sum, item) => sum + item.prewarmOverflowCreates,
				0,
			),
			lastContextResetError: runtimes
				.filter((item) => item.lastContextResetError !== undefined)
				.at(-1)?.lastContextResetError,
			dispatches: runtimes.reduce((sum, item) => sum + item.dispatches, 0),
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
		await Promise.allSettled(
			[...this.#apps.values()].map((entry) => entry.connection.dispose()),
		);
		this.#apps.clear();
		await Promise.allSettled([...this.#runtimePromises.values()]);
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
		const handle = this.#client.agentOSAppsApp.getOrCreate([appId]);
		const connection = handle.connect();
		const entry: AppCacheEntry = {
			appId,
			handle,
			connection,
			ready: connection.ready,
			epoch: 0,
			highestRevision: 0,
			lastUsedAt: Date.now(),
			refs: 0,
		};
		connection.on("releaseActivated", (event) => {
			if (!validReleaseEvent(event) || event.revision <= entry.highestRevision)
				return;
			entry.highestRevision = event.revision;
			entry.epoch += 1;
			entry.mapping = undefined;
			void this.#resolveAndPrepare(entry).catch(() => {});
		});
		connection.onClose(() => {
			entry.epoch += 1;
			entry.mapping = undefined;
		});
		connection.onOpen(() => {
			if (entry.mapping || entry.highestRevision > 0) {
				entry.epoch += 1;
				entry.mapping = undefined;
				void this.#resolveAndPrepare(entry).catch(() => {});
			}
		});
		this.#apps.set(appId, entry);
		return { entry, hit: false };
	}

	async #resolveAndPrepare(
		entry: AppCacheEntry,
		trace?: RequestTrace,
	): Promise<AppMapping> {
		if (entry.mapping) return entry.mapping;
		if (entry.resolvePromise !== undefined) return entry.resolvePromise;
		const promise = (async () => {
			for (;;) {
				const epoch = entry.epoch;
				await measureOptional(trace, "actor-connect", () => entry.ready);
				const resolution = await measureOptional(trace, "actor-resolve", () =>
					entry.handle.resolveDeployment(),
				);
				if (
					entry.epoch !== epoch ||
					resolution.revision < entry.highestRevision
				)
					continue;
				entry.highestRevision = Math.max(
					entry.highestRevision,
					resolution.revision,
				);
				const runtime = await this.#prepareRuntime(
					entry.handle,
					resolution,
					trace,
				);
				if (
					entry.epoch !== epoch ||
					resolution.revision < entry.highestRevision
				)
					continue;
				const mapping = { resolution, runtime };
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
		handle: AppHandle,
		resolution: AppRouteResolution,
		trace?: RequestTrace,
	): Promise<PreparedRuntime> {
		if (this.#disposed) {
			throw new DynamicAppsError(
				"agentos_apps_executor_disposed",
				"Dynamic Apps executor is shutting down",
			);
		}
		const key = `${resolution.artifactHash}:${DIRECT_RUNTIME_FORMAT}`;
		const existing = this.#runtimes.get(key);
		if (existing && !existing.stale) {
			existing.lastUsedAt = Date.now();
			return existing;
		}
		const pending = this.#runtimePromises.get(key);
		if (pending) return pending;
		const promise = this.#createRuntime(key, handle, resolution, trace);
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
		handle: AppHandle,
		resolution: AppRouteResolution,
		trace?: RequestTrace,
	): Promise<PreparedRuntime> {
		await this.#pruneCaches(true, resolution.artifactBytes);
		if (resolution.artifactBytes > this.config.runtimeCacheMaxBytes) {
			throw new DynamicAppsError(
				"agentos_apps_artifact_cache_limit",
				"application artifact is larger than the configured runtime cache",
			);
		}
		const manifest = await measureOptional(trace, "artifact-manifest", () =>
			handle.getArtifactManifest(resolution.release),
		);
		validateManifest(manifest, resolution);
		const artifact = await measureOptional(
			trace,
			"artifact-download",
			async () => {
				const chunks: Uint8Array[] = [];
				const digest = createHash("sha256");
				let bytes = 0;
				for (let index = 0; index < manifest.chunks; index += 1) {
					const chunk = new Uint8Array(
						await handle.readArtifactChunk(resolution.release, index),
					);
					bytes += chunk.byteLength;
					if (
						chunk.byteLength > manifest.chunkBytes ||
						bytes > manifest.bytes
					) {
						throw new DynamicAppsError(
							"agentos_apps_artifact_chunk_invalid",
							`artifact chunk ${index} has an invalid length`,
						);
					}
					digest.update(chunk);
					chunks.push(chunk);
				}
				if (
					bytes !== manifest.bytes ||
					digest.digest("hex") !== manifest.hash
				) {
					throw new DynamicAppsError(
						"agentos_apps_artifact_hash_mismatch",
						"downloaded artifact failed size or hash verification",
					);
				}
				return new Uint8Array(Buffer.concat(chunks, bytes));
			},
		);
		const source = await measureOptional(trace, "artifact-parse", async () =>
			extractAospkgTextFile(artifact, DIRECT_BUNDLE_PATH),
		);
		const runtime: PreparedRuntime = {
			key,
			release: resolution.release,
			artifactHash: resolution.artifactHash,
			artifactBytes: resolution.artifactBytes,
			source,
			cleanSlots: [],
			inUse: 0,
			refilling: 0,
			refs: 0,
			stale: false,
			disposing: false,
			lastUsedAt: Date.now(),
			backgroundTasks: new Set(),
			isolateCreates: 0,
			isolateDisposes: 0,
			contextCreates: 0,
			contextDisposes: 0,
			contextResetFailures: 0,
			prewarmOverflowCreates: 0,
			dispatches: 0,
		};
		if (this.config.isolateMode !== "fresh") {
			runtime.snapshot = await measureOptional(
				trace,
				"snapshot-create",
				async () =>
					ivm.Isolate.createSnapshot([
						{
							code: ISOLATE_BOOTSTRAP_SOURCE,
							filename: "dynamic-apps:bootstrap",
						},
						{ code: source, filename: "dynamic-apps:application" },
					]),
			);
		}
		this.#runtimes.set(key, runtime);
		try {
			if (this.#disposed) {
				throw new DynamicAppsError(
					"agentos_apps_executor_disposed",
					"Dynamic Apps executor is shutting down",
				);
			}
			if (this.config.isolateMode === "prewarm") {
				await measureOptional(trace, "isolate-prewarm", () =>
					this.#fillPool(runtime),
				);
			}
			if (this.#disposed) {
				throw new DynamicAppsError(
					"agentos_apps_executor_disposed",
					"Dynamic Apps executor is shutting down",
				);
			}
			return runtime;
		} catch (error) {
			runtime.stale = true;
			this.#runtimes.delete(key);
			await this.#disposeRuntime(runtime);
			throw error;
		}
	}

	async #execute(
		runtime: PreparedRuntime,
		envelope: RequestEnvelope,
		trace: RequestTrace,
	): Promise<Response> {
		let slot: IsolateSlot | undefined;
		let prewarmActive = false;
		let completedSuccessfully = false;
		try {
			if (this.config.isolateMode === "prewarm") {
				slot = await measure(trace, "isolate-lease", () =>
					this.#acquireSlot(runtime),
				);
				prewarmActive = true;
			}
			if (!slot) {
				if (prewarmActive) runtime.prewarmOverflowCreates += 1;
				slot = await measure(trace, "isolate-create", () =>
					this.#createSlot(runtime),
				);
			}
			const executionSlot = slot;
			const output = await measure(trace, "evaluation", () =>
				this.#dispatch(executionSlot, envelope),
			);
			runtime.dispatches += 1;
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
			if (slot) {
				const startedAt = performance.now();
				if (prewarmActive && completedSuccessfully) {
					this.#releaseSlotContext(runtime, slot);
				} else {
					this.#disposeSlot(runtime, slot);
				}
				trace.phases.set(
					prewarmActive ? "context-destroy" : "isolate-destroy",
					performance.now() - startedAt,
				);
			}
			if (prewarmActive) {
				const shouldCache =
					slot !== undefined &&
					completedSuccessfully &&
					!runtime.stale &&
					!this.#disposed &&
					runtime.cleanSlots.length + runtime.inUse - 1 + runtime.refilling <
						this.config.isolatePoolSize;
				if (shouldCache && slot) {
					const startedAt = performance.now();
					try {
						await this.#initializeSlot(runtime, slot);
						this.#offerSlot(runtime, slot);
					} catch (error) {
						runtime.contextResetFailures += 1;
						runtime.lastContextResetError =
							error instanceof Error ? error.message : String(error);
						this.#disposeSlot(runtime, slot);
					}
					trace.phases.set("context-reset", performance.now() - startedAt);
				} else if (slot && completedSuccessfully) {
					this.#disposeSlot(runtime, slot);
				}
				runtime.inUse -= 1;
				this.#ensurePool(runtime);
			}
		}
	}

	async #dispatch(
		slot: IsolateSlot,
		envelope: RequestEnvelope,
	): Promise<ResponseEnvelope> {
		try {
			if (!slot.dispatch)
				throw new Error("application isolate has no active dispatcher");
			const output = await withDeadline(
				slot.dispatch.apply(undefined, [JSON.stringify(envelope)], {
					arguments: { copy: true },
					result: { copy: true, promise: true },
					timeout: this.config.executionTimeoutMs,
				}),
				this.config.executionTimeoutMs,
			);
			if (typeof output !== "string")
				throw new Error("dispatcher returned non-text");
			return JSON.parse(output) as ResponseEnvelope;
		} catch (error) {
			if (error instanceof Error && /timed out/i.test(error.message)) {
				throw new DynamicAppsError(
					"agentos_apps_execution_timeout",
					`application execution exceeded ${this.config.executionTimeoutMs}ms`,
				);
			}
			throw new ApplicationHandlerError(
				error instanceof Error ? error.message : String(error),
			);
		}
	}

	async #createSlot(runtime: PreparedRuntime): Promise<IsolateSlot> {
		const isolate = new ivm.Isolate({
			memoryLimit: this.config.isolateHeapLimitMb,
			...(runtime.snapshot ? { snapshot: runtime.snapshot } : {}),
		});
		const slot: IsolateSlot = {
			isolate,
			pooled: false,
			lastUsedAt: Date.now(),
		};
		try {
			await this.#initializeSlot(runtime, slot);
			runtime.isolateCreates += 1;
			return slot;
		} catch (error) {
			isolate.dispose();
			throw error;
		}
	}

	async #initializeSlot(
		runtime: PreparedRuntime,
		slot: IsolateSlot,
	): Promise<void> {
		const context = await slot.isolate.createContext();
		try {
			if (!runtime.snapshot) {
				const script = await slot.isolate.compileScript(
					`${ISOLATE_BOOTSTRAP_SOURCE}\n${runtime.source}`,
					{ filename: "dynamic-apps:application" },
				);
				await script.run(context, { timeout: this.config.executionTimeoutMs });
			}
			const dispatch = await context.global.get("__dynamicAppDispatch", {
				reference: true,
			});
			if (!(dispatch instanceof ivm.Reference)) {
				throw new Error("application bundle did not install a dispatcher");
			}
			slot.context = context;
			slot.dispatch = dispatch as NonNullable<IsolateSlot["dispatch"]>;
			slot.lastUsedAt = Date.now();
			runtime.contextCreates += 1;
		} catch (error) {
			context.release();
			throw error;
		}
	}

	async #fillPool(runtime: PreparedRuntime): Promise<void> {
		if (await this.#memoryPressure()) return;
		const reserved = this.#reservePoolSlots(this.config.isolatePoolSize);
		const outcomes = await Promise.allSettled(
			Array.from({ length: reserved }, () => this.#createSlot(runtime)),
		);
		const failed = outcomes.find(
			(outcome): outcome is PromiseRejectedResult =>
				outcome.status === "rejected",
		);
		if (failed) {
			for (const outcome of outcomes) {
				this.#poolReservations -= 1;
				if (outcome.status === "fulfilled") {
					this.#disposeSlot(runtime, outcome.value);
				}
			}
			throw failed.reason;
		}
		for (const outcome of outcomes) {
			this.#poolReservations -= 1;
			if (outcome.status === "fulfilled")
				this.#offerSlot(runtime, outcome.value);
		}
	}

	#ensurePool(runtime: PreparedRuntime): void {
		if (
			this.config.isolateMode !== "prewarm" ||
			runtime.stale ||
			this.#disposed
		)
			return;
		const missingForRuntime =
			this.config.isolatePoolSize -
			(runtime.cleanSlots.length + runtime.inUse + runtime.refilling);
		const missing = this.#reservePoolSlots(missingForRuntime);
		for (let index = 0; index < missing; index += 1) {
			runtime.refilling += 1;
			let reserved = true;
			const task = this.#memoryPressure()
				.then(async (pressure) => {
					if (pressure || runtime.stale) return;
					const slot = await this.#createSlot(runtime);
					this.#poolReservations -= 1;
					reserved = false;
					this.#offerSlot(runtime, slot);
				})
				.catch(() => {})
				.finally(() => {
					if (reserved) this.#poolReservations -= 1;
					runtime.refilling -= 1;
					void this.#maybeDisposeRuntime(runtime);
				});
			this.#track(runtime, task);
		}
	}

	async #acquireSlot(
		runtime: PreparedRuntime,
	): Promise<IsolateSlot | undefined> {
		const slot = runtime.cleanSlots.shift();
		// Account for the lease before this async method resolves. Otherwise a
		// concurrent refill can observe the slot missing from both cleanSlots and
		// inUse and overfill the bounded pool.
		runtime.inUse += 1;
		if (slot) {
			slot.lastUsedAt = Date.now();
			return slot;
		}
		return undefined;
	}

	#offerSlot(runtime: PreparedRuntime, slot: IsolateSlot): void {
		if (runtime.stale || this.#disposed) {
			this.#disposeSlot(runtime, slot);
			return;
		}
		if (!slot.pooled) {
			if (
				this.#pooledIsolates + this.#poolReservations >=
				this.config.isolatePoolMaxTotal
			) {
				this.#disposeSlot(runtime, slot);
				return;
			}
			slot.pooled = true;
			this.#pooledIsolates += 1;
		}
		runtime.cleanSlots.push(slot);
	}

	#disposeSlot(runtime: PreparedRuntime, slot: IsolateSlot): void {
		if (slot.pooled) {
			slot.pooled = false;
			this.#pooledIsolates = Math.max(0, this.#pooledIsolates - 1);
		}
		this.#releaseSlotContext(runtime, slot);
		try {
			slot.isolate.dispose();
		} catch {}
		runtime.isolateDisposes += 1;
	}

	#releaseSlotContext(runtime: PreparedRuntime, slot: IsolateSlot): void {
		try {
			slot.dispatch?.release();
		} catch {}
		try {
			slot.context?.release();
		} catch {}
		if (slot.dispatch || slot.context) runtime.contextDisposes += 1;
		slot.dispatch = undefined;
		slot.context = undefined;
	}

	#reservePoolSlots(requested: number): number {
		const available = Math.max(
			0,
			this.config.isolatePoolMaxTotal -
				this.#pooledIsolates -
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
			const keep: IsolateSlot[] = [];
			for (const slot of runtime.cleanSlots) {
				if (now - slot.lastUsedAt >= this.config.isolateIdleTtlMs) {
					this.#disposeSlot(runtime, slot);
				} else {
					keep.push(slot);
				}
			}
			runtime.cleanSlots = keep;
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
				void entry.connection.dispose();
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
				runtime.stale = true;
				this.#runtimes.delete(runtime.key);
				for (const app of this.#apps.values()) {
					if (app.mapping?.runtime === runtime) app.mapping = undefined;
				}
				bytes -= runtime.artifactBytes;
				void this.#maybeDisposeRuntime(runtime);
			}
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
		if (runtime.disposing) return;
		runtime.disposing = true;
		await Promise.allSettled([...runtime.backgroundTasks]);
		for (const slot of runtime.cleanSlots.splice(0))
			this.#disposeSlot(runtime, slot);
		runtime.snapshot?.release();
	}

	#track(runtime: PreparedRuntime, task: Promise<unknown>): void {
		runtime.backgroundTasks.add(task);
		void task
			.finally(() => runtime.backgroundTasks.delete(task))
			.catch(() => {});
	}

	async #memoryPressure(): Promise<boolean> {
		try {
			const [currentText, maxText] = await Promise.all([
				readFile("/sys/fs/cgroup/memory.current", "utf8"),
				readFile("/sys/fs/cgroup/memory.max", "utf8"),
			]);
			if (maxText.trim() === "max") return false;
			const current = Number(currentText.trim());
			const maximum = Number(maxText.trim());
			return (
				Number.isFinite(current) &&
				Number.isFinite(maximum) &&
				maximum > 0 &&
				(current / maximum) * 100 >= this.config.memoryHighWaterPercent
			);
		} catch {
			return false;
		}
	}

	#finishTrace(headers: Headers, trace: RequestTrace): void {
		const total = performance.now() - trace.startedAt;
		headers.set("x-agentos-app-release", trace.release ?? "unknown");
		headers.set(
			"x-agentos-app-cold-start",
			trace.cacheOutcome === "app-hit" ? "0" : "1",
		);
		if (this.config.timingHeaders) {
			headers.set("x-agentos-app-cache", trace.cacheOutcome);
			headers.set("x-agentos-app-isolate-mode", trace.isolateMode);
			for (const [name, value] of trace.phases) {
				headers.set(`x-agentos-bench-${name}-ms`, value.toFixed(2));
			}
			headers.set("x-agentos-bench-server-total-ms", total.toFixed(2));
		}
		if (this.config.logRequests) {
			console.log(
				JSON.stringify({
					event: "dynamic_apps_request",
					requestId: randomUUID(),
					release: trace.release,
					cache: trace.cacheOutcome,
					isolateMode: trace.isolateMode,
					totalMs: total,
					phases: Object.fromEntries(trace.phases),
				}),
			);
		}
	}
}

export class ApplicationHandlerError extends Error {}

function validReleaseEvent(event: unknown): event is ReleaseActivatedEvent {
	if (!event || typeof event !== "object") return false;
	const value = event as Partial<ReleaseActivatedEvent>;
	return (
		Number.isInteger(value.revision) &&
		typeof value.release === "string" &&
		typeof value.artifactHash === "string" &&
		typeof value.activatedAt === "number"
	);
}

function validateManifest(
	manifest: ArtifactManifest,
	resolution: AppRouteResolution,
): void {
	if (
		manifest.format !== DIRECT_RUNTIME_FORMAT ||
		manifest.hash !== resolution.artifactHash ||
		manifest.bytes !== resolution.artifactBytes ||
		!Number.isInteger(manifest.chunks) ||
		manifest.chunks <= 0 ||
		manifest.chunks > 128 ||
		!Number.isInteger(manifest.chunkBytes) ||
		manifest.chunkBytes <= 0 ||
		!/^[a-f0-9]{64}$/.test(manifest.hash)
	) {
		throw new DynamicAppsError(
			"agentos_apps_artifact_manifest_mismatch",
			"app actor returned an invalid artifact manifest",
		);
	}
}

function extractAospkgTextFile(bytes: Uint8Array, target: string): string {
	const buffer = Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
	if (
		buffer.byteLength < 16 ||
		buffer[0] !== 137 ||
		buffer.subarray(1, 4).toString("ascii") !== "AOS"
	) {
		throw new DynamicAppsError(
			"agentos_apps_artifact_format_invalid",
			"application artifact is not an AOSP package",
		);
	}
	let offset = 16 + buffer.readUInt32LE(8) + buffer.readUInt32LE(12);
	while (offset + 512 <= buffer.byteLength) {
		const header = buffer.subarray(offset, offset + 512);
		if (header.every((value) => value === 0)) break;
		const name = tarString(header.subarray(0, 100));
		const prefix = tarString(header.subarray(345, 500));
		const path = `${prefix ? `${prefix}/` : ""}${name}`.replace(/^\.\//, "");
		const sizeText = tarString(header.subarray(124, 136)).trim();
		const size = Number.parseInt(sizeText || "0", 8);
		if (!Number.isSafeInteger(size) || size < 0) break;
		const dataOffset = offset + 512;
		const next = dataOffset + Math.ceil(size / 512) * 512;
		if (next > buffer.byteLength) break;
		if (path === target || path === `/${target}`) {
			return new TextDecoder("utf-8", { fatal: true }).decode(
				buffer.subarray(dataOffset, dataOffset + size),
			);
		}
		offset = next;
	}
	throw new DynamicAppsError(
		"agentos_apps_artifact_entry_missing",
		`application artifact is missing ${target}`,
	);
}

function tarString(bytes: Uint8Array): string {
	const end = bytes.indexOf(0);
	return Buffer.from(end < 0 ? bytes : bytes.subarray(0, end)).toString("utf8");
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
	for (const name of [...HOP_BY_HOP_HEADERS, ...connectionTokens])
		headers.delete(name);
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

async function withDeadline<T>(operation: Promise<T>, timeoutMs: number) {
	let timer: ReturnType<typeof setTimeout> | undefined;
	try {
		return await Promise.race([
			operation,
			new Promise<never>((_, reject) => {
				timer = setTimeout(
					() => reject(new Error("isolate promise timed out")),
					timeoutMs,
				);
			}),
		]);
	} finally {
		if (timer) clearTimeout(timer);
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
		await this.#acquire();
	}

	release(): void {
		if (this.#active <= 0) return;
		this.#active -= 1;
		this.#wake();
	}

	dispose(): void {
		this.#disposed = true;
		for (const item of this.#queue.splice(0))
			item.reject(new Error("disposed"));
	}

	async #acquire(): Promise<void> {
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

	#wake(): void {
		if (this.#active >= this.capacity) return;
		this.#queue.shift()?.resolve();
	}
}

const ISOLATE_BOOTSTRAP_SOURCE = String.raw`
(() => {
  const utf8Encode = (text) => {
    const escaped = unescape(encodeURIComponent(String(text)));
    const bytes = new Uint8Array(escaped.length);
    for (let i = 0; i < escaped.length; i++) bytes[i] = escaped.charCodeAt(i);
    return bytes;
  };
  const utf8Decode = (bytes) => {
    let binary = "";
    for (let i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
    return decodeURIComponent(escape(binary));
  };
  const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  globalThis.__dynamicAppsBase64Encode = (bytes) => {
		const chunks = [];
		let out = "";
    for (let i = 0; i < bytes.length; i += 3) {
      const n = (bytes[i] << 16) | ((bytes[i + 1] ?? 0) << 8) | (bytes[i + 2] ?? 0);
      out += alphabet[(n >> 18) & 63] + alphabet[(n >> 12) & 63] +
        (i + 1 < bytes.length ? alphabet[(n >> 6) & 63] : "=") +
        (i + 2 < bytes.length ? alphabet[n & 63] : "=");
			if (out.length >= 16384) { chunks.push(out); out = ""; }
    }
		chunks.push(out);
		return chunks.join("");
  };
  globalThis.__dynamicAppsBase64Decode = (text) => {
    const clean = String(text).replace(/=+$/, "");
		const out = new Uint8Array(Math.floor(clean.length * 3 / 4));
		let offset = 0;
    let bits = 0, count = 0;
    for (const char of clean) {
      const value = alphabet.indexOf(char);
      if (value < 0) continue;
      bits = (bits << 6) | value;
      count += 6;
			if (count >= 8) {
				count -= 8;
				out[offset++] = (bits >> count) & 255;
				bits &= count === 0 ? 0 : (1 << count) - 1;
			}
    }
		return offset === out.length ? out : out.slice(0, offset);
  };
  class TextEncoder { encode(value = "") { return utf8Encode(value); } }
  class TextDecoder { decode(value = new Uint8Array()) { return utf8Decode(new Uint8Array(value)); } }
  class Headers {
    constructor(init) {
      this._items = [];
      if (init instanceof Headers) init = init._items;
      if (Array.isArray(init)) for (const pair of init) this.append(pair[0], pair[1]);
      else if (init) for (const key of Object.keys(init)) this.append(key, init[key]);
    }
    append(name, value) { this._items.push([String(name).toLowerCase(), String(value)]); }
    set(name, value) { this.delete(name); this.append(name, value); }
    get(name) { const values = this._items.filter(x => x[0] === String(name).toLowerCase()).map(x => x[1]); return values.length ? values.join(", ") : null; }
    has(name) { return this._items.some(x => x[0] === String(name).toLowerCase()); }
    delete(name) { name = String(name).toLowerCase(); this._items = this._items.filter(x => x[0] !== name); }
    forEach(fn, self) { for (const [name, value] of this.entries()) fn.call(self, value, name, this); }
    *entries() { const seen = new Set(); for (const [name] of this._items) if (!seen.has(name)) { seen.add(name); yield [name, this.get(name)]; } }
    *keys() { for (const [name] of this.entries()) yield name; }
    *values() { for (const [, value] of this.entries()) yield value; }
    [Symbol.iterator]() { return this.entries(); }
    getSetCookie() { return this._items.filter(x => x[0] === "set-cookie").map(x => x[1]); }
  }
  const bodyBytes = (body) => body == null ? new Uint8Array() : body instanceof Uint8Array ? body.slice() : body instanceof ArrayBuffer ? new Uint8Array(body.slice(0)) : utf8Encode(body);
  class Body {
    constructor(body) { this._body = bodyBytes(body); this.bodyUsed = false; }
    async arrayBuffer() { this.bodyUsed = true; return this._body.slice().buffer; }
    async text() { this.bodyUsed = true; return utf8Decode(this._body); }
    async json() { return JSON.parse(await this.text()); }
  }
  class Request extends Body {
    constructor(input, init = {}) {
      const prior = input instanceof Request ? input : null;
      super(init.body ?? prior?._body);
      this.url = prior ? prior.url : String(input);
      this.method = String(init.method ?? prior?.method ?? "GET").toUpperCase();
      this.headers = new Headers(init.headers ?? prior?.headers);
    }
    clone() { return new Request(this); }
  }
  const statusText = { 200: "OK", 201: "Created", 204: "No Content", 301: "Moved Permanently", 302: "Found", 304: "Not Modified", 400: "Bad Request", 404: "Not Found", 500: "Internal Server Error" };
  class Response extends Body {
    constructor(body = null, init = {}) {
      super(body);
      this.status = Number(init.status ?? 200);
      this.statusText = String(init.statusText ?? statusText[this.status] ?? "");
      this.headers = new Headers(init.headers);
      this.ok = this.status >= 200 && this.status < 300;
    }
    clone() { return new Response(this._body, { status: this.status, statusText: this.statusText, headers: this.headers }); }
    static json(value, init = {}) { const headers = new Headers(init.headers); if (!headers.has("content-type")) headers.set("content-type", "application/json"); return new Response(JSON.stringify(value), { ...init, headers }); }
    static redirect(url, status = 302) { return new Response(null, { status, headers: { location: String(url) } }); }
  }
  class URLSearchParams {
    constructor(input = "") { this._items = []; for (const item of String(input).replace(/^\?/, "").split("&")) { if (!item) continue; const [key, ...rest] = item.split("="); this.append(decodeURIComponent(key.replace(/\+/g, " ")), decodeURIComponent(rest.join("=").replace(/\+/g, " "))); } }
    append(key, value) { this._items.push([String(key), String(value)]); }
    get(key) { const item = this._items.find(x => x[0] === String(key)); return item ? item[1] : null; }
    getAll(key) { return this._items.filter(x => x[0] === String(key)).map(x => x[1]); }
    has(key) { return this._items.some(x => x[0] === String(key)); }
    set(key, value) { this.delete(key); this.append(key, value); }
    delete(key) { key = String(key); this._items = this._items.filter(x => x[0] !== key); }
    entries() { return this._items[Symbol.iterator](); }
    [Symbol.iterator]() { return this.entries(); }
    toString() { return this._items.map(x => encodeURIComponent(x[0]).replace(/%20/g, "+") + "=" + encodeURIComponent(x[1]).replace(/%20/g, "+")).join("&"); }
  }
  class URL {
    constructor(input, base) {
      input = String(input);
      if (base && !/^[a-z][a-z0-9+.-]*:/i.test(input)) input = String(base).replace(/\/[^/]*$/, "/") + input;
      const match = /^([a-z][a-z0-9+.-]*:)(?:\/\/([^/?#]*))?([^?#]*)(\?[^#]*)?(#.*)?$/i.exec(input);
      if (!match) throw new TypeError("Invalid URL");
      this.protocol = match[1]; this.host = match[2] ?? ""; this.hostname = this.host.split(":")[0];
      this.pathname = match[3] || "/"; this.search = match[4] ?? ""; this.hash = match[5] ?? "";
      this.origin = this.host ? this.protocol + "//" + this.host : "null";
      this.searchParams = new URLSearchParams(this.search);
    }
    toString() { const query = this.searchParams.toString(); return (this.host ? this.protocol + "//" + this.host : this.protocol) + this.pathname + (query ? "?" + query : "") + this.hash; }
    get href() { return this.toString(); }
  }
  globalThis.TextEncoder = TextEncoder; globalThis.TextDecoder = TextDecoder;
  globalThis.Headers = Headers; globalThis.Request = Request; globalThis.Response = Response;
  globalThis.URL = URL; globalThis.URLSearchParams = URLSearchParams;
  globalThis.performance = { now: () => Date.now() };
})();`;

let defaultExecutor: DynamicAppsExecutor | undefined;

export function getDefaultExecutor(): DynamicAppsExecutor {
	defaultExecutor ??= new DynamicAppsExecutor();
	return defaultExecutor;
}

export async function resetDefaultExecutorForTest(): Promise<void> {
	await defaultExecutor?.dispose();
	defaultExecutor = undefined;
}
