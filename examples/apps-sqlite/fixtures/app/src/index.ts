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

export default app;
