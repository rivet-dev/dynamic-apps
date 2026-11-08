import { deployApp } from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";
import { FAILOVER_APP_ID, failoverFixtureFiles } from "./failover-fixture.js";

// Host A: deploys the app, writes actor state, then waits to be killed.
const deployment = await deployApp({
	appId: FAILOVER_APP_ID,
	files: failoverFixtureFiles(),
});
const client = createClient({
	namespace: deployment.namespace,
	poolName: deployment.pool,
}) as unknown as {
	counter: {
		getOrCreate(key: string[]): { add(amount: number): Promise<number> };
	};
};
const count = await client.counter.getOrCreate(["failover"]).add(7);
console.log(`FAILOVER_READY ${JSON.stringify({ deployment, count })}`);
setInterval(() => {}, 60_000);
