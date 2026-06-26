// Node test for the guest-side T1 ring. Run: node sab-ring.test.mjs
import { RingWriter, RingReader, HEADER_LEN } from './sab-ring.mjs';
import assert from 'node:assert';

const RING = 64;
const enc = (s) => new TextEncoder().encode(s);
const newSab = () => new SharedArrayBuffer(HEADER_LEN + RING);

// 1. Roundtrip: writer produces, reader consumes, in order, then empty.
{
  const sab = newSab();
  const w = new RingWriter(sab, RING);
  const r = new RingReader(sab, RING);
  assert.strictEqual(w.write(enc('hello')), true);
  assert.strictEqual(w.write(enc('w2')), true);
  assert.deepStrictEqual([...r.read()], [...enc('hello')]);
  assert.deepStrictEqual([...r.read()], [...enc('w2')]);
  assert.strictEqual(r.read(), null);
}

// 2. Backpressure: with the reader publishing its consumer, the writer fills then resumes as space frees.
{
  const sab = newSab();
  const w = new RingWriter(sab, RING);
  const r = new RingReader(sab, RING);
  let written = 0;
  while (w.write(new Uint8Array(8))) written++;
  assert.strictEqual(written, 5); // 63 usable / (4+8) per record = 5
  assert.notStrictEqual(r.read(), null); // drain one -> publishes consumer
  assert.strictEqual(w.write(new Uint8Array(8)), true); // space freed
}

// 3. Wrap: drive the consumer near the end so a record wraps, and confirm it reads back intact.
{
  const sab = newSab();
  const w = new RingWriter(sab, RING);
  const r = new RingReader(sab, RING);
  // 4 records of 11-byte payload = 15 bytes each => producer at 60 after 4. read 4 to move consumer to 60.
  for (let i = 0; i < 4; i++) assert.strictEqual(w.write(new Uint8Array(11).fill(i)), true);
  for (let i = 0; i < 4; i++) assert.deepStrictEqual([...r.read()], [...new Uint8Array(11).fill(i)]);
  // now producer=consumer=60; write a record that wraps (len@60..64, payload wraps to 0..)
  assert.strictEqual(w.write(enc('WRAP')), true);
  assert.deepStrictEqual([...r.read()], [...enc('WRAP')]);
}

// 4. Byte-layout compatibility with the Rust kernel reader: a known record's exact bytes + producer index.
{
  const sab = newSab();
  const w = new RingWriter(sab, RING);
  w.write(enc('Hi')); // expect [len=2 LE][0x48 'H'][0x69 'i'] at data[0..6]
  const data = new Uint8Array(sab, HEADER_LEN, 6);
  assert.deepStrictEqual([...data], [2, 0, 0, 0, 0x48, 0x69], 'record layout must match Rust [len:u32 LE][payload]');
  const head = new DataView(sab, 0, HEADER_LEN);
  assert.strictEqual(head.getUint32(PRODUCER_OFF_TEST(), true), 6, 'producer index published LE at offset 0');
}
function PRODUCER_OFF_TEST() { return 0; }

console.log('sab-ring guest JS: all 4 test groups passed (roundtrip, backpressure, wrap, Rust byte-layout match)');
