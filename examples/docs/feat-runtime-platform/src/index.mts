/**
 * Runtime & Platform - what host environment guest JavaScript sees.
 *
 * The `NodeRuntime` façade boots a fully virtualized VM and runs guest code on
 * the **Node platform**: the `process` and `Buffer` globals, `node:*` builtin
 * modules, and full npm-style module resolution are all present. This is the
 * one platform the façade exposes today; the lower-level esbuild-style
 * `platform` knob (browser | neutral | bare) is not surfaced by `NodeRuntime`.
 *
 * This example proves the Node surface is live by having guest code probe its
 * own globals and builtins, then return a structured report to the host.
 */

import { NodeRuntime } from "secure-exec";

const rt = await NodeRuntime.create();

try {
	// Probe the guest's host environment from inside the kernel-backed isolate.
	const probe = await rt.run<{
		platform: string;
		hasProcess: boolean;
		hasBuffer: boolean;
		nodeVersion: string;
		sha256: string;
		joinedPath: string;
	}>(`
		const { createHash } = await import("node:crypto");
		const { join } = await import("node:path");

		const sha256 = createHash("sha256").update("secure-exec").digest("hex");

		globalThis.__return({
			// process/Buffer are Node-platform globals (absent on browser/neutral/bare).
			platform: typeof process !== "undefined" ? process.platform : "(no process)",
			hasProcess: typeof process !== "undefined",
			hasBuffer: typeof Buffer !== "undefined",
			nodeVersion: typeof process !== "undefined" ? process.versions.node : "(none)",
			// node:* builtin modules resolve and run inside the isolate
			// (dynamic import() keeps this snippet a single expression body).
			sha256,
			joinedPath: join("/home/agentos", "report.txt"),
		});
	`);

	if (probe.exitCode !== 0) {
		throw new Error(`guest probe failed (exit ${probe.exitCode}): ${probe.stderr}`);
	}

	const r = probe.value!;
	console.log("Guest host environment (Node platform):");
	console.log(`  process global present : ${r.hasProcess}`);
	console.log(`  Buffer global present  : ${r.hasBuffer}`);
	console.log(`  process.platform       : ${r.platform}`);
	console.log(`  process.versions.node  : ${r.nodeVersion}`);
	console.log(`  node:crypto sha256     : ${r.sha256}`);
	console.log(`  node:path join         : ${r.joinedPath}`);

	// And a plain exec() run, showing stdout capture with the same Node surface.
	const hello = await rt.exec(
		`console.log("hello from", process.platform, "/ Buffer:", Buffer.from("hi").toString("base64"));`,
	);
	console.log("\nexec() stdout:");
	process.stdout.write(`  ${hello.stdout}`);
	console.log(`exec() exitCode: ${hello.exitCode}`);
} finally {
	await rt.dispose();
}
