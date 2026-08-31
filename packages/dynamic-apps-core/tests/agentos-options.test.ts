import type { AgentOsOptions } from "@rivet-dev/agentos-core";
import { describe, expect, test, vi } from "vitest";
import { resolveRuntimeAgentOsOptions } from "../src/executor.js";

describe("agentOS runtime options", () => {
	test("preserves multiple software packages and observability callbacks", () => {
		const software = [
			{ packagePath: "/packages/one" },
			{ packagePath: "/packages/two" },
		];
		const onAgentStderr = vi.fn();
		const onLimitWarning = vi.fn();
		const options = resolveRuntimeAgentOsOptions(
			{ software, onAgentStderr, onLimitWarning },
			"/tmp/release.aospkg",
			96,
		);

		expect(options.software).toEqual(software);
		expect(options.onAgentStderr).toBe(onAgentStderr);
		expect(options.onLimitWarning).toBe(onLimitWarning);
		expect(options.defaultSoftware).toBe(false);
		expect(options.limits?.jsRuntime?.v8HeapLimitMb).toBe(96);
		expect(options.mounts?.at(-1)?.path).toBe("/app");
	});

	test("preserves caller options while protecting executor-owned settings", () => {
		const options = resolveRuntimeAgentOsOptions(
			{
				defaultSoftware: true,
				allowedNodeBuiltins: ["node:path"],
				limits: {
					resources: { maxSockets: 512 },
					jsRuntime: { capturedOutputLimitBytes: 1024, v8HeapLimitMb: 512 },
				},
			} satisfies AgentOsOptions,
			"/tmp/release.aospkg",
			64,
		);

		expect(options.defaultSoftware).toBe(true);
		expect(options.allowedNodeBuiltins).toEqual(["node:path"]);
		expect(options.limits?.resources?.maxSockets).toBe(512);
		expect(options.limits?.jsRuntime).toEqual({
			capturedOutputLimitBytes: 1024,
			v8HeapLimitMb: 64,
		});
	});

	test("rejects a caller-provided /app mount", () => {
		expect(() =>
			resolveRuntimeAgentOsOptions(
				{
					mounts: [
						{
							path: "/app",
							readOnly: true,
							plugin: { id: "test", config: {} },
						},
					],
				},
				"/tmp/release.aospkg",
				64,
			),
		).toThrow('vm.mounts cannot replace the reserved "/app" mount');
	});
});
