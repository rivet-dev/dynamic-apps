/*
 * Copyright (c) 2014 Peter de Ridder, <peter@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "task-manager.h"
#include "process-tree-view.h"
#include "process-tree-model.h"



enum {
	PROP_0,
	PROP_MODEL
};

typedef struct {
	GtkTreeIter	iter;
	GtkTreePath *	path;
	GSequenceIter *	list;
	GNode *		tree;
} XtmCrossLink;

typedef struct _XtmProcessTreeModelClass XtmProcessTreeModelClass;
struct _XtmProcessTreeModelClass
{
	GObjectClass	parent_class;
};
struct _XtmProcessTreeModel
{
	GObject		parent;
	/*<private>*/
	GtkTreeModel *	model;
	GNode *		tree;
	GSequence *	list;
	gint		c_column;
	gint		p_column;
	gint		stamp;
};

static void		xtm_process_tree_model_iface_init		(GtkTreeModelIface *iface);

G_DEFINE_TYPE_WITH_CODE (XtmProcessTreeModel, xtm_process_tree_model, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, xtm_process_tree_model_iface_init))

static void			xtm_process_tree_model_finalize		(GObject *object);
static void			xtm_process_tree_model_set_property	(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void			xtm_process_tree_model_get_property	(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

static GtkTreeModelFlags	xtm_process_tree_model_get_flags	(GtkTreeModel *model);
static gint			xtm_process_tree_model_get_n_columns	(GtkTreeModel *model);
static GType			xtm_process_tree_model_get_column_type	(GtkTreeModel *model, gint idx);
static gboolean			xtm_process_tree_model_get_iter		(GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath *		xtm_process_tree_model_get_path		(GtkTreeModel *model, GtkTreeIter *iter);
static void			xtm_process_tree_model_get_value	(GtkTreeModel *model, GtkTreeIter *iter, gint column, GValue *value);
static gboolean			xtm_process_tree_model_iter_next	(GtkTreeModel *model, GtkTreeIter *iter);
static gboolean			xtm_process_tree_model_iter_children	(GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean			xtm_process_tree_model_iter_has_child	(GtkTreeModel *model, GtkTreeIter *iter);
static gint			xtm_process_tree_model_iter_n_children	(GtkTreeModel *model, GtkTreeIter *iter);
static gboolean			xtm_process_tree_model_iter_nth_child	(GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent, gint n);
static gboolean			xtm_process_tree_model_iter_parent	(GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child);

static void			xtm_process_tree_model_row_changed	(XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeIter *iter, GtkTreeModel *model);
static void			xtm_process_tree_model_row_inserted	(XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeIter *iter, GtkTreeModel *model);
//static void			xtm_process_tree_model_row_has_child_toggled	(XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeIter *iter, GtkTreeModel *model);
static void			xtm_process_tree_model_row_deleted	(XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeModel *model);
static void			xtm_process_tree_model_rows_reordered	(XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeIter *iter, gint *new_order, GtkTreeModel *model);

static void			xtm_process_tree_model_set_model	(XtmProcessTreeModel *treemodel, GtkTreeModel *model);



static XtmCrossLink *
xtm_cross_link_new (void)
{
	return g_new0 (XtmCrossLink, 1);
}



static void
xtm_cross_link_free (gpointer data)
{
	XtmCrossLink *lnk = data;

	if (lnk != NULL && lnk->path != NULL)
		gtk_tree_path_free (lnk->path);

	g_free (lnk);
}



static void
xtm_process_tree_model_class_init (XtmProcessTreeModelClass *klass)
{
	GObjectClass *class = G_OBJECT_CLASS (klass);
	xtm_process_tree_model_parent_class = g_type_class_peek_parent (klass);
	class->finalize = xtm_process_tree_model_finalize;
	class->set_property = xtm_process_tree_model_set_property;
	class->get_property = xtm_process_tree_model_get_property;

	g_object_class_install_property (class, PROP_MODEL,
		g_param_spec_object ("model", NULL, NULL, GTK_TYPE_TREE_MODEL, G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
}

static void
xtm_process_tree_model_iface_init (GtkTreeModelIface *iface)
{
	iface->row_changed = NULL;
	iface->row_inserted = NULL;
	iface->row_has_child_toggled = NULL;
	iface->row_deleted = NULL;
	iface->rows_reordered = NULL;

	iface->get_flags = xtm_process_tree_model_get_flags;
	iface->get_n_columns = xtm_process_tree_model_get_n_columns;
	iface->get_column_type = xtm_process_tree_model_get_column_type;
	iface->get_iter = xtm_process_tree_model_get_iter;
	iface->get_path = xtm_process_tree_model_get_path;
	iface->get_value = xtm_process_tree_model_get_value;
	iface->iter_next = xtm_process_tree_model_iter_next;
	iface->iter_children = xtm_process_tree_model_iter_children;
	iface->iter_has_child = xtm_process_tree_model_iter_has_child;
	iface->iter_n_children = xtm_process_tree_model_iter_n_children;
	iface->iter_nth_child = xtm_process_tree_model_iter_nth_child;
	iface->iter_parent = xtm_process_tree_model_iter_parent;
	iface->ref_node = NULL;
	iface->unref_node = NULL;
}

static void
xtm_process_tree_model_init (XtmProcessTreeModel *treemodel)
{
	treemodel->list = g_sequence_new (xtm_cross_link_free);
	treemodel->tree = g_node_new (NULL);

	treemodel->c_column = XTM_PTV_COLUMN_PID;
	treemodel->p_column = XTM_PTV_COLUMN_PPID;

	treemodel->stamp = (gint)g_random_int ();
	if (treemodel->stamp == 0)
		treemodel->stamp = 1;
}

static void
xtm_process_tree_model_finalize (GObject *object)
{
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (object);

	g_node_destroy (treemodel->tree);
	g_sequence_free (treemodel->list);

	G_OBJECT_CLASS (xtm_process_tree_model_parent_class)->finalize (object);
}

static void
xtm_process_tree_model_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec __unused)
{
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (object);
	switch (property_id)
	{
		case PROP_MODEL:
			xtm_process_tree_model_set_model (treemodel, g_value_get_object (value));
			break;
	}
}

static void
xtm_process_tree_model_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec __unused)
{
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (object);
	switch (property_id)
	{
		case PROP_MODEL:
			g_value_set_object (value, treemodel->model);
			break;
	}
}

static GtkTreeModelFlags
xtm_process_tree_model_get_flags (GtkTreeModel *model __unused)
{
	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
xtm_process_tree_model_get_n_columns (GtkTreeModel *model)
{
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	model = treemodel->model;
	g_return_val_if_fail (model, 0);
	return gtk_tree_model_get_n_columns (model);
}

static GType
xtm_process_tree_model_get_column_type (GtkTreeModel *model, gint idx)
{
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	model = treemodel->model;
	g_return_val_if_fail (model, 0);
	return gtk_tree_model_get_column_type (model, idx);
}

static gboolean
xtm_process_tree_model_get_iter (GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path)
{
	GNode *node;
	gint *indices;
	gint depth, i;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	node = treemodel->tree;
	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);
	g_return_val_if_fail (depth > 0, FALSE);
	/* Walk the tree to create the iter */
	for (i = 0; i < depth; i++)
	{
		node = g_node_nth_child (node, (guint)indices[i]);
		if (node == NULL)
			break;
	}
	/* Make iter only valid if a node has been found */
	iter->stamp = node ? treemodel->stamp : 0;
	iter->user_data = node;
	return node != NULL;
}

static GtkTreePath *
xtm_process_tree_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	GNode *child, *parent;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);

	g_return_val_if_fail (iter->stamp == treemodel->stamp, NULL);

	child = iter->user_data;
	parent = child->parent;
	path = gtk_tree_path_new ();
	/* Walk up to build the path */
	while (parent)
	{
		gtk_tree_path_prepend_index (path, g_node_child_position (parent, child));
		child = parent;
		parent = child->parent;
	}
	return path;
}

static void
xtm_process_tree_model_get_value (GtkTreeModel *model, GtkTreeIter *iter, gint column, GValue *value)
{
	XtmCrossLink *lnk;
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	g_return_if_fail (iter->stamp == treemodel->stamp);
	model = treemodel->model;
	g_return_if_fail (model);
	node = iter->user_data;
	lnk = node->data;
	iter = &lnk->iter;
	/* Use path for non-persistent models */
	if (lnk->path)
		gtk_tree_model_get_iter (model, iter, lnk->path);
	gtk_tree_model_get_value (model, iter, column, value);
}

static gboolean
xtm_process_tree_model_iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);

	g_return_val_if_fail (iter->stamp == treemodel->stamp, FALSE);

	node = iter->user_data;
	iter->user_data = g_node_next_sibling (node);
	/* Make iter invalid if no node has been found */
	if (iter->user_data == NULL)
		iter->stamp = 0;
	return iter->user_data != NULL;
}

