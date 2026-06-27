# T1 SAB ring transport — security design note (cron prerequisite, write BEFORE implementing)

T1 replaces the per-syscall control-channel round-trip (request/response fds + CBOR→JSON→base64 for binary) with a
guest↔kernel SharedArrayBuffer **ring** + `Atomics.wait/notify`. The kernel reads ring data the **guest wrote**.
Per the trust model (CLAUDE.md): the executor is the adversary; **every byte the guest writes into the ring is
HOSTILE input** and must be validated by the sidecar (the TCB). This note defines that validation; it is the
gate before any T1 code lands.

## What exists today (T1 generalizes this, doesn't invent it)
- `AGENT_OS_NODE_SYNC_RPC_*` (javascript.rs:58-82): `REQUEST_FD`, `RESPONSE_FD`, `DATA_BYTES` (default **4 MiB**
  SAB data buffer), `WAIT_TIMEOUT_MS`. Blocking via `Atomics.wait` (wasm.rs:3406).
- A zero-copy SAB **data buffer** + atomics substrate already exists (the M8.6 framebuffer fast path). Large
  binary payloads ride it (memcpy). The gap T1 closes: the **control channel** (method+args, small binary) still
  uses the fd round-trip + `cbor_to_json` base64 (v8_runtime.rs:433). T1 moves the control channel into a ring.

## Ring layout (proposed)
Two SPSC rings in one SAB per guest: **G→K** (guest produces requests) and **K→G** (kernel produces responses).
Each ring: a fixed header + a power-of-two data region.
- Header (kernel-readable, guest-writable for G→K producer fields): `producer_index` (u32, guest-owned for G→K),
  `consumer_index` (u32, kernel-owned for G→K), `doorbell` (u32, atomic), `ring_size` (u32, **kernel-set at setup,
  never re-read from the guest**), `record_count`/`seq` (u32).
- Records: `[len:u32][method_id:u32][payload:len bytes]` written at `producer_index % ring_size`, wrap-aware.

## THE THREAT MODEL — guest-written fields are hostile
The guest controls: `producer_index`, every record's `len`, the payload bytes, the `doorbell`, and `seq`. It may
mutate any of them **concurrently** while the kernel reads (the SAB is shared memory; there is no lock the guest
respects). Attacks to defeat:

1. **Out-of-bounds offset/length → OOB read/write (escape).** A `len` or implied offset that points outside the
   data region would let the kernel read/write kernel or other-guest memory.
   - **Defense:** `ring_size` is kernel-owned (set at setup, stored kernel-side, NEVER re-read from the SAB).
     Every `producer_index` and `len` is range-checked against the kernel's own `ring_size`. `offset + len` uses
     **checked arithmetic** (no integer overflow); a wrapped record's two segments are each bounds-checked.
     Reject (and tear down the guest) on any violation — do not clamp-and-continue silently.

2. **Double-fetch / TOCTOU.** The guest mutates `len` or the payload *after* the kernel validates `len` but
   *before* it copies the payload (classic double-fetch).
   - **Defense:** **copy-out-then-validate-then-use.** The kernel `memcpy`s `len` and the payload OUT of the SAB
     into kernel-private memory in one shot, then validates the COPY and operates only on the copy. **Never read a
     guest-written field from the SAB twice**, and never act on SAB bytes in place. Read `len` once into a local;
     bound it; copy exactly that many bytes; ignore the SAB thereafter for this record.

3. **Producer-index manipulation / replay.** The guest sets `producer_index` backward, forward past what it wrote,
   or to alias old records.
   - **Defense:** the kernel keeps its OWN `consumer_index` kernel-side; it processes records strictly from
     `consumer_index` forward, advancing it itself. It treats the guest `producer_index` only as "there may be
     data up to here", clamped to `consumer_index + ring_size`. Stale/garbage records past real production are
     length-validated like any other (a bogus `len` is rejected, not trusted).

4. **Unbounded length → DoS / OOM.** A `len` near u32::MAX to force a huge kernel alloc.
   - **Defense:** a kernel `MAX_RECORD_BYTES` cap (≤ ring_size); `len > cap` is rejected. No allocation sized by an
     unvalidated guest length.

5. **Doorbell / wait wedging → DoS.** The guest never rings the doorbell (kernel waits forever) or floods it.
   - **Defense:** the kernel's `Atomics.wait`-equivalent is **always bounded** (`WAIT_TIMEOUT_MS`); on timeout it
     re-scans and makes progress on other guests. One shared doorbell word; the kernel scans per-ring `seq` to
     find which ring(s) have data (docs §"Implementation notes"). A guest cannot starve the service loop because
     each guest's ring drain is bounded per turn (fairness; see isolate-pool scheduling risk in the arch doc).

