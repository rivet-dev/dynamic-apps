# WASM Engine Research: should secure-exec move off V8 for (threaded) native-wasm?

Internal research note. Compiled 2026-06-27 from a multi-agent research sweep (engine deep-dives,
the secure-exec bridge map, production-adoption survey, and a benchmark hunt). Companion to
`WASM-THREADS-SPEC.md` (threading implementation) and `INTERNAL-TOOLING.md`.

> The wasmtime-vs-V8 head-to-head is in §9.5; startup/memory detail in §9.2-9.3. Net: throughput is
> ~a wash on general compute (the crypto gap is a V8 `wide_arithmetic`/SIMD artifact), Wasmtime wins
> instantiation (~1000x) and memory; no clean recent peer-reviewed Wasmtime-vs-V8 general-compute
> benchmark exists in the literature.

---

## 0. TL;DR / recommendation

- **Do NOT switch runtimes to fix threading.** The only trustable alternative (Wasmtime) has the
  *least* multi-tenant-safe threading of the field; switching trades a working isolation property for
  an unsolved one.
- **secure-exec's "guest wasm inside V8 isolates" model is the proven mainstream choice** for running
  untrusted wasm+JS together (Cloudflare Workers, Vercel, Netlify, Deno, Supabase all do the same).
  It is not an odd bet.
- **One V8 isolate per wasm thread is permanent inside V8** and inside every JS engine. No proposal
  (V8 shared-heap, TC39 Shared Structs, Wasm shared-everything-threads) relaxes the
  "one-thread-per-isolate" rule; they only expand *what state is shared between* single-threaded
  isolates.
- **The real overhead is not threads, it's JS glue.** Native-wasm guests currently run *as JS inside
  V8*, dragging the ~500KB undici+node-polyfill bridge bundle into every isolate — 100% dead weight
  for a C/GTK app, threaded or not.
- **If a second engine is ever justified, it is Wasmtime** (production consensus + only trustable
  non-V8 option), for native-wasm guests ONLY (keep V8 for JS/pyodide), gated on a kernel-brokered
  thread-spawn layer (never `wasmtime-wasi-threads`, which `process::exit`s the host on a guest trap).
- **Performance does not justify a switch.** On general compute V8 is ~1.3-1.6x native like everyone
  else; the headline "Node 8x slower" was crypto + cold-JIT-warmup specific. The wasmtime win would be
  footprint + cold-start + dropping JS glue, NOT raw throughput.
- **Decision order: (1) measure under the GLib smoke test, (2) in-V8 isolate pooling, (3) only then
  consider a Wasmtime native-wasm executor.**

---

## 1. Why engine choice matters here (trust model recap)

secure-exec is a sandbox: the security boundary is **sidecar <-> executor**, and the executor runs
*actively hostile* guest code. So the engine is not a perf detail; it is part of the TCB. Two
consequences drive everything below:

- The engine must hold against hostile guests (escape, cross-VM read, resource exhaustion).
- "Trusted" therefore means *proven against adversarial code*, with a fast disclosure/patch process —
  not "low CVE count" (which usually means "not scrutinized").

---

## 2. How WASM threads work (spec) vs how secure-exec does it

### 2.1 The spec model
The WebAssembly threads proposal (Phase 4, shipped in browsers since 2018) **deliberately does not
define thread creation.** It adds only:
- a `shared` linear memory type (backed by `SharedArrayBuffer`), and
- atomic instructions (`atomic.wait`/`notify`, atomic rmw).

Thread *spawning* is punted to the host/embedder. Every toolchain therefore uses an
**instance-per-thread** model: a new thread is a separate module instance, connected to the others
ONLY by the shared memory.

- In a browser / V8: a thread = a **Web Worker** = a separate JS agent = a separate V8 isolate, which
  re-instantiates the module over the same `WebAssembly.Memory`.
- `wasi-threads` (`wasi_thread_spawn`) and Emscripten pthreads both follow this exact shape.

Why a whole separate instance? **V8 is hard-wired to one thread per isolate** — an isolate can never be
entered by two threads at once, and isolates can't share mutable JS/heap state. The only cross-isolate
sharing V8 permits is the `SharedArrayBuffer` backing store.

