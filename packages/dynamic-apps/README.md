# `@rivet-dev/dynamic-apps`

Dynamic Apps exposes three runtime values:

- `deployApp(input, options?)` builds, persists, and activates an immutable app
  release through the per-app Rivet state actor.
- `appsRouter` is a Hono router that serves the active release at
	`/:appId/*` through cached agentOS VMs.
- `setDynamicAppsLogHandler(handler)` receives structured application, actor,
	build, and runtime logs.

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

The first preview buffers request and response bodies. Direct apps run in a
Node 22 agentOS sandbox, so supported Node builtins and the standard Node Web
APIs are available. Native addons, WebSockets, and streaming ordinary HTTP
bodies are not supported.

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
	endpoint: deployment.endpoint,
	namespace: deployment.namespace,
	poolName: deployment.pool,
	token: deployment.token,
});
```

Every app is deployed to its own stable Rivet namespace. On Rivet Cloud, set
`RIVET_CLOUD_TOKEN` to a server-side `cloud_api_*` project token. Dynamic Apps
uses it to provision the namespace and namespace-scoped access, secret, and
publishable credentials. The management credential is never returned or passed
to app code; `deployApp()` returns only the app's publishable token.
Rivet Compute derives the app actor callback from its `.rivet.run` hostname.
Set `DYNAMIC_APPS_CALLBACK_URL` only when the host is exposed at a different
public origin; Dynamic Apps appends `/api/rivet`.

Actor requests follow the normal Rivet Engine path. The app's serverless
callback loads its verified actor bundle into a bounded process-local worker
thread and uses the host's pinned RivetKit WebAssembly runtime. State, actions,
events, connections, and streaming actor responses are handled by RivetKit;
ordinary HTTP for the same app still uses the agentOS evaluation path.

Callback admission happens before the request body is read. The worker cache
is a strict process limit across active and idle app bundles; a new bundle is
rejected with `agentos_apps_no_capacity` when every worker slot is busy.
Existing bundles continue sharing their worker up to the callback concurrency
limit.

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

Run Node 22 or newer normally:

```sh
node dist/server.js
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

`DYNAMIC_APPS_EXECUTION_MODE` selects the request execution strategy:

| Mode | Cached object | Cache-hit request |
| --- | --- | --- |
| `pooled` (default) | verified artifact, one agentOS VM, and up to N retained contexts | lease a context, evaluate once, reset and reinitialize it, then return it to the pool |
| `ephemeral` | verified artifact and one agentOS VM | evaluate in a fresh context managed by agentOS |

The context pool is a cache, not a capacity limit. A burst beyond the pool uses
ephemeral evaluations under the global execution limit; only the configured
number of retained contexts remain idle afterward.

| Variable | Default |
| --- | ---: |
| `DYNAMIC_APPS_CONTEXT_POOL_SIZE` | `2` |
| `DYNAMIC_APPS_CONTEXT_POOL_MAX_TOTAL` | `8` |
| `DYNAMIC_APPS_CONTEXT_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_CONTEXT_HEAP_LIMIT_MB` | `64` |
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
| `DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS` | `10000` |
| `DYNAMIC_APPS_ACTOR_WORKER_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES` | `1048576` |
| `DYNAMIC_APPS_ACTOR_REQUEST_CONCURRENCY` | `64` |
| `DYNAMIC_APPS_ACTOR_REQUEST_QUEUE_SIZE` | `128` |
| `DYNAMIC_APPS_ACTOR_REQUEST_QUEUE_WAIT_MS` | `5000` |
| `DYNAMIC_APPS_ACTOR_REQUEST_TIMEOUT_MS` | `30000` |

Inside a finite cgroup, execution concurrency, both context-pool limits, and
the actor-worker limit are upper bounds. At startup they are reduced when
necessary to keep the configured heap cost below
`DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT`, with host and payload headroom. They
are unchanged when the cgroup has no finite memory limit. Effective direct
concurrency appears as `executionConcurrency` in executor diagnostics, and the
effective actor limit appears as `workerLimit` in actor diagnostics.

`DYNAMIC_APPS_CONTROL_TOKEN` optionally overrides only the token used by
Dynamic Apps Engine control-plane requests when running against a local or
self-hosted Engine. Rivet Cloud namespace provisioning uses
`RIVET_CLOUD_TOKEN` instead. Neither credential is passed to app code or used by
the external app actor client.

For idempotent multi-region deploys, `deployApp` resolves an existing app actor
with `get()` when the supplied RivetKit client supports it, then falls back to
`getOrCreate()` only when that actor is absent. Structural clients that expose
only `getOrCreate()` retain the original behavior.

`DYNAMIC_APPS_TIMING_HEADERS=1` adds benchmark-only phase headers.
`DYNAMIC_APPS_LOG_REQUESTS=1` emits structured request timing records through
the configured log handler without request/response bodies or credentials.

## Collecting logs

Register one synchronous handler during host startup:

```ts
import { setDynamicAppsLogHandler } from "@rivet-dev/dynamic-apps";

setDynamicAppsLogHandler((event) => {
	process.stdout.write(`${JSON.stringify(event)}\n`);
});
```

The handler receives application stdout/stderr, actor worker output, build
phases, and runtime events. Enqueue into a logging SDK rather than performing a
blocking network request in the callback.

## Memory and trust boundary

Each immutable release owns one bounded agentOS VM with a read-only mounted
artifact. Context count, runtime entries, artifact bytes, idle TTLs, execution
concurrency, queues, and cgroup high-water eviction are independently bounded.
Successful pooled contexts are reset and reinitialized; failed, timed-out, or
aborted contexts are deleted.

agentOS supplies the filesystem, process, environment, and network permission
boundary for direct HTTP. App actor code still runs in a bounded worker thread
inside the host process. Run one trust domain per container and keep the host
and agentOS runtime patched; do not treat actor workers as a boundary for
mutually hostile tenants.
