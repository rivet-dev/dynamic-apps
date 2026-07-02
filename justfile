set positional-arguments := true

ci:
	bash scripts/ci.sh

# Run the secureexec.dev site (landing + /docs) locally with hot reload
docs:
	pnpm --filter @secure-exec/website dev

# Build the secureexec.dev site to website/dist
docs-build:
	pnpm --filter @secure-exec/website build

# Verify the docs theme matches the Rivet docs (needs `just docs` running).
# Two gates: computed-style token diff (0 mismatches) + content-cloned fixture
# per-component pixel diff (0 components diverge). compare-visual just emits
# side-by-side composites for vision review (not a gate).
docs-verify:
	node website/scripts/compare-docs.mjs
	node website/scripts/compare-fixture.mjs
	node website/scripts/compare-visual.mjs

# --- Registry (@agentos-software/* packages) -------------------------------
# Full flow + package format: registry/README.md.

# Compile ALL native wasm command binaries (slow; needed once per checkout)
registry-native:
	make -C registry/native wasm

# Recompile ONE command binary (cargo package cmd-<CMD>), e.g. `just registry-native-cmd sh`
registry-native-cmd CMD:
	make -C registry/native wasm-cmd CMD="{{ CMD }}"

# Build one registry package (stage bin/ + tsc + assemble dist/package), or all when PKG is empty.
# Bootstrap note: on a fresh checkout the `agentos-toolchain` bin symlinks are
# only created by `pnpm install` AFTER the toolchain's dist exists — so prime it.
registry-build PKG="":
	#!/usr/bin/env bash
	set -euo pipefail
	npx turbo build --filter '@rivet-dev/agentos-toolchain' >/dev/null
	pnpm install --frozen-lockfile >/dev/null
	if [ -n "{{ PKG }}" ]; then
		dir=""
		for base in registry/software registry/agent; do
			[ -d "$base/{{ PKG }}" ] && dir="$base/{{ PKG }}"
		done
		[ -n "$dir" ] || { echo "ERROR: no registry/software/{{ PKG }} or registry/agent/{{ PKG }}"; exit 1; }
		npx turbo build --filter "./$dir"
	else
		npx turbo build --filter './registry/software/*' --filter './registry/agent/*'
	fi

# Run the registry integration tests (registry/tests)
registry-test *args:
	pnpm --dir registry test "$@"

# Publish one software/agent package. Dist-tag defaults to `dev`; pass
# TAG=latest ONLY for a deliberate release (it moves the latest pointer).
registry-publish PKG TAG="dev":
	#!/usr/bin/env bash
	set -euo pipefail
	dir=""
	for base in registry/software registry/agent; do
		[ -d "$base/{{ PKG }}" ] && dir="$base/{{ PKG }}"
	done
	[ -n "$dir" ] || { echo "ERROR: no registry/software/{{ PKG }} or registry/agent/{{ PKG }}"; exit 1; }
	if [ "{{ TAG }}" = "latest" ]; then
		node packages/agentos-toolchain/dist/cli.js publish "$dir" --latest
	else
		node packages/agentos-toolchain/dist/cli.js publish "$dir" --tag "{{ TAG }}"
	fi

# Publish ALL built software packages (skips unbuilt ones with a notice)
registry-publish-all TAG="dev":
	#!/usr/bin/env bash
	set -euo pipefail
	for dir in registry/software/*/; do
		[ -f "$dir/package.json" ] || continue
		if [ ! -f "$dir/dist/index.js" ]; then
			echo "SKIP: $dir (not built)"
			continue
		fi
		if [ "{{ TAG }}" = "latest" ]; then
			node packages/agentos-toolchain/dist/cli.js publish "$dir" --latest
		else
			node packages/agentos-toolchain/dist/cli.js publish "$dir" --tag "{{ TAG }}"
		fi
	done

# Show per-package state (version, staged bin/, assembled dist). --remote adds npm dist-tags.
registry-status *args:
	node registry/scripts/status.mjs "$@"

release *args:
	pnpm --filter=publish release "$@"

# Cut a release-preview (debug build, npm-only, branch dist-tag; also publishes
# the registry packages under that tag) — see the release-preview skill.
release-preview REF:
	gh workflow run .github/workflows/publish.yaml --ref "{{ REF }}"

test-bounded cmd='pnpm test':
	#!/usr/bin/env bash
	set -euo pipefail

	repo_root='{{justfile_directory()}}'
	cmd="${1:-pnpm test}"
	avail_kb="$(awk '/MemAvailable/ {print $2}' /proc/meminfo)"
	cpus="$(nproc --all)"

	if [[ -z "$avail_kb" || -z "$cpus" ]]; then
		echo "Could not determine CPU or memory budget." >&2
		exit 1
	fi

	mem_max_kb=$((avail_kb * 60 / 100))
	mem_high_kb=$((mem_max_kb * 85 / 100))
	cpu_quota="$((cpus * 60))%"

	printf 'Running with CPUQuota=%s MemoryHigh=%sK MemoryMax=%sK\n' \
		"$cpu_quota" "$mem_high_kb" "$mem_max_kb"

	# Resource limits are scoped to the whole transient unit, so test runners and
	# every child process they spawn share the same CPU, memory, IO, and task caps.
	#
	# MemoryHigh starts reclaim/throttling before the hard MemoryMax. MemoryMax is
	# based on currently available memory, not total memory, to avoid host pressure.
	# CPUQuota limits aggregate CPU to 60% of logical cores; CPUWeight and Nice make
	# other work win contention. IOWeight and idle IO scheduling keep large test
	# output/builds from making the host sticky. OOMScoreAdjust makes this bounded
	# run a preferred kill target under pressure, and TasksMax prevents runaway
	# process fan-out.
	exec systemd-run --user --wait --collect --pipe \
		-p MemoryAccounting=yes \
		-p MemoryHigh="${mem_high_kb}K" \
		-p MemoryMax="${mem_max_kb}K" \
		-p MemorySwapMax=0 \
		-p CPUAccounting=yes \
		-p CPUQuota="$cpu_quota" \
		-p CPUWeight=20 \
		-p Nice=10 \
		-p IOWeight=20 \
		-p IOSchedulingClass=idle \
		-p OOMScoreAdjust=500 \
		-p TasksMax=512 \
		bash -lc 'cd "$1" && exec bash -lc "$2"' bounded-test "$repo_root" "$cmd"
