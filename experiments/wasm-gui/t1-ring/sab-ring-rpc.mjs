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

  // Blocking request/response. Returns the response payload Uint8Array.
  rpc(requestBytes, serviceHost, maxSpins = 1_000_000) {
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
