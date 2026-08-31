import {
	appsRouter,
	deployApp,
	setDynamicAppsLogHandler,
} from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";

setDynamicAppsLogHandler((event) => {
	console.error(
		`[dynamic-apps:${event.source}:${event.stream ?? event.level}] ${event.message}`,
	);
});

const appId = "dynamic-apps-sanity";
const deployment = await deployApp({
	appId,
	files: {
		"package.json": JSON.stringify({
			name: "dynamic-apps-sanity",
			version: "1.0.0",
			private: true,
			type: "module",
			main: "index.js",
			dependencies: {
				"@hono/node-server": "2.1.1",
				hono: "4.13.3",
				rivetkit: "2.3.11",
			},
		}),
		"index.js": `
import { Hono } from "hono";
import { serve } from "@hono/node-server";
import { actor, setup } from "rivetkit";
import { db } from "rivetkit/db";
const counter = actor({
  db: db({ async onMigrate(database) {
    await database.execute("CREATE TABLE IF NOT EXISTS counts (id TEXT PRIMARY KEY, value INTEGER NOT NULL)");
  }}),
  actions: {
    async increment(c) {
      await c.db.execute("INSERT INTO counts (id, value) VALUES ('main', 1) ON CONFLICT(id) DO UPDATE SET value = value + 1");
      const rows = await c.db.execute("SELECT value FROM counts WHERE id = 'main'");
      return Number(rows[0]?.value ?? 0);
    },
  },
});
const registry = setup({ use: { counter } });
const app = new Hono();
app.all("/api/rivet/*", (c) => registry.handler(c.req.raw));
app.all("*", () => Response.json({ ok: true, path: "direct" }));
if (process.env.RIVETKIT_RUNTIME_MODE === "serverless") {
  await new Promise((resolve, reject) => {
    const server = serve(
      { fetch: app.fetch, port: Number(process.env.PORT), hostname: "0.0.0.0" },
      resolve,
    );
    server.once("error", reject);
  });
}
export default app;
`,
	},
});

const direct = await appsRouter.request(`/${appId}/`);
if (!direct.ok || !((await direct.json()) as { ok?: boolean }).ok) {
	throw new Error(`direct sanity failed with HTTP ${direct.status}`);
}

const client = createClient({
	namespace: deployment.namespace,
	poolName: deployment.pool,
}) as unknown as {
	counter: { getOrCreate(key: string[]): { increment(): Promise<number> } };
	dispose(): Promise<void>;
};
const counter = client.counter.getOrCreate(["sanity"]);
const first = await counter.increment();
const second = await counter.increment();
await client.dispose();
if (first !== 1 || second !== 2)
	throw new Error(`SQLite sanity returned ${first}, ${second}`);

console.log(
	JSON.stringify(
		{
			event: "dynamic_apps_sanity_passed",
			deployment,
			direct: true,
			health: true,
			metadata: true,
			sqlite: [first, second],
		},
		null,
		2,
	),
);