6. **Backpressure / ring full.** Guest overruns the ring or the kernel can't keep up.
   - **Defense:** bounded ring; producer blocks (its own `Atomics.wait` on a not-full condition) when full. The
     kernel signals space via `Atomics.notify`. The kernel never grows a buffer on guest demand.

## Invariant (the one rule)
**The kernel trusts NO guest-written index/length/seq/offset.** For every record: read each control field exactly
once into kernel-private memory, bounds-check against kernel-owned sizes with checked arithmetic, copy the payload
out before use, and tear down the guest on any violation. `ring_size` and `consumer_index` are kernel-owned and
never sourced from the SAB. This keeps T1 a transport change with **no new escape surface** — which is what makes
it "possibly autonomous" (arch doc §"transport fix"), as long as this validation is built in from the first line.

## Out of scope for T1 (stays as-is)
- Cross-VM sharing (never — a machine boundary). Rings are intra-VM, per-guest.
- The thread-safe multiplex (separate, later step; the genuine TCB concurrency review).
- The never-self-approve list (D-Bus-to-host, host-fd, GPU, host-network) — untouched by T1.

## Build order once approved
1. Define the ring header + record format as a shared Rust/JS contract (bridge-contract.json).
2. Kernel-side reader with ALL validation above + a stress/fuzz test feeding hostile indices/lengths/seq.
3. Guest-side writer (the runner) behind a flag; keep the fd path until parity is proven (then remove, per the
   versionless-lockstep rule — no permanent fallback).
4. MEASURE (constraint #4): per-syscall latency + a multi-guest desktop render, before/after.

---

## Integration design (2026-06-26) — phased; the doorbell is NOT needed for the main win

Read the existing native sync-RPC signaling (crates/execution/src/wasm.rs):
- `respond_sync_rpc_success`/`_error` (wasm.rs:433/450 -> javascript.rs) = the response-delivery seam.
- `DeferredSyncRpcResponder` (wasm.rs:446) = sync-RPC responses can already be completed CROSS-THREAD (used for
  off-thread net.poll_wait). So async/threaded completion is already supported.
- `Atomics.wait(syntheticWaitArray, 0, 0, waitMs)` (wasm.rs:4244) = the guest's poll-loop SLEEP, not a real
  cross-thread doorbell. Nothing `Atomics.notify`s it; it just times out and re-polls.
- The kernel-forwarded RPCs are the ones where `handle_internal_wasm_sync_rpc_request` returns `Ok(false)`
  (wasm.rs:980) and the sidecar services them. T1 targets exactly these (framebuffer/X-socket/DNS/perm).

### Phase 1 (the main win, no new doorbell): swap the PAYLOAD transport
Keep the existing request-dispatch + `respond_sync_rpc_success` signaling. Change only WHERE the bytes ride:
- Guest request: instead of marshalling args+binary through cbor->json->**base64** (v8_runtime.rs:433), the guest
  writes the request record into the G->K ring (JS `RingWriter`, t1-ring/sab-ring.mjs).
- Kernel: read it from the G->K ring (Rust `SabRingReader`, validated/hostile), service it, write the response
  record into the K->G ring (Rust `SabRingWriter`); call `respond_sync_rpc_success` to signal as today.
- Guest response: read the record from the K->G ring (JS `RingReader`) instead of decoding base64.
This eliminates the base64 hop for binary payloads (the Root-3 cost on the kernel-forwarded framebuffer/X-socket
traffic) while reusing all existing control flow. Lower-risk; ships the throughput win first.

### Phase 2 (optional, only if measured): the real Atomics doorbell
Replace the synthetic-wait poll + event dispatch with a real `Atomics.notify` on the header doorbell word so the
servicing thread wakes directly (skips the poll-loop turn) for the full sub-us per-call latency. This is the
architecture-doc T1 vision; do it only if Phase-1 measurement shows the event-dispatch turn dominates.

### Allocation
The rings are per-guest, allocated alongside the existing `AGENT_OS_NODE_SYNC_RPC_DATA_BYTES` SAB (default 4 MiB);
the ring header + two ring regions can live in that same SAB. `ring_size` is kernel-chosen (kernel-owned, never
from the guest), satisfying the security model.

### Next concrete step
Wire Phase 1 at the kernel-forwarded seam: where the sidecar currently decodes the base64 sync-RPC payload, read
from the G->K ring via `SabRingReader`; where it encodes the response, write via `SabRingWriter`. Measure
framebuffer-blit + X-socket round-trip cost before/after.

### Precise Phase-1 wiring seam (located in code)
The Rust host already gets raw byte access to a V8 (Shared)ArrayBuffer's backing store (crates/v8-runtime/src/
bridge.rs:554-563 uses exactly this for CBOR Bytes today):
```
let bs = ab.get_backing_store();
let ptr = bs.data().unwrap().as_ptr() as *mut u8;
let slice = unsafe { std::slice::from_raw_parts_mut(ptr, len) }; // &mut [u8] over the (S)AB
```
`SabRingReader`/`SabRingWriter` take exactly `&[u8]`/`&mut [u8]`, so they plug directly onto that slice. Wiring:
1. At runtime setup, allocate a per-guest ring SAB (a SharedArrayBuffer; `ring_size` KERNEL-chosen) alongside the
   existing `AGENT_OS_NODE_SYNC_RPC_DATA_BYTES` SAB, and expose it to the runner as a global so the guest JS
   `RingWriter`/`RingReader` (t1-ring/sab-ring.mjs) operate on a `Uint8Array` view of the same bytes.
