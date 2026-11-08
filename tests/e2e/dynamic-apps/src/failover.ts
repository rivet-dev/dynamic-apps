import { spawn } from "node:child_process";
import {
	setDynamicAppsLogHandler,
	startDynamicAppsHost,
} from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";

// Host A runs in its own process group so killing it also kills its agentOS
// sidecar and guest VM, as losing a machine would.
const hostA = spawn(
	process.execPath,
	["--import", "tsx", "src/failover-host.ts"],
	{
		cwd: process.cwd(),
		env: process.env,
		detached: true,
		stdio: ["ignore", "pipe", "inherit"],
	},
);
const ready = await new Promise<{
	deployment: { namespace: string; pool: string };
	count: number;
}>((resolve, reject) => {
	let output = "";
	hostA.stdout.on("data", (chunk: Buffer) => {
		output += chunk.toString();
		const match = output.match(/FAILOVER_READY (.+)\n/);
		if (match?.[1]) resolve(JSON.parse(match[1]));
	});
	hostA.once("exit", (code) => reject(new Error(`host A exited with ${code}`)));
});
assert(ready.count === 7, "host A did not write the actor state");
if (hostA.pid === undefined) throw new Error("host A has no pid");
process.kill(-hostA.pid, "SIGKILL");
const killedAt = performance.now();

setDynamicAppsLogHandler((event) => {
	if (event.level === "error") console.error(JSON.stringify(event));
});

// Host B only starts its registry. Its recovery loop must wake the app actor
// that died with host A, which restarts the app's envoy here.
await startDynamicAppsHost();

const client = createClient({
	namespace: ready.deployment.namespace,
	poolName: ready.deployment.pool,
}) as unknown as {
	counter: { getOrCreate(key: string[]): { inspect(): Promise<number> } };
	dispose(): Promise<void>;
};
const deadline = Date.now() + 150_000;
let recovered: number | undefined;
let last: unknown;
let attempts = 0;
while (Date.now() < deadline) {
	attempts += 1;
	try {
		recovered = await Promise.race([
			client.counter.getOrCreate(["failover"]).inspect(),
			new Promise<never>((_, reject) =>
				setTimeout(() => reject(new Error("inspect timed out")), 15_000),
			),
		]);
		if (recovered === 7) break;
	} catch (error) {
		last = error;
	}
	// Recovery is driven by the Engine's lost threshold and the recovery loop.
	await new Promise((resolve) => setTimeout(resolve, 1_000));
}
await client.dispose();
assert(
	recovered === 7,
	`actor did not recover after host A died after ${attempts} attempts; last: ${String(last)}`,
);
console.log(
	JSON.stringify({
		event: "dynamic_apps_failover_passed",
		recoveredCount: recovered,
		recoveryMs: Math.round(performance.now() - killedAt),
	}),
);

function assert(condition: unknown, message: string): asserts condition {
	if (!condition) throw new Error(message);
}
