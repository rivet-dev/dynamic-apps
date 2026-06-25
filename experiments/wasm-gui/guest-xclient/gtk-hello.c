/* M8 spike: a real GTK 3 application, cross-compiled to wasm32-wasip1[-threads] and run as a guest on
 * the secure-exec wasm X server. Creates a top-level window with a vertical box containing a label and
 * a button (a genuine GTK widget tree rendered through GDK/cairo/pango to the X server). Plain
 * gtk_init()+gtk_main() (no GApplication/D-Bus). Proves the full GTK stack runs end to end in wasm.
 * Diagnostics go to stderr (g_printerr) because the host captures the client's stderr. */
#include <gtk/gtk.h>

static void on_click(GtkButton *b, gpointer user_data) {
    (void) user_data;
    gtk_button_set_label(b, "clicked!");
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer data) {
    (void) w; (void) cr; (void) data;
    g_printerr("M8-GTK: draw signal fired (cairo painting to the X window)\n");
    return FALSE; /* let GTK continue its own drawing */
}

static gboolean quit_timer(gpointer data) {
    (void) data;
    g_printerr("M8-GTK: quit timer -> gtk_main_quit\n");
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

/* Force a periodic repaint so the content survives a window-manager reparent/resize even if the
 * post-reparent Expose is dropped (under openbox the client area was going black). */
static gboolean redraw_timer(gpointer data) {
    if (GTK_IS_WIDGET(data)) gtk_widget_queue_draw(GTK_WIDGET(data));
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv) {
    g_printerr("M8-GTK: before gtk_init\n");
    gtk_init(&argc, &argv);
    g_printerr("M8-GTK: after gtk_init (display connected)\n");

    /* XU1 acceptance: report the settings GTK resolved from the X XSETTINGS manager selection that
     * xfsettingsd publishes (Net/ThemeName -> gtk-theme-name, etc). If xfsettingsd pushed the xfconf
     * xsettings channel over X, these print "Greybird" / the channel values; with no manager they
     * stay at GTK's compiled defaults. This is the definitive end-to-end XSETTINGS-push assertion. */
    {
        GtkSettings *st = gtk_settings_get_default();
        gchar *theme = NULL, *icons = NULL, *font = NULL;
        gint dpi = -1;
        g_object_get(st, "gtk-theme-name", &theme, "gtk-icon-theme-name", &icons,
                     "gtk-font-name", &font, "gtk-xft-dpi", &dpi, NULL);
        g_printerr("XU1-XSETTINGS: gtk-theme-name=%s gtk-icon-theme-name=%s gtk-font-name=%s gtk-xft-dpi=%d\n",
                   theme ? theme : "(null)", icons ? icons : "(null)", font ? font : "(null)", dpi);
        g_free(theme); g_free(icons); g_free(font);
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "secure-exec GTK3 (wasm)");
    gtk_window_set_default_size(GTK_WINDOW(win), 360, 200);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "draw", G_CALLBACK(on_draw), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *label = gtk_label_new("Hello from GTK 3 on wasm32-wasip1");
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

    GtkWidget *button = gtk_button_new_with_label("Click me");
    g_signal_connect(button, "clicked", G_CALLBACK(on_click), NULL);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    g_printerr("M8-GTK: widget tree shown; entering gtk_main()\n");
    g_timeout_add(90000, quit_timer, NULL);
    g_timeout_add(500, redraw_timer, win);
    gtk_main();
    g_printerr("M8-GTK: gtk_main returned (clean exit)\n");
    return 0;
}
