import { createHash } from "node:crypto";
import { posix } from "node:path";

const MAX_FILE_PATH_BYTES = 1_024;

export const DIRECT_ENTRYPOINT = "direct-v2/main.mjs";
export const DIRECT_BUNDLE_PATH = "direct/main.mjs";
export const ACTOR_BUNDLE_PATH = "actor/main.mjs";
export const DIRECT_RUNTIME_FORMAT = "dynamic-apps-direct-v2";

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
	hash.update("dynamic-apps-release-v19-mounted-hono-router\0");
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
}): string {
	const entrypoint = `./${normalizeAppPath(input.entrypoint)}`;
	return `const dynamicAppsModuleImportStartedAt = performance.now();
const dynamicAppsPreviousRuntimeMode = process.env.RIVETKIT_RUNTIME_MODE;
delete process.env.RIVETKIT_RUNTIME_MODE;
let application;
try {
  application = await import(${JSON.stringify(entrypoint)});
} finally {
  if (dynamicAppsPreviousRuntimeMode === undefined) delete process.env.RIVETKIT_RUNTIME_MODE;
  else process.env.RIVETKIT_RUNTIME_MODE = dynamicAppsPreviousRuntimeMode;
}
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

/** Initializes RivetKit WASM, then starts the application's own HTTP server. */
export function actorRunnerSource(entrypointInput: string): string {
	const entrypoint = `./${normalizeAppPath(entrypointInput)}`;
	return `import { readFile } from "node:fs/promises";
import initializeRivetKit from "@rivetkit/rivetkit-wasm";

const wasmUrl = new URL(__AGENTOS_RIVETKIT_WASM_PATH__, import.meta.url);
await initializeRivetKit({ module_or_path: await readFile(wasmUrl) });
await import(${JSON.stringify(entrypoint)});
console.log("DYNAMIC_APPS_SERVER_READY:" + (process.env.DYNAMIC_APPS_READY_NONCE ?? ""));
`;
}
