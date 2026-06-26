# Generic performance findings — ultracode analysis (2026-06-26)

Multi-agent analysis (45 agents: 6 dimensions → adversarial verify → synthesis) over the main repo. The verify
pass already downgraded hype and rejected over-scoped variants. Full machine output:
`tasks/wxm6t87o5.output`.

## ★ Roadmap-changing correction: postMessage→SAB is BROWSER-ONLY

The B roadmap's **T1 ("replace postMessage+base64 syscall RPC with a SAB ring")** was premised on postMessage
being on the hot path. The analysis shows it largely **is not**, for the path that matters:

- **postMessage is ONLY on the BROWSER executor path** (`packages/browser/src/worker.ts:805` sends sync-syscall
  ARGS via postMessage; the RESPONSE already returns via the SAB `dataBuffer` + Atomics).
- **The native production path — what the wasm-gui desktop runs on — is fully IN-PROCESS** (the V8 isolate and
  the Rust kernel share one address space). **There is NO postMessage and NO base64 on the native hot syscall
  path.** So postMessage→SAB **does not apply to native at all.**
- Even in the browser, moving the request into SAB removes only the args structured-clone, **NOT the event-loop
  hop** (the main thread cannot `Atomics.wait`), so "removes postMessage entirely" is false.

**Implication:** T1 is **demoted** for the native desktop goal. The native syscall path still has serialization
waste, but it's clones/base64-round-trips INSIDE the in-process bridge (findings below), not postMessage. The
real near-term native wins are the autonomous findings here; the big structural wins remain **Root-1**
(per-guest compute) and **Root-2** (parallelism).

## Top 3 (best ROI/effort, all autonomous)

1. **Per-arm OS thread → one per-session timer thread** (`javascript.rs:3065`, also `python.rs:1265`).
   `arm_kernel_timer` does `thread::spawn`+sleep per timer arm; `setInterval(f,0)` re-spawns ~1000 threads/sec
   and can hit EAGAIN. Replace with a libuv-style single timer thread (BinaryHeap deadline + condvar). Removes a
   real thread-exhaustion crash class. Autonomous (executor-side).
2. **In-place `MemoryFileSystem::pwrite`** (`vfs.rs:335/910`, `kernel.rs:2300`). Default pwrite does
   `read_file` (clones the WHOLE file) + copy + write_file. Override to mutate `InodeKind::File{data}` in place.
   Kills an O(filesize) clone per dirty write — directly the **Xvfb framebuffer rewrite path** (the read/clone
   side of the M8.6 loop). Autonomous (kernel VFS; bounds already enforced). FIRST verify no overlay/root
   wrapper shadows pwrite.
3. **TextEncoder/TextDecoder ASCII fast path** (`v8-bridge.source.js ~2203/2410/2243`). Drop the boxed-array
   intermediate; fix `encodeInto`'s per-char realloc; add all-<128 scan. Broad across socket/HTTP/Buffer-utf8.
   Autonomous (pure in-isolate JS, falls back for non-ASCII). Bare V8 ships no native impl.

## Other verified autonomous levers (ranked)

4. **1ms idle busy-poll loop** (`session.rs:1747`): cache the `_getPendingTimerCount` Function handle + keep
   pending counts in Rust counters so exit checks never call into JS. (REJECT removing the 1ms `pump_v8_message_loop`
   tick — it is load-bearing for async WASM settling.)
5. **Triple format hop CBOR→serde_json::Value→CBOR** (`javascript.rs:1073`): consume the owned arg Vec by value,
   special-case only `writeFile` string→bytes; skip the per-syscall arg-tree clone.
6. **Single-iov fast path in WASI iov** (`wasm.rs:2478`): `subarray` (view) + `iovsLen===1` short-circuit drops
   2 of 3 JS-side payload copies — hot for the framebuffer pwrite loop. (Keep the `_boundedIovLength` cap.)
7. **Batch `child_process.poll`** (`v8-bridge.source.js:8888`, server already buffers in a VecDeque at
   `execution.rs:6066`): return all queued chunks+exit in one RPC (~64x fewer RPCs for chunk-heavy children).
8. **nextTick drain O(n²)→O(n)** (`v8-bridge.source.js:24694`): read-index pointer instead of `Array.shift()`.
   (Use read-index, NOT the snapshot-swap variant — it breaks re-entrant nextTick ordering.)
