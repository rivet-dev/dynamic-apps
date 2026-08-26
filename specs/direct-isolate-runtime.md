# Direct-isolate Dynamic Apps rewrite

Status: approved for implementation  
API baseline: `packages/dynamic-apps/API_CONTRACT.md`  
Baseline implementation: JJ `xuymorrq`, commit `baca1719`  
Target: first preview release of the rewritten `@rivet-dev/dynamic-apps`

## Decision

Preserve the current `deployApp` and application-actor build/persistence path,
but replace the scaler/replica serving graph with one state actor and one
process-local executor:

```text
deploy:
caller -> deployApp -> existing app actor deploy action
                  -> existing sandboxed build VM/package pipeline
                  -> existing release/artifact tables
                  -> activate immutable direct-execution release
                  -> publish releaseActivated

first request in a server process:
client -> appsRouter -> app state actor -> active release + artifact chunks
                     -> verify and prepare cached runtime -> fresh V8 session
                     -> application fetch -> response -> destroy V8 session

cache-hit request:
client -> appsRouter -> cached prepared runtime -> clean V8 lease
                     -> application fetch -> response -> destroy V8 session

invalidation:
app state actor --releaseActivated(revision, release, artifactHash)-->
appsRouter cache -> switch new requests to new immutable runtime
                 -> retire old runtime after its in-flight requests finish
```

The long-lived cached object is the immutable application artifact plus a
prepared agentOS VM/configuration. A separate, shorter-lived and independently
bounded clean-isolate pool may hold contexts that have run only the
host-controlled no-op prewarm. Each clean isolate is leased to at most one
application request, then reset/destroyed; it is never returned dirty. With the
pool disabled, the request creates and destroys its V8 session inline.
Application globals therefore cannot survive between requests in either mode.

This design assumes the initial customer is small enough for a bounded
process-local cache to be useful. Rivet remains responsible for durable app and
release state, artifact persistence, and cache-invalidation events. It is no
longer on the cache-hit request path.

## Goals and acceptance criteria

The preview is acceptable only when all of the following are true:

- The exact `appsRouter` and `deployApp` surface in
  `packages/dynamic-apps/API_CONTRACT.md` is mechanically locked before the old
  implementation is removed.
- A cache-hit request performs zero Rivet actor calls and uses a fresh V8
  session or a clean single-use prewarmed session. A session that has executed
  application code is reset/destroyed before it can be replenished.
- Deployment activation is durably ordered: a failed build or incomplete
  artifact never replaces the last ready release.
- A monotonic actor event invalidates each live process cache without polling;
  reconnecting subscribers reread actor state so missed events cannot leave a
  process permanently stale.
- Local and Rivet Compute load tests pass the correctness, resource, and latency
  gates below.
- The existing source preparation, sandboxed builder, release persistence,
  chunk storage, deployment retry, namespace, result, and rollback behavior is
  retained unless a direct-module artifact requires a narrowly documented
  packaging change.
- The package documentation describes only the retained surface and the new
  execution model.
- A preview, never a stable tag, is published and verified from a clean
  registry-installed consumer.

The rewrite optimizes the **server duration** measured inside `appsRouter`.
Public ingress latency is recorded separately and is not counted against the
server target.

## Non-goals

- Preserving `setup`, `setupApps`, `createAppsRouter`, `./advanced`, exported
  actor definitions, inspector APIs, or any other old package API.
- Preserving replica, scaler, admission-lease, rolling-replacement, or dirty
  application-isolate reuse behavior.
- Supporting application-defined RivetKit actors, WebSockets, streaming bodies,
  static-only packages, named `fetch`, or default-function entrypoints in the
  first preview.
- Migrating already-deployed production apps or cleaning up their old scaler
  and replica actor state.
- Replacing the working deployment pipeline with a new upload protocol or
  moving untrusted builds into the caller process.
- Solving multi-process cache coherence with a new database or event log.
- Optimizing artifact download before measuring it separately.

## Public API lock and destructive rewrite

`packages/dynamic-apps/API_CONTRACT.md` is normative. Before deleting the old
code, add characterization tests that run against it and record:

1. bidirectional TypeScript assignability for `typeof appsRouter` and
   `typeof deployApp`;
2. router behavior, including URL rewriting, all methods, headers, limits,
   duplicate cookies, bodyless responses, and error mapping;
3. deploy behavior for directory and in-memory inputs, injected clients,
   namespace creation, retry behavior, and exact result keys; and
4. the old generated declaration and packed export map as an explicit removal
   snapshot, not a test expected to match the new two-export package.

Commit those tests while the old code still passes. Then delete the old
implementation in the same JJ stack and rebuild from empty internals. JJ
history is the archive; no legacy source tree, compatibility adapter, or dead
feature flag remains in the package.

After the rewrite, the root module exposes only the two runtime values. Internal
types may appear in declarations only as needed to type those values. The
package removes the `./advanced` export.

## Application contract

The supported build output is one bundled ESM module with this shape:

```ts
export default {
	async fetch(request: Request): Promise<Response> {
		return new Response("ok");
	},
};
```

The bundle may contain dependencies resolved by the existing builder, but it
must be self-contained, deterministic, and executable from a read-only mounted
artifact. The dispatcher rejects any other default-export shape with a typed
Dynamic Apps error.

