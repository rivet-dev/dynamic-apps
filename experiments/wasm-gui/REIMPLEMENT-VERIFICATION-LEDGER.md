# wasmgui Re-implementation — Adversarial Verification Ledger

_Compiled 2026-06-30 (PST). Independent skeptics tried to **refute** each load-bearing claim; this ledger records what survived._

## 1. Headline

Of **15 load-bearing claims**: **14 CONFIRMED**, **1 PARTIAL**, **0 REFUTED**, **0 UNVERIFIABLE**.
Plus **3 orchestrator-self-verified anchors (A/B/C): all CONFIRMED**.

**The plan's foundation HELD.** Every substantive technical claim the re-implementation rests on is true against the code/docs. The single PARTIAL is a **wrong-file citation**, not a wrong fact — the fact is real, it just lives in a different file than the plan cites. No claim was refuted; nothing in the plan's reasoning collapsed. The plan is safe to execute. The only real hazard is **stale line/file citations** (5 of 15 claims): a re-implementer who blindly trusts a cited `file:line` will sometimes land on unrelated code. Treat every line number as approximate and re-grep the named symbol.

## 2. Verdict table

| Claim id | Verdict | Citation accurate? | One-line finding |
|---|---|---|---|
| C-inline-default-on | CONFIRMED | off-by-lines | Inline dispatch defaults **ON** (`unwrap_or(true)`); two docstrings still wrongly say "Default OFF". |
| C-sabring-dead | CONFIRMED | accurate | `with_ring_backing_slices` has **zero** callers; no guest consumes the t1 req/resp rings. |
| C-shmreg-dead | CONFIRMED | accurate | `ShmRegistry` constructed only inside `#[cfg(test)]`; dead outside tests. |
| C-bulksab-live | CONFIRMED | accurate | Bulk-SAB fs path is a real end-to-end chain — but **gated behind `SECURE_EXEC_T1_RING` (OFF by default)**. |
| C-threads-live | CONFIRMED | accurate | `register_thread_spawn` fires every session; `spawn_wasm_thread` is a real ~115-line handler. |
| C-host-caps | CONFIRMED | off-by-lines | Two independent 30s timers (V8 CPU 30000ms, wasm fuel 30s); X server needs its own `execute_env`. |
| C-host-io | CONFIRMED | off-by-lines | Input via XTEST over host AF_UNIX; framebuffer scraped from sidecar shadow dir (wire read starves). |
| C-fdread-positional | CONFIRMED | off-by-lines | `_fdRead` reads positionally from `entry.offset`; cited JS lines are ~3612-3905, not 1576-1612. |
| C-tj-fix | CONFIRMED | accurate | `isKernelPipeFd`/`kernelPipeRecv` route 0x50000000+ fds away from host_net; new vs fork base. |
| C-fpcast | CONFIRMED | accurate | build-gtk-app.sh:99 defaults `@64`; 3.7x (15.3s→4.1s) 0-pixel-diff; measured GTK true-max 25. |
| C-glyph-retracted | CONFIRMED | accurate | Glyph "floor" RETRACTED; native ~10ms by same method; T-wake round-trip is the open lever. |
| C-root2-reversed | CONFIRMED | accurate | Root-2 reversed; service thread ~99% idle; Path C spin REFUTED; co-location (Path B) sole survivor. |
| C-h1-tls | CONFIRMED | accurate | TLS-errno needs `-matomics -mbulk-memory`; direct clang (not libtool) link; `__wasi_init_tp` load-bearing. |
| C-f1b-ingest | CONFIRMED | accurate | F1b event-ingest LANDED + default-ON (`unwrap_or(true)`), cold-boot-windowed; fp 4.3→2.5s, no ir cost. |
| **C-governance** | **PARTIAL** | **wrong-file** | Wakeup invariant + ~100ms ir confirmed; **64ms-glyph-floor distinction is NOT in bench-ir.sh** (misattributed). |

## 3. Detail on the non-clean claim (PARTIAL)

### C-governance (C2 + C3 governance facts) — PARTIAL, wrong-file

**What holds (all underlying facts are real):**
- **Part (a) — fully confirmed.** `/home/nathan/secure-exec-wasmgui/CLAUDE.md:30` states the invariant verbatim: *"Wakeups are event-driven, never timer-polled… Never add a timer/poll fallback… A missed wake is a permanent-hang bug."* And `CLAUDE.md:40`: *"A perf gap vs native Linux is a runtime bug until a 1:1 native trace proves otherwise — never call it a floor."*
- **Part (b), the ~100ms half — confirmed in the cited file.** `bench-ir.sh:29`: *"True warm single-keystroke ir ~100ms (was mis-read as ~20-88ms)."*

**What broke — the misattribution:**
- The claim says `bench-ir.sh` *also* states "64ms is the glyph-RENDER floor (a different quantity)." It does **not** — `grep` for `64` and `floor` in `bench-ir.sh` returns nothing.
- That 64ms-glyph-render-floor distinction actually lives in **`REIMPLEMENT-GAPS-AUDIT.md:70-71`** and **`REIMPLEMENT-ON-CLEAN-MAIN.md:295`**.

**Exact correction the plan needs (applies to plan items C2/C3):**
> In the C2/C3 governance citation, **split the source attribution**: cite `bench-ir.sh:29` for the *~100ms warm single-keystroke ir*, and cite `REIMPLEMENT-GAPS-AUDIT.md:70-71` (or `REIMPLEMENT-ON-CLEAN-MAIN.md:295`) for the *64ms glyph-render-floor-is-a-different-quantity* distinction. Do **not** attribute the 64ms statement to `bench-ir.sh`.

