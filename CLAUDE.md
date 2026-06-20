# secure-exec

secure-exec is the fully virtualized runtime extracted from Agent OS. The kernel provides a POSIX-like VM with a virtual filesystem, process table, socket table, pipes, PTYs, permission policy, and managed language runtimes.

## Trust Model

secure-exec is a sandbox: it runs untrusted code safely for a trusted caller. Decide which side of this boundary something is on before judging whether it is a security bug. Three components:

- **Client** (trusted, *except for anything it submits for execution*). The party that speaks the sidecar wire protocol. The client process and every value it sends are trusted: `CreateVmConfig`, mount descriptors and their plugin configs (host_dir paths, S3 endpoints/credentials, Google Drive, sandbox-agent), the permission policy, network allowlist, resource limits, env, and DNS overrides. Configuration is **not** an attack surface. The one thing from the client that is *not* trusted is the code/payload it asks to run, because that runs in the executor.
- **Sidecar** (trusted; the TCB and the enforcement point). Brokers client requests and owns the kernel, VFS, mount/plugin registry, socket table, and permission policy. It is responsible for enforcing the boundary against the executor.
- **Executor** — V8 isolates or WASM (untrusted; the adversary). Runs guest JS/Python/WASM plus any third-party/npm/agent-generated code. Assume everything here is actively hostile. How code reached the executor never makes it trusted.

**The security boundary is sidecar ↔ executor.** The runtime must stop guest code in the executor from: escaping the kernel boundary (real host fs/network/process/memory), bypassing the *applied* permission policy/allowlist/limits, exhausting host resources beyond configured bounds, or reading another VM's state.

**A defect that requires the client to supply a malicious config/endpoint/credential/policy is NOT a sandbox vulnerability** — the client is configuring its own VM and already controls the host. Treat such hardening as defense-in-depth, not as an escape, and do not add validation that only guards trusted client-provided configuration.

Two corollaries that are easy to get wrong:

- *Trusted policy, untrusted subject.* The permission policy and limits are trusted input, but the guest executor is the subject they bind. "Guest bypasses an applied permission / egress rule / resource cap" is in-scope and serious. Trusted = who sets the rule; untrusted = who is bound by it.
- *Trusted mount, untrusted traffic.* A host-backed mount (host_dir, s3, …) comes from trusted config, so its existence/target/credentials are not attack surface, but the guest drives I/O through it, so confining those guest operations to the mount root (symlink / `..` / TOCTOU / path-aliasing escapes) is in-scope.

**Transport scope.** The wire protocol is same-version lockstep and single-client over stdio (one trusted client per sidecar process). There is no second, mutually-distrusting client, so wire-level authn/authz-between-clients and VM-to-VM access via forged connection ids are out of scope until a multi-client transport exists.

## Runtime Invariants

- All guest code must execute inside the kernel isolation boundary with zero host escapes.
- No runtime may spawn unsandboxed host processes, touch real host filesystems, open real host network sockets, or call real Node.js builtins for guest work.
- Guest JavaScript runs in V8 isolates through `crates/v8-runtime/`; never use `Command::new("node")` for guest execution.
- Every guest syscall goes through kernel-owned VFS, process, socket, pipe, PTY, permission, and DNS paths.
- Present normal Linux semantics to tools. Fix runtime compatibility in secure-exec instead of patching callers around runtime quirks.

## Project Boundaries

- Keep this repo Agent OS-agnostic: no ACP, agents, sessions, `agent-os-protocol`, `agent-os-client`, or `agent-os-sidecar` dependencies in secure-exec code.
- `crates/bridge/` is the browser/native portability seam. Shared contracts belong there.
- `crates/execution/` is the native execution implementation and must not become the browser portability layer.
- `crates/sidecar/` builds the `secure-exec-sidecar` crate and binary. Extension APIs must stay transport-agnostic.
- The protocol has no backwards compatibility. The sidecar and its clients run in same-version lockstep, so never add protocol or config versioning, runtime negotiation, fallbacks, or converters. Wire types and configs such as `CreateVmConfig` carry no `version` field beyond the single same-version handshake. Change the protocol freely and update all sides together.
- Wire/client parity: every capability implemented in the wire protocol must be reachable from BOTH client APIs — the TypeScript client (`NodeRuntime` / `secure-exec`) and the Rust client. When you add or change a protocol capability, expose it on both; never leave wire functionality unreachable from the TS or Rust API.
- Config travels on the **BARE wire/structured request**, not the ambient `AGENT_OS_*` env channel. Classify every setting into three buckets: (1) **process-wide / host / build / test → env** (shared across all VMs, not per-VM configurable); (2) **per-VM bootstrap-before-wire → env carve-out** (must exist at `exec` time before the wire/sync-RPC bridge is up: sandbox root, inherited bridge fds, entrypoint/payload); (3) **per-VM runtime config → BARE wire** (anything per-VM the established wire/bridge could carry — limits, virtualized identity, isolation policy). New per-VM settings default to bucket 3. Migrated so far: resource limits + virtualized identity (`process.*` via the runtime shim, `os.*` via the `__agentOsVirtualOs` global). Isolation policy (guest path mappings, extra fs read/write paths, allowed Node builtins, loopback-exempt ports, WASM permission tier) is still on env — bucket 3 but not yet moved. Anti-pattern: a value carried on the wire then silently re-emitted as an env knob (and maybe never read) is the **dead-cap** failure mode — if it's on the wire, the engine must read it from the wire path, never from a duplicated env var. See `crates/sidecar/CLAUDE.md` for the rule.
- JavaScript host-emulation config (`CreateVmConfig.jsRuntime`) mirrors esbuild's vocabulary so users carry over a known mental model. The host environment presented to guest JS is a `platform`; its values are esbuild's exactly — `node` | `browser` | `neutral` — plus the one sanctioned extension `bare` (language-only: ECMAScript spec globals + WebAssembly, nothing host-provided), for which esbuild has no equivalent. Do not invent other platform names. Wherever a JS runtime/resolution config property has an esbuild equivalent, take esbuild's name and value spelling over any other source (esbuild > tsconfig > ad-hoc); introduce a non-esbuild name only when esbuild has no equivalent concept (e.g. `moduleResolution`, `allowedBuiltins`).
- `packages/core/` is `@secure-exec/core`, the generic TypeScript protocol, client, descriptor, and runtime asset package.
- `packages/build-tools/` is `@secure-exec/build-tools`, the workspace-only generator package for V8 bridge and base filesystem assets.
- Registry software, filesystem, and tool packages live under `registry/` with the `@secure-exec/*` npm scope.

