/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <cairo-gobject.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "process-tree-model.h"
#include "process-tree-view.h"
#include "task-manager.h"
#include "settings.h"



/* Tree view columns */
enum
{
	COLUMN_COMMAND = 0,
	COLUMN_PID,
	COLUMN_PPID,
	COLUMN_STATE,
	COLUMN_VSZ,
	COLUMN_RSS,
	COLUMN_UID,
	COLUMN_CPU,
	COLUMN_PRIORITY,
	N_COLUMNS,
};

typedef struct _XtmProcessTreeViewClass XtmProcessTreeViewClass;
struct _XtmProcessTreeViewClass
{
	GtkTreeViewClass	parent_class;
};
struct _XtmProcessTreeView
{
	GtkTreeView		parent;
	/*<private>*/
	GtkListStore *		model;
	GtkTreeModel *		model_filter;
	gchar *			cmd_filter;
	GtkTreeModel *		model_tree;
	GtkTreeViewColumn *	sort_column;
	gushort			columns_positions[N_COLUMNS];
	XtmSettings *		settings;
	guint			owner_uid;
	gboolean		show_all_processes_cached;
};
G_DEFINE_TYPE (XtmProcessTreeView, xtm_process_tree_view, GTK_TYPE_TREE_VIEW)

static void		xtm_process_tree_view_finalize			(GObject *object);

static gboolean		treeview_clicked				(XtmProcessTreeView *treeview, GdkEventButton *event);
static gboolean		treeview_key_pressed				(XtmProcessTreeView *treeview, GdkEventKey *event);
static void		column_task_pack_cells				(XtmProcessTreeView *treeview, GtkTreeViewColumn *column);
static void		columns_changed					(XtmProcessTreeView *treeview);
static void		read_columns_positions				(XtmProcessTreeView *treeview);
static void		save_columns_positions				(XtmProcessTreeView *treeview);
static void		column_clicked					(GtkTreeViewColumn *column, XtmProcessTreeView *treeview);
static gboolean		visible_func					(GtkTreeModel *model, GtkTreeIter *iter, XtmProcessTreeView *treeview);
static gboolean		search_func					(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer user_data);
static void		settings_changed				(GObject *object, GParamSpec *pspec, XtmProcessTreeView *treeview);
static void		expand_row					(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, XtmProcessTreeView *treeview);
static gboolean		treeview_query_tooltip		(GtkWidget *widget, gint x, gint y, gboolean keyboard_tip, GtkTooltip *tooltip, gpointer data);

static void
xtm_process_tree_view_class_init (XtmProcessTreeViewClass *klass)
{
	GObjectClass *class = G_OBJECT_CLASS (klass);
	xtm_process_tree_view_parent_class = g_type_class_peek_parent (klass);
	class->finalize = xtm_process_tree_view_finalize;
}

static void
xtm_process_tree_view_init (XtmProcessTreeView *treeview)
{
	GtkCellRenderer *cell_text, *cell_right_aligned;
	GtkTreeViewColumn *column;
	gboolean visible;
	gboolean tree;

	treeview->settings = xtm_settings_get_default ();
	treeview->owner_uid = getuid ();
	g_signal_connect (treeview->settings, "notify", G_CALLBACK (settings_changed), treeview);
	g_object_get (treeview->settings, "show-all-processes", &treeview->show_all_processes_cached, "process-tree", &tree, NULL);

	/* Create tree view model */
#ifdef HAVE_WNCK
	treeview->model = gtk_list_store_new (XTM_PTV_N_COLUMNS, CAIRO_GOBJECT_TYPE_SURFACE,
#else
	treeview->model = gtk_list_store_new (XTM_PTV_N_COLUMNS,
#endif
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT64,
		G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_FLOAT, G_TYPE_STRING, G_TYPE_INT,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_LONG);

	treeview->model_filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (treeview->model), NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (treeview->model_filter), (GtkTreeModelFilterVisibleFunc)visible_func, treeview, NULL);

	treeview->model_tree = xtm_process_tree_model_new (GTK_TREE_MODEL (treeview->model_filter));

	g_object_set (treeview, "search-column", XTM_PTV_COLUMN_COMMAND, "model", tree ? treeview->model_tree : treeview->model_filter, NULL);
	if (tree)
	{
		gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));
		g_signal_connect (treeview->model_tree, "row-has-child-toggled", G_CALLBACK (expand_row), treeview);
	}

	treeview->cmd_filter = NULL;

	/* Create cell renderer for tree view columns */
	cell_text = gtk_cell_renderer_text_new();

	cell_right_aligned = gtk_cell_renderer_text_new ();
	g_object_set (cell_right_aligned, "xalign", 1.0, NULL);

	/* Retrieve initial tree view columns positions */
	read_columns_positions (treeview);

	/* Create tree view columns */