static gboolean
xtm_process_tree_model_iter_children (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	g_return_val_if_fail (parent == NULL || parent->stamp == treemodel->stamp, FALSE);
	if (parent == NULL)
		node = treemodel->tree;
	else
		node = parent->user_data;
	iter->user_data = g_node_first_child (node);
	/* Make iter only valid if a node has been found */
	iter->stamp = iter->user_data ? treemodel->stamp : 0;
	return iter->user_data != NULL;
}

static gboolean
xtm_process_tree_model_iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);

	g_return_val_if_fail (iter->stamp == treemodel->stamp, FALSE);

	node = iter->user_data;
	return g_node_first_child (node) != NULL;
}

static gint
xtm_process_tree_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	g_return_val_if_fail (iter == NULL || iter->stamp == treemodel->stamp, 0);
	if (iter == NULL)
		node = treemodel->tree;
	else
		node = iter->user_data;
	return (gint)g_node_n_children (node);
}

static gboolean
xtm_process_tree_model_iter_nth_child (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	g_return_val_if_fail (parent == NULL || parent->stamp == treemodel->stamp, FALSE);
	if (parent == NULL)
		node = treemodel->tree;
	else
		node = parent->user_data;
	iter->user_data = g_node_nth_child (node, (guint)n);
	/* Make iter only valid if a node has been found */
	iter->stamp = iter->user_data ? treemodel->stamp : 0;
	return iter->user_data != NULL;
}

