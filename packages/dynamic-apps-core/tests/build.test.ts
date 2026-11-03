import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { promisify } from "node:util";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { afterEach, describe, expect, test } from "vitest";
import { extractAospkgTextFile } from "../src/artifact.js";
import {
	buildAppRelease,
	DEFAULT_MAX_RESPONSE_BYTES,
	readBuildConfig,
} from "../src/build.js";
import {
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
	directRunnerSource,
} from "../src/runtime.js";

const execFileAsync = promisify(execFile);
const temporaryDirectories: string[] = [];

afterEach(async () => {
	await Promise.all(
		temporaryDirectories
			.splice(0)
			.map((path) => rm(path, { recursive: true, force: true })),
	);
});

describe("buildAppRelease", () => {
	test("validates security ceiling overrides", () => {
		expect(() => readBuildConfig({ maxFiles: 2_001 })).toThrowError(
			/maximum|maxFiles|between/,
		);
		expect(readBuildConfig({ maxFiles: 1 }).maxFiles).toBe(1);
	});

	test("verifies and copies cached artifacts", async () => {
		const cached = await makeArtifact();
		let cacheKey = "";
		const result = await buildAppRelease(
			{
				appId: "demo",
				files: {
					"package.json": new TextEncoder().encode(
						JSON.stringify({ type: "module", main: "index.js" }),
					),
					"index.js": new TextEncoder().encode(
						"export default { fetch() { return new Response('ok') } }",
					),
				},
			},
			{
				artifactCache: {
					async get(buildId) {
						cacheKey = buildId;
						return cached;
					},
					async put() {
						throw new Error("cache hit must not write");
					},
				},
			},
		);
		expect(result.buildId).toBe(cacheKey);
		expect(result.buildId).toMatch(/^[a-f0-9]{64}$/);
		expect(result.artifact).toMatchObject({
			format: DIRECT_RUNTIME_FORMAT,
			entrypoint: DIRECT_ENTRYPOINT,
			byteLength: cached.byteLength,
			usesRivetKit: false,
		});
		expect(result.artifact.hash).toBe(
			createHash("sha256").update(cached).digest("hex"),
		);
		const original = result.artifact.bytes[0];
		cached[0] = original === 0 ? 1 : 0;
		expect(result.artifact.bytes[0]).toBe(original);
	});

	test("keys cached wrappers by the effective response limit", async () => {
		const cachedArtifacts = await Promise.all([
			makeArtifact(1024),
			makeArtifact(2048),
			makeArtifact(DEFAULT_MAX_RESPONSE_BYTES),
		]);
		const cache = new Map<string, Uint8Array>();
		const cacheReads: string[] = [];
		const artifactCache = {
			async get(buildId: string) {
				cacheReads.push(buildId);
				if (!cache.has(buildId)) {
					const artifact = cachedArtifacts[cache.size];
					if (!artifact) throw new Error("unexpected cache miss");
					cache.set(buildId, artifact);
				}
				return cache.get(buildId);
			},
			async put() {
				throw new Error("pre-populated cache must not write");
			},
		};
		const input = {
			appId: "response-limit",
			files: {
				"package.json": new TextEncoder().encode(
					JSON.stringify({ type: "module", main: "index.mjs" }),
				),
				"index.mjs": new TextEncoder().encode(
					`export default { fetch() { return new Response("x".repeat(1536)) } }`,
				),
			},
		};

		const lower = await buildAppRelease(input, {
			config: { maxResponseBytes: 1024 },
			artifactCache,
		});
		const higher = await buildAppRelease(input, {
			config: { maxResponseBytes: 2048 },
			artifactCache,
		});
		const higherAgain = await buildAppRelease(input, {
			config: { maxResponseBytes: 2048 },
			artifactCache,
		});
		const implicitDefault = await buildAppRelease(input, { artifactCache });
		const explicitDefault = await buildAppRelease(input, {
			config: { maxResponseBytes: DEFAULT_MAX_RESPONSE_BYTES },
			artifactCache,
		});

		expect(lower.buildId).not.toBe(higher.buildId);
		expect(higherAgain.buildId).toBe(higher.buildId);
		expect(higherAgain.artifact.hash).toBe(higher.artifact.hash);
		expect(explicitDefault.buildId).toBe(implicitDefault.buildId);
		expect(explicitDefault.artifact.hash).toBe(implicitDefault.artifact.hash);
		expect(cacheReads).toEqual([
			lower.buildId,
			higher.buildId,
			higher.buildId,
			implicitDefault.buildId,
			implicitDefault.buildId,
		]);
		expect(cache.size).toBe(3);
		await expect(dispatchArtifact(lower.artifact.bytes)).rejects.toThrow(
			"Dynamic App response exceeds the configured limit",
		);
		const response = await dispatchArtifact(higher.artifact.bytes);
		expect(Buffer.from(response.bodyBase64, "base64")).toHaveLength(1536);
	});
});

async function makeArtifact(
	maxResponseBytes = DEFAULT_MAX_RESPONSE_BYTES,
): Promise<Uint8Array> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-core-build-"));
	temporaryDirectories.push(directory);
	await mkdir(join(directory, "direct"));
	await writeFile(
		join(directory, "direct", "main.mjs"),
		directRunnerSource({
			entrypoint: "index.mjs",
			release: `response-limit-${maxResponseBytes}`,
			maxResponseBytes,
		}),
	);
	await writeFile(
		join(directory, "direct", "index.mjs"),
		`export default { fetch() { return new Response("x".repeat(1536)) } }`,
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

async function dispatchArtifact(
	artifact: Uint8Array,
): Promise<{ bodyBase64: string }> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-dispatch-"));
	temporaryDirectories.push(directory);
	await writeFile(
		join(directory, "index.mjs"),
		extractAospkgTextFile(artifact, "direct/index.mjs"),
	);
	const runnerPath = join(directory, "main.mjs");
	await writeFile(
		runnerPath,
		extractAospkgTextFile(artifact, "direct/main.mjs"),
	);
	const runner = (await import(pathToFileURL(runnerPath).href)) as {
		dispatch(input: {
			url: string;
			method: string;
			headers: Array<[string, string]>;
		}): Promise<{ bodyBase64: string }>;
	};
	return runner.dispatch({
		url: "https://example.test/",
		method: "GET",
		headers: [],
	});
}
