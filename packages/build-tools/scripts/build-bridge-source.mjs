import { build } from "esbuild";
import { mkdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

// Assemble the readable, un-minified V8 bridge seam (crates/execution/assets/
// v8-bridge.source.js) from the TypeScript sources under bridge-src/.
//
// This step ONLY combines the local bridge-src/*.ts modules. Every third-party
// import (node: builtins, bare npm packages such as undici / web-streams-polyfill,
// and the ./undici-shims/* relative helpers) is kept EXTERNAL so the emitted seam
// preserves the exact import structure of the hand-authored source. The downstream
// minify build (build-v8-bridge.mjs) is the single place that resolves + inlines
// those imports, so keeping them external here makes the seam byte/behaviour-neutral
// versus the original single-file source.
const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const packageRoot = path.resolve(scriptDir, "..");
const workspaceRoot = path.resolve(packageRoot, "..", "..");
const bridgeEntry = path.join(packageRoot, "bridge-src", "index.ts");
const bridgeOutput = path.join(
	workspaceRoot,
	"crates",
	"execution",
	"assets",
	"v8-bridge.source.js",
);

await mkdir(path.dirname(bridgeOutput), { recursive: true });

const result = await build({
	entryPoints: [bridgeEntry],
	outfile: bridgeOutput,
	bundle: true,
	minify: false,
	format: "esm",
	platform: "neutral",
	target: "es2020",
	// Keep native class fields (do not lower to __publicField helpers) so the
	// seam preserves readable `static builtinModules = [...]` etc. that the
	// downstream text-consumers (check-polyfill-registry.mjs) grep for.
	supported: {
		"class-field": true,
		"class-static-field": true,
	},
	loader: { ".ts": "ts" },
	write: true,
	treeShaking: false,
	// Externalize everything that is not a local bridge-src relative module.
	packages: "external",
	plugins: [
		{
			name: "secure-exec-bridge-source-externals",
			setup(pluginBuild) {
				// node: builtins stay as import specifiers.
				pluginBuild.onResolve({ filter: /^node:/ }, () => ({
					external: true,
				}));
				// ./undici-shims/* helpers are resolved by the downstream build
				// relative to the seam's own directory (crates/execution/assets/).
				pluginBuild.onResolve({ filter: /^\.\/undici-shims\// }, () => ({
					external: true,
				}));
			},
		},
	],
});

if (result.errors.length > 0) {
	throw new Error(`Failed to build v8-bridge.source.js: ${result.errors[0].text}`);
}

console.log(`Built ${path.relative(workspaceRoot, bridgeOutput)}`);
