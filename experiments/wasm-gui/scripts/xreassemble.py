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

def cat(log, direction, sub):
    b = bytearray()
    for line in log:
        m = re.search(r"\[xtrace\] (\S+) (\S+) len=(\d+) ([0-9a-f]*)", line)
        if not m or m.group(1) != direction: continue
        if sub and sub not in m.group(2): continue
        b += bytes.fromhex(m.group(4))
    return bytes(b)

def main():
    log = open(sys.argv[1], errors="replace").read().splitlines()
    sub = sys.argv[2] if len(sys.argv) > 2 else "X0"
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