static gboolean
xtm_process_tree_model_iter_parent (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child)
{
	GNode *node;
	XtmProcessTreeModel *treemodel = XTM_PROCESS_TREE_MODEL (model);
	g_return_val_if_fail (child->stamp == treemodel->stamp, FALSE);
	node = child->user_data;
	node = node->parent;
	/* root has no parent */
	if (node == treemodel->tree)
		node = NULL;
	/* Make iter only valid if a node has been found */
	iter->stamp = node ? treemodel->stamp : 0;
	iter->user_data = node;
	return node != NULL;
}

struct find_node_struct
{
	GValue			p_value;
	XtmProcessTreeModel *	treemodel;
	GNode *			parent;
};

static gboolean
find_node (GNode *node, gpointer data)
{
	XtmCrossLink *lnk = node->data;
	struct find_node_struct *found = data;
	gboolean same = FALSE;
	GValue c_value = {0};
	if (lnk == NULL)
		return FALSE;
	/* Use path for non-persistent models */
	if (lnk->path)
		gtk_tree_model_get_iter (found->treemodel->model, &lnk->iter, lnk->path);
	gtk_tree_model_get_value (found->treemodel->model, &lnk->iter, found->treemodel->c_column, &c_value);
	/* Find the node with the c_value we are looking for */
	if (g_value_get_uint (&c_value) > 0) /* PID of 0 doesn't exist */
		same = g_value_get_uint (&found->p_value) == g_value_get_uint (&c_value);
	g_value_unset (&c_value);
	if (same)
		found->parent = node;
	return same;
}

static gboolean
signal_insert (GNode *node, gpointer data)
{
	XtmProcessTreeModel *treemodel = data;
	GtkTreePath *s_path;
	GtkTreeIter s_iter;

	s_iter.stamp = treemodel->stamp;
	s_iter.user_data = node;
	s_iter.user_data2 = NULL;
	s_iter.user_data3 = NULL;
	s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
	gtk_tree_path_free (s_path);

	return FALSE;
}

static gboolean
signal_children (GNode *node, gpointer data)
{
	XtmProcessTreeModel *treemodel = data;
	GtkTreePath *s_path;
	GtkTreeIter s_iter;

	s_iter.stamp = treemodel->stamp;
	s_iter.user_data = node;
	s_iter.user_data2 = NULL;
	s_iter.user_data3 = NULL;
	s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
	gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
	gtk_tree_path_free (s_path);

	return FALSE;
}

static GNode *
find_sibling (GNode *parent, GSequenceIter *child)
{
	GSequenceIter *prev;
	/* Go backward in the list until a node is found with the same parent */
	for (prev = g_sequence_iter_prev (child); prev != child; prev = g_sequence_iter_prev (child))
    	{
		XtmCrossLink *lnk;
		child = prev;
		lnk = g_sequence_get (child);
		if (lnk->tree->parent == parent)
		{
			return lnk->tree;
		}
    	}
	return NULL;
}

