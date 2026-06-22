<p align="center">
  <img src="https://secureexec.dev/secure-exec-logo.png" alt="Secure Exec" height="200" />
</p>

<h3 align="center">Secure Node.js Execution Without a Sandbox</h3>

<p align="center">
  A lightweight library for secure Node.js execution.<br />
  No containers, no VMs — just npm-compatible sandboxing out of the box.<br />
  Powered by the same tech as Cloudflare Workers.
</p>

<p align="center">
  <a href="https://secureexec.dev/docs">Documentation</a> — <a href="https://secureexec.dev/docs/sdk-overview">SDK Overview</a> — <a href="https://rivet.dev/discord">Discord</a>
</p>

```
npm install secure-exec
```

## Why Secure Exec

Give your AI agent the ability to write and run code safely.

- **No infrastructure required** — No Docker daemon, no hypervisor, no orchestrator. Runs anywhere Node.js, Bun, or an HTML5 browser runs. Deploy to Lambda, a VPS, or a static site — your existing deployment works.
- **Node.js & npm compatibility** — fs, child_process, http, dns, process, os — bridged to real host capabilities, not stubbed. Run Express, Hono, Next.js, and any npm package.
- **Built for AI agents** — Give your AI agent the ability to write and run code safely. Works with the Vercel AI SDK, LangChain, and any tool-use framework.
- **Deny-by-default permissions** — Filesystem, network, child processes, and env vars are all blocked unless explicitly allowed. Permissions are composable functions — grant read but not write, allow fetch but block spawn.
- **Configurable resource limits** — CPU time budgets and memory caps. Runaway code is terminated deterministically — no OOM crashes, no infinite loops, no host exhaustion.
- **Powered by V8 isolates** — The same isolation primitive behind Cloudflare Workers for Platforms and every browser tab. Battle-tested at scale by the infrastructure you already trust.

## Features

