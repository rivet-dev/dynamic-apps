import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readdir, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { promisify } from "node:util";
import { AgentOs } from "@rivet-dev/agentos-core";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { describe, expect, test } from "vitest";
import {
	actorRunnerSource,
	directRunnerSource,
} from "../../dynamic-apps/src/runtime.js";

const execFileAsync = promisify(execFile);
const packageRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const builder = join(packageRoot, "cli", "apps-builder.mjs");

describe("apps-builder", () => {
	test("emits an executable Node ESM dispatcher with Node builtins", async () => {
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
				'import { basename } from "node:path";',
				"let count = 0;",
				"export default {",
				"  fetch(request: Request) {",
				"    count += 1;",
				"    return new Response(basename(new URL(request.url).pathname) + ':' + request.method + ':' + count);",
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
			directAgentOs: true,
			maxOutputBytes: 1024 * 1024,
			maxOutputFiles: 16,
			maxFileBytes: 1024 * 1024,
		};
		await writeFile(configPath, JSON.stringify(config));

		await execFileAsync(process.execPath, [builder, configPath]);
		const source = await readFile(join(release, "main.mjs"), "utf8");
		expect(source).toMatch(/\bexport\s*\{/);
		const output = await dispatchInAgentOs(release, {
			url: "https://example.test/nested",
			method: "POST",
			headers: [],
		});
		expect(Buffer.from(output.bodyBase64, "base64").toString()).toBe(
			"nested:POST:1",
		);
		expect(source).not.toContain("__dynamicAppsBase64Decode");
	});

	test("bundles real RivetKit and invokes it inside agentOS", async () => {
		const root = await mkdtemp(join(tmpdir(), "agentos-apps-rivetkit-"));
		const workspace = join(root, "workspace");
		const release = join(root, "release");
		await mkdir(workspace, { recursive: true });
		await writeFile(
			join(workspace, "runner.mjs"),
			directRunnerSource({
				entrypoint: "app.ts",
				release: "rivetkit-direct-test",
				maxResponseBytes: 1024 * 1024,
			}),
		);
		await writeFile(
			join(workspace, "app.ts"),
			[
				'import { actor, setup } from "rivetkit";',
				"const counter = actor({ state: { count: 0 } });",
				"const registry = setup({ use: { counter } });",
				"export default {",
				"  fetch(request) {",
				'    if (new URL(request.url).pathname.startsWith("/api/rivet")) return registry.handler(request);',
				'    return new Response(typeof registry.handler + ":" + typeof counter);',
				"  },",
				"};",
			].join("\n"),
		);
		const configPath = join(root, "config.json");
		await writeFile(
			configPath,
			JSON.stringify({
				workspace,
				release,
				entrypoint: "runner.mjs",
				version: "rivetkit-direct-test",
				sourceFiles: ["app.ts"],
				usesRivetKit: true,
				directAgentOs: true,
				maxOutputBytes: 32 * 1024 * 1024,
				maxOutputFiles: 64,
				maxFileBytes: 16 * 1024 * 1024,
			}),
		);

		await execFileAsync(process.execPath, [builder, configPath]);
		const paths = await listFiles(release);
		expect(
			paths.some(
				(path) =>
					path.startsWith("modules/rivetkit-") && path.endsWith(".wasm"),
			),
		).toBe(true);
		const source = await readFile(join(release, "main.mjs"), "utf8");
		expect(source).not.toContain(
			"RivetKit actor callbacks use the actor runtime",
		);
		const output = await dispatchInAgentOs(release, {
			url: "https://example.test/",
			method: "GET",
			headers: [],
		});
		expect(Buffer.from(output.bodyBase64, "base64").toString()).toBe(
			"function:object",
		);
	}, 30_000);

	test("rejects native Node addons in direct agentOS bundles", async () => {
		const root = await mkdtemp(join(tmpdir(), "agentos-apps-native-addon-"));
		const workspace = join(root, "workspace");
		const release = join(root, "release");
		await mkdir(workspace, { recursive: true });
		await writeFile(join(workspace, "addon.node"), new Uint8Array([1, 2, 3]));
		await writeFile(
			join(workspace, "entry.mjs"),
			'import addon from "./addon.node"; export default addon;',
		);
		const configPath = join(root, "config.json");
		await writeFile(
			configPath,
			JSON.stringify({
				workspace,
				release,
				entrypoint: "entry.mjs",
				version: "native-test",
				sourceFiles: ["entry.mjs", "addon.node"],
				usesRivetKit: false,
				directAgentOs: true,
				maxOutputBytes: 1024 * 1024,
				maxOutputFiles: 16,
				maxFileBytes: 1024 * 1024,
			}),
		);
		await expect(
			execFileAsync(process.execPath, [builder, configPath]),
		).rejects.toMatchObject({
			stderr: expect.stringContaining("native Node addon is unsupported"),
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

	test("emits a small platform-linked actor fetch bundle", async () => {
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
				"const registry = setup({ use: { counter } });",
				"export default { fetch: (request) => registry.handler(request) };",
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

async function dispatchInAgentOs(
	release: string,
	request: {
		url: string;
		method: string;
		headers: Array<[string, string]>;
		bodyBase64?: string;
	},
): Promise<{
	status: number;
	statusText: string;
	headers: Array<[string, string]>;
	bodyBase64: string;
}> {
	const archive = `${release}.tar`;
	const artifact = `${release}.aospkg`;
	await execFileAsync("tar", ["-cf", archive, "-C", release, "."]);
	await writeFile(
		artifact,
		packAospkgFromTarBytes(await readFile(archive)).bytes,
	);
	const vm = await AgentOs.create({
		defaultSoftware: false,
		mounts: [
			{
				path: "/app",
				readOnly: true,
				plugin: {
					id: "agentos_packages",
					config: {
						kind: "tar",
						tarPath: artifact,
						root: "/",
						readOnly: true,
					},
				},
			},
		],
		permissions: {
			fs: "allow",
			childProcess: "allow",
			process: "allow",
			env: "allow",
			network: "allow",
		},
	});
	try {
		const result = await vm.javascript.evaluate<{
			status: number;
			statusText: string;
			headers: Array<[string, string]>;
			bodyBase64: string;
		}>('await (await import("/app/main.mjs")).dispatch(inputs.request)', {
			inputs: { request },
			timeoutMs: 10_000,
		});
		if (result.outcome !== "succeeded" || result.value === undefined) {
			throw new Error(`agentOS dispatcher failed: ${JSON.stringify(result)}`);
		}
		return result.value;
	} finally {
		await vm.dispose();
	}
}