### 2.2 secure-exec's implementation
secure-exec runs guests as wasm inside V8 isolates, so a pthread/wasi-thread spawn becomes **a fresh
V8 isolate on its own OS thread** (`crates/v8-runtime/src/wasm_threads.rs`). Two spawn paths:

- **Bare-isolate direct-spawn** (`wasm_threads.rs:357-416`) — compute-only threads, spawned via
  `std::thread::Builder` from the parent isolate.
- **Sidecar-mediated** (`crates/sidecar/src/execution.rs:5542-5639`, `spawn_wasm_thread`) — threads
  that touch fds/sockets/VFS; they **share the parent process's kernel fd table**.

Shared memory crosses isolates via V8's `ValueSerializer` backing-store transfer
(`wasm_threads.rs:155-169`), exactly like a browser postMessaging a `SharedArrayBuffer`. Caps: 128
live threads process-wide (`DEFAULT_MAX_LIVE_THREADS`) + a per-VM `max_threads` at the sidecar.

This is the **same architecture browsers use** for pthreads — not a quirk. Per CLAUDE.md,
single-threaded execution is an intentional/permanent concession; wasi-threads is opt-in, off by
default.

```
            ONE shared WebAssembly.Memory (SAB backing store)
            serialized across isolates via V8 ValueSerializer
        +----------+----------------+----------------+
        |          |                |                |
   +----+-----+ +--+-------+   +----+-----+
   | Isolate0 | | Isolate1 |   | IsolateN |  each: own OS thread,
   | (main)   | | (thread) |   | (thread) |        own GC heap,
   +----+-----+ +--+-------+   +----+-----+        own bridge ctx
        +-------- sync-RPC bridge ----------+
                        |
            single-threaded SIDECAR loop
            (serializes host calls; shared fd table)
```

---

## 3. Will "one isolate per wasm thread" ever change? (No.)

Researched across V8, JSC, SpiderMonkey, the Wasm CG, and TC39. Every active proposal expands *what
state can be shared between* single-threaded agents; none relaxes "an isolate is entered by at most one
thread at a time."

| Proposal | Status (2026) | Relaxes 1-thread-per-isolate? | What it actually does |
|---|---|---|---|
| Wasm threads (original) | Phase 4, shipped | No | Shares linear memory only |
| **shared-everything-threads** | Phase 1, pre-spec | **No** | Shares GC objects/tables/globals; browsers still implement spawn on Web Workers |
| **TC39 Shared Structs** | Stage 2 | **No (explicitly)** | Shared mutable objects across agents + `Atomics.Mutex`/`Condition`; V8 flag `--harmony-struct` |
| **V8 "shared heap / shared isolate"** | shipped behind `--harmony-struct` | **No** | Shared memory *between* isolates; each keeps its own main thread; GC needs a global safepoint that stops every client's main thread |
| JSPI | Phase 4, shipped | N/A | Single-threaded async suspend/resume |
| Stack switching | Phase 3 | N/A | One instance, multiple stacks (coroutines/green threads), cooperative |

Hard rule, from V8 docs: *"at any point in time there can only be a single thread executing a given
isolate."* The browser is getting a better *front door* to the same threading model (a standardized
`thread.spawn` builtin), never a new model. The only ways to "escape the isolate" are a non-V8 engine
(for parallelism) or stack-switching (for cooperative concurrency).

---

## 4. Engine trust tiers (the decisive axis)

For *actively hostile* code, most of the field is disqualified, not merely weaker.

