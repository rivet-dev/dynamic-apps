import { mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { setup } from "rivetkit";
import { createAppsActors } from "./actors.js";
import {
	startEnvoyReconciler,
	stopEnvoyReconcilerForTest,
} from "./envoy-reconciler.js";

const artifactCacheDirectory = process.env.DYNAMIC_APPS_E2E_ARTIFACT_CACHE;

export const privateAppsRegistry = setup({
	// Artifact chunks are binary deployment traffic and expand during RivetKit
	// action serialization. The default 64 KiB action limit is too small.
	maxIncomingMessageSize: 1024 * 1024,
	// The app-state actor does not use the optional Services sidecar.
	startServices: false,
	use: createAppsActors({
		artifactCache: artifactCacheDirectory
			? {
					async get(release) {
						try {
							return new Uint8Array(
								await readFile(
									join(artifactCacheDirectory, `${release}.aospkg`),
								),
							);
						} catch (error) {
							if ((error as NodeJS.ErrnoException).code === "ENOENT")
								return undefined;
							throw error;
						}
					},
					async put(release, artifact) {
						await mkdir(artifactCacheDirectory, { recursive: true });
						const target = join(artifactCacheDirectory, `${release}.aospkg`);
						const temporary = `${target}.${process.pid}.tmp`;
						await writeFile(temporary, artifact);
						await rename(temporary, target);
					},
				}
			: undefined,
	}) as unknown as Record<
		string,
		ReturnType<typeof createAppsActors>["dynamicAppsApp"]
	>,
});

let startPromise: Promise<void> | undefined;

/**
 * Registers the private app-state actors with the Engine and starts the loop
 * that restarts app envoys after a host process dies.
 */
export async function ensurePrivateAppsRegistry(): Promise<void> {
	startPromise ??= privateAppsRegistry.startAndWait().then(
		() => startEnvoyReconciler(),
		(error) => {
			startPromise = undefined;
			throw error;
		},
	);
	await startPromise;
}

/**
 * Starts this host's part of Dynamic Apps: the private actors that run each
 * app's envoy and the loop that restarts envoys after a host dies. Call it once
 * when the host process boots. Without it a host joins only after its first
 * request, so apps that serve only actor traffic can stay offline after every
 * host restarts.
 */
export async function startDynamicAppsHost(): Promise<void> {
	await ensurePrivateAppsRegistry();
}

/** Handle the host's explicit root /api/rivet routes. */
export async function handlePrivateAppsRegistry(
	request: Request,
): Promise<Response> {
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
	stopEnvoyReconcilerForTest();
	await privateAppsRegistry.shutdown();
	startPromise = undefined;
}
