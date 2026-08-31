# `@rivet-dev/dynamic-apps`

This package is the batteries-included, **Rivet-backed adapter** for Dynamic
Apps. For storage you control, use
[`@rivet-dev/dynamic-apps-core`](../dynamic-apps-core/README.md).

Dynamic Apps exposes three runtime values:

- `deployApp(input, options?)` builds, persists, and activates an immutable app
  release through the per-app Rivet state actor.
- `appsRouter` is a Hono router that serves the active release at
	`/:appId/*` through cached agentOS VMs.
- `setDynamicAppsLogHandler(handler)` receives structured application, actor,
	build, and runtime logs.

Deployment builds through the storage-independent core, then the adapter stores
the AOSP artifact through its per-app Rivet actor with atomic activation,
rollback behavior, and a `releaseActivated` event. After the first load, a
cache-hit request makes no actor or storage call.

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

An app that declares `rivetkit` mounts the registry handler in its normal fetch
router and starts an HTTP server on `PORT` when it runs in serverless mode.
Dynamic Apps waits for the server entrypoint's listening callback before
forwarding callbacks:

```ts
import { actor, setup } from "rivetkit";
import { Hono } from "hono";
import { serve } from "@hono/node-server";

const counter = actor({
	state: { value: 0 },
	actions: {
		increment(c) {
			return ++c.state.value;
		},
	},
});

const registry = setup({ use: { counter } });
const app = new Hono();
app.all("/api/rivet/*", (c) => registry.handler(c.req.raw));
app.all("*", () => new Response("ok"));
if (process.env.RIVETKIT_RUNTIME_MODE === "serverless") {
	await new Promise((resolve, reject) => {
		const server = serve(
			{
				fetch: app.fetch,
				port: Number(process.env.PORT),
				hostname: "0.0.0.0",
			},
			resolve,
		);
		server.once("error", reject);
	});
}
export default app;
```

Awaiting Hono's listening callback makes module completion the readiness signal;
Dynamic Apps does not poll an application health route.

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

The deploy CLI's cached management credential is not injected into the app.
Pass the token as a server-side Compute environment variable:

```sh
npx @rivetkit/cli deploy \
  --namespace <namespace> \
  --env PORT=3000 \
  --env RIVET_CLOUD_TOKEN="$RIVET_CLOUD_TOKEN"
```

By default, the app actor callback enters through the host app actor's Rivet
gateway and authenticates with the publishable credential from
`RIVET_PUBLIC_ENDPOINT`. This keeps the child app namespace separate from the
host registry namespace. `DYNAMIC_APPS_CALLBACK_URL` is only for a custom
receiver that accepts child-namespace lifecycle callbacks; Dynamic Apps appends
`/api/rivet` to that origin.

Actor requests follow the normal Rivet Engine path. The app's serverless
callback mounts its verified artifact in agentOS and runs the bundled RivetKit
WebAssembly runtime in serverless mode. State, actions, events, connections,
and streaming actor responses are handled by RivetKit inside the sandbox;
ordinary HTTP for the same app uses the agentOS evaluation path.

Rivet Engine requires `/api/rivet/start` to return a long-lived SSE control
stream containing the real runner-init packet and keepalive pings. Actor traffic
does not travel in this response; it uses RivetKit's outbound WebSocket. Dynamic
Apps forwards the server's response with agentOS's native VM HTTP stream,
including backpressure and cancellation. No app code executes in the host.

Dynamic Apps does not impose a separate actor-callback concurrency gate. The
configured actor runtime count controls retained idle entries only; active
AgentOS runtimes are not rejected or evicted because that target is full.

## Host integration

Mount application traffic and the private Rivet callback separately:

```ts
import { serve } from "@hono/node-server";
import { appsRouter } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();

server.all("/api/rivet/*", (c) => appsRouter.fetch(c.req.raw));
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
| `DYNAMIC_APPS_ACTOR_WORKER_START_TIMEOUT_MS` | `30000` |
| `DYNAMIC_APPS_ACTOR_WORKER_IDLE_TTL_MS` | `30000` |
| `DYNAMIC_APPS_ACTOR_START_PAYLOAD_MAX_BYTES` | `16777216` |
| `DYNAMIC_APPS_ACTOR_REQUEST_TIMEOUT_MS` | `30000` |

Inside a finite cgroup, direct execution concurrency and both direct
context-pool limits are upper bounds. At startup they are reduced when
necessary to keep the configured heap cost below
`DYNAMIC_APPS_MEMORY_HIGH_WATER_PERCENT`, with host and payload headroom. They
are unchanged when the cgroup has no finite memory limit. Effective direct
concurrency appears as `executionConcurrency` in executor diagnostics. The
actor `workerLimit` diagnostic is the retained-idle target, not admission.

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

The handler receives application stdout/stderr, sandboxed actor output, build
phases, and runtime events. Enqueue into a logging SDK rather than performing a
blocking network request in the callback.

## Memory and trust boundary

Each immutable release owns one bounded agentOS VM with a read-only mounted
artifact. Context count, runtime entries, artifact bytes, idle TTLs, execution
concurrency, queues, and cgroup high-water eviction are independently bounded.
Successful pooled contexts are reset and reinitialized; failed, timed-out, or
aborted contexts are deleted.

agentOS supplies the filesystem, process, environment, and network permission
boundary for both direct HTTP and app-defined actors. RivetKit actors use the
bundled WASM runtime; uploaded app code is never imported into the Dynamic Apps
host process.
