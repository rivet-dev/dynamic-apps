import { createHash } from "node:crypto";
import { DynamicAppsError } from "@rivet-dev/dynamic-apps-core/internal";
import { controlFetch } from "./control-request.js";
import { appRunnerPool, ensureServerlessRunnerConfig } from "./runtime.js";

const DEFAULT_ENDPOINT = "http://localhost:6420";
const DEFAULT_CLOUD_ENDPOINT = "https://cloud-api.rivet.dev";
const MAX_CONTROL_RESPONSE_BYTES = 1024 * 1024;

export interface ResolvedRivetConnection {
	endpoint: string;
	namespace: string;
	token?: string;
}

export interface ProvisionedAppNamespace {
	endpoint: string;
	namespace: string;
	pool: string;
	cloudNamespace?: string;
	controlToken?: string;
	runnerToken?: string;
	publicToken?: string;
}

async function readBoundedJson(response: Response): Promise<unknown> {
	if (!response.body) return null;
	const reader = response.body.getReader();
	const chunks: Uint8Array[] = [];
	let bytes = 0;
	try {
		for (;;) {
			const { value, done } = await reader.read();
			if (done) break;
			bytes += value.byteLength;
			if (bytes > MAX_CONTROL_RESPONSE_BYTES) {
				await reader.cancel("Dynamic Apps control response limit exceeded");
				throw new DynamicAppsError(
					"dynamic_apps_control_response_limit",
					`Rivet control response exceeded ${MAX_CONTROL_RESPONSE_BYTES} bytes`,
					{ limit: MAX_CONTROL_RESPONSE_BYTES },
				);
			}
			chunks.push(value);
		}
	} finally {
		reader.releaseLock();
	}
	const text = new TextDecoder().decode(Buffer.concat(chunks));
	return text ? JSON.parse(text) : null;
}

/**
 * Resolves the same standard Rivet connection variables as createClient(), but
 * only when a deployment needs control-plane information. This is not called
 * at module import time.
 */
export function resolveDefaultRivetConnection(): ResolvedRivetConnection {
	const rawEndpoint =
		process.env.RIVET_ENGINE ?? process.env.RIVET_ENDPOINT ?? DEFAULT_ENDPOINT;
	const url = new URL(rawEndpoint);
	const endpointNamespace = url.username
		? decodeURIComponent(url.username)
		: undefined;
	const endpointToken = url.password
		? decodeURIComponent(url.password)
		: undefined;
	url.username = "";
	url.password = "";
	return {
		endpoint: url.toString().replace(/\/$/, ""),
		namespace: endpointNamespace ?? process.env.RIVET_NAMESPACE ?? "default",
		token:
			process.env.DYNAMIC_APPS_CONTROL_TOKEN ??
			endpointToken ??
			process.env.RIVET_TOKEN,
	};
}

function namespaceName(appId: string, hostNamespace: string): string {
	const suffix = createHash("sha256")
		.update(hostNamespace)
		.update("\0")
		.update(appId)
		.digest("hex")
		.slice(0, 10);
	return `${`agentos-app-${appId}`.slice(0, 63 - suffix.length - 1)}-${suffix}`;
}

function controlHeaders(token?: string): Record<string, string> {
	return {
		accept: "application/json",
		"content-type": "application/json",
		...(token ? { authorization: `Bearer ${token}` } : {}),
	};
}

async function checkedJson<T>(
	url: URL,
	init: RequestInit,
	code: string,
	operation: string,
): Promise<T> {
	const response = await controlFetch(url, init);
	if (!response.ok) {
		throw new DynamicAppsError(
			code,
			`${operation} failed with HTTP ${response.status}`,
			{ status: response.status },
		);
	}
	return (await readBoundedJson(response)) as T;
}

function cloudUrl(path: string, organization?: string): URL {
	const url = new URL(
		path,
		process.env.RIVET_CLOUD_ENDPOINT ?? DEFAULT_CLOUD_ENDPOINT,
	);
	if (organization) url.searchParams.set("org", organization);
	return url;
}

function appNamespaceDisplayName(appId: string, hostNamespace: string): string {
	const suffix = createHash("sha256")
		.update(hostNamespace)
		.update("\0")
		.update(appId)
		.digest("hex")
		.slice(0, 12);
	return `Dynamic App ${appId} ${suffix}`.slice(0, 128);
}

