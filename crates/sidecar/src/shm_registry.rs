//! Brokered shared-memory segment registry (lever 3 / XU MIT-SHM X11 pixmaps).
//!
//! The X server and X clients are mutually-untrusted guests. Instead of copying pixmap bytes through per-pixel
//! sync-RPCs (slow -- the framebuffer/X-socket Root-3 throughput cost), a client allocates a brokered shared
//! segment (like SysV SHM / X's MIT-SHM): the kernel owns the registry mapping a segment id to its
//! size/owner/attachers, and the v8-runtime hands each attached guest a view onto the shared backing. This module
//! is the kernel-side registry + bounds POLICY only (backing SAB allocation + the per-guest view live in the
//! v8-runtime, which owns guest memory; this crate is `#![forbid(unsafe_code)]`, so it holds no raw pointers).
//!
//! Trust model: like the T1 ring, every guest-supplied field (segment id, offset, length) is HOSTILE and validated
//! here before any byte transfer is permitted -- `validate_access` is the gate. Bounds (limits invariant): all of
//! MAX_SEGMENTS / MAX_SEGMENT_BYTES / MAX_TOTAL_BYTES are bounded with typed errors naming the cap (operator-tunable
//! VmLimits wiring is a follow-up; these are the invariant ceilings).

use std::collections::{HashMap, HashSet};

/// Max live segments across all guests in one VM.
pub(crate) const MAX_SEGMENTS: usize = 1024;
/// Max bytes for a single segment (64 MiB -- a 4K RGBA pixmap is ~33 MiB, so this fits one comfortably).
pub(crate) const MAX_SEGMENT_BYTES: usize = 64 << 20;
/// Max total shared-segment bytes across the VM (512 MiB).
pub(crate) const MAX_TOTAL_BYTES: usize = 512 << 20;

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum ShmError {
    TooManySegments { cap: usize },
    SegmentTooLarge { len: usize, cap: usize },
    TotalExceeded { would_be: usize, cap: usize },
    NoSuchSegment { id: u32 },
    NotAttached { vm: u32, id: u32 },
    OutOfBounds { id: u32, offset: usize, len: usize, seg_len: usize },
    NotOwner { vm: u32, id: u32 },
}

#[derive(Debug)]
struct Segment {
    len: usize,
    owner: u32,
    attached: HashSet<u32>,
}

/// Per-VM registry of brokered shared segments. Pure-safe (no raw memory); the backing lives in the v8-runtime.
#[derive(Debug, Default)]
pub(crate) struct ShmRegistry {
    segments: HashMap<u32, Segment>,
    next_id: u32,
    total_bytes: usize,
}

impl ShmRegistry {
    pub(crate) fn new() -> Self {
        Self { segments: HashMap::new(), next_id: 1, total_bytes: 0 }
    }

    /// Create a segment owned by `owner` (auto-attached). Bounds-checked against all three caps. Returns the new id.
    pub(crate) fn create(&mut self, owner: u32, len: usize) -> Result<u32, ShmError> {
        if len > MAX_SEGMENT_BYTES {
            return Err(ShmError::SegmentTooLarge { len, cap: MAX_SEGMENT_BYTES });
        }
        if self.segments.len() >= MAX_SEGMENTS {
            return Err(ShmError::TooManySegments { cap: MAX_SEGMENTS });
        }
        let would_be = self.total_bytes + len;
        if would_be > MAX_TOTAL_BYTES {
            return Err(ShmError::TotalExceeded { would_be, cap: MAX_TOTAL_BYTES });
        }
        // Allocate a non-zero id that is not already live (robust across a u32 wrap; MAX_SEGMENTS << u32::MAX so
        // this loop is effectively free).
        let mut id = self.next_id;
        self.next_id = self.next_id.wrapping_add(1);
        while id == 0 || self.segments.contains_key(&id) {
            id = self.next_id;
            self.next_id = self.next_id.wrapping_add(1);
        }
        let mut attached = HashSet::new();
        attached.insert(owner);
        self.segments.insert(id, Segment { len, owner, attached });
        self.total_bytes = would_be;
        Ok(id)
    }

