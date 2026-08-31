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
import { resolveDefaultRivetConnection } from "../src/control-plane.js";
import { deployApp } from "../src/deploy.js";
import {
	DynamicAppsExecutor,
	type ExecutorConfig,
	readExecutorConfig,
} from "../src/executor.js";
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

afterEach(() => setRouterRequestOverride());

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
										namespace: input.namespace,
										pool: input.runtime.pool,
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
			"namespace",
			"pool",
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
			RIVET_ENDPOINT: "https://api.rivet.dev",
			RIVET_NAMESPACE: "app-namespace",
			RIVET_TOKEN: "pk_example",
			RIVET_POOL: "app-pool",
		});
	});
});

function deploymentResult(input: {
	namespace: string;
	runtime: { pool: string };
}) {
	return {
		appId: "demo",
		release: "release-1",
		namespace: input.namespace,
		pool: input.runtime.pool,
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
		expect(source).toContain("__dynamicAppDispatch");
		expect(source).not.toContain("listen(");
		expect(source).not.toContain("createServer");
	});

	test("supports fresh, snapshot, and prewarm config", () => {
		expect(
			readExecutorConfig({ DYNAMIC_APPS_ISOLATE_MODE: "fresh" }).isolateMode,
		).toBe("fresh");
		expect(
			readExecutorConfig({
				DYNAMIC_APPS_ISOLATE_MODE: "prewarm",
				DYNAMIC_APPS_ISOLATE_POOL_SIZE: "0",
			}).isolateMode,
		).toBe("snapshot");
		expect(
			readExecutorConfig({ DYNAMIC_APPS_ISOLATE_MODE: "snapshot" }).isolateMode,
		).toBe("snapshot");
	});
});

describe("direct V8 execution", () => {
	test.each([
		"fresh",
		"snapshot",
		"prewarm",
	] as const)("isolates every request in %s mode", async (mode) => {
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

	test("reuses bounded prewarmed isolates while resetting context state", async () => {
		const artifact = await makeArtifact("reuse");
		const fake = fakeStateClient(artifact);
		const executor = new DynamicAppsExecutor(
			executorConfig("prewarm"),
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
				cleanIsolates: 2,
				isolateCreates: 2,
				isolateDisposes: 0,
				contextCreates: 22,
				contextDisposes: 20,
				dispatches: 20,
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
				...executorConfig("prewarm"),
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

	test("bounds the total prewarm cache across applications", async () => {
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
				...executorConfig("prewarm"),
				runtimeCacheMaxEntries: artifacts.length,
				isolatePoolMaxTotal: 4,
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
				cleanIsolates: 4,
			});
		} finally {
			await executor.dispose();
			await Promise.all(artifacts.map((artifact) => artifact.dispose()));
		}
	});
});

describe("actor callback resource limits", () => {
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
});

async function waitForCleanPool(
	executor: DynamicAppsExecutor,
	expected: number,
): Promise<void> {
	for (let attempt = 0; attempt < 100; attempt += 1) {
		if (executor.diagnostics().cleanIsolates === expected) return;
		await new Promise((resolve) => setTimeout(resolve, 1));
	}
	throw new Error("prewarm pool did not refill");
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

function executorConfig(
	mode: "fresh" | "snapshot" | "prewarm",
): ExecutorConfig {
	return {
		isolateMode: mode,
		isolatePoolSize: 2,
		isolateIdleTtlMs: 30_000,
		isolateHeapLimitMb: 128,
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

async function makeArtifact(marker: string): Promise<TestArtifact> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-test-"));
	const archive = join(directory, "app.tar");
	const moduleSource = `let counter = 0;
globalThis.__dynamicAppDispatch = async function(inputJson) {
	const input = JSON.parse(inputJson);
  counter += 1;
  const request = new Request(input.url, { method: input.method, headers: input.headers });
  const body = JSON.stringify({
    counter,
    marker: ${JSON.stringify(marker)},
    path: new URL(request.url).pathname + new URL(request.url).search,
    authorization: request.headers.get("authorization"),
    privateToken: request.headers.get("x-rivet-token"),
  });
	return JSON.stringify({
    status: 200,
    statusText: "OK",
    headers: [["content-type", "application/json"], ["set-cookie", "a=1"], ["set-cookie", "b=2"]],
		bodyBase64: globalThis.__dynamicAppsBase64Encode(new TextEncoder().encode(body)),
    timing: { moduleImportMs: 0, handlerMs: 0 },
	});
};`;
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

function fakeStateClient(artifact: TestArtifact) {
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