Request and response bodies are buffered. The application receives a real
Fetch API `Request`; the host receives a real `Response`. The serialization
envelope carries status, status text, ordered header pairs, and base64 body
bytes so duplicate `Set-Cookie` headers and arbitrary bytes survive the V8
boundary. The 16 KiB absolute-URL limit, 256-byte method limit, 64 KiB
request-header limit, and 1 MiB body limit guarantee the complete encoded input
fits the 2 MiB evaluation cap.

## State actor and schema

Keep the private actor identity `agentOSAppsApp` and the key `[appId]`. This is
required by `deployApp`'s structural injected-client contract. The actor is a
state and artifact authority only; it never serves an application request.

The authoritative logical state remains:

```ts
interface AppState {
	activeRelease: string | null;
	namespace: string | null;
	revision: number;
}
```

No schema change is required for the rewrite. Retain the existing
`agentos_apps_releases`, `agentos_apps_release_files`, and
`agentos_apps_artifact_chunks` tables and the columns needed by the retained
deploy contract. `release_files` and obsolete release columns may remain unused;
recreating a slimmer version of the same tables is not worth a migration. Old
scaler and replica data is not read, migrated, or deleted by this preview.

The existing deployment identity and persistence format stay in place. The
only artifact-format change is a versioned builder/runner identity so an old
Node HTTP-server bundle cannot collide with a new direct-execution bundle. The
current deployment action still:

- derives a release ID from normalized source and deployment metadata;
- builds in its sandboxed agentOS VM;
- writes the resulting AOSP package to the existing fixed-size artifact chunks;
- records release metadata and status in the existing tables; and
- activates the ready release by updating `activeRelease` and `revision`.

Do not add a caller-side builder, a second upload protocol, new release tables,
or new public deployment credentials. This keeps build isolation, retries,
artifact persistence, `createNamespace`, activation, rollback, and the
`Deployment` result behavior on the already-shipped path.

Before deletion, capture a real 0.2.15 actor database fixture; the existing
`legacy-0.2.15.json` file is metadata, not a database. The new actor must open
both a clean database and that captured database without destructive migration.
A legacy artifact lacks the `direct-v1/main.mjs` sentinel and is reported as not
deployed until a new-format release is activated; serving existing deployments
across the preview upgrade is explicitly not required.

The private actor surface retains the current deployment and artifact actions,
with serving-only actions removed:

- `deploy(input)`: the same serialized actor action reached by the exact
  retained `getOrCreate([appId]).deploy(input)` call. It validates source,
  creates the sandboxed build VM, installs/builds/bundles, persists chunks,
  validates the direct module, marks it ready, activates it, emits
  `releaseActivated`, and returns the existing five-field deployment result.
- `resolveDeployment()`: return the active release, monotonic revision,
  artifact hash/size, entrypoint, namespace, regions, scaling metadata, and
  compatibility pool. Return not-deployed when no compatible ready release is
  active. It no longer returns a scaler key for request routing.
- `getArtifactManifest(release)`: return immutable release metadata, format
  sentinel, and ordered chunk descriptors. Exact builder/minimum ABI data is
  verified from the hash-protected artifact manifest during preparation.
- `readArtifactChunk(release, chunkIndex)`: return one bounded immutable chunk.
- `releaseActivated`: event payload
  `{ revision, release, artifactHash, activatedAt }`.

An optional private inspection action may expose state for tests and metrics,
but it must not be exported from the package or used by the request path. The
state actor keeps the current RivetKit connection/authentication behavior in
this preview; inventing a new HMAC protocol would be an unrelated deploy-path
change. Existing server-side Rivet credentials remain required and must never
be logged or exposed to application code.

The retained structural `options.client` call still invokes
`agentOSAppsApp.getOrCreate([appId]).deploy(input)` exactly, so existing
mocks/adapters and default deployment behavior remain compatible.

Deployment for one app and `resolveDeployment` reads share the actor's existing
serialization boundary. Activation ordering is strict:

1. persist and hash every artifact chunk;
2. reread metadata and verify byte/chunk totals and artifact hash;
3. import the final direct module in a disposable clean isolate and validate its
   handler shape without invoking user code;
4. mark the release ready;
5. set `activeRelease` and increment `revision` using the existing durable actor
   state path;
6. emit `releaseActivated`; and
7. return the existing deployment result.

If build, persistence, or validation fails, mark that candidate failed and
leave the previous active release unchanged. This is ordered durability, not a
claim that SQLite, actor state, and event delivery share one transaction.
Crash-injection tests cover every boundary. Retain at most 20 releases using
the existing bounded cleanup, now simplified because replicas never retain
release references.

### Private actor registration

Removing `setup` and `setupApps` means registration becomes an internal package
responsibility. The module owns one private RivetKit registry containing only
`agentOSAppsApp`.

There is one explicit constraint conflict to approve before implementation:
with current RivetKit, the existing host `registry.start()` can start only the
registry whose actor definitions the host owns. If `setupApps` and actor exports
are deleted, that registry cannot contain the private state actor. Therefore
“only `appsRouter`/`deployApp` remain” and “keep the old host
`registry.start()` unchanged” cannot both be true. This spec selects the
smallest two-export solution below: local `startAndWait()` plus the serverless
direct-fetch handler. The alternative is to retain a third bootstrap API or add
a new RivetKit registry-composition feature, both contrary to the requested
minimal rewrite.

