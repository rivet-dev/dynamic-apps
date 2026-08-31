import { randomUUID } from "node:crypto";
import { availableParallelism } from "node:os";
import { appsRouter } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";
import { createClient } from "rivetkit/client";
import { resolveDefaultRivetConnection } from "../../../packages/dynamic-apps/src/control-plane.js";
import {
	DynamicAppsExecutor,
	readExecutorConfig,
} from "../../../packages/dynamic-apps/src/executor.js";
import { appRunnerPool } from "../../../packages/dynamic-apps/src/runtime.js";
import {
	ACTOR_BENCHMARK_APP_ID,
	BENCHMARK_APP_ID,
	type BenchmarkDeploymentClient,
	deployActorBenchmarkFixture,
	deployBenchmarkFixture,
} from "./fixture.js";

interface AppHandle {
	resolve(): Promise<string>;
	resolveDeployment(): Promise<unknown>;
}

interface StateClient {
	agentOSAppsApp: {
		getOrCreate(key: string[]): AppHandle;
	};
}

interface ActorApplicationHandle {
	add(amount: number): Promise<number>;
	inspect(): Promise<number>;
	connect(): Promise<{
		on(name: "changed", callback: (value: number) => void): () => void;
		add(amount: number): Promise<number>;
		inspect(): Promise<number>;
		dispose(): Promise<void>;
	}>;
}

interface ActorApplicationClient {
	counter: {
		get(key: string[]): ActorApplicationHandle;
		getOrCreate(key: string[]): ActorApplicationHandle;
	};
	dispose(): Promise<void>;
}

const ACTOR_LOAD_KEY = ["load-v2"];

