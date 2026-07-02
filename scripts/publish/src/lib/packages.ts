/**
 * Single source of truth for the set of secure-exec packages we publish.
 */
import { execSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, relative, resolve } from "node:path";

export interface Package {
	name: string;
	/** Directory containing the package.json (absolute). */
	dir: string;
	/** Directory relative to repo root. */
	relDir: string;
}

export interface DiscoverPackagesOptions {
	/** Reserved for parity with the rivetkit discovery API. */
	includeReleaseOnly?: boolean;
}

export const EXCLUDED = new Set<string>([
	"publish",
]);

export interface MetaPackageSpec {
	/** Name of the meta package. */
	meta: string;
	/** Prefix of the platform-specific packages to inject. */
	platformPrefix: string;
}

export const META_PACKAGES: readonly MetaPackageSpec[] = [
	{
		meta: "@secure-exec/sidecar",
		platformPrefix: "@secure-exec/sidecar-",
	},
];

const SIDECAR_BINARY_PACKAGE_DIRS = ["packages/sidecar/npm"] as const;

export const SECURE_EXEC_WORKSPACE_PACKAGES = new Set([
	"secure-exec",
	"@secure-exec/browser",
	"@secure-exec/core",
	"@agentos-software/manifest",
	"@secure-exec/sandbox",
	"@secure-exec/sidecar",
	"@secure-exec/typescript",
]);

export const DEFAULT_SIDECAR_PLATFORMS = [
	"linux-x64-gnu",
	"linux-arm64-gnu",
	"darwin-x64",
	"darwin-arm64",
] as const;

export function sidecarPlatforms(): string[] {
	const env = process.env.SIDECAR_PLATFORMS?.trim();
	if (env) return env.split(/\s+/).filter(Boolean);
	return [...DEFAULT_SIDECAR_PLATFORMS];
}

function isPublishable(pkg: { name?: string; private?: boolean }): boolean {
	if (!pkg.name) return false;
	if (pkg.private) return false;
	if (EXCLUDED.has(pkg.name)) return false;
	return true;
}

function readPackageJson(
	dir: string,
): { name?: string; private?: boolean } | null {
	const pkgPath = join(dir, "package.json");
	if (!existsSync(pkgPath)) return null;
	try {
		return JSON.parse(readFileSync(pkgPath, "utf8"));
	} catch {
		return null;
	}
}

export function discoverPackages(
	repoRoot: string,
	_opts: DiscoverPackagesOptions = {},
): Package[] {
	const packages: Package[] = [];
	const seen = new Set<string>();

	const add = (dir: string) => {
		const absDir = resolve(dir);
		const pkg = readPackageJson(absDir);
		if (!pkg) return;
		if (!pkg.name) return;
		if (!isPublishable(pkg)) return;
		if (seen.has(pkg.name)) return;
		seen.add(pkg.name);
		packages.push({
			name: pkg.name,
			dir: absDir,
			relDir: relative(repoRoot, absDir),
		});
	};

	const platformAllowlist = new Set(sidecarPlatforms());
	for (const packageDir of SIDECAR_BINARY_PACKAGE_DIRS) {
		const npmDir = join(repoRoot, packageDir);
		if (existsSync(npmDir)) {
			for (const entry of readdirSync(npmDir).sort()) {
				if (!platformAllowlist.has(entry)) continue;
				const platDir = join(npmDir, entry);
				if (!statSync(platDir).isDirectory()) continue;
				add(platDir);
			}
		}
	}

	const pnpmList = execSync("pnpm -r list --json --depth -1", {
		cwd: repoRoot,
		encoding: "utf8",
		maxBuffer: 16 * 1024 * 1024,
	});
	const workspacePkgs: Array<{
		name: string;
		path: string;
		private?: boolean;
	}> = JSON.parse(pnpmList);
	// PREVIEWS also publish the @agentos-software/* registry packages under the
	// branch dist-tag (version 0.0.0-<branch>.<sha>), so a downstream (agent-os)
	// can resolve the WHOLE dependency surface of one secure-exec sha from npm.
	// RELEASES never include them: they version per-package and release via
	// `just registry-publish <pkg> latest` (agentos-toolchain publish). See
	// registry/README.md for the publish contract.
	const includeRegistryPackages =
		process.env.PUBLISH_INCLUDE_REGISTRY_PACKAGES === "1";
	for (const p of workspacePkgs) {
		if (!p.name) continue;
		// Only the curated secure-exec workspace packages are published on every
		// trigger; the manifest is the one @agentos-software/* package always
		// included (no wasm payload, the runtime depends on it).
		const isRegistryPackage =
			p.name.startsWith("@agentos-software/") &&
			(p.path.includes("registry/software") || p.path.includes("registry/agent"));
		if (
			!SECURE_EXEC_WORKSPACE_PACKAGES.has(p.name) &&
			!(includeRegistryPackages && isRegistryPackage)
		)
			continue;
		add(p.path);
	}

	return packages;
}

export function buildMetaPlatformMap(
	packages: Package[],
): Map<string, string[]> {
	return new Map(
		META_PACKAGES.map(({ meta, platformPrefix }) => [
			meta,
			packages
				.filter((p) => p.name.startsWith(platformPrefix))
				.map((p) => p.name)
				.sort(),
		]),
	);
}

export function assertDiscoverySanity(packages: Package[]): void {
	const byName = new Set(packages.map((p) => p.name));
	const required = [
		"@secure-exec/browser",
		"@secure-exec/core",
		"@agentos-software/manifest",
		"@secure-exec/sandbox",
		"@secure-exec/sidecar",
	];
	const missing = required.filter((r) => !byName.has(r));
	if (missing.length > 0) {
		throw new Error(
			`package discovery missing required packages: ${missing.join(", ")}`,
		);
	}
	const metaMap = buildMetaPlatformMap(packages);
	for (const { meta } of META_PACKAGES) {
		if (!byName.has(meta)) continue;
		const plats = metaMap.get(meta) ?? [];
		if (plats.length === 0) {
			throw new Error(
				`meta package ${meta} has zero platform packages discovered`,
			);
		}
	}
}
