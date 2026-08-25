# Dynamic Apps cold-isolate versus warm-actor benchmark

Run on 2026-08-25 (US/Pacific). The original attribution was wrong in two
important ways:

1. The reported 459 ms “V8 isolate start” included option resolution, eight
   default software packages, VM creation, mount reconciliation, and bootstrap
   filesystem work. Native V8 VM creation itself is about **2.5 ms p50**.
2. The reported 103 ms “actor request” was two serial query-backed actor calls
   inside the app actor. A direct warmed actor action in Rivet Cloud is about
   **23 ms p50**; a key query plus the action is about **43 ms**.

After removing software the cold request does not use, the same-release Cloud
comparison is:

| Path | Samples | End-to-end p50 | p95 | Server p50 | Server p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Cloud edge no-op | 100 | 214.74 ms | 296.70 ms | 0.06 ms | 0.11 ms |
| Cloud warm, current architecture | 100 | 473.35 ms | 630.36 ms | 241.38 ms | 296.40 ms |
| Cloud warm, cached app actor ID | 100 | 421.68 ms | 537.27 ms | 194.19 ms | 223.76 ms |
| Cloud cold, new VM every request | 30 | 846.68 ms | 1,042.35 ms | 616.31 ms | 675.36 ms |

All requests succeeded. Every warm request used the same ready replica and
reported `cold-start: 0`; every cold request had a unique `edge-cold/<uuid>` VM
identity. Both Cloud paths used the same active 7,459-byte fixture release in
the same deployed app.

The unchanged warm architecture saves **374.93 ms (60.8%)** of server time
against the corrected cold implementation; cold is **2.55x** slower. Caching
only the stable app actor ID increases the saving to **422.12 ms (68.5%)** and
makes cold **3.17x** slower. The public ingress is roughly 215 ms from the test
client, so server headers—not raw end-to-end time—are the useful architecture
comparison.

## Architecture under test

```text
cold: client -> Compute edge server -> app actor artifact storage
             -> download/materialize bundle -> new AgentOS V8 VM
             -> new guest Node process -> one request -> synchronous dispose

warm: client -> Compute edge server -> api.rivet.dev -> app actor
             -> api.rivet.dev -> scaler actor
             -> api.rivet.dev -> warmed replica actor/V8 VM
```

The cold edge has no in-memory artifact cache. It resolves the persisted
deployment, reads and verifies the artifact, creates one VM, serves one request,
and destroys the VM and temporary artifact before returning. The warm route is
the existing app/scaler/replica implementation on `main`; the only warm variant
adds a process-local cache for the stable app actor ID so the cost of that one
lookup can be measured separately.

The workload is a zero-dependency JavaScript `fetch()` function returning a
47-byte JSON response. Build/deploy and warm qualification requests are
excluded. Phase percentiles are calculated independently and therefore do not
need to sum exactly.

## Why the warm request is still about 241 ms

The low-level Cloud routing probe ran 200 sequential requests per case:

| Actor operation from Compute | Server p50 | p95 |
| --- | ---: | ---: |
| Resolve warmed actor key | 18.30 ms | 22.11 ms |
| Direct action by actor ID | 23.08 ms | 27.66 ms |
| Direct fetch by actor ID | 23.87 ms | 27.34 ms |
| Query-backed action by key | 43.27 ms | 49.80 ms |
| Query-backed action, skip ready wait | 32.54 ms | 39.56 ms |
| Query-backed fetch by key | 42.88 ms | 53.01 ms |
| Actor handler itself | 0.07 ms | 0.10 ms |
| Nested actor request, outer total | 65.84 ms | 83.11 ms |
| └ peer key resolution inside handler | 18.14 ms | 22.95 ms |
| └ peer direct action inside handler | 22.49 ms | 38.37 ms |

A normal query-backed action is almost exactly a key resolution plus a direct
action: 18.30 + 23.08 = 41.38 ms, versus 43.27 ms measured. The handler's own
work is effectively zero. The time is routing and network, not application
execution.

The Compute logs confirm that the serverless process in `us-west-1` uses
`https://api.rivet.dev/` as both its Engine and client endpoint. Each nested
actor operation therefore leaves the Compute container, traverses the public
API/gateway path, and comes back. This is not an in-process or datacenter-local
actor call.

The current warm request performs several of these serially:

| Warm phase | p50 | p95 |
| --- | ---: | ---: |
| App release SQLite lookup | 0.93 ms | 1.31 ms |
| Edge app actor resolution | 20.21 ms | 29.31 ms |
| Scaler acquire, query-backed | 48.91 ms | 78.14 ms |
| Replica stream start, query-backed | 57.43 ms | 93.42 ms |
| App request through response headers | 109.76 ms | 154.96 ms |
| Edge-to-app gateway through headers/body orchestration | 217.33 ms | 276.45 ms |
| **Complete edge server request** | **241.38 ms** | **296.40 ms** |

The outer gateway phase also contains the replica stream-read and scaler
release actions needed to complete the tiny body. This is why subtracting only
the 109.76 ms app-header phase leaves a large remainder: it is more actor
traffic, not unexplained guest work.