| Engine | Trustable for hostile code? | The disqualifier / basis |
|---|---|---|
| **V8** | YES — gold standard | Runs hostile code in billions of browsers; full-time security team + Project Zero; fastest CVE response; unmatched fuzzing. Carries full JS+JIT attack surface though |
| **Wasmtime** | YES — only credible alternative | Rust host (eliminates ~70% of memory-safety bug class); small wasm-only TCB; Cranelift partly formally verified (ISLE/Crocus, VeriWasm); 24/7 OSS-Fuzz incl. differential fuzzing vs V8; real disclosure process; Fastly/Shopify run untrusted tenants |
| **WAMR** | NO (for hostile) | C codebase = any runtime bug is a host escape in the TCB; admits ROP/side-channel exposure; embedded/TEE-focused. Fine for *trusted* code |
| **Wasmer** | NO (for hostile) | History of FS-sandbox *escape* CVEs (cwd auto-preopened by default ~1.5yr, CVE-2023-51661); no external audit; no OSS-Fuzz; no Spectre story; ~9-person shrinking vendor |
| **WasmEdge** | NO (for hostile) | Native-AOT-by-default (runs native code; `--force-interpreter` to be safe); side-channels declared out of scope; wrapping-bounds-check CVE (CVE-2025-69261); C++ core behind unstable FFI Rust SDK |
| **wasmi** (interpreter) | YES for determinism, slow | The security-paranoid choice: no JIT codegen surface at all. Used by Stellar Soroban, Casper |
| **wazero** (pure-Go) | moderate | No CGo; pure-Go safety; slowest steady-state. Used by Traefik/MOSN |

Key reframe: **wasmtime has *more* published CVEs than Wasmer/WAMR/WasmEdge because it is scrutinized
hardest** (incl. an April-2026 batch of 12 advisories, 2 critical: a Winch escape + an aarch64
miscompile). Quiet CVE records = absence of attention, not absence of bugs.

### 4.1 Why wasmtime earns trust (detail)
1. Rust host — whole memory-safety bug class gone; wasm parser has zero `unsafe`.
2. Small focused TCB — wasm-only; no JS object model / prototype chains / GC / giant JIT (the source
   of most V8 0-days). Escape surface collapses to Cranelift codegen.
3. Partial formal verification of the compiler (ISLE lowering via SMT, Crocus/VeriISLE ASPLOS 2024;
   VeriWasm proves output stays in the SFI sandbox).
4. Continuous differential fuzzing on OSS-Fuzz, including against V8 + the spec interpreter.
5. Defense-in-depth on by default: 2GB guard regions, guard pages, Spectre mitigations
   (call_indirect/br_table/heap checks), memory zeroing on instance reuse, pooling allocator + MPK.
6. Real disclosure process (GHSA + RustSec mirror), defined Tier-1 platforms.
7. Production hostile-tenant track record (Fastly, Shopify).

Caveats: the compiler IS the TCB (miscompiles = escapes); recurring pooling-allocator cross-instance
leak CVEs (the "read another VM's state" risk); aarch64 only Tier 2, no continuous aarch64 fuzzing as
of April 2026.

### 4.2 Who built it
Bytecode Alliance (nonprofit; founded 2019 by Mozilla, Fastly, Intel, Red Hat). Originated at Mozilla;
Cranelift began at Mozilla ("Cretonne"). Core team largely migrated Mozilla -> Fastly. Maintainers:
Alex Crichton (lead, Fastly), Chris Fallin (Cranelift), Nick Fitzgerald, Pat Hickey. It is the
reference implementation maintained by the people who write the wasm/WASI specs.

---

## 5. The threading paradox

> The most security-mature alternative has the *least* multi-tenant-safe threading.

- **Wasmtime `wasi-threads` calls `process::exit` when any guest thread traps** — one hostile thread
  kills the whole sidecar and every other VM. Its own source says "not suitable for multi-tenant
  embeddings." Enabling threads also **disables the pooling allocator** (kills the density win).
- **WAMR** threading is mature but a **fixed build-time pool (default ~4)** on a C core.
- **Wasmer** threads only via its proprietary **WASIX** divergence.

There is currently **no runtime that is simultaneously gold-standard-secure AND production-ready for
hostile *threaded* multi-tenant guests** out of the box. secure-exec's isolate-per-thread model
actually sidesteps this — each thread is a fully isolated unit, so a trapping thread can't read or kill
its siblings. **Switching engines to fix threading would give up isolation we already have.**

---

## 6. Bridge mechanism + cost of a second engine

From the secure-exec bridge map (`crates/v8-runtime`, `crates/bridge`, `crates/sidecar`,
`crates/execution`):

