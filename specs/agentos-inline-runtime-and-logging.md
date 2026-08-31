# agentOS inline request runtime and log collection

Status: implementation specification  
Supersedes: `specs/direct-isolate-runtime.md`  
Public API baseline: `packages/dynamic-apps/API_CONTRACT.md`

## Decision

Remove `isolated-vm` completely. Ordinary Dynamic Apps HTTP requests execute
through the agentOS library's headless JavaScript API in the Compute process;
they must not start an execution actor, guest process, HTTP listener, or nested
Node server.

Keep the existing deployment implementation and durable `agentOSAppsApp`
state actor. It remains responsible for building releases in an agentOS build
VM, persisting release artifacts in actor SQLite, activation, rollback, and
release invalidation events.

Add one process-wide structured log hook so the host application can forward
application, actor, build, and runtime logs to stdout or its logging provider.

```text
deployApp
  -> agentOSAppsApp[appId]
  -> existing agentOS build VM
  -> immutable AOSP artifact in actor SQLite

first direct request in a Compute process
  -> appsRouter
  -> resolve release and verify artifact
  -> materialize artifact once
  -> AgentOs.create() once for that immutable release
  -> mount artifact at /app
  -> execute fetch through vm.javascript.evaluate()

warm direct request
  -> appsRouter
  -> cached release AgentOs instance
  -> ephemeral evaluation or leased retained context
  -> direct fetch result

app-defined actor request
  -> Rivet gateway
  -> authenticated Dynamic Apps callback
  -> existing bounded Node worker
  -> real RivetKit WASM registry
```

The app-defined actor worker remains in this revision. It does not use
`isolated-vm`, and its streaming response protocol is required for RivetKit
connections and events. Replacing that worker with one-shot agentOS
`evaluate()` would regress streaming semantics. The worker must, however,
forward stdout and stderr through the new log hook.

## Non-goals

- Do not change `deployApp()` inputs, results, namespace behavior, build
  rollback, release schema, artifact chunking, or state-actor identity.
- Do not reintroduce scaler or replica execution actors for ordinary HTTP.
- Do not boot `node /app/main.mjs`, poll a readiness URL, or call `vm.fetch()`
  for direct request execution.
- Do not expose the agentOS instance, actor definitions, or cache controls as
  public JavaScript APIs.
- Do not promise durable log delivery. The hook is an in-process emission
  surface; the configured logging backend owns buffering and delivery.

## Public API

Preserve the existing values and add one setter plus its public types:

```ts
export { appsRouter, deployApp, setDynamicAppsLogHandler };

export type DynamicAppsLogLevel = "debug" | "info" | "warn" | "error";
export type DynamicAppsLogSource =
	| "application"
	| "actor"
	| "build"
	| "runtime";

export interface DynamicAppsLogEvent {
	version: 1;
	timestamp: number;
	level: DynamicAppsLogLevel;
	source: DynamicAppsLogSource;
	message: string;
	appId?: string;
	release?: string;
	requestId?: string;
	actorId?: string;
	stream?: "stdout" | "stderr";
	metadata?: Readonly<Record<string, string | number | boolean | null>>;
}

export type DynamicAppsLogHandler = (
	event: Readonly<DynamicAppsLogEvent>,
) => void;

export declare function setDynamicAppsLogHandler(
	handler: DynamicAppsLogHandler | undefined,
): void;
```

The setter is process-global because `appsRouter`, the private RivetKit
registry, the direct runtime cache, and the actor-worker cache are already
process-global singletons.

Behavior:

- Setting a handler replaces the previous handler atomically. Passing
  `undefined` disables delivery.
- Events and nested metadata are frozen before delivery.
- The handler is called synchronously, and its return value is ignored. It must
  enqueue or write the event without blocking on a remote network request.
- A handler exception never changes an application response. Emit one
  rate-limited diagnostic to host stderr, without recursively invoking the
  handler.
- Never include request/response bodies, authorization headers, endpoint
  credentials, environment variables, actor callback secrets, or build source
  in metadata.
