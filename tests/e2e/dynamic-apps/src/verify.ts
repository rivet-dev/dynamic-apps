import {
	appsRouter,
	deployApp,
	setDynamicAppsLogHandler,
} from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";

// Guest output is otherwise dropped, which hides why an app fails.
setDynamicAppsLogHandler((event) => {
	if (event.level === "error" || event.source === "actor") {
		console.error(JSON.stringify(event));
	}
});

const appId = "dynamic-apps-direct-e2e";
const ACTOR_APP_ID = "dynamic-apps-actor-e2e";
const startedAt = performance.now();

const firstDeployment = await deployFixture("one");
const first = await request("/first?q=1");
const second = await request("/second?q=2");
assert(first.marker === "one", "first release returned the wrong marker");
assert(second.marker === "one", "cached release returned the wrong marker");
assert(first.counter === 1 && second.counter === 1, "request state leaked");
assert(first.path === "/first?q=1", "mounted path was not rewritten");

const secondDeployment = await deployFixture("two");
let updated = await request("/updated");
const invalidationDeadline = Date.now() + 10_000;
while (updated.marker !== "two" && Date.now() < invalidationDeadline) {
	await new Promise((resolve) => setTimeout(resolve, 25));
	updated = await request("/updated");
}
assert(updated.marker === "two", "release event did not invalidate the cache");
assert(updated.counter === 1, "updated release reused dirty request state");

let invalidHandlerRejected = false;
try {
	await deployApp({
		appId,
		files: {
			"package.json": packageJson(),
			"index.js": "export default 42",
		},
	});
} catch {
	invalidHandlerRejected = true;
}
assert(invalidHandlerRejected, "invalid default export activated");
const afterFailure = await request("/after-failure");
assert(
	afterFailure.marker === "two",
	"failed deployment replaced active release",
);

const loadStartedAt = performance.now();
const load = await Promise.all(
	Array.from({ length: 64 }, (_, index) => request(`/load/${index}`)),
);
const loadMs = performance.now() - loadStartedAt;
assert(
	load.every((item) => item.marker === "two"),
	"load returned stale data",
);
assert(
	load.every((item) => item.counter === 1),
	"load leaked isolate state",
);

const actorDeployment = await deployActorFixture("v1");
const actorClient = createClient({
	namespace: actorDeployment.namespace,
	poolName: actorDeployment.pool,
}) as unknown as ActorApplicationClient;
const actorKey = [`e2e-${Date.now()}`];
const actor = await actorClient.counter.getOrCreate(actorKey).connect();
let unsubscribe = () => {};
const observedEvent = new Promise<number>((resolve, reject) => {
	const timeout = setTimeout(
		() => reject(new Error("actor event verification timed out")),
		10_000,
	);
	unsubscribe = actor.on("changed", (value) => {
		clearTimeout(timeout);
		resolve(value);
	});
});
const actorFirst = await actor.add(2);
const actorEvent = await observedEvent;
unsubscribe();
const actorSecond = await actor.add(3);
const actorCurrent = await actor.inspect();
await actor.dispose();
assert(actorFirst === 2, "actor action returned the wrong initial state");
assert(actorEvent === 2, "actor event returned the wrong state");
assert(actorSecond === 5, "actor action did not preserve state");
assert(actorCurrent === 5, "actor inspect returned the wrong state");
const actorHttpResponse = await appsRouter.request(
	`/${ACTOR_APP_ID}/direct-http`,
);
assert(
	actorHttpResponse.ok,
	`actor app direct HTTP request failed with ${actorHttpResponse.status}: ${await actorHttpResponse.clone().text()}`,
);
const actorHttp = (await actorHttpResponse.json()) as {
	ok: boolean;
	workload: string;
};
assert(
	actorHttp.ok && actorHttp.workload === "actor-and-direct-http",
	"actor app direct HTTP returned the wrong response",
);

// A guest that exits is restarted by its supervisor, and the Engine moves the
// actor to the new envoy with its state intact.
const counterHandle = actorClient.counter.getOrCreate(actorKey);
await counterHandle.crash().catch(() => {});
const afterCrash = await eventually(
	() => counterHandle.inspect(),
	(value) => value === 5,
	"actor did not recover after its envoy crashed",
);

// A new release starts its envoy before the previous one drains, and the
// actor keeps its state across the rollout.
const actorRedeployment = await deployActorFixture("v2");
const redeployedRelease = await eventually(
	() => counterHandle.release(),
	(value) => value === "v2",
	"actor did not move to the redeployed release",
);
const afterRedeploy = await counterHandle.inspect();
assert(afterRedeploy === 5, "actor state was lost across the redeploy");
await eventually(
	() => connectedNodeVersions(actorRedeployment),
	(versions) => versions.length === 1,
	"the previous release's envoy did not drain",
);
await actorClient.dispose();

