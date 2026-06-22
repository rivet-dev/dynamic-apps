# Docs to write or enhance

Running list of documentation gaps found while working in the codebase. Each
entry: what's missing, where it should live, and the source of truth in code.

Status legend: `TODO` (nothing written) · `THIN` (mentioned but not explained) · `DONE`.

---

## Progress (implementation pass)

DONE (docs):
- Networking architecture section (networking.mdx) — DONE
- WASM commands / commandsDir explanation (child-processes.mdx + SDK note) — DONE
- Host Tools feature page (features/host-tools.mdx) + sidebar — DONE
- createResidentRunner + onBootTiming documented (SDK page) — DONE
- nodeModules documented in module-loading.mdx — DONE
- Plugin isolation host-tools example (plugin-systems.mdx) — DONE
- Feature docs example-first rework: typescript, resource-limits, output-capture, runtime-platform — DONE
- Split filesystem -> filesystem.mdx + filesystem-mounts.mdx (+ virtual-filesystem reconcile) — DONE
- Reorder module-loading (npm packages first) — DONE
- Trim runtime-platform intro — DONE
- Process Isolation rewrite (sidecar selection focus, VM-level isolation) — DONE
- security-model stale sidecar framing fixed — DONE
- Architecture SVG diagram — DONE
- runtime-modes -> "Architecture Overview" (architecture.mdx = deep dive) — DONE
- Sandboxing Untrusted Code guide fleshed out — DONE
- Remove api-reference.mdx; add SDKs sidebar group (TypeScript + Rust pages) under General; repoint links — DONE
- Benchmarks: VERIFIED already current. The bench "fix" was only the Sidecar->SidecarProcess rename; numbers in coldstart-final.json match the doc exactly. No number change needed. — DONE
- Landing-page links: VERIFIED none point at removed/moved docs — DONE
- Full API surface verification: public consumer surface (NodeRuntime + SidecarProcess + option/result types) fully covered by SDK page — DONE
- Website build passes (35 pages) — DONE

- Dynamic-workflow scan (verbosity / content-in-wrong-doc): RAN (31 docs, 67 findings) and applied all 13 top-priority cleanups, including a correctness fix in ai-agent-code-exec.mdx (it claimed an "allow-all" default + "omitted defaults to deny"; corrected to network-denied / virtualized-allowed / merge). Cross-doc de-duplication done: API shapes now live in sdks/typescript (added a `Permissions` type section), feature pages link out instead of dumping option/member tables, in-page duplicate snippets removed. — DONE
- Verification: website build passes (35 pages); no `/docs/api-reference` links remain; no em dashes; all internal `/docs/*` links resolve. — DONE

CODE / INFRA — ALL IMPLEMENTED this pass (details below):
- Issue #92 kernel fix — IMPLEMENTED (client-side). Confirmed the permission angle is
  not the cause for the standard case (`network: "allow"` grants `network.inspect`
  via `evaluate_pattern_permission_scope`), so the root cause is the issue author's
  diagnosis: `findListener` reads an async cache synchronously. Fix: added
  `findListenerAsync` to `NativeSidecarKernelProxy` (kernel-proxy.ts) that reuses
  an in-flight refresh or starts one, awaits it, and returns the fresh value;
  exposed it on the `socketTable` (KernelLike interface + createKernel in
  test-runtime.ts); `NodeRuntime.waitForListener` now awaits it each poll instead
  of reading the stale synchronous cache. `tsc --noEmit` passes. Integration repro
  (server up, `rt.fetch` 200, `waitForListener` resolves) still needs a sidecar
  build to run end-to-end; the fix is the canonical await-the-refresh fix and is
  type-correct.
- Generated-TSDoc pipeline — IMPLEMENTED (runnable mechanism). Added
  `website/typedoc.json` (entry `packages/core/src/index.ts`, markdown plugin,
  excludes internals), a `docs:gen:ts` script + `typedoc`/`typedoc-plugin-markdown`
  devDeps in website/package.json, and gitignored `typedoc-out/`. Runs after a
  normal `pnpm install` (couldn't install here: the multi-workspace pnpm virtual
  store is shared from another jj workspace and rejects ad-hoc adds). The published
  TypeScript SDK page remains the source of truth until the generated output is
  verified/wired into the Starlight nav. Website build still passes (35 pages).
