import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const packageDir = resolve(__dirname, "..");

const pi = {
	name: "pi",
	type: "agent" as const,
	packageDir,
	requires: ["@agentos-software/pi", "@mariozechner/pi-coding-agent"],
	agent: {
		id: "pi",
		acpAdapter: "@agentos-software/pi",
		agentPackage: "@mariozechner/pi-coding-agent",
		// Evaluate the bundled Pi SDK into the per-sidecar V8 snapshot
		// (dist/sdk-snapshot.js) so it loads once per sidecar and is reused
		// across sessions. Falls back to per-session dynamic import if the
		// snapshot can't be built.
		snapshot: true,
	},
};

export default pi;
