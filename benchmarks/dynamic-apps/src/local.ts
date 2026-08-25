import { type ChildProcess, spawn } from "node:child_process";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { serve } from "@hono/node-server";
import { getEnginePath } from "@rivetkit/engine-cli";
import getPort from "get-port";
import { createBenchmarkApplication } from "./edge.js";
import { deployBenchmarkFixture } from "./fixture.js";
import { runBenchmarkSuite } from "./suite.js";

const root = await mkdtemp(join(tmpdir(), "dynamic-apps-cold-benchmark-"));
const databasePath = join(root, "db");
await mkdir(databasePath, { recursive: true });
const guardPort = await getPort();
const peerPort = await getPort({ exclude: [guardPort] });
const metricsPort = await getPort({ exclude: [guardPort, peerPort] });
const endpoint = `http://127.0.0.1:${guardPort}`;
const edgePort = await getPort({ exclude: [guardPort, peerPort, metricsPort] });
const edgeHost = argument("--host") ?? "127.0.0.1";
const configPath = join(root, "engine.json");
await writeFile(
	configPath,
	JSON.stringify({
		file_system: { path: databasePath },
		guard: { host: "127.0.0.1", port: guardPort },
		api_peer: { host: "127.0.0.1", port: peerPort },
		metrics: { host: "127.0.0.1", port: metricsPort },
		topology: {
			datacenter_label: 1,
			datacenters: {
				default: {
					datacenter_label: 1,
					is_leader: true,
					public_url: endpoint,
					peer_url: `http://127.0.0.1:${peerPort}`,
					proxy_url: null,
				},
			},
		},
		telemetry: { enabled: false },
		runtime: { allow_version_rollback: true },
	}),
);

const enginePath = process.env.BENCH_ENGINE_PATH ?? getEnginePath();
const engine = spawn(enginePath, ["--config", configPath, "start"], {
	stdio: ["ignore", "ignore", "inherit"],
});

try {
	await waitUntilHealthy(endpoint, engine);
	process.env.RIVET_ENDPOINT = endpoint;
	delete process.env.RIVET_ENGINE;
	delete process.env.RIVET_RUN_ENGINE;
	const { registry } = await import("./registry.js");
	registry.start();
	const deployment = await deployBenchmarkFixture();
	const application = createBenchmarkApplication();
	const edge = serve({
		fetch: application.fetch,
		port: edgePort,
		hostname: edgeHost,
	});
	try {
		const suite = await runBenchmarkSuite(`http://127.0.0.1:${edgePort}`);
		console.log(
			JSON.stringify(
				{ runtime: "local-rivet-engine", deployment, ...suite },
				null,
				2,
			),
		);
	} finally {
		await new Promise<void>((resolve, reject) =>
			edge.close((error) => (error ? reject(error) : resolve())),
		);
	}
} finally {
	await stopEngine(engine);
	await rm(root, { recursive: true, force: true });
}
process.exit(0);

function argument(name: string): string | undefined {
	const index = process.argv.indexOf(name);
	return index >= 0 ? process.argv[index + 1] : undefined;
}

async function waitUntilHealthy(
	endpoint: string,
	process: ChildProcess,
): Promise<void> {
	const deadline = Date.now() + 30_000;
	while (Date.now() < deadline) {
		if (process.exitCode !== null) {
			throw new Error(`Rivet Engine exited with code ${process.exitCode}`);
		}
		try {
			if ((await fetch(`${endpoint}/health`)).ok) return;
		} catch {}
		await new Promise((resolve) => setTimeout(resolve, 100));
	}
	throw new Error("Rivet Engine did not become healthy");
}

async function stopEngine(process: ChildProcess): Promise<void> {
	if (process.exitCode !== null) return;
	process.kill("SIGTERM");
	const stopped = await Promise.race([
		new Promise<true>((resolve) => process.once("exit", () => resolve(true))),
		new Promise<false>((resolve) => setTimeout(() => resolve(false), 10_000)),
	]);
	if (stopped) return;
	process.kill("SIGKILL");
	await new Promise<void>((resolve) => process.once("exit", () => resolve()));
}
