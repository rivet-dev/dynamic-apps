import { afterEach, describe, expect, it, vi } from "vitest";
import { createBrowserNetworkAdapter } from "../../src/driver.js";

// ---------------------------------------------------------------------------
// Security test T1 (se-ts shard, PLAN.md): J.3 / D.1
//
// The browser network adapter delegates guest fetch() to the host's ambient
// `fetch`. In a browser, calling `fetch(url, { ... })` WITHOUT an explicit
// `credentials: "omit"` lets the request carry the host origin's ambient
// credentials (same-origin cookies, HTTP auth, and — for cross-origin requests
// using credentials:"include"/"same-origin" defaults). That means untrusted
// guest code can issue requests that ride on the embedding page's session,
// reading/acting as the logged-in user (credential exfiltration / CSRF-style
// escalation from inside the sandbox).
//
// Secure behavior: the adapter must pin `credentials: "omit"` on every request
// so no ambient host-origin cookies or Authorization headers leak into guest
// traffic. It must also not silently inject host cookies/Authorization.
//
// EXPECTED PER PLAN: likely FAIL → documents the break (driver.ts calls
// `fetch(url, { method, headers, body })` with no credentials policy).
// ---------------------------------------------------------------------------

describe("security: browser fetch adapter omits ambient credentials (T1, J.3/D.1)", () => {
	const originalFetch = globalThis.fetch;

	afterEach(() => {
		globalThis.fetch = originalFetch;
		vi.restoreAllMocks();
	});

	function installCapturingFetch(): { calls: Array<[string, RequestInit | undefined]> } {
		const calls: Array<[string, RequestInit | undefined]> = [];
		globalThis.fetch = (async (
			input: RequestInfo | URL,
			init?: RequestInit,
		) => {
			calls.push([String(input), init]);
			const headers = new Headers({ "content-type": "text/plain" });
			return {
				ok: true,
				status: 200,
				statusText: "OK",
				headers,
				url: String(input),
				redirected: false,
				async text() {
					return "body";
				},
				async arrayBuffer() {
					return new ArrayBuffer(0);
				},
			} as unknown as Response;
		}) as typeof fetch;
		return { calls };
	}

	it("pins credentials to 'omit' on the underlying fetch call", async () => {
		const { calls } = installCapturingFetch();
		const adapter = createBrowserNetworkAdapter();

		await adapter.fetch("https://victim.example/api", { method: "GET" });

		expect(calls).toHaveLength(1);
		const init = calls[0][1];
		expect(
			init?.credentials,
			"adapter must set credentials:'omit' so ambient host cookies/Authorization are not sent",
		).toBe("omit");
	});

	it("does not silently inject host-origin cookies or Authorization headers", async () => {
		const { calls } = installCapturingFetch();
		const adapter = createBrowserNetworkAdapter();

		await adapter.fetch("https://victim.example/api", {
			method: "GET",
			headers: { "x-guest": "1" },
		});

		const init = calls[0][1];
		const headers = new Headers(init?.headers ?? {});
		expect(headers.has("cookie")).toBe(false);
		expect(headers.has("authorization")).toBe(false);
		// And the credentials policy must still be omit even when guest passes headers.
		expect(
			init?.credentials,
			"adapter must keep credentials:'omit' regardless of guest-supplied headers",
		).toBe("omit");
	});

	it("httpRequest path also pins credentials to 'omit'", async () => {
		const { calls } = installCapturingFetch();
		const adapter = createBrowserNetworkAdapter();

		await adapter.httpRequest("https://victim.example/api", { method: "POST" });

		const init = calls[0][1];
		expect(
			init?.credentials,
			"httpRequest must set credentials:'omit' so ambient host credentials are not sent",
		).toBe("omit");
	});
});
