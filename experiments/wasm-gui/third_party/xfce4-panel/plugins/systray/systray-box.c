/*
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <common/panel-private.h>
#include <common/panel-debug.h>

#include "systray-box.h"
#include "systray-socket.h"
#include "sn-config.h"

#define SPACING    (2)
#define OFFSCREEN  (-9999)

/* some icon implementations request a 1x1 size for invisible icons */
#define REQUISITION_IS_INVISIBLE(child_req) ((child_req).width <= 1 && (child_req).height <= 1)



static void     systray_box_get_property          (GObject         *object,
                                                   guint            prop_id,
                                                   GValue          *value,
                                                   GParamSpec      *pspec);
static void     systray_box_finalize              (GObject         *object);
static void     systray_box_get_preferred_length  (GtkWidget       *widget,
                                                   gint            *minimum_length,
                                                   gint            *natural_length);
static void     systray_box_get_preferred_width   (GtkWidget       *widget,
                                                   gint            *minimum_width,
                                                   gint            *natural_width);
static void     systray_box_get_preferred_height  (GtkWidget       *widget,
                                                   gint            *minimum_height,
                                                   gint            *natural_height);
static void     systray_box_size_allocate         (GtkWidget       *widget,
                                                   GtkAllocation   *allocation);
static void     systray_box_add                   (GtkContainer    *container,
                                                   GtkWidget       *child);
static void     systray_box_remove                (GtkContainer    *container,
                                                   GtkWidget       *child);
static void     systray_box_forall                (GtkContainer    *container,
                                                   gboolean         include_internals,
                                                   GtkCallback      callback,
                                                   gpointer         callback_data);
static GType    systray_box_child_type            (GtkContainer    *container);
static gint     systray_box_compare_function      (gconstpointer    a,
                                                   gconstpointer    b,
                                                   gpointer         user_data);



enum
{
  PROP_0,
  PROP_HAS_HIDDEN
};

struct _SystrayBoxClass
{
  GtkContainerClass __parent__;
};

struct _SystrayBox
{
  GtkContainer  __parent__;

  /* all the icons packed in this box */
  GSList       *children;

  /* table of item indexes */
  GHashTable   *names_ordered;

  /* orientation of the box */
  guint         horizontal : 1;

  /* hidden children counter */
  gint          n_hidden_children;
  gint          n_visible_children;

  /* whether hidden icons are visible */
  guint         show_hidden : 1;

  /* dimensions */
  gint          size_max;
  gint          nrows;
  gint          row_size;
  gint          row_padding;

  /* whether icons are squared */
  guint         square_icons : 1;

  /* whether icons are in a single row */
  guint         single_row : 1;

  /* allocated size by the plugin */
  gint          size_alloc_init;
  gint          size_alloc;
};



XFCE_PANEL_DEFINE_TYPE (SystrayBox, systray_box, GTK_TYPE_CONTAINER)



