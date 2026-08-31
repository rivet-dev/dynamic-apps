# Dynamic Apps Core extraction and storage hooks

Status: implementation-ready specification  
Source working copy inspected: JJ change `zpnmumsrltvt`, August 31, 2026  
Target release: `0.12.0-rc.2` if it is still unused; otherwise the next unused
`0.12.0-rc.N`, identical for all three published packages  
Scope: extract build and direct serving into `@rivet-dev/dynamic-apps-core`; retain `@rivet-dev/dynamic-apps` as the Rivet actor-backed adapter

## Goal

Create a storage-independent package that lets a host build and serve Dynamic
Apps by providing exactly three release lifecycle hooks:

- `publishRelease`: durably publish and atomically activate a built release.
- `loadActiveRelease`: load the active release and its complete artifact.
- `watchActiveRelease`: receive invalidations when an app's active release may
  have changed.

The existing `@rivet-dev/dynamic-apps` package becomes a batteries-included
implementation of those hooks using its existing Rivet state actor and SQLite
tables. Its public runtime surface remains exactly `deployApp` and `appsRouter`.
App-defined RivetKit actors remain supported end to end.

The split must produce this dependency direction:

```text
@rivet-dev/dynamic-apps-builder
                 ^
                 |
@rivet-dev/dynamic-apps-core       rivetkit
                 ^                   ^
                 |                   |
                 +--- @rivet-dev/dynamic-apps
```

`dynamic-apps-core` must not import RivetKit, create actors, provision Rivet
namespaces, know about SQLite, or know that the default storage implementation
chunks artifacts.

Core may build and carry the optional `actor/main.mjs` payload and
`usesRivetKit` bit, but it never executes that payload. A higher-level adapter
such as `@rivet-dev/dynamic-apps` owns actor registration, callbacks, state, and
runner configuration.

## Non-goals

- Do not replace the existing agentOS VM build or Apps builder.
- Do not change the direct application handler contract.
- Do not change V8 isolate modes, memory limits, queue semantics, or context
  reset rules as part of this extraction.
- Do not remove app-defined actor support from the default package.
- Do not introduce a public `resolveRelease` hook. Resolution plus artifact
  download is one `loadActiveRelease` operation at the core boundary.
- Do not change the existing SQLite schema in this refactor.
- Do not make `watchActiveRelease` optional. A no-op watcher is only correct for
  a store where an `appId` is immutable for the lifetime of every serving
  process.
- Do not expose the actor adapter's manifest/chunk protocol from core.

## Resulting user APIs

### Storage-independent package

A user who owns storage uses `@rivet-dev/dynamic-apps-core`:

```ts
import { createDynamicApps } from "@rivet-dev/dynamic-apps-core";

const dynamicApps = createDynamicApps({
	async publishRelease(input) {
		// Store input.artifact.bytes, then atomically make this release active.
		return { appId: input.appId, release: input.buildId };
	},
	async loadActiveRelease(appId) {
		// Return metadata and complete bytes in one logical operation.
		return loadFromMyStore(appId);
	},
	async watchActiveRelease(appId, invalidate) {
		// Resolve only after the subscription is live.
		return subscribeToMyStore(appId, invalidate);
	},
});

await dynamicApps.deployApp({ appId: "hello", source });
server.route("/apps", dynamicApps.appsRouter);

await dynamicApps.dispose();
```

The instance owns its build configuration, executor, Hono router, subscriptions,
runtime cache, isolate pool, cleanup timer, and disposal lifecycle. There is no
core process singleton.

### Existing Rivet-backed package

Existing application code remains unchanged:

```ts
import { appsRouter, deployApp } from "@rivet-dev/dynamic-apps";

await deployApp({ appId: "hello", source });
server.route("/apps", appsRouter);
```

The root of `@rivet-dev/dynamic-apps` must continue to export exactly those two
runtime values. Do not add core factory exports to this package; users import
them from `@rivet-dev/dynamic-apps-core`.

## Exact core types

Create `packages/dynamic-apps-core/src/types.ts` with the following public
contract. JSDoc may be expanded, but names, required fields, and semantics must
not drift during implementation.

```ts
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
	| (DeployAppBase & {
			source: URL;
			files?: never;
	  })
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

export function createDynamicApps<TDeployment, TDeployOptions = undefined>(
	options: DynamicAppsOptions<TDeployment, TDeployOptions>,
): DynamicApps<TDeployment, TDeployOptions>;
```

Move the existing `IsolateMode` and `ExecutorConfig` declarations from
`packages/dynamic-apps/src/executor.ts` to this file without changing their
fields. Add and export these build support types:

```ts
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
```

`ReleaseArtifact.bytes` is owned by the receiver of a hook call. Core must pass
a fresh `Uint8Array` copy to `publishRelease`, and must copy bytes returned by
`loadActiveRelease` before caching them. This prevents a hook from mutating a
cached artifact after return.

### Hook guarantees

`publishRelease` has transaction-like semantics at the hook boundary:

1. It receives a completely built and verified immutable artifact.
2. It must make the artifact durable before making it active.
3. It must not resolve until the release is active and immediately readable by
   `loadActiveRelease` in the storage system's consistency model.
4. On failure, the previous active release must remain active.
5. It may return any deployment result type selected by the factory generic.

`loadActiveRelease` has these guarantees:

1. It returns the active release metadata and complete artifact bytes in one
   call at the core boundary.
2. It returns `undefined` only when the app has no active release.
3. `artifact.byteLength` equals `artifact.bytes.byteLength`.
4. `artifact.hash` is lowercase SHA-256. Core independently verifies it before
   preparing a runtime.
5. Its result is a coherent snapshot: metadata and bytes belong to the same
   release.

`watchActiveRelease` has these guarantees:

1. Its promise resolves only once the subscription is live. Core subscribes
   before its first load so an activation cannot be missed between load and
   subscribe.
2. It invokes `invalidate` after every activation that occurs after the promise
   resolves.
3. If the underlying connection drops or may have missed events, it invokes
   `invalidate` before or upon reconnection. Core then reloads authoritative
   state.
4. Duplicate invalidations are allowed. Core treats them as hints, not release
   data.
5. Its returned function fully releases the subscription and is safe to call
   more than once.

The core must call its local invalidation path after `publishRelease` resolves.
This guarantees same-process read-after-deploy even if the storage event is
coalesced or arrives before the publish promise resolves.

## Deployment and serving flows

### Core deployment

