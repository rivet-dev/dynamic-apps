import { serve } from "@hono/node-server";
import { appsRouter, deployApp } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();
server.all("/api/rivet/*", (c) => appsRouter.fetch(c.req.raw));
server.route("/apps", appsRouter);

// docs:start deploy
// Deploy the site directory. Its package.json and src/index.ts serve public/.
await deployApp({
	appId: "static-website",
	source: new URL("../fixtures/app/", import.meta.url),
});
// docs:end deploy

serve({ fetch: server.fetch, port: Number(process.env.PORT ?? 3000) });
console.log("Open http://localhost:3000/apps/static-website/");
