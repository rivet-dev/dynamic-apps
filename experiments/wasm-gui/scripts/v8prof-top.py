#!/usr/bin/env python3
# Symbolize a V8 --prof tick log (SECURE_EXEC_V8PROF=1 writes /tmp/secure-exec-v8.log). Maps each
# tick's PC + stack to the code object containing it (from code-creation records) and reports the
# hottest functions -> names the wasm function a busy-spin is stuck in (needs SECURE_EXEC_KEEP_NAMES=1
# so the guest .wasm keeps its name section). ~ perf report. See INTERNAL-TOOLING.md.
import sys, csv, bisect
csv.field_size_limit(1 << 30)
from collections import Counter

path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/secure-exec-v8.log"
starts, ends, names = [], [], []   # sorted-by-start code intervals
recs = []

def to_int(x):
    try: return int(x, 16) if x.startswith("0x") else int(x)
    except Exception: return None

with open(path, errors="replace") as f:
    for row in csv.reader(f):
        if not row: continue
        tag = row[0]
        if tag == "code-creation" and len(row) >= 7:
            # code-creation,<type>,<kind>,<ts>,<addr>,<size>,"<name>",...
            addr, size = to_int(row[4]), to_int(row[5])
            name = row[6]
            ctype = row[1]
            if addr is None or size is None: continue
            recs.append((addr, addr + size, f"[{ctype}] {name}"))
        elif tag == "tick" and len(row) >= 2:
            pass  # handled in second pass

recs.sort()
starts = [r[0] for r in recs]

def lookup(pc):
    if pc is None: return None
    i = bisect.bisect_right(starts, pc) - 1
    if 0 <= i < len(recs) and recs[i][0] <= pc < recs[i][1]:
        return recs[i][2]
    return None

top = Counter()       # by top-of-stack PC (where the CPU actually is)
incl = Counter()      # inclusive: any frame on the stack
ticks = 0
with open(path, errors="replace") as f:
    for row in csv.reader(f):
        if not row or row[0] != "tick" or len(row) < 2: continue
        ticks += 1
        pc = to_int(row[1])
        sym = lookup(pc)
        if sym: top[sym] += 1
        seen = set()
        for cell in row[2:]:
            a = to_int(cell)
            s = lookup(a)
            if s and s not in seen:
                incl[s] += 1; seen.add(s)

print(f"== {ticks} ticks, {len(recs)} code objects ==")
print("\n-- TOP-OF-STACK (where the CPU is spinning) --")
for sym, n in top.most_common(20):
    print(f"  {n:6}  {sym}")
print("\n-- INCLUSIVE (function present anywhere on the stack) --")
for sym, n in incl.most_common(20):
    print(f"  {n:6}  {sym}")
