#!/usr/bin/env bash
# Publish the built @agentos-software/* software packages.
#
#   bash registry/publish-software.sh --dry-run   # preview, no upload, no auth needed
#   bash registry/publish-software.sh             # real publish (needs your npm auth)
#
# Publishes 24 of 27 packages at their package.json version with `--tag rc`
# (the version is a prerelease, so npm requires a dist-tag, not `latest`).
# Excludes:
#   wget    - build bug (duplicate getsockname/getpeername vs patched sysroot)
#   duckdb  - skipped (heavy CMake->wasm build)
#   make    - wasm command not built
set -uo pipefail
cd "$(cd "$(dirname "$0")" && pwd)"   # -> registry/

TAG=rc
DRY=""
[ "${1:-}" = "--dry-run" ] && DRY="--dry-run"

if [ -z "$DRY" ]; then
  if ! npm whoami >/dev/null 2>&1; then
    echo "ERROR: not logged in to npm. Run 'npm login' first (needs @agentos-software publish rights)." >&2
    exit 1
  fi
  echo "Publishing as npm user: $(npm whoami)"
fi

ok=0; skipped=""; failed=""
for d in software/*/; do
  n=$(basename "$d")
  case "$n" in wget|duckdb|make|build-essential) skipped="$skipped $n"; continue;; esac
  if [ ! -f "$d/dist/index.js" ]; then skipped="$skipped $n(no-dist)"; continue; fi
  echo "==> $n"
  if ( cd "$d" && pnpm publish --access public --tag "$TAG" --no-git-checks $DRY ); then
    ok=$((ok + 1))
  else
    failed="$failed $n"
  fi
done

echo
echo "Done. Published: $ok   Skipped:${skipped:- none}   Failed:${failed:- none}"
[ -z "$failed" ]
