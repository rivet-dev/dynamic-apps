/* XU3 diagnostic: reproduce the applicationsmenu deadlock in isolation. The panel's applicationsmenu
 * plugin builds its menu via garcon_menu_new_applications() + garcon_menu_load(); the populated menu
 * deadlocks the panel mid-construct (every thread parks on a futex). This standalone probe does just the
 * garcon load so we can stackdump a SMALL, name-section-bearing binary and find the exact wait. */
#include <garcon/garcon.h>
#include <stdio.h>

int main(void) {
  fprintf(stderr, "GARCONPROBE: new_applications\n"); fflush(stderr);
  GarconMenu *menu = garcon_menu_new_applications();
  if (!menu) { fprintf(stderr, "GARCONPROBE: no menu\n"); return 1; }
  fprintf(stderr, "GARCONPROBE: load (suspect deadlock here)\n"); fflush(stderr);
  GError *err = NULL;
  gboolean ok = garcon_menu_load(menu, NULL, &err);
  fprintf(stderr, "GARCONPROBE: loaded ok=%d err=%s\n", ok, err ? err->message : "(none)"); fflush(stderr);
  if (ok) {
    GList *elems = garcon_menu_get_elements(menu);
    fprintf(stderr, "GARCONPROBE: top-level elements=%d\n", g_list_length(elems));
  }
  fprintf(stderr, "GARCONPROBE: done (no deadlock)\n"); fflush(stderr);
  return 0;
}