export function createBenchmarkApplication(): Hono {
	const app = new Hono();
	const benchmarkInstance = randomUUID();
	const benchmarkStartedAt = Date.now();
	app.use("/bench/*", async (c, next) => {
		await next();
		c.header("x-agentos-bench-instance", benchmarkInstance);
		c.header(
			"x-agentos-bench-process-age-ms",
			String(Date.now() - benchmarkStartedAt),
		);
	});
	const baseConfig = readExecutorConfig({
		...process.env,
		DYNAMIC_APPS_TIMING_HEADERS: "1",
	});
	const warm = new DynamicAppsExecutor({
		...baseConfig,
		isolateMode: "prewarm",
		isolatePoolSize: integerEnv("BENCH_WARM_POOL_SIZE", 8, 1, 128),
	});
	const snapshot = new DynamicAppsExecutor({
		...baseConfig,
		isolateMode: "snapshot",
		isolatePoolSize: 0,
	});
	const fresh = new DynamicAppsExecutor({
		...baseConfig,
		isolateMode: "fresh",
		isolatePoolSize: 0,
	});
	const client = createClient() as unknown as StateClient &
		BenchmarkDeploymentClient;
	let actorApplicationDeployment:
		| Promise<Awaited<ReturnType<typeof deployActorBenchmarkFixture>>>
		| undefined;
	let actorApplicationClient: ActorApplicationClient | undefined;
	const getActorApplicationDeployment = () => {
		actorApplicationDeployment ??= deployActorBenchmarkFixture(client).catch(
			(error) => {
				actorApplicationDeployment = undefined;
				throw error;
			},
		);
		return actorApplicationDeployment;
	};
	const getActorApplicationClient = () => {
		const connection = resolveDefaultRivetConnection();
		actorApplicationClient ??= createClient(
			actorApplicationClientConfig({
				namespace: connection.namespace,
				pool: appRunnerPool(ACTOR_BENCHMARK_APP_ID),
			}),
		) as unknown as ActorApplicationClient;
		return actorApplicationClient;
	};
	const privateRegistry = (request: Request) => {
		const headers = new Headers(request.headers);
		headers.set("x-agentos-app-registry-dispatch", "1");
		return appsRouter.fetch(new Request(request, { headers }));
	};
	app.all("/api/rivet", (c) => privateRegistry(c.req.raw));
	app.all("/api/rivet/*", (c) => privateRegistry(c.req.raw));

	app.all("/bench/noop", () => {
		const startedAt = performance.now();
		const response = Response.json({ ok: true, path: "noop" });
		response.headers.set(
			"x-agentos-bench-edge-total-ms",
			(performance.now() - startedAt).toFixed(2),
		);
		return response;
	});

	const direct = (
		executor: DynamicAppsExecutor,
		prefix: string,
		architecture: "warm" | "snapshot" | "fresh",
		request: Request,
	) => {
		const url = new URL(request.url);
		url.pathname = url.pathname.slice(prefix.length) || "/";
		return executor
			.request(BENCHMARK_APP_ID, new Request(url, request))
			.then((response) => {
				response.headers.set("x-agentos-bench-architecture", architecture);
				return response;
			});
	};
	app.all("/bench/warm", (c) => direct(warm, "/bench/warm", "warm", c.req.raw));
	app.all("/bench/warm/*", (c) =>
		direct(warm, "/bench/warm", "warm", c.req.raw),
	);
	app.all("/bench/fresh", (c) =>
		direct(fresh, "/bench/fresh", "fresh", c.req.raw),
	);
	app.all("/bench/fresh/*", (c) =>
		direct(fresh, "/bench/fresh", "fresh", c.req.raw),
	);
	app.all("/bench/snapshot", (c) =>
		direct(snapshot, "/bench/snapshot", "snapshot", c.req.raw),
	);
	app.all("/bench/snapshot/*", (c) =>
		direct(snapshot, "/bench/snapshot", "snapshot", c.req.raw),
	);

	app.all("/bench/actor/resolve", async () => {
		const startedAt = performance.now();
		const handle = client.agentOSAppsApp.getOrCreate([BENCHMARK_APP_ID]);
		const actorId = await handle.resolve();
		const response = Response.json({ ok: true, actorId });
		response.headers.set(
			"x-agentos-bench-actor-resolve-ms",
			(performance.now() - startedAt).toFixed(2),
		);
		return response;
	});
	app.all("/bench/actor/action", async () => {
		const startedAt = performance.now();
		const resolution = await client.agentOSAppsApp
			.getOrCreate([BENCHMARK_APP_ID])
			.resolveDeployment();
		const response = Response.json({ ok: true, resolution });
		response.headers.set(
			"x-agentos-bench-actor-action-ms",
			(performance.now() - startedAt).toFixed(2),
		);
		return response;
	});

	app.post("/bench/setup", async () => {
		const deployment = await deployBenchmarkFixture(client);
		return Response.json({ status: "ready", deployment });
	});
	app.post("/bench/actor-app/setup", async () => {
		const deployment = await getActorApplicationDeployment();
		const actorClient = getActorApplicationClient();
		try {
			await actorClient.counter.get(ACTOR_LOAD_KEY).inspect();
		} catch (error) {
			if (!isActorNotFound(error)) throw error;
			await actorClient.counter.getOrCreate(ACTOR_LOAD_KEY).inspect();
		}
		return Response.json({ status: "ready", deployment });
	});
	app.all("/bench/actor-app/action", async () => {
		const startedAt = performance.now();
		const actorClient = getActorApplicationClient();
		const value = await actorClient.counter.get(ACTOR_LOAD_KEY).add(1);
		const response = Response.json({ ok: true, value });
		response.headers.set(
			"x-agentos-bench-actor-app-action-ms",
			(performance.now() - startedAt).toFixed(2),
		);
		return response;
	});
	app.post("/bench/actor-app/verify", async () => {
		const deployment = await getActorApplicationDeployment();
		const actorClient = createClient(
			actorApplicationClientConfig(deployment),
		) as unknown as ActorApplicationClient;
		try {
			const handle = actorClient.counter.getOrCreate([
				`verify-${Date.now()}-${Math.random()}`,
			]);
			const counter = await handle.connect();
			let unsubscribe = () => {};
			const changed = new Promise<number>((resolve, reject) => {
				const timeout = setTimeout(
					() => reject(new Error("actor event verification timed out")),
					10_000,
				);
				unsubscribe = counter.on("changed", (value) => {
					clearTimeout(timeout);
					resolve(value);
				});
			});
			const first = await counter.add(2);
			const observedEvent = await changed;
			unsubscribe();
			const second = await counter.add(3);
			const current = await counter.inspect();
			await counter.dispose();
			const directResponse = await warm.request(
				ACTOR_BENCHMARK_APP_ID,
				new Request("http://dynamic-app.test/"),
			);
			if (!directResponse.ok) {
				throw new Error(
					`actor app direct HTTP failed with ${directResponse.status}`,
				);
			}
			return Response.json({
				status: "ready",
				appId: ACTOR_BENCHMARK_APP_ID,
				deployment,
				first,
				second,
				current,
				observedEvent,
				direct: await directResponse.json(),
			});
		} finally {
			await actorClient.dispose();
		}
	});
	app.get("/bench/setup", async () => {
		try {
			const deployment = await client.agentOSAppsApp
				.getOrCreate([BENCHMARK_APP_ID])
				.resolveDeployment();
			return Response.json({ status: "ready", deployment });
		} catch (error) {
			const code =
				typeof error === "object" && error !== null && "code" in error
					? String(error.code)
					: undefined;
			if (code === "agentos_apps_not_deployed") {
				return Response.json({ status: "not-started" }, { status: 404 });
			}
			throw error;
		}
	});
	app.get("/bench/info", () =>
		Response.json({
			appId: BENCHMARK_APP_ID,
			benchmarkInstance,
			benchmarkStartedAt,
			process: {
				pid: process.pid,
				uptimeSeconds: process.uptime(),
				memory: process.memoryUsage(),
			},
			cpuParallelism: availableParallelism(),
			warm: warm.diagnostics(),
			snapshot: snapshot.diagnostics(),
			fresh: fresh.diagnostics(),
			paths: [
				"/bench/noop",
				"/bench/actor/resolve",
				"/bench/actor/action",
				"/bench/warm",
				"/bench/snapshot",
				"/bench/fresh",
				"POST /bench/actor-app/setup",
				"POST /bench/actor-app/verify",
				"/bench/actor-app/action",
				"POST /bench/setup",
			],
		}),
	);
	app.get("/health", () => new Response("ok"));

	app.notFound(() => new Response("Not Found", { status: 404 }));
	app.onError((error) => {
		const details = benchmarkErrorDetails(error);
		console.error(
			JSON.stringify({
				event: "dynamic_apps_benchmark_error",
				...details,
				stack: error instanceof Error ? error.stack : undefined,
			}),
		);
		return Response.json(
			{ error: "benchmark failed", ...details },
			{ status: 500 },
		);
	});
	return app;
}

