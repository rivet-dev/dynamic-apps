#!/usr/bin/env bash
# Stage the xfce4-settings-manager fixture VM tree: the Settings menu, the category .directory files, and the
# settings dialogs' .desktop. The manager (xfce-settings-manager-dialog.c) does garcon_menu_load then, per
# top-level sub-menu, garcon_menu_get_directory() -- and at line ~1252 SKIPS any sub-menu whose directory is NULL.
# So each grid category REQUIRES a .directory file; without them the grid renders empty (the long T37-T43 mystery).
# Each settings .desktop carries OnlyShowIn=XFCE, so the host must advertise XDG_CURRENT_DESKTOP=XFCE (done in the
# host cenv) or garcon hides them. Output VM tree: $1 (default /tmp/vmsettingsmenu). Mount it with --vm-tree.
set -uo pipefail
OUT="${1:-/tmp/vmsettingsmenu}"
EXP="$(cd "$(dirname "$0")/.." && pwd)"; SS="$EXP/third_party/xfce4-settings"
rm -rf "$OUT"; mkdir -p "$OUT/etc/xdg/menus" "$OUT/usr/share/applications" "$OUT/usr/share/desktop-directories"
# ALL dialog .desktop (glob *.desktop, NOT xfce-* -- that misses xfce4-accessibility-settings.desktop).
for f in $(find "$SS/dialogs" -name '*.desktop' 2>/dev/null); do cp "$f" "$OUT/usr/share/applications/"; done
# The .directory files the manager requires (Name shows as the grid category header).
for c in personal:Personal:preferences-desktop hardware:Hardware:preferences-desktop-peripherals system:System:preferences-system; do
  n="${c%%:*}"; rest="${c#*:}"; name="${rest%%:*}"; icon="${rest#*:}"
  printf '[Desktop Entry]\nVersion=1.0\nType=Directory\nName=%s\nIcon=%s\n' "$name" "$icon" \
    > "$OUT/usr/share/desktop-directories/xfce-$n.directory"
done
# The menu: absolute AppDir/DirectoryDir (no XDG_DATA_DIRS dependency) + the 3 category sub-menus, each pulling
# its X-XFCE-*Settings category. Empty categories (no matching .desktop) are auto-skipped by the manager.
cat > "$OUT/etc/xdg/menus/xfce-settings-manager.menu" <<'XML'
<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" "http://www.freedesktop.org/standards/menu-spec/menu-1.0.dtd">
<Menu>
  <Name>Settings</Name>
  <AppDir>/usr/share/applications</AppDir>
  <DirectoryDir>/usr/share/desktop-directories</DirectoryDir>
  <Menu><Name>Personal</Name><Directory>xfce-personal.directory</Directory>
    <Include><And><Category>X-XFCE-PersonalSettings</Category></And></Include></Menu>
  <Menu><Name>Hardware</Name><Directory>xfce-hardware.directory</Directory>
    <Include><And><Category>X-XFCE-HardwareSettings</Category></And></Include></Menu>
  <Menu><Name>System</Name><Directory>xfce-system.directory</Directory>
    <Include><And><Category>X-XFCE-SystemSettings</Category></And></Include></Menu>
</Menu>
XML
echo "staged settings-manager fixture: $(ls "$OUT/usr/share/applications" | wc -l) .desktop + 3 .directory + menu -> $OUT"
