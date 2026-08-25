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
	[
		"install",
		"--ignore-scripts",
		"--no-audit",
		"--no-fund",
		"--loglevel=error",
	],
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
await access(join(mainRoot, "assets/inspector/deployment/index.html"));
await access(join(mainRoot, "assets/inspector/scaler/index.html"));
await access(join(mainRoot, "assets/inspector/replica/index.html"));

const main = await import(pathToFileURL(join(mainRoot, "dist/index.js")));
for (const name of ["setup", "setupApps", "deployApp", "appsRouter"]) {
	if (!(name in main))
		throw new Error(`packed main package is missing ${name}`);
}

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
}

const declaration = await readFile(join(mainRoot, "dist/index.d.ts"), "utf8");
if (/from ["']@rivet-dev\/agentos/.test(declaration)) {
	throw new Error(
		"packed public declarations expose agentOS implementation types",
	);
}

const workspace = join(fixture, "builder-smoke");
const release = join(fixture, "builder-release");
await mkdir(workspace, { recursive: true });
await writeFile(
	join(workspace, "entry.ts"),
	'export default "packed builder";\n',
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

console.log(
	`Verified ${basename(builderTarball)} and ${basename(mainTarball)}.`,
);