function errorCode(error: unknown): string | undefined {
	if (typeof error !== "object" || error === null || !("code" in error)) {
		return undefined;
	}
	return typeof error.code === "string" ? error.code : undefined;
}

function isActorNotFound(error: unknown): boolean {
	if (typeof error !== "object" || error === null) return false;
	const code = errorCode(error);
	return (
		code === "actor_not_found" ||
		(code === "not_found" && "group" in error && error.group === "actor")
	);
}

export function benchmarkErrorDetails(error: unknown): {
	code?: string;
	message: string;
} {
	const code =
		typeof error === "object" && error !== null && "code" in error
			? String(error.code).slice(0, 128)
			: undefined;
	const rawMessage = error instanceof Error ? error.message : String(error);
	const message = rawMessage
		.replace(/https?:\/\/[^/@\s]+@/giu, "https://[redacted]@")
		.replace(/\b(?:cloud_api|pk|sk)_[a-zA-Z0-9_-]+\b/gu, "[redacted]")
		.slice(0, 1_024);
	return { ...(code ? { code } : {}), message };
}

export function actorApplicationClientConfig(
	deployment: { namespace: string; pool: string },
	rawEndpoint = process.env.RIVET_PUBLIC_ENDPOINT ??
		process.env.RIVET_ENDPOINT ??
		"http://localhost:6420",
): {
	endpoint: string;
	namespace: string;
	poolName: string;
	token?: string;
} {
	const endpoint = new URL(rawEndpoint);
	const endpointToken = endpoint.password
		? decodeURIComponent(endpoint.password)
		: undefined;
	endpoint.username = "";
	endpoint.password = "";
	return {
		endpoint: endpoint.toString().replace(/\/$/u, ""),
		namespace: deployment.namespace,
		poolName: deployment.pool,
		...(endpointToken || process.env.RIVET_TOKEN
			? { token: endpointToken ?? process.env.RIVET_TOKEN }
			: {}),
	};
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
