import { createClient } from "rivetkit/client";
import { DynamicAppsError } from "./errors.js";
import { ensurePrivateAppsRegistry } from "./registry.js";
import { prepareSource } from "./source.js";
import type {
	DeployAppInput,
	Deployment,
	PreparedDeployAppInput,
} from "./types.js";

interface DeploymentHandle {
	deploy(
		input: PreparedDeployAppInput,
	): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }>;
}

interface DeploymentActorGroup {
	/** Resolve an existing stable app actor without constraining its datacenter. */
	get?(key: string | string[]): DeploymentHandle;
	getOrCreate(key: string | string[]): DeploymentHandle;
}

export interface DeployAppOptions {
	/** An ordinary RivetKit client. The default client is created lazily. */
	client?: {
		agentOSAppsApp: DeploymentActorGroup;
	};
}

let defaultClient: NonNullable<DeployAppOptions["client"]> | undefined;
const HOST_REGISTRY_READY_TIMEOUT_MS = 15_000;
const HOST_REGISTRY_RETRY_DELAY_MS = 50;

function getDefaultClient(): NonNullable<DeployAppOptions["client"]> {
	defaultClient ??= createClient() as unknown as NonNullable<
		DeployAppOptions["client"]
	>;
	return defaultClient;
}

export async function deployApp(
	input: DeployAppInput,
	options: DeployAppOptions = {},
): Promise<Deployment> {
	const files = await prepareSource(input);
	if (!options.client) await ensurePrivateAppsRegistry();
	const client = options.client ?? getDefaultClient();
	const prepared: PreparedDeployAppInput = {
		appId: input.appId,
		files,
		regions: input.regions,
		scaling: input.scaling,
	};
	const result = await deployThroughStableActor(
		client.agentOSAppsApp,
		input.appId,
		prepared,
	);
	return {
		appId: input.appId,
		release: result.release,
		endpoint: result.endpoint,
		namespace: result.namespace,
		pool: result.pool,
		...(result.token ? { token: result.token } : {}),
		regions: result.regions,
	};
}

async function deployThroughStableActor(
	group: DeploymentActorGroup,
	appId: string,
	input: PreparedDeployAppInput,
): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }> {
	if (group.get) {
		try {
			return await deployWhenHostRegistryIsReady(group.get([appId]), input);
		} catch (error) {
			if (!isActorNotFound(error)) throw error;
		}
	}
	return deployWhenHostRegistryIsReady(group.getOrCreate([appId]), input);
}

function isActorNotFound(error: unknown): boolean {
	if (typeof error !== "object" || error === null) return false;
	const code = getErrorCode(error);
	return (
		code === "actor_not_found" ||
		(code === "not_found" && "group" in error && error.group === "actor")
	);
}

async function deployWhenHostRegistryIsReady(
	app: DeploymentHandle,
	input: PreparedDeployAppInput,
): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }> {
	const deadline = Date.now() + HOST_REGISTRY_READY_TIMEOUT_MS;
	let lastError: unknown;

	do {
		try {
			return await app.deploy(input);
		} catch (error) {
			if (getErrorCode(error) !== "no_runner_config_configured") throw error;
			lastError = error;
			await new Promise((resolve) =>
				setTimeout(resolve, HOST_REGISTRY_RETRY_DELAY_MS),
			);
		}
	} while (Date.now() < deadline);

	throw new DynamicAppsError(
		"host_registry_not_ready",
		`Dynamic Apps could not reach its private actor registry within ${HOST_REGISTRY_READY_TIMEOUT_MS}ms.`,
		{
			timeoutMs: HOST_REGISTRY_READY_TIMEOUT_MS,
			lastCode: getErrorCode(lastError),
		},
	);
}

function getErrorCode(error: unknown): string | undefined {
	if (typeof error !== "object" || error === null || !("code" in error)) {
		return undefined;
	}
	return typeof error.code === "string" ? error.code : undefined;
}

/** @internal Test-only reset for verifying lazy client creation. */
export function resetDefaultAppsClientForTest(): void {
	defaultClient = undefined;
}
