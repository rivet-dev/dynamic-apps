import { afterEach, describe, expect, test, vi } from "vitest";
import {
	DynamicAppsLogLineDecoder,
	emitDynamicAppsLog,
	MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES,
	setDynamicAppsLogHandler,
} from "../src/logging.js";

afterEach(() => {
	setDynamicAppsLogHandler(undefined);
	vi.restoreAllMocks();
});

describe("structured Dynamic Apps logging", () => {
	test("replaces and removes the process-global handler", () => {
		const first: string[] = [];
		const second: string[] = [];
		setDynamicAppsLogHandler((event) => first.push(event.message));
		emit("first");
		setDynamicAppsLogHandler((event) => second.push(event.message));
		emit("second");
		setDynamicAppsLogHandler(undefined);
		emit("disabled");
		expect(first).toEqual(["first"]);
		expect(second).toEqual(["second"]);
	});

	test("freezes events and metadata before synchronous delivery", () => {
		let received:
			| Parameters<
					NonNullable<Parameters<typeof setDynamicAppsLogHandler>[0]>
			  >[0]
			| undefined;
		setDynamicAppsLogHandler((event) => {
			received = event;
		});
		emitDynamicAppsLog({
			level: "info",
			source: "runtime",
			message: "complete",
			metadata: { durationMs: 1 },
		});
		expect(received).toMatchObject({ version: 1, message: "complete" });
		expect(Object.isFrozen(received)).toBe(true);
		expect(Object.isFrozen(received?.metadata)).toBe(true);
	});

	test("isolates throwing handlers and truncates messages at 64 KiB", () => {
		const stderr = vi.spyOn(process.stderr, "write").mockReturnValue(true);
		setDynamicAppsLogHandler(() => {
			throw new Error("logger offline");
		});
		expect(() => emit("safe response")).not.toThrow();
		expect(stderr).toHaveBeenCalled();

		let event:
			| { message: string; metadata?: Readonly<Record<string, unknown>> }
			| undefined;
		setDynamicAppsLogHandler((value) => {
			event = value;
		});
		emit("🙂".repeat(MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES));
		expect(Buffer.byteLength(event?.message ?? "")).toBeLessThanOrEqual(
			MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES,
		);
		expect(event?.metadata?.truncated).toBe(true);
	});

	test("reconstructs split UTF-8 lines and flushes a final fragment", () => {
		const lines: Array<[string, boolean]> = [];
		const decoder = new DynamicAppsLogLineDecoder((message, truncated) =>
			lines.push([message, truncated]),
		);
		const bytes = Buffer.from("first 🙂\nlast");
		for (const byte of bytes) decoder.write(Uint8Array.of(byte));
		decoder.end();
		expect(lines).toEqual([
			["first 🙂", false],
			["last", false],
		]);
	});
});

function emit(message: string): void {
	emitDynamicAppsLog({
		level: "info",
		source: "runtime",
		message,
	});
}
