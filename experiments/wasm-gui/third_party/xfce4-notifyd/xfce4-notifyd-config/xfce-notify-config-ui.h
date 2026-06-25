#ifndef __RESOURCE_xfce_notify_config_ui_H__
#define __RESOURCE_xfce_notify_config_ui_H__

#include <gio/gio.h>

extern GResource *xfce_notify_config_ui_get_resource (void);

extern void xfce_notify_config_ui_register_resource (void);
extern void xfce_notify_config_ui_unregister_resource (void);

#endif
