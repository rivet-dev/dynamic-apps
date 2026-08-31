import { createHash } from "node:crypto";
import {
	mkdir,
	mkdtemp,
	readFile,
	rm,
	symlink,
	writeFile,
} from "node:fs/promises";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { dirname, join, posix } from "node:path";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";
import { DynamicAppsError } from "./errors.js";
import { capConcurrencyForMemory, readCgroupMemory } from "./memory.js";
import { ACTOR_BUNDLE_PATH } from "./runtime.js";

const MAX_ACTOR_FILES = 4_096;
const MAX_ACTOR_FILE_BYTES = 32 * 1024 * 1024;
const MAX_ACTOR_ARTIFACT_BYTES = 64 * 1024 * 1024;

interface ActorRuntimeConfig {
	maxEntries: number;
	heapLimitMb: number;
	startTimeoutMs: number;
	idleTtlMs: number;
	maxStartPayloadBytes: number;
	memoryHighWaterPercent: number;
	requestConcurrency: number;
	requestQueueSize: number;
	requestQueueWaitMs: number;
	requestTimeoutMs: number;
}

export interface ActorRuntimeRequest {
	key: string;
	loadArtifact: () => Promise<Uint8Array>;
	endpoint: string;
	namespace: string;
	pool: string;
	request: Request;
}

interface PendingRequest {
	resolve(response: Response): void;
	reject(error: unknown): void;
	controller?: ReadableStreamDefaultController<Uint8Array>;
	waitingAck: boolean;
	settled: boolean;
	abort?: () => void;
	releaseAdmission(): void;
	timeout?: ReturnType<typeof setTimeout>;
}

interface RuntimeEntry {
	key: string;
	directory: string;
	worker: Worker;
	ready: Promise<void>;
	pending: Map<string, PendingRequest>;
	active: number;
	lastUsedAt: number;
	disposed: boolean;
	nextRequestId: number;
}

type WorkerMessage =
	| { type: "ready" }
	| { type: "head"; id: string; status: number; headers: [string, string][] }
	| { type: "chunk"; id: string; chunk: Uint8Array }
	| { type: "end"; id: string }
	| { type: "error"; id?: string; message: string };

export class DynamicActorRuntime {
	readonly config: ActorRuntimeConfig;
	readonly #entries = new Map<string, RuntimeEntry>();
	readonly #creating = new Map<string, Promise<RuntimeEntry>>();
	readonly #admission: ActorAdmission;
	readonly #timer: ReturnType<typeof setInterval>;
	#workerReservations = 0;
	#disposed = false;
	#disposePromise?: Promise<void>;

