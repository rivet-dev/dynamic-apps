import { createHash } from "node:crypto";
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
import { extractAospkgTextFile } from "./artifact.js";
import { DynamicAppsError } from "./errors.js";
import {
	ACTOR_BUNDLE_PATH,
	actorRunnerSource,
	canonicalDeploymentHash,
	DIRECT_BUNDLE_PATH,
	DIRECT_ENTRYPOINT,
	DIRECT_RUNTIME_FORMAT,
	directRunnerSource,
	normalizeAppPath,
} from "./runtime.js";
import { validateAppId } from "./source.js";
import type {
	BuildAppReleaseInput,
	BuildArtifactCache,
	BuildConfig,
	BuiltAppRelease,
	DynamicAppsLogger,
} from "./types.js";

export const DEFAULT_MAX_SOURCE_BYTES = 4 * 1024 * 1024;
export const DEFAULT_MAX_FILES = 2_000;
export const DEFAULT_MAX_DEPENDENCIES = 256;
export const DEFAULT_BUILD_TIMEOUT_MS = 15 * 60_000;
export const DEFAULT_MAX_RESPONSE_BYTES = 4 * 1024 * 1024;
export const DEFAULT_MAX_BUILD_OUTPUT_BYTES = 2 * 1024 * 1024;
export const DEFAULT_MAX_BUILD_ARTIFACT_BYTES = 64 * 1024 * 1024;
export const DEFAULT_MAX_BUILD_ARTIFACT_FILES = 4_096;
export const DEFAULT_MAX_BUILD_ARTIFACT_FILE_BYTES = 32 * 1024 * 1024;
export const DEFAULT_MAX_BUILD_FILESYSTEM_BYTES = 2 * 1024 * 1024 * 1024;

const DEFAULT_BUILD_CONFIG: BuildConfig = {
	maxSourceBytes: DEFAULT_MAX_SOURCE_BYTES,
	maxFiles: DEFAULT_MAX_FILES,
	maxDependencies: DEFAULT_MAX_DEPENDENCIES,
	buildTimeoutMs: DEFAULT_BUILD_TIMEOUT_MS,
	maxResponseBytes: DEFAULT_MAX_RESPONSE_BYTES,
	maxBuildOutputBytes: DEFAULT_MAX_BUILD_OUTPUT_BYTES,
	maxBuildArtifactBytes: DEFAULT_MAX_BUILD_ARTIFACT_BYTES,
	maxBuildArtifactFiles: DEFAULT_MAX_BUILD_ARTIFACT_FILES,
	maxBuildArtifactFileBytes: DEFAULT_MAX_BUILD_ARTIFACT_FILE_BYTES,
	maxBuildFilesystemBytes: DEFAULT_MAX_BUILD_FILESYSTEM_BYTES,
};

const NOOP_LOGGER: DynamicAppsLogger = {
	info() {},
	error() {},
};

export interface ExecResult {
	exitCode: number;
	stdout: string;
	stderr: string;
}

