//! Kernel-side reader for the T1 guest→kernel SharedArrayBuffer ring transport.
//!
//! Security model (see experiments/wasm-gui/T1-SAB-RING-SECURITY-DESIGN.md): the guest writes records and the
//! producer index into a SAB the KERNEL allocated. Every guest-written field (`producer_index`, each record
//! `len`, the payload bytes) is HOSTILE. The kernel owns `ring_size` and `consumer_index` and never sources them
//! from the SAB. The reader's invariant: it never reads outside the kernel-allocated data region regardless of
//! hostile input, copies the payload OUT before any use (no double-fetch / TOCTOU), and caps record size.
//!
//! Layout: a 16-byte header [producer_index:u32][consumer_index:u32][doorbell:u32][seq:u32] followed by a data
//! region of exactly `ring_size` bytes. Records are `[len:u32][payload:len bytes]` at byte offsets in the data
//! region, wrap-aware. `producer_index`/`consumer_index` are byte offsets into the data region in [0, ring_size).

pub(crate) const SAB_RING_HEADER_LEN: usize = 16;
const PRODUCER_INDEX_OFFSET: usize = 0;
const CONSUMER_INDEX_OFFSET: usize = 4;

/// Hard cap on a single record's payload, independent of ring size, so a hostile `len` cannot drive a large
/// kernel allocation even if the guest also forges a consistent producer index.
pub(crate) const MAX_RECORD_BYTES: usize = 1 << 20; // 1 MiB

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum RingError {
    /// The kernel-allocated SAB is smaller than header + ring_size. A setup invariant violation (not guest input).
    BackingTooSmall,
    /// `ring_size` is zero or not within the SAB. Setup invariant violation.
    InvalidRingSize,
    /// A record `len` exceeds `MAX_RECORD_BYTES`. Hostile input; tear down the guest.
    RecordTooLarge,
    /// A record `len` claims more bytes than the guest has produced. Hostile/corrupt; tear down the guest.
    IncompleteRecord,
}

/// Kernel-owned reader state for one guest→kernel ring. `ring_size` and `consumer_index` live HERE, never in the
/// SAB, so the guest cannot move them.
pub(crate) struct SabRingReader {
    ring_size: u32,
    consumer_index: u32,
}

impl SabRingReader {
    /// `ring_size` is the kernel-chosen data-region size (the kernel allocated the SAB to fit header + ring_size).
    pub(crate) fn new(ring_size: u32) -> Result<Self, RingError> {
        if ring_size == 0 {
            return Err(RingError::InvalidRingSize);
        }
        Ok(Self { ring_size, consumer_index: 0 })
    }

    /// Read one u32 from the header (kernel-trusted offset within the header, but the VALUE is guest-written).
    fn read_header_u32(sab: &[u8], offset: usize) -> u32 {
        // offset is a fixed kernel constant < SAB_RING_HEADER_LEN; the caller has verified sab covers the header.
        u32::from_le_bytes([sab[offset], sab[offset + 1], sab[offset + 2], sab[offset + 3]])
    }

    /// Read `len` bytes from the data region starting at data-region byte offset `start` (wrap-aware). `start` and
    /// `len` are pre-validated to keep the access within [0, ring_size); this only handles the wrap split.
    fn read_data_wrapped(&self, data: &[u8], start: u32, len: u32) -> Vec<u8> {
        let start = start as usize;
        let len = len as usize;
        let ring = self.ring_size as usize;
        let mut out = Vec::with_capacity(len);
        let first = (ring - start).min(len);
        out.extend_from_slice(&data[start..start + first]);
        if first < len {
            out.extend_from_slice(&data[0..len - first]);
        }
        out
    }

