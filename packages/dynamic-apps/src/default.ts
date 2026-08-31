import { createDynamicApps } from "@rivet-dev/dynamic-apps-core";
import { getDefaultActorRuntime } from "./actor-runtime.js";
import {
	createRivetReleaseStore,
	type RivetDeployOptions,
} from "./release-store.js";
import type { Deployment } from "./types.js";

const releaseStore = createRivetReleaseStore();

export const defaultDynamicApps = createDynamicApps<
	Deployment,
	RivetDeployOptions
>({
	...releaseStore,
	serverRuntime: getDefaultActorRuntime(),
	logger: {
		info: (event) => console.log(JSON.stringify(event)),
		error: (event) => console.error(JSON.stringify(event)),
	},
});
