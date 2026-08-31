import { serve } from "@hono/node-server";
import { appsRouter, deployApp } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();
server.all("/api/rivet/*", (c) => appsRouter.fetch(c.req.raw));

// An agent or upload endpoint would pass generated files here. This example
// deploys its checked-in fixture and returns the deployment to the caller.
server.post("/deploy/:name", async (c) =>
	c.json(
		await deployApp({
			appId: c.req.param("name"),
			source: new URL("../fixtures/app/", import.meta.url),
		}),
	),
);

server.route("/apps", appsRouter);

serve({ fetch: server.fetch, port: Number(process.env.PORT ?? 3000) });
