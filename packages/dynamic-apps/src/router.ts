import { Hono } from "hono";
import type { BlankEnv, BlankSchema } from "hono/types";
import { DynamicAppsError } from "./errors.js";
import { ApplicationHandlerError, getDefaultExecutor } from "./executor.js";
import { handlePrivateAppsRegistry } from "./registry.js";
import { validateAppId } from "./source.js";

const PRIVATE_REGISTRY_SENTINEL = "x-agentos-app-registry-dispatch";
const MAX_URL_BYTES = 16 * 1024;
const MAX_METHOD_BYTES = 256;
let requestOverride:
	| ((appId: string, request: Request) => Promise<Response>)
	| undefined;

function errorCode(error: unknown): string | undefined {
	if (error instanceof DynamicAppsError) return error.code;
	if (typeof error !== "object" || error === null || !("code" in error)) {
		return undefined;
	}
	return typeof error.code === "string" ? error.code : undefined;
}

function ordinaryRoutingError(error: unknown): Response | undefined {
	if (error instanceof ApplicationHandlerError) {
		return new Response("Internal Server Error", { status: 500 });
	}
	const code = errorCode(error);
	const message = error instanceof Error ? error.message : "";
	if (code === "agentos_apps_not_deployed") {
		return new Response("Dynamic App has no active release", { status: 503 });
	}
	if (code === "agentos_apps_region_not_deployed") {
		const region = (error as { metadata?: { requestedRegion?: unknown } })
			.metadata?.requestedRegion;
		return new Response(
			`Dynamic App is not deployed in requested region ${typeof region === "string" ? region : "unknown"}`,
			{ status: 421 },
		);
	}
	if (code === "agentos_apps_no_region") {
		return new Response("Dynamic App has no configured region", {
			status: 503,
		});
	}
	if (code === "agentos_apps_request_limit") {
		if (message.includes("URL")) {
			return new Response("Request URL exceeds Dynamic Apps limit", {
				status: 414,
			});
		}
		if (message.includes("method")) {
			return new Response("Request method exceeds Dynamic Apps limit", {
				status: 400,
			});
		}
		if (message.includes("header")) {
			return new Response("Request headers exceed Dynamic Apps limit", {
				status: 431,
			});
		}
		if (message.includes("body")) {
			return new Response("Request body exceeds Dynamic Apps limit", {
				status: 413,
			});
		}
	}
	return undefined;
}

function exceptionResponse(error: unknown): Response {
	const ordinary = ordinaryRoutingError(error);
	if (ordinary) return ordinary;
	const code = errorCode(error);
	const status =
		code === "agentos_apps_invalid_app_id"
			? 400
			: code === "agentos_apps_not_deployed" ||
					code === "agentos_apps_region_not_deployed"
				? 404
				: code === "agentos_apps_request_limit"
					? 413
					: code?.startsWith("agentos_apps_")
						? 503
						: 500;
	return Response.json(
		{
			error: {
				code: code ?? "agentos_apps_internal_error",
				message:
					error instanceof Error
						? error.message
						: "Dynamic Apps request failed",
			},
		},
		{ status },
	);
}

const router: Hono<BlankEnv, BlankSchema, "/"> = new Hono();

const handler = async (context: {
	req: {
		param(name: string): string | undefined;
		path: string;
		routePath: string;
		raw: Request;
	};
}): Promise<Response> => {
	try {
		const appId = context.req.param("appId") ?? "";
		validateAppId(appId);
		const original = context.req.raw;
		if (Buffer.byteLength(original.url) > MAX_URL_BYTES) {
			return new Response("Request URL exceeds Dynamic Apps limit", {
				status: 414,
			});
		}
		if (Buffer.byteLength(original.method) > MAX_METHOD_BYTES) {
			return new Response("Request method exceeds Dynamic Apps limit", {
				status: 400,
			});
		}
		const url = new URL(original.url);
		const parameterOffset = context.req.routePath.indexOf("/:appId");
		const mountPath =
			parameterOffset < 0
				? ""
				: context.req.routePath.slice(0, parameterOffset);
		const applicationPath = `${mountPath}/${appId}`;
		const suffix = context.req.path.startsWith(applicationPath)
			? context.req.path.slice(applicationPath.length)
			: "";
		if (suffix === "") {
			url.pathname = `${url.pathname}/`;
			return Response.redirect(url, 308);
		}
		url.pathname = suffix.startsWith("/") ? suffix : `/${suffix}`;
		const forwarded = new Request(url, original);
		return await (requestOverride
			? requestOverride(appId, forwarded)
			: getDefaultExecutor().request(appId, forwarded));
	} catch (error) {
		return exceptionResponse(error);
	}
};

router.all("/:appId", handler);
router.all("/:appId/*", handler);

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

/** @internal Test and benchmark seam; not exported from the package root. */
export function setRouterRequestOverride(
	override?: (appId: string, request: Request) => Promise<Response>,
): void {
	requestOverride = override;
}
