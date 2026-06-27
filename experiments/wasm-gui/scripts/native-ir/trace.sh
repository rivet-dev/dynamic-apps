#!/bin/bash
set -u
W=800; H=600
mkdir -p /fb /root/.config/gtk-3.0
printf '[Settings]\ngtk-cursor-blink=false\n' > /root/.config/gtk-3.0/settings.ini
export HOME=/root
Xvfb :99 -screen 0 ${W}x${H}x24 -fbdir /fb -nolisten tcp >/tmp/xvfb.log 2>&1 &
export DISPLAY=:99
for i in $(seq 1 100); do [ -e /fb/Xvfb_screen0 ] && break; sleep 0.05; done
strace -f -ttt -T -e trace=poll,ppoll,recvmsg,sendmsg -o /fb/strace.log mousepad >/tmp/app.log 2>&1 &
sleep 4
xdotool mousemove 300 400 click 1; sleep 0.3
xdotool type --delay 40 Hello; sleep 0.6
date +%s.%N > /fb/keytime.txt
xdotool type --delay 0 X
sleep 0.5
pkill -9 mousepad strace Xvfb 2>/dev/null; sleep 0.3
