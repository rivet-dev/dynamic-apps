/* XU4/XU5/XU6 file-view gate fix (toolchain/link layer, constraint #5 -- a --wrap shim, not a source
 * patch, like gmodule-shim.c / the writev wrap).
 *
 * In the sandbox there is NO dlopen, so no gvfs daemon backend modules can ever load -- the default GVfs
 * is ALWAYS the built-in local vfs. But g_vfs_get_default() routes through _g_io_module_get_default ->
 * _g_io_modules_ensure_loaded(), whose built-in GIO type-registration traps (RuntimeError: unreachable)
 * in wasm, blocking the entire GFile object path (g_file_new_for_path/query_info -> xfdesktop icons,
 * Thunar folders, file dialogs). g_vfs_get_local() and g_vfs_get_file_for_path() both WORK, so the local
 * vfs itself is fine. Wrap g_vfs_get_default to return the local vfs directly -- the correct value for a
 * module-less sandbox -- bypassing the trapping module-init machinery. Link with -Wl,--wrap=g_vfs_get_default. */

extern void *g_vfs_get_local(void);

void *__wrap_g_vfs_get_default(void) {
  return g_vfs_get_local();
}
