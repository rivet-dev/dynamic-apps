import { strict as assert } from "node:assert";
import { createServer } from "node:http";
import { describe, it } from "node:test";
import { ensureCloudSetup } from "./cloud-stress.js";
import { actorApplicationClientConfig, benchmarkErrorDetails } from "./edge.js";
import { readLoadConfig, runLoadTest } from "./load.js";

describe("Dynamic Apps load driver", () => {
	it("rejects an impossible success-rate gate", () => {
		assert.throws(
			() => readLoadConfig({ LOAD_TEST_MIN_SUCCESS_RATE: "1.1" }),
			/LOAD_TEST_MIN_SUCCESS_RATE must be a number between 0 and 1/,
		);
	});

	it("redacts credentials from benchmark error details", () => {
		const error = Object.assign(
			new Error(
				"failed https://namespace:sk_example@api.rivet.dev with cloud_api_example",
			),
			{ code: "runner_failed" },
		);
		assert.deepEqual(benchmarkErrorDetails(error), {
			code: "runner_failed",
			message: "failed https://[redacted]@api.rivet.dev with [redacted]",
		});
	});

	it("separates endpoint auth from the deployed app namespace", () => {
		assert.deepEqual(
			actorApplicationClientConfig(
				{ namespace: "app-namespace", pool: "app-pool" },
				"https://host-namespace:pk_example@api.rivet.dev",
			),
			{
				endpoint: "https://api.rivet.dev",
				namespace: "app-namespace",
				poolName: "app-pool",
				token: "pk_example",
			},
		);
	});

	it("records cold and warm latency with a hard request bound", async () => {
		let requestCount = 0;
		const server = createServer((_request, response) => {
			requestCount += 1;
			response.setHeader(
				"x-agentos-app-cold-start",
				requestCount % 2 === 1 ? "1" : "0",
			);
			response.setHeader(
				"x-agentos-app-replica",
				`replica-${requestCount % 2}`,
			);
			response.setHeader("x-agentos-app-replica-count", "2");
			response.setHeader(
				"x-agentos-app-queue-delay-ms",
				String([9, 1, 3, 7][requestCount - 1]),
			);
			response.setHeader("x-agentos-app-cold-start-ms", "7");
			response.setHeader("x-agentos-app-bundle-load-ms", "2");
			response.setHeader("x-agentos-bench-edge-total-ms", "5");
			response.end("hello");
		});
		await new Promise<void>((resolve) =>
			server.listen(0, "127.0.0.1", resolve),
		);

		try {
			const address = server.address();
			assert(address && typeof address !== "string");
			const result = await runLoadTest({
				...readLoadConfig({}),
				target: `http://127.0.0.1:${address.port}`,
				concurrency: 2,
				durationSeconds: 1,
				maxRequests: 4,
			});

			assert.equal(result.completed, 4);
			assert.equal(result.successRate, 1);
			assert.equal(result.coldStarts, 2);
			assert.equal(result.warmRequests, 2);
			assert.equal(result.unclassifiedRequests, 0);
			assert.equal(result.warmHitRate, 0.5);
			assert.equal(result.replicaHeaderCoverage, 1);
			assert.equal(result.benchmarkInstanceHeaderCoverage, 0);
			assert.equal(result.maximumReplicaCount, 2);
			assert.deepEqual(result.queueDelayMs, { p50: 3, p95: 9, max: 9 });
			assert.equal(result.serverColdStartMs.p50, 7);
			assert.equal(result.serverPhaseMs.bundleLoad?.p50, 2);
			assert.equal(result.serverPhaseMs["edge-total"]?.p50, 5);
			assert.equal(result.serverPhaseSamples["edge-total"], 4);
			assert.equal(result.stoppedBy, "request-limit");
			assert(result.coldLatencyMs.p50 > 0);
			assert(result.warmLatencyMs.p50 > 0);
		} finally {
			await new Promise<void>((resolve, reject) =>
				server.close((error) => (error ? reject(error) : resolve())),
			);
		}
	});

	it("validates response bodies and echoed request identities", async () => {
		const result = await runLoadTest(
			{
				...readLoadConfig({}),
				target: "http://load.test",
				concurrency: 1,
				durationSeconds: 1,
				maxRequests: 2,
				validateJsonOk: true,
				echoRequestId: true,
			},
			async (input) => {
				const requestId = new URL(String(input)).searchParams.get("requestId");
				return Response.json({ ok: true, requestId });
			},
		);

		assert.equal(result.successRate, 1);
		assert.deepEqual(result.statuses, { "200": 2 });
	});

	it("rejects a successful response with the wrong request identity", async () => {
		const result = await runLoadTest(
			{
				...readLoadConfig({}),
				target: "http://load.test",
				concurrency: 1,
				durationSeconds: 1,
				maxRequests: 1,
				validateJsonOk: true,
				echoRequestId: true,
			},
			async () => Response.json({ ok: true, requestId: "wrong" }),
		);

		assert.equal(result.successRate, 0);
		assert.deepEqual(result.statuses, { InvalidBenchmarkResponseError: 1 });
	});

	it("preserves a non-success HTTP status when body validation fails", async () => {
		const result = await runLoadTest(
			{
				...readLoadConfig({}),
				target: "http://load.test",
				concurrency: 1,
				durationSeconds: 1,
				maxRequests: 1,
				validateJsonOk: true,
			},
			async () => new Response("upstream timeout", { status: 504 }),
		);

		assert.equal(result.successRate, 0);
		assert.deepEqual(result.statuses, { "504": 1 });
	});

	it("resumes setup after the ingress times out an idempotent POST", async () => {
		let attempts = 0;
		const result = await ensureCloudSetup(
			"http://setup.test",
			"/bench/setup",
			1_000,
			0,
			async (_input, init) => {
				attempts += 1;
				if (attempts === 1) {
					assert.equal(init?.method, "GET");
					return new Response("not ready", { status: 404 });
				}
				if (attempts === 2) {
					assert.equal(init?.method, "POST");
					return new Response("timed out", { status: 504 });
				}
				assert.equal(init?.method, "GET");
				return Response.json({ status: "ready" });
			},
		);

		assert.deepEqual(result, { status: "ready" });
		assert.equal(attempts, 3);
	});

	it("fails a request instead of buffering an oversized response", async () => {
		const result = await runLoadTest(
			{
				...readLoadConfig({}),
				target: "http://load.test",
				concurrency: 1,
				durationSeconds: 1,
				maxRequests: 1,
				maxResponseBytes: 4,
			},
			async () => new Response("too large"),
		);

		assert.equal(result.completed, 1);
		assert.equal(result.successRate, 0);
		assert.deepEqual(result.statuses, { ResponseBodyLimitError: 1 });
	});
});
