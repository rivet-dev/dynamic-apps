import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { Hono } from "hono";
import { afterEach, describe, expect, test } from "vitest";
import {
	actorWorkerEnvironment,
	DynamicActorRuntime,
} from "../src/actor-runtime.js";
import type { AppRouteResolution } from "../src/actors.js";
import { forwardActorCallbackRequest } from "../src/actors.js";
import { resolveDefaultRivetConnection } from "../src/control-plane.js";
import { deployApp } from "../src/deploy.js";
import {
	capExecutionConcurrencyForMemory,
	DynamicAppsExecutor,
	type ExecutorConfig,
	readExecutorConfig,
} from "../src/executor.js";
import {
	type DynamicAppsLogEvent,
	setDynamicAppsLogHandler,
} from "../src/logging.js";
import { appsRouter, setRouterRequestOverride } from "../src/router.js";
import {
	canonicalDeploymentHash,
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
	directRunnerSource,
	normalizeAppPath,
} from "../src/runtime.js";
import { prepareSource } from "../src/source.js";

const execFileAsync = promisify(execFile);

afterEach(() => {
	setRouterRequestOverride();
	setDynamicAppsLogHandler(undefined);
});

describe("retained public surface", () => {
	test("rewrites a prefix-mounted application request", async () => {
		const server = new Hono();
		let observed: { appId: string; request: Request } | undefined;
		setRouterRequestOverride(async (appId, request) => {
			observed = { appId, request };
			return new Response("ok");
		});
		server.route("/apps", appsRouter);
		const response = await server.request(
			"http://example.test/apps/demo/nested?q=1",
			{
				method: "POST",
				headers: { "x-test": "yes" },
				body: "body",
			},
		);
		expect(response.status).toBe(200);
		expect(observed?.appId).toBe("demo");
		expect(observed && new URL(observed.request.url).pathname).toBe("/nested");
		expect(observed && new URL(observed.request.url).search).toBe("?q=1");
		expect(observed?.request.method).toBe("POST");
		expect(await observed?.request.text()).toBe("body");
	});

	test("redirects a bare app path before executor lookup", async () => {
		let calls = 0;
		setRouterRequestOverride(async () => {
			calls += 1;
			return new Response("unexpected");
		});
		const response = await appsRouter.request("/demo?x=1");
		expect(response.status).toBe(308);
		expect(response.headers.get("location")).toBe("http://localhost/demo/?x=1");
		expect(calls).toBe(0);
	});

	test("rejects invalid app ids before executor lookup", async () => {
		let calls = 0;
		setRouterRequestOverride(async () => {
			calls += 1;
			return new Response("unexpected");
		});
		const response = await appsRouter.request("/INVALID/");
		expect(response.status).toBe(400);
		expect(await response.json()).toMatchObject({
			error: { code: "agentos_apps_invalid_app_id" },
		});
		expect(calls).toBe(0);
	});

	test("preserves deployApp's injected actor call and result", async () => {
		let key: string | string[] | undefined;
		let prepared: unknown;
		const result = await deployApp(
			{
				appId: "demo",
				files: {
					"package.json": '{"type":"module","main":"index.js"}',
					"index.js": "export default { fetch() {} }",
				},
				regions: ["us-west"],
			},
			{
				client: {
					agentOSAppsApp: {
						getOrCreate(value) {
							key = value;
							return {
								async deploy(input) {
									prepared = input;
									return {
										appId: "demo",
										release: "release-1",
										endpoint: "https://api.example.test",
										namespace: "app-demo",
										pool: "agentos-apps-demo",
										token: "pk_demo",
										regions: ["us-west"],
										appActorId: "actor-1",
										usesRivetKit: false,
									};
								},
							};
						},
					},
				},
			},
		);
		expect(key).toEqual(["demo"]);
		expect(Object.keys(result)).toEqual([
			"appId",
			"release",
			"endpoint",
			"namespace",
			"pool",
			"token",
			"regions",
		]);
		expect(prepared).toMatchObject({ appId: "demo", regions: ["us-west"] });
	});

	test("deploys through an existing stable actor before creating one", async () => {
		const calls: string[] = [];
		await deployApp(
			{
				appId: "demo",
				files: {
					"package.json": '{"type":"module","main":"index.js"}',
					"index.js": "export default { fetch() {} }",
				},
			},
			{
				client: {
					agentOSAppsApp: {
						get() {
							calls.push("get");
							return {
								async deploy(input) {
									return deploymentResult(input);
								},
							};
						},
						getOrCreate() {
							calls.push("getOrCreate");
							throw new Error("should not create an existing app actor");
						},
					},
				},
			},
		);
		expect(calls).toEqual(["get"]);
	});

	test("creates the stable actor when an existing deployment actor is absent", async () => {
		const calls: string[] = [];
		await deployApp(
			{
				appId: "demo",
				files: {
					"package.json": '{"type":"module","main":"index.js"}',
					"index.js": "export default { fetch() {} }",
				},
			},
			{
				client: {
					agentOSAppsApp: {
						get() {
							calls.push("get");
							return {
								async deploy() {
									throw Object.assign(new Error("missing"), {
										code: "not_found",
										group: "actor",
									});
								},
							};
						},
						getOrCreate() {
							calls.push("getOrCreate");
							return {
								async deploy(input) {
									return deploymentResult(input);
								},
							};
						},
					},
				},
			},
		);
		expect(calls).toEqual(["get", "getOrCreate"]);
	});

	test("keeps the optional runner control token separate", () => {
		const previousEndpoint = process.env.RIVET_ENDPOINT;
		const previousControlToken = process.env.DYNAMIC_APPS_CONTROL_TOKEN;
		process.env.RIVET_ENDPOINT =
			"https://test-namespace:runtime-token@api.example.test";
		process.env.DYNAMIC_APPS_CONTROL_TOKEN = "control-token";
		try {
			expect(resolveDefaultRivetConnection()).toEqual({
				endpoint: "https://api.example.test",
				namespace: "test-namespace",
				token: "control-token",
			});
		} finally {
			if (previousEndpoint === undefined) delete process.env.RIVET_ENDPOINT;
			else process.env.RIVET_ENDPOINT = previousEndpoint;
			if (previousControlToken === undefined) {
				delete process.env.DYNAMIC_APPS_CONTROL_TOKEN;
			} else {
				process.env.DYNAMIC_APPS_CONTROL_TOKEN = previousControlToken;
			}
		}
	});

	test("separates public endpoint auth for an app actor worker", () => {
		expect(
			actorWorkerEnvironment({
				endpoint: "https://app-namespace:pk_example@api.rivet.dev",
				key: "release:hash",
				namespace: "app-namespace",
				pool: "app-pool",
			}),
		).toMatchObject({
			RIVETKIT_RUNTIME: "wasm",
			RIVET_ENDPOINT: "https://api.rivet.dev",
			RIVET_NAMESPACE: "app-namespace",
			RIVET_TOKEN: "pk_example",
			RIVET_POOL: "app-pool",
		});
	});
});

