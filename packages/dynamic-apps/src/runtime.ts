import { createHash } from "node:crypto";
import { controlFetch } from "./control-request.js";

const MAX_ENGINE_RESPONSE_BYTES = 1024 * 1024;
const MAX_ENGINE_DATACENTERS = 128;

export const APP_CALLBACK_SECRET_HEADER = "x-agentos-app-callback-token";

/** Stable compatibility pool retained in deployApp's result. */
export function appRunnerPool(appId: string): string {
	const suffix = createHash("sha256").update(appId).digest("hex").slice(0, 16);
	return `agentos-apps-${suffix}`;
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

/** Point an app namespace's stable pool at its authenticated callback URL. */
export async function ensureServerlessRunnerConfig(input: {
	endpoint: string;
	namespace: string;
	url: string;
	pool: string;
	token?: string;
	callbackToken?: string;
	callbackSecret: string;
}): Promise<void> {
	const headers = {
		accept: "application/json",
		"content-type": "application/json",
		...(input.token ? { authorization: `Bearer ${input.token}` } : {}),
	};
	const datacentersResponse = await controlFetch(
		engineUrl(input.endpoint, ["datacenters"], input.namespace),
		{ headers },
	);
	const datacentersText = await readBoundedText(datacentersResponse);
	if (!datacentersResponse.ok) {
		throw new Error(
			`Rivet datacenter lookup failed with HTTP ${datacentersResponse.status}: ${datacentersText}`,
		);
	}
	const parsed = JSON.parse(datacentersText) as {
		datacenters?: Array<{ name?: unknown }>;
	};
	if (
		!Array.isArray(parsed.datacenters) ||
		parsed.datacenters.length === 0 ||
		parsed.datacenters.length > MAX_ENGINE_DATACENTERS
	) {
		throw new Error("Rivet datacenter lookup returned an invalid count");
	}
	const datacenters: Record<string, unknown> = {};
	for (const datacenter of parsed.datacenters) {
		if (typeof datacenter.name !== "string" || !datacenter.name) {
			throw new Error("Rivet datacenter lookup returned an invalid name");
		}
		datacenters[datacenter.name] = {
			serverless: {
				url: input.url,
				headers: {
					[APP_CALLBACK_SECRET_HEADER]: input.callbackSecret,
					...(input.callbackToken
						? { "x-rivet-token": input.callbackToken }
						: {}),
				},
				request_lifespan: 60 * 60,
				metadata_poll_interval: 1_000,
				max_runners: 1_024,
				min_runners: 0,
				runners_margin: 0,
				slots_per_runner: 1,
			},
			metadata: {},
			drain_on_version_upgrade: true,
		};
	}
	const response = await controlFetch(
		engineUrl(input.endpoint, ["runner-configs", input.pool], input.namespace),
		{ method: "PUT", headers, body: JSON.stringify({ datacenters }) },
	);
	const body = await readBoundedText(response);
	if (!response.ok) {
		throw new Error(
			`Rivet runner config upsert failed with HTTP ${response.status}: ${body}`,
		);
	}

	// Publish the registry protocol synchronously so the first actor allocation
	// cannot race the background metadata poller into the legacy runner path.
	const refreshResponse = await controlFetch(
		engineUrl(
			input.endpoint,
			["runner-configs", input.pool, "refresh-metadata"],
			input.namespace,
		),
		{ method: "POST", headers, body: "{}" },
	);
	const refreshBody = await readBoundedText(refreshResponse);
	if (!refreshResponse.ok) {
		throw new Error(
			`Rivet runner metadata refresh failed with HTTP ${refreshResponse.status}: ${refreshBody}`,
		);
	}
}
