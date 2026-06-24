# Spec: Virtual memory & `mmap` emulation — the page-cache the MMU gives Linux for free

Living spec (**DRAFT v1**, 2026-06-24). Status legend: ⬜ todo · 🟡 in progress · ✅ done · ❌ blocked.

**Relationship to the other specs (read these first):**
- [`SPEC.md`](./SPEC.md) — the DE spec. Constraint #5 ("components build from UNMODIFIED upstream;
  fixes live in the platform layer") is the rule this spec serves: the framebuffer-export path here is
  the canonical example of repaying a per-component patch into the platform memory layer.
- [`XUBUNTU-SPEC.md`](./XUBUNTU-SPEC.md) — the successor target. Its components map host-backed files
  with `mmap(MAP_SHARED)` (framebuffer, caches), so the model here is load-bearing for it.
- [`INTERNAL-TOOLING.md`](./INTERNAL-TOOLING.md) — the observability tools (named-Xvfb symbolizer,
  sync-RPC tracer, lock/lockdep watchdog, `rpcprof`) that found the throughput spin this spec fixes.
- [`WASM-THREADS-SPEC.md`](./WASM-THREADS-SPEC.md) — the threading substrate; relevant to the
  out-of-scope inter-process `MAP_SHARED` discussion in §6.

Code: `toolchain/wasi-compat.c` (`__wrap_mmap`/`__wrap_munmap`/`msync`), `scripts/link-xvfb.sh`
(`-Wl,--wrap=mmap,--wrap=munmap`), and the runner delta encoder in
`crates/execution/src/node_import_cache.rs` (`[fb-delta]` in the guest `fd_pwrite` path).
Proof: `~/tmp/gui-progress/2026-06-24T15/proof-fbexport-stock-mmap-lxde.png` (stock Xvfb mmap path
renders green identical).

---

## 1. North star

Present **normal Linux virtual-memory semantics** to unmodified guest binaries — specifically,
file-backed `mmap(MAP_SHARED)` with write-back — well enough that a stock X server, GTK, fontconfig,
and freetype run untouched, **without** a real MMU, page tables, page faults, or a kernel page cache
underneath. The platform layer *is* the page cache.

This is a SPEC.md constraint-#5 statement: the memory model is the work, the unmodified components are
the test.

---

## 2. What native Linux gives us for free (and where it lives)

The thing we depend on does **not** live in X, in GTK, or in any library. It lives in the **kernel
virtual-memory subsystem** — the MMU + the page-fault handler + the page cache, working invisibly
beneath an oblivious program.

The **MMU** (Memory Management Unit) is CPU hardware between virtual addresses and physical RAM:

- **Translation** — virtual → physical via page tables; every process gets its own flat address
  space (4 KB page granularity).
- **Protection** — per-page R/W/X bits; violations trap.
- **Faults** — an unmapped/protected access traps into the kernel. This is the magic primitive:
  demand paging, copy-on-write, lazy `mmap` fault-in, and swap are all "the kernel handles a fault."
- **Dirty/accessed bits** — hardware flips a per-page dirty bit on write. The page-cache writeback
  flusher reads those bits to write back **only changed pages**, asynchronously.

Concretely, stock Xvfb's framebuffer export (upstream `vfbBlockHandler`) is just:
`mmap(MAP_SHARED, fd)` the framebuffer file once → render into the mapped pages with plain stores →
`msync(MS_ASYNC)`. Xvfb **never does a per-frame copy**. "Only changed pixels hit the backing file"
is delivered for free, below Xvfb, by the MMU (dirty bits) + page cache (lazy writeback). The X
DAMAGE extension / compositor dirty-rects are a *separate, higher* optimization for compositing; the
framebuffer-to-file path doesn't need them.

---

## 3. Why wasm can't do `mmap` (the constraint)

wasm has **no MMU and no virtual memory**. Guest memory is one flat linear buffer. `mmap` needs
page-table machinery the guest doesn't have:

- **No mapping a file into an address range.** You can't point an address at a file's pages; you can
  only `memcpy` bytes into the linear array. So wasi-libc emulates file `mmap` by `malloc` + `pread`
  the whole file in.
