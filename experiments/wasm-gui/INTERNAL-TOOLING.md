# Internal debugging/observability tooling

Companion to `SPEC.md` constraint #4. Where SPEC.md says *"when stuck, build a tool that parallels
native debugging,"* this doc records **how we decide what to build**, the **catalog of tools with a
checklist**, and the **short-term plan**.

> **Public mirror — keep in sync.** The user-facing catalog of the SHIPPED tools (each mapped to its
> Linux parallel) lives at `website/src/content/docs/docs/debugging-tools.mdx` ("Linux Debugging
> Tools", in the docs sidebar under **Debugging**). When a tool here flips to `[x]`/`[~]` (or its env
> var / Linux analog changes), update that MDX page in the same change. This doc is the source of
> truth for the internal catalog + rationale; the MDX page is the trimmed, user-actionable view (set
> the flag, read the output, the Linux tool it stands in for) and must not narrate runtime internals.

## 1. Why this exists

On this project the bottleneck is almost never the bug's difficulty, it is **observability**. A guest
runs as wasm inside a V8 isolate inside the sidecar, with none of the native toolkit. The M8
`gtk_init` hang is the case study: two wrong theories (X-server starvation, then a futex deadlock)
each survived for hours, and were only killed in minutes once we had a sync-RPC trace + `/proc` thread
states. The lesson: **invest in reusable observability, not one-off `fprintf`-and-rebuild bisection.**

## 2. Constraints & principles (NON-NEGOTIABLE)

1. **When blocked, name the native technique first.** Ask "what would I reach for on a native host?"
   (`gdb bt`, `gdb thread apply all bt`, `strace -f`, `/proc/<pid>/wchan`, `perf`/flamegraph,
   `helgrind`/`mutrace`, `xtrace`, `GDK_DEBUG`/`GDK_SYNCHRONIZE`). That names the *capability* you need.
2. **The CLI is veneer; the capability is the value.** `ps`/`top`/`lsof` are thin formatters over
   `/proc`; `gdb`/`strace` are valuable for their *mechanism* (unwind a stopped stack, intercept
   syscalls), not their REPL. We never want the CLI for debugging.
3. **We debug from the host, so expose to the host.** Output goes to host stdout / a structured dump /
   an on-hang bundle, never to an in-VM CLI we then scrape.
4. **wasm guests have no native CPU.** No register file, no machine instructions, no `INT3`. So the
   `ptrace`/register/single-step class (`gdb`/`strace`/`ltrace`/`perf`/`valgrind`/`rr`) **cannot run as
   the real tool against a guest** — those capabilities must be **built wasm-aware into the runtime**.
   `PEEK/POKE`→linear memory and `PTRACE_SYSCALL`→the kernel's syscall mediation are the only faithful
   mappings.
5. **Port only to reuse non-trivial parsing/symbolization, host-side.** Don't reinvent DWARF or the
   X11 protocol. Do NOT port a CLI to run in-VM for debugging. (Making real `ps`/`top`/`strace` run
   in-VM is a separate *product-conformance* goal, driven by guest workloads, not by us debugging.)
6. **Off by default, zero-cost when off.** Every tool is gated by an env var / flag; no overhead in
   normal runs. Pure instrumentation, no behavior change (so it carries no security-review burden,
   unlike a runtime *fix*).
7. **Prefer the real tool when it genuinely reaches the layer.** The native toolchain DOES apply to
   the *sidecar* (it is a native process): `/proc`, and (if installed) `gdb`/`strace`/`perf` — with V8
   `--gdbjit`/`--perf-prof` even reaching guest frames. Try that before building a parallel; build the
   parallel only when the real tool can't reach guest semantics.

## 3. Decision framework

```
blocked on a hang / crash / wrong behavior
        |
  name the native tool you'd use
        |
  does the REAL tool reach this layer?
   |                         |
  yes (sidecar/native)      no (guest semantics)
   |                         |
  use it (install/flag)     is the capability a thin read of data we own?
                             |                              |
                            yes -> EXPOSE it natively       no (needs unwind/intercept/instrument)
                            (host dump, no CLI)             -> ANALYZE: build wasm-aware introspection
                                                              (reuse parsing libs host-side = PORT)
```

## 4. Catalog + checklist

Priority: **P1** cracks current work / unblocks others; **P2** broad reuse; **P3** nice-to-have.
Status: `[x]` done, `[~]` partial, `[ ]` todo.

