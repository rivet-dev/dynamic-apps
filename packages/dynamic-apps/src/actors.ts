import { createHash, randomUUID, timingSafeEqual } from "node:crypto";
import type { BuildArtifactCache } from "@rivet-dev/dynamic-apps-core";
import {
	buildAppRelease,
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
	DynamicAppsError,
} from "@rivet-dev/dynamic-apps-core/internal";
import { type AnyActorDefinition, actor, UserError } from "rivetkit";
import { db, type RawAccess } from "rivetkit/db";
import { getDefaultActorRuntime } from "./actor-runtime.js";
import {
	configureAppNamespaceRunner,
	provisionAppNamespace,
	resolveDefaultRivetConnection,
	unprovisionedAppNamespace,
} from "./control-plane.js";
import { APP_CALLBACK_SECRET_HEADER } from "./runtime.js";
import type {
	AppReleaseInfo,
	AppRouteResolution,
	AppScaling,
	Deployment,
	PreparedDeployAppInput,
} from "./types.js";

const DEFAULT_MAX_VERSIONS = 20;
const DEFAULT_MAX_REGIONS = 8;
const DEFAULT_MAX_REQUEST_BYTES = 1024 * 1024;
const DEFAULT_MAX_RESPONSE_BYTES = 4 * 1024 * 1024;
const MAX_REPLICAS = 128;
// Keep the raw payload comfortably below the Engine action-message limit after
// Uint8Array JSON/base64 serialization and protocol framing.
export const ARTIFACT_CHUNK_BYTES = 128 * 1024;
const MAX_ARTIFACT_BYTES = 64 * 1024 * 1024;
const MAX_ARTIFACT_CHUNKS = Math.ceil(
	MAX_ARTIFACT_BYTES / ARTIFACT_CHUNK_BYTES,
);
const SOURCE_CHUNK_BYTES = 512 * 1024;
const MAX_SOURCE_CHUNKS =
	Math.ceil((4 * 1024 * 1024) / SOURCE_CHUNK_BYTES) + 2_000;
const RELEASE_PATTERN = /^[a-f0-9]{64}$/;

type AnyActorContext = {
	actorId: string;
	key: string[];
	region: string;
	state: unknown;
	db: RawAccess;
	keepAwake<T>(promise: Promise<T>): Promise<T>;
	broadcast(name: string, ...args: unknown[]): void;
	log: {
		info(value: unknown): void;
		error(value: unknown): void;
	};
};

export interface StoredAppRelease extends AppReleaseInfo {
	entrypoint: string;
	namespace: string;
	runtimeEndpoint: string;
	runtimePool: string;
	callbackSecret: string;
	usesRivetKit: boolean;
}

export interface AppState {
	activeRelease: string | null;
	activatingRelease?: string | null;
	namespace: string | null;
	revision: number;
	cloudNamespace?: string | null;
	runnerToken?: string | null;
	publicToken?: string | null;
	publishSequence?: number;
	latestPublishSequence?: number;
}

export interface DynamicAppsActors {
	dynamicAppsApp: AnyActorDefinition;
}