2. At the kernel-forwarded sync-RPC servicing point, take the ring SAB's backing-store slice (above), make
   `SabRingReader::new(ring_size)` (kernel-owned size), `read_record` the request (already validated as HOSTILE),
   service it, `SabRingWriter::write_record` the response onto the same backing store, then `publish_consumer`.
3. The guest's existing synthetic-wait poll loop re-checks after the host responds — no new doorbell (Phase 2 only).

Security: `get_backing_store` yields a raw slice, but `SabRingReader` already treats every guest-written field as
hostile and never reads/writes outside `ring_size`, so a guest scribbling the shared SAB cannot push the host OOB.
The only new `unsafe` is `from_raw_parts_mut` over the V8-owned backing store (valid for the SAB lifetime+len) —
the same pattern the existing bridge already uses (bridge.rs:557-562). This keeps T1 "no new escape surface".

### Ring vs bulk dataBuffer — the size-split (2026-06-26)
The control ring carries records up to `ring_size - 5` bytes (ring_size - 1 reserved - 4 len prefix). The
kernel-forwarded RPCs the ring targets (X-socket writes, DNS, permission checks, small/medium fs) fit comfortably
in a few-KiB ring. LARGER payloads (the framebuffer blit ~1.2 MiB, big X11 requests) do NOT go on the ring — they
keep using the existing bulk dataBuffer SAB (the proven M8.6 memcpy path). RingChannel.rpc now REJECTS an
over-capacity request upfront with a clear error (instead of spin-stalling forever), so the caller routes large
payloads to the bulk path. Validated: 2000-RPC realistic-load test (4 KiB ring, payloads 1B..ring-cap, tight
wraps) + an over-capacity rejection test, both green (experiments/wasm-gui/t1-ring/sab-ring-rpc.test.mjs).

## End-to-end wiring plan (2026-06-26) — handle-flow + servicing point pinned

Servicing chain (mapped): guest emits `WasmExecutionEvent::SyncRpcRequest` (wasm.rs) -> mapped to
`ActiveExecutionEvent::JavascriptSyncRpcRequest` (execution.rs:2682) -> serviced by the per-execution sync-RPC
path in crates/sidecar/src/execution.rs (the request structs at execution.rs:916/937/969/981; service entry
service_javascript_sync_rpc), NOT the top-level service.rs handle_javascript_sync_rpc_request. THIS resolves the
earlier 0-lines traces: they were placed in service.rs; the embedded WASM kernel-forwarded RPCs go through
execution.rs. Place the T1 wiring + any Root-2 trace HERE.

Handle-flow (resolved): the ring SAB lives in the V8 isolate (v8-runtime); the servicing runs in the sidecar
(execution.rs). A v8 `SharedRef<BackingStore>` is `Send`, so:
1. v8-runtime, at guest setup (session.rs context setup, ~line 916): create a per-guest ring SAB
   (`v8::SharedArrayBuffer`), expose it to the guest as a global (e.g. `__agentOsT1Ring`), and capture its
   backing-store `SharedRef` (ptr+len).
2. Hand that `SharedRef` (ptr/len) to the sidecar execution state once (via the execution handle / a setup event),
   gated behind `SECURE_EXEC_T1_RING`.
