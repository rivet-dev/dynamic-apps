#!/usr/bin/env python3
# Native baseline measurements for the wasm-gui desktop workload.
# MODE=desktop : full Xfce set (xfwm4, xfce4-panel, xfdesktop, thunar, mousepad) launched
#                concurrently after the WM; per-window appear times, coverage settle, per-proc
#                CPU/threads/RSS, keystroke ir under the full desktop, root screenshot.
# MODE=scale   : Xvfb + xfwm4 + N mousepads launched simultaneously (N env); time until all N
#                windows exist + coverage settles; Xvfb CPU.
# MODE=xcount  : same desktop set routed through x11trace; dumps the protocol log to /out for
#                request counting. APPSET=mousepad restricts to Xvfb+mousepad (single-app count).
import os, re, subprocess, time

W = int(os.environ.get("W", "1024")); H = int(os.environ.get("H", "768"))
MODE = os.environ.get("MODE", "desktop")
N = int(os.environ.get("N", "4"))
APPSET = os.environ.get("APPSET", "desktop")
FB = "/fb/Xvfb_screen0"
HDR = None
procs = {}

def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)

def launch(name, cmd, env_extra=None):
    e = dict(os.environ)
    if env_extra: e.update(env_extra)
    p = subprocess.Popen(cmd, stdout=open(f"/tmp/{name}.log", "w"),
                         stderr=subprocess.STDOUT, env=e)
    procs[name] = p
    return p

def fb_bytes():
    try:
        with open(FB, "rb") as f: return f.read()
    except Exception:
        return None

def coverage():
    global HDR
    b = fb_bytes()
    if not b or len(b) < W * H * 4: return None
    if HDR is None: HDR = len(b) - W * H * 4
    nb = tot = 0; mv = memoryview(b)
    for i in range(HDR, HDR + W * H * 4 - 3, 64 * 4):
        if mv[i] > 16 or mv[i + 1] > 16 or mv[i + 2] > 16: nb += 1
        tot += 1
    return 100.0 * nb / max(tot, 1)

def wins(cls):
    r = sh(["xdotool", "search", "--class", cls])
    return [w for w in r.stdout.split() if w]

def proc_cpu(pid):
    # returns (cpu_seconds, threads, rss_kb) or None
    try:
        with open(f"/proc/{pid}/stat") as f:
            parts = f.read().rsplit(") ", 1)[1].split()
        cpu = (int(parts[11]) + int(parts[12])) / 100.0  # utime+stime @100Hz
        thr = int(parts[17])
        rss = 0
        with open(f"/proc/{pid}/status") as f:
            m = re.search(r"VmRSS:\s+(\d+) kB", f.read())
            if m: rss = int(m.group(1))
        return cpu, thr, rss
    except Exception:
        return None

def find_pids(names):
    out = {}
    for pid in os.listdir("/proc"):
        if not pid.isdigit(): continue
        try:
            with open(f"/proc/{pid}/comm") as f: comm = f.read().strip()
        except Exception:
            continue
        if comm in names: out.setdefault(comm, []).append(int(pid))
    return out

def start_x(t0):
    launch("Xvfb", ["Xvfb", ":99", "-screen", "0", f"{W}x{H}x24", "-fbdir", "/fb", "-nolisten", "tcp"])
    while not os.path.exists(FB) and time.monotonic() - t0 < 15: time.sleep(0.01)
    os.environ["DISPLAY"] = ":99"
    while time.monotonic() - t0 < 15:
        if sh(["xdpyinfo"]).returncode == 0: break
        time.sleep(0.02)
    return time.monotonic() - t0

def start_dbus():
    out = sh(["dbus-launch"]).stdout
    for line in out.splitlines():
        k, _, v = line.partition("=")
        if k in ("DBUS_SESSION_BUS_ADDRESS", "DBUS_SESSION_BUS_PID"):
            os.environ[k] = v.rstrip(";")

def wait_wm(t0, timeout=15):
    while time.monotonic() - t0 < timeout:
        r = sh(["xprop", "-root", "_NET_SUPPORTING_WM_CHECK"])
        if "window id" in r.stdout: return time.monotonic() - t0
        time.sleep(0.02)
    return None

def save_ppm(path):
    b = fb_bytes()
    if not b or HDR is None: return
    px = b[HDR:HDR + W * H * 4]
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (W, H))
        row = bytearray(W * H * 3)
        for i in range(W * H):
            row[i*3] = px[i*4+2]; row[i*3+1] = px[i*4+1]; row[i*3+2] = px[i*4]
        f.write(row)

def report_procs(names):
    print(f"{'proc':<14}{'cpu_s':>8}{'thr':>5}{'rss_mb':>8}")
    for comm, pids in sorted(find_pids(names).items()):
        for pid in pids:
            st = proc_cpu(pid)
            if st: print(f"{comm:<14}{st[0]:>8.2f}{st[1]:>5}{st[2]/1024:>8.1f}")