```text
deployApp(input, options)
  -> prepareSource(input)
  -> validateBuildInput(files)
  -> compute buildId from normalized source + packaging identity
  -> load artifact from optional build cache OR create agentOS VM
  -> npm install/build/prune
  -> build direct bundle (+ actor bundle when RivetKit is declared)
  -> validate direct bundle
  -> deterministic tar + AOSP package
  -> verify size + SHA-256
  -> publishRelease({ appId, artifact, regions, scaling, createdAt }, options)
  -> invalidate the local app mapping
  -> return the hook's TDeployment result unchanged
```

The build ID is an internal deterministic identity, not necessarily the
storage provider's public release ID. The default Rivet adapter derives its
release ID from the build ID plus deployment metadata. The runner may embed the
build ID in its internal metadata; serving reports the active release ID
returned by storage.

### Cache miss

```text
request
  -> validate/normalize route
  -> acquire execution admission
  -> create per-app cache entry
  -> await watchActiveRelease subscription readiness
  -> capture local invalidation epoch
  -> loadActiveRelease(appId)
  -> if epoch changed while loading, discard and repeat load
  -> copy and verify artifact bytes/hash/format
  -> reuse runtime by artifact hash, or parse/snapshot/prewarm it
  -> execute request
```

### Warm request

```text
request
  -> validate/normalize route
  -> acquire execution admission
  -> read cached app -> runtime mapping
  -> lease/create isolate according to configured mode
  -> execute request
```

A warm request makes zero calls to all three release hooks.

### Invalidation

```text
watch callback
  -> increment app entry epoch
  -> clear only the app -> runtime mapping
  -> asynchronously reload and prepare the new active release
  -> keep an already referenced old runtime alive until its request completes
  -> evict the old runtime later by normal LRU/TTL/memory rules
```

Do not immediately mark an artifact-hash runtime stale merely because one app
mapping changed. Another app may share the same immutable artifact.

## Exact file and symbol movement

When this section says **move**, move the existing implementation and its tests;
do not retype or redesign it. Preserve history where practical with filesystem
moves before editing imports.

### New `packages/dynamic-apps-core`

| From | To | Instruction |
| --- | --- | --- |
| `packages/dynamic-apps/src/errors.ts` | `packages/dynamic-apps-core/src/errors.ts` | Move `DynamicAppsError` unchanged. |
| `packages/dynamic-apps/src/memory.ts` | `packages/dynamic-apps-core/src/memory.ts` | Move the complete file unchanged. |
| `packages/dynamic-apps/src/source.ts` | `packages/dynamic-apps-core/src/source.ts` | Move `validateAppId`, source directory traversal, bounds enforcement, and `prepareSource` unchanged; update imports only. |
| `packages/dynamic-apps/src/executor.ts` | `packages/dynamic-apps-core/src/executor.ts` | Move isolate execution, request serialization, response validation, memory admission, semaphore, runtime cache, timing, and bootstrap code. Replace only the Rivet-specific acquisition seam described below. |
| `packages/dynamic-apps/src/router.ts` | `packages/dynamic-apps-core/src/router.ts` | Move route validation, prefix rewriting, redirects, error mapping, and Hono handlers. Replace the global/default executor and private-registry dispatch as described below. |

Split `packages/dynamic-apps/src/runtime.ts`:

- Move `DIRECT_ENTRYPOINT`, `DIRECT_BUNDLE_PATH`, `ACTOR_BUNDLE_PATH`,
  `DIRECT_RUNTIME_FORMAT`, `normalizeAppPath`, `canonicalDeploymentHash`,
  `directRunnerSource`, and `actorRunnerSource` to
  `packages/dynamic-apps-core/src/runtime.ts` unchanged.
- Keep `APP_CALLBACK_SECRET_HEADER`, `appRunnerPool`, `readBoundedText`,
  `engineUrl`, and `ensureServerlessRunnerConfig` in
  `packages/dynamic-apps/src/runtime.ts`.
- Update the retained adapter runtime file to import shared constants and path
  helpers from `@rivet-dev/dynamic-apps-core/internal`.

While moving `executor.ts`, move `extractAospkgTextFile` and its `tarString`
helper unchanged into `packages/dynamic-apps-core/src/artifact.ts`. Import that
helper locally from both executor and build code so cached build artifacts and
loaded serving artifacts use one parser. Do not export the parser from either
package entry point.

Split `packages/dynamic-apps/src/actors.ts`:

- Move constants `DEFAULT_MAX_SOURCE_BYTES`, `DEFAULT_MAX_FILES`,
  `DEFAULT_MAX_DEPENDENCIES`, `DEFAULT_BUILD_TIMEOUT_MS`,
  `DEFAULT_MAX_RESPONSE_BYTES`, `DEFAULT_MAX_BUILD_OUTPUT_BYTES`,
  `DEFAULT_MAX_BUILD_ARTIFACT_BYTES`,
  `DEFAULT_MAX_BUILD_ARTIFACT_FILES`,
  `DEFAULT_MAX_BUILD_ARTIFACT_FILE_BYTES`, and
  `DEFAULT_MAX_BUILD_FILESYSTEM_BYTES` to
  `packages/dynamic-apps-core/src/build.ts`.
- Move types `ExecResult`, `BuildHandle`, and `BuildPlan` to core `build.ts`.
- Move functions `textFile`, `packageExport`, `installPackageJson`,
  `validateDeployment`, `boundedOutput`, `throwCommandFailure`,
  `buildRelease`, and `createBuildVmFactory` to core `build.ts`.
- While moving `buildRelease`, replace only its `AnyActorContext` logging/failure
  dependencies: use `DynamicAppsLogger` and throw `DynamicAppsError`. Do not
  change its VM commands, install flags, bundle configs, validation command,
  packing, limits, or cleanup behavior.
- Rename the moved public orchestration function to `buildAppRelease`. Its input
  is normalized files plus core build configuration, and its output is
  `BuiltAppRelease`.
- Keep `normalizeRegions` and `normalizeScaling` in the Rivet adapter because
  the default region comes from actor context and scaling is persisted by that
  adapter.
- Keep all SQLite migration and persistence helpers, callback authentication,
  namespace/runner setup, actor definition, and `forwardActorCallbackRequest`
  in `packages/dynamic-apps/src/actors.ts`.

Delete the old copies only after imports compile and the corresponding moved
tests pass. Do not leave forwarding modules in `@rivet-dev/dynamic-apps` except
where the retained package needs an internal import.

### Files that remain entirely in `@rivet-dev/dynamic-apps`

- `src/actor-runtime.ts`
- `src/control-plane.ts`
- `src/control-request.ts`
- `src/registry.ts`

