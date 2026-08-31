import { mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { setup } from "rivetkit";
import { createAppsActors } from "./actors.js";

const artifactCacheDirectory = process.env.DYNAMIC_APPS_E2E_ARTIFACT_CACHE;

export const privateAppsRegistry = setup({
	// Artifact chunks are binary deployment traffic and expand during RivetKit
	// action serialization. The default 64 KiB action limit is too small.
	maxIncomingMessageSize: 1024 * 1024,
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
