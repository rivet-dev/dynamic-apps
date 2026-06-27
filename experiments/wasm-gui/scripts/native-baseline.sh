#!/bin/bash
# Native first-paint + input->response latency baseline. Xvfb -fbdir (raw fb, same as the wasm setup).
set -u
APP="${1:-mousepad}"; MODE="${2:-paint}"
W=800; H=600
mkdir -p /fb
Xvfb :99 -screen 0 ${W}x${H}x24 -fbdir /fb -nolisten tcp >/tmp/xvfb.log 2>&1 &
XPID=$!
export DISPLAY=:99
for i in $(seq 1 100); do [ -e /fb/Xvfb_screen0 ] && break; sleep 0.05; done
T0=$(date +%s.%N)
$APP >/tmp/app.log 2>&1 &
APID=$!
python3 - "$T0" "$MODE" "$W" "$H" <<'PY'
import sys,time
t0=float(sys.argv[1]); mode=sys.argv[2]; W=int(sys.argv[3]); H=int(sys.argv[4])
path="/fb/Xvfb_screen0"
def coverage():
    try: b=open(path,'rb').read()
    except: return None
    if len(b)<W*H*4: return None
    # stride-sample every 32nd pixel for speed
    nb=tot=0
    step=32*4
    mv=memoryview(b)
    for i in range(0,W*H*4-3,step):
        if mv[i]>16 or mv[i+1]>16 or mv[i+2]>16: nb+=1
        tot+=1
    return 100.0*nb/max(tot,1)
def fbhash():
    try: b=open(path,'rb').read()
    except: return 0
    return hash(b[::997])  # cheap content fingerprint

# Phase 1: first paint
seen_clear=False; paint_ms=None
while time.time()-t0 < 40:
    time.sleep(0.05)
    c=coverage()
    if c is None: continue
    if c<2.0: seen_clear=True
    if seen_clear and c>2.0:
        paint_ms=int((time.time()-t0)*1000)
        print(f"[firstpaint] {paint_ms}ms ({c:.1f}% non-black)")
        break
if paint_ms is None:
    print("[firstpaint] TIMEOUT"); sys.exit(0)
if mode!="input": sys.exit(0)

# Phase 2: input->response latency. Let it settle, snapshot fb, send a key, time until fb changes.
import subprocess
time.sleep(2.0)
base=fbhash()
# focus + type a character via xdotool
subprocess.run(["xdotool","search","--name",".","windowactivate","--sync"],timeout=5,
               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
ti=time.time()
subprocess.run(["xdotool","type","--delay","0","HELLO"],timeout=5,
               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
resp=None
while time.time()-ti < 5:
    if fbhash()!=base:
        resp=int((time.time()-ti)*1000); break
    time.sleep(0.005)
print(f"[input-response] {resp if resp is not None else 'TIMEOUT'}ms")
PY
kill $APID $XPID 2>/dev/null
