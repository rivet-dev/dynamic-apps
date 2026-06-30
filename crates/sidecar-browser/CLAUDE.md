# Browser Support

- Browser support is untested after the secure-exec split: the scaffold's service logic (extension dispatch, VM filesystem context, worker lifecycle) is covered by smoke/bridge/service tests against a mock BrowserWorkerBridge; end-to-end browser/worker-transport integration is still unvalidated.
- Provenance: moved from rivet-dev/agentos@87ed8e21e454.
- Keep the browser sidecar separate from the native sidecar because worker transport and main-thread ownership differ from stdio/socket transport.