function deploymentResult(_input: unknown) {
	return {
		appId: "demo",
		release: "release-1",
		endpoint: "https://api.example.test",
		namespace: "app-demo",
		pool: "agentos-apps-demo",
		token: "pk_demo",
		regions: ["default"],
		appActorId: "actor-1",
		usesRivetKit: false,
	};
}

describe("source and runtime contract", () => {
	test("copies and deterministically orders generated files", async () => {
		const bytes = new Uint8Array([1, 2, 3]);
		const files = await prepareSource({
			appId: "demo",
			files: { "z.js": bytes, "a.js": "a" },
		});
		bytes[0] = 9;
		expect(Object.keys(files)).toEqual(["a.js", "z.js"]);
		expect([...files["z.js"]]).toEqual([1, 2, 3]);
	});

	test("normalizes paths and hashes content boundaries", () => {
		expect(normalizeAppPath("src/./index.js")).toBe("src/index.js");
		expect(() => normalizeAppPath("../secret")).toThrow();
		const base = {
			entrypoint: "index.js",
			build: false,
			packagingIdentity: "test",
		};
		expect(
			canonicalDeploymentHash({
				...base,
				files: { "a.js": new Uint8Array([1]) },
			}),
		).not.toBe(
			canonicalDeploymentHash({
				...base,
				files: { "a.js": new Uint8Array([2]) },
			}),
		);
	});

	test("emits only the direct dispatcher contract", () => {
		const source = directRunnerSource({
			entrypoint: "index.js",
			release: "release-1",
			maxResponseBytes: 1024,
		});
		expect(source).toContain("export async function dispatch");
		expect(source).not.toContain("listen(");
		expect(source).not.toContain("createServer");
	});

	test("supports ephemeral and pooled execution config", () => {
		expect(
			readExecutorConfig({ DYNAMIC_APPS_EXECUTION_MODE: "ephemeral" })
				.executionMode,
		).toBe("ephemeral");
		expect(
			readExecutorConfig({
				DYNAMIC_APPS_EXECUTION_MODE: "pooled",
				DYNAMIC_APPS_CONTEXT_POOL_SIZE: "0",
			}).executionMode,
		).toBe("pooled");
	});

	test("caps agentOS context admission below a finite cgroup high-water mark", () => {
		expect(
			capExecutionConcurrencyForMemory({
				requested: 32,
				contextHeapLimitMb: 64,
				memoryHighWaterPercent: 70,
				currentBytes: 128 * 1024 * 1024,
				maxBytes: 512 * 1024 * 1024,
			}),
		).toBe(1);
	});
});

