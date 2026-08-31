import type { Hono } from "hono";
import type { BlankEnv, BlankSchema } from "hono/types";

export interface AppScaling {
	minReplicas?: number;
	maxReplicas?: number;
	targetConcurrency?: number;
}

interface DeployAppBase {
	appId: string;
	/** @deprecated Retained by the Rivet adapter for source compatibility. */
	createNamespace?: boolean;
	regions?: string[];
	scaling?: AppScaling;
}

export type DeployAppInput =
	| (DeployAppBase & { source: URL; files?: never })
	| (DeployAppBase & {
			files: Record<string, string | Uint8Array>;
			source?: never;
	  });

export interface ReleaseArtifact {
	format: "agentos-apps-direct-v2";
	entrypoint: "direct-v2/main.mjs";
	hash: string;
	bytes: Uint8Array;
	byteLength: number;
	usesRivetKit: boolean;
}

export interface PublishReleaseInput {
	appId: string;
	buildId: string;
	artifact: ReleaseArtifact;
	regions?: string[];
	scaling?: AppScaling;
	createdAt: number;
}

export interface ActiveRelease {
	appId: string;
	release: string;
	artifact: ReleaseArtifact;
	regions: string[];
	scaling: Required<AppScaling>;
	maxRequestBytes: number;
	maxResponseBytes: number;
}

export type ReleaseInvalidation = () => void;
export type Unsubscribe = () => void | Promise<void>;

export interface ReleaseLoadContext {
	/** Adds a store-specific sub-phase to request timing diagnostics. */
	recordTiming(name: string, durationMs: number): void;
}

export type ExecutionMode = "ephemeral" | "pooled";

export interface ExecutorConfig {
	executionMode: ExecutionMode;
	contextPoolSize: number;
	contextPoolMaxTotal: number;
	contextIdleTtlMs: number;
	contextHeapLimitMb: number;
	runtimeCacheMaxEntries: number;
	runtimeCacheMaxBytes: number;
	runtimeCacheIdleTtlMs: number;
	memoryHighWaterPercent: number;
	executionConcurrency: number;
	executionQueueSize: number;
	executionQueueWaitMs: number;
	executionTimeoutMs: number;
	timingHeaders: boolean;
	logRequests: boolean;
}

export interface BuildConfig {
	maxSourceBytes: number;
	maxFiles: number;
	maxDependencies: number;
	buildTimeoutMs: number;
	maxResponseBytes: number;
	maxBuildOutputBytes: number;
	maxBuildArtifactBytes: number;
	maxBuildArtifactFiles: number;
	maxBuildArtifactFileBytes: number;
	maxBuildFilesystemBytes: number;
}

export interface BuildArtifactCache {
	get(buildId: string): Promise<Uint8Array | undefined>;
	put(buildId: string, artifact: Uint8Array): Promise<void>;
}

export interface BuiltAppRelease {
	buildId: string;
	artifact: ReleaseArtifact;
}

export interface DynamicAppsLogger {
	info(event: Record<string, unknown>): void;
	error(event: Record<string, unknown>): void;
}

export interface DynamicAppsOptions<TDeployment, TDeployOptions = undefined> {
	publishRelease(
		input: PublishReleaseInput,
		options: TDeployOptions | undefined,
	): Promise<TDeployment>;
	loadActiveRelease(
		appId: string,
		context: ReleaseLoadContext,
	): Promise<ActiveRelease | undefined>;
	watchActiveRelease(
		appId: string,
		invalidate: ReleaseInvalidation,
	): Promise<Unsubscribe>;
	executor?: Partial<ExecutorConfig>;
	build?: Partial<BuildConfig>;
	artifactCache?: BuildArtifactCache;
	logger?: DynamicAppsLogger;
}

export interface DynamicApps<TDeployment, TDeployOptions = undefined> {
	deployApp(
		input: DeployAppInput,
		options?: TDeployOptions,
	): Promise<TDeployment>;
	appsRouter: Hono<BlankEnv, BlankSchema, "/">;
	diagnostics(): Record<string, unknown>;
	dispose(): Promise<void>;
}

export interface BuildAppReleaseInput {
	appId: string;
	files: Record<string, Uint8Array>;
}