- In local/envoy mode, the package invokes one memoized
  `privateRegistry.startAndWait()` before its first actor operation. This starts
  the Engine connection and opens no HTTP listener.
- The Hono route table inside `appsRouter` contains application routes only.
  Its direct `.fetch(request)` entrypoint has a thin absolute-path dispatcher:
  only a request carrying the host-added private dispatch sentinel at exact
  root `/api/rivet` or `/api/rivet/*` enters registry handling; every other path
  calls the ordinary Hono fetch implementation. In serverless mode the dispatch
  calls `privateRegistry.handler(request)`. In local/envoy mode, health and
  metadata use `privateRegistry.routes` and no call to `handler()` is allowed,
  because that would create a second serverless runtime beside
  `startAndWait()`. It never starts a second listener.
- A prefix-mounted host uses the exact topology below:

  ```ts
  const server = new Hono();

  const dispatchToAppsRegistry = (request: Request) => {
    const headers = new Headers(request.headers);
    headers.set("x-agentos-app-registry-dispatch", "1");
    return appsRouter.fetch(new Request(request, { headers }));
  };

  // Absolute Engine callback path; register before unrelated catch-alls.
  server.all("/api/rivet", (c) => dispatchToAppsRegistry(c.req.raw));
  server.all("/api/rivet/*", (c) => dispatchToAppsRegistry(c.req.raw));

  // User requests remain /apps/:appId/*.
  server.route("/apps", appsRouter);
  ```

  `appsRouter` consumes/removes the private sentinel before either registry or
  application handling. The explicit host routes invoke the direct `.fetch`
  dispatcher. Hono's
  `server.route()` copies only the application route table, not that dispatcher,
  so valid app ID `api` and its `/rivet` path remain ordinary application
  routes under `/apps/api/rivet`. Mounting `appsRouter` again at `/` is
  forbidden: its `/:appId/*` route would expose apps at root and could consume
  unrelated host routes.

Subject to that approval, this host bootstrap is the one necessary deploy-path
change caused by deleting `setup`/`setupApps`: the old host `registry.start()`
cannot register a private actor that is no longer exported. The Dockerfile,
Compute CLI command, port, namespace, and public application URLs do not change.

Before deleting the legacy actor files, a phase-zero spike must prove this exact
topology against a local Engine and `RIVETKIT_RUNTIME_MODE=serverless`: callback
health and metadata work at root, an app route works under `/apps`, an unrelated
root route still works, `/apps/api/rivet` reaches valid app ID `api` rather than
the private callback, and only one HTTP listener is open. If that spike fails,
stop and record the RivetKit friction; do not restore a third public setup API
or silently ship a different mount contract.

## Deployment flow

The caller-facing deployment path does not change: callers still pass a source
directory or generated files to `deployApp` and receive the same five result
fields. An injected adapter sees exactly the locked
`getOrCreate([appId]).deploy(input)` call. The default handle remains the
existing RivetKit client to the app state actor.

The retained internal deployment path is:

```text
deployApp
  -> normalize source and resolve/provision namespace exactly as today
  -> agentOSAppsApp[appId].deploy
      -> validate source and deployment metadata
      -> create the existing ephemeral sandboxed agentOS build VM
      -> install/build/bundle/package inside that VM
      -> persist the package in existing release/chunk tables
      -> validate direct-module import in a disposable clean isolate
      -> mark ready, activate revision, and broadcast invalidation
  -> return Deployment
```

The deployment split is explicit:

| Retain unchanged | Narrow serving-related change |
| --- | --- |
| `deployApp` inputs, options, injected-client call, retries, namespace handling, and result | generated runner exports a direct dispatcher instead of listening on HTTP |
| app actor `deploy` action and per-app serialization | direct-module validation replaces replica warmup validation |
| sandboxed source install/build/bundle/package pipeline | activation emits one monotonic cache-invalidation event |
| release/artifact tables, chunking, retention, active pointer, and rollback | scaler rollout, replica warmup, and replica retirement are deleted |

Untrusted `npm install`, lifecycle scripts, `npm run build`, bundling, and
packaging continue to run only inside that resource-bounded build VM, never in
the Compute host process. Preserve the current filesystem, timeout, artifact
limits, networking policy, cleanup, retry, and failure semantics unless a
correctness test proves a serving-format change requires a narrow adjustment.
The app actor remains the deployment serialization and state boundary.

The retained source-to-bundle behavior and limits are normative in
`API_CONTRACT.md`. The builder's output is direct ESM `main.mjs`, not the old
HTTP-server runner. Concretely, retain the existing builder and AOSP packaging
pipeline but replace its generated host wrapper. Validation imports the final
persisted-format bundle in a fresh session, verifies the default object has a
callable `fetch`, then destroys the session before activation. It does not
invoke the handler.

There is no scaler creation, replica preparation, warmup request, admission
lease, region rollout, or retirement. `regions` and `scaling` remain validated,
stored, and returned as compatibility metadata, but do not affect local
execution placement or capacity. `createNamespace` remains functional.

Compute must still be able to call the private registry at the host's root
`/api/rivet` path. `createNamespace: true` remains part of the library contract,
but Cloud qualification in this work never uses it because every mutation is
restricted to `dynamic-apps-ben-562e-production-sqac`.

## Executor and request lifecycle

### Prepared runtime

A prepared runtime is keyed by:

```text
artifactHash + builderFormatVersion + agentOS/runtime ABI version
```

