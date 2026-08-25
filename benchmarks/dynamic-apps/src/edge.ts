import { createHash, randomUUID } from "node:crypto";
import { mkdtemp, open, rm } from "node:fs/promises";
import { availableParallelism, tmpdir } from "node:os";
import { join } from "node:path";
import { AgentOs } from "@rivet-dev/agentos-core";
import { Hono } from "hono";
import { createClient } from "rivetkit/client";
import { BENCHMARK_APP_ID, deployBenchmarkFixture } from "./fixture.js";
import type { registry } from "./registry.js";

const APP_PORT = 3_080;
const MAX_ARTIFACT_CHUNKS = 128;
const MAX_RESPONSE_BYTES = 4 * 1024 * 1024;
const READY_TIMEOUT_MS = 30_000;
const COLD_SIDECAR_POOL = "dynamic-apps-benchmark-cold";
const PROBE_ACTOR_KEY = "routing-probe";
const PROBE_PEER_ACTOR_KEY = "routing-probe-peer";

type ProbeVariant =
	| "resolve"
	| "action-direct"
	| "action-direct-skip"
	| "action-query"
	| "action-query-skip"
	| "fetch-direct"
	| "fetch-direct-skip"
	| "fetch-query"
	| "fetch-query-skip"
	| "nested";

interface ArtifactManifest {
	hash: string;
	bytes: number;
	chunks: number;
	chunkBytes: number;
}

interface AppHandle {
	resolve(): Promise<string>;
	fetch(request: Request): Promise<Response>;
	resolveDeployment(): Promise<{ release: string }>;
	getArtifactManifest(release: string): Promise<ArtifactManifest>;
	readArtifactChunk(release: string, index: number): Promise<Uint8Array>;
}

export interface BenchmarkApplicationOptions {
	registryProxyUrl?: string;
	coldConcurrency?: number;
	client?: ReturnType<typeof createClient<typeof registry>>;
}

export function createBenchmarkApplication(
	options: BenchmarkApplicationOptions = {},
): Hono {
	const app = new Hono();
	const client = options.client ?? createClient<typeof registry>();
	let fixtureDeployment: ReturnType<typeof deployBenchmarkFixture> | undefined;
	let fixtureDeploymentResult:
		| Awaited<ReturnType<typeof deployBenchmarkFixture>>
		| undefined;
	let fixtureDeploymentError: string | undefined;
	const coldSlots = new Semaphore(
		options.coldConcurrency ??
			integerEnv("BENCH_COLD_CONCURRENCY", availableParallelism(), 1, 1_000),
	);

	if (options.registryProxyUrl) {
		const proxy = async (request: Request): Promise<Response> => {
			const target = new URL(request.url);
			const upstream = new URL(
				`${target.pathname}${target.search}`,
				options.registryProxyUrl,
			);
			return fetch(upstream, request);
		};
		app.all("/api/rivet", (c) => proxy(c.req.raw));
		app.all("/api/rivet/*", (c) => proxy(c.req.raw));
	}

	app.all("/bench/noop", (_context) => {
		const startedAt = performance.now();
		const response = Response.json({ ok: true, path: "noop" });
		response.headers.set(
			"x-agentos-bench-edge-total-ms",
			elapsed(startedAt).toFixed(2),
		);
		response.headers.set("x-agentos-bench-path", "noop");
		return response;
	});

	for (const variant of [
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
	] as const) {
		app.all(`/bench/actor/${variant}`, () =>
			forwardActorProbe(client, variant),
		);
	}

	const warm = (request: Request) =>
		forwardWarmRequest(client, request, "/bench/warm", false);
	app.all("/bench/warm", (c) => warm(c.req.raw));
	app.all("/bench/warm/*", (c) => warm(c.req.raw));
	const warmDirect = (request: Request) =>
		forwardWarmRequest(client, request, "/bench/warm-direct", true);
	app.all("/bench/warm-direct", (c) => warmDirect(c.req.raw));
	app.all("/bench/warm-direct/*", (c) => warmDirect(c.req.raw));

	const cold = (request: Request) =>
		coldSlots.run((queueWaitMs) =>
			forwardColdRequest(client, request, queueWaitMs),
		);
	app.all("/bench/cold", (c) => cold(c.req.raw));
	app.all("/bench/cold/*", (c) => cold(c.req.raw));

	app.post("/bench/setup", () => {
		if (!fixtureDeployment && !fixtureDeploymentResult) {
			fixtureDeploymentError = undefined;
			fixtureDeployment = deployBenchmarkFixture();
			void fixtureDeployment.then(
				(result) => {
					fixtureDeploymentResult = result;
				},
				(error) => {
					fixtureDeployment = undefined;
					fixtureDeploymentError =
						error instanceof Error ? error.message : String(error);
				},
			);
		}
		return Response.json(
			{
				status: fixtureDeploymentResult ? "ready" : "started",
				deployment: fixtureDeploymentResult,
			},
			{ status: fixtureDeploymentResult ? 200 : 202 },
		);
	});
	app.get("/bench/setup", () =>
		Response.json({
			status: fixtureDeploymentResult
				? "ready"
				: fixtureDeploymentError
					? "failed"
					: fixtureDeployment !== undefined
						? "pending"
						: "not-started",
			deployment: fixtureDeploymentResult,
			error: fixtureDeploymentError,
		}),
	);

	app.get("/bench/info", () =>
		Response.json({
			appId: BENCHMARK_APP_ID,
			coldBundleCache: false,
			coldConcurrency: coldSlots.capacity,
			coldSidecarPool: COLD_SIDECAR_POOL,
			paths: [
				"/bench/noop",
				"/bench/actor/*",
				"/bench/warm",
				"/bench/warm-direct",
				"/bench/cold",
				"POST /bench/setup",
			],
		}),
	);

	app.notFound(() => new Response("Not Found", { status: 404 }));
	app.onError((error) => {
		console.error(
			JSON.stringify({
				event: "dynamic_apps_benchmark_error",
				message: error instanceof Error ? error.message : String(error),
				stack: error instanceof Error ? error.stack : undefined,
			}),
		);
		return Response.json(
			{
				error: error instanceof Error ? error.message : "benchmark failed",
			},
			{ status: 500 },
		);
	});
	return app;
}

