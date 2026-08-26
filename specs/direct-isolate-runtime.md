# Direct-isolate Dynamic Apps rewrite

Status: direct request and deployed-app actor runtimes implemented and qualified locally and on Rivet Compute  
Public API baseline: `packages/dynamic-apps/API_CONTRACT.md`  
Baseline implementation: JJ `xuymorrq`, commit `baca1719`

## Executive decision

Keep the existing deployment implementation and durable per-app actor. Delete
the scaler/replica serving graph and serve requests in the edge process with
`isolated-vm`.

```text
deployApp
  -> agentOSAppsApp[appId]
  -> existing sandboxed agentOS build VM and apps-builder
  -> existing AOSP package, release rows, chunks, activation, rollback
  -> releaseActivated(revision, release, artifactHash)

first request in one edge process
  -> appsRouter
  -> connect to state actor and resolve active release
  -> read and verify artifact chunks
  -> extract direct IIFE bundle and optionally create a V8 heap snapshot
  -> execute request in a bounded isolate

cache-hit request
  -> appsRouter
  -> process-local app/runtime cache (zero actor calls)
  -> execute according to fresh, snapshot, or prewarm mode

deployment invalidation
  -> releaseActivated event
  -> atomically invalidate app mapping
  -> resolve and prepare the newly active immutable release
  -> retire the old runtime after in-flight references drain
```

The deployment actor remains the source of durable state and cache invalidation.
It is not on the cache-hit request path.

## Scope

The package root exposes exactly two runtime values:

```ts
export { appsRouter };
export { deployApp };
```

The exact input, output, routing, retry, error, and limit behavior is locked in
`packages/dynamic-apps/API_CONTRACT.md`. `setup`, `setupApps`,
`createAppsRouter`, `./advanced`, actor definitions, error classes, inspector
APIs, and all other old exports are intentionally removed. JJ history is the
archive; no compatibility implementation remains.

Ordinary application HTTP remains a buffered direct-isolate request/response
path. A deployment that depends on `rivetkit` may additionally export a
registry for app-defined actors. WebSockets, streaming ordinary app bodies,
static-only packages, Node builtins in application code, and dirty
JavaScript-context reuse remain out of scope.

## Retained deployment implementation

The following implementation remains in place:

- `deployApp` directory/generated-file inputs, normalization, namespace
  behavior, injected-client structural call, retries, and five-field result;
- existing actors are resolved with `get()` when available before the retained
  `getOrCreate()` fallback, avoiding cross-datacenter creation races during
  idempotent deploys;
- private actor name `agentOSAppsApp` keyed by `[appId]`;
- sandboxed agentOS build VM, install/build timeout and resource limits;
- `@rivet-dev/dynamic-apps-builder` and AOSP packaging;
- release metadata, artifact chunks, active-release pointer, revision, and
  bounded release retention;
- build/import validation before activation;
- failed candidate rollback semantics; and
- monotonic `releaseActivated` events.

There is no schema migration for the rewrite. Existing release/artifact tables
remain. Scaler/replica tables and code are not read or recreated. A release hash
uses a new direct-runtime domain so old HTTP-runner artifacts cannot collide
with direct artifacts.

The builder always emits a self-contained browser-targeted direct IIFE, rejects
application Node builtins, and persists it under `direct/` in the same AOSP
package pipeline. When the app declares a `rivetkit` dependency, it also emits
a platform-linked actor registry bundle under `actor/`. The host supplies its
pinned RivetKit runtime instead of duplicating RivetKit in every app artifact.
Deployment validates the direct bundle before activation and the actor bundle
when its runner starts. An incomplete or invalid artifact never replaces the
prior active release.

## Application and HTTP contract

The app entrypoint must default-export an object with `fetch(request)` (a
default fetch function remains accepted for actor-app compatibility):

```ts
export default {
	async fetch(request: Request): Promise<Response> {
		return new Response("ok");
	},
};
```

The builder installs one internal `globalThis.__dynamicAppDispatch` function.
The host sends a JSON envelope containing URL, method, ordered headers, and an
optional base64 body. The dispatcher reconstructs a fresh request, invokes the
handler, buffers and bounds the response, and returns status, status text,
ordered headers, base64 body, and guest timings.

Private credentials and hop-by-hop headers are stripped before entering the
isolate. The limits are 16 KiB URL, 256-byte method, 256 header pairs/64 KiB,
1 MiB request body, 4 MiB response body, and 1 KiB status text. GET/HEAD and
204/205/304 response-body semantics are enforced by the host.

