# GObject fpcast performance — mechanism + concrete integration plan

Scope: the #1 single-app slowness in the wasm Xubuntu desktop is GObject
construction, and the dominant cost inside it is **binaryen `--fpcast-emu`
uniform-arity padding**, not the call opcode. This document restates the exact
mechanism, lays out the full mitigation family with implementation specifics,
and gives a ranked, concrete recommendation for **our** clang + wasi-sdk +
binaryen toolchain (NOT Emscripten).

References for our toolchain in this doc:
- `experiments/wasm-gui/scripts/build-gtk-app.sh` (GTK apps: xfwm4, lxpanel,
  gtk-hello, openbox; uses `--fpcast-emu -pa max-func-params@64`, default `64`,
  override `SECURE_EXEC_FPCAST_MAXP`, pre-fpcast save `SECURE_EXEC_KEEP_PREFPCAST`).
- `experiments/wasm-gui/scripts/link-xapp.sh:24` (X Athena apps: twm/xclock/xcalc;
  uses **bare** `wasm-opt --fpcast-emu` → binaryen's default `max-func-params=16`).
- `PERF-FINDINGS.md` (Root-1 correction: typed-function-references is NOT the fix;
  the cost is uniform-arity padding; V8 130 has wasm-gc + typed-func-refs default-on,
  `crates/.../isolate.rs:102-109`).

---

## 1. The mechanism, and exactly why it is slow

### 1a. Why GObject needs it
GLib's `toolchain-requirements.md` makes "calling functions through differently
typed function pointers" a **hard requirement** — calling a fn pointer with the
wrong pointer type, with **extra** args (ignored), or with **too few** args. This
is pervasive in GObject: the generic closure marshaller (`g_cclosure_marshal_generic`),
signal dispatch, `GTypeClass`/interface init vfunc tables, property
setters/getters. Native archs tolerate the UB casts; wasm `call_indirect`
type-checks strictly and **traps** on a signature mismatch. So a GObject build
cannot run on wasm without making mismatched indirect calls work.
*(Source: glib toolchain-requirements.md; emscripten#23952, which reversed the
deprecation of `EMULATE_FUNCTION_POINTER_CASTS` precisely because GLib cannot be
hand-patched the way CPython was.)*

### 1b. What binaryen `--fpcast-emu` actually does
From `FuncCastEmulation.cpp` (the pass behind `wasm-opt --fpcast-emu`):

- It defines **one** uniform ABI signature for *all* indirect calls:
  `(i64 × N) -> i64`, where `N = max-func-params` (default **16**, line 160).
- **Every table function** gets a thunk `byn$fpcast-emu$<name>` whose params are
  `N × i64`; the thunk converts each i64 back to the real param type (`fromABI`),
  calls the real function, and converts the result up to i64 (`toABI`). The table
  entry is rewritten to point at the thunk (`makeThunk`, lines 164-219).
- **Every `call_indirect` site** is rewritten (`visitCallIndirect`, lines 126-145):
  each real operand is widened to i64 (`ExtendUInt32` / `ReinterpretFloat*`), then
  the operand list is **padded with `i64.const 0` up to N**, the call's heap type
  is swapped to the uniform ABI type, and the i64 result is narrowed back.

So a 1-pointer GObject vfunc call (`(i32) -> i32`) is compiled into: extend the
1 real arg to i64, push `N-1` zero i64s, call the thunk through the uniform type,
the thunk drops `N-1` args + narrows the 1 real one, then narrow the result back.

### 1c. Why it dominates GObject construction
- Cost per indirect call ≈ `O(N)` (the padding + per-arg coerce), and the tax is
  paid by **every** indirect call — including the **correctly-typed majority**
  (real vfunc dispatch through correctly-typed pointers). fpcast-emu cannot tell a
  legitimate call from a UB cast; it taxes all of them uniformly.
- GObject is almost entirely indirect calls (class_init/interface_init vtables,
  property install, signal marshalling, the CSS cascade in GTK). So total cost ≈
  `(#indirect-calls) × (arity coercions + pad + thunk hop)`, and that product is
  the ~13s GObject/CSS construction figure.
- **Measured here:** `@128 → @64` cut GTK first-widget construction ~3.7× (15.3s →
  4.1s, 0-pixel render diff). That superlinear-looking win is padding shrinking
  *plus* smaller thunks/binary (icache). The padding is the lever.
- The true max arity across our GTK surface (gtk-hello, mousepad, xfwm4,
  xfce4-panel, xfdesktop, thunar) is **25** — so `@64` is still ~2.5× over the real
  floor, i.e. every indirect call still pads ~39 dead i64 args.

---

## 2. The full mitigation family (concrete)

### Lever A — tune `max-func-params` down to the true max (we are here, with headroom)
The padding cost scales with `N`. We are at `@64`; the measured true max is `25`.
Lowering `N` toward 25 removes the remaining dead-arg padding on every call.

- **How:** change `SECURE_EXEC_FPCAST_MAXP` default in `build-gtk-app.sh:99` (and
  optionally unify `link-xapp.sh:24`, which is on the bare default 16).
- **Safety is build-time, not runtime:** `FuncCastEmulation` `Fatal()`s at pass
  time if any `call_indirect` has more operands than `N`
  (`visitCallIndirect`, lines 127-128: *"max-func-params needs to be at least …"*).
  So setting `N` too low **fails the build loudly**, it never produces a guest that
  traps at runtime. This makes probing down to the true max safe to automate.
- **Headroom:** `@64 → @32` is another ~halving of padding (32 vs a 25 floor =
  tight but valid). Returns diminish as `N` approaches 25 because the real-arg
  coercion + thunk hop is a fixed floor that survives any `N`.

### Lever B — selective / per-signature trampolines (the real structural fix)
This is hoodmane's proposal in emscripten#23952 and the direction of Pyodide's
newer work (§3b): stop using one uniform wide ABI; instead, at each call site,
**keep correctly-typed calls native** and only adapt the genuine mismatches.

- **Mechanism (wasm-gc `ref.test`):** for a call site whose *declared* signature is
  `S`, emit `ref.test (ref S)` on the target funcref (via `table.get`). If it
  matches (the common case) call **directly** through `call_ref` with **zero
  coercion and zero padding**; only on mismatch fall to an arity-adapting slow path
  (drop extra args / supply `0` for missing args, the same semantics fpcast-emu
  gives, but only for the few signatures that actually occur at that site).
- **Why it wins where A cannot:** A still taxes *every* indirect call (the floor in
  §1c). B converts the **correctly-typed majority of GObject vfunc dispatch to
  native cost** and pays an adapter only on the UB-cast minority. That floor is
  exactly the residual A can never remove.
- **Runtime is ready:** our V8 130 has wasm-gc + typed-func-refs default-on
  (`isolate.rs:102-109`), so `ref.test` / `call_ref` run without flags.
- **What does NOT exist yet:** there is no upstream binaryen pass that does this —
  it is a proposal, not merged code. Implementing B = writing/vendoring a custom
  binaryen pass (a "selective fpcast-emu" that leaves matching `call_indirect`
  alone and rewrites only mismatching sites to a `ref.test`-dispatched trampoline).
  Our toolchain already vendors patches (`registry/native/patches/*`), so a vendored
  binaryen pass fits the established pattern, but it is real compiler work.
- **C-level expressibility (adjacent):** clang now has
  `__builtin_wasm_test_function_pointer_signature(f)` (the intrinsic Pyodide added
  for §3b), which lowers to `ref.test`. It lets the *source* self-guard a cast, but
  it requires patching GLib call sites and so is not a drop-in for an opaque binary;
  the binaryen-pass form is preferred for us because it needs no GLib source edits.

### Lever C — Pyodide's JS-trampoline approach (NOT applicable to GTK; documented to reject)
Pyodide 0.19 deleted `EMULATE_FUNCTION_POINTER_CASTS` by hand-patching CPython's
**small, finite** set of bad call sites to call an `EM_JS` trampoline
(`wasmTable.get(func)(self,args,kwargs)`); JS call semantics forgive arity, so the
mismatch disappears. Result: **-25% code size (12 MB → 9.1 MB), +10–20% speed,
recursion 120 → 1000.**
- **Why it does not transfer:** it depends on the bad call sites being *few and
  known*. GLib's casts are pervasive and live in the generic marshaller + GType
  init — there is no short list to patch (this is the exact reason emscripten#23952
  *un-deprecated* blanket emulation for GLib).
- **And it is wrong for our model anyway:** our wasm runs inside a V8 isolate with
  **synchronous in-context imports**; routing every GObject indirect call out to a
  JS frame would be catastrophic on the construction-dense path (the opposite of
  the win we want), and JS frames also break JSPI/stack-switching. Reject for the
  general GTK case.

### Lever D — LLVM/clang compile-time thunk generation (partial; track upstream)
The fluendo/kleisauke work on emscripten#23952 (`fluendo/llvm-project`
`wasm-function-pointers`) teaches Clang CodeGen to **emit a correctly-typed wrapper
thunk at compile time** when a cast changes the param count, detecting the cast in
`CGCall.cpp`/`CGExprConstant.cpp` and placing `__fp_*` thunks in the elem segment —
eliminating the need for fpcast-emu for the cases it covers.
- **Limits:** handles **fewer-arg** casts only (you cannot invent values for
  *extra* params — see toolchain-requirements' "additional arguments" requirement),
  so it **reduces but does not remove** the need for emulation; GLib still has
  more-arg and opaque-`void(*)(void)` casts. Status: draft PR, ICEs encountered,
  not upstream, and needs a **custom-built clang** (the authors hit clang-plugin
  ABI mismatch vs a vendored clang). High build cost, partial coverage. **Track,
  don't adopt yet.**

### Lever E — per-signature GObject marshallers (source-side, partial)
Force GObject to use the generated typed marshallers (`g_cclosure_marshal_VOID__*`)
instead of the generic `g_cclosure_marshal_generic` for the signals the desktop
exercises → those become correctly-typed `call_indirect`, needing no emulation.
- **Limits:** addresses signal *emission*, not the construction-heavy
  class_init/property/vfunc path that dominates `g_object_new`. Pervasive GLib
  source patching for a partial slice. Lower priority than A/B.

---

## 3. The Pyodide source chain (so the mechanism claims are anchored)
- **3a (0.19, JS trampolines):** `wasmTable.get(func)(self,args,kwargs)` patched at
  CPython's finite call sites; relies on JS arity-forgiveness. Numbers as in Lever
  C. *(blog.pyodide.org "Function Pointer Cast Handling in Pyodide"; 0.19 release
  notes.)*
- **3b (newer, wasm-gc):** to remove the JS frames (which block JSPI), use
  **`ref.test` to detect the function reference's signature at runtime and dispatch
  by arity in-wasm** (a `countArgs`-style switch via `table.get` → `ref.test`
  against each candidate type), calling directly without crossing into JS; exposed
  to C as `__builtin_wasm_test_function_pointer_signature()`. This is the same
  primitive Lever B uses. *(blog.pyodide.org "JSPI and function pointer cast
  handling".)*

---

## 4. Ranked recommendation for our toolchain

| Rank | Change | Where | Expected construction win | Cost | Risk |
|---|---|---|---|---|---|
| **1** | **Lower `max-func-params` to true-max + thin margin (`@32`, target floor 25)** + add a build-time true-max probe | `build-gtk-app.sh:99` default; consider unifying `link-xapp.sh:24` | Further but **diminishing** vs the `@64` baseline — captures most of the remaining ~39-dead-arg padding; floor is the real-arg coerce/thunk hop that survives any N | **Hours**, ~1 line + assertion | **Low.** Too-low N = **build-time `Fatal`**, never a runtime trap (FuncCastEmulation l.127). 0-pixel render diff expected, as with `@128→@64` |
| **2** | **Vendored binaryen "selective fpcast" pass**: leave matching `call_indirect` native (`call_ref`), rewrite only mismatched sites to a `ref.test`-dispatched per-signature trampoline | new pass in our vendored binaryen, invoked from `build-gtk-app.sh` in place of `--fpcast-emu` | **Largest** — removes the per-call tax on the correctly-typed GObject vfunc majority (the §1c floor), not just the padding | **Weeks** (write/maintain a binaryen pass) | **Medium.** Must correctly classify mismatch sites; wrong classification → trap. Mitigate with a fpcast-emu fallback build + the existing render-diff check. Runtime is ready (wasm-gc default-on) |
| 3 | Adopt fluendo/kleisauke **clang thunk-gen** to drop fpcast-emu for fewer-arg casts | patched clang in `registry/native` | Shrinks what pass #2 must cover; partial alone | High (custom clang build, ABI-fragile), not upstream | Medium-high; **track**, don't adopt |
| 4 | **Typed GObject marshallers** for desktop signals | GLib source patches | Helps signal emission, not construction | Medium (pervasive) | Low correctness, low value for construction |

### Concrete next action
1. **Do #1 now.** Set `SECURE_EXEC_FPCAST_MAXP` default to `32` in
   `build-gtk-app.sh:99`. Add a one-shot probe per app: build with descending `N`
   until the pass `Fatal`s, record the true max, then set `N = true_max` (or
   `true_max + small_margin`) **per app** rather than one global `@64`. Because the
   guard is build-time, this is safe to script and yields the tightest padding each
   app can take. Re-run the existing render-diff check (expect 0-pixel diff).
2. **Then invest in #2** as the Root-1 endpoint. #1 buys the cheap residual; #2 is
   the only lever that removes the floor #1 cannot. Build it behind a flag with the
   current `--fpcast-emu @<true-max>` path as the verified fallback.

---

## 5. Sources
- GLib `toolchain-requirements.md` — "Calling functions through differently typed
  function pointers" hard requirement; `-sEMULATE_FUNCTION_POINTER_CASTS` required +
  has a perf penalty.
  *(prior-art: 2026-06-26T12-55-glib-toolchain-requirements.md.)*
- emscripten#23952 thread — un-deprecating emulation for GLib; hoodmane's `ref.test`
  binaryen-pass proposal; kleisauke/turran LLVM/clang thunk-gen experiments
  (`fluendo/llvm-project` `wasm-function-pointers`); the more-arg cast is
  unsolvable by thunks.
  *(prior-art: 2026-06-26T12-55-emscripten-issue-23952.txt.)*
- Binaryen `FuncCastEmulation.cpp` — uniform `(i64×N)->i64` ABI, per-table-function
  thunks, per-call padding, default `max-func-params=16`, build-time `Fatal` guard.
  *(prior-art: 2026-06-26T12-55-binaryen-func-cast-emulation.cpp;
  github.com/WebAssembly/binaryen src/passes/FuncCastEmulation.cpp.)*
- Pyodide blog "Function Pointer Cast Handling in Pyodide" + 0.19 release notes —
  JS-trampoline mechanism; -25% size (12→9.1 MB), +10–20% speed, recursion 120→1000.
  *(blog.pyodide.org/posts/function-pointer-cast-handling/, /posts/0.19-release/.)*
- Pyodide blog "JSPI and function pointer cast handling" — `ref.test` in-wasm
  dispatch, `countArgs`, `__builtin_wasm_test_function_pointer_signature()`.
  *(blog.pyodide.org/posts/jspi-and-function-pointer-cast-handling/.)*
- This repo: `build-gtk-app.sh` (`@64`, true-max 25), `link-xapp.sh:24` (bare/16),
  `PERF-FINDINGS.md` (Root-1 correction; V8 130 wasm-gc default-on, isolate.rs:102-109).
</content>
</invoke>

---

## 6. Feasibility finding (2026-06-26) — Lever B needs binaryen-from-source; Lever A is marginal

Investigated the toolchain (non-thrash, code-reading):
- `wasm-opt` is a **prebuilt binary**: `/home/nathan/.cargo/bin/wasm-opt`, **version 116** (cargo-installed). There
  is **no `third_party/binaryen` source checkout** and no binaryen build step. `build-gtk-app.sh:107/112/114` and
  `link-xapp.sh:24` invoke that prebuilt `wasm-opt`.

### Implications
- **Lever A (arity)** is a one-line/script change against the prebuilt wasm-opt — trivially feasible — but it is
  **marginal**: my earlier measurement `@64→@28 ≈ 2.6%`, and §1c's floor (real-arg coerce + thunk hop) survives any
  `N`. So A buys only the last sliver. Do NOT spend build+measure churn cycles iterating A; if banked at all, set a
  tighter default (`@32`) or run the per-app probe opportunistically during a build that is happening anyway.
- **Lever B (the real lever)** is NOT a wasm-opt flag. It requires: (1) **vendor + build binaryen from source** at a
  version with stable wasm-gc / `ref.test` / `call_ref` (post-v116 line), a new heavy C++ toolchain dependency;
  (2) write a custom pass `SelectiveFpcastEmulation` (leave matching `call_indirect` as `call_ref`; rewrite only
  signature-mismatch sites to a `ref.test`-dispatched per-signature trampoline); (3) swap that custom `wasm-opt`
  into `build-gtk-app.sh` behind `SECURE_EXEC_SELECTIVE_FPCAST=1`, keeping the prebuilt `--fpcast-emu @<true-max>`
  path as the verified fallback; (4) verify with the existing render-diff (expect 0-pixel) + before/after
  construction timing.

### Concrete Lever B build plan (focused-session)
1. Vendor binaryen source under `registry/native/third_party/binaryen` (or a pinned git submodule), build `wasm-opt`
   (cmake + ninja), confirm `--version` > 116 and wasm-gc enabled.
2. Add `src/passes/SelectiveFpcastEmulation.cpp` modeled on `FuncCastEmulation.cpp` but: for each `call_indirect`,
   compare the site's declared heap type to the candidate target type via `ref.test`; emit `call_ref` on match;
   only on mismatch emit the arity-adapting trampoline (drop extra / zero-fill missing) for the few real signatures.
3. Register the pass; invoke it from `build-gtk-app.sh` as the pass-1 replacement for `--fpcast-emu`.
4. Verify: render-diff 0-pixel on gtk-hello/xfwm4/panel; construction timing before/after; binary-size delta.

### Go/no-go (strategic — flag to the user, do not silently commit a binaryen-source build)
- **B is the only lever that removes the §1c per-call floor** and is plausibly another large win (the floor is
  what's left after the `@64` padding win). It is the right Root-1 endpoint.
- **But B is weeks of compiler work + a from-source binaryen build added to the toolchain** — a real infrastructure
  commitment. That dependency decision deserves an explicit go-ahead, not a silent fragment action.
- **Recommendation:** treat B as a dedicated focused-session project (plan above is ready). Meanwhile do NOT burn
  cron fires on marginal Lever A; the higher-ROI fragment-and-focused work is **T1 SAB transport** (design+security
  done, reader built+tested) which the architecture doc ranks first and which is lower-risk than both B and the
  Root-2 multiplex.