These are Rivet-specific and must not move to core.

### New core files

Create:

- `packages/dynamic-apps-core/src/index.ts`: public exports only.
- `packages/dynamic-apps-core/src/internal.ts`: implementation exports used by
  the Rivet adapter.
- `packages/dynamic-apps-core/src/types.ts`: contracts above plus moved executor
  types.
- `packages/dynamic-apps-core/src/build.ts`: extracted agentOS build.
- `packages/dynamic-apps-core/src/factory.ts`: instance construction and deploy
  orchestration.
- `packages/dynamic-apps-core/src/executor.ts`: storage-independent executor.
- `packages/dynamic-apps-core/src/router.ts`: injected-executor router factory.
- `packages/dynamic-apps-core/src/runtime.ts`, `source.ts`, `memory.ts`, and
  `errors.ts` from the moves above, plus the extracted `artifact.ts`.
- `packages/dynamic-apps-core/tests/core.test.ts`.
- `packages/dynamic-apps-core/tests/build.test.ts`.
- `packages/dynamic-apps-core/package.json`, `tsconfig.json`, and `README.md`.

Create in the adapter:

- `packages/dynamic-apps/src/release-store.ts`: all client-side Rivet hook
  implementations and chunk transfer.
- `packages/dynamic-apps/src/default.ts`: constructs the singleton core instance
  used by retained root exports.

## Core implementation details

### `build.ts`

`buildAppRelease` must accept already prepared byte files so both the factory
and the adapter's legacy actor action can use the exact same implementation:

```ts
export interface BuildAppReleaseInput {
	appId: string;
	files: Record<string, Uint8Array>;
}

export async function buildAppRelease(
	input: BuildAppReleaseInput,
	options?: {
		config?: Partial<BuildConfig>;
		artifactCache?: BuildArtifactCache;
		logger?: DynamicAppsLogger;
	},
): Promise<BuiltAppRelease>;
```

Implementation requirements:

1. Merge `options.config` over current constants; validate every limit once.
   For this extraction, source/file/dependency and artifact/filesystem settings
   are security ceilings: overrides may lower but not raise the retained
   maximums. This lets `prepareSource` move unchanged with its current hard
   bounds.
2. Run the moved `validateDeployment` and use its returned `BuildPlan`.
3. Compute `buildId` with the moved deterministic hash function using normalized
   source files, entrypoint, build flag, and the existing packaging identity.
   Do not include a storage namespace or actor ID.
4. Use `buildId` as the optional artifact-cache key and as the internal runner
   version passed to `directRunnerSource`.
5. Execute the moved build pipeline unchanged.
6. Return a `BuiltAppRelease` containing the build ID and a `ReleaseArtifact`
   with a copied byte array, SHA-256, exact byte length, direct
   format/entrypoint constants, and `usesRivetKit` from the plan.
7. The default logger is a no-op. Build phase logs retain their current event
   names and elapsed times when a logger is supplied.
8. Cached artifacts must still be checked for maximum size and SHA-256. Parse
   and verify that the direct bundle exists before returning a cached artifact;
   do not blindly trust cache bytes.

Export `buildAppRelease` only from `@rivet-dev/dynamic-apps-core/internal` for
this release. The public factory is the supported build entry point.

### `factory.ts`

Implement `createDynamicApps` with no module-level mutable state:

1. Validate that all three hooks are functions.
2. Resolve executor and build configuration once per instance.
3. Construct one `DynamicAppsExecutor` with the three hook functions.
4. Construct one router by calling `createAppsRouter(executor)`.
5. `deployApp` calls `prepareSource`, then `buildAppRelease`, then the supplied
   `publishRelease`. It maps the build result to `{ appId, buildId, artifact,
   regions, scaling, createdAt }`; source files never reach the publish hook.
6. After publish resolves, call `executor.invalidate(input.appId)` before
   returning the hook result unchanged.
7. If publish rejects, do not invalidate the current mapping and do not alter
   the returned error.
8. `diagnostics()` delegates to the executor and adds no credentials or
   artifact contents.
9. `dispose()` is idempotent and delegates to the executor. After disposal,
   deploy and request operations fail with `agentos_apps_executor_disposed`.

The factory keeps an instance-local disposed flag and set of in-flight deploy
promises. Check the flag before source preparation and again after build but
before publish, so a build finishing during shutdown cannot activate a new
release. `dispose()` marks the instance disposed, waits for already-started
publish calls and build cleanup with `Promise.allSettled`, then disposes the
executor. It must not leak an agentOS VM or publish after the post-build check.

### `executor.ts`

Remove these Rivet-specific imports and declarations from the moved file:

- `createClient` from `rivetkit/client`
- `AppRouteResolution` from adapter actors
- `ensurePrivateAppsRegistry`
- `ArtifactManifest`
- `ReleaseActivatedEvent`
- `AppConnection`
- `AppHandle`
- `StateClient`

Change the constructor to receive a release source:

```ts
interface ExecutorReleaseSource {
	loadActiveRelease(
		appId: string,
		context: ReleaseLoadContext,
	): Promise<ActiveRelease | undefined>;
	watchActiveRelease(
		appId: string,
		invalidate: ReleaseInvalidation,
	): Promise<Unsubscribe>;
}

export class DynamicAppsExecutor {
	constructor(source: ExecutorReleaseSource, config: ExecutorConfig);
	request(appId: string, request: Request): Promise<Response>;
	invalidate(appId: string): void;
	diagnostics(): Record<string, unknown>;
	dispose(): Promise<void>;
}
```

Replace `AppCacheEntry` with:

```ts
interface AppCacheEntry {
	appId: string;
	subscription: Promise<Unsubscribe>;
	unsubscribe?: Unsubscribe;
	mapping?: AppMapping;
	resolvePromise?: Promise<AppMapping>;
	epoch: number;
	lastUsedAt: number;
	refs: number;
}
```

`AppMapping.resolution` becomes `ActiveRelease`.

Implement entry creation in this order:

1. Create and insert the entry synchronously before invoking the watch hook, so
   a synchronous invalidation callback can find it.
2. Call `watchActiveRelease(appId, () => invalidate(appId))`.
3. Save the promise immediately.
4. When it resolves, save the returned unsubscribe function unless the entry
   was evicted or the executor disposed; in that case invoke it immediately.
5. If subscription setup rejects, remove the entry if it is still current and
   propagate the error to the request. A later request may retry.

Implement resolution as an epoch loop:

