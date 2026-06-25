/*
 *  Copyright (c) 2012-2013 Andrzej Radecki <andrzejr@xfce.org>
 *  Copyright (c) 2017      Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4panel/libxfce4panel.h>

#include "sn-box.h"
#include "sn-button.h"
#include "sn-util.h"



static void                  sn_box_get_property                     (GObject                 *object,
                                                                      guint                    prop_id,
                                                                      GValue                  *value,
                                                                      GParamSpec              *pspec);
static void                  sn_box_finalize                         (GObject                 *object);

static void                  sn_box_collect_known_items              (SnBox                   *box,
                                                                      GHashTable              *result);

static void                  sn_box_list_changed                     (SnBox                   *box,
                                                                      SnConfig                *config);

static void                  sn_box_add                              (GtkContainer            *container,
                                                                      GtkWidget               *child);

static void                  sn_box_remove                           (GtkContainer            *container,
                                                                      GtkWidget               *child);

static void                  sn_box_forall                           (GtkContainer            *container,
                                                                      gboolean                 include_internals,
                                                                      GtkCallback              callback,
                                                                      gpointer                 callback_data);

static GType                 sn_box_child_type                       (GtkContainer            *container);

static void                  sn_box_get_preferred_width              (GtkWidget               *widget,
                                                                      gint                    *minimal_width,
                                                                      gint                    *natural_width);

static void                  sn_box_get_preferred_height             (GtkWidget               *widget,
                                                                      gint                    *minimal_height,
                                                                      gint                    *natural_height);

static void                  sn_box_size_allocate                    (GtkWidget               *widget,
                                                                     GtkAllocation            *allocation);



enum
{
  PROP_0,
  PROP_HAS_HIDDEN
};

struct _SnBoxClass
{
  GtkContainerClass    __parent__;
};

struct _SnBox
{
  GtkContainer         __parent__;

  SnConfig            *config;

  /* in theory it's possible to have multiple items with same name */
  GHashTable          *children;

  /* hidden children counter */
  gint                 n_hidden_children;
  gint                 n_visible_children;
  gboolean             show_hidden;
};

G_DEFINE_TYPE (SnBox, sn_box, GTK_TYPE_CONTAINER)



static void
sn_box_class_init (SnBoxClass *klass)
{
  GObjectClass      *object_class;
  GtkWidgetClass    *widget_class;
  GtkContainerClass *container_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = sn_box_get_property;
  object_class->finalize = sn_box_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->get_preferred_width = sn_box_get_preferred_width;
  widget_class->get_preferred_height = sn_box_get_preferred_height;
  widget_class->size_allocate = sn_box_size_allocate;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add = sn_box_add;
  container_class->remove = sn_box_remove;
  container_class->forall = sn_box_forall;
  container_class->child_type = sn_box_child_type;

  g_object_class_install_property (object_class,
                                   PROP_HAS_HIDDEN,
                                   g_param_spec_boolean ("has-hidden",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}



static void
sn_box_init (SnBox *box)
{
  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (box), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);

  box->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}



static void
sn_box_get_property(GObject *object,
                    guint prop_id,
                    GValue *value,
                    GParamSpec *pspec)
{
  SnBox *box = XFCE_SN_BOX(object);

  switch (prop_id)
  {
  case PROP_HAS_HIDDEN:
    g_value_set_boolean(value, box->n_hidden_children > 0);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}



static void
sn_box_finalize (GObject *object)
{
  SnBox *box = XFCE_SN_BOX (object);

  g_hash_table_destroy (box->children);

  G_OBJECT_CLASS (sn_box_parent_class)->finalize (object);
}



GtkWidget *
sn_box_new (SnConfig *config)
{
  SnBox *box = g_object_new (XFCE_TYPE_SN_BOX, NULL);

  box->config = config;

  sn_signal_connect_weak_swapped (G_OBJECT (box->config), "collect-known-items",
                                  G_CALLBACK (sn_box_collect_known_items), box);
  sn_signal_connect_weak_swapped (G_OBJECT (box->config), "items-list-changed",
                                  G_CALLBACK (sn_box_list_changed), box);

  return GTK_WIDGET (box);
}



static void
sn_box_collect_known_items_callback (GtkWidget *widget,
                                     gpointer   user_data)
{
  SnButton   *button = XFCE_SN_BUTTON (widget);
  GHashTable *table = user_data;
  gchar      *name;

  name = g_strdup (sn_button_get_name (button));
  g_hash_table_replace (table, name, name);
}



static void
sn_box_collect_known_items (SnBox      *box,
                            GHashTable *result)
{
  gtk_container_foreach (GTK_CONTAINER (box),
                         sn_box_collect_known_items_callback, result);
}



static void
sn_box_list_changed (SnBox    *box,
                     SnConfig *config)
{
  SnButton *button;
  GList    *known_items, *li, *li_int, *li_tmp;
  gint      n_hidden_children = 0, n_visible_children = 0;

  g_return_if_fail (XFCE_IS_SN_BOX (box));
  g_return_if_fail (XFCE_IS_SN_CONFIG (config));

  known_items = sn_config_get_known_items (box->config);
  for (li = known_items; li != NULL; li = li->next)
    {
      li_int = g_hash_table_lookup (box->children, li->data);
      for (li_tmp = li_int; li_tmp != NULL; li_tmp = li_tmp->next)
        {
          button = li_tmp->data;
          if (!sn_config_is_hidden (box->config,
                                    sn_button_get_name (button)))
            {
              gtk_widget_map (GTK_WIDGET(button));
              n_visible_children++;
            }
          else
            {
              gtk_widget_set_mapped (GTK_WIDGET (button), box->show_hidden);
              n_hidden_children++;
            }
        }
    }
  
  box->n_visible_children = n_visible_children;
  if (box->n_hidden_children != n_hidden_children)
    {
      box->n_hidden_children = n_hidden_children;
      g_object_notify (G_OBJECT (box), "has-hidden");
    }

  gtk_widget_queue_resize (GTK_WIDGET (box));
}



static void
sn_box_add (GtkContainer *container,
            GtkWidget    *child)
{
  SnBox       *box = XFCE_SN_BOX (container);
  SnButton    *button = XFCE_SN_BUTTON (child);
  GList       *li;
  const gchar *name;

  g_return_if_fail (XFCE_IS_SN_BOX (box));
  g_return_if_fail (XFCE_IS_SN_BUTTON (button));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (child)) == NULL);

  name = sn_button_get_name (button);
  li = g_hash_table_lookup (box->children, name);
  li = g_list_prepend (li, button);
  g_hash_table_replace (box->children, g_strdup (name), li);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  gtk_widget_queue_resize (GTK_WIDGET (container));
}