Preparing it means downloading and verifying immutable chunks, materializing or
mounting the bundled artifact read-only, and creating/configuring a reusable
agentOS VM handle. It must not evaluate the application module or retain
application JavaScript state.

Use the existing `AgentOs.javascript.evaluate` path first. agentOS already
creates a new JavaScript session/V8 isolate for an evaluation without a
`contextId`. The evaluation receives a frozen `inputs` value and runs a small
host-controlled dispatcher that:

1. imports the bundled entrypoint;
2. reconstructs the `Request`;
3. invokes `default.fetch(request)`;
4. fully reads and bounds the `Response`; and
5. returns the serializable response envelope.

The request does not start the old guest HTTP server. Keep the packaged
artifact immutable and use the existing agentOS evaluator subprocess and
permission model; the evaluator itself requires process-spawn permission, so
the implementation must not claim that all child-process capability is
disabled. Writable filesystem behavior and resource teardown are tested and
documented as observed rather than expanded into an unrelated agentOS security
rewrite.

The current public agentOS API exposes no V8 heap-snapshot create/restore
operation. Therefore the preview supports two honest modes:

- `fresh`: omit `contextId`; agentOS creates and tears down the JavaScript
  session on the request path.
- `prewarm`: maintain a bounded pool of clean contexts. Each slot is created
  and evaluated once with only a host no-op before it becomes available. A
  request leases the slot exactly once, imports and runs the application, then
  removes that dirty slot from circulation. The refill path resets or deletes
  it, performs the host no-op again, and only then returns the clean slot to the
  pool. Refill is asynchronous to the completed response but consumes bounded
  executor capacity.

There is deliberately no `snapshot` mode until agentOS provides a public,
tested heap-snapshot capability. If `snapshot` is configured, startup fails
with a clear unsupported-mode error instead of silently treating context reset
as snapshot restore. A future snapshot mode must pass the same global/module
isolation tests and memory accounting before becoming selectable.

### Cache

Maintain two bounded process-local maps:

```ts
appId -> {
	release: string;
	revision: number;
	highestObservedRevision: number;
	resolveEpoch: number;
	artifactHash: string;
	regions: string[];
	subscription: Subscription;
	refs: number;
	verified: boolean;
	lastUsedAt: number;
}

runtimeKey -> {
	preparedVm: AgentOs;
	artifactBytes: number;
	estimatedResidentBytes: number;
	cleanContexts: ContextSlot[];
	refillingContexts: number;
	refs: number;
	stale: boolean;
	lastUsedAt: number;
}
```

Initial defaults are private configuration, not package exports:

| Environment variable | Default | Meaning |
| --- | ---: | --- |
| `DYNAMIC_APPS_ISOLATE_MODE` | `prewarm` | `fresh` or clean single-use `prewarm`; `snapshot` fails closed |
| `DYNAMIC_APPS_ISOLATE_POOL_SIZE` | `2` | maximum clean/refilling contexts per prepared runtime |
| `DYNAMIC_APPS_ISOLATE_IDLE_TTL_MS` | `30000` | retire unused clean contexts after this interval |
| `DYNAMIC_APPS_RUNTIME_CACHE_MAX_ENTRIES` | `16` | maximum prepared runtime entries |
| `DYNAMIC_APPS_RUNTIME_CACHE_MAX_BYTES` | `268435456` | aggregate cached artifact-byte cap (256 MiB) |
| `DYNAMIC_APPS_RUNTIME_CACHE_IDLE_TTL_MS` | `900000` | prepared runtime/app mapping idle TTL (15 minutes) |
| `DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT` | `70` | stop admission/refill above this cgroup-memory fraction |
| `DYNAMIC_APPS_EXECUTION_CONCURRENCY` | container CPU count | maximum active evaluations, minimum one |
| `DYNAMIC_APPS_EXECUTION_QUEUE_SIZE` | `64` | maximum queued requests |
| `DYNAMIC_APPS_EXECUTION_QUEUE_WAIT_MS` | `5000` | maximum queue wait |
| `DYNAMIC_APPS_EXECUTION_TIMEOUT_MS` | `30000` | per-evaluation wall timeout |

Numeric values are strictly parsed and range checked at startup. Invalid,
negative, NaN, or unreasonably large values fail closed. Pool size zero is
accepted as an alias for `fresh` to simplify memory-constrained deployments.

Preparation is singleflight per runtime key. Cache admission checks both entry
and artifact-byte limits plus measured prepared-VM/native resident footprint.
Before admission, evict LRU zero-ref entries until the process is below the
memory high-water mark; reject preparation rather than risk the cgroup hard
limit. LRU/TTL eviction marks a runtime stale and disposes it only after
`refs === 0`. Subscription entries are bounded by the same app LRU/TTL and have
their own refs, so requests for unbounded app IDs cannot leak connections or
close one while resolve/prefetch is active.

Preparation uses release-keyed temporary directories and atomic rename. Failed
preparation removes its directory; process startup removes orphan directories
that do not correspond to a live cache entry.

Clean-context refill obeys the same global execution semaphore and memory
high-water gate as foreground work. It never creates more than the configured
per-runtime pool size, never makes a dirty slot available, and is cancelled
when the runtime becomes stale. Under pressure, the pool shrinks first; the
request then queues for a later clean slot or, when explicitly configured as
`fresh`, pays isolate creation inline. The runtime cache, clean-context pool,
and their memory limits are independent so artifact TTL can remain long while
clean isolates remain short-lived.

