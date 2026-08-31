import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { afterEach, describe, expect, test } from "vitest";
import { createDynamicApps } from "../src/index.js";
import type {
	ActiveRelease,
	PublishReleaseInput,
	ReleaseInvalidation,
} from "../src/types.js";

const execFileAsync = promisify(execFile);
const temporaryDirectories: string[] = [];

afterEach(async () => {
	await Promise.all(
		temporaryDirectories
			.splice(0)
			.map((path) => rm(path, { recursive: true, force: true })),
	);
});

describe("createDynamicApps", () => {
	test("publishes a copied artifact and serves warm requests without hooks", async () => {
		const bytes = await makeArtifact("one");
		let active: ActiveRelease | undefined;
		let invalidate: ReleaseInvalidation | undefined;
		let loads = 0;
		let publishes = 0;
		let watches = 0;
		let unsubscribes = 0;
		let observedPublish: PublishReleaseInput | undefined;
		const dynamicApps = createDynamicApps({
			artifactCache: {
				async get() {
					return bytes;
				},
				async put() {},
			},
			async publishRelease(input) {
				publishes += 1;
				observedPublish = input;
				active = {
					appId: input.appId,
					release: `release-${publishes}`,
					artifact: {
						...input.artifact,
						bytes: new Uint8Array(input.artifact.bytes),
					},
					maxRequestBytes: 1024 * 1024,
					maxResponseBytes: 4 * 1024 * 1024,
				};
				input.artifact.bytes[0] ^= 1;
				return { release: active.release };
			},
			async loadActiveRelease() {
				loads += 1;
				return active;
			},
			async watchActiveRelease(_appId, callback) {
				watches += 1;
				invalidate = callback;
				return () => {
					unsubscribes += 1;
				};
			},
			executor: {
				executionMode: "ephemeral",
				timingHeaders: true,
			},
		});
		try {
			const deployment = await dynamicApps.deployApp({
				appId: "demo",
				files: {
					"package.json": JSON.stringify({
						type: "module",
						main: "index.js",
					}),
					"index.js": "export default { fetch() {} }",
				},
			});
			expect(deployment).toEqual({ release: "release-1" });
			expect(observedPublish).not.toHaveProperty("files");
			expect(active?.artifact.bytes[0]).toBe(bytes[0]);

			const first = await dynamicApps.appsRouter.request("/demo/");
			expect(await first.text()).toBe("one");
			const second = await dynamicApps.appsRouter.request("/demo/");
			expect(await second.text()).toBe("one");
			expect({ loads, watches }).toEqual({ loads: 1, watches: 1 });
			expect(first.headers.get("x-agentos-app-release")).toBe("release-1");

			invalidate?.();
			await waitFor(() => loads === 2);
		} finally {
			await dynamicApps.dispose();
		}
		expect(unsubscribes).toBe(1);
	});

	test("subscribes before the first release load", async () => {
		const bytes = await makeArtifact("ordered");
		const order: string[] = [];
		let makeReady = () => {};
		const ready = new Promise<void>((resolve) => {
			makeReady = resolve;
		});
		const dynamicApps = createDynamicApps({
			async publishRelease() {},
			async watchActiveRelease() {
				order.push("watch");
				await ready;
				order.push("ready");
				return () => {};
			},
			async loadActiveRelease(appId) {
				order.push("load");
				return release(appId, "ordered", bytes);
			},
			executor: { executionMode: "ephemeral" },
		});
		try {
			const response = dynamicApps.appsRouter.request("/demo/");
			await waitFor(() => order.includes("watch"));
			expect(order).toEqual(["watch"]);
			makeReady();
			expect(await (await response).text()).toBe("ordered");
			expect(order).toEqual(["watch", "ready", "load"]);
		} finally {
			makeReady();
			await dynamicApps.dispose();
		}
	});

	test("caches a verified copy of release metadata", async () => {
		const bytes = await makeArtifact("copied");
		const loaded = release("demo", "before", bytes);
		const dynamicApps = createDynamicApps({
			async publishRelease() {},
			async watchActiveRelease() {
				return () => {};
			},
			async loadActiveRelease() {
				return loaded;
			},
			executor: { executionMode: "ephemeral" },
		});
		try {
			const first = await dynamicApps.appsRouter.request("/demo/");
			expect(first.headers.get("x-agentos-app-release")).toBe("release-before");
			loaded.release = "release-after";
			loaded.artifact.bytes[0] ^= 1;
			const second = await dynamicApps.appsRouter.request("/demo/");
			expect(second.headers.get("x-agentos-app-release")).toBe(
				"release-before",
			);
			expect(await second.text()).toBe("copied");
		} finally {
			await dynamicApps.dispose();
		}
	});

	test("routes RivetKit releases through the shared server runtime", async () => {
		const bytes = await makeArtifact("unused-direct-entrypoint");
		const active = release("demo", "actors", bytes);
		active.artifact.usesRivetKit = true;
		active.server = {
			environment: {
				RIVET_ENDPOINT: "https://api.rivet.dev",
				RIVET_NAMESPACE: "demo",
			},
		};
		const requests: Array<{ key: string; path: string; environment: string }> =
			[];
		const dynamicApps = createDynamicApps({
			async publishRelease() {},
			async watchActiveRelease() {
				return () => {};
			},
			async loadActiveRelease() {
				return active;
			},
			serverRuntime: {
				async request(input) {
					requests.push({
						key: input.key,
						path: new URL(input.request.url).pathname,
						environment: input.environment.RIVET_NAMESPACE ?? "",
					});
					expect(await input.loadArtifact()).toEqual(bytes);
					return new Response(`server:${requests.length}`);
				},
			},
			executor: { executionMode: "ephemeral" },
		});
		try {
			expect(
				await (await dynamicApps.appsRouter.request("/demo/one")).text(),
			).toBe("server:1");
			expect(
				await (await dynamicApps.appsRouter.request("/demo/two")).text(),
			).toBe("server:2");
			expect(requests).toEqual([
				{
					key: `release-actors:${active.artifact.hash}`,
					path: "/one",
					environment: "demo",
				},
				{
					key: `release-actors:${active.artifact.hash}`,
					path: "/two",
					environment: "demo",
				},
			]);
			expect(dynamicApps.diagnostics()).toMatchObject({ runtimes: 0 });
		} finally {
			await dynamicApps.dispose();
		}
	});
});

