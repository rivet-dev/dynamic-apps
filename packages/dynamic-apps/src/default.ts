import { createDynamicApps } from "@rivet-dev/dynamic-apps-core";
import { createRivetReleaseStore } from "./release-store.js";

const releaseStore = createRivetReleaseStore();

export const defaultDynamicApps = createDynamicApps({
	...releaseStore,
	logger: {
		info: (event) => console.log(JSON.stringify(event)),
		error: (event) => console.error(JSON.stringify(event)),
	},
});
