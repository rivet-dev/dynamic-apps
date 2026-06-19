use crate::wire::{
    self, AuthenticatedResponse, ExtEnvelope, OwnershipScope, ProtocolCodecError, ProtocolFrame,
    RequestFrame, RequestId, RequestPayload, ResponseFrame, ResponsePayload, SessionOpenedResponse,
    SidecarResponseFrame, WireDispatchResult, WireFrameCodec,
};
use crate::{
    Extension, ExtensionInterruptRequest, NativeSidecar, NativeSidecarConfig, SidecarError,
    SidecarRequestTransport,
};
use secure_exec_bridge::{
    BridgeTypes, ChmodRequest, ClockBridge, ClockRequest, CommandPermissionRequest,
    CreateDirRequest, CreateJavascriptContextRequest, CreateWasmContextRequest, DiagnosticRecord,
    DirectoryEntry, EnvironmentPermissionRequest, EventBridge, ExecutionBridge, ExecutionEvent,
    ExecutionHandleRequest, FileMetadata, FilesystemBridge, FilesystemPermissionRequest,
    FilesystemSnapshot, FlushFilesystemStateRequest, GuestContextHandle, KillExecutionRequest,
    LifecycleEventRecord, LoadFilesystemStateRequest, LogRecord, NetworkPermissionRequest,
    PathRequest, PermissionBridge, PermissionDecision, PersistenceBridge,
    PollExecutionEventRequest, RandomBridge, RandomBytesRequest, ReadDirRequest, ReadFileRequest,
    RenameRequest, ScheduleTimerRequest, ScheduledTimer, StartExecutionRequest, StartedExecution,
    StructuredEventRecord, SymlinkRequest, TruncateRequest, WriteExecutionStdinRequest,
    WriteFileRequest,
};
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt;
use std::fs::{self, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::fs::{symlink as create_symlink, MetadataExt, PermissionsExt};
use std::path::{Path, PathBuf};
use std::sync::{mpsc, Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime};
use tokio::sync::mpsc::{channel, unbounded_channel, Receiver};
use tokio::time;

// Guest sync fs/module RPCs are serviced by `pump_process_events` on this timer,
// so a blocked guest call waits up to one interval before the host even sees it.
// At 5ms this dominated per-call latency (~5ms/stat); 250us cuts it ~11x (stat
// 7.5s -> ~0.65s over 1500 ops) and the sub-ms tokio timer is honored. Idle
// pumps are cheap no-ops (try_recv + zero-timeout poll), so the higher cadence
// costs negligible CPU when no guest is issuing RPCs.
const EVENT_PUMP_INTERVAL: Duration = Duration::from_micros(250);
const MAX_STDIN_FRAME_QUEUE: usize = 128;
const MAX_EVENT_READY_QUEUE: usize = 1;
const MAX_STDOUT_FRAME_QUEUE: usize = 128;

#[cfg(test)]
fn request_frame(
    request_id: RequestId,
    ownership: OwnershipScope,
    payload: RequestPayload,
) -> RequestFrame {
    RequestFrame {
        schema: wire::protocol_schema(),
        request_id,
        ownership,
        payload,
    }
}

fn response_frame(
    request_id: RequestId,
    ownership: OwnershipScope,
    payload: ResponsePayload,
) -> ResponseFrame {
    ResponseFrame {
        schema: wire::protocol_schema(),
        request_id,
        ownership,
        payload,
    }
}

#[cfg(test)]
fn connection_ownership(connection_id: &str) -> OwnershipScope {
    OwnershipScope::ConnectionOwnership(wire::ConnectionOwnership {
        connection_id: connection_id.to_owned(),
    })
}

fn session_ownership(connection_id: &str, session_id: &str) -> OwnershipScope {
    OwnershipScope::SessionOwnership(wire::SessionOwnership {
        connection_id: connection_id.to_owned(),
        session_id: session_id.to_owned(),
    })
}

#[cfg(test)]
fn vm_ownership(connection_id: &str, session_id: &str, vm_id: &str) -> OwnershipScope {
    OwnershipScope::VmOwnership(wire::VmOwnership {
        connection_id: connection_id.to_owned(),
        session_id: session_id.to_owned(),
        vm_id: vm_id.to_owned(),
    })
}

fn wire_protocol_error(error: ProtocolCodecError) -> SidecarError {
    SidecarError::InvalidState(format!("invalid generated wire protocol frame: {error}"))
}

pub fn run() -> Result<(), Box<dyn Error>> {
    run_with_extensions(Vec::new())
}

pub fn run_with_extensions(extensions: Vec<Box<dyn Extension>>) -> Result<(), Box<dyn Error>> {
    tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()?
        .block_on(run_async(extensions))
}

async fn run_async(extensions: Vec<Box<dyn Extension>>) -> Result<(), Box<dyn Error>> {
    let config = NativeSidecarConfig {
        compile_cache_root: Some(default_compile_cache_root()),
        ..NativeSidecarConfig::default()
    };
    let codec = WireFrameCodec::new(config.max_frame_bytes);
    let mut sidecar =
        NativeSidecar::with_config_and_extensions(LocalBridge::default(), config, extensions)?;
    let mut active_sessions = BTreeSet::<SessionScope>::new();
    let mut active_connections = BTreeSet::<String>::new();
    let (stdin_tx, mut stdin_rx) =
        channel::<Result<Option<ProtocolFrame>, String>>(MAX_STDIN_FRAME_QUEUE);
    let (event_ready_tx, mut event_ready_rx) = channel::<()>(MAX_EVENT_READY_QUEUE);
    let (write_tx, write_rx) = mpsc::sync_channel::<ProtocolFrame>(MAX_STDOUT_FRAME_QUEUE);
    let (write_error_tx, mut write_error_rx) = unbounded_channel::<String>();
    let callback_transport = Arc::new(FrameSidecarRequestTransport::new(write_tx.clone()));
    sidecar.set_sidecar_request_transport(callback_transport.clone());
    let mut event_pump = time::interval(EVENT_PUMP_INTERVAL);
    let writer_codec = codec.clone();
    let reader_codec = codec.clone();
    let writer_error_tx = write_error_tx.clone();
    thread::spawn(move || {
        let mut writer = io::BufWriter::new(io::stdout());
        while let Ok(frame) = write_rx.recv() {
            if let Err(error) = write_frame(&writer_codec, &mut writer, &frame) {
                let _ = writer_error_tx.send(error.to_string());
                break;
            }
        }
    });

    thread::spawn({
        let callback_transport = callback_transport.clone();
        let read_error_tx = write_error_tx.clone();
        move || {
            let mut stdin = io::stdin();
            loop {
                let frame = match read_frame(&reader_codec, &mut stdin) {
                    Ok(Some(ProtocolFrame::SidecarResponseFrame(response))) => {
                        if callback_transport.accept_response(response.clone()) {
                            continue;
                        }
                        Ok(Some(ProtocolFrame::SidecarResponseFrame(response)))
                    }
                    Ok(Some(frame)) => Ok(Some(frame)),
                    other => other,
                }
                .map_err(|error: Box<dyn Error>| error.to_string());
                let should_stop = matches!(frame, Ok(None) | Err(_));
                match enqueue_stdin_frame(&stdin_tx, frame) {
                    Ok(()) => {}
                    Err(StdinFrameQueueError::Full(message)) => {
                        let _ = read_error_tx.send(message);
                        break;
                    }
                    Err(StdinFrameQueueError::Closed) => break,
                }
                if should_stop {
                    break;
                }
            }
        }
    });

    flush_sidecar_requests(&mut sidecar, &write_tx)?;
    let mut pending_frame: Option<ProtocolFrame> = None;

    loop {
        if let Some(frame) = pending_frame.take() {
            handle_protocol_frame(
                frame,
                &mut sidecar,
                &mut stdin_rx,
                &mut pending_frame,
                &write_tx,
                &mut active_sessions,
                &mut active_connections,
            )
            .await?;
            continue;
        }

        tokio::select! {
            maybe_frame = stdin_rx.recv() => {
                let Some(frame) = maybe_frame else {
                    break;
                };
                let Some(frame) = frame.map_err(io::Error::other)? else {
                    break;
                };
                handle_protocol_frame(
                    frame,
                    &mut sidecar,
                    &mut stdin_rx,
                    &mut pending_frame,
                    &write_tx,
                    &mut active_sessions,
                    &mut active_connections,
                ).await?;
            }
            maybe_ready = event_ready_rx.recv() => {
                let Some(()) = maybe_ready else {
                    break;
                };
                loop {
                    let mut emitted_frame = false;
                    for session in active_sessions.iter().cloned().collect::<Vec<_>>() {
                        if let Some(frame) = sidecar
                            .poll_event_wire(&session.ownership_scope(), Duration::ZERO)
                            .await?
                        {
                            send_output_frame(&write_tx, ProtocolFrame::EventFrame(frame))?;
                            emitted_frame = true;
                        }
                    }

                    if !emitted_frame {
                        break;
                    }
                }
                flush_sidecar_requests(&mut sidecar, &write_tx)?;
            }
            _ = event_pump.tick() => {
                for session in active_sessions.iter().cloned().collect::<Vec<_>>() {
                    if sidecar.pump_process_events(&session.compat_ownership_scope()).await? {
                        let _ = event_ready_tx.try_send(());
                    }
                }
                flush_sidecar_requests(&mut sidecar, &write_tx)?;
            }
            maybe_write_error = write_error_rx.recv() => {
                if let Some(error) = maybe_write_error {
                    return Err(io::Error::new(io::ErrorKind::BrokenPipe, error).into());
                }
            }
        }
    }

    cleanup_connections(&mut sidecar, &active_connections).await;
    Ok(())
}

async fn handle_protocol_frame(
    frame: ProtocolFrame,
    sidecar: &mut NativeSidecar<LocalBridge>,
    stdin_rx: &mut Receiver<Result<Option<ProtocolFrame>, String>>,
    pending_frame: &mut Option<ProtocolFrame>,
    write_tx: &mpsc::SyncSender<ProtocolFrame>,
    active_sessions: &mut BTreeSet<SessionScope>,
    active_connections: &mut BTreeSet<String>,
) -> Result<(), Box<dyn Error>> {
    match frame {
        ProtocolFrame::RequestFrame(request) => {
            let (dispatch, extra_responses) =
                dispatch_with_prompt_interrupt(sidecar, request.clone(), stdin_rx, pending_frame)
                    .await?;
            track_session_state(
                &dispatch.response.payload,
                active_sessions,
                active_connections,
            );

            send_output_frame(write_tx, ProtocolFrame::ResponseFrame(dispatch.response))?;
            for response in extra_responses {
                send_output_frame(write_tx, ProtocolFrame::ResponseFrame(response))?;
            }
            for event in dispatch.events {
                send_output_frame(write_tx, ProtocolFrame::EventFrame(event))?;
            }
            flush_sidecar_requests(sidecar, write_tx)?;
        }
        ProtocolFrame::SidecarResponseFrame(response) => {
            sidecar.accept_wire_sidecar_response(response)?;
            flush_sidecar_requests(sidecar, write_tx)?;
        }
        other => {
            return Err(format!(
                "expected request or sidecar_response frame on stdin, received {}",
                frame_kind(&other)
            )
            .into());
        }
    }
    Ok(())
}

async fn dispatch_with_prompt_interrupt(
    sidecar: &mut NativeSidecar<LocalBridge>,
    request: RequestFrame,
    stdin_rx: &mut Receiver<Result<Option<ProtocolFrame>, String>>,
    pending_frame: &mut Option<ProtocolFrame>,
) -> Result<(WireDispatchResult, Vec<ResponseFrame>), Box<dyn Error>> {
    let Some(blocking_request) = blocking_extension_request(sidecar, &request) else {
        return Ok((sidecar.dispatch_wire(request).await?, Vec::new()));
    };

    let mut dispatch = Box::pin(sidecar.dispatch_wire(request.clone()));
    tokio::select! {
        result = dispatch.as_mut() => Ok((result?, Vec::new())),
        maybe_frame = stdin_rx.recv() => {
            let frame = decode_stdin_frame(maybe_frame)?;
            if let Some(frame) = frame {
                if let Some(interrupt) = extension_interrupt_response(&blocking_request, &request, &frame) {
                    drop(dispatch);
                    let mut extra_responses = Vec::new();
                    if let Some(response) = interrupt.interrupting_response {
                        extra_responses.push(response);
                    } else {
                        *pending_frame = Some(frame);
                    }
                    return Ok((interrupt.interrupted_dispatch, extra_responses));
                }
                *pending_frame = Some(frame);
            }
            Ok((dispatch.await?, Vec::new()))
        }
    }
}

fn decode_stdin_frame(
    maybe_frame: Option<Result<Option<ProtocolFrame>, String>>,
) -> Result<Option<ProtocolFrame>, Box<dyn Error>> {
    let Some(frame) = maybe_frame else {
        return Ok(None);
    };
    Ok(frame.map_err(io::Error::other)?)
}

struct BlockingExtensionRequest {
    namespace: String,
    payload: Vec<u8>,
    extension: Arc<dyn Extension>,
}

struct ExtensionInterruptDispatch {
    interrupted_dispatch: WireDispatchResult,
    interrupting_response: Option<ResponseFrame>,
}

fn blocking_extension_request(
    sidecar: &NativeSidecar<LocalBridge>,
    request: &RequestFrame,
) -> Option<BlockingExtensionRequest> {
    let RequestPayload::ExtEnvelope(envelope) = &request.payload else {
        return None;
    };
    let extension = sidecar.extensions.get(&envelope.namespace)?.clone();
    if !extension.is_blocking_request(&envelope.payload) {
        return None;
    }
    Some(BlockingExtensionRequest {
        namespace: envelope.namespace.clone(),
        payload: envelope.payload.clone(),
        extension,
    })
}

fn extension_interrupt_response(
    blocking_request: &BlockingExtensionRequest,
    active_request: &RequestFrame,
    frame: &ProtocolFrame,
) -> Option<ExtensionInterruptDispatch> {
    match frame {
        ProtocolFrame::RequestFrame(request) => {
            if request.ownership != active_request.ownership {
                return None;
            }
            let interrupt = match &request.payload {
                RequestPayload::ExtEnvelope(envelope)
                    if envelope.namespace == blocking_request.namespace =>
                {
                    blocking_request.extension.interrupt_blocking_request(
                        &blocking_request.payload,
                        ExtensionInterruptRequest::ExtensionPayload(&envelope.payload),
                    )?
                }
                RequestPayload::ExtEnvelope(_) => return None,
                RequestPayload::KillProcessRequest(_) => {
                    blocking_request.extension.interrupt_blocking_request(
                        &blocking_request.payload,
                        ExtensionInterruptRequest::KillProcess,
                    )?
                }
                // Control-plane setup, inspection, filesystem, process plumbing, and
                // persistence requests run concurrently with an in-flight prompt and
                // must not interrupt it. DisposeVm is deliberately non-interrupting for
                // now; see the todo entry about dispose racing a blocked prompt.
                RequestPayload::AuthenticateRequest(_)
                | RequestPayload::OpenSessionRequest(_)
                | RequestPayload::CreateVmRequest(_)
                | RequestPayload::DisposeVmRequest(_)
                | RequestPayload::BootstrapRootFilesystemRequest(_)
                | RequestPayload::ConfigureVmRequest(_)
                | RequestPayload::RegisterHostCallbacksRequest(_)
                | RequestPayload::CreateLayerRequest
                | RequestPayload::SealLayerRequest(_)
                | RequestPayload::ImportSnapshotRequest(_)
                | RequestPayload::ExportSnapshotRequest(_)
                | RequestPayload::CreateOverlayRequest(_)
                | RequestPayload::GuestFilesystemCallRequest(_)
                | RequestPayload::SnapshotRootFilesystemRequest
                | RequestPayload::ExecuteRequest(_)
                | RequestPayload::WriteStdinRequest(_)
                | RequestPayload::CloseStdinRequest(_)
                | RequestPayload::GetProcessSnapshotRequest
                | RequestPayload::FindListenerRequest(_)
                | RequestPayload::FindBoundUdpRequest(_)
                | RequestPayload::VmFetchRequest(_)
                | RequestPayload::GetSignalStateRequest(_)
                | RequestPayload::GetZombieTimerCountRequest
                | RequestPayload::HostFilesystemCallRequest(_)
                | RequestPayload::PersistenceLoadRequest(_)
                | RequestPayload::PersistenceFlushRequest(_) => return None,
            };
            let interrupted_dispatch = interrupted_extension_dispatch(
                active_request,
                &blocking_request.namespace,
                interrupt.interrupted_response_payload,
            );
            let interrupting_response = interrupt.interrupting_response_payload.map(|payload| {
                response_frame(
                    request.request_id,
                    request.ownership.clone(),
                    ResponsePayload::ExtEnvelope(ExtEnvelope {
                        namespace: blocking_request.namespace.clone(),
                        payload,
                    }),
                )
            });
            Some(ExtensionInterruptDispatch {
                interrupted_dispatch,
                interrupting_response,
            })
        }
        // Response, Event, and SidecarRequest frames are sidecar-to-host only. If one
        // arrives on stdin it is requeued and rejected as a protocol error by
        // handle_protocol_frame, so it must not synthesize a cancelled prompt first.
        // SidecarResponse frames answer sidecar-initiated callbacks and may be the very
        // response the blocked prompt dispatch is waiting on, so they never interrupt.
        ProtocolFrame::ResponseFrame(_)
        | ProtocolFrame::EventFrame(_)
        | ProtocolFrame::SidecarRequestFrame(_)
        | ProtocolFrame::SidecarResponseFrame(_) => None,
    }
}

fn interrupted_extension_dispatch(
    request: &RequestFrame,
    namespace: &str,
    payload: Vec<u8>,
) -> WireDispatchResult {
    match &request.payload {
        RequestPayload::ExtEnvelope(_) => {
            let response = ResponsePayload::ExtEnvelope(ExtEnvelope {
                namespace: namespace.to_string(),
                payload,
            });
            WireDispatchResult {
                response: response_frame(request.request_id, request.ownership.clone(), response),
                events: Vec::new(),
            }
        }
        RequestPayload::AuthenticateRequest(_)
        | RequestPayload::OpenSessionRequest(_)
        | RequestPayload::CreateVmRequest(_)
        | RequestPayload::DisposeVmRequest(_)
        | RequestPayload::BootstrapRootFilesystemRequest(_)
        | RequestPayload::ConfigureVmRequest(_)
        | RequestPayload::RegisterHostCallbacksRequest(_)
        | RequestPayload::CreateLayerRequest
        | RequestPayload::SealLayerRequest(_)
        | RequestPayload::ImportSnapshotRequest(_)
        | RequestPayload::ExportSnapshotRequest(_)
        | RequestPayload::CreateOverlayRequest(_)
        | RequestPayload::GuestFilesystemCallRequest(_)
        | RequestPayload::SnapshotRootFilesystemRequest
        | RequestPayload::ExecuteRequest(_)
        | RequestPayload::WriteStdinRequest(_)
        | RequestPayload::CloseStdinRequest(_)
        | RequestPayload::KillProcessRequest(_)
        | RequestPayload::GetProcessSnapshotRequest
        | RequestPayload::FindListenerRequest(_)
        | RequestPayload::FindBoundUdpRequest(_)
        | RequestPayload::VmFetchRequest(_)
        | RequestPayload::GetSignalStateRequest(_)
        | RequestPayload::GetZombieTimerCountRequest
        | RequestPayload::HostFilesystemCallRequest(_)
        | RequestPayload::PersistenceLoadRequest(_)
        | RequestPayload::PersistenceFlushRequest(_) => {
            unreachable!("interrupted extension dispatch requires an extension request");
        }
    }
}

async fn cleanup_connections(
    sidecar: &mut NativeSidecar<LocalBridge>,
    active_connections: &BTreeSet<String>,
) {
    for connection_id in active_connections {
        let _ = sidecar.remove_connection(connection_id).await;
    }
}

fn track_session_state(
    payload: &ResponsePayload,
    active_sessions: &mut BTreeSet<SessionScope>,
    active_connections: &mut BTreeSet<String>,
) {
    match payload {
        ResponsePayload::AuthenticatedResponse(AuthenticatedResponse { connection_id, .. }) => {
            active_connections.insert(connection_id.clone());
        }
        ResponsePayload::SessionOpenedResponse(SessionOpenedResponse {
            session_id,
            owner_connection_id,
        }) => {
            active_sessions.insert(SessionScope {
                connection_id: owner_connection_id.clone(),
                session_id: session_id.clone(),
            });
        }
        _ => {}
    }
}

fn read_frame(
    codec: &WireFrameCodec,
    reader: &mut impl Read,
) -> Result<Option<ProtocolFrame>, Box<dyn Error>> {
    let mut prefix = [0u8; 4];
    match reader.read_exact(&mut prefix) {
        Ok(()) => {}
        Err(error) if error.kind() == io::ErrorKind::UnexpectedEof => {
            return Ok(None);
        }
        Err(error) => return Err(error.into()),
    }

    let declared_len = u32::from_be_bytes(prefix) as usize;
    if declared_len > codec.max_frame_bytes() {
        return Err(ProtocolCodecError::FrameTooLarge {
            size: declared_len,
            max: codec.max_frame_bytes(),
        }
        .into());
    }
    let total_len = prefix.len().saturating_add(declared_len);
    let mut bytes = Vec::with_capacity(total_len);
    bytes.extend_from_slice(&prefix);
    bytes.resize(total_len, 0);
    reader.read_exact(&mut bytes[prefix.len()..])?;

    Ok(Some(codec.decode(&bytes)?))
}

fn write_frame(
    codec: &WireFrameCodec,
    writer: &mut impl Write,
    frame: &ProtocolFrame,
) -> Result<(), Box<dyn Error>> {
    let bytes = codec.encode(frame)?;
    writer.write_all(&bytes)?;
    writer.flush()?;
    Ok(())
}

fn frame_kind(frame: &ProtocolFrame) -> &'static str {
    match frame {
        ProtocolFrame::RequestFrame(_) => "request",
        ProtocolFrame::ResponseFrame(_) => "response",
        ProtocolFrame::EventFrame(_) => "event",
        ProtocolFrame::SidecarRequestFrame(_) => "sidecar_request",
        ProtocolFrame::SidecarResponseFrame(_) => "sidecar_response",
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum StdinFrameQueueError {
    Full(String),
    Closed,
}

fn enqueue_stdin_frame(
    sender: &tokio::sync::mpsc::Sender<Result<Option<ProtocolFrame>, String>>,
    frame: Result<Option<ProtocolFrame>, String>,
) -> Result<(), StdinFrameQueueError> {
    sender.try_send(frame).map_err(|error| match error {
        tokio::sync::mpsc::error::TrySendError::Full(_) => StdinFrameQueueError::Full(format!(
            "stdin frame queue exceeded {MAX_STDIN_FRAME_QUEUE} pending frames"
        )),
        tokio::sync::mpsc::error::TrySendError::Closed(_) => StdinFrameQueueError::Closed,
    })
}

fn flush_sidecar_requests(
    sidecar: &mut NativeSidecar<LocalBridge>,
    writer: &mpsc::SyncSender<ProtocolFrame>,
) -> Result<(), Box<dyn Error>> {
    while let Some(request) = sidecar.pop_wire_sidecar_request()? {
        send_output_frame(writer, ProtocolFrame::SidecarRequestFrame(request))?;
    }
    Ok(())
}

fn send_output_frame(
    writer: &mpsc::SyncSender<ProtocolFrame>,
    frame: ProtocolFrame,
) -> Result<(), io::Error> {
    writer.try_send(frame).map_err(|error| {
        let message = match error {
            mpsc::TrySendError::Full(_) => {
                format!("stdout frame queue exceeded {MAX_STDOUT_FRAME_QUEUE} pending frames")
            }
            mpsc::TrySendError::Disconnected(_) => String::from("stdout writer disconnected"),
        };
        io::Error::new(io::ErrorKind::BrokenPipe, message)
    })
}

fn default_compile_cache_root() -> PathBuf {
    // Stable across sidecar processes so V8 compile-cache (cachedData) survives a
    // fresh sidecar/VM and benefits cold starts. Previously keyed by PID, which
    // gave every process an empty cache — cold module imports never reused
    // compiled bytecode. Entries are namespaced+validated downstream by
    // `stable_compile_cache_namespace_hash` + V8's source/version checks, so a
    // shared root is safe; stale or mismatched entries are simply ignored.
    std::env::temp_dir().join("secure-exec-sidecar-compile-cache")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::wire::{AuthenticateRequest, KillProcessRequest};
    use crate::{ExtensionContext, ExtensionFuture, ExtensionInterruptResponse, ExtensionResponse};
    use std::io::Cursor;

    const TEST_EXTENSION_NAMESPACE: &str = "dev.rivet.secure-exec.test.blocking";

    #[test]
    fn read_frame_rejects_oversized_prefix_before_allocating_payload() {
        let codec = WireFrameCodec::new(16);
        let mut reader = Cursor::new((32_u32).to_be_bytes().to_vec());

        let error = read_frame(&codec, &mut reader).expect_err("oversized frame should fail");
        let error = error
            .downcast::<ProtocolCodecError>()
            .expect("protocol codec error");
        assert!(matches!(
            *error,
            ProtocolCodecError::FrameTooLarge { size: 32, max: 16 }
        ));
    }

    #[test]
    fn stdio_work_queues_are_bounded() {
        let (stdin_tx, _stdin_rx) =
            channel::<Result<Option<ProtocolFrame>, String>>(MAX_STDIN_FRAME_QUEUE);
        for _ in 0..MAX_STDIN_FRAME_QUEUE {
            enqueue_stdin_frame(&stdin_tx, Ok(None))
                .expect("stdin frame queue should accept capacity");
        }
        assert!(matches!(
            enqueue_stdin_frame(&stdin_tx, Ok(None)),
            Err(StdinFrameQueueError::Full(_))
        ));

        let (event_ready_tx, _event_ready_rx) = channel::<()>(MAX_EVENT_READY_QUEUE);
        event_ready_tx
            .try_send(())
            .expect("event-ready queue should accept capacity");
        assert!(matches!(
            event_ready_tx.try_send(()),
            Err(tokio::sync::mpsc::error::TrySendError::Full(_))
        ));

        let (stdout_tx, _stdout_rx) = mpsc::sync_channel(MAX_STDOUT_FRAME_QUEUE);
        for request_id in 0..MAX_STDOUT_FRAME_QUEUE {
            send_output_frame(
                &stdout_tx,
                ProtocolFrame::RequestFrame(request_frame(
                    request_id as RequestId,
                    connection_ownership("conn-queue"),
                    RequestPayload::AuthenticateRequest(AuthenticateRequest {
                        client_name: String::from("queue-test"),
                        auth_token: String::from("token"),
                        protocol_version: wire::PROTOCOL_VERSION,
                        bridge_version: secure_exec_bridge::bridge_contract().version,
                    }),
                )),
            )
            .expect("stdout frame queue should accept capacity");
        }
        let error = send_output_frame(
            &stdout_tx,
            ProtocolFrame::RequestFrame(request_frame(
                MAX_STDOUT_FRAME_QUEUE as RequestId,
                connection_ownership("conn-queue"),
                RequestPayload::AuthenticateRequest(AuthenticateRequest {
                    client_name: String::from("queue-test"),
                    auth_token: String::from("token"),
                    protocol_version: wire::PROTOCOL_VERSION,
                    bridge_version: secure_exec_bridge::bridge_contract().version,
                }),
            )),
        )
        .expect_err("stdout frame queue should reject overflow");
        assert!(
            error.to_string().contains("stdout frame queue exceeded"),
            "unexpected stdout queue error: {error}"
        );
    }

    #[test]
    fn read_frame_decodes_wire_authenticate_request() {
        let codec = WireFrameCodec::new(wire::DEFAULT_MAX_FRAME_BYTES);
        let frame = ProtocolFrame::RequestFrame(request_frame(
            1,
            connection_ownership("client-hint"),
            RequestPayload::AuthenticateRequest(AuthenticateRequest {
                client_name: "probe".to_string(),
                auth_token: "probe-token".to_string(),
                protocol_version: wire::PROTOCOL_VERSION,
                bridge_version: secure_exec_bridge::bridge_contract().version,
            }),
        ));
        let encoded = codec.encode(&frame).expect("encode wire frame");
        let mut reader = Cursor::new(encoded);

        let decoded = read_frame(&codec, &mut reader)
            .expect("decode bare frame")
            .expect("frame present");

        assert_eq!(decoded, frame);
    }

    #[test]
    fn extension_close_interrupts_matching_blocking_request() {
        let ownership = vm_ownership("conn-1", "session-1", "vm-1");
        let prompt = test_extension_request_frame(10, ownership.clone(), "prompt:ext-session-1");
        let close = ProtocolFrame::RequestFrame(test_extension_request_frame(
            11,
            ownership,
            "close:ext-session-1",
        ));

        let blocking_request = blocking_extension_request(&prompt);
        let interrupt = extension_interrupt_response(&blocking_request, &prompt, &close)
            .expect("close should interrupt prompt");

        assert_eq!(interrupt.interrupted_dispatch.response.request_id, 10);
        let ResponsePayload::ExtEnvelope(envelope) =
            interrupt.interrupted_dispatch.response.payload
        else {
            panic!("expected extension response");
        };
        assert_eq!(envelope.namespace, TEST_EXTENSION_NAMESPACE);
        assert_eq!(envelope.payload, b"prompt-cancelled:ext-session-1");
    }

    #[test]
    fn extension_cancel_interrupt_gets_synthetic_response() {
        let ownership = vm_ownership("conn-1", "session-1", "vm-1");
        let prompt = test_extension_request_frame(10, ownership.clone(), "prompt:ext-session-1");
        let cancel = ProtocolFrame::RequestFrame(test_extension_request_frame(
            11,
            ownership,
            "cancel:ext-session-1",
        ));

        let blocking_request = blocking_extension_request(&prompt);
        let interrupt = extension_interrupt_response(&blocking_request, &prompt, &cancel)
            .expect("cancel should interrupt prompt");
        let response = interrupt
            .interrupting_response
            .expect("cancel should get a response");

        assert_eq!(response.request_id, 11);
        let ResponsePayload::ExtEnvelope(envelope) = response.payload else {
            panic!("expected extension response");
        };
        assert_eq!(envelope.namespace, TEST_EXTENSION_NAMESPACE);
        assert_eq!(envelope.payload, b"cancelled:ext-session-1");
    }

    #[test]
    fn kill_process_interrupts_blocking_extension_request() {
        let ownership = vm_ownership("conn-1", "session-1", "vm-1");
        let prompt = test_extension_request_frame(10, ownership.clone(), "prompt:ext-session-1");
        let kill = ProtocolFrame::RequestFrame(request_frame(
            11,
            ownership,
            RequestPayload::KillProcessRequest(KillProcessRequest {
                process_id: "adapter-process".to_string(),
                signal: "SIGTERM".to_string(),
            }),
        ));

        let blocking_request = blocking_extension_request(&prompt);
        let interrupt = extension_interrupt_response(&blocking_request, &prompt, &kill)
            .expect("kill should interrupt prompt");

        assert_eq!(interrupt.interrupted_dispatch.response.request_id, 10);
        assert!(interrupt.interrupting_response.is_none());
    }

    fn test_extension_request_frame(
        request_id: RequestId,
        ownership: OwnershipScope,
        payload: &str,
    ) -> RequestFrame {
        request_frame(
            request_id,
            ownership,
            RequestPayload::ExtEnvelope(ExtEnvelope {
                namespace: TEST_EXTENSION_NAMESPACE.to_string(),
                payload: payload.as_bytes().to_vec(),
            }),
        )
    }

    fn blocking_extension_request(request: &RequestFrame) -> BlockingExtensionRequest {
        let RequestPayload::ExtEnvelope(envelope) = &request.payload else {
            panic!("expected extension request");
        };
        BlockingExtensionRequest {
            namespace: TEST_EXTENSION_NAMESPACE.to_string(),
            payload: envelope.payload.clone(),
            extension: Arc::new(TestBlockingInterruptExtension),
        }
    }

    struct TestBlockingInterruptExtension;

    impl Extension for TestBlockingInterruptExtension {
        fn namespace(&self) -> &str {
            TEST_EXTENSION_NAMESPACE
        }

        fn handle_request<'a>(
            &'a self,
            _ctx: ExtensionContext<'a>,
            _payload: Vec<u8>,
        ) -> ExtensionFuture<'a, ExtensionResponse> {
            Box::pin(async { Ok(ExtensionResponse::new(Vec::new())) })
        }

        fn is_blocking_request(&self, payload: &[u8]) -> bool {
            parse_test_payload(payload).is_some_and(|(kind, _session_id)| kind == "prompt")
        }

        fn interrupt_blocking_request(
            &self,
            blocking_payload: &[u8],
            interrupt: ExtensionInterruptRequest<'_>,
        ) -> Option<ExtensionInterruptResponse> {
            let (blocking_kind, blocking_session_id) = parse_test_payload(blocking_payload)?;
            if blocking_kind != "prompt" {
                return None;
            }

            let interrupted_response_payload =
                encode_test_response("prompt-cancelled", blocking_session_id);
            match interrupt {
                ExtensionInterruptRequest::KillProcess => Some(ExtensionInterruptResponse {
                    interrupted_response_payload,
                    interrupting_response_payload: None,
                }),
                ExtensionInterruptRequest::ExtensionPayload(payload) => {
                    let (interrupt_kind, interrupt_session_id) = parse_test_payload(payload)?;
                    match interrupt_kind {
                        "close" if interrupt_session_id == blocking_session_id => {
                            Some(ExtensionInterruptResponse {
                                interrupted_response_payload,
                                interrupting_response_payload: None,
                            })
                        }
                        "cancel" if interrupt_session_id == blocking_session_id => {
                            Some(ExtensionInterruptResponse {
                                interrupted_response_payload,
                                interrupting_response_payload: Some(encode_test_response(
                                    "cancelled",
                                    interrupt_session_id,
                                )),
                            })
                        }
                        "prompt" | "close" | "cancel" => None,
                        _ => None,
                    }
                }
            }
        }
    }

    fn parse_test_payload(payload: &[u8]) -> Option<(&str, &str)> {
        let payload = std::str::from_utf8(payload).ok()?;
        payload.split_once(':')
    }

    fn encode_test_response(kind: &str, session_id: &str) -> Vec<u8> {
        format!("{kind}:{session_id}").into_bytes()
    }
}

