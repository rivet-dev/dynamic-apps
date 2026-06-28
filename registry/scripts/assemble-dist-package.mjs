#!/usr/bin/env node

/**
 * Assemble a clean `dist/package/` for a WASM / command registry package.
 *
 * Why this exists: a package's `src/index.ts` sets the descriptor `dir` that the
 * sidecar projects read-only under `/opt/agentos/<name>/<version>`. The sidecar
 * copies that WHOLE directory verbatim. If `dir` points at the package source root
 * it drags in `src/`, dev `node_modules/`, `tsconfig.json`, `.turbo/`, `dist/` — all
 * mounted into every VM. The descriptor must instead point at a CLEAN runtime dir
 * that holds ONLY what the package ships at runtime:
 *
 *   dist/package/
 *     package.json   { name, version, bin: { <cmd>: "bin/<cmd>" } }
 *     bin/<cmd>      the compiled WASM command binaries (copied verbatim)
 *     share/...      (optional) man pages, if the source package ships them
 *
 * Commands are derived from the source `bin/` directory's files (the projection's
 * WASM fallback links every file in `bin/`), and `name`/`version` are read from the
 * source `package.json`. The src descriptor is then pointed at this dir, i.e.
 * `dir = resolve(dirname(fileURLToPath(import.meta.url)), "package")` (from
 * `dist/index.js` that resolves to `dist/package`).
 *
 * This handles the COMMON case (WASM / compiled command packages). It does NOT
 * handle:
 *   - JS agent packages (pi, claude, opencode, pi-cli) whose `dist/package` is a
 *     self-contained node runtime closure — those are built with
 *     `@rivet-dev/agentos-toolchain pack ./<dir> --out dist/package --agent <cmd>`.
 *   - Meta packages (build-essential, common, everything) that default-export an
 *     ARRAY of descriptors and have no `dir`/`bin` of their own — they ship nothing.
 *
 * Usage (run from a package directory, or pass the package dir):
 *   node /abs/registry/scripts/assemble-dist-package.mjs [packageDir]
 *
 * Idempotent: the output `dist/package/` is removed and rebuilt on every run.
 */

import {
	cpSync,
	existsSync,
	mkdirSync,
	readFileSync,
	readdirSync,
	rmSync,
	statSync,
	writeFileSync,
} from "node:fs";
import { basename, join, resolve } from "node:path";

function fail(message) {
	process.stderr.write(`error: ${message}\n`);
	process.exit(1);
}

function main() {
	const packageDir = resolve(process.argv[2] ?? process.cwd());

	// 1. Read name + version from the source package.json.
	const srcPkgPath = join(packageDir, "package.json");
	if (!existsSync(srcPkgPath)) {
		fail(`no package.json in ${packageDir}`);
	}
	let srcPkg;
	try {
		srcPkg = JSON.parse(readFileSync(srcPkgPath, "utf8"));
	} catch (error) {
		fail(`package.json in ${packageDir} is not valid JSON: ${String(error)}`);
	}
	const name = srcPkg.name;
	const version = srcPkg.version;
	if (typeof name !== "string" || name.length === 0) {
		fail(`package.json in ${packageDir} is missing a valid "name"`);
	}
	if (typeof version !== "string" || version.length === 0) {
		fail(`package.json in ${packageDir} is missing a valid "version"`);
	}
	const srcManifest = readJsonIfExists(join(packageDir, "agentos-package.json"));
	const manifest = {
		name:
			typeof srcManifest?.name === "string" && srcManifest.name.length > 0
				? srcManifest.name
				: unscopedName(name),
		...(srcManifest?.agent !== undefined ? { agent: srcManifest.agent } : {}),
		...(srcManifest?.provides !== undefined
			? { provides: srcManifest.provides }
			: {}),
	};

	// 2. Commands = the files in the source bin/ directory. This is the WASM /
	//    command case. A package with NO bin/ (or an empty one) is a PLANNED
	//    package whose binaries are built on upload and gitignored (e.g. make,
	//    zip, duckdb) — it ships a valid, empty placeholder now and picks up its
	//    commands automatically once the upload build populates bin/. (Meta
	//    packages that export an ARRAY never run this helper — they have no `dir`.)
	const srcBinDir = join(packageDir, "bin");
	const hasBinDir = existsSync(srcBinDir) && statSync(srcBinDir).isDirectory();
	const commands = hasBinDir
		? readdirSync(srcBinDir, { withFileTypes: true })
				.filter((entry) => entry.isFile() || entry.isSymbolicLink())
				.map((entry) => entry.name)
				.sort()
		: [];

	// 3. (Re)create a clean output dir — idempotent.
	const outDir = join(packageDir, "dist", "package");
	rmSync(outDir, { recursive: true, force: true });
	mkdirSync(outDir, { recursive: true });

	// 4. Copy bin/ verbatim (preserve symlinks so any in-package relative aliases
	//    stay in-tree, matching the sidecar's verbatim projection copy). Skipped
	//    for a planned/empty package — there is nothing to copy yet.
	if (commands.length > 0) {
		cpSync(srcBinDir, join(outDir, "bin"), {
			recursive: true,
			verbatimSymlinks: true,
		});
	}

	// 5. Copy share/ (man pages, etc.) if the package ships it — the projection
	//    builds a man symlink farm from <pkg>/share/man.
	const srcShareDir = join(packageDir, "share");
	if (existsSync(srcShareDir) && statSync(srcShareDir).isDirectory()) {
		cpSync(srcShareDir, join(outDir, "share"), {
			recursive: true,
			verbatimSymlinks: true,
		});
	}

	// 6. Write the clean runtime package.json. The bin map names every command
	//    explicitly (cmd -> bin/cmd); the sidecar reads `version` here and links
	//    each command into /opt/agentos/bin.
	const bin = {};
	for (const cmd of commands) {
		bin[cmd] = `bin/${basename(cmd)}`;
	}
	writeFileSync(
		join(outDir, "package.json"),
		`${JSON.stringify({ name, version, bin }, null, 2)}\n`,
	);
	writeFileSync(
		join(outDir, "agentos-package.json"),
		`${JSON.stringify(manifest, null, 2)}\n`,
	);

	process.stdout.write(
		commands.length > 0
			? `assembled ${name}@${version} -> ${outDir}\n` +
					`  commands (${commands.length}): ${commands.join(", ")}\n`
			: `assembled ${name}@${version} -> ${outDir} (PLANNED: empty placeholder, ` +
					`no bin/ yet — commands appear once built on upload)\n`,
	);
}

function readJsonIfExists(path) {
	if (!existsSync(path)) return undefined;
	try {
		return JSON.parse(readFileSync(path, "utf8"));
	} catch (error) {
		fail(`${path} is not valid JSON: ${String(error)}`);
	}
}

function unscopedName(name) {
	return name.replace(/^@[^/]+\//, "");
}

main();
