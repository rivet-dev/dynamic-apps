import { Hono } from "hono";

const app = new Hono();

app.get("/", (context) =>
	context.json({
		message: "Replace this seed with the generated application.",
	}),
);

export default app;
