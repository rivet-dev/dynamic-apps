/* gmodule g_module_symbol shim (constraint #5: platform/toolchain, component untouched).
   The runtime's g_module_open(NULL)=main-module handle has a dlsym that does NOT resolve the wasm export table,
   so g_module_symbol(main, "thunar_extension_initialize") returns NULL even though the symbol is linked AND
   exported (objdump confirms it). Wrap g_module_symbol to return the statically-linked address directly for the
   thunar-sbr entry points; everything else passes through. Wired via -Wl,--wrap=g_module_symbol. */
#include <string.h>
extern void thunar_extension_initialize(void *plugin);
extern void thunar_extension_shutdown(void);
extern void thunar_extension_list_types(const void *types, int *n_types);
extern int __real_g_module_symbol(void *module, const char *symbol_name, void **symbol);

int __wrap_g_module_symbol(void *module, const char *symbol_name, void **symbol) {
    if (symbol_name != 0 && symbol != 0) {
        if (strcmp(symbol_name, "thunar_extension_initialize") == 0) { *symbol = (void *) &thunar_extension_initialize; return 1; }
        if (strcmp(symbol_name, "thunar_extension_shutdown")   == 0) { *symbol = (void *) &thunar_extension_shutdown;   return 1; }
        if (strcmp(symbol_name, "thunar_extension_list_types") == 0) { *symbol = (void *) &thunar_extension_list_types; return 1; }
    }
    return __real_g_module_symbol(module, symbol_name, symbol);
}