	constructor(env: NodeJS.ProcessEnv = process.env) {
		const requestedMaxEntries = integerEnv(
			env,
			"DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES",
			4,
			1,
			1_024,
		);
		const heapLimitMb = integerEnv(
			env,
			"DYNAMIC_APPS_ACTOR_WORKER_HEAP_LIMIT_MB",
			96,
			16,
			2_048,
		);
		const memoryHighWaterPercent = integerEnv(
			env,
			"DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT",
			70,
			10,
			95,
		);
		const cgroupMemory = readCgroupMemory();
		this.config = {
			maxEntries: cgroupMemory
				? capConcurrencyForMemory({
						requested: requestedMaxEntries,
						heapLimitMb,
						memoryHighWaterPercent,
						currentBytes: cgroupMemory.currentBytes,
						maxBytes: cgroupMemory.maxBytes,
					})
				: requestedMaxEntries,
			heapLimitMb,
			startTimeoutMs: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS",
				10_000,
				10,
				5 * 60_000,
			),
			idleTtlMs: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_WORKER_IDLE_TTL_MS",
				30_000,
				1_000,
				60 * 60_000,
			),
			maxStartPayloadBytes: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES",
				1024 * 1024,
				1,
				16 * 1024 * 1024,
			),
			memoryHighWaterPercent,
			requestConcurrency: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_REQUEST_CONCURRENCY",
				64,
				1,
				4_096,
			),
			requestQueueSize: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_REQUEST_QUEUE_SIZE",
				128,
				0,
				100_000,
			),
			requestQueueWaitMs: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_REQUEST_QUEUE_WAIT_MS",
				5_000,
				1,
				60_000,
			),
			requestTimeoutMs: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_REQUEST_TIMEOUT_MS",
				30_000,
				10,
				5 * 60_000,
			),
		};
		this.#admission = new ActorAdmission(
			this.config.requestConcurrency,
			this.config.requestQueueSize,
			this.config.requestQueueWaitMs,
		);
		this.#timer = setInterval(
			() => void this.#prune(),
			Math.min(this.config.idleTtlMs, 30_000),
		);
		this.#timer.unref?.();
	}

	async request(input: ActorRuntimeRequest): Promise<Response> {
		await this.#admission.acquire();
		let admissionHandedOff = false;
		try {
			const body = await readBoundedBody(
				input.request.body,
				this.config.maxStartPayloadBytes,
			);
			if (!body) {
				return new Response("RivetKit actor start payload exceeds limit", {
					status: 413,
				});
			}
			const entry = await this.#entry(input);
			try {
				await entry.ready;
			} catch (error) {
				entry.active = Math.max(0, entry.active - 1);
				entry.lastUsedAt = Date.now();
				if (this.#entries.size > this.config.maxEntries) void this.#prune();
				throw error;
			}
			const id = String(++entry.nextRequestId);
			return new Promise<Response>((resolve, reject) => {
				const pending: PendingRequest = {
					resolve,
					reject,
					waitingAck: false,
					settled: false,
					releaseAdmission: () => this.#admission.release(),
				};
				const cancel = () => entry.worker.postMessage({ type: "cancel", id });
				pending.abort = cancel;
				entry.pending.set(id, pending);
				admissionHandedOff = true;
				pending.timeout = setTimeout(() => {
					this.#failEntry(
						entry,
						new Error(
							`Dynamic App actor request exceeded ${this.config.requestTimeoutMs}ms`,
						),
					);
				}, this.config.requestTimeoutMs);
				try {
					entry.worker.postMessage({
						type: "request",
						id,
						method: input.request.method,
						url: input.request.url,
						headers: [...input.request.headers.entries()],
						body,
					});
					input.request.signal.addEventListener("abort", cancel, {
						once: true,
					});
					if (input.request.signal.aborted) cancel();
				} catch (error) {
					this.#settle(entry, id);
					reject(error);
				}
			});
		} finally {
			if (!admissionHandedOff) this.#admission.release();
		}
	}

	async invalidate(key: string): Promise<void> {
		const entry = this.#entries.get(key);
		if (!entry) return;
		this.#entries.delete(key);
		await this.#disposeEntry(entry);
	}

	diagnostics(): Record<string, number> {
		const entries = [...this.#entries.values()];
		return {
			workerLimit: this.config.maxEntries,
			entries: entries.length,
			creating: this.#creating.size,
			workerReservations: this.#workerReservations,
			activeRequests: entries.reduce((sum, entry) => sum + entry.active, 0),
			pendingRequests: entries.reduce(
				(sum, entry) => sum + entry.pending.size,
				0,
			),
			admittedRequests: this.#admission.active,
			queuedRequests: this.#admission.queued,
		};
	}

	async dispose(): Promise<void> {
		if (this.#disposePromise !== undefined) return this.#disposePromise;
		this.#disposed = true;
		clearInterval(this.#timer);
		this.#admission.dispose();
		this.#disposePromise = this.#finishDispose();
		return this.#disposePromise;
	}

	async #finishDispose(): Promise<void> {
		while (this.#creating.size > 0) {
			await Promise.allSettled([...this.#creating.values()]);
			await new Promise<void>((resolve) => setImmediate(resolve));
		}
		await Promise.allSettled(
			[...this.#entries.values()].map((entry) => this.#disposeEntry(entry)),
		);
		this.#entries.clear();
	}

	async #entry(input: ActorRuntimeRequest): Promise<RuntimeEntry> {
		if (this.#disposed) throw new Error("Dynamic App actor runtime disposed");
		const existing = this.#entries.get(input.key);
		if (existing && !existing.disposed) return this.#lease(existing);
		const pending = this.#creating.get(input.key);
		if (pending) return this.#lease(await pending);
		const reservation = this.#reserveWorker();
		const promise = reservation.then(async () => {
			const entry = await this.#createEntry(input);
			if (this.#disposed) {
				await this.#disposeEntry(entry);
				throw new Error(
					"Dynamic App actor runtime disposed during worker creation",
				);
			}
			return entry;
		});
		this.#creating.set(input.key, promise);
		let reserved = true;
		try {
			const entry = await promise;
			this.#creating.delete(input.key);
			this.#workerReservations = Math.max(0, this.#workerReservations - 1);
			reserved = false;
			if (this.#disposed) {
				await this.#disposeEntry(entry);
				throw new Error(
					"Dynamic App actor runtime disposed during worker creation",
				);
			}
			this.#entries.set(input.key, entry);
			this.#lease(entry);
			await this.#prune(entry.key);
			return entry;
		} finally {
			if (reserved) {
				this.#workerReservations = Math.max(0, this.#workerReservations - 1);
			}
			if (this.#creating.get(input.key) === promise) {
				this.#creating.delete(input.key);
			}
		}
	}

	#reserveWorker(): Promise<void> {
		if (this.#disposed) throw new Error("Dynamic App actor runtime disposed");
		if (
			this.#entries.size + this.#workerReservations <
			this.config.maxEntries
		) {
			this.#workerReservations += 1;
			return Promise.resolve();
		}
		const idle = [...this.#entries.values()]
			.filter((entry) => entry.active === 0 && !entry.disposed)
			.sort((a, b) => a.lastUsedAt - b.lastUsedAt)[0];
		if (!idle) {
			throw new DynamicAppsError(
				"agentos_apps_no_capacity",
				`Dynamic Apps actor worker limit ${this.config.maxEntries} is busy`,
			);
		}
		this.#entries.delete(idle.key);
		this.#workerReservations += 1;
		return this.#disposeEntry(idle);
	}

	#lease(entry: RuntimeEntry): RuntimeEntry {
		if (entry.disposed) {
			throw new Error(
				"Dynamic App actor worker was disposed before request lease",
			);
		}
		entry.active += 1;
		entry.lastUsedAt = Date.now();
		return entry;
	}

	async #createEntry(input: ActorRuntimeRequest): Promise<RuntimeEntry> {
		const directory = await mkdtemp(join(tmpdir(), "dynamic-app-actor-"));
		try {
			const files = extractActorFiles(await input.loadArtifact());
			for (const [path, bytes] of files) {
				const target = join(directory, ...path.split("/"));
				await mkdir(dirname(target), { recursive: true });
				await writeFile(target, bytes, { mode: 0o600 });
			}
			const platformPackages = await resolvePlatformActorPackages();
			await mkdir(join(directory, "node_modules"), { recursive: true });
			await symlink(
				platformPackages.rivetkit,
				join(directory, "node_modules", "rivetkit"),
				"dir",
			);
			const entrypoint = pathToFileURL(join(directory, "main.mjs")).href;
			let readyResolve = () => {};
			let readyReject = (_error: unknown) => {};
			const ready = new Promise<void>((resolve, reject) => {
				readyResolve = resolve;
				readyReject = reject;
			});
			const worker = new Worker(
				new URL(
					`data:text/javascript,${encodeURIComponent(ACTOR_WORKER_SOURCE)}`,
				),
				{
					workerData: {
						entrypoint,
						wasmPath: platformPackages.wasmPath,
					},
					env: actorWorkerEnvironment(input),
					resourceLimits: {
						maxOldGenerationSizeMb: this.config.heapLimitMb,
						maxYoungGenerationSizeMb: Math.max(
							4,
							Math.min(32, Math.floor(this.config.heapLimitMb / 4)),
						),
					},
				},
			);
			const entry: RuntimeEntry = {
				key: input.key,
				directory,
				worker,
				ready,
				pending: new Map(),
				active: 0,
				lastUsedAt: Date.now(),
				disposed: false,
				nextRequestId: 0,
			};
			let startupSettled = false;
			const startupTimer = setTimeout(() => {
				if (startupSettled || entry.disposed) return;
				startupSettled = true;
				const error = new Error(
					`Dynamic App actor worker startup exceeded ${this.config.startTimeoutMs}ms`,
				);
				readyReject(error);
				this.#failEntry(entry, error);
			}, this.config.startTimeoutMs);
			startupTimer.unref?.();
			const finishStartup = () => {
				if (startupSettled) return;
				startupSettled = true;
				clearTimeout(startupTimer);
			};
			worker.on("message", (message: WorkerMessage) => {
				if (message.type === "ready") {
					finishStartup();
					readyResolve();
					return;
				}
				if (message.type === "error" && !message.id) {
					finishStartup();
					readyReject(new Error(message.message));
					this.#failEntry(entry, new Error(message.message));
					return;
				}
				if (message.id) this.#handleMessage(entry, message);
			});
			worker.once("error", (error) => {
				finishStartup();
				readyReject(error);
				this.#failEntry(entry, error);
			});
			worker.once("exit", (code) => {
				finishStartup();
				if (!entry.disposed) {
					const error = new Error(
						`Dynamic App actor worker exited with ${code}`,
					);
					readyReject(error);
					this.#failEntry(entry, error);
				}
			});
			return entry;
		} catch (error) {
			await rm(directory, { recursive: true, force: true });
			throw error;
		}
	}

	#handleMessage(entry: RuntimeEntry, message: WorkerMessage): void {
		if (!("id" in message) || !message.id) return;
		const pending = entry.pending.get(message.id);
		if (!pending || pending.settled) return;
		if (message.type === "head") {
			if (pending.timeout) {
				clearTimeout(pending.timeout);
				pending.timeout = undefined;
			}
			const stream = new ReadableStream<Uint8Array>({
				start: (controller) => {
					pending.controller = controller;
				},
				pull: () => {
					if (!pending.waitingAck) return;
					pending.waitingAck = false;
					entry.worker.postMessage({ type: "ack", id: message.id });
				},
				cancel: () => {
					entry.worker.postMessage({ type: "cancel", id: message.id });
					this.#settle(entry, message.id);
				},
			});
			pending.resolve(
				new Response(stream, {
					status: message.status,
					headers: message.headers,
				}),
			);
			return;
		}
		if (message.type === "chunk") {
			pending.controller?.enqueue(new Uint8Array(message.chunk));
			if ((pending.controller?.desiredSize ?? 1) > 0) {
				entry.worker.postMessage({ type: "ack", id: message.id });
			} else {
				pending.waitingAck = true;
			}
			return;
		}
		if (message.type === "end") {
			pending.controller?.close();
			this.#settle(entry, message.id);
			return;
		}
		if (message.type === "error") {
			const error = new Error(message.message);
			if (pending.controller) pending.controller.error(error);
			else pending.reject(error);
			this.#settle(entry, message.id);
		}
	}

	#settle(entry: RuntimeEntry, id: string): void {
		const pending = entry.pending.get(id);
		if (!pending || pending.settled) return;
		pending.settled = true;
		entry.pending.delete(id);
		if (pending.timeout) clearTimeout(pending.timeout);
		pending.releaseAdmission();
		entry.active = Math.max(0, entry.active - 1);
		entry.lastUsedAt = Date.now();
		if (this.#entries.size > this.config.maxEntries) void this.#prune();
	}

	#failEntry(entry: RuntimeEntry, error: unknown): void {
		this.#entries.delete(entry.key);
		for (const [id, pending] of entry.pending) {
			if (pending.controller) pending.controller.error(error);
			else pending.reject(error);
			this.#settle(entry, id);
		}
		void this.#disposeEntry(entry);
	}

	async #prune(protectedKey?: string): Promise<void> {
		if (this.#disposed) return;
		const now = Date.now();
		const memoryPressure = await this.#memoryPressure();
		const entries = [...this.#entries.values()].sort(
			(a, b) => a.lastUsedAt - b.lastUsedAt,
		);
		for (const entry of entries) {
			if (
				entry.key !== protectedKey &&
				entry.active === 0 &&
				(memoryPressure ||
					now - entry.lastUsedAt >= this.config.idleTtlMs ||
					this.#entries.size > this.config.maxEntries)
			) {
				this.#entries.delete(entry.key);
				await this.#disposeEntry(entry);
			}
		}
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

	async #disposeEntry(entry: RuntimeEntry): Promise<void> {
		if (entry.disposed) return;
		entry.disposed = true;
		for (const [id, pending] of entry.pending) {
			pending.reject(new Error("Dynamic App actor worker disposed"));
			pending.controller?.error(new Error("Dynamic App actor worker disposed"));
			this.#settle(entry, id);
		}
		await Promise.allSettled([
			entry.worker.terminate(),
			rm(entry.directory, { recursive: true, force: true }),
		]);
	}
}

async function readBoundedBody(
	body: ReadableStream<Uint8Array> | null,
	limit: number,
): Promise<Uint8Array | undefined> {
	if (!body) return new Uint8Array();
	const reader = body.getReader();
	const chunks: Uint8Array[] = [];
	let bytes = 0;
	try {
		for (;;) {
			const { value, done } = await reader.read();
			if (done) return new Uint8Array(Buffer.concat(chunks, bytes));
			bytes += value.byteLength;
			if (bytes > limit) {
				await reader.cancel("RivetKit actor start payload exceeds limit");
				return undefined;
			}
			chunks.push(value);
		}
	} finally {
		reader.releaseLock();
	}
}

let platformActorPackages:
	| Promise<{ rivetkit: string; wasmPath: string }>
	| undefined;

function resolvePlatformActorPackages(): Promise<{
	rivetkit: string;
	wasmPath: string;
}> {
	platformActorPackages ??= (async () => {
		const hostRequire = createRequire(import.meta.url);
		const rivetkitEntry = hostRequire.resolve("rivetkit");
		const rivetkit = await findPackageRoot(rivetkitEntry);
		const wasmPath = createRequire(rivetkitEntry).resolve(
			"@rivetkit/rivetkit-wasm/rivetkit_wasm_bg.wasm",
		);
		return { rivetkit, wasmPath };
	})();
	return platformActorPackages;
}

async function findPackageRoot(entrypoint: string): Promise<string> {
	let directory = dirname(entrypoint);
	for (;;) {
		try {
			const value = JSON.parse(
				await readFile(join(directory, "package.json"), "utf8"),
			) as { name?: unknown };
			if (typeof value.name === "string" && value.name) return directory;
		} catch (error) {
			if (
				typeof error !== "object" ||
				error === null ||
				!("code" in error) ||
				error.code !== "ENOENT"
			) {
				throw error;
			}
		}
		const parent = dirname(directory);
		if (parent === directory) {
			throw new Error(`Could not find package root for ${entrypoint}`);
		}
		directory = parent;
	}
}

/** @internal */
export function actorWorkerEnvironment(
	input: Pick<ActorRuntimeRequest, "endpoint" | "key" | "namespace" | "pool">,
): NodeJS.ProcessEnv {
	const endpoint = new URL(input.endpoint);
	const endpointNamespace = endpoint.username
		? decodeURIComponent(endpoint.username)
		: undefined;
	const endpointToken = endpoint.password
		? decodeURIComponent(endpoint.password)
		: undefined;
	if (endpointNamespace && endpointNamespace !== input.namespace) {
		throw new Error(
			`Dynamic App actor endpoint namespace ${endpointNamespace} does not match ${input.namespace}`,
		);
	}
	endpoint.username = "";
	endpoint.password = "";
	return {
		NODE_ENV: "production",
		RIVETKIT_RUNTIME: "wasm",
		RIVETKIT_RUNTIME_MODE: "serverless",
		RIVET_ENDPOINT: endpoint.toString().replace(/\/$/u, ""),
		RIVET_NAMESPACE: input.namespace,
		...(endpointToken ? { RIVET_TOKEN: endpointToken } : {}),
		RIVET_POOL: input.pool,
		RIVET_RUNNER: input.pool,
		RIVET_RUNNER_POOL: input.pool,
		RIVET_ENVOY_VERSION: String(actorEnvoyVersion(input.key)),
	};
}

function actorEnvoyVersion(key: string): number {
	return (
		createHash("sha256").update(key).digest().readUInt32BE(0) & 0x7fffffff || 1
	);
}

function extractActorFiles(artifact: Uint8Array): Map<string, Uint8Array> {
	const buffer = Buffer.from(
		artifact.buffer,
		artifact.byteOffset,
		artifact.byteLength,
	);
	if (
		buffer.byteLength < 16 ||
		buffer[0] !== 137 ||
		buffer.subarray(1, 4).toString("ascii") !== "AOS"
	) {
		throw new Error("Dynamic App actor artifact is not an AOSP package");
	}
	let offset = 16 + buffer.readUInt32LE(8) + buffer.readUInt32LE(12);
	let total = 0;
	const output = new Map<string, Uint8Array>();
	while (offset + 512 <= buffer.byteLength) {
		const header = buffer.subarray(offset, offset + 512);
		if (header.every((value) => value === 0)) break;
		const name = tarString(header.subarray(0, 100));
		const prefix = tarString(header.subarray(345, 500));
		const path = `${prefix ? `${prefix}/` : ""}${name}`.replace(/^\.\//, "");
		const size = Number.parseInt(
			tarString(header.subarray(124, 136)) || "0",
			8,
		);
		const type = String.fromCharCode(header[156] ?? 0);
		if (!Number.isSafeInteger(size) || size < 0) {
			throw new Error("Dynamic App actor artifact has an invalid tar entry");
		}
		const dataOffset = offset + 512;
		const next = dataOffset + Math.ceil(size / 512) * 512;
		if (next > buffer.byteLength) {
			throw new Error("Dynamic App actor artifact is truncated");
		}
		if (path.startsWith("actor/") && (type === "\0" || type === "0")) {
			const relative = posix.normalize(path.slice("actor/".length));
			if (
				!relative ||
				relative === "." ||
				relative === ".." ||
				relative.startsWith("../") ||
				posix.isAbsolute(relative) ||
				size > MAX_ACTOR_FILE_BYTES
			) {
				throw new Error("Dynamic App actor artifact contains an invalid path");
			}
			total += size;
			if (total > MAX_ACTOR_ARTIFACT_BYTES || output.size >= MAX_ACTOR_FILES) {
				throw new Error("Dynamic App actor artifact exceeds extraction limits");
			}
			output.set(
				relative,
				new Uint8Array(buffer.subarray(dataOffset, dataOffset + size)),
			);
		}
		offset = next;
	}
	if (!output.has(ACTOR_BUNDLE_PATH.slice("actor/".length))) {
		throw new Error("Dynamic App artifact has no actor bundle");
	}
	return output;
}

function tarString(bytes: Uint8Array): string {
	const end = bytes.indexOf(0);
	return Buffer.from(end < 0 ? bytes : bytes.subarray(0, end)).toString("utf8");
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
		throw new Error(`${name} must be an integer from ${minimum} to ${maximum}`);
	}
	return value;
}

class ActorAdmission {
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
		if (this.#disposed) throw new Error("actor runtime disposed");
		if (this.#active < this.capacity) {
			this.#active += 1;
			return;
		}
		if (this.#queue.length >= this.#maxQueued) {
			throw new DynamicAppsError(
				"agentos_apps_no_capacity",
				"Dynamic Apps actor callback queue is full",
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
						`Dynamic Apps actor callback queue exceeded ${this.#waitMs}ms`,
					),
				);
			}, this.#waitMs);
			this.#queue.push(item);
		});
	}

	release(): void {
		if (this.#active <= 0) return;
		this.#active -= 1;
		this.#queue.shift()?.resolve();
	}

	dispose(): void {
		this.#disposed = true;
		for (const item of this.#queue.splice(0)) {
			item.reject(new Error("actor runtime disposed"));
		}
	}
}