```ts
for (;;) {
	await entry.subscription;
	const epoch = entry.epoch;
	const release = await source.loadActiveRelease(entry.appId, timingContext);
	if (entry.epoch !== epoch) continue;
	if (!release) throw notDeployedError;
	const runtime = await prepareRuntime(release);
	if (entry.epoch !== epoch) continue;
	return publishMapping(entry, release, runtime);
}
```

Keep single-flight behavior with `resolvePromise`. `invalidate(appId)` increments
the epoch and clears `mapping`; if the entry is active, start a background
single-flight resolution and swallow only that background promise's rejection.
The next foreground request must still observe and report the error.

Do not evict an app entry while `refs > 0` or `resolvePromise` is present. Before
publishing a resolved mapping, verify that `this.#apps.get(appId)` is still the
same entry and that the executor is not disposed; otherwise discard the mapping
without resurrecting the entry.

Replace manifest/chunk download in `#createRuntime` with:

1. Copy `release.artifact.bytes`.
2. Validate app ID, release, regions, limits, format, entrypoint, hash syntax,
   declared byte length, actual byte length, and SHA-256.
3. Record copying and validation as `artifact-verify`.
4. Continue at the existing `artifact-parse`, snapshot, and prewarm code.

The runtime key remains artifact hash plus runtime format. Runtime artifact byte
accounting uses the actual verified artifact length.

The `ReleaseLoadContext.recordTiming` implementation must:

- accept only finite, non-negative durations;
- normalize a phase name to lowercase ASCII kebab case;
- prefix storage phases with `store-`;
- refuse names longer than 64 bytes;
- never allow a hook to overwrite a core timing phase.

Preserve current timing headers. Rename the outer cold load phase to
`release-load`; the default adapter records `store-actor-connect`,
`store-actor-resolve`, `store-artifact-manifest`, and
`store-artifact-download` sub-phases.

On app entry TTL/LRU eviction and executor disposal, await or attach cleanup to
the subscription promise and invoke its unsubscribe function exactly once.
Unsubscription failure must not prevent isolate disposal; include it in
`Promise.allSettled` during full disposal.

Remove `getDefaultExecutor` and `resetDefaultExecutorForTest` from core. The
Rivet adapter owns its one default instance.

### `router.ts`

Replace the module-level router and request override with:

```ts
export interface AppRequestExecutor {
	request(appId: string, request: Request): Promise<Response>;
}

export function createAppsRouter(
	executor: AppRequestExecutor,
): Hono<BlankEnv, BlankSchema, "/">;
```

Move the existing handler body unchanged except for calling the injected
executor. Remove `PRIVATE_REGISTRY_SENTINEL`, `handlePrivateAppsRegistry`,
`requestOverride`, `setRouterRequestOverride`, and the custom `router.fetch`
override. Those belong only to the Rivet wrapper.

Unit tests inject an `AppRequestExecutor`; do not add a new global test seam.

### Public and internal exports

`packages/dynamic-apps-core/src/index.ts` exports:

- `createDynamicApps`
- public types from `types.ts`
- no actor, Rivet, chunk, registry, or control-plane types

`packages/dynamic-apps-core/src/internal.ts` exports only what the adapter needs:

- `buildAppRelease`
- `DynamicAppsError`
- `ACTOR_BUNDLE_PATH`, `DIRECT_BUNDLE_PATH`, `DIRECT_ENTRYPOINT`,
  `DIRECT_RUNTIME_FORMAT`
- `normalizeAppPath`, `canonicalDeploymentHash`, `actorRunnerSource`, and
  `directRunnerSource`
- `prepareSource` and `validateAppId` for the retained injected-client deploy
  compatibility path
- `capConcurrencyForMemory` and `readCgroupMemory`

Do not export `DynamicAppsExecutor` publicly in this release. Factory
`diagnostics` and `dispose` are sufficient.

## Rivet adapter implementation

### Default instance

In `packages/dynamic-apps/src/default.ts`:

1. Lazily create the ordinary RivetKit client exactly as today.
2. Create the actor release hooks from `release-store.ts`.
3. Construct one core instance with those hooks.
4. Export internal `defaultDynamicApps` for `deploy.ts` and `router.ts`.

Pass an adapter logger that emits the existing structured build-phase events to
`console.log`/`console.error`; do not log source, artifact bytes, request bodies,
response bodies, or credentials. Core itself keeps a no-op default logger.

Do not create the client or start the private registry at module import. Hook
execution remains lazy.

### Retained adapter types and root files

Rewrite `packages/dynamic-apps/src/types.ts` as the Rivet-only portion of the
old file:

- Import `AppScaling` and `DeployAppInput` from core for internal type use; do
  not re-export them from the adapter root.
- Keep `Deployment`, `AppReleaseInfo`, and `PreparedDeployAppInput` with their
  existing exact fields.
- Add `usesRivetKit: boolean` to the internal `AppRouteResolution` returned by
  `resolveDeployment`, and populate it from the stored release. The release
  store needs this field to construct core `ReleaseArtifact`; it is not added
  to the public `Deployment` result.
- Keep `PreparedDeployAppInput.files` because the legacy injected-client action
  still accepts source bytes.
- Add actor-protocol input/result interfaces in `release-store.ts`, not to the
  package root declaration.

Rewrite `packages/dynamic-apps/src/deploy.ts` but retain the current exported
signature exactly:

```ts
export async function deployApp(
	input: DeployAppInput,
	options: DeployAppOptions = {},
): Promise<Deployment>;
```

The no-client branch calls `defaultDynamicApps.deployApp(input)`. The injected
client branch moves the current implementation unchanged: call core-internal
`prepareSource`, construct `PreparedDeployAppInput`, resolve the stable actor,
retry host-registry readiness as today, call its legacy `deploy` action, and
project exactly the seven documented deployment keys. Keep
`resetDefaultAppsClientForTest` internal if release-store tests still need it;
otherwise replace it with an internal reset owned by `default.ts`.

Keep `packages/dynamic-apps/src/index.ts` textually equivalent to:

```ts
export { deployApp } from "./deploy.js";
export { appsRouter } from "./router.js";
```

Delete adapter `errors.ts`, `memory.ts`, and `source.ts` after every retained
import points to core internal. Do not add adapter forwarding exports for them.

### Actor release store

`packages/dynamic-apps/src/release-store.ts` implements the core hooks. Keep the
adapter's public chunk size at the current internal value of 512 KiB.

Define structural client types for these actor actions:

```ts
interface BeginReleasePublishInput {
	appId: string;
	buildId: string;
	format: typeof DIRECT_RUNTIME_FORMAT;
	entrypoint: typeof DIRECT_ENTRYPOINT;
	artifactHash: string;
	artifactBytes: number;
	usesRivetKit: boolean;
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

interface AppReleaseHandle {
	beginReleasePublish(input: BeginReleasePublishInput): Promise<BeginReleasePublishResult>;
	writeReleaseChunk(input: WriteReleaseChunkInput): Promise<void>;
	commitReleasePublish(input: CommitReleasePublishInput): Promise<Deployment & { appActorId: string; usesRivetKit: boolean }>;
	resolveDeployment(): Promise<AppRouteResolution>;
	getArtifactManifest(release: string): Promise<ArtifactManifest>;
	readArtifactChunk(release: string, index: number): Promise<Uint8Array>;
	connect(): AppConnection;
}
```

Keep a private per-app driver entry containing the stable handle, connection,
readiness promise, listener removers, and disposal state. `watchActiveRelease`
creates that entry; `loadActiveRelease` reuses it, which ensures resolution and
chunk calls use the already connected handle. Unsubscribe removes the entry
only if it is still the current object. Publish may reuse the handle but must
not create a second long-lived connection. Do not add a second artifact or app
mapping cache in the driver.

The hook implementation behaves as follows.

#### Publish

1. Ensure the private registry is ready when not serverless.
2. Resolve the stable `[appId]` actor with `get` first and fall back to
   `getOrCreate` only for actor-scoped not-found, preserving current behavior.
3. Call `beginReleasePublish` with build ID, artifact hash/length/format,
   `usesRivetKit`, requested regions/scaling, and creation time. Do not send the
   complete artifact in this call.
4. If `uploadRequired`, split bytes into exactly the returned `chunkBytes` and
   call `writeReleaseChunk` sequentially from index zero. Sequential transfer is
   deliberate for the first extraction: it is simpler, bounded, and deployment
   latency is not the serving hot path.
5. Call `commitReleasePublish` with release, sequence, and expected chunk count.
6. Return its `Deployment` unchanged.

#### Load

Move the existing executor-side actor protocol into this hook:

1. Await connection readiness and record `actor-connect`.
2. Call `resolveDeployment` and record `actor-resolve`.
3. Call `getArtifactManifest` and record `artifact-manifest`.
4. Read chunks in ascending order, enforce current per-chunk and count bounds,
   concatenate them, and record `artifact-download`.
5. Validate manifest consistency before constructing `ActiveRelease`.
6. Return the complete bytes. Core performs a second independent hash check.

Do not cache artifacts in `release-store.ts`; core owns caching.

#### Watch

1. Get the stable actor handle and call `connect()`.
2. Register `releaseActivated`, `onClose`, and `onOpen` handlers before awaiting
   `connection.ready`.
3. `releaseActivated` always calls invalidation. Event revision filtering is
   unnecessary in the adapter because core coalesces via epoch and single
   flight.
4. `onClose` calls invalidation.
5. `onOpen` calls invalidation except for the initial open that fulfills
   `ready`.
6. Resolve the hook only after `ready` resolves.
7. Return an idempotent unsubscribe that removes listeners and disposes the
   connection.

### Actor persistence protocol

Keep `agentOSAppsApp` and all current tables. Extend `AppState` with optional
fields so old serialized state needs no migration:

```ts
interface AppState {
	// existing fields remain
	publishSequence?: number;
	latestPublishSequence?: number;
}
```

Add three actor actions.

#### `beginReleasePublish`

Under the existing per-actor `serialized` lock:

1. Verify actor key and input app ID match.
2. Validate format, build ID, SHA-256, artifact byte length, requested regions,
   and scaling.
3. Provision/reuse the app namespace with the existing control-plane code.
4. Normalize regions using the actor's current region fallback and normalize
   scaling with current defaults/bounds.
5. Compute the default adapter release ID as SHA-256 over a new domain string,
   build ID, artifact hash, normalized regions/scaling, namespace, runtime
   endpoint/pool, and `usesRivetKit`. Keep this helper private to the adapter.
6. Increment `publishSequence`, set `latestPublishSequence`, and return it.
7. Reuse the existing callback secret for actor-enabled releases, creating one
   only when no prior release has one.
8. If the exact release row is already `ready` and its hash/length/metadata
   match, return `uploadRequired: false` without deleting chunks.
9. Otherwise upsert the release row as `building`, storing the expected artifact
   hash and length, and delete old chunks for that release.
10. Do not change `activeRelease`.

No source files are written on the new path. Leave the release-files table and
helpers in place for old rows and the compatibility action.

#### `writeReleaseChunk`

1. Verify release ID syntax, sequence, integer index, and `content` type.
2. Require a matching `building` release row.
3. Require index to be below the expected `ceil(artifactBytes / chunkBytes)`.
4. Require every non-final chunk to be exactly `chunkBytes`, and the final chunk
   to be exactly the remaining byte count.
5. Upsert `(release_id, chunk_index)` and byte length. Retrying the same chunk is
   idempotent.
6. Do not activate, broadcast, or configure a runner.

The sequence check prevents a stale publisher for the same app from continuing
after a newer `beginReleasePublish` has superseded it.

#### `commitReleasePublish`

Under the existing per-actor `serialized` lock:

1. Require `sequence === latestPublishSequence`. Otherwise throw
   `agentos_apps_publish_superseded` and keep the current active release.
2. Load the building/ready row and verify expected chunk count and total bytes.
3. Stream/read all stored chunks in order and compute SHA-256; compare it to the
   expected hash. Do not mark ready on mismatch.
4. Mark the row `ready` only after verification.
5. For an actor-enabled release, validate its public endpoint and call the
   existing `configureAppNamespaceRunner` before activation.
6. If runner configuration fails, leave the previous active release unchanged.
   A verified but inactive ready row may remain for retry.
7. Set `activeRelease`, increment the existing state revision, and broadcast the
   existing `releaseActivated` event.
8. Apply the existing `DEFAULT_MAX_VERSIONS` cleanup without deleting the
   newly active release.
9. Return the existing exact `Deployment` shape.

Logical activation remains atomic without a schema change: incomplete or failed
uploads never replace `state.activeRelease`. SQLite rows may show an inactive
`building`, `failed`, or `ready` release, which is acceptable and inspectable.

### Compatibility `deploy` actor action

Keep the existing actor action named `deploy` and its exact
`PreparedDeployAppInput -> Deployment & { appActorId; usesRivetKit }` structural
signature because it is visible through `DeployAppOptions.client` in the
retained declaration.

