// docs:start setup
import { serve } from "@hono/node-server";
import {
	type ActiveRelease,
	createDynamicApps,
} from "@rivet-dev/dynamic-apps-core";
import { Hono } from "hono";

// Development only: releases disappear on restart and updates cannot reach
// another process. Use durable storage and cross-process invalidation in production.
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
		// The complete artifact is stored before this single active-map update.
		active.set(input.appId, release);
		for (const invalidate of listeners.get(input.appId) ?? []) invalidate();
		return { appId: input.appId, release: release.release };
	},
	async loadActiveRelease(appId) {
		const release = active.get(appId);
		return release
			? {
					...release,
					artifact: {
						...release.artifact,
						bytes: new Uint8Array(release.artifact.bytes),
					},
				}
			: undefined;
	},
	async watchActiveRelease(appId, invalidate) {
		const appListeners = listeners.get(appId) ?? new Set();
		appListeners.add(invalidate);
		listeners.set(appId, appListeners);
		return () => {
			appListeners.delete(invalidate);
			if (appListeners.size === 0) listeners.delete(appId);
		};
	},
});
// docs:end setup

// docs:start deploy
await dynamicApps.deployApp({
	appId: "hello",
	files: {
		"package.json": JSON.stringify({
			private: true,
			type: "module",
			main: "index.js",
		}),
		"index.js": `
			export default {
				fetch() {
					return new Response("Hello from Dynamic Apps Core!");
				},
			};
		`,
	},
});
// docs:end deploy

// docs:start routing
const app = new Hono();
app.route("/apps", dynamicApps.appsRouter);

const port = Number(process.env.PORT ?? 3000);
serve({ fetch: app.fetch, port });
console.log(`Dynamic Apps Core listening on http://localhost:${port}`);
// docs:end routing
