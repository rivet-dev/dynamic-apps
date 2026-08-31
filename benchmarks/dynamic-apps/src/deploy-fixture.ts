import { createClient } from "rivetkit/client";
import {
	BENCHMARK_APP_ID,
	type BenchmarkDeploymentClient,
	deployBenchmarkFixture,
} from "./fixture.js";

const actorClient = createClient() as unknown as {
	dynamicAppsApp: {
		getOrCreate(key: string[]): ActorHandle;
		getForId(actorId: string): ActorHandle;
	};
};
interface ActorHandle {
	resolve(): Promise<string>;
}
const actorId = process.env.BENCH_APP_ACTOR_ID;
const actor = actorId
	? actorClient.dynamicAppsApp.getForId(actorId)
	: actorClient.dynamicAppsApp.getOrCreate([BENCHMARK_APP_ID]);
const deploymentClient = {
	dynamicAppsApp: { getOrCreate: () => actor },
} as unknown as BenchmarkDeploymentClient;
const deployment = await deployBenchmarkFixture(deploymentClient);
const resolvedActorId = await actor.resolve();
const endpoint = new URL(process.env.RIVET_ENDPOINT ?? "http://localhost:6420");
endpoint.username = "";
endpoint.password = "";

console.log(
	JSON.stringify(
		{
			deployment,
			actorId: resolvedActorId,
			target: new URL(
				`/gateway/${resolvedActorId}/request/`,
				endpoint,
			).toString(),
		},
		null,
		2,
	),
);