const ACTOR_WORKER_SOURCE = `
import { readFile } from "node:fs/promises";
import { parentPort, workerData } from "node:worker_threads";
if (!parentPort) throw new Error("Dynamic App actor worker has no parent port");
const { registry } = await import(workerData.entrypoint);
if (typeof registry?.handler !== "function") throw new TypeError("Dynamic App actor registry is invalid");
if (process.env.RIVETKIT_RUNTIME === "wasm" && registry.config) {
  registry.config.wasm = {
    ...registry.config.wasm,
    initInput: await readFile(workerData.wasmPath),
  };
}
const requests = new Map();
const acknowledgements = new Map();
const waitForAck = (id) => new Promise((resolve) => acknowledgements.set(id, resolve));
const finishAck = (id) => { const resolve = acknowledgements.get(id); acknowledgements.delete(id); resolve?.(); };
const describeError = (error) => {
  if (!(error instanceof Error)) return String(error);
  const details = [error.stack || error.message];
  let cause = error.cause;
  for (let depth = 0; cause !== undefined && depth < 4; depth += 1) {
    details.push("Caused by: " + (cause instanceof Error ? cause.stack || cause.message : String(cause)));
    cause = cause instanceof Error ? cause.cause : undefined;
  }
  return details.join("\\n");
};
parentPort.on("message", (message) => {
  if (message.type === "ack") { finishAck(message.id); return; }
  if (message.type === "cancel") {
    requests.get(message.id)?.abort(new Error("host cancelled actor callback"));
    finishAck(message.id);
    return;
  }
  if (message.type !== "request") return;
  void (async () => {
    const controller = new AbortController();
    requests.set(message.id, controller);
    try {
      const response = await registry.handler(new Request(message.url, {
        method: message.method,
        headers: message.headers,
        body: message.method === "GET" || message.method === "HEAD" ? undefined : message.body,
        signal: controller.signal,
      }));
      parentPort.postMessage({
        type: "head",
        id: message.id,
        status: response.status,
        headers: [...response.headers.entries()],
      });
      if (response.body) {
        const reader = response.body.getReader();
        try {
          for (;;) {
            const chunk = await reader.read();
            if (chunk.done) break;
            const bytes = new Uint8Array(chunk.value);
            parentPort.postMessage({ type: "chunk", id: message.id, chunk: bytes }, [bytes.buffer]);
            await waitForAck(message.id);
          }
        } finally {
          reader.releaseLock();
        }
      }
      parentPort.postMessage({ type: "end", id: message.id });
    } catch (error) {
      parentPort.postMessage({ type: "error", id: message.id, message: describeError(error) });
    } finally {
      requests.delete(message.id);
      finishAck(message.id);
    }
  })();
});
parentPort.postMessage({ type: "ready" });
`;

let defaultActorRuntime: DynamicActorRuntime | undefined;

export function getDefaultActorRuntime(): DynamicActorRuntime {
	defaultActorRuntime ??= new DynamicActorRuntime();
	return defaultActorRuntime;
}

/** @internal */
export async function resetDefaultActorRuntimeForTest(): Promise<void> {
	await defaultActorRuntime?.dispose();
	defaultActorRuntime = undefined;
}
