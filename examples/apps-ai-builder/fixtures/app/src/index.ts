import { serve } from "@hono/node-server";
import { Hono } from "hono";

const app = new Hono();

app.get("/", (context) =>
	context.json({
		message: "Replace this seed with the generated application.",
	}),
);

serve({
	fetch: app.fetch,
	port: Number(process.env.PORT ?? 3000),
});
