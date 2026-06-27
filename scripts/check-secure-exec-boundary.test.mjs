import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const scriptPath = join(dirname(fileURLToPath(import.meta.url)), 'check-secure-exec-boundary.mjs');

function withFixture(fn) {
	const root = mkdtempSync(join(tmpdir(), 'secure-exec-boundary-'));
	try {
		return fn(root);
	} finally {
		rmSync(root, { recursive: true, force: true });
	}
}

function writeJson(root, rel, value) {
	const path = join(root, rel);
	mkdirSync(dirname(path), { recursive: true });
	writeFileSync(path, `${JSON.stringify(value, null, '\t')}\n`);
}

test('passes a secure-exec-only fixture', () => {
	withFixture((root) => {
		writeJson(root, 'package.json', {
			name: 'secure-exec-workspace',
			dependencies: {
				'@secure-exec/core': 'workspace:*',
			},
		});
		execFileSync(process.execPath, [scriptPath, '--root', root], { stdio: 'pipe' });
	});
});

test('rejects Agent OS package dependencies', () => {
	withFixture((root) => {
		writeJson(root, 'package.json', {
			name: 'secure-exec-workspace',
			dependencies: {
				'@rivet-dev/agentos-core': 'workspace:*',
			},
		});
		const result = spawnSync(process.execPath, [scriptPath, '--root', root], {
			encoding: 'utf8',
		});
		assert.notEqual(result.status, 0);
		assert.match(result.stderr, /@rivet-dev\/agentos-core/);
	});
});

test('ignores forbidden specifiers inside boundary-check test fixtures', () => {
	withFixture((root) => {
		writeJson(root, 'package.json', { name: 'secure-exec-workspace' });
		const testPath = join(root, 'scripts/check-registry-test-runtime-boundary.test.mjs');
		mkdirSync(dirname(testPath), { recursive: true });
		// A boundary-check test legitimately embeds a forbidden import as a
		// string fixture; it must not be treated as a real violation.
		writeFileSync(
			testPath,
			'const fixture = \'import { x } from "@rivet-dev/agentos-core/test/runtime";\\n\';\n',
		);
		execFileSync(process.execPath, [scriptPath, '--root', root], { stdio: 'pipe' });
	});
});

test('rejects Agent OS Rust crate references', () => {
	withFixture((root) => {
		writeJson(root, 'package.json', { name: 'secure-exec-workspace' });
		const cargoPath = join(root, 'crates/sidecar/Cargo.toml');
		mkdirSync(dirname(cargoPath), { recursive: true });
		writeFileSync(cargoPath, '[dependencies]\nagentos-sidecar = { path = "../sidecar" }\n');
		const result = spawnSync(process.execPath, [scriptPath, '--root', root], {
			encoding: 'utf8',
		});
		assert.notEqual(result.status, 0);
		assert.match(result.stderr, /agentos-sidecar/);
	});
});