#define COLUMN_PROPERTIES "expand", TRUE, "clickable", TRUE, "reorderable", TRUE, "resizable", TRUE, "visible", TRUE
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (GTK_TREE_VIEW_COLUMN (column), _("Task"));
	column_task_pack_cells (treeview, column);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_COMMAND));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_COMMAND));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_COMMAND]);

#undef COLUMN_PROPERTIES
#define COLUMN_PROPERTIES "expand", FALSE, "clickable", TRUE, "reorderable", TRUE, "resizable", TRUE, "visible", visible
	g_object_get (treeview->settings, "column-pid", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("PID"), cell_right_aligned, "text", XTM_PTV_COLUMN_PID, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_PID));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_PID));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_PID]);

	g_object_get (treeview->settings, "column-ppid", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("PPID"), cell_right_aligned, "text", XTM_PTV_COLUMN_PPID, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_PPID));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_PPID));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_PPID]);

	g_object_get (treeview->settings, "column-state", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("State"), cell_text, "text", XTM_PTV_COLUMN_STATE, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	gtk_tree_view_column_set_min_width (GTK_TREE_VIEW_COLUMN (column), 32);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_STATE));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_STATE));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_STATE]);

	g_object_get (treeview->settings, "column-vsz", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("VSZ"), cell_right_aligned, "text", XTM_PTV_COLUMN_VSZ_STR, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_VSZ));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_VSZ));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_VSZ]);

	g_object_get (treeview->settings, "column-rss", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("RSS"), cell_right_aligned, "text", XTM_PTV_COLUMN_RSS_STR, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_RSS));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_RSS));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_RSS]);

	g_object_get (treeview->settings, "column-uid", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("UID"), cell_text, "text", XTM_PTV_COLUMN_UID_STR, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_UID));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_UID));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_UID]);

	g_object_get (treeview->settings, "column-cpu", &visible, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("CPU"), cell_right_aligned, "text", XTM_PTV_COLUMN_CPU_STR, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_CPU));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_CPU));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_CPU]);

	g_object_get (treeview->settings, "column-priority", &visible, NULL);
	/* TRANSLATORS: “Prio.” is short for Priority, it appears in the tree view header. */
	column = gtk_tree_view_column_new_with_attributes (_("Prio."), cell_right_aligned, "text", XTM_PTV_COLUMN_PRIORITY, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
	g_object_set (column, COLUMN_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (column), "sort-column-id", GINT_TO_POINTER (XTM_PTV_COLUMN_PRIORITY));
	g_object_set_data (G_OBJECT (column), "column-id", GINT_TO_POINTER (COLUMN_PRIORITY));
	g_signal_connect (column, "clicked", G_CALLBACK (column_clicked), treeview);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, treeview->columns_positions[COLUMN_PRIORITY]);

	/* Set initial sort column */
	{
		guint sort_column_idu;
		gint sort_column_id;
		GtkSortType sort_type;

		g_object_get (treeview->settings, "sort-column-id", &sort_column_idu, "sort-type", &sort_type, NULL);
		treeview->sort_column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), treeview->columns_positions[sort_column_idu]);
		gtk_tree_view_column_set_sort_indicator (treeview->sort_column, TRUE);
		gtk_tree_view_column_set_sort_order (treeview->sort_column, sort_type);
		sort_column_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (treeview->sort_column), "sort-column-id"));
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (treeview->model), sort_column_id, sort_type);
	}

	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (treeview), (GtkTreeViewSearchEqualFunc)search_func, NULL, NULL);
	g_signal_connect (treeview, "columns-changed", G_CALLBACK (columns_changed), NULL);
	g_signal_connect (treeview, "button-press-event", G_CALLBACK (treeview_clicked), NULL);
	g_signal_connect (treeview, "key-press-event", G_CALLBACK (treeview_key_pressed), NULL);
	g_signal_connect (treeview, "query-tooltip", G_CALLBACK (treeview_query_tooltip), NULL);
	gtk_widget_set_has_tooltip (GTK_WIDGET (treeview), TRUE);
}

