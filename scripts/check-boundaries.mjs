import { readdir, readFile } from "node:fs/promises";
import { join } from "node:path";

const root = new URL("../", import.meta.url);

async function read(path) {
	return readFile(new URL(path, root), "utf8");
}

async function walk(path) {
	const directory = new URL(path, root);
	const entries = await readdir(directory, { withFileTypes: true });
	return (
		await Promise.all(
			entries.map(async (entry) => {
				const child = join(path, entry.name);
				return entry.isDirectory() ? walk(`${child}/`) : [child];
			}),
		)
	).flat();
}

function assert(condition, message) {
	if (!condition) throw new Error(message);
}

const mainPackage = JSON.parse(
	await read("packages/dynamic-apps/package.json"),
);
const corePackage = JSON.parse(
	await read("packages/dynamic-apps-core/package.json"),
);
assert(
	!corePackage.dependencies?.["@rivet-dev/agentos"] &&
		corePackage.dependencies?.["@rivet-dev/agentos-core"] === "0.2.15" &&
		corePackage.dependencies?.["@rivet-dev/agentos-toolchain"] === "0.2.15",
	"core must use exact pinned agentOS Core implementation dependencies",
);
assert(
	!corePackage.dependencies?.["isolated-vm"],
	"core must use agentOS contexts rather than isolated-vm",
);
assert(
	!corePackage.peerDependencies?.["@rivet-dev/agentos-core"],
	"agentOS core must not be a peer dependency",
);
assert(
	corePackage.dependencies?.["@rivet-dev/dynamic-apps-builder"] ===
		`workspace:${corePackage.version}` ||
		corePackage.dependencies?.["@rivet-dev/dynamic-apps-builder"] ===
			corePackage.version,
	"core must use the exact matching builder version",
);
assert(
	mainPackage.dependencies?.["@rivet-dev/dynamic-apps-core"] ===
		`workspace:${mainPackage.version}` ||
		mainPackage.dependencies?.["@rivet-dev/dynamic-apps-core"] ===
			mainPackage.version,
	"the adapter must use the exact matching core version",
);
for (const dependency of [
	"@agentos-software/sh",
	"@agentos-software/tar",
	"@rivet-dev/agentos",
	"@rivet-dev/agentos-core",
	"@rivet-dev/agentos-toolchain",
	"@rivet-dev/dynamic-apps-builder",
	"isolated-vm",
]) {
	assert(
		!mainPackage.dependencies?.[dependency],
		`adapter production dependency leaked from core: ${dependency}`,
	);
}
assert(
	!corePackage.dependencies?.rivetkit && !corePackage.devDependencies?.rivetkit,
	"core must not depend on RivetKit",
);
for (const path of await walk("packages/dynamic-apps-core/src/")) {
	if (!path.endsWith(".ts")) continue;
	const source = await read(path);
	assert(
		!/^\s*import[^\n]*["']rivetkit(?:\/[^"']*)?["']/m.test(source),
		`${path} imports RivetKit`,
	);
}

for (const path of await walk("examples/")) {
	if (!path.endsWith(".ts") && !path.endsWith("package.json")) continue;
	const source = await read(path);
	if (path.startsWith("examples/apps-core-quickstart/")) continue;
	assert(
		!source.includes('from "@rivet-dev/agentos"') &&
			!source.includes('"@rivet-dev/agentos":'),
		`${path} exposes the Dynamic Apps deployment implementation`,
	);
}

const actors = await read("packages/dynamic-apps/src/actors.ts");
for (const identity of [
	"dynamicAppsApp: AnyActorDefinition;",
	"const dynamicAppsApp = actor({",
	"beginReleasePublish:",
	"commitReleasePublish:",
]) {
	assert(
		actors.includes(identity),
		`compatibility identity changed: ${identity}`,
	);
}

const runtime = await read("packages/dynamic-apps-core/src/runtime.ts");
assert(
	runtime.includes(
		'hash.update("dynamic-apps-release-v19-mounted-hono-router\\0")',
	),
	"release hash domain changed",
);

const index = await read("packages/dynamic-apps/src/index.ts");
assert(
	index.includes("export { appsRouter }") &&
		index.includes("export { deployApp }") &&
		index.includes("setDynamicAppsLogHandler,") &&
		!index.includes("setupApps") &&
		!index.includes("createAppsRouter"),
	"package root must expose only the retained values and log handler",
);

const logging = await read("packages/dynamic-apps/src/logging.ts");
assert(
	logging.includes("export function setDynamicAppsLogHandler") &&
		logging.includes("export interface DynamicAppsLogEvent"),
	"structured logging public surface is missing",
);
const coreIndex = await read("packages/dynamic-apps-core/src/index.ts");
assert(
	coreIndex.includes("createDynamicApps") &&
		!coreIndex.includes("buildAppRelease") &&
		!coreIndex.includes("DynamicAppsExecutor"),
	"core root must export the factory without internal build/executor values",
);

const builderManifest = JSON.parse(
	await read("packages/dynamic-apps-builder/agentos-package.json"),
);
assert(
	builderManifest.name === "apps-builder",
	"the VM package identity must remain apps-builder",
);

const declarations = (await walk("packages/dynamic-apps/dist/"))
	.filter((path) => path.endsWith(".d.ts"))
	.map((path) => read(path));
for (const [index, source] of (await Promise.all(declarations)).entries()) {
	assert(
		!source.includes('from "@rivet-dev/agentos') &&
			!source.includes('from "@agentos-software/manifest"'),
		`public declaration ${index + 1} leaks an implementation package`,
	);
}

const coreDeclarations = (await walk("packages/dynamic-apps-core/dist/"))
	.filter((path) => path.endsWith(".d.ts"))
	.map((path) => read(path));
for (const [index, source] of (await Promise.all(coreDeclarations)).entries()) {
	assert(
		!source.includes('from "rivetkit') && !source.includes("from 'rivetkit"),
		`core declaration ${index + 1} leaks RivetKit`,
	);
}
for (const [index, source] of (await Promise.all(declarations)).entries()) {
	assert(
		!source.includes("@rivet-dev/dynamic-apps-core/internal"),
		`adapter declaration ${index + 1} leaks core internal types`,
	);
}

console.log("Dynamic Apps package boundaries are valid.");