function release(
	appId: string,
	name: string,
	bytes: Uint8Array,
): ActiveRelease {
	return {
		appId,
		release: `release-${name}`,
		artifact: {
			format: "dynamic-apps-direct-v2",
			entrypoint: "direct-v2/main.mjs",
			hash: createHash("sha256").update(bytes).digest("hex"),
			bytes: new Uint8Array(bytes),
			byteLength: bytes.byteLength,
			usesRivetKit: false,
		},
		maxRequestBytes: 1024 * 1024,
		maxResponseBytes: 4 * 1024 * 1024,
	};
}

async function makeArtifact(body: string): Promise<Uint8Array> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-core-test-"));
	temporaryDirectories.push(directory);
	await mkdir(join(directory, "direct"));
	await writeFile(
		join(directory, "direct", "main.mjs"),
		`export async function dispatch() { return {
			status: 200,
			statusText: "OK",
			headers: [],
			bodyBase64: Buffer.from(${JSON.stringify(body)}).toString("base64"),
		}; }`,
	);
	await writeFile(
		join(directory, "agentos-package.json"),
		JSON.stringify({ name: "test-app", version: "1.0.0" }),
	);
	const archive = join(directory, "app.tar");
	await execFileAsync(
		"tar",
		["-cf", archive, "direct", "agentos-package.json"],
		{ cwd: directory },
	);
	return new Uint8Array(packAospkgFromTarBytes(await readFile(archive)).bytes);
}

async function waitFor(predicate: () => boolean): Promise<void> {
	for (let attempt = 0; attempt < 1_000; attempt += 1) {
		if (predicate()) return;
		await new Promise((resolve) => setTimeout(resolve, 1));
	}
	throw new Error("condition did not become true");
}