interface BeginReleasePublishInput {
	appId: string;
	buildId: string;
	format: typeof DIRECT_RUNTIME_FORMAT;
	entrypoint: typeof DIRECT_ENTRYPOINT;
	artifactHash: string;
	artifactBytes: number;
	usesRivetKit: boolean;
	createNamespace?: boolean;
	regions?: string[];
	scaling?: AppScaling;
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

const locks = new Map<string, Promise<void>>();

async function serialized<T>(key: string, run: () => Promise<T>): Promise<T> {
	const previous = locks.get(key) ?? Promise.resolve();
	let release = () => {};
	const gate = new Promise<void>((resolve) => {
		release = resolve;
	});
	const current = previous.then(() => gate);
	locks.set(key, current);
	await previous;
	try {
		return await run();
	} finally {
		release();
		if (locks.get(key) === current) locks.delete(key);
	}
}

function fail(
	code: string,
	message: string,
	metadata?: Record<string, unknown>,
): never {
	throw new UserError(message, { code, metadata });
}

async function actorBoundary<T>(run: () => Promise<T>): Promise<T> {
	try {
		return await run();
	} catch (error) {
		if (error instanceof DynamicAppsError) {
			fail(error.code, error.message, error.metadata);
		}
		throw error;
	}
}

function positiveInteger(value: number, name: string, maximum: number): number {
	if (!Number.isInteger(value) || value < 1 || value > maximum) {
		fail(
			"dynamic_apps_invalid_config",
			`${name} must be an integer between 1 and ${maximum}`,
			{ name, maximum },
		);
	}
	return value;
}

export function normalizeScaling(
	input: AppScaling | undefined,
): Required<AppScaling> {
	if (
		input !== undefined &&
		(typeof input !== "object" || input === null || Array.isArray(input))
	) {
		fail("dynamic_apps_invalid_scaling", "scaling must be an object");
	}
	const minReplicas = input?.minReplicas ?? 0;
	const maxReplicas = input?.maxReplicas ?? 128;
	const targetConcurrency = input?.targetConcurrency ?? 8;
	if (
		!Number.isInteger(minReplicas) ||
		minReplicas < 0 ||
		minReplicas > MAX_REPLICAS
	) {
		fail(
			"dynamic_apps_invalid_scaling",
			`scaling.minReplicas must be an integer between 0 and ${MAX_REPLICAS}`,
		);
	}
	positiveInteger(maxReplicas, "scaling.maxReplicas", MAX_REPLICAS);
	positiveInteger(targetConcurrency, "scaling.targetConcurrency", 1_024);
	if (minReplicas > maxReplicas) {
		fail(
			"dynamic_apps_invalid_scaling",
			"scaling.minReplicas cannot exceed scaling.maxReplicas",
		);
	}
	return { minReplicas, maxReplicas, targetConcurrency };
}

function normalizeRegions(
	regions: string[] | undefined,
	fallbackRegion: string,
): string[] {
	if (regions !== undefined && !Array.isArray(regions)) {
		fail("dynamic_apps_invalid_regions", "regions must be an array");
	}
	const unique = [...new Set(regions ?? [fallbackRegion || "default"])];
	if (unique.length === 0 || unique.length > DEFAULT_MAX_REGIONS) {
		fail(
			"dynamic_apps_invalid_regions",
			`an app must have between 1 and ${DEFAULT_MAX_REGIONS} regions`,
		);
	}
	for (const region of unique) {
		if (!/^[a-z0-9][a-z0-9-]{0,62}$/.test(region)) {
			fail(
				"dynamic_apps_invalid_region",
				`invalid region ${JSON.stringify(region)}`,
			);
		}
	}
	return unique;
}

export async function migrateAppsTables(database: RawAccess): Promise<void> {
	await database.execute(`
		CREATE TABLE IF NOT EXISTS dynamic_apps_releases (
			release_id TEXT PRIMARY KEY,
			created_at INTEGER NOT NULL,
			status TEXT NOT NULL,
			entrypoint TEXT NOT NULL,
			artifact_hash TEXT NOT NULL DEFAULT '',
			artifact_bytes INTEGER NOT NULL DEFAULT 0,
			build_error TEXT,
			regions_json TEXT NOT NULL,
			scaling_json TEXT NOT NULL,
			namespace TEXT NOT NULL,
			envoy_version INTEGER NOT NULL,
			runtime_endpoint TEXT NOT NULL,
			runtime_pool TEXT NOT NULL,
			callback_secret TEXT NOT NULL DEFAULT '',
			uses_rivetkit INTEGER NOT NULL DEFAULT 0
				CHECK (uses_rivetkit IN (0, 1))
		);
		CREATE TABLE IF NOT EXISTS dynamic_apps_release_files (
			release_id TEXT NOT NULL,
			path TEXT NOT NULL,
			chunk_index INTEGER NOT NULL,
			content BLOB NOT NULL,
			byte_length INTEGER NOT NULL,
			PRIMARY KEY (release_id, path, chunk_index)
		);
		CREATE TABLE IF NOT EXISTS dynamic_apps_artifact_chunks (
			release_id TEXT NOT NULL,
			chunk_index INTEGER NOT NULL,
			content BLOB NOT NULL,
			byte_length INTEGER NOT NULL,
			PRIMARY KEY (release_id, chunk_index)
		);
		CREATE INDEX IF NOT EXISTS idx_dynamic_apps_releases_created_at
			ON dynamic_apps_releases(created_at);
	`);
	const columns = await database.execute<{ name: string }>(
		"PRAGMA table_info(dynamic_apps_releases)",
	);
	if (!columns.some((column) => column.name === "callback_secret")) {
		await database.execute(
			`ALTER TABLE dynamic_apps_releases
			 ADD COLUMN callback_secret TEXT NOT NULL DEFAULT ''`,
		);
	}
	if (!columns.some((column) => column.name === "uses_rivetkit")) {
		await database.execute(
			`ALTER TABLE dynamic_apps_releases
			 ADD COLUMN uses_rivetkit INTEGER NOT NULL DEFAULT 0
			 CHECK (uses_rivetkit IN (0, 1))`,
		);
	}
}

async function deleteReleaseFilesBatched(
	database: RawAccess,
	releaseId: string,
): Promise<void> {
	for (let batch = 0; batch < MAX_SOURCE_CHUNKS; batch += 1) {
		const rows = await database.execute<{ chunks: number }>(
			`SELECT COUNT(*) AS chunks FROM dynamic_apps_release_files
			 WHERE release_id = ?`,
			releaseId,
		);
		if (Number(rows[0]?.chunks ?? 0) === 0) return;
		await database.execute(
			`DELETE FROM dynamic_apps_release_files WHERE rowid IN (
				SELECT rowid FROM dynamic_apps_release_files
				WHERE release_id = ? ORDER BY path, chunk_index LIMIT 1
			)`,
			releaseId,
		);
	}
	fail(
		"dynamic_apps_source_cleanup_limit",
		`source cleanup exceeded ${MAX_SOURCE_CHUNKS} bounded batches`,
	);
}

async function persistReleaseFilesBatched(
	database: RawAccess,
	releaseId: string,
	files: Record<string, Uint8Array>,
): Promise<void> {
	for (const [path, content] of Object.entries(files)) {
		const count = Math.max(
			1,
			Math.ceil(content.byteLength / SOURCE_CHUNK_BYTES),
		);
		for (let index = 0; index < count; index += 1) {
			const chunk = content.slice(
				index * SOURCE_CHUNK_BYTES,
				(index + 1) * SOURCE_CHUNK_BYTES,
			);
			await database.execute(
				`INSERT INTO dynamic_apps_release_files
				 (release_id, path, chunk_index, content, byte_length)
				 VALUES (?, ?, ?, ?, ?)`,
				releaseId,
				path,
				index,
				chunk,
				chunk.byteLength,
			);
		}
	}
}

async function deleteArtifactChunksBatched(
	database: RawAccess,
	releaseId: string,
): Promise<void> {
	for (let batch = 0; batch < MAX_ARTIFACT_CHUNKS; batch += 1) {
		const rows = await database.execute<{ chunks: number }>(
			`SELECT COUNT(*) AS chunks FROM dynamic_apps_artifact_chunks
			 WHERE release_id = ?`,
			releaseId,
		);
		if (Number(rows[0]?.chunks ?? 0) === 0) return;
		await database.execute(
			`DELETE FROM dynamic_apps_artifact_chunks WHERE rowid IN (
				SELECT rowid FROM dynamic_apps_artifact_chunks
				WHERE release_id = ? ORDER BY chunk_index LIMIT 1
			)`,
			releaseId,
		);
	}
	fail(
		"dynamic_apps_artifact_cleanup_limit",
		`artifact cleanup exceeded ${MAX_ARTIFACT_CHUNKS} bounded batches`,
	);
}

interface ReleaseRow extends Record<string, unknown> {
	release_id: string;
	created_at: number;
	status: StoredAppRelease["status"];
	entrypoint: string;
	artifact_hash: string;
	artifact_bytes: number;
	build_error: string | null;
	regions_json: string;
	scaling_json: string;
	namespace: string;
	runtime_endpoint: string;
	runtime_pool: string;
	callback_secret: string;
	uses_rivetkit: number;
}

function releaseFromRow(row: ReleaseRow): StoredAppRelease {
	return {
		release: row.release_id,
		createdAt: Number(row.created_at),
		status: row.status,
		entrypoint: row.entrypoint,
		artifactHash: row.artifact_hash,
		artifactBytes: Number(row.artifact_bytes),
		error: row.build_error ?? undefined,
		regions: JSON.parse(row.regions_json) as string[],
		scaling: JSON.parse(row.scaling_json) as Required<AppScaling>,
		namespace: row.namespace,
		runtimeEndpoint: row.runtime_endpoint,
		runtimePool: row.runtime_pool,
		callbackSecret: row.callback_secret ?? "",
		usesRivetKit: Number(row.uses_rivetkit ?? 0) === 1,
	};
}

async function getStoredRelease(
	database: RawAccess,
	releaseId: string,
): Promise<StoredAppRelease | undefined> {
	const rows = await database.execute<ReleaseRow>(
		"SELECT * FROM dynamic_apps_releases WHERE release_id = ?",
		releaseId,
	);
	return rows[0] ? releaseFromRow(rows[0]) : undefined;
}

async function listStoredReleases(
	database: RawAccess,
): Promise<StoredAppRelease[]> {
	const rows = await database.execute<ReleaseRow>(
		"SELECT * FROM dynamic_apps_releases ORDER BY created_at ASC",
	);
	return rows.map(releaseFromRow);
}

async function readStoredArtifact(
	database: RawAccess,
	release: StoredAppRelease,
): Promise<Uint8Array> {
	const rows = await database.execute<{
		chunk_index: number;
		content: Uint8Array;
		byte_length: number;
	}>(
		`SELECT chunk_index, content, byte_length
		 FROM dynamic_apps_artifact_chunks
		 WHERE release_id = ? ORDER BY chunk_index ASC`,
		release.release,
	);
	const expectedChunks = Math.ceil(
		release.artifactBytes / ARTIFACT_CHUNK_BYTES,
	);
	if (
		rows.length !== expectedChunks ||
		rows.length < 1 ||
		rows.length > MAX_ARTIFACT_CHUNKS
	) {
		fail(
			"dynamic_apps_artifact_manifest_invalid",
			`artifact ${release.release} has an invalid chunk count`,
		);
	}
	let bytes = 0;
	const chunks: Uint8Array[] = [];
	for (let index = 0; index < rows.length; index += 1) {
		const row = rows[index];
		const content = row ? new Uint8Array(row.content) : undefined;
		const expected =
			index === rows.length - 1
				? release.artifactBytes - index * ARTIFACT_CHUNK_BYTES
				: ARTIFACT_CHUNK_BYTES;
		if (
			!row ||
			!content ||
			Number(row.chunk_index) !== index ||
			content.byteLength !== Number(row.byte_length) ||
			content.byteLength !== expected
		) {
			fail(
				"dynamic_apps_artifact_manifest_invalid",
				`artifact ${release.release} contains an invalid chunk`,
			);
		}
		bytes += content.byteLength;
		chunks.push(content);
	}
	const artifact = new Uint8Array(Buffer.concat(chunks, bytes));
	if (
		bytes !== release.artifactBytes ||
		createHash("sha256").update(artifact).digest("hex") !== release.artifactHash
	) {
		fail(
			"dynamic_apps_artifact_hash_mismatch",
			`artifact ${release.release} failed hash verification`,
		);
	}
	return artifact;
}

function normalizeActorCallbackPath(
	request: Request,
): "/api/rivet/metadata" | "/api/rivet/start" | undefined {
	if (!request.headers.get("user-agent")?.startsWith("RivetEngine/")) return;
	const pathname = new URL(request.url).pathname;
	if (request.method === "GET" && pathname.endsWith("/metadata")) {
		return "/api/rivet/metadata";
	}
	if (
		(request.method === "GET" || request.method === "POST") &&
		pathname.endsWith("/start")
	) {
		return "/api/rivet/start";
	}
	return;
}

function validCallbackSecret(request: Request, expected: string): boolean {
	const received = request.headers.get(APP_CALLBACK_SECRET_HEADER);
	if (!received || !expected) return false;
	return timingSafeEqual(
		createHash("sha256").update(received).digest(),
		createHash("sha256").update(expected).digest(),
	);
}

function actorPublicEndpoint(
	release: StoredAppRelease,
	state: AppState,
): string {
	if (state.runnerToken) {
		const endpoint = new URL(release.runtimeEndpoint);
		endpoint.username = release.namespace;
		endpoint.password = state.runnerToken;
		return endpoint.toString();
	}
	const publicEndpoint = process.env.RIVET_PUBLIC_ENDPOINT;
	const raw =
		publicEndpoint ?? process.env.RIVET_ENDPOINT ?? release.runtimeEndpoint;
	const endpoint = new URL(raw);
	if (!publicEndpoint && (endpoint.username || endpoint.password)) {
		throw new DynamicAppsError(
			"dynamic_apps_public_endpoint_required",
			"RIVET_PUBLIC_ENDPOINT is required to run app actors when RIVET_ENDPOINT contains credentials",
		);
	}
	if (endpoint.username) endpoint.username = release.namespace;
	return endpoint.toString();
}

function validateBeginInput(
	c: AnyActorContext,
	input: BeginReleasePublishInput,
): string {
	const appId = c.key[0];
	if (!appId || c.key.length !== 1 || input.appId !== appId) {
		fail(
			"dynamic_apps_app_id_mismatch",
			"deployApp appId must match the stable application actor key",
			{ appId: input.appId, actorKey: c.key },
		);
	}
	if (
		input.format !== DIRECT_RUNTIME_FORMAT ||
		input.entrypoint !== DIRECT_ENTRYPOINT ||
		!RELEASE_PATTERN.test(input.buildId) ||
		!RELEASE_PATTERN.test(input.artifactHash) ||
		!Number.isSafeInteger(input.artifactBytes) ||
		input.artifactBytes < 1 ||
		input.artifactBytes > MAX_ARTIFACT_BYTES ||
		typeof input.usesRivetKit !== "boolean" ||
		(input.createNamespace !== undefined &&
			typeof input.createNamespace !== "boolean") ||
		!Number.isSafeInteger(input.createdAt) ||
		input.createdAt < 0
	) {
		fail("dynamic_apps_publish_invalid", "release publish metadata is invalid");
	}
	if (input.createNamespace === false && input.usesRivetKit) {
		fail(
			"dynamic_apps_publish_invalid",
			"apps that use rivetkit require a namespace; remove createNamespace: false",
		);
	}
	return appId;
}

function releaseIdFor(input: {
	buildId: string;
	artifactHash: string;
	regions: string[];
	scaling: Required<AppScaling>;
	namespace: string;
	endpoint: string;
	pool: string;
	usesRivetKit: boolean;
}): string {
	const hash = createHash("sha256");
	hash.update("dynamic-apps-rivet-release-v1\0");
	for (const value of [
		input.buildId,
		input.artifactHash,
		JSON.stringify(input.regions),
		JSON.stringify(input.scaling),
		input.namespace,
		input.endpoint,
		input.pool,
		input.usesRivetKit ? "1" : "0",
	]) {
		const bytes = Buffer.from(value);
		const length = Buffer.allocUnsafe(8);
		length.writeBigUInt64BE(BigInt(bytes.byteLength));
		hash.update(length);
		hash.update(bytes);
	}
	return hash.digest("hex");
}

async function beginReleasePublishLocked(
	c: AnyActorContext,
	input: BeginReleasePublishInput,
): Promise<BeginReleasePublishResult> {
	const appId = validateBeginInput(c, input);
	const state = c.state as AppState;
	const regions = normalizeRegions(input.regions, c.region);
	const scaling = normalizeScaling(input.scaling);
	// createNamespace: false disables provisioning entirely; unset keeps the
	// default behavior of giving every app its own stable namespace.
	const runtime =
		input.createNamespace === false
			? unprovisionedAppNamespace(appId)
			: await provisionAppNamespace(appId, resolveDefaultRivetConnection(), {
					namespace: state.namespace,
					cloudNamespace: state.cloudNamespace,
				});
	state.namespace = runtime.namespace;
	state.cloudNamespace = runtime.cloudNamespace ?? null;
	state.runnerToken = runtime.runnerToken ?? null;
	state.publicToken = runtime.publicToken ?? null;
	const releaseId = releaseIdFor({
		buildId: input.buildId,
		artifactHash: input.artifactHash,
		regions,
		scaling,
		namespace: runtime.namespace,
		endpoint: runtime.endpoint,
		pool: runtime.pool,
		usesRivetKit: input.usesRivetKit,
	});
	const releases = await listStoredReleases(c.db);
	const existing = await getStoredRelease(c.db, releaseId);
	const callbackSecret = input.usesRivetKit
		? existing?.callbackSecret ||
			releases.find((candidate) => candidate.callbackSecret)?.callbackSecret ||
			randomUUID()
		: "";
	const sequence = (state.publishSequence ?? 0) + 1;
	state.publishSequence = sequence;
	state.latestPublishSequence = sequence;
	const metadataMatches =
		existing?.status === "ready" &&
		existing.entrypoint === DIRECT_ENTRYPOINT &&
		existing.artifactHash === input.artifactHash &&
		existing.artifactBytes === input.artifactBytes &&
		JSON.stringify(existing.regions) === JSON.stringify(regions) &&
		JSON.stringify(existing.scaling) === JSON.stringify(scaling) &&
		existing.namespace === runtime.namespace &&
		existing.runtimeEndpoint === runtime.endpoint &&
		existing.runtimePool === runtime.pool &&
		existing.usesRivetKit === input.usesRivetKit;
	if (metadataMatches) {
		return {
			release: releaseId,
			sequence,
			uploadRequired: false,
			chunkBytes: ARTIFACT_CHUNK_BYTES,
		};
	}
	await deleteArtifactChunksBatched(c.db, releaseId);
	await c.db.execute(
		`INSERT INTO dynamic_apps_releases (
			release_id, created_at, status, entrypoint,
			artifact_hash, artifact_bytes, build_error,
			regions_json, scaling_json, namespace, envoy_version,
			runtime_endpoint, runtime_pool, callback_secret, uses_rivetkit
		) VALUES (?, ?, 'building', ?, ?, ?, NULL, ?, ?, ?, 1, ?, ?, ?, ?)
		ON CONFLICT(release_id) DO UPDATE SET
			status = 'building', entrypoint = excluded.entrypoint,
			artifact_hash = excluded.artifact_hash,
			artifact_bytes = excluded.artifact_bytes, build_error = NULL,
			regions_json = excluded.regions_json,
			scaling_json = excluded.scaling_json,
			namespace = excluded.namespace,
			runtime_endpoint = excluded.runtime_endpoint,
			runtime_pool = excluded.runtime_pool,
			callback_secret = excluded.callback_secret,
			uses_rivetkit = excluded.uses_rivetkit`,
		releaseId,
		existing?.createdAt ?? input.createdAt,
		DIRECT_ENTRYPOINT,
		input.artifactHash,
		input.artifactBytes,
		JSON.stringify(regions),
		JSON.stringify(scaling),
		runtime.namespace,
		runtime.endpoint,
		runtime.pool,
		callbackSecret,
		input.usesRivetKit ? 1 : 0,
	);
	return {
		release: releaseId,
		sequence,
		uploadRequired: true,
		chunkBytes: ARTIFACT_CHUNK_BYTES,
	};
}

async function writeReleaseChunk(
	c: AnyActorContext,
	input: WriteReleaseChunkInput,
): Promise<void> {
	const state = c.state as AppState;
	if (
		!RELEASE_PATTERN.test(input.release) ||
		!Number.isSafeInteger(input.sequence) ||
		input.sequence < 1 ||
		input.sequence !== state.latestPublishSequence ||
		!Number.isSafeInteger(input.index) ||
		input.index < 0 ||
		!(input.content instanceof Uint8Array)
	) {
		fail(
			"dynamic_apps_invalid_artifact_chunk",
			"release chunk metadata is invalid or superseded",
		);
	}
	const release = await getStoredRelease(c.db, input.release);
	if (!release || release.status !== "building") {
		fail(
			"dynamic_apps_artifact_not_building",
			`release ${input.release} is not accepting artifact chunks`,
		);
	}
	const chunks = Math.ceil(release.artifactBytes / ARTIFACT_CHUNK_BYTES);
	const expected =
		input.index === chunks - 1
			? release.artifactBytes - input.index * ARTIFACT_CHUNK_BYTES
			: ARTIFACT_CHUNK_BYTES;
	if (input.index >= chunks || input.content.byteLength !== expected) {
		fail(
			"dynamic_apps_invalid_artifact_chunk",
			`artifact chunk ${input.index} has an invalid length`,
		);
	}
	const content = new Uint8Array(input.content);
	await c.db.execute(
		`INSERT INTO dynamic_apps_artifact_chunks
		 (release_id, chunk_index, content, byte_length)
		 VALUES (?, ?, ?, ?)
		 ON CONFLICT(release_id, chunk_index) DO UPDATE SET
			content = excluded.content, byte_length = excluded.byte_length`,
		input.release,
		input.index,
		content,
		content.byteLength,
	);
}

async function commitReleasePublishLocked(
	c: AnyActorContext,
	input: CommitReleasePublishInput,
): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }> {
	const state = c.state as AppState;
	if (
		!RELEASE_PATTERN.test(input.release) ||
		!Number.isSafeInteger(input.sequence) ||
		input.sequence !== state.latestPublishSequence
	) {
		fail(
			"dynamic_apps_publish_superseded",
			"a newer release publish superseded this upload",
		);
	}
	const release = await getStoredRelease(c.db, input.release);
	if (
		!release ||
		(release.status !== "building" && release.status !== "ready")
	) {
		fail(
			"dynamic_apps_artifact_not_ready",
			`release ${input.release} cannot be committed`,
		);
	}
	const expectedChunks = Math.ceil(
		release.artifactBytes / ARTIFACT_CHUNK_BYTES,
	);
	if (
		!Number.isSafeInteger(input.chunks) ||
		input.chunks !== expectedChunks ||
		input.chunks < 1 ||
		input.chunks > MAX_ARTIFACT_CHUNKS
	) {
		fail(
			"dynamic_apps_artifact_manifest_invalid",
			"release commit has an invalid artifact chunk count",
		);
	}
	await readStoredArtifact(c.db, release);
	if (release.status === "ready" && state.activeRelease === release.release) {
		return deploymentForRelease(c, state, release);
	}
	if (release.status !== "ready") {
		await c.db.execute(
			`UPDATE dynamic_apps_releases SET status = 'ready', build_error = NULL
			 WHERE release_id = ?`,
			release.release,
		);
		release.status = "ready";
	}
	const appId = c.key[0];
	if (!appId)
		fail("dynamic_apps_invalid_app_id", "application actor key is missing");
	const runtime = await provisionAppNamespace(
		appId,
		resolveDefaultRivetConnection(),
		{
			namespace: state.namespace,
			cloudNamespace: state.cloudNamespace,
		},
	);
	state.namespace = runtime.namespace;
	state.cloudNamespace = runtime.cloudNamespace ?? null;
	state.runnerToken = runtime.runnerToken ?? null;
	state.publicToken = runtime.publicToken ?? null;
	if (release.usesRivetKit) {
		const publicEndpoint = actorPublicEndpoint(release, state);
		if (
			runtime.endpoint.replace(/\/$/, "") !==
			release.runtimeEndpoint.replace(/\/$/, "")
		) {
			fail(
				"dynamic_apps_runtime_changed",
				"the app actor Rivet endpoint does not match the deployment runtime",
			);
		}
		state.activatingRelease = release.release;
		try {
			const metadataResponse = await getDefaultActorRuntime().request({
				key: `${release.release}:${release.artifactHash}`,
				appId,
				release: release.release,
				loadArtifact: () => readStoredArtifact(c.db, release),
				endpoint: publicEndpoint,
				namespace: release.namespace,
				pool: release.runtimePool,
				request: new Request("http://dynamic-app.internal/api/rivet/metadata", {
					headers: { "user-agent": "RivetEngine/prewarm" },
				}),
			});
			if (!metadataResponse.ok) {
				throw new Error(
					`Dynamic App actor metadata prewarm returned HTTP ${metadataResponse.status}`,
				);
			}
			await metadataResponse.arrayBuffer();
			await configureAppNamespaceRunner(
				c.actorId,
				{
					endpoint: release.runtimeEndpoint,
					namespace: release.namespace,
					pool: release.runtimePool,
					controlToken: runtime.controlToken,
				},
				release.callbackSecret,
			);
		} catch (error) {
			state.activatingRelease = null;
			c.log.error({
				msg: "Dynamic App actor runner configuration failed",
				release: release.release,
				error: error instanceof Error ? error.stack : String(error),
			});
			throw error;
		}
	}
	state.activeRelease = release.release;
	state.activatingRelease = null;
	state.revision += 1;
	c.broadcast("releaseActivated", {
		revision: state.revision,
		release: release.release,
		artifactHash: release.artifactHash,
		activatedAt: Date.now(),
	});
	const releases = await listStoredReleases(c.db);
	const removable = releases
		.filter((candidate) => candidate.release !== release.release)
		.sort((a, b) => a.createdAt - b.createdAt);
	let retained = releases.length;
	while (retained > DEFAULT_MAX_VERSIONS) {
		const candidate = removable.shift();
		if (!candidate) break;
		await deleteArtifactChunksBatched(c.db, candidate.release);
		await deleteReleaseFilesBatched(c.db, candidate.release);
		await c.db.execute(
			"DELETE FROM dynamic_apps_releases WHERE release_id = ?",
			candidate.release,
		);
		retained -= 1;
	}
	return deploymentForRelease(c, state, release);
}