The first preview provides a deliberately small Fetch-compatible runtime:
`Headers`, `Request`, `Response`, `URL`, `URLSearchParams`, UTF-8 codecs,
base64 helpers, and monotonic `performance.now`. It does not expose host
objects, filesystem, process, environment, network fetch, or Node modules.

## App-defined RivetKit actors

Actor support retains the pre-rewrite user contract without adding a package
root export. An actor-enabled app declares `rivetkit`, exports `registry`, and
may keep calling `registry.start()`; the platform suppresses that call while
loading the artifact:

```ts
import { actor, event, setup } from "rivetkit";

const room = actor({
	state: { count: 0 },
	events: { changed: event<number>() },
	actions: {
		increment(c) {
			c.state.count += 1;
			c.broadcast("changed", c.state.count);
			return c.state.count;
		},
	},
});

export const registry = setup({ use: { room } });
registry.start();
export default { fetch: () => new Response("ok") };
```

`deployApp` continues returning `namespace` and `pool`; an ordinary RivetKit
client uses those fields to create and call the deployed actors. Deployment
provisions the per-app namespace and configures its stable serverless runner
pool only after both bundles validate. Runner-configuration failure rolls back
the active release.

Engine metadata/start callbacks enter the existing `agentOSAppsApp` request
hook through an authenticated callback URL. That actor validates the callback
secret and release, then dispatches the streaming request to a process-local
actor runtime. The runtime extracts only verified `actor/` files and starts one
bounded Node worker-thread V8 isolate per active app release. The worker uses
the host's pinned RivetKit native core, preserves the `/start` response stream,
backpressure, and cancellation, and uses the deployment's namespace/pool.
Ordinary app HTTP never enters this worker and remains eligible for direct
snapshot/prewarm caching.

Actor workers are singleflight, reference counted, bounded by entry count,
heap limit, idle TTL, and cgroup pressure. A release event prevents new actor
callbacks from entering a stale worker; existing actor streams drain or are
terminated at their runner lifecycle boundary. Worker failure poisons that
runtime and fails its open streams instead of reusing it. Defaults are:

| Environment variable | Default |
| --- | ---: |
| `DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES` | `4` |
| `DYNAMIC_APPS_ACTOR_WORKER_HEAP_LIMIT_MB` | `96` |
| `DYNAMIC_APPS_ACTOR_WORKER_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES` | `1048576` |

`DYNAMIC_APPS_CONTROL_TOKEN` optionally supplies a separate credential for
Dynamic Apps control-plane requests independently of `RIVET_ENDPOINT`. The
control token remains host-only, is never copied into an app worker, and is not
used for app actor requests.

The worker receives only the public app-namespace Rivet connection. It does not
receive the host deployment/control credential. This remains one application
trust domain per container, matching the direct-runtime preview boundary.
An authenticated server must provide `RIVET_PUBLIC_ENDPOINT`; activation fails
closed when only a credential-bearing secret endpoint is available.

## Private registry integration

Removing the public setup API makes the package own one private RivetKit
registry containing only `agentOSAppsApp`.

- Local/envoy mode memoizes `privateRegistry.startAndWait()` before the first
  default client operation.
- Serverless mode dispatches Compute callbacks through
  `privateRegistry.handler(request)`.
- The host mounts application routes and the private callback separately:

```ts
const dispatchRegistry = (request: Request) => {
	const headers = new Headers(request.headers);
	headers.set("x-agentos-app-registry-dispatch", "1");
	return appsRouter.fetch(new Request(request, { headers }));
};

server.all("/api/rivet", (c) => dispatchRegistry(c.req.raw));
server.all("/api/rivet/*", (c) => dispatchRegistry(c.req.raw));
server.route("/apps", appsRouter);
```

The Dockerfile, Compute CLI, port, namespace, and public app URL do not change.
This callback mount is the only host integration change required by deleting
the public actor registry.

## Executor and caching

The process keeps two bounded maps:

```text
appId -> actor connection, authoritative revision/release, runtime reference
artifactHash + direct ABI -> verified source, optional snapshot, isolate pool
```

Preparation is singleflight. It connects/subscribes before resolving, validates
the actor manifest, downloads ordered chunks, verifies byte count and SHA-256,
extracts `main.mjs`, and creates a V8 snapshot for snapshot/prewarm modes. A
cache-hit request performs no actor operation.

