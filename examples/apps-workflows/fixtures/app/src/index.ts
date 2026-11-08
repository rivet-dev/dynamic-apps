import { setup, workflow } from "@rivet-dev/workflows";
import { Hono } from "hono";

type Status = "placed" | "paid" | "shipped" | "delivered";

// The workflow runs when the actor is created. Each step is durable, so the
// actor can sleep, scale to zero, and resume exactly where it left off.
const order = workflow({
	state: { status: "placed" as Status },
	actions: {
		status: (c) => c.state.status,
	},
	run: async (wf) => {
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
	},
});

export const registry = setup({ use: { order } });

const app = new Hono();
app.all("/api/rivet/*", (c) => registry.handler(c.req.raw));
app.get("/", (c) =>
	c.json({ message: "Use the RivetKit client to read orders." }),
);

export default app;
