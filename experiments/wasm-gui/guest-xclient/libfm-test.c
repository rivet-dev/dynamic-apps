/* M8.3 headless test: drive libfm (the LXDE data layer) to list a directory of the kernel VFS, proving
 * the cross-compiled libfm + glib/gio stack runs end to end in wasm. No X / no GUI: libfm's FmFolder
 * loads asynchronously on the GLib main loop, so we run a GMainLoop until "finish-loading", then print
 * the entries. Diagnostics to stderr (the host captures it). Usage: libfm-test [dir] (default /usr/share). */
#include <libfm/fm.h>
#include <glib.h>

static GMainLoop *loop;
static int rc = 1;

static void on_finish(FmFolder *folder, gpointer data) {
    (void) data;
    FmFileInfoList *files = fm_folder_get_files(folder);
    guint n = files ? fm_file_info_list_get_length(files) : 0;
    g_printerr("LIBFM-TEST: folder loaded, %u entries:\n", n);
    GList *l = files ? fm_file_info_list_peek_head_link(files) : NULL;
    int i = 0;
    for (; l && i < 25; l = l->next, ++i) {
        FmFileInfo *fi = (FmFileInfo *) l->data;
        g_printerr("  - %s\n", fm_file_info_get_name(fi));
    }
    if (n > 0) { g_printerr("LIBFM-TEST: PASS (listed %u entries via libfm)\n", n); rc = 0; }
    else        g_printerr("LIBFM-TEST: FAIL (folder loaded but empty)\n");
    g_main_loop_quit(loop);
}

static gboolean timeout_fail(gpointer d) {
    (void) d;
    g_printerr("LIBFM-TEST: FAIL (timeout — folder never finished loading)\n");
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    g_printerr("LIBFM-TEST: fm_init\n");
    fm_init(NULL);
    loop = g_main_loop_new(NULL, FALSE);
    const char *dir = argc > 1 ? argv[1] : "/";
    g_printerr("LIBFM-TEST: listing %s\n", dir);
    FmFolder *folder = fm_folder_from_path_name(dir);
    if (!folder) { g_printerr("LIBFM-TEST: FAIL (no folder)\n"); return 1; }
    if (fm_folder_is_loaded(folder))
        on_finish(folder, NULL);
    else
        g_signal_connect(folder, "finish-loading", G_CALLBACK(on_finish), NULL);
    g_timeout_add(20000, timeout_fail, NULL);
    g_main_loop_run(loop);
    return rc;
}
