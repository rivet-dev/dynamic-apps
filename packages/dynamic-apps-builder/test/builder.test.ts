import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readdir, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { promisify } from "node:util";
import { createContext, runInContext } from "node:vm";
import { describe, expect, test } from "vitest";
import {
	actorRunnerSource,
	directRunnerSource,
} from "../../dynamic-apps/src/runtime.js";

const execFileAsync = promisify(execFile);
const packageRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const builder = join(packageRoot, "cli", "apps-builder.mjs");

describe("apps-builder", () => {
	test("emits an executable direct-isolate IIFE and rejects Node builtins", async () => {
		const root = await mkdtemp(join(tmpdir(), "agentos-apps-direct-builder-"));
		const workspace = join(root, "workspace");
		const release = join(root, "release");
		await mkdir(workspace, { recursive: true });
		await writeFile(
			join(workspace, "runner.mjs"),
			directRunnerSource({
				entrypoint: "app.ts",
				release: "direct-test",
				maxResponseBytes: 1024 * 1024,
			}),
		);
		await writeFile(
			join(workspace, "app.ts"),
			[
				"let count = 0;",
				"export default {",
				"  fetch(request: Request) {",
				"    count += 1;",
				"    return new Response(request.method + ':' + new URL(request.url).pathname + ':' + count);",
				"  },",
				"};",
			].join("\n"),
		);
		const configPath = join(root, "config.json");
		const config = {
			workspace,
			release,
			entrypoint: "runner.mjs",
			version: "direct-test",
			sourceFiles: ["app.ts"],
			usesRivetKit: false,
			directIsolate: true,
			maxOutputBytes: 1024 * 1024,
			maxOutputFiles: 16,
			maxFileBytes: 1024 * 1024,
		};
		await writeFile(configPath, JSON.stringify(config));

		await execFileAsync(process.execPath, [builder, configPath]);
		const source = await readFile(join(release, "main.mjs"), "utf8");
		expect(source).not.toMatch(/^\s*(?:import|export)\s/m);
		const sandbox = {
			Headers,
			Request,
			Response,
			URL,
			Uint8Array,
			performance,
			__dynamicAppsBase64Decode: (value: string) =>
				new Uint8Array(Buffer.from(value, "base64")),
			__dynamicAppsBase64Encode: (value: Uint8Array) =>
				Buffer.from(value).toString("base64"),
			__dynamicAppDispatch: undefined as
				| ((input: string) => Promise<string>)
				| undefined,
		};
		const context = createContext(sandbox);
		runInContext(source, context);
		if (!sandbox.__dynamicAppDispatch)
			throw new Error("direct bundle did not install its dispatcher");
		const output = JSON.parse(
			await sandbox.__dynamicAppDispatch(
				JSON.stringify({
					url: "https://example.test/nested",
					method: "POST",
					headers: [],
				}),
			),
		);
		expect(Buffer.from(output.bodyBase64, "base64").toString()).toBe(
			"POST:/nested:1",
		);

		await writeFile(
			join(workspace, "app.ts"),
			'import { readFile } from "node:fs/promises"; export default { fetch: () => new Response(String(readFile)) };',
		);
		await expect(
			execFileAsync(process.execPath, [builder, configPath]),
		).rejects.toMatchObject({
			stderr: expect.stringContaining(
				'Node builtin "node:fs/promises" is unsupported',
			),
		});
	});

	test("emits a minimal executable TypeScript release with static assets", async () => {
		const root = await mkdtemp(join(tmpdir(), "agentos-apps-builder-"));
		const workspace = join(root, "workspace");
		const release = join(root, "release");
		await mkdir(join(workspace, "src"), { recursive: true });
		await mkdir(join(workspace, "public"), { recursive: true });
		await writeFile(
			join(workspace, "entry.ts"),
			[
				'import { greeting } from "./src/app.ts";',
				'import query from "#query";',
				'import wasmPath from "./src/module.wasm";',
				"export default { greeting, query, wasmPath };",
			].join("\n"),
		);
		await writeFile(
			join(workspace, "package.json"),
			JSON.stringify({
				type: "module",
				imports: {
					"#query": {
						node: "./src/query.sql",
						default: "./src/missing.sql",
					},
				},
			}),
		);
		await writeFile(
			join(workspace, "src", "app.ts"),
			'export const greeting: string = "hello from Dynamic Apps";\n',
		);
		await writeFile(
			join(workspace, "src", "query.sql"),
			"select 'hello from sqlite';\n",
		);
		await writeFile(
			join(workspace, "src", "module.wasm"),
			new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0]),
		);
		await writeFile(
			join(workspace, "public", "index.html"),
			"<h1>Hello</h1>\n",
		);
		await writeFile(
			join(workspace, "package-lock.json"),
			'{"must":"not ship"}\n',
		);
		const configPath = join(root, "config.json");
		await writeFile(
			configPath,
			JSON.stringify({
				workspace,
				release,
				entrypoint: "entry.ts",
				version: "release-test",
				staticRoot: "public",
				sourceFiles: [
					"src/app.ts",
					"src/query.sql",
					"src/module.wasm",
					"public/index.html",
				],
				usesRivetKit: false,
				maxOutputBytes: 1024 * 1024,
				maxOutputFiles: 32,
				maxFileBytes: 512 * 1024,
			}),
		);

		await execFileAsync(process.execPath, [builder, configPath]);

		const paths = await listFiles(release);
		const wasmPath = paths.find(
			(path) => path.startsWith("modules/module-") && path.endsWith(".wasm"),
		);
		expect(paths).toEqual([
			"agentos-package.json",
			"main.mjs",
			"manifest.json",
			wasmPath,
			"public/index.html",
		]);
		expect(paths).not.toContain("package-lock.json");
		expect(paths.some((path) => path.startsWith("src/"))).toBe(false);
		expect(paths.some((path) => path.startsWith("node_modules/"))).toBe(false);

		const loaded = await import(
			`${pathToFileURL(join(release, "main.mjs")).href}?test=${Date.now()}`
		);
		expect(loaded.default).toEqual({
			greeting: "hello from Dynamic Apps",
			query: "select 'hello from sqlite';\n",
			wasmPath: expect.stringMatching(/^\.\/modules\/module-[A-Z0-9]+\.wasm$/),
		});

		const manifest = JSON.parse(
			await readFile(join(release, "manifest.json"), "utf8"),
		);
		expect(manifest.version).toBe(1);
		expect(manifest.mainModule).toBe("main.mjs");
		expect(manifest.modules).toHaveLength(2);
		expect(manifest.assets).toHaveLength(1);
		const main = await readFile(join(release, "main.mjs"));
		expect(manifest.modules[0]).toMatchObject({
			path: "main.mjs",
			size: main.byteLength,
			hash: createHash("sha256").update(main).digest("hex"),
		});
	});

	test("emits a small platform-linked actor registry bundle", async () => {
		const root = await mkdtemp(join(tmpdir(), "agentos-apps-actor-builder-"));
		const workspace = join(root, "workspace");
		const release = join(root, "release");
		await mkdir(join(workspace, "src"), { recursive: true });
		await writeFile(
			join(workspace, "runner.mjs"),
			actorRunnerSource("src/index.mjs"),
		);
		await writeFile(
			join(workspace, "src", "index.mjs"),
			[
				'import { actor, setup } from "rivetkit";',
				"const counter = actor({ state: { count: 0 } });",
				"export const registry = setup({ use: { counter } });",
				"registry.start();",
			].join("\n"),
		);
		const configPath = join(root, "config.json");
		await writeFile(
			configPath,
			JSON.stringify({
				workspace,
				release,
				entrypoint: "runner.mjs",
				version: "actor-test",
				sourceFiles: ["src/index.mjs"],
				usesRivetKit: true,
				platformRivetKit: true,
				maxOutputBytes: 1024 * 1024,
				maxOutputFiles: 16,
				maxFileBytes: 1024 * 1024,
			}),
		);

		await execFileAsync(process.execPath, [builder, configPath]);
		const paths = await listFiles(release);
		expect(paths).toEqual([
			"agentos-package.json",
			"main.mjs",
			"manifest.json",
		]);
		const source = await readFile(join(release, "main.mjs"), "utf8");
		expect(source).toContain('from"rivetkit"');
		expect(Buffer.byteLength(source)).toBeLessThan(32 * 1024);
	});
});

async function listFiles(root: string): Promise<string[]> {
	const paths: string[] = [];
	const walk = async (directory: string) => {
		for (const entry of await readdir(directory, { withFileTypes: true })) {
			const path = join(directory, entry.name);
			if (entry.isDirectory()) {
				await walk(path);
			} else {
				paths.push(path.slice(root.length + 1).replaceAll("\\", "/"));
			}
		}
	};
	await walk(root);
	return paths.sort();
}
