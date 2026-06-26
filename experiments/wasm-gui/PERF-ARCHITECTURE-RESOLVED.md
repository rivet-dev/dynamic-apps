
## WASM RPC routing is SPLIT (2026-06-26) — resolves the measurement discrepancy; bounds T1's reach
Read `handle_internal_wasm_sync_rpc_request` (crates/execution/src/wasm.rs:933-991). The WASM guest's sync RPCs
take one of two paths, decided per-call:
- **Executor-internal (host fs, per-isolate thread, fast):** module-resolution RPCs
  (`try_service_standalone_module_sync_rpc`), module-file reads, and `fs.openSync`/fs ops whose guest path
  `translate_wasm_guest_path(...)` maps to a **WASI preopen host path** (the sandbox root / mounted dirs). These are
  serviced directly in the executor against the real host fs under the sandbox root — they NEVER emit a
  SyncRpcRequest event and NEVER reach the sidecar handler.
- **Kernel-forwarded (sidecar, single service thread, serialized = Root 2):** when `translate_wasm_guest_path`
  returns `None` (line 980, `Ok(false)` = "not handled internally"), the RPC falls through to the sidecar/kernel
  (kernel VFS, socket table, synthetic /proc, /dev, etc.).

This resolves why my 4 instrumentation attempts saw 0 sidecar-handler hits during the desktop session: the
desktop's preopen-mapped fs traffic is executor-internal and bypasses the sidecar handler entirely.

### T1 implication (important)
T1 (the SAB ring transport) replaces the **kernel-forwarded sync-RPC** path's base64+round-trip. It does NOT touch
the executor-internal host-fs path (already direct host I/O, fast). So **T1's desktop impact = however much of the
hot path is kernel-forwarded** (kernel VFS like the framebuffer device, the X11 socket via the kernel socket table,
DNS, permission checks) vs preopen-mapped host fs. The framebuffer (#2, a kernel VFS MemoryFileSystem path) and the
X11 socket (kernel socket table) are kernel-forwarded, so T1 + brokered shared segments (#3) target exactly them —
which is consistent with the doc's "socket/X11 traffic" and "framebuffer blit" rows being the T1/segment wins.

### Open question (focused-session, needs runtime tracing — do NOT fragment-thrash it)
Exactly which fraction of the live multi-app desktop's hot RPCs are kernel-forwarded vs executor-internal, and the
per-call cost of each, requires tracing the running session at BOTH `wasm.rs:933` (internal) and the
kernel-forwarded handler. That is the focused-session measurement (the architecture is now understood enough to
place both probes correctly); fragment-guessing a single probe is what thrashed before.