Caching the app actor ID removes the explicit 20 ms lookup and turns the outer
query-backed app fetch into a direct fetch. It saves **47.19 ms p50**, but the
internal scaler and replica calls remain query-backed, leaving **194.19 ms** of
server time.

### Is a sub-10-ms warm request possible here?

Not with the current hop graph. One direct actor action is already about 23 ms
in Cloud, and a complete warm response makes multiple serial actor calls.
Caching scaler and replica IDs would help, but even five direct 23 ms calls have
an approximate 115 ms network floor.

The next meaningful warm optimization is architectural:

- cache the app and scaler actor IDs and return a replica actor ID in each
  admission, eliminating repeated key queries;
- collapse admission and execution into one actor, or otherwise avoid separate
  stream-read and release gateway round trips for a 47-byte response;
- provide a datacenter-local/internal Engine endpoint to Compute workloads.

A current-main Rivet Engine release build reaches **5.77 ms p50** for a direct
local actor action and **8.91 ms** for a local query-backed action, so the
sub-10-ms target is credible for one colocated hop. It is not credible for the
current multi-hop Cloud architecture.

I tested a Guard fast path that bypassed ready-actor event subscriptions. A
same-revision 200-request A/B changed direct actions by -0.16 ms and nested
actions by +0.41 ms—noise—so that patch was rejected and no Rivet source change
was retained. The subscription setup is not this bottleneck.

## Correct cold-start attribution

The cold route now sets `defaultSoftware: false`. It launches AgentOS's builtin
Node command directly and does not use the eight default Unix software
packages, so installing and mounting them on every request was pure overhead.

| Cloud cold phase | p50 | p95 |
| --- | ---: | ---: |
| Edge app resolution | 19.72 ms | 39.10 ms |
| Deployment lookup | 23.66 ms | 33.25 ms |
| Artifact manifest | 26.36 ms | 42.13 ms |
| Artifact download | 28.74 ms | 43.65 ms |
| Artifact materialization | 0.95 ms | 2.49 ms |
| AgentOS create/configure, broadly labelled `cold-isolate` | 61.44 ms | 67.21 ms |
| Guest process spawn RPC | 0.17 ms | 0.31 ms |
| Guest ready wait | 274.95 ms | 310.36 ms |
| Request to response headers | 65.68 ms | 88.29 ms |
| Response body | 24.05 ms | 85.54 ms |
| Synchronous VM/artifact disposal | 65.60 ms | 111.80 ms |
| **Complete edge server request** | **616.31 ms** | **675.36 ms** |

Download is only **28.74 ms (4.7%)**. Making it free leaves about 587.57 ms, so
download optimization alone does not alter the decision.

The `cold-isolate` name is retained for compatibility with the load report, but
it is not a pure V8 timer. A shared-sidecar AgentOS microbenchmark separates it:

| Shared-sidecar AgentOS create scenario | Create p50 |
| --- | ---: |
| Bare VM, no default software | 8.18 ms |
| Placeholder default software | 11.60 ms |
| One tiny application package mount | 7.40 ms |
| Eight real common packages plus app mount | 56.32 ms |

In the real-package case, native VM creation is **2.55 ms**, mount/package
configuration is 31.21 ms, and bootstrap-directory work is 21.42 ms. Bare
native `create_vm` is about **2.4 ms**. Therefore the unavoidable V8 creation
floor is a few milliseconds, not half a second.

Local guest instrumentation further splits the ready wait:

| Local corrected cold phase | p50 |
| --- | ---: |
| AgentOS create/configure | 30.24 ms |
| Guest ready wait | 112.18 ms |
| └ embedded Node/runtime bootstrap before module starts | 92.66 ms |
| └ guest module import and HTTP readiness | 20.00 ms |
| Complete local cold server request | 258.32 ms |

The largest cold startup target is now the guest Node/runtime bootstrap, not V8
creation and not download. Snapshotting/preinitializing that runtime, or serving
the handler directly from the newly created V8 VM without booting a separate
Node-compatible command, is the experiment most likely to close the remaining
gap. Synchronous disposal is the second obvious target if request semantics can
safely hand cleanup to a bounded background task.

## Local comparison

| Path | Samples | End-to-end p50 | p95 | Server p50 | Server p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Local edge no-op | 100 | 1.51 ms | 2.28 ms | 0.01 ms | 0.03 ms |
| Local warm, current | 100 | 196.25 ms | 223.40 ms | 194.39 ms | 221.56 ms |
| Local warm, cached app ID | 100 | 184.08 ms | 205.03 ms | 182.32 ms | 203.26 ms |
| Local cold, corrected | 30 | 259.46 ms | 283.28 ms | 258.32 ms | 281.33 ms |

Locally, current warm saves only **63.93 ms (24.7%)** and cached-ID warm saves
76.00 ms (29.4%). This is much less compelling than Cloud because local
artifact access and VM configuration are cheaper while the nested warm actor
graph remains expensive.

## Decision

**Keeping isolates warm wins against the implementation that exists today, but
the benchmark does not show that warm actors are intrinsically necessary.**