- **[TypeScript](https://secureexec.dev/docs/features/typescript)** — Compile and type-check TypeScript inside the sandbox.
- **[Permissions](https://secureexec.dev/docs/features/permissions)** — Control what sandboxed code can access on the host.
- **[Filesystem & Mounts](https://secureexec.dev/docs/features/filesystem)** — Filesystem backends for sandboxed code.
- **[Virtual Filesystem](https://secureexec.dev/docs/features/virtual-filesystem)** — A fully virtual filesystem inside the kernel, isolated from the host disk.
- **[Networking](https://secureexec.dev/docs/features/networking)** — Network access for sandboxed code.
- **[NPM & Module Loading](https://secureexec.dev/docs/features/module-loading)** — How sandboxed code resolves and loads modules.
- **[Runtime & Platform](https://secureexec.dev/docs/features/runtime-platform)** — The host environment guest code sees, plus the platform ladder.
- **[Output Capture](https://secureexec.dev/docs/features/output-capture)** — Capture console output from sandboxed code.
- **[Resource Limits](https://secureexec.dev/docs/features/resource-limits)** — Bound and cancel guest execution with timeouts, memory, and CPU-time limits.
- **[Child Processes](https://secureexec.dev/docs/features/child-processes)** — Spawn child processes from sandboxed code.

## Quickstart

**1. Install**

```bash
npm install secure-exec
```

**2. Create a runtime**

`NodeRuntime.create()` boots a fully virtualized VM behind the native sidecar. Guest code runs inside the kernel isolation boundary with no host escapes. All options are optional: `cwd` defaults to `/workspace`, and permissions default to a secure policy that denies network access (see step 4).

```ts
import { NodeRuntime } from "secure-exec";

const runtime = await NodeRuntime.create();
```

**3. Run code**

Use `run()` when you want a JSON value back; the guest calls `globalThis.__return(value)` to set it. Use `exec()` when you care about side effects and want to capture `stdout`/`stderr`/`exitCode`. Guest code runs as an ES module, so `import` and top-level `await` both work.

```ts
import { NodeRuntime } from "secure-exec";

// Boot a fully virtualized runtime. Guest code runs inside the kernel
// isolation boundary - no host escapes.
const runtime = await NodeRuntime.create();

try {
  // run() executes guest JavaScript as an ES module and returns the value the
  // guest passes to globalThis.__return(). stdout/stderr are captured too.
  const result = await runtime.run<{ message: string; sum: number }>(`
    console.log("hello from secure-exec");
    __return({ message: "hello from secure-exec", sum: 1 + 2 });
  `);

  console.log("stdout:", JSON.stringify(result.stdout.trim()));
  console.log("value:", result.value);
  console.log("exitCode:", result.exitCode);
} finally {
  // Tear down the VM and release the sidecar.
  await runtime.dispose();
}
```

**4. Configure permissions (optional)**

Guest code is **deny-by-default**: the sandbox has no network access until you opt in (the filesystem and processes are fully virtualized and never touch the host). Pass a `permissions` policy to `NodeRuntime.create()` to open up capabilities. It merges over the secure default, so you only specify what you want to change.

```ts
const runtime = await NodeRuntime.create({
  permissions: {
    // Virtualized and enabled by default (these never touch the host):
    fs: "allow",           // the in-VM filesystem
    childProcess: "allow", // spawning processes inside the VM
    process: "allow",      // process info (pid, cwd, ...)
    env: "allow",          // environment variables
    // Denied by default - opt in explicitly:
    network: "allow",      // outbound network access
    tool: "allow",         // host callbacks
  },
});
```

Set any scope to `"deny"` to lock it down. See [Permissions](https://secureexec.dev/docs/features/permissions) to learn more.

*[See the full quickstart →](https://secureexec.dev/docs/quickstart)*

<!--
## Benchmarks

V8 isolates vs. sandboxes.

### Cold start

| Percentile | Secure Exec | Fastest sandbox |
|------------|-------------|-----------------|
| p50        | 16.2 ms     | 440 ms          |
| p95        | 17.9 ms     | 950 ms          |
| p99        | 17.9 ms     | 3,150 ms        |

<details>
<summary>Methodology</summary>

**What's measured:** Time from requesting an execution to first code running.

**Why the gap:** Secure Exec spins up a V8 isolate inside the host process. No container, no VM, no network hop. Sandboxes must boot an entire container or microVM, allocate memory, and establish a network connection before code can run.

**Sandbox baseline:** [e2b](https://www.computesdk.com/benchmarks/), the fastest provider on ComputeSDK as of March 18, 2026.

**Secure Exec:** Median of 10,000 runs (100 iterations × 100 samples) on Intel i7-12700KF.

[Our benchmarks →](https://secureexec.dev/docs/benchmarks)
</details>

### Memory per instance

| Runtime                  | Memory    |
|--------------------------|-----------|
| Secure Exec              | ~3.4 MB   |
| Sandbox provider minimum | ~256 MB   |

<details>
<summary>Methodology</summary>

**What's measured:** Memory footprint added per concurrent execution.

**Why the gap:** V8 isolates share the host process and its V8 engine. Each additional execution only adds its own heap and stack (~3.4 MB). Sandboxes allocate a dedicated container with a minimum memory reservation, even if the code inside uses far less.

**What this means:** On a 1 GB server, you can run ~210 concurrent Secure Exec executions vs. ~4 sandboxes.

**Sandbox baseline:** 256 MB, the smallest minimum among popular providers (Modal, Cloudflare Containers) as of March 18, 2026.

**Secure Exec:** 3.4 MB, the converged average per execution under sustained load.

[Our benchmarks →](https://secureexec.dev/docs/benchmarks)
</details>

### Cost per execution-second

| Hardware     | Secure Exec      | vs. cheapest sandbox ($0.000625/s) |
|--------------|------------------|------------------------------------|
| AWS ARM      | $0.000011/s      | 56x cheaper                        |
| AWS x86      | $0.000014/s      | 45x cheaper                        |
| Hetzner ARM  | $0.0000016/s     | 380x cheaper                       |
| Hetzner x86  | $0.0000027/s     | 232x cheaper                       |

<details>
<summary>Methodology</summary>

**What's measured:** `server price per second ÷ concurrent executions per server`

**Why it's cheaper:** Each execution uses ~3.4 MB instead of a 256 MB container minimum. And you run on your own hardware, which is significantly cheaper than per-second sandbox billing.

**Sandbox baseline:** Cloudflare Containers, the cheapest sandbox provider benchmarked. Billed at $0.0000025/GiB·s with a 256 MB minimum (March 18, 2026).

**Secure Exec:** 3.4 MB baseline per execution, assuming 70% utilization.

[Our benchmarks →](https://secureexec.dev/docs/benchmarks) · [Full cost breakdown →](https://secureexec.dev/docs/cost-evaluation)
</details>
-->

## Secure Exec vs. Sandboxes

Not every workload needs a full OS. Secure Exec gives you V8-level isolation for code execution — no container required.

- **Secure Exec** — Run untrusted code (Node.js, Python) inside your backend process
- **Sandboxes** — Spin up a full OS with root access, system packages, and persistent disk

|                      | Secure Exec                              | Sandbox                      |
|----------------------|------------------------------------------|------------------------------|
| **Performance**      | ✅ Native V8                             | ✅ Native container          |
| **Permissions**      | ✅ Granular deny-by-default              | ❌ Coarse-grained            |
| **Setup**            | ✅ Just `npm install` — no vendor account | ❌ Vendor account required  |
| **Infrastructure**   | ✅ Run on any cloud or hardware          | ❌ Hardware lock-in          |
| **Egress**           | ✅ No egress fees                        | ❌ Per-GB egress fees        |
| **API keys**         | ✅ None                                  | ❌ Required                  |

[**Full comparison guide →**](https://secureexec.dev/docs/comparison/sandbox)

> **Need a full sandboxed operating system? We've got that too.** <br/>
>
> The [Sandbox Agent SDK](https://sandboxagent.dev/) lets you run coding agents in sandboxes and control them over HTTP. Supports Claude Code, Codex, OpenCode, Amp, and Pi. Works with E2B, Daytona, Vercel, Docker, and Cloudflare.

## FAQ

<details>
<summary>How does it work?</summary>

Secure Exec runs untrusted code inside [V8 isolates](https://v8.dev/docs/embed) — the same isolation primitive that powers every Chromium tab and Cloudflare Workers. Each execution gets its own heap, its own globals, and a deny-by-default permission boundary. There is no container, no VM, and no Docker daemon — just fast, lightweight isolation using battle-tested web technology. [Architecture →](https://secureexec.dev/docs/sdk-overview)
</details>

<details>
<summary>Does this require Docker, nested virtualization, or a hypervisor?</summary>

No. Secure Exec is a pure npm package — `npm install secure-exec` is all you need. It has zero infrastructure dependencies: no Docker daemon, no hypervisor, no orchestrator, no sidecar. It runs anywhere Node.js or Bun runs.
</details>

<details>
<summary>Can it run in serverless environments?</summary>

We are actively validating serverless platforms, but Secure Exec should work everywhere that provides a standard Node.js-like runtime. This includes Vercel Fluid Compute, AWS Lambda, and Google Cloud Run. Cloudflare Workers is not supported because it does not expose the V8 APIs that Secure Exec relies on.
</details>

<details>
<summary>When should I use a sandbox vs. Secure Exec?</summary>

Use **Secure Exec** when you need fast, lightweight code execution — AI tool calls, code evaluation, user-submitted scripts — without provisioning infrastructure. Use a **sandbox** (e2b, Modal, Daytona) when you need a full operating-system environment with persistent disk, root access, or GPU passthrough. [Full comparison →](https://secureexec.dev/docs/comparison/sandbox)
</details>

<details>
<summary>Can I run npm install in Secure Exec to dynamically install modules?</summary>

Yes. Secure Exec supports dynamic module installation via npm inside the execution environment.
</details>

<details>
<summary>Can I use it to run dev servers like Express, Hono, or Next.js?</summary>

Yes. Secure Exec bridges Node.js APIs including http, net, and child_process, so frameworks like Express, Hono, and Next.js work out of the box. For production deployments, pair Secure Exec with [Rivet Actors](https://rivet.dev/docs/actors) to get built-in routing, scaling, and lifecycle management for each server instance.
</details>

<details>
<summary>Can it be used for long-running tasks?</summary>

Yes. For orchestrating stateful, long-running tasks, we recommend pairing Secure Exec with [Rivet Actors](https://rivet.dev/docs/actors). Rivet Actors provide durable state, automatic persistence, and fault-tolerant orchestration — so each long-running task survives restarts and can be monitored, paused, or resumed without you building that infrastructure yourself.
</details>

<details>
<summary>What are common use cases?</summary>

- [AI agent code execution and tool use](https://secureexec.dev/docs/use-cases/ai-agent-code-exec)
- [User-facing dev servers (Express, Hono, Next.js)](https://secureexec.dev/docs/use-cases/dev-servers)
- MCP tool-code execution
- [Sandboxed plugin / extension systems](https://secureexec.dev/docs/use-cases/plugin-systems)
- Interactive coding playgrounds
</details>

<details>
<summary>Does this have Node.js compatibility?</summary>

Yes. Most Node.js core modules work — including fs, child_process, http, dns, process, and os. These are bridged to real host capabilities, not stubbed.
</details>

<details>
<summary>Does this have access to a full operating system?</summary>

Yes. Secure Exec includes a virtual kernel with a system bridge that supports a granular permission model. Filesystem, network, child processes, and environment variables are all available — gated behind deny-by-default permissions.
</details>

<details>
<summary>Does Secure Exec support JIT compilation?</summary>

Yes. Secure Exec runs on native V8 isolates, so your code is JIT-compiled by V8's TurboFan optimizing compiler — the same pipeline that powers Chrome and Node.js. This means full optimization tiers, inline caching, and speculative optimization out of the box.
</details>

<details>
<summary>How does Secure Exec compare to WASM-based JavaScript runtimes like QuickJS?</summary>

WASM-based runtimes like [QuickJS](https://bellard.org/quickjs/) (via quickjs-emscripten) compile a separate JS engine to WebAssembly, which means your code runs through an interpreter inside WASM — not native V8. Secure Exec uses native V8 isolates directly, so you get the same JIT-compiled performance as JavaScript running on the host. No interpretation overhead, no WASM translation layer, and full Node.js API compatibility.
</details>

## Links

- [Documentation](https://secureexec.dev/docs)
- [Changelog](https://github.com/rivet-dev/secure-exec/releases)
- [Discord](https://rivet.dev/discord)
- [GitHub](https://github.com/rivet-dev/secure-exec)

## License

Apache-2.0
