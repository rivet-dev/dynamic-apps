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
			regions: input.regions ?? ["local"],
			scaling: {
				minReplicas: input.scaling?.minReplicas ?? 0,
				maxReplicas: input.scaling?.maxReplicas ?? 1,
				targetConcurrency: input.scaling?.targetConcurrency ?? 8,
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

The default `pooled` mode leases a bounded agentOS context and resets it after
each request. `ephemeral` mode creates a fresh context per request while reusing
the immutable release VM. Use container isolation between trust domains.

The in-memory example is development-only: it loses releases on restart and
cannot invalidate another process. Use durable object storage plus a reliable
cross-process invalidation channel in production.