### 4a. EXPOSE — surface state we already own (native API / host dump; cheap)
- [x] **P1 sync-RPC trace** (`SECURE_EXEC_TRACE`) — guest↔sidecar call stream per process, timed. The
      `strace`-capability. (`crates/sidecar/src/execution.rs` `service_javascript_sync_rpc`.)
- [x] **P2 socket I/O trace** (`SECURE_EXEC_NET_TRACE`) — per-isolate `net_send`/`net_recv`/`net_poll`
      with fd + byte counts + result (`NETTRACE …`). The `strace -e network`/`tcpdump` capability for the
      kernel socket table. (`crates/execution/src/node_import_cache.rs` `netTrace`.) Caveat: it traces
      the CALLING isolate, so it surfaces the per-isolate-socket-table gap directly (a worker-thread
      isolate has an empty `hostNetSockets` → `net_send` misses with EBADF before any sidecar call).
- [x] **P1 wake-cause profiler** (`SECURE_EXEC_WAKEPROF`) — per-process histogram of every
      `net.poll_wait` completion by CAUSE (immediate / pre-advanced / direct-notify / pool-notify /
      pool-DEADLINE / inline-notify/deadline), keyed by readiness ptr + guest name (pid + entrypoint).
      A bimodal split (deadline-dominated vs notify-dominated vs immediate-spin) discriminates lost-wake
      from latency from busy-spin. Sidecar-side, so it sees ALL guests (unlike NET_TRACE which traces
      the calling isolate). (`crates/sidecar/src/state.rs` `wakeprof_record`.) Flag/log only — never
      completes a wait (honors the CLAUDE.md "wakeups are event-driven, never timer-polled" constraint).
- [~] **P2 poll fd-state in NET_TRACE** — `net_poll`'s `poll` trace line now also logs, per polled fd,
      `:cl=`(closed) `:ch=`(readChunks len) and a `:srv`/`:pipe`/`:nosock`/`:nosid` tag, plus a `spin0`
      line for zero-wait polls that found nothing ready. Partial coverage of the fd-table-dump below; it
      surfaced the xfconfd undrained-kernel-pipe spin (a pipe stuck POLLIN-readable). (`node_import_cache.rs` `netTrace`.)
- [ ] **P1 process/scheduler view** — `ActiveProcess` table: `kernel_pid`→label (`xclient0`/`server`),
      per-process **run-state** (requires runtime→kernel plumbing of isolate state — the bit that
      reveals a spin), last-pumped, blocked-on. Replaces `ps`/`top`.
- [ ] **P2 socket/fd table dump** — per-process open fds, socket states, **buffered byte counts**
      (readChunks), listeners/pending accepts. Replaces `lsof` + "did the reply land in the buffer."
- [ ] **P2 RPC trace ring buffer** — keep last N calls in memory always-on; dump on hang/demand.
      Kills the firehose + the "forgot to enable tracing" problem.
- [ ] **P3 pump/scheduler decision log** — per cycle: which process advanced / why it yielded.
- [ ] **P3 resource counters** — fuel, memory, live-threads-vs-cap.