#[derive(Debug, Clone)]
struct LocalBridge {
    started_at: Instant,
    next_timer_id: usize,
    snapshots: BTreeMap<String, FilesystemSnapshot>,
}

impl Default for LocalBridge {
    fn default() -> Self {
        Self {
            started_at: Instant::now(),
            next_timer_id: 0,
            snapshots: BTreeMap::new(),
        }
    }
}

impl BridgeTypes for LocalBridge {
    type Error = LocalBridgeError;
}

impl FilesystemBridge for LocalBridge {
    fn read_file(&mut self, request: ReadFileRequest) -> Result<Vec<u8>, Self::Error> {
        fs::read(Self::host_path(&request.path))
            .map_err(|error| LocalBridgeError::io("read", &request.path, error))
    }

    fn write_file(&mut self, request: WriteFileRequest) -> Result<(), Self::Error> {
        let host_path = Self::host_path(&request.path);
        if let Some(parent) = host_path.parent() {
            fs::create_dir_all(parent)
                .map_err(|error| LocalBridgeError::io("mkdir", &request.path, error))?;
        }
        fs::write(host_path, request.contents)
            .map_err(|error| LocalBridgeError::io("write", &request.path, error))
    }

    fn stat(&mut self, request: PathRequest) -> Result<FileMetadata, Self::Error> {
        fs::metadata(Self::host_path(&request.path))
            .map(Self::file_metadata)
            .map_err(|error| LocalBridgeError::io("stat", &request.path, error))
    }

