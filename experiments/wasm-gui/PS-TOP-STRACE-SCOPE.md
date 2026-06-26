# ps / top / strace — scope (approved task #21), measured against the kernel

Constraint #4: scoped before building. Findings from crates/kernel/src/kernel.rs:

## ps / top — KERNEL IS READY; remaining work is a wasm-command build (bounded)
- `/proc` enumerates pids: `proc_read_dir(ProcNode::RootDir)` (kernel.rs:3555-3570) yields every active pid as a
  dir entry plus cpuinfo/loadavg/meminfo/mounts/self/uptime/version.
- Per-pid nodes exist: `PidDir`, `PidFdDir`, `PidCmdline`, `PidEnviron`, `PidCwdLink`, `PidStatFile` (3388-3395).
- So a real `ps`/`top` that reads `/proc/[pid]/stat` + `/proc/[pid]/cmdline` works with NO kernel change.
- Remaining work: build/stage a `ps`/`top` binary as a wasm command (procps, or a minimal reader) via the
  `make -C registry/native wasm` toolchain. That's a focused wasm-command-build session (the toolchain has the
  cross-env/CC friction documented in ROOT-2-MULTIPLEX-DESIGN.md), but the kernel side needs nothing.
- If a field ps wants is missing (e.g. `/proc/[pid]/status`), adding it is a COORDINATED procfs change (per
  crates/kernel/CLAUDE.md: path resolution + read_dir + read-bytes + stat sizing + filetype/inode switches +
  tests/identity.rs together).

## strace — NEEDS A KERNEL SYSCALL-TRACE CAPABILITY (focused-session, larger)
- wasm/wasip1 has no `ptrace`, so a real strace cannot attach the Linux way. The kernel (which already brokers
  every guest syscall via the sync-RPC path) would need to expose a syscall-trace stream (per-pid, opt-in) that a
  guest `strace` consumes. That is a NEW kernel observability capability, not a command build.
- This overlaps the Root-2 instrumentation: both want per-syscall visibility at the sidecar sync-RPC boundary
  (`WasmExecutionEvent::SyncRpcRequest` handling, execution.rs:2685/2759). Build the trace hook once, expose it to
  BOTH the host Root-2 measurement AND a guest strace.

## Bottom line
- `ps`/`top`: unblocked at the kernel; needs one wasm-command-build session.
- `strace`: needs a kernel syscall-trace capability (shareable with Root-2 instrumentation) — focused-session.
