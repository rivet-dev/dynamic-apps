#!/usr/bin/env bash
# Stage an icon-theme VM tree for GTK apps (pcmanfm et al.). GTK3's default gtk-icon-theme-name is
# "Adwaita" and it falls back through "hicolor", but neither theme exists in the base VFS, so every
# icon lookup (user-home, folder, mimetypes) fails. gdk-pixbuf is built with the PNG loader compiled
# in (-Dbuiltin_loaders=png), so we stage the PNG raster icons (NOT the scalable/ SVGs, which would
# need librsvg). The host installs this tree at the VM root via --vm-tree, mirroring the in-VM layout
# (usr/share/icons/{Adwaita,hicolor}/...). Output: $1 (default /tmp/vmicons).
set -uo pipefail
OUT="${1:-/tmp/vmicons}"
SRC_ADWAITA="${ADWAITA_SRC:-/usr/share/icons/Adwaita}"
SRC_HICOLOR="${HICOLOR_SRC:-/usr/share/icons/hicolor}"
# GTK scales PNGs to the requested size, so a couple of sizes cover the file-manager UI (side pane 16,
# list view 24, icon view 48). The vm-tree install is one wire write per file, so keep the set small:
# stage a CURATED list of the icon names a file manager actually renders, not all ~3500 Adwaita PNGs.
SIZES="${ICON_SIZES:-16x16 24x24 48x48}"
NAMES="${ICON_NAMES:-\
user-home user-desktop user-trash folder folder-open folder-remote folder-new \
network-server network-workgroup drive-harddisk drive-removable-media media-removable \
text-x-generic text-x-generic-template application-x-generic application-x-executable \
inode-directory image-x-generic audio-x-generic video-x-generic text-html application-pdf \
package-x-generic font-x-generic text-x-script application-x-archive \
go-up go-previous go-next go-home go-jump edit-find view-refresh document-open document-new \
edit-cut edit-copy edit-paste edit-delete list-add list-remove window-close \
view-list-symbolic view-grid-symbolic open-menu-symbolic pan-up-symbolic pan-down-symbolic \
image-missing dialog-error dialog-information emblem-symbolic-link}"

[ -d "$SRC_ADWAITA" ] || { echo "no host Adwaita theme at $SRC_ADWAITA"; exit 1; }
rm -rf "$OUT"
DST_ADWAITA="$OUT/usr/share/icons/Adwaita"
mkdir -p "$DST_ADWAITA"
# index.theme drives directory lookup; copy it verbatim (it lists more dirs than we stage — GTK just
# skips the absent ones).
cp "$SRC_ADWAITA/index.theme" "$DST_ADWAITA/" 2>/dev/null || true

n=0
for s in $SIZES; do
  [ -d "$SRC_ADWAITA/$s" ] || continue
  for name in $NAMES; do
    # Each icon lives under exactly one category subdir (places/, mimetypes/, actions/, ...); find it.
    f="$(find "$SRC_ADWAITA/$s" -name "$name.png" -print -quit 2>/dev/null)"
    [ -n "$f" ] || continue
    rel="${f#$SRC_ADWAITA/}"
    mkdir -p "$DST_ADWAITA/$(dirname "$rel")"
    cp "$f" "$DST_ADWAITA/$rel"
    n=$((n+1))
  done
done

# hicolor: the universal fallback theme. Its index.theme must exist or GTK warns it "was not found".
if [ -d "$SRC_HICOLOR" ]; then
  DST_HICOLOR="$OUT/usr/share/icons/hicolor"
  mkdir -p "$DST_HICOLOR"
  cp "$SRC_HICOLOR/index.theme" "$DST_HICOLOR/" 2>/dev/null || true
fi

# Stage xfce4-settings' own org.xfce.settings.* app icons into hicolor/{size}/apps so the Settings Manager grid
# and the settings dialogs show REAL icons instead of the missing-icon placeholder. PNG only (GTK builtin loader);
# the host hicolor index.theme (copied above) already lists the {size}/apps dirs.
SETTINGS_ICONS="$(dirname "$0")/../third_party/xfce4-settings/icons"
if [ -d "$SETTINGS_ICONS" ] && [ -d "$OUT/usr/share/icons/hicolor" ]; then
  si=0
  for szdir in "$SETTINGS_ICONS"/*x*/; do
    [ -d "$szdir" ] || continue; sz=$(basename "$szdir"); mkdir -p "$OUT/usr/share/icons/hicolor/$sz/apps"
    for f in "$szdir"org.xfce.*.png; do [ -f "$f" ] && { cp "$f" "$OUT/usr/share/icons/hicolor/$sz/apps/"; si=$((si+1)); }; done
  done
  echo "staged $si xfce4-settings app icons into hicolor"
fi

# GTK reads the user settings.ini for the icon theme; pin Adwaita explicitly (belt-and-suspenders,
# since the built-in default is already Adwaita).
mkdir -p "$OUT/root/.config/gtk-3.0"
cat > "$OUT/root/.config/gtk-3.0/settings.ini" <<'INI'
[Settings]
gtk-icon-theme-name=Adwaita
gtk-fallback-icon-theme=hicolor
gtk-cursor-blink=false
gtk-cursor-blink-time=2000000
INI

echo "staged $n Adwaita PNG icons (sizes: $SIZES) + hicolor index + gtk settings into $OUT"