static void
systray_box_class_init (SystrayBoxClass *klass)
{
  GObjectClass      *gobject_class;
  GtkWidgetClass    *gtkwidget_class;
  GtkContainerClass *gtkcontainer_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = systray_box_get_property;
  gobject_class->finalize = systray_box_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_preferred_width = systray_box_get_preferred_width;
  gtkwidget_class->get_preferred_height = systray_box_get_preferred_height;
  gtkwidget_class->size_allocate = systray_box_size_allocate;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->add = systray_box_add;
  gtkcontainer_class->remove = systray_box_remove;
  gtkcontainer_class->forall = systray_box_forall;
  gtkcontainer_class->child_type = systray_box_child_type;

  g_object_class_install_property (gobject_class,
                                   PROP_HAS_HIDDEN,
                                   g_param_spec_boolean ("has-hidden",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}



static void
systray_box_init (SystrayBox *box)
{
  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);

  box->children = NULL;
  box->names_ordered = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  box->size_max = DEFAULT_ICON_SIZE;
  box->size_alloc_init = DEFAULT_ICON_SIZE;
  box->size_alloc = DEFAULT_ICON_SIZE;
  box->n_hidden_children = 0;
  box->n_visible_children = 0;
  box->horizontal = TRUE;
  box->show_hidden = DEFAULT_HIDE_NEW_ITEMS;
  box->square_icons = DEFAULT_SQUARE_ICONS;
}



static void
systray_box_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (object);

  switch (prop_id)
    {
    case PROP_HAS_HIDDEN:
      g_value_set_boolean (value, box->n_hidden_children > 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
systray_box_finalize (GObject *object)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (object);

  g_hash_table_destroy (box->names_ordered);

  /* check if we're leaking */
  if (G_UNLIKELY (box->children != NULL))
    {
      /* free the child list */
      g_slist_free (box->children);
      panel_debug (PANEL_DEBUG_SYSTRAY, "Not all icons has been removed from the systray.");
    }

  G_OBJECT_CLASS (systray_box_parent_class)->finalize (object);
}



static void
systray_box_size_get_max_child_size (SystrayBox *box,
                                     gint       *rows_ret,
                                     gint       *icon_size_ret,
                                     gint       *row_size_ret,
                                     gint       *offset_ret)
{
  if (rows_ret != NULL)
    *rows_ret = box->nrows;

  if (icon_size_ret != NULL)
    *icon_size_ret = box->square_icons ? box->row_size : box->size_max;

  if (row_size_ret != NULL)
    *row_size_ret = box->row_size;

  if (offset_ret != NULL)
    *offset_ret = box->row_padding;
}



static void
systray_box_get_preferred_width   (GtkWidget       *widget,
                                   gint            *minimum_width,
                                   gint            *natural_width)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (widget);

  if (box->horizontal)
    {
      systray_box_get_preferred_length (widget, minimum_width, natural_width);
    }
  else
    {
      if (minimum_width != NULL)
        *minimum_width = box->size_alloc_init;
      if (natural_width != NULL)
        *natural_width = box->size_alloc_init;
    }
}



static void
systray_box_get_preferred_height  (GtkWidget       *widget,
                                   gint            *minimum_height,
                                   gint            *natural_height)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (widget);

  if (box->horizontal)
    {
      if (minimum_height != NULL)
        *minimum_height = box->size_alloc_init;
      if (natural_height != NULL)
        *natural_height = box->size_alloc_init;
    }
  else
    {
      systray_box_get_preferred_length (widget, minimum_height, natural_height);
    }
}



static void
systray_box_get_preferred_length (GtkWidget      *widget,
                                  gint           *minimum_length,
                                  gint           *natural_length)
{
  SystrayBox       *box = XFCE_SYSTRAY_BOX (widget);
  GtkWidget        *child;
  GtkRequisition    child_req;
  gint              n_hidden_children = 0;
  gint              rows;
  gdouble           cols;
  gint              icon_size;
  gdouble           cells;
  gint              min_seq_cells = -1;
  gdouble           ratio;
  GSList           *li;
  gboolean          hidden;
  gint              length;
  GtkStyleContext  *ctx;
  GtkBorder         padding;

  box->n_visible_children = 0;

  /* get some info about the n_rows we're going to allocate */
  systray_box_size_get_max_child_size (box, &rows, &icon_size, NULL, NULL);

  for (li = box->children, cells = 0.00; li != NULL; li = li->next)
    {
      child = GTK_WIDGET (li->data);
      panel_return_if_fail (XFCE_IS_SYSTRAY_SOCKET (child));

      gtk_widget_get_preferred_size (child, NULL, &child_req);

      /* skip invisible requisitions (see macro) or hidden widgets */
      if (REQUISITION_IS_INVISIBLE (child_req)
          || !gtk_widget_get_visible (child))
        continue;

      hidden = systray_socket_get_hidden (XFCE_SYSTRAY_SOCKET (child));
      if (hidden)
        n_hidden_children++;

      /* if we show hidden icons */
      if (!hidden || box->show_hidden)
        {
          /* special handling for non-squared icons. this only works if
           * the icon size ratio is > 1.00, if this is lower then 1.00
           * the icon implementation should respect the tray orientation */
          if (!box->square_icons && G_UNLIKELY (child_req.width != child_req.height))
            {
              ratio = (gdouble) child_req.width / (gdouble) child_req.height;
              if (!box->horizontal)
                ratio = 1 / ratio;

              if (ratio > 1.00)
                {
                  if (G_UNLIKELY (rows > 1))
                    {
                      /* align to whole blocks if we have multiple rows */
                      ratio = ceil (ratio);

                      /* update the min sequential number of blocks */
                      min_seq_cells = MAX (min_seq_cells, ratio);
                    }

                  cells += ratio;
                  box->n_visible_children++;

                  continue;
                }
            }

          /* don't do anything with the actual size,
           * just count the number of cells */
          cells += 1.00;
          box->n_visible_children++;
        }
    }

  panel_debug_filtered (PANEL_DEBUG_SYSTRAY,
      "requested cells=%g, rows=%d, icon_size=%d, children=%d",
      cells, rows, icon_size, box->n_visible_children);

  if (cells > 0.00)
    {
      cols = cells / (gdouble) rows;
      if (rows > 1)
        cols = ceil (cols);
      if (cols * rows < cells)
        cols += 1.00;

      /* make sure we have enough columns to fix the minimum amount of cells */
      if (min_seq_cells != -1)
        cols = MAX (min_seq_cells, cols);

      if (box->square_icons)
        length = icon_size * cols;
      else
        length = icon_size * cols + (cols - 1) * SPACING;
    }
  else
    {
      length = 0;
    }

  /* emit property if changed */
  if (box->n_hidden_children != n_hidden_children)
    {
      panel_debug_filtered (PANEL_DEBUG_SYSTRAY,
          "hidden children changed (%d -> %d)",
          n_hidden_children, box->n_hidden_children);

      box->n_hidden_children = n_hidden_children;
      g_object_notify (G_OBJECT (box), "has-hidden");
    }

  /* add border size */
  ctx = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (ctx, gtk_widget_get_state_flags (widget), &padding);
  length += MAX (padding.left+padding.right, padding.top+padding.bottom);

  if (minimum_length != NULL)
    *minimum_length = length;

  if (natural_length != NULL)
    *natural_length = length;
}



