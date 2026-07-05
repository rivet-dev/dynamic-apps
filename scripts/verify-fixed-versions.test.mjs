import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), "..");
const gateArgs = [
	"--filter=publish",
	"exec",
	"tsx",
	"src/ci/verify-fixed-versions.ts",
];

function runGate(root) {
	const args = [...gateArgs];
	if (root) args.push(`--root=${root}`);
	return execFileSync("pnpm", args, { cwd: repoRoot, stdio: "pipe" });
}

function gateExitCode(root) {
	try {
		runGate(root);
		return 0;
	} catch (err) {
		return err.status;
	}
}

function writeJson(root, rel, value) {
	const path = join(root, rel);
	mkdirSync(join(path, ".."), { recursive: true });
	writeFileSync(path, `${JSON.stringify(value, null, "\t")}\n`);
}

function writeFixture(root, packageVersion) {
	writeJson(root, "package.json", {
		name: "secure-exec-fixture",
		private: true,
		packageManager: "pnpm@10.13.1",
	});
	writeFileSync(
		join(root, "pnpm-workspace.yaml"),
		["packages:", "  - packages/*", ""].join("\n"),
	);
	writeJson(root, "packages/core/package.json", {
		name: "@secure-exec/core",
		version: packageVersion,
	});
	writeFileSync(
		join(root, "Cargo.toml"),
		`[workspace.package]
version = "0.0.1"

[workspace.dependencies]
secure-exec-bridge = { path = "crates/bridge", version = "0.0.1" }
`,
	);
}

test("passes on the current tree", () => {
	runGate();
});

test("fails when a discovered package drifts off 0.0.1", () => {
	const root = mkdtempSync(join(tmpdir(), "secure-exec-fixed-versions-"));
	try {
		writeFixture(root, "0.3.4-rc.1");
		const exitCode = gateExitCode(root);
		if (exitCode !== 1) {
			throw new Error(`expected gate to exit 1 on a drifted version, got ${exitCode}`);
		}
	} finally {
		rmSync(root, { recursive: true, force: true });
	}
});
