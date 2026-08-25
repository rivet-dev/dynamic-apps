import { deployApp } from "@rivet-dev/dynamic-apps";

export const BENCHMARK_APP_ID = "dynamic-apps-cold-start-benchmark-v2";

export async function deployBenchmarkFixture() {
	const region = process.env.BENCH_REGION;
	return deployApp({
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
export default function fetch() {
  return Response.json({ ok: true, workload: "basic-request-response" });
}
`,
		},
		scaling: {
			minReplicas: 1,
			maxReplicas: 1,
			targetConcurrency: 128,
		},
		...(region ? { regions: [region] } : {}),
	});
}