### Resolve and invalidation protocol

The first request for an app:

1. acquires a ref on the app entry, creates/subscribes to the app actor, and
   waits for subscription readiness;
2. captures the current `resolveEpoch` and calls `resolveDeployment` after
   readiness;
3. under the app mutex, installs the result only if the epoch is unchanged and
   its revision is at least `highestObservedRevision`; otherwise it discards the
   result and retries;
4. downloads/verifies/prepares the runtime through singleflight; and
5. executes the request.

Subscribing before reading closes the read/subscribe race. The event is only an
invalidation signal: on a revision above `highestObservedRevision`, it updates
that high-water mark, increments `resolveEpoch`, immediately marks the app
mapping unverified, and blocks new admissions. It then performs an authenticated
`resolveDeployment()` to obtain release, hash, and regions. A resolve captures
the epoch before its call and may install only when the epoch is still equal and
the returned revision is at least the high-water mark; otherwise it is stale,
is discarded, and retries. This prevents a revision N resolve from installing
after a revision N+1 event. Only a complete authoritative result may be
installed.

Under one per-app
cache mutex, a request validates `{ revision, runtimeKey }` and acquires its
runtime lease/ref at one linearization point. Only after holding that ref may it
wait on the execution semaphore. Under the same mutex, a higher event revision
begins the verified mapping replacement and a best-effort singleflight
prefetch. Requests linearized before invalidation may finish on the old
immutable artifact; no request can linearize between invalidation and the
verified swap. Queue timeout/cancellation releases the ref.

Events at or below `highestObservedRevision` are ignored. A completed preparation
rechecks `{ appId, revision, runtimeKey }` under the mutex before it can install
or execute, so an old singleflight cannot win after rapid deployments.

Rivet subscription events are not treated as replayable. `onClose` increments
`resolveEpoch`, marks the mapping unverified, and blocks new admissions; every
`onOpen` captures the new epoch and calls `resolveDeployment` again before the
mapping becomes current. This applies to every reconnect, not just a one-shot
ready promise. If prefetch fails, the mapping remains on the new revision but
its next request retries preparation; it must never silently fall back to the
old release.

### Per-request lifecycle

On a cache hit:

1. enforce request limits and strip private/hop-by-hop headers;
2. atomically acquire the current revision's runtime lease/reference;
3. enter the bounded execution queue and acquire the semaphore;
4. either create a fresh no-`contextId` evaluation or lease one clean prewarmed
   context according to `DYNAMIC_APPS_ISOLATE_MODE`;
5. invoke the dispatcher with immutable serialized request inputs;
6. validate the response envelope and enforce the 4 MiB response limit;
7. synchronously retire the dirty session/context from availability;
8. decrement the reference and release capacity; and
9. construct the host `Response`.

In `prewarm` mode, reset/delete plus no-op refill starts only after step 7 and
is separately timed as background recycle. The slot cannot serve another
request until refill finishes. In `fresh` mode, isolate creation and destruction
remain entirely inside the request duration.

Cancellation and timeout propagate into the evaluation and still await session
teardown. The base64/JSON transport limit includes headroom above 5.34 MiB so a
valid 4 MiB logical response, and above 1.34 MiB so a valid 1 MiB logical
request, cannot fail due only to encoding expansion. Request/response headers
are limited to 256 pairs and 64 KiB total UTF-8 names/values; response status
text is limited to 1 KiB. Evaluation input is capped at 2 MiB and output at 6
MiB. A poisoned or crashed
prepared VM is evicted, not reused. Prepared VM/artifact eviction and bounded
clean-context refill may run outside the completed response after reference
count reaches zero. Dirty contexts are removed from availability before the
response finishes and the refill queue is strictly bounded by pool size.

New buffered-execution failures use stable private codes and the retained JSON
error envelope: `agentos_apps_no_capacity` for a full or expired queue,
`agentos_apps_execution_timeout`, `agentos_apps_execution_limit`,
`agentos_apps_invalid_handler`, and
`agentos_apps_invalid_response`; the existing
`agentos_apps_response_limit` covers a body over 4 MiB and
`agentos_apps_response_header_limit` covers the response envelope. They map to 503. A
deployment-time import/export validation failure uses
`agentos_apps_invalid_handler` and never activates the candidate. An
application handler exception retains the baseline plain-text 500
`Internal Server Error` response.

## Observability

Every request receives a correlation ID used in structured logs. Logs and
benchmark records capture phase durations without exposing secrets or
request/response bodies:

- router parsing and input buffering;
- actor subscribe and active-release resolution;
- artifact manifest lookup;
- artifact chunk download;
- artifact verification/materialization;
- prepared VM configuration;
- execution queue wait;
- V8 session/isolate creation;
- clean-context lease and background recycle;
- dispatcher startup and application module import;
- application `fetch`;
- response serialization/body read;
- V8 session destruction;
- outer server duration; and
- cache outcome (`app-hit`, `runtime-hit`, `singleflight-wait`, `miss`, or
  `evicted`).

Also export gauges/counters for cache entries/bytes, app subscriptions, active
evaluations, queue depth/rejections, event revision lag, VM evictions, session
  create/destroy/reset/refill totals, clean/dirty/refilling context counts,
  errors by phase, process RSS, open file descriptors, and temporary artifact
  bytes.

