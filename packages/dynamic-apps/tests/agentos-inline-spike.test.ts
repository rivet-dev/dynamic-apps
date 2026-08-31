import { execFile } from "node:child_process";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";
import { AgentOs } from "@rivet-dev/agentos-core";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import { expect, test } from "vitest";

const execFileAsync = promisify(execFile);

test("agentOS supports the inline Dynamic Apps execution contract", async () => {
	const directory = await mkdtemp(
		join(tmpdir(), "dynamic-apps-agentos-spike-"),
	);
	const archive = join(directory, "app.tar");
	const artifact = join(directory, "app.aospkg");
	let vm: AgentOs | undefined;
	try {
		await mkdir(join(directory, "direct"));
		await writeFile(
			join(directory, "direct", "main.mjs"),
			`let count = 0;
export async function dispatch(request) {
  count += 1;
  console.log("stdout:" + request.path);
  console.error("stderr:" + request.path);
  return { status: 200, path: request.path, count };
}
`,
		);
		await writeFile(
			join(directory, "agentos-package.json"),
			JSON.stringify({ name: "dynamic-apps-agentos-spike", version: "1.0.0" }),
		);
		await execFileAsync(
			"tar",
			["-cf", archive, "direct", "agentos-package.json"],
			{ cwd: directory },
		);
		await writeFile(
			artifact,
			packAospkgFromTarBytes(await readFile(archive)).bytes,
		);

		vm = await AgentOs.create({
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

		const stdout: Uint8Array[] = [];
		const stderr: Uint8Array[] = [];
		const activeVm = vm;
		const evaluate = (contextId?: string) =>
			activeVm.javascript.evaluate<{
				status: number;
				path: string;
				count: number;
			}>(
				`await (await import("/app/direct/main.mjs")).dispatch(inputs.request)`,
				{
					...(contextId ? { contextId } : {}),
					inputs: { request: { path: "/spike" } },
					onStdout: (chunk) => stdout.push(chunk),
					onStderr: (chunk) => stderr.push(chunk),
					timeoutMs: 5_000,
				},
			);

		await vm.createContext("retained");
		expect(await evaluate("retained")).toMatchObject({
			outcome: "succeeded",
			value: { status: 200, path: "/spike", count: 1 },
		});
		expect(Buffer.concat(stdout).toString()).toContain("stdout:/spike");
		expect(Buffer.concat(stderr).toString()).toContain("stderr:/spike");

		await vm.contexts.reset("retained");
		expect(await evaluate("retained")).toMatchObject({
			outcome: "succeeded",
			value: { count: 1 },
		});
		await vm.contexts.delete("retained");
		expect(await vm.contexts.list()).toEqual([]);

		expect(
			await vm.javascript.evaluate("await new Promise(() => {})", {
				timeoutMs: 25,
			}),
		).toMatchObject({ outcome: "timed_out" });
		const controller = new AbortController();
		const aborted = vm.javascript.evaluate("await new Promise(() => {})", {
			signal: controller.signal,
			timeoutMs: 5_000,
		});
		controller.abort();
		await expect(aborted).rejects.toMatchObject({ name: "AbortError" });
	} finally {
		await vm?.dispose();
		await rm(directory, { recursive: true, force: true });
	}
}, 30_000);
