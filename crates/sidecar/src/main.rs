fn main() {
    // Default to WARN so near-limit / backpressure warnings actually surface
    // (they were swallowed at ERROR-only); operators can tune via SECURE_EXEC_LOG
    // (e.g. `error` to quiet, `debug` for queue snapshots). Logs MUST go to stderr:
    // stdout is the framed wire-protocol channel, so logging there would corrupt it.
    let level = std::env::var("SECURE_EXEC_LOG")
        .ok()
        .and_then(|value| value.parse::<tracing::Level>().ok())
        .unwrap_or(tracing::Level::WARN);
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_max_level(level)
        .init();
    if let Err(error) = secure_exec_sidecar::stdio::run() {
        tracing::error!(?error, "secure-exec-sidecar startup failed");
        std::process::exit(1);
    }
}
