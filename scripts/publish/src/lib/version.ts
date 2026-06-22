/**
 * Version management split across two surfaces:
 *
 * - `bumpPackageJsons` — rewrites every discovered publishable package.json
 *   `version` field and injects `optionalDependencies` on meta packages.
 *   Safe to call in CI on every run. Uses discovery as the source of truth.
 *   Does NOT touch Cargo.toml or non-discovered files.
 *
 * - `bumpCargoVersions` — rewrites the Rust `[workspace.package]` version and
 *   the lock-step internal crate `version` fields under `[workspace.dependencies]`
 *   so the crates.io publish chain stays consistent.
 *
 * - `updateSourceFiles` — rewrites non-package.json, non-Cargo source files
 *   (example dependency specs). Called only by the local `cut-release.ts`.
 *
 * - `resolveVersion` / `shouldTagAsLatest` — semver helpers for the local cut.
 */
import * as fs from "node:fs/promises";
import { join, resolve as resolvePath } from "node:path";
import { $ } from "execa";
import { glob } from "glob";
import * as semver from "semver";
import { scoped } from "./logger.js";
import { buildMetaPlatformMap, discoverPackages } from "./packages.js";

const log = scoped("version");

interface PackageJson {
	name?: string;
	version?: string;
	dependencies?: Record<string, string>;
	devDependencies?: Record<string, string>;
	peerDependencies?: Record<string, string>;
	optionalDependencies?: Record<string, string>;
}

const DEP_FIELDS = [
	"dependencies",
	"devDependencies",
	"peerDependencies",
	"optionalDependencies",
] as const;

export interface BumpOptions {
	/** If true, report actions but do not write. */
	dryRun?: boolean;
	/**
	 * When true, only rewrite the `version` field. Does not touch dependency
	 * references or inject `optionalDependencies`. Safe to commit to git
	 * because it preserves `workspace:*` dep specs that the lockfile expects.
	 *
	 * When false (default), also rewrites `workspace:*` deps to the literal
	 * version and injects `optionalDependencies` on meta packages. This is
	 * the publish-time mode used by CI — never committed.
	 */
	versionOnly?: boolean;
}

/**
 * Rewrite every discovered package's `version` to the given string.
 *
 * In full mode (default, `versionOnly: false`): also injects
 * `optionalDependencies` on meta packages and rewrites `workspace:*`
 * dependency references to the literal version. This is the publish-time
 * mode used by CI and must NOT be committed — it breaks
 * `pnpm install --frozen-lockfile` because the lockfile expects
 * `workspace:*`, not literal versions.
 *
 * In version-only mode (`versionOnly: true`): only rewrites the `version`
 * field. Safe to commit. Used by `cut-release.ts` so the repo records the
 * new version in package.jsons without breaking the lockfile.
 *
 * Returns the number of files written.
 */
export async function bumpPackageJsons(
	repoRoot: string,
	version: string,
	opts: BumpOptions = {},
): Promise<number> {
	const packages = discoverPackages(repoRoot);
	const packageNames = new Set(packages.map((p) => p.name));
	const metaPlatformMap = buildMetaPlatformMap(packages);
	const versionOnly = opts.versionOnly ?? false;

	let updated = 0;
	for (const pkg of packages) {
		const pkgJsonPath = join(pkg.dir, "package.json");
		const raw = await fs.readFile(pkgJsonPath, "utf8");
		const pkgJson: PackageJson = JSON.parse(raw);

		pkgJson.version = version;

		if (!versionOnly) {
			// Inject optionalDependencies on meta packages so end users get the
			// correct platform-specific binary via npm's os/cpu/libc resolution.
			const platformPkgs = metaPlatformMap.get(pkg.name);
			if (platformPkgs && platformPkgs.length > 0) {
				pkgJson.optionalDependencies = pkgJson.optionalDependencies ?? {};
				for (const platPkg of platformPkgs) {
					pkgJson.optionalDependencies[platPkg] = version;
				}
			}

			for (const field of DEP_FIELDS) {
				const deps = pkgJson[field];
				if (!deps) continue;
				for (const [dep, spec] of Object.entries(deps)) {
					const isWorkspace =
						typeof spec === "string" && spec.startsWith("workspace:");
					if (!isWorkspace) continue;
					// Convert workspace:* deps on any in-repo package to the literal
					// version. `packageNames` only covers DISCOVERED (CI-published)
					// packages, so a dep on a manually-published-but-excluded package
					// (e.g. @secure-exec/core) would otherwise leak `workspace:*` into
					// the published tarball and break consumers. The whole @secure-exec
					// set publishes at one version, so pin those too.
					const isOurPkg =
						packageNames.has(dep) || dep.startsWith("@secure-exec/");
					if (!isOurPkg) continue;
					deps[dep] = version;
				}
			}
		}

		// Tab-indented, trailing newline — matches the repo convention.
		const newContent = `${JSON.stringify(pkgJson, null, "\t")}\n`;
		if (opts.dryRun) {
			log.info(`[dry-run] would update ${pkg.name} -> ${version}`);
		} else {
			await fs.writeFile(pkgJsonPath, newContent);
			log.info(`updated ${pkg.name} -> ${version}`);
		}
		updated++;
	}

	log.info(`total: ${updated} package.json files updated to ${version}`);
	return updated;
}

/**
 * Rewrite the Rust workspace version and the lock-step internal crate versions.
 *
 * a6 declares internal crate deps as inline tables in `[workspace.dependencies]`,
 * e.g. `secure-exec-bridge = { path = "crates/bridge", version = "0.2.0-rc.3" }`,
 * so a single regex rewrites every `version` field next to a `path`.
 */
