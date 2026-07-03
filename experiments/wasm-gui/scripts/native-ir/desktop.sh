#!/bin/bash
# Native baseline for the wasm-gui DESKTOP workload (Xvfb + xfwm4 + xfce4-panel +
# xfdesktop + Thunar + mousepad). Modes via MODE env: desktop | scale | xcount.
set -u
export HOME=/root
mkdir -p /fb /out /root/.config/gtk-3.0 /root/.config/xfce4/xfconf/xfce-perchannel-xml /root/.cache
# parity with the wasm fixture: caret blink off
cat > /root/.config/gtk-3.0/settings.ini <<INI
[Settings]
gtk-cursor-blink=false
gtk-cursor-blink-time=2000000
INI
# pre-seed the panel config (same as scripts/prepare-xfce4-panel.sh does for the wasm run)
# so xfce4-panel skips the first-run wizard dialog.
cp /etc/xdg/xfce4/panel/default.xml /root/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml 2>/dev/null || true
exec python3 /desktop-measure.py