- Cap one message at 64 KiB after UTF-8 decoding. Mark truncation with
  `metadata.truncated: true`.

## agentOS direct execution contract

### Mandatory API spike

Before deleting the old executor, add a focused test proving the pinned
`@rivet-dev/agentos-core` version supports this exact sequence:

1. `AgentOs.create({ defaultSoftware: false, mounts: [...] })`.
2. `vm.javascript.evaluate()` dynamically imports a module from the mounted
   application artifact.
3. `inputs` passes the request envelope without source interpolation.
4. An async evaluation returns a JSON-serializable response envelope.
5. `onStdout` and `onStderr` receive application console output.
6. `createContext()`, `contexts.reset()`, and `contexts.delete()` work with the
   imported module and leave no state after reset.
7. `AbortSignal` and `timeoutMs` cancel a stalled evaluation.

Do not add another custom V8 bridge if any item fails. Fix or update agentOS,
then consume the resulting preview or release through the normal package
dependency.

### Release runtime

Each cached immutable release owns:

- the verified AOSP bytes and hash;
- one private temporary directory and materialized AOSP file;
- one `AgentOs` instance with the artifact mounted read-only at `/app`;
- an optional bounded pool of retained JavaScript context IDs;
- active request/reference counts, last-used time, and invalidation state; and
- log line decoders for active executions only.

Create the VM with the shared agentOS sidecar and no default software:

```ts
AgentOs.create({
	sidecar: { kind: "shared", pool: "dynamic-apps-direct" },
	defaultSoftware: false,
	mounts: [
		{
			path: "/app",
			readOnly: true,
			plugin: {
				id: "agentos_packages",
				config: {
					kind: "tar",
					tarPath: materializedArtifactPath,
					root: "/",
					readOnly: true,
				},
			},
		},
	],
	permissions: {
		fs: "allow",
		childProcess: "allow",
		process: "allow",
		env: "allow",
		network: "allow",
	},
});
```

The permissions operate inside the agentOS sandbox. Host filesystem objects,
credentials, and Node globals must never be injected into guest code.

### Request ABI

Keep the current bounded request/response envelope so routing behavior does not
change:

```ts
interface RequestEnvelope {
	url: string;
	method: string;
	headers: Array<[string, string]>;
	bodyBase64?: string;
}

interface ResponseEnvelope {
	status: number;
	statusText: string;
	headers: Array<[string, string]>;
	bodyBase64: string;
	timing?: Record<string, number>;
}
```

Pass the request as `inputs.request`. The agentOS-side dispatcher uses the real
Node `Request`, `Response`, `Headers`, `URL`, `Buffer`, and module loader. It
invokes the default exported function or `fetch()` method, buffers the response
within the existing size limit, and returns the response envelope as the
evaluation value.

Do not pass a JSON string through a custom host reference. Do not define custom
Web API shims. Do not expose an HTTP listener inside the guest.

### Execution modes

Replace direct V8 terminology with agentOS execution terminology:

| Mode | Behavior |
| --- | --- |
| `ephemeral` | Cache the release VM, but call `javascript.evaluate()` without a `contextId`; agentOS provides a fresh execution context for every request. |
| `pooled` (default) | Lease a bounded retained context, execute once, reset it, reinitialize the dispatcher, and return it to the pool. Overflow uses ephemeral evaluation. |

Remove `snapshot`; agentOS owns its internal V8 bootstrap/snapshot strategy.
Dynamic Apps must not construct or retain native V8 isolates itself.

Replace environment variables as follows:

| Remove | Add | Default |
| --- | --- | ---: |
| `DYNAMIC_APPS_ISOLATE_MODE` | `DYNAMIC_APPS_EXECUTION_MODE` (`ephemeral` or `pooled`) | `pooled` |
| `DYNAMIC_APPS_ISOLATE_POOL_SIZE` | `DYNAMIC_APPS_CONTEXT_POOL_SIZE` | `2` |
| `DYNAMIC_APPS_ISOLATE_POOL_MAX_TOTAL` | `DYNAMIC_APPS_CONTEXT_POOL_MAX_TOTAL` | `8` |
| `DYNAMIC_APPS_ISOLATE_IDLE_TTL_MS` | `DYNAMIC_APPS_CONTEXT_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_ISOLATE_HEAP_LIMIT_MB` | `DYNAMIC_APPS_CONTEXT_HEAP_LIMIT_MB` | `64` |