    fn lstat(&mut self, request: PathRequest) -> Result<FileMetadata, Self::Error> {
        fs::symlink_metadata(Self::host_path(&request.path))
            .map(Self::file_metadata)
            .map_err(|error| LocalBridgeError::io("lstat", &request.path, error))
    }

    fn read_dir(&mut self, request: ReadDirRequest) -> Result<Vec<DirectoryEntry>, Self::Error> {
        let mut entries = fs::read_dir(Self::host_path(&request.path))
            .map_err(|error| LocalBridgeError::io("readdir", &request.path, error))?
            .map(|entry| {
                let entry =
                    entry.map_err(|error| LocalBridgeError::io("readdir", &request.path, error))?;
                let kind = entry
                    .file_type()
                    .map(Self::file_kind)
                    .map_err(|error| LocalBridgeError::io("readdir", &request.path, error))?;
                Ok(DirectoryEntry {
                    name: entry.file_name().to_string_lossy().into_owned(),
                    kind,
                })
            })
            .collect::<Result<Vec<_>, LocalBridgeError>>()?;
        entries.sort_by(|left, right| left.name.cmp(&right.name));
        Ok(entries)
    }

    fn create_dir(&mut self, request: CreateDirRequest) -> Result<(), Self::Error> {
        let host_path = Self::host_path(&request.path);
        if request.recursive {
            fs::create_dir_all(host_path)
        } else {
            fs::create_dir(host_path)
        }
        .map_err(|error| LocalBridgeError::io("mkdir", &request.path, error))
    }

