#ifndef __RESOURCE_notification_plugin_settings_ui_H__
#define __RESOURCE_notification_plugin_settings_ui_H__

#include <gio/gio.h>

extern GResource *notification_plugin_settings_ui_get_resource (void);

extern void notification_plugin_settings_ui_register_resource (void);
extern void notification_plugin_settings_ui_unregister_resource (void);

#endif
