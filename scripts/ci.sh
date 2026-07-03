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

# DEBUG DRAFT — DO NOT MERGE: minimal repro of the core vitest CI timeouts.
run_step pnpm install --frozen-lockfile
run_step pnpm exec turbo run build --filter=@secure-exec/core...
run_step cargo build -p secure-exec-sidecar
run_step make -C registry/native wasm
run_step node packages/core/scripts/copy-wasm-commands.mjs
cd packages/core
run_step pnpm exec vitest run --no-file-parallelism tests/mount-fs-custom-vfs.test.ts tests/node-runtime-exec-output.test.ts
exit 0
