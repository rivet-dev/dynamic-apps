import {
	fromGeneratedRootFilesystemEntry,
	type LiveRootFilesystemEntry,
	type LiveRootFilesystemEntryEncoding,
} from "./filesystem.js";
import { fromGeneratedExtEnvelope, type LiveExtEnvelope } from "./ext.js";
import type * as protocol from "./generated-protocol.js";
import { bigIntToSafeNumber } from "./numbers.js";
import {
	fromGeneratedFilesystemOperation,
	fromGeneratedGuestFilesystemOperation,
	fromGeneratedRootFilesystemEntryEncoding,
	fromGeneratedSignalDispositionAction,
	type LiveFilesystemOperation,
	type LiveGuestFilesystemOperation,
	type LiveSignalDispositionAction,
} from "./protocol-maps.js";
import {
	fromGeneratedGuestFilesystemStat,
	fromGeneratedProcessSnapshotEntry,
	fromGeneratedSocketStateEntry,
	type LiveGuestFilesystemStat,
	type LiveProcessSnapshotEntry,
	type LiveSocketStateEntry,
} from "./state.js";

export interface LiveSignalHandlerRegistration {
	action: LiveSignalDispositionAction;
	mask: number[];
	flags: number;
}

export type LiveResponsePayload =
	| {
			type: "authenticated";
			sidecar_id: string;
			connection_id: string;
			max_frame_bytes: number;
	  }
	| {
			type: "session_opened";
			session_id: string;
			owner_connection_id: string;
	  }
	| {
			type: "vm_created";
			vm_id: string;
	  }
	| {
			type: "vm_configured";
			applied_mounts: number;
			applied_software: number;
	  }
	| {
			type: "host_callbacks_registered";
			registration: string;
			command_count: number;
	  }
	| {
			type: "layer_created";
			layer_id: string;
	  }
	| {
			type: "layer_sealed";
			layer_id: string;
	  }
	| {
			type: "snapshot_imported";
			layer_id: string;
	  }
	| {
			type: "snapshot_exported";
			layer_id: string;
			entries: LiveRootFilesystemEntry[];
	  }
	| {
			type: "overlay_created";
			layer_id: string;
	  }
	| {
			type: "root_filesystem_bootstrapped";
			entry_count: number;
	  }
	| {
			type: "guest_filesystem_result";
			operation: LiveGuestFilesystemOperation;
			path: string;
			content?: string;
			encoding?: LiveRootFilesystemEntryEncoding;
			entries?: string[];
			stat?: LiveGuestFilesystemStat;
			exists?: boolean;
			target?: string;
	  }
	| {
			type: "root_filesystem_snapshot";
			entries: LiveRootFilesystemEntry[];
	  }
	| {
			type: "vm_disposed";
			vm_id: string;
	  }
	| {
			type: "process_started";
			process_id: string;
			pid?: number;
	  }
	| {
			type: "stdin_written";
			process_id: string;
			accepted_bytes: number;
	  }
	| {
			type: "pty_resized";
			process_id: string;
			cols: number;
			rows: number;
	  }
	| {
			type: "stdin_closed";
			process_id: string;
	  }
	| {
			type: "process_killed";
			process_id: string;
	  }
	| {
			type: "process_snapshot";
			processes: LiveProcessSnapshotEntry[];
	  }
	| {
			type: "listener_snapshot";
			listener?: LiveSocketStateEntry;
	  }
	| {
			type: "bound_udp_snapshot";
			socket?: LiveSocketStateEntry;
	  }
	| {
			type: "vm_fetch_result";
			response_json: string;
	  }
	| {
			type: "signal_state";
			process_id: string;
			handlers: Record<string, LiveSignalHandlerRegistration>;
	  }
	| {
			type: "zombie_timer_count";
			count: number;
	  }
	| {
			type: "filesystem_result";
			operation: LiveFilesystemOperation;
			status: string;
			payload_size_bytes: number;
	  }
	| {
			type: "persistence_state";
			key: string;
			found: boolean;
			payload_size_bytes: number;
	  }
	| {
			type: "persistence_flushed";
			key: string;
			committed_bytes: number;
	  }
	| {
			type: "rejected";
			code: string;
			message: string;
	  }
	| {
			type: "ext_result";
			envelope: LiveExtEnvelope;
	  };

