#!/usr/bin/env bash
# Prepare a minimal X core font directory (/tmp/vmfonts by default) the host installs into the VM
# (--fonts-dir) so the wasm X server can serve real fonts via `-fp /fonts`. Uses the host system's
# X11 PCF fonts + mkfontdir. Run once before the M6 desktop test/example.
set -euo pipefail
OUT="${1:-/tmp/vmfonts}"
MISC=/usr/share/fonts/X11/misc
DPI=/usr/share/fonts/X11/75dpi
command -v mkfontdir >/dev/null || { echo "need mkfontdir (xfonts-utils)"; exit 1; }
[ -d "$MISC" ] || { echo "need X core fonts at $MISC (xfonts-base)"; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT"
for f in cursor 6x13 6x13B 7x13 7x13B 8x13 8x13B 9x15 9x15B 10x20 5x8 12x24; do
  [ -f "$MISC/$f.pcf.gz" ] && cp "$MISC/$f.pcf.gz" "$OUT/" || true
done
for f in helvR12 helvB12 helvR14 helvB14 timR12 timR14 courR12 courB12; do
  [ -f "$DPI/$f.pcf.gz" ] && cp "$DPI/$f.pcf.gz" "$OUT/" || true
done
cd "$OUT"
mkfontdir .
# Add iso8859-1 duplicate entries (the PCF files contain Latin-1) for the C-locale fontset, plus
# the classic aliases apps expect.
tail -n +2 fonts.dir | sed 's/iso10646-1$/iso8859-1/' > /tmp/.fd8859 || true
{ tail -n +2 fonts.dir; cat /tmp/.fd8859; } | sort -u > /tmp/.fdall
{ echo "$(wc -l < /tmp/.fdall)"; cat /tmp/.fdall; } > fonts.dir
cat > fonts.alias <<'EOF'
fixed -misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso8859-1
variable -adobe-helvetica-bold-r-normal--12-120-75-75-p-70-iso8859-1
EOF
echo "prepared $OUT ($(ls *.pcf.gz | wc -l) PCF fonts, $(head -1 fonts.dir) fonts.dir entries)"
