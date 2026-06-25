#!/usr/bin/env bash
# Stage the xfce4-panel xfconf config into a VM tree. xfce4-panel, on first run with no config, forks
# `xfce4-panel-migrate` to create the default config -> "Failed to fork" in wasm (no fork). Pre-staging a
# valid xfconf channel (configver>=2) makes the panel find a config and skip the migration fork entirely.
# The config is a real xfconf perchannel xml (the same format as the xfwm4/xsettings channels). Output: $1
# (default /tmp/vmxfce4panel). Set PANEL_PLUGINS=1 once the gmodule static-plugin path works to add the
# Whisker/tasklist/clock/systray plugin set; default is an empty bar (plugins load via dlopen, WIP).
set -uo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-/tmp/vmxfce4panel}"
CHDIR="$OUT/root/.config/xfce4/xfconf/xfce-perchannel-xml"
rm -rf "$OUT"; mkdir -p "$CHDIR" "$OUT/root/.cache"
cat > "$CHDIR/xfce4-panel.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-panel" version="1.0">
  <property name="configver" type="int" value="2"/>
  <property name="panels" type="array">
    <value type="int" value="1"/>
    <property name="panel-1" type="empty">
      <property name="position" type="string" value="p=6;x=0;y=0"/>
      <property name="length" type="uint" value="100"/>
      <property name="position-locked" type="bool" value="true"/>
      <property name="size" type="uint" value="28"/>
      <property name="plugin-ids" type="array"/>
    </property>
  </property>
  <property name="plugins" type="empty"/>
</channel>
XML
echo "staged xfce4-panel xfconf config (empty bar, configver=2 -> skips the migration fork) into $OUT"
