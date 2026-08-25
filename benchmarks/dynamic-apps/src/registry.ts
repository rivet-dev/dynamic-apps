import { setup, setupApps } from "@rivet-dev/dynamic-apps";
import { actor } from "rivetkit";

const { appsActors } = setupApps();

const benchmarkProbe = actor({
	options: { noSleep: true },
	state: { requests: 0 },
	onRequest: (c, request) => {
		const startedAt = performance.now();
		c.state.requests += 1;
		const response = Response.json({
			ok: true,
			method: request.method,
			requests: c.state.requests,
		});
		response.headers.set(
			"x-agentos-bench-actor-handler-ms",
			(performance.now() - startedAt).toFixed(2),
		);
		return response;
	},
	actions: {
		ping: (c) => {
			const startedAt = performance.now();
			c.state.requests += 1;
			return {
				ok: true,
				requests: c.state.requests,
				handlerMs: performance.now() - startedAt,
			};
		},
		pingPeer: async (c, peerKey: string) => {
			const peer = c.client().benchmarkProbe.getOrCreate([peerKey]);
			const resolveStartedAt = performance.now();
			const peerActorId = await peer.resolve();
			const resolvedAt = performance.now();
			const action = await peer.action({ name: "ping", args: [] });
			const actionAt = performance.now();
			return {
				ok: true,
				peerActorId,
				peerResolveMs: resolvedAt - resolveStartedAt,
				peerActionMs: actionAt - resolvedAt,
				action,
			};
		},
	},
});

export const registry = setup({
	use: {
		...appsActors,
		benchmarkProbe,
	},
});