export function fromGeneratedResponsePayload(
	payload: protocol.ResponsePayload,
): LiveResponsePayload {
	switch (payload.tag) {
		case "AuthenticatedResponse":
			return {
				type: "authenticated",
				sidecar_id: payload.val.sidecarId,
				connection_id: payload.val.connectionId,
				max_frame_bytes: payload.val.maxFrameBytes,
			};
		case "SessionOpenedResponse":
			return {
				type: "session_opened",
				session_id: payload.val.sessionId,
				owner_connection_id: payload.val.ownerConnectionId,
			};
		case "VmCreatedResponse":
			return { type: "vm_created", vm_id: payload.val.vmId };
		case "VmDisposedResponse":
			return { type: "vm_disposed", vm_id: payload.val.vmId };
		case "RootFilesystemBootstrappedResponse":
			return {
				type: "root_filesystem_bootstrapped",
				entry_count: payload.val.entryCount,
			};
		case "VmConfiguredResponse":
			return {
				type: "vm_configured",
				applied_mounts: payload.val.appliedMounts,
				applied_software: payload.val.appliedSoftware,
			};
		case "HostCallbacksRegisteredResponse":
			return {
				type: "host_callbacks_registered",
				registration: payload.val.registration,
				command_count: payload.val.commandCount,
			};
		case "LayerCreatedResponse":
			return { type: "layer_created", layer_id: payload.val.layerId };
		case "LayerSealedResponse":
			return { type: "layer_sealed", layer_id: payload.val.layerId };
		case "SnapshotImportedResponse":
			return { type: "snapshot_imported", layer_id: payload.val.layerId };
		case "SnapshotExportedResponse":
			return {
				type: "snapshot_exported",
				layer_id: payload.val.layerId,
				entries: payload.val.entries.map(fromGeneratedRootFilesystemEntry),
			};
		case "OverlayCreatedResponse":
			return { type: "overlay_created", layer_id: payload.val.layerId };
		case "GuestFilesystemResultResponse":
			return {
				type: "guest_filesystem_result",
				operation: fromGeneratedGuestFilesystemOperation(payload.val.operation),
				path: payload.val.path,
				...(payload.val.content !== null
					? { content: payload.val.content }
					: {}),
				...(payload.val.encoding !== null
					? {
							encoding: fromGeneratedRootFilesystemEntryEncoding(
								payload.val.encoding,
							),
						}
					: {}),
				...(payload.val.entries !== null
					? { entries: [...payload.val.entries] }
					: {}),
				...(payload.val.stat !== null
					? { stat: fromGeneratedGuestFilesystemStat(payload.val.stat) }
					: {}),
				...(payload.val.exists !== null ? { exists: payload.val.exists } : {}),
				...(payload.val.target !== null ? { target: payload.val.target } : {}),
			};
		case "RootFilesystemSnapshotResponse":
			return {
				type: "root_filesystem_snapshot",
				entries: payload.val.entries.map(fromGeneratedRootFilesystemEntry),
			};
		case "ProcessStartedResponse":
			return {
				type: "process_started",
				process_id: payload.val.processId,
				...(payload.val.pid !== null ? { pid: payload.val.pid } : {}),
			};
		case "StdinWrittenResponse":
			return {
				type: "stdin_written",
				process_id: payload.val.processId,
				accepted_bytes: bigIntToSafeNumber(
					payload.val.acceptedBytes,
					"stdin_written.accepted_bytes",
				),
			};
		case "PtyResizedResponse":
			return {
				type: "pty_resized",
				process_id: payload.val.processId,
				cols: payload.val.cols,
				rows: payload.val.rows,
			};
		case "StdinClosedResponse":
			return { type: "stdin_closed", process_id: payload.val.processId };
		case "ProcessKilledResponse":
			return { type: "process_killed", process_id: payload.val.processId };
		case "ProcessSnapshotResponse":
			return {
				type: "process_snapshot",
				processes: payload.val.processes.map(fromGeneratedProcessSnapshotEntry),
			};
		case "ListenerSnapshotResponse":
			return {
				type: "listener_snapshot",
				...(payload.val.listener !== null
					? { listener: fromGeneratedSocketStateEntry(payload.val.listener) }
					: {}),
			};
		case "BoundUdpSnapshotResponse":
			return {
				type: "bound_udp_snapshot",
				...(payload.val.socket !== null
					? { socket: fromGeneratedSocketStateEntry(payload.val.socket) }
					: {}),
			};
		case "SignalStateResponse":
			return {
				type: "signal_state",
				process_id: payload.val.processId,
				handlers: Object.fromEntries(
					[...payload.val.handlers].map(([signal, registration]) => [
						String(signal),
						{
							action: fromGeneratedSignalDispositionAction(registration.action),
							mask: Array.from(registration.mask),
							flags: registration.flags,
						},
					]),
				),
			};
		case "ZombieTimerCountResponse":
			return {
				type: "zombie_timer_count",
				count: bigIntToSafeNumber(
					payload.val.count,
					"zombie_timer_count.count",
				),
			};
		case "FilesystemResultResponse":
			return {
				type: "filesystem_result",
				operation: fromGeneratedFilesystemOperation(payload.val.operation),
				status: payload.val.status,
				payload_size_bytes: bigIntToSafeNumber(
					payload.val.payloadSizeBytes,
					"filesystem_result.payload_size_bytes",
				),
			};
		case "PermissionDecisionResponse":
			throw new Error(
				"unsupported bare response payload tag: permission_decision",
			);
		case "PersistenceStateResponse":
			return {
				type: "persistence_state",
				key: payload.val.key,
				found: payload.val.found,
				payload_size_bytes: bigIntToSafeNumber(
					payload.val.payloadSizeBytes,
					"persistence_state.payload_size_bytes",
				),
			};
		case "PersistenceFlushedResponse":
			return {
				type: "persistence_flushed",
				key: payload.val.key,
				committed_bytes: bigIntToSafeNumber(
					payload.val.committedBytes,
					"persistence_flushed.committed_bytes",
				),
			};
		case "RejectedResponse":
			return {
				type: "rejected",
				code: payload.val.code,
				message: payload.val.message,
			};
		case "VmFetchResponse":
			return {
				type: "vm_fetch_result",
				response_json: payload.val.responseJson,
			};
		case "ExtEnvelope":
			return {
				type: "ext_result",
				envelope: fromGeneratedExtEnvelope(payload.val),
			};
	}
}
