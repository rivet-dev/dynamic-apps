import { setup as agentOSSetup } from "@rivet-dev/agentos";
import type { setup as rivetkitSetup } from "rivetkit";
import { createAppsActors, type DynamicAppsActors } from "./actors.js";

export { deployApp } from "./deploy.js";
export { AgentOSAppsError, DynamicAppsError } from "./errors.js";
export {
	type AgentOSAppsRoutingClient,
	appsRouter,
	type CreateAppsRouterOptions,
	createAppsRouter,
	type DynamicAppsRoutingClient,
} from "./router.js";
export type {
	AppReleaseInfo,
	AppScaling,
	DeployAppInput,
	Deployment,
} from "./types.js";

/**
 * RivetKit setup configured for the VM runtime used by Dynamic Apps.
 *
 * The public type intentionally comes from RivetKit. The underlying VM runtime
 * is a transitive implementation detail and does not need user configuration.
 */
export const setup: typeof rivetkitSetup = agentOSSetup as typeof rivetkitSetup;

export function setupApps(): { appsActors: DynamicAppsActors } {
	return {
		appsActors: createAppsActors(),
	};
}