    fn remove_file(&mut self, request: PathRequest) -> Result<(), Self::Error> {
        fs::remove_file(Self::host_path(&request.path))
            .map_err(|error| LocalBridgeError::io("unlink", &request.path, error))
    }

    fn remove_dir(&mut self, request: PathRequest) -> Result<(), Self::Error> {
        fs::remove_dir(Self::host_path(&request.path))
            .map_err(|error| LocalBridgeError::io("rmdir", &request.path, error))
    }

    fn rename(&mut self, request: RenameRequest) -> Result<(), Self::Error> {
        let from_path = Self::host_path(&request.from_path);
        let to_path = Self::host_path(&request.to_path);
        if let Some(parent) = to_path.parent() {
            fs::create_dir_all(parent)
                .map_err(|error| LocalBridgeError::io("mkdir", &request.to_path, error))?;
        }
        fs::rename(from_path, to_path).map_err(|error| {
            LocalBridgeError::unsupported(format!(
                "rename {} -> {}: {}",
                request.from_path, request.to_path, error
            ))
        })
    }

    fn symlink(&mut self, request: SymlinkRequest) -> Result<(), Self::Error> {
        let link_path = Self::host_path(&request.link_path);
        if let Some(parent) = link_path.parent() {
            fs::create_dir_all(parent)
                .map_err(|error| LocalBridgeError::io("mkdir", &request.link_path, error))?;
        }
        create_symlink(&request.target_path, link_path)
            .map_err(|error| LocalBridgeError::io("symlink", &request.link_path, error))
    }

