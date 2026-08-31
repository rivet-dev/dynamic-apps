import { serve } from "@hono/node-server";
import { Hono } from "hono";
import { actor, setup } from "rivetkit";
import { db } from "rivetkit/db";

// Each actor owns its own SQLite database.
const notes = actor({
	db: db({
		async onMigrate(database) {
			await database.execute(`
				CREATE TABLE IF NOT EXISTS notes (
					id INTEGER PRIMARY KEY AUTOINCREMENT,
					body TEXT NOT NULL
				)
			`);
		},
	}),
	actions: {
		async add(c, body: string) {
			await c.db.execute("INSERT INTO notes (body) VALUES (?)", body);
		},
		async list(c) {
			return c.db.execute("SELECT id, body FROM notes ORDER BY id");
		},
	},
});

export const registry = setup({ use: { notes } });

const app = new Hono();
app.all("/api/rivet/*", (c) => registry.handler(c.req.raw));
app.get("/", (c) =>
	c.json({ message: "Use the RivetKit client to add notes." }),
);

// Dynamic Apps runs the actors in serverless mode and waits for this listener.
if (process.env.RIVETKIT_RUNTIME_MODE === "serverless") {
	await new Promise<void>((resolve, reject) => {
		const server = serve(
			{ fetch: app.fetch, port: Number(process.env.PORT), hostname: "0.0.0.0" },
			() => resolve(),
		);
		server.once("error", reject);
	});
}

export default app;