    /// Read a little-endian u32 from the data region at wrap-aware offset `start` (start in [0, ring_size)).
    fn read_data_u32_wrapped(&self, data: &[u8], start: u32) -> u32 {
        let bytes = self.read_data_wrapped(data, start, 4);
        u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])
    }

    /// Attempt to read the next record. Returns the copied-out payload (kernel-private), advancing the
    /// kernel-owned consumer index. `Ok(None)` = no complete record available. `Err` = hostile/corrupt input;
    /// the caller MUST tear down the guest. The kernel allocated `sab`, so its length is trusted.
    pub(crate) fn read_record(&mut self, sab: &[u8]) -> Result<Option<Vec<u8>>, RingError> {
        let ring = self.ring_size as usize;
        if sab.len() < SAB_RING_HEADER_LEN + ring {
            return Err(RingError::BackingTooSmall);
        }
        let data = &sab[SAB_RING_HEADER_LEN..SAB_RING_HEADER_LEN + ring];

        // producer_index is guest-written and HOSTILE: defensively reduce mod ring_size so it is always a valid
        // data-region offset in [0, ring_size). consumer_index is kernel-owned and already in range.
        let producer = Self::read_header_u32(sab, PRODUCER_INDEX_OFFSET) % self.ring_size;
        let consumer = self.consumer_index;

        // available bytes the guest claims to have produced; bounded in [0, ring_size) by construction.
        let available = (producer + self.ring_size - consumer) % self.ring_size;
        if available < 4 {
            return Ok(None); // not even a length prefix yet
        }

        // Read the record length ONCE into a kernel local (no double-fetch). Then bound it before any use.
        let len = self.read_data_u32_wrapped(data, consumer);
        if len as usize > MAX_RECORD_BYTES {
            return Err(RingError::RecordTooLarge);
        }
        // The record (4-byte prefix + len payload) must fit within what the guest claims to have produced.
        // Since available < ring_size and this passes, 4 + len < ring_size, so the copy stays in the data region.
        if 4u32.saturating_add(len) > available {
            return Err(RingError::IncompleteRecord);
        }

        let payload_start = (consumer + 4) % self.ring_size;
        let payload = self.read_data_wrapped(data, payload_start, len);

        // Advance the kernel-owned consumer index past this record.
        self.consumer_index = (consumer + 4 + len) % self.ring_size;
        Ok(Some(payload))
    }

    /// Publish the kernel-owned consumer index into the SAB header so the guest (the ring's producer) can compute
    /// free space for backpressure. Write-only: the authoritative consumer stays in this struct and is NEVER read
    /// back from the SAB (the guest could corrupt it). Call after draining a batch of records.
    pub(crate) fn publish_consumer(&self, sab: &mut [u8]) {
        if sab.len() >= SAB_RING_HEADER_LEN {
            sab[CONSUMER_INDEX_OFFSET..CONSUMER_INDEX_OFFSET + 4]
                .copy_from_slice(&self.consumer_index.to_le_bytes());
        }
    }
}

/// Kernel-side WRITER for the kernel->guest response ring. The kernel owns `producer_index` (here, authoritative)
/// and publishes it to the SAB for the guest. The guest owns `consumer_index` (in the SAB), which is HOSTILE: the
/// kernel reads it only to compute free space for backpressure (defensively reduced mod ring_size) and never
/// trusts it to bound a write. A bogus consumer at worst makes the kernel see the ring as full (it stalls) or as
/// having space it doesn't (it overwrites the guest's own unread responses) -- the guest harming only itself,
/// never the kernel or another VM.
pub(crate) struct SabRingWriter {
    ring_size: u32,
    producer_index: u32,
}

impl SabRingWriter {
    pub(crate) fn new(ring_size: u32) -> Result<Self, RingError> {
        if ring_size == 0 {
            return Err(RingError::InvalidRingSize);
        }
        Ok(Self { ring_size, producer_index: 0 })
    }

    fn write_data_wrapped(&self, data: &mut [u8], start: u32, bytes: &[u8]) {
        let start = start as usize;
        let ring = self.ring_size as usize;
        let first = (ring - start).min(bytes.len());
        data[start..start + first].copy_from_slice(&bytes[..first]);
        if first < bytes.len() {
            data[0..bytes.len() - first].copy_from_slice(&bytes[first..]);
        }
    }

