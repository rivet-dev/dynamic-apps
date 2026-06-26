/* gmodule static-plugin shim (constraint #5: platform/toolchain, component source untouched).
   The static no-dlopen wasm build cannot g_module_open(".so") the thunarx / xfce4-panel plugins. Instead the
   plugin objects are statically linked into the host binary, and gmodule already returns the MAIN module's
   symbol table for g_module_open(NULL) (glib gmodule.c). So for a plugin-directory path we hand back the main
   module; the caller's g_module_symbol(handle, "thunar_extension_initialize") then resolves the statically
   linked init. Wired via -Wl,--wrap=g_module_open. Non-plugin paths pass through unchanged.

   Plugin-path markers (extend as more plugin families are wired):
     "thunarx-"        -> Thunar extensions (thunar-sbr renamers, ...)
     "xfce4/panel" / "panel-plugins" -> xfce4-panel plugins (future) */
#include <string.h>
typedef struct _GModule GModule;
extern GModule *__real_g_module_open(const char *file_name, int flags);

GModule *__wrap_g_module_open(const char *file_name, int flags) {
    if (file_name != 0 &&
        (strstr(file_name, "thunarx-") != 0 ||
         strstr(file_name, "xfce4/panel") != 0 ||
         strstr(file_name, "panel-plugins") != 0)) {
        return __real_g_module_open((const char *) 0, flags); /* main module = the static link */
    }
    return __real_g_module_open(file_name, flags);
}
