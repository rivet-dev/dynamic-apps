// Execution budget enforcement via dedicated watchdog threads.
//
// Two INDEPENDENT mechanisms live here:
//
//   * `TimeoutGuard` — a WALL-CLOCK timer. It counts elapsed real time
//     INCLUDING idle/await, so it can cap a guest that blocks or awaits
//     indefinitely. It is an INDEPENDENT, opt-in backstop armed only when the
//     operator sets `AGENT_OS_V8_WALL_CLOCK_LIMIT_MS` (off by default so
//     long-lived ACP adapters are never killed by a default).
//
//   * `CpuBudgetGuard` — a TRUE CPU-TIME budget. It samples the EXECUTION
//     thread's per-thread CPU clock (`pthread_getcpuclockid` +
//     `clock_gettime`). Because a thread's CPU clock does not advance while the
//     thread is parked/awaiting I/O, this counts ONLY active JS CPU time and
//     EXCLUDES idle/await. V8 has no native budget primitive, so this poll +
//     `terminate_execution()` approach is the standard embedder pattern. Armed
//     only when the operator opts in via `AGENT_OS_V8_CPU_TIME_LIMIT_MS`.
//
// The two guards are independent: setting one env knob arms only that guard,
// and when both are set whichever fires first terminates execution.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

pub(crate) const TIMEOUT_GUARD_START_ERROR_CODE: &str = "ERR_TIMEOUT_GUARD_START";
#[cfg_attr(test, allow(dead_code))]
pub(crate) const CPU_BUDGET_GUARD_START_ERROR_CODE: &str = "ERR_CPU_BUDGET_GUARD_START";

/// How often the CPU-budget watchdog samples the execution thread's CPU clock.
#[cfg_attr(test, allow(dead_code))]
const CPU_BUDGET_POLL_INTERVAL: Duration = Duration::from_millis(50);

/// An opaque handle to a specific thread's CPU-time clock, captured ON that
/// thread and safe to read from another (watchdog) thread.
///
/// The POSIX per-thread CPU clock id is derived from the thread's `pthread_t`
/// and remains valid for the lifetime of that thread, so the watchdog can poll
/// it via `clock_gettime` without running on the execution thread itself.
#[cfg(unix)]
#[cfg_attr(test, allow(dead_code))]
#[derive(Clone, Copy)]
pub(crate) struct ThreadCpuClock {
    clockid: libc::clockid_t,
}

/// Capture the CALLING thread's CPU-time clock. Must be invoked on the thread
/// whose CPU time should be measured (i.e. the execution thread).
///
/// Returns `None` if the platform refuses to expose a per-thread CPU clock, in
/// which case no CPU budget can be enforced.
#[cfg(unix)]
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn current_thread_cpu_clock() -> Option<ThreadCpuClock> {
    // SAFETY: `pthread_self` is always callable; `pthread_getcpuclockid` writes
    // a valid clockid into `clockid` on success (return 0).
    unsafe {
        let mut clockid: libc::clockid_t = 0;
        let rc = libc::pthread_getcpuclockid(libc::pthread_self(), &mut clockid);
        if rc == 0 {
            Some(ThreadCpuClock { clockid })
        } else {
            None
        }
    }
}

#[cfg(unix)]
impl ThreadCpuClock {
    /// Read accumulated CPU time for the captured thread, in milliseconds.
    /// Returns `None` if the clock read fails.
    #[cfg_attr(test, allow(dead_code))]
    fn elapsed_ms(self) -> Option<u64> {
        // SAFETY: `clockid` came from a successful `pthread_getcpuclockid`; the
        // timespec is fully written by `clock_gettime` on success.
        unsafe {
            let mut ts: libc::timespec = std::mem::zeroed();
            if libc::clock_gettime(self.clockid, &mut ts) == 0 {
                let ms = (ts.tv_sec as i128) * 1_000 + (ts.tv_nsec as i128) / 1_000_000;
                Some(ms.max(0) as u64)
            } else {
                None
            }
        }
    }
}

/// Guard for per-execution TRUE CPU-time budget enforcement.
///
/// Spawns a watchdog thread that polls the execution thread's CPU clock every
/// [`CPU_BUDGET_POLL_INTERVAL`]. When accumulated active-JS CPU time exceeds the
/// budget, it calls `v8::Isolate::terminate_execution()` and signals the
/// execution abort with [`crate::session::ExecutionAbortReason::CpuBudgetExceeded`].
/// A guest that mostly awaits/idles accrues little CPU time and is NOT killed.
///
/// Drop or call `cancel()` to stop the watchdog (execution completed normally).
pub(crate) struct CpuBudgetGuard {
    cancel_tx: Option<crossbeam_channel::Sender<()>>,
    fired: Arc<AtomicBool>,
    join_handle: Option<thread::JoinHandle<()>>,
}