Rewrite only its implementation:

1. Call core internal `buildAppRelease` with the prepared byte files.
2. Feed the result into the same private begin/write/commit implementation
   without actor RPC round trips.
3. Retain source-file persistence on this compatibility path if required by an
   existing test; it is not used by the new default path.

Catch `DynamicAppsError` at the actor action boundary and convert it with the
existing `fail(code, message, metadata)` helper so callers continue receiving a
RivetKit `UserError`. Do the same around core/control-plane errors in the new
publish actions. Outside an actor action, core errors remain ordinary
`DynamicAppsError` instances.

The top-level adapter `deployApp` behaves as follows:

- With no injected `options.client`, delegate to the default core instance so
  the build occurs before the release-store upload.
- With `options.client`, retain the current `prepareSource` plus
  `client.agentOSAppsApp.get/getOrCreate(...).deploy(preparedInput)` path exactly.

This preserves existing injected-client behavior while making the ordinary
path use the new core architecture. Add a code comment explaining why the two
paths exist and a test proving both.

### App-defined actor callbacks

Do not move or simplify this path:

```text
Rivet gateway
  -> private registry onRequest
  -> read active release from SQLite
  -> verify callback secret
  -> read actor bundle from the same stored AOSP artifact
  -> DynamicActorRuntime worker
  -> app-defined RivetKit registry
```

`actor-runtime.ts` continues to read `actor/main.mjs`, now importing the path
constant from core internal. State, actions, events, connections, SQLite in the
app-defined actor, direct HTTP serving, and streaming responses must all remain
covered by cloud end-to-end tests.

### Adapter router wrapper

The core router does not know about the private Rivet registry. In adapter
`router.ts`:

1. Obtain `defaultDynamicApps.appsRouter`.
2. Preserve the existing custom `fetch` behavior for the private sentinel
   header and `/api/rivet` paths.
3. Delegate all ordinary app traffic to the core router.
4. Export the wrapped value as `appsRouter` with the current exact Hono type.

Do not restore `setRouterRequestOverride`; adapter tests should instantiate a
core factory with fake hooks or exercise the wrapped router directly.

## Package manifests and build graph

Create `packages/dynamic-apps-core/package.json`:

```json
{
	"name": "@rivet-dev/dynamic-apps-core",
	"version": "0.12.0-rc.2",
	"description": "Build and serve isolated dynamic applications with user-defined release storage.",
	"license": "Apache-2.0",
	"repository": {
		"type": "git",
		"url": "https://github.com/rivet-dev/dynamic-apps.git",
		"directory": "packages/dynamic-apps-core"
	},
	"type": "module",
	"sideEffects": false,
	"files": ["dist", "package.json"],
	"exports": {
		".": {
			"import": {
				"types": "./dist/index.d.ts",
				"default": "./dist/index.js"
			}
		},
		"./internal": {
			"import": {
				"types": "./dist/internal.d.ts",
				"default": "./dist/internal.js"
			}
		}
	},
	"engines": { "node": ">=22.0.0" },
	"scripts": {
		"build": "tsup src/index.ts src/internal.ts --format esm --dts --sourcemap --clean --external @rivet-dev/agentos --external @rivet-dev/agentos-core --external @rivet-dev/agentos-toolchain --external @rivet-dev/dynamic-apps-builder --external @agentos-software/sh --external @agentos-software/tar",
		"check-types": "tsc --noEmit",
		"test": "vitest run"
	},
	"dependencies": {
		"@agentos-software/sh": "0.2.15",
		"@agentos-software/tar": "0.3.5",
		"@rivet-dev/agentos": "0.2.15",
		"@rivet-dev/agentos-core": "0.2.15",
		"@rivet-dev/agentos-toolchain": "0.2.15",
		"@rivet-dev/dynamic-apps-builder": "workspace:0.12.0-rc.2",
		"hono": "^4.7.0",
		"isolated-vm": "^6.2.0"
	},
	"devDependencies": {
		"@types/node": "^22.19.15",
		"tsup": "^8.4.0",
		"typescript": "^5.7.3",
		"vitest": "^2.1.8"
	}
}
```

Move these dependencies from `packages/dynamic-apps/package.json` to core:

- `@agentos-software/sh`
- `@agentos-software/tar`
- `@rivet-dev/agentos`
- `@rivet-dev/agentos-core`
- `@rivet-dev/agentos-toolchain`
- `@rivet-dev/dynamic-apps-builder`
- `hono`
- `isolated-vm`

Keep `rivetkit` only in `@rivet-dev/dynamic-apps`. Add
`@rivet-dev/dynamic-apps-core: workspace:0.12.0-rc.2` to that package. Keep
`hono` as a direct adapter dependency because the retained public declaration
names its Hono router type. Remove `isolated-vm` from the adapter. If retained
adapter tests still import `@rivet-dev/agentos-toolchain` to construct actor
fixtures, list it as a dev dependency only; production adapter code must reach
the toolchain exclusively through core.

Update root scripts to build/test in this order:

1. `@rivet-dev/dynamic-apps-builder`
2. `@rivet-dev/dynamic-apps-core`
3. `@rivet-dev/dynamic-apps`
4. benchmarks/e2e as currently applicable

Update `pnpm-lock.yaml` with `pnpm install`; do not hand-edit it.

## Boundary and packaging checks

Update `scripts/check-boundaries.mjs` to assert:

- Core has the exact pinned agentOS and matching builder dependencies.
- Core has no `rivetkit` dependency or source import.
- Adapter depends on the exact matching core version.
- Adapter no longer directly depends on agentOS/build toolchain packages.
- Examples import only the default adapter unless they intentionally demonstrate
  custom storage.
- Adapter root still exports exactly `appsRouter` and `deployApp`.
- Core root exports `createDynamicApps` and does not export internal build or
  executor classes.
- Core declarations contain no RivetKit import.
- Adapter declarations do not leak core `internal` types.

Update `scripts/test-packed.mjs` to:

1. Pack builder, core, and adapter.
2. Install all three tarballs in the fixture, with file dependencies overriding
   their mutual published-version references.
3. Assert no `workspace:` or `catalog:` specifiers remain.
4. Import core and assert its runtime exports.
5. Construct core with an in-memory hook implementation, publish a tiny app,
   and serve one request.
6. Import adapter and retain the exact `appsRouter`/`deployApp` export assertion.
7. Retain direct and actor Apps builder smoke tests.

