import { createDynamicApps } from "@rivet-dev/dynamic-apps-core";
import { resolveVmOptions } from "@rivet-dev/dynamic-apps-core/internal";
import { getDefaultActorRuntime } from "./actor-runtime.js";
import {
	createRivetReleaseStore,
	type RivetDeployOptions,
} from "./release-store.js";
import type { Deployment } from "./types.js";
import { getDynamicAppsVmOptions } from "./vm-options.js";

const releaseStore = createRivetReleaseStore();

export const defaultDynamicApps = createDynamicApps<
	Deployment,
	RivetDeployOptions
>({
	...releaseStore,
	serverRuntime: getDefaultActorRuntime(),
	vm: async (context) =>
		(await resolveVmOptions(getDynamicAppsVmOptions(), context)) ?? {},
	logger: {
		info: (event) => console.log(JSON.stringify(event)),
		error: (event) => console.error(JSON.stringify(event)),
	},
});