Keep the existing runtime artifact-cache, execution-concurrency, queue,
timeout, memory-high-water, timing-header, and request-log settings, renaming
only fields whose behavior was tied specifically to `isolated-vm`.

### Clean-request semantics

Both modes must preserve the existing clean-request guarantee:

- application globals and module-level mutable state begin clean for every
  ordinary request;
- a failed, timed-out, or aborted context is deleted rather than reused;
- a successful pooled context is reset and reinitialized before it becomes
  available again; and
- if reset or reinitialization fails, delete that context and replenish the
  pool asynchronously under the global context cap.

Prove this with an application-level counter that must return `1` for every
request, including concurrent requests and overflow beyond the pool.

### Invalidation and shutdown

Keep the current state-actor release subscription. On activation or reconnect,
invalidate the old release mapping immediately, stop leasing its contexts, and
dispose it only after active references drain.

Runtime disposal order is:

1. stop new admission;
2. cancel or drain pending evaluations within the shutdown timeout;
3. delete retained contexts;
4. dispose `AgentOs`;
5. remove the private materialized artifact directory.

Every step is idempotent. A partially created runtime must execute the same
cleanup path.

## Build output

The direct release must be a Node-targeted ESM bundle, not a browser IIFE.

- Accept supported Node builtins and resolve them through agentOS at runtime.
- Continue rejecting native `.node` addons.
- Delete the Dynamic Apps RivetKit stub entirely.
- For an app importing RivetKit, build the direct bundle with the real RivetKit
  WASM runtime and its WASM asset. Suppress the application's `registry.start()`
  only while importing the direct entrypoint, then restore it. Actor definitions
  remain real objects; only actor startup is host-managed.
- Keep the separate platform-linked actor bundle used by the existing bounded
  actor worker.
- Continue validating both bundles before activating a release.

The direct dispatcher should be an exported async function in
`direct/main.mjs`. It accepts a plain request envelope and returns a plain
response envelope. The host's inline evaluation imports and calls that
function; it does not duplicate dispatcher source per request.

## Structured log flow

### Direct application

For every `javascript.evaluate()` call, supply `onStdout` and `onStderr`.
Maintain independent streaming UTF-8 line decoders so split multibyte code
points and split lines are reconstructed correctly. Emit:

- stdout as `source: "application", level: "info"`;
- stderr as `source: "application", level: "error"`; and
- any unterminated final fragment when the execution completes.

Include `appId`, release, request ID, and stream.

### App-defined actors

Create actor workers with `stdout: true` and `stderr: true`, consume both Node
streams, and use the same bounded line decoder. Add `appId` and release to
`ActorRuntimeRequest` so output can be attributed. Emit with
`source: "actor"`. Keep worker transport errors as runtime error events.

### Build and host runtime

- Route existing build-phase messages and bounded build stdout/stderr through
  `source: "build"` while retaining RivetKit `c.log` for infrastructure
  observability.
- Route the existing `DYNAMIC_APPS_LOG_REQUESTS=1` summary through
  `source: "runtime"` instead of calling `console.log()` directly.
- Emit runtime preparation, eviction, reset failure, worker exit, and disposal
  errors with identifiers but without source, bodies, headers, or credentials.
- Internal fallback diagnostics may still write directly to host stderr when
  the configured handler itself fails.

## Exact file changes

### Package runtime

#### `packages/dynamic-apps/src/executor.ts`

Rewrite the implementation in place to minimize import churn:

- remove the `isolated-vm` import and every `ivm.Isolate`, snapshot, context,
  host-reference, and custom bootstrap type;
- add `AgentOs` and agentOS option imports;
- replace `IsolateMode`, `IsolateSlot`, and `PreparedRuntime` with agentOS
  execution-mode, VM, and context-pool equivalents;
