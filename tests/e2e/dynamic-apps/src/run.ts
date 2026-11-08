import { spawn } from "node:child_process";
import { randomBytes } from "node:crypto";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { getEnginePath } from "@rivetkit/engine-cli";
import getPort from "get-port";

const fast = process.argv.includes("--fast");
const buildOnly = process.argv.includes("--build-only");
const sanity = process.argv.includes("--sanity");
const failover = process.argv.includes("--failover");
const root = await mkdtemp(join(tmpdir(), "dynamic-apps-e2e-"));
const databasePath = join(root, "db");
await mkdir(databasePath, { recursive: true });
const configuredGuardPort = Number(process.env.DYNAMIC_APPS_E2E_ENGINE_PORT);
const guardPort =
	Number.isInteger(configuredGuardPort) && configuredGuardPort > 0
		? configuredGuardPort
		: await getPort();
const peerPort = await getPort({ exclude: [guardPort] });
const metricsPort = await getPort({ exclude: [guardPort, peerPort] });
const endpoint = `http://127.0.0.1:${guardPort}`;
const peerEndpoint = `http://127.0.0.1:${peerPort}`;
const configPath = join(root, "engine.json");
// Rivet Engine requires an admin token to start.
const adminToken = randomBytes(32).toString("hex");
const logDirectory = join(root, "log");
await mkdir(logDirectory, { recursive: true });
await writeFile(
	configPath,
	JSON.stringify({
		topology: {
			datacenter_label: 1,
			datacenters: {
				default: {
					datacenter_label: 1,
					is_leader: true,
					public_url: endpoint,
					peer_url: peerEndpoint,
				},
			},
		},
	}),
);

// Mirrors the single-worker local engine that RivetKit starts for development.
const engine = spawn(getEnginePath(), ["--config", configPath], {
	stdio: ["ignore", "inherit", "inherit"],
	env: {
		...process.env,
		RIVET__AUTH__ADMIN_TOKEN: adminToken,
		RIVET__GUARD__HOST: "127.0.0.1",
		RIVET__GUARD__PORT: String(guardPort),
		RIVET__API_PEER__HOST: "127.0.0.1",
		RIVET__API_PEER__PORT: String(peerPort),
		RIVET__METRICS__HOST: "127.0.0.1",
		RIVET__METRICS__PORT: String(metricsPort),
		RIVET__STORAGE__LOG_DIRECTORY: logDirectory,
		RIVET__OBJECT_STORAGE__PRIMARY__FILE_SYSTEM__PATH: databasePath,
		RIVET__STORAGE__BALANCING__ENABLED: "false",
		RIVET__STORAGE__AUTO_PROVISION_PARTITIONS: "1",
		RIVET__STORAGE__LOG_SHARD__MAX_DISK_BYTES: String(4 * 1024 ** 3),
		RIVET__STORAGE__COLD_STORE__DISK_CACHE_BYTES: "0",
		RIVET__STORAGE__WRITE_TO_COLD_AFTER_MS: "5000",
		RIVET__STORAGE__STOP_TIMEOUT_MS: "6000",
		RIVET__STORAGE__STOP_TEARDOWN_MS: "2000",
		RIVET__PEGBOARD__NODE_LOST_THRESHOLD: "7000",
		RIVET__RUNTIME__WORKER_SHUTDOWN_DURATION: "1",
		RIVET__RUNTIME__GUARD_SHUTDOWN_DURATION: "1",
		RIVET__RUNTIME__FORCE_SHUTDOWN_DURATION: "10",
	},
});

try {
	await waitUntilHealthy(endpoint, engine);
	process.env.RIVET_ENDPOINT = endpoint;
	process.env.RIVET_TOKEN = adminToken;
	// App envoys authenticate with the credentials in the public endpoint. The
	// local Engine has only an admin token, so the test hands that to guests.
	process.env.RIVET_PUBLIC_ENDPOINT = `http://default:${adminToken}@127.0.0.1:${guardPort}`;
	process.env.DYNAMIC_APPS_E2E_FAST = fast ? "1" : "0";
	process.env.DYNAMIC_APPS_E2E_BUILD_ONLY = buildOnly ? "1" : "0";
	if (fast) {
		process.env.DYNAMIC_APPS_E2E_ARTIFACT_CACHE = join(
			tmpdir(),
			"dynamic-apps-artifact-cache-v1",
		);
	} else {
		delete process.env.DYNAMIC_APPS_E2E_ARTIFACT_CACHE;
	}
	delete process.env.RIVET_ENGINE;
	delete process.env.RIVET_RUN_ENGINE;
	await import(
		sanity ? "./sanity.js" : failover ? "./failover.js" : "./verify.js"
	);
} finally {
	await stopEngine(engine);
	await rm(root, { recursive: true, force: true });
}
process.exit(0);

async function stopEngine(process: ReturnType<typeof spawn>): Promise<void> {
	if (process.exitCode !== null) return;
	process.kill("SIGTERM");
	const stopped = await Promise.race([
		new Promise<true>((resolve) => process.once("exit", () => resolve(true))),
		new Promise<false>((resolve) =>
			setTimeout(() => resolve(false), fast ? 1_000 : 10_000),
		),
	]);
	if (stopped) return;
	console.warn("Rivet Engine exceeded the E2E shutdown limit; sending SIGKILL");
	process.kill("SIGKILL");
	await new Promise<void>((resolve) => process.once("exit", () => resolve()));
}

async function waitUntilHealthy(
	endpoint: string,
	process: ReturnType<typeof spawn>,
): Promise<void> {
	const deadline = Date.now() + 60_000;
	let lastError: unknown;
	while (Date.now() < deadline) {
		if (process.exitCode !== null) {
			throw new Error(`Rivet Engine exited with code ${process.exitCode}`);
		}
		try {
			// Health passes before storage routing is ready, so also wait for an
			// authenticated read that goes through storage.
			const response = await fetch(`${endpoint}/namespaces?limit=1`, {
				headers: { authorization: `Bearer ${adminToken}` },
			});
			if (response.ok) return;
			lastError = new Error(`health returned ${response.status}`);
		} catch (error) {
			lastError = error;
		}
		await new Promise((resolve) => setTimeout(resolve, 100));
	}
	throw new Error("Rivet Engine did not become healthy", { cause: lastError });
}