9. **base64**: redundant base64 round-trip in `cbor_to_json` on the in-process BridgeCall path
   (`v8_runtime.rs:433`, distinct from the M8.6 path); and swap the hand-rolled char-by-char base64
   (`v8_runtime.rs:504`) for the `base64` crate already in deps.
10. **Build-time embedded bridge snapshot** (`snapshot.rs:41`): generate the canonical V8 snapshot at build time
    (`include_bytes!`) instead of compiling+serializing on first guest Execute per process. Cuts cold-start.

## Relation to the B roadmap

- **Root-1 (fpcast arity tuning + per-signature marshallers)** — the ~13.4s GObject CSS cascade is fpcast-emu uniform-arity padding (NOT the call opcode; typed-func-refs was disproven — see the Root-1 CORRECTION below). Biggest single-guest win. KEEP, start here.
  under `--fpcast-emu`. Biggest single-guest win. KEEP, start here.
- **Root-2 (thread-safe kernel + multiplex)** — the parallelism wall. KEEP.
- **T1 (postMessage→SAB)** — DEMOTED for native (see above). The serialization wins on native are #5/#6/#9.
- **Brokered shared segments / MIT-SHM** — still valid for the browser path + cross-guest IPC; lower priority
  for native single-VM than the autonomous clone-elimination wins.

## Refined sequencing (informed by the analysis)

Grab the cheap autonomous framebuffer/throughput wins (#2 pwrite, #6 iov, #1 timer thread) alongside the big
**Root-1** investigation; they're quick, measurable, and hit the desktop hot path — then **Root-2** for the
multi-guest parallelism. T1's SAB-ring work is reframed as in-process clone/base64 elimination (#5/#9), not a
postMessage replacement, and only the browser executor benefits from a request-side ring.

## ★★ Root-1 CORRECTION (2026-06-26): typed-function-references is the WRONG fix

A deep feasibility agent (read the toolchain + V8 runtime) found the named approach does not work:
- **`call_ref` type-checks exactly like `call_indirect`** — a GObject cast mismatch still traps. Typed
  function references / wasm-GC do NOT remove the mismatched-signature cost; they just move it. There is no wasm
  typing feature that lets one call site dispatch to genuinely-different runtime signatures without per-signature
  adaptation (true in MVP and GC).
- **The real cost is fpcast-emu's UNIFORM max-arity padding + per-call trampoline**, not the call opcode.
  binaryen's `FuncCastEmulation` pads EVERY indirect-callable function + EVERY call site to one wide uniform
  signature (all params → i64, fixed arity), so every GObject closure/marshal/vfunc call shuffles the full padded
  arity. Cost ≈ (#indirect-calls) × (arity coercions + extra dispatch). GObject is almost all indirect calls.
- **V8-here CAN run GC-wasm** (V8 130, GC + typed-func-refs default-on, no flags set — `isolate.rs:102-109`), so
  the approach is not blocked by the runtime; it's just aimed at the wrong layer. NOT the fix.
- **Correction to the prior framing:** there is NO `max-func-params@128` tuning anywhere in-repo. `link-xapp.sh:24`
  uses BARE `--fpcast-emu` → binaryen's DEFAULT arity. So we're paying the default padding on every call.

### The ACTUAL Root-1 fix (cheapest-first)
1. **QUICK WIN (hours, one line):** `--fpcast-emu --pass-arg=max-func-params@<measured-true-max>` at
   `link-xapp.sh:24`. The padding cost scales with the fixed arity; GObject closures are mostly 1–4 pointer args,
   so lowering from binaryen's default to the measured true max should remove a large fraction of the ~12s.
   **Risk:** a genuinely-indirect function with more params than N breaks → MEASURE the true max, keep an
   app-still-runs assertion. Highest value-per-effort; do FIRST.
2. **ROOT CAUSE (days):** kill the generic-marshaller path. The UB-cast traffic is GObject's *generic* marshaller
   (`g_cclosure_marshal_generic`, libffi-style arbitrary-signature). Force per-signature C marshallers
   (`g_cclosure_marshal_VOID__*`) for the signals `show_all` exercises → correctly-typed `call_indirect`, no
   emulation → drop/scope `--fpcast-emu` narrowly. Biggest structural win.
3. (weeks) per-arity trampoline tables (custom binaryen). Only if 1+2 insufficient.

So Root-1's roadmap entry becomes "fpcast arity tuning + per-signature marshallers", NOT typed-function-references.
