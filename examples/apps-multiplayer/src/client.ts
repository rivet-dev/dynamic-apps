import type { deployApp } from "@rivet-dev/dynamic-apps";
import { createClient } from "rivetkit/client";
import type { registry } from "../fixtures/app/src/index.js";

// Deploy the app through the host server and read back its connection details.
const host = process.env.HOST_URL ?? "http://localhost:3000";
const response = await fetch(`${host}/deploy/multiplayer-room`, {
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

// Open a realtime connection and receive every broadcast from the room.
const room = client.room.getOrCreate(["lobby"]).connect();
room.on("changed", (players) => console.log("players", players));

await room.join("alice");
await room.move("alice", 4, 8);
await room.dispose();
// docs:end client

await client.dispose();
