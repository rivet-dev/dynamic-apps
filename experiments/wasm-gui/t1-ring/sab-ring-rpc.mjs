// Guest-side sync-RPC over the T1 SAB rings. The RingWriter/RingReader hold the producer/consumer indices in
// their own state (not re-read from the SAB), so they MUST persist across RPCs -- one RingChannel per guest, not a
// fresh writer/reader per call. (A fresh writer would restart its producer at 0 and desync from the kernel's
// accumulated consumer position.)
//
// `serviceHost` is the runtime hook that lets the host make progress between spins. In the real runtime it is the
// existing synthetic-wait poll yield after which the sidecar has drained the request ring + filled the response
// ring; in tests it is a direct callback running the mock host. No new doorbell (Phase 1).
import { RingWriter, RingReader } from './sab-ring.mjs';

export class RingChannel {
  // reqSab = the guest->kernel request ring; respSab = the kernel->guest response ring.
  constructor(reqSab, respSab, ringSize) {
    this.writer = new RingWriter(reqSab, ringSize); // persistent producer index
    this.reader = new RingReader(respSab, ringSize); // persistent consumer index
  }

  // Largest payload a single record can ever hold in this ring: ring_size - 1 (the reserved empty/full byte) - 4
  // (the u32 length prefix). Payloads above this go on the bulk dataBuffer SAB path, not the control ring.
  maxPayload() {
    return this.writer.ringSize - 5;
  }

  // Blocking request/response. Returns the response payload Uint8Array. Rejects an over-capacity request UPFRONT
  // (it could never fit, so spinning would hang) -- callers route large payloads through the bulk dataBuffer path.
  rpc(requestBytes, serviceHost, maxSpins = 1_000_000) {
    if (requestBytes.length > this.maxPayload()) {
      throw new Error(
        `T1 ring: request ${requestBytes.length}B exceeds ring capacity ${this.maxPayload()}B ` +
          `(use the bulk dataBuffer path for large payloads)`,
      );
    }
    let spins = 0;
    while (!this.writer.write(requestBytes)) {
      serviceHost();
      if (++spins > maxSpins) throw new Error('T1 ring: request write stalled (host not draining)');
    }
    let resp = null;
    spins = 0;
    while ((resp = this.reader.read()) === null) {
      serviceHost();
      if (++spins > maxSpins) throw new Error('T1 ring: response read stalled (host not responding)');
    }
    return resp;
  }
}