The phase sources are explicit:

- host spans measure router/buffering, actor calls, download/materialization,
  queue wait, prepared-VM setup, and total `evaluate`/server duration;
- the dispatcher envelope measures module import, handler await, and response
  read/serialization using the same monotonic clock; and
- an optional benchmark-only agentOS diagnostic hook measures session/isolate
  creation and synchronous destruction. Without that hook these two are
  reported honestly as combined agentOS/session residual, never fabricated by
  subtracting independent percentile values.

Only cache outcome, total server/evaluation duration, active/queued work,
rejections/errors, cache memory, and event lag are required as permanent
production metrics for the preview. The more detailed phase records may remain
structured benchmark logs. Compute qualification must prove the chosen metrics
are actually scrapeable or queryable, not merely emitted to a disconnected
sink.

Detailed timing response headers are benchmark-only and disabled in normal
production use. The retained public API does not promise them.

## Correctness test matrix

### Before deletion

- Old-code API type, router, and deploy characterization tests plus the explicit
  old declaration/export removal snapshot described above.
- A real 0.2.15 actor database fixture captured from the baseline and checked
  into test fixtures; the existing JSON metadata file is not sufficient.
- A registration spike proving `appsRouter` alone exposes the private state
  actor to both a local Rivet Engine and a simulated serverless handler.
- An evaluator spike proving no-`contextId` and clean-prewarmed-context module
  import, default-fetch invocation, response fidelity, global cleanup, and
  measured create/reset/refill behavior using the exact package/sidecar
  candidate.
- The same zero-dependency benchmark fixture run on baseline `baca1719` locally
  and in Compute, with raw samples retained outside the code being deleted.

### Unit and integration

- app ID, source path/file count/byte limits, scaling/region normalization, and
  deterministic release preparation;
- successful activation, failed-build rollback, incomplete/corrupt artifact,
  20-release retention, same-release redeploy, and crash injection after
  artifact commit, readiness, pointer mutation, event broadcast, and action
  response;
- every accepted HTTP method, mount prefix, redirect/query behavior, binary bodies,
  duplicate cookies, header stripping, all bodyless statuses, and exact error
  JSON/status mapping;
- global counter and random-instance fixtures proving every request sees fresh
  module/global state;
- exact-boundary 16 KiB URL, 256-byte method, 64 KiB header, 1 MiB request, and
  4 MiB response fixtures including encoded transport overhead and
  one-byte-over failures;
- fixtures proving application module/global state and environment mutation do
  not persist between requests in either mode, and a dirty context is never
  returned to the clean pool;
- cache hit with zero actor calls, singleflight miss, LRU/TTL/byte eviction,
  clean-pool TTL/refill/memory-pressure shrink, poisoned VM replacement,
  unsupported snapshot rejection, invalid configuration, and bounded overload;
- deploy/request race, invalidation during an in-flight old request, monotonic
  event rejection, reconnect after a missed event, failed prefetch, and rapid
  successive deploys;
- client cancellation, execution timeout, thrown/rejected handlers, invalid
  export/response, OOM/CPU limit, and guaranteed session teardown;
- restart with empty process cache proving durable actor state and artifact
  recovery;
- clean and legacy-0.2.15 actor databases, rejecting legacy artifact format
  until a new deployment without destructive migration;
- the current Node-targeted builder output and `node:module` banner importing
  correctly in the direct evaluator.

Run package tests, real E2E tests, type checks, build, boundary checks, lint,
packed-package tests, and every docs/example build explicitly. The workspace's
ordinary `pnpm test` is not a substitute for the real E2E or example commands.

## Performance and load-test plan

Use the same deterministic zero-dependency handler, whose final artifact is at
most 10 KiB, for baseline and rewrite comparisons. Record raw per-request phase
samples and report sample count, concurrency, warmup, p50/p95/p99/max,
throughput, failures, runtime versions, CPU/memory limits, region, artifact
size, and client-to-server location. Every response contains and validates a
unique request ID so concurrency cannot hide cross-request corruption. Do not
add phase percentiles together; derive total duration from its own samples.

Run these cases separately:

1. router no-op control;
2. actor-ready artifact miss: live state actor, empty app/runtime caches,
   resolve + download + prepare + fresh isolate;
3. app mapping hit but prepared-runtime miss after explicit runtime eviction;
4. steady cache hit in `fresh` mode;
5. steady cache hit in `prewarm` mode, with request and background recycle
   durations reported separately;
6. pool-drain behavior at concurrency above the configured clean pool;
7. steady cache hit during a redeploy/invalidation;
8. executor-process cold: a fresh child process for each request while the
   state actor remains ready; and
9. full Compute replacement/wake, reported separately from executor cold.

Before deletion, record the current `baca1719` actor architecture with this
fixture and the same environment. After the rewrite, take at least 1,000
post-warmup samples for every hot latency gate and at least 100 independent
executor-process-cold samples. Compare both server duration and public
end-to-end duration with the preserved warm baseline.

For local tests, use a local Rivet Engine and run sequential (`c=1`),
below-saturation `c=8`, and below-saturation `c=32` profiles plus at least 10,000
requests for stability. Run a separate open-loop overload profile above
capacity; it must return the typed 503 capacity error within the five-second
queue deadline rather than grow work without bound. Also run multi-app churn
past 16 entries and 256 MiB to force both LRU and memory-pressure eviction.

