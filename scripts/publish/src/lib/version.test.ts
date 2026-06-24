import assert from "node:assert/strict";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { DEFAULT_SIDECAR_PLATFORMS } from "./packages.js";
import { bumpCargoVersions, bumpPackageJsons } from "./version.js";

async function writeJson(root: string, rel: string, value: unknown) {
	const path = join(root, rel);
	await mkdir(join(path, ".."), { recursive: true });
	await writeFile(path, `${JSON.stringify(value, null, "\t")}\n`);
}

test("bumpCargoVersions rewrites secure-exec workspace dependency versions", async () => {
	const repoRoot = await mkdtemp(join(tmpdir(), "secure-exec-version-test-"));
	try {
		await writeFile(
			join(repoRoot, "Cargo.toml"),
			`[workspace.package]
version = "0.2.0"

[workspace.dependencies]
secure-exec-bridge = { path = "crates/bridge", version = "0.2.0" }
secure-exec-sidecar = { path = "crates/sidecar", version = "0.2.0" }
secure-exec-client = { path = "crates/secure-exec-client", version = "0.2.0" }
serde = "1"
`,
		);

		await bumpCargoVersions(repoRoot, "0.3.0");

		const cargoToml = await readFile(join(repoRoot, "Cargo.toml"), "utf8");
		assert.match(cargoToml, /version = "0\.3\.0"/);
		assert.match(
			cargoToml,
			/secure-exec-bridge = \{ path = "crates\/bridge", version = "0\.3\.0" \}/,
		);
		assert.match(
			cargoToml,
			/secure-exec-sidecar = \{ path = "crates\/sidecar", version = "0\.3\.0" \}/,
		);
		assert.match(
			cargoToml,
			/secure-exec-client = \{ path = "crates\/secure-exec-client", version = "0\.3\.0" \}/,
		);
		assert.match(cargoToml, /serde = "1"/);
	} finally {
		await rm(repoRoot, { recursive: true, force: true });
	}
});

test("bumpPackageJsons injects secure-exec sidecar platform optional dependency", async () => {
	const repoRoot = await mkdtemp(join(tmpdir(), "secure-exec-version-test-"));
	try {
		await writeJson(repoRoot, "package.json", {
			name: "secure-exec-workspace",
			private: true,
			packageManager: "pnpm@10.13.1",
		});
		await writeFile(
			join(repoRoot, "pnpm-workspace.yaml"),
			[
				"packages:",
				"  - packages/*",
				"  - packages/sidecar/npm/*",
				"  - registry/file-system/*",
				"  - registry/tool/*",
				"",
			].join("\n"),
		);
		for (const [rel, name] of [
			["packages/core", "@secure-exec/core"],
			["packages/browser", "@secure-exec/browser"],
			["packages/registry-types", "@secure-exec/registry-types"],
			["packages/sidecar", "@secure-exec/sidecar"],
			...DEFAULT_SIDECAR_PLATFORMS.map((platform) => [
				`packages/sidecar/npm/${platform}`,
				`@secure-exec/sidecar-${platform}`,
			]),
			["registry/file-system/s3", "@secure-exec/s3"],
			["registry/file-system/google-drive", "@secure-exec/google-drive"],
			["registry/tool/sandbox", "@secure-exec/sandbox"],
		]) {
			await writeJson(repoRoot, join(rel, "package.json"), {
				name,
				version: "0.0.0",
			});
		}

		await bumpPackageJsons(repoRoot, "0.3.0");

		const sidecarManifest = JSON.parse(
			await readFile(join(repoRoot, "packages/sidecar/package.json"), "utf8"),
		);
		assert.deepEqual(
			sidecarManifest.optionalDependencies,
			Object.fromEntries(
				DEFAULT_SIDECAR_PLATFORMS.map((platform) => [
					`@secure-exec/sidecar-${platform}`,
					"0.3.0",
				]).sort(),
			),
		);
	} finally {
		await rm(repoRoot, { recursive: true, force: true });
	}
});
