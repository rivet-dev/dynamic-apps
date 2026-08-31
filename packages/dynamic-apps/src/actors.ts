import { createHash, randomUUID, timingSafeEqual } from "node:crypto";
import { chmod, mkdtemp, readFile, rm, stat } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import sh from "@agentos-software/sh";
import tar from "@agentos-software/tar";
import {
	AgentOs,
	type AgentOsOptions,
	createHostDirBackend,
} from "@rivet-dev/agentos-core";
import { packAospkgFromTarBytes } from "@rivet-dev/agentos-toolchain";
import appsBuilder, {
	appBundleManifestVersion,
	appsBuilderVersion,
} from "@rivet-dev/dynamic-apps-builder";
import { type AnyActorDefinition, actor, UserError } from "rivetkit";
import { db, type RawAccess } from "rivetkit/db";
import { getDefaultActorRuntime } from "./actor-runtime.js";
import {
	configureAppNamespaceRunner,
	provisionAppNamespace,
	resolveDefaultRivetConnection,
} from "./control-plane.js";
import { DynamicAppsError } from "./errors.js";
import { DynamicAppsLogLineDecoder, emitDynamicAppsLog } from "./logging.js";
import {
	APP_CALLBACK_SECRET_HEADER,
	actorRunnerSource,
	canonicalDeploymentHash,
	DIRECT_BUNDLE_PATH,
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
	directRunnerSource,
	normalizeAppPath,
} from "./runtime.js";
import type {
	AppReleaseInfo,
	AppScaling,
	Deployment,
	PreparedDeployAppInput,
} from "./types.js";

const DEFAULT_MAX_SOURCE_BYTES = 4 * 1024 * 1024;
const DEFAULT_MAX_FILES = 2_000;
const DEFAULT_MAX_VERSIONS = 20;
const DEFAULT_MAX_REGIONS = 8;
const DEFAULT_MAX_DEPENDENCIES = 256;
const DEFAULT_BUILD_TIMEOUT_MS = 15 * 60_000;
const DEFAULT_MAX_REQUEST_BYTES = 1024 * 1024;
const DEFAULT_MAX_RESPONSE_BYTES = 4 * 1024 * 1024;
const DEFAULT_MAX_BUILD_OUTPUT_BYTES = 2 * 1024 * 1024;
const DEFAULT_MAX_BUILD_ARTIFACT_BYTES = 64 * 1024 * 1024;
const DEFAULT_MAX_BUILD_ARTIFACT_FILES = 4_096;
const DEFAULT_MAX_BUILD_ARTIFACT_FILE_BYTES = 32 * 1024 * 1024;
const DEFAULT_MAX_BUILD_FILESYSTEM_BYTES = 2 * 1024 * 1024 * 1024;
const MAX_REPLICAS = 128;
export const ARTIFACT_CHUNK_BYTES = 512 * 1024;
const MAX_ARTIFACT_CHUNKS = Math.ceil(
	DEFAULT_MAX_BUILD_ARTIFACT_BYTES / ARTIFACT_CHUNK_BYTES,
);
const SOURCE_CHUNK_BYTES = 512 * 1024;
const MAX_SOURCE_CHUNKS =
	Math.ceil(DEFAULT_MAX_SOURCE_BYTES / SOURCE_CHUNK_BYTES) + DEFAULT_MAX_FILES;

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

interface ExecResult {
	exitCode: number;
	stdout: string;
	stderr: string;
}

interface BuildHandle {
	artifactGuestPath: string;
	writeFiles(
		entries: Array<{ path: string; content: string | Uint8Array }>,
	): Promise<Array<{ path: string; success: boolean; error?: string }>>;
	execArgv(
		command: string,
		args: string[],
		options?: {
			cwd?: string;
			env?: Record<string, string>;
			timeout?: number;
			captureStdio?: boolean;
		},
	): Promise<ExecResult>;
	artifactSize(): Promise<number>;
	readArtifact(): Promise<Uint8Array>;
	dispose(): Promise<void>;
}

interface BuildPlan {
	entrypoint: string;
	build: boolean;
	dependencyCount: number;
	hasLockfile: boolean;
	usesRivetKit: boolean;
}

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
	namespace: string | null;
	revision: number;
	cloudNamespace?: string | null;
	runnerToken?: string | null;
	publicToken?: string | null;
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
}