static void
sn_box_remove (GtkContainer *container,
               GtkWidget    *child)
{
  SnBox       *box = XFCE_SN_BOX (container);
  SnButton    *button = XFCE_SN_BUTTON (child);
  GList       *li, *li_tmp;
  const gchar *name;

  /* search the child */
  name = sn_button_get_name (button);
  li = g_hash_table_lookup (box->children, name);
  li_tmp = g_list_find (li, button);
  if (G_LIKELY (li_tmp != NULL))
    {
      /* unparent widget */
      li = g_list_remove_link (li, li_tmp);
      g_hash_table_replace (box->children, g_strdup (name), li);
      gtk_widget_unparent (child);

      /* resize, so we update has-hidden */
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}



static void
sn_box_forall (GtkContainer *container,
               gboolean      include_internals,
               GtkCallback   callback,
               gpointer      callback_data)
{
  SnBox    *box = XFCE_SN_BOX (container);
  SnButton *button;
  GList    *known_items, *li, *li_int, *li_tmp;

  /* run callback for all children */
  known_items = sn_config_get_known_items (box->config);
  for (li = known_items; li != NULL; li = li->next)
    {
      li_int = g_hash_table_lookup (box->children, li->data);
      for (li_tmp = li_int; li_tmp != NULL; li_tmp = li_tmp->next)
        {
          button = li_tmp->data;
          callback (GTK_WIDGET (button), callback_data);
        }
    }
}



static GType
sn_box_child_type (GtkContainer *container)
{
  return XFCE_TYPE_SN_BUTTON;
}



static void
sn_box_measure_and_allocate (GtkWidget *widget,
                             gint      *minimum_length,
                             gint      *natural_length,
                             gboolean   allocate,
                             gint       x0,
                             gint       y0,
                             gboolean   horizontal)
{
  SnBox          *box = XFCE_SN_BOX (widget);
  SnButton       *button;
  GList          *known_items, *li, *li_int, *li_tmp;
  gint            panel_size, config_nrows, icon_size, hx_size, hy_size, nrows;
  gboolean        single_row, single_horizontal, square_icons, rect_child;
  gint            total_length, column_length, item_length, row;
  GtkRequisition  child_req;
  GtkAllocation   child_alloc;

  gint n_hidden_children = 0, n_visible_children = 0;

  panel_size = sn_config_get_panel_size (box->config);
  config_nrows = sn_config_get_nrows (box->config);
  icon_size = sn_config_get_icon_size (box->config);
  single_row = sn_config_get_single_row (box->config);
  square_icons = sn_config_get_square_icons (box->config);
  if (square_icons)
    {
      nrows = single_row ? 1 : MAX (1, config_nrows);
      hx_size = hy_size = panel_size / nrows;
    }
  else
    {
      hx_size = MIN (icon_size, panel_size);
      nrows = single_row ? 1 : MAX (1, panel_size / hx_size);
      hy_size = panel_size / nrows;
    }

  total_length = 0;
  column_length = 0;
  row = 0;

  known_items = sn_config_get_known_items (box->config);
  for (li = known_items; li != NULL; li = li->next)
    {
      li_int = g_hash_table_lookup (box->children, li->data);
      for (li_tmp = li_int; li_tmp != NULL; li_tmp = li_tmp->next)
        {
          button = li_tmp->data;
          if (sn_config_is_hidden (box->config,
                                   sn_button_get_name (button)))
            {
              n_hidden_children++;
              if (!box->show_hidden)
                {
                  gtk_widget_unmap (GTK_WIDGET (button));
                  continue;
                }
            }
          gtk_widget_map (GTK_WIDGET (button));
          n_visible_children++;

          gtk_widget_get_preferred_size (GTK_WIDGET (button), NULL, &child_req);

          rect_child = child_req.width > child_req.height;
          if (horizontal)
            {
              if (square_icons && (!rect_child || (config_nrows >= 2 && !single_row)))
                item_length = hx_size;
              else
                item_length = MAX (hx_size, child_req.width);

              column_length = MAX (column_length, item_length);
              single_horizontal = FALSE;
            }
          else
            {
              column_length = hx_size;
              single_horizontal = rect_child;

              if (square_icons)
                item_length = single_horizontal ? panel_size : hy_size;
              else
                item_length = MAX (MIN (panel_size, child_req.width), hy_size);
            }

          if (single_horizontal)
            {
              if (row > 0)
                total_length += hx_size;
              row = -1; /* will become 0 later and take the full length */
            }

          if (allocate)
            {
              if (horizontal)
                {
                  child_alloc.x = x0 + total_length;
                  child_alloc.y = y0 + row * hy_size;
                  child_alloc.width = item_length;
                  child_alloc.height = hy_size;
                }
              else
                {
                  child_alloc.x = x0 + (single_horizontal ? 0 : row * hy_size);
                  child_alloc.y = y0 + total_length;
                  child_alloc.width = item_length;
                  child_alloc.height = hx_size;
                }

              gtk_widget_size_allocate (GTK_WIDGET (button), &child_alloc);
            }

          row = (row + 1) % nrows;

          if (row == 0)
            {
              total_length += column_length;
              column_length = 0;
            }
        }
    }

  total_length += column_length;

  if (minimum_length != NULL)
    *minimum_length = total_length;

  if (natural_length != NULL)
    *natural_length = total_length;

  box->n_visible_children = n_visible_children;
  if (box->n_hidden_children != n_hidden_children)
  {
    box->n_hidden_children = n_hidden_children;
    g_object_notify(G_OBJECT(box), "has-hidden");
  }
}



static void
sn_box_get_preferred_width (GtkWidget *widget,
                            gint      *minimum_width,
                            gint      *natural_width)
{
  SnBox *box = XFCE_SN_BOX (widget);
  gint   panel_size;

  if (sn_config_get_panel_orientation (box->config) == GTK_ORIENTATION_HORIZONTAL)
    {
      sn_box_measure_and_allocate (widget, minimum_width, natural_width,
                                   FALSE, 0, 0, TRUE);
    }
  else
    {
      panel_size = sn_config_get_panel_size (box->config);
      if (minimum_width != NULL)
        *minimum_width = panel_size;
      if (natural_width != NULL)
        *natural_width = panel_size;
    }
}



static void
sn_box_get_preferred_height (GtkWidget *widget,
                             gint      *minimum_height,
                             gint      *natural_height)
{
  SnBox *box = XFCE_SN_BOX (widget);
  gint   panel_size;

  if (sn_config_get_panel_orientation (box->config) == GTK_ORIENTATION_VERTICAL)
    {
      sn_box_measure_and_allocate (widget, minimum_height, natural_height,
                                   FALSE, 0, 0, FALSE);
    }
  else
    {
      panel_size = sn_config_get_panel_size (box->config);
      if (minimum_height != NULL)
        *minimum_height = panel_size;
      if (natural_height != NULL)
        *natural_height = panel_size;
    }
}



static void
sn_box_size_allocate (GtkWidget     *widget,
                      GtkAllocation *allocation)
{
  SnBox *box = XFCE_SN_BOX (widget);

  gtk_widget_set_allocation (widget, allocation);

  sn_box_measure_and_allocate (widget, NULL, NULL,
                               TRUE, allocation->x, allocation->y,
                               sn_config_get_panel_orientation (box->config) ==
                               GTK_ORIENTATION_HORIZONTAL);
}



void
sn_box_remove_item (SnBox  *box,
                    SnItem *item)
{
  SnButton *button;
  GList    *known_items, *li, *li_int, *li_tmp;

  g_return_if_fail (XFCE_IS_SN_BOX (box));

  known_items = sn_config_get_known_items (box->config);
  for (li = known_items; li != NULL; li = li->next)
    {
      li_int = g_hash_table_lookup (box->children, li->data);
      for (li_tmp = li_int; li_tmp != NULL; li_tmp = li_tmp->next)
        {
          button = li_tmp->data;
          if (sn_button_get_item (button) == item)
            {
              gtk_container_remove (GTK_CONTAINER (box), GTK_WIDGET (button));
              return;
            }
        }
    }
}

gboolean
sn_box_has_hidden_items (SnBox *box)
{
  g_return_val_if_fail (XFCE_IS_SN_BOX (box), FALSE);
  return box->n_hidden_children > 0;
}

void
sn_box_set_show_hidden (SnBox      *box,
                        gboolean    show_hidden)
{
  g_return_if_fail (XFCE_IS_SN_BOX (box));

  if (box->show_hidden != show_hidden)
    {
      box->show_hidden = show_hidden;

      if (box->children != NULL)
        gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}