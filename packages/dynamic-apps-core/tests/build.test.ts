import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { afterEach, describe, expect, test } from "vitest";
import { buildAppRelease, readBuildConfig } from "../src/build.js";
import { DIRECT_ENTRYPOINT, DIRECT_RUNTIME_FORMAT } from "../src/runtime.js";

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
});

async function makeArtifact(): Promise<Uint8Array> {
	const directory = await mkdtemp(join(tmpdir(), "dynamic-apps-core-build-"));
	temporaryDirectories.push(directory);
	await mkdir(join(directory, "direct"));
	await writeFile(
		join(directory, "direct", "main.mjs"),
		`export const dynamicAppMetadata = { format: ${JSON.stringify(DIRECT_RUNTIME_FORMAT)} };
export async function dispatch() { return { status: 200, headers: [], bodyBase64: "" }; }`,
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