- **JS glue per isolate:** before guest code, each isolate loads the V8 bridge bundle
  (`crates/execution/assets/v8-bridge.source.js`, generated by
  `packages/build-tools/scripts/build-v8-bridge.mjs`) — undici fetch/HTTP, node-stdlib-browser
  polyfills (fs/net/crypto/dns/...), CJS/ESM interop. ~500KB compressed / ~2-3MB in-isolate, restored
  via a V8 snapshot (`crates/v8-runtime/src/snapshot.rs`).
- **Host-call path:** synchronous 2-way RPC. Guest calls a native `FunctionTemplate` callback
  (`bridge.rs`), args serialized via V8 `ValueSerializer`, sent on a channel, the isolate thread
  **blocks** until the matching `BridgeResponse` (`host_call.rs:sync_call`), deserialized back.
- **Native-wasm today runs *as JS*:** `crates/execution/src/wasm.rs`
  (`build_wasm_runner_module_source`) generates a JS runner that calls `WebAssembly.instantiate` and
  routes every host import back through the JS bridge callbacks. So a pure-C/GTK app carries the full
  JS bridge bundle it never uses.
- **Portability seam:** the kernel + bridge *contract* (`crates/bridge`) + sidecar service routing are
  engine-agnostic and reusable (~25%). V8-specific: the serializer, FunctionTemplate callbacks,
  snapshot, async/promise handling, and the SAB-based threading.

Cost estimate: the agent's "~75% rewrite / 6-12 weeks" was for **full JS parity** on wasmtime, which is
NOT the goal. A **native-wasm-only** path doesn't need undici, node polyfills, ESM/CJS interop, or
`NodeImportCache`; it needs WASI host functions wired to the kernel via wasmtime's `Linker` +
`Caller<'_, T>` (which hangs kernel state directly; guest->host call ~3-5ns). The reusable kernel/
contract layer stays; most of the JS-specific 75% does not apply.

---

## 7. Production adoption: three camps

| Domain | Winning engine | Who |
|---|---|---|
| JS-native edge (wasm inside V8) | **V8** | Cloudflare Workers, Vercel, Netlify, Deno Deploy, Supabase; Akamai (wasm *blocked*); GCP Service Extensions (likely V8, unconfirmed) |
| Untrusted-code serverless / FaaS | **Wasmtime** | Fastly Compute, Shopify Functions, Fermyon->Akamai (75M req/s GA), wasmCloud/Cosmonic, Suborbital |
| Databases & streaming UDFs | **Wasmtime** (sweep) | SingleStore, Redpanda, ScyllaDB (experimental), InfinyOn Fluvio |
| Kubernetes / edge compute | **Wasmtime** | Azure SpinKube (GA path after AKS WASI pools retired May 2025), MS Hyperlight |
| API gateways / proxies | **V8** by traffic (Envoy/Istio default); **Wasmtime** by deliberate choice (Kong) | Envoy/Istio=V8, Kong=Wasmtime, Higress=WAMR+AOT, Traefik/MOSN=wazero |
| Blockchain (chains) | **Wasmtime** | DFINITY ICP, Polkadot chain runtime, NEAR (near-vm) |
| Blockchain (hostile contracts) | **wasmi / Wasmer-Singlepass** (no optimizing JIT) | Stellar Soroban, Casper (wasmi); CosmWasm, Arbitrum Stylus (Wasmer) |
| Embedded / IoT / TEE | **WAMR** (sweep) | Amazon Prime Video, Disney+, Sony cameras, Xiaomi, Alibaba Higress |
| Big-cloud untrusted serverless | **microVMs, NOT wasm** | AWS Lambda = Firecracker (15T invocations/mo); AWS has no GA wasm product |

Three findings on-point for secure-exec:
1. **secure-exec's wasm-in-V8 model = the Cloudflare/Vercel/Deno camp** — the most battle-tested model
   for untrusted wasm+JS at planet scale. Not an odd limb.
