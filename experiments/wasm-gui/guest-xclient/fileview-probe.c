/* XU4/XU5/XU6 FILE-VIEW GATE minimal repro: g_file_new_for_path / g_file_get_path / g_file_query_info
 * trap (RuntimeError: unreachable) or hang under wasm-opt --fpcast-emu, while raw lstat + g_file_get_contents
 * (no GFile vtable) work. This is the single blocker for xfdesktop icons, Thunar folder views, and file
 * dialogs. Tiny binary to test wasm-opt config variations (drop -pa, skip -Oz, newer binaryen) for one
 * that dispatches the GFile/GVfs vtable correctly. Build: scripts/build-gtk-app.sh fileview-probe. */
#include <gio/gio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "/etc/machine-id";

  /* EMPTY-PATH HANG REPRO (XU5 Thunar block = gtk_image_new_from_file("") -> gdk_pixbuf opens ""). Each
   * call should return -1/ENOENT immediately; a missing "...done" line = the empty-path op HANGS. */
  errno = 0; g_printerr("FVEMPTY: lstat(\"\")...\n");
  struct stat est; int er = lstat("", &est);
  g_printerr("FVEMPTY: lstat(\"\")=%d errno=%d(%s)\n", er, errno, strerror(errno));
  errno = 0; g_printerr("FVEMPTY: stat(\"\")...\n");
  int sr = stat("", &est);
  g_printerr("FVEMPTY: stat(\"\")=%d errno=%d(%s)\n", sr, errno, strerror(errno));
  errno = 0; g_printerr("FVEMPTY: open(\"\", O_RDONLY)...\n");
  int efd = open("", O_RDONLY);
  g_printerr("FVEMPTY: open(\"\")=%d errno=%d(%s)\n", efd, errno, strerror(errno));
  if (efd >= 0) close(efd);
  errno = 0; g_printerr("FVEMPTY: fopen(\"\", rb)...\n");
  FILE *ef = fopen("", "rb");
  g_printerr("FVEMPTY: fopen(\"\")=%p errno=%d(%s) -- empty-path ops ALL RETURNED\n", (void*) ef, errno, strerror(errno));
  if (ef) fclose(ef);

  /* Baseline: raw lstat (no GFile vtable) is known to work. */
  struct stat st;
  int r = lstat(path, &st);
  g_printerr("FVPROBE: lstat(%s)=%d size=%lld (baseline, no vtable)\n", path, r, (long long) st.st_size);

  /* BISECT: g_vfs_get_local() is the DIRECT local-vfs path (_g_local_vfs_new, no _g_io_module_get_default
   * / module scan). g_vfs_get_default() goes through _g_io_module_get_default -> _g_io_modules_ensure_loaded
   * (module scan + built-in type registration). Test get_local FIRST to localize the trap. */
  g_printerr("FVPROBE: g_vfs_get_local (direct, no module scan)...\n");
  GVfs *lvfs = g_vfs_get_local();
  g_printerr("FVPROBE: local vfs=%p (get_local PASSED -- local vfs type registers fine)\n", (void*) lvfs);
  g_printerr("FVPROBE: g_vfs_get_file_for_path(local)...\n");
  GFile *lf = g_vfs_get_file_for_path(lvfs, path);
  g_printerr("FVPROBE: local get_file_for_path=%p (PASSED -- can make GFiles via local vfs!)\n", (void*) lf);

  g_printerr("FVPROBE: g_vfs_get_default (via _g_io_module_get_default + ensure_loaded)...\n");
  GVfs *vfs = g_vfs_get_default();
  g_printerr("FVPROBE: default vfs=%p active=%d (get_default PASSED)\n", (void*) vfs, vfs ? g_vfs_is_active(vfs) : -1);

  /* The gate: GFile creation goes through the GVfs vtable. */
  g_printerr("FVPROBE: g_file_new_for_path...\n");
  GFile *f = g_file_new_for_path(path);
  g_printerr("FVPROBE: GFile=%p (new_for_path PASSED)\n", (void*) f);

  g_printerr("FVPROBE: g_file_get_path...\n");
  char *p = g_file_get_path(f);
  g_printerr("FVPROBE: get_path=%s (PASSED)\n", p ? p : "(null)");
  g_free(p);

  /* The exact call xfdesktop/Thunar make per item. */
  GError *err = NULL;
  g_printerr("FVPROBE: g_file_query_info...\n");
  GFileInfo *info = g_file_query_info(f, "standard::*", G_FILE_QUERY_INFO_NONE, NULL, &err);
  g_printerr("FVPROBE: query_info=%p err=%s (PASSED)\n", (void*) info, err ? err->message : "(none)");
  if (err) g_clear_error(&err);
  if (info) g_object_unref(info);
  g_object_unref(f);

  g_printerr("FVPROBE: ★ ALL GFile OPS COMPLETED -- file-view gate is OPEN with this build config\n");
  return 0;
}
