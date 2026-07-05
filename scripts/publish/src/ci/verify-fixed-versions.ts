#!/usr/bin/env tsx
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { discoverPackages } from "../lib/packages.js";

const EXPECTED_VERSION = "0.0.1";
const defaultRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../../../..");

function parseArgs(argv: string[]) {
	const options = { root: defaultRoot };
	for (let i = 0; i < argv.length; i++) {
		const arg = argv[i];
		if (arg === "--root") {
			options.root = argv[++i];
			continue;
		}
		if (arg.startsWith("--root=")) {
			options.root = arg.slice("--root=".length);
			continue;
		}
		throw new Error(`unknown argument: ${arg}`);
	}
	return { root: resolve(options.root) };
}

function toRel(root: string, path: string) {
	return relative(root, path).split(sep).join("/");
}

function readPackageVersion(
	path: string,
	relPath: string,
	failures: string[],
) {
	let manifest: { version?: unknown };
	try {
		manifest = JSON.parse(readFileSync(path, "utf8"));
	} catch (err) {
		failures.push(
			`${relPath} could not be parsed: ${err instanceof Error ? err.message : String(err)}`,
		);
		return undefined;
	}
	return manifest.version;
}

function readWorkspacePackageVersion(root: string, failures: string[]) {
	const cargoPath = join(root, "Cargo.toml");
	if (!existsSync(cargoPath)) {
		failures.push("Cargo.toml is missing");
		return undefined;
	}

	let inWorkspacePackage = false;
	for (const line of readFileSync(cargoPath, "utf8").split("\n")) {
		const header = line.match(/^\[([^\]]+)\]\s*$/);
		if (header) {
			inWorkspacePackage = header[1] === "workspace.package";
			continue;
		}
		if (!inWorkspacePackage) continue;
		const version = line.match(/^\s*version\s*=\s*"([^"]+)"/)?.[1];
		if (version) return version;
	}

	failures.push("Cargo.toml [workspace.package] is missing version");
	return undefined;
}

function checkWorkspaceCrateDeps(root: string, failures: string[]) {
	const cargoPath = join(root, "Cargo.toml");
	if (!existsSync(cargoPath)) return;

	let inDeps = false;
	for (const line of readFileSync(cargoPath, "utf8").split("\n")) {
		const header = line.match(/^\[([^\]]+)\]\s*$/);
		if (header) {
			inDeps = header[1] === "workspace.dependencies";
			continue;
		}
		if (!inDeps) continue;
		if (!/path\s*=\s*"crates\//.test(line)) continue;

		const name = line.match(/^\s*([A-Za-z0-9_-]+)\s*=/)?.[1];
		const version = line.match(/version\s*=\s*"([^"]+)"/)?.[1];
		if (version !== EXPECTED_VERSION) {
			failures.push(
				`Cargo.toml [workspace.dependencies] ${name ?? "(unknown)"} version is ${version === undefined ? "missing" : `"${version}"`}`,
			);
		}
	}
}

export function auditFixedVersions(options: { root?: string } = {}) {
	const root = resolve(options.root ?? defaultRoot);
	const failures: string[] = [];

	if (!existsSync(root)) {
		return {
			root,
			ok: false,
			packageCount: 0,
			failures: [`${root} does not exist`],
		};
	}

	const packages = discoverPackages(root).sort((a, b) =>
		a.relDir.localeCompare(b.relDir),
	);

	for (const pkg of packages) {
		const pkgJsonPath = join(pkg.dir, "package.json");
		const relPath = toRel(root, pkgJsonPath);
		const version = readPackageVersion(pkgJsonPath, relPath, failures);
		if (version !== EXPECTED_VERSION) {
			failures.push(
				`${relPath} version is ${version === undefined ? "missing" : `"${String(version)}"`}`,
			);
		}
	}

	const cargoVersion = readWorkspacePackageVersion(root, failures);
	if (cargoVersion !== undefined && cargoVersion !== EXPECTED_VERSION) {
		failures.push(
			`Cargo.toml [workspace.package] version is "${cargoVersion}"`,
		);
	}
	checkWorkspaceCrateDeps(root, failures);

	return {
		root,
		ok: failures.length === 0,
		packageCount: packages.length,
		failures,
	};
}

export function main(argv = process.argv.slice(2)) {
	const options = parseArgs(argv);
	const result = auditFixedVersions(options);
	if (result.ok) {
		process.stdout.write(
			`verify-fixed-versions: OK (${result.packageCount} packages + Cargo.toml pinned to ${EXPECTED_VERSION})\n`,
		);
		return 0;
	}

	for (const failure of result.failures) {
		process.stderr.write(`verify-fixed-versions: ${failure}\n`);
	}
	return 1;
}

if (import.meta.url === `file://${process.argv[1]}`) {
	process.exitCode = main();
}