static void
systray_box_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  SystrayBox       *box = XFCE_SYSTRAY_BOX (widget);
  GtkWidget        *child;
  GtkAllocation     child_alloc;
  GtkRequisition    child_req;
  gint              rows;
  gint              icon_size;
  gint              row_size;
  gdouble           ratio;
  gint              x, x_start, x_end;
  gint              y, y_start, y_end;
  gint              offset;
  GSList           *li;
  gint              alloc_size;
  gint              idx;
  GtkStyleContext  *ctx;
  GtkBorder         padding;
  gint              spacing;

  gtk_widget_set_allocation (widget, allocation);

  ctx = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (ctx, gtk_widget_get_state_flags (widget), &padding);

  alloc_size = box->horizontal ? allocation->height : allocation->width;
  spacing = box->square_icons ? 0 : SPACING;

  systray_box_size_get_max_child_size (box, &rows, &icon_size, &row_size, &offset);

  panel_debug_filtered (PANEL_DEBUG_SYSTRAY, "allocate rows=%d, icon_size=%d, w=%d, h=%d, horiz=%s, border=%d",
                        rows, icon_size, allocation->width, allocation->height,
                        PANEL_DEBUG_BOOL (box->horizontal), padding.left);

  /* get allocation bounds */
  x_start = allocation->x + padding.left;
  x_end = allocation->x + allocation->width - padding.right;

  y_start = allocation->y + padding.top;
  y_end = allocation->y + allocation->height - padding.bottom;

  /* add offset to center the tray contents */
  if (box->horizontal)
    y_start += offset;
  else
    x_start += offset;

  restart_allocation:

  x = x_start;
  y = y_start;

  for (li = box->children; li != NULL; li = li->next)
    {
      child = GTK_WIDGET (li->data);
      panel_return_if_fail (XFCE_IS_SYSTRAY_SOCKET (child));

      if (!gtk_widget_get_visible (child))
        continue;

      gtk_widget_get_preferred_size (child, NULL, &child_req);

      if (REQUISITION_IS_INVISIBLE (child_req)
          || (!box->show_hidden
              && systray_socket_get_hidden (XFCE_SYSTRAY_SOCKET (child))))
        {
          /* position hidden icons offscreen if we don't show hidden icons
           * or the requested size looks like an invisible icons (see macro) */
          child_alloc.x = child_alloc.y = OFFSCREEN;

          /* some implementations (hi nm-applet) start their setup on
           * a size-changed signal, so make sure this event is triggered
           * by allocation a normal size instead of 1x1 */
          child_alloc.width = child_alloc.height = icon_size;
        }
      else
        {
          /* special case handling for non-squared icons */
          if (!box->square_icons && G_UNLIKELY (child_req.width != child_req.height))
            {
              ratio = (gdouble) child_req.width / (gdouble) child_req.height;

              if (box->horizontal)
                {
                  child_alloc.height = icon_size;
                  child_alloc.width = icon_size * ratio;
                  child_alloc.y = child_alloc.x = 0;

                  if (rows > 1)
                    {
                      ratio = ceil (ratio);
                      child_alloc.x = ((ratio * icon_size) - child_alloc.width) / 2;
                    }
                }
              else
                {
                  ratio = 1 / ratio;

                  child_alloc.width = icon_size;
                  child_alloc.height = icon_size * ratio;
                  child_alloc.x = child_alloc.y = 0;

                  if (rows > 1)
                    {
                      ratio = ceil (ratio);
                      child_alloc.y = ((ratio * icon_size) - child_alloc.height) / 2;
                    }
                }
            }
          else
            {
              /* fix icon to row size */
              if (box->square_icons)
                {
                  child_alloc.width = MIN (icon_size, box->size_max);
                  child_alloc.height = MIN (icon_size, box->size_max);
                  child_alloc.x = (icon_size - child_alloc.width) / 2;
                  child_alloc.y = (icon_size - child_alloc.height) / 2;
                }
              else
                {
                  child_alloc.width = icon_size;
                  child_alloc.height = icon_size;
                  child_alloc.x = 0;
                  child_alloc.y = 0;
                }

              ratio = 1.00;
            }

          if ((box->horizontal && x + child_alloc.width > x_end)
              || (!box->horizontal && y + child_alloc.height > y_end))
            {
              if (ratio >= 2
                  && li->next != NULL)
                {
                  /* child doesn't fit, but maybe we still have space for the
                   * next icon, so move the child 1 step forward in the list
                   * and restart allocating the box */
                  idx = g_slist_position (box->children, li);
                  box->children = g_slist_delete_link (box->children, li);
                  box->children = g_slist_insert (box->children, child, idx + 1);

                  goto restart_allocation;
                }

              if (box->horizontal)
                {
                  x = x_start;
                  y += row_size;

                  if (!box->square_icons && y > y_end)
                    {
                      /* we overflow the number of rows, restart
                       * allocation with 1px smaller icons */
                      icon_size--;

                      panel_debug_filtered (PANEL_DEBUG_SYSTRAY,
                          "y overflow (%d > %d), restart with icon_size=%d",
                          y, y_end, icon_size);

                      goto restart_allocation;
                    }
                }
              else
                {
                  y = y_start;
                  x += row_size;

                  if (!box->square_icons && x > x_end)
                    {
                      /* we overflow the number of rows, restart
                       * allocation with 1px smaller icons */
                      icon_size--;

                      panel_debug_filtered (PANEL_DEBUG_SYSTRAY,
                          "x overflow (%d > %d), restart with icon_size=%d",
                          x, x_end, icon_size);

                      goto restart_allocation;
                    }
                }
            }

          child_alloc.x += x;
          child_alloc.y += y;

          if (box->horizontal)
            x += icon_size * ratio + spacing;
          else
            y += icon_size * ratio + spacing;
        }

      panel_debug_filtered (PANEL_DEBUG_SYSTRAY, "allocated %s[%p] at (%d,%d;%d,%d)",
          systray_socket_get_name (XFCE_SYSTRAY_SOCKET (child)), child,
          child_alloc.x, child_alloc.y, child_alloc.width, child_alloc.height);

      gtk_widget_size_allocate (child, &child_alloc);
    }

  /* recalculate size with higher precise */
  if (alloc_size != box->size_alloc)
    {
      box->size_alloc = alloc_size;
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}



