import { createClient } from "rivetkit/client";
import { ENVOY_INDEX_KEY, ENVOY_INDEX_MAX_PAGE } from "./actors.js";
import { emitDynamicAppsLog } from "./logging.js";

// Bounds how long a RivetKit app stays offline after its host process dies
// without a graceful shutdown.
const RECONCILE_INTERVAL_MS = 60_000;
const RECONCILE_CONCURRENCY = 16;
// One unreachable app must not hold up every other app's recovery.
const ENSURE_TIMEOUT_MS = 30_000;

interface ReconcilerClient {
	dynamicAppsEnvoyIndex: {
		getOrCreate(key: string[]): {
			list(
				after: string | null,
				limit: number,
			): Promise<{ appIds: string[]; next: string | null }>;
		};
	};
	dynamicAppsApp: {
		getOrCreate(key: string[]): { ensureEnvoy(): Promise<void> };
	};
}

let timer: ReturnType<typeof setInterval> | undefined;
let running = false;

/**
 * Periodically wakes every app whose active release runs an envoy.
 *
 * This poll is a deliberate last resort. When a host process dies without a
 * graceful shutdown, the Engine only notices that the app actors it ran are
 * gone once something touches them, and nothing else touches an app that
 * serves only actor traffic. Every host runs this loop, so recovery does not
 * depend on any single host surviving. Each pass costs one call per app.
 */
export function startEnvoyReconciler(): void {
	if (timer) return;
	const client = createClient() as unknown as ReconcilerClient;
	const tick = () => void reconcile(client);
	timer = setInterval(tick, RECONCILE_INTERVAL_MS);
	timer.unref?.();
	tick();
}

/** @internal */
export function stopEnvoyReconcilerForTest(): void {
	if (timer) clearInterval(timer);
	timer = undefined;
}

async function reconcile(client: ReconcilerClient): Promise<void> {
	if (running) return;
	running = true;
	try {
		const index = client.dynamicAppsEnvoyIndex.getOrCreate(ENVOY_INDEX_KEY);
		let after: string | null = null;
		let total = 0;
		let failed = 0;
		do {
			const page = await index.list(after, ENVOY_INDEX_MAX_PAGE);
			total += page.appIds.length;
			for (
				let start = 0;
				start < page.appIds.length;
				start += RECONCILE_CONCURRENCY
			) {
				const batch = page.appIds.slice(start, start + RECONCILE_CONCURRENCY);
				const results = await Promise.allSettled(
					batch.map((appId) =>
						withTimeout(
							client.dynamicAppsApp.getOrCreate([appId]).ensureEnvoy(),
							ENSURE_TIMEOUT_MS,
						),
					),
				);
				failed += results.filter(
					(result) => result.status === "rejected",
				).length;
			}
			after = page.next;
		} while (after !== null);
		if (failed > 0) {
			emitDynamicAppsLog({
				level: "error",
				source: "runtime",
				message: `Dynamic Apps envoy reconciliation could not reach ${failed} of ${total} apps; retrying next pass`,
			});
		}
	} catch (error) {
		emitDynamicAppsLog({
			level: "error",
			source: "runtime",
			message: `Dynamic Apps envoy reconciliation failed: ${error instanceof Error ? error.message : String(error)}`,
		});
	} finally {
		running = false;
	}
}

async function withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
	let timer: ReturnType<typeof setTimeout> | undefined;
	try {
		return await Promise.race([
			promise,
			new Promise<never>((_, reject) => {
				timer = setTimeout(
					() => reject(new Error(`timed out after ${ms}ms`)),
					ms,
				);
			}),
		]);
	} finally {
		if (timer) clearTimeout(timer);
	}
}
