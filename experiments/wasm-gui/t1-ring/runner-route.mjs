// Guest-side T1 sync-RPC routing decision. In the real wasm runner (the embedded JS in crates/execution/src/
// wasm.rs), the guest calls syncRpc(method, args) for every kernel-forwarded operation. This module is the routing
// policy the runner embeds: when a T1 RingChannel is present AND the encoded request fits the ring's single-record
// capacity, route through the ring (the fast SAB path that replaces base64-over-postMessage); otherwise fall back to
// the legacy transport (base64 / the bulk dataBuffer SAB) for oversized payloads or when no ring was provisioned.
//
// Kept pure + dependency-free (node-testable without node_modules) so the routing contract is verified independently
// of the full runtime build. The runner wires the real encodeRequest/decodeResponse/fallback/serviceHost; this only
// owns the decision + the ring round-trip.

// `ringChannel` is a RingChannel (sab-ring-rpc.mjs) or null. `encodeRequest(method,args) -> Uint8Array`,
// `decodeResponse(Uint8Array) -> value`, `fallback(method,args) -> value` (legacy path), `serviceHost()` is the
// runtime's progress hook (the existing synthetic-wait poll yield -- no new doorbell, Phase 1).
export function makeSyncRpcRouter({ ringChannel, encodeRequest, decodeResponse, fallback, serviceHost }) {
  if (typeof encodeRequest !== 'function' || typeof decodeResponse !== 'function' || typeof fallback !== 'function') {
    throw new Error('makeSyncRpcRouter: encodeRequest/decodeResponse/fallback are required');
  }
  let ringCalls = 0;
  let fallbackCalls = 0;
  const syncRpc = (method, args) => {
    if (ringChannel) {
      const reqBytes = encodeRequest(method, args);
      // The ring carries one record at a time; oversized requests go on the legacy/bulk path (the ring would
      // reject them upfront anyway -- see RingChannel.maxPayload()).
      if (reqBytes.length <= ringChannel.maxPayload()) {
        const respBytes = ringChannel.rpc(reqBytes, serviceHost);
        ringCalls++;
        return decodeResponse(respBytes);
      }
    }
    fallbackCalls++;
    return fallback(method, args);
  };
  // Expose counters so the runtime can confirm the fast path is actually taken (and for the measurement).
  syncRpc.stats = () => ({ ringCalls, fallbackCalls });
  return syncRpc;
}
