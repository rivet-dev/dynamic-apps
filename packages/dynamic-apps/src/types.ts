export interface AppScaling {
	minReplicas?: number;
	maxReplicas?: number;
	targetConcurrency?: number;
}

interface DeployAppBase {
	/** Stable URL-safe identifier used for routing and namespace isolation. */
	appId: string;
	/** @deprecated Every application is now isolated in its own namespace. */
	createNamespace?: boolean;
	regions?: string[];
	scaling?: AppScaling;
}

export type DeployAppInput =
	| (DeployAppBase & {
			/** Local application directory. */
			source: URL;
			files?: never;
	  })
	| (DeployAppBase & {
			/** Complete generated application tree. */
			files: Record<string, string | Uint8Array>;
			source?: never;
	  });

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
