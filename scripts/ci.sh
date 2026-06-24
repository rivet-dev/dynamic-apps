#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ -d /workspace/.cargo && -d /workspace/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin ]]; then
	export CARGO_HOME=/workspace/.cargo
	export RUSTUP_HOME=/workspace/.rustup
	export PATH="/workspace/.cargo/bin:${PATH}"
	export RUSTC=/workspace/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/rustc
	export RUSTDOC=/workspace/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/rustdoc
fi

export CARGO_HTTP_TIMEOUT="${CARGO_HTTP_TIMEOUT:-120}"
export CARGO_NET_RETRY="${CARGO_NET_RETRY:-10}"

run_step() {
	echo ""
	echo "==> $*"
	"$@"
}

run_step pnpm install --frozen-lockfile
run_step pnpm build
run_step pnpm check-types
run_step node --test scripts/check-secure-exec-boundary.test.mjs
run_step node scripts/check-secure-exec-boundary.mjs
run_step pnpm --dir scripts/publish run check-types
run_step pnpm --dir scripts/publish test
run_step cargo fmt --check
run_step cargo clippy --workspace --all-targets -- -D warnings
run_step env CARGO_INCREMENTAL=0 cargo test --workspace -- --test-threads=1

if [[ "${CI_FORK_PULL_REQUEST:-0}" == "1" ]]; then
	run_step pnpm exec turbo run test --concurrency=1
else
	run_step env SECURE_EXEC_E2E_NETWORK=1 pnpm exec turbo run test --concurrency=1
fi