#[cfg(unix)]
impl CpuBudgetGuard {
    /// Spawn the CPU-budget watchdog.
    ///
    /// - `budget_ms`: TRUE CPU-time budget in milliseconds (active JS only)
    /// - `cpu_clock`: the execution thread's CPU clock (captured on that thread)
    /// - `isolate_handle`: V8 isolate handle for `terminate_execution()`
    /// - `execution_abort`: signalled with `CpuBudgetExceeded` when the budget is exhausted
    #[cfg_attr(test, allow(dead_code))]
    pub(crate) fn new(
        budget_ms: u32,
        cpu_clock: ThreadCpuClock,
        isolate_handle: v8::IsolateHandle,
        execution_abort: crate::session::SharedExecutionAbort,
    ) -> Result<Self, String> {
        let (cancel_tx, cancel_rx) = crossbeam_channel::bounded::<()>(1);
        let fired = Arc::new(AtomicBool::new(false));
        let fired_clone = Arc::clone(&fired);

        // Snapshot the thread's CPU time at arm so the budget measures CPU
        // consumed DURING this execution, not cumulative thread lifetime.
        let baseline_ms = cpu_clock.elapsed_ms().unwrap_or(0);
        let budget_ms = budget_ms as u64;

        let handle = thread::Builder::new()
            .name("cpu-budget".into())
            .spawn(move || {
                let ticker = crossbeam_channel::tick(CPU_BUDGET_POLL_INTERVAL);
                loop {
                    crossbeam_channel::select! {
                        recv(cancel_rx) -> _ => {
                            // Cancelled — execution completed normally.
                            return;
                        }
                        recv(ticker) -> _ => {
                            let used = cpu_clock
                                .elapsed_ms()
                                .unwrap_or(baseline_ms)
                                .saturating_sub(baseline_ms);
                            if used >= budget_ms {
                                fired_clone.store(true, Ordering::SeqCst);
                                isolate_handle.terminate_execution();
                                crate::session::signal_execution_abort(
                                    &execution_abort,
                                    crate::session::ExecutionAbortReason::CpuBudgetExceeded,
                                );
                                return;
                            }
                        }
                    }
                }
            })
            .map_err(|error| {
                format!(
                    "{CPU_BUDGET_GUARD_START_ERROR_CODE}: failed to spawn cpu-budget thread: {error}"
                )
            })?;

        Ok(CpuBudgetGuard {
            cancel_tx: Some(cancel_tx),
            fired,
            join_handle: Some(handle),
        })
    }

    /// Cancel the watchdog (execution completed normally). Blocks until the
    /// watchdog thread exits.
    pub(crate) fn cancel(&mut self) {
        self.cancel_tx.take();
        if let Some(h) = self.join_handle.take() {
            let _ = h.join();
        }
    }

    /// Check whether the CPU budget was exhausted.
    #[cfg_attr(test, allow(dead_code))]
    pub(crate) fn exceeded(&self) -> bool {
        self.fired.load(Ordering::SeqCst)
    }
}

#[cfg(unix)]
impl Drop for CpuBudgetGuard {
    fn drop(&mut self) {
        self.cancel();
    }
}

// Non-unix fallback: there is no portable per-thread CPU clock, so the
// CPU-budget watchdog cannot be enforced. `current_thread_cpu_clock` returns
// `None`, which makes the session surface a clear "cannot enforce" error if a
// CPU budget is requested, rather than silently running uncapped.
#[cfg(not(unix))]
#[derive(Clone, Copy)]
pub(crate) struct ThreadCpuClock;

#[cfg(not(unix))]
pub(crate) fn current_thread_cpu_clock() -> Option<ThreadCpuClock> {
    None
}

#[cfg(not(unix))]
impl CpuBudgetGuard {
    pub(crate) fn new(
        _budget_ms: u32,
        _cpu_clock: ThreadCpuClock,
        _isolate_handle: v8::IsolateHandle,
        _execution_abort: crate::session::SharedExecutionAbort,
    ) -> Result<Self, String> {
        Err(format!(
            "{CPU_BUDGET_GUARD_START_ERROR_CODE}: per-thread CPU clock not supported on this platform"
        ))
    }

    pub(crate) fn cancel(&mut self) {}

    #[cfg_attr(test, allow(dead_code))]
    pub(crate) fn exceeded(&self) -> bool {
        self.fired.load(Ordering::SeqCst)
    }
}

