/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * Based on ThunarPreferences:
 * Copyright (c) Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "settings.h"



enum
{
	PROP_SHOW_ALL_PROCESSES = 1,
	PROP_SHOW_LEGEND,
	PROP_MORE_PRECISION,
	PROP_FULL_COMMAND_LINE,
	PROP_SHOW_STATUS_ICON,
	PROP_SHOW_APPLICATION_ICONS,
	PROP_PROMPT_TERMINATE_TASK,
	PROP_REFRESH_RATE,
	PROP_COLUMNS_POSITIONS,
	PROP_COLUMN_UID,
	PROP_COLUMN_PID,
	PROP_COLUMN_PPID,
	PROP_COLUMN_STATE,
	PROP_COLUMN_VSZ,
	PROP_COLUMN_RSS,
	PROP_COLUMN_CPU,
	PROP_COLUMN_PRIORITY,
	PROP_SORT_COLUMN_ID,
	PROP_SORT_TYPE,
	PROP_HANDLE_POSITION,
	PROP_PROCESS_TREE,
	N_PROPS,
};
typedef struct _XtmSettingsClass XtmSettingsClass;
struct _XtmSettingsClass
{
	GObjectClass		parent_class;
};
struct _XtmSettings
{
	GObject			parent;
	/*<private>*/
	GValue			values[N_PROPS];
};
G_DEFINE_TYPE (XtmSettings, xtm_settings, G_TYPE_OBJECT)