- Today in Cloud, current warm saves about 375 ms of server time; the trivial
  app-ID cache raises that to 422 ms. That is a material user-facing win.
- The true V8 startup floor is about 2.5 ms. Most cold time is guest runtime
  bootstrap, request dispatch, control-plane artifact reads, and teardown—all
  optimizable implementation costs.
- The warm design misses the requested 10 ms target by 19–24x because it turns
  one request into several public gateway round trips.
- The warmed VM streaming path has a production-blocking reliability issue:
  after roughly 200–256 completed streams, `vmFetchStreamStart` returns
  `internal_error` and later calls time out while actor health remains live.

So the justified near-term choice is to keep warm execution while collapsing
its actor hops and fixing stream exhaustion. In parallel, a direct
isolate-per-request prototype that avoids the 93–275 ms guest Node bootstrap is
worth building: if it approaches the measured single-digit-millisecond native
VM floor, the complexity argument can reverse.

## Reliability and operational fixes made

- Fixed one-replica rolling replacement. Previously
  `minReplicas == maxReplicas == 1` blocked both warming the replacement and
  removing the failed replica. A draining replacement may now exceed the cap by
  exactly one until the new replica is ready, after which the old one is
  destroyed. Replacement runs as a kept-awake background lifecycle operation,
  so the drain action does not outlive the public request timeout.
- Gave each execution replica its own shared AgentOS sidecar pool. On the
  one-CPU Compute container, the default shared pool admitted only one active
  V8 executor; the draining VM occupied it and warming the replacement failed
  with `ERR_AGENTOS_VM_EXECUTOR_LIMIT`. Separate per-replica pools permit the
  brief overlap required for a safe handoff.
- Verified the complete fix in the deployed app: the drain action returned in
  **66.19 ms**, the original warm replica continued serving while its
  replacement warmed, and the replacement became the sole ready replica after
  **24.905 s**. A subsequent 20-request steady-state check used only that
  replica, reported zero cold starts, and measured **213.71 ms p50** for the
  current warm server path and **191.91 ms p50** with the app actor ID cached.
- Removed three Cloud-only correctness traps encountered during qualification:
  the literal pseudo-region `"default"` is no longer sent as an explicit actor
  placement, the isolated build output is made writable by the sidecar uid, and
  long-lived warm replicas receive a bounded 24-hour cumulative guest-JavaScript
  CPU budget instead of exhausting the short default after ordinary traffic.
- Changed benchmark setup to start deployment asynchronously and expose setup
  status. A long app build can outlive Rivet Run's request timeout; the previous
  endpoint returned 504 even though deployment completed successfully.
- Added opt-in AgentOS VM phase tracing and sidecar-stderr forwarding in the
  separate AgentOS workspace. Default logging behavior is unchanged.

## Deployment and reproduction

- Required Cloud namespace: `dynamic-apps-ben-562e-production-sqac`
- Effective Engine namespace: `dynamic-apps-ben-562e-dynamic-apps-ben-3335`
- Public app: `https://dynamic-apps-ben-562e-dynamic-apps-ben-3335.rivet.run`
- 100/100/100/30 qualification fixture release:
  `2f022627fd95df75220f3f6547e74227a733a3f59c78628fda9544491163bbd6`
- Current live post-fix fixture release:
  `e7d02a9f1bbbba891b5a7597fc897c8d7c5f74f0d4025532280ac2508430c6f0`
- Current image: tag `1787684435`, digest
  `sha256:72f18e237beaf8a0f41e81a5614ffa3bbfad17cea4b9e9742c514b97bdcb6f9a`
- Fixture artifact: 7,459 bytes, one persisted chunk

```sh
pnpm --filter @rivet-dev/dynamic-apps-benchmarks benchmark:local

BENCH_BASE_URL=https://dynamic-apps-ben-562e-dynamic-apps-ben-3335.rivet.run \
BENCH_SEQUENTIAL_REQUESTS=100 \
BENCH_WARM_REQUESTS=100 \
BENCH_CASES=noopSequential,warmSequential,warmDirectSequential,coldSequential \
pnpm --filter @rivet-dev/dynamic-apps-benchmarks benchmark:suite

BENCH_BASE_URL=https://dynamic-apps-ben-562e-dynamic-apps-ben-3335.rivet.run \
BENCH_PROFILE=routing \
BENCH_SEQUENTIAL_REQUESTS=200 \
pnpm --filter @rivet-dev/dynamic-apps-benchmarks benchmark:suite

npx @rivetkit/cli deploy \
  --namespace dynamic-apps-ben-562e-production-sqac \
  --env PORT=3000 \
  --env BENCH_COLD_CONCURRENCY=1 \
  --yes
```

The `routing` benchmark profile runs the direct/query/skip-ready/nested actor
matrix. The full profile defaults to 80 current-warm, 80 cached-ID warm, and 32
concurrent warm requests so it stays below the known stream-exhaustion
threshold. Larger stress runs are explicit through `BENCH_WARM_REQUESTS` and
`BENCH_WARM_CONCURRENT_REQUESTS`.
