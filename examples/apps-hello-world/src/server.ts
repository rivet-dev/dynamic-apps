import { serve } from "@hono/node-server";
import { appsRouter } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();

const dispatchRegistry = (request: Request) => {
	const headers = new Headers(request.headers);
	headers.set("x-agentos-app-registry-dispatch", "1");
	return appsRouter.fetch(new Request(request, { headers }));
};
server.all("/api/rivet", (c) => dispatchRegistry(c.req.raw));
server.all("/api/rivet/*", (c) => dispatchRegistry(c.req.raw));

// Mount every deployed application at /apps/:appId.
server.route("/apps", appsRouter);

// Serve the host router over HTTP.
serve({
	fetch: server.fetch,
	port: 3000,
});
