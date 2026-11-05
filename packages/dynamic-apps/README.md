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

An app that declares `rivetkit` exports its registry as `registry` next to its
default fetch handler:

```ts
import { actor, setup } from "rivetkit";
import { Hono } from "hono";

const counter = actor({
	state: { value: 0 },
	actions: {
		increment(c) {
			return ++c.state.value;
		},
	},
});

export const registry = setup({ use: { counter } });
const app = new Hono();
app.all("*", () => new Response("ok"));
export default app;
```

Do not call `serve()`, `registry.start()`, `registry.handler()`, or bind a port
in application code. Dynamic Apps runs the registry and the HTTP listener.

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

Actors run on Rivet v3. When a RivetKit release is activated, the app's
private actor boots the release's artifact in an agentOS VM and starts the
exported registry as a long-lived envoy in the app's namespace and pool. The
release only becomes active once that envoy has connected. The VM stays running
while the release is active, so it does not scale to zero or out: each RivetKit
app holds one VM on one host process. Deploying a new release starts its envoy
first, then drains the previous one so its actors move to the new release.

If the guest process exits, the host restarts it with backoff. If a host
process stops gracefully, the app actor moves to another host and starts the
envoy there. Every host also wakes each RivetKit app once a minute, which
restarts the envoy of an app whose host process died without a graceful
shutdown.

Envoys authenticate with the credentials in `RIVET_PUBLIC_ENDPOINT`, or with
the namespace secret token that Dynamic Apps provisions on Rivet Cloud. App
code receives that credential, so on a self-hosted Engine give
`RIVET_PUBLIC_ENDPOINT` a token scoped to app namespaces, not an admin token.

Ordinary HTTP routes of a RivetKit app run in separate cached agentOS VMs that
do not start an envoy. No app code executes in the host.

## Host integration

Mount application traffic and the host registry's Rivet routes separately:

```ts
import { serve } from "@hono/node-server";
import { appsRouter, startDynamicAppsHost } from "@rivet-dev/dynamic-apps";
import { Hono } from "hono";

const server = new Hono();

server.all("/api/rivet/*", (c) => appsRouter.fetch(c.req.raw));
server.route("/apps", appsRouter);

await startDynamicAppsHost();
serve({ fetch: server.fetch, port: 3000 });
```

Call `startDynamicAppsHost()` once when the host boots. It starts the host's
private actors and the loop that restarts app envoys after a host dies. Without
it a host only joins after its first request, so after every host restarts,
apps that serve only actor traffic would stay offline.

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

These modes apply to apps without RivetKit actors. Actor-enabled apps use one
cached server process for both ordinary HTTP and Rivet callbacks.

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

## Configuring app VMs

`setDynamicAppsVmOptions(options)` sets the agentOS options for every app VM,
including VMs that run app-defined actors. A function receives
`{ appId, release }` and is called once per release VM, so permissions and host
functions can differ per app:

```ts
import { setDynamicAppsVmOptions } from "@rivet-dev/dynamic-apps";

setDynamicAppsVmOptions(({ appId }) => ({
	permissions: { network: "deny" },
	hostFunctions: { github: githubHostFunctionsFor(appId) },
}));
```

App code, including actor code, calls host functions as globals, for example
`await github.createIssue({ title })`.

VMs are cached per release, so set the options before the first request.
Changed options apply only to VMs created afterwards.

Actor VMs keep VM-local listeners, VM-local loopback HTTP, and the Rivet Engine
endpoint reachable under a `deny` or rule-set network policy. Rules you add are
evaluated after these, so an explicit deny rule for that access still wins.

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
artifact. Apps without actors use resettable evaluation contexts. Actor-enabled
apps use one cached server process so ordinary HTTP and Rivet's streaming
callback share a single module instance.

agentOS supplies the filesystem, process, environment, and network permission
boundary for both direct HTTP and app-defined actors. RivetKit actors use the
bundled WASM runtime; uploaded app code is never imported into the Dynamic Apps
host process.
