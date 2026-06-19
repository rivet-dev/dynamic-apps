#!/usr/bin/env bash
#
# Run the Secure Exec cold-start, warm-start, and memory benchmarks against the
# public `secure-exec` SDK, saving timestamped JSON results to results/.
#
# Build a release sidecar first for meaningful timings:
#   cargo build --release -p secure-exec-sidecar
#
# The harness needs an absolute path to a sidecar binary. If
# SECURE_EXEC_SIDECAR_BIN is not already set, this script points it at the
# repo's release build.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"

: "${SECURE_EXEC_SIDECAR_BIN:=$REPO_ROOT/target/release/secure-exec-sidecar}"
export SECURE_EXEC_SIDECAR_BIN

if [[ ! -x "$SECURE_EXEC_SIDECAR_BIN" ]]; then
	echo "ERROR: sidecar binary not found at $SECURE_EXEC_SIDECAR_BIN" >&2
	echo "Build it with: cargo build --release -p secure-exec-sidecar" >&2
	exit 1
fi

RESULTS_DIR="$HERE/results"
mkdir -p "$RESULTS_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"

echo "Sidecar: $SECURE_EXEC_SIDECAR_BIN"
echo "Results: $RESULTS_DIR"
echo

echo "=== cold + warm start ==="
npx tsx "$HERE/coldstart.bench.ts" \
	>"$RESULTS_DIR/coldstart-$STAMP.json" \
	2>"$RESULTS_DIR/coldstart-$STAMP.log"

echo "=== memory ==="
node --expose-gc --import tsx/esm "$HERE/memory.bench.ts" \
	>"$RESULTS_DIR/memory-$STAMP.json" \
	2>"$RESULTS_DIR/memory-$STAMP.log"

echo
echo "Done. Results saved to:"
echo "  $RESULTS_DIR/coldstart-$STAMP.json"
echo "  $RESULTS_DIR/coldstart-$STAMP.log"
echo "  $RESULTS_DIR/memory-$STAMP.json"
echo "  $RESULTS_DIR/memory-$STAMP.log"