static void
xtm_process_tree_view_finalize (GObject *object)
{
	XtmProcessTreeView *treeview = XTM_PROCESS_TREE_VIEW (object);

	if (GTK_IS_TREE_MODEL (treeview->model))
	{
		g_object_unref (treeview->model);
		treeview->model = NULL;
	}

	if (GTK_IS_TREE_MODEL (treeview->model_filter))
	{
		g_object_unref (treeview->model_filter);
		treeview->model_filter = NULL;
	}

	if (GTK_IS_TREE_MODEL (treeview->model_tree))
	{
		g_object_unref (treeview->model_tree);
		treeview->model_tree = NULL;
	}

	if (XTM_IS_SETTINGS (treeview->settings))
	{
		g_object_unref (treeview->settings);
	}

	G_OBJECT_CLASS (xtm_process_tree_view_parent_class)->finalize (object);
}

/**
 * Helper functions
 */

static void
column_task_pack_cells (XtmProcessTreeView *treeview, GtkTreeViewColumn *column)
{
	GtkCellRenderer *cell_cmdline;
	gboolean show_application_icons;

	g_object_get (treeview->settings, "show-application-icons", &show_application_icons, NULL);
	if (show_application_icons)
	{
#ifdef HAVE_WNCK
		GtkCellRenderer *cell_icon = gtk_cell_renderer_pixbuf_new ();
		gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), cell_icon, FALSE);
		gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), cell_icon, "surface", XTM_PTV_COLUMN_ICON, "cell-background", XTM_PTV_COLUMN_BACKGROUND, NULL);
#endif
	}

	cell_cmdline = gtk_cell_renderer_text_new ();
	g_object_set (cell_cmdline, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), cell_cmdline, TRUE);
	gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), cell_cmdline, "text", XTM_PTV_COLUMN_COMMAND, "cell-background", XTM_PTV_COLUMN_BACKGROUND, "foreground", XTM_PTV_COLUMN_FOREGROUND, NULL);
}

static void
columns_changed (XtmProcessTreeView *treeview)
{
	GList *columns, *l;
	GtkTreeViewColumn *column;
	gint column_id;
	gushort i;

	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (treeview));
	if (g_list_length (columns) < N_COLUMNS)
	{
		g_list_free (columns);
		return;
	}

	for (l = columns, i = 0; l != NULL; l = l->next, i++)
	{
		column = GTK_TREE_VIEW_COLUMN (l->data);
		column_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (column), "column-id"));
		treeview->columns_positions[column_id] = i;
	}

	g_list_free (columns);
	save_columns_positions (treeview);
}

static void
read_columns_positions (XtmProcessTreeView *treeview)
{
	gushort i;
	gchar *columns_positions;
	gchar **columns_positions_split;

	g_object_get (treeview->settings, "columns-positions", &columns_positions, NULL);

	if (columns_positions == NULL)
	{
		for (i = 0; i < N_COLUMNS; i++)
			treeview->columns_positions[i] = i;
	}
	else
	{
		columns_positions_split = g_strsplit (columns_positions, ";", N_COLUMNS + 1);
		for (i = 0; i < N_COLUMNS && columns_positions_split[i] != NULL; i++)
			treeview->columns_positions[i] = (gushort)g_ascii_strtoll (columns_positions_split[i], NULL, 10);
		g_strfreev (columns_positions_split);
		g_free (columns_positions);
	}
}

static void
save_columns_positions (XtmProcessTreeView *treeview)
{
	gint i;
	gulong offset = 0;
#define COLUMNS_POSITIONS_STRLEN (N_COLUMNS * 4 + 1)
	gchar columns_positions[COLUMNS_POSITIONS_STRLEN] = { 0 };

	for (i = 0; i < N_COLUMNS; i++)
		offset += (gulong)g_snprintf (&columns_positions[offset], (sizeof(columns_positions) - offset), "%d;", treeview->columns_positions[i]);

	g_object_set (treeview->settings, "columns-positions", columns_positions, NULL);
}

