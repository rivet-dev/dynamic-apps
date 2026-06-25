#!/usr/bin/env bash
# Stage the xfce4-panel runtime fixtures into a VM tree:
#  - the xfconf xfce4-panel config (configver=2 -> the panel finds a config and skips the first-run
#    `fork(xfce4-panel-migrate)` that fails in wasm).
#  - for each STATIC plugin linked into xfce4-panel.wasm (via the gmodule shim): a STUB <name>.so at the
#    panel's compile-time PANEL_PLUGINS_LIB_DIR (the panel g_file_test()s the file EXISTS before opening;
#    the shim ignores its bytes) and the plugin .desktop (X-XFCE-Internal=TRUE -> INTERNAL mode, no
#    wrapper fork) at the panel data dir, plus the plugin entry in the config.
# Output: $1 (default /tmp/vmxfce4panel). PLUGINS env = space list of plugin names to enable (default
# "separator"; extend as clock/tasklist/systray/whisker get static-linked).
set -uo pipefail
cd "$(dirname "$0")/.."
EXP="$(pwd)"
OUT="${1:-/tmp/vmxfce4panel}"
PLUGINS="${PLUGINS:-clock tasklist systray separator}"
PREFIX="$EXP/third_party/wasm-prefix-threads"
LIBDIR="$PREFIX/lib/xfce4"                 # panel LIBDIR = $(libdir)/xfce4
SODIR="$OUT$LIBDIR/panel/plugins"          # PANEL_PLUGINS_LIB_DIR = LIBDIR/panel/plugins
DESKDIR="$OUT/usr/share/xfce4/panel/plugins"  # PANEL_PLUGINS_DATA_DIR = (datadir=/usr/share)/xfce4/panel/plugins
CHDIR="$OUT/root/.config/xfce4/xfconf/xfce-perchannel-xml"
rm -rf "$OUT"; mkdir -p "$SODIR" "$DESKDIR" "$CHDIR" "$OUT/root/.cache"

# Per-plugin: stub .so + .desktop (copy upstream .desktop.in, force X-XFCE-Internal=TRUE).
idx=0; pluginids=""; pluginprops=""
for p in $PLUGINS; do
  idx=$((idx+1))
  printf 'stub' > "$SODIR/lib$p.so"
  SRC_IN="$EXP/third_party/xfce4-panel/plugins/$p/$p.desktop.in"
  if [ -f "$SRC_IN" ]; then
    sed -e 's/^_Name=/Name=/' -e 's/^_Comment=/Comment=/' \
        -e 's/^X-XFCE-Internal=.*/X-XFCE-Internal=TRUE/' "$SRC_IN" > "$DESKDIR/$p.desktop"
    grep -q '^X-XFCE-Internal' "$DESKDIR/$p.desktop" || echo 'X-XFCE-Internal=TRUE' >> "$DESKDIR/$p.desktop"
  else
    printf '[Xfce Panel]\nType=X-XFCE-PanelPlugin\nName=%s\nX-XFCE-Module=%s\nX-XFCE-Internal=TRUE\nX-XFCE-API=2.0\n' "$p" "$p" > "$DESKDIR/$p.desktop"
  fi
  pluginids="$pluginids        <value type=\"int\" value=\"$idx\"/>
"
  pluginprops="$pluginprops    <property name=\"plugin-$idx\" type=\"string\" value=\"$p\"/>
"
done

cat > "$CHDIR/xfce4-panel.xml" <<XML
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
      <property name="plugin-ids" type="array">
$pluginids      </property>
    </property>
  </property>
  <property name="plugins" type="empty">
$pluginprops  </property>
</channel>
XML
echo "staged xfce4-panel config + $(echo $PLUGINS | wc -w) static plugin(s) ($PLUGINS) into $OUT"