    /// Attach `vm` to an existing segment (hostile id validated). Returns the segment length so the runtime can
    /// size the guest's view.
    pub(crate) fn attach(&mut self, vm: u32, id: u32) -> Result<usize, ShmError> {
        let seg = self.segments.get_mut(&id).ok_or(ShmError::NoSuchSegment { id })?;
        seg.attached.insert(vm);
        Ok(seg.len)
    }

    /// Detach `vm`. When the last attacher detaches, the segment is destroyed and its bytes are freed.
    pub(crate) fn detach(&mut self, vm: u32, id: u32) -> Result<(), ShmError> {
        let seg = self.segments.get_mut(&id).ok_or(ShmError::NoSuchSegment { id })?;
        seg.attached.remove(&vm);
        if seg.attached.is_empty() {
            let len = seg.len;
            self.segments.remove(&id);
            self.total_bytes -= len;
        }
        Ok(())
    }

    /// Explicit destroy by the owner (frees regardless of remaining attachers -- matches MIT-SHM ShmDetach by the
    /// allocating client). Non-owners cannot destroy another guest's segment.
    pub(crate) fn destroy(&mut self, vm: u32, id: u32) -> Result<(), ShmError> {
        let seg = self.segments.get(&id).ok_or(ShmError::NoSuchSegment { id })?;
        if seg.owner != vm {
            return Err(ShmError::NotOwner { vm, id });
        }
        let len = seg.len;
        self.segments.remove(&id);
        self.total_bytes -= len;
        Ok(())
    }

    /// THE GATE: validate a guest-requested `[offset, offset+len)` access to a segment `vm` is attached to. Every
    /// argument is HOSTILE (id/offset/len from the guest). Overflow-safe (checked add). Call this before any byte
    /// transfer into/out of the shared backing.
    pub(crate) fn validate_access(
        &self,
        vm: u32,
        id: u32,
        offset: usize,
        len: usize,
    ) -> Result<(), ShmError> {
        let seg = self.segments.get(&id).ok_or(ShmError::NoSuchSegment { id })?;
        if !seg.attached.contains(&vm) {
            return Err(ShmError::NotAttached { vm, id });
        }
        let end = offset
            .checked_add(len)
            .ok_or(ShmError::OutOfBounds { id, offset, len, seg_len: seg.len })?;
        if end > seg.len {
            return Err(ShmError::OutOfBounds { id, offset, len, seg_len: seg.len });
        }
        Ok(())
    }

