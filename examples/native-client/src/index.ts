import { NativeSidecarProcessClient } from "@secure-exec/core/sidecar-client";

const decoder = new TextDecoder();

const client = NativeSidecarProcessClient.spawn({
	cwd: process.cwd(),
});

try {
	const session = await client.authenticateAndOpenSession({
		example: "native-client",
	});
	const vm = await client.createVm(session, {
		runtime: "java_script",
		config: {
			env: {},
			rootFilesystem: {
				mode: "ephemeral",
				disableDefaultBaseLayer: false,
				lowers: [{ kind: "bundledBaseFilesystem" }],
				bootstrapEntries: [],
			},
			loopbackExemptPorts: [],
		},
	});

	try {
		await client.writeFile(
			session,
			vm,
			"/tmp/message.txt",
			"hello from secure-exec\n",
		);
		const content = await client.readFile(session, vm, "/tmp/message.txt");
		console.log(decoder.decode(content).trim());
	} finally {
		await client.disposeVm(session, vm);
	}
} finally {
	await client.dispose();
}
