import type { deployApp } from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";
import type { registry } from "../fixtures/app/src/index.js";

// Deploy the app through the host server and read back its connection details.
const host = process.env.HOST_URL ?? "http://localhost:3000";
const response = await fetch(`${host}/deploy/sqlite-notes`, {
	method: "POST",
});
if (!response.ok) throw new Error(`deploy failed: ${await response.text()}`);
const deployment = (await response.json()) as Awaited<
	ReturnType<typeof deployApp>
>;

// docs:start client
// Connect to the actors inside the app's own Rivet namespace.
const client = createClient<typeof registry>({
	endpoint: deployment.endpoint,
	namespace: deployment.namespace,
	poolName: deployment.pool,
	token: deployment.token,
});

const notes = client.notes.getOrCreate(["shared"]);
await notes.add("Hello from the RivetKit client");
console.log(await notes.list());
// docs:end client

await client.dispose();
