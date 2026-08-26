# `@rivet-dev/dynamic-apps`

Dynamic Apps exposes two runtime values:

- `deployApp(input, options?)` builds, persists, and activates an immutable app
  release through the per-app Rivet state actor.
- `appsRouter` is a Hono router that serves the active release at
  `/:appId/*` from process-local V8 isolates.

Deployment still uses the existing resource-bounded agentOS build VM, builder
package, AOSP artifact chunks, activation ordering, rollback behavior, and
`releaseActivated` event. The serving path no longer uses scaler or execution
actors. After the first resolve/download, a cache-hit request makes no actor
call.

## Direct HTTP contract

An app must default-export an object with `fetch(request)` or a fetch function:

```ts
export default {
	async fetch(request: Request): Promise<Response> {
		return Response.json({ path: new URL(request.url).pathname });
	},
};
```

The first preview buffers request and response bodies, rejects Node builtins,
and supports the host-provided Fetch subset documented by the tests. It does
not support WebSockets, streaming ordinary HTTP bodies, or Node APIs inside the
direct app isolate.

## App-defined actors

An app that declares `rivetkit` may also export a registry. The platform owns
the runner lifecycle, so the existing `registry.start()` call remains valid:

```ts
import { actor, setup } from "rivetkit";

const counter = actor({
	state: { value: 0 },
	actions: {
		increment(c) {
			return ++c.state.value;
		},
	},
});

export const registry = setup({ use: { counter } });
registry.start();

export default () => new Response("ok");
```

Use the unchanged `deployApp` result to create the app client:

```ts
import { createClient } from "rivetkit/client";

const deployment = await deployApp({ appId: "counter", source: appSource });
const client = createClient({
	namespace: deployment.namespace,
	poolName: deployment.pool,
});
```

On an authenticated server deployment, configure `RIVET_PUBLIC_ENDPOINT` with
the public Engine endpoint/publishable credential used by app actors. If the
host `RIVET_ENDPOINT` contains a secret credential and no public endpoint is
available, actor-enabled activation fails instead of copying that secret into
the app worker. URL authentication is separated into endpoint, namespace, and
token fields before RivetKit starts.

Actor requests follow the normal Rivet Engine path. The app's serverless
callback loads its verified actor bundle into a bounded process-local worker
thread and uses the host's pinned RivetKit native runtime. State, actions,
events, connections, and streaming actor responses are handled by RivetKit;
ordinary HTTP for the same app still uses the direct isolate path.

## Host integration

Mount application traffic and the private Rivet callback separately:

```ts
import { serve } from "@hono/node-server";
import { appsRouter } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();

const dispatchRegistry = (request: Request) => {
	const headers = new Headers(request.headers);
	headers.set("x-agentos-app-registry-dispatch", "1");
	return appsRouter.fetch(new Request(request, { headers }));
};

server.all("/api/rivet", (c) => dispatchRegistry(c.req.raw));
server.all("/api/rivet/*", (c) => dispatchRegistry(c.req.raw));
server.route("/apps", appsRouter);

serve({ fetch: server.fetch, port: 3000 });
```

Run Node 22 or newer with `--no-node-snapshot`, as required by `isolated-vm`:

```sh
node --no-node-snapshot dist/server.js
```

Deploying an app keeps the existing call shape:

```ts
import { deployApp } from "@rivet-dev/dynamic-apps";

const deployment = await deployApp({
	appId: "hello-world",
	source: new URL("./generated-app/", import.meta.url),
});
```

## Execution modes

`DYNAMIC_APPS_ISOLATE_MODE` selects the request isolation strategy:

| Mode | Cached object | Cache-hit request |
| --- | --- | --- |
| `prewarm` (default) | artifact, V8 heap snapshot, and up to N reusable clean isolates | lease an isolate, run once, destroy its context, restore a clean context from the snapshot, return the isolate to the pool |
| `snapshot` | artifact and V8 heap snapshot | create one isolate from the snapshot, run once, destroy it |
| `fresh` | verified artifact source | create an empty isolate, compile/evaluate the bundle, run once, destroy it |

The prewarm pool is a cache, not a capacity limit. A burst beyond the pool uses
snapshot-restored overflow isolates under the global execution limit; only the
configured number remain idle afterward. Setting the pool size to zero selects
`snapshot` mode.

| Variable | Default |
| --- | ---: |
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
| `DYNAMIC_APPS_ACTOR_WORKER_MAX_ENTRIES` | `4` |
| `DYNAMIC_APPS_ACTOR_WORKER_HEAP_LIMIT_MB` | `96` |
| `DYNAMIC_APPS_ACTOR_WORKER_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES` | `1048576` |

`DYNAMIC_APPS_CONTROL_TOKEN` optionally overrides only the token used by
Dynamic Apps control-plane requests, including explicit app-namespace
provisioning, datacenter discovery, and actor runner-pool configuration. It is
unnecessary on a local Engine or when the `RIVET_ENDPOINT` secret has the
required permissions. It is never passed to app code or used by the app actor
client.

For idempotent multi-region deploys, `deployApp` resolves an existing app actor
with `get()` when the supplied RivetKit client supports it, then falls back to
`getOrCreate()` only when that actor is absent. Structural clients that expose
only `getOrCreate()` retain the original behavior.

`DYNAMIC_APPS_TIMING_HEADERS=1` adds benchmark-only phase headers.
`DYNAMIC_APPS_LOG_REQUESTS=1` writes structured request timing records without
request/response bodies or credentials.

## Memory and trust boundary

Each cached isolate has its own configured V8 heap limit. The pool, runtime
entry count, artifact bytes, idle TTLs, execution concurrency, queue, and cgroup
high-water eviction are independently bounded. A used JavaScript context is
never reused: it is released before a fresh snapshot-backed context is created
on the cached native isolate.

This preview executes isolates in the serving process. `isolated-vm` is a V8
isolation primitive, not a complete hostile multi-tenant sandbox, and snapshot
creation can terminate the process if top-level application code exhausts
native resources. App actor code runs in a worker-thread V8 isolate but shares
the same containing process. Run one trust domain per container, keep Node/V8
patched, and rely on Compute restart isolation; do not treat this preview as a
boundary for mutually hostile tenants.
