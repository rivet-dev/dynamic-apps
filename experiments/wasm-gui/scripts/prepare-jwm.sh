#!/usr/bin/env bash
# Stage JWM's config (~/.jwmrc) as a VM tree the host installs with --vm-tree. JWM (Joe's Window
# Manager) is the M7 desktop shell: a panel/taskbar with a window list + live clock + a root menu.
set -uo pipefail
OUT="${1:-/tmp/vmjwm}"
rm -rf "$OUT"; mkdir -p "$OUT/root"
cat > "$OUT/root/.jwmrc" <<'XML'
<?xml version="1.0"?>
<JWM>
  <RootMenu label="Menu" height="20">
    <Program label="xclock">/xclock.wasm</Program>
    <Restart label="Restart"/>
  </RootMenu>
  <Tray x="0" y="-1" height="26" autohide="off">
    <TaskList maxwidth="220"/>
    <Spacer width="10"/>
    <Clock format="%H:%M:%S">xclock</Clock>
  </Tray>
  <WindowStyle><Font>DejaVu Sans-12</Font></WindowStyle>
  <TrayStyle><Font>DejaVu Sans-12</Font></TrayStyle>
  <MenuStyle><Font>DejaVu Sans-12</Font></MenuStyle>
  <Group><Option>tiled</Option></Group>
  <FocusModel>click</FocusModel>
  <Desktops width="2" height="1"/>
  <Key key="Up">up</Key>
  <Key key="Down">down</Key>
</JWM>
XML
echo "staged JWM config into $OUT"
