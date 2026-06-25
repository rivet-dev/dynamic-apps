#!/usr/bin/env bash
# XU6: stage compiled GSettings schemas (mousepad + gtk) into a vm-tree so GtkApplication apps that read
# GSettings (mousepad-settings.c etc.) find their schema and do not warn/early-exit. Constraint #5: the
# schemas are the UNMODIFIED upstream .gschema.xml; only the staging (a runtime fixture) is ours.
set -uo pipefail
cd "$(dirname "$0")/.."; EXP="$(pwd)"
export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"
SD=/tmp/schemas-src; rm -rf "$SD"; mkdir -p "$SD"
cp third_party/mousepad/mousepad/org.xfce.mousepad.gschema.xml "$SD/" 2>/dev/null
cp third_party/gtk3/gtk/org.gtk.Settings.*.gschema.xml "$SD/" 2>/dev/null
glib-compile-schemas "$SD"
VMS="${1:-/tmp/vmschemas}"; rm -rf "$VMS"; mkdir -p "$VMS/usr/share/glib-2.0/schemas"
cp "$SD/gschemas.compiled" "$VMS/usr/share/glib-2.0/schemas/"
echo "staged gschemas -> $VMS ($(stat -c%s "$VMS/usr/share/glib-2.0/schemas/gschemas.compiled") bytes)"
