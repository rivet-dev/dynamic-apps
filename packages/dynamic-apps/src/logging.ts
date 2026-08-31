import {
	DynamicAppsLogLineDecoder,
	emitDynamicAppsLog as emitCoreDynamicAppsLog,
	MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES,
	setDynamicAppsLogHandler as setCoreDynamicAppsLogHandler,
} from "@rivet-dev/dynamic-apps-core/internal";

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

export { DynamicAppsLogLineDecoder, MAX_DYNAMIC_APPS_LOG_MESSAGE_BYTES };

export function setDynamicAppsLogHandler(
	handler: DynamicAppsLogHandler | undefined,
): void {
	setCoreDynamicAppsLogHandler(handler);
}

/** @internal */
export function emitDynamicAppsLog(input: DynamicAppsLogInput): void {
	emitCoreDynamicAppsLog(input);
}