static void
xtm_process_tree_model_row_changed (XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeIter *iter, GtkTreeModel *model)
{
	GtkTreePath *s_path;
	GtkTreeIter s_iter;
	XtmCrossLink *lnk, *c_link;
	GNode *node, *next_node, *old_parent;
	struct find_node_struct found;
	GValue c_value = {0};
	GValue p_value = {0};
	gboolean same = TRUE;
	gboolean signal_parent;

	g_return_if_fail (gtk_tree_path_get_depth (path) == 1);

	/* Get the changed item */
	lnk = g_sequence_get (
		g_sequence_get_iter_at_pos (treemodel->list, *gtk_tree_path_get_indices (path)));

	g_return_if_fail (lnk != NULL);

	s_iter.stamp = treemodel->stamp;
	s_iter.user_data2 = NULL;
	s_iter.user_data3 = NULL;

	/* Use the root entry as fall-back if no parent could be found */
	memset(&found, 0, sizeof(found));
	found.parent = treemodel->tree;
	found.treemodel = treemodel;
	gtk_tree_model_get_value (model, iter, treemodel->p_column, &found.p_value);
	old_parent = lnk->tree->parent;
	/* Check if the parent is still the same */
	if (!find_node (old_parent, &found))
	{
		/* Find the new parent */
		g_node_traverse (treemodel->tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_node, &found);
		if (found.parent != old_parent)
		{
			/* We have a new parent */
			s_iter.user_data = lnk->tree;
			s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);

			g_node_unlink (lnk->tree);

			/* Signal the delete */
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (treemodel), s_path);
			gtk_tree_path_free (s_path);

			/* Signal had child toggled if the old parent has no more children */
			if (old_parent != treemodel->tree && g_node_first_child (old_parent) == NULL)
			{
				s_iter.user_data = old_parent;
				s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
				gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
				gtk_tree_path_free (s_path);
			}

			/* Signal has child toggled if the new parent had no children */
			signal_parent = found.parent != treemodel->tree && g_node_first_child (found.parent) == NULL;
			/* Find the previous sibling at the same level to preserve sorting order */
			g_node_insert_after (found.parent, find_sibling (found.parent, lnk->list), lnk->tree); /* we could bypass find_sibling if the parent has no children */

			/* Signal the insert */
			g_node_traverse (lnk->tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				signal_insert, treemodel);

			if (signal_parent)
			{
				s_iter.user_data = found.parent;
				s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
				gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
				gtk_tree_path_free (s_path);
			}

			/* Signal has child toggled for all nodes with children */
			/* We use this for auto expanding. It is not really clear if has child toggled should be called before the child insert is signaled */
			g_node_traverse (lnk->tree, G_PRE_ORDER, G_TRAVERSE_NON_LEAVES, -1,
				signal_children, treemodel);

			same = FALSE;
		}
	}
	g_value_unset (&found.p_value);

	/* Only signal row changed if it just didn't delete/insert (re-parent) */
	if (same)
	{
		/* Signal the change */
		s_iter.user_data = lnk->tree;
		s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
		gtk_tree_path_free (s_path);
	}

	/* Maybe this just became a parent? */
	gtk_tree_model_get_value (model, iter, treemodel->c_column, &c_value);
	if (g_value_get_uint (&c_value) > 0) /* PID of 0 doesn't exist */
	{
		/* Search the top level of the tree for possible children */
		for (node = g_node_first_child (treemodel->tree); node; node = next_node)
		{
			next_node = g_node_next_sibling (node);

			/* Skip ourself */
			if (node == lnk->tree)
				continue;

			c_link = node->data;
			/* Use path for non-persistent models */
			if (c_link->path)
				gtk_tree_model_get_iter (model, &c_link->iter, c_link->path);
			gtk_tree_model_get_value (model, &c_link->iter, treemodel->p_column, &p_value);
			/* See if this is a child */
			same = g_value_get_uint (&p_value) == g_value_get_uint (&c_value);
			g_value_unset (&p_value);
			if (same)
			{
				s_iter.user_data = node;
				s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);

				g_node_unlink (node);

				/* Signal the delete */
				gtk_tree_model_row_deleted (GTK_TREE_MODEL (treemodel), s_path);
				gtk_tree_path_free (s_path);

				/* Signal has child toggled if this is the first child */
				signal_parent = g_node_first_child (lnk->tree) == NULL;
				/* Assuming we had no children, we can just append to keep the sorting order correct */
				g_node_append (lnk->tree, node);

				/* Signal the insert */
				g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
					signal_insert, treemodel);

				if (signal_parent)
				{
					s_iter.user_data = lnk->tree;
					s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
					gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
					gtk_tree_path_free (s_path);
				}

				/* Signal has child toggled for all nodes with children */
				/* We use this for auto expanding. It is not really clear if has child toggled should be called before the child insert is signaled */
				g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_NON_LEAVES, -1,
					signal_children, treemodel);
			}
		}
	}
	g_value_unset (&c_value);
}