Sample RSS, cgroup memory, open FDs, temporary bytes, subscriptions, cache
entries, queue depth, runtime refs, and created/destroyed sessions before,
during, and after each run. After drain:

- active evaluations, queue depth, and runtime refs equal zero;
- created sessions equal destroyed sessions;
- FDs and temporary bytes return within 5% of the post-warmup baseline;
- median RSS in the final 20% is no more than 5% above the preceding 20%; and
- RSS remains below 70% of the cgroup memory limit.

Hard latency gates for the zero-dependency fixture are:

| Case | Gate |
| --- | --- |
| Prewarmed cache-hit server duration, `c=1` | p50 <= 25 ms and p95 <= 50 ms |
| Fresh-isolate cache-hit server duration, `c=1` | measured and attributed; target p50 <= 100 ms and p95 <= 250 ms |
| Cache-hit actor calls | exactly 0 per request |
| Executor-process-cold server duration, `c=1`, <=10 KiB artifact | p50 <= 150 ms and p95 <= 250 ms |
| Invalidation after event processed | no newly admitted stale requests |
| Invalidation event observation | <= 1 second |
| Below-saturation `c=8` and `c=32` | 100% correct 2xx responses; bounded queue/resources |
| Open-loop overload | bounded typed 503s; no crash or unbounded growth |
| 10,000-request stability | all numeric drain/plateau gates above pass |

The 50 ms cache-hit p95 is a sequential hot-path gate, not a promise at
concurrency 32 on a one-CPU container. Concurrent runs report queue delay,
service time, throughput, and rejection behavior separately.

The phase-zero local evaluator microbenchmark on the currently resolved
agentOS candidate measured a trivial expression at roughly 69 ms p50 for a
fresh no-context evaluation, roughly 6 ms p50 for the first application
evaluation in a clean prewarmed context, and roughly 65 ms p50 for reset plus
no-op refill. These are feasibility observations, not release benchmark
results. They explain why recycle must be measured separately and why a pool
can improve small-scale latency while reducing throughput when it drains.

The initial engineering budget for a **prewarmed** cache-hit request is
directional, not a substitute for measurement:

| Component | Expected p50 budget |
| --- | ---: |
| Router validation, buffering, and envelope creation | 1-3 ms |
| Queue wait at `c=1` | <1 ms |
| Clean-context lease | <1 ms |
| Dispatcher startup and cached-bundle module import | 2-8 ms |
| Zero-dependency application `fetch` | <1 ms |
| Response serialization and session destruction | 3-8 ms |
| **Cache-hit server duration** | **10-25 ms** |

Fresh-mode and refill budgets use the observed agentOS evaluation floor, not an
unrelated native V8 constructor benchmark. Instrumentation must split isolate
creation from module import and destruction when a diagnostic hook is present;
otherwise report the combined evaluation/session duration and do not claim an
isolate-only number.

Artifact download is always timed and reported separately. It is not optimized
until the prepared-runtime hot path passes and the cold breakdown shows it is a
meaningful remaining component.

## Rivet Compute qualification

Build and run the production Docker image locally with
`RIVETKIT_RUNTIME_MODE=serverless`, expose the configured port, and verify
`/api/rivet/health` before deployment.

Deploy only to namespace
`dynamic-apps-ben-562e-production-sqac`; every deploy command must include:

```text
--namespace dynamic-apps-ben-562e-production-sqac
```

Never deploy or mutate the default `production` namespace. Before deployment,
prove that Compute and the deploy caller use the existing namespace
configuration without logging credentials. No new Dynamic Apps secret is added
to the CLI command or Docker image. After deployment:

1. assert the resolved project/namespace is exactly the allowed namespace
   before every mutating CLI or API operation;
2. verify public health and the private actor callback handler;
3. deploy the fixture without `createNamespace` and verify the state actor
   persists its release;
4. measure explicit actor-ready artifact miss and steady cache hit requests;
5. take 100 executor-process-cold samples with the benchmark child-process
   harness;
6. redeploy the fixture while load is active and verify event invalidation;
7. restart/replace the Compute process and verify empty-cache recovery; and
8. run the same below-saturation `c=1`, `c=8`, `c=32`, open-loop overload,
   multi-app churn, and 10,000-request stability matrix.

Report public end-to-end and server duration separately. The server hot-path
gate remains p50 <= 25 ms and p95 <= 50 ms in Compute. If it misses, use the
phase data to decide whether the floor is V8/session creation, module import,
serialization, queueing, or host overhead before changing architecture.

## Documentation and examples

Update the README, docs site, examples, and package metadata to:

- describe durable actor state plus process-local prepared-runtime caching and
  per-request V8 isolation;
- show the two-export API and private `/api/rivet` host route;
- document the default export object/`fetch` application contract and removed
  WebSocket, streaming, static-only, and nested-RivetKit behavior;
- explain deployment activation, invalidation, cache bounds, limits,
  cancellation, and restart behavior;
- document the existing server-side Rivet credential requirement and the rule
  that credentials may not be exposed to app code;
- document every isolate/runtime cache environment variable, fresh versus
  clean-prewarm semantics, memory behavior, and the absence of true snapshot
  support in the current agentOS API;
- remove warm replica, scaler, region placement, custom actor, inspector, and
  `setup`/`setupApps` examples;