- retain request limits, state-actor resolution, artifact verification,
  semaphore admission, release invalidation, timing headers, cgroup checks,
  and bounded cleanup;
- materialize one verified artifact per cached release and mount it in one
  cached `AgentOs` instance;
- dispatch with `javascript.evaluate()` and `inputs`;
- attach direct stdout/stderr log forwarding;
- reset successful pooled contexts and delete failed contexts;
- replace isolate diagnostics with VM/context/evaluation diagnostics; and
- remove `ISOLATE_BOOTSTRAP_SOURCE` completely.

The file must contain no import from `isolated-vm`, no custom `Request` or
`Response` implementation, and no `--no-node-snapshot` assumption.

#### `packages/dynamic-apps/src/runtime.ts`

- change `directRunnerSource()` from an IIFE that installs globals into a
  Node-targeted ESM dispatcher export;
- reconstruct requests with Node's real Web APIs;
- use `Buffer` for bounded Base64 conversion;
- temporarily suppress `Registry.prototype.start` while importing an
  actor-enabled app for direct serving;
- preserve response status, status text, ordered headers, repeated
  `set-cookie`, body limits, and phase timing; and
- leave `actorRunnerSource()` and authenticated callback configuration intact.

#### `packages/dynamic-apps/src/logging.ts` (new)

Implement the public event types, the process-global handler setter, frozen
event construction, message/metadata bounds, handler-error isolation, and the
incremental UTF-8 line decoder shared by direct executions and actor workers.

#### `packages/dynamic-apps/src/index.ts`

Export `setDynamicAppsLogHandler` and the log event/handler/source/level types
alongside the unchanged `appsRouter` and `deployApp` exports.

#### `packages/dynamic-apps/src/actor-runtime.ts`

- retain the bounded Node worker and real RivetKit WASM runtime;
- add `appId` and release attribution to `ActorRuntimeRequest`;
- opt into worker stdout/stderr streams and forward them through the shared
  line decoder and log emitter;
- emit bounded worker startup, exit, timeout, and transport errors; and
- dispose stream listeners with each worker entry.

#### `packages/dynamic-apps/src/actors.ts`

- pass app ID and release into actor-runtime requests;
- route build phases and bounded build process output through the shared log
  emitter while preserving `c.log` calls;
- do not change migrations, release tables, actions, artifact persistence, or
  callback authentication; and
- continue using the agentOS library for the build VM.

#### `packages/dynamic-apps/src/router.ts`

Keep route matching and error shapes unchanged. Ensure every request receives
one request ID before executor dispatch so all application/runtime log events
for that request share it. Do not expose the ID to guest headers unless the
caller already provided an accepted trace header.

#### `packages/dynamic-apps/src/memory.ts`

Rename isolate-specific helper arguments and diagnostics to contexts/VMs.
Keep cgroup-aware admission. Include agentOS VM/context limits in the same
memory-cap calculation; do not count only the JavaScript heap while ignoring
the sidecar and mounted artifact.

#### `packages/dynamic-apps/package.json`

- delete `isolated-vm`;
- keep `@rivet-dev/agentos-core` as the direct runtime dependency;
- update the description to say applications execute in agentOS; and
- retain Node 22 as the minimum runtime.

#### `pnpm-lock.yaml`

Regenerate from the package manifest. Verify all `isolated-vm` packages and
native artifacts disappear from the lockfile.

### Builder

#### `packages/dynamic-apps-builder/cli/apps-builder.mjs`

- remove `directIsolate`, `stubRivetKit`, `rivetKitStub()`, browser-IIFE output,
  and direct-runtime rejection of Node builtins;
- add an explicit `directAgentOs` build mode using `platform: "node"`, ESM,
  and the existing bounded filesystem plugin;
- bundle real RivetKit plus its WASM asset for actor-enabled direct bundles;
- continue keeping RivetKit external only for the platform-linked actor bundle;
  and
- retain native-addon rejection and output limits.

#### `packages/dynamic-apps-builder/test/builder.test.ts`

