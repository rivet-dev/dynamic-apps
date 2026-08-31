import { serve } from "@hono/node-server";
import { appsRouter } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();

// This is how the Rivet control plane communicates with your backend.
server.all("/api/rivet/*", (c) => appsRouter.fetch(c.req.raw));

// Mount every deployed application at /apps/:appId.
server.route("/apps", appsRouter);

// Serve the host router over HTTP.
serve({
	fetch: server.fetch,
	port: 3000,
});