export interface DynamicAppsActors {
	agentOSAppsApp: AnyActorDefinition;
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

function positiveInteger(value: number, name: string, maximum: number): number {
	if (!Number.isInteger(value) || value < 1 || value > maximum) {
		fail(
			"agentos_apps_invalid_config",
			`${name} must be an integer between 1 and ${maximum}`,
			{ name, maximum },
		);
	}
	return value;
}

export function normalizeScaling(
	input: AppScaling | undefined,
): Required<AppScaling> {
	const minReplicas = input?.minReplicas ?? 0;
	const maxReplicas = input?.maxReplicas ?? 128;
	const targetConcurrency = input?.targetConcurrency ?? 8;
	if (
		!Number.isInteger(minReplicas) ||
		minReplicas < 0 ||
		minReplicas > MAX_REPLICAS
	) {
		fail(
			"agentos_apps_invalid_scaling",
			`scaling.minReplicas must be an integer between 0 and ${MAX_REPLICAS}`,
		);
	}
	positiveInteger(maxReplicas, "scaling.maxReplicas", MAX_REPLICAS);
	positiveInteger(targetConcurrency, "scaling.targetConcurrency", 1_024);
	if (minReplicas > maxReplicas) {
		fail(
			"agentos_apps_invalid_scaling",
			"scaling.minReplicas cannot exceed scaling.maxReplicas",
		);
	}
	return { minReplicas, maxReplicas, targetConcurrency };
}

export async function migrateAppsTables(database: RawAccess): Promise<void> {
	await database.execute(`
		CREATE TABLE IF NOT EXISTS agentos_apps_releases (
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
		CREATE TABLE IF NOT EXISTS agentos_apps_release_files (
			release_id TEXT NOT NULL,
			path TEXT NOT NULL,
			chunk_index INTEGER NOT NULL,
			content BLOB NOT NULL,
			byte_length INTEGER NOT NULL,
			PRIMARY KEY (release_id, path, chunk_index)
		);
		CREATE TABLE IF NOT EXISTS agentos_apps_artifact_chunks (
			release_id TEXT NOT NULL,
			chunk_index INTEGER NOT NULL,
			content BLOB NOT NULL,
			byte_length INTEGER NOT NULL,
			PRIMARY KEY (release_id, chunk_index)
		);
		CREATE INDEX IF NOT EXISTS idx_agentos_apps_releases_created_at
			ON agentos_apps_releases(created_at);
	`);
	const columns = await database.execute<{ name: string }>(
		"PRAGMA table_info(agentos_apps_releases)",
	);
	if (!columns.some((column) => column.name === "callback_secret")) {
		await database.execute(
			`ALTER TABLE agentos_apps_releases
			 ADD COLUMN callback_secret TEXT NOT NULL DEFAULT ''`,
		);
	}
	if (!columns.some((column) => column.name === "uses_rivetkit")) {
		await database.execute(
			`ALTER TABLE agentos_apps_releases
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
			`SELECT COUNT(*) AS chunks FROM agentos_apps_release_files
			 WHERE release_id = ?`,
			releaseId,
		);
		if (Number(rows[0]?.chunks ?? 0) === 0) return;
		await database.execute(
			`DELETE FROM agentos_apps_release_files WHERE rowid IN (
				SELECT rowid FROM agentos_apps_release_files
				WHERE release_id = ? ORDER BY path, chunk_index LIMIT 1
			)`,
			releaseId,
		);
	}
	fail(
		"agentos_apps_source_cleanup_limit",
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
				`INSERT INTO agentos_apps_release_files
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
			`SELECT COUNT(*) AS chunks FROM agentos_apps_artifact_chunks
			 WHERE release_id = ?`,
			releaseId,
		);
		if (Number(rows[0]?.chunks ?? 0) === 0) return;
		await database.execute(
			`DELETE FROM agentos_apps_artifact_chunks WHERE rowid IN (
				SELECT rowid FROM agentos_apps_artifact_chunks
				WHERE release_id = ? ORDER BY chunk_index LIMIT 1
			)`,
			releaseId,
		);
	}
	fail(
		"agentos_apps_artifact_cleanup_limit",
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
		"SELECT * FROM agentos_apps_releases WHERE release_id = ?",
		releaseId,
	);
	return rows[0] ? releaseFromRow(rows[0]) : undefined;
}

async function listStoredReleases(
	database: RawAccess,
): Promise<StoredAppRelease[]> {
	const rows = await database.execute<ReleaseRow>(
		"SELECT * FROM agentos_apps_releases ORDER BY created_at ASC",
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
		 FROM agentos_apps_artifact_chunks
		 WHERE release_id = ? ORDER BY chunk_index ASC`,
		release.release,
	);
	if (rows.length === 0 || rows.length > MAX_ARTIFACT_CHUNKS) {
		fail(
			"agentos_apps_artifact_manifest_invalid",
			`artifact ${release.release} has an invalid chunk count`,
		);
	}
	let bytes = 0;
	const chunks: Uint8Array[] = [];
	for (let index = 0; index < rows.length; index += 1) {
		const row = rows[index];
		const content = row ? new Uint8Array(row.content) : undefined;
		if (
			!row ||
			!content ||
			Number(row.chunk_index) !== index ||
			content.byteLength !== Number(row.byte_length)
		) {
			fail(
				"agentos_apps_artifact_manifest_invalid",
				`artifact ${release.release} contains an invalid chunk`,
			);
		}
		bytes += content.byteLength;
		chunks.push(content);
	}
	if (bytes !== release.artifactBytes) {
		fail(
			"agentos_apps_artifact_manifest_invalid",
			`artifact ${release.release} failed its byte count`,
		);
	}
	const artifact = new Uint8Array(Buffer.concat(chunks, bytes));
	if (
		createHash("sha256").update(artifact).digest("hex") !== release.artifactHash
	) {
		fail(
			"agentos_apps_artifact_hash_mismatch",
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
			"agentos_apps_public_endpoint_required",
			"RIVET_PUBLIC_ENDPOINT is required to run app actors when RIVET_ENDPOINT contains credentials",
		);
	}
	if (endpoint.username) endpoint.username = release.namespace;
	return endpoint.toString();
}

function textFile(
	files: Record<string, Uint8Array>,
	path: string,
): string | undefined {
	const content = files[path];
	return content ? new TextDecoder().decode(content) : undefined;
}

function packageExport(value: unknown): string | undefined {
	if (typeof value === "string") return value;
	if (!value || typeof value !== "object" || Array.isArray(value)) return;
	const object = value as Record<string, unknown>;
	return (
		packageExport(object["."]) ??
		packageExport(object.import) ??
		packageExport(object.default)
	);
}

function installPackageJson(
	files: Record<string, Uint8Array>,
	plan: BuildPlan,
): Uint8Array | undefined {
	const source = textFile(files, "package.json");
	if (!source || !plan.usesRivetKit) return files["package.json"];
	const value = JSON.parse(source) as {
		dependencies?: Record<string, unknown>;
		devDependencies?: Record<string, unknown>;
	};
	for (const dependencies of [value.dependencies, value.devDependencies]) {
		if (dependencies) delete dependencies.rivetkit;
	}
	return new TextEncoder().encode(JSON.stringify(value));
}

function validateDeployment(
	input: PreparedDeployAppInput,
	limits: { maxSourceBytes: number; maxFiles: number; maxDependencies: number },
): BuildPlan {
	if (!input || typeof input !== "object" || !input.files) {
		fail(
			"agentos_apps_invalid_files",
			"deployApp files must contain the complete application tree",
		);
	}
	const files = Object.entries(input.files);
	if (files.length === 0 || files.length > limits.maxFiles) {
		fail(
			"agentos_apps_file_count_limit",
			`deployment must contain between 1 and ${limits.maxFiles} files`,
			{ observed: files.length, limit: limits.maxFiles },
		);
	}
	let sourceBytes = 0;
	const normalizedFiles: Record<string, Uint8Array> = {};
	for (const [path, content] of files) {
		const normalizedPath = normalizeAppPath(path);
		if (normalizedFiles[normalizedPath]) {
			fail(
				"agentos_apps_duplicate_file_path",
				`multiple deployment paths normalize to ${normalizedPath}`,
			);
		}
		if (!(content instanceof Uint8Array)) {
			fail(
				"agentos_apps_invalid_file",
				`deployment file ${path} must be a string or Uint8Array`,
			);
		}
		normalizedFiles[normalizedPath] = content;
		sourceBytes += content.byteLength;
	}
	if (sourceBytes > limits.maxSourceBytes) {
		fail(
			"agentos_apps_source_limit",
			`deployment source is ${sourceBytes} bytes, exceeding maxSourceBytes ${limits.maxSourceBytes}`,
			{ observed: sourceBytes, limit: limits.maxSourceBytes },
		);
	}
	input.files = normalizedFiles;
	const packageJsonSource = textFile(normalizedFiles, "package.json");
	if (!packageJsonSource) {
		fail(
			"agentos_apps_entrypoint_not_found",
			"direct applications must contain package.json and a server entrypoint",
		);
	}
	let packageJson: {
		dependencies?: unknown;
		devDependencies?: unknown;
		scripts?: { build?: unknown };
		exports?: unknown;
		main?: unknown;
	};
	try {
		packageJson = JSON.parse(packageJsonSource);
	} catch (error) {
		fail(
			"agentos_apps_invalid_package_json",
			"package.json is not valid JSON",
			{ error: String(error) },
		);
	}
	const dependencyMaps = [
		packageJson.dependencies,
		packageJson.devDependencies,
	].filter(
		(value): value is Record<string, unknown> =>
			typeof value === "object" && value !== null && !Array.isArray(value),
	);
	const dependencyCount = dependencyMaps.reduce(
		(count, dependencies) => count + Object.keys(dependencies).length,
		0,
	);
	if (dependencyCount > limits.maxDependencies) {
		fail(
			"agentos_apps_dependency_limit",
			`deployment has ${dependencyCount} dependencies, exceeding maxDependencies ${limits.maxDependencies}`,
			{ observed: dependencyCount, limit: limits.maxDependencies },
		);
	}
	const usesRivetKit = dependencyMaps.some(
		(dependencies) => typeof dependencies.rivetkit === "string",
	);
	const build = typeof packageJson.scripts?.build === "string";
	const declared =
		packageExport(packageJson.exports) ??
		(typeof packageJson.main === "string" ? packageJson.main : undefined);
	if (declared) {
		return {
			entrypoint: normalizeAppPath(declared),
			build,
			dependencyCount,
			hasLockfile: Boolean(normalizedFiles["package-lock.json"]),
			usesRivetKit,
		};
	}
	for (const candidate of [
		"src/index.mjs",
		"src/index.js",
		"index.mjs",
		"index.js",
	]) {
		if (normalizedFiles[candidate]) {
			return {
				entrypoint: candidate,
				build,
				dependencyCount,
				hasLockfile: Boolean(normalizedFiles["package-lock.json"]),
				usesRivetKit,
			};
		}
	}
	fail(
		"agentos_apps_entrypoint_not_found",
		"could not infer a direct server entrypoint",
	);
}

function normalizeRegions(
	regions: string[] | undefined,
	fallbackRegion: string,
	maxRegions: number,
): string[] {
	const unique = [...new Set(regions ?? [fallbackRegion || "default"])];
	if (unique.length === 0 || unique.length > maxRegions) {
		fail(
			"agentos_apps_invalid_regions",
			`an app must have between 1 and ${maxRegions} regions`,
			{ maxRegions },
		);
	}
	for (const region of unique) {
		if (!/^[a-z0-9][a-z0-9-]{0,62}$/.test(region)) {
			fail(
				"agentos_apps_invalid_region",
				`invalid region ${JSON.stringify(region)}`,
			);
		}
	}
	return unique;
}

function boundedOutput(value: string, maximum: number): string {
	const bytes = Buffer.from(value);
	if (bytes.byteLength <= maximum) return value;
	return `${bytes.subarray(0, maximum).toString("utf8")}\n[truncated at ${maximum} bytes]`;
}

function emitBuildOutput(
	appId: string,
	release: string,
	result: Pick<ExecResult, "stdout" | "stderr">,
): void {
	for (const stream of ["stdout", "stderr"] as const) {
		const decoder = new DynamicAppsLogLineDecoder((message, truncated) =>
			emitDynamicAppsLog({
				level: stream === "stdout" ? "info" : "error",
				source: "build",
				message,
				appId,
				release,
				stream,
				...(truncated ? { metadata: { truncated: true } } : {}),
			}),
		);
		decoder.write(Buffer.from(result[stream]));
		decoder.end();
	}
}

function throwCommandFailure(
	kind: "install" | "build" | "pack",
	command: string,
	result: ExecResult,
	maxOutputBytes: number,
): never {
	fail(
		`agentos_apps_${kind}_failed`,
		`${command} failed with exit code ${result.exitCode}`,
		{
			exitCode: result.exitCode,
			stdout: boundedOutput(result.stdout, maxOutputBytes),
			stderr: boundedOutput(result.stderr, maxOutputBytes),
		},
	);
}

async function buildRelease(
	c: AnyActorContext,
	input: PreparedDeployAppInput,
	plan: BuildPlan,
	release: string,
	config: {
		createBuildVm: () => Promise<BuildHandle>;
		buildTimeoutMs: number;
		maxResponseBytes: number;
		maxBuildOutputBytes: number;
		maxBuildArtifactBytes: number;
		artifactCache?: {
			get(release: string): Promise<Uint8Array | undefined>;
			put(release: string, artifact: Uint8Array): Promise<void>;
		};
	},
): Promise<{ hash: string; size: number; bytes: Uint8Array }> {
	const cached = await config.artifactCache?.get(release);
	if (cached) {
		if (cached.byteLength > config.maxBuildArtifactBytes) {
			fail(
				"agentos_apps_build_artifact_size_limit",
				`cached artifact exceeds ${config.maxBuildArtifactBytes} bytes`,
			);
		}
		return {
			hash: createHash("sha256").update(cached).digest("hex"),
			size: cached.byteLength,
			bytes: cached,
		};
	}
	const build = await config.createBuildVm();
	const startedAt = performance.now();
	const phase = (name: string) => {
		const elapsedMs = performance.now() - startedAt;
		c.log.info({
			msg: "Dynamic Apps build phase completed",
			release,
			phase: name,
			elapsedMs,
		});
		emitDynamicAppsLog({
			level: "info",
			source: "build",
			message: "Dynamic Apps build phase completed",
			appId: input.appId,
			release,
			metadata: { phase: name, elapsedMs },
		});
	};
	let buildError: unknown;
	try {
		const files = Object.entries(input.files).map(([path, content]) => ({
			path: `/workspace/${normalizeAppPath(path)}`,
			content:
				path === "package.json"
					? (installPackageJson(input.files, plan) ?? content)
					: content,
		}));
		files.push({
			path: "/workspace/direct-runner.mjs",
			content: new TextEncoder().encode(
				directRunnerSource({
					entrypoint: plan.entrypoint,
					release,
					maxResponseBytes: config.maxResponseBytes,
					usesRivetKit: plan.usesRivetKit,
				}),
			),
		});
		if (plan.usesRivetKit) {
			files.push({
				path: "/workspace/actor-runner.mjs",
				content: new TextEncoder().encode(actorRunnerSource(plan.entrypoint)),
			});
		}
		const writes = await build.writeFiles(files);
		const failedWrite = writes.find((entry) => !entry.success);
		if (failedWrite) {
			fail(
				"agentos_apps_build_write_failed",
				`failed to write build input ${failedWrite.path}: ${failedWrite.error ?? "unknown error"}`,
				{ path: failedWrite.path, error: failedWrite.error },
			);
		}
		const installArgs = [
			plan.hasLockfile && !plan.usesRivetKit ? "ci" : "install",
			"--install-strategy=shallow",
			"--include=dev",
			"--omit=optional",
			"--omit=peer",
			"--legacy-peer-deps",
			"--no-audit",
			"--no-fund",
			"--maxsockets=16",
			"--loglevel=error",
		];
		const install = await build.execArgv("npm", installArgs, {
			cwd: "/workspace",
			env: { NODE_ENV: "development", NPM_CONFIG_PRODUCTION: "false" },
			timeout: config.buildTimeoutMs,
			captureStdio: true,
		});
		emitBuildOutput(input.appId, release, install);
		if (install.exitCode !== 0) {
			throwCommandFailure(
				"install",
				`npm ${installArgs[0]}`,
				install,
				config.maxBuildOutputBytes,
			);
		}
		phase("dependencies_installed");
		if (plan.build) {
			const result = await build.execArgv("npm", ["run", "build"], {
				cwd: "/workspace",
				timeout: config.buildTimeoutMs,
				captureStdio: true,
			});
			emitBuildOutput(input.appId, release, result);
			if (result.exitCode !== 0) {
				throwCommandFailure(
					"build",
					"npm run build",
					result,
					config.maxBuildOutputBytes,
				);
			}
			phase("application_built");
		}
		const prune = await build.execArgv(
			"npm",
			[
				"prune",
				"--omit=dev",
				"--omit=optional",
				"--omit=peer",
				"--legacy-peer-deps",
			],
			{
				cwd: "/workspace",
				timeout: config.buildTimeoutMs,
				captureStdio: true,
			},
		);
		emitBuildOutput(input.appId, release, prune);
		if (prune.exitCode !== 0) {
			throwCommandFailure(
				"install",
				"npm prune --omit=dev --omit=optional",
				prune,
				config.maxBuildOutputBytes,
			);
		}
		const nativeAddonCheck = await build.execArgv(
			"node",
			[
				"-e",
				'const fs=require("node:fs"); const path=require("node:path"); const found=[]; const walk=(p)=>{if(!fs.existsSync(p))return; for(const e of fs.readdirSync(p,{withFileTypes:true})){const q=path.join(p,e.name); if(e.isDirectory())walk(q); else if(e.name.endsWith(".node"))found.push(q)}}; walk("node_modules"); if(found.length){console.error(found.slice(0,32).join("\\n")); process.exit(42)}',
			],
			{
				cwd: "/workspace",
				timeout: config.buildTimeoutMs,
				captureStdio: true,
			},
		);
		emitBuildOutput(input.appId, release, nativeAddonCheck);
		if (nativeAddonCheck.exitCode === 42) {
			fail(
				"agentos_apps_native_addon_unsupported",
				"application contains native Node addons",
				{
					files: boundedOutput(
						nativeAddonCheck.stderr,
						config.maxBuildOutputBytes,
					),
				},
			);
		}
		if (nativeAddonCheck.exitCode !== 0) {
			throwCommandFailure(
				"build",
				"native addon scan",
				nativeAddonCheck,
				config.maxBuildOutputBytes,
			);
		}
		const directConfigPath = "/workspace/.agentos-app-direct-build.json";
		const configWrites = await build.writeFiles([
			{
				path: directConfigPath,
				content: JSON.stringify({
					version: release,
					workspace: "/workspace",
					release: "/release/direct",
					entrypoint: "direct-runner.mjs",
					sourceFiles: Object.keys(input.files),
					usesRivetKit: plan.usesRivetKit,
					directAgentOs: true,
					maxOutputBytes: config.maxBuildArtifactBytes,
					maxOutputFiles: DEFAULT_MAX_BUILD_ARTIFACT_FILES,
					maxFileBytes: DEFAULT_MAX_BUILD_ARTIFACT_FILE_BYTES,
				}),
			},
		]);
		const failedConfigWrite = configWrites.find((entry) => !entry.success);
		if (failedConfigWrite) {
			fail(
				"agentos_apps_build_write_failed",
				`failed to write Apps builder input ${failedConfigWrite.path}`,
			);
		}
		const directBundle = await build.execArgv(
			"node",
			["/opt/agentos/bin/apps-builder", directConfigPath],
			{
				cwd: "/workspace",
				timeout: config.buildTimeoutMs,
				captureStdio: true,
			},
		);
		emitBuildOutput(input.appId, release, directBundle);
		if (directBundle.exitCode !== 0) {
			throwCommandFailure(
				"build",
				"apps-builder (direct)",
				directBundle,
				config.maxBuildOutputBytes,
			);
		}
		if (plan.usesRivetKit) {
			const actorConfigPath = "/workspace/.agentos-app-actor-build.json";
			const actorConfigWrite = await build.writeFiles([
				{
					path: actorConfigPath,
					content: JSON.stringify({
						version: release,
						workspace: "/workspace",
						release: "/release/actor",
						entrypoint: "actor-runner.mjs",
						sourceFiles: Object.keys(input.files),
						usesRivetKit: true,
						platformRivetKit: true,
						maxOutputBytes: config.maxBuildArtifactBytes,
						maxOutputFiles: DEFAULT_MAX_BUILD_ARTIFACT_FILES,
						maxFileBytes: DEFAULT_MAX_BUILD_ARTIFACT_FILE_BYTES,
					}),
				},
			]);
			if (actorConfigWrite.some((entry) => !entry.success)) {
				fail(
					"agentos_apps_build_write_failed",
					"failed to write actor Apps builder input",
				);
			}
			const actorBundle = await build.execArgv(
				"node",
				["/opt/agentos/bin/apps-builder", actorConfigPath],
				{
					cwd: "/workspace",
					timeout: config.buildTimeoutMs,
					captureStdio: true,
				},
			);
			emitBuildOutput(input.appId, release, actorBundle);
			if (actorBundle.exitCode !== 0) {
				throwCommandFailure(
					"build",
					"apps-builder (actor)",
					actorBundle,
					config.maxBuildOutputBytes,
				);
			}
		}
		phase("release_bundled");
		const validation = await build.execArgv(
			"node",
			[
				"-e",
				`import("/release/${DIRECT_BUNDLE_PATH}").then((module)=>{if(module.dynamicAppMetadata?.format!==${JSON.stringify(DIRECT_RUNTIME_FORMAT)}||typeof module.dispatch!=="function") throw new TypeError("invalid direct app handler")}).catch((error)=>{console.error(error);process.exitCode=1})`,
			],
			{
				cwd: "/release",
				timeout: config.buildTimeoutMs,
				captureStdio: true,
			},
		);
		emitBuildOutput(input.appId, release, validation);
		if (validation.exitCode !== 0) {
			fail(
				"agentos_apps_invalid_handler",
				"application entrypoint could not be imported as a direct fetch handler",
				{
					stderr: boundedOutput(validation.stderr, config.maxBuildOutputBytes),
				},
			);
		}
		const rootManifestWrite = await build.writeFiles([
			{
				path: "/release/agentos-package.json",
				content: JSON.stringify({ name: "agentos-app", version: release }),
			},
		]);
		if (rootManifestWrite.some((entry) => !entry.success)) {
			fail(
				"agentos_apps_build_write_failed",
				"failed to write root application package manifest",
			);
		}
		phase("release_validated");
		const pack = await build.execArgv(
			"tar",
			[
				"--sort=name",
				"--mtime=@0",
				"--owner=0",
				"--group=0",
				"--numeric-owner",
				"-cf",
				build.artifactGuestPath,
				".",
			],
			{
				cwd: "/release",
				timeout: config.buildTimeoutMs,
				captureStdio: true,
			},
		);
		emitBuildOutput(input.appId, release, pack);
		if (pack.exitCode !== 0) {
			throwCommandFailure("pack", "tar", pack, config.maxBuildOutputBytes);
		}
		phase("release_archived");
		const archiveSize = await build.artifactSize();
		if (
			!Number.isSafeInteger(archiveSize) ||
			archiveSize < 0 ||
			archiveSize > config.maxBuildArtifactBytes
		) {
			fail(
				"agentos_apps_build_artifact_size_limit",
				`built application archive is ${archiveSize} bytes, limit is ${config.maxBuildArtifactBytes}`,
			);
		}
		const sourceTar = Buffer.from(await build.readArtifact());
		if (sourceTar.byteLength !== archiveSize) {
			fail(
				"agentos_apps_build_artifact_truncated",
				`build artifact contained ${sourceTar.byteLength} bytes, expected ${archiveSize}`,
			);
		}
		const packed = packAospkgFromTarBytes(sourceTar).bytes;
		const artifactHash = createHash("sha256").update(packed).digest("hex");
		await config.artifactCache?.put(release, new Uint8Array(packed));
		return {
			hash: artifactHash,
			size: packed.byteLength,
			bytes: new Uint8Array(packed),
		};
	} catch (error) {
		buildError = error;
		throw error;
	} finally {
		await build.dispose().catch((disposeError) => {
			emitDynamicAppsLog({
				level: "error",
				source: "build",
				message: "failed to dispose Dynamic Apps build VM",
				appId: input.appId,
				release,
			});
			if (!buildError) throw disposeError;
			c.log.error({
				msg: "failed to dispose Dynamic Apps build VM after build failure",
				disposeError,
			});
		});
	}
}

function createBuildVmFactory(
	maxBuildArtifactBytes: number,
): () => Promise<BuildHandle> {
	const options: AgentOsOptions = {
		defaultSoftware: false,
		software: [sh, tar, appsBuilder],
		permissions: {
			fs: "allow",
			childProcess: "allow",
			process: "allow",
			env: "allow",
			network: "allow",
		},
		limits: {
			tls: { maxBufferedBytes: 16 * 1024 * 1024 },
			jsRuntime: { v8HeapLimitMb: 1_024 },
			resources: {
				maxProcesses: 64,
				maxOpenFds: 2_048,
				maxPreadBytes: 15 * 1024 * 1024,
				maxFdWriteBytes: 16 * 1024 * 1024,
				maxSocketBufferedBytes: 16 * 1024 * 1024,
				maxFilesystemBytes: Math.max(
					DEFAULT_MAX_BUILD_FILESYSTEM_BYTES,
					maxBuildArtifactBytes * 2,
				),
			},
		},
	};
	return async () => {
		const outputDirectory = await mkdtemp(
			join(tmpdir(), "agentos-apps-build-output-"),
		);
		await chmod(outputDirectory, 0o777);
		const artifactGuestPath = "/agentos-app-output/agentos-app.tar";
		const artifactHostPath = join(outputDirectory, "agentos-app.tar");
		let vm: AgentOs;
		try {
			vm = await AgentOs.create({
				...options,
				mounts: [
					{
						path: "/agentos-app-output",
						readOnly: false,
						plugin: createHostDirBackend({
							hostPath: outputDirectory,
							readOnly: false,
						}),
					},
				],
			});
		} catch (error) {
			await rm(outputDirectory, { recursive: true, force: true });
			throw error;
		}
		return {
			artifactGuestPath,
			writeFiles: (...args) => vm.writeFiles(...args),
			execArgv: (...args) => vm.execArgv(...args),
			artifactSize: async () => (await stat(artifactHostPath)).size,
			readArtifact: async () =>
				new Uint8Array(await readFile(artifactHostPath)),
			dispose: async () => {
				const results = await Promise.allSettled([
					vm.dispose(),
					rm(outputDirectory, { recursive: true, force: true }),
				]);
				const failures = results.flatMap((result) =>
					result.status === "rejected" ? [result.reason] : [],
				);
				if (failures.length > 0) {
					throw new AggregateError(
						failures,
						"failed to dispose Dynamic Apps build VM output",
					);
				}
			},
		};
	};
}

export function createAppsActors(
	options: {
		artifactCache?: {
			get(release: string): Promise<Uint8Array | undefined>;
			put(release: string, artifact: Uint8Array): Promise<void>;
		};
	} = {},
): DynamicAppsActors {
	const createBuildVm = createBuildVmFactory(DEFAULT_MAX_BUILD_ARTIFACT_BYTES);
	const forwardActorRequest = async (
		c: AnyActorContext,
		request: Request,
	): Promise<Response> => {
		const callbackPath = normalizeActorCallbackPath(request);
		if (!callbackPath) return new Response("Not Found", { status: 404 });
		const state = c.state as AppState;
		const release = state.activeRelease
			? await getStoredRelease(c.db, state.activeRelease)
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
				appId: c.key[0] ?? "unknown",
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
	const agentOSAppsApp = actor({
		options: { actionTimeout: DEFAULT_BUILD_TIMEOUT_MS + 60_000 },
		db: db({ onMigrate: migrateAppsTables }),
		onRequest: forwardActorRequest,
		createState: (): AppState => ({
			activeRelease: null,
			namespace: null,
			revision: 0,
			cloudNamespace: null,
			runnerToken: null,
			publicToken: null,
		}),
		actions: {
			deploy: async (
				c: AnyActorContext,
				input: PreparedDeployAppInput,
			): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }> =>
				c.keepAwake(
					serialized(`app:${c.actorId}`, async () => {
						const appId = c.key[0];
						if (!appId || c.key.length !== 1 || input.appId !== appId) {
							fail(
								"agentos_apps_app_id_mismatch",
								"deployApp appId must match the stable application actor key",
								{ appId: input.appId, actorKey: c.key },
							);
						}
						const plan = validateDeployment(input, {
							maxSourceBytes: DEFAULT_MAX_SOURCE_BYTES,
							maxFiles: DEFAULT_MAX_FILES,
							maxDependencies: DEFAULT_MAX_DEPENDENCIES,
						});
						const state = c.state as AppState;
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
						const regions = normalizeRegions(
							input.regions,
							c.region,
							DEFAULT_MAX_REGIONS,
						);
						const scaling = normalizeScaling(input.scaling);
						const releaseId = canonicalDeploymentHash({
							files: input.files,
							entrypoint: plan.entrypoint,
							build: plan.build,
							packagingIdentity: [
								`apps-builder@${appsBuilderVersion}`,
								`manifest@${appBundleManifestVersion}`,
								"direct@2",
								`actors@${plan.usesRivetKit ? 1 : 0}`,
								"esbuild-wasm@0.27.4",
							].join(";"),
							deploymentIdentity: JSON.stringify({
								regions,
								scaling,
								namespace: runtime.namespace,
								runtime: {
									endpoint: runtime.endpoint,
									pool: runtime.pool,
								},
								usesRivetKit: plan.usesRivetKit,
							}),
						});
						const releasesBefore = await listStoredReleases(c.db);
						let release = await getStoredRelease(c.db, releaseId);
						const callbackSecret = plan.usesRivetKit
							? release?.callbackSecret ||
								releasesBefore.find((candidate) => candidate.callbackSecret)
									?.callbackSecret ||
								randomUUID()
							: "";
						if (!release || release.status !== "ready") {
							const createdAt = release?.createdAt ?? Date.now();
							await deleteArtifactChunksBatched(c.db, releaseId);
							await c.db.execute(
								`INSERT INTO agentos_apps_releases (
									release_id, created_at, status, entrypoint,
									artifact_hash, artifact_bytes, build_error,
									regions_json, scaling_json, namespace, envoy_version,
									runtime_endpoint, runtime_pool, callback_secret,
									uses_rivetkit
								) VALUES (?, ?, 'building', ?, '', 0, NULL, ?, ?, ?, 1, ?, ?, ?, ?)
								ON CONFLICT(release_id) DO UPDATE SET
									status = 'building', entrypoint = excluded.entrypoint,
									artifact_hash = '', artifact_bytes = 0, build_error = NULL,
									regions_json = excluded.regions_json,
									scaling_json = excluded.scaling_json,
									namespace = excluded.namespace,
									runtime_endpoint = excluded.runtime_endpoint,
									runtime_pool = excluded.runtime_pool,
									callback_secret = excluded.callback_secret,
									uses_rivetkit = excluded.uses_rivetkit`,
								releaseId,
								createdAt,
								DIRECT_ENTRYPOINT,
								JSON.stringify(regions),
								JSON.stringify(scaling),
								runtime.namespace,
								runtime.endpoint,
								runtime.pool,
								callbackSecret,
								plan.usesRivetKit ? 1 : 0,
							);
							await deleteReleaseFilesBatched(c.db, releaseId);
							await persistReleaseFilesBatched(c.db, releaseId, input.files);
							try {
								const artifact = await buildRelease(c, input, plan, releaseId, {
									createBuildVm,
									buildTimeoutMs: DEFAULT_BUILD_TIMEOUT_MS,
									maxResponseBytes: DEFAULT_MAX_RESPONSE_BYTES,
									maxBuildOutputBytes: DEFAULT_MAX_BUILD_OUTPUT_BYTES,
									maxBuildArtifactBytes: DEFAULT_MAX_BUILD_ARTIFACT_BYTES,
									artifactCache: options.artifactCache,
								});
								const chunkCount = Math.ceil(
									artifact.size / ARTIFACT_CHUNK_BYTES,
								);
								if (chunkCount > MAX_ARTIFACT_CHUNKS) {
									fail(
										"agentos_apps_artifact_chunk_limit",
										`artifact requires ${chunkCount} chunks`,
									);
								}
								await deleteArtifactChunksBatched(c.db, releaseId);
								for (let index = 0; index < chunkCount; index += 1) {
									const chunk = artifact.bytes.slice(
										index * ARTIFACT_CHUNK_BYTES,
										(index + 1) * ARTIFACT_CHUNK_BYTES,
									);
									await c.db.execute(
										`INSERT INTO agentos_apps_artifact_chunks
										 (release_id, chunk_index, content, byte_length)
										 VALUES (?, ?, ?, ?)`,
										releaseId,
										index,
										chunk,
										chunk.byteLength,
									);
								}
								const totals = await c.db.execute<{
									bytes: number;
									chunks: number;
								}>(
									`SELECT COALESCE(SUM(byte_length), 0) AS bytes,
									 COUNT(*) AS chunks FROM agentos_apps_artifact_chunks
									 WHERE release_id = ?`,
									releaseId,
								);
								if (
									Number(totals[0]?.bytes ?? 0) !== artifact.size ||
									Number(totals[0]?.chunks ?? 0) !== chunkCount
								) {
									fail(
										"agentos_apps_artifact_persist_mismatch",
										"persisted artifact chunks failed length verification",
									);
								}
								await c.db.execute(
									`UPDATE agentos_apps_releases SET status = 'ready',
									 artifact_hash = ?, artifact_bytes = ?, build_error = NULL
									 WHERE release_id = ?`,
									artifact.hash,
									artifact.size,
									releaseId,
								);
							} catch (error) {
								await deleteArtifactChunksBatched(c.db, releaseId);
								await c.db.execute(
									`UPDATE agentos_apps_releases SET status = 'failed',
									 build_error = ? WHERE release_id = ?`,
									error instanceof Error ? error.message : String(error),
									releaseId,
								);
								throw error;
							}
							release = await getStoredRelease(c.db, releaseId);
						} else {
							await c.db.execute(
								`UPDATE agentos_apps_releases SET regions_json = ?,
									scaling_json = ?, runtime_endpoint = ?, runtime_pool = ?,
									callback_secret = ?, uses_rivetkit = ?
									WHERE release_id = ?`,
								JSON.stringify(regions),
								JSON.stringify(scaling),
								runtime.endpoint,
								runtime.pool,
								callbackSecret,
								plan.usesRivetKit ? 1 : 0,
								releaseId,
							);
							release = await getStoredRelease(c.db, releaseId);
						}
						if (!release || release.status !== "ready") {
							fail(
								"agentos_apps_artifact_not_ready",
								"built artifact was not ready for activation",
							);
						}
						const previousRelease = state.activeRelease;
						state.activeRelease = releaseId;
						try {
							if (release.usesRivetKit) {
								actorPublicEndpoint(release, state);
								if (
									runtime.endpoint.replace(/\/$/, "") !==
									release.runtimeEndpoint.replace(/\/$/, "")
								) {
									fail(
										"agentos_apps_runtime_changed",
										"the app actor Rivet endpoint does not match the deployment runtime",
										{
											expected: runtime.endpoint,
											received: release.runtimeEndpoint,
										},
									);
								}
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
							}
						} catch (error) {
							state.activeRelease = previousRelease;
							c.log.error({
								msg: "Dynamic App actor runner configuration failed",
								release: release.release,
								error: error instanceof Error ? error.stack : String(error),
							});
							if (error instanceof DynamicAppsError) {
								fail(error.code, error.message, error.metadata);
							}
							throw error;
						}
						state.revision += 1;
						const activatedAt = Date.now();
						c.broadcast("releaseActivated", {
							revision: state.revision,
							release: releaseId,
							artifactHash: release.artifactHash,
							activatedAt,
						});
						const releases = await listStoredReleases(c.db);
						const removable = releases
							.filter((candidate) => candidate.release !== releaseId)
							.sort((a, b) => a.createdAt - b.createdAt);
						let retained = releases.length;
						while (retained > DEFAULT_MAX_VERSIONS) {
							const candidate = removable.shift();
							if (!candidate) break;
							await deleteArtifactChunksBatched(c.db, candidate.release);
							await deleteReleaseFilesBatched(c.db, candidate.release);
							await c.db.execute(
								"DELETE FROM agentos_apps_releases WHERE release_id = ?",
								candidate.release,
							);
							retained -= 1;
						}
						return {
							appId,
							release: releaseId,
							endpoint: runtime.endpoint,
							namespace: runtime.namespace,
							pool: runtime.pool,
							...(runtime.publicToken ? { token: runtime.publicToken } : {}),
							regions,
							appActorId: c.actorId,
							usesRivetKit: release.usesRivetKit,
						};
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
						"agentos_apps_invalid_app_id",
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
						"agentos_apps_not_deployed",
						"app has no active direct release; call app.deploy() first",
					);
				}
				if (requestedRegion && !release.regions.includes(requestedRegion)) {
					fail(
						"agentos_apps_region_not_deployed",
						`app is not deployed in requested region ${requestedRegion}`,
						{ requestedRegion, regions: release.regions },
					);
				}
				const region = requestedRegion ?? release.regions[0];
				if (!region) fail("agentos_apps_no_region", "active app has no region");
				return {
					appId,
					release: release.release,
					region,
					regions: release.regions,
					revision: state.revision,
					artifactHash: release.artifactHash,
					artifactBytes: release.artifactBytes,
					entrypoint: DIRECT_ENTRYPOINT,
					namespace: release.namespace,
					scaling: release.scaling,
					maxRequestBytes: DEFAULT_MAX_REQUEST_BYTES,
					maxResponseBytes: DEFAULT_MAX_RESPONSE_BYTES,
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
						"agentos_apps_artifact_not_ready",
						`artifact for release ${releaseId} is not ready`,
					);
				}
				const rows = await c.db.execute<{ chunks: number; bytes: number }>(
					`SELECT COUNT(*) AS chunks,
					 COALESCE(SUM(byte_length), 0) AS bytes
					 FROM agentos_apps_artifact_chunks WHERE release_id = ?`,
					releaseId,
				);
				const chunks = Number(rows[0]?.chunks ?? 0);
				const bytes = Number(rows[0]?.bytes ?? 0);
				if (chunks > MAX_ARTIFACT_CHUNKS || bytes !== release.artifactBytes) {
					fail(
						"agentos_apps_artifact_manifest_invalid",
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
						"agentos_apps_invalid_artifact_chunk",
						`artifact chunk index must be between 0 and ${MAX_ARTIFACT_CHUNKS - 1}`,
					);
				}
				const rows = await c.db.execute<{
					content: Uint8Array;
					byte_length: number;
				}>(
					`SELECT content, byte_length FROM agentos_apps_artifact_chunks
					 WHERE release_id = ? AND chunk_index = ?`,
					releaseId,
					index,
				);
				const row = rows[0];
				if (!row) {
					fail(
						"agentos_apps_artifact_chunk_not_found",
						`artifact chunk ${index} for release ${releaseId} was not found`,
					);
				}
				const content = new Uint8Array(row.content);
				if (
					content.byteLength !== Number(row.byte_length) ||
					content.byteLength > ARTIFACT_CHUNK_BYTES
				) {
					fail(
						"agentos_apps_artifact_chunk_invalid",
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
	return { agentOSAppsApp };
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
