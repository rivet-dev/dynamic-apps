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
