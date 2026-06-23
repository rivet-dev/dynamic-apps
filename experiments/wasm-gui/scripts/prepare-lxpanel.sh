#!/usr/bin/env bash
# Generate the LXDE panel config tree for the wasm lxpanel run, installed into the VM at
# /etc/xdg/lxpanel/default/ via the host's --vm-tree. lxpanel's config parser is LINE-BASED: every
# setting must be on its own line (a single-line "Global { a=1 b=2 }" block parses only partially and
# leaves edge/widthtype/height at defaults, which lays the panel out as a vertical right-edge strip).
#
#   usage: prepare-lxpanel.sh [DEST]   (default /tmp/vmlxpanel)
set -euo pipefail
DEST="${1:-/tmp/vmlxpanel}"
PANELS="$DEST/etc/xdg/lxpanel/default/panels"
mkdir -p "$PANELS"

cat > "$PANELS/panel" <<'EOF'
Global {
    edge=bottom
    align=left
    margin=0
    widthtype=percent
    width=100
    height=26
    transparent=0
    tintcolor=#000000
    alpha=0
    setdocktype=1
    setpartialstrut=1
    usefontcolor=1
    fontcolor=#ffffff
    usefontsize=1
    fontsize=12
    background=0
    iconsize=24
}
Plugin {
    type=space
    Config {
        Size=4
    }
}
Plugin {
    type=dclock
    Config {
        ClockFmt=%H:%M:%S
        TooltipFmt=%A %x
        BoldFont=0
    }
}
EOF

# lxpanel reads the active profile config too; provide a minimal one so it does not warn.
cat > "$DEST/etc/xdg/lxpanel/default/config" <<'EOF'
[Command]
FileManager=
Terminal=
EOF

echo "prepared lxpanel config tree at $DEST"
