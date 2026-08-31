import { execFile } from "node:child_process";
import {
	access,
	mkdir,
	mkdtemp,
	readdir,
	readFile,
	rm,
	writeFile,
} from "node:fs/promises";
import { tmpdir } from "node:os";
import { basename, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);
const repositoryRoot = resolve(new URL("../", import.meta.url).pathname);
const packDirectory = join(repositoryRoot, ".pack");

await rm(packDirectory, { recursive: true, force: true });
await mkdir(packDirectory, { recursive: true });
for (const packagePath of [
	"packages/dynamic-apps-builder",
	"packages/dynamic-apps",
]) {
	await execFileAsync(
		"pnpm",
		["--dir", packagePath, "pack", "--pack-destination", packDirectory],
		{ cwd: repositoryRoot },
	);
}

const tarballs = await readdir(packDirectory);
const builderTarball = join(
	packDirectory,
	tarballs.find((name) => name.includes("dynamic-apps-builder")) ?? "missing",
);
const mainTarball = join(
	packDirectory,
	tarballs.find(
		(name) => name.includes("dynamic-apps-") && !name.includes("builder"),
	) ?? "missing",
);
await access(builderTarball);
await access(mainTarball);

const fixture = await mkdtemp(join(tmpdir(), "dynamic-apps-packed-"));
await writeFile(
	join(fixture, "package.json"),
	JSON.stringify({
		private: true,
		type: "module",
		dependencies: {
			"@rivet-dev/dynamic-apps": `file:${mainTarball}`,
			"@rivet-dev/dynamic-apps-builder": `file:${builderTarball}`,
		},
	}),
);
await execFileAsync(
	"npm",
	["install", "--no-audit", "--no-fund", "--loglevel=error"],
	{ cwd: fixture },
);

const builderRoot = join(
	fixture,
	"node_modules/@rivet-dev/dynamic-apps-builder",
);
const mainRoot = join(fixture, "node_modules/@rivet-dev/dynamic-apps");
const builder = await import(pathToFileURL(join(builderRoot, "dist/index.js")));
if (basename(builder.default.packagePath) !== "package.aospkg") {
	throw new Error("packed builder did not export package.aospkg");
}
await access(builder.default.packagePath);

const main = await import(pathToFileURL(join(mainRoot, "dist/index.js")));
const exports = Object.keys(main).sort();
if (
	JSON.stringify(exports) !==
	JSON.stringify(["appsRouter", "deployApp", "setDynamicAppsLogHandler"])
) {
	throw new Error(`packed main package has unexpected exports: ${exports}`);
}
main.setDynamicAppsLogHandler(() => {});
main.setDynamicAppsLogHandler(undefined);

for (const packageRoot of [builderRoot, mainRoot]) {
	const manifest = JSON.parse(
		await readFile(join(packageRoot, "package.json"), "utf8"),
	);
	const serialized = JSON.stringify(manifest);
	if (serialized.includes("workspace:") || serialized.includes("catalog:")) {
		throw new Error(
			`${manifest.name} contains an unpublished dependency specifier`,
		);
	}
	if (
		manifest.name === "@rivet-dev/dynamic-apps" &&
		manifest.dependencies?.["isolated-vm"]
	) {
		throw new Error(
			"packed Dynamic Apps still depends directly on isolated-vm",
		);
	}
}

const declaration = await readFile(join(mainRoot, "dist/index.d.ts"), "utf8");
if (/from ["']@rivet-dev\/agentos/.test(declaration)) {
	throw new Error(
		"packed public declarations expose agentOS implementation types",
	);
}
if (
	!declaration.includes("setDynamicAppsLogHandler") ||
	!declaration.includes("interface DynamicAppsLogEvent")
) {
	throw new Error("packed declarations omit the structured log API");
}

const workspace = join(fixture, "builder-smoke");
const release = join(fixture, "builder-release");
await mkdir(workspace, { recursive: true });
await writeFile(
	join(workspace, "entry.ts"),
	'export async function dispatch() { return { status: 200, statusText: "OK", headers: [], bodyBase64: "" }; }\n',
);
await writeFile(
	join(workspace, "package.json"),
	JSON.stringify({ private: true, type: "module" }),
);
const configPath = join(fixture, "builder-config.json");
await writeFile(
	configPath,
	JSON.stringify({
		workspace,
		release,
		entrypoint: "entry.ts",
		version: "packed-smoke",
		sourceFiles: ["entry.ts"],
		usesRivetKit: false,
		directAgentOs: true,
		maxOutputBytes: 1024 * 1024,
		maxOutputFiles: 16,
		maxFileBytes: 512 * 1024,
	}),
);
await execFileAsync(process.execPath, [
	join(builderRoot, "cli/apps-builder.mjs"),
	configPath,
]);
await access(join(release, "main.mjs"));
const directModule = await import(pathToFileURL(join(release, "main.mjs")));
if (typeof directModule.dispatch !== "function") {
	throw new Error("packed direct builder did not emit an ESM dispatcher");
}

const actorWorkspace = join(fixture, "actor-builder-smoke");
const actorRelease = join(fixture, "actor-builder-release");
await mkdir(actorWorkspace, { recursive: true });
await writeFile(
	join(actorWorkspace, "entry.ts"),
	'import { setup } from "rivetkit"; export const registry = setup({ use: {} });\n',
);
await writeFile(
	join(actorWorkspace, "package.json"),
	JSON.stringify({ private: true, type: "module" }),
);
const actorConfigPath = join(fixture, "actor-builder-config.json");
await writeFile(
	actorConfigPath,
	JSON.stringify({
		workspace: actorWorkspace,
		release: actorRelease,
		entrypoint: "entry.ts",
		version: "packed-actor-smoke",
		sourceFiles: ["entry.ts"],
		usesRivetKit: true,
		platformRivetKit: true,
		maxOutputBytes: 1024 * 1024,
		maxOutputFiles: 16,
		maxFileBytes: 512 * 1024,
	}),
);
await execFileAsync(process.execPath, [
	join(builderRoot, "cli/apps-builder.mjs"),
	actorConfigPath,
]);
const actorEntrypoint = join(actorRelease, "main.mjs");
await access(actorEntrypoint);
const actorModule = await import(pathToFileURL(actorEntrypoint));
if (typeof actorModule.registry?.handler !== "function") {
	throw new Error("packed actor builder did not emit a RivetKit registry");
}

console.log(
	`Verified ${basename(builderTarball)} and ${basename(mainTarball)}.`,
);