- replace browser-isolate assertions with Node ESM dispatcher assertions;
- prove a direct app can import `node:fs`;
- prove native addons still fail;
- prove the direct artifact contains no fake RivetKit stub;
- import and invoke the generated dispatcher inside agentOS; and
- retain the separate actor-bundle test.

### Public contract and documentation

#### `packages/dynamic-apps/API_CONTRACT.md`

- change the exact root export count from two to three runtime values;
- add the exact log types and setter signature above;
- replace direct-isolate behavior with agentOS inline execution behavior;
- remove browser-only and Node-builtin restrictions;
- preserve all `appsRouter` and `deployApp` compatibility clauses; and
- add packed declaration/runtime tests for the new export.

#### `packages/dynamic-apps/README.md`

- replace the direct-isolate architecture and configuration sections with
  agentOS VM caching and ephemeral/pooled context modes;
- remove `--no-node-snapshot` startup instructions;
- document Node API availability inside the agentOS sandbox;
- add the concise log-hook example; and
- retain the security warning, updated to describe agentOS boundaries instead
  of in-process `isolated-vm` boundaries.

#### `docs/content/docs/index.mdx`

Replace snapshot/isolate diagrams and caveats with the state actor -> cached
agentOS VM -> headless evaluation path.

#### `docs/content/docs/quickstart.mdx`

Remove `--no-node-snapshot`. Run the server with ordinary Node/tsx arguments.

#### `docs/content/docs/deploy.mdx`

Remove the statement that Node builtins are unsupported. State that code runs
inside agentOS with the configured filesystem, process, environment, and
network permission boundary. Keep native addons unsupported.

#### `docs/content/docs/logging.mdx` (new)

Add a brief public page titled **Collecting logs**:

1. Explain that application `console.log`/stdout, `console.error`/stderr, actor
   worker output, and host runtime summaries become structured events.
2. Show `setDynamicAppsLogHandler((event) =>
   process.stdout.write(JSON.stringify(event) + "\\n"))` as the recommended
   Cloud Run/Rivet Compute aggregation path.
3. Show forwarding into a user's synchronous/buffered logger.
4. Document event fields, truncation, best-effort delivery, and that request
   bodies, auth headers, and secrets are excluded.
5. Warn not to perform blocking network requests directly in the callback;
   enqueue into the logging SDK instead.

Keep the page user-focused. Do not describe worker internals, sidecar RPC, or
artifact caches.

#### `docs/sidebar.json`

Add **Collecting logs** under Capabilities after **Realtime Events**.

#### `specs/direct-isolate-runtime.md`

Replace the old specification with a short superseded notice linking to this
file. Do not leave it marked as the active architecture.

### Deployment and benchmarks

#### `Dockerfile`

Remove `--no-node-snapshot` from `CMD`. Do not change Compute runtime mode,
port, or deployment topology.

#### `benchmarks/dynamic-apps/src/edge.ts`

- replace fresh/snapshot/prewarm executors and endpoints with ephemeral and
  pooled agentOS executors;
- expose agentOS VM/context/evaluation timing headers;
- keep edge no-op and app-defined actor cases; and
- add a fixture using Node APIs plus stdout/stderr.

#### `benchmarks/dynamic-apps/src/suite.ts`

Rename cases and profiles to ephemeral/pooled, collect VM prepare, context
lease/reset, module import, evaluation, and log-dispatch phases, and retain
outer versus server timing separation.

#### `benchmarks/dynamic-apps/src/cloud-stress.ts`

Replace the snapshot traffic share with ephemeral traffic. Keep pooled and
actor traffic, partial-result preservation, ramp behavior, and failure abort.

#### `benchmarks/dynamic-apps/src/runtime-stress.ts`

Replace isolate churn with VM/runtime-cache and context-pool churn. Cover
context reset failure, evaluation timeout, invalidation while active, artifact
cleanup, log floods, slow/throwing handlers, and cgroup pressure.

#### `benchmarks/dynamic-apps/RESULTS.md`

