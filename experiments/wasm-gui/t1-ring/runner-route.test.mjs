// Node test for the guest-side T1 routing contract. Run: node runner-route.test.mjs
import { makeSyncRpcRouter } from './runner-route.mjs';
import { RingChannel } from './sab-ring-rpc.mjs';
import { HEADER_LEN, RingReader, RingWriter } from './sab-ring.mjs';
import assert from 'node:assert';

const RING = 64;
const enc = (s) => new TextEncoder().encode(s);
const dec = (b) => new TextDecoder().decode(b);

// A persistent mock kernel over the same two rings (echoes "K:" + request), like the Rust kernel side.
function ringPair() {
  const reqSab = new SharedArrayBuffer(HEADER_LEN + RING);
  const respSab = new SharedArrayBuffer(HEADER_LEN + RING);
  const chan = new RingChannel(reqSab, respSab, RING);
  const hr = new RingReader(reqSab, RING);
  const hw = new RingWriter(respSab, RING);
  const serviceHost = () => {
    let q;
    while ((q = hr.read()) !== null) {
      const resp = enc('K:' + dec(q));
      while (!hw.write(resp)) { /* one in-flight */ }
    }
  };
  return { chan, serviceHost };
}

const encodeRequest = (method, args) => enc(method + '|' + (args ?? ''));
const decodeResponse = (b) => dec(b);

// 1. With a ring + a small request, route through the ring (fast path), not the fallback.
{
  const { chan, serviceHost } = ringPair();
  let fbCalls = 0;
  const fallback = () => { fbCalls++; return 'FB'; };
  const syncRpc = makeSyncRpcRouter({ ringChannel: chan, encodeRequest, decodeResponse, fallback, serviceHost });
  const out = syncRpc('x.poll', 'a'); // encoded "x.poll|a" = 8 bytes <= maxPayload(59)
  assert.strictEqual(out, 'K:x.poll|a', 'small request must round-trip through the ring');
  assert.strictEqual(fbCalls, 0, 'fallback must NOT be used for the fast path');
  assert.deepStrictEqual(syncRpc.stats(), { ringCalls: 1, fallbackCalls: 0 });
}

// 2. Oversized request (> ring maxPayload) falls back to the legacy/bulk path instead of hanging.
{
  const { chan, serviceHost } = ringPair();
  let fbArgs = null;
  const fallback = (m, a) => { fbArgs = [m, a]; return 'BULK'; };
  const syncRpc = makeSyncRpcRouter({ ringChannel: chan, encodeRequest, decodeResponse, fallback, serviceHost });
  const big = 'y'.repeat(200); // encoded > maxPayload(59) -> must fall back
  assert.strictEqual(syncRpc('fb.write', big), 'BULK', 'oversized must use the fallback');
  assert.deepStrictEqual(fbArgs, ['fb.write', big]);
  assert.deepStrictEqual(syncRpc.stats(), { ringCalls: 0, fallbackCalls: 1 });
}

// 3. No ring provisioned (flag off) -> always fall back, every call.
{
  let fbCalls = 0;
  const fallback = () => { fbCalls++; return 'LEGACY'; };
  const syncRpc = makeSyncRpcRouter({ ringChannel: null, encodeRequest, decodeResponse, fallback });
  assert.strictEqual(syncRpc('a', '1'), 'LEGACY');
  assert.strictEqual(syncRpc('b', '2'), 'LEGACY');
  assert.strictEqual(fbCalls, 2);
  assert.deepStrictEqual(syncRpc.stats(), { ringCalls: 0, fallbackCalls: 2 });
}

// 4. Mixed stream over one router: small->ring, big->fallback, interleaved, all correct + counted.
{
  const { chan, serviceHost } = ringPair();
  const fallback = (m) => 'FB:' + m;
  const syncRpc = makeSyncRpcRouter({ ringChannel: chan, encodeRequest, decodeResponse, fallback, serviceHost });
  for (let i = 0; i < 50; i++) {
    if (i % 5 === 0) {
      assert.strictEqual(syncRpc('big', 'z'.repeat(100)), 'FB:big');
    } else {
      assert.strictEqual(syncRpc('p', String(i)), 'K:p|' + i);
    }
  }
  const s = syncRpc.stats();
  assert.strictEqual(s.ringCalls, 40);
  assert.strictEqual(s.fallbackCalls, 10);
}

console.log('runner-route: fast-path + oversized-fallback + no-ring-fallback + mixed-stream passed (T1 guest routing)');