    fn read_link(&mut self, request: PathRequest) -> Result<String, Self::Error> {
        fs::read_link(Self::host_path(&request.path))
            .map(|target| target.to_string_lossy().into_owned())
            .map_err(|error| LocalBridgeError::io("readlink", &request.path, error))
    }

    fn chmod(&mut self, request: ChmodRequest) -> Result<(), Self::Error> {
        let permissions = fs::Permissions::from_mode(request.mode);
        fs::set_permissions(Self::host_path(&request.path), permissions)
            .map_err(|error| LocalBridgeError::io("chmod", &request.path, error))
    }

    fn truncate(&mut self, request: TruncateRequest) -> Result<(), Self::Error> {
        OpenOptions::new()
            .write(true)
            .create(false)
            .open(Self::host_path(&request.path))
            .and_then(|file| file.set_len(request.len))
            .map_err(|error| LocalBridgeError::io("truncate", &request.path, error))
    }

    fn exists(&mut self, request: PathRequest) -> Result<bool, Self::Error> {
        Ok(fs::symlink_metadata(Self::host_path(&request.path)).is_ok())
    }
}

impl PermissionBridge for LocalBridge {
    fn check_filesystem_access(
        &mut self,
        request: FilesystemPermissionRequest,
    ) -> Result<PermissionDecision, Self::Error> {
        Ok(PermissionDecision::deny(format!(
            "no static filesystem policy registered for {}:{}",
            request.vm_id, request.path
        )))
    }

