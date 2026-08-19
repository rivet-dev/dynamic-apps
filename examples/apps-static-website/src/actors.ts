import { setup, setupApps } from "@rivet-dev/dynamic-apps";

const { appsActors } = setupApps();

export const registry = setup({
	use: {
		// These actors manage app deployments and scaling.
		...appsActors,
	},
});