const probeActorIds = new WeakMap<object, Promise<string>>();
const appActorIds = new WeakMap<object, Promise<string>>();

function probeActorId(
	client: ReturnType<typeof createClient<typeof registry>>,
): Promise<string> {
	const existing = probeActorIds.get(client);
	if (existing) return existing;
	const pending = client.benchmarkProbe
		.getOrCreate([PROBE_ACTOR_KEY])
		.resolve();
	probeActorIds.set(client, pending);
	return pending;
}

function appActorId(
	client: ReturnType<typeof createClient<typeof registry>>,
): Promise<string> {
	const existing = appActorIds.get(client);
	if (existing) return existing;
	const pending = client.agentOSAppsApp
		.getOrCreate([BENCHMARK_APP_ID])
		.resolve()
		.catch((error) => {
			appActorIds.delete(client);
			throw error;
		});
	appActorIds.set(client, pending);
	return pending;
}

async function forwardActorProbe(
	client: ReturnType<typeof createClient<typeof registry>>,
	variant: ProbeVariant,
): Promise<Response> {
	const startedAt = performance.now();
	const timing = new Map<string, number>();
	let output: unknown;

	if (variant === "resolve") {
		const handle = client.benchmarkProbe.getOrCreate([PROBE_ACTOR_KEY]);
		output = await measure(timing, "actor-resolve", () => handle.resolve());
	} else if (variant.startsWith("action-direct") || variant === "nested") {
		const actorId = await measure(timing, "actor-id-cache", () =>
			probeActorId(client),
		);
		const handle = client.benchmarkProbe.getForId(actorId);
		output = await measure(timing, "actor-request", () =>
			variant === "nested"
				? handle.action({
						name: "pingPeer",
						args: [PROBE_PEER_ACTOR_KEY],
					})
				: handle.action({
						name: "ping",
						args: [],
						skipReadyWait: variant === "action-direct-skip",
					}),
		);
		if (variant === "nested" && output && typeof output === "object") {
			const nested = output as {
				peerResolveMs?: unknown;
				peerActionMs?: unknown;
			};
			if (typeof nested.peerResolveMs === "number") {
				timing.set("actor-peer-resolve", nested.peerResolveMs);
			}
			if (typeof nested.peerActionMs === "number") {
				timing.set("actor-peer-action", nested.peerActionMs);
			}
		}
	} else if (variant.startsWith("action-query")) {
		const handle = client.benchmarkProbe.getOrCreate([PROBE_ACTOR_KEY]);
		output = await measure(timing, "actor-request", () =>
			handle.action({
				name: "ping",
				args: [],
				skipReadyWait: variant === "action-query-skip",
			}),
		);
	} else if (variant.startsWith("fetch-direct")) {
		const actorId = await measure(timing, "actor-id-cache", () =>
			probeActorId(client),
		);
		const handle = client.benchmarkProbe.getForId(actorId);
		const response = await measure(timing, "actor-request", () =>
			handle.fetch("/ping", {
				skipReadyWait: variant === "fetch-direct-skip",
			}),
		);
		output = await response.json();
		const handlerMs = response.headers.get("x-agentos-bench-actor-handler-ms");
		if (handlerMs !== null) timing.set("actor-handler", Number(handlerMs));
	} else {
		const handle = client.benchmarkProbe.getOrCreate([PROBE_ACTOR_KEY]);
		const response = await measure(timing, "actor-request", () =>
			handle.fetch("/ping", {
				skipReadyWait: variant === "fetch-query-skip",
			}),
		);
		output = await response.json();
		const handlerMs = response.headers.get("x-agentos-bench-actor-handler-ms");
		if (handlerMs !== null) timing.set("actor-handler", Number(handlerMs));
	}

	timing.set("edge-total", elapsed(startedAt));
	const response = Response.json({ ok: true, variant, output });
	for (const [phase, value] of timing)
		setTiming(response.headers, phase, value);
	response.headers.set("x-agentos-bench-path", `actor-${variant}`);
	return response;
}

