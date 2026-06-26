/* Build-linked shim (NOT a component patch): the static no-dlopen wasm build does not run the dynamic-loader
   type registration a normal .so build does, so GtkBuilder's g_type_from_name("XfceTitledDialog") fails for
   dialogs (e.g. xfce4-keyboard-settings) that -- unlike appearance/mouse/display -- never call
   xfce_titled_dialog_get_type() themselves before loading their .ui.

   A pre-main __attribute__((constructor)) runs TOO EARLY (before GObject's type system is initialized ->
   g_type_register_static asserts static_quark_type_flags). The appearance/mouse/display dialogs register the
   type right AFTER gtk_init_with_args(), so we replicate exactly that timing by wrapping it (-Wl,--wrap):
   run the real init, then ensure the type. Keyboard SOURCE stays unmodified (constraint #5). */
typedef unsigned long GType;
extern GType xfce_titled_dialog_get_type(void);
extern int __real_gtk_init_with_args(int *argc, char ***argv, const char *param,
                                     const void *entries, const char *translation_domain, void *error);
int __wrap_gtk_init_with_args(int *argc, char ***argv, const char *param,
                              const void *entries, const char *translation_domain, void *error) {
    int r = __real_gtk_init_with_args(argc, argv, param, entries, translation_domain, error);
    (void) xfce_titled_dialog_get_type();
    return r;
}
