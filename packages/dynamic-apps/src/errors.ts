export class DynamicAppsError extends Error {
	readonly code: string;
	readonly metadata?: Record<string, unknown>;

	constructor(
		code: string,
		message: string,
		metadata?: Record<string, unknown>,
	) {
		super(message);
		this.name = "DynamicAppsError";
		this.code = code;
		this.metadata = metadata;
	}
}