static void
cb_send_signal (GtkMenuItem *mi, gpointer user_data)
{
	XtmSettings *settings;
	gboolean prompt_terminate_task;
	GtkWidget *dialog;
	GPid pid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (mi), "pid"));
	gint xtm_signal = GPOINTER_TO_INT (user_data);

	settings = xtm_settings_get_default ();
	g_object_get (settings, "prompt-terminate-task", &prompt_terminate_task, NULL);
	g_object_unref (settings);

	if ((xtm_signal == XTM_SIGNAL_TERMINATE && prompt_terminate_task) || xtm_signal == XTM_SIGNAL_KILL)
	{
		gint res;
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
			(xtm_signal == XTM_SIGNAL_TERMINATE) ? _("Terminate task") : _("Kill task"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
			_("Are you sure you want to send the %s signal to the PID %d?"),
			(xtm_signal == XTM_SIGNAL_TERMINATE) ? _("terminate") : _("kill"), pid);
		gtk_window_set_title (GTK_WINDOW (dialog), _("Task Manager"));
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
		res = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		if (res != GTK_RESPONSE_YES)
			return;
	}

	if (!send_signal_to_pid (pid, xtm_signal))
	{
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			_("Error sending signal"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
			_("An error was encountered by sending a signal to the PID %d. "
			"It is likely you don't have the required privileges."), pid);
		gtk_window_set_title (GTK_WINDOW (dialog), _("Task Manager"));
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	if (xtm_signal == XTM_SIGNAL_TERMINATE || xtm_signal == XTM_SIGNAL_KILL)
	{
		GtkTreeSelection *selection;
		GtkWidget *treeview;
		treeview = g_object_get_data (G_OBJECT (mi), "treeview");
		if (NULL == treeview)
			return;
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
		gtk_tree_selection_unselect_all (selection);
	}
}

static void
cb_set_priority (GtkMenuItem *mi, gpointer user_data)
{
	GPid pid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (mi), "pid"));
	gint priority = GPOINTER_TO_INT (user_data);

	if (!set_priority_to_pid (pid, priority))
	{
		GtkWidget *dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			_("Error setting priority"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("An error was encountered by setting a priority to the PID %d. "
			"It is likely you don't have the required privileges."), pid);
		gtk_window_set_title (GTK_WINDOW (dialog), _("Task Manager"));
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

/**
 * Process the "Copy command line" context menu item.
 *
 * The clipboard is only modified if finding the process was successful,
 * i.e. if the process died while the user had the context menu open and then
 * chose to copy the command line afterwards, nothing happens.
 */
static void
cb_copy_command_line (GtkMenuItem *mi, gpointer user_data)
{
	XtmProcessTreeView *view = XTM_PROCESS_TREE_VIEW (user_data);
	GPid pid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (mi), "pid"));
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	gboolean keep_looking = TRUE;
	GtkTreeIter iter;
	GtkClipboard *clipboard;

	if (!gtk_tree_model_get_iter_first (model, &iter))
	{
		return;
	}

	/* Look up the process in the model by the known PID */
	while (keep_looking)
	{
		GPid pid_iter;
		gtk_tree_model_get (model, &iter, XTM_PTV_COLUMN_PID, &pid_iter, -1);

		if (pid_iter == pid)
		{
			gchar *cmdline;
			gtk_tree_model_get (model, &iter, XTM_PTV_COLUMN_COMMAND_RAW, &cmdline, -1);

			clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
			gtk_clipboard_set_text (clipboard, cmdline, -1);

			keep_looking = FALSE;
		}
		else
		{
			keep_looking = gtk_tree_model_iter_next (model, &iter);
		}
	}
}

