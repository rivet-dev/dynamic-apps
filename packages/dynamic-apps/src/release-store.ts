import { createHash } from "node:crypto";
import type {
	ActiveRelease,
	PublishReleaseInput,
	ReleaseInvalidation,
	ReleaseLoadContext,
	Unsubscribe,
} from "@rivet-dev/dynamic-apps-core";
import {
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
} from "@rivet-dev/dynamic-apps-core/internal";
import { createClient } from "rivetkit/client";
import { actorWorkerEnvironment } from "./actor-runtime.js";
import { ensurePrivateAppsRegistry } from "./registry.js";
import type { AppRouteResolution, Deployment } from "./types.js";

// Keep the raw payload comfortably below the Engine action-message limit after
// Uint8Array JSON/base64 serialization and protocol framing.
export const ARTIFACT_CHUNK_BYTES = 128 * 1024;
const MAX_ARTIFACT_BYTES = 64 * 1024 * 1024;
const MAX_ARTIFACT_CHUNKS = Math.ceil(
	MAX_ARTIFACT_BYTES / ARTIFACT_CHUNK_BYTES,
);

interface BeginReleasePublishInput {
	appId: string;
	buildId: string;
	format: typeof DIRECT_RUNTIME_FORMAT;
	entrypoint: typeof DIRECT_ENTRYPOINT;
	artifactHash: string;
	artifactBytes: number;
	usesRivetKit: boolean;
	createNamespace?: boolean;
	createdAt: number;
}

interface BeginReleasePublishResult {
	release: string;
	sequence: number;
	uploadRequired: boolean;
	chunkBytes: number;
}

interface WriteReleaseChunkInput {
	release: string;
	sequence: number;
	index: number;
	content: Uint8Array;
}

interface CommitReleasePublishInput {
	release: string;
	sequence: number;
	chunks: number;
}

interface ArtifactManifest {
	format: string;
	hash: string;
	bytes: number;
	chunks: number;
	chunkBytes: number;
}

interface ReleaseActivatedEvent {
	revision: number;
	release: string;
	artifactHash: string;
	activatedAt: number;
}

interface AppConnection {
	ready: Promise<void>;
	on(
		name: "releaseActivated",
		callback: (event: ReleaseActivatedEvent) => void,
	): () => void;
	onOpen(callback: () => void): () => void;
	onClose(callback: () => void): () => void;
	dispose(): Promise<void>;
}

interface AppReleaseHandle {
	beginReleasePublish(
		input: BeginReleasePublishInput,
	): Promise<BeginReleasePublishResult>;
	writeReleaseChunk(input: WriteReleaseChunkInput): Promise<void>;
	commitReleasePublish(
		input: CommitReleasePublishInput,
	): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }>;
	resolveDeployment(): Promise<AppRouteResolution>;
	getArtifactManifest(release: string): Promise<ArtifactManifest>;
	readArtifactChunk(release: string, index: number): Promise<Uint8Array>;
	connect(): AppConnection;
}

interface ReleaseActorGroup {
	get?(key: string | string[]): AppReleaseHandle;
	getOrCreate(key: string | string[]): AppReleaseHandle;
}

interface ReleaseStoreClient {
	dynamicAppsApp: ReleaseActorGroup;
}

interface DriverEntry {
	appId: string;
	handle?: AppReleaseHandle;
	connection?: AppConnection;
	ready: Promise<void>;
	listenerRemovers: Array<() => void>;
	disposed: boolean;
	unsubscribe?: Unsubscribe;
}

export interface RivetDeployOptions {
	createNamespace?: boolean;
}

export interface RivetReleaseStore {
	publishRelease(
		input: PublishReleaseInput,
		options?: RivetDeployOptions,
	): Promise<Deployment>;
	loadActiveRelease(
		appId: string,
		context: ReleaseLoadContext,
	): Promise<ActiveRelease | undefined>;
	watchActiveRelease(
		appId: string,
		invalidate: ReleaseInvalidation,
	): Promise<Unsubscribe>;
}

