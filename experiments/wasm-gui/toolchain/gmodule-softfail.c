/* Generic GModule soft-fail shim (toolchain/libc layer, constraint #5). wasm has no dlopen, so the real
 * g_module_open traps. GIO's _g_io_modules_ensure_loaded() (reached by g_vfs_get_default, the GSettings
 * backend, the volume monitor, etc.) scans for loadable modules via g_module_open; if that traps, the
 * whole GFile object path is blocked (g_file_new_for_path -> g_vfs_get_default -> ensure_loaded -> trap),
 * which is THE file-view gate blocking xfdesktop icons / Thunar folders / file dialogs.
 *
 * This shim makes g_module_open return NULL gracefully (exactly what xfce4-panel's gmodule-shim.c does for
 * non-plugin modules -- proven to let the scan complete + the built-in types, incl. the local vfs, register).
 * Unlike gmodule-shim.c there is NO panel-plugin table: pure soft-fail, for components that have no static
 * plugins of their own (xfdesktop, Thunar, the probe). Link with -Wl,--wrap=g_module_open,... . The panel
 * keeps its own table-bearing shim. */
#include <stdlib.h>
typedef int gboolean;

void *__wrap_g_module_open(const char *file_name, int flags) { (void) file_name; (void) flags; return 0; }
void *__wrap_g_module_open_full(const char *file_name, int flags, void *error) {
  (void) file_name; (void) flags; (void) error; return 0;
}
gboolean __wrap_g_module_symbol(void *handle, const char *symbol_name, void **symbol) {
  (void) handle; (void) symbol_name; if (symbol) *symbol = 0; return 0;
}
gboolean __wrap_g_module_close(void *handle) { (void) handle; return 1; }
void __wrap_g_module_make_resident(void *handle) { (void) handle; }
const char *__wrap_g_module_error(void) { return "gmodule-softfail: no dlopen in wasm"; }
gboolean __wrap_g_module_supported(void) { return 1; }