async function provisionCloudNamespace(
	appId: string,
	connection: ResolvedRivetConnection,
	existingCloudNamespace?: string,
): Promise<ProvisionedAppNamespace> {
	const cloudToken = process.env.RIVET_CLOUD_TOKEN;
	if (!cloudToken) {
		throw new DynamicAppsError(
			"dynamic_apps_cloud_token_required",
			"RIVET_CLOUD_TOKEN is required",
		);
	}
	const headers = controlHeaders(cloudToken);
	const identity = await checkedJson<{
		project: string;
		organization: string;
	}>(
		cloudUrl("/tokens/api/inspect"),
		{ headers },
		"dynamic_apps_cloud_token_invalid",
		"Rivet Cloud token inspection",
	);
	const displayName = appNamespaceDisplayName(appId, connection.namespace);
	let cloudNamespace = existingCloudNamespace;
	if (!cloudNamespace) {
		let cursor: string | undefined;
		do {
			const url = cloudUrl(
				`/projects/${encodeURIComponent(identity.project)}/namespaces`,
				identity.organization,
			);
			url.searchParams.set("limit", "100");
			if (cursor) url.searchParams.set("cursor", cursor);
			const listed = await checkedJson<{
				namespaces: Array<{ name: string; displayName: string }>;
				pagination?: { cursor?: string };
			}>(
				url,
				{ headers },
				"dynamic_apps_namespace_lookup_failed",
				"Rivet Cloud namespace lookup",
			);
			cloudNamespace = listed.namespaces.find(
				(candidate) => candidate.displayName === displayName,
			)?.name;
			cursor = cloudNamespace ? undefined : listed.pagination?.cursor;
		} while (cursor);
	}
	if (!cloudNamespace) {
		const created = await checkedJson<{
			namespace: {
				name: string;
				access: { engineNamespaceName: string };
			};
		}>(
			cloudUrl(
				`/projects/${encodeURIComponent(identity.project)}/namespaces`,
				identity.organization,
			),
			{
				method: "POST",
				headers,
				body: JSON.stringify({ displayName }),
			},
			"dynamic_apps_namespace_create_failed",
			"Rivet Cloud namespace creation",
		);
		cloudNamespace = created.namespace.name;
	}
	const namespacePath = `/projects/${encodeURIComponent(identity.project)}/namespaces/${encodeURIComponent(cloudNamespace)}`;
	const [{ namespace }, access, secret, publishable] = await Promise.all([
		checkedJson<{
			namespace: { access: { engineNamespaceName: string } };
		}>(
			cloudUrl(namespacePath, identity.organization),
			{ headers },
			"dynamic_apps_namespace_lookup_failed",
			"Rivet Cloud namespace resolution",
		),
		checkedJson<{ token: string }>(
			cloudUrl(`${namespacePath}/tokens/access`, identity.organization),
			{ method: "POST", headers },
			"dynamic_apps_namespace_token_failed",
			"Rivet Cloud access token creation",
		),
		checkedJson<{ token: string }>(
			cloudUrl(`${namespacePath}/tokens/secret`, identity.organization),
			{ method: "POST", headers },
			"dynamic_apps_namespace_token_failed",
			"Rivet Cloud runner token creation",
		),
		checkedJson<{ token: string }>(
			cloudUrl(`${namespacePath}/tokens/publishable`, identity.organization),
			{ method: "POST", headers },
			"dynamic_apps_namespace_token_failed",
			"Rivet Cloud publishable token creation",
		),
	]);
	return {
		endpoint: connection.endpoint,
		namespace: namespace.access.engineNamespaceName,
		pool: appRunnerPool(appId),
		cloudNamespace,
		controlToken: access.token,
		runnerToken: secret.token,
		publicToken: publishable.token,
	};
}