describe("direct agentOS execution", () => {
	test("does not publish a runtime that finishes preparing during shutdown", async () => {
		const artifact = await makeArtifact("shutdown-prepare");
		let releaseChunk = () => {};
		const chunkGate = new Promise<void>((resolve) => {
			releaseChunk = resolve;
		});
		let markChunkStarted = () => {};
		const chunkStarted = new Promise<void>((resolve) => {
			markChunkStarted = resolve;
		});
		const fake = fakeStateClient(artifact, {
			beforeChunk: async () => {
				markChunkStarted();
				await chunkGate;
			},
		});
		const executor = new DynamicAppsExecutor(
			executorConfig("pooled"),
			fake.client,
		);
		const outcome = executor
			.request("demo", new Request("http://example.test/shutdown"))
			.then(
				() => "response" as const,
				() => "rejected" as const,
			);
		try {
			await chunkStarted;
			const dispose = executor.dispose();
			releaseChunk();
			await dispose;
			expect(await outcome).toBe("rejected");
			expect(executor.diagnostics()).toMatchObject({
				runtimes: 0,
				pooledContexts: 0,
				poolReservations: 0,
			});
		} finally {
			releaseChunk();
			await executor.dispose();
			await artifact.dispose();
		}
	});

	test("times out an asynchronous handler that never settles", async () => {
		const artifact = await makeArtifact(
			"async-stall",
			`export async function dispatch() {
	  return new Promise(() => {});
	}`,
		);
		const fake = fakeStateClient(artifact);
		const config = { ...executorConfig("pooled"), executionTimeoutMs: 50 };
		const executor = new DynamicAppsExecutor(config, fake.client);
		try {
			const outcome = await Promise.race([
				executor.request("demo", new Request("http://example.test/stall")).then(
					() => "response" as const,
					(error: unknown) =>
						String(error).includes("execution exceeded")
							? ("timeout" as const)
							: Promise.reject(error),
				),
				new Promise<"hung">((resolve) =>
					setTimeout(() => resolve("hung"), 1_000),
				),
			]);
			expect(outcome).toBe("timeout");
		} finally {
			await executor.dispose();
			await artifact.dispose();
		}
	}, 5_000);

	test("round-trips binary request bodies through the agentOS envelope", async () => {
		const artifact = await makeArtifact("binary");
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(
			executorConfig("pooled"),
			fake.client,
		);
		const input = Uint8Array.from(
			{ length: 65_537 },
			(_, index) => index % 256,
		);
		try {
			const response = await executor.request(
				"demo",
				new Request("http://example.test/binary", {
					method: "POST",
					body: input,
				}),
			);
			expect(await response.json()).toMatchObject({
				requestBodyBase64: Buffer.from(input).toString("base64"),
			});
		} finally {
			await executor.dispose();
			await artifact.dispose();
		}
	});

	test.each([
		"ephemeral",
		"pooled",
	] as const)("starts every request clean in %s mode", async (mode) => {
		const artifact = await makeArtifact("one");
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(executorConfig(mode), fake.client);
		try {
			const first = await executor.request(
				"demo",
				new Request("http://example.test/path?q=1", {
					headers: {
						authorization: "Bearer user-token",
						"x-rivet-token": "private-token",
					},
				}),
			);
			const second = await executor.request(
				"demo",
				new Request("http://example.test/path?q=2"),
			);
			expect(await first.json()).toMatchObject({
				counter: 1,
				marker: "one",
				path: "/path?q=1",
				authorization: "Bearer user-token",
				privateToken: null,
			});
			expect(await second.json()).toMatchObject({
				counter: 1,
				marker: "one",
				path: "/path?q=2",
			});
			expect(fake.calls.resolve).toBe(1);
			expect(fake.calls.manifest).toBe(1);
			expect(fake.calls.chunk).toBe(1);
		} finally {
			await executor.dispose();
			await artifact.dispose();
		}
	}, 120_000);

	test("reuses bounded retained contexts while resetting module state", async () => {
		const artifact = await makeArtifact("reuse");
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(
			executorConfig("pooled"),
			fake.client,
		);
		try {
			for (let index = 0; index < 20; index += 1) {
				const response = await executor.request(
					"demo",
					new Request(`http://example.test/${index}`),
				);
				expect((await response.json()).counter).toBe(1);
				await waitForCleanPool(executor, 2);
			}
			expect(executor.diagnostics()).toMatchObject({
				cleanContexts: 2,
				vmCreates: 1,
				contextCreates: 2,
				contextDisposes: 0,
				evaluations: 20,
			});
		} finally {
			await executor.dispose();
			await artifact.dispose();
		}
	});

	test("admits a request before buffering its body", async () => {
		const artifact = await makeArtifact("admission");
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(
			{
				...executorConfig("pooled"),
				executionConcurrency: 1,
				executionQueueSize: 0,
			},
			fake.client,
		);
		let releaseFirstBody = () => {};
		const firstBodyGate = new Promise<void>((resolve) => {
			releaseFirstBody = resolve;
		});
		let firstBodyPulled = false;
		let secondBodyPulls = 0;
		try {
			await executor.request("demo", new Request("http://example.test/warm"));
			const first = executor.request(
				"demo",
				streamingRequest("http://example.test/first", {
					async pull(controller) {
						if (firstBodyPulled) return;
						firstBodyPulled = true;
						controller.enqueue(new Uint8Array([1]));
						await firstBodyGate;
						controller.close();
					},
				}),
			);
			await waitFor(() => firstBodyPulled);
			const second = executor.request(
				"demo",
				streamingRequest("http://example.test/second", {
					pull(controller) {
						secondBodyPulls += 1;
						controller.close();
					},
				}),
			);
			const secondError = second.then(
				() => undefined,
				(error: unknown) => error,
			);
			await new Promise((resolve) => setTimeout(resolve, 25));
			expect(secondBodyPulls).toBe(0);
			releaseFirstBody();
			expect((await first).status).toBe(200);
			expect(await secondError).toMatchObject({
				code: "agentos_apps_no_capacity",
			});
		} finally {
			releaseFirstBody();
			await executor.dispose();
			await artifact.dispose();
		}
	});

	test("bounds the total retained context cache across applications", async () => {
		const artifacts = await Promise.all(
			Array.from({ length: 6 }, (_, index) => makeArtifact(`multi-${index}`)),
		);
		const fake = fakeMultiStateClient(
			new Map(
				artifacts.map((artifact, index) => [`app-${index}`, artifact] as const),
			),
		);
		const executor = new DynamicAppsExecutor(
			{
				...executorConfig("pooled"),
				runtimeCacheMaxEntries: artifacts.length,
				contextPoolMaxTotal: 4,
			} as ExecutorConfig,
			fake.client,
		);
		try {
			for (let index = 0; index < artifacts.length; index += 1) {
				const response = await executor.request(
					`app-${index}`,
					new Request(`http://example.test/${index}`),
				);
				expect(response.status).toBe(200);
			}
			expect(executor.diagnostics()).toMatchObject({
				runtimes: artifacts.length,
				cleanContexts: 4,
			});
		} finally {
			await executor.dispose();
			await Promise.all(artifacts.map((artifact) => artifact.dispose()));
		}
	});

	test("runs supported Node builtins inside the sandbox", async () => {
		const artifact = await makeArtifact(
			"node-api",
			`import { basename } from "node:path";
export async function dispatch(input) {
  return { status: 200, statusText: "OK", headers: [], bodyBase64: Buffer.from(basename(new URL(input.url).pathname)).toString("base64") };
}`,
		);
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(
			executorConfig("ephemeral"),
			fake.client,
		);
		try {
			const response = await executor.request(
				"demo",
				new Request("http://example.test/path/file.txt"),
			);
			expect(await response.text()).toBe("file.txt");
		} finally {
			await executor.dispose();
			await artifact.dispose();
		}
	});

	test("attributes application output and request summaries", async () => {
		const events: Readonly<DynamicAppsLogEvent>[] = [];
		setDynamicAppsLogHandler((event) => events.push(event));
		const artifact = await makeArtifact(
			"logging",
			`export async function dispatch() {
  console.log("application stdout");
  console.error("application stderr");
  return { status: 200, statusText: "OK", headers: [], bodyBase64: "" };
}`,
		);
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(
			{ ...executorConfig("ephemeral"), logRequests: true },
			fake.client,
		);
		try {
			const response = await executor.request(
				"demo",
				new Request("http://example.test/log", {
					headers: { authorization: "Bearer never-log-this" },
				}),
				"request-test",
			);
			expect(response.status).toBe(200);
			expect(events).toEqual(
				expect.arrayContaining([
					expect.objectContaining({
						source: "application",
						stream: "stdout",
						message: "application stdout",
						appId: "demo",
						requestId: "request-test",
					}),
					expect.objectContaining({
						source: "application",
						stream: "stderr",
						message: "application stderr",
					}),
					expect.objectContaining({
						source: "runtime",
						message: "Dynamic Apps request completed",
						requestId: "request-test",
					}),
				]),
			);
			expect(JSON.stringify(events)).not.toContain("never-log-this");
		} finally {
			await executor.dispose();
			await artifact.dispose();
		}
	});
});

