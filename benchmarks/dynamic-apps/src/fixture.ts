import { deployApp } from "@rivet-dev/dynamic-apps";

export const BENCHMARK_APP_ID = "dynamic-apps-cold-start-benchmark-v2";
export const ACTOR_BENCHMARK_APP_ID = "dynamic-apps-actor-runtime-benchmark-v2";

export type BenchmarkDeploymentClient = NonNullable<
	NonNullable<Parameters<typeof deployApp>[1]>["client"]
>;

export async function deployBenchmarkFixture(
	client?: BenchmarkDeploymentClient,
) {
	const region = process.env.BENCH_REGION;
	return deployApp(
		{
			appId: BENCHMARK_APP_ID,
			files: {
				"package.json": JSON.stringify({
					name: "dynamic-apps-cold-start-fixture",
					version: "0.0.0",
					private: true,
					type: "module",
					main: "index.js",
				}),
				"index.js": `
import { basename } from "node:path";

export default {
  fetch(request) {
    const url = new URL(request.url);
		if (url.searchParams.get("logs") === "1") {
			console.log("benchmark stdout");
			console.error("benchmark stderr");
		}
    return Response.json({
      ok: true,
      workload: "basic-request-response",
      requestId: url.searchParams.get("requestId"),
			node: { platform: process.platform, file: basename(import.meta.filename) },
    });
  },
};
`,
			},
			scaling: {
				minReplicas: 1,
				maxReplicas: 1,
				targetConcurrency: 128,
			},
			...(region ? { regions: [region] } : {}),
		},
		client ? { client } : undefined,
	);
}

export async function deployActorBenchmarkFixture(
	client?: BenchmarkDeploymentClient,
) {
	const region = process.env.BENCH_REGION;
	return deployApp(
		{
			appId: ACTOR_BENCHMARK_APP_ID,
			files: {
				"package.json": JSON.stringify({
					name: "dynamic-apps-actor-fixture",
					version: "0.0.0",
					private: true,
					type: "module",
					main: "index.js",
					dependencies: {
						rivetkit: "2.3.11",
					},
				}),
				"index.js": `
import { actor, event, setup } from "rivetkit";
import { db } from "rivetkit/db";

const counter = actor({
	db: db({
		async onMigrate(database) {
			await database.execute(
				"CREATE TABLE IF NOT EXISTS benchmark_counter (id INTEGER PRIMARY KEY CHECK (id = 1), value INTEGER NOT NULL)",
			);
		},
	}),
	events: { changed: event() },
	actions: {
		async add(c, amount = 1) {
			await c.db.execute(
				"INSERT INTO benchmark_counter (id, value) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET value = value + excluded.value",
				amount,
			);
			const rows = await c.db.execute(
				"SELECT value FROM benchmark_counter WHERE id = 1",
			);
			const value = Number(rows[0]?.value ?? 0);
			c.broadcast("changed", value);
			return value;
		},
		async inspect(c) {
			const rows = await c.db.execute(
				"SELECT value FROM benchmark_counter WHERE id = 1",
			);
			return Number(rows[0]?.value ?? 0);
		},
	},
});

export const registry = setup({ use: { counter } });
registry.start();

export default function fetch() {
  return Response.json({ ok: true, workload: "actor-and-direct-http" });
}
`,
			},
			scaling: {
				minReplicas: 0,
				maxReplicas: 16,
				targetConcurrency: 8,
			},
			...(region ? { regions: [region] } : {}),
		},
		client ? { client } : undefined,
	);
}
