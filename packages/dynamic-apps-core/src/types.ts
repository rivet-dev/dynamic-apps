import type { AgentOsOptions } from "@rivet-dev/agentos-core";
import type { Hono } from "hono";
import type { BlankEnv, BlankSchema } from "hono/types";

interface DeployAppBase {
	appId: string;
	/**
	 * Whether the storage adapter provisions an isolated namespace for this
	 * app. Defaults to true, or to the app's stored setting from an earlier
	 * explicit deploy. Set false to skip provisioning; apps deployed without a
	 * namespace cannot use app-defined actors.
	 */
	createNamespace?: boolean;
}

export type DeployAppInput =
	| (DeployAppBase & { source: URL; files?: never })
	| (DeployAppBase & {
			files: Record<string, string | Uint8Array>;
			source?: never;
	  });

export interface ReleaseArtifact {
	format: "dynamic-apps-direct-v2";
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
	createdAt: number;
}

export interface ActiveRelease {
	appId: string;
	release: string;
	artifact: ReleaseArtifact;
	maxRequestBytes: number;
	maxResponseBytes: number;
	/** Required runtime environment for releases that use RivetKit. */
	server?: {
		environment: Record<string, string>;
	};
}

export interface ApplicationServerRuntimeRequest {
	key: string;
	appId: string;
	release: string;
	loadArtifact(): Promise<Uint8Array>;
	environment: Record<string, string>;
	request: Request;
	maxRequestBytes?: number;
	maxResponseBytes?: number;
}

export interface ApplicationServerRuntime {
	request(input: ApplicationServerRuntimeRequest): Promise<Response>;
	diagnostics?(): Record<string, unknown>;
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
	/** Additional options passed to every application-serving VM. */
	vm?: AgentOsOptions;
	/** Shared HTTP runtime used by releases that contain RivetKit actors. */
	serverRuntime?: ApplicationServerRuntime;
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
