import { createHash } from "node:crypto";
import { controlFetch } from "./control-request.js";

const MAX_ENGINE_RESPONSE_BYTES = 1024 * 1024;
const MAX_POOL_NODE_PAGES = 64;

/** Stable node pool that hosts one app's actors. */
export function appRunnerPool(appId: string): string {
	const suffix = createHash("sha256").update(appId).digest("hex").slice(0, 16);
	return `dynamic-apps-${suffix}`;
}

async function readBoundedText(response: Response): Promise<string> {
	if (!response.body) return "";
	const reader = response.body.getReader();
	const chunks: Uint8Array[] = [];
	let bytes = 0;
	try {
		for (;;) {
			const { value, done } = await reader.read();
			if (done) break;
			bytes += value.byteLength;
			if (bytes > MAX_ENGINE_RESPONSE_BYTES) {
				await reader.cancel();
				throw new RangeError("Rivet Engine response exceeded limit");
			}
			chunks.push(value);
		}
	} finally {
		reader.releaseLock();
	}
	return new TextDecoder().decode(Buffer.concat(chunks));
}

function engineUrl(endpoint: string, path: string[], namespace: string): URL {
	const url = new URL(endpoint);
	url.pathname = `/${path.map(encodeURIComponent).join("/")}`;
	url.search = "";
	url.searchParams.set("namespace", namespace);
	return url;
}

interface EngineControl {
	endpoint: string;
	namespace: string;
	token?: string;
}

function controlHeaders(token?: string): Record<string, string> {
	return {
		accept: "application/json",
		"content-type": "application/json",
		...(token ? { authorization: `Bearer ${token}` } : {}),
	};
}

/**
 * Creates or updates the node pool an app's envoy connects to. The pool must
 * exist before the envoy connects, because the Engine rejects connections to
 * unknown pools. Newer node versions drain older ones so a missed graceful
 * shutdown still moves actors to the active release.
 */
export async function ensureEnvoyPool(
	input: EngineControl & { pool: string },
): Promise<void> {
	const response = await controlFetch(
		engineUrl(input.endpoint, ["pools", input.pool], input.namespace),
		{
			method: "PUT",
			headers: controlHeaders(input.token),
			body: JSON.stringify({ drain_on_version_upgrade: true }),
		},
	);
	const body = await readBoundedText(response);
	if (!response.ok) {
		throw new Error(
			`Rivet pool upsert failed with HTTP ${response.status}: ${body}`,
		);
	}
}

/** Whether a connected, non-draining node of `version` serves `pool`. */
export async function poolHasConnectedNode(
	input: EngineControl & { pool: string; version: number },
): Promise<boolean> {
	let cursor: string | undefined;
	for (let page = 0; page < MAX_POOL_NODE_PAGES; page += 1) {
		const url = engineUrl(
			input.endpoint,
			["pools", input.pool, "nodes"],
			input.namespace,
		);
		if (cursor) url.searchParams.set("cursor", cursor);
		const response = await controlFetch(url, {
			headers: controlHeaders(input.token),
		});
		const body = await readBoundedText(response);
		if (!response.ok) {
			throw new Error(
				`Rivet pool node listing failed with HTTP ${response.status}: ${body}`,
			);
		}
		const parsed = JSON.parse(body) as {
			nodes?: Array<{
				version?: unknown;
				connected?: unknown;
				stopping?: unknown;
			}>;
			pagination?: { cursor?: string | null };
		};
		const found = parsed.nodes?.some(
			(node) =>
				node.version === input.version &&
				node.connected === true &&
				node.stopping !== true,
		);
		if (found) return true;
		cursor = parsed.pagination?.cursor ?? undefined;
		if (!cursor) return false;
	}
	return false;
}