- **No page faults / demand paging.** There is no way to trap a memory access in wasm, so pages
  can't be lazily faulted in.
- **No hardware dirty bits.** Nothing records which pages were written, so writeback can't tell what
  changed — it would have to flush everything.

Worse, stock `wasi-emulated-mman` `mmap(MAP_SHARED)` **does not write back at all** — it returns a
malloc'd copy and frees it on `munmap`. So a stock X server's framebuffer export is silently a no-op.

And the boundary is not a cheap memcpy: each write crosses the wasm-isolate ↔ host seam as a
**sync-RPC** (originally base64-encoded). A full-frame push was ~24 ms for a 1.2 MB frame and it
**blocked the single-threaded X server**, starving request processing → the `WaitForSomething
timeout=0` feedback spin (the "97% futex storm"; see `M8-STATUS-LOG.md` iter9–13).

---

## 4. What we implemented (the platform layer = the page cache) ✅

Two halves, both off the X server, both in the platform layer so the components stay unmodified.

### 4a. File-backed `MAP_SHARED` writeback — `toolchain/wasi-compat.c`

Stand in for the page cache's writeback path. Wired via `-Wl,--wrap=mmap,--wrap=munmap` in
`scripts/link-xvfb.sh`:

- `__wrap_mmap` calls `__real_mmap`, then **records** every `MAP_SHARED`, non-anonymous, `fd >= 0`
  mapping in a registry (addr, len, fd, off).
- A real `msync(addr, len, flags)` `pwrite`s the tracked region back to its fd.
- `__wrap_munmap` flushes the tracked region, then `__real_munmap`.
- `MAP_PRIVATE` caches (fontconfig/freetype) pass straight through to `__real_mmap`, untracked.

This is what lets **stock** Xvfb (`mmap(MAP_SHARED)+msync`, the restored upstream `vfbBlockHandler`)
export its framebuffer at all. It repaid the prior in-Xvfb `malloc`+per-block-`pwrite` patch
(`M8-STATUS-LOG.md` iter21, commit `1bbae0b51`).

### 4b. Dirty-page tracking via diff — runner `fd_pwrite` `[fb-delta]`

Stand in for the MMU's missing hardware dirty bits. In `crates/execution/src/node_import_cache.rs`,
the guest `fd_pwrite` path: for a large write (`n >= 65536`) at the same offset as the previous write
to that handle, diff against the last frame in **8 KB blocks** and write only each maximal **run** of
changed blocks (at `off + runStart`); skip identical frames entirely. Always correct — the backing
file already holds every byte we don't send.

- A single `min..max` changed-range collapses to a full write when changes are scattered (app window
  near the top + a panel clock at the bottom span the whole frame), so the delta is **block-granular
  run delta**, not one range (`M8-STATUS-LOG.md` iter11→11b, commits `7f2e62616`, `02ea646b9`).
- Net effect: sync-RPC cost scales with **changed** pixels, not frame size. This un-starved the
  multi-client desktop and broke the `timeout=0` spin → full LXDE renders.

**Why diff and not a marked API:** Xvfb scribbles the framebuffer through raw C pointers from
unmodified code, so there is no write call to instrument. A frame diff is the only zero-instrumentation
way to recover dirty pages, and an O(buffer) scan per *flush* is trivially cheap next to the boundary
crossing it saves (flushes are far rarer than writes).

---

## 5. How much can we "smudge"? (the toolchain-ownership analysis)

Owning the full toolchain (clang + wasi-libc + patched std sysroot + crate patches + linker wraps)
moves interception up the stack. We can intercept at every layer **except raw memory instructions**:

| Layer | Interceptable? | How |
|---|---|---|
| syscalls / libc (`mmap`, `msync`, `fcntl`, `ioctl`, `writev`) | ✅ yes | `wasi-compat.c`, `-Wl,--wrap` |
| std / sysroot behavior | ✅ yes | patched-std build (`patches/*.patch` → `patch-std`) |
| linker symbols | ✅ yes | `--wrap`, interposition |
| whole-program codegen | ✅ yes, **at a cost** | instrument every load/store with a software check |
| raw `i32.load` / `i32.store`, **transparently** | ❌ no | stock wasm exposes no seam |