static GtkWidget *
build_context_menu (XtmProcessTreeView *treeview, GPid pid)
{
	GtkWidget *menu, *menu_priority, *mi, *accel_label;

	menu = gtk_menu_new ();

	if (!pid_is_sleeping (pid))
	{
		mi = gtk_menu_item_new_with_label (_("Stop"));
		g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
		gtk_container_add (GTK_CONTAINER (menu), mi);
		g_signal_connect (mi, "activate", G_CALLBACK (cb_send_signal), GINT_TO_POINTER (XTM_SIGNAL_STOP));
	}
	else
	{
		mi = gtk_menu_item_new_with_label (_("Continue"));
		g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
		gtk_container_add (GTK_CONTAINER (menu), mi);
		g_signal_connect (mi, "activate", G_CALLBACK (cb_send_signal), GINT_TO_POINTER (XTM_SIGNAL_CONTINUE));
	}

	mi = gtk_menu_item_new_with_label (_("Terminate"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	g_object_set_data (G_OBJECT (mi), "treeview", treeview);
	gtk_container_add (GTK_CONTAINER (menu), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_send_signal), GINT_TO_POINTER (XTM_SIGNAL_TERMINATE));

	mi = gtk_menu_item_new_with_label (_("Kill"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_send_signal), GINT_TO_POINTER (XTM_SIGNAL_KILL));

	menu_priority = gtk_menu_new ();

	mi = gtk_menu_item_new_with_label (_("Very low"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu_priority), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_set_priority), GINT_TO_POINTER (XTM_PRIORITY_VERY_LOW));

	mi = gtk_menu_item_new_with_label (_("Low"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu_priority), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_set_priority), GINT_TO_POINTER (XTM_PRIORITY_LOW));

	mi = gtk_menu_item_new_with_label (_("Normal"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu_priority), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_set_priority), GINT_TO_POINTER (XTM_PRIORITY_NORMAL));

	mi = gtk_menu_item_new_with_label (_("High"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu_priority), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_set_priority), GINT_TO_POINTER (XTM_PRIORITY_HIGH));

	mi = gtk_menu_item_new_with_label (_("Very high"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu_priority), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_set_priority), GINT_TO_POINTER (XTM_PRIORITY_VERY_HIGH));

	mi = gtk_menu_item_new_with_label (_("Priority"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu_priority);
	gtk_container_add (GTK_CONTAINER (menu), mi);

	mi = gtk_menu_item_new_with_label (_("Copy command line"));
	g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
	gtk_container_add (GTK_CONTAINER (menu), mi);
	g_signal_connect (mi, "activate", G_CALLBACK (cb_copy_command_line), treeview);
	/* Refer to treeview_key_pressed to see how the Ctrl-c press is handled */
	accel_label = gtk_bin_get_child (GTK_BIN (mi));
	gtk_accel_label_set_accel(GTK_ACCEL_LABEL (accel_label), GDK_KEY_c, GDK_CONTROL_MASK);

	gtk_widget_show_all (menu);

	return menu;
}

static GtkWidget *
create_popup_menu (XtmProcessTreeView *treeview, GPid pid, guint activate_time)
{
	static GtkWidget *menu = NULL;

	if (menu != NULL)
		gtk_widget_destroy (menu);

	menu = build_context_menu (treeview, pid);
	return menu;
}

static gboolean
treeview_clicked (XtmProcessTreeView *treeview, GdkEventButton *event)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean tree;
	GPid pid;

	if (event->button != 3)
		return FALSE;

	g_object_get (treeview->settings, "process-tree", &tree, NULL);
	model = GTK_TREE_MODEL (tree ? treeview->model_tree : treeview->model_filter);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL);
	if (path == NULL)
		return FALSE;
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, XTM_PTV_COLUMN_PID, &pid, -1);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	G_DEBUG_FMT ("Found iter with pid %d", pid);

	gtk_menu_popup_at_pointer (GTK_MENU (create_popup_menu (treeview, pid, event->time)), NULL);

	return TRUE;
}

static gboolean
treeview_key_pressed (XtmProcessTreeView *treeview, GdkEventKey *event)
{
	GPid pid;

	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GdkModifierType modifiers;

	modifiers = gtk_accelerator_get_default_mod_mask ();

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (model, &iter, XTM_PTV_COLUMN_PID, &pid, -1);

	if (event->keyval == GDK_KEY_Menu)
	{
		GdkRectangle rect = { .x = 5, .y = 5, .width = 0, .height = 0 };
		GdkWindow *window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (treeview));

		gtk_menu_popup_at_rect (GTK_MENU (create_popup_menu (treeview, pid, event->time)),
								window, &rect, GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
		return TRUE;
	}
	else if (event->keyval == GDK_KEY_Delete)
	{
		/* Fake menuitem for the cb_send_signal callback */
		GtkWidget *mi;
		mi = gtk_menu_item_new_with_label (_("Stop"));
		g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
		if ((event->state & modifiers) == GDK_SHIFT_MASK)
			cb_send_signal (GTK_MENU_ITEM (mi), GINT_TO_POINTER (XTM_SIGNAL_KILL));
		else
			cb_send_signal (GTK_MENU_ITEM (mi), GINT_TO_POINTER (XTM_SIGNAL_TERMINATE));
		return TRUE;
	}
	else if (event->keyval == GDK_KEY_c && (event->state & modifiers) == GDK_CONTROL_MASK)
	{
		/* Same trick as above */
		GtkWidget *mi = gtk_menu_item_new_with_label (_("Copy command line"));
		g_object_set_data (G_OBJECT (mi), "pid", GINT_TO_POINTER (pid));
		cb_copy_command_line (GTK_MENU_ITEM (mi), treeview);
		return TRUE;
	}

	else
		return FALSE;
}

