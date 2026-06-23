#!/usr/bin/env python3
"""Tool #2: X11 protocol decoder for the secure-exec xtrace tap.

Reads a host log containing `[xtrace] <dir> <path> len=<n> <hex>` lines (emitted by the sidecar's
unix-socket tap under SECURE_EXEC_XTRACE) and decodes the X11 wire stream per direction, so a stuck
client<->server exchange is visible: the last request the client sent, and whether the server replied.

C>S = client->server (requests, after the 12-byte setup). S>C = server->client (setup reply, then
replies/events/errors). We reassemble each direction's byte stream and walk it:
  request : opcode(1) data(1) length(2, in 4-byte units) ...     (sequence is implicit, ++ per request)
  reply   : 1 data(1) seq(2) length(4, extra 4-byte units) ...
  error   : 0 code(1) seq(2) ...                                 (32 bytes)
  event   : 2..34 ... seq(2) ...                                 (32 bytes)
Usage: xdecode.py <logfile> [path-substring]   (path-substring filters to one socket, e.g. X0)
"""
import sys, re

# A small slice of the core X11 request opcode table (enough to read a panel's traffic).
REQ = {
    1: "CreateWindow", 2: "ChangeWindowAttributes", 3: "GetWindowAttributes", 8: "MapWindow",
    10: "UnmapWindow", 12: "ConfigureWindow", 14: "GetGeometry", 15: "QueryTree",
    16: "InternAtom", 17: "GetAtomName", 18: "ChangeProperty", 19: "DeleteProperty",
    20: "GetProperty", 23: "GetSelectionOwner", 38: "QueryPointer", 40: "TranslateCoords",
    43: "GetInputFocus", 45: "OpenFont", 47: "QueryFont", 48: "QueryTextExtents",
    53: "CreatePixmap", 55: "CreateGC", 56: "ChangeGC", 60: "FreeGC", 62: "CopyArea",
    66: "PolySegment", 70: "PolyFillRectangle", 72: "PutImage", 73: "GetImage",
    91: "QueryColors", 94: "CreateGlyphCursor", 98: "QueryExtension", 119: "GetModifierMapping",
}
EVENTS = {2: "KeyPress", 3: "KeyRelease", 4: "ButtonPress", 6: "MotionNotify",
          12: "Expose", 18: "DestroyNotify", 19: "UnmapNotify", 22: "ConfigureNotify",
          21: "ReparentNotify", 28: "PropertyNotify", 34: "MappingNotify"}

def reassemble(path, log, want_dir, sub):
    data = bytearray()
    for line in log:
        m = re.search(r"\[xtrace\] (\S+) (\S+) len=(\d+) ([0-9a-f]*)", line)
        if not m: continue
        d, p, n, hx = m.group(1), m.group(2), int(m.group(3)), m.group(4)
        if d != want_dir: continue
        if sub and sub not in p: continue
        # only the leading (<=48) bytes are dumped per write; we can still see headers but not full bodies.
        data += bytes.fromhex(hx)
    return data

def main():
    if len(sys.argv) < 2:
        print(__doc__); return
    log = open(sys.argv[1], errors="replace").read().splitlines()
    sub = sys.argv[2] if len(sys.argv) > 2 else None
    cs = reassemble(None, log, "C>S", sub)
    sc = reassemble(None, log, "S>C", sub)
    print(f"# C>S {len(cs)} bytes, S>C {len(sc)} bytes (headers only; bodies truncated to 48B/write)")
    # The dump truncates each write to 48 bytes, so we can't reliably stride by length across writes.
    # Instead, decode each captured chunk's HEADER independently — enough to see request opcodes and
    # reply/event/error types + sequence numbers in send order.
    print("\n=== requests (client->server), in send order ===")
    seq = 0
    for line in log:
        m = re.search(r"\[xtrace\] C>S (\S+) len=(\d+) ([0-9a-f]+)", line)
        if not m or (sub and sub not in m.group(1)): continue
        b = bytes.fromhex(m.group(3))
        # a single write may batch several requests; walk by the 4*length stride within the captured head
        off = 0
        while off + 4 <= len(b):
            op = b[off]; ln = int.from_bytes(b[off+2:off+4], "little")
            seq += 1
            print(f"  req#{seq:<4} op={op:<3} {REQ.get(op,'?'):<22} len={ln*4}B")
            if ln == 0: break
            off += ln * 4
    print("\n=== server->client (replies/events/errors), in arrival order ===")
    for line in log:
        m = re.search(r"\[xtrace\] S>C (\S+) len=(\d+) ([0-9a-f]+)", line)
        if not m or (sub and sub not in m.group(1)): continue
        b = bytes.fromhex(m.group(3))
        if not b: continue
        t = b[0]
        s = int.from_bytes(b[2:4], "little") if len(b) >= 4 else -1
        if t == 1:
            ln = int.from_bytes(b[4:8], "little") if len(b) >= 8 else 0
            print(f"  REPLY   seq={s:<5} extra={ln*4}B")
        elif t == 0:
            code = b[1] if len(b) > 1 else -1
            print(f"  ERROR   seq={s:<5} code={code}")
        else:
            print(f"  EVENT   seq={s:<5} {EVENTS.get(t&0x7f, t&0x7f)}")

if __name__ == "__main__":
    main()
