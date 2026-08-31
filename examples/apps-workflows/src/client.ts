import type { deployApp } from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";
import type { registry } from "../fixtures/app/src/index.js";

// Deploy the app through the host server and read back its connection details.
const host = process.env.HOST_URL ?? "http://localhost:3000";
const response = await fetch(`${host}/deploy/order-workflow`, {
	method: "POST",
});
if (!response.ok) throw new Error(`deploy failed: ${await response.text()}`);
const deployment = (await response.json()) as Awaited<
	ReturnType<typeof deployApp>
>;

// docs:start client
const client = createClient<typeof registry>({
	endpoint: deployment.endpoint,
	namespace: deployment.namespace,
	poolName: deployment.pool,
	token: deployment.token,
});

// Creating the actor starts its workflow. Poll until it finishes.
const order = client.order.getOrCreate(["order-1042"]);
let status = await order.status();
while (status !== "delivered") {
	console.log("status", status);
	await new Promise((resolve) => setTimeout(resolve, 500));
	status = await order.status();
}
console.log("status", status);
// docs:end client

await client.dispose();
