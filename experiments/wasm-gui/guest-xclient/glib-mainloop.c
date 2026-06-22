/* Does the GLib main loop BLOCK in poll (state S, correct) or BUSY-SPIN (state R, the worker bug)?
 * A 2s timeout quits the loop; while running, the thread must be parked in poll, not spinning. */
#include <glib.h>
#include <stdio.h>
static GMainLoop *loop;
static gboolean quit_cb(gpointer d){ (void)d; fprintf(stderr,"GML: timeout fired -> quit\n");fflush(stderr); g_main_loop_quit(loop); return FALSE; }
int main(void){
    fprintf(stderr,"GML: start\n");fflush(stderr);
    loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(8000, quit_cb, NULL);
    fprintf(stderr,"GML: run (should block ~2s in poll, NOT spin)\n");fflush(stderr);
    g_main_loop_run(loop);
    fprintf(stderr,"GML: loop returned -> done\n");fflush(stderr);
    return 0;
}
