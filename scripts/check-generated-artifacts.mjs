#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, "..");

const checkedPaths = [
	"packages/core/src/generated-protocol.ts",
	"packages/core/src/generated",
	"packages/browser/src/generated/util-polyfill.ts",
	"packages/browser/src/generated/buffer-polyfill.ts",
	"packages/browser/src/generated/path-polyfill.ts",
	"crates/bridge/bridge-contract.json",
	"crates/execution/assets/polyfill-registry.json",
	"crates/execution/assets/wasi-preview1-imports.json",
	"crates/execution/assets/v8-bridge.js",
	"crates/execution/assets/v8-bridge-zlib.js",
	"crates/v8-runtime/assets/generated/v8-bridge.js",
	"crates/v8-runtime/assets/generated/v8-bridge-zlib.js",
];

assertNoLegacyVmConfigBindings();
assertNoLegacyBuildSupportShims();
const beforeDiff = capture("jj", [
	"diff",
	"--git",
	"--color=never",
	"--",
	...checkedPaths,
]);
run("pnpm", ["--dir", "packages/build-tools", "build:protocol"]);
run("node", ["packages/build-tools/scripts/build-browser-util-polyfill.mjs"]);
run("node", ["packages/build-tools/scripts/build-browser-buffer-polyfill.mjs"]);
run("node", ["packages/build-tools/scripts/build-browser-path-polyfill.mjs"]);
run("node", ["packages/build-tools/scripts/build-v8-bridge.mjs"]);
run("node", [
	"packages/build-tools/scripts/build-v8-bridge.mjs",
	"--out-dir",
	path.join("crates", "v8-runtime", "assets", "generated"),
]);
run("node", ["scripts/check-polyfill-registry.mjs"]);
run("node", ["scripts/check-crypto-conformance.mjs"]);
run("node", ["scripts/check-module-resolution-conformance.mjs"]);
run("pnpm", ["--dir", "packages/browser", "check:bridge-contract"]);
run("pnpm", ["--dir", "packages/browser", "check:signals"]);
run("pnpm", ["--dir", "packages/browser", "check:wasi-surface"]);
run("cargo", ["test", "-p", "secure-exec-bridge", "bridge_contract", "--quiet"]);
run("cargo", ["test", "-p", "secure-exec-vm-config", "--quiet"]);
const afterDiff = capture("jj", [
	"diff",
	"--git",
	"--color=never",
	"--",
	...checkedPaths,
]);

if (afterDiff !== beforeDiff) {
	const changed = capture("jj", ["diff", "--name-only", "--", ...checkedPaths])
		.split("\n")
		.map((line) => line.trim())
		.filter(Boolean);
	console.error(
		[
			"Generated artifact drift detected.",
			"Run the generators and commit the resulting changes:",
			"  pnpm --dir packages/build-tools build:protocol",
			"  node packages/build-tools/scripts/build-browser-util-polyfill.mjs",
			"  node packages/build-tools/scripts/build-v8-bridge.mjs",
			"  node packages/build-tools/scripts/build-v8-bridge.mjs --out-dir crates/v8-runtime/assets/generated",
			"  cargo test -p secure-exec-vm-config --quiet",
			"  pnpm --dir packages/browser check:bridge-contract",
			"  pnpm --dir packages/browser check:wasi-surface",
			"",
			"Changed checked paths:",
			...changed.map((file) => `  ${file}`),
			"",
		].join("\n"),
	);
	run("jj", ["diff", "--stat", "--", ...checkedPaths], { allowFailure: true });
	process.exit(1);
}

function run(command, args, options = {}) {
	const result = spawnSync(command, args, {
		cwd: repoRoot,
		stdio: "inherit",
		shell: process.platform === "win32",
	});
	if (!options.allowFailure && result.status !== 0) {
		process.exit(result.status ?? 1);
	}
}

function capture(command, args) {
	const result = spawnSync(command, args, {
		cwd: repoRoot,
		encoding: "utf8",
		maxBuffer: 128 * 1024 * 1024,
		shell: process.platform === "win32",
	});
	if (result.error) {
		console.error(result.error.message);
		process.exit(1);
	}
	if (result.status !== 0) {
		process.stderr.write(result.stderr);
		process.exit(result.status ?? 1);
	}
	return result.stdout;
}

function assertNoLegacyVmConfigBindings() {
	const legacyDir = path.join(repoRoot, "crates", "vm-config", "bindings");
	let entries;
	try {
		entries = readdirSync(legacyDir);
	} catch (error) {
		if (error?.code === "ENOENT") {
			return;
		}
		throw error;
	}
	if (entries.length === 0) {
		return;
	}
	console.error(
		[
			"Stale vm-config bindings detected.",
			"The canonical TypeScript vm-config output is packages/core/src/generated/.",
			"Remove crates/vm-config/bindings/ so generated DTOs do not drift.",
			"",
		].join("\n"),
	);
	process.exit(1);
}

function assertNoLegacyBuildSupportShims() {
	const legacyFiles = [
		path.join(repoRoot, "crates", "execution", "build_support.rs"),
		path.join(repoRoot, "crates", "v8-runtime", "build_support.rs"),
	];
	const present = legacyFiles.filter((file) => existsSync(file));
	if (present.length === 0) {
		return;
	}
	console.error(
		[
			"Stale V8 bridge build-support shims detected.",
			"The canonical implementation is crates/build-support/v8_bridge_build.rs.",
			"Remove these duplicate files so native bridge builds cannot drift:",
			...present.map((file) => `  ${path.relative(repoRoot, file)}`),
			"",
		].join("\n"),
	);
	process.exit(1);
}
