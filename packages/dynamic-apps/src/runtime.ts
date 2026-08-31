import { createHash } from "node:crypto";
import { posix } from "node:path";
import { controlFetch } from "./control-request.js";

const MAX_FILE_PATH_BYTES = 1_024;
const MAX_ENGINE_RESPONSE_BYTES = 1024 * 1024;
const MAX_ENGINE_DATACENTERS = 128;

export const DIRECT_ENTRYPOINT = "direct-v2/main.mjs";
export const DIRECT_BUNDLE_PATH = "direct/main.mjs";
export const ACTOR_BUNDLE_PATH = "actor/main.mjs";
export const DIRECT_RUNTIME_FORMAT = "agentos-apps-direct-v2";
export const APP_CALLBACK_SECRET_HEADER = "x-agentos-app-callback-token";

/** Stable compatibility pool retained in deployApp's result. */
export function appRunnerPool(appId: string): string {
	const suffix = createHash("sha256").update(appId).digest("hex").slice(0, 16);
	return `agentos-apps-${suffix}`;
}

export function normalizeAppPath(input: string): string {
	if (typeof input !== "string" || input.length === 0 || input.includes("\0")) {
		throw new Error(
			"application file paths must be non-empty strings without NUL bytes",
		);
	}
	const normalized = posix.normalize(`/${input}`).slice(1);
	if (
		input.startsWith("/") ||
		input.split("/").includes("..") ||
		normalized === "" ||
		normalized === "." ||
		normalized === ".." ||
		normalized.startsWith("../") ||
		Buffer.byteLength(normalized) > MAX_FILE_PATH_BYTES
	) {
		throw new Error(
			`application file path escapes its root: ${JSON.stringify(input)}`,
		);
	}
	return normalized;
}

export function canonicalDeploymentHash(input: {
	files: Record<string, Uint8Array>;
	entrypoint: string;
	build: boolean;
	packagingIdentity: string;
	deploymentIdentity?: string;
}): string {
	const hash = createHash("sha256");
	hash.update("agentos-apps-release-v17-direct-actors\0");
	const field = (value: string | Uint8Array) => {
		const bytes = typeof value === "string" ? Buffer.from(value) : value;
		const length = Buffer.allocUnsafe(8);
		length.writeBigUInt64BE(BigInt(bytes.byteLength));
		hash.update(length);
		hash.update(bytes);
	};
	for (const [path, content] of Object.entries(input.files).sort(([a], [b]) =>
		a < b ? -1 : a > b ? 1 : 0,
	)) {
		field(normalizeAppPath(path));
		field(content);
	}
	field(normalizeAppPath(input.entrypoint));
	field(JSON.stringify({ build: input.build }));
	field(input.packagingIdentity);
	field(input.deploymentIdentity ?? "");
	return hash.digest("hex");
}

/**
 * Host-controlled wrapper bundled with the application. It exports one direct
 * dispatcher and never opens a socket or starts a guest process of its own.
 */
export function directRunnerSource(input: {
	entrypoint: string;
	release: string;
	maxResponseBytes: number;
	usesRivetKit?: boolean;
}): string {
	const entrypoint = `./${normalizeAppPath(input.entrypoint)}`;
	const importApplication = input.usesRivetKit
		? `const dynamicAppsModuleImportStartedAt = performance.now();
import { Registry } from "rivetkit";
const originalStart = Registry.prototype.start;
Registry.prototype.start = function dynamicAppsManagedStart() {};
let application;
try {
  application = await import(${JSON.stringify(entrypoint)});
} finally {
  Registry.prototype.start = originalStart;
}`
		: `const dynamicAppsModuleImportStartedAt = performance.now();
const application = await import(${JSON.stringify(entrypoint)});`;
	return `${importApplication}
const dynamicAppsModuleImportMs = performance.now() - dynamicAppsModuleImportStartedAt;
const exported = application.default;
const appFetch = typeof exported === "function"
  ? exported
  : typeof exported?.fetch === "function"
    ? exported.fetch.bind(exported)
    : undefined;
if (!appFetch) {
  throw new TypeError(
    "Dynamic App entrypoint default export must be an object with fetch(request)",
  );
}

export const dynamicAppMetadata = Object.freeze({
  format: ${JSON.stringify(DIRECT_RUNTIME_FORMAT)},
  release: ${JSON.stringify(input.release)},
});

export async function dispatch(input) {
  const startedAt = performance.now();
  const body = input.bodyBase64
    ? Buffer.from(input.bodyBase64, "base64")
    : undefined;
  const request = new Request(input.url, {
    method: input.method,
    headers: input.headers,
    body: input.method === "GET" || input.method === "HEAD" ? undefined : body,
  });
  const requestBuiltAt = performance.now();
  const response = await appFetch(request);
  const handlerAt = performance.now();
  if (!(response instanceof Response)) {
    throw new TypeError("Dynamic App fetch handler must return a Response");
  }
  const declaredLength = Number(response.headers.get("content-length") ?? 0);
  if (Number.isFinite(declaredLength) && declaredLength > ${input.maxResponseBytes}) {
    throw new RangeError("Dynamic App response exceeds the configured limit");
  }
  const responseBody = new Uint8Array(await response.arrayBuffer());
  if (responseBody.byteLength > ${input.maxResponseBytes}) {
    throw new RangeError("Dynamic App response exceeds the configured limit");
  }
  const headers = [];
  response.headers.forEach((value, name) => {
    if (name !== "set-cookie") headers.push([name, value]);
  });
  for (const cookie of response.headers.getSetCookie?.() ?? []) {
    headers.push(["set-cookie", cookie]);
  }
  const serializedAt = performance.now();
	return {
    status: response.status,
    statusText: response.statusText,
    headers,
		bodyBase64: Buffer.from(responseBody).toString("base64"),
    timing: {
		moduleImportMs: dynamicAppsModuleImportMs,
      requestBuildMs: requestBuiltAt - startedAt,
      handlerMs: handlerAt - requestBuiltAt,
      responseSerializeMs: serializedAt - handlerAt,
      dispatcherMs: serializedAt - startedAt,
    },
	};
}
`;
}

/** Host-owned wrapper for the optional app-defined RivetKit registry. */
export function actorRunnerSource(entrypointInput: string): string {
	const entrypoint = `./${normalizeAppPath(entrypointInput)}`;
	return `import { Registry } from "rivetkit";
const originalStart = Registry.prototype.start;
Registry.prototype.start = function dynamicAppsManagedStart() {};
let application;
try {
  application = await import(${JSON.stringify(entrypoint)});
} finally {
  Registry.prototype.start = originalStart;
}
if (typeof application.registry?.handler !== "function") {
  throw new TypeError("Dynamic App using RivetKit must export const registry = setup(...)");
}
export const registry = application.registry;
`;
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
				headers: { [APP_CALLBACK_SECRET_HEADER]: input.callbackSecret },
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
}
