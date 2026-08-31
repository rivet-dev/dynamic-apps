import type { Hono } from "hono";
import type { BlankEnv, BlankSchema } from "hono/types";
import { defaultDynamicApps } from "./default.js";
import { handlePrivateAppsRegistry } from "./registry.js";

const PRIVATE_REGISTRY_SENTINEL = "x-agentos-app-registry-dispatch";

const router = defaultDynamicApps.appsRouter;
const honoFetch = router.fetch.bind(router);
router.fetch = (async (request: Request, ...rest: unknown[]) => {
	if (request.headers.get(PRIVATE_REGISTRY_SENTINEL) === "1") {
		const headers = new Headers(request.headers);
		headers.delete(PRIVATE_REGISTRY_SENTINEL);
		const cleanRequest = new Request(request, { headers });
		const path = new URL(cleanRequest.url).pathname.replace(/\/+$/, "") || "/";
		if (path === "/api/rivet" || path.startsWith("/api/rivet/")) {
			return handlePrivateAppsRegistry(cleanRequest);
		}
		return new Response("Not Found", { status: 404 });
	}
	return honoFetch(request, ...rest);
}) as typeof router.fetch;

export const appsRouter: Hono<BlankEnv, BlankSchema, "/"> = router;
