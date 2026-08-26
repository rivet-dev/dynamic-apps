# Direct-isolate Dynamic Apps benchmark

Qualified 2026-08-26 (US/Pacific). The rewritten architecture keeps the
durable per-app Rivet actor for releases and invalidation, but removes scaler
and execution actors from ordinary HTTP:

```text
deploy: deployApp -> app state actor -> AgentOS build VM -> AOSP release

first direct request:
  client -> Compute -> state actor -> manifest/chunks -> snapshot/pool -> app

warm direct request:
  client -> Compute -> local verified runtime -> V8 isolate -> app
  (zero actor calls)

app-defined actor request:
  client -> Compute -> api.rivet.dev gateway -> app actor
         -> authenticated serverless callback -> bounded local worker
```

The final Compute image is tag `1787733972`, digest
`sha256:a06d89124e5d7def1ecf2ad049cec26ca68be57d0e1fd487a0789e8053da1e85`.
It ran with 1 vCPU, 2 GiB memory, min/max scale 1, instance concurrency 80,
direct isolate pool 2, actor-worker cache 4, and actor-worker heap limit 96 MiB.
Every Cloud mutation targeted only
`dynamic-apps-ben-562e-production-sqac`.

## Decision

**Use the direct local-isolate path for ordinary request/response. Keep Rivet
actors for durable state and app-defined actor semantics.**

The unavoidable fresh V8-isolate floor is about **4.7 ms**, while a reused clean
native isolate serves the trivial fixture in about **2.0–2.3 ms** of server
time. A warm app-defined actor action costs about **39 ms** inside the Compute
server and **265 ms** end-to-end from the benchmark client. The actor topology
therefore adds substantially more latency than creating a fresh direct isolate;
its complexity is justified only when actor state/events/connections are the
feature, not as a cache for stateless HTTP execution.

Public Rivet Run ingress from this client is roughly 190–220 ms at low load, so
outer latency hides the architectural difference. Server timing headers are
the relevant comparison.

## Final Rivet Compute direct results

The workload is a zero-dependency JSON `fetch()` handler. Initialization and
warm-up requests are excluded from the steady samples.

| Mode | Samples | Outer p50 | Outer p95 | Server p50 | Server p95 | Isolate create p50 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Prewarm, pool 2 | 4 | 185.61 ms | 205.26 ms | **2.32 ms** | **2.53 ms** | n/a |
| Snapshot per request | 4 | 201.38 ms | 218.26 ms | **6.45 ms** | **7.55 ms** | 3.92 ms |
| Fresh compile per request | 2 | 195.88 ms | 199.93 ms | **7.06 ms** | **7.58 ms** | **4.73 ms** |
| Edge no-op | 4 | 220.86 ms | 291.05 ms | 0.08 ms | 0.10 ms | n/a |

The first request after a forced empty cache separated the optimizable phases:

| Phase | First prewarm request |
| --- | ---: |
| State actor connect | 57.28 ms |
| State actor resolve | 66.36 ms |
| Artifact manifest | 37.65 ms |
| Artifact download | 36.40 ms |
| Artifact parse | 0.61 ms |
| Snapshot creation | 17.38 ms |
| Fill two-isolate pool | 8.79 ms |
| Complete server initialization/request | 232.95 ms |

Download is intentionally measured but not optimized here. More importantly,
it is absent on cache hits and is not the performance floor: fresh native
isolate creation is only a few milliseconds.

### 10,000-request stability

Pool 2, concurrency 32:

| Metric | Result |
| --- | ---: |
| Successful responses | **10,000 / 10,000** |
| Throughput | 103.63 req/s |
| Outer p50 / p95 / p99 | 291.49 / 468.98 / 607.30 ms |
| Server p50 / p95 / p99 | **2.04 / 25.69 / 70.07 ms** |
| Context reset p50 / p95 | 1.09 / 2.57 ms |
| Overflow snapshot isolates | 2,123 |
| Reset failures | **0** |
| RSS before / after | 305.8 / 311.2 MB |
| Clean pool after drain | **2** |

The small pool deliberately trades burst-tail CPU for bounded idle memory.
Even with 21.23% overflow creation at concurrency 32, the stated server target
(p50 <= 25 ms, p95 <= 50 ms) passed and memory returned to its steady bound.

## App-defined actor results

The actor fixture uses ordinary RivetKit state, actions, an event subscription,
and a direct HTTP handler in the same deployment. Correctness passed locally
and on Compute: a new keyed actor transitioned **2 -> 5**, emitted event value
`2`, read back `5`, and the release continued serving direct HTTP.

