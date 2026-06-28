import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { checkRegistrySoftwareSplit } from "./check-registry-software-split.mjs";

function withFixture(fn) {
	const root = mkdtempSync(join(tmpdir(), "registry-software-split-"));
	try {
		return fn(root);
	} finally {
		rmSync(root, { recursive: true, force: true });
	}
}

function writeJson(root, rel, value) {
	const path = join(root, rel);
	mkdirSync(join(path, ".."), { recursive: true });
	writeFileSync(path, `${JSON.stringify(value, null, "\t")}\n`);
}

test("accepts agentos-software registry software package metadata", () => {
	withFixture((root) => {
		writeJson(root, "registry/software/coreutils/package.json", {
			name: "@agentos-software/coreutils",
			dependencies: {
				"@agentos-software/manifest": "workspace:*",
			},
		});

		assert.deepEqual(checkRegistrySoftwareSplit({ root }), []);
	});
});

test("rejects package names that do not match the directory", () => {
	withFixture((root) => {
		writeJson(root, "registry/software/grep/package.json", {
			name: "@rivet-dev/agent-os-pkg-grep",
		});

		assert.deepEqual(checkRegistrySoftwareSplit({ root }), [
			"registry/software/grep/package.json must be named @agentos-software/grep, found @rivet-dev/agent-os-pkg-grep",
		]);
	});
});

test("rejects Agent OS dependencies inside software manifests", () => {
	withFixture((root) => {
		writeJson(root, "registry/software/common/package.json", {
			name: "@agentos-software/common",
			dependencies: {
				"@rivet-dev/agent-os-core": "workspace:*",
			},
		});

		assert.deepEqual(checkRegistrySoftwareSplit({ root }), [
			"@agentos-software/common must not depend on Agent OS package @rivet-dev/agent-os-core in registry software dependencies",
		]);
	});
});