### 4b. ANALYZE — wasm-aware introspection we build (our gdb/helgrind/perf)
- [x] **P1 all-isolate stack-dump + per-thread verdict** (our `gdb thread apply all bt`,
      `SECURE_EXEC_STACKDUMP_AFTER_MS`). Watchdog interrupts every registered isolate and prints, per
      thread, a one-line **classification** (`[stackdump] <label> => …`) followed by the **native**
      backtrace (`crates/v8-runtime/src/isolate.rs`, `classify_stack`). The verdict is the
      **deadlock-vs-livelock** call that was the hard part of M8.0: `PARKED-ON-FUTEX` (V8
      `FutexEmulation`/`Runtime_Wasm*AtomicWait` = a guest `pthread_mutex`/`cond`/raw `atomic.wait` —
      deadlock candidate), `BLOCKED-IN-HOST` (sidecar/kernel wait, e.g. `net.poll`/`recv`), or `RUNNING`
      (JIT'd wasm / V8 builtin — livelock/CPU-spin candidate if it stays across rounds). Proven both ways
      (`~/tmp/gui-progress/proof-m8.1-stackdump-verdict.txt`): `threads-atomicwait` → PARKED, healthy
      `gtk-hello` mid-init → RUNNING. LIMIT: still no wasm function NAMES (rusty_v8 forbids a HandleScope
      inside an interrupt → V8's wasm frame walk is unreachable → native frames bottom out at the JIT
      CEntry trampoline; naming needs the DWARF symbolizer below) and no futex ADDRESS / lock-owner
      (those live in wasm linear memory — needs the threaded-libc futex trace, a follow-up).
- [~] **P1 V8 `--prof` wasm sampler** (`SECURE_EXEC_V8PROF=1` → `/tmp/secure-exec-v8.log`;
      `scripts/v8prof-top.py` symbolizes; `scripts/diag.sh v8prof <guest>`). Works: profiles all
      isolates, names JS frames, surfaces the hot wasm call chain. On the M8 hang it showed a sustained
      wasm busy-spin in fpcast-emu thunks (indirect fn-ptr calls) with no `net.poll` = GLib dispatching
      an always-ready GSource without polling. LIMIT: V8 logs wasm as `wasm-function[N]`, and the only
      build that keeps a name section (`SECURE_EXEC_KEEP_NAMES=1`) names just the `byn$fpcast-emu$N`
      thunks — `--fpcast-emu` (required for GTK's cross-signature fn-ptr casts) erases the C names. So
      the exact GSource is not named via `--prof`.
- [ ] **P1 DWARF line-symbolizer off the pre-fpcast binary** — map `wasm-function[N]`/byte-offset to
      `func:file:line` using the DWARF in the guest `.wasm` *before* the fpcast-emu pass (reuse
      `llvm-symbolizer`/`gimli`, host-side). The way around the fpcast-emu name wall. ≈ `addr2line`.
- [ ] **P1 lock/contention tracer** (our `helgrind`/`mutrace`) — instrument the wasm pthread
      mutex/cond / futex `atomic.wait`/`notify`: log acquire/release/contention per (thread, lock-addr),
      flag lock-order inversions. Names the two locks in the current livelock.
- [ ] **P2 on-hang diagnostic bundle** (meta-tool) — watchdog timeout auto-dumps all-isolate stacks +
      process/scheduler view + socket/fd table + RPC ring buffer. One flag → full picture on any wedge.
- [ ] **P2 wasm sampling profiler** (our `perf`/flamegraph) — periodic isolate-interrupt sampling →
      folded stacks → flamegraph. Finds busy-spin hotspots.
- [ ] **P3 deadlock detector** — wait-for graph from the lock tracer; auto-detect cycles.
- [ ] **P3 guest snapshot** (our `gcore`) — dump an isolate's linear memory + module for offline poking.

### 4c. PORT/REUSE — existing logic we shouldn't reinvent (host-side helpers)
- [ ] **P1 DWARF symbolizer** (reuse `llvm-symbolizer`/`addr2line`/`gimli`) — wasm PC/offset →
      `func:file:line` from the guest `.wasm` DWARF. Pairs 1:1 with the stack-dump (useless without it).
- [ ] **P2 X11 protocol decoder** (reuse xtrace tables / libxcb XML / Wireshark dissector) — decode the
      X wire-tap bytes into request/reply/error names.
- [x] **P3 wasm inspectors** (`wasm-objdump`/`wasm-tools`/`wasm2wat`) — in use for module ABI checks.
- [ ] **(not for debugging) real `ps`/`top`/`strace` in-VM** — product-conformance only; defer until a
      guest workload requires it.

## 5. Short-term plan (build order)

For the current M8 bug first, maximizing downstream reuse:
1. **all-isolate stack-dump (4b)** + **DWARF symbolizer (4c)** → `gdb bt` for both spinning threads.
   *This is the immediate goal — it pinpoints the livelock.*
2. **lock/contention tracer (4b)** → names the exact two locks if the stacks alone aren't conclusive.
3. **process/scheduler view (4a)** → cheap, replaces `ps`/`top`, reusable on every future hang.
4. **on-hang bundle (4b)** → wraps 1+3+RPC-trace so the next wedge self-reports.
5. as needed: socket/fd table (4a), sampling profiler (4b), X wire tap + decoder (4a/4c).

## 6. Status log
- 2026-06-21: doc created. sync-RPC trace shipped; it refuted two M8 theories and (with `/proc` thread
  states) localized the `gtk_init` hang to a busy-spin livelock between the GTK leader and a GLib
  worker thread. Next: all-isolate stack-dump + DWARF symbolizer.
- 2026-06-24: added `SECURE_EXEC_NET_TRACE` (socket I/O trace, 4a) while debugging XU1 GDBus. Published
  the user-facing catalog at `website/src/content/docs/docs/debugging-tools.mdx` ("Linux Debugging
  Tools", sidebar → Debugging) mirroring the shipped tools to their Linux parallels; keep it in sync
  with this doc (see the header note).
