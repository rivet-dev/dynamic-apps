import { describe, expect, it } from "vitest";
import * as protocol from "../src/generated-protocol.js";
import { toGeneratedRequestPayload } from "../src/request-payloads.js";

describe("request payload conversion", () => {
	it("maps authentication and session requests", () => {
		expect(
			toGeneratedRequestPayload({
				type: "authenticate",
				client_name: "agent-os",
				auth_token: "token",
				protocol_version: 7,
				bridge_version: 1,
			}),
		).toEqual({
			tag: "AuthenticateRequest",
			val: {
				clientName: "agent-os",
				authToken: "token",
				protocolVersion: 7,
				bridgeVersion: 1,
			},
		});

		expect(
			toGeneratedRequestPayload({
				type: "open_session",
				placement: { kind: "shared", pool: "default" },
				metadata: { owner: "test" },
			}),
		).toEqual({
			tag: "OpenSessionRequest",
			val: {
				placement: {
					tag: "SidecarPlacementShared",
					val: { pool: "default" },
				},
				metadata: new Map([["owner", "test"]]),
			},
		});
	});

	it("maps VM creation and configuration requests", () => {
		expect(
			toGeneratedRequestPayload({
				type: "create_vm",
				runtime: "java_script",
				metadata: { app: "demo" },
				root_filesystem: {
					mode: "read_only",
					disable_default_base_layer: true,
					bootstrap_entries: [{ path: "/tmp", kind: "directory" }],
				},
				permissions: { fs: "allow" },
			}),
		).toMatchObject({
			tag: "CreateVmRequest",
			val: {
				runtime: protocol.GuestRuntimeKind.JavaScript,
				metadata: new Map([["app", "demo"]]),
				rootFilesystem: {
					mode: protocol.RootFilesystemMode.ReadOnly,
					disableDefaultBaseLayer: true,
				},
			},
		});

		expect(
			toGeneratedRequestPayload({
				type: "configure_vm",
				mounts: [
					{
						guest_path: "/workspace",
						read_only: true,
						plugin: { id: "host", config: { source: "/src" } },
					},
				],
				software: [{ package_name: "node", root: "/software/node" }],
				instructions: ["use node"],
				projected_modules: [
					{ package_name: "@acme/tool", entrypoint: "dist/index.js" },
				],
				command_permissions: { node: "read-only" },
				loopback_exempt_ports: [8080],
			}),
		).toMatchObject({
			tag: "ConfigureVmRequest",
			val: {
				moduleAccessCwd: null,
				instructions: ["use node"],
				commandPermissions: new Map([
					["node", protocol.WasmPermissionTier.ReadOnly],
				]),
			},
		});
	});

	it("maps host callback registration JSON fields", () => {
		expect(
			toGeneratedRequestPayload({
				type: "register_host_callbacks",
				name: "tools",
				description: "demo tools",
				command_aliases: ["agentos-tools"],
				registry_command_aliases: ["agentos"],
				callbacks: {
					read: {
						description: "read a file",
						input_schema: { type: "object" },
						timeout_ms: 2500,
						examples: [{ description: "basic", input: { path: "/tmp" } }],
					},
				},
			}),
		).toEqual({
			tag: "RegisterHostCallbacksRequest",
			val: {
				name: "tools",
				description: "demo tools",
				commandAliases: ["agentos-tools"],
				registryCommandAliases: ["agentos"],
				callbacks: new Map([
					[
						"read",
						{
							description: "read a file",
							inputSchema: '{"type":"object"}',
							timeoutMs: 2500n,
							examples: [
								{
									description: "basic",
									input: '{"path":"/tmp"}',
								},
							],
						},
					],
				]),
			},
		});
	});

	it("maps filesystem and process requests", () => {
		expect(
			toGeneratedRequestPayload({
				type: "guest_filesystem_call",
				operation: "pread",
				path: "/tmp/file",
				len: 10,
				offset: 2,
				encoding: "base64",
			}),
		).toEqual({
			tag: "GuestFilesystemCallRequest",
			val: {
				operation: protocol.GuestFilesystemOperation.Pread,
				path: "/tmp/file",
				destinationPath: null,
				target: null,
				content: null,
				encoding: protocol.RootFilesystemEntryEncoding.BasE64,
				recursive: false,
				mode: null,
				uid: null,
				gid: null,
				atimeMs: null,
				mtimeMs: null,
				len: 10n,
				offset: 2n,
			},
		});

		expect(
			toGeneratedRequestPayload({
				type: "execute",
				process_id: "proc",
				command: "node",
				args: ["-e", "0"],
				env: { A: "1" },
				runtime: "java_script",
				wasm_permission_tier: "isolated",
			}),
		).toEqual({
			tag: "ExecuteRequest",
			val: {
				processId: "proc",
				command: "node",
				runtime: protocol.GuestRuntimeKind.JavaScript,
				entrypoint: null,
				args: ["-e", "0"],
				env: new Map([["A", "1"]]),
				cwd: null,
				wasmPermissionTier: protocol.WasmPermissionTier.Isolated,
			},
		});
	});

	it("maps stdin and ext requests", () => {
		expect(
			toGeneratedRequestPayload({
				type: "write_stdin",
				process_id: "proc",
				chunk: new Uint8Array([1, 2, 3]),
			}),
		).toMatchObject({
			tag: "WriteStdinRequest",
			val: {
				processId: "proc",
			},
		});

		expect(
			toGeneratedRequestPayload({
				type: "ext",
				envelope: {
					namespace: "dev.test",
					payload: new Uint8Array([4, 5]),
				},
			}),
		).toMatchObject({
			tag: "ExtEnvelope",
			val: {
				namespace: "dev.test",
			},
		});
	});
});
