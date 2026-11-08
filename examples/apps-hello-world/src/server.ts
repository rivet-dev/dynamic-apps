import { serve } from "@hono/node-server";
import { appsRouter, startDynamicAppsHost } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();

// Exposes the host's Rivet health, metadata, and metrics routes.
server.all("/api/rivet/*", (c) => appsRouter.fetch(c.req.raw));

// Mount every deployed application at /apps/:appId.
server.route("/apps", appsRouter);

// Start running deployed apps' actors as soon as the host boots.
await startDynamicAppsHost();

// Serve the host router over HTTP.
serve({
	fetch: server.fetch,
	port: 3000,
});