function deploymentForRelease(
	c: AnyActorContext,
	state: AppState,
	release: StoredAppRelease,
): Deployment & { appActorId: string; usesRivetKit: boolean } {
	const appId = c.key[0];
	if (!appId)
		fail("dynamic_apps_invalid_app_id", "application actor key is missing");
	return {
		appId,
		release: release.release,
		endpoint: release.runtimeEndpoint,
		namespace: release.namespace,
		pool: release.runtimePool,
		...(state.publicToken ? { token: state.publicToken } : {}),
		appActorId: c.actorId,
		usesRivetKit: release.usesRivetKit,
	};
}

export function createAppsActors(
	options: { artifactCache?: BuildArtifactCache } = {},
): DynamicAppsActors {
	const forwardActorRequest = async (
		c: AnyActorContext,
		request: Request,
	): Promise<Response> => {
		const callbackPath = normalizeActorCallbackPath(request);
		if (!callbackPath) return new Response("Not Found", { status: 404 });
		const state = c.state as AppState;
		const routedRelease = state.activatingRelease ?? state.activeRelease;
		const release = routedRelease
			? await getStoredRelease(c.db, routedRelease)
			: undefined;
		if (!release || release.status !== "ready" || !release.usesRivetKit) {
			return new Response("Dynamic App has no active actor registry", {
				status: 503,
			});
		}
		if (!validCallbackSecret(request, release.callbackSecret)) {
			return new Response("Unauthorized", { status: 401 });
		}
		try {
			return await getDefaultActorRuntime().request({
				key: `${release.release}:${release.artifactHash}`,
				appId: c.key[0],
				release: release.release,
				loadArtifact: () => readStoredArtifact(c.db, release),
				endpoint: actorPublicEndpoint(release, state),
				namespace: release.namespace,
				pool: release.runtimePool,
				request: forwardActorCallbackRequest(request, callbackPath),
			});
		} catch (error) {
			c.log.error({
				msg: "Dynamic App actor callback failed",
				release: release.release,
				error: error instanceof Error ? error.stack : String(error),
			});
			throw error;
		}
	};

	const dynamicAppsApp = actor({
		options: { actionTimeout: 16 * 60_000 },
		db: db({ onMigrate: migrateAppsTables }),
		onRequest: forwardActorRequest,
		createState: (): AppState => ({
			activeRelease: null,
			namespace: null,
			revision: 0,
			cloudNamespace: null,
			runnerToken: null,
			publicToken: null,
			publishSequence: 0,
			latestPublishSequence: 0,
		}),
		actions: {
			beginReleasePublish: (
				c: AnyActorContext,
				input: BeginReleasePublishInput,
			) =>
				c.keepAwake(
					actorBoundary(() =>
						serialized(`app:${c.actorId}`, () =>
							beginReleasePublishLocked(c, input),
						),
					),
				),
			writeReleaseChunk: (c: AnyActorContext, input: WriteReleaseChunkInput) =>
				c.keepAwake(
					actorBoundary(() =>
						serialized(`app:${c.actorId}`, () => writeReleaseChunk(c, input)),
					),
				),
			commitReleasePublish: (
				c: AnyActorContext,
				input: CommitReleasePublishInput,
			) =>
				c.keepAwake(
					actorBoundary(() =>
						serialized(`app:${c.actorId}`, () =>
							commitReleasePublishLocked(c, input),
						),
					),
				),
			deploy: (
				c: AnyActorContext,
				input: PreparedDeployAppInput,
			): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }> =>
				c.keepAwake(
					actorBoundary(async () => {
						const appId = c.key[0];
						if (!appId || c.key.length !== 1 || input.appId !== appId) {
							fail(
								"dynamic_apps_app_id_mismatch",
								"deployApp appId must match the stable application actor key",
							);
						}
						const built = await buildAppRelease(
							{ appId, files: input.files },
							{
								artifactCache: options.artifactCache,
								logger: {
									info: (event) => c.log.info(event),
									error: (event) => c.log.error(event),
								},
							},
						);
						return serialized(`app:${c.actorId}`, async () => {
							const publishInput: BeginReleasePublishInput = {
								appId,
								buildId: built.buildId,
								format: built.artifact.format,
								entrypoint: built.artifact.entrypoint,
								artifactHash: built.artifact.hash,
								artifactBytes: built.artifact.byteLength,
								usesRivetKit: built.artifact.usesRivetKit,
								createNamespace: input.createNamespace,
								createdAt: Date.now(),
							};
							const begin = await beginReleasePublishLocked(c, publishInput);
							await deleteReleaseFilesBatched(c.db, begin.release);
							await persistReleaseFilesBatched(
								c.db,
								begin.release,
								input.files,
							);
							const chunks = Math.ceil(
								built.artifact.byteLength / ARTIFACT_CHUNK_BYTES,
							);
							if (begin.uploadRequired) {
								for (let index = 0; index < chunks; index += 1) {
									await writeReleaseChunk(c, {
										release: begin.release,
										sequence: begin.sequence,
										index,
										content: built.artifact.bytes.slice(
											index * ARTIFACT_CHUNK_BYTES,
											(index + 1) * ARTIFACT_CHUNK_BYTES,
										),
									});
								}
							}
							return commitReleasePublishLocked(c, {
								release: begin.release,
								sequence: begin.sequence,
								chunks,
							});
						});
					}),
				),
			resolveDeployment: async (
				c: AnyActorContext,
				requestedRegion?: string,
			): Promise<AppRouteResolution> => {
				const state = c.state as AppState;
				const appId = c.key[0];
				if (!appId) {
					fail(
						"dynamic_apps_invalid_app_id",
						"application actor key is missing",
					);
				}
				const release = state.activeRelease
					? await getStoredRelease(c.db, state.activeRelease)
					: undefined;
				if (
					!release ||
					release.status !== "ready" ||
					release.entrypoint !== DIRECT_ENTRYPOINT
				) {
					fail(
						"dynamic_apps_not_deployed",
						"app has no active direct release; call app.deploy() first",
					);
				}
				if (requestedRegion && !release.regions.includes(requestedRegion)) {
					fail(
						"dynamic_apps_region_not_deployed",
						`app is not deployed in requested region ${requestedRegion}`,
						{ requestedRegion, regions: release.regions },
					);
				}
				const region = requestedRegion ?? release.regions[0];
				if (!region) fail("dynamic_apps_no_region", "active app has no region");
				return {
					appId,
					release: release.release,
					region,
					regions: [...release.regions],
					revision: state.revision,
					artifactHash: release.artifactHash,
					artifactBytes: release.artifactBytes,
					entrypoint: DIRECT_ENTRYPOINT,
					namespace: release.namespace,
					scaling: { ...release.scaling },
					maxRequestBytes: DEFAULT_MAX_REQUEST_BYTES,
					maxResponseBytes: DEFAULT_MAX_RESPONSE_BYTES,
					usesRivetKit: release.usesRivetKit,
					...(release.usesRivetKit
						? {
								serverlessEndpoint: actorPublicEndpoint(release, state),
								runtimePool: release.runtimePool,
							}
						: {}),
				};
			},
			getArtifactManifest: async (c: AnyActorContext, releaseId: string) => {
				const release = await getStoredRelease(c.db, releaseId);
				if (
					!release ||
					release.status !== "ready" ||
					release.entrypoint !== DIRECT_ENTRYPOINT
				) {
					fail(
						"dynamic_apps_artifact_not_ready",
						`artifact for release ${releaseId} is not ready`,
					);
				}
				const rows = await c.db.execute<{ chunks: number; bytes: number }>(
					`SELECT COUNT(*) AS chunks,
					 COALESCE(SUM(byte_length), 0) AS bytes
					 FROM dynamic_apps_artifact_chunks WHERE release_id = ?`,
					releaseId,
				);
				const chunks = Number(rows[0]?.chunks ?? 0);
				const bytes = Number(rows[0]?.bytes ?? 0);
				if (
					chunks !== Math.ceil(bytes / ARTIFACT_CHUNK_BYTES) ||
					chunks > MAX_ARTIFACT_CHUNKS ||
					bytes !== release.artifactBytes
				) {
					fail(
						"dynamic_apps_artifact_manifest_invalid",
						`artifact ${releaseId} failed persisted manifest validation`,
					);
				}
				return {
					format: DIRECT_RUNTIME_FORMAT,
					hash: release.artifactHash,
					bytes,
					chunks,
					chunkBytes: ARTIFACT_CHUNK_BYTES,
				};
			},
			readArtifactChunk: async (
				c: AnyActorContext,
				releaseId: string,
				index: number,
			) => {
				if (
					!Number.isInteger(index) ||
					index < 0 ||
					index >= MAX_ARTIFACT_CHUNKS
				) {
					fail(
						"dynamic_apps_invalid_artifact_chunk",
						`artifact chunk index must be between 0 and ${MAX_ARTIFACT_CHUNKS - 1}`,
					);
				}
				const rows = await c.db.execute<{
					content: Uint8Array;
					byte_length: number;
				}>(
					`SELECT content, byte_length FROM dynamic_apps_artifact_chunks
					 WHERE release_id = ? AND chunk_index = ?`,
					releaseId,
					index,
				);
				const row = rows[0];
				if (!row) {
					fail(
						"dynamic_apps_artifact_chunk_not_found",
						`artifact chunk ${index} for release ${releaseId} was not found`,
					);
				}
				const content = new Uint8Array(row.content);
				if (
					content.byteLength !== Number(row.byte_length) ||
					content.byteLength > ARTIFACT_CHUNK_BYTES
				) {
					fail(
						"dynamic_apps_artifact_chunk_invalid",
						`artifact chunk ${index} failed length validation`,
					);
				}
				return content;
			},
			inspect: async (c: AnyActorContext) => {
				const state = c.state as AppState;
				return {
					activeRelease: state.activeRelease,
					namespace: state.namespace,
					revision: state.revision,
					releases: await listStoredReleases(c.db),
				};
			},
		},
	});
	return { dynamicAppsApp };
}

/** @internal Preserves callback streaming until runtime admission. */
export function forwardActorCallbackRequest(
	request: Request,
	callbackPath: string,
): Request {
	const url = new URL(request.url);
	url.pathname = callbackPath;
	const headers = new Headers(request.headers);
	headers.delete(APP_CALLBACK_SECRET_HEADER);
	const body =
		request.method === "GET" || request.method === "HEAD"
			? undefined
			: request.body;
	return new Request(url, {
		method: request.method,
		headers,
		body,
		signal: request.signal,
		...(body ? { duplex: "half" } : {}),
	} as RequestInit & { duplex?: "half" });
}
