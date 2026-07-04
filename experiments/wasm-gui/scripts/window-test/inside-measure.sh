#!/bin/bash
# Runs INSIDE the sx-wintest container. Measures desktop-boot-time = wall from host launch until the
# GUEST framebuffer coverage settles (all launched apps painted), mirroring the native mode_desktop
# coverage-settle definition in scripts/native-ir/desktop-measure.py. Emits a single machine-readable
# line `BOOT_MS=<n>` (or `BOOT_TIMEOUT`) plus per-milestone timing, and saves the settle framebuffer.
#
# This is the honest "ours" numerator for the DESKTOP-BOOT-PERF loop. It does NOT do the xdotool input
# round-trip (that is inside-xfce.sh's interactivity proof); boot-time only.
#
# Env: W,H (guest screen), XFCE_CLIENTS, BINDIR (target/release|target/debug), SX_BOOT_TIMEOUT,
#      SECURE_EXEC_PERFCLOCK (pass-through), COV_SETTLE_S, COV_MIN.
set -u
: "${W:=800}"; : "${H:=600}"
: "${BINDIR:=target/release}"
: "${SX_BOOT_TIMEOUT:=180}"
: "${COV_SETTLE_S:=2.0}"
# COV_MIN: first-paint threshold (any real content). FULL_MIN: settle requires this much coverage so a
# panel-only / degraded render (~5%) is NOT counted as a full desktop — a full Xfce desktop (xfdesktop
# background + panel + app windows) settles ~95%; panel-only ~4.8%. 40% cleanly separates them.
: "${COV_MIN:=3.0}"
: "${FULL_MIN:=40.0}"
EXP=/repo/experiments/wasm-gui
HOST_BIN="/repo/${BINDIR}/wasm-gui-host"
SIDECAR_BIN="/repo/${BINDIR}/secure-exec-sidecar"

# Host winit display (the winit window that mirrors the guest fb). The boot measurement reads the GUEST
# fb directly, but the host binary (built --features window) still creates a winit window that needs X.
Xvfb :99 -screen 0 1280x1024x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
export DISPLAY=:99
for i in $(seq 1 50); do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 0.2; done

cd "$EXP"
CLIENTS="${XFCE_CLIENTS:-xfwm4 xfce4-panel xfdesktop thunar mousepad}"
CLIENT_ARGS=(); for c in $CLIENTS; do CLIENT_ARGS+=(--client "$EXP/${c}.wasm"); done
NCLIENTS=$(echo $CLIENTS | wc -w)
LAST_IDX=$(( NCLIENTS - 1 ))

