import { spawn } from "node:child_process";
import { createServer } from "node:net";

const port = await new Promise((resolve, reject) => {
	const server = createServer();
	server.once("error", reject);
	server.listen(0, "127.0.0.1", () => {
		const address = server.address();
		if (!address || typeof address === "string") {
			server.close();
			reject(new Error("could not allocate a Core Quick Start test port"));
			return;
		}
		server.close(() => resolve(address.port));
	});
});

const child = spawn(
	process.execPath,
	[
		"--import",
		"tsx",
		"examples/apps-core-quickstart/src/server.ts",
		"--host",
		"0.0.0.0",
	],
	{
		stdio: ["ignore", "pipe", "inherit"],
		env: { ...process.env, PORT: String(port) },
	},
);
let output = "";
child.stdout.on("data", (chunk) => {
	output += chunk;
});

try {
	const deadline = Date.now() + 5 * 60_000;
	for (;;) {
		if (child.exitCode !== null) {
			throw new Error(`Core Quick Start exited early with ${child.exitCode}`);
		}
		try {
			const response = await fetch(`http://127.0.0.1:${port}/apps/hello/`);
			if (
				response.ok &&
				(await response.text()) === "Hello from Dynamic Apps Core!"
			) {
				break;
			}
		} catch {}
		if (Date.now() >= deadline) {
			throw new Error(`Core Quick Start did not become ready:\n${output}`);
		}
		await new Promise((resolve) => setTimeout(resolve, 250));
	}
} finally {
	if (child.exitCode === null) {
		child.kill("SIGTERM");
		await new Promise((resolve) => {
			const timer = setTimeout(() => child.kill("SIGKILL"), 5_000);
			child.once("exit", () => {
				clearTimeout(timer);
				resolve(undefined);
			});
		});
	}
}
