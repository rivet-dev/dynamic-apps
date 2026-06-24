#!/usr/bin/env bash
# Stage a self-contained freedesktop application-menu fixture tree (a .menu + .desktop + .directory
# files) for the M8.3 menu-cache-gen test. Data/fixtures only — no component source. The .menu uses
# absolute in-VM <AppDir>/<DirectoryDir> paths so menu-cache-gen needs no XDG env to find them.
set -euo pipefail
OUT="${1:-/tmp/vmmenucache}"
rm -rf "$OUT"; mkdir -p "$OUT/menus" "$OUT/applications" "$OUT/desktop-directories"

mkfile() { mkdir -p "$(dirname "$1")"; cat > "$1"; }

# --- category .directory entries ---
for c in Accessories:Accessories Internet:Internet System:System Office:Office; do
  name="${c%%:*}"
  mkfile "$OUT/desktop-directories/$name.directory" <<EOF
[Desktop Entry]
Type=Directory
Name=$name
Icon=folder
EOF
done

# --- application .desktop entries (Categories drive the menu placement) ---
add_app() { # name exec categories
  mkfile "$OUT/applications/$1.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$1
Exec=$2
Icon=$1
Categories=$3
EOF
}
add_app "File Manager"   "pcmanfm %U"  "System;FileManager;Utility;"
add_app "Terminal"       "xterm"       "System;TerminalEmulator;Utility;"
add_app "Text Editor"    "editor %F"   "Utility;TextEditor;"
add_app "Web Browser"    "browser %u"  "Network;WebBrowser;"
add_app "Mail"           "mail"        "Network;Email;"
add_app "Calculator"     "calc"        "Utility;Calculator;"
add_app "Spreadsheet"    "sheet %F"    "Office;Spreadsheet;"

# --- the menu layout: categorized application menu ---
mkfile "$OUT/menus/sandbox-applications.menu" <<EOF
<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN"
 "http://www.freedesktop.org/standards/menu-spec/menu-1.0.dtd">
<Menu>
  <Name>Applications</Name>
  <AppDir>/applications</AppDir>
  <DirectoryDir>/desktop-directories</DirectoryDir>
  <Menu>
    <Name>Accessories</Name>
    <Directory>Accessories.directory</Directory>
    <Include><And><Category>Utility</Category></And></Include>
  </Menu>
  <Menu>
    <Name>Internet</Name>
    <Directory>Internet.directory</Directory>
    <Include><And><Category>Network</Category></And></Include>
  </Menu>
  <Menu>
    <Name>Office</Name>
    <Directory>Office.directory</Directory>
    <Include><And><Category>Office</Category></And></Include>
  </Menu>
  <Menu>
    <Name>System</Name>
    <Directory>System.directory</Directory>
    <Include><And><Category>System</Category></And></Include>
  </Menu>
</Menu>
EOF
echo "staged menu-cache fixtures at $OUT ($(find "$OUT" -type f | wc -l) files)"
