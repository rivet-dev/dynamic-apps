import { NodeRuntime } from "secure-exec";

// Boot a VM and seed a file into its virtual filesystem at boot with `files`.
// The bytes are copied into the kernel's in-memory filesystem; the host
// filesystem is never exposed to the guest.
const rt = await NodeRuntime.create({
	files: { "/home/agentos/seed.json": JSON.stringify({ ok: true }) },
});

try {
	// Write another file from the host with rt.writeFile, then have the guest
	// read both back. The guest only ever sees the virtual filesystem.
	await rt.writeFile("/home/agentos/note.txt", "written from the host\n");

	const result = await rt.run(`
		const { readFileSync } = await import("node:fs");
		const seed = JSON.parse(readFileSync("/home/agentos/seed.json", "utf8"));
		const note = readFileSync("/home/agentos/note.txt", "utf8").trim();
		console.log("guest read seed:", JSON.stringify(seed));
		console.log("guest read note:", note);
		globalThis.__return({ seed, note });
	`);

	console.log("guest stdout:", result.stdout.trim());
	console.log("guest exit code:", result.exitCode);
	console.log("value returned to host:", result.value);

	// Read a guest-written file back on the host with rt.readFile.
	const bytes = await rt.readFile("/home/agentos/seed.json");
	console.log("host rt.readFile:", new TextDecoder().decode(bytes));

	// The virtual filesystem is isolated from the host disk. The guest path does
	// not exist on the real host - prove it by checking the same path here.
	const { existsSync } = await import("node:fs");
	const guestPath = "/home/agentos/seed.json";
	console.log(
		`host sees ${guestPath}?`,
		existsSync(guestPath) ? "YES (unexpected!)" : "NO - isolated from host",
	);
} finally {
	await rt.dispose();
}