# Record host launch time (ns) so the python monitor can compute wall-from-launch.
date +%s.%N > /tmp/host_t0
APP_SETTLE_MS="${APP_SETTLE_MS:-6000}" SX_SERIAL_LAUNCH="${SX_SERIAL_LAUNCH:-0}" \
  SX_SERIAL_SETTLE_MS="${SX_SERIAL_SETTLE_MS:-12000}" NO_AT_BRIDGE=1 \
  SX_PARALLEL_VMS="${SX_PARALLEL_VMS:-0}" \
  SX_INFRA_DBUS_MS="${SX_INFRA_DBUS_MS:-2000}" \
  SX_INFRA_SVC_MS="${SX_INFRA_SVC_MS:-4000}" \
  SX_WM_FALLBACK_MS="${SX_WM_FALLBACK_MS:-12000}" \
  SX_LAUNCH_STAGGER_MS="${SX_LAUNCH_STAGGER_MS:-800}" \
  SX_BURST_LAUNCH="${SX_BURST_LAUNCH:-0}" \
  SX_READY_GATE="${SX_READY_GATE:-0}" \
  SECURE_EXEC_INLINE_SOCKET_DATA="${SECURE_EXEC_INLINE_SOCKET_DATA:-0}" \
  SECURE_EXEC_RPC_PROFILE="${SECURE_EXEC_RPC_PROFILE:-0}" \
  SECURE_EXEC_RPCPROF="${SECURE_EXEC_RPCPROF:-0}" \
  SECURE_EXEC_RPC_WATCHDOG_MS="${SECURE_EXEC_RPC_WATCHDOG_MS:-0}" \
  SECURE_EXEC_RPC_WATCHDOG_DUMP_MS="${SECURE_EXEC_RPC_WATCHDOG_DUMP_MS:-0}" \
  SECURE_EXEC_RPC_PROFILE_EVERY="${SECURE_EXEC_RPC_PROFILE_EVERY:-5000}" \
  SX_XTEST_DELAY_S="${SX_XTEST_DELAY_S:-0}" SX_FB_STREAM_MS="${SX_FB_STREAM_MS:-33}" \
  SECURE_EXEC_PERFCLOCK="${SECURE_EXEC_PERFCLOCK:-0}" \
  SECURE_EXEC_WASM_SKIP_PREWARM="${SECURE_EXEC_WASM_SKIP_PREWARM:-0}" \
  SECURE_EXEC_WAKEPROF="${SECURE_EXEC_WAKEPROF:-0}" \
  SECURE_EXEC_DATA_NOTIFIER="${SECURE_EXEC_DATA_NOTIFIER:-0}" \
  SECURE_EXEC_INLINE_PEER_NOTIFY="${SECURE_EXEC_INLINE_PEER_NOTIFY:-0}" \
  SECURE_EXEC_KEEP_NAMES="${SECURE_EXEC_KEEP_NAMES:-0}" \
  SECURE_EXEC_PATHOPENPROF="${SECURE_EXEC_PATHOPENPROF:-0}" \
  SECURE_EXEC_RPC_BLOCK_US="${SECURE_EXEC_RPC_BLOCK_US:-300000}" \
  SECURE_EXEC_RECV_OFFBROKER="${SECURE_EXEC_RECV_OFFBROKER:-0}" \
  SECURE_EXEC_T1_RING="${SECURE_EXEC_T1_RING:-0}" \
  SECURE_EXEC_POLL_MAX_WAIT_MS="${SECURE_EXEC_POLL_MAX_WAIT_MS:-0}" \
  SECURE_EXEC_RTPROBE="${SECURE_EXEC_RTPROBE:-0}" \
  SECURE_EXEC_POLLSTAT="${SECURE_EXEC_POLLSTAT:-0}" \
  SECURE_EXEC_DEADLINE_PROBE="${SECURE_EXEC_DEADLINE_PROBE:-0}" \
  SECURE_EXEC_POLLWAITPROF="${SECURE_EXEC_POLLWAITPROF:-0}" \
  SECURE_EXEC_TEE_GUEST_STDERR="${SECURE_EXEC_TEE_GUEST_STDERR:-0}" \
  SECURE_EXEC_POLL_TRACE="${SECURE_EXEC_POLL_TRACE:-0}" \
  SECURE_EXEC_HOPPROF="${SECURE_EXEC_HOPPROF:-0}" \
  "$HOST_BIN" --desktop \
  --server "$EXP/Xvfb.wasm" \
  --dbus "$EXP/dbus-daemon.wasm" \
  --dbus-service "$EXP/xfconfd.wasm" \
  "${CLIENT_ARGS[@]}" \
  --fonts-dir /tmp/vmfonts --locale-dir /tmp/vmlocale \
  --vm-tree /tmp/vmxu3sess-dbus --vm-tree /tmp/vmxu3sess --vm-tree /tmp/vmthemes \
  --vm-tree /tmp/vmxfwm4 --vm-tree /tmp/vmxft --vm-tree /tmp/vmschemas --vm-tree /tmp/vmxkb \
  --sidecar "$SIDECAR_BIN" \
  -- :0 -screen 0 "${W}x${H}x24" -nolisten tcp -nolock -listen local -noreset -fbdir /data \
  >/out/host.log 2>&1 &
HOSTPID=$!

