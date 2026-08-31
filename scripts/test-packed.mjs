import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
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
	"packages/dynamic-apps-core",
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
		(name) =>
			name.includes("dynamic-apps-") &&
			!name.includes("builder") &&
			!name.includes("core"),
	) ?? "missing",
);
const coreTarball = join(
	packDirectory,
	tarballs.find((name) => name.includes("dynamic-apps-core")) ?? "missing",
);
await access(builderTarball);
await access(coreTarball);
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
			"@rivet-dev/dynamic-apps-core": `file:${coreTarball}`,
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
const coreRoot = join(fixture, "node_modules/@rivet-dev/dynamic-apps-core");
const builder = await import(pathToFileURL(join(builderRoot, "dist/index.js")));
if (basename(builder.default.packagePath) !== "package.aospkg") {
	throw new Error("packed builder did not export package.aospkg");
}
await access(builder.default.packagePath);

const core = await import(pathToFileURL(join(coreRoot, "dist/index.js")));
if (
	JSON.stringify(Object.keys(core).sort()) !==
	JSON.stringify(["createDynamicApps"])
) {
	throw new Error(
		`packed core package has unexpected exports: ${Object.keys(core)}`,
	);
}

const main = await import(pathToFileURL(join(mainRoot, "dist/index.js")));
const exports = Object.keys(main).sort();
if (
	JSON.stringify(exports) !==
	JSON.stringify([
		"appsRouter",
		"deployApp",
		"rivetActorsSkill",
		"setDynamicAppsLogHandler",
		"webServerSkill",
	])
) {
	throw new Error(`packed main package has unexpected exports: ${exports}`);
}
main.setDynamicAppsLogHandler(() => {});
main.setDynamicAppsLogHandler(undefined);

for (const packageRoot of [builderRoot, coreRoot, mainRoot]) {
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

const coreArtifactDirectory = join(fixture, "core-artifact");
const coreArtifactArchive = join(fixture, "core-artifact.tar");
await mkdir(join(coreArtifactDirectory, "direct"), { recursive: true });
await writeFile(
	join(coreArtifactDirectory, "direct/main.mjs"),
	await readFile(join(release, "main.mjs"), "utf8"),
);
await writeFile(
	join(coreArtifactDirectory, "agentos-package.json"),
	JSON.stringify({ name: "packed-core-smoke", version: "1.0.0" }),
);
await execFileAsync(
	"tar",
	["-cf", coreArtifactArchive, "direct", "agentos-package.json"],
	{ cwd: coreArtifactDirectory },
);
const { packAospkgFromTarBytes } = await import(
	pathToFileURL(
		join(
			repositoryRoot,
			"packages/dynamic-apps-core/node_modules/@rivet-dev/agentos-toolchain/dist/index.js",
		),
	)
);
const coreArtifact = new Uint8Array(
	packAospkgFromTarBytes(await readFile(coreArtifactArchive)).bytes,
);
let activeRelease;
const dynamicApps = core.createDynamicApps({
	artifactCache: {
		async get() {
			return coreArtifact;
		},
		async put() {},
	},
	async publishRelease(input) {
		activeRelease = {
			appId: input.appId,
			release: "packed-1",
			artifact: {
				...input.artifact,
				bytes: new Uint8Array(input.artifact.bytes),
				hash: createHash("sha256").update(input.artifact.bytes).digest("hex"),
			},
			maxRequestBytes: 1024 * 1024,
			maxResponseBytes: 4 * 1024 * 1024,
		};
	},
	async loadActiveRelease() {
		return activeRelease;
	},
	async watchActiveRelease() {
		return () => {};
	},
	executor: { executionMode: "ephemeral" },
});
try {
	await dynamicApps.deployApp({
		appId: "hello",
		files: {
			"package.json": JSON.stringify({ type: "module", main: "index.js" }),
			"index.js": "export default { fetch() { return new Response('ok') } }",
		},
	});
	const response = await dynamicApps.appsRouter.request("/hello/");
	if (response.status !== 200) {
		throw new Error(`packed core smoke returned HTTP ${response.status}`);
	}
} finally {
	await dynamicApps.dispose();
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
	`Verified ${basename(builderTarball)}, ${basename(coreTarball)}, and ${basename(mainTarball)}.`,
);
