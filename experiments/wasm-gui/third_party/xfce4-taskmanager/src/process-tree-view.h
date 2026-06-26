/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PROCESS_TREE_VIEW_H
#define PROCESS_TREE_VIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

enum
{
#ifdef HAVE_WNCK
	XTM_PTV_COLUMN_ICON,
#endif
	/* might be prettified; shown to user */
	XTM_PTV_COLUMN_COMMAND,
	/* never prettified; used for copying command line to clipboard */
	XTM_PTV_COLUMN_COMMAND_RAW,
	XTM_PTV_COLUMN_PID,
	XTM_PTV_COLUMN_PPID,
	XTM_PTV_COLUMN_STATE,
	XTM_PTV_COLUMN_VSZ,
	XTM_PTV_COLUMN_VSZ_STR,
	XTM_PTV_COLUMN_RSS,
	XTM_PTV_COLUMN_RSS_STR,
	XTM_PTV_COLUMN_UID,
	XTM_PTV_COLUMN_UID_STR,
	XTM_PTV_COLUMN_CPU,
	XTM_PTV_COLUMN_CPU_STR,
	XTM_PTV_COLUMN_PRIORITY,
	XTM_PTV_COLUMN_BACKGROUND,
	XTM_PTV_COLUMN_FOREGROUND,
	XTM_PTV_COLUMN_TIMESTAMP,
	XTM_PTV_N_COLUMNS,
};

#define XTM_TYPE_PROCESS_TREE_VIEW		(xtm_process_tree_view_get_type ())
#define XTM_PROCESS_TREE_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_PROCESS_TREE_VIEW, XtmProcessTreeView))
#define XTM_PROCESS_TREE_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_PROCESS_TREE_VIEW, XtmProcessTreeViewClass))
#define XTM_IS_PROCESS_TREE_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_PROCESS_TREE_VIEW))
#define XTM_IS_PROCESS_TREE_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_PROCESS_TREE_VIEW))
#define XTM_PROCESS_TREE_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_PROCESS_TREE_VIEW, XtmProcessTreeViewClass))

typedef struct _XtmProcessTreeView XtmProcessTreeView;

GType		xtm_process_tree_view_get_type			(void);
GtkWidget *	xtm_process_tree_view_new			(void);
void		xtm_process_tree_view_set_filter		(XtmProcessTreeView *treeview, const gchar *cmd_filter);
GtkTreeModel *	xtm_process_tree_view_get_model			(XtmProcessTreeView *treeview);
void		xtm_process_tree_view_highlight_pid		(XtmProcessTreeView *treeview, GPid pid);

#endif /* !PROCESS_TREE_VIEW_H */