console.log(
	JSON.stringify(
		{
			event: "dynamic_apps_e2e_passed",
			firstDeployment,
			secondDeployment,
			failedBuildPreservedActiveRelease: true,
			invalidationObserved: true,
			requests: load.length + 5,
			loadMs,
			actorDeployment,
			actor: {
				first: actorFirst,
				second: actorSecond,
				current: actorCurrent,
				observedEvent: actorEvent,
				directHttp: actorHttp,
				afterCrash,
				redeployedRelease,
				afterRedeploy,
			},
			totalMs: performance.now() - startedAt,
		},
		null,
		2,
	),
);

interface ActorApplicationClient {
	counter: {
		getOrCreate(key: string[]): {
			connect(): Promise<{
				on(name: "changed", callback: (value: number) => void): () => void;
				add(amount: number): Promise<number>;
				inspect(): Promise<number>;
				dispose(): Promise<void>;
			}>;
			inspect(): Promise<number>;
			release(): Promise<string>;
			crash(): Promise<void>;
		};
	};
	dispose(): Promise<void>;
}

async function deployActorFixture(marker: string) {
	return deployApp({
		appId: ACTOR_APP_ID,
		files: {
			"package.json": JSON.stringify({
				name: "dynamic-apps-actor-e2e-fixture",
				version: "1.0.0",
				private: true,
				type: "module",
				main: "index.js",
				dependencies: {
					hono: "4.13.5",
					rivetkit: "3.0.2",
				},
			}),
			"index.js": `
import { Hono } from "hono";
import { actor, event, setup } from "rivetkit";

const counter = actor({
  state: { count: 0 },
  events: { changed: event() },
  actions: {
    add(c, amount = 1) {
      c.state.count += amount;
      c.broadcast("changed", c.state.count);
      return c.state.count;
    },
    inspect(c) {
      return c.state.count;
    },
    release() {
      return ${JSON.stringify(marker)};
    },
    crash() {
      setTimeout(() => process.exit(1), 10);
    },
  },
});

export const registry = setup({ use: { counter } });
const app = new Hono();
app.all("*", () => Response.json({ ok: true, workload: "actor-and-direct-http" }));
export default app;
`,
		},
	});
}

async function deployFixture(marker: string) {
	return deployApp({
		appId,
		files: {
			"package.json": packageJson(),
			"index.js": `let counter = 0;
export default {
  async fetch(request) {
    counter += 1;
    const url = new URL(request.url);
    return Response.json({
      marker: ${JSON.stringify(marker)},
      counter,
      path: url.pathname + url.search,
    });
  },
};`,
		},
	});
}

function packageJson(): string {
	return JSON.stringify({
		name: "dynamic-apps-direct-e2e-fixture",
		version: "1.0.0",
		private: true,
		type: "module",
		main: "index.js",
	});
}

async function request(path: string): Promise<{
	marker: string;
	counter: number;
	path: string;
}> {
	const response = await appsRouter.request(`/${appId}${path}`);
	if (!response.ok) {
		throw new Error(
			`request ${path} failed with ${response.status}: ${await response.text()}`,
		);
	}
	return response.json();
}

/** Polls until `check` passes; recovery and rollout finish asynchronously. */
async function eventually<T>(
	read: () => Promise<T>,
	check: (value: T) => boolean,
	message: string,
	timeoutMs = 60_000,
): Promise<T> {
	const deadline = Date.now() + timeoutMs;
	let last: unknown;
	while (Date.now() < deadline) {
		try {
			const value = await read();
			if (check(value)) return value;
			last = value;
		} catch (error) {
			last = error;
		}
		await new Promise((resolve) => setTimeout(resolve, 250));
	}
	throw new Error(`${message}; last result: ${String(last)}`);
}

async function connectedNodeVersions(deployment: {
	endpoint: string;
	namespace: string;
	pool: string;
}): Promise<number[]> {
	const url = new URL(
		`/pools/${encodeURIComponent(deployment.pool)}/nodes`,
		deployment.endpoint,
	);
	url.searchParams.set("namespace", deployment.namespace);
	const response = await fetch(url, {
		headers: { authorization: `Bearer ${process.env.RIVET_TOKEN}` },
	});
	if (!response.ok) throw new Error(`node listing returned ${response.status}`);
	const body = (await response.json()) as {
		nodes: Array<{ version: number; connected: boolean; stopping: boolean }>;
	};
	return body.nodes
		.filter((node) => node.connected && !node.stopping)
		.map((node) => node.version);
}

function assert(condition: unknown, message: string): asserts condition {
	if (!condition) throw new Error(message);
}
