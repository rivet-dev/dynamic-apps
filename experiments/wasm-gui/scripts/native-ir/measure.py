import sys, time, subprocess, os, zlib
W, H = 800, 600
path = "/fb/Xvfb_screen0"
def read_all():
    try:
        with open(path, "rb") as f: return f.read()
    except Exception: return None
def coverage():
    b = read_all()
    if not b or len(b) < W*H*4: return None
    nb=tot=0; mv=memoryview(b)
    for i in range(0, W*H*4-3, 32*4):
        if mv[i]>16 or mv[i+1]>16 or mv[i+2]>16: nb+=1
        tot+=1
    return 100.0*nb/max(tot,1)

t0=time.time(); sc=False
while time.time()-t0<40:
    time.sleep(0.05); c=coverage()
    if c is None: continue
    if c<2.0: sc=True
    if sc and c>2.0:
        print(f"[firstpaint] {int((time.time()-t0)*1000)}ms"); break
bpp = os.path.getsize(path)//(W*H)

mp = subprocess.run(["xdotool","search","--name","Mousepad"],capture_output=True,text=True).stdout.split()
win = mp[0] if mp else None
print(f"[win] mousepad={win}")
# CLICK inside the text area to focus the widget (no WM => click-to-focus), then warm up.
# no-WM => PointerRoot focus: keep the pointer INSIDE the window (below the top text line, row 400),
# set explicit focus, and click to focus the text widget. Glyph renders at top (~row 62); the
# pointer/cursor at row 400 is excluded by the top-200-row detection region.
if win: subprocess.run(["xdotool","windowfocus",win])
subprocess.run(["xdotool","mousemove","300","400","click","1"]); time.sleep(0.3)
if win: subprocess.run(["xdotool","windowfocus",win])

def measure_once(region, warm="HelloWorld"):
    subprocess.run(["xdotool","type","--delay","40",warm]); time.sleep(0.5)
    base=read_all(); bh=zlib.crc32(base[:region]) if region else zlib.crc32(base)
    ti=time.time()
    subprocess.run(["xdotool","type","--delay","0","X"])
    while time.time()-ti<4:
        cur=read_all()
        if cur and (zlib.crc32(cur[:region]) if region else zlib.crc32(cur))!=bh:
            ms=(time.time()-ti)*1000.0
            n=min(len(base),len(cur))
            f=next((i for i in range(n) if base[i]!=cur[i]),None)
            l=next((i for i in range(n-1,-1,-1) if base[i]!=cur[i]),None)
            db=sum(1 for i in range(n) if base[i]!=cur[i])
            bpr=W*bpp
            subprocess.run(["xdotool","key","ctrl+a"]); subprocess.run(["xdotool","key","Delete"]); time.sleep(0.3)
            return ms, (f//bpr if f is not None else -1), (l//bpr if l is not None else -1), db
        time.sleep(0.0005)
    subprocess.run(["xdotool","key","ctrl+a"]); subprocess.run(["xdotool","key","Delete"]); time.sleep(0.3)
    return None,0,0,0

# first: whole-fb to confirm typing lands + see where text renders
ms,r0,r1,db = measure_once(0)
print(f"[wholefb] {ms}ms damage rows[{r0}..{r1}] diffbytes={db}" if ms else "[wholefb] TIMEOUT")

# then: glyph-region (top 200 rows) trustworthy runs
runs=[]
region=200*W*bpp
for i in range(6):
    ms,r0,r1,db = measure_once(region)
    print(f"[native-ir] run{i}: {ms:.1f}ms rows[{r0}..{r1}] db={db}" if ms else f"[native-ir] run{i}: TIMEOUT")
    if ms: runs.append(ms)
if runs:
    runs.sort(); print(f"[native-ir] median={runs[len(runs)//2]:.1f}ms min={runs[0]:.1f}ms all={[round(x,1) for x in runs]}")
