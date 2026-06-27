# XU7 channel inventory — transport & notify-path map (artifact for T-A)

Read-only code analysis (no builds). Determines whether the cross-process-pipe lost-wake (T-A) is
reachable in XU7's hot path. **Verdict: NOT reachable — XU7 uses host AF_UNIX sockets, which notify.**

## Channel map

| Channel | Transport | Intra/Cross process | Notify path | Lost-wake? |
|---------|-----------|---------------------|-------------|------------|
| X11 client ↔ Xvfb | host AF_UNIX socket (`/tmp/.X11-unix/X0`) | cross-process | reader thread → `socket_readiness.notify()` on data (execution.rs:12606), EOF (12592), error (12617) | **NO** |
| D-Bus client ↔ dbus-daemon | host AF_UNIX socket (`/tmp/.dbus/session`) | cross-process | same reader-thread notify path | **NO** |
| GLib GWakeup (main↔worker) | kernel pipe / eventfd self-pipe | **intra-process** (both fds one process, gwakeup.c:163) | write → `__kernel_fd_write` → `socket_readiness.notify()` (execution.rs:16638) | N/A |
| app stdout/stderr → parent | kernel pipe (default stdio) | cross-process *if forked* | `__kernel_fd_write` → notify (16638) | NO (notify present) — and **not present in current harness** |

## Key evidence (file:line)

- Xvfb listener: `ActiveUnixListener::bind(host_path, guest_path, backlog)` (execution.rs:20082) →
  real host `UnixListener::bind(host_path)` (execution.rs:1920).
- X client connect: `ActiveUnixSocket::connect(host_path, guest_path, Arc::clone(&process.socket_readiness))`
  (execution.rs:1968–1972) — passes the **client process's** readiness.
- Unix socket reader thread: `spawn_unix_socket_reader(... readiness ...)` (execution.rs:12569–12623);
  `readiness.notify()` on data (12606) / EOF (12592) / error (12617) → wakes the reading process.
- GWakeup self-pipe: `g_unix_open_pipe(wakeup->fds, ...)` (gwakeup.c:163); `struct _GWakeup { gint fds[2]; }`
  (gwakeup.c:122–125) — both ends in one process.
- Kernel pipe write notify: `kernel.fd_write(...)` then `if written > 0 { process.socket_readiness.notify(); }`
  (execution.rs:16630–16639).
- Pipe manager local CV (NOT cross-process readiness): `notify_waiters_and_pollers` (pipe_manager.rs:321,331)
  — local condvar only; the cross-process wake is the explicit 16638 call.

## Consequences for the ledger

- **T-A REFUTED for XU7 hot path** (the cross-process-pipe mechanism isn't traversed).
- **10ms cap still fires** in XU7 (every GTK app has the intra-process GWakeup pipe → `pollSetHasPipes`),
  but is **redundant** (GWakeup notifies via 16638) → cap removal (task #26) expected safe for XU7.
- **D1 still decisive**: confirms refutation if wakes are notify-dominated; reopens T-B (socket-notify
  race) if any starved guest is deadline-dominated despite socket traffic.
- New weight on **T-F** (every X client = a host socket + a sidecar reader thread; N clients × Xvfb →
  thread fan-out) and **T-C/T-D** (single-threaded Xvfb serializes all clients over the host-socket bridge).
