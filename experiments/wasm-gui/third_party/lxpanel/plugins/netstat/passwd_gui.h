#ifndef HAVE_NS_PASSWDGUI_H
#define HAVE_NS_PASSWDGUI_H

#include "netstat.h"

struct pgui *passwd_gui_new(ap_setting *aps);
void passwd_gui_set_style(struct pgui *pg, GtkStyle *style);
void passwd_gui_destroy(struct pgui *pg);
#endif