export function createRivetReleaseStore(
	clientInput?: ReleaseStoreClient,
): RivetReleaseStore {
	let client = clientInput;
	const drivers = new Map<string, DriverEntry>();
	const getClient = () =>
		(client ??= createClient() as unknown as ReleaseStoreClient);

	const publishRelease = async (
		input: PublishReleaseInput,
		options?: RivetDeployOptions,
	): Promise<Deployment> => {
		await ensurePrivateAppsRegistry();
		const group = getClient().dynamicAppsApp;
		const driver = drivers.get(input.appId);
		let handle: AppReleaseHandle;
		let usedExisting = false;
		if (driver) {
			await driver.ready;
			if (!driver.handle) throw new Error("release driver has no actor handle");
			handle = driver.handle;
		} else if (group.get) {
			handle = group.get([input.appId]);
			usedExisting = true;
		} else {
			handle = group.getOrCreate([input.appId]);
		}
		const beginInput: BeginReleasePublishInput = {
			appId: input.appId,
			buildId: input.buildId,
			format: DIRECT_RUNTIME_FORMAT,
			entrypoint: DIRECT_ENTRYPOINT,
			artifactHash: input.artifact.hash,
			artifactBytes: input.artifact.byteLength,
			usesRivetKit: input.artifact.usesRivetKit,
			createNamespace: options?.createNamespace,
			createdAt: input.createdAt,
		};
		let begin: BeginReleasePublishResult;
		try {
			begin = await handle.beginReleasePublish(beginInput);
		} catch (error) {
			if (!usedExisting || !isActorNotFound(error)) throw error;
			handle = group.getOrCreate([input.appId]);
			begin = await handle.beginReleasePublish(beginInput);
		}
		if (
			!/^[a-f0-9]{64}$/.test(begin.release) ||
			!Number.isSafeInteger(begin.sequence) ||
			begin.sequence < 1 ||
			typeof begin.uploadRequired !== "boolean" ||
			begin.chunkBytes !== ARTIFACT_CHUNK_BYTES
		) {
			throw new Error("app actor returned invalid release upload metadata");
		}
		const chunks = Math.ceil(input.artifact.byteLength / begin.chunkBytes);
		if (chunks < 1 || chunks > MAX_ARTIFACT_CHUNKS) {
			throw new Error("application artifact requires an invalid chunk count");
		}
		if (begin.uploadRequired) {
			for (let index = 0; index < chunks; index += 1) {
				await handle.writeReleaseChunk({
					release: begin.release,
					sequence: begin.sequence,
					index,
					content: input.artifact.bytes.slice(
						index * begin.chunkBytes,
						(index + 1) * begin.chunkBytes,
					),
				});
			}
		}
		const result = await handle.commitReleasePublish({
			release: begin.release,
			sequence: begin.sequence,
			chunks,
		});
		return projectDeployment(result);
	};

	const loadActiveRelease = async (
		appId: string,
		context: ReleaseLoadContext,
	): Promise<ActiveRelease | undefined> => {
		const driver = drivers.get(appId);
		if (!driver)
			throw new Error("release load requires an active subscription");
		await timed(context, "actor-connect", () => driver.ready);
		const handle = driver.handle;
		if (!handle) throw new Error("release driver has no actor handle");
		let resolution: AppRouteResolution;
		try {
			resolution = await timed(context, "actor-resolve", () =>
				handle.resolveDeployment(),
			);
		} catch (error) {
			if (getErrorCode(error) === "dynamic_apps_not_deployed") return undefined;
			throw error;
		}
		const manifest = await timed(context, "artifact-manifest", () =>
			handle.getArtifactManifest(resolution.release),
		);
		validateManifest(manifest, resolution);
		const bytes = await timed(context, "artifact-download", async () => {
			const chunks: Uint8Array[] = [];
			const digest = createHash("sha256");
			let total = 0;
			for (let index = 0; index < manifest.chunks; index += 1) {
				const content = new Uint8Array(
					await handle.readArtifactChunk(resolution.release, index),
				);
				const expected =
					index === manifest.chunks - 1
						? manifest.bytes - index * manifest.chunkBytes
						: manifest.chunkBytes;
				if (content.byteLength !== expected) {
					throw new Error(`artifact chunk ${index} has an invalid length`);
				}
				total += content.byteLength;
				digest.update(content);
				chunks.push(content);
			}
			if (total !== manifest.bytes || digest.digest("hex") !== manifest.hash) {
				throw new Error("downloaded artifact failed size or hash verification");
			}
			return new Uint8Array(Buffer.concat(chunks, total));
		});
		return {
			appId: resolution.appId,
			release: resolution.release,
			artifact: {
				format: DIRECT_RUNTIME_FORMAT,
				entrypoint: DIRECT_ENTRYPOINT,
				hash: resolution.artifactHash,
				bytes,
				byteLength: bytes.byteLength,
				usesRivetKit: resolution.usesRivetKit,
			},
			maxRequestBytes: resolution.maxRequestBytes,
			maxResponseBytes: resolution.maxResponseBytes,
			...(resolution.usesRivetKit &&
			resolution.serverlessEndpoint &&
			resolution.runtimePool
				? {
						server: {
							environment: definedEnvironment(
								actorWorkerEnvironment({
									endpoint: resolution.serverlessEndpoint,
									key: `${resolution.release}:${resolution.artifactHash}`,
									namespace: resolution.namespace,
									pool: resolution.runtimePool,
								}),
							),
						},
					}
				: {}),
		};
	};

	const watchActiveRelease = async (
		appId: string,
		invalidate: ReleaseInvalidation,
	): Promise<Unsubscribe> => {
		await ensurePrivateAppsRegistry();
		const existing = drivers.get(appId);
		if (existing) {
			await existing.ready;
			if (!existing.unsubscribe)
				throw new Error("release driver is unavailable");
			return existing.unsubscribe;
		}
		const entry: DriverEntry = {
			appId,
			ready: undefined as unknown as Promise<void>,
			listenerRemovers: [],
			disposed: false,
		};
		drivers.set(appId, entry);
		entry.ready = connectDriver(
			entry,
			getClient().dynamicAppsApp,
			invalidate,
		).catch(async (error) => {
			if (drivers.get(appId) === entry) drivers.delete(appId);
			for (const remove of entry.listenerRemovers.splice(0)) remove();
			await entry.connection?.dispose().catch(() => {});
			entry.connection = undefined;
			entry.handle = undefined;
			throw error;
		});
		await entry.ready;
		let unsubscribePromise: Promise<void> | undefined;
		const unsubscribe = () => {
			unsubscribePromise ??= (async () => {
				entry.disposed = true;
				if (drivers.get(appId) === entry) drivers.delete(appId);
				for (const remove of entry.listenerRemovers.splice(0)) remove();
				await entry.connection?.dispose();
			})();
			return unsubscribePromise;
		};
		entry.unsubscribe = unsubscribe;
		return unsubscribe;
	};

	return { publishRelease, loadActiveRelease, watchActiveRelease };
}

