# Rivet Dynamic Apps

> [!WARNING]
> Dynamic Apps currently supports only Rivet 2.x. Rivet 3.x support is coming
> soon.

Build user-generated request handlers in a sandboxed deployment VM, keep their
release state in a Rivet actor, and serve cache-hit requests from bounded local
V8 isolates without routing through execution actors.

See the [package guide](packages/dynamic-apps/README.md), the
[API contract](packages/dynamic-apps/API_CONTRACT.md), and the
[benchmark report](benchmarks/dynamic-apps/RESULTS.md).

RivetKit reference: https://rivet.dev/llms.txt