/** Idempotently provisions the isolated application namespace. */
export async function provisionAppNamespace(
	appId: string,
	connection = resolveDefaultRivetConnection(),
	existing?: { namespace?: string | null; cloudNamespace?: string | null },
): Promise<ProvisionedAppNamespace> {
	if (process.env.RIVET_CLOUD_TOKEN) {
		return provisionCloudNamespace(
			appId,
			connection,
			existing?.cloudNamespace ?? undefined,
		);
	}
	const namespace = namespaceName(appId, connection.namespace);
	const headers = controlHeaders(connection.token);
	const lookupUrl = new URL("/namespaces", connection.endpoint);
	lookupUrl.searchParams.set("name", namespace);
	lookupUrl.searchParams.set("limit", "1");
	const lookup = async (): Promise<boolean> => {
		const response = await controlFetch(lookupUrl, {
			headers,
		});
		if (!response.ok) {
			throw new DynamicAppsError(
				"dynamic_apps_namespace_lookup_failed",
				`Rivet namespace lookup failed with HTTP ${response.status}`,
				{ status: response.status },
			);
		}
		const body = (await readBoundedJson(response)) as {
			namespaces?: Array<{ name?: unknown }>;
		};
		return body.namespaces?.some((entry) => entry.name === namespace) ?? false;
	};

	if (!(await lookup())) {
		const response = await controlFetch(
			new URL("/namespaces", connection.endpoint),
			{
				method: "POST",
				headers,
				body: JSON.stringify({
					name: namespace,
					display_name: `Dynamic App ${appId}`,
				}),
			},
		);
		if (!response.ok && !(await lookup())) {
			throw new DynamicAppsError(
				"dynamic_apps_namespace_create_failed",
				`Rivet namespace creation failed with HTTP ${response.status}`,
				{ status: response.status },
			);
		}
	}

	return {
		namespace,
		endpoint: connection.endpoint,
		pool: appRunnerPool(appId),
		controlToken: connection.token,
	};
}

/** Runtime coordinates for an app deployed with createNamespace: false. */
export function unprovisionedAppNamespace(
	appId: string,
	connection = resolveDefaultRivetConnection(),
): ProvisionedAppNamespace {
	return {
		namespace: connection.namespace,
		endpoint: connection.endpoint,
		pool: appRunnerPool(appId),
		controlToken: connection.token,
	};
}

function serverlessAppCallback(
	appActorId: string,
	connection: ResolvedRivetConnection,
): { url: string; token?: string } {
	const configured = process.env.DYNAMIC_APPS_CALLBACK_URL;
	if (configured) {
		return { url: new URL("/api/rivet", configured).toString() };
	}

	const publicEndpoint = process.env.RIVET_PUBLIC_ENDPOINT;
	const callbackConnection = publicEndpoint
		? resolveRivetConnection(publicEndpoint)
		: connection;
	return {
		url: new URL(
			`/gateway/${encodeURIComponent(appActorId)}/request/.agentos/apps/rivet`,
			callbackConnection.endpoint,
		).toString(),
		...(callbackConnection.token ? { token: callbackConnection.token } : {}),
	};
}

function resolveRivetConnection(rawEndpoint: string): ResolvedRivetConnection {
	const url = new URL(rawEndpoint);
	const namespace = url.username
		? decodeURIComponent(url.username)
		: (process.env.RIVET_NAMESPACE ?? "default");
	const token = url.password ? decodeURIComponent(url.password) : undefined;
	url.username = "";
	url.password = "";
	return {
		endpoint: url.toString().replace(/\/$/, ""),
		namespace,
		...(token ? { token } : {}),
	};
}

/** Configure the nested actor pool only after its release is ready. */
export async function configureAppNamespaceRunner(
	appActorId: string,
	runtime: {
		endpoint: string;
		namespace: string;
		pool: string;
		controlToken?: string;
	},
	callbackSecret: string,
	callbackConnection = resolveDefaultRivetConnection(),
): Promise<void> {
	try {
		const callback = serverlessAppCallback(appActorId, callbackConnection);
		await ensureServerlessRunnerConfig({
			endpoint: runtime.endpoint,
			namespace: runtime.namespace,
			url: callback.url,
			pool: runtime.pool,
			token: runtime.controlToken,
			callbackToken: callback.token,
			callbackSecret,
		});
	} catch (error) {
		throw new DynamicAppsError(
			"dynamic_apps_runner_config_failed",
			`Rivet runner configuration failed for namespace ${runtime.namespace} and pool ${runtime.pool}`,
			{
				namespace: runtime.namespace,
				pool: runtime.pool,
				error: error instanceof Error ? error.message : String(error),
			},
		);
	}
}