# ★ Peak-thread sampler: track the sidecar's PEAK live-thread count during boot (catches the concurrent-
# compile burst — each guest's WebAssembly.Module compile spawns several V8 threads; N guests compiling at
# once = a thread spike that oversubscribes the cores). Runs in the background for the boot's duration.
echo 0 > /tmp/peak_threads
(
  PEAK=0
  for _ in $(seq 1 500); do
    SP=""
    for p in /proc/[0-9]*; do
      # Match the sidecar by process NAME (comm, truncated to 15 chars), NOT cmdline — the HOST's cmdline
      # contains "--sidecar /path/secure-exec-sidecar" so a cmdline grep matches the host, not the sidecar.
      [ "$(cat "$p/comm" 2>/dev/null)" = "secure-exec-sid" ] && { SP="${p#/proc/}"; break; }
    done
    if [ -n "$SP" ]; then
      T=$(ls "/proc/$SP/task" 2>/dev/null | wc -l)
      [ "${T:-0}" -gt "$PEAK" ] && { PEAK=$T; echo "$PEAK" > /tmp/peak_threads; }
    fi
    sleep 0.25
  done
) &
SAMPLER=$!

python3 - "$W" "$H" "$LAST_IDX" "$SX_BOOT_TIMEOUT" "$COV_SETTLE_S" "$COV_MIN" "$FULL_MIN" <<'PY'
import glob, os, re, sys, time
W, H = int(sys.argv[1]), int(sys.argv[2])
LAST_IDX = int(sys.argv[3]); TIMEOUT = float(sys.argv[4])
SETTLE_S = float(sys.argv[5]); COV_MIN = float(sys.argv[6]); FULL_MIN = float(sys.argv[7])
with open("/tmp/host_t0") as f: T0 = float(f.read().strip())
HOST_LOG = "/out/host.log"
HDR = None

def fb_path():
    g = glob.glob("/tmp/secure-exec-sidecar-shadow-vm-*/data/Xvfb_screen0")
    return g[0] if g else None

def coverage():
    global HDR
    p = fb_path()
    if not p: return None
    try:
        with open(p, "rb") as f: b = f.read()
    except Exception:
        return None
    if len(b) < W*H*4: return None
    if HDR is None: HDR = len(b) - W*H*4
    mv = memoryview(b); nb = tot = 0
    for i in range(HDR, HDR + W*H*4 - 3, 64*4):
        if mv[i] > 16 or mv[i+1] > 16 or mv[i+2] > 16: nb += 1
        tot += 1
    return 100.0 * nb / max(tot, 1)

def all_launched():
    try:
        with open(HOST_LOG) as f: t = f.read()
    except Exception:
        return False
    return f"launched xclient{LAST_IDX}" in t

def save_ppm(path, b):
    if HDR is None: return
    px = b[HDR:HDR+W*H*4]
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (W, H))
        row = bytearray(W*H*3)
        for i in range(W*H):
            row[i*3]=px[i*4+2]; row[i*3+1]=px[i*4+1]; row[i*3+2]=px[i*4]
        f.write(row)

def now(): return time.time() - T0

last = None; stable_since = None; launched_at = None; firstpaint_at = None; maxcov = 0.0
deadline = time.time() + TIMEOUT
while time.time() < deadline:
    c = coverage(); t = now()
    if all_launched() and launched_at is None:
        launched_at = t; print(f"[t] all {LAST_IDX+1} clients launched: {t*1000:.0f}ms", flush=True)
    if c is not None:
        if c > maxcov: maxcov = c
        if firstpaint_at is None and c > COV_MIN:
            firstpaint_at = t; print(f"[t] first paint (cov>{COV_MIN}%): {t*1000:.0f}ms", flush=True)
        # A FULL desktop settles at high coverage (xfdesktop background + panel + windows ~95%). Require
        # c >= FULL_MIN so a panel-only / degraded render (~5%) is NOT accepted as success.
        if last is not None and abs(c - last) < 0.5 and c >= FULL_MIN:
            if stable_since is None: stable_since = t
            elif (t - stable_since) >= SETTLE_S and all_launched():
                p = fb_path(); b = open(p, "rb").read() if p else b""
                save_ppm("/out/boot-settle.ppm", b)
                print(f"[t] FULL desktop settled: {stable_since*1000:.0f}ms (cov={c:.1f}%)", flush=True)
                print(f"BOOT_MS={stable_since*1000:.0f}", flush=True)
                sys.exit(0)
        else:
            stable_since = None
        last = c
    time.sleep(0.3)

