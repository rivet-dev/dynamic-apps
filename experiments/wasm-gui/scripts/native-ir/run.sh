#!/bin/bash
set -u
W=800; H=600
mkdir -p /fb /root/.config/gtk-3.0
# trustworthy-metric parity: disable caret blink (same as the wasm fixture)
cat > /root/.config/gtk-3.0/settings.ini <<INI
[Settings]
gtk-cursor-blink=false
gtk-cursor-blink-time=2000000
INI
export HOME=/root
Xvfb :99 -screen 0 ${W}x${H}x24 -fbdir /fb -nolisten tcp >/tmp/xvfb.log 2>&1 &
XPID=$!
export DISPLAY=:99
for i in $(seq 1 100); do [ -e /fb/Xvfb_screen0 ] && break; sleep 0.05; done
mousepad >/tmp/app.log 2>&1 &
APID=$!
python3 /measure.py
kill $APID $XPID 2>/dev/null
