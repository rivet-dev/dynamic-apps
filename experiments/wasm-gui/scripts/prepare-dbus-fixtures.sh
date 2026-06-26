#!/usr/bin/env bash
# Stage the D-Bus session-bus config fixture (XU0). Data/fixtures only — no component source.
# A minimal session bus: fixed listen path so clients have a known address; permissive policy (single
# trusted user in the sandbox); EXTERNAL + ANONYMOUS auth so a client connects even when peer-cred
# checking is limited.
set -euo pipefail
OUT="${1:-/tmp/vmdbus}"
rm -rf "$OUT"; mkdir -p "$OUT/etc/dbus-1" "$OUT/tmp/.dbus"
cat > "$OUT/etc/dbus-1/session.conf" <<'XML'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:path=/tmp/.dbus/session</listen>
  <auth>EXTERNAL</auth>
  <auth>ANONYMOUS</auth>
  <allow_anonymous/>
  <!-- The wasm session is far slower than native: under concurrent multi-guest load the single sidecar
       service thread starves each guest's D-Bus auth handshake well past the 30s default, so dbus-daemon
       drops the connection ("not authenticated soon enough") and the guest fails to init xfconf -> blank
       render. Raise the auth + pending-fd timeouts to accommodate the slow environment (config, not a
       dbus-daemon patch). -->
  <limit name="auth_timeout">600000</limit>
  <limit name="pending_fd_timeout">600000</limit>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
XML
echo "staged dbus session.conf at $OUT/etc/dbus-1/session.conf"
