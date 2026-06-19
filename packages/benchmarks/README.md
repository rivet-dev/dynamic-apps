# Secure Exec Benchmarks

These benchmarks measure the public `secure-exec` SDK paths used by consumers.

## Cold Start Matrix

`coldstart.bench.ts` writes machine-readable JSON to stdout and human-readable progress to stderr. It measures:

- `owned-sidecar`: `NodeRuntime.create()` owns a fresh sidecar for each runtime.
- `shared-sidecar`: a `Sidecar` is created once per batch and passed to `NodeRuntime.create({ sidecar })`; sidecar setup is measured separately and excluded from cold start.
- `resident-runner`: a shared sidecar plus `runtime.createResidentRunner()`, so repeated tiny snippets reuse one live guest Node process.

The result JSON includes hardware metadata, aggregate cold/warm latency, and phase timings such as `session_open`, `vm_create`, `runtime_mount_wasm`, `first_exec`, and resident-runner phases.

## Run

Build a release sidecar first for meaningful timings:

```bash
cargo build --release -p secure-exec-sidecar
```

Run the full benchmark suite:

```bash
pnpm --dir packages/benchmarks bench
```

This writes timestamped files under `packages/benchmarks/results/`:

```text
coldstart-YYYYMMDD-HHMMSS.json
coldstart-YYYYMMDD-HHMMSS.log
memory-YYYYMMDD-HHMMSS.json
memory-YYYYMMDD-HHMMSS.log
```

Run only the cold-start matrix:

```bash
SECURE_EXEC_SIDECAR_BIN="$PWD/target/release/secure-exec-sidecar" \
	pnpm --silent --dir packages/benchmarks bench:coldstart \
	> packages/benchmarks/results/coldstart-local.json \
	2> packages/benchmarks/results/coldstart-local.log
```

Quick smoke run:

```bash
BENCH_BATCH_SIZES=1 \
BENCH_ITERATIONS=1 \
BENCH_WARMUP=0 \
BENCH_SCENARIOS=owned-sidecar,shared-sidecar,resident-runner \
SECURE_EXEC_SIDECAR_BIN="$PWD/target/release/secure-exec-sidecar" \
	pnpm --silent --dir packages/benchmarks bench:coldstart
```

Useful knobs:

```text
BENCH_BATCH_SIZES=1,10,50,100,200
BENCH_ITERATIONS=5
BENCH_WARMUP=1
BENCH_SCENARIOS=owned-sidecar,shared-sidecar,resident-runner
BENCH_MAX_LIVE_RUNTIMES=8
BENCH_MAX_RESIDENT_RUNNERS=1
BENCH_EXEC_TIMEOUT_MS=30000
SECURE_EXEC_SIDECAR_BIN=/abs/path/to/secure-exec-sidecar
```

## Checked-In Results

Current captured results:

- `results/coldstart-final.json`
- `results/coldstart-final.log`
- `results/coldstart-resident-full-matrix-20260619.json`
- `results/coldstart-resident-full-matrix-20260619.log`

`coldstart-final.*` is the latest full run from June 19, 2026. It includes the three SDK scenarios above. It also contains an `isolate-only` reference row from a lower-level one-off V8 snapshot/restore benchmark; that row is captured for comparison but is not part of the normal SDK benchmark command.
