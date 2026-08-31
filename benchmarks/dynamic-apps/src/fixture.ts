import { deployApp } from "@rivet-dev/dynamic-apps";

export const BENCHMARK_APP_ID = "dynamic-apps-cold-start-benchmark-v2";
export const ACTOR_BENCHMARK_APP_ID = "dynamic-apps-actor-runtime-benchmark-v1";

export type BenchmarkDeploymentClient = NonNullable<
	NonNullable<Parameters<typeof deployApp>[1]>["client"]
>;

export async function deployBenchmarkFixture(
	client?: BenchmarkDeploymentClient,
) {
	const region = process.env.BENCH_REGION;
	return deployApp(
		{
			appId: BENCHMARK_APP_ID,
			files: {
				"package.json": JSON.stringify({
					name: "dynamic-apps-cold-start-fixture",
					version: "0.0.0",
					private: true,
					type: "module",
					main: "index.js",
				}),
				"index.js": `
export default {
  fetch(request) {
    const url = new URL(request.url);
    return Response.json({
      ok: true,
      workload: "basic-request-response",
      requestId: url.searchParams.get("requestId"),
    });
  },
};
`,
			},
			scaling: {
				minReplicas: 1,
				maxReplicas: 1,
				targetConcurrency: 128,
			},
			...(region ? { regions: [region] } : {}),
		},
		client ? { client } : undefined,
	);
}

export async function deployActorBenchmarkFixture(
	client?: BenchmarkDeploymentClient,
) {
	const region = process.env.BENCH_REGION;
	return deployApp(
		{
			appId: ACTOR_BENCHMARK_APP_ID,
			files: {
				"package.json": JSON.stringify({
					name: "dynamic-apps-actor-fixture",
					version: "0.0.0",
					private: true,
					type: "module",
					main: "index.js",
					dependencies: {
						rivetkit: "0.0.0-fix-rivetkit-wasm-serve-config.e2b11f9",
					},
				}),
				"index.js": `
import { actor, event, setup } from "rivetkit";

const counter = actor({
  state: { count: 0 },
  events: { changed: event() },
  actions: {
    add(c, amount = 1) {
      c.state.count += amount;
      c.broadcast("changed", c.state.count);
      return c.state.count;
    },
    inspect(c) {
      return c.state.count;
    },
  },
});

export const registry = setup({ use: { counter } });
registry.start();

export default function fetch() {
  return Response.json({ ok: true, workload: "actor-and-direct-http" });
}
`,
			},
			scaling: {
				minReplicas: 0,
				maxReplicas: 16,
				targetConcurrency: 8,
			},
			...(region ? { regions: [region] } : {}),
		},
		client ? { client } : undefined,
	);
}
