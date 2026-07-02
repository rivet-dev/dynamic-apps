# secure-exec Registry

Software packages for secure-exec VMs: WASM command binaries
(`registry/software/*`), JavaScript agent adapters (`registry/agent/*`), and
tool packages (`registry/tool/*`). Everything under the `@agentos-software/*`
npm scope.

## Consuming packages

```bash
npm install @agentos-software/coreutils @agentos-software/grep
# or a meta-package for a complete set:
npm install @agentos-software/common
```

Each package default-exports a descriptor whose `packageDir` points at the
self-contained runtime dir the sidecar projects under
`/opt/agentos/<name>/<version>` (meta-packages export an array of descriptors):

```typescript
import coreutils from "@agentos-software/coreutils";
import grep from "@agentos-software/grep";

export const software = [coreutils, grep];
```

## Package anatomy

```
registry/software/<pkg>/
├── package.json           name, per-package semver version, build script
├── agentos-package.json   manifest: runtime fields (name/agent/provides) +
│                          staging fields (commands/aliases/stubs)
├── src/index.ts           descriptor: packageDir -> ./package/ (dist/package)
├── bin/                   staged command binaries (gitignored, built)
└── dist/package/          the assembled runtime dir (shipped in the npm tarball):
    ├── package.json       { name, version, bin: { <cmd>: "bin/<cmd>" } }
    ├── agentos-package.json
    └── bin/<cmd>          the binaries, copied verbatim
```

The whole lifecycle is owned by **`@rivet-dev/agentos-toolchain`**
(`packages/agentos-toolchain`) — the same CLI 3rd-party repos use to build and
publish their own agentOS packages (`npx @rivet-dev/agentos-toolchain`):

- `stage --commands-dir <dir>` — populate `bin/` from a compiled commands
  directory, per the `commands` / `aliases` / `stubs` lists in
  `agentos-package.json`.
- `build` — assemble the clean `dist/package/` runtime dir from `bin/`.
- `pack` — build a self-contained node-closure package (JS agents).
- `publish` — publish to npm; dist-tag `dev` by default, `latest` only with an
  explicit `--latest`.

## Building

All recipes run from the repo root (see `justfile`):

```bash
just registry-native            # compile ALL native wasm command binaries (slow, once per checkout)
just registry-native-cmd sh     # recompile ONE command (cargo package cmd-sh)
just registry-build             # stage + assemble every registry package
just registry-build coreutils   # ... or just one
just registry-status            # per-package state; --remote adds npm dist-tags
just registry-test              # registry integration tests (registry/tests)
```

The native build (`registry/native`) compiles each `crates/commands/<name>`
(cargo package `cmd-<name>`) to `wasm32-wasip1` with a patched std
(`-Z build-std`, `patches/`), runs `wasm-opt -O3`, and drops the binaries in
`registry/native/target/wasm32-wasip1/release/commands/`. Package builds then
run `agentos-toolchain stage` (with `--if-missing skip`, so a checkout without
the native build still assembles valid empty placeholders) followed by `tsc`
and `agentos-toolchain build`.

Within this repo, everything consumes the LOCAL builds by default: the registry
packages are pnpm workspace members, so tests and examples resolve them via
`workspace:*` — no publish needed for local development.

Exceptions:
- `software/codex/wasm/` is the install target of the codex fork's build
  (`make -C registry/native codex`); `software/codex-cli` stages from it.
- C-built commands (sqlite3, zip, unzip, wget, duckdb) need the patched
  sysroot (`make -C registry/native/c`); without it those packages stay
  empty placeholders.

## Publishing

Packages **version independently** (per-package semver in each
`package.json`). Publishing NEVER moves the `latest` dist-tag unless asked:

```bash
just registry-publish coreutils            # publish @agentos-software/coreutils under dist-tag `dev`
just registry-publish coreutils my-branch  # ... under a custom tag
just registry-publish coreutils latest     # DELIBERATE release: moves `latest`
just registry-publish-all                  # every built software package, dist-tag `dev`
```

Bump the package's `version` in its `package.json` (commit it) before
publishing. CI does not publish these packages (the publish workflow's package
discovery skips `@agentos-software/*` except the manifest); the agent packages
under `registry/agent/*` preview-publish via `.github/workflows/publish.yaml`
under a branch dist-tag.

agent-os consumes the published packages pinned per-package in its catalog
(`just agentos-pkgs-status` there), and flips to these local checkouts with
`just agentos-pkgs-local`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to add new packages.

## License

Apache-2.0