## Build And Assets

- The VM base filesystem artifact is derived from Alpine Linux, but runtime source should stay generic.
- Rebuild the base filesystem with `pnpm --dir packages/build-tools snapshot:alpine-defaults`, then `pnpm --dir packages/build-tools build:base-filesystem`.
- The V8 bridge bundle is generated from `packages/build-tools/scripts/build-v8-bridge.mjs`; keep its generated assets aligned with bridge-contract changes.
- `registry/native` owns the Rust-to-WASM command build; package-local `registry/software/*/wasm/` output is release material.

## npm Compatibility

- npm packages must work unmodified inside the VM. Fix module resolution or polyfills instead of bundling or patching published packages.
- Never hardcode support for specific npm packages in secure-exec. Do not special-case package names (e.g. `minimatch`, `glob`, `undici`) in module resolution, format detection, export synthesis, polyfills, or interop. When a real package exposes a general gap, fix the underlying mechanism so every package benefits; a package-name branch is a sign the real bug is elsewhere. The only allowed name-based handling is for Node.js builtins (e.g. `node:fs`), never third-party packages.
- The VM presents mounted `node_modules` faithfully, like a real filesystem (Docker-style), symlinks included. Module resolution must match naive Node.js resolution over that filesystem — the ancestor `node_modules` walk, `exports`/`imports`/conditions, and `realpath`/symlink following — and nothing more.
- No package-manager-specific resolution heuristics. The resolver must not understand pnpm/yarn internals — e.g. no scanning the `.pnpm` virtual store or guessing a version when no symlink points to it. pnpm/yarn layouts resolve because the VFS exposes their symlinks, not because the resolver special-cases them. If a real layout fails, fix VFS symlink fidelity (or `exports`/conditions handling), never add a layout-aware shortcut.
- Use `node-stdlib-browser` for pure-JS builtins and bridge-backed polyfills for kernel-backed modules such as `fs`, `net`, `child_process`, `dns`, `http`, `os`, and `crypto`.
- Guest `fetch()` must run through undici inside the V8 isolate, then through the kernel socket table.

## Native Binary Distribution

- Ship `secure-exec-sidecar` through `@secure-exec/sidecar` plus platform packages declared as optional dependencies.
- Publish platform binary packages with `npm publish`, not `pnpm publish`, so executable bits are preserved.
- Resolver packages must return an absolute binary path. Callers pass that typed path to process spawning instead of relying on global environment mutation.

## Website

- `website/` is `@secure-exec/website`, a unified Astro app serving the public site at **secureexec.dev**: `/` is the React/Tailwind landing page and `/docs/*` is the Starlight documentation.
- The docs theme matches the Rivet docs (rivet.dev) 1:1: light-only "porcelain" palette, Manrope + JetBrains Mono, dark code blocks.
- Docs prose must not use em dashes (`—`). Rephrase with commas, colons, parentheses, or separate sentences. This applies to all content under `website/src/content/docs`.
- Every substantial documentation code block must be backed by a runnable example package under `examples/docs/*` that is verified end-to-end (it actually runs against the sidecar), with the doc code copied verbatim from that example and a `*[See Full Example](…)*` link to it. Tiny illustrative snippets (a line or two showing a signature or option shape) are exempt; anything a reader could copy and run should come from a real, passing example. Keep the doc block and the example in sync.
- Docs styling is owned by the shared **`@rivet-dev/docs-theme`** repo (`github.com/rivet-dev/docs-theme`), consumed via `github:rivet-dev/docs-theme#<tag>` and wired in via `...docsTheme(starlight, siteConfig)` in `astro.config.mjs`. **To change any docs styling** (header, sidebar, code blocks, fonts, palette), edit that repo and follow its CLAUDE.md release workflow — never restyle docs in `website/src`. To change *this site's* identity/nav/sidebar/landing, edit `website/docs.config.mjs` (sidebar icons via each item's `attrs['data-icon']`). Re-test with `pnpm --dir website build`.

## Version Control (jj)

- This checkout is jj colocated (jj over git). Prefer `jj` for commits/branches; avoid `git commit`/`git checkout`, which fight jj's working-copy commit.
- Large pyodide/sandbox assets exceed jj's default snapshot size limit. Commit them with `jj --config snapshot.max-new-file-size=16777216 ...`, or gitignore them.

## CLAUDE.md Convention

- Every directory with `CLAUDE.md` must also have `AGENTS.md` as a symlink to `CLAUDE.md`.
- Keep CLAUDE entries concise and limited to design constraints, invariants, and non-obvious rules.
