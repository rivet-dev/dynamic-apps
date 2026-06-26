// Guest-side SharedArrayBuffer ring transport (T1), byte-compatible with the kernel-side
// crates/sidecar/src/sab_ring.rs. The guest produces REQUESTS into a G->K ring (kernel reads via Rust
// SabRingReader) and consumes RESPONSES from a K->G ring (kernel wrote via Rust SabRingWriter).
//
// Layout (matches Rust): header [producer:u32 LE @0][consumer:u32 LE @4][doorbell:u32 @8][seq:u32 @12] = 16 bytes,
// then a data region of exactly ringSize bytes. Records are [len:u32 LE][payload]. All little-endian to match
// Rust u32::from_le_bytes / to_le_bytes. Indices are byte offsets into the data region in [0, ringSize).

export const HEADER_LEN = 16;
const PRODUCER_OFF = 0;
const CONSUMER_OFF = 4;
export const MAX_RECORD_BYTES = 1 << 20; // 1 MiB, matches Rust MAX_RECORD_BYTES

function readU32LE(view, off) {
  return view.getUint32(off, true);
}
function writeU32LE(view, off, val) {
  view.setUint32(off, val >>> 0, true);
}

// Guest WRITER for the guest->kernel request ring. The guest owns producerIndex (here, authoritative, published at
// PRODUCER_OFF). The kernel publishes its consumer at CONSUMER_OFF; the guest reads it only for backpressure.
export class RingWriter {
  constructor(sab, ringSize) {
    this.ringSize = ringSize;
    this.producer = 0;
    this.head = new DataView(sab, 0, HEADER_LEN);
    this.data = new Uint8Array(sab, HEADER_LEN, ringSize);
  }

  _writeWrapped(start, bytes) {
    const first = Math.min(this.ringSize - start, bytes.length);
    this.data.set(bytes.subarray(0, first), start);
    if (first < bytes.length) this.data.set(bytes.subarray(first), 0);
  }

  // Returns true if written, false on backpressure (ring full; retry after the kernel drains).
  write(payload) {
    if (payload.length > MAX_RECORD_BYTES) throw new Error('record exceeds MAX_RECORD_BYTES');
    const need = 4 + payload.length;
    const consumer = readU32LE(this.head, CONSUMER_OFF) % this.ringSize; // peer-owned
    const used = (this.producer + this.ringSize - consumer) % this.ringSize;
    const free = this.ringSize - 1 - used; // leave one byte so producer==consumer means empty
    if (need > free) return false;
    const lenBytes = new Uint8Array(4);
    new DataView(lenBytes.buffer).setUint32(0, payload.length, true);
    this._writeWrapped(this.producer, lenBytes);
    this._writeWrapped((this.producer + 4) % this.ringSize, payload);
    this.producer = (this.producer + need) % this.ringSize;
    writeU32LE(this.head, PRODUCER_OFF, this.producer); // publish for the kernel
    return true;
  }
}

// Guest READER for the kernel->guest response ring. The guest owns consumerIndex (here) + publishes it at
// CONSUMER_OFF for the kernel's backpressure. The kernel publishes its producer at PRODUCER_OFF.
export class RingReader {
  constructor(sab, ringSize) {
    this.ringSize = ringSize;
    this.consumer = 0;
    this.head = new DataView(sab, 0, HEADER_LEN);
    this.data = new Uint8Array(sab, HEADER_LEN, ringSize);
  }

  _readWrapped(start, len) {
    const out = new Uint8Array(len);
    const first = Math.min(this.ringSize - start, len);
    out.set(this.data.subarray(start, start + first), 0);
    if (first < len) out.set(this.data.subarray(0, len - first), first);
    return out;
  }

  // Returns the payload Uint8Array, or null if no complete record is available.
  read() {
    const producer = readU32LE(this.head, PRODUCER_OFF) % this.ringSize;
    const avail = (producer + this.ringSize - this.consumer) % this.ringSize;
    if (avail < 4) return null;
    const lenBytes = this._readWrapped(this.consumer, 4);
    const len = new DataView(lenBytes.buffer, lenBytes.byteOffset, 4).getUint32(0, true);
    if (len > MAX_RECORD_BYTES || 4 + len > avail) return null; // defensive (kernel is trusted, but bound anyway)
    const payload = this._readWrapped((this.consumer + 4) % this.ringSize, len);
    this.consumer = (this.consumer + 4 + len) % this.ringSize;
    writeU32LE(this.head, CONSUMER_OFF, this.consumer); // publish for the kernel
    return payload;
  }
}