The defining feature of an MMU is intercepting a **plain load/store transparently** — and wasm gives
no hook for that. So a *transparent* hardware-style MMU is impossible at the instruction level.

A **software MMU** is nonetheless buildable via the codegen row: since we own clang, guest code could
route each access through a software TLB / permission bitmap / dirty side-table, and even synthesize
page faults by checking a bitmap before each store (this is what ASan and software-fault-isolation do).
It is real, but it taxes **every** load/store (~2–10× on memory-heavy code), which defeats the reason
to be on wasm. Nobody runs a general software MMU for that reason.

**Verdict:** anything MMU-shaped is either (a) explicit-API interposition — cheap, what we do — or
(b) codegen instrumentation — possible but pays a per-access tax you do not want on a hot framebuffer.
The diff in §4b is us deliberately choosing the cheap interception point: zero per-access cost, one
O(buffer) scan per flush.

---

## 6. Can we implement `mmap` "for real"? Scope of what's feasible

- **File-backed `MAP_SHARED` + writeback — ✅ done, realistic.** Exactly §4a. Faithful enough that
  stock Xvfb works.
- **Live dirty-page tracking — ❌ not without an MMU.** No write trap → flush-everything or **diff**.
  Diff is the chosen answer (§4b).
- **Lazy demand paging — ❌.** No page-fault mechanism → must eagerly load the whole file on map.
- **Inter-process `MAP_SHARED` (two guest procs sharing live memory) — ❌ out of scope.** Each
  process is a separate isolate with its own linear memory; coherent cross-process file mappings would
  need a `SharedArrayBuffer` wired between isolates plus explicit coherency. Threads can share via SAB
  (see `WASM-THREADS-SPEC.md`); arbitrary coherent cross-process file mappings are not pursued.

So we implement a **practical subset** (file-backed shared + explicit writeback + diffed dirty
tracking). The parts that are genuinely impossible to do cheaply (demand paging, live dirty tracking,
cross-process coherence) are exactly the parts the MMU gives Linux for free.

---

## 7. Invariants & non-goals

- **TRIGGER — build the optimization when, and only when, perf diverges from a real OS.** Default is
  the cheap path (file-backed writeback + identical-frame/run-delta diff). Do **not** chase further
  mmap/VM fidelity for correctness parity alone. **But the moment an operation is markedly slower than
  the same call would be on a native OS** — a guest burning time a real kernel would not — treat that
  gap as the signal to **take the detour**: the missing MMU dirty-tracking / page-cache writeback is
  almost certainly re-shipping a full buffer across the sandbox boundary (the M8.6 framebuffer spin is
  the canonical case). Implement the targeted diff/mmap optimization then, scoped to the hot mapping.
  Perf-vs-native is what justifies the work; absent a measured gap, stay on the cheap path.
- **INV1 — Components stay unmodified (SPEC.md #5).** No `mmap`/dirty-tracking patch may live in X /
  GTK / fontconfig source. The platform layer (`wasi-compat.c` + runner) carries it.
- **INV2 — Always correct under partial writeback.** Any delta/dirty optimization must guarantee the
  backing file already holds every byte not re-sent. Identical-frame skips and run deltas both satisfy
  this; a future scheme must prove it too.
- **INV3 — Zero cost when idle.** No per-access instrumentation in the normal path; diff cost is per
  *flush*, gated by the large-write threshold.
- **Non-goal — a general software MMU.** Rejected on the per-access tax (§5). Revisit only if a
  workload needs page-fault semantics (COW, guard pages) that interposition can't supply.
- **Non-goal — inter-process coherent shared mappings** (§6).

---

## 8. Open items

- ⬜ Confirm `MAP_PRIVATE` pass-through covers all Xubuntu/Xfce mmap users (mmap'd font/icon caches,
  GdkPixbuf) with no accidental writeback expectation.
- ⬜ Decide the large-write threshold (currently 64 KiB) and block size (currently 8 KiB) against
  Xfce's larger default framebuffers; measure diff-scan cost at 1920×1080.
- ⬜ Consider a cheap "assume-dirty since last msync" hint from `__wrap`ped `msync` length to bound the
  diff scan when the caller already knows the touched extent.