    fn check_network_access(
        &mut self,
        request: NetworkPermissionRequest,
    ) -> Result<PermissionDecision, Self::Error> {
        Ok(PermissionDecision::deny(format!(
            "no static network policy registered for {}:{}",
            request.vm_id, request.resource
        )))
    }

    fn check_command_execution(
        &mut self,
        request: CommandPermissionRequest,
    ) -> Result<PermissionDecision, Self::Error> {
        Ok(PermissionDecision::deny(format!(
            "no static child_process policy registered for {}:{}",
            request.vm_id, request.command
        )))
    }

    fn check_environment_access(
        &mut self,
        request: EnvironmentPermissionRequest,
    ) -> Result<PermissionDecision, Self::Error> {
        Ok(PermissionDecision::deny(format!(
            "no static env policy registered for {}:{}",
            request.vm_id, request.key
        )))
    }
}

impl PersistenceBridge for LocalBridge {
    fn load_filesystem_state(
        &mut self,
        request: LoadFilesystemStateRequest,
    ) -> Result<Option<FilesystemSnapshot>, Self::Error> {
        Ok(self.snapshots.get(&request.vm_id).cloned())
    }

    fn flush_filesystem_state(
        &mut self,
        request: FlushFilesystemStateRequest,
    ) -> Result<(), Self::Error> {
        self.snapshots.insert(request.vm_id, request.snapshot);
        Ok(())
    }
}