async function forwardWarmRequest(
	client: ReturnType<typeof createClient<typeof registry>>,
	request: Request,
	prefix: string,
	direct: boolean,
): Promise<Response> {
	const requestStartedAt = performance.now();
	const resolveStartedAt = performance.now();
	const handle = direct
		? (client.agentOSAppsApp.getForId(
				await appActorId(client),
			) as unknown as AppHandle)
		: (client.agentOSAppsApp.getOrCreate([
				BENCHMARK_APP_ID,
			]) as unknown as AppHandle);
	if (!direct) await handle.resolve();
	const resolvedAt = performance.now();
	const upstreamRequest = forwardedRequest(request, prefix);
	const gatewayStartedAt = performance.now();
	const upstream = await handle.fetch(upstreamRequest);
	const gatewayHeadersAt = performance.now();
	const body = new Uint8Array(await upstream.arrayBuffer());
	const gatewayBodyAt = performance.now();
	if (body.byteLength > MAX_RESPONSE_BYTES) {
		throw new Error(`warm response exceeded ${MAX_RESPONSE_BYTES} bytes`);
	}

	const headers = new Headers(upstream.headers);
	stripTransportHeaders(headers);
	setTiming(headers, "edge-resolve", resolvedAt - resolveStartedAt);
	setTiming(
		headers,
		"edge-gateway-headers",
		gatewayHeadersAt - gatewayStartedAt,
	);
	setTiming(headers, "edge-gateway-body", gatewayBodyAt - gatewayHeadersAt);
	setTiming(headers, "edge-total", gatewayBodyAt - requestStartedAt);
	headers.set("x-agentos-bench-path", direct ? "warm-direct" : "warm");
	logRequest("warm", headers, body.byteLength);
	return new Response(body, {
		status: upstream.status,
		statusText: upstream.statusText,
		headers,
	});
}

