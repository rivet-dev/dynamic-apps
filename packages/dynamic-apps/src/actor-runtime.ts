import { createHash, randomUUID } from "node:crypto";
import { chmod, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { AgentOs } from "@rivet-dev/agentos-core";
import { emitDynamicAppsLog } from "./logging.js";

const ACTOR_HTTP_PORT = 3000;
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

interface ActorRuntimeConfig {
	maxEntries: number;
	heapLimitMb: number;
	startTimeoutMs: number;
	idleTtlMs: number;
	maxStartPayloadBytes: number;
	memoryHighWaterPercent: number;
	requestTimeoutMs: number;
}

export interface ActorRuntimeRequest {
	key: string;
	appId?: string;
	release?: string;
	loadArtifact: () => Promise<Uint8Array>;
	endpoint: string;
	namespace: string;
	pool: string;
	request: Request;
}

interface RuntimeEntry {
	key: string;
	appId: string;
	release: string;
	directory: string;
	vm: AgentOs;
	pid: number;
	active: number;
	lastUsedAt: number;
	disposed: boolean;
}

/** Runs RivetKit callbacks in cached agentOS VMs using its native HTTP stream. */
export class DynamicActorRuntime {
	readonly config: ActorRuntimeConfig;
	readonly #entries = new Map<string, RuntimeEntry>();
	readonly #creating = new Map<string, Promise<RuntimeEntry>>();
	readonly #timer: ReturnType<typeof setInterval>;
	#disposed = false;
	#disposePromise?: Promise<void>;

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
			startTimeoutMs: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS",
				30_000,
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
				16 * 1024 * 1024,
				1,
				64 * 1024 * 1024,
			),
			memoryHighWaterPercent: integerEnv(
				env,
				"DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT",
				70,
				10,
				95,
			),
			requestTimeoutMs: integerEnv(
				env,
				"DYNAMIC_APPS_ACTOR_REQUEST_TIMEOUT_MS",
				30_000,
				10,
				5 * 60_000,
			),
		};
		this.#timer = setInterval(
			() => void this.#prune(),
			Math.min(this.config.idleTtlMs, 30_000),
		);
		this.#timer.unref?.();
	}

	async request(input: ActorRuntimeRequest): Promise<Response> {
		const body = await readBoundedBody(
			input.request.body,
			this.config.maxStartPayloadBytes,
		);
		if (!body)
			return new Response("RivetKit actor start payload exceeds limit", {
				status: 413,
			});
		const entry = await this.#entry(input);
		const request = new Request(input.request.url, {
			method: input.request.method,
			headers: input.request.headers,
			body:
				input.request.method === "GET" || input.request.method === "HEAD"
					? undefined
					: Buffer.from(body),
		});
		let head: Awaited<ReturnType<AgentOs["fetchStreamStart"]>>;
		try {
			head = await withTimeout(
				entry.vm.fetchStreamStart(ACTOR_HTTP_PORT, request),
				this.config.requestTimeoutMs,
				`Dynamic App actor response headers exceeded ${this.config.requestTimeoutMs}ms`,
			);
		} catch (error) {
			this.#release(entry);
			this.#failEntry(entry, error);
			throw error;
		}

		let settled = false;
		const settle = () => {
			if (settled) return;
			settled = true;
			input.request.signal.removeEventListener("abort", cancel);
			this.#release(entry);
		};
		const cancel = () => {
			if (settled) return;
			void entry.vm.fetchStreamCancel(head.streamId).catch(() => {});
			settle();
		};
		input.request.signal.addEventListener("abort", cancel, { once: true });
		if (input.request.signal.aborted) cancel();

		const stream = new ReadableStream<Uint8Array>({
			pull: async (controller) => {
				if (settled) return controller.close();
				try {
					const chunk = await entry.vm.fetchStreamRead(head.streamId);
					if (chunk.body.byteLength > 0) controller.enqueue(chunk.body);
					if (chunk.done) {
						controller.close();
						settle();
					}
				} catch (error) {
					controller.error(error);
					settle();
				}
			},
			cancel: async () => {
				if (settled) return;
				await entry.vm.fetchStreamCancel(head.streamId).catch(() => {});
				settle();
			},
		});
		return new Response(stream, {
			status: head.status,
			statusText: head.statusText,
			headers: proxyResponseHeaders(head.headers),
		});
	}

	async invalidate(key: string): Promise<void> {
		const entry = this.#entries.get(key);
		if (!entry) return;
		this.#entries.delete(key);
		await this.#disposeEntry(entry);
	}

	diagnostics(): Record<string, number> {
		const entries = [...this.#entries.values()];
		const active = entries.reduce((sum, entry) => sum + entry.active, 0);
		return {
			workerLimit: this.config.maxEntries,
			entries: entries.length,
			creating: this.#creating.size,
			workerReservations: 0,
			activeRequests: active,
			pendingRequests: active,
			admittedRequests: active,
			queuedRequests: 0,
		};
	}

	async dispose(): Promise<void> {
		if (this.#disposePromise !== undefined) return this.#disposePromise;
		this.#disposed = true;
		clearInterval(this.#timer);
		this.#disposePromise = this.#finishDispose();
		return this.#disposePromise;
	}

	async #finishDispose(): Promise<void> {
		while (this.#creating.size > 0)
			await Promise.allSettled([...this.#creating.values()]);
		await Promise.allSettled(
			[...this.#entries.values()].map((entry) => this.#disposeEntry(entry)),
		);
		this.#entries.clear();
	}

	async #entry(input: ActorRuntimeRequest): Promise<RuntimeEntry> {
		if (this.#disposed) throw new Error("Dynamic App actor runtime disposed");
		const existing = this.#entries.get(input.key);
		if (existing && !existing.disposed) return this.#lease(existing);
		const creating = this.#creating.get(input.key);
		if (creating) return this.#lease(await creating);
		const promise = this.#createEntry(input);
		this.#creating.set(input.key, promise);
		try {
			const entry = await promise;
			if (this.#disposed) {
				await this.#disposeEntry(entry);
				throw new Error("Dynamic App actor runtime disposed during creation");
			}
			this.#entries.set(input.key, entry);
			this.#lease(entry);
			await this.#prune(entry.key);
			return entry;
		} finally {
			if (this.#creating.get(input.key) === promise)
				this.#creating.delete(input.key);
		}
	}

	#lease(entry: RuntimeEntry): RuntimeEntry {
		if (entry.disposed)
			throw new Error("Dynamic App actor runtime was disposed");
		entry.active++;
		entry.lastUsedAt = Date.now();
		return entry;
	}

	#release(entry: RuntimeEntry): void {
		entry.active = Math.max(0, entry.active - 1);
		entry.lastUsedAt = Date.now();
		if (this.#entries.size > this.config.maxEntries) void this.#prune();
	}

	async #createEntry(input: ActorRuntimeRequest): Promise<RuntimeEntry> {
		const directory = await mkdtemp(
			join(tmpdir(), "dynamic-app-actor-agentos-"),
		);
		const artifactPath = join(directory, "release.aospkg");
		let vm: AgentOs | undefined;
		try {
			await chmod(directory, 0o700);
			await writeFile(artifactPath, await input.loadArtifact(), {
				mode: 0o600,
			});
			vm = await AgentOs.create({
				sidecar: { kind: "shared", pool: "dynamic-apps-actors" },
				defaultSoftware: false,
				loopbackExemptPorts: loopbackExemptPorts(input.endpoint),
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
				limits: { jsRuntime: { v8HeapLimitMb: this.config.heapLimitMb } },
			});
			const pendingLogs: Array<["stdout" | "stderr", Uint8Array]> = [];
			const readyNonce = randomUUID();
			const readyMarker = `DYNAMIC_APPS_SERVER_READY:${readyNonce}`;
			let readyBuffer = "";
			let resolveReady = () => {};
			let rejectReady = (_error: unknown) => {};
			const ready = new Promise<void>((resolve, reject) => {
				resolveReady = resolve;
				rejectReady = reject;
			});
			let entry: RuntimeEntry | undefined;
			const process = await vm.process.spawn("node", ["/app/actor/main.mjs"], {
				cwd: "/app/actor",
				env: stringEnvironment({
					...actorWorkerEnvironment(input),
					DYNAMIC_APPS_READY_NONCE: readyNonce,
				}),
				onStdout: (data) => {
					readyBuffer = `${readyBuffer}${new TextDecoder().decode(data)}`.slice(
						-4_096,
					);
					if (readyBuffer.includes(readyMarker)) resolveReady();
					if (entry) this.#logGuest(entry, "stdout", data);
					else pendingLogs.push(["stdout", data]);
				},
				onStderr: (data) =>
					entry
						? this.#logGuest(entry, "stderr", data)
						: pendingLogs.push(["stderr", data]),
			});
			entry = {
				key: input.key,
				appId: input.appId ?? "unknown",
				release: input.release ?? "unknown",
				directory,
				vm,
				pid: process.pid,
				active: 0,
				lastUsedAt: Date.now(),
				disposed: false,
			};
			for (const [stream, data] of pendingLogs)
				this.#logGuest(entry, stream, data);
			const processExit = vm.process.wait(process.pid);
			void processExit.then((exit) => {
				const error = new Error(
					`Dynamic App agentOS actor process exited with ${exit.exitCode ?? 1}`,
				);
				rejectReady(error);
				if (!entry || entry.disposed) return;
				this.#failEntry(entry, error);
			});
			await withTimeout(
				ready,
				this.config.startTimeoutMs,
				`Dynamic App agentOS actor startup exceeded ${this.config.startTimeoutMs}ms`,
			);
			return entry;
		} catch (error) {
			await vm?.dispose().catch(() => {});
			await rm(directory, { recursive: true, force: true }).catch(() => {});
			throw error;
		}
	}

	#failEntry(entry: RuntimeEntry, error: unknown): void {
		this.#entries.delete(entry.key);
		emitDynamicAppsLog({
			level: "error",
			source: "actor",
			message:
				error instanceof Error ? (error.stack ?? error.message) : String(error),
			appId: entry.appId,
			release: entry.release,
		});
		void this.#disposeEntry(entry);
	}

	#logGuest(
		entry: Pick<RuntimeEntry, "appId" | "release">,
		stream: "stdout" | "stderr",
		data: Uint8Array,
	): void {
		emitDynamicAppsLog({
			level: stream === "stdout" ? "info" : "error",
			source: "actor",
			message: new TextDecoder().decode(data).slice(0, 64 * 1024),
			appId: entry.appId,
			release: entry.release,
			stream,
		});
	}

	async #prune(protectedKey?: string): Promise<void> {
		if (this.#disposed) return;
		const now = Date.now();
		const memoryPressure = await this.#memoryPressure();
		for (const entry of [...this.#entries.values()].sort(
			(a, b) => a.lastUsedAt - b.lastUsedAt,
		)) {
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
		const results = await Promise.allSettled([
			entry.vm.dispose(),
			rm(entry.directory, { recursive: true, force: true }),
		]);
		if (results.some((result) => result.status === "rejected")) {
			emitDynamicAppsLog({
				level: "error",
				source: "runtime",
				message: "Dynamic App agentOS actor disposal failed",
				appId: entry.appId,
				release: entry.release,
			});
		}
	}
}

function proxyResponseHeaders(
	input: Headers | Iterable<readonly [string, string]>,
): Headers {
	const headers = new Headers();
	if (input instanceof Headers) {
		input.forEach((value, name) => {
			headers.append(name, value);
		});
	} else {
		for (const [name, value] of input) headers.append(name, value);
	}
	for (const name of HOP_BY_HOP_HEADERS) headers.delete(name);
	return headers;
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

async function withTimeout<T>(
	promise: Promise<T>,
	timeoutMs: number,
	message: string,
): Promise<T> {
	let timer: ReturnType<typeof setTimeout> | undefined;
	try {
		return await Promise.race([
			promise,
			new Promise<never>((_, reject) => {
				timer = setTimeout(() => reject(new Error(message)), timeoutMs);
				timer.unref?.();
			}),
		]);
	} finally {
		if (timer) clearTimeout(timer);
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
	if (endpointNamespace && endpointNamespace !== input.namespace)
		throw new Error(
			`Dynamic App actor endpoint namespace ${endpointNamespace} does not match ${input.namespace}`,
		);
	endpoint.username = "";
	endpoint.password = "";
	return {
		NODE_ENV: "production",
		PORT: String(ACTOR_HTTP_PORT),
		RIVETKIT_RUNTIME: "wasm",
		RIVETKIT_RUNTIME_MODE: "serverless",
		RIVET_ENDPOINT: endpoint.toString().replace(/\/$/u, ""),
		RIVET_NAMESPACE: input.namespace,
		...(endpointToken ? { RIVET_TOKEN: endpointToken } : {}),
		RIVET_POOL: input.pool,
		RIVET_ENVOY_VERSION: String(actorEnvoyVersion(input.key)),
		...(process.env.AGENTOS_DEBUG_HTTP_BRIDGE
			? { AGENTOS_DEBUG_HTTP_BRIDGE: process.env.AGENTOS_DEBUG_HTTP_BRIDGE }
			: {}),
	};
}

function stringEnvironment(env: NodeJS.ProcessEnv): Record<string, string> {
	return Object.fromEntries(
		Object.entries(env).filter(
			(entry): entry is [string, string] => typeof entry[1] === "string",
		),
	);
}

function loopbackExemptPorts(endpoint: string): number[] {
	const url = new URL(endpoint);
	if (
		url.hostname !== "127.0.0.1" &&
		url.hostname !== "localhost" &&
		url.hostname !== "[::1]"
	) {
		return [];
	}
	return [Number(url.port || (url.protocol === "https:" ? 443 : 80))];
}

function actorEnvoyVersion(key: string): number {
	return (
		createHash("sha256").update(key).digest().readUInt32BE(0) & 0x7fffffff || 1
	);
}

function integerEnv(
	env: NodeJS.ProcessEnv,
	name: string,
	fallback: number,
	minimum: number,
	maximum: number,
): number {
	const value = Number(env[name] ?? fallback);
	if (!Number.isSafeInteger(value) || value < minimum || value > maximum)
		throw new Error(`${name} must be an integer from ${minimum} to ${maximum}`);
	return value;
}

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