export interface BuildHandle {
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

export interface BuildPlan {
	entrypoint: string;
	build: boolean;
	dependencyCount: number;
	hasLockfile: boolean;
	usesRivetKit: boolean;
}

export function readBuildConfig(
	overrides: Partial<BuildConfig> = {},
): BuildConfig {
	const config = { ...DEFAULT_BUILD_CONFIG, ...overrides };
	for (const key of Object.keys(DEFAULT_BUILD_CONFIG) as Array<
		keyof BuildConfig
	>) {
		const value = config[key];
		const maximum = DEFAULT_BUILD_CONFIG[key];
		if (!Number.isSafeInteger(value) || value < 1 || value > maximum) {
			throw new DynamicAppsError(
				"dynamic_apps_invalid_config",
				`${key} must be an integer between 1 and ${maximum}`,
				{ name: key, maximum },
			);
		}
	}
	return config;
}

function fail(
	code: string,
	message: string,
	metadata?: Record<string, unknown>,
): never {
	throw new DynamicAppsError(code, message, metadata);
}

export function textFile(
	files: Record<string, Uint8Array>,
	path: string,
): string | undefined {
	const content = files[path];
	return content ? new TextDecoder().decode(content) : undefined;
}

export function packageExport(value: unknown): string | undefined {
	if (typeof value === "string") return value;
	if (!value || typeof value !== "object" || Array.isArray(value)) return;
	const object = value as Record<string, unknown>;
	return (
		packageExport(object["."]) ??
		packageExport(object.import) ??
		packageExport(object.default)
	);
}

export function installPackageJson(
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

export function validateDeployment(
	input: BuildAppReleaseInput,
	limits: Pick<BuildConfig, "maxSourceBytes" | "maxFiles" | "maxDependencies">,
): BuildPlan {
	if (!input || typeof input !== "object" || !input.files) {
		fail(
			"dynamic_apps_invalid_files",
			"deployApp files must contain the complete application tree",
		);
	}
	const files = Object.entries(input.files);
	if (files.length === 0 || files.length > limits.maxFiles) {
		fail(
			"dynamic_apps_file_count_limit",
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
				"dynamic_apps_duplicate_file_path",
				`multiple deployment paths normalize to ${normalizedPath}`,
			);
		}
		if (!(content instanceof Uint8Array)) {
			fail(
				"dynamic_apps_invalid_file",
				`deployment file ${path} must be a Uint8Array`,
			);
		}
		normalizedFiles[normalizedPath] = new Uint8Array(content);
		sourceBytes += content.byteLength;
	}
	if (sourceBytes > limits.maxSourceBytes) {
		fail(
			"dynamic_apps_source_limit",
			`deployment source is ${sourceBytes} bytes, exceeding maxSourceBytes ${limits.maxSourceBytes}`,
			{ observed: sourceBytes, limit: limits.maxSourceBytes },
		);
	}
	input.files = normalizedFiles;
	const packageJsonSource = textFile(normalizedFiles, "package.json");
	if (!packageJsonSource) {
		fail(
			"dynamic_apps_entrypoint_not_found",
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
			"dynamic_apps_invalid_package_json",
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
			"dynamic_apps_dependency_limit",
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
		"dynamic_apps_entrypoint_not_found",
		"could not infer a direct server entrypoint",
	);
}

export function boundedOutput(value: string, maximum: number): string {
	const bytes = Buffer.from(value);
	if (bytes.byteLength <= maximum) return value;
	return `${bytes.subarray(0, maximum).toString("utf8")}\n[truncated at ${maximum} bytes]`;
}

export function throwCommandFailure(
	kind: "install" | "build" | "pack",
	command: string,
	result: ExecResult,
	maxOutputBytes: number,
): never {
	fail(
		`dynamic_apps_${kind}_failed`,
		`${command} failed with exit code ${result.exitCode}`,
		{
			exitCode: result.exitCode,
			stdout: boundedOutput(result.stdout, maxOutputBytes),
			stderr: boundedOutput(result.stderr, maxOutputBytes),
		},
	);
}

function artifactResult(
	buildId: string,
	bytesInput: Uint8Array,
	usesRivetKit: boolean,
	maxBytes: number,
): BuiltAppRelease {
	if (!(bytesInput instanceof Uint8Array) || bytesInput.byteLength > maxBytes) {
		fail(
			"dynamic_apps_build_artifact_size_limit",
			`cached artifact exceeds ${maxBytes} bytes`,
		);
	}
	const bytes = new Uint8Array(bytesInput);
	extractAospkgTextFile(bytes, DIRECT_BUNDLE_PATH);
	if (usesRivetKit) extractAospkgTextFile(bytes, ACTOR_BUNDLE_PATH);
	return {
		buildId,
		artifact: {
			format: DIRECT_RUNTIME_FORMAT,
			entrypoint: DIRECT_ENTRYPOINT,
			hash: createHash("sha256").update(bytes).digest("hex"),
			bytes,
			byteLength: bytes.byteLength,
			usesRivetKit,
		},
	};
}

export async function buildAppRelease(
	input: BuildAppReleaseInput,
	options: {
		config?: Partial<BuildConfig>;
		artifactCache?: BuildArtifactCache;
		logger?: DynamicAppsLogger;
	} = {},
): Promise<BuiltAppRelease> {
	validateAppId(input.appId);
	const config = readBuildConfig(options.config);
	const normalizedInput = {
		appId: input.appId,
		files: { ...input.files },
	};
	const plan = validateDeployment(normalizedInput, config);
	const buildId = canonicalDeploymentHash({
		files: normalizedInput.files,
		entrypoint: plan.entrypoint,
		build: plan.build,
		packagingIdentity: [
			`apps-builder@${appsBuilderVersion}`,
			`manifest@${appBundleManifestVersion}`,
			"direct@2",
			`actors@${plan.usesRivetKit ? 1 : 0}`,
			"esbuild-wasm@0.27.4",
		].join(";"),
	});
	const cached = await options.artifactCache?.get(buildId);
	if (cached !== undefined) {
		return artifactResult(
			buildId,
			cached,
			plan.usesRivetKit,
			config.maxBuildArtifactBytes,
		);
	}
	const artifact = await buildRelease(
		normalizedInput,
		plan,
		buildId,
		config,
		options.logger ?? NOOP_LOGGER,
	);
	await options.artifactCache?.put(buildId, new Uint8Array(artifact));
	return artifactResult(
		buildId,
		artifact,
		plan.usesRivetKit,
		config.maxBuildArtifactBytes,
	);
}

async function buildRelease(
	input: BuildAppReleaseInput,
	plan: BuildPlan,
	buildId: string,
	config: BuildConfig,
	logger: DynamicAppsLogger,
): Promise<Uint8Array> {
	const build = await createBuildVmFactory(config)();
	const startedAt = performance.now();
	const phase = (name: string) =>
		logger.info({
			msg: "Dynamic Apps build phase completed",
			release: buildId,
			phase: name,
			elapsedMs: performance.now() - startedAt,
		});
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
					release: buildId,
					maxResponseBytes: config.maxResponseBytes,
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
				"dynamic_apps_build_write_failed",
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
		if (nativeAddonCheck.exitCode === 42) {
			fail(
				"dynamic_apps_native_addon_unsupported",
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
					version: buildId,
					workspace: "/workspace",
					release: "/release/direct",
					entrypoint: "direct-runner.mjs",
					sourceFiles: Object.keys(input.files),
					usesRivetKit: plan.usesRivetKit,
					directAgentOs: true,
					maxOutputBytes: config.maxBuildArtifactBytes,
					maxOutputFiles: config.maxBuildArtifactFiles,
					maxFileBytes: config.maxBuildArtifactFileBytes,
				}),
			},
		]);
		const failedConfigWrite = configWrites.find((entry) => !entry.success);
		if (failedConfigWrite) {
			fail(
				"dynamic_apps_build_write_failed",
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
						version: buildId,
						workspace: "/workspace",
						release: "/release/actor",
						entrypoint: "actor-runner.mjs",
						sourceFiles: Object.keys(input.files),
						usesRivetKit: true,
						directAgentOs: true,
						maxOutputBytes: config.maxBuildArtifactBytes,
						maxOutputFiles: config.maxBuildArtifactFiles,
						maxFileBytes: config.maxBuildArtifactFileBytes,
					}),
				},
			]);
			if (actorConfigWrite.some((entry) => !entry.success)) {
				fail(
					"dynamic_apps_build_write_failed",
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
		if (validation.exitCode !== 0) {
			fail(
				"dynamic_apps_invalid_handler",
				"application entrypoint could not be imported as a direct fetch handler",
				{
					stderr: boundedOutput(validation.stderr, config.maxBuildOutputBytes),
				},
			);
		}
		const rootManifestWrite = await build.writeFiles([
			{
				path: "/release/agentos-package.json",
				content: JSON.stringify({ name: "agentos-app", version: buildId }),
			},
		]);
		if (rootManifestWrite.some((entry) => !entry.success)) {
			fail(
				"dynamic_apps_build_write_failed",
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
				"dynamic_apps_build_artifact_size_limit",
				`built application archive is ${archiveSize} bytes, limit is ${config.maxBuildArtifactBytes}`,
			);
		}
		const sourceTar = Buffer.from(await build.readArtifact());
		if (sourceTar.byteLength !== archiveSize) {
			fail(
				"dynamic_apps_build_artifact_truncated",
				`build artifact contained ${sourceTar.byteLength} bytes, expected ${archiveSize}`,
			);
		}
		return new Uint8Array(packAospkgFromTarBytes(sourceTar).bytes);
	} catch (error) {
		buildError = error;
		throw error;
	} finally {
		await build.dispose().catch((disposeError) => {
			if (!buildError) throw disposeError;
			logger.error({
				msg: "failed to dispose Dynamic Apps build VM after build failure",
				disposeError,
			});
		});
	}
}

export function createBuildVmFactory(
	config: BuildConfig,
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
				maxFilesystemBytes: config.maxBuildFilesystemBytes,
			},
		},
	};
	return async () => {
		const outputDirectory = await mkdtemp(
			join(tmpdir(), "dynamic-apps-build-output-"),
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
