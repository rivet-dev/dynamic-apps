<!-- Consolidated decision brief for the two human sign-offs that gate XU6-terminal and XU7.
     Written 2026-06-26 once the autonomous/clean work was exhausted. Not self-approved -- these are the
     TCB/architecture decisions the loop is explicitly told to surface, never decide. -->
# Two sign-offs gate the last 2 of 8 XU milestones

Everything cleanly reachable without a TCB/architecture decision is **done**: XU0-XU5; XU6 = 5 bundled apps
+ 4 settings dialogs + the Settings Manager hub window render; keyboard typing / launcher search / menu
popups all work; reusable infra (XKB keymap, dbus-fixture, POST_INJECT_DELAY) landed. The remaining two
milestones each wait on one decision only you can make.

---

## 1. XU6 terminal -- wire `host_process.proc_spawn`

**What it unblocks:** xfce4-terminal (the one remaining XU6 app). VTE already links, renders, opens the
kernel PTY (`openpt rc=0`), and reaches `vte_terminal_spawn_async`. It then traps because
`host_process.proc_spawn` has **no sidecar handler** (only `__pty_open` is wired).

**The work:** add a `proc_spawn` handler in the wasm sidecar that spawns a **sandboxed guest shell**
(e.g. a `brush.wasm`) on VTE's PTY slave fd -- routing to the kernel's existing `spawn_process`. The
keyboard half is already proven (typing into mousepad works end-to-end via the staged XKB keymap + XTEST).
A real shell guest (brush) would also need building.

**Why it's a decision, not a free action:** it grants the wasm-GUI sandbox a **process-spawning capability**.
It is *not* a host escape -- the spawned shell is itself a sandboxed guest (the wasm execution model; the
same capability exists for node `child_process`). It is not in the explicit never-self-approve list
(D-Bus / host-fd / GPU / network). So it is *arguably* autonomous, but it is a genuine capability + a
substantial wiring, so I am surfacing it rather than self-approving.

**Recommendation:** **Approvable.** Sandboxed guest-spawning is the wasm model and not a host-boundary
expansion. If you confirm, the path is: wire `proc_spawn` -> guest shell on the PTY, build/stage `brush.wasm`,
verify a typed command echoes (the input path is ready).

---

## 2. XU7 full session -- the Root 2 service-thread multiplex

**What it unblocks:** XU7 acceptance -- the full 4-guest Xubuntu session rendering together (the last
milestone). Every component renders **solo**; the 4-guest session does not.

**The root cause (definitively characterized):** ONE sidecar sync-RPC service thread serializes **every**
guest's syscalls. The wasm X server (Xvfb) is itself a guest on that thread, so under 4 concurrent heavy
GTK guests it is **starved** -- it rendered ~1 window in 216s. Proven by exhaustively peeling every
contained lever: the `@32` fpcast cascade fix (~3.7x faster construction) + the D-Bus `auth_timeout` fix
made the session *much* healthier (all 4 launch, survives ~2x longer, 0 auth/xfconf timeouts at 3 guests),
but the serialization is the wall.

```
   single sidecar service thread  ──►  [ Xvfb ][ xfwm4 ][ xfdesktop ][ panel ][ thunar ]
            (serialized)                  ▲ the X server competes for the same thread,
                                            so it can't drain draw requests -> blank
```

**The work:** parallelism in the sidecar -- per-guest service threads, OR (narrower) a dedicated thread
for the X server's IO. **Either requires the kernel / VFS / socket-table / permission-policy to be
thread-safe** for concurrent guest access.

**Why it's a decision:** it is a deep **TCB concurrency refactor**. The risk is concurrency bugs in the
trusted computing base, which would affect the correctness/security of *every* VM, not just the desktop
demo. The `@32` + `auth_timeout` fixes are real prerequisites that, combined with the multiplex, should
render the session.

**Recommendation:** **Genuine sign-off required** -- this is not self-approvable. It is the single
highest-leverage change left (it also lifts the per-thread ceiling for any multi-guest workload), but it
must be scoped and reviewed as a TCB change.

---

*Status detail: `M8-STATUS-LOG.md` (the full T-numbered engineering log) and `HANDOFF-PERF-AND-CEILING.md`
(the Root 1/Root 2 analysis). Visual proof of every rendered component: `~/tmp/gui-progress/`.*
