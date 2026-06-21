/* M8 spike: a real GTK 3 application, cross-compiled to wasm32-wasip1 and run as a guest on the
 * secure-exec wasm X server. Creates a top-level window with a vertical box containing a label and a
 * button (a genuine GTK widget tree rendered through GDK/cairo/pango to the X server). Plain
 * gtk_init()+gtk_main() (no GApplication/D-Bus). Proves the full GTK stack runs end to end in wasm. */
#include <gtk/gtk.h>

static void on_click(GtkButton *b, gpointer user_data) {
    (void) user_data;
    gtk_button_set_label(b, "clicked!");
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "secure-exec GTK3 (wasm)");
    gtk_window_set_default_size(GTK_WINDOW(win), 360, 200);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *label = gtk_label_new("Hello from GTK 3 on wasm32-wasip1");
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

    GtkWidget *button = gtk_button_new_with_label("Click me");
    g_signal_connect(button, "clicked", G_CALLBACK(on_click), NULL);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    g_print("M8-GTK: widget tree built and shown; entering gtk_main()\n");
    gtk_main();
    return 0;
}
