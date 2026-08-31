import { createHash } from "node:crypto";
import { afterEach, describe, expect, test } from "vitest";
import {
	ARTIFACT_CHUNK_BYTES,
	createRivetReleaseStore,
} from "../src/release-store.js";

const previousRuntimeMode = process.env.RIVETKIT_RUNTIME_MODE;

afterEach(() => {
	if (previousRuntimeMode === undefined) {
		delete process.env.RIVETKIT_RUNTIME_MODE;
	} else {
		process.env.RIVETKIT_RUNTIME_MODE = previousRuntimeMode;
	}
});

describe("Rivet release store", () => {
	test("publishes sequential chunks and returns only Deployment fields", async () => {
		process.env.RIVETKIT_RUNTIME_MODE = "serverless";
		const bytes = Uint8Array.from(
			{ length: ARTIFACT_CHUNK_BYTES + 7 },
			(_, index) => index % 251,
		);
		const writes: Array<{ index: number; bytes: number }> = [];
		const handle = {
			async beginReleasePublish() {
				return {
					release: "a".repeat(64),
					sequence: 1,
					uploadRequired: true,
					chunkBytes: ARTIFACT_CHUNK_BYTES,
				};
			},
			async writeReleaseChunk(input: { index: number; content: Uint8Array }) {
				writes.push({ index: input.index, bytes: input.content.byteLength });
			},
			async commitReleasePublish() {
				return {
					appId: "demo",
					release: "a".repeat(64),
					endpoint: "https://example.test",
					namespace: "demo",
					pool: "pool",
					token: "public",
					regions: ["local"],
					appActorId: "actor",
					usesRivetKit: false,
				};
			},
		};
		const store = createRivetReleaseStore({
			dynamicAppsApp: {
				get: () => handle as never,
				getOrCreate: () => handle as never,
			},
		});
		const result = await store.publishRelease({
			appId: "demo",
			buildId: "b".repeat(64),
			artifact: {
				format: "dynamic-apps-direct-v2",
				entrypoint: "direct-v2/main.mjs",
				hash: createHash("sha256").update(bytes).digest("hex"),
				bytes,
				byteLength: bytes.byteLength,
				usesRivetKit: false,
			},
			createdAt: Date.now(),
		});
		expect(writes).toEqual([
			{ index: 0, bytes: ARTIFACT_CHUNK_BYTES },
			{ index: 1, bytes: 7 },
		]);
		expect(Object.keys(result)).toEqual([
			"appId",
			"release",
			"endpoint",
			"namespace",
			"pool",
			"token",
		]);
	});

	test("subscribes, downloads, verifies, and releases a driver", async () => {
		process.env.RIVETKIT_RUNTIME_MODE = "serverless";
		const bytes = new Uint8Array([1, 2, 3, 4]);
		const hash = createHash("sha256").update(bytes).digest("hex");
		let disposed = 0;
		const handle = {
			connect() {
				return {
					ready: Promise.resolve(),
					on: () => () => {},
					onOpen: () => () => {},
					onClose: () => () => {},
					async dispose() {
						disposed += 1;
					},
				};
			},
			async resolveDeployment() {
				return {
					appId: "demo",
					release: "c".repeat(64),
					region: "local",
					regions: ["local"],
					revision: 1,
					artifactHash: hash,
					artifactBytes: bytes.byteLength,
					entrypoint: "direct-v2/main.mjs" as const,
					namespace: "demo",
					scaling: { minReplicas: 0, maxReplicas: 1, targetConcurrency: 1 },
					maxRequestBytes: 1024,
					maxResponseBytes: 1024,
					usesRivetKit: true,
					serverlessEndpoint: "https://demo:runtime-token@example.test",
					runtimePool: "actor-pool",
				};
			},
			async getArtifactManifest() {
				return {
					format: "dynamic-apps-direct-v2",
					hash,
					bytes: bytes.byteLength,
					chunks: 1,
					chunkBytes: ARTIFACT_CHUNK_BYTES,
				};
			},
			async readArtifactChunk() {
				return bytes;
			},
		};
		const store = createRivetReleaseStore({
			dynamicAppsApp: {
				get: () => handle as never,
				getOrCreate: () => handle as never,
			},
		});
		const unsubscribe = await store.watchActiveRelease("demo", () => {});
		const timings: string[] = [];
		const release = await store.loadActiveRelease("demo", {
			recordTiming: (name) => timings.push(name),
		});
		expect(release?.artifact.bytes).toEqual(bytes);
		expect(release?.server?.environment).toMatchObject({
			RIVET_ENDPOINT: "https://example.test",
			RIVET_NAMESPACE: "demo",
			RIVET_POOL: "actor-pool",
			RIVET_TOKEN: "runtime-token",
			RIVETKIT_RUNTIME: "wasm",
			RIVETKIT_RUNTIME_MODE: "serverless",
		});
		expect(timings).toEqual([
			"actor-connect",
			"actor-resolve",
			"artifact-manifest",
			"artifact-download",
		]);
		await unsubscribe();
		await unsubscribe();
		expect(disposed).toBe(1);
	});

	test("cleans up a failed connection and allows a later retry", async () => {
		process.env.RIVETKIT_RUNTIME_MODE = "serverless";
		let attempts = 0;
		let disposed = 0;
		let listenersRemoved = 0;
		const handle = {
			connect() {
				attempts += 1;
				const succeeds = attempts > 1;
				return {
					ready: succeeds
						? Promise.resolve()
						: Promise.reject(new Error("connection failed")),
					on: () => () => {
						listenersRemoved += 1;
					},
					onOpen: () => () => {
						listenersRemoved += 1;
					},
					onClose: () => () => {
						listenersRemoved += 1;
					},
					async dispose() {
						disposed += 1;
					},
				};
			},
		};
		const store = createRivetReleaseStore({
			dynamicAppsApp: {
				get: () => handle as never,
				getOrCreate: () => handle as never,
			},
		});
		await expect(store.watchActiveRelease("demo", () => {})).rejects.toThrow(
			"connection failed",
		);
		expect({ disposed, listenersRemoved }).toEqual({
			disposed: 1,
			listenersRemoved: 3,
		});
		const unsubscribe = await store.watchActiveRelease("demo", () => {});
		await unsubscribe();
		expect(attempts).toBe(2);
		expect(disposed).toBe(2);
	});
});
