import type { DeployAppInput } from "@rivet-dev/dynamic-apps-core";
import type { DIRECT_ENTRYPOINT } from "@rivet-dev/dynamic-apps-core/internal";

export type { DeployAppInput };

/** Legacy actor wire/storage shape. Not part of the public deploy surface. */
export interface AppScaling {
	minReplicas?: number;
	maxReplicas?: number;
	targetConcurrency?: number;
}

export interface Deployment {
	appId: string;
	release: string;
	/** Credential-free Rivet Engine endpoint for this application. */
	endpoint: string;
	namespace: string;
	pool: string;
	/** Publishable token scoped to this application's namespace, when required. */
	token?: string;
}

export interface AppReleaseInfo {
	release: string;
	artifactHash: string;
	artifactBytes: number;
	createdAt: number;
	regions: string[];
	scaling: Required<AppScaling>;
	status: "building" | "ready" | "failed";
	error?: string;
}

export interface PreparedDeployAppInput {
	appId: string;
	files: Record<string, Uint8Array>;
	createNamespace?: boolean;
}

export interface AppRouteResolution {
	appId: string;
	release: string;
	region: string;
	regions: string[];
	revision: number;
	artifactHash: string;
	artifactBytes: number;
	entrypoint: typeof DIRECT_ENTRYPOINT;
	namespace: string;
	scaling: Required<AppScaling>;
	maxRequestBytes: number;
	maxResponseBytes: number;
	usesRivetKit: boolean;
	serverlessEndpoint?: string;
	runtimePool?: string;
}