2. **Security-paranoia gradient:** for the *most* hostile code (blockchain contracts handling money),
   the field deliberately avoids optimizing JITs — Soroban/Casper use the **wasmi interpreter**
   ("every shipped JIT has had critical RCE/JIT-bomb vulns"); CosmWasm uses **Wasmer Singlepass**
   (non-optimizing). A caution against chasing JIT throughput at a hostile boundary.
3. **AWS** (most conservative hyperscaler) chose Firecracker **microVMs over wasm** for untrusted
   serverless.

When a deliberate standalone wasm engine is chosen, it is **overwhelmingly Wasmtime** — so a future
second engine should be Wasmtime, confirmed by the market, not WAMR/Wasmer/WasmEdge.

---

## 8. Benchmarks: credibility first

Most 2024-2026 "wasm runtime comparison" sites are AI-generated junk (wasmruntime.com, reintech.io,
morphllm, bonviewpress "Comparative Study" with suspiciously round numbers) — **discarded**. Credible
core:

| Source | Type | Credibility |
|---|---|---|
| Jangda et al., USENIX ATC 2019 ("Not So Fast") | Peer-reviewed | High (but browser engines, dated) |
| WarpDiff, ASE 2023 (arXiv 2309.12167) | Peer-reviewed | High (relative, not absolute) |
| eWAPA, IEEE 2024 (arXiv 2409.10252) | Peer-reviewed | High (startup + I/O; narrow) |
| **Wasm-R3, OOPSLA 2024 (arXiv 2409.00708)** | Peer-reviewed | High (realistic 27-bench suite; V8/JSC/SM + Wasmtime/Wasmer) |
| **CCGrid 2024, Kakati & Brorsson (orbilu 10993/62285)** | Peer-reviewed | High (PolyBench, ~1.3x native, x86+ARM+RISC-V) |
| WAMI, 2025 (arXiv 2506.16048) | Preprint | Med-high (matched-mode PolyBench) |
| **Wasure, Feb 2026 (arXiv 2602.05488)** | Preprint | High method; broadest (all standalone + V8/JSC/SM, 8 suites) |
| Besozzi 2025 (arXiv 2509.09400); Lumos, ACM IOT 2025 (arXiv 2510.05118) | Peer-reviewed | High (serverless cold-start) |
| Frank Denis, 00f.net 2026-06-23 | Expert blog, full method | High (libsodium crypto only) |
| Bytecode Alliance Wasmtime perf; WAMR wiki; wasm3 docs | Vendor / stale | Medium / directional |

---

## 9. Benchmark findings by dimension

### 9.1 Throughput (steady-state, vs native)
- **Crypto (Denis 2026):** WAMR-AOT 1.57x < WasmEdge-AOT 1.74x < Wasmer 2.08x < Wasmtime-Cranelift
  2.41x < Wazero 4.72x < Node/V8 7.95x < Bun 8.77x. With `wide_arithmetic` (Wasmtime/Wasmer only) the
  order inverts: Wasmer 1.33x, Wasmtime 1.46x. CAVEAT: single workload family; mixes AOT vs JIT.
- **General compute (PolyBench, CCGrid 2024, peer-reviewed):** ~**1.3x native** on both x86 and ARM.
- **Matched-mode PolyBench (WAMI 2025):** WAMR-AOT ~1.9% *faster* than native; Wasmtime-AOT ~4.1%
  slower; interpreters within ~10%.
- **Broad (Wasure 2026):** rankings swing **up to 2 orders of magnitude by workload**; **browser
  engines (incl. V8) show tight, competitive distributions on general compute**, falling behind mainly
  on SIMD/crypto.

**Net:** on general compute the top engines INCLUDING V8 cluster ~1.3-1.6x native. The "Node 8x" figure
is crypto + cold-JIT-warmup specific, not V8's true wasm speed. LLVM/AOT engines (WAMR, WasmEdge,
Wasmer-LLVM) lead peak crypto; Cranelift trades a little peak for fast/verifiable compile.

### 9.2 Startup / instantiation
- Among compiling runtimes Wasmtime instantiates fastest (eWAPA 2024): Wasm3 (interp) << Wasmtime ~
  preview2 << WAMR (~100x Wasm3) << Wasmer (~2000x Wasm3).