export async function bumpCargoVersions(
	repoRoot: string,
	version: string,
	opts: Pick<BumpOptions, "dryRun"> = {},
): Promise<void> {
	const cargoTomlPath = join(repoRoot, "Cargo.toml");
	const cargoToml = await fs.readFile(cargoTomlPath, "utf8");
	let next = cargoToml.replace(
		/(\[workspace\.package\]\n(?:[^\n]*\n)*?[ \t]*version = )"[^"]+"/,
		`$1"${version}"`,
	);
	// Match EVERY internal path-dep, not just `agentos-*`/`secure-exec-*`: the
	// presence of a `path = "..."` is what marks an in-repo workspace crate, so
	// unprefixed members (e.g. `vfs = { path = "crates/vfs", version = "..." }`)
	// must be bumped in lock-step too. Missing one leaves a dependent pinned to
	// the old `^0.3.0-rc.1` while the crate itself moved to the preview version,
	// which fails cargo resolution ("failed to select a version for `vfs`").
	next = next.replace(
		/^([a-zA-Z0-9_-]+ = \{ path = "[^"]+", version = ")[^"]+(" \})/gm,
		`$1${version}$2`,
	);

	if (next === cargoToml) {
		log.info(`Cargo.toml Rust versions already set to ${version}`);
		return;
	}

	if (opts.dryRun) {
		log.info(`[dry-run] would update Cargo.toml Rust versions -> ${version}`);
	} else {
		await fs.writeFile(cargoTomlPath, next);
		log.info(`updated Cargo.toml Rust versions -> ${version}`);
	}
}

/**
 * Rewrite non-package.json, non-Cargo source files to the given version.
 * Called only by the local release cutter. Examples that pin `@secure-exec/*`
 * to a literal version (rather than `workspace:*`) get updated so released
 * examples carry the new version. `required: false` because a6 examples use
 * `workspace:*` today, so a no-match is expected and not an error.
 */
export async function updateSourceFiles(
	repoRoot: string,
	version: string,
): Promise<void> {
	const findReplace: Array<{
		path: string;
		find: RegExp;
		replace: string;
		required?: boolean;
	}> = [
		{
			path: "examples/**/package.json",
			find: /"(@secure-exec\/[^"]+)": "\^?[0-9]+\.[0-9]+\.[0-9]+(?:-[^"]+)?"/g,
			replace: `"$1": "^${version}"`,
			required: false,
		},
	];

	for (const { path: globPath, find, replace, required = true } of findReplace) {
		const paths = await glob(globPath, {
			cwd: repoRoot,
			ignore: ["**/node_modules/**"],
		});
		if (paths.length === 0) {
			if (required) {
				throw new Error(`no paths matched: ${globPath}`);
			}
			continue;
		}
		for (const fileRelPath of paths) {
			const filePath = resolvePath(repoRoot, fileRelPath);
			const file = await fs.readFile(filePath, "utf-8");

			find.lastIndex = 0;
			const hasMatch = find.test(file);
			if (!hasMatch) continue;

			find.lastIndex = 0;
			const newFile = file.replace(find, replace);
			await fs.writeFile(filePath, newFile);
			log.info(`updated ${fileRelPath}`);
		}
	}
}

// -----------------------------------------------------------------------------
// Local semver helpers — used only by `cut-release.ts`.
// -----------------------------------------------------------------------------

async function getAllGitVersions(): Promise<string[]> {
	try {
		await $`git fetch --tags --force --quiet`;
	} catch {
		throw new Error(
			"could not fetch git tags — refusing to compute latest flag from stale local tags",
		);
	}
	const result = await $`git tag -l v*`;
	const tags = result.stdout.trim().split("\n").filter(Boolean);
	if (tags.length === 0) return [];
	return tags
		.map((tag) => tag.replace(/^v/, ""))
		.filter((v) => semver.valid(v))
		.sort((a, b) => semver.rcompare(a, b));
}

export async function getLatestGitVersion(): Promise<string | null> {
	const versions = await getAllGitVersions();
	const stable = versions.filter((v) => {
		const p = semver.parse(v);
		return p && p.prerelease.length === 0;
	});
	return stable[0] ?? null;
}

export async function listRecentVersions(limit = 10): Promise<string[]> {
	const all = await getAllGitVersions();
	return all.slice(0, limit);
}

/**
 * Auto-detect whether a version should be tagged as `latest`. A version is
 * `latest` only if it has no prerelease identifier AND is greater than any
 * existing stable git tag.
 */
export async function shouldTagAsLatest(version: string): Promise<boolean> {
	const parsed = semver.parse(version);
	if (!parsed) throw new Error(`invalid semantic version: ${version}`);
	if (parsed.prerelease.length > 0) return false;
	const latest = await getLatestGitVersion();
	if (!latest) return true;
	return semver.gt(version, latest);
}

export interface ResolveVersionOpts {
	version?: string;
	major?: boolean;
	minor?: boolean;
	patch?: boolean;
}

export async function resolveVersion(
	opts: ResolveVersionOpts,
): Promise<string> {
	if (opts.version) {
		if (!semver.valid(opts.version)) {
			throw new Error(`invalid semantic version: ${opts.version}`);
		}
		return opts.version;
	}
	if (!opts.major && !opts.minor && !opts.patch) {
		throw new Error("must provide --version, --major, --minor, or --patch");
	}
	const latest = await getLatestGitVersion();
	if (!latest) {
		throw new Error(
			"no existing version tags found — use --version to set an explicit version",
		);
	}
	let next: string | null = null;
	if (opts.major) next = semver.inc(latest, "major");
	else if (opts.minor) next = semver.inc(latest, "minor");
	else if (opts.patch) next = semver.inc(latest, "patch");
	if (!next) throw new Error("failed to compute next version");
	return next;
}
