#!/usr/bin/env bash
cd /home/nathan/secure-exec-wasmgui/experiments/wasm-gui
( SECURE_EXEC_V8PROF=0 TIMEOUT=210 OUT=/tmp/lxde-mx.log bash scripts/test-m8-lxde.sh >/tmp/lxde-mx-run.log 2>&1 ) &
echo "how many sidecars exist mid-run will be shown when spin detected"
HOT=""
for i in $(seq 1 90); do
  PIDS=$(pgrep -f "target/debug/secure-exec-sidecar")
  [ -z "$PIDS" ] && { sleep 1; continue; }
  declare -A A=()
  for p in $PIDS; do A[$p]=$(awk '{print $14+$15}' /proc/$p/stat 2>/dev/null); done
  sleep 1
  best=0; bestp=""
  for p in $PIDS; do
    b=$(awk '{print $14+$15}' /proc/$p/stat 2>/dev/null)
    d=$(( ${b:-0} - ${A[$p]:-0} ))
    if [ "$d" -gt "$best" ] 2>/dev/null; then best=$d; bestp=$p; fi
  done
  if [ "$best" -ge 70 ] 2>/dev/null; then HOT=$bestp; echo "[t~${i}s] $(echo "$PIDS"|wc -l) sidecars; HOTTEST=$bestp delta=${best}%"; break; fi
done
[ -z "$HOT" ] && { echo "no spin caught (max deltas stayed low)"; exit 0; }
timeout 6 strace -f -e trace=futex -p "$HOT" 2>/tmp/futex-raw.txt
grep -E "futex\(" /tmp/futex-raw.txt > /tmp/futex-lines.txt
echo "futex calls in 6s: $(wc -l </tmp/futex-lines.txt)"
echo "-- OP histogram --"; grep -oE "FUTEX_[A-Z_|]+" /tmp/futex-lines.txt | sort | uniq -c | sort -rn | head
echo "-- RETURN (first token) histogram --"; sed -E 's/.*\) += +//' /tmp/futex-lines.txt | awk '{print $1}' | sort | uniq -c | sort -rn | head
echo "-- WAIT samples --"; grep -E "FUTEX_WAIT" /tmp/futex-lines.txt | head -8
echo "-- WAKE samples --"; grep -E "FUTEX_WAKE" /tmp/futex-lines.txt | head -4
echo "-- top threads --"; grep -oE "^\[pid +[0-9]+\]" /tmp/futex-lines.txt | sort | uniq -c | sort -rn | head