Replace the architectural decision and qualification results after the new
runtime passes. Preserve the old numbers only in a clearly labeled historical
comparison. Never reuse the 616 ms guest-server result as the agentOS inline
latency.

### Tests

#### `packages/dynamic-apps/tests/direct.test.ts`

Replace isolated-VM-specific tests with:

- mandatory agentOS inline API spike;
- ephemeral request isolation;
- pooled context reset and overflow;
- full Node builtins in the sandbox;
- request/response limits and repeated cookies;
- timeout/abort and poisoned-context eviction;
- release invalidation and concurrent drain;
- bounded VM/context cache and disposal;
- no guest process or HTTP listener; and
- real RivetKit imports in a direct actor-enabled release.

#### `packages/dynamic-apps/tests/logging.test.ts` (new)

Test stdout/stderr line reconstruction, UTF-8 splits, truncation, frozen
events, source attribution, handler replacement/removal, throwing handlers,
secret exclusion, actor-worker output, and runtime request summaries.

#### `benchmarks/dynamic-apps/src/load.test.ts`

Update case names and assert partial results still survive early agentOS
runtime or log-handler failures.

#### `scripts/test-packed.mjs`

Update the expected declaration/runtime exports and prove the packed package
has no `isolated-vm` dependency or Node snapshot startup requirement.

#### `scripts/check-boundaries.mjs`

Allow the new internal `logging.ts` dependency edges only where required.
Continue preventing builder code from importing the runtime package.

## Verification sequence

1. Run the mandatory agentOS inline spike before deleting the old executor.
2. Build the builder package and run its tests.
3. Run Dynamic Apps unit tests, type checking, lint, boundary checks, and
   packed-package tests.
4. Run a local real Rivet Engine end-to-end test:
   - deploy a Node-API direct application;
   - serve repeated ephemeral and pooled requests;
   - verify clean globals;
   - capture stdout/stderr through the public hook;
   - deploy an app-defined actor;
   - verify state, SQLite, actions, events, callback streaming, direct HTTP,
     and actor logs.
5. Run local load and stress suites, including memory pressure and log floods.
6. Deploy to Rivet Compute using the existing deployment path.
7. Repeat correctness, ramp, multi-container, and bounded soak tests. Confirm
   no guest Node process or readiness polling appears in timings or logs.
8. Update public docs and benchmark results only after the deployed behavior
   matches them.

## Performance gates

Measure these before accepting the rewrite; do not infer them from native V8
creation or the old guest-server benchmark.

| Case | Target |
| --- | ---: |
| Cached pooled agentOS context, server p50 | <= 10 ms |
| Cached pooled agentOS context, server p95 | <= 25 ms |
| Cached VM + ephemeral agentOS evaluation, server p50 | <= 25 ms |
| Cached VM + ephemeral agentOS evaluation, server p95 | <= 50 ms |
| Disabled log hook overhead at p50 | <= 1% |
| Enabled stdout hook added server p50 | <= 2 ms |
| Correct responses under context overflow | 100% |
| Cross-request global leakage | 0 |
| Stale release responses after activation barrier | 0 |
| Unbounded VM/context/temp-file growth | 0 |

If pooled agentOS misses the latency target, report VM prepare, context lease,
module import, evaluation, reset, serialization, and logging independently.
Do not replace agentOS with another direct V8 integration to meet the target.

## Completion criteria

- `isolated-vm` is absent from package manifests, the lockfile, runtime source,
  Docker commands, and active public documentation.
- Ordinary HTTP executes only through the agentOS headless JavaScript API.
- No direct request starts a guest process or HTTP listener.
- Node APIs work inside the agentOS sandbox.
- Both ephemeral and pooled modes preserve clean-request semantics.
- App-defined RivetKit actors still pass deployment, SQLite, state, action,
  event, callback-streaming, and direct-HTTP tests.
- `setDynamicAppsLogHandler` receives attributed direct, actor, build, and
  runtime logs without affecting request correctness.
- The collecting-logs page matches the shipped API.
- Local and Compute performance, load, memory, and soak gates pass.
- Deployment state schema and `deployApp` behavior remain unchanged.
