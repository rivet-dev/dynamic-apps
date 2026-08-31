import type { AppScaling, DeployAppInput } from "@rivet-dev/dynamic-apps-core";
import type { DIRECT_ENTRYPOINT } from "@rivet-dev/dynamic-apps-core/internal";

export type { AppScaling, DeployAppInput };

export interface Deployment {
	appId: string;
	release: string;
	/** Credential-free Rivet Engine endpoint for this application. */
	endpoint: string;
	namespace: string;
	pool: string;
	/** Publishable token scoped to this application's namespace, when required. */
	token?: string;
	regions: string[];
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
	regions?: string[];
	scaling?: AppScaling;
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
}
