import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
	assertDiscoverySanity,
	buildMetaPlatformMap,
	DEFAULT_SIDECAR_PLATFORMS,
	discoverPackages,
	SECURE_EXEC_WORKSPACE_PACKAGES,
} from "./packages.js";

function withFixture(fn: (root: string) => void) {
	const root = mkdtempSync(join(tmpdir(), "publish-packages-"));
	try {
		fn(root);
	} finally {
		rmSync(root, { recursive: true, force: true });
	}
}

function writeJson(root: string, rel: string, value: unknown) {
	const path = join(root, rel);
	mkdirSync(join(path, ".."), { recursive: true });
	writeFileSync(path, `${JSON.stringify(value, null, "\t")}\n`);
}

function writeSecureExecWorkspace(root: string) {
	writeJson(root, "package.json", {
		name: "secure-exec-workspace",
		private: true,
		packageManager: "pnpm@10.13.1",
	});
	writeFileSync(
		join(root, "pnpm-workspace.yaml"),
		[
			"packages:",
			"  - packages/*",
			"  - registry/tool/*",
			"",
		].join("\n"),
	);
	for (const [rel, name] of [
		["packages/core", "@secure-exec/core"],
		["packages/browser", "@secure-exec/browser"],
		["packages/sidecar", "@secure-exec/sidecar"],
		...DEFAULT_SIDECAR_PLATFORMS.map((platform) => [
			`packages/sidecar/npm/${platform}`,
			`@secure-exec/sidecar-${platform}`,
		]),
		["packages/registry-types", "@agentos-software/manifest"],
		["registry/tool/sandbox", "@secure-exec/sandbox"],
	]) {
		writeJson(root, join(rel, "package.json"), {
			name,
			version: "0.0.0",
		});
	}
}

test("discovers secure-exec-only packages", () => {
	withFixture((root) => {
		writeSecureExecWorkspace(root);

		const packages = discoverPackages(root);
		const names = packages.map((pkg) => pkg.name);

		assert(names.includes("@secure-exec/browser"));
		assert(names.includes("@secure-exec/core"));
		assert(names.includes("@secure-exec/sidecar-linux-x64-gnu"));
		assert(names.includes("@secure-exec/sidecar"));
		assert(
			names.indexOf("@secure-exec/sidecar-linux-x64-gnu") <
				names.indexOf("@secure-exec/sidecar"),
		);
		assert.doesNotThrow(() => assertDiscoverySanity(packages));
	});
});

test("allowlists secure-exec browser package for post-split discovery", () => {
	assert(SECURE_EXEC_WORKSPACE_PACKAGES.has("@secure-exec/browser"));
});

test("builds platform map for the secure-exec sidecar meta package", () => {
	withFixture((root) => {
		writeSecureExecWorkspace(root);
		const packages = discoverPackages(root);
		const metaMap = buildMetaPlatformMap(packages);

		assert.deepEqual(
			metaMap.get("@secure-exec/sidecar"),
			DEFAULT_SIDECAR_PLATFORMS.map(
				(platform) => `@secure-exec/sidecar-${platform}`,
			).sort(),
		);
	});
});

test("sanity check requires secure-exec registry packages and sidecar resolver", () => {
	withFixture((root) => {
		writeSecureExecWorkspace(root);
		const packages = discoverPackages(root);

		assert.doesNotThrow(() => assertDiscoverySanity(packages));
		assert.throws(
			() =>
				assertDiscoverySanity(
					packages.filter((pkg) => pkg.name !== "@secure-exec/sidecar"),
				),
			/package discovery missing required packages: @secure-exec\/sidecar/,
		);
		assert.throws(
			() =>
				assertDiscoverySanity(
					packages.filter((pkg) => pkg.name !== "@secure-exec/browser"),
				),
			/package discovery missing required packages: @secure-exec\/browser/,
		);
		assert.throws(
			() =>
				assertDiscoverySanity(
					packages.filter((pkg) => pkg.name !== "@secure-exec/core"),
				),
			/package discovery missing required packages: @secure-exec\/core/,
		);
	});
});
