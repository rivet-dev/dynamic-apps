#!/usr/bin/env python3
"""Reassemble the FULL X11 client<->server byte streams from an xtrace log and parse them
request-by-request with correct sequence numbers, to find the exact request the server stops
answering. Needs the sidecar xtrace cap large enough that no write is truncated.

Usage: xreassemble.py <logfile> [path-substring]
"""
import sys, re

REQ = {1:"CreateWindow",2:"ChangeWindowAttributes",3:"GetWindowAttributes",8:"MapWindow",
 10:"UnmapWindow",12:"ConfigureWindow",14:"GetGeometry",15:"QueryTree",16:"InternAtom",
 17:"GetAtomName",18:"ChangeProperty",19:"DeleteProperty",20:"GetProperty",23:"GetSelectionOwner",
 38:"QueryPointer",40:"TranslateCoords",43:"GetInputFocus",45:"OpenFont",47:"QueryFont",
 48:"QueryTextExtents",53:"CreatePixmap",54:"FreePixmap",55:"CreateGC",56:"ChangeGC",60:"FreeGC",
 62:"CopyArea",66:"PolySegment",70:"PolyFillRectangle",72:"PutImage",73:"GetImage",91:"QueryColors",
 94:"CreateGlyphCursor",98:"QueryExtension",119:"GetModifierMapping"}
# requests that produce a reply (client will block in xcb_wait_for_reply on these)
HAS_REPLY = {3,14,15,16,17,20,23,38,40,43,47,48,73,91,98,119,55}

# xtrace lines are either the old `[xtrace] <dir> <path> len=<n> <hex>` or the timestamped
# `[xtrace] ts=<epoch_us> <dir> <path> len=<n> <hex>` (SECURE_EXEC_XTRACE with the D1 wall-clock).
XLINE = re.compile(r"\[xtrace\] (?:ts=(\d+) )?(\S+) (\S+) len=(\d+) ([0-9a-f]*)")

def cat(log, direction, sub):
    b = bytearray()
    for line in log:
        m = XLINE.search(line)
        if not m or m.group(2) != direction: continue
        if sub and sub not in m.group(3): continue
        b += bytes.fromhex(m.group(5))
    return bytes(b)

def cat_ts(log, direction, sub):
    """Like cat(), but also return offset->timestamp boundaries: list of (start_offset, ts_us)."""
    b = bytearray(); bounds = []
    for line in log:
        m = XLINE.search(line)
        if not m or m.group(2) != direction: continue
        if sub and sub not in m.group(3): continue
        ts = int(m.group(1)) if m.group(1) else None
        bounds.append((len(b), ts))
        b += bytes.fromhex(m.group(5))
    return bytes(b), bounds

def ts_at(bounds, off):
    """Timestamp of the xtrace chunk that contains byte offset `off`."""
    ts = None
    for (start, t) in bounds:
        if start <= off: ts = t
        else: break
    return ts

