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
	mainPackage.dependencies?.["@rivet-dev/agentos"] === "0.2.15",
	"agentOS must remain a pinned implementation dependency",
);
assert(
	!mainPackage.peerDependencies?.["@rivet-dev/agentos"],
	"agentOS must not be a peer dependency",
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
		`${path} exposes agentOS instead of the Dynamic Apps setup wrapper`,
	);
}

const actors = await read("packages/dynamic-apps/src/actors.ts");
for (const identity of [
	'const APP_ACTOR_NAME = "agentOSAppsApp"',
	'const SCALER_ACTOR_NAME = "agentOSAppsScaler"',
	'const REPLICA_ACTOR_NAME = "agentOSAppsReplica"',
	'"/opt/agentos/bin/apps-builder"',
]) {
	assert(
		actors.includes(identity),
		`compatibility identity changed: ${identity}`,
	);
}

const runtime = await read("packages/dynamic-apps/src/runtime.ts");
assert(
	runtime.includes('hash.update("agentos-apps-release-v15\\0")'),
	"release hash domain changed",
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