print(f"[t] BOOT TIMEOUT after {TIMEOUT:.0f}s (last cov={last}, maxcov={maxcov:.1f}%, need>={FULL_MIN}%, launched={all_launched()})", flush=True)
p = fb_path()
if p:
    try: save_ppm("/out/boot-timeout.ppm", open(p, "rb").read())
    except Exception: pass
print("BOOT_TIMEOUT", flush=True)
sys.exit(1)
PY
RC=$?

# ★ CPU accounting: of the boot wall-time, how much did the sidecar (= ALL wasm guests + dispatch) actually
# spend ON-CPU (genuine wasm compute) vs BLOCKED (syscalls / poll-waits / cross-thread waiting)? Sampled at
# settle/timeout. CPU-s << wall = wait-bound (runtime-fixable); CPU-s ≈ wall × busy-threads = compute-bound.
SPID=""
for p in /proc/[0-9]*; do
  if [ "$(cat "$p/comm" 2>/dev/null)" = "secure-exec-sid" ]; then
    SPID="${p#/proc/}"; break
  fi
done
if [ -n "$SPID" ] && [ -r "/proc/$SPID/stat" ]; then
  SC_CPU=$(awk '{printf "%.1f", ($14+$15)/100}' "/proc/$SPID/stat" 2>/dev/null)
  SC_THR=$(ls "/proc/$SPID/task" 2>/dev/null | wc -l)
  NCPU=$(nproc 2>/dev/null)
  echo "SIDECAR_CPU_SECONDS=${SC_CPU} SIDECAR_THREADS=${SC_THR} NCPU=${NCPU} (compare to boot wall BOOT_MS)"
  # ★ SPIN CHECK: the desktop is now settled/IDLE. Sample CPU over 4s of doing NOTHING. If cores are still
  # burning, the CPU is a BUSY-SPIN (poll loops), not productive work. Native idles at ~0.
  C1=$(awk '{print $14+$15}' "/proc/$SPID/stat" 2>/dev/null)
  sleep 4
  C2=$(awk '{print $14+$15}' "/proc/$SPID/stat" 2>/dev/null)
  echo "SIDECAR_IDLE_CPU_CORES=$(awk "BEGIN{printf \"%.2f\", ($C2-$C1)/100/4}")  (>0.1 = busy-spin, not real work)"
  # ★ THREAD ENUM: what ARE the ~160 threads? (comm histogram). Isolate-execution threads vs helper threads.
  echo "=== sidecar thread comm histogram (top 20) ==="
  for t in /proc/$SPID/task/*/comm; do cat "$t" 2>/dev/null; done | sort | uniq -c | sort -rn | head -20
else
  echo "SIDECAR_CPU_SECONDS=unknown (sidecar pid not found)"
fi
kill "${SAMPLER:-0}" 2>/dev/null
echo "PEAK_SIDECAR_THREADS=$(cat /tmp/peak_threads 2>/dev/null)"

echo "=== select-block probes (dispatch-task holds >1s) ==="
grep -aE "\[select-block\]|\[pump-gap\]" /out/host.log | head -40 || true
echo "=== launch markers ==="
grep -aE "launched xclient|dbus-daemon launched|dbus service|interactive window" /out/host.log | head -12 || true
kill "$HOSTPID" 2>/dev/null
sleep 0.5
kill -9 "$HOSTPID" 2>/dev/null
exit $RC