static void	xtm_settings_get_property			(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void	xtm_settings_set_property			(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void	xtm_settings_finalize			(GObject *object);

static void
xtm_settings_class_init (XtmSettingsClass *klass)
{
	GObjectClass *class = G_OBJECT_CLASS (klass);
	xtm_settings_parent_class = g_type_class_peek_parent (klass);
	class->get_property = xtm_settings_get_property;
	class->set_property = xtm_settings_set_property;
	class->finalize = xtm_settings_finalize;
	g_object_class_install_property (class, PROP_SHOW_ALL_PROCESSES,
		g_param_spec_boolean ("show-all-processes", "ShowAllProcesses", "Show all processes", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_SHOW_LEGEND,
		g_param_spec_boolean ("show-legend", "ShowLegend", "Show legend", TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_MORE_PRECISION,
		g_param_spec_boolean ("more-precision", "MorePrecision", "More precision", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_FULL_COMMAND_LINE,
		g_param_spec_boolean ("full-command-line", "FullCommandLine", "Full command line", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_SHOW_STATUS_ICON,
		g_param_spec_boolean ("show-status-icon", "ShowStatusIcon", "Show/hide the status icon", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_SHOW_APPLICATION_ICONS,
		g_param_spec_boolean ("show-application-icons", "ShowApplicationIcons", "Show application icons", TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_PROMPT_TERMINATE_TASK,
		g_param_spec_boolean ("prompt-terminate-task", "PromptTerminateTask", "Prompt dialog for terminating a task", TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_REFRESH_RATE,
		g_param_spec_uint ("refresh-rate", "RefreshRate", "Refresh rate in milliseconds", 0, G_MAXUINT, 750, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMNS_POSITIONS,
		g_param_spec_string ("columns-positions", "ColumnsPositions", "Positions of the tree view columns", NULL, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_UID,
		g_param_spec_boolean ("column-uid", "ColumnUID", "Show column UID", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_PID,
		g_param_spec_boolean ("column-pid", "ColumnPID", "Show column PID", TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_PPID,
		g_param_spec_boolean ("column-ppid", "ColumnPPID", "Show column PPID", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_STATE,
		g_param_spec_boolean ("column-state", "ColumnState", "Show column state", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_VSZ,
		g_param_spec_boolean ("column-vsz", "ColumnVSZ", "Show column VSZ", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_RSS,
		g_param_spec_boolean ("column-rss", "ColumnRSS", "Show column RSS", TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_CPU,
		g_param_spec_boolean ("column-cpu", "ColumnCPU", "Show column CPU", TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_COLUMN_PRIORITY,
		g_param_spec_boolean ("column-priority", "ColumnPriority", "Show column priority", FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_SORT_COLUMN_ID,
		g_param_spec_uint ("sort-column-id", "SortColumn", "Sort by column id", 0, G_MAXUINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_SORT_TYPE,
		g_param_spec_uint ("sort-type", "SortType", "Sort type (asc/dsc)", 0, 1, 0, G_PARAM_READWRITE));
	g_object_class_install_property (class, PROP_PROCESS_TREE,
		g_param_spec_boolean ("process-tree", "ProcessTreeView", "Process tree", FALSE, G_PARAM_READWRITE));
}

static void
xtm_settings_init (XtmSettings *settings)
{

}

static void
xtm_settings_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GValue *src = XTM_SETTINGS (object)->values + property_id;
	if (G_IS_VALUE (src))
		g_value_copy (src, value);
	else
		g_param_value_set_default (pspec, value);
}

static void
xtm_settings_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GValue *dest = XTM_SETTINGS (object)->values + property_id;
	if (!G_IS_VALUE (dest))
	{
		g_value_init (dest, pspec->value_type);
		g_param_value_set_default (pspec, dest);
	}
	if (g_param_values_cmp (pspec, value, dest) != 0)
	{
		g_value_copy (value, dest);
	}
}

static void
xtm_settings_finalize (GObject *object)
{
	XtmSettings *settings = XTM_SETTINGS (object);

	for (gint i = 0; i < N_PROPS; i++)
	{
		if (G_IS_VALUE (settings->values + i))
			g_value_unset (settings->values + i);
	}

	G_OBJECT_CLASS (xtm_settings_parent_class)->finalize (object);
}

void
xtm_settings_bind_xfconf (XtmSettings *settings, XfconfChannel *channel)
{
	/* general settings */
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		xfconf_g_property_bind (channel, SETTING_SHOW_STATUS_ICON, G_TYPE_BOOLEAN,
			G_OBJECT (settings), "show-status-icon");
#endif
	xfconf_g_property_bind (channel, SETTING_PROMPT_TERMINATE_TASK, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "prompt-terminate-task");

	/* interface settings */
	xfconf_g_property_bind (channel, SETTING_SHOW_ALL_PROCESSES, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "show-all-processes");
	xfconf_g_property_bind (channel, SETTING_SHOW_APPLICATION_ICONS, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "show-application-icons");
	xfconf_g_property_bind (channel, SETTING_SHOW_LEGEND, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "show-legend");
	xfconf_g_property_bind (channel, SETTING_FULL_COMMAND_LINE, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "full-command-line");
	xfconf_g_property_bind (channel, SETTING_MORE_PRECISION, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "more-precision");
	xfconf_g_property_bind (channel, SETTING_PROCESS_TREE, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "process-tree");
	xfconf_g_property_bind (channel, SETTING_REFRESH_RATE, G_TYPE_UINT,
		G_OBJECT (settings), "refresh-rate");

	/* column visibility */
	xfconf_g_property_bind (channel, SETTING_COLUMN_PID, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-pid");
	xfconf_g_property_bind (channel, SETTING_COLUMN_PPID, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-ppid");
	xfconf_g_property_bind (channel, SETTING_COLUMN_STATE, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-state");
	xfconf_g_property_bind (channel, SETTING_COLUMN_VSZ, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-vsz");
	xfconf_g_property_bind (channel, SETTING_COLUMN_RSS, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-rss");
	xfconf_g_property_bind (channel, SETTING_COLUMN_UID, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-uid");
	xfconf_g_property_bind (channel, SETTING_COLUMN_CPU, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-cpu");
	xfconf_g_property_bind (channel, SETTING_COLUMN_PRIORITY, G_TYPE_BOOLEAN,
		G_OBJECT (settings), "column-priority");

	xfconf_g_property_bind (channel, SETTING_COLUMN_SORT_ID, G_TYPE_UINT,
		G_OBJECT (settings), "sort-column-id");
	xfconf_g_property_bind (channel, SETTING_COLUMN_SORT_TYPE, G_TYPE_UINT,
		G_OBJECT (settings), "sort-type");
	xfconf_g_property_bind (channel, SETTING_COLUMN_POSITIONS, G_TYPE_STRING,
		G_OBJECT (settings), "columns-positions");
}

XtmSettings *
xtm_settings_get_default (void)
{
	static XtmSettings *settings = NULL;
	if (settings == NULL)
	{
		settings = g_object_new (XTM_TYPE_SETTINGS, NULL);
		g_object_add_weak_pointer (G_OBJECT (settings), (gpointer)&settings);
	}
	else
	{
		g_object_ref (settings);
	}
	return settings;
}