impl ClockBridge for LocalBridge {
    fn wall_clock(&mut self, _request: ClockRequest) -> Result<SystemTime, Self::Error> {
        Ok(SystemTime::now())
    }

    fn monotonic_clock(&mut self, _request: ClockRequest) -> Result<Duration, Self::Error> {
        Ok(self.started_at.elapsed())
    }

    fn schedule_timer(
        &mut self,
        request: ScheduleTimerRequest,
    ) -> Result<ScheduledTimer, Self::Error> {
        self.next_timer_id += 1;
        Ok(ScheduledTimer {
            timer_id: format!("timer-{}", self.next_timer_id),
            delay: request.delay,
        })
    }
}

impl RandomBridge for LocalBridge {
    fn fill_random_bytes(&mut self, request: RandomBytesRequest) -> Result<Vec<u8>, Self::Error> {
        Ok(vec![0u8; request.len])
    }
}

impl EventBridge for LocalBridge {
    fn emit_structured_event(&mut self, _event: StructuredEventRecord) -> Result<(), Self::Error> {
        Ok(())
    }

    fn emit_diagnostic(&mut self, _event: DiagnosticRecord) -> Result<(), Self::Error> {
        Ok(())
    }

    fn emit_log(&mut self, _event: LogRecord) -> Result<(), Self::Error> {
        Ok(())
    }