The load endpoint deploys and creates the actor only during setup. Measured
requests call `get()` on the already-running actor, so these numbers contain no
deployment or actor-creation race.

| Environment/case | Success | Outer p50 | Outer p95 | Server action p50 | Server action p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Local, sequential 100 | 100% | 24.45 ms | 47.96 ms | 22.48 ms | 45.46 ms |
| Local, c16 / 256 | 100% | 63.08 ms | 151.57 ms | 58.78 ms | 102.19 ms |
| Compute, sequential 100 | **100%** | 264.65 ms | 352.48 ms | **38.94 ms** | **48.24 ms** |
| Compute, c16 / 256 | **100%** | 293.02 ms | 652.26 ms | **39.72 ms** | **57.02 ms** |

The server-side actor action is about 17x the prewarmed direct-isolate p50 and
about 5.5x the complete fresh-isolate server p50. Public ingress adds another
roughly 225 ms at the median. This confirms that the earlier actor-heavy design
was benchmarking network topology rather than a useful V8 warm-cache advantage.

## Local direct baseline

The same direct executor on a local Engine completed a 10,000-request pool-2,
concurrency-2 run at 100% success:

| Metric | Result |
| --- | ---: |
| Outer p50 / p95 | 2.16 / 3.39 ms |
| Server p50 / p95 | 0.68 / 1.39 ms |
| Context reset p50 / p95 | 0.42 / 0.96 ms |
| Native isolate creates | 2 |
| RSS before / after | 234.6 / 481.6 MB |

A pool-1, concurrency-1 sample measured outer 2.28 / 3.61 ms and server
0.70 / 1.50 ms (p50/p95). These results establish that the direct executor is
not intrinsically slow; Cloud outer latency is ingress rather than isolate work.

## Reliability findings and fixes

- The benchmark originally redeployed lazily in every Compute process. Under
  multi-region concurrency, duplicate `getOrCreate` calls raced over the stable
  app actor's datacenter and produced `key_reserved_in_different_datacenter`.
  Setup now owns deployment/actor creation, warm load uses `get()`, and
  `deployApp` prefers an existing actor before its compatible getOrCreate
  fallback.
- Actor endpoint URL auth is split into explicit endpoint, namespace, and
  publishable token. This removed duplicate-namespace validation failures and
  prevents a host secret from entering the app worker.
- Actor correctness and both actor load cases pass on the final image. The
  forced first serverless wake can pay a runner cold start; it is excluded from
  the warm measurements requested here.
- Direct cache-hit serving makes exactly zero actor calls. Release events
  invalidate the mapping preemptively; reconnect also invalidates before a
  durable re-resolve.

## Platform and deployment caveats

Compute's injected secret token could not list datacenters or update runner
configs. Cloud qualification therefore used a short-lived access token only as
`DYNAMIC_APPS_CONTROL_TOKEN`. A separate Rivet platform revision
`nvzuskkn` (`fix(cloud): allow secret tokens to configure runner pools`) adds
the required secret ACLs and one-time reconciliation for existing pools. It was
typechecked but was **not** deployed to production from this benchmark task.

The Compute CLI also regenerates and pushes an 836 MB dependency layer for a
source-only change (about 3 GB uncompressed). That affects deployment time, not
request latency, and should be fixed independently.

## Reproduction

```sh
BENCH_PROFILE=actors \
pnpm --filter @rivet-dev/dynamic-apps-benchmarks benchmark:local -- --host 0.0.0.0

BENCH_BASE_URL=https://dynamic-apps-ben-562e-dynamic-apps-ben-3335.rivet.run \
BENCH_PROFILE=actors \
pnpm --filter @rivet-dev/dynamic-apps-benchmarks benchmark:suite

BENCH_BASE_URL=https://dynamic-apps-ben-562e-dynamic-apps-ben-3335.rivet.run \
BENCH_PROFILE=stability \
BENCH_STABILITY_CONCURRENCY=32 \
BENCH_STABILITY_REQUESTS=10000 \
pnpm --filter @rivet-dev/dynamic-apps-benchmarks benchmark:suite
```

The public Compute URL is
`https://dynamic-apps-ben-562e-dynamic-apps-ben-3335.rivet.run`. The namespace
display name required by every deploy command remains
`dynamic-apps-ben-562e-production-sqac`.
