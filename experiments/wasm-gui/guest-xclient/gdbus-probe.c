/*
 * XU1 linchpin probe: prove GIO's GDBus client can reach the wasm dbus-daemon over the host_net
 * AF_UNIX socket table. xfconf/xfsettingsd talk to the session bus through GDBus (not libdbus), so
 * this is the integration gate before building the whole xfconf stack. Connects to the session bus
 * (DBUS_SESSION_BUS_ADDRESS), then calls org.freedesktop.DBus.ListNames and prints the result.
 *
 * Build: linked against the threaded GLib/GIO stack + dbus_creds (host_net recvmsg/sendmsg/read/
 * getsockopt(SO_PEERCRED)) — the same platform shims the dbus binaries use (constraint #5).
 */
#include <gio/gio.h>
#include <stdio.h>

int main(void) {
    GError *err = NULL;

    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (!conn) {
        fprintf(stderr, "GDBUS-PROBE: g_bus_get_sync FAILED: %s\n", err ? err->message : "(null)");
        return 1;
    }
    fprintf(stderr, "GDBUS-PROBE: connected, unique name = %s\n",
            g_dbus_connection_get_unique_name(conn));

    GVariant *res = g_dbus_connection_call_sync(
        conn, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "ListNames", NULL, G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, 8000, NULL, &err);
    if (!res) {
        fprintf(stderr, "GDBUS-PROBE: ListNames FAILED: %s\n", err ? err->message : "(null)");
        return 2;
    }

    GVariantIter *it = NULL;
    const char *name = NULL;
    int n = 0;
    g_variant_get(res, "(as)", &it);
    while (g_variant_iter_loop(it, "&s", &name)) {
        fprintf(stderr, "GDBUS-PROBE: name[%d] = %s\n", n++, name);
    }
    g_variant_iter_free(it);
    g_variant_unref(res);

    fprintf(stderr, "GDBUS-PROBE: PASS ListNames returned %d names\n", n);
    fflush(stderr);
    return 0;
}