/// Guard for per-session CPU timeout enforcement.
///
/// Spawns a timer thread that calls `v8::Isolate::terminate_execution()`
/// and closes the active execution abort channel to unblock any channel-based
/// readers when the timeout elapses. Drop or call `cancel()` to prevent firing.
pub struct TimeoutGuard {
    /// Sender side of cancellation channel — dropped to cancel the timer
    cancel_tx: Option<crossbeam_channel::Sender<()>>,
    /// Set to true when the timeout fired
    fired: Arc<AtomicBool>,
    /// Timer thread handle
    join_handle: Option<thread::JoinHandle<()>>,
}

impl TimeoutGuard {
    /// Spawn a timeout timer thread.
    ///
    /// - `timeout_ms`: wall-clock time limit in milliseconds
    /// - `isolate_handle`: V8 isolate handle for `terminate_execution()`
    /// - `abort_tx`: dropped on timeout to unblock channel readers via `select!`
    pub(crate) fn new(
        timeout_ms: u32,
        isolate_handle: v8::IsolateHandle,
        abort_tx: crossbeam_channel::Sender<()>,
    ) -> Result<Self, String> {
        Self::spawn(timeout_ms, isolate_handle, move || {
            drop(abort_tx);
        })
    }

    /// Spawn a wall-clock backstop that signals the execution abort with
    /// [`crate::session::ExecutionAbortReason::WallClockTimedOut`] when the limit
    /// elapses. Unlike the CPU budget, this counts elapsed real time INCLUDING
    /// idle/await. Armed only when the operator opts in via
    /// `AGENT_OS_V8_WALL_CLOCK_LIMIT_MS`.
    #[cfg_attr(test, allow(dead_code))]
    pub(crate) fn with_execution_abort(
        timeout_ms: u32,
        isolate_handle: v8::IsolateHandle,
        execution_abort: crate::session::SharedExecutionAbort,
    ) -> Result<Self, String> {
        Self::spawn(timeout_ms, isolate_handle, move || {
            crate::session::signal_execution_abort(
                &execution_abort,
                crate::session::ExecutionAbortReason::WallClockTimedOut,
            );
        })
    }

    fn spawn(
        timeout_ms: u32,
        isolate_handle: v8::IsolateHandle,
        on_timeout: impl FnOnce() + Send + 'static,
    ) -> Result<Self, String> {
        let (cancel_tx, cancel_rx) = crossbeam_channel::bounded::<()>(1);
        let fired = Arc::new(AtomicBool::new(false));
        let fired_clone = Arc::clone(&fired);

        let handle = thread::Builder::new()
            .name("timeout".into())
            .spawn(move || {
                let timer = crossbeam_channel::after(Duration::from_millis(timeout_ms as u64));

                crossbeam_channel::select! {
                    recv(timer) -> _ => {
                        // Timeout elapsed — terminate V8 execution
                        fired_clone.store(true, Ordering::SeqCst);
                        isolate_handle.terminate_execution();
                        on_timeout();
                    }
                    recv(cancel_rx) -> _ => {
                        // Cancelled — execution completed normally
                    }
                }
            })
            .map_err(|error| {
                format!("{TIMEOUT_GUARD_START_ERROR_CODE}: failed to spawn timeout thread: {error}")
            })?;

        Ok(TimeoutGuard {
            cancel_tx: Some(cancel_tx),
            fired,
            join_handle: Some(handle),
        })
    }

    /// Cancel the timeout (execution completed normally).
    /// Blocks until the timer thread exits.
    pub fn cancel(&mut self) {
        // Drop the cancel sender to unblock the timer thread's select!
        self.cancel_tx.take();
        if let Some(h) = self.join_handle.take() {
            let _ = h.join();
        }
    }

    /// Check if the timeout fired.
    pub fn timed_out(&self) -> bool {
        self.fired.load(Ordering::SeqCst)
    }
}

impl Drop for TimeoutGuard {
    fn drop(&mut self) {
        self.cancel();
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn timeout_guard_cancel_before_fire() {
        // Timer set to 5 seconds, cancelled immediately — should not fire
        let (abort_tx, abort_rx) = crossbeam_channel::bounded::<()>(0);

        // Create a minimal V8 platform + isolate just for the handle
        // We avoid actual V8 in tests — use a different approach
        // Instead, test the cancellation logic without V8

        // We can't easily get a v8::IsolateHandle without V8 init,
        // so we test the TimeoutGuard flow via integration in execution::tests
        drop(abort_tx);
        drop(abort_rx);
    }

    #[test]
    fn timeout_guard_fires_on_expiry() {
        // Tested via V8 integration tests in execution::tests
        // This placeholder confirms the module compiles correctly
    }
}