static void
column_clicked (GtkTreeViewColumn *column, XtmProcessTreeView *treeview)
{
	gint sort_column_id;
	GtkSortType sort_type;

	gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (treeview->model), &sort_column_id, &sort_type);
	G_DEBUG_FMT ("Last sort column %d; sort type: %d", sort_column_id, sort_type);

	if (treeview->sort_column != column)
	{
		gtk_tree_view_column_set_sort_indicator (treeview->sort_column, FALSE);
		gtk_tree_view_column_set_sort_indicator (column, TRUE);
		sort_column_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (column), "sort-column-id"));
		sort_type = GTK_SORT_DESCENDING;
	}
	else
	{
		sort_type = (sort_type == GTK_SORT_ASCENDING) ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
	}

	G_DEBUG_FMT ("New sort column %d; sort type: %d", sort_column_id, sort_type);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (treeview->model), sort_column_id, sort_type);
	gtk_tree_view_column_set_sort_order (column, sort_type);

	treeview->sort_column = column;

	g_object_set(treeview->settings,
	    "sort-column-id", GPOINTER_TO_INT(g_object_get_data(G_OBJECT(treeview->sort_column), "column-id")),
	    "sort-type", sort_type,
	    NULL);
}

void
xtm_process_tree_view_set_filter(XtmProcessTreeView *treeview, const gchar *cmd_filter)
{
	g_free(treeview->cmd_filter);
	treeview->cmd_filter = g_strdup(cmd_filter);

	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (treeview->model_filter));
}

static gboolean
visible_func (GtkTreeModel *model, GtkTreeIter *iter, XtmProcessTreeView *treeview)
{
	gchar *cmdline, *cmdline_lower, *key_lower, *p = NULL;
	gboolean  mach_filter = TRUE, match_uid = TRUE;
	guint uid;

	if(treeview->cmd_filter) {
		gtk_tree_model_get (GTK_TREE_MODEL (model), iter, XTM_PTV_COLUMN_COMMAND, &cmdline, -1);
		if(cmdline) {
			cmdline_lower = g_ascii_strdown (cmdline, -1);
			key_lower = g_ascii_strdown (treeview->cmd_filter, -1);

			p = g_strrstr (cmdline_lower, key_lower);

			g_free (key_lower);
			g_free (cmdline_lower);
			g_free (cmdline);
		}

		if(!p)
			mach_filter = FALSE;
	}
	if (!treeview->show_all_processes_cached) {
		gtk_tree_model_get (GTK_TREE_MODEL (treeview->model), iter, XTM_PTV_COLUMN_UID, &uid, -1);
		if (treeview->owner_uid != uid)
			match_uid = FALSE;
	}

	return (mach_filter && match_uid);
}

static gboolean
search_func (GtkTreeModel *model, gint column __unused, const gchar *key, GtkTreeIter *iter, gpointer user_data __unused)
{
	gchar *cmdline, *cmdline_lower;
	gchar *key_lower;
	gchar *p;

	gtk_tree_model_get (GTK_TREE_MODEL (model), iter, XTM_PTV_COLUMN_COMMAND, &cmdline, -1);
	cmdline_lower = g_ascii_strdown (cmdline, -1);
	key_lower = g_ascii_strdown (key, -1);

	p = g_strrstr_len (cmdline_lower, -1, key_lower);

	g_free (key_lower);
	g_free (cmdline_lower);
	g_free (cmdline);

	return (p == NULL) ? TRUE : FALSE;
}