describe("actor callback resource limits", () => {
	test("does not publish a worker that finishes creating during shutdown", async () => {
		const artifact = await makeActorArtifact(`
export const registry = { handler: () => new Response("ok") };
`);
		const runtime = new DynamicActorRuntime();
		let releaseArtifact = () => {};
		const artifactGate = new Promise<void>((resolve) => {
			releaseArtifact = resolve;
		});
		let markLoadStarted = () => {};
		const loadStarted = new Promise<void>((resolve) => {
			markLoadStarted = resolve;
		});
		const outcome = runtime
			.request({
				key: "shutdown-create",
				loadArtifact: async () => {
					markLoadStarted();
					await artifactGate;
					return artifact.bytes;
				},
				endpoint: "http://example.test",
				namespace: "test",
				pool: "default",
				request: new Request("http://example.test/shutdown", {
					method: "POST",
				}),
			})
			.then(
				() => "response" as const,
				() => "rejected" as const,
			);
		try {
			await loadStarted;
			const dispose = runtime.dispose();
			releaseArtifact();
			await dispose;
			expect(await outcome).toBe("rejected");
			expect(runtime.diagnostics()).toMatchObject({
				entries: 0,
				creating: 0,
				workerReservations: 0,
			});
		} finally {
			releaseArtifact();
			await runtime.dispose();
			await artifact.dispose();
		}
	});

	test("bounds concurrent actor workers as well as idle cache entries", async () => {
		const artifact = await makeActorArtifact(`
export const registry = {
  async handler() {
    await new Promise((resolve) => setTimeout(resolve, 100));
    return new Response("ok");
  },
};
`);
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES: "4",
		});
		const outcomes = Array.from({ length: 16 }, (_, index) =>
			runtime
				.request({
					key: `bounded-${index}`,
					loadArtifact: async () => artifact.bytes,
					endpoint: "http://example.test",
					namespace: "test",
					pool: "default",
					request: new Request(`http://example.test/${index}`, {
						method: "POST",
					}),
				})
				.then(
					(response) => response.arrayBuffer(),
					() => undefined,
				),
		);
		try {
			await new Promise((resolve) => setTimeout(resolve, 50));
			const diagnostics = runtime.diagnostics();
			expect(diagnostics.entries + diagnostics.creating).toBeLessThanOrEqual(4);
		} finally {
			await Promise.allSettled(outcomes);
			await runtime.dispose();
			await artifact.dispose();
		}
	}, 5_000);

	test("times out an actor handler that blocks its worker", async () => {
		const artifact = await makeActorArtifact(`
export const registry = {
  handler() {
    while (true) {}
  },
};
`);
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_REQUEST_TIMEOUT_MS: "50",
			DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS: "1000",
		});
		try {
			const outcome = await Promise.race([
				runtime
					.request({
						key: "handler-stall",
						loadArtifact: async () => artifact.bytes,
						endpoint: "http://example.test",
						namespace: "test",
						pool: "default",
						request: new Request("http://example.test/stall", {
							method: "POST",
						}),
					})
					.then(
						() => "response" as const,
						(error: unknown) =>
							String(error).includes("request exceeded")
								? ("timeout" as const)
								: Promise.reject(error),
					),
				new Promise<"hung">((resolve) =>
					setTimeout(() => resolve("hung"), 250),
				),
			]);
			expect(outcome).toBe("timeout");
		} finally {
			await runtime.dispose();
			await artifact.dispose();
		}
	}, 5_000);

	test("allows an actor response stream to outlive the handler timeout", async () => {
		const artifact = await makeActorArtifact(`
export const registry = {
  handler() {
    return new Response(new ReadableStream({
      async start(controller) {
        controller.enqueue(new TextEncoder().encode("started-"));
        await new Promise((resolve) => setTimeout(resolve, 100));
        controller.enqueue(new TextEncoder().encode("finished"));
        controller.close();
      },
    }));
  },
};
`);
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_REQUEST_TIMEOUT_MS: "50",
		});
		try {
			const response = await runtime.request({
				key: "long-response",
				loadArtifact: async () => artifact.bytes,
				endpoint: "http://example.test",
				namespace: "test",
				pool: "default",
				request: new Request("http://example.test/stream", {
					method: "POST",
				}),
			});
			expect(await response.text()).toBe("started-finished");
		} finally {
			await runtime.dispose();
			await artifact.dispose();
		}
	}, 5_000);

	test("forwards actor callback bodies without eager buffering", async () => {
		let pulls = 0;
		const source = streamingRequest("http://example.test/api/rivet/start", {
			pull(controller) {
				pulls += 1;
				controller.enqueue(Uint8Array.of(1, 2, 3));
				controller.close();
			},
		});
		const forwarded = forwardActorCallbackRequest(source, "/start");
		expect(pulls).toBe(0);
		expect(new URL(forwarded.url).pathname).toBe("/start");
		expect(new Uint8Array(await forwarded.arrayBuffer())).toEqual(
			Uint8Array.of(1, 2, 3),
		);
		expect(pulls).toBe(1);
	});

	test("preserves actor worker error stacks and causes", async () => {
		const artifact = await makeActorArtifact(`
export const registry = {
  async handler() {
    throw new Error("outer failure", { cause: new Error("inner failure") });
  },
};
`);
		const runtime = new DynamicActorRuntime();
		try {
			await expect(
				runtime.request({
					key: "worker-error",
					loadArtifact: async () => artifact.bytes,
					endpoint: "http://example.test",
					namespace: "test",
					pool: "default",
					request: new Request("http://example.test/start", {
						method: "POST",
					}),
				}),
			).rejects.toThrow(/outer failure[\s\S]*Caused by:[\s\S]*inner failure/u);
		} finally {
			await runtime.dispose();
			await artifact.dispose();
		}
	});

	test("admits actor callback bodies before reading them", async () => {
		const artifact = await makeActorArtifact(`
export const registry = {
  async handler() {
    return new Response("ok");
  },
};
`);
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_REQUEST_CONCURRENCY: "4",
			DYNAMIC_APPS_ACTOR_REQUEST_QUEUE_SIZE: "4",
			DYNAMIC_APPS_ACTOR_REQUEST_QUEUE_WAIT_MS: "1000",
		});
		let release = () => {};
		const gate = new Promise<void>((resolve) => {
			release = resolve;
		});
		let pulls = 0;
		const outcomes = Array.from({ length: 16 }, (_, index) =>
			runtime
				.request({
					key: "admission",
					loadArtifact: async () => artifact.bytes,
					endpoint: "http://example.test",
					namespace: "test",
					pool: "default",
					request: streamingRequest(`http://example.test/${index}`, {
						async pull(controller) {
							pulls += 1;
							await gate;
							controller.close();
						},
					}),
				})
				.then(
					(response) => response.arrayBuffer(),
					() => undefined,
				),
		);
		try {
			await waitFor(() => pulls >= 4);
			await new Promise((resolve) => setTimeout(resolve, 20));
			expect(pulls).toBe(4);
		} finally {
			release();
			await Promise.allSettled(outcomes);
			await runtime.dispose();
			await artifact.dispose();
		}
	});

	test("stops reading a callback body when it crosses the limit", async () => {
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES: "1024",
		});
		let pulls = 0;
		let cancelled = false;
		const request = streamingRequest("http://example.test/start", {
			pull(controller) {
				pulls += 1;
				controller.enqueue(new Uint8Array(1024));
				if (pulls === 64) controller.close();
			},
			cancel() {
				cancelled = true;
			},
		});
		try {
			const response = await runtime.request({
				key: "oversized",
				loadArtifact: async () => {
					throw new Error("oversized input must fail before artifact loading");
				},
				endpoint: "http://example.test",
				namespace: "test",
				pool: "default",
				request,
			});
			expect(response.status).toBe(413);
			expect(pulls).toBeLessThanOrEqual(2);
			expect(cancelled).toBe(true);
		} finally {
			await runtime.dispose();
		}
	});

	test("fails an actor worker whose startup never completes", async () => {
		const artifact = await makeActorArtifact("while (true) {}");
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS: "50",
		});
		try {
			const outcome = await Promise.race([
				runtime
					.request({
						key: "stalled",
						loadArtifact: async () => artifact.bytes,
						endpoint: "http://example.test",
						namespace: "test",
						pool: "default",
						request: new Request("http://example.test/start", {
							method: "POST",
						}),
					})
					.then(
						() => ({ status: "response" as const }),
						(error: unknown) => ({ status: "error" as const, error }),
					),
				new Promise<{ status: "hung" }>((resolve) =>
					setTimeout(() => resolve({ status: "hung" }), 250),
				),
			]);
			expect(outcome).toMatchObject({ status: "error" });
			if (outcome.status === "error") {
				expect(String(outcome.error)).toContain("startup exceeded");
			}
		} finally {
			await runtime.dispose();
			await artifact.dispose();
		}
	});

	test("serves concurrent actor worker cache churn without losing requests", async () => {
		const artifact = await makeActorArtifact(`
export const registry = {
  async handler() {
    return new Response("ok");
  },
};
`);
		const runtime = new DynamicActorRuntime({
			DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES: "4",
			DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS: "1000",
		});
		try {
			const outcome = await Promise.race([
				Promise.all(
					Array.from({ length: 8 }, async (_, index) => {
						try {
							const response = await runtime.request({
								key: `churn-${index}`,
								loadArtifact: async () => artifact.bytes,
								endpoint: "http://example.test",
								namespace: "test",
								pool: "default",
								request: new Request(`http://example.test/${index}`, {
									method: "POST",
								}),
							});
							expect(await response.text()).toBe("ok");
						} catch (error) {
							expect(error).toMatchObject({
								code: "agentos_apps_no_capacity",
							});
						}
					}),
				).then(() => "complete" as const),
				new Promise<"hung">((resolve) =>
					setTimeout(() => resolve("hung"), 1_000),
				),
			]);
			expect(outcome).toBe("complete");
			await waitFor(() => runtime.diagnostics().entries <= 4);
			expect(runtime.diagnostics()).toMatchObject({
				activeRequests: 0,
				pendingRequests: 0,
			});
		} finally {
			await runtime.dispose();
			await artifact.dispose();
		}
	}, 5_000);

	test("attributes actor worker stdout and stderr", async () => {
		const events: Readonly<DynamicAppsLogEvent>[] = [];
		setDynamicAppsLogHandler((event) => events.push(event));
		const artifact = await makeActorArtifact(`
console.log("actor stdout");
console.error("actor stderr");
export const registry = { handler: () => new Response("ok") };
`);
		const runtime = new DynamicActorRuntime();
		try {
			const response = await runtime.request({
				key: "actor-logging",
				appId: "demo",
				release: "release-logging",
				loadArtifact: async () => artifact.bytes,
				endpoint: "http://example.test",
				namespace: "test",
				pool: "default",
				request: new Request("http://example.test/start", { method: "POST" }),
			});
			expect(await response.text()).toBe("ok");
			await waitFor(
				() => events.filter((event) => event.source === "actor").length >= 2,
			);
			expect(events).toEqual(
				expect.arrayContaining([
					expect.objectContaining({
						source: "actor",
						stream: "stdout",
						message: "actor stdout",
						appId: "demo",
						release: "release-logging",
					}),
					expect.objectContaining({
						source: "actor",
						stream: "stderr",
						message: "actor stderr",
					}),
				]),
			);
		} finally {
			await runtime.dispose();
			await artifact.dispose();
		}
	});
});

