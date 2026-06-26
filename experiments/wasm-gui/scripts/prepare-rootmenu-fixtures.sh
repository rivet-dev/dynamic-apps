#!/usr/bin/env bash
# Stage a garcon ROOT-MENU fixture for xfdesktop's right-click desktop menu: xfce-applications.menu (the
# name garcon_menu_new_applications() loads) under /etc/xdg/menus where garcon searches ($XDG_CONFIG_DIRS),
# plus .desktop apps + .directory categories under /usr/share. ABSOLUTE <AppDir>/<DirectoryDir> so it
# resolves with no XDG env. Data/fixtures only (constraint #5). Mirrors prepare-menucache-fixtures.sh.
set -euo pipefail
OUT="${1:-/tmp/vmrootmenu}"
rm -rf "$OUT"
mkdir -p "$OUT/etc/xdg/menus" "$OUT/usr/share/applications" "$OUT/usr/share/desktop-directories"
mkfile() { mkdir -p "$(dirname "$1")"; cat > "$1"; }

for c in Accessories Internet System Office Graphics; do
  mkfile "$OUT/usr/share/desktop-directories/xfce-$c.directory" <<EOF
[Desktop Entry]
Type=Directory
Name=$c
Icon=applications-$c
EOF
done

add_app() { # name exec categories
  mkfile "$OUT/usr/share/applications/$(echo "$1" | tr ' ' '-').desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$1
Exec=$2
Icon=$3
Categories=$4
EOF
}
add_app "File Manager"  "pcmanfm %U" "system-file-manager"   "System;FileManager;Utility;"
add_app "Terminal"      "xterm"      "utilities-terminal"    "System;TerminalEmulator;Utility;"
add_app "Text Editor"   "mousepad"   "accessories-text-editor" "Utility;TextEditor;"
add_app "Web Browser"   "browser"    "web-browser"           "Network;WebBrowser;"
add_app "Mail Reader"   "mail"       "internet-mail"         "Network;Email;"
add_app "Image Viewer"  "ristretto"  "multimedia-photo-viewer" "Graphics;Viewer;"
add_app "Screenshot"    "xfce4-screenshooter" "applets-screenshooter" "Graphics;Utility;"

mkfile "$OUT/etc/xdg/menus/xfce-applications.menu" <<EOF
<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN"
 "http://www.freedesktop.org/standards/menu-spec/menu-1.0.dtd">
<Menu>
  <Name>Xfce</Name>
  <AppDir>/usr/share/applications</AppDir>
  <DirectoryDir>/usr/share/desktop-directories</DirectoryDir>
  <Menu>
    <Name>Accessories</Name><Directory>xfce-Accessories.directory</Directory>
    <Include><And><Category>Utility</Category></And></Include>
  </Menu>
  <Menu>
    <Name>Graphics</Name><Directory>xfce-Graphics.directory</Directory>
    <Include><And><Category>Graphics</Category></And></Include>
  </Menu>
  <Menu>
    <Name>Internet</Name><Directory>xfce-Internet.directory</Directory>
    <Include><And><Category>Network</Category></And></Include>
  </Menu>
  <Menu>
    <Name>System</Name><Directory>xfce-System.directory</Directory>
    <Include><And><Category>System</Category></And></Include>
  </Menu>
</Menu>
EOF
echo "staged root-menu fixtures at $OUT ($(find "$OUT" -type f | wc -l) files)"
