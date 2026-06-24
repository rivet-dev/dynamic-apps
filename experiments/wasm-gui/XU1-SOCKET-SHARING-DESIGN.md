# XU1 design note — cross-thread host_net socket sharing (the GDBus blocker)

Status: **DESIGN / not yet implemented** (2026-06-24). Blocker for XU1 (GDBus → xfconf/xfsettingsd).
Root cause empirically confirmed via `SECURE_EXEC_NET_TRACE` (proof
`~/tmp/gui-progress/2026-06-24T21/xu1-rootcause-netttrace.txt`).

## The problem

GDBus authenticates on the **main thread**, then does all message I/O on a **GDBus worker thread**.
A wasm thread runs in its **own V8 isolate** (`node_import_cache.rs` ~14282, "second-isolate spawn
sharing guestSharedMemory"), and a host_net socket is visible **only in the isolate that created it**.
The worker isolate's first socket op on the main thread's fd fails and GIO reports "connection closed".

Three layers each scope socket state too narrowly:

1. **Runner (per-isolate).** `hostNetSockets` is a module-level `Map` in `node_import_cache.rs` (~11144);
   each thread isolate has its own. `getHostNetSocket(fd)` misses → `net_send`/`net_recv` return EBADF.
   The trace shows it: main isolate does auth I/O on `fd=…824 sid=unix-socket-1`; the worker isolate
   logs `send fd=…824 BADF (no socket in this isolate; known=0)`.
2. **Sidecar (per-process_id).** The real connection (`UnixSocket` = an owned `UnixStream` + reader
   thread + event channel) lives in `process.unix_sockets`, keyed by `socket_id`, on the **owning**
   `active_process`. A worker thread is a separate `active_process` (`{parent}~thread~{N}`, line 5615)
   so its `unix_sockets` is empty even though it shares `parent_kernel_pid` (line 5570).
3. **Readiness (per-process).** `process.socket_readiness: Arc<SocketReadiness>` (line 363) is signalled
   by that socket's reader thread into the **owner** process; `net.poll_wait` waits on the *caller's*
   readiness, so a worker would not be woken by the owner's socket.

M8's X clients never hit this: GTK does all X I/O on the main thread.

## Why not patch GDBus (constraint #5)

GDBus always uses a dedicated worker thread for the connection; there is no supported single-thread
mode. The fix must be in the platform layer (runtime/sidecar), not the component.

## Chosen approach — route a thread's socket op to its owning ancestor process

The sidecar already has the machinery: `descendant_parent_process` / `descendant_parent_process_mut`
(lines 3929/3938) walk the `~thread~` parent chain, and worker process ids encode the parent. So a
worker's socket op can be serviced on the **owner's** `UnixSocket` + `socket_readiness` without moving
or duplicating the connection (only one thread uses the socket at a time post-auth, so no concurrency
rewire is needed).

### Part A — runner: resolve fd → socket_id across isolates

The worker isolate has the fd (it lives in shared wasm memory) but not the `socket_id`. Add a tiny
sidecar-side registry so any isolate can resolve it:

- New RPC `net.register_guest_fd(fd, socketId)`: the runner calls it whenever it assigns `socketId`
  to a socket (in `net_connect` and the `accept` path) and whenever `net_set_nonblock` changes. Store
  it on the VM keyed by `(kernel_pid, fd)` (kernel_pid is shared across the threads).
- New RPC `net.resolve_guest_fd(fd) -> {socketId, nonblock} | null`: `getHostNetSocket(fd)`, on a miss,
  calls it; if found, builds a minimal local `hostNetSockets` entry (`socketId`, `nonblock`, empty
  `readChunks`) and proceeds. Subsequent ops hit the local cache.
- Caveat: each isolate starts `nextHostNetSocketFd` at `0x40000000`, so fds are only unique while
  worker isolates don't create their own sockets (true for the GDBus case). Hardening: offset the base
  per thread id, or key the registry by the creating isolate too. Not required for XU1.

### Part B — sidecar: service a thread's socket op on the owner

In the net.* handlers (`net.write`, `net.socket_read`, `net.shutdown`, `net.destroy`,
`net.socket_wait_connect`, and the `net.poll_wait` readiness wait), when `socket_id` is **not** in the
caller's `tcp_sockets`/`unix_sockets` and the caller `is_thread`, resolve to the owning ancestor via
the `~thread~` chain and operate on **its** socket. For `net.poll_wait`, a thread must wait on the
**owner's** `socket_readiness` (clone the owner's `Arc` for the wait) so the owner's reader thread
wakes it.

## Test + regression plan

1. `scripts/build-gdbus-probe.sh` then the `--bus-test` probe → expect `GDBUS-PROBE: PASS ListNames …`.
2. `SECURE_EXEC_NET_TRACE=1` should show the worker isolate's `send/recv` now succeeding (no `BADF`).
3. **M8 regression (must stay green):** `scripts/test-m5-twm.sh` (twm decorates) and a full LXDE run,
   since this touches the shared net.* / readiness machinery the X path uses.
4. Then build `libxfce4util` + `xfconf` (xfconfd + xfconf-query) on the working GDBus path → XU1.

## Plumbing constraint for Part B (found while scoping)

`service_javascript_sync_rpc` (execution.rs ~13806) receives a **single `&mut process`**, not the VM
or sibling processes. So the net.* handlers cannot reach the owner's `unix_sockets`/`socket_readiness`
as written. Part B therefore needs one of:
- have the **caller** (line ~6569, which holds the VM) resolve the owning ancestor for a thread and
  pass its socket tables + `socket_readiness` into the request (e.g. an optional
  `owner_socket_ctx`), or
- promote host_net socket tables + readiness to a **per-`kernel_pid`** structure (shared `Arc`) that
  every thread's request carries — the cleaner long-term shape, larger blast radius.
This is a dispatch-plumbing change, not a local edit, which is the concrete reason it is staged
separately with the M8 regression gate.

## Why deferred (not implemented in this iteration)

Part B changes the exact per-process socket + readiness machinery the working M8 X spine relies on, and
(above) needs a dispatch-plumbing change to give the handler the owner's socket context; it must land
with the full M8 regression pass, not rushed. The analysis here is the hard part (done +
trace-confirmed); implementation is now mechanical with a known shape.