async function waitForCleanPool(
	executor: DynamicAppsExecutor,
	expected: number,
): Promise<void> {
	for (let attempt = 0; attempt < 100; attempt += 1) {
		if (executor.diagnostics().cleanContexts === expected) return;
		await new Promise((resolve) => setTimeout(resolve, 1));
	}
	throw new Error("retained context pool did not refill");
}

async function waitFor(predicate: () => boolean): Promise<void> {
	for (let attempt = 0; attempt < 1_000; attempt += 1) {
		if (predicate()) return;
		await new Promise((resolve) => setTimeout(resolve, 1));
	}
	throw new Error("condition did not become true");
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

function executorConfig(mode: "ephemeral" | "pooled"): ExecutorConfig {
	return {
		executionMode: mode,
		contextPoolSize: 2,
		contextPoolMaxTotal: 8,
		contextIdleTtlMs: 30_000,
		contextHeapLimitMb: 128,
		runtimeCacheMaxEntries: 4,
		runtimeCacheMaxBytes: 64 * 1024 * 1024,
		runtimeCacheIdleTtlMs: 60_000,
		memoryHighWaterPercent: 95,
		executionConcurrency: 8,
		executionQueueSize: 16,
		executionQueueWaitMs: 5_000,
		executionTimeoutMs: 30_000,
		timingHeaders: true,
		logRequests: false,
	};
}

interface TestArtifact {
	bytes: Uint8Array;
	hash: string;
	release: string;
	dispose(): Promise<void>;
}

async function makeArtifact(
	marker: string,
	customSource?: string,
): Promise<TestArtifact> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-test-"));
	const archive = join(directory, "app.tar");
	const moduleSource =
		customSource ??
		`let counter = 0;
export async function dispatch(input) {
	const requestBody = Buffer.from(input.bodyBase64 || "", "base64");
  counter += 1;
  const request = new Request(input.url, { method: input.method, headers: input.headers });
  const body = JSON.stringify({
    counter,
    marker: ${JSON.stringify(marker)},
    path: new URL(request.url).pathname + new URL(request.url).search,
    authorization: request.headers.get("authorization"),
    privateToken: request.headers.get("x-rivet-token"),
		requestBodyBase64: Buffer.from(requestBody).toString("base64"),
  });
	return {
    status: 200,
    statusText: "OK",
    headers: [["content-type", "application/json"], ["set-cookie", "a=1"], ["set-cookie", "b=2"]],
		bodyBase64: Buffer.from(body).toString("base64"),
    timing: { moduleImportMs: 0, handlerMs: 0 },
	};
}`;
	await mkdir(join(directory, "direct"));
	await writeFile(join(directory, "direct", "main.mjs"), moduleSource);
	await writeFile(
		join(directory, "agentos-package.json"),
		JSON.stringify({ name: `dynamic-app-test-${marker}`, version: "1.0.0" }),
	);
	await execFileAsync(
		"tar",
		["-cf", archive, "direct", "agentos-package.json"],
		{ cwd: directory },
	);
	const packed = new Uint8Array(
		packAospkgFromTarBytes(await readFile(archive)).bytes,
	);
	return {
		bytes: packed,
		hash: createHash("sha256").update(packed).digest("hex"),
		release: `release-${marker}`,
		dispose: () => rm(directory, { recursive: true, force: true }),
	};
}