    /// Write one response record into the ring. Returns `Ok(true)` if written, `Ok(false)` if the ring lacks space
    /// (backpressure; the caller retries after the guest drains). The kernel is the sole producer; `payload` is
    /// kernel-trusted but capped at `MAX_RECORD_BYTES` defensively.
    pub(crate) fn write_record(&mut self, sab: &mut [u8], payload: &[u8]) -> Result<bool, RingError> {
        let ring = self.ring_size as usize;
        if sab.len() < SAB_RING_HEADER_LEN + ring {
            return Err(RingError::BackingTooSmall);
        }
        if payload.len() > MAX_RECORD_BYTES {
            return Err(RingError::RecordTooLarge);
        }
        let need = 4 + payload.len();

        // The guest-written consumer index is HOSTILE: defensively reduce mod ring_size so free-space math stays
        // bounded. The kernel owns producer_index in this struct.
        let consumer = SabRingReader::read_header_u32(sab, CONSUMER_INDEX_OFFSET) % self.ring_size;
        let producer = self.producer_index;
        let used = (producer + self.ring_size - consumer) % self.ring_size;
        // Leave one byte free so producer == consumer unambiguously means empty, not full.
        let free = (self.ring_size - 1 - used) as usize;
        if need > free {
            return Ok(false); // backpressure: ring full
        }

        let (header, data) = sab.split_at_mut(SAB_RING_HEADER_LEN);
        let data = &mut data[..ring];
        self.write_data_wrapped(data, producer, &(payload.len() as u32).to_le_bytes());
        self.write_data_wrapped(data, (producer + 4) % self.ring_size, payload);
        self.producer_index = (producer + need as u32) % self.ring_size;
        // Publish the kernel-owned producer index so the guest can read up to it.
        header[PRODUCER_INDEX_OFFSET..PRODUCER_INDEX_OFFSET + 4]
            .copy_from_slice(&self.producer_index.to_le_bytes());
        Ok(true)
    }
}

/// Kernel-side sync-RPC endpoint over the two T1 rings: reads requests from the guest->kernel ring and writes
/// responses into the kernel->guest ring. The Rust counterpart to the guest-side JS `RingChannel`. Holds both ring
/// indices, so it is persistent -- one `SabRingEndpoint` per guest, created at guest setup (NOT per syscall).
pub(crate) struct SabRingEndpoint {
    request_reader: SabRingReader,  // guest -> kernel (the kernel consumes)
    response_writer: SabRingWriter, // kernel -> guest (the kernel produces)
}

impl SabRingEndpoint {
    pub(crate) fn new(ring_size: u32) -> Result<Self, RingError> {
        Ok(Self {
            request_reader: SabRingReader::new(ring_size)?,
            response_writer: SabRingWriter::new(ring_size)?,
        })
    }

    /// Read the next pending request from the request ring (every guest-written field validated as HOSTILE), then
    /// publish the kernel consumer index so the guest's request writer can compute backpressure. `req_sab` is the
    /// request ring's (Shared)ArrayBuffer backing store. `Ok(None)` = no complete request pending.
    pub(crate) fn read_request(&mut self, req_sab: &mut [u8]) -> Result<Option<Vec<u8>>, RingError> {
        let record = self.request_reader.read_record(req_sab)?;
        if record.is_some() {
            self.request_reader.publish_consumer(req_sab);
        }
        Ok(record)
    }

    /// Write a response into the response ring's backing store. `Ok(false)` = ring full (backpressure; retry after
    /// the guest drains). `payload` is kernel-trusted, capped at `MAX_RECORD_BYTES`.
    pub(crate) fn write_response(&mut self, resp_sab: &mut [u8], payload: &[u8]) -> Result<bool, RingError> {
        self.response_writer.write_record(resp_sab, payload)
    }