/* do_path is only used for non-persistent models */
static void
do_path (gpointer data, gpointer user_data)
{
	XtmCrossLink *lnk = data;
	void (*func) (GtkTreePath*) = (void (*) (GtkTreePath*))user_data;
	/* Use path for non-persistent models */
	g_return_if_fail (lnk->path);
	func (lnk->path);
}

static void
xtm_process_tree_model_row_inserted (XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeIter *iter, GtkTreeModel *model)
{
	GtkTreePath *s_path;
	GtkTreeIter s_iter;
	XtmCrossLink *lnk, *c_link;
	GNode *node, *next_node;
	struct find_node_struct found;
	GValue c_value = {0};
	GValue p_value = {0};
	gboolean same;
	gboolean not_persist = TRUE;
	gboolean signal_parent;

	g_return_if_fail (gtk_tree_path_get_depth (path) == 1);

	/* Take a reference on this node, to want to stay informed about any changes in this row */
	gtk_tree_model_ref_node(model, iter);

	not_persist = ! (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_ITERS_PERSIST);

	s_iter.stamp = treemodel->stamp;
	s_iter.user_data2 = NULL;
	s_iter.user_data3 = NULL;

	/* Insert the new item */
	lnk = xtm_cross_link_new ();
	lnk->iter = *iter;
	/* Use path for non-persistent models */
	if (not_persist)
		lnk->path = gtk_tree_path_copy (path);
	/* Insert the link in the shadow list */
	lnk->list = g_sequence_insert_before (
		g_sequence_get_iter_at_pos (treemodel->list, *gtk_tree_path_get_indices (path)),
		lnk);
	/* Need to update all path caches after the insert and increment them with one */
	if (not_persist)
		g_sequence_foreach_range (g_sequence_iter_next (lnk->list), g_sequence_get_end_iter (treemodel->list),
			do_path, (gpointer)gtk_tree_path_next);

	memset(&found, 0, sizeof(found));
	found.parent = treemodel->tree;
	found.treemodel = treemodel;
	gtk_tree_model_get_value (model, iter, treemodel->p_column, &found.p_value);
	/* Find the parent */
	g_node_traverse (treemodel->tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, find_node, &found);
	g_value_unset (&found.p_value);
	/* Signal has child toggled if this is the first child */
	signal_parent = found.parent != treemodel->tree && g_node_first_child (found.parent) == NULL;
	/* Find the previous sibling at the same level to preserve sorting order */
	lnk->tree = g_node_insert_data_after (found.parent, find_sibling (found.parent, lnk->list), lnk);

	/* Signal the new item */
	s_iter.user_data = lnk->tree;
	s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
	gtk_tree_path_free (s_path);

	if (signal_parent)
	{
		s_iter.user_data = found.parent;
		s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
		gtk_tree_path_free (s_path);
	}

	/* Maybe this just became a parent? */
	gtk_tree_model_get_value (model, iter, treemodel->c_column, &c_value);
	if (g_value_get_uint (&c_value) > 0) /* PID of 0 doesn't exist */
	{
		/* Search the top level of the tree for possible children */
		for (node = g_node_first_child (treemodel->tree); node; node = next_node)
		{
			next_node = g_node_next_sibling (node);

			/* Skip ourself */
			if (node == lnk->tree)
				continue;

			c_link = node->data;
			/* Use path for non-persistent models */
			if (c_link->path)
				gtk_tree_model_get_iter (model, &c_link->iter, c_link->path);
			gtk_tree_model_get_value (model, &c_link->iter, treemodel->p_column, &p_value);
			/* See if this is a child */
			same = g_value_get_uint (&p_value) == g_value_get_uint (&c_value);
			g_value_unset (&p_value);
			if (same)
			{
				s_iter.user_data = node;
				s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);

				g_node_unlink (node);

				/* Signal the delete */
				gtk_tree_model_row_deleted (GTK_TREE_MODEL (treemodel), s_path);
				gtk_tree_path_free (s_path);

				/* Signal has child toggled if this is the first child */
				signal_parent = g_node_first_child (lnk->tree) == NULL;
				/* We can't have children, we can just append to keep the sorting order correct */
				g_node_append (lnk->tree, node);

				/* Signal the insert */
				g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
					signal_insert, treemodel);

				if (signal_parent)
				{
					s_iter.user_data = lnk->tree;
					s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
					gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
					gtk_tree_path_free (s_path);
				}

				/* Signal has child toggled for all nodes with children */
				/* We use this for auto expanding. It is not really clear if has child toggled should be called before the child insert is signaled */
				g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_NON_LEAVES, -1,
					signal_children, treemodel);
			}
		}
	}
	g_value_unset (&c_value);
}