async function makeActorArtifact(source: string): Promise<TestArtifact> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-actor-test-"));
	const archive = join(directory, "app.tar");
	await mkdir(join(directory, "actor"));
	await writeFile(join(directory, "actor", "main.mjs"), source);
	await writeFile(
		join(directory, "agentos-package.json"),
		JSON.stringify({ name: "dynamic-actor-test", version: "1.0.0" }),
	);
	await execFileAsync(
		"tar",
		["-cf", archive, "actor", "agentos-package.json"],
		{ cwd: directory },
	);
	const packed = new Uint8Array(
		packAospkgFromTarBytes(await readFile(archive)).bytes,
	);
	return {
		bytes: packed,
		hash: createHash("sha256").update(packed).digest("hex"),
		release: "release-actor-stall",
		dispose: () => rm(directory, { recursive: true, force: true }),
	};
}

function fakeStateClient(
	artifact: TestArtifact,
	options: { beforeChunk?: () => Promise<void> } = {},
) {
	const calls = { resolve: 0, manifest: 0, chunk: 0 };
	const resolution: AppRouteResolution = {
		appId: "demo",
		release: artifact.release,
		region: "local",
		regions: ["local"],
		revision: 1,
		artifactHash: artifact.hash,
		artifactBytes: artifact.bytes.byteLength,
		entrypoint: DIRECT_ENTRYPOINT,
		namespace: "test",
		scaling: { minReplicas: 0, maxReplicas: 128, targetConcurrency: 8 },
		maxRequestBytes: 1024 * 1024,
		maxResponseBytes: 4 * 1024 * 1024,
	};
	const client = {
		agentOSAppsApp: {
			getOrCreate() {
				return {
					async resolveDeployment() {
						calls.resolve += 1;
						return resolution;
					},
					async getArtifactManifest() {
						calls.manifest += 1;
						return {
							format: DIRECT_RUNTIME_FORMAT,
							hash: artifact.hash,
							bytes: artifact.bytes.byteLength,
							chunks: 1,
							chunkBytes: artifact.bytes.byteLength,
						};
					},
					async readArtifactChunk() {
						calls.chunk += 1;
						await options.beforeChunk?.();
						return artifact.bytes;
					},
					connect() {
						return {
							ready: Promise.resolve(),
							connStatus: "connected",
							on: () => () => {},
							onOpen: () => () => {},
							onClose: () => () => {},
							dispose: async () => {},
						};
					},
				};
			},
		},
	};
	return { client, calls };
}

