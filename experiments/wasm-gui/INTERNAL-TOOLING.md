# Internal debugging/observability tooling

Companion to `SPEC.md` constraint #4. Where SPEC.md says *"when stuck, build a tool that parallels
native debugging,"* this doc records **how we decide what to build**, the **catalog of tools with a
checklist**, and the **short-term plan**.

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
- [ ] **P1 all-isolate stack-dump** (our `gdb thread apply all bt`) — interrupt every session isolate,
      capture wasm frames, dump **all at once** (a livelock needs both sides). Cracks the current bug.
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
