# `@rivet-dev/dynamic-apps-core`

Build and serve isolated Dynamic Apps with storage you control. Core needs three
release lifecycle hooks: **publish a release**, **load the active release**, and
**watch for updates**.

```ts
import {
	createDynamicApps,
	type ActiveRelease,
} from "@rivet-dev/dynamic-apps-core";

const active = new Map<string, ActiveRelease>();
const listeners = new Map<string, Set<() => void>>();

const dynamicApps = createDynamicApps({
	async publishRelease(input) {
		const release: ActiveRelease = {
			appId: input.appId,
			release: input.buildId,
			artifact: {
				...input.artifact,
				bytes: new Uint8Array(input.artifact.bytes),
			},
			maxRequestBytes: 1024 * 1024,
			maxResponseBytes: 4 * 1024 * 1024,
		};
		active.set(input.appId, release);
		for (const invalidate of listeners.get(input.appId) ?? []) invalidate();
		return { appId: input.appId, release: release.release };
	},
	async loadActiveRelease(appId) {
		const release = active.get(appId);
		return release && {
			...release,
			artifact: {
				...release.artifact,
				bytes: new Uint8Array(release.artifact.bytes),
			},
		};
	},
	async watchActiveRelease(appId, invalidate) {
		const appListeners = listeners.get(appId) ?? new Set();
		appListeners.add(invalidate);
		listeners.set(appId, appListeners);
		return () => appListeners.delete(invalidate);
	},
});
```

`publishRelease` must durably store the complete artifact before atomically
activating it. `watchActiveRelease` must resolve only after its subscription is
live and invalidate after updates or a connection that may have missed them.
A no-op watcher is unsafe when an app ID can change while another serving
process is running.

Each factory instance owns its builder configuration, router, release
subscriptions, runtime cache, agentOS context pool, and cleanup timer. Await
`dynamicApps.dispose()` during shutdown.

Application entrypoints always default-export a Fetch-compatible handler. They
must not call `serve()`, `listen()`, or `registry.start()`; the serving runtime
owns the listener.

For a release whose artifact has `usesRivetKit: true`, `loadActiveRelease` must
also return its `server.environment`, and the factory must receive an
`ApplicationServerRuntime` through `serverRuntime`. That shared runtime handles
ordinary HTTP and Rivet callbacks in one cached process. The standard
`@rivet-dev/dynamic-apps` package wires this automatically.

The default `pooled` mode leases a bounded agentOS context and resets it after
each request. `ephemeral` mode creates a fresh context per request while reusing
the immutable release VM. Use container isolation between trust domains.

Pass additional VM configuration through `vm`. This supports any
number of agentOS software packages as well as runtime observability callbacks:

```ts
const dynamicApps = createDynamicApps({
	// Release hooks omitted.
	vm: {
		software: [firstPackage, secondPackage],
		onAgentStderr: (event) => logger.error(event),
		onLimitWarning: (warning) => logger.warn(warning),
	},
});
```

Dynamic Apps retains ownership of the `/app` artifact mount and the V8 heap
limit used for executor memory accounting. Other agentOS options pass through
to each serving VM. The Dynamic Apps `logger` continues to receive application
stdout/stderr and build events.

The in-memory example is development-only: it loses releases on restart and
cannot invalidate another process. Use durable object storage plus a reliable
cross-process invalidation channel in production.
