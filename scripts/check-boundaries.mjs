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
assert(
	!mainPackage.dependencies?.["@rivet-dev/agentos"],
	"Dynamic Apps must use agentOS core rather than the actor package",
);
assert(
	mainPackage.dependencies?.["@rivet-dev/agentos-core"] === "0.2.15",
	"agentOS core must remain a pinned implementation dependency",
);
assert(
	!mainPackage.dependencies?.["isolated-vm"],
	"isolated-vm must not be a direct runtime dependency",
);
assert(
	!mainPackage.peerDependencies?.["@rivet-dev/agentos-core"],
	"agentOS core must not be a peer dependency",
);
assert(
	mainPackage.dependencies?.["@rivet-dev/dynamic-apps-builder"] ===
		`workspace:${mainPackage.version}` ||
		mainPackage.dependencies?.["@rivet-dev/dynamic-apps-builder"] ===
			mainPackage.version,
	"the builder must use the exact matching workspace version",
);

for (const path of await walk("examples/")) {
	if (!path.endsWith(".ts") && !path.endsWith("package.json")) continue;
	const source = await read(path);
	assert(
		!source.includes('from "@rivet-dev/agentos"') &&
			!source.includes('"@rivet-dev/agentos":'),
		`${path} exposes the Dynamic Apps deployment implementation`,
	);
}

const actors = await read("packages/dynamic-apps/src/actors.ts");
for (const identity of [
	"agentOSAppsApp: AnyActorDefinition;",
	"const agentOSAppsApp = actor({",
	'"/opt/agentos/bin/apps-builder"',
]) {
	assert(
		actors.includes(identity),
		`compatibility identity changed: ${identity}`,
	);
}

const runtime = await read("packages/dynamic-apps/src/runtime.ts");
assert(
	runtime.includes('hash.update("agentos-apps-release-v17-direct-actors\\0")'),
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

console.log("Dynamic Apps package boundaries are valid.");