Update `scripts/set-release-version.mjs` to set all three package versions and
rewrite both exact internal dependency versions.

Update `.github/workflows/ci.yml` to run `npm pack --dry-run` in core.

Update `.github/workflows/publish.yml` to:

1. Check that builder, core, and adapter versions are unused.
2. Publish builder first, core second, adapter third.
3. Continue using npm trusted publishing/OIDC; do not add a long-lived npm
   token.
4. Use the same computed dist-tag for all three packages.

## Tests to move and add

### Move existing tests to core

Move these existing `direct.test.ts` groups/cases without redesigning their
behavior:

- Router prefix rewrite, bare-path redirect, and invalid app ID tests; replace
  the request override with an injected executor.
- Source copying/order/path normalization tests.
- Direct runner contract and isolate mode configuration tests.
- Cgroup concurrency cap test.
- All tests under `direct V8 execution`.

Rewrite their fake state client into a fake release source returning one
`ActiveRelease`. Artifact fixture generation remains unchanged.

### Keep existing tests in adapter

- Injected actor deploy behavior and stable actor get/fallback tests.
- Control token/public endpoint tests.
- All actor callback worker/resource/streaming tests.
- Actor callback request forwarding tests.
- Cloud namespace and runner configuration tests.

### New core hook-contract tests

Add deterministic tests for all of these:

1. `deployApp` prepares/builds, calls publish once with verified bytes, forwards
   generic options, returns the hook result unchanged, and invalidates locally.
2. Publish rejection leaves an already warm mapping usable.
3. First request subscribes before loading.
4. First request calls load once and a warm second request calls no hook.
5. Invalidation clears the mapping and causes the next request to load once.
6. Invalidation during a blocked load causes that result to be discarded and a
   second load to win.
7. Duplicate invalidations coalesce through one resolution promise.
8. Subscription rejection is visible and a later request can retry.
9. Connection-loss invalidation behavior is exercised through a fake watcher.
10. Entry eviction invokes unsubscribe exactly once.
11. Disposal during subscription setup invokes unsubscribe after setup resolves.
12. Dispose is idempotent and rejects later requests/deploys.
13. A build that finishes after disposal begins cleans up and does not call
    publish; an already-started publish is awaited.
14. Hash, length, format, entrypoint, region, and limit corruption are rejected
    before isolate creation.
15. Two app IDs with the same artifact hash share a prepared runtime but retain
    independent watchers and mappings.
16. A changed release ID with identical bytes reuses the runtime while updating
    response release headers.
17. Hook timing names are normalized/bounded and cannot overwrite core timings.

### New adapter SQLite/protocol tests

Use a real local Rivet engine and the actor's real SQLite database. Do not mock
SQL for these cases:

1. Begin/write/commit stores an artifact and makes it resolvable.
2. A real `SELECT`, `CREATE TABLE`, `INSERT`, and `SELECT` in an app-defined
   actor action persists across two calls.
3. Partial upload leaves the previous release active.
4. Bad chunk length/index is rejected.
5. Commit detects missing, extra, reordered, or corrupted chunks.
6. Retrying a chunk and commit is idempotent.
7. A later begin supersedes an earlier concurrent publisher.
8. Runner configuration failure preserves the previous active release.
9. Successful commit broadcasts invalidation and a warm serving process starts
   serving the new release without restart.
10. Existing pre-extraction SQLite rows still resolve and download.
11. The legacy `deploy` action still accepts prepared files and returns its old
    result shape.
12. Ordinary top-level `deployApp` uses begin/write/commit, while injected-client
    `deployApp` still calls the legacy action.

### End-to-end app-defined actor test

The cloud fixture must contain an actual RivetKit actor with SQLite use in its
action, for example:

```ts
const databaseActor = actor({
	async createState() {
		return {};
	},
	actions: {
		async increment(c) {
			await c.db.execute("CREATE TABLE IF NOT EXISTS counts (id TEXT PRIMARY KEY, value INTEGER NOT NULL)");
			await c.db.execute("INSERT INTO counts (id, value) VALUES ('main', 1) ON CONFLICT(id) DO UPDATE SET value = value + 1");
			const rows = await c.db.execute<{ value: number }>("SELECT value FROM counts WHERE id = 'main'");
			return Number(rows[0]?.value ?? 0);
		},
	},
});
```

The exact DB API may be adjusted to the pinned RivetKit version, but the test
must execute real SQL, return `1` then `2`, and repeat after actor sleep/wake or
reconnection. Merely proving in-memory actor state is insufficient.

## Load, soak, and performance verification

Run the existing benchmark suites against both storage implementations:

- Core with an in-memory hook store, to isolate build/executor overhead.
- Default Rivet actor store locally.
- Default Rivet actor store on Rivet Compute.

Every report must separate:

- request queue and request buffering;
- subscription/connect;
- active release resolution;
- artifact manifest and download inside the adapter;
- artifact copy/hash/parse;
- snapshot creation;
- isolate prewarm/create/lease/reset/destroy;
- user handler evaluation;
- total server time and client end-to-end time.

Required behavioral assertions:

- A warm request performs zero release-store calls.
- A redeploy invalidates every subscribed serving container.
- Artifact download occurs once per container per uncached artifact, not once per
  request.
- Isolate and runtime cache entry counts never exceed configured global bounds.
- Used contexts are never reused.
- Memory pressure evicts idle entries and does not kill in-flight requests.

Performance gates for the retained default `prewarm` mode, excluding user
handler time and excluding first artifact download:

- Local warm server overhead: p50 below 10 ms, p95 below 25 ms.
- Rivet Compute warm server overhead: p50 at most 25 ms, p95 at most 50 ms.
- No statistically significant regression beyond 10% from the pre-extraction
  same-revision baseline at identical concurrency/container count.

These are serving gates, not deployment gates. Artifact upload/download is
measured but is not optimized in this refactor.

Run:

1. Local correctness suite.
2. Local load ramp until saturation, recording throughput, latency, errors,
   queue depth, memory, runtime count, and isolate count.
3. Cloud smoke test for direct HTTP and real-SQL actor actions.
4. Cloud ramp that reaches multiple Compute containers; record Cloud Run
   instance count during scale-up and scale-down.
5. One-hour cloud soak at the highest stable rate below saturation, with
   periodic redeploy/invalidation and actor SQL calls.
6. Abort the soak immediately on correctness error, patch in a new JJ revision,
   rerun focused reproduction, then restart the full soak.

The soak passes only with zero incorrect responses, zero stale releases after
the invalidation convergence window, zero unbounded memory growth, and no
unexpected actor/database errors.