function definedEnvironment(env: NodeJS.ProcessEnv): Record<string, string> {
	return Object.fromEntries(
		Object.entries(env).filter(
			(entry): entry is [string, string] => typeof entry[1] === "string",
		),
	);
}

async function connectDriver(
	entry: DriverEntry,
	group: ReleaseActorGroup,
	invalidate: ReleaseInvalidation,
): Promise<void> {
	const attempt = async (handle: AppReleaseHandle): Promise<void> => {
		const connection = handle.connect();
		entry.handle = handle;
		entry.connection = connection;
		let ready = false;
		entry.listenerRemovers.push(
			connection.on("releaseActivated", () => invalidate()),
			connection.onClose(() => invalidate()),
			connection.onOpen(() => {
				if (ready) invalidate();
			}),
		);
		await connection.ready;
		ready = true;
	};
	if (group.get) {
		try {
			await attempt(group.get([entry.appId]));
			return;
		} catch (error) {
			for (const remove of entry.listenerRemovers.splice(0)) remove();
			await entry.connection?.dispose().catch(() => {});
			entry.connection = undefined;
			entry.handle = undefined;
			if (!isActorNotFound(error)) throw error;
		}
	}
	await attempt(group.getOrCreate([entry.appId]));
}

function validateManifest(
	manifest: ArtifactManifest,
	resolution: AppRouteResolution,
): void {
	if (
		manifest.format !== DIRECT_RUNTIME_FORMAT ||
		manifest.hash !== resolution.artifactHash ||
		manifest.bytes !== resolution.artifactBytes ||
		!/^[a-f0-9]{64}$/.test(manifest.hash) ||
		!Number.isSafeInteger(manifest.bytes) ||
		manifest.bytes < 1 ||
		manifest.bytes > MAX_ARTIFACT_BYTES ||
		!Number.isSafeInteger(manifest.chunks) ||
		manifest.chunks < 1 ||
		manifest.chunks > MAX_ARTIFACT_CHUNKS ||
		manifest.chunkBytes !== ARTIFACT_CHUNK_BYTES ||
		manifest.chunks !== Math.ceil(manifest.bytes / manifest.chunkBytes)
	) {
		throw new Error("app actor returned an invalid artifact manifest");
	}
}

async function timed<T>(
	context: ReleaseLoadContext,
	name: string,
	operation: () => Promise<T>,
): Promise<T> {
	const startedAt = performance.now();
	try {
		return await operation();
	} finally {
		context.recordTiming(name, performance.now() - startedAt);
	}
}

function projectDeployment(input: Deployment): Deployment {
	return {
		appId: input.appId,
		release: input.release,
		endpoint: input.endpoint,
		namespace: input.namespace,
		pool: input.pool,
		...(input.token ? { token: input.token } : {}),
	};
}

function isActorNotFound(error: unknown): boolean {
	const code = getErrorCode(error);
	return (
		code === "actor_not_found" ||
		(code === "not_found" &&
			typeof error === "object" &&
			error !== null &&
			"group" in error &&
			error.group === "actor")
	);
}

function getErrorCode(error: unknown): string | undefined {
	if (typeof error !== "object" || error === null || !("code" in error)) {
		return undefined;
	}
	return typeof error.code === "string" ? error.code : undefined;
}
