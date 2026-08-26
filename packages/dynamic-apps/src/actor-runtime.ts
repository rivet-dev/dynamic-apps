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
import { ACTOR_BUNDLE_PATH } from "./runtime.js";

const MAX_ACTOR_FILES = 4_096;
const MAX_ACTOR_FILE_BYTES = 32 * 1024 * 1024;
const MAX_ACTOR_ARTIFACT_BYTES = 64 * 1024 * 1024;

interface ActorRuntimeConfig {
	maxEntries: number;
	heapLimitMb: number;
	idleTtlMs: number;
	maxStartPayloadBytes: number;
	memoryHighWaterPercent: number;
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
	readonly #timer: ReturnType<typeof setInterval>;

	constructor(env: NodeJS.ProcessEnv = process.env) {
		this.config = {
			maxEntries: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES",
				4,
				1,
				1_024,
			),
			heapLimitMb: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_WORKER_HEAP_LIMIT_MB",
				96,
				16,
				2_048,
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
			memoryHighWaterPercent: integerEnv(
				env,
				"DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT",
				70,
				10,
				95,
			),
		};
		this.#timer = setInterval(
			() => void this.#prune(),
			Math.min(this.config.idleTtlMs, 30_000),
		);
		this.#timer.unref?.();
	}

	async request(input: ActorRuntimeRequest): Promise<Response> {
		const body = new Uint8Array(await input.request.arrayBuffer());
		if (body.byteLength > this.config.maxStartPayloadBytes) {
			return new Response("RivetKit actor start payload exceeds limit", {
				status: 413,
			});
		}
		const entry = await this.#entry(input);
		entry.active += 1;
		entry.lastUsedAt = Date.now();
		try {
			await entry.ready;
		} catch (error) {
			entry.active = Math.max(0, entry.active - 1);
			entry.lastUsedAt = Date.now();
			throw error;
		}
		const id = String(++entry.nextRequestId);
		return new Promise<Response>((resolve, reject) => {
			const pending: PendingRequest = {
				resolve,
				reject,
				waitingAck: false,
				settled: false,
			};
			const cancel = () => entry.worker.postMessage({ type: "cancel", id });
			pending.abort = cancel;
			entry.pending.set(id, pending);
			try {
				entry.worker.postMessage({
					type: "request",
					id,
					method: input.request.method,
					url: input.request.url,
					headers: [...input.request.headers.entries()],
					body,
				});
				input.request.signal.addEventListener("abort", cancel, { once: true });
				if (input.request.signal.aborted) cancel();
			} catch (error) {
				this.#settle(entry, id);
				reject(error);
			}
		});
	}

	async invalidate(key: string): Promise<void> {
		const entry = this.#entries.get(key);
		if (!entry) return;
		this.#entries.delete(key);
		await this.#disposeEntry(entry);
	}

	async dispose(): Promise<void> {
		clearInterval(this.#timer);
		await Promise.allSettled(
			[...this.#entries.values()].map((entry) => this.#disposeEntry(entry)),
		);
		this.#entries.clear();
	}

	async #entry(input: ActorRuntimeRequest): Promise<RuntimeEntry> {
		const existing = this.#entries.get(input.key);
		if (existing && !existing.disposed) return existing;
		const pending = this.#creating.get(input.key);
		if (pending) return pending;
		const promise = this.#createEntry(input);
		this.#creating.set(input.key, promise);
		try {
			const entry = await promise;
			this.#entries.set(input.key, entry);
			await this.#prune(entry.key);
			return entry;
		} finally {
			if (this.#creating.get(input.key) === promise) {
				this.#creating.delete(input.key);
			}
		}
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
					workerData: { entrypoint },
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
			worker.on("message", (message: WorkerMessage) => {
				if (message.type === "ready") {
					readyResolve();
					return;
				}
				if (message.type === "error" && !message.id) {
					readyReject(new Error(message.message));
					this.#failEntry(entry, new Error(message.message));
					return;
				}
				if (message.id) this.#handleMessage(entry, message);
			});
			worker.once("error", (error) => {
				readyReject(error);
				this.#failEntry(entry, error);
			});
			worker.once("exit", (code) => {
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
		entry.active = Math.max(0, entry.active - 1);
		entry.lastUsedAt = Date.now();
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
		for (const pending of entry.pending.values()) {
			pending.reject(new Error("Dynamic App actor worker disposed"));
			pending.controller?.error(new Error("Dynamic App actor worker disposed"));
		}
		entry.pending.clear();
		await Promise.allSettled([
			entry.worker.terminate(),
			rm(entry.directory, { recursive: true, force: true }),
		]);
	}
}

let platformActorPackages: Promise<{ rivetkit: string }> | undefined;

function resolvePlatformActorPackages(): Promise<{ rivetkit: string }> {
	platformActorPackages ??= (async () => {
		const hostRequire = createRequire(import.meta.url);
		const rivetkitEntry = hostRequire.resolve("rivetkit");
		const rivetkit = await findPackageRoot(rivetkitEntry);
		return { rivetkit };
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
		RIVETKIT_RUNTIME: "native",
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

const ACTOR_WORKER_SOURCE = `
import { parentPort, workerData } from "node:worker_threads";
if (!parentPort) throw new Error("Dynamic App actor worker has no parent port");
const { registry } = await import(workerData.entrypoint);
if (typeof registry?.handler !== "function") throw new TypeError("Dynamic App actor registry is invalid");
const requests = new Map();
const acknowledgements = new Map();
const waitForAck = (id) => new Promise((resolve) => acknowledgements.set(id, resolve));
const finishAck = (id) => { const resolve = acknowledgements.get(id); acknowledgements.delete(id); resolve?.(); };
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
      parentPort.postMessage({ type: "error", id: message.id, message: error instanceof Error ? error.message : String(error) });
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