- Wasmtime instantiation ~5us (SpiderMonkey.wasm 2ms -> 5us via CoW + lazy init); Lucet/Wasmtime
  lineage full instantiation ~52us (Fastly). Wasmtime cold start 5.6ms no-op / 16.9ms / 188ms
  (Besozzi); 5-17x faster than Firecracker on compute-light.
- **V8 isolate ~5ms to create vs Wasmtime instance ~5us — a ~1000x gap** (Bytecode Alliance: "the
  fastest alternative, a JS isolate, [takes] about 5ms; a Wasm instance takes 5µs"). This is the
  number most relevant to secure-exec: we create a NEW isolate per thread, so we pay isolate-creation
  cost per thread, not the cheaper in-isolate `WebAssembly.instantiate`. CAVEATS: (a) the 5us is one
  large module with lazy-init + pooling, (b) **pooling is disabled when threads are on**, so the
  threaded-path advantage is real but smaller than 1000x, (c) secure-exec mitigates isolate creation
  with V8 snapshots. V8's in-isolate wasm-instantiate latency (us) is not separately published.

### 9.3 Memory footprint
- Thinnest dimension; no clean four-way RSS study. Directional: V8 isolate ~2-3MB; Wasmtime pooling
  allocator -> KB-scale per instance (but threads disable pooling); WAMR embedded interp ~56KB code,
  classic ~365KB / fast ~485KB working set; iwasm ~50KB. Low confidence. (See §9.5.)

### 9.4 Compile time
- Only Wasmtime self-data: baseline (non-optimizing) compiler 15-20x faster than Cranelift-optimizing,
  at 1.1-1.5x slower code. No credible cross-runtime numbers.

### 9.5 Wasmtime vs V8 head-to-head

| Dimension | Wasmtime (Cranelift) | V8 (TurboFan) | Winner | Confidence |
|---|---|---|---|---|
| Peak throughput, **general compute** | ~85-90% of native | ~90-95% native (PolyBench), ~65% (SPEC) | **V8 by ~2%** (2020 Cranelift-README datapoint); roughly **parity** | **Low** — one old indirect datapoint |
| Peak throughput, **crypto** | 2.41x native (2026 libsodium) | 7.95x native | **Wasmtime ~3.3x** | High value, but **crypto-specific artifact** |
| **Startup / instantiation** | ~5us (bare wasm instance) | ~5ms (isolate create) | **Wasmtime (~1000x)** | Medium (V8 in-isolate wasm-instantiate us not sourced) |
| **Compile speed** | Cranelift ~10x faster than LLVM; faster than TurboFan | slower | **Wasmtime** | Medium |
| **Memory (multi-tenant RSS)** | lower (bare wasm) | higher (full JS heap, ~2-3MB/isolate) | **Wasmtime** | Low (secondary sources) |

**The honest verdict on throughput:** on general compute, Wasmtime and V8 are **within ~2-15% of each
other** — V8 historically a hair ahead on raw code quality (Cranelift ~2% slower per the only direct
comparison, arXiv 2011.13127, 2020), Cranelift competitive and faster to compile. Corroborating: on
ARM 2021 libsodium they were within ~10% (Wasmtime 1.85x, V8 2.04x). The huge 2026 crypto gap
(2.41x vs 7.95x) is **V8 lacking the `wide_arithmetic` proposal + SIMD/crypto paths**, NOT a general
TurboFan deficiency.

**Critical literature gap:** there is **no clean, recent (2023+), peer-reviewed head-to-head Wasmtime
vs V8 on general compute.** It falls between the browser-focused papers (V8 only: Jangda 2019) and the
standalone-runtime papers (Wasmtime/WAMR/WasmEdge, no V8). Wasure 2026 *runs* both but publishes
distributions (violin plots), not extractable per-suite Wasmtime-vs-V8 numbers; Wasm-R3 reports
portability, not timing. So nobody can hand you a defensible "X% faster" number for general compute —
distrust anyone who claims one.

**For secure-exec specifically:** throughput is ~a wash on general compute, so it does NOT justify a
switch. The defensible Wasmtime wins are **instantiation (~1000x cheaper per thread), memory density,
and shedding the JS bridge bundle** — footprint/startup, not raw speed. Both engines share JIT
miscompilation as the primary escape vector, but V8's attack surface (JS object model + multi-tier JIT)
is far larger than Wasmtime's (Cranelift only) — the security argument for a native-wasm Wasmtime path,
independent of perf.

### 9.6 Gaps
- No peer-reviewed, native-normalized, broad-workload leaderboard across all standalone runtimes PLUS
  V8 in matched AOT modes. Wasure is closest (preprint, distributions not a ranking).
- No standalone-runtime SPEC CPU data (only Jangda 2019, browsers). No independent CoreMark dataset.
- Cross-runtime compile-time and server-class RSS are barely measured.

---

## 10. Recommendation (phased)

```
   STAY ON V8 for JS/pyodide (always).
        |
   [1] MEASURE: GLib/GTK smoke test under real thread-pool load
       (SECURE_EXEC_STACKDUMP / TRACE). Is isolate creation actually
       the bottleneck for the off-by-default threads feature?
        |
   [2] CHEAP WIN: in-V8 isolate pooling (already scoped in
       WASM-THREADS-SPEC.md). Keeps browser parity + isolation.
        |
   [3] ONLY IF JUSTIFIED: add a Wasmtime executor for native-wasm
       guests ONLY. Sheds the JS bridge bundle + lighter threads.
       - keep V8 for JS/pyodide (wasmtime runs neither)
       - kernel-brokered thread-spawn (NEVER wasmtime-wasi-threads)
       - accept pooling-allocator/threads conflict
       - reuse kernel + bridge contract + service layer
```

Switching the whole runtime is the wrong trade: the only trustable target has unsolved multi-tenant
threading, and you'd give up isolation you already have. Performance doesn't force it either.

---

## 11. Open questions / what to measure next

1. **Own numbers** — run the repo's native-wasm guests (GTK/xfce binaries here, css-bench/map-bench/
   pango-bench) on V8-as-today vs a wasmtime prototype: instantiation, RSS per instance, steady-state.
   Far more trustworthy than libsodium extrapolation.
