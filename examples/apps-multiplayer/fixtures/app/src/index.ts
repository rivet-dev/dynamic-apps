import { serve } from "@hono/node-server";
import { Hono } from "hono";
import { actor, event, setup } from "rivetkit";

type Position = { x: number; y: number };

// One actor per room. State is shared by every connected client.
const room = actor({
	state: { players: {} as Record<string, Position> },
	events: { changed: event() },
	actions: {
		join(c, player: string) {
			c.state.players[player] ??= { x: 0, y: 0 };
			c.broadcast("changed", c.state.players);
			return c.state.players;
		},
		move(c, player: string, x: number, y: number) {
			c.state.players[player] = { x, y };
			c.broadcast("changed", c.state.players);
			return c.state.players;
		},
	},
});

export const registry = setup({ use: { room } });

const app = new Hono();
app.all("/api/rivet/*", (c) => registry.handler(c.req.raw));
app.get("/", (c) =>
	c.json({ message: "Use the RivetKit client to join a room." }),
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
