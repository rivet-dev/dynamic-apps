#!/usr/bin/env bash
# Stage a VM filesystem tree for Xft/fontconfig: a couple of TrueType fonts plus an /etc/fonts config
# pointing fontconfig at them. The host installs this tree at the VM root with --vm-tree, so guest Xft
# clients can resolve fontconfig patterns ("DejaVu Sans-22") to a real TTF and render antialiased text.
# Output: $1 (default /tmp/vmxft), mirroring the in-VM layout (etc/, usr/share/fonts/, var/cache/...).
set -uo pipefail
OUT="${1:-/tmp/vmxft}"
rm -rf "$OUT"; mkdir -p "$OUT/etc/fonts" "$OUT/usr/share/fonts/truetype" "$OUT/var/cache/fontconfig"
# install_tree only copies files (skips empty dirs), so drop a marker to ensure the cache dir exists.
: > "$OUT/var/cache/fontconfig/.keep"

# Pull a small, predictable set of TTFs from the host. Linux commonly has DejaVu/Liberation under
# /usr/share; macOS has compact Verdana/Arial Narrow/SF fonts under /System/Library. Keep each file
# comfortably below the sidecar protocol frame limit because VM trees are installed file-by-file.
n=0
for f in /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
         /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf \
         /usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf \
         /usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf \
         "/System/Library/Fonts/Supplemental/Verdana.ttf" \
         "/System/Library/Fonts/Supplemental/Arial Narrow.ttf" \
         /System/Library/Fonts/SFNSMono.ttf; do
  [ -f "$f" ] && cp "$f" "$OUT/usr/share/fonts/truetype/" && n=$((n+1))
done
[ "$n" -gt 0 ] || { echo "no host TTFs found under /usr/share/fonts or /System/Library/Fonts"; exit 1; }

cat > "$OUT/etc/fonts/fonts.conf" <<'XML'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>/usr/share/fonts</dir>
  <cachedir>/var/cache/fontconfig</cachedir>
  <!-- Always antialias; this is the whole point of the Xft path. -->
  <match target="font"><edit name="antialias" mode="assign"><bool>true</bool></edit></match>
  <match target="font"><edit name="hinting" mode="assign"><bool>true</bool></edit></match>
  <!-- Map the generic families to what we shipped. -->
  <alias><family>sans-serif</family><prefer><family>DejaVu Sans</family><family>Verdana</family></prefer></alias>
  <alias><family>serif</family><prefer><family>DejaVu Serif</family><family>Verdana</family></prefer></alias>
  <alias><family>monospace</family><prefer><family>DejaVu Sans Mono</family><family>SF Mono</family><family>Verdana</family></prefer></alias>
</fontconfig>
XML
echo "staged $n TTFs + fonts.conf into $OUT"
