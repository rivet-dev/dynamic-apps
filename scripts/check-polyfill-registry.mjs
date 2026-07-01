#!/usr/bin/env node
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, "..");

const registryPath = path.join(
	repoRoot,
	"crates",
	"execution",
	"assets",
	"polyfill-registry.json",
);
const builtinModulesPath = path.join(
	repoRoot,
	"packages",
	"build-tools",
	"bridge-src",
	"builtins",
	"builtin-modules.ts",
);

const registry = JSON.parse(readFileSync(registryPath, "utf8"));
const builtinModulesSource = readFileSync(builtinModulesPath, "utf8");
const registryNames = new Set(
	(registry.groups ?? []).flatMap((group) => group.names ?? []),
);

const builtinModules = extractStringList(
	builtinModulesSource,
	/static builtinModules = \[([\s\S]*?)\];/,
	"Module.builtinModules",
);
const loadBuiltinCases = new Set(
	[...builtinModulesSource.matchAll(/case "([^"]+)":/g)]
		.map((match) => match[1])
		.filter((name) => builtinModules.has(name)),
);

const errors = [];
reportMissing(
	[...builtinModules].filter((name) => !registryNames.has(name)),
	"is in Module.builtinModules but missing from polyfill-registry.json",
);
reportMissing(
	[...registryNames].filter((name) => !builtinModules.has(name)),
	"is in polyfill-registry.json but missing from Module.builtinModules",
);
reportMissing(
	[...builtinModules].filter((name) => !loadBuiltinCases.has(name)),
	"is in Module.builtinModules but missing from loadBuiltinModule()",
);

if (errors.length > 0) {
	console.error("Polyfill registry drift detected:");
	for (const error of errors) {
		console.error(`  - ${error}`);
	}
	process.exit(1);
}

function extractStringList(source, pattern, label) {
	const match = source.match(pattern);
	if (!match) {
		console.error(
			`Unable to find ${label} in packages/build-tools/bridge-src/builtins/builtin-modules.ts`,
		);
		process.exit(1);
	}
	return new Set([...match[1].matchAll(/"([^"]+)"/g)].map((entry) => entry[1]));
}

function reportMissing(names, suffix) {
	for (const name of names.sort((a, b) => a.localeCompare(b))) {
		errors.push(`${name} ${suffix}`);
	}
}