static void
xtm_process_tree_model_row_deleted (XtmProcessTreeModel *treemodel, GtkTreePath *path, GtkTreeModel *model)
{
	GtkTreePath *s_path;
	GtkTreeIter s_iter;
	XtmCrossLink *lnk;
	GNode *del_node, *node, *old_parent;
	gboolean not_persist = TRUE;

	g_return_if_fail (gtk_tree_path_get_depth (path) == 1);

	not_persist = ! (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_ITERS_PERSIST);

	s_iter.stamp = treemodel->stamp;
	s_iter.user_data2 = NULL;
	s_iter.user_data3 = NULL;

	/* Get the deleted item */
	lnk = g_sequence_get (
		g_sequence_get_iter_at_pos (treemodel->list, *gtk_tree_path_get_indices (path)));

	g_return_if_fail (lnk != NULL);

	s_iter.user_data = lnk->tree;
	s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);

	/* Need to update all path caches after the remove and decrement them with one */
	if (not_persist)
		g_sequence_foreach_range (g_sequence_iter_next (lnk->list), g_sequence_get_end_iter (treemodel->list),
			do_path, (gpointer)gtk_tree_path_prev);

	del_node = lnk->tree;
	old_parent = del_node->parent;
	g_node_unlink (del_node);
	del_node->data = NULL;
	/* Remove the link from the shadow list */
	g_sequence_remove (lnk->list);

	/* Signal the delete */
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (treemodel), s_path);
	gtk_tree_path_free (s_path);

	/* Signal had child toggled if the old parent has no more children */
	if (old_parent != treemodel->tree && g_node_first_child (old_parent) == NULL)
	{
		s_iter.user_data = old_parent;
		s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (treemodel), s_path, &s_iter);
		gtk_tree_path_free (s_path);
	}

	/* Move all the children to the root of the tree */
	while ((node = g_node_first_child (del_node)))
	{
		XtmCrossLink *i_link = node->data;
		g_node_unlink (node);
		/* Find the previous sibling at the same level to preserve sorting order */
		g_node_insert_after (treemodel->tree, find_sibling (treemodel->tree, i_link->list), node);
		/* Signal the insert */
		g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			signal_insert, treemodel);
		/* Signal has child toggled for all nodes with children */
		/* We use this for auto expanding. It is not really clear if has child toggled should be called before the child insert is signaled */
		g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_NON_LEAVES, -1,
			signal_children, treemodel);
	}

	g_node_destroy (del_node);
}