static void
systray_box_add (GtkContainer *container,
                 GtkWidget    *child)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (container);

  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));
  panel_return_if_fail (GTK_IS_WIDGET (child));
  panel_return_if_fail (gtk_widget_get_parent (child) == NULL);

  box->children = g_slist_insert_sorted_with_data (box->children, child,
                                                    systray_box_compare_function,
                                                    box);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  gtk_widget_queue_resize (GTK_WIDGET (container));
}



static void
systray_box_remove (GtkContainer *container,
                    GtkWidget    *child)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (container);
  GSList     *li;

  /* search the child */
  li = g_slist_find (box->children, child);
  if (G_LIKELY (li != NULL))
    {
      panel_assert (GTK_WIDGET (li->data) == child);

      /* unparent widget */
      box->children = g_slist_remove_link (box->children, li);
      gtk_widget_unparent (child);

      /* resize, so we update has-hidden */
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}



static void
systray_box_forall (GtkContainer *container,
                    gboolean      include_internals,
                    GtkCallback   callback,
                    gpointer      callback_data)
{
  SystrayBox *box = XFCE_SYSTRAY_BOX (container);
  GSList     *li, *lnext;

  /* run callback for all children */
  for (li = box->children; li != NULL; li = lnext)
    {
      lnext = li->next;
      (*callback) (GTK_WIDGET (li->data), callback_data);
    }
}



static GType
systray_box_child_type (GtkContainer *container)

{
  return GTK_TYPE_WIDGET;
}