2. **Thread cost under GLib** — actual per-isolate cost when a GLib thread pool sprays workers; is the
   "2-3MB/isolate" claim real here?
3. **Pooling prototype** — measure isolate-pool reuse savings before considering any engine work.
4. **Native-wasm bridge slimming** — even staying on V8, can native-wasm guests skip most of the
   undici/node-polyfill bundle? That captures part of the win with zero engine change.

---

## 12. Sources (selected)
- Wasm threads proposal: https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md
- shared-everything-threads: https://github.com/WebAssembly/shared-everything-threads
- TC39 Shared Structs: https://github.com/tc39/proposal-structs
- V8 isolate ref: https://v8.github.io/api/head/classv8_1_1Isolate.html
- Wasmtime security: https://docs.wasmtime.dev/security.html ; https://bytecodealliance.org/articles/security-and-correctness-in-wasmtime
- wasmtime-wasi-threads (process::exit): https://crates.io/crates/wasmtime-wasi-threads
- WAMR security: https://wamr.gitbook.io/document/basics/introduction/security_feature
- Engine choice essay: https://blog.colinbreck.com/choosing-a-webassembly-run-time/
- Stellar "why no JIT": https://stellar.org/blog/developers/why-doesnt-soroban-use-a-jit
- Benchmarks: https://00f.net/2026/06/23/webassembly-runtimes-2026/ ; https://arxiv.org/abs/2602.05488 ; https://arxiv.org/abs/2409.00708 ; https://orbilu.uni.lu/handle/10993/62285 ; https://arxiv.org/abs/2506.16048 ; https://arxiv.org/abs/2509.09400
- Production: Fastly https://docs.fastly.com/products/compute ; Shopify https://shopify.engineering/javascript-in-webassembly-for-shopify-functions ; DFINITY https://wiki.internetcomputer.org/wiki/WebAssembly ; Envoy https://www.envoyproxy.io/docs/envoy/latest/configuration/other_features/wasm
