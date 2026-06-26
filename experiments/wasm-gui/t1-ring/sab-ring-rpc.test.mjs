// End-to-end T1 ring protocol test. The "host" uses the symmetric JS ring (byte-compatible with the Rust
// SabRingReader/Writer) on the other end, so a green run validates the whole request->service->response cycle.
// Run: node sab-ring-rpc.test.mjs
import { RingChannel } from './sab-ring-rpc.mjs';
import { RingReader, RingWriter, HEADER_LEN } from './sab-ring.mjs';
import assert from 'node:assert';

const RING = 64;
const enc = (s) => new TextEncoder().encode(s);
const dec = (b) => new TextDecoder().decode(b);
const newSab = () => new SharedArrayBuffer(HEADER_LEN + RING);

// Build a guest RingChannel + a persistent mock host over the same two rings. The host reads requests (G->K) and
// writes responses (K->G); both keep their own persistent indices, like the real Rust kernel side.
function pair(transform) {
  const reqSab = newSab();
  const respSab = newSab();
  const chan = new RingChannel(reqSab, respSab, RING);
  const hostReader = new RingReader(reqSab, RING);
  const hostWriter = new RingWriter(respSab, RING);
  const serviceHost = () => {
    let req;
    while ((req = hostReader.read()) !== null) {
      const resp = transform(req);
      while (!hostWriter.write(resp)) { /* response ring sized for one in-flight reply */ }
    }
  };
  return { chan, serviceHost };
}

// 1. Round-trip: "ping" -> host prefixes "echo:" -> guest reads "echo:ping".
{
  const { chan, serviceHost } = pair((req) => enc('echo:' + dec(req)));
  assert.strictEqual(dec(chan.rpc(enc('ping'), serviceHost)), 'echo:ping');
}

// 2. Many sequential RPCs over the SAME channel (persistent indices) force wrap-around and stay correct + ordered.
{
  const { chan, serviceHost } = pair((req) => req); // echo verbatim
  for (let i = 0; i < 200; i++) {
    const msg = enc('msg-' + i);
    assert.deepStrictEqual([...chan.rpc(msg, serviceHost)], [...msg], `RPC ${i}`);
  }
}

// 3. Larger payload that still fits a single record round-trips.
{
  const { chan, serviceHost } = pair((req) => req);
  const big = new Uint8Array(40).fill(0xAB);
  assert.deepStrictEqual([...chan.rpc(big, serviceHost)], [...big]);
}

// 4. Realistic load: a 4 KiB control ring, 2000 RPCs with payloads 1B..near-capacity, varied each time to force
//    tight wrap-arounds + backpressure -- validates the ring at realistic kernel-forwarded RPC sizes.
{
  const BIGRING = 4096;
  const reqSab = new SharedArrayBuffer(HEADER_LEN + BIGRING);
  const respSab = new SharedArrayBuffer(HEADER_LEN + BIGRING);
  const chan = new RingChannel(reqSab, respSab, BIGRING);
  const hr = new RingReader(reqSab, BIGRING);
  const hw = new RingWriter(respSab, BIGRING);
  const host = () => { let q; while ((q = hr.read()) !== null) { while (!hw.write(q)) { /* echo */ } } };
  let state = 12345;
  const rnd = () => (state = (state * 1103515245 + 12345) & 0x7fffffff);
  for (let i = 0; i < 2000; i++) {
    const len = 1 + (rnd() % chan.maxPayload());
    const msg = new Uint8Array(len);
    for (let j = 0; j < len; j++) msg[j] = (i + j) & 0xff;
    const got = chan.rpc(msg, host);
    assert.strictEqual(got.length, len, `RPC ${i} length`);
    assert.strictEqual(got[0], msg[0], `RPC ${i} first byte`);
    assert.strictEqual(got[len - 1], msg[len - 1], `RPC ${i} last byte`);
  }
}

// 5. Over-capacity request is rejected UPFRONT (the ring/dataBuffer size-split), not spun into a hang.
{
  const { chan, serviceHost } = pair((req) => req);
  const tooBig = new Uint8Array(RING); // RING(64) > maxPayload (RING-5=59)
  assert.throws(() => chan.rpc(tooBig, serviceHost), /exceeds ring capacity/, 'over-capacity must reject upfront');
}

console.log('sab-ring-rpc: round-trip + 200 seq + large + 2000 realistic-load + size-split-guard passed (T1 ring)');