The subscription protocol is monotonic. An event above the observed revision
increments the entry epoch and immediately removes the verified mapping. A
resolve may install only if its captured epoch remains current and its revision
is at least the event high-water mark. Disconnect/reconnect invalidates and
rereads state, so missed non-replayed events cannot leave a permanent stale
mapping. In-flight requests may finish on their immutable old release; newly
admitted requests cannot use it after invalidation.

### Isolation modes

| Mode | Behavior |
| --- | --- |
| `fresh` | Cache verified source. For every request create an empty isolate, compile/evaluate bootstrap and bundle, run once, and destroy the isolate. |
| `snapshot` | Cache one app-specific V8 heap snapshot. For every request create an isolate from it, create a context, run once, and destroy the isolate. |
| `prewarm` | Cache the artifact, snapshot, and up to N native isolates. Lease one isolate, run its clean context once, synchronously release the dirty context, create a new context from the snapshot, and return the native isolate to the pool. |

No JavaScript context is reused. Module/global state therefore starts from the
snapshot for every request. Reusing the native isolate preserves V8 allocation
and routing machinery without preserving application objects. A timed-out,
invalid, or failed request poisons and destroys its native isolate instead of
returning it to the pool.

The prewarm pool is a cache, not a capacity limit. When concurrency exceeds the
pool, overflow requests create snapshot-backed isolates under the global
execution semaphore. At drain, only N clean native isolates remain. Setting N
to zero changes prewarm to snapshot mode.

Defaults:

| Environment variable | Default |
| --- | ---: |
| `DYNAMIC_APPS_ISOLATE_MODE` | `prewarm` |
| `DYNAMIC_APPS_ISOLATE_POOL_SIZE` | `2` |
| `DYNAMIC_APPS_ISOLATE_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_ISOLATE_HEAP_LIMIT_MB` | `64` |
| `DYNAMIC_APPS_RUNTIME_CACHE_MAX_ENTRIES` | `16` |
| `DYNAMIC_APPS_RUNTIME_CACHE_MAX_BYTES` | `268435456` |
| `DYNAMIC_APPS_RUNTIME_CACHE_IDLE_TTL_MS` | `900000` |
| `DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT` | `70` |
| `DYNAMIC_APPS_EXECUTION_CONCURRENCY` | available CPU count |
| `DYNAMIC_APPS_EXECUTION_QUEUE_SIZE` | `64` |
| `DYNAMIC_APPS_EXECUTION_QUEUE_WAIT_MS` | `5000` |
| `DYNAMIC_APPS_EXECUTION_TIMEOUT_MS` | `30000` |

All numeric configuration is range checked. Cache admission evicts zero-ref LRU
entries for entry/byte/TTL/cgroup pressure. A periodic high-water check retires
cached runtimes. Execution concurrency and queue length are bounded; overflow
fails with a typed 503 rather than admitting unbounded isolates.

Node must run with `--no-node-snapshot`, as required by `isolated-vm` on modern
Node versions. The production Docker command includes it.

## Trust boundary

Build/install remains in the existing sandboxed VM. Serving intentionally moves
the direct bundle into an `isolated-vm` isolate inside the edge process.
`isolated-vm` must not receive host References or objects.

This preview is not a complete mutually-hostile multi-tenant sandbox. V8 bugs
can compromise or crash the process, and `Isolate.createSnapshot` evaluates
top-level app code without the normal isolate memory limit; excessive native
allocation can terminate the container. Actor code runs in a worker-thread V8
isolate but shares the containing process and is not a hostile-code boundary.
Run one trust domain per container, keep Node/V8 patched, rely on Compute
container restart isolation, and do not expose this preview as a boundary
between hostile tenants. Moving direct snapshot creation/execution and actor
runners into sacrificial processes is a follow-up hardening option.

## Observability

With `DYNAMIC_APPS_TIMING_HEADERS=1`, the benchmark records:

- registry ready, actor connect/resolve;
- artifact manifest/download/parse;
- snapshot creation and initial pool fill;
- execution queue, isolate lease/create, context destroy/reset, isolate destroy;
- guest request build, handler, response serialization, and dispatcher;
- evaluation and complete server duration; and
- app/runtime cache outcome and isolate mode.

Diagnostics expose app/runtime/artifact counts, clean/in-use/refilling isolates,
active/queued evaluations, RSS, external snapshot bytes, isolate and context
create/dispose counts, reset failures, overflow creates, and dispatch count.
Structured request logs are opt-in and exclude bodies and credentials.

## Correctness qualification

Required automated checks:

- exact packed root exports and declaration surface;
- router mount/redirect/path/query/method/body/header/error behavior;
- injected-client `get([appId]).deploy(input)` preference, actor-not-found-only
  `getOrCreate([appId]).deploy(input)` fallback, legacy getOrCreate-only client,
  and five-field result;
- deterministic source/release identity and path/size limits;
- direct IIFE output, Node-builtin rejection, handler validation;
- fresh/snapshot/prewarm global counter isolation;
- bounded native-isolate reuse with context create/dispose parity;
- first resolve/download singleflight and zero actor calls after cache hit;
- deployment activation, event invalidation, failed-build rollback, and rapid
  concurrent request isolation against a real local Rivet Engine;
- actor-app dual-bundle validation, runner configuration rollback, callback
  authentication, metadata/start streaming, worker cancellation/cleanup, and
  an end-to-end state/action/event/client test against a real local Engine;
- unit tests, E2E, type checks, build, lint, boundary checks, packed tarball
  install, examples, and Docker serverless health.

The E2E must deploy release A, serve it, deploy B, observe B without polling,
reject an invalid candidate while B remains active, and run at least 64
concurrent requests that all observe request-local state. A separate actor app
must create a keyed actor through `deployment.namespace`/`deployment.pool`,
persist state across actions, deliver an event subscription, and continue
serving direct HTTP on the same release.

## Performance qualification

Benchmark the same zero-dependency <=10 KiB handler through side-by-side routes:

1. edge no-op;
2. low-level actor key resolve;
3. low-level actor action;
4. first request in `fresh`, `snapshot`, and `prewarm` executors, including
   actor, download, snapshot, and pool phases;
5. steady sequential fresh/snapshot/prewarm;
6. below-saturation concurrency 2, 8, and 32;
7. configured-pool concurrency with exact native-isolate count;
8. at least 10,000 requests for stability; and
9. deployment invalidation while requests are active.

Report outer latency and server duration independently. Never add independent
phase percentiles. Artifact download is measured but not optimized in this
work.

Gates for the trivial fixture:

| Case | Gate |
| --- | --- |
| Prewarm cache-hit server, sequential | p50 <= 10 ms, p95 <= 25 ms |
| Snapshot cache-hit server, sequential | p50 <= 10 ms, p95 <= 25 ms |
| Fresh cache-hit server, sequential | p50 <= 25 ms, p95 <= 50 ms |
| Cache-hit actor calls | exactly zero |
| Below-saturation | 100% correct responses, bounded isolates/queue |
| Pool reuse | native isolate creates remain at configured pool size when concurrency <= pool |
| 10,000 stability | 100% success; active/queue/in-use return to zero; no reset failures |
| Invalidation | no newly admitted request uses the invalidated mapping |

The local and Rivet Compute measurements after implementation are recorded in
`benchmarks/dynamic-apps/RESULTS.md`. The final Cloud qualification uses one
image and immutable fixture release for each compared execution mode.

## Compute qualification

Every Cloud mutation is restricted to:

```text
dynamic-apps-ben-562e-production-sqac
```

Every deploy command must explicitly pass:

```sh
--namespace dynamic-apps-ben-562e-production-sqac
```

Never deploy to or modify default `production`. Build and run the Docker image
locally with `RIVETKIT_RUNTIME_MODE=serverless`, verify `/health` and
`/api/rivet/health`, then deploy. In Cloud:

1. verify public health and callback health;
2. deploy the benchmark fixture without namespace creation;
3. run initialization and steady fresh/snapshot/prewarm suites;
4. run the 10,000-request stability profile and collect diagnostics;
5. redeploy the fixture and verify invalidation;
6. restart/replace Compute and verify durable empty-cache recovery; and
7. preserve image/package versions and raw phase output in the report.

## Documentation and prerelease

Update the root/package guides, retained examples, API contract, spec, benchmark
report, boundary check, and packed-package test. Remove every example and asset
whose purpose depends on the deleted actor/scaler/replica APIs. Keep
https://rivet.dev/llms.txt in `AGENTS.md` as the RivetKit reference.

Choose matching prerelease versions for `@rivet-dev/dynamic-apps-builder` and
`@rivet-dev/dynamic-apps`. Pack both, install them into a clean registry-like
consumer, verify exact manifests/exports/native installation, then publish the
builder first and Dynamic Apps second under the requested prerelease dist-tag.
Never move `latest` or publish a stable release. Reinstall the exact published
versions and repeat the smoke test before declaring the goal complete.
