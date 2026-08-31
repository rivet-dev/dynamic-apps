import { serve } from "@hono/node-server";
import { Hono } from "hono";
import { actor, setup } from "rivetkit";
import { workflow } from "rivetkit/workflow";

type Status = "placed" | "paid" | "shipped" | "delivered";

// The workflow runs when the actor is created. Each step is durable, so the
// actor can sleep, scale to zero, and resume exactly where it left off.
const order = actor({
	state: { status: "placed" as Status },
	actions: {
		status: (c) => c.state.status,
	},
	run: workflow(async (wf) => {
		await wf.step("charge", async (c) => {
			c.state.status = "paid";
		});
		await wf.step("ship", async (c) => {
			c.state.status = "shipped";
		});
		await wf.sleep("in transit", 2_000);
		await wf.step("deliver", async (c) => {
			c.state.status = "delivered";
		});
	}),
});

export const registry = setup({ use: { order } });

const app = new Hono();
app.all("/api/rivet/*", (c) => registry.handler(c.req.raw));
app.get("/", (c) =>
	c.json({ message: "Use the RivetKit client to read orders." }),
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