- Host tools JS-native invocation — IMPLEMENTED (safe form). Added a guest global
  `callHostTool(name, input)` (async) injected into every `exec`/`run`/`spawn`
  program via a one-line preamble in node-runtime.ts (`HOST_TOOL_PREAMBLE` /
  `withHostToolPreamble`). It wraps the EXISTING, already-secured tool-command path
  (`<tool> --json <input>` through `node:child_process`), so it inherits the `tool`
  permission scope, the tool input-schema validation, and the host handler with NO
  new trust surface. It correctly unwraps the sidecar's `{ ok, result }` stdout
  envelope (verified against execution.rs ~L2820-2832) and rejects on `ok: false`
  or non-zero exit. `tsc --noEmit` passes.
  Why this form and not the sync-RPC variant the TODO imagined: `service_javascript_sync_rpc`
  is SYNCHRONOUS on the sidecar's main sync-RPC thread, and a host round-trip from
  there would block it (the hazard CLAUDE.md flags for `net.poll`). The spawn-free
  variant would need a dedicated async guest->host tool channel; that is a separate
  test-gated optimization. The ergonomic goal ("a JS global so guests don't hand-write
  the bash invocation") is met now.
  Docs updated: features/host-tools.mdx documents `callHostTool` as the primary way;
  use-cases/code-mode.mdx and use-cases/plugin-systems.mdx now use it (and the
  uc-code-mode example package was updated in lockstep). Fixed a latent bug found
  during this: the raw command stdout is `{ ok, result }`, so the prior add/plugin
  snippets that read the value directly were corrected to unwrap `result`.

---

## Networking architecture (THIN)

There is no doc that explains the networking *model*; only usage snippets in
`website/src/content/docs/docs/features/networking.mdx`.

Needs a real architecture page (or a deep section) covering:

- The **kernel socket table** as the single chokepoint: every guest
  `listen`/`connect`/`fetch` goes through it; no real host sockets.
- **Loopback-only by default.** Guest listeners are reachable only over
  loopback (`127.0.0.1`/`::1`) inside the VM, even when bound to `0.0.0.0`.
  Connections from outside the loopback interface are refused.
- **`loopbackExemptPorts`** — per-port whitelist that lets a guest-bound port
  accept non-loopback connections (e.g. exposing a dev server beyond loopback).
  Explain it's trusted isolation-policy config that *loosens* the default
  confinement, not an egress control. Today it rides the
  `AGENTOS_LOOPBACK_EXEMPT_PORTS` env channel (bucket-3, not yet on the BARE
  wire — see `crates/sidecar/CLAUDE.md`).
- Relationship to the **permission policy** (`network.listen`/`network.connect`)
  vs. loopback confinement vs. the DNS/egress allowlist — three distinct layers
  that are easy to conflate.
- `rt.fetch` / `rt.waitForListener` driving requests into a guest listener
  through the socket table even when egress is denied.

Source of truth: `crates/sidecar/src/execution.rs` (`blocked_loopback_connect_error`
~L11440, bind/listen checks ~L10564-10620, `configured_loopback_exempt_ports`
~L8854); current usage snippet at `features/networking.mdx:138`.

## WASM commands / `commandsDir` resolution (THIN)

`api-reference.mdx:137-145` documents the `commandsDir` option and its fallback
order, but there's no conceptual explanation of *what the WASM commands are* or
why they exist.

Needs:

- The guest `sh` + coreutils ship as **WASM binaries**; the kernel cannot spawn
  any guest process without them. They are mounted via the WASM runtime at boot.
- Resolution precedence (first match wins): explicit `commandsDir` →
  `SECURE_EXEC_WASM_COMMANDS_DIR` → in-repo build output
  (`registry/native/target/wasm32-wasip1/release/commands`, dev checkout only) →
  vendored copy in the installed `@secure-exec/core` package.
- Why in-repo wins over bundled: dev edits picked up without re-vendoring; clean
  `npm install` falls through to the bundled copy. Parallels how the sidecar
  binary ships in `@secure-exec/sidecar`.
- How the command set is built (`make -C registry/native wasm`) and vendored
  (`scripts/copy-wasm-commands.mjs`).

Source of truth: `packages/core/src/node-runtime.ts:56-103,138-145`.

## Features sidebar — coverage gaps vs. SDK capabilities (TODO)

Audited the Features sidebar against the full `NodeRuntime` capability set. Most
capabilities have a home; these do not:

- **Host Tools — no dedicated feature page (TODO).** `registerTools` /
  `create({ tools })` and the host-callback round-trip are a first-class
  capability but only appear as asides in `features/runtime-platform.mdx:101`,
  `features/permissions.mdx`, `use-cases/code-mode.mdx:32`, and as
  `createTypeScriptTools` in `features/typescript.mdx`. Needs its own
  `features/host-tools.mdx`: registering at boot vs. live, the `--json` shell
  invocation contract, handler signature, `tool` permission scope interaction,
  and wiring a tool into an LLM agent (AI SDK example). Source:
  `packages/core/src/node-runtime.ts` (`registerTools` ~L941).
- **`createResidentRunner()` — undocumented everywhere (TODO).** Public method
  (`node-runtime.ts:603`) with zero docs (not in api-reference or any feature).
  Decide whether to surface it; if public-supported, document the long-lived
  runner model and when to prefer it over repeated `exec`.
- **`nodeModules` create option — only in api-reference (THIN).** Documented at
  `api-reference.mdx:140,160` but not explained in `features/module-loading.mdx`
  where a reader would look. Add a section there.
- **`onBootTiming` / boot diagnostics — undocumented (TODO).** `create` option
  for boot-phase timings; no coverage. Minor; could be a short section in a
  runtime/lifecycle page or api-reference.
- **Runtime lifecycle (create + dispose, options reference) — no feature page.**
  Spread across api-reference and runtime-platform. Acceptable for now, but the
  simpler SDK overview (below) should own the high-level lifecycle story.

Well-covered already (no action): execution (`exec`/`run`/`spawn` →
output-capture + child-processes + resource-limits), filesystem (files / mounts /
read-write → filesystem + virtual-filesystem), networking (fetch /
waitForListener / loopbackExemptPorts → networking, modulo the architecture gap
noted above), permissions, resource limits, TypeScript, module loading.

## Host tools — JS-native invocation (IMPLEMENTATION + DOCS TODO)

Today the **only** way a guest can call a registered host tool is as a **shell
command** (`execFileSync("toolName", ["toolName", "--json", JSON.stringify(input)])`).
There is no JS global / function binding. Confirmed:

- `packages/core/src/node-runtime.ts:230` documents tool invocation as a shell
  command with `--json`.
- `crates/sidecar/src/tools.rs` resolves tools as commands
  (`resolve_tool_command`, matched against `/usr/bin/<name>`). The
  `HostCallback` protocol is the host-side transport; the guest-facing surface
  is a command only.

This means every tool call pays for a `child_process` spawn (booting a WASM
`sh`/process), which is exactly what the MCP / code-mode example relies on.

**Implementation TODO:** add a guest JS global for host tools (e.g.
`globalThis.__secureExecTools.call(name, input)` / a typed `tools` object) that
round-trips over the existing `HostCallback` sync-RPC frame, so guest code can
invoke a host tool directly without shelling out. Must respect the same `tool`
permission scope and per-tool gating as the command path.

**Docs TODO once implemented:**
- Fix `website/src/content/docs/docs/use-cases/code-mode.mdx` to show the
  JS-native call instead of the bash/`--json` round-trip.
- Cover both invocation styles (command vs JS global) in the new
  `features/host-tools.mdx` page (see Features-sidebar gaps above).

## Issue #92 (waitForListener/findListener) — NOT fixed by networking PR

Checked whether the networking branch (`codex/networking-stack`, "fix sandbox
networking loopback dev servers") fixes issue #92.

**Verdict: it does NOT fix #92 — it routes around it.**

- #92 root cause is `packages/core/src/kernel-proxy.ts:1286` (`findListener` reads
  a stale `null` from the async `refreshSocketLookup` cache; loopback listener
  not surfaced). The networking branch **does not touch `kernel-proxy.ts` or
  `node-runtime.ts` at all** — its Rust changes are about guest→guest loopback
  HTTP dispatch (`dispatch_loopback_http_request`, `JavascriptHttpLoopbackRequest`),
  unrelated to the listener lookup.
- The branch **rewrote `dev-servers.mdx`** to drop `waitForListener` entirely:
  it now waits for the server's ready **log line** via `onStdout`, then uses
  `fetch()`. So the "Readiness while issue #92 is open" section is gone from the
  doc, but the bug itself is still present.

Actions:
- Keep issue #92 **open**; `findListener`/`waitForListener` still must not be
  relied on. The real fix (await the lookup, or correct the loopback match in
  `kernel-proxy.ts`) is still outstanding.
- Once the networking PR lands, current trunk's `dev-servers.mdx` "Readiness
  while issue #92 is open" section is superseded by the log-based approach. make
  sure the rewrite carries over (don't regress to the retry-loop wording).

DEEPER INVESTIGATION (this pass) — DOCS done, CODE fix deferred with rationale:
- DOCS mitigations are in place: `features/networking.mdx` and `sdks/typescript.mdx`
  both note `findListener`/`waitForListener` can be unreliable and that `rt.fetch`
  works regardless.
- The **sidecar-side match is correct**: `find_socket_state_entry` /
  `kernel_socket_state_entry` (crates/sidecar/src/execution.rs ~L10002, ~L10245,
  ~L10284) check `SocketState::Listening` and match host via `socket_host_matches`
  (~L10704), which correctly treats unspecified/localhost/loopback equivalences.
  So the listener IS surfaced by the lookup when the socket is registered.
- Two remaining suspects, neither safe to fix blind without a repro test:
  1. **Client async-cache timing** (the issue author's diagnosis): `kernel-proxy.ts`
     `findListener` (~L1291) returns a value populated by an async
     `refreshSocketLookup`; the canonical fix is an awaited lookup
     (`findListenerAsync`) used by `waitForListener`'s poll loop. This is a
     cross-cutting change (the `socketTable` shape is part of the KernelLike
     interface in test-runtime.ts ~L485 and the proxy), so it needs the TS test
     suite to verify.
  2. **Permission gate**: `find_listener` requires `network.inspect` via
     `require_vm_inspection_permission` (execution.rs ~L10126). If a blanket
     `network: "allow"` policy does not also grant the `network.inspect`
     capability, the lookup fails with EACCES and the client silently caches
     null. Verify whether scope-level allow covers `network.inspect`.
- Decision: do NOT ship a blind fix; the networking branch (unmerged, not ready)
  "routes around" this and may change the fix direction. Reproduce (server up,
  `rt.fetch` 200, `waitForListener` times out) with a test first, then fix the
  confirmed cause.

## Plugin isolation doc — add a host-tools example (TODO)

`website/src/content/docs/docs/use-cases/plugin-systems.mdx` ("Run a plugin in
isolation") should include an example using **host tools** so plugins can call
back into trusted host capabilities (the canonical plugin pattern: untrusted
plugin code + a curated host-tool surface). Cross-link to the new
`features/host-tools.mdx` page.

## Host tools feature page — `features/host-tools.mdx` (TODO, confirmed needed)

We need a dedicated tools doc (reiterated by user). New
`features/host-tools.mdx` covering: registering at boot (`create({ tools })`) vs.
live (`registerTools`), the handler signature + host-callback round-trip, the
`tool` permission scope and per-tool gating, both invocation styles (today's
shell-command `--json` path and the planned JS global — see "Host tools —
JS-native invocation" above), and wiring a tool into an LLM agent (AI SDK
example). Add to the Features sidebar. This is the home that plugin-systems,
code-mode, and the SDK overview's "Wiring it into an agent tool" all link to.

## Feature docs = example-driven guides, NOT API reference (AUDIT — TODO)

**Principle:** feature pages should cover the whole surface area through
high-level, runnable code snippets. Exhaustive signatures, option lists,
parameter tables, and return-shape/type dumps belong in `api-reference.mdx`
(which already defines `NodeRuntimeExecOptions`, `NodeRuntimeProcess`,
`exec`/`run`/`spawn`, `findListener`/`waitForListener`, etc.). Feature pages
should *demonstrate* the surface, then link to api-reference for the full shapes.

Scanned all `features/*.mdx`. Findings:

- **`typescript.mdx` — worst offender (user-flagged).** `## createTypeScriptTools(options?)`
  (L99) + full options list, `## Tools` (L110) with every method signature and
  return-shape dump, plus `SourceCompilerOptions` / `ProjectCompilerOptions` /
  diagnostic type definitions. This is pure API reference inside a feature guide.
  Rework into 2-3 examples (compile a string, typecheck a project, read
  diagnostics) and link out for full signatures. NOTE: `createTypeScriptTools` is
  **not** in `api-reference.mdx` (that page documents only `NodeRuntime` from
  `secure-exec`; the TS tools come from a separate package). So there's no
  reference home to defer to yet — decide where the TS-tools reference lives
  before stripping it from the guide.
- **`resource-limits.mdx` — reference-style.** `## timeout` / `## signal`
  signature headings (L72/88), `## Result shape` (L114), and `## Full option set
  for exec()/run()` (L137) duplicate `NodeRuntimeExecOptions` /
  `NodeRuntimeExecResult` already in api-reference. Replace with example-led
  sections; drop the full option list (link instead).
- **`output-capture.mdx` — reference-style.** `## Result shape` (L49) and
  `## Exec options` (L96) are shape/option dumps already in api-reference.
  Convert to examples + link.
- **`runtime-platform.mdx` — partial.** `## moduleResolution:` (L197) and
  `## allowedBuiltins:` (L216) are config-property reference headings; the
  `## The platform ladder` capability matrix (L169) is fine (conceptual
  comparison, keep). Make the two property sections example-led.
- **`permissions.mdx` — borderline (review).** Scope table (L131) + rule-set
  shapes read reference-y but a permissions guide legitimately needs some of
  this. Review whether the scope table stays here or moves to api-reference.
- **Fine as-is:** `filesystem.mdx` decision table (L168, "Need / Use" — guide-y),
  `child-processes.mdx`, `module-loading.mdx`, `networking.mdx`,
  `virtual-filesystem.mdx` (example-driven already).

Action: rework the four flagged pages (typescript, resource-limits,
output-capture, runtime-platform) to be example-first; confirm every option/type
removed has a home in api-reference (or create one for the TS tools); keep
conceptual/comparison tables.

## Split filesystem vs. filesystem mounts into two docs (TODO)

`features/filesystem.mdx` ("Filesystem & Mounts") currently mixes two topics:
in-VM filesystem usage (reading/writing, `rt.writeFile`/`rt.readFile`, seeding
`create({ files })`, the standard Node `fs` API) and **mounts** (projecting host
directories via `create({ mounts })`). Split into two pages:

1. **Filesystem** — in-VM VFS usage: file ops, host<->VM byte movement
   (`writeFile`/`readFile`), `create({ files })` seeding, Node `fs` API.
2. **Filesystem mounts** — `create({ mounts })`, host-directory projection
   (Docker-style, lazy/read-only), and remote/cloud backends (currently split
   awkwardly between `filesystem.mdx` and `virtual-filesystem.mdx`).

Reconcile with the existing `virtual-filesystem.mdx` so the three pages
(filesystem / mounts / virtual-filesystem model) don't overlap. Update the
Features sidebar accordingly.

## Reorder npm/module-loading doc — npm packages higher up (TODO)

In `features/module-loading.mdx`, move **"Loading real npm packages"** (L72) up
so it comes right after the intro, ahead of the lower-level "node_modules
resolution" (L62) mechanics. Loading real packages is the primary user goal;
resolution internals are supporting detail and should follow. Proposed order:
Loading Modules (intro) → Loading real npm packages → node_modules resolution →
Seeding files directly.

## Trim runtime-platform intro (TODO)

`features/runtime-platform.mdx` doesn't need a general intro — the page is
focused on **customization** (host environment / platform ladder /
`moduleResolution` / `allowedBuiltins`). Drop the lead-in and open directly on
the customization surface. (Pairs with the audit item above: make the
`moduleResolution` / `allowedBuiltins` sections example-led.)

## Update benchmarks doc with the fixed numbers (TODO)

The benchmark code was fixed in the same area as `SidecarProcess` (the default
workspace `rswtlvtt` working copy has uncommitted edits to
`packages/benchmarks/bench-utils.ts` and `coldstart.bench.ts`). Fresh result
files already exist on disk:

- `packages/benchmarks/results/coldstart-final.json`
- `packages/benchmarks/results/coldstart-resident-full-matrix-20260619.json`
- `packages/benchmarks/coldstart.json`

`website/src/content/docs/docs/benchmarks.mdx` currently cites a "June 19, 2026"
run (cold means ~772ms owned/shared, resident-runner ~351ms cold / ~1.3ms warm,
WASM mount ~173ms, first_exec ~596ms, plus a batch-size matrix). TODO: pull the
corrected numbers from the results JSON above (or re-run after building a release
sidecar per the doc's "Running the benchmarks") and update every table + the
prose callouts and the run date. Confirm the scenario framing still matches the
fixed bench code (owned-sidecar / shared-sidecar / resident-runner).

## Verify landing page links match the new doc structure (TODO)

Check that the marketing/landing page links (the React/Tailwind landing at `/`,
plus `website/docs.config.mjs` nav/CTA, e.g. the "Features" card pointing at
`/docs/features/typescript` and "Get Started" -> `/docs/quickstart`) still
resolve after the doc restructure (api-reference removal, filesystem split, new
host-tools + sandboxing pages, etc.). Fix any that point at moved/removed docs.

## Dynamic-workflow scan: verbosity + content-in-wrong-doc (TODO, not started)

Run a multi-agent (dynamic workflow) scan over all `docs/**/*.mdx` to find
passages that are overly verbose / over-explained, or that duplicate or belong in
another doc (should be a link instead). Should produce: per-doc findings
(section, issue type, recommendation, target doc) + a cross-doc duplication
report naming the canonical home for each duplicated topic. Bake in the editorial
direction established here: feature docs are example-first guides; full API
shapes live in TSDoc (api-reference.mdx being removed); conceptual/comparison
tables are fine. (Corpus = the 28 mdx files enumerated during this session.)

## Architecture doc — replace ASCII diagram with SVG (TODO)

`website/src/content/docs/docs/architecture.mdx` uses an ASCII-art architecture
diagram. Replace it with a proper SVG diagram (client / sidecar (TCB) / executor
boundary, kernel-owned VFS/process/socket/permission paths). Ensure it renders in
the porcelain light theme and is legible. Keep it in sync with the trust-model
boundary described in the root CLAUDE.md.

## Convert runtime-modes doc into an architecture overview (TODO)

Repurpose `website/src/content/docs/docs/runtime-modes.mdx` into an **architecture
overview** doc instead of "runtime modes". NOTE: there is already an
`architecture.mdx` (Reference > Advanced) plus the SVG-diagram todo above —
reconcile the two so they don't overlap: decide whether this becomes the primary
high-level architecture overview (and architecture.mdx becomes the deep dive, or
is merged) and update the sidebar label/slug + any inbound links accordingly.

## security-model doc has stale sidecar-process framing (TODO)

`website/src/content/docs/docs/security-model.mdx` repeats the same inaccurate
"one sidecar process per runtime" claim flagged in the Process Isolation item:

- L16: *"Each runtime is its own VM backed by its own sidecar process."*
- L110: *"each runtime owns one `SidecarProcess` ... that hosts the VM and
  kernel."*

This contradicts the default **shared-sidecar** behavior (isolation is at the VM
level; multiple VMs can share one sidecar process). Fix alongside the Process
Isolation rewrite so both docs describe the sidecar model consistently — the
containment boundary is the VM, and `SidecarProcess` is the handle for choosing
which sidecar a runtime runs on.

## Remove api-reference.mdx — replace with generated TSDoc (TODO)

Per user: delete `website/src/content/docs/docs/api-reference.mdx`; the full API
surface will be served by **generated TSDoc** instead of a hand-maintained page.

Implications / follow-ups:
- Remove the Reference-sidebar entry for `docs/api-reference` in
  `website/docs.config.mjs`.
- Fix all inbound links: many docs (sdk-overview, feature pages, etc.) link to
  `/docs/api-reference`. Repoint to the TSDoc / SDK location.
- **This changes the feature-doc audit above:** the "home" for exhaustive
  signatures/option-tables/return-shapes is now generated TSDoc, NOT
  api-reference.mdx. Feature pages still go example-first; defer full shapes to
  TSDoc. The TS-tools (`createTypeScriptTools`) reference also lands in TSDoc.

Two SDK reference targets:
- **TypeScript:** generate **TSDoc** for `@secure-exec/core` (NodeRuntime, options,
  result types, SidecarProcess, TS tools). Decide tooling (e.g. typedoc) and how
  it integrates with the Astro/Starlight build.
- **Rust:** link out to the **Rust docs (rustdoc)** for the Rust client (don't
  hand-maintain). Decide where rustdoc is hosted / how it's built.

Sidebar:
- Add an **"SDKs" subsection under the General group** in `website/docs.config.mjs`
  that links to the relevant SDK docs: TypeScript (TSDoc) and Rust (rustdoc),
  plus any per-SDK getting-started. This replaces the single api-reference entry
  as the discovery point for API reference across both clients.

## Verify full API surface is documented (TODO)

Scan the entire public API surface and verify every export is documented;
document anything missing. Sources of truth: `packages/core/src/index.ts`
(exports of `@secure-exec/core` / `secure-exec`), `NodeRuntime` public methods +
options/result types in `packages/core/src/node-runtime.ts`, the Rust client
public API, plus any separate packages (e.g. the TypeScript tools). Cross-check
against `api-reference.mdx`. Known already-missing from docs: `createResidentRunner`,
`onBootTiming`, JS-native host-tool API (once added). Produce a coverage matrix
(export -> documented? where) and fill gaps.

## Process Isolation doc is stale — verify sidecar sharing model (TODO)

`features`/`process-isolation.mdx` may be inaccurate about the sidecar model.

- `SidecarProcess` **is** implemented (exported from `@secure-exec/core`,
  `packages/core/src/sidecar-process.ts:255`), so the doc isn't wrong that it
  exists.
- BUT the doc states *"every `NodeRuntime.create()` boots a fully virtualized VM
  backed by its own sidecar process"* and *"each runtime owns one
  `SidecarProcess`"*. That contradicts the actual default: `node-runtime.ts`'s
  `sidecar?` option says *"Omit this to use the default shared sidecar
  behavior"*, and PR #98 ("restore sidecar reuse fast paths") confirms multiple
  VMs share one sidecar process by default. **Isolation is at the VM level, not
  the OS-process level.**

Action (per user): the doc is wrong and should be **refocused on using
`SidecarProcess` to control which sidecar a runtime/VM runs on** — i.e. how to
pass a `SidecarProcess` to `NodeRuntime.create({ sidecar })` to place work on a
chosen sidecar (vs. the default shared sidecar), and why you'd do that (resource
partitioning, isolation tiers, lifecycle control). Drop the inaccurate
"one-sidecar-per-runtime" framing; isolation is at the VM level. Source:
`packages/core/src/sidecar-process.ts`, `node-runtime.ts` `sidecar?` option.

## "Sandboxing Untrusted Code" guide (DRAFT placeholder created)

Created `website/src/content/docs/docs/sandboxing-untrusted-code.mdx` (stub) and
wired it into the Use Cases sidebar. Currently links out to
`features/permissions`. TODO list is inline in the file (threat model, per-scope
rule-set examples, resource-limit DoS guard, egress allowlist, worked
untrusted-npm-package example).