    pub(crate) fn total_bytes(&self) -> usize {
        self.total_bytes
    }
    pub(crate) fn segment_count(&self) -> usize {
        self.segments.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn create_attach_validate_roundtrip() {
        let mut r = ShmRegistry::new();
        let id = r.create(1, 4096).unwrap();
        assert_eq!(r.segment_count(), 1);
        assert_eq!(r.total_bytes(), 4096);
        // owner is auto-attached + can access in-bounds
        assert_eq!(r.validate_access(1, id, 0, 4096), Ok(()));
        assert_eq!(r.validate_access(1, id, 4095, 1), Ok(()));
        // a second guest must attach first
        assert_eq!(r.validate_access(2, id, 0, 1), Err(ShmError::NotAttached { vm: 2, id }));
        assert_eq!(r.attach(2, id).unwrap(), 4096);
        assert_eq!(r.validate_access(2, id, 100, 200), Ok(()));
    }

    #[test]
    fn create_enforces_all_three_caps() {
        let mut r = ShmRegistry::new();
        assert_eq!(
            r.create(1, MAX_SEGMENT_BYTES + 1),
            Err(ShmError::SegmentTooLarge { len: MAX_SEGMENT_BYTES + 1, cap: MAX_SEGMENT_BYTES })
        );
        // total cap: fill to near MAX_TOTAL_BYTES with max-size segments, then the next over-fills
        let mut r2 = ShmRegistry::new();
        let per = MAX_SEGMENT_BYTES;
        let n = MAX_TOTAL_BYTES / per;
        for _ in 0..n {
            r2.create(1, per).unwrap();
        }
        // one more max-size segment would exceed the total
        match r2.create(1, per) {
            Err(ShmError::TotalExceeded { cap, .. }) => assert_eq!(cap, MAX_TOTAL_BYTES),
            other => panic!("expected TotalExceeded, got {other:?}"),
        }
    }

    #[test]
    fn too_many_segments_is_capped() {
        let mut r = ShmRegistry::new();
        for _ in 0..MAX_SEGMENTS {
            r.create(1, 8).unwrap();
        }
        assert_eq!(r.segment_count(), MAX_SEGMENTS);
        assert_eq!(r.create(1, 8), Err(ShmError::TooManySegments { cap: MAX_SEGMENTS }));
    }

    #[test]
    fn validate_access_rejects_hostile_inputs() {
        let mut r = ShmRegistry::new();
        let id = r.create(7, 1000).unwrap();
        // unknown id
        assert_eq!(r.validate_access(7, 999_999, 0, 1), Err(ShmError::NoSuchSegment { id: 999_999 }));
        // out of bounds: offset+len > len
        assert_eq!(
            r.validate_access(7, id, 900, 200),
            Err(ShmError::OutOfBounds { id, offset: 900, len: 200, seg_len: 1000 })
        );
        // exact end is allowed; one past is not
        assert_eq!(r.validate_access(7, id, 0, 1000), Ok(()));
        assert!(r.validate_access(7, id, 0, 1001).is_err());
        // offset+len overflow (usize wrap) must be rejected, not wrap to a small in-bounds end
        assert_eq!(
            r.validate_access(7, id, usize::MAX, 10),
            Err(ShmError::OutOfBounds { id, offset: usize::MAX, len: 10, seg_len: 1000 })
        );
    }

    #[test]
    fn detach_refcounts_and_frees_on_last() {
        let mut r = ShmRegistry::new();
        let id = r.create(1, 2048).unwrap(); // owner 1 attached
        r.attach(2, id).unwrap();
        assert_eq!(r.total_bytes(), 2048);
        // detach one -> still alive
        r.detach(1, id).unwrap();
        assert_eq!(r.segment_count(), 1);
        assert_eq!(r.total_bytes(), 2048);
        // detach the last -> destroyed + freed
        r.detach(2, id).unwrap();
        assert_eq!(r.segment_count(), 0);
        assert_eq!(r.total_bytes(), 0);
        // detaching a gone segment is a typed error, not a panic
        assert_eq!(r.detach(2, id), Err(ShmError::NoSuchSegment { id }));
    }

    #[test]
    fn destroy_is_owner_only_and_frees() {
        let mut r = ShmRegistry::new();
        let id = r.create(5, 4096).unwrap();
        r.attach(6, id).unwrap();
        assert_eq!(r.destroy(6, id), Err(ShmError::NotOwner { vm: 6, id }));
        assert_eq!(r.destroy(5, id), Ok(()));
        assert_eq!(r.total_bytes(), 0);
        assert_eq!(r.segment_count(), 0);
    }

    #[test]
    fn ids_are_nonzero_and_unique_across_churn() {
        let mut r = ShmRegistry::new();
        let mut seen = std::collections::HashSet::new();
        for _ in 0..5000 {
            let id = r.create(1, 8).unwrap();
            assert_ne!(id, 0, "id must never be 0");
            assert!(seen.insert(id), "id {id} reused while live");
            r.destroy(1, id).unwrap(); // free so we don't hit MAX_SEGMENTS
        }
    }
}