def roundtrips(log, sub):
    """D1: count request->reply round-trips that fall in the host's [ir-mark] inject..detect window."""
    inj = det = None
    for line in log:
        m = re.search(r"\[ir-mark\] inject .*wall=(\d+)", line)
        if m: inj = int(m.group(1))
        m = re.search(r"\[ir-mark\] detect .*wall=(\d+)", line)
        if m: det = int(m.group(1))
    cs, csb = cat_ts(log, "C>S", sub)
    sc, scb = cat_ts(log, "S>C", sub)
    o = 0
    if cs[:1] in (b'l', b'B'):
        nlen = int.from_bytes(cs[6:8], 'little'); dlen = int.from_bytes(cs[8:10], 'little')
        o = 12 + ((nlen+3)//4)*4 + ((dlen+3)//4)*4
    seq = 0; reqs = []  # (seq, op, name, has_reply, ts)
    while o + 4 <= len(cs):
        op = cs[o]; ln = int.from_bytes(cs[o+2:o+4], 'little')
        if ln == 0:
            if o + 8 > len(cs): break
            ln = int.from_bytes(cs[o+4:o+8], 'little')
        nbytes = ln*4
        if nbytes < 4: break
        seq = (seq+1) & 0xffffffff
        reqs.append((seq, op, REQ.get(op, f"op{op}"), op in HAS_REPLY, ts_at(csb, o)))
        o += nbytes
    # replies/events in window (server side)
    p = 0
    if sc[:1] in (b'\x00', b'\x01', b'\x02'):
        rlen = int.from_bytes(sc[6:8], 'little'); p = 8 + rlen*4
    replies_in = events_in = 0
    while p + 32 <= len(sc):
        t = sc[p]; tsp = ts_at(scb, p)
        if t == 1:
            extra = int.from_bytes(sc[p+4:p+8], 'little')
            if inj is not None and tsp is not None and inj <= tsp <= det: replies_in += 1
            p += 32 + extra*4
        else:
            if t >= 2 and inj is not None and tsp is not None and inj <= tsp <= det: events_in += 1
            p += 32
    print(f"# D1 round-trip window: inject={inj} detect={det} (epoch us), span={ (det-inj)/1000 if inj and det else '?'}ms")
    win = [r for r in reqs if r[4] is not None and inj is not None and inj <= r[4] <= det]
    win_reply = [r for r in win if r[3]]
    print(f"# requests in window: {len(win)} total, {len(win_reply)} reply-bearing (blocking round-trips)")
    print(f"# server->client in window: {replies_in} replies, {events_in} events")
    from collections import Counter
    c = Counter(r[2] for r in win)
    print(f"# request mix in window: {dict(c.most_common())}")
    cr = Counter(r[2] for r in win_reply)
    print(f"# reply-bearing (round-trip) mix in window: {dict(cr.most_common())}")
    print(f"# total over whole run: {len(reqs)} requests, {sum(1 for r in reqs if r[3])} reply-bearing")
    print(f"# ordered round-trips in window:")
    for r in win_reply:
        print(f"    seq={r[0]:<5} {r[2]:<18} ts={r[4]} (+{(r[4]-inj)/1000:.1f}ms)")

def main():
    log = open(sys.argv[1], errors="replace").read().splitlines()
    sub = sys.argv[2] if len(sys.argv) > 2 else "X0"
    if "--rt" in sys.argv:
        roundtrips(log, sub); return
    cs = cat(log, "C>S", sub); sc = cat(log, "S>C", sub)
    # --- skip the client setup request (byte order 'l'/'B') ---
    o = 0
    if cs[:1] in (b'l', b'B'):
        nlen = int.from_bytes(cs[6:8], 'little'); dlen = int.from_bytes(cs[8:10], 'little')
        o = 12 + ((nlen+3)//4)*4 + ((dlen+3)//4)*4
    # --- parse requests, assigning sequence numbers (server starts replies at seq 1) ---
    reqs = []   # (seq, opcode, name, has_reply)
    seq = 0
    while o + 4 <= len(cs):
        op = cs[o]; ln = int.from_bytes(cs[o+2:o+4], 'little')
        if ln == 0:  # BigRequests: real length in the next u32
            if o + 8 > len(cs): break
            ln = int.from_bytes(cs[o+4:o+8], 'little')
        nbytes = ln * 4
        if nbytes < 4: break  # desync guard
        seq = (seq + 1) & 0xffffffff
        reqs.append((seq, op, REQ.get(op, f"op{op}"), op in HAS_REPLY))
        o += nbytes
    # --- skip the server setup reply, then parse replies/events/errors ---
    p = 0
    if sc[:1] in (b'\x00', b'\x01', b'\x02'):
        rlen = int.from_bytes(sc[6:8], 'little'); p = 8 + rlen*4
    got_reply = set(); errors = []
    while p + 32 <= len(sc):
        t = sc[p]
        s = int.from_bytes(sc[p+2:p+4], 'little')
        if t == 1:
            extra = int.from_bytes(sc[p+4:p+8], 'little'); got_reply.add(s); p += 32 + extra*4
        elif t == 0:
            errors.append((s, sc[p+1])); p += 32
        else:
            p += 32
    # full request listing with the first resource id (window/drawable/etc.)
    if "--list" in sys.argv:
        o2 = o0 = 0
        # re-walk to grab the resource id (bytes 4-8) of each request
        oo = 12
        if cs[:1] in (b'l', b'B'):
            nlen = int.from_bytes(cs[6:8],'little'); dlen=int.from_bytes(cs[8:10],'little')
            oo = 12 + ((nlen+3)//4)*4 + ((dlen+3)//4)*4
        sq = 0
        print("# full request list (seq op name resource):")
        while oo + 4 <= len(cs):
            op = cs[oo]; ln = int.from_bytes(cs[oo+2:oo+4],'little')
            hdr = oo
            if ln == 0:
                ln = int.from_bytes(cs[oo+4:oo+8],'little')
            nb = ln*4
            if nb < 4:
                print(f"  !! DESYNC at byte {oo}: op={op} ln={ln}"); break
            sq += 1
            res = int.from_bytes(cs[oo+4:oo+8],'little') if oo+8<=len(cs) else 0
            nm = REQ.get(op, f"op{op}")
            mark = " <<ERR" if sq in (71,72) else ""
            if op in (1,8,53,55,62,70,72,2,18,12) or sq>=120 or mark:
                print(f"  seq={sq:<4} op={op:<3} {nm:<20} res={res:#010x}{mark}")
            oo += nb
        return
    # --- report: the first reply-expecting request with no reply = the stuck point ---
    last_reply = max(got_reply) if got_reply else 0
    print(f"# parsed {len(reqs)} requests, {len(got_reply)} replies (last reply seq={last_reply}), {len(errors)} errors")
    if errors: print("# ERRORS:", errors[:10])
    print("# reply-expecting requests and whether a reply arrived:")
    stuck = None
    for (sq, op, name, hr) in reqs:
        if not hr: continue
        ok = sq in got_reply
        flag = "" if ok else "   <-- NO REPLY"
        if not ok and stuck is None and sq > last_reply: stuck = (sq, name)
        if sq >= last_reply - 6:
            print(f"  seq={sq:<5} {name:<18} reply={'yes' if ok else 'NO'}{flag}")
    print(f"\n# => first reply-expecting request past the last reply with NO reply: {stuck}")
    # show the requests immediately around the stuck point (all, incl. no-reply ones)
    if stuck:
        print(f"# requests around seq {stuck[0]}:")
        for (sq, op, name, hr) in reqs:
            if abs(sq - stuck[0]) <= 4:
                print(f"    seq={sq:<5} op={op:<3} {name}{' (expects reply)' if hr else ''}")

if __name__ == "__main__":
    main()