    fn emit_lifecycle(&mut self, _event: LifecycleEventRecord) -> Result<(), Self::Error> {
        Ok(())
    }
}

impl ExecutionBridge for LocalBridge {
    fn create_javascript_context(
        &mut self,
        _request: CreateJavascriptContextRequest,
    ) -> Result<GuestContextHandle, Self::Error> {
        Err(LocalBridgeError::unsupported(
            "execution bridge is handled internally by the native sidecar",
        ))
    }

    fn create_wasm_context(
        &mut self,
        _request: CreateWasmContextRequest,
    ) -> Result<GuestContextHandle, Self::Error> {
        Err(LocalBridgeError::unsupported(
            "execution bridge is handled internally by the native sidecar",
        ))
    }

    fn start_execution(
        &mut self,
        _request: StartExecutionRequest,
    ) -> Result<StartedExecution, Self::Error> {
        Err(LocalBridgeError::unsupported(
            "execution bridge is handled internally by the native sidecar",
        ))
    }

    fn write_stdin(&mut self, _request: WriteExecutionStdinRequest) -> Result<(), Self::Error> {
        Err(LocalBridgeError::unsupported(
            "execution bridge is handled internally by the native sidecar",
        ))
    }

    fn close_stdin(&mut self, _request: ExecutionHandleRequest) -> Result<(), Self::Error> {
        Err(LocalBridgeError::unsupported(
            "execution bridge is handled internally by the native sidecar",
        ))
    }

    fn kill_execution(&mut self, _request: KillExecutionRequest) -> Result<(), Self::Error> {
        Err(LocalBridgeError::unsupported(
            "execution bridge is handled internally by the native sidecar",
        ))
    }

    fn poll_execution_event(
        &mut self,
        _request: PollExecutionEventRequest,
    ) -> Result<Option<ExecutionEvent>, Self::Error> {
        Ok(None)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
struct SessionScope {
    connection_id: String,
    session_id: String,
}

impl SessionScope {
    fn ownership_scope(&self) -> OwnershipScope {
        session_ownership(&self.connection_id, &self.session_id)
    }

    fn compat_ownership_scope(&self) -> crate::protocol::OwnershipScope {
        wire::ownership_scope_to_compat(self.ownership_scope())
    }
}

struct FrameSidecarRequestTransport {
    writer: mpsc::SyncSender<ProtocolFrame>,
    pending: Arc<Mutex<BTreeMap<RequestId, mpsc::SyncSender<SidecarResponseFrame>>>>,
}

impl FrameSidecarRequestTransport {
    fn new(writer: mpsc::SyncSender<ProtocolFrame>) -> Self {
        Self {
            writer,
            pending: Arc::new(Mutex::new(BTreeMap::new())),
        }
    }

    fn accept_response(&self, response: SidecarResponseFrame) -> bool {
        let sender = {
            let mut pending = match self.pending.lock() {
                Ok(pending) => pending,
                Err(_) => return false,
            };
            pending.remove(&response.request_id)
        };
        let Some(sender) = sender else {
            return false;
        };
        let _ = sender.send(response);
        true
    }
}

impl SidecarRequestTransport for FrameSidecarRequestTransport {
    fn send_request(
        &self,
        request: crate::protocol::SidecarRequestFrame,
        timeout: Duration,
    ) -> Result<crate::protocol::SidecarResponseFrame, SidecarError> {
        let request =
            wire::sidecar_request_frame_from_compat(request).map_err(wire_protocol_error)?;
        let (sender, receiver) = mpsc::sync_channel(1);
        self.pending
            .lock()
            .map_err(|_| {
                SidecarError::Bridge(String::from("sidecar callback waiter map lock poisoned"))
            })?
            .insert(request.request_id, sender);
        if let Err(error) = send_output_frame(
            &self.writer,
            ProtocolFrame::SidecarRequestFrame(request.clone()),
        ) {
            let _ = self
                .pending
                .lock()
                .map(|mut pending| pending.remove(&request.request_id));
            return Err(SidecarError::Io(format!(
                "failed to write sidecar request frame: {error}"
            )));
        }
        match receiver.recv_timeout(timeout) {
            Ok(response) => {
                wire::sidecar_response_frame_to_compat(response).map_err(wire_protocol_error)
            }
            Err(mpsc::RecvTimeoutError::Timeout) => {
                let _ = self
                    .pending
                    .lock()
                    .map(|mut pending| pending.remove(&request.request_id));
                Err(SidecarError::Io(format!(
                    "timed out waiting for sidecar response after {}s",
                    timeout.as_secs()
                )))
            }
            Err(mpsc::RecvTimeoutError::Disconnected) => Err(SidecarError::Io(String::from(
                "sidecar response waiter disconnected",
            ))),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct LocalBridgeError {
    message: String,
}

impl LocalBridgeError {
    fn unsupported(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }

    fn io(operation: &str, path: &str, error: io::Error) -> Self {
        Self::unsupported(format!("{operation} {path}: {error}"))
    }
}

impl fmt::Display for LocalBridgeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl Error for LocalBridgeError {}

impl LocalBridge {
    fn host_path(path: &str) -> PathBuf {
        let candidate = Path::new(path);
        if candidate.is_absolute() {
            candidate.to_path_buf()
        } else {
            std::env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .join(candidate)
        }
    }

    fn file_metadata(metadata: fs::Metadata) -> FileMetadata {
        FileMetadata {
            mode: metadata.permissions().mode(),
            size: metadata.size(),
            kind: Self::file_kind(metadata.file_type()),
        }
    }

    fn file_kind(file_type: fs::FileType) -> secure_exec_bridge::FileKind {
        if file_type.is_file() {
            secure_exec_bridge::FileKind::File
        } else if file_type.is_dir() {
            secure_exec_bridge::FileKind::Directory
        } else if file_type.is_symlink() {
            secure_exec_bridge::FileKind::SymbolicLink
        } else {
            secure_exec_bridge::FileKind::Other
        }
    }
}
