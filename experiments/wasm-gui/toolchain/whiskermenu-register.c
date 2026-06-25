/* XU3: registration shim for whiskermenu (a platform-layer adapter, NOT a patch to whiskermenu's
 * source). whiskermenu's plugin.cpp exports a RAW construct (whiskermenu_construct) and relies on the
 * panel's normal .so loading to wrap it; our static-plugin path doesn't get that wrapper. Mapping the
 * raw construct directly into the gmodule table calls it too early -- before the panel sets the
 * XfcePanelPlugin's name/unique-id -> xfce_panel_plugin_lookup_rc_file -> relative_filename reads a
 * garbage name -> memory-out-of-bounds.
 *
 * XFCE_PANEL_PLUGIN_REGISTER() generates the proper xfce_panel_module_construct, which DEFERS
 * whiskermenu_construct to a "realize" signal handler -- run AFTER the panel has set the plugin's
 * name/unique-id. That is exactly the wrapper whiskermenu's own .so would have. build-whiskermenu.sh
 * compiles this with -Dxfce_panel_module_construct=whiskermenu_module_entry so the gmodule static-plugin
 * table can map (whiskermenu, xfce_panel_module_construct) -> whiskermenu_module_entry. */
#include <libxfce4panel/libxfce4panel.h>

extern void whiskermenu_construct(XfcePanelPlugin *plugin);

XFCE_PANEL_PLUGIN_REGISTER(whiskermenu_construct)