async function forwardColdRequest(
	client: ReturnType<typeof createClient<typeof registry>>,
	request: Request,
	queueWaitMs: number,
): Promise<Response> {
	const requestStartedAt = performance.now() - queueWaitMs;
	const requestId = randomUUID();
	let directory: string | undefined;
	let vm: AgentOs | undefined;
	const timing = new Map<string, number>();
	timing.set("edge-queue", queueWaitMs);

	try {
		const handle = client.agentOSAppsApp.getOrCreate([
			BENCHMARK_APP_ID,
		]) as unknown as AppHandle;
		await measure(timing, "edge-resolve", () => handle.resolve());
		const deployment = await measure(timing, "cold-deployment", () =>
			handle.resolveDeployment(),
		);
		const manifest = await measure(timing, "cold-manifest", () =>
			handle.getArtifactManifest(deployment.release),
		);
		validateManifest(manifest);

		const chunks = await measure(timing, "cold-download", async () => {
			const downloaded: Uint8Array[] = [];
			let bytes = 0;
			for (let index = 0; index < manifest.chunks; index += 1) {
				const chunk = new Uint8Array(
					await handle.readArtifactChunk(deployment.release, index),
				);
				bytes += chunk.byteLength;
				if (chunk.byteLength > manifest.chunkBytes || bytes > manifest.bytes) {
					throw new Error(`artifact chunk ${index} has an invalid length`);
				}
				downloaded.push(chunk);
			}
			if (bytes !== manifest.bytes) {
				throw new Error(
					`downloaded artifact has ${bytes} bytes; expected ${manifest.bytes}`,
				);
			}
			return downloaded;
		});

		const materialized = await measure(timing, "cold-materialize", async () => {
			directory = await mkdtemp(join(tmpdir(), "dynamic-apps-edge-cold-"));
			const path = join(directory, `${deployment.release}.aospkg`);
			const file = await open(path, "wx", 0o600);
			const digest = createHash("sha256");
			try {
				for (const chunk of chunks) {
					digest.update(chunk);
					await file.writeFile(chunk);
				}
			} finally {
				await file.close();
			}
			const hash = digest.digest("hex");
			if (hash !== manifest.hash) {
				throw new Error(
					`downloaded artifact hash ${hash} did not match ${manifest.hash}`,
				);
			}
			return path;
		});

		const requestVm = await measure(timing, "cold-isolate", () =>
			AgentOs.create({
				sidecar: { kind: "shared", pool: COLD_SIDECAR_POOL },
				// This path launches the built-in Node runtime directly. Projecting the
				// default Unix command bundle adds startup work without serving the request.
				defaultSoftware: false,
				mounts: [
					{
						path: "/app",
						plugin: {
							id: "agentos_packages",
							config: {
								kind: "tar",
								tarPath: materialized,
								root: "/",
								readOnly: true,
							},
						},
						readOnly: true,
					},
				],
				permissions: {
					fs: "allow",
					childProcess: "allow",
					process: "allow",
					env: "allow",
					network: "allow",
				},
				limits: { http: { maxFetchResponseBytes: MAX_RESPONSE_BYTES } },
			}),
		);
		vm = requestVm;
		await measure(timing, "cold-process-spawn", () =>
			requestVm.process.spawn("node", ["/app/main.mjs"], {
				cwd: "/app",
				env: { NODE_ENV: "production" },
				onStderr: (data) =>
					console.error(
						JSON.stringify({
							event: "dynamic_apps_cold_guest_stderr",
							requestId,
							output: new TextDecoder().decode(data).slice(0, 2_048),
						}),
					),
			}),
		);
		const guestUptimeMs = await measure(timing, "cold-guest-ready", () =>
			waitForGuest(requestVm, deployment.release),
		);
		timing.set("cold-guest-uptime", guestUptimeMs);
		timing.set(
			"cold-node-bootstrap",
			Math.max(0, (timing.get("cold-guest-ready") ?? 0) - guestUptimeMs),
		);

		const guestRequest = forwardedRequest(request, "/bench/cold");
		const guestResponse = await measure(timing, "cold-dispatch-headers", () =>
			requestVm.fetchStreamStart(APP_PORT, guestRequest),
		);
		const body = await measure(timing, "cold-response-body", async () => {
			const parts: Uint8Array[] = [];
			let bytes = 0;
			for (;;) {
				const chunk = await requestVm.fetchStreamRead(guestResponse.streamId);
				bytes += chunk.body.byteLength;
				if (bytes > MAX_RESPONSE_BYTES) {
					await requestVm.fetchStreamCancel(guestResponse.streamId);
					throw new Error(`cold response exceeded ${MAX_RESPONSE_BYTES} bytes`);
				}
				if (chunk.body.byteLength > 0) parts.push(chunk.body);
				if (chunk.done) return new Uint8Array(Buffer.concat(parts, bytes));
			}
		});

		if (!directory) throw new Error("cold artifact directory was not created");
		const artifactDirectory = directory;
		await measure(timing, "cold-dispose", async () => {
			await requestVm.dispose();
			vm = undefined;
			await rm(artifactDirectory, { recursive: true, force: true });
			directory = undefined;
		});
		const edgeTotalMs = performance.now() - requestStartedAt;
		timing.set("edge-total", edgeTotalMs);

		const headers = new Headers(guestResponse.headers);
		stripTransportHeaders(headers);
		for (const [phase, value] of timing) setTiming(headers, phase, value);
		headers.set("x-agentos-bench-path", "cold");
		headers.set("x-agentos-app-cold-start", "1");
		headers.set("x-agentos-app-cold-start-ms", edgeTotalMs.toFixed(2));
		headers.set("x-agentos-app-replica", `edge-cold/${requestId}`);
		headers.set("x-agentos-app-replica-count", "1");
		headers.set("x-agentos-app-queue-delay-ms", queueWaitMs.toFixed(2));
		headers.set("x-agentos-bench-artifact-bytes", String(manifest.bytes));
		headers.set("x-agentos-bench-artifact-chunks", String(manifest.chunks));
		logRequest("cold", headers, body.byteLength);
		return new Response(body, {
			status: guestResponse.status,
			statusText: guestResponse.statusText,
			headers,
		});
	} finally {
		await Promise.allSettled([
			vm?.dispose(),
			directory
				? rm(directory, { recursive: true, force: true })
				: Promise.resolve(),
		]);
	}
}

