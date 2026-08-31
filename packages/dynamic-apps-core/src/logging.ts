export type DynamicAppsLogLevel = "debug" | "info" | "warn" | "error";

export type DynamicAppsLogSource =
	| "application"
	| "actor"
	| "build"
	| "runtime";

export interface DynamicAppsLogEvent {
	version: 1;
	timestamp: number;
	level: DynamicAppsLogLevel;
	source: DynamicAppsLogSource;
	message: string;
	appId?: string;
	release?: string;
	requestId?: string;
	actorId?: string;
	stream?: "stdout" | "stderr";
	metadata?: Readonly<Record<string, string | number | boolean | null>>;
}

export type DynamicAppsLogHandler = (
	event: Readonly<DynamicAppsLogEvent>,
) => void;

type DynamicAppsLogInput = Omit<DynamicAppsLogEvent, "version" | "timestamp">;

export const MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES = 64 * 1024;
const HANDLER_ERROR_DIAGNOSTIC_INTERVAL_MS = 60_000;

let logHandler: DynamicAppsLogHandler | undefined;
let lastHandlerErrorDiagnosticAt = 0;

export function setDynamicAppsLogHandler(
	handler: DynamicAppsLogHandler | undefined,
): void {
	logHandler = handler;
}

/** @internal */
export function emitDynamicAppsLog(input: DynamicAppsLogInput): void {
	const handler = logHandler;
	if (!handler) return;
	const truncated = truncateUtf8(input.message);
	const metadata = input.metadata
		? Object.freeze({
				...input.metadata,
				...(truncated.truncated ? { truncated: true } : {}),
			})
		: truncated.truncated
			? Object.freeze({ truncated: true })
			: undefined;
	const event = Object.freeze({
		...input,
		version: 1 as const,
		timestamp: Date.now(),
		message: truncated.value,
		...(metadata ? { metadata } : {}),
	});
	try {
		handler(event);
	} catch (error) {
		const now = Date.now();
		if (
			now - lastHandlerErrorDiagnosticAt >=
			HANDLER_ERROR_DIAGNOSTIC_INTERVAL_MS
		) {
			lastHandlerErrorDiagnosticAt = now;
			const message = error instanceof Error ? error.message : String(error);
			process.stderr.write(
				`[dynamic-apps] log handler failed: ${truncateUtf8(message).value}\n`,
			);
		}
	}
}

/** Incrementally reconstructs bounded UTF-8 lines from a byte stream. */
export class DynamicAppsLogLineDecoder {
	readonly #decoder = new TextDecoder();
	readonly #emit: (message: string, truncated: boolean) => void;
	#buffer = "";
	#bufferBytes = 0;
	#truncated = false;
	#ended = false;

	constructor(emit: (message: string, truncated: boolean) => void) {
		this.#emit = emit;
	}

	write(chunk: Uint8Array): void {
		if (this.#ended) return;
		this.#consume(this.#decoder.decode(chunk, { stream: true }));
	}

	end(): void {
		if (this.#ended) return;
		this.#ended = true;
		this.#consume(this.#decoder.decode());
		if (this.#buffer || this.#truncated) this.#flushLine();
	}

	#consume(text: string): void {
		for (const character of text) {
			if (character === "\n") {
				this.#flushLine();
				continue;
			}
			if (this.#truncated) continue;
			const bytes = Buffer.byteLength(character);
			if (this.#bufferBytes + bytes > MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES) {
				this.#truncated = true;
				continue;
			}
			this.#buffer += character;
			this.#bufferBytes += bytes;
		}
	}

	#flushLine(): void {
		const message = this.#buffer.endsWith("\r")
			? this.#buffer.slice(0, -1)
			: this.#buffer;
		this.#emit(message, this.#truncated);
		this.#buffer = "";
		this.#bufferBytes = 0;
		this.#truncated = false;
	}
}

function truncateUtf8(value: string): { value: string; truncated: boolean } {
	if (Buffer.byteLength(value) <= MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES) {
		return { value, truncated: false };
	}
	let output = "";
	let bytes = 0;
	for (const character of value) {
		const size = Buffer.byteLength(character);
		if (bytes + size > MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES) break;
		output += character;
		bytes += size;
	}
	return { value: output, truncated: true };
}
