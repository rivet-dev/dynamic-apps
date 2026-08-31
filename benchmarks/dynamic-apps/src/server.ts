import { serve } from "@hono/node-server";
import { createBenchmarkApplication } from "./edge.js";

const port = integer(process.env.PORT ?? "3000", "PORT");
const host = argument("--host") ?? "127.0.0.1";
const application = createBenchmarkApplication();

serve({ fetch: application.fetch, port, hostname: host });
console.log(
	JSON.stringify({
		event: "dynamic_apps_benchmark_started",
		url: `http://${host}:${port}`,
		serverless: process.env.RIVETKIT_RUNTIME_MODE === "serverless",
	}),
);

function argument(name: string): string | undefined {
	const index = process.argv.indexOf(name);
	return index >= 0 ? process.argv[index + 1] : undefined;
}

function integer(value: string, name: string): number {
	const parsed = Number(value);
	if (!Number.isInteger(parsed) || parsed < 1 || parsed > 65_535) {
		throw new Error(`${name} must be an integer between 1 and 65535`);
	}
	return parsed;
}
