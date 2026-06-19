import { describe, expect, it } from "vitest";
import type { NetworkAdapter, Permissions } from "../../src/runtime.js";
import { wrapNetworkAdapter } from "../../src/runtime.js";

// A network adapter that records every egress and would "succeed" — i.e. the
// attacker's request reaches the wire if the permission layer does not stop it.
function recordingAdapter(): {
	adapter: NetworkAdapter;
	egress: string[];
} {
	const egress: string[] = [];
	const adapter: NetworkAdapter = {
		async fetch(url) {
			egress.push(`fetch ${url}`);
			return {
				ok: true,
				status: 200,
				statusText: "OK",
				headers: {},
				body: "SECRET",
				url,
				redirected: false,
			};
		},
		async dnsLookup(hostname) {
			egress.push(`dns ${hostname}`);
			return { address: "1.2.3.4", family: 4 };
		},
		async httpRequest(url) {
			egress.push(`http ${url}`);
			return { status: 200, statusText: "OK", headers: {}, body: "SECRET", url };
		},
	};
	return { adapter, egress };
}

// ---------------------------------------------------------------------------
// Security test T3 (se-ts shard): H.2 / D.1 — FAILURES.md#F-009
//
// Host-keyed denials must be enforceable. A realistic egress policy blocks the
// cloud-metadata IP (169.254.169.254) and other internal hosts by HOST, not by
// the full URL string. Before the fix, wrapNetworkAdapter forwarded only
// `{ url }` to the network check for fetch()/httpRequest() — it never parsed
// out host/port. So a policy that denies by `request.host === "169.254.169.254"`
// never saw a host and therefore never matched, letting the guest reach the
// metadata endpoint.
//
// The fix parses host (and port) from the request URL so host-/port-keyed deny
// rules — the natural way operators write SSRF guards — actually match.
// ---------------------------------------------------------------------------
describe("security: network policy receives parsed host and port (T3, H.2/D.1, F-009)", () => {
	const METADATA_HOST = "169.254.169.254";
	const METADATA_URL = `http://${METADATA_HOST}/latest/meta-data/iam/security-credentials/`;

	function hostKeyedDenyMetadata(): Permissions {
		return {
			network: (req: { url?: string; host?: string; port?: number }) => {
				// Operator denies the metadata host by HOST (the natural way to
				// write such a rule). Default-allow everything else.
				if (req.host === METADATA_HOST) {
					return { allow: false, reason: "metadata blocked" };
				}
				return { allow: true };
			},
		};
	}

	it("enforces a host-keyed deny of the cloud metadata endpoint on fetch", async () => {
		const { adapter, egress } = recordingAdapter();
		const wrapped = wrapNetworkAdapter(adapter, hostKeyedDenyMetadata());

		await expect(
			wrapped.fetch(METADATA_URL),
			"fetch to the metadata host must be denied by a host-keyed policy",
		).rejects.toThrow();
		expect(
			egress,
			"the metadata request must never reach the wire",
		).toEqual([]);
	});

	it("enforces a host-keyed deny of the cloud metadata endpoint on httpRequest", async () => {
		const { adapter, egress } = recordingAdapter();
		const wrapped = wrapNetworkAdapter(adapter, hostKeyedDenyMetadata());

		await expect(wrapped.httpRequest(METADATA_URL)).rejects.toThrow();
		expect(egress).toEqual([]);
	});

	it("still allows a non-matching host and forwards the request", async () => {
		const { adapter, egress } = recordingAdapter();
		const wrapped = wrapNetworkAdapter(adapter, hostKeyedDenyMetadata());

		const res = await wrapped.fetch("https://example.com/ok");
		expect(res.status).toBe(200);
		expect(egress).toEqual(["fetch https://example.com/ok"]);
	});

	it("passes parsed host and inferred port to the policy callback", async () => {
		const { adapter } = recordingAdapter();
		const seen: Array<{ url?: string; host?: string; port?: number }> = [];
		const permissions: Permissions = {
			network: (req) => {
				seen.push(req);
				return { allow: true };
			},
		};
		const wrapped = wrapNetworkAdapter(adapter, permissions);

		await wrapped.fetch("https://example.com:8443/path");
		await wrapped.fetch("http://example.org/path");
		await wrapped.fetch("https://example.net/path");

		expect(seen[0]).toMatchObject({ host: "example.com", port: 8443 });
		expect(seen[1]).toMatchObject({ host: "example.org", port: 80 });
		expect(seen[2]).toMatchObject({ host: "example.net", port: 443 });
	});
});
