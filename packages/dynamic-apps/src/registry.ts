import { setup } from "rivetkit";
import { createAppsActors } from "./actors.js";

export const privateAppsRegistry = setup({
	use: createAppsActors() as unknown as Record<
		string,
		ReturnType<typeof createAppsActors>["agentOSAppsApp"]
	>,
});

let startPromise: Promise<void> | undefined;

export function isServerlessRuntime(): boolean {
	return process.env.RIVETKIT_RUNTIME_MODE === "serverless";
}

/** Ensure the private app-state actor is registered with a local/envoy Engine. */
export async function ensurePrivateAppsRegistry(): Promise<void> {
	if (isServerlessRuntime()) return;
	startPromise ??= privateAppsRegistry.startAndWait().catch((error) => {
		startPromise = undefined;
		throw error;
	});
	await startPromise;
}

/** Handle the host's explicit root /api/rivet callback routes. */
export async function handlePrivateAppsRegistry(
	request: Request,
): Promise<Response> {
	if (isServerlessRuntime()) return privateAppsRegistry.handler(request);
	await ensurePrivateAppsRegistry();
	const path = new URL(request.url).pathname.replace(/\/+$/, "") || "/";
	if (request.method === "GET" && path === "/api/rivet/health") {
		return privateAppsRegistry.routes.health();
	}
	if (request.method === "GET" && path === "/api/rivet/metadata") {
		return privateAppsRegistry.routes.metadata();
	}
	if (request.method === "GET" && path === "/api/rivet/metrics") {
		return privateAppsRegistry.routes.prometheusMetrics(request);
	}
	return new Response("Not Found", { status: 404 });
}

/** @internal */
export async function shutdownPrivateAppsRegistryForTest(): Promise<void> {
	await privateAppsRegistry.shutdown();
	startPromise = undefined;
}