No fact is wrong; only the pointer is. This is a documentation-source fix, not a logic fix. **It does not weaken C2/C3.**

## 4. Citation-accuracy issues (re-impl footguns) — even where the claim HELD

These claims are **true**, but a re-implementer who jumps to the cited `file:line` will land on the wrong code. Re-grep the named symbol instead of trusting the number.

| Claim | Cited | Actual | Footgun |
|---|---|---|---|
| C-inline-default-on | execution.rs `:14070-14086` (enabled fn); docstrings `:14070-14074`, `:3182-3185` | enabled fn is `:14117` (parse `:14123-14126`). `:14070-14086` is unrelated `UnixInlineNetDrain`. Stale "Default OFF" docstrings live at `:14112-14114` + inline `:3194-3198`; `:3182-3185` is `spawn_process` argv. | Wrong line → lands on net-drain/argv code, not the env-parse logic. **Note the live doc/code mismatch: two docstrings still claim "Default OFF" while code defaults ON.** |
| C-host-caps | xserver execute path `~:854 / :1634`; CPU default `javascript.rs:~2083`; fuel `wasm.rs:~64` | xserver `execute_env` calls at `:1001` and `:2174` (`:854` = `run_pty_test`, `:1634` = framebuffer reader). CPU default at `javascript.rs:2106-2114` (~23 lines below). `wasm.rs:64` exact. | Wrong xserver lines point at PTY/framebuffer code, not the env opt-out. |
| C-host-io | broken-live-delivery `:373-377 / :538-540`; XTEST `:394-625`; inject `:1538-1577`; scrape `:1579-1637` | `:538-540` is XInput command parser, **not** the broken-delivery comment — that's at `:1539-1540` (likely off-by-1000 typo for 1538-1540, already covered by the inject citation). All other refs accurate. | One stray line ref; substance intact. |
| C-fdread-positional | wasm.rs `~357-384, 1576-1612` (positional read) | Positional `_fdRead`/`entry.offset`/`_fdSeek` JS shim is `~3612-3905`; `1576-1612` is the **Rust `fs.readSync` RPC handler**. `WasmCharDevice` `:357-384` correct. | Biggest line jump in the set — `1576-1612` is a different layer entirely. Go to **~3612-3905** for the offset logic. |
| C-governance | bench-ir.sh `~6-9, 21-29` for 64ms floor | 64ms floor not in bench-ir.sh at all → `REIMPLEMENT-GAPS-AUDIT.md:70-71` / `REIMPLEMENT-ON-CLEAN-MAIN.md:295` | Wrong file (see §3). |

Accurate-citation claims (no footgun): C-sabring-dead, C-shmreg-dead, C-bulksab-live, C-threads-live, C-tj-fix, C-fpcast, C-glyph-retracted, C-root2-reversed, C-h1-tls, C-f1b-ingest.

**Two extra behavioral caveats worth carrying into implementation (claims still CONFIRMED):**
- **C-bulksab-live**: the bulk-SAB fast path is fully serviced *but only fires when `SECURE_EXEC_T1_RING=1`* — it is **OFF by default**, and shares the same flag as the dead req/resp ring. Don't assume it's active in a default config.
- **C-inline-default-on / C-f1b-ingest**: stale "Default OFF"/"Gated OFF" doc comments survive next to code that defaults ON. Trust the `unwrap_or(true)` logic, not the comments — and consider fixing the comments as part of the re-impl to avoid re-confusing the next reader.

## 5. Self-verified anchors (orchestrator) — CONFIRMED

| Anchor | Statement | Status |
|---|---|---|
| **A** | `registry/native` is **0-diff** vs fork [K12]. | CONFIRMED |
| **B** | main **deleted** `crates/kernel/src/{vfs,overlay_fs,root_fs,mount_table,mount_plugin}.rs` and **created** `crates/vfs/src/posix/*` (the kernel→vfs split) [B]. | CONFIRMED |
| **C** | wasmgui's `registry/native` has **no `HOST_NET` branch** (the K13 repayment-gap). | CONFIRMED |

## 6. Net confidence statement

**The plan is safe to execute as written.** 14/15 load-bearing claims and all 3 anchors are CONFIRMED; the lone PARTIAL is a citation-source error with the underlying fact fully intact. **No claim was refuted — the foundation did not crack anywhere.**

**The only required edit** before/at implementation time:
- **C2/C3 (C-governance):** fix the source attribution — 64ms glyph-render-floor distinction → `REIMPLEMENT-GAPS-AUDIT.md:70-71` (not `bench-ir.sh`); keep `bench-ir.sh:29` for the ~100ms ir.

**Re-check (line refs only, not logic) at implementation time** — re-grep the named symbol rather than trusting the cited line in these 5: `C-inline-default-on`, `C-host-caps`, `C-host-io`, `C-fdread-positional`, `C-governance`. Everything else can be followed at its cited location.

**Two configuration gotchas to honor while building:** the bulk-SAB path (K11) and the dead req/resp ring share `SECURE_EXEC_T1_RING` (OFF by default — bulk path is reachable but inert until set); and inline-dispatch/F1b are default-ON despite leftover "OFF" doc comments.
