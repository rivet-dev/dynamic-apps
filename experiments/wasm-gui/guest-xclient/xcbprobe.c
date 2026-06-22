#include <stdio.h>
#include <xcb/xcb.h>
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(void){
    m("XP:start\n");
    int screen=0;
    xcb_connection_t *c = xcb_connect(":0", &screen);
    if (xcb_connection_has_error(c)) { m("XP:connect_err\n"); return 1; }
    m("XP:connected\n");
    /* a basic round-trip: get input focus (request + blocking reply) */
    xcb_get_input_focus_cookie_t ck = xcb_get_input_focus(c);
    m("XP:request_sent\n");
    xcb_generic_error_t *err = NULL;
    xcb_get_input_focus_reply_t *rep = xcb_get_input_focus_reply(c, ck, &err);
    if (rep) { fprintf(stderr,"XP:roundtrip_ok focus=%u\n", rep->focus); fflush(stderr); free(rep); }
    else { m("XP:roundtrip_FAILED\n"); }
    /* second round-trip: intern an atom */
    xcb_intern_atom_cookie_t ac = xcb_intern_atom(c, 0, 4, "WM_S0");
    xcb_intern_atom_reply_t *ar = xcb_intern_atom_reply(c, ac, NULL);
    if (ar) { fprintf(stderr,"XP:atom_ok=%u\n", ar->atom); fflush(stderr); free(ar); }
    else m("XP:atom_FAILED\n");
    m("XP:done\n");
    return 0;
}
