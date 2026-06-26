/* Keep the statically-linked thunar-sbr plugin entry points past wasm-ld --gc-sections. They are only looked
   up at RUNTIME via g_module_symbol (no link-time reference), so the GC strips them -- which is why
   --whole-archive, -u, and direct-object linking all leave 0 thunar_extension_initialize in the binary. The
   correct fix: reference the symbols from a constructor (a GC root), which both pulls them from libthunar-sbr.a
   and keeps them. Constraint #5: platform/toolchain shim, thunar source untouched. Linked only into thunar. */
extern void thunar_extension_initialize(void *plugin);
extern void thunar_extension_shutdown(void);
extern void thunar_extension_list_types(const void *types, int *n_types);

void *volatile __secure_exec_thunar_sbr_keep[3];

__attribute__((constructor)) static void __keep_thunar_sbr(void) {
    __secure_exec_thunar_sbr_keep[0] = (void *) &thunar_extension_initialize;
    __secure_exec_thunar_sbr_keep[1] = (void *) &thunar_extension_shutdown;
    __secure_exec_thunar_sbr_keep[2] = (void *) &thunar_extension_list_types;
}