function fakeMultiStateClient(artifacts: Map<string, TestArtifact>) {
	const calls = { resolve: 0, manifest: 0, chunk: 0 };
	const client = {
		agentOSAppsApp: {
			getOrCreate(key: string[]) {
				const appId = key[0] ?? "";
				const artifact = artifacts.get(appId);
				if (!artifact) throw new Error(`unknown test app ${appId}`);
				const resolution: AppRouteResolution = {
					appId,
					release: artifact.release,
					region: "local",
					regions: ["local"],
					revision: 1,
					artifactHash: artifact.hash,
					artifactBytes: artifact.bytes.byteLength,
					entrypoint: DIRECT_ENTRYPOINT,
					namespace: "test",
					scaling: {
						minReplicas: 0,
						maxReplicas: 128,
						targetConcurrency: 8,
					},
					maxRequestBytes: 1024 * 1024,
					maxResponseBytes: 4 * 1024 * 1024,
				};
				return {
					async resolveDeployment() {
						calls.resolve += 1;
						return resolution;
					},
					async getArtifactManifest() {
						calls.manifest += 1;
						return {
							format: DIRECT_RUNTIME_FORMAT,
							hash: artifact.hash,
							bytes: artifact.bytes.byteLength,
							chunks: 1,
							chunkBytes: artifact.bytes.byteLength,
						};
					},
					async readArtifactChunk() {
						calls.chunk += 1;
						return artifact.bytes;
					},
					connect() {
						return {
							ready: Promise.resolve(),
							on: () => () => {},
							onOpen: () => () => {},
							onClose: () => () => {},
							dispose: async () => {},
						};
					},
				};
			},
		},
	};
	return { client, calls };
}
