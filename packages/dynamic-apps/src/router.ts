import { Hono } from "hono";
import type { BlankEnv, BlankSchema } from "hono/types";
import { defaultDynamicApps } from "./default.js";
import { handlePrivateAppsRegistry } from "./registry.js";

const router = new Hono<BlankEnv, BlankSchema, "/">();

router.all("/api/rivet/*", (context, next) => {
	const path = new URL(context.req.raw.url).pathname;
	if (!path.startsWith("/api/rivet/")) return next();
	return handlePrivateAppsRegistry(context.req.raw);
});
router.route("/", defaultDynamicApps.appsRouter);

export const appsRouter: Hono<BlankEnv, BlankSchema, "/"> = router;