static gboolean
reorder_children (GNode *parent, gpointer data)
{
	XtmProcessTreeModel *treemodel = data;
	GSequenceIter *iter;
	GNode *child = NULL;
	XtmCrossLink *lnk;
	GtkTreeIter s_iter;
	GtkTreePath *s_path;
	guint size, i;
	gint *new_order;
	gint c_pos, old_pos;
	gboolean moved = FALSE;
	size = g_node_n_children (parent);
	if (size < 2)
		return FALSE;
	/* Initialize the reorder list */
	new_order = g_new (gint, size);
	for (i = 0; i < size; i++)
	{
		new_order[i] = (gint)i;
	}
	i = 0;
	/* Walk the whole list in the new order */
	for (iter = g_sequence_get_begin_iter (treemodel->list); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter))
	{
		lnk = g_sequence_get (iter);
		/* Is this item a child of the current node */
		if (lnk->tree->parent == parent)
		{
			/* Get the current position in the tree */
			c_pos = g_node_child_position (parent, lnk->tree);
			g_node_unlink (lnk->tree);
			/* Place it as the next one in the tree */
			g_node_insert_after (parent, child, lnk->tree);
			child = lnk->tree;
			/* Get the true old position */
			old_pos = new_order[c_pos];
			/* calculate the amount the child was moved in the list */
			c_pos -= i;
			if (c_pos > 0)
			{
				/* move the items in between to keep order list in sync with the current tree */
				memmove ((new_order + i + 1), (new_order + i), ((guint)c_pos * sizeof(gint)));
				moved = TRUE;
			}
			/* Store the old position at the new location */
			new_order[i++] = old_pos;
		}
	}
	g_return_val_if_fail (size == i, TRUE);
	/* The content of new_order can only be changed if we moved something */
	if (moved)
	{
		/* Signal the reorder */
		s_iter.stamp = treemodel->stamp;
		s_iter.user_data = parent;
		s_iter.user_data2 = NULL;
		s_iter.user_data3 = NULL;
		s_path = xtm_process_tree_model_get_path (GTK_TREE_MODEL (treemodel), &s_iter);
		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (treemodel), s_path, &s_iter, new_order);
		gtk_tree_path_free (s_path);
	}
	g_free (new_order);
	return FALSE;
}

static void
xtm_process_tree_model_rows_reordered (XtmProcessTreeModel *treemodel, GtkTreePath *path __unused, GtkTreeIter *iter __unused, gint *new_order, GtkTreeModel *model)
{
	gint i, size;
	GSequence *s_list;
	GSequenceIter *s_iter;
	GSequenceIter *swap1, *swap2;
	XtmCrossLink *lnk;
	gboolean not_persist = TRUE;

	size = g_sequence_get_length (treemodel->list);

	if (G_UNLIKELY (size == 0))
		return;

	not_persist = ! (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_ITERS_PERSIST);

	/* New list to hold the new order */
	s_list = g_sequence_new (xtm_cross_link_free);
	s_iter = g_sequence_get_end_iter (s_list);

	for (i = 0; i < size; i++)
	{
		/* make a dummy to hold the space */
		swap1 = g_sequence_insert_before (s_iter, NULL);
		swap2 = g_sequence_get_iter_at_pos (treemodel->list, new_order[i]);
		/* swap the data between the two lists */
		g_sequence_swap (swap1, swap2);

		/* Use path for non-persistent models */
		if (not_persist)
		{
			lnk = g_sequence_get (swap2);
			g_warn_if_fail (lnk->path);
			gtk_tree_path_up (lnk->path);
			/* Update the path with the new location */
			gtk_tree_path_append_index (lnk->path, i);
		}
	}

	g_sequence_free (treemodel->list);
	treemodel->list = s_list;

	/* Reorder the whole tree */
	g_node_traverse (treemodel->tree, G_PRE_ORDER, G_TRAVERSE_NON_LEAFS, -1,
		reorder_children, treemodel);
}

static void
xtm_process_tree_model_set_model (XtmProcessTreeModel *treemodel, GtkTreeModel *model)
{
	g_return_if_fail (GTK_IS_TREE_MODEL (model));

	treemodel->model = model;

	g_signal_connect_object (model, "row-changed", G_CALLBACK (xtm_process_tree_model_row_changed), treemodel, G_CONNECT_SWAPPED);
	g_signal_connect_object (model, "row-inserted", G_CALLBACK (xtm_process_tree_model_row_inserted), treemodel, G_CONNECT_SWAPPED);
	//g_signal_connect_object (model, "row-has-child-toggled", G_CALLBACK (xtm_process_tree_model_row_has_child_toggled), treemodel, G_CONNECT_SWAPPED);
	g_signal_connect_object (model, "row-deleted", G_CALLBACK (xtm_process_tree_model_row_deleted), treemodel, G_CONNECT_SWAPPED);
	g_signal_connect_object (model, "rows-reordered", G_CALLBACK (xtm_process_tree_model_rows_reordered), treemodel, G_CONNECT_SWAPPED);
}



GtkTreeModel *
xtm_process_tree_model_new (GtkTreeModel * model)
{
	/* Only support flat models to build a tree */
	g_return_val_if_fail (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_LIST_ONLY, NULL);

	return g_object_new (XTM_TYPE_PROCESS_TREE_MODEL, "model", model, NULL);
}