function forwardedRequest(request: Request, prefix: string): Request {
	const source = new URL(request.url);
	const suffix = source.pathname.slice(prefix.length);
	source.pathname = suffix === "" ? "/" : suffix;
	const headers = new Headers(request.headers);
	headers.delete("x-rivet-token");
	headers.delete("connection");
	headers.delete("transfer-encoding");
	return new Request(source, {
		method: request.method,
		headers,
		body:
			request.method === "GET" || request.method === "HEAD"
				? undefined
				: request.body,
		duplex: request.body ? "half" : undefined,
	} as RequestInit);
}

async function waitForGuest(vm: AgentOs, release: string): Promise<number> {
	const deadline = performance.now() + READY_TIMEOUT_MS;
	for (;;) {
		try {
			const response = await vm.fetch(
				APP_PORT,
				new Request("http://dynamic-app/.agentos/ready"),
			);
			if (response.ok) {
				const body = (await response.json()) as {
					release?: unknown;
					guestUptimeMs?: unknown;
				};
				if (
					body.release === release &&
					typeof body.guestUptimeMs === "number" &&
					Number.isFinite(body.guestUptimeMs)
				) {
					return body.guestUptimeMs;
				}
			}
		} catch {}
		if (performance.now() >= deadline) {
			throw new Error(
				`cold guest did not become ready in ${READY_TIMEOUT_MS}ms`,
			);
		}
		await new Promise((resolve) => setTimeout(resolve, 1));
	}
}

function validateManifest(manifest: ArtifactManifest): void {
	if (
		!Number.isInteger(manifest.chunks) ||
		manifest.chunks <= 0 ||
		manifest.chunks > MAX_ARTIFACT_CHUNKS ||
		!Number.isInteger(manifest.bytes) ||
		manifest.bytes <= 0 ||
		!Number.isInteger(manifest.chunkBytes) ||
		manifest.chunkBytes <= 0 ||
		!/^[a-f0-9]{64}$/.test(manifest.hash)
	) {
		throw new Error("app actor returned an invalid artifact manifest");
	}
}

async function measure<T>(
	timing: Map<string, number>,
	name: string,
	operation: () => Promise<T>,
): Promise<T> {
	const startedAt = performance.now();
	try {
		return await operation();
	} finally {
		timing.set(name, elapsed(startedAt));
	}
}

function setTiming(headers: Headers, phase: string, value: number): void {
	headers.set(`x-agentos-bench-${phase}-ms`, value.toFixed(2));
}

function stripTransportHeaders(headers: Headers): void {
	for (const name of [
		"connection",
		"content-length",
		"keep-alive",
		"transfer-encoding",
		"upgrade",
	]) {
		headers.delete(name);
	}
}

function logRequest(path: "warm" | "cold", headers: Headers, bytes: number) {
	if (process.env.BENCH_LOG_REQUESTS !== "1") return;
	const headerTiming: Record<string, number> = {};
	headers.forEach((value, name) => {
		if (name.startsWith("x-agentos-bench-") && name.endsWith("-ms")) {
			headerTiming[name] = Number(value);
		}
	});
	console.log(
		JSON.stringify({
			event: "dynamic_apps_benchmark_request",
			path,
			bytes,
			timing: headerTiming,
		}),
	);
}

function elapsed(startedAt: number): number {
	return performance.now() - startedAt;
}

function integerEnv(
	name: string,
	fallback: number,
	minimum: number,
	maximum: number,
): number {
	const value = Number(process.env[name] ?? fallback);
	if (!Number.isInteger(value) || value < minimum || value > maximum) {
		throw new Error(`${name} must be between ${minimum} and ${maximum}`);
	}
	return value;
}

class Semaphore {
	readonly capacity: number;
	#active = 0;
	#waiting: Array<() => void> = [];

	constructor(capacity: number) {
		this.capacity = capacity;
	}

	async run<T>(operation: (queueWaitMs: number) => Promise<T>): Promise<T> {
		const queuedAt = performance.now();
		if (this.#active >= this.capacity) {
			await new Promise<void>((resolve) => this.#waiting.push(resolve));
		}
		this.#active += 1;
		try {
			return await operation(elapsed(queuedAt));
		} finally {
			this.#active -= 1;
			this.#waiting.shift()?.();
		}
	}
}