## Documentation changes

Create `packages/dynamic-apps-core/README.md` with:

- the three-hook mental model;
- a complete in-memory example;
- hook atomicity/subscription requirements;
- instance disposal;
- isolate modes and security boundary;
- a warning that a no-op watcher is unsafe for mutable app IDs across multiple
  processes.

Update public docs:

- `docs/content/docs/index.mdx`: describe core underneath the default Rivet
  adapter; change the architecture diagram so the agentOS build is in core and
  the actor is storage/control plane, not the serving hop.
- Add a **separate Core Quick Start** at
  `docs/content/docs/core-quickstart.mdx`. Keep the existing
  `docs/content/docs/quickstart.mdx` focused on the batteries-included
  `@rivet-dev/dynamic-apps` package; do not combine the two setup paths into one
  page. Add both pages to `docs/sidebar.json` with distinct labels:
  `Quick Start` and `Core Quick Start`.
- `docs/content/docs/deploy.mdx`: explain publish semantics and retain Rivet
  Cloud token/namespace instructions for the default adapter.
- `docs/content/docs/routing.mdx`: explain that warm requests are local and make
  no storage/actor call.
- `docs/content/docs/state-and-data.mdx`: retain app-defined actor and SQLite
  instructions.
- Add `docs/content/docs/custom-storage.mdx` and sidebar entry with the exact
  core factory example and hook guarantees.
- Update `packages/dynamic-apps/README.md` to call the package the Rivet-backed
  adapter and link core for custom storage.
- Update `packages/dynamic-apps/API_CONTRACT.md` to state that its retained
  two-value surface is unchanged, ordinary deploy now builds through core, and
  the injected-client path intentionally retains the legacy actor action.
- Keep the project `AGENTS.md` link to `https://rivet.dev/llms.txt`.

The Core Quick Start must be complete and runnable without Rivet or RivetKit. It
must walk through this exact flow in order:

1. Install `@rivet-dev/dynamic-apps-core`, `@hono/node-server`, and `hono`.
2. Define a minimal process-local release store using a `Map` of active
   releases and a `Map` of update listeners.
3. Implement all three hooks: atomically set the complete artifact in
   `publishRelease`, return a copied active release from `loadActiveRelease`,
   and register/return an unsubscribe function from `watchActiveRelease`.
4. Call `createDynamicApps` with those hooks.
5. Mount `dynamicApps.appsRouter` at `/apps` on a Hono server.
6. Call `dynamicApps.deployApp` with a complete two-file generated app
   (`package.json` and `index.js`) so the example does not depend on an
   unshown fixture directory.
7. Start the server with the documented Node/`isolated-vm` flags and show a
   `curl` request to `/apps/hello/` with its expected response.
8. Handle process shutdown by awaiting `dynamicApps.dispose()`.
9. Explain the request lifecycle in one compact diagram: agentOS build ->
   `publishRelease`; first request -> `watchActiveRelease` +
   `loadActiveRelease`; warm request -> local isolate cache with zero hooks.
10. End by linking to `custom-storage.mdx` for durable, multi-process storage
    and to the ordinary Quick Start for the Rivet actor-backed implementation.

The in-memory store must be prominently labeled **development-only**: it loses
releases on restart and cannot invalidate another process. Do not imply that it
is a production persistence implementation.

Create `examples/apps-core-quickstart/` from the exact Core Quick Start code,
with its own `package.json`, `tsconfig.json`, and source file. Add a CI smoke test
that starts it on `--host 0.0.0.0`, deploys the generated app, verifies the
expected HTTP response, and shuts down cleanly. The documentation code and
example must share one source or be checked for equivalence so they cannot
silently drift.

Public docs should call the operations **publish a release**, **load the active
release**, and **watch for updates**. Do not use `resolve` as a user-facing
operation and do not explain actor artifact chunks outside troubleshooting.

## Schema and migration

No SQL migration is required.

- Keep all existing tables and columns.
- New uploads reuse release rows and artifact chunk rows.
- New optional sequence fields live in actor state and default from missing to
  zero.
- Existing ready releases remain readable by unchanged resolve/manifest/chunk
  actions.
- Existing source-file rows may remain until ordinary version retention deletes
  their releases.
- Do not perform a bulk data rewrite during deploy.

Before release, add a fixture created by the pre-extraction code and prove the
new adapter serves it. This is the migration gate.

## Implementation order and JJ revisions

Implement as a short stack of reviewable JJ revisions, without empty revisions:

1. **Core package skeleton and pure moves**: manifests, types, errors, memory,
   source, runtime split, router factory.
2. **Build extraction**: move agentOS build and tests; keep adapter compiling via
   internal imports.
3. **Storage-independent executor/factory**: hooks, epoch race handling, core
   tests, in-memory packed smoke.
4. **Rivet release store protocol**: actor begin/write/commit, SQLite tests,
   legacy deploy compatibility.
5. **Default adapter wiring and actor callbacks**: root API tests and direct +
   app-defined actor end to end.
6. **Docs, packaging, CI, and publish workflow**.
7. **Local/cloud load, ramp, and soak patches**: one revision per discovered
   correctness/performance fix.
8. **Release candidate metadata**: `0.12.0-rc.2`, final packed test, OIDC publish.

Each revision must pass the focused tests for its layer before proceeding. The
final stack must pass `pnpm build`, `pnpm check-types`, `pnpm test`,
`pnpm check-boundaries`, `pnpm lint`, and `pnpm test:packed` from a clean install.

## Definition of done

The refactor is complete only when all of the following are true:

- A custom user store can build, publish, serve, update, and dispose an app using
  only the three hooks and no Rivet dependency.
- The default package still exposes exactly `deployApp` and `appsRouter` with
  bidirectionally compatible TypeScript signatures.
- Default warm requests execute locally with zero actor/store calls.
- Default deployment persists through the state actor, invalidates serving
  caches, and preserves the previous active release on every tested failure.
- Existing stored releases remain readable without a schema migration.
- App-defined actors work on Rivet Cloud, including real SQLite queries that
  persist across calls/reconnection.
- Local and cloud ramp/soak suites meet correctness, memory, invalidation, and
  latency gates.
- Builder, core, and adapter packed tarballs install together without workspace
  specifiers.
- Public docs cover both default Rivet storage and custom storage.
- The separate Core Quick Start works from a clean install without any Rivet
  environment variables, and its in-memory limitations are explicit.
- `0.12.0-rc.2` is published in builder -> core -> adapter order through OIDC.
