import type { DynamicAppsVmOptions } from "@rivet-dev/dynamic-apps-core";

let vmOptions: DynamicAppsVmOptions | undefined;

/**
 * Sets the agentOS options for every app VM, including VMs that run
 * app-defined actors. Pass a function to vary permissions or host functions
 * per app. Call it during host startup, before the first request.
 */
export function setDynamicAppsVmOptions(
	options: DynamicAppsVmOptions | undefined,
): void {
	vmOptions = options;
}

/** @internal */
export function getDynamicAppsVmOptions(): DynamicAppsVmOptions | undefined {
	return vmOptions;
}