static gint
systray_box_compare_function (gconstpointer a,
                              gconstpointer b,
                              gpointer      user_data)
{
  SystrayBox  *box = user_data;
  const gchar *name_a, *name_b;
  gint         index_a = -1, index_b = -1;
  gboolean     hidden_a, hidden_b;
  gpointer     value;

  /* sort hidden icons before visible ones */
  hidden_a = systray_socket_get_hidden (XFCE_SYSTRAY_SOCKET (a));
  hidden_b = systray_socket_get_hidden (XFCE_SYSTRAY_SOCKET (b));
  if (hidden_a != hidden_b)
    return hidden_a ? 1 : -1;

  name_a = systray_socket_get_name (XFCE_SYSTRAY_SOCKET (a));
  name_b = systray_socket_get_name (XFCE_SYSTRAY_SOCKET (b));

  if (name_a != NULL && g_hash_table_lookup_extended (box->names_ordered, name_a, NULL, &value))
    index_a = GPOINTER_TO_INT (value);
  if (name_b != NULL && g_hash_table_lookup_extended (box->names_ordered, name_b, NULL, &value))
    index_b = GPOINTER_TO_INT (value);

  /* sort ordered icons before unordered ones */
  if ((index_a >= 0) != (index_b >= 0))
    return index_a >= 0 ? 1 : -1;

  /* sort ordered icons by index */
  if (index_a >= 0 && index_b >= 0)
    return index_a - index_b;

  /* sort unordered icons by name */
  return g_strcmp0 (name_a, name_b);
}



GtkWidget *
systray_box_new (void)
{
  return g_object_new (XFCE_TYPE_SYSTRAY_BOX, NULL);
}



void
systray_box_set_orientation (SystrayBox     *box,
                             GtkOrientation  orientation)
{
  gboolean horizontal;

  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));

  horizontal = !!(orientation == GTK_ORIENTATION_HORIZONTAL);
  if (G_LIKELY (box->horizontal != horizontal))
    {
      box->horizontal = horizontal;

      if (box->children != NULL)
        gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}



void
systray_box_set_dimensions (SystrayBox *box,
                            gint        icon_size,
                            gint        n_rows,
                            gint        row_size,
                            gint        padding)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));

  if (G_UNLIKELY (icon_size == box->size_max && n_rows == box->nrows && row_size == box->row_size && padding == box->row_padding))
    {
      return;
    }

  box->size_max = icon_size;
  box->nrows = n_rows;
  box->row_size = row_size;
  box->row_padding = padding;

  if (box->children != NULL)
    gtk_widget_queue_resize (GTK_WIDGET (box));
}



void
systray_box_set_size_alloc (SystrayBox *box,
                            gint        size_alloc)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));

  if (G_LIKELY (size_alloc != box->size_alloc))
    {
      box->size_alloc_init = size_alloc;
      box->size_alloc = size_alloc;

      if (box->children != NULL)
        gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}



void
systray_box_set_show_hidden (SystrayBox *box,
                             gboolean    show_hidden)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));

  if (box->show_hidden != show_hidden)
    {
      box->show_hidden = show_hidden;

      if (box->children != NULL)
        gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}



gboolean
systray_box_get_show_hidden (SystrayBox *box)
{
  panel_return_val_if_fail (XFCE_IS_SYSTRAY_BOX (box), FALSE);

  return box->show_hidden;
}



void
systray_box_set_squared (SystrayBox *box,
                         gboolean    square_icons)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));

  if (box->square_icons != square_icons)
    {
      box->square_icons = square_icons;

      if (box->children != NULL)
        gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}



gboolean
systray_box_get_squared (SystrayBox *box)
{
  panel_return_val_if_fail (XFCE_IS_SYSTRAY_BOX (box), FALSE);

  return box->square_icons;
}



void
systray_box_update (SystrayBox *box,
                    GSList     *names_ordered)
{
  GSList *li;
  gint    i;

  panel_return_if_fail (XFCE_IS_SYSTRAY_BOX (box));

  g_hash_table_remove_all (box->names_ordered);

  for (li = names_ordered, i = 0; li != NULL; li = li->next, i++)
    g_hash_table_replace (box->names_ordered, g_strdup (li->data), GINT_TO_POINTER (i));

  box->children = g_slist_sort_with_data (box->children,
                                           systray_box_compare_function,
                                           box);

  /* update the box, so we update the has-hidden property */
  gtk_widget_queue_resize (GTK_WIDGET (box));
}



gboolean
systray_box_has_hidden_items (SystrayBox *box)
{
  g_return_val_if_fail (XFCE_IS_SYSTRAY_BOX (box), FALSE);
  return box->n_hidden_children > 0;
}


void
systray_box_set_single_row (SystrayBox *box,
                            gboolean    single_row)
{
  box->single_row = single_row;
  gtk_widget_queue_resize (GTK_WIDGET (box));
}