3. sidecar execution.rs, at the kernel-forwarded sync-RPC servicing: instead of decoding the base64 request, call
   `SabRingEndpoint::service_from_raw(req_ptr, req_len, resp_ptr, resp_len, |req| service_kernel_rpc(req))` using
   the held backing-store ptr/len. `read_request` validates every hostile field; the response is written to the
   ring; the guest's existing synthetic-wait poll picks it up (Phase 1, no doorbell).
4. guest runner (wasm.rs embedded JS): when `__agentOsT1Ring` is present, route the sync-RPC through the inlined
   RingChannel (t1-ring/sab-ring.mjs logic) instead of the base64 path; else fall back (flag off).
5. MEASURE: per-RPC servicing time at execution.rs before/after, + a multi-guest desktop render.

Foundation status: data + protocol + host servicing (`service_from_raw`) all BUILT + TESTED (16/16 Rust, JS e2e +
2000-RPC realistic load). The above 5 steps are the remaining mechanical wiring, now pinned to file:line.

### Correction (2026-06-26): the sidecar crate is `#![forbid(unsafe_code)]`
In-crate compilation (edition 2024) revealed crates/sidecar/src/lib.rs has `#![forbid(unsafe_code)]` (the
edition-2021 standalone tests masked it). So the unsafe raw-pointer servicing (`service_from_raw`/`drain_all`,
which call `slice::from_raw_parts_mut` over the V8 backing store) CANNOT live in the sidecar. Removed them. The
sidecar's sab_ring is now purely SAFE (slice-based: SabRingReader/Writer/SabRingEndpoint with read_request/
write_response/publish_consumer over `&mut [u8]`) -- 15/15 in-crate tests pass. The unsafe `from_raw_parts_mut`
boundary belongs in the v8-runtime crate (which already does `get_backing_store()` + `from_raw_parts` and allows
unsafe). Revised wiring: v8-runtime gets the ring SAB's backing-store slice (unsafe, there) and calls the SAFE
sidecar `SabRingEndpoint::read_request`/`write_response`. This is cleaner (the unsafe stays in the one crate that
owns raw V8 memory; the sidecar TCB logic stays unsafe-free).

## Handoff design (2026-06-27) — the precise cross-crate wiring, mapped

Flow today: guest emits sync-RPC -> ActiveExecutionEvent::JavascriptSyncRpcRequest (base64 payload) consumed by the
sidecar servicing loop (crates/sidecar/src/execution.rs:17013+) -> service_javascript_sync_rpc -> response back via
wasm.rs:433 respond_sync_rpc_success (base64). The v8-runtime session loop (crates/v8-runtime/src/session.rs) runs
the guest + emits these events; the sidecar execution.rs is the driver that services them over the
JavascriptExecution handle.

T1 wiring (Phase 1, no doorbell):
1. v8-runtime session setup (session.rs, after inject_globals ~:944): when SECURE_EXEC_T1_RING is set, call
   allocate_t1_ring_sab(scope, "__secure_exec_t1_req", REQ_BYTES) + a second for "__secure_exec_t1_resp". Hold the
   two backing-store SharedRefs (Send) in the session state.
2. Handoff: send the two SharedRefs to the sidecar ONCE at execution start -- add an ActiveExecutionEvent::
   T1RingReady { req: SharedRef<BackingStore>, resp: SharedRef<BackingStore> } (SharedRef is Send) OR a field on the
   execution-start handshake. The sidecar stores them in the per-execution state alongside `process`.
3. Sidecar servicing loop (execution.rs:17013): when the T1 rings are present, BEFORE/INSTEAD OF the base64 path,
   call SabRingEndpoint::drain_all(req_ptr,req_len, resp_ptr,resp_len, |req_bytes| service_one(req_bytes)) using the
   stored backing stores' data() ptr/len -- the unsafe from_raw_parts lives in a v8-runtime helper (sidecar is
   forbid-unsafe), so expose a `fn service_ring(req_bs, resp_bs, service_fn)` in v8-runtime that the sidecar calls.
4. Guest runner (wasm.rs embedded JS): when globalThis.__secure_exec_t1_req exists, route sync-RPCs through
   makeSyncRpcRouter(RingChannel(...)) instead of the base64 path; the serviceHost spin reuses the existing
   synthetic-wait poll (no new doorbell).
5. MEASURE: re-run SECURE_EXEC_TRACE=1; compare the ~70k poll round-trip wall-time before/after.

Key constraint: the unsafe slice construction (from_raw_parts over the backing store) stays in v8-runtime
(allocate_t1_ring_sab is already there); the sidecar only ever sees safe &[u8]/&mut [u8] via a v8-runtime shim that
wraps SabRingEndpoint. This keeps the sidecar #![forbid(unsafe_code)] intact.