static void
settings_changed (GObject *object, GParamSpec *pspec, XtmProcessTreeView *treeview)
{
	if (g_str_has_prefix (pspec->name, "column-"))
	{
		gboolean visible;
		gushort column_id;

		column_id = COLUMN_UID;
		if (!g_strcmp0 (pspec->name, "column-pid"))
			column_id = COLUMN_PID;
		else if (!g_strcmp0 (pspec->name, "column-ppid"))
			column_id = COLUMN_PPID;
		else if (!g_strcmp0 (pspec->name, "column-state"))
			column_id = COLUMN_STATE;
		else if (!g_strcmp0 (pspec->name, "column-vsz"))
			column_id = COLUMN_VSZ;
		else if (!g_strcmp0 (pspec->name, "column-rss"))
			column_id = COLUMN_RSS;
		else if (!g_strcmp0 (pspec->name, "column-cpu"))
			column_id = COLUMN_CPU;
		else if (!g_strcmp0 (pspec->name, "column-priority"))
			column_id = COLUMN_PRIORITY;

		g_object_get (object, pspec->name, &visible, NULL);
		gtk_tree_view_column_set_visible (gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), treeview->columns_positions[column_id]), visible);
	}
	else if (!g_strcmp0 (pspec->name, "show-application-icons"))
	{
		GtkTreeViewColumn *column;
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), treeview->columns_positions[COLUMN_COMMAND]);
		gtk_tree_view_column_clear (column);
		column_task_pack_cells (treeview, column);
	}
	else if (!g_strcmp0 (pspec->name, "show-all-processes"))
	{
		g_object_get (object, pspec->name, &treeview->show_all_processes_cached, NULL);
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (treeview->model_filter));
	}
	else if (!g_strcmp0 (pspec->name, "process-tree"))
	{
		gboolean tree;
		g_object_get (object, pspec->name, &tree, NULL);
		gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), tree ? treeview->model_tree : treeview->model_filter);
		if (tree)
		{
			gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));
			g_signal_connect (treeview->model_tree, "row-has-child-toggled", G_CALLBACK (expand_row), treeview);
		}
		else
		{
			g_signal_handlers_disconnect_by_func (treeview->model_tree, (gpointer)G_CALLBACK (expand_row), treeview);
		}
	}
}


static void
expand_row (GtkTreeModel *model __unused, GtkTreePath *path, GtkTreeIter *iter __unused, XtmProcessTreeView *treeview)
{
	gtk_tree_view_expand_row (GTK_TREE_VIEW (treeview), path, FALSE);
}



GtkWidget *
xtm_process_tree_view_new (void)
{
	return g_object_new (XTM_TYPE_PROCESS_TREE_VIEW, NULL);
}

GtkTreeModel *
xtm_process_tree_view_get_model (XtmProcessTreeView *treeview)
{
	return GTK_TREE_MODEL (treeview->model);
}

void
xtm_process_tree_view_highlight_pid (XtmProcessTreeView *treeview, GPid pid) {
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter 	 iter;
	gboolean     	 valid;
	gboolean			 tree;
	GPid		pid_iter;
	GtkTreeIter	child_iter;
	gboolean validParent;

	g_object_get (treeview->settings, "process-tree", &tree, NULL);
	model = GTK_TREE_MODEL (tree ? treeview->model_tree : treeview->model_filter);

	g_return_if_fail (model != NULL);

	/* Get first row in list store */
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid)
	{
		gtk_tree_model_get (model, &iter, XTM_PTV_COLUMN_PID, &pid_iter, -1);
		if (pid == pid_iter)
		{
			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), path);

			/* set cursor before scrolling to unstick the view from top if needed */
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
			gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path, NULL, TRUE, 0.5, 0);
			gtk_tree_path_free (path);
			break;
		}

		if (gtk_tree_model_iter_has_child (model, &iter))
		{
			GtkTreeIter	parent_iter = iter;

			valid = gtk_tree_model_iter_children (model, &iter, &parent_iter);
		}
		else
		{
			child_iter = iter;
			valid = gtk_tree_model_iter_next (model, &iter);
			if (tree && !valid)
			{
				//finding my way up again
				do {
					validParent = gtk_tree_model_iter_parent (model, &iter, &child_iter);
					child_iter = iter;
					valid = gtk_tree_model_iter_next (model, &iter);
				} while (!valid && validParent);
			}
		}
	}
}

static gboolean
treeview_query_tooltip (GtkWidget *widget, gint x, gint y, gboolean keyboard_tip, GtkTooltip *tooltip, gpointer data)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

	if (gtk_tree_view_get_tooltip_context (tree_view, &x, &y, keyboard_tip, &model, &path, &iter))
	{
		gchar *tooltip_text;

		gtk_tree_model_get (model, &iter, XTM_PTV_COLUMN_COMMAND, &tooltip_text, -1);
		gtk_tooltip_set_text (tooltip, tooltip_text);
		gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

		gtk_tree_path_free (path);
		g_free (tooltip_text);

		return TRUE;
	}

	return FALSE;
}
