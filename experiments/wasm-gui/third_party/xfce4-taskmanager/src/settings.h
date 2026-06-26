/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include <xfconf/xfconf.h>

#define DEFAULT_WINDOW_HEIGHT	600
#define DEFAULT_WINDOW_WIDTH	400

#define CHANNEL                          "xfce4-taskmanager"

/* general settings */
#define SETTING_SHOW_STATUS_ICON         "/show-status-icon"
#define SETTING_PROMPT_TERMINATE_TASK    "/prompt-terminate-task"
#define SETTING_WINDOW_MAXIMIZED         "/window-maximized"
#define SETTING_WINDOW_WIDTH             "/window-width"
#define SETTING_WINDOW_HEIGHT            "/window-height"

/* interface settings */
#define SETTING_SHOW_FILTER              "/interface/show-filter"
#define SETTING_HANDLE_POSITION          "/interface/handle-position"
#define SETTING_SHOW_LEGEND              "/interface/show-legend"

#define SETTING_SHOW_ALL_PROCESSES       "/interface/show-all-processes"
#define SETTING_SHOW_APPLICATION_ICONS   "/interface/show-application-icons"
#define SETTING_FULL_COMMAND_LINE        "/interface/full-command-line"
#define SETTING_MORE_PRECISION           "/interface/more-precision"
#define SETTING_PROCESS_TREE             "/interface/process-tree"
#define SETTING_REFRESH_RATE             "/interface/refresh-rate"

/* column visibility */
#define SETTING_COLUMN_PID               "/columns/column-pid"
#define SETTING_COLUMN_PPID              "/columns/column-ppid"
#define SETTING_COLUMN_STATE             "/columns/column-state"
#define SETTING_COLUMN_VSZ               "/columns/column-vsz"
#define SETTING_COLUMN_RSS               "/columns/column-rss"
#define SETTING_COLUMN_UID               "/columns/column-uid"
#define SETTING_COLUMN_CPU               "/columns/column-cpu"
#define SETTING_COLUMN_PRIORITY          "/columns/column-priority"
#define SETTING_COLUMN_SORT_ID           "/columns/sort-id"
#define SETTING_COLUMN_SORT_TYPE         "/columns/sort-type"
#define SETTING_COLUMN_POSITIONS         "/columns/positions"

#define XTM_TYPE_SETTINGS			(xtm_settings_get_type ())
#define XTM_SETTINGS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_SETTINGS, XtmSettings))
#define XTM_SETTINGS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_SETTINGS, XtmSettingsClass))
#define XTM_IS_SETTINGS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_SETTINGS))
#define XTM_IS_SETTINGS_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_SETTINGS))
#define XTM_SETTINGS_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_SETTINGS, XtmSettingsClass))

typedef struct _XtmSettings XtmSettings;

void		xtm_settings_bind_xfconf			(XtmSettings *settings, XfconfChannel *channel);
GType		xtm_settings_get_type				(void);
XtmSettings *	xtm_settings_get_default			(void);



#endif /* !SETTINGS_H */