DESKTOP_PROCS = {"Xvfb", "xfwm4", "xfce4-panel", "xfdesktop", "thunar", "Thunar",
                 "mousepad", "xfconfd", "dbus-daemon", "xfsettingsd"}

def best_win(cls):
    # largest visible window of the class (GTK creates phantom 10x10 utility windows)
    cands = sh(["xdotool", "search", "--onlyvisible", "--class", cls]).stdout.split() or wins(cls)
    best = None; barea = -1
    for w in cands:
        g = sh(["xdotool", "getwindowgeometry", "--shell", w]).stdout
        geo = dict(l.split("=") for l in g.split() if "=" in l)
        area = int(geo.get("WIDTH", 0)) * int(geo.get("HEIGHT", 0))
        if area > barea: barea, best = area, (w, geo)
    return best

def measure_ir(cls, runs=6):
    # keystroke input->response inside mousepad, damage-detect within the window's text rows
    mp_win, geo = best_win(cls)
    sh(["xdotool", "windowactivate", "--sync", mp_win]); time.sleep(0.3)
    X, Y = int(geo.get("X", 0)), int(geo.get("Y", 0))
    WIDTH, HEIGHT = int(geo.get("WIDTH", 400)), int(geo.get("HEIGHT", 300))
    print(f"[ir] mousepad geo x={X} y={Y} w={WIDTH} h={HEIGHT}")
    sh(["xdotool", "mousemove", str(X + WIDTH // 2), str(min(H - 10, Y + HEIGHT // 2)), "click", "1"])
    time.sleep(0.4)
    sh(["xdotool", "windowfocus", mp_win])
    # glyphs render near the top of the client area (empirically Y+23..Y+39); the frame
    # titlebar is above Y and the panel is rows 0..~26, so starting at Y+5 excludes both.
    r0 = max(0, Y + 5); r1 = min(H, Y + 280)
    a, b = HDR + r0 * W * 4, HDR + r1 * W * 4
    import zlib
    # diagnostic: whole-fb damage from one keystroke
    base = fb_bytes()
    sh(["xdotool", "type", "--delay", "40", "Zq"]); time.sleep(0.8)
    cur = fb_bytes()
    n = min(len(base), len(cur)); bpr = W * 4
    f = next((i for i in range(HDR, n) if base[i] != cur[i]), None)
    l = next((i for i in range(n - 1, HDR - 1, -1) if base[i] != cur[i]), None)
    if f is None:
        print("[ir] wholefb: NO DAMAGE from warmup keystrokes"); save_ppm("/out/ir-nodamage.ppm")
    else:
        print(f"[ir] wholefb damage rows {(f-HDR)//bpr}..{(l-HDR)//bpr} (region rows {r0}..{r1})")
    sh(["xdotool", "key", "ctrl+a"]); sh(["xdotool", "key", "Delete"]); time.sleep(0.3)
    res = []
    for i in range(runs):
        sh(["xdotool", "type", "--delay", "40", "HelloWorld"]); time.sleep(0.5)
        base = fb_bytes(); bh = zlib.crc32(base[a:b])
        ti = time.monotonic()
        sh(["xdotool", "type", "--delay", "0", "X"])
        ms = None
        while time.monotonic() - ti < 4:
            cur = fb_bytes()
            if cur and zlib.crc32(cur[a:b]) != bh:
                ms = (time.monotonic() - ti) * 1000.0; break
            time.sleep(0.0005)
        if ms is None:
            cur = fb_bytes(); n = min(len(base), len(cur)); bpr = W * 4
            f = next((j for j in range(HDR, n) if base[j] != cur[j]), None)
            l = next((j for j in range(n - 1, HDR - 1, -1) if base[j] != cur[j]), None)
            print(f"[ir] timeout diag: damage rows "
                  f"{'NONE' if f is None else f'{(f-HDR)//bpr}..{(l-HDR)//bpr}'} vs region {r0}..{r1}")
            save_ppm(f"/out/ir-timeout-run{i}.ppm")
        sh(["xdotool", "key", "ctrl+a"]); sh(["xdotool", "key", "Delete"]); time.sleep(0.3)
        print(f"[ir] run{i}: {ms:.1f}ms" if ms else f"[ir] run{i}: TIMEOUT")
        if ms: res.append(ms)
    if res:
        res.sort()
        print(f"[ir] median={res[len(res)//2]:.1f}ms min={res[0]:.1f}ms all={[round(x,1) for x in res]}")

def mode_desktop():
    t0 = time.monotonic()
    tx = start_x(t0)
    print(f"[t] X ready: {tx*1000:.0f}ms")
    start_dbus()
    launch("xfwm4", ["xfwm4"])
    twm = wait_wm(t0)
    print(f"[t] WM managing: {twm*1000:.0f}ms" if twm else "[t] WM TIMEOUT")
    apps = [("xfce4-panel", ["xfce4-panel"], "xfce4-panel"),
            ("xfdesktop", ["xfdesktop"], "xfdesktop"),
            ("thunar", ["thunar", "/root"], "thunar"),
            ("mousepad", ["mousepad"], "mousepad")]
    tl = time.monotonic() - t0
    for nm, cmd, _ in apps: launch(nm, cmd)
    print(f"[t] 4 apps launched concurrently at: {tl*1000:.0f}ms")
    seen = {}
    last_cov = None; stable_since = None; settle = None
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        for nm, _, cls in apps:
            if nm not in seen and wins(cls):
                seen[nm] = time.monotonic() - t0
                print(f"[t] window {nm}: {seen[nm]*1000:.0f}ms")
        c = coverage(); tnow = time.monotonic() - t0
        if c is not None:
            if last_cov is not None and abs(c - last_cov) < 0.25:
                if stable_since is None: stable_since = tnow
                elif tnow - stable_since >= 2.0 and len(seen) == 4:
                    settle = stable_since; break
            else:
                stable_since = None
            last_cov = c
        time.sleep(0.05)
    print(f"[t] coverage settled: {settle*1000:.0f}ms (coverage={last_cov:.1f}%)" if settle
          else f"[t] settle TIMEOUT (coverage={last_cov})")
    time.sleep(0.5)
    print("[procs] after full desktop startup:")
    report_procs(DESKTOP_PROCS)
    save_ppm("/out/desktop.ppm")
    if wins("mousepad"): measure_ir("mousepad")
    print("[procs] after ir runs:")
    report_procs(DESKTOP_PROCS)

def mode_scale():
    t0 = time.monotonic()
    tx = start_x(t0)
    launch("xfwm4", ["xfwm4"])
    twm = wait_wm(t0)
    print(f"[t] X ready {tx*1000:.0f}ms, WM {twm*1000:.0f}ms, N={N}")
    tl = time.monotonic()
    for i in range(N):
        # separate bogus bus address per instance so GApplication cannot unify instances
        launch(f"mousepad{i}", ["mousepad", "--disable-server"],
               {"DBUS_SESSION_BUS_ADDRESS": f"unix:path=/nonexistent-{i}"})
    nwin = 0; t_all = None
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        cur = len(wins("mousepad"))
        if cur != nwin:
            nwin = cur
            print(f"[t] {nwin}/{N} mousepad windows at +{(time.monotonic()-tl)*1000:.0f}ms")
        if nwin >= N:
            t_all = time.monotonic() - tl; break
        time.sleep(0.03)
    # settle
    last_cov = None; stable_since = None; settle = None
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        c = coverage(); tnow = time.monotonic() - tl
        if c is not None:
            if last_cov is not None and abs(c - last_cov) < 0.25:
                if stable_since is None: stable_since = tnow
                elif tnow - stable_since >= 2.0: settle = stable_since; break
            else:
                stable_since = None
            last_cov = c
        time.sleep(0.05)
    npids = len(find_pids({"mousepad"}).get("mousepad", []))
    print(f"[scale] N={N} all_windows={'%.0fms' % (t_all*1000) if t_all else 'TIMEOUT'} "
          f"settle={'%.0fms' % (settle*1000) if settle else 'TIMEOUT'} procs={npids} cov={last_cov:.1f}%")
    report_procs({"Xvfb", "xfwm4", "mousepad"})

def mode_xcount():
    t0 = time.monotonic()
    start_x(t0)
    launch("xtrace", ["xtrace", "-n", "-d", ":99", "-D", ":9", "-k", "-o", "/out/xtrace.log"])
    time.sleep(1.0)
    start_dbus()
    env9 = {"DISPLAY": ":9"}
    if APPSET == "mousepad":
        launch("mousepad", ["mousepad"], env9)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if wins("mousepad"): break
            time.sleep(0.05)
        time.sleep(3)
    else:
        launch("xfwm4", ["xfwm4"], env9)
        wait_wm(t0)
        apps = [("xfce4-panel", ["xfce4-panel"], "xfce4-panel"),
                ("xfdesktop", ["xfdesktop"], "xfdesktop"),
                ("thunar", ["thunar", "/root"], "thunar"),
                ("mousepad", ["mousepad"], "mousepad")]
        for nm, cmd, _ in apps: launch(nm, cmd, env9)
        seen = set()
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            for nm, _, cls in apps:
                if nm not in seen and wins(cls): seen.add(nm)
            if len(seen) == 4: break
            time.sleep(0.1)
        print(f"[xcount] windows seen: {sorted(seen)}")
        time.sleep(5)
    for nm, p in list(procs.items()):
        if nm not in ("Xvfb", "xtrace"): p.terminate()
    time.sleep(1.5)
    procs["xtrace"].terminate(); time.sleep(0.5)
    sz = os.path.getsize("/out/xtrace.log") if os.path.exists("/out/xtrace.log") else 0
    print(f"[xcount] xtrace log bytes={sz}")

if MODE == "desktop": mode_desktop()
elif MODE == "scale": mode_scale()
elif MODE == "xcount": mode_xcount()
for p in procs.values():
    try: p.terminate()
    except Exception: pass
