import { createClient } from "rivetkit/client";
import { BENCHMARK_APP_ID, deployBenchmarkFixture } from "./fixture.js";
import type { registry } from "./registry.js";

const deployment = await deployBenchmarkFixture();
const client = createClient<typeof registry>();
const actor = client.agentOSAppsApp.getOrCreate([BENCHMARK_APP_ID]);
const actorId = await actor.resolve();
const endpoint = new URL(process.env.RIVET_ENDPOINT ?? "http://localhost:6420");
endpoint.username = "";
endpoint.password = "";

console.log(
	JSON.stringify(
		{
			deployment,
			actorId,
			target: new URL(`/gateway/${actorId}/request/`, endpoint).toString(),
		},
		null,
		2,
	),
);
