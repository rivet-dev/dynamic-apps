import { appsRouter, deployApp } from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";

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

const actorDeployment = await deployActorFixture();
const actorClient = createClient({
	namespace: actorDeployment.namespace,
	poolName: actorDeployment.pool,
}) as unknown as ActorApplicationClient;
const actor = await actorClient.counter
	.getOrCreate([`e2e-${Date.now()}`])
	.connect();
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
await actorClient.dispose();
assert(actorFirst === 2, "actor action returned the wrong initial state");
assert(actorEvent === 2, "actor event returned the wrong state");
assert(actorSecond === 5, "actor action did not preserve state");
assert(actorCurrent === 5, "actor inspect returned the wrong state");
const actorHttpResponse = await appsRouter.request(
	`/${ACTOR_APP_ID}/direct-http`,
);
assert(actorHttpResponse.ok, "actor app direct HTTP request failed");
const actorHttp = (await actorHttpResponse.json()) as {
	ok: boolean;
	workload: string;
};
assert(
	actorHttp.ok && actorHttp.workload === "actor-and-direct-http",
	"actor app direct HTTP returned the wrong response",
);

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
		};
	};
	dispose(): Promise<void>;
}

async function deployActorFixture() {
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
					rivetkit: "2.3.11",
				},
			}),
			"index.js": `
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
  },
});

export const registry = setup({ use: { counter } });
registry.start();

export default function fetch() {
  return Response.json({ ok: true, workload: "actor-and-direct-http" });
}
`,
		},
		scaling: {
			minReplicas: 0,
			maxReplicas: 4,
			targetConcurrency: 8,
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

function assert(condition: unknown, message: string): asserts condition {
	if (!condition) throw new Error(message);
}