    /// Service one pending request directly over raw (Shared)ArrayBuffer backing-store pointers, as obtained from
    /// V8 `get_backing_store()` (the bridge.rs:554-563 pattern). This centralizes + tests the single `unsafe` slice
    /// construction so the V8 integration layer stays a thin caller. Returns `Ok(true)` if a request was serviced
    /// and a response written, `Ok(false)` if no request was pending or the response ring was full (backpressure).
    ///
    /// # Safety
    /// `req_ptr`/`resp_ptr` must each be valid for reads+writes of `req_len`/`resp_len` bytes for the duration of
    /// the call, and not aliased by other live references. V8 backing stores satisfy this for the synchronous
    /// lifetime of the servicing call (the SAB outlives it). The guest may concurrently mutate the request SAB,
    /// which is exactly why `read_request` copies-out-then-validates every hostile field.
    pub(crate) unsafe fn service_from_raw(
        &mut self,
        req_ptr: *mut u8,
        req_len: usize,
        resp_ptr: *mut u8,
        resp_len: usize,
        service: impl FnOnce(&[u8]) -> Vec<u8>,
    ) -> Result<bool, RingError> {
        let req_sab = std::slice::from_raw_parts_mut(req_ptr, req_len);
        let Some(request) = self.read_request(req_sab)? else {
            return Ok(false);
        };
        let response = service(&request);
        let resp_sab = std::slice::from_raw_parts_mut(resp_ptr, resp_len);
        self.write_response(resp_sab, &response)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const RING: u32 = 64;

    /// Build a SAB with a header (producer index set) + data region of RING bytes preloaded with `data`.
    fn sab_with(producer: u32, data: &[u8]) -> Vec<u8> {
        let mut sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        sab[0..4].copy_from_slice(&producer.to_le_bytes());
        sab[SAB_RING_HEADER_LEN..SAB_RING_HEADER_LEN + data.len()].copy_from_slice(data);
        sab
    }

    /// Encode a record [len][payload] into a fresh data region starting at offset 0.
    fn record(payload: &[u8]) -> Vec<u8> {
        let mut d = Vec::new();
        d.extend_from_slice(&(payload.len() as u32).to_le_bytes());
        d.extend_from_slice(payload);
        d
    }

    #[test]
    fn reads_a_valid_record() {
        let rec = record(b"hello");
        let sab = sab_with(rec.len() as u32, &rec);
        let mut r = SabRingReader::new(RING).unwrap();
        assert_eq!(r.read_record(&sab).unwrap(), Some(b"hello".to_vec()));
        // consumer advanced; next read sees nothing.
        assert_eq!(r.read_record(&sab).unwrap(), None);
    }

    #[test]
    fn reader_publishes_consumer_for_guest_backpressure() {
        // After draining, the reader publishes its consumer index into the SAB header so the guest producer can
        // compute free space. The published value must equal the advanced consumer (here 9 = 4-byte len + "hello").
        let rec = record(b"hello");
        let mut sab = sab_with(rec.len() as u32, &rec);
        let mut r = SabRingReader::new(RING).unwrap();
        assert_eq!(r.read_record(&sab).unwrap(), Some(b"hello".to_vec()));
        r.publish_consumer(&mut sab);
        let published = u32::from_le_bytes([
            sab[CONSUMER_INDEX_OFFSET],
            sab[CONSUMER_INDEX_OFFSET + 1],
            sab[CONSUMER_INDEX_OFFSET + 2],
            sab[CONSUMER_INDEX_OFFSET + 3],
        ]);
        assert_eq!(published, 9);
    }

    #[test]
    fn empty_ring_yields_none() {
        let sab = sab_with(0, &[]);
        let mut r = SabRingReader::new(RING).unwrap();
        assert_eq!(r.read_record(&sab).unwrap(), None);
    }

    #[test]
    fn hostile_huge_len_is_rejected_not_oob() {
        // len = u32::MAX, producer claims the whole ring. Must reject, never read out of bounds.
        let mut data = Vec::new();
        data.extend_from_slice(&u32::MAX.to_le_bytes());
        let sab = sab_with(RING - 1, &data);
        let mut r = SabRingReader::new(RING).unwrap();
        assert_eq!(r.read_record(&sab), Err(RingError::RecordTooLarge));
    }

    #[test]
    fn len_exceeding_produced_is_incomplete() {
        // len says 40 but the guest only produced 8 bytes of availability.
        let mut data = Vec::new();
        data.extend_from_slice(&40u32.to_le_bytes());
        let sab = sab_with(8, &data);
        let mut r = SabRingReader::new(RING).unwrap();
        assert_eq!(r.read_record(&sab), Err(RingError::IncompleteRecord));
    }

    #[test]
    fn hostile_producer_index_past_ring_does_not_oob() {
        // producer_index far past ring_size: must be reduced mod ring_size, no panic / OOB.
        let rec = record(b"hi");
        let sab = sab_with(u32::MAX, &rec);
        let mut r = SabRingReader::new(RING).unwrap();
        // available = (MAX % 64 - 0) mod 64 = 63; record (4+2=6) fits, so it reads the (attacker-chosen) bytes.
        let got = r.read_record(&sab).unwrap();
        assert_eq!(got, Some(b"hi".to_vec()));
    }

    #[test]
    fn wrapped_record_reads_correctly() {
        // A real circular buffer reuses space: the filler is consumed BEFORE the wrapping record is written.
        // Model that as two temporal snapshots (the data region is overwritten between them), not one buffer
        // holding both at once (which would exceed ring_size).
        let mut r = SabRingReader::new(RING).unwrap();
        // Snapshot 1: filler record [len=56][56x7] at offset 0 (60 bytes); producer=60. Read it -> consumer=60.
        let mut data = vec![0u8; RING as usize];
        data[0..4].copy_from_slice(&56u32.to_le_bytes());
        for b in &mut data[4..60] {
            *b = 7;
        }
        let sab1 = sab_with(60, &data);
        assert_eq!(r.read_record(&sab1).unwrap(), Some(vec![7u8; 56]));
        // Snapshot 2: the wrapping record reuses the freed space: [len=4] at bytes 60..64, payload "WXYZ" wraps to
        // 0..4. producer=4 => 8 bytes outstanding since consumer=60.
        let mut data2 = vec![0u8; RING as usize];
        data2[60..64].copy_from_slice(&4u32.to_le_bytes());
        data2[0..4].copy_from_slice(b"WXYZ");
        let sab2 = sab_with(4, &data2);
        assert_eq!(r.read_record(&sab2).unwrap(), Some(b"WXYZ".to_vec()));
    }

    #[test]
    fn zero_ring_size_rejected() {
        assert_eq!(SabRingReader::new(0).err(), Some(RingError::InvalidRingSize));
        assert_eq!(SabRingWriter::new(0).err(), Some(RingError::InvalidRingSize));
    }

    #[test]
    fn writer_roundtrips_through_reader() {
        // What the writer writes (publishing the producer index), a reader over the same layout reads back.
        let mut sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut w = SabRingWriter::new(RING).unwrap();
        assert_eq!(w.write_record(&mut sab, b"response-1").unwrap(), true);
        assert_eq!(w.write_record(&mut sab, b"r2").unwrap(), true);
        let mut r = SabRingReader::new(RING).unwrap();
        assert_eq!(r.read_record(&sab).unwrap(), Some(b"response-1".to_vec()));
        assert_eq!(r.read_record(&sab).unwrap(), Some(b"r2".to_vec()));
        assert_eq!(r.read_record(&sab).unwrap(), None);
    }

    #[test]
    fn writer_backpressures_when_full() {
        // Consumer never advances (guest does not drain); the writer must stop with Ok(false), not overflow.
        let mut sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut w = SabRingWriter::new(RING).unwrap();
        let mut written = 0;
        while w.write_record(&mut sab, b"12345678").unwrap() {
            written += 1;
            assert!(written < 100, "must backpressure long before this");
        }
        // ring 64, reserve 1 -> 63 usable; each record is 4+8=12 -> 5 fit.
        assert_eq!(written, 5);
    }

    #[test]
    fn writer_caps_oversized_payload() {
        let mut sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut w = SabRingWriter::new(RING).unwrap();
        let huge = vec![0u8; MAX_RECORD_BYTES + 1];
        assert_eq!(w.write_record(&mut sab, &huge), Err(RingError::RecordTooLarge));
    }

    #[test]
    fn writer_hostile_consumer_index_does_not_oob() {
        // A garbage guest-written consumer index must only mis-judge free space, never panic / read out of bounds.
        let mut sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        sab[CONSUMER_INDEX_OFFSET..CONSUMER_INDEX_OFFSET + 4].copy_from_slice(&u32::MAX.to_le_bytes());
        let mut w = SabRingWriter::new(RING).unwrap();
        let _ = w.write_record(&mut sab, b"x").unwrap(); // must not panic
    }

    #[test]
    fn endpoint_roundtrips_request_response() {
        // Guest writes a request into the G->K ring; the kernel endpoint reads it (publishing its consumer) and
        // writes a response into the K->G ring; the guest reads the response (publishing ITS consumer). In-crate
        // mirror of the JS t1-ring end-to-end test: 200 sequential round-trips, forcing wrap-around both ways.
        let mut req_sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut resp_sab = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut guest_writer = SabRingWriter::new(RING).unwrap(); // guest produces requests
        let mut guest_reader = SabRingReader::new(RING).unwrap(); // guest consumes responses
        let mut endpoint = SabRingEndpoint::new(RING).unwrap();
        for i in 0..200u32 {
            let req = format!("req-{i}");
            assert!(guest_writer.write_record(&mut req_sab, req.as_bytes()).unwrap(), "req {i} write");
            assert_eq!(endpoint.read_request(&mut req_sab).unwrap(), Some(req.into_bytes()));
            let resp = format!("resp-{i}");
            assert!(endpoint.write_response(&mut resp_sab, resp.as_bytes()).unwrap(), "resp {i} write");
            assert_eq!(guest_reader.read_record(&resp_sab).unwrap(), Some(resp.into_bytes()));
            // guest publishes its consumer so the kernel's response writer sees freed space (backpressure).
            guest_reader.publish_consumer(&mut resp_sab);
        }
    }

    #[test]
    fn service_from_raw_roundtrips_over_pointers() {
        // The host-side servicing entry over raw backing-store pointers (what V8 get_backing_store yields).
        let mut req = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut resp = vec![0u8; SAB_RING_HEADER_LEN + RING as usize];
        let mut guest_writer = SabRingWriter::new(RING).unwrap();
        let mut guest_reader = SabRingReader::new(RING).unwrap();
        let mut endpoint = SabRingEndpoint::new(RING).unwrap();
        assert!(guest_writer.write_record(&mut req, b"hi").unwrap());
        let (rp, rl) = (req.as_mut_ptr(), req.len());
        let (sp, sl) = (resp.as_mut_ptr(), resp.len());
        let serviced = unsafe {
            endpoint.service_from_raw(rp, rl, sp, sl, |r| {
                let mut v = b"got:".to_vec();
                v.extend_from_slice(r);
                v
            })
        }
        .unwrap();
        assert!(serviced);
        assert_eq!(guest_reader.read_record(&resp).unwrap(), Some(b"got:hi".to_vec()));
        // No request pending now -> Ok(false) (not an error, not a hang).
        let (rp, rl) = (req.as_mut_ptr(), req.len());
        let (sp, sl) = (resp.as_mut_ptr(), resp.len());
        let again = unsafe { endpoint.service_from_raw(rp, rl, sp, sl, |r| r.to_vec()) }.unwrap();
        assert!(!again);
    }

    // Deterministic LCG so the fuzz is reproducible without a rand dependency.
    fn lcg(state: &mut u64) -> u32 {
        *state = state
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        (*state >> 33) as u32
    }

    #[test]
    fn reader_never_oobs_on_hostile_fuzz() {
        // Fill the whole SAB (header + data) with random bytes => arbitrary hostile producer index + record
        // lengths + payloads. Every read_record must return Ok/Err and NEVER panic or read out of bounds (a panic
        // would fail the test). Any payload it does yield must be bounded by the ring.
        let mut st: u64 = 0x1234_5678_9abc_def0;
        for ring in [8u32, 16, 64, 257, 1024] {
            for _ in 0..2000 {
                let mut sab = vec![0u8; SAB_RING_HEADER_LEN + ring as usize];
                for b in sab.iter_mut() {
                    *b = lcg(&mut st) as u8;
                }
                let mut r = SabRingReader::new(ring).unwrap();
                for _ in 0..ring {
                    match r.read_record(&sab) {
                        Ok(Some(p)) => assert!(p.len() < ring as usize && p.len() <= MAX_RECORD_BYTES),
                        Ok(None) | Err(_) => break,
                    }
                }
            }
        }
    }

    #[test]
    fn writer_never_oobs_on_hostile_consumer_fuzz() {
        // Scribble a random (hostile) consumer index before each write, and vary payload size including > ring.
        // Every write_record must return Ok(_) or Err(RecordTooLarge) and NEVER panic / write out of bounds.
        let mut st: u64 = 0xdead_beef_cafe_babe;
        for ring in [8u32, 16, 64, 257, 1024] {
            for _ in 0..2000 {
                let mut sab = vec![0u8; SAB_RING_HEADER_LEN + ring as usize];
                let mut w = SabRingWriter::new(ring).unwrap();
                for _ in 0..ring {
                    let hostile = lcg(&mut st);
                    sab[CONSUMER_INDEX_OFFSET..CONSUMER_INDEX_OFFSET + 4]
                        .copy_from_slice(&hostile.to_le_bytes());
                    let plen = (lcg(&mut st) % (ring + 8)) as usize;
                    let payload = vec![0xABu8; plen];
                    match w.write_record(&mut sab, &payload) {
                        Ok(_) | Err(RingError::RecordTooLarge) => {}
                        Err(e) => panic!("unexpected writer error: {e:?}"),
                    }
                }
            }
        }
    }
}
