const CONTROL_TIMEOUT_MS = 15_000;
const MAX_CONTROL_ATTEMPTS = 6;
const BASE_RETRY_DELAY_MS = 50;
const MAX_RETRY_DELAY_MS = 2_000;
const MAX_REFUSAL_BODY_BYTES = 4 * 1024;

function retryableStatus(status: number): boolean {
	return status === 409 || status === 429 || status >= 500;
}

/**
 * The retry hint of a structured Engine overload refusal, or undefined when the
 * response is not one. Rivet v3 sheds load at admission with a typed
 * `overloaded` error that some routes return as HTTP 400, so the status code
 * alone does not say whether a request may be retried.
 */
async function overloadRetryAfterMs(
	response: Response,
): Promise<number | undefined> {
	if (response.ok || !response.body) return undefined;
	const length = Number(response.headers.get("content-length") ?? 0);
	if (length > MAX_REFUSAL_BODY_BYTES) return undefined;
	try {
		const text = await response.clone().text();
		if (text.length > MAX_REFUSAL_BODY_BYTES) return undefined;
		const body = JSON.parse(text) as {
			code?: unknown;
			metadata?: { retry_after_ms?: unknown };
		};
		if (body.code !== "overloaded") return undefined;
		const retryAfter = body.metadata?.retry_after_ms;
		return typeof retryAfter === "number" && retryAfter >= 0 ? retryAfter : 0;
	} catch {
		return undefined;
	}
}

function retryDelay(
	response: Response | undefined,
	attempt: number,
	overloadRetryAfter: number | undefined,
): number {
	if (overloadRetryAfter !== undefined) {
		return Math.min(
			Math.max(overloadRetryAfter, BASE_RETRY_DELAY_MS),
			MAX_RETRY_DELAY_MS,
		);
	}
	const retryAfter = response?.headers.get("retry-after");
	if (retryAfter) {
		const seconds = Number(retryAfter);
		if (Number.isFinite(seconds) && seconds >= 0) {
			return Math.min(seconds * 1_000, MAX_RETRY_DELAY_MS);
		}
		const at = Date.parse(retryAfter);
		if (Number.isFinite(at)) {
			return Math.min(Math.max(0, at - Date.now()), MAX_RETRY_DELAY_MS);
		}
	}
	return Math.min(
		BASE_RETRY_DELAY_MS * 2 ** Math.max(0, attempt - 1),
		MAX_RETRY_DELAY_MS,
	);
}

/** Bounded retry policy for idempotent Rivet control-plane requests. */
export async function controlFetch(
	input: string | URL,
	init: RequestInit = {},
): Promise<Response> {
	const deadline = Date.now() + CONTROL_TIMEOUT_MS;
	let lastError: unknown;

	for (let attempt = 1; attempt <= MAX_CONTROL_ATTEMPTS; attempt += 1) {
		let response: Response | undefined;
		let overloadRetryAfter: number | undefined;
		try {
			response = await fetch(input, {
				...init,
				signal: AbortSignal.timeout(Math.max(1, deadline - Date.now())),
			});
			overloadRetryAfter = await overloadRetryAfterMs(response);
			if (
				(!retryableStatus(response.status) &&
					overloadRetryAfter === undefined) ||
				attempt === MAX_CONTROL_ATTEMPTS
			) {
				return response;
			}
		} catch (error) {
			lastError = error;
			if (attempt === MAX_CONTROL_ATTEMPTS || Date.now() >= deadline)
				throw error;
		}

		const delay = retryDelay(response, attempt, overloadRetryAfter);
		if (Date.now() + delay >= deadline) {
			if (response) return response;
			throw lastError;
		}
		if (response?.body) await response.body.cancel();
		await new Promise((resolve) => setTimeout(resolve, delay));
	}

	throw lastError;
}