- update routing, deploy, state, troubleshooting, authentication, and reference
  pages; and
- publish a reproducible benchmark report with phase breakdowns and raw-data
  location.

Keep the project reference to <https://rivet.dev/llms.txt> in `AGENTS.md` for
future RivetKit work.

Update `scripts/check-boundaries.mjs` and `scripts/test-packed.mjs` so they
assert the new dependency boundaries and exact two-export package. Remove or
rewrite examples whose purpose depends on deleted features; do not leave broken
legacy examples hidden from CI.

## Preview release and rollback

Dependency qualification comes first:

1. Choose explicit prerelease versions for Dynamic Apps and every changed
   package, including `@rivet-dev/dynamic-apps-builder`, before qualification;
   do not defer version selection until publish time.
2. Use the currently resolved agentOS only for the phase-zero feasibility
   prototype.
3. Before baseline or source qualification, remove the workspace override of
   `@rivet-dev/agentos-core`. If the published 0.2.15 dependency is sufficient,
   regenerate the lockfile against exactly 0.2.15.
4. The first preview must use only the existing public agentOS evaluator API.
   A later true-snapshot mode requires a separately published and exactly
   pinned agentOS version with that capability.
5. If the builder is unchanged, pin its existing exact registry version. If it
   changes, set the Dynamic Apps packed manifest to the chosen exact builder
   prerelease. Before that builder exists in the registry, pack both candidates
   and install both tarballs together into a clean workspace-free consumer, as
   `scripts/test-packed.mjs` does, so dependency resolution cannot fetch the old
   builder. Alternatively publish the builder preview first and pin it exactly.
6. Assert the resolved package, builder, and sidecar versions with the package
   manager; inspect every packed manifest; record tarball SHA-256 values; and
   use those same tarball bytes or exact already-published dependencies for
   local consumer, Docker, and Compute qualification. Do not qualify against a
   workspace override/check-out and publish different bits later.

After local and Compute source qualification passes:

1. publish the already-qualified builder tarball first if it changed, using its
   chosen prerelease and the `preview` dist-tag;
2. publish the already-qualified Dynamic Apps tarball with `dist_tag=preview`
   (never `auto` or `latest`);
3. install both exact preview versions into a clean registry-only consumer and
   verify their tarball hashes/manifests match the qualified candidates;
4. rerun smoke/E2E, build its production container, deploy it to the same
   allowed namespace after an exact-namespace preflight, and rerun the hot-path
   smoke benchmark; and
5. record exact package versions, sidecar version, image digest, benchmark
   report, and JJ revision.

Do not publish a stable version or move `latest` in this work.

Before the first new Compute deploy, record the currently active release,
source fixture, known-good image digest, package version, and JJ revision. The
old image cannot execute a newly built direct-module artifact. The rollback
procedure therefore is:

1. assert the exact allowed namespace;
2. redeploy the recorded old image;
3. redeploy the recorded source with the old package so it activates an
   old-format release; and
4. verify state access and application requests.

Drill this sequence once in `dynamic-apps-ben-562e-production-sqac` before the
preview is cut and record recovery time. npm rollback is separate: pin the
previous known-good version in the consumer and rebuild; never mutate an
already-published npm version. The deleted implementation remains in JJ history
and is not carried forward in source.

## Implementation sequence and stop gates

Shape the JJ stack as a non-empty spec/API revision, an old-code
characterization/baseline revision, and a destructive-rewrite revision. Keep
the existing bookmark on the stack head and do not leave an empty revision as a
stack boundary.

1. **Lock the API, hosting, and evaluator.** Land `API_CONTRACT.md`, old-code
   characterization tests/baseline samples, the reviewed removal snapshot, the
   real 0.2.15 actor database fixture, the exact local/serverless
   private-registry spike, and the fresh/prewarmed evaluator spike using
   installable dependency bits. Stop if the retained surface, handler/startup
   topology, response fidelity, or per-request isolation cannot be proved.
2. **Delete the internals.** Remove old app/scaler/replica routing, setup
   exports, inspectors, proxies, leases, streaming bridges, and obsolete tests.
3. **Retain deploy/state.** Lift the existing deployment helpers and app actor
   into the new minimal package, change only the generated direct runner,
   validation/activation handoff, serving-action removal, and invalidation
   event. Keep the current schema and `deploy` call path.
4. **Rebuild execution.** Implement both proven `fresh` and clean single-use
   `prewarm` paths with immutable prepared runtimes, bounded refill, numeric
   and memory limits, race-safe cache leases, and reconnect-safe invalidation.
5. **Restore the router/deploy contract.** Make all characterization and new
   narrowing tests pass without compatibility shims.
6. **Qualify locally.** Run complete correctness, Docker/serverless, phase
   benchmark, concurrency, invalidation-under-load, and stability tests.
7. **Qualify on Compute.** Deploy only to the named namespace and repeat the
   full benchmark/load/restart/invalidation matrix.
8. **Production hardening.** Resolve measured failures, rerun the entire
   affected matrix, drill rollback, finish docs/examples/observability, and
   verify no legacy exports or dead code remain.
9. **Cut and verify preview.** Publish with the preview dist-tag and qualify a
   clean registry consumer in the same Cloud namespace.

No later phase waives an earlier gate. A missed performance target produces a
phase-attributed investigation and another measured pass; it does not justify
silently restoring the old actor-per-request graph.
