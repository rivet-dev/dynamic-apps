/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 * Copyright (c) 2012 Stephan Arts <stephan@xfce.org>
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 * Modified by Stephan Arts <stephan@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "util.h"
#include "file.h"
#include "thumbnailer.h"
#include "icon_bar.h"
#include "main_window.h"

#include <glib/gi18n.h>



enum
{
    PROP_0,
    PROP_ORIENTATION,
    PROP_MODEL,
    PROP_ACTIVE,
    PROP_SHOW_TEXT,
    PROP_SCROLLED_WINDOW
};

enum
{
    SELECTION_CHANGED,
    LAST_SIGNAL,
};

static guint icon_bar_signals[LAST_SIGNAL];

typedef struct _RsttoIconBarItem RsttoIconBarItem;



static void
rstto_icon_bar_finalize (GObject *object);
static void
rstto_icon_bar_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec);
static void
rstto_icon_bar_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec);


static void
rstto_icon_bar_realize (GtkWidget *widget);
static void
rstto_icon_bar_unrealize (GtkWidget *widget);
static void
rstto_icon_bar_get_preferred_width (GtkWidget *widget,
                                    gint *minimal_width,
                                    gint *natural_width);
static void
rstto_icon_bar_get_preferred_height (GtkWidget *widget,
                                     gint *minimal_height,
                                     gint *natural_height);
static void
rstto_icon_bar_size_allocate (GtkWidget *widget,
                              GtkAllocation *allocation);
static gboolean
rstto_icon_bar_draw (GtkWidget *widget,
                     cairo_t *cr);
static gboolean
rstto_icon_bar_enter (GtkWidget *widget,
                      GdkEventCrossing *event);
static gboolean
rstto_icon_bar_leave (GtkWidget *widget,
                      GdkEventCrossing *event);
static gboolean
rstto_icon_bar_motion (GtkWidget *widget,
                       GdkEventMotion *event);
static gboolean
rstto_icon_bar_button_press (GtkWidget *widget,
                             GdkEventButton *event);
static gboolean
rstto_icon_bar_button_release (GtkWidget *widget,
                               GdkEventButton *event);
static void
rstto_icon_bar_destroy (GtkWidget *widget);


static void
rstto_icon_bar_set_scale_factor (RsttoIconBar *icon_bar);
static void
rstto_icon_bar_size_request (GtkWidget *widget,
                             GtkRequisition *requisition);
static gboolean
rstto_icon_bar_scroll (GtkWidget *widget,
                       GdkEventScroll *event);

static RsttoIconBarItem *
rstto_icon_bar_get_item_at_pos (RsttoIconBar *icon_bar,
                                gint x,
                                gint y);
static void
rstto_icon_bar_queue_draw_item (RsttoIconBar *icon_bar,
                                RsttoIconBarItem *item);
static void
rstto_icon_bar_paint_item (RsttoIconBar *icon_bar,
                           RsttoIconBarItem *item,
                           cairo_t *cr);
static void
cb_rstto_thumbnail_size_changed (GObject *settings,
                                 GParamSpec *pspec,
                                 gpointer user_data);
static void
cb_rstto_icon_bar_adjustment_changed (GtkAdjustment *adjustment,
                                      RsttoIconBar *icon_bar);
static void
rstto_icon_bar_update_missing_icon (RsttoIconBar *icon_bar);


static RsttoIconBarItem *
rstto_icon_bar_item_new (void);
static void
rstto_icon_bar_item_free (gpointer item);
static void
rstto_icon_bar_build_items (RsttoIconBar *icon_bar);
static void
rstto_icon_bar_row_changed (GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            RsttoIconBar *icon_bar);
static void
rstto_icon_bar_row_inserted (GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             RsttoIconBar *icon_bar);
static void
rstto_icon_bar_row_deleted (GtkTreeModel *model,
                            GtkTreePath *path,
                            RsttoIconBar *icon_bar);
static void
rstto_icon_bar_rows_reordered (GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gint *new_order,
                               RsttoIconBar *icon_bar);
static void
rstto_icon_bar_list_sorted (RsttoImageList *list,
                            RsttoIconBar *icon_bar);
static void
rstto_icon_bar_list_remove_all (RsttoImageList *list,
                                RsttoIconBar *icon_bar);



struct _RsttoIconBarItem
{
    GtkTreeIter iter;
    gint index;
};

struct _RsttoIconBarPrivate
{
    GdkWindow      *bin_window;
    GtkWidget      *s_window;

    gint            width;
    gint            height;

    RsttoIconBarItem *active_item;
    RsttoIconBarItem *single_click_item;
    RsttoIconBarItem *cursor_item;

    GList          *items;
    gint            item_size;
    gint            n_visible_items;

    GtkAdjustment  *hadjustment;
    GtkAdjustment  *vadjustment;

    RsttoSettings  *settings;
    RsttoThumbnailer *thumbnailer;

    RsttoThumbnailSize thumbnail_size;
    GdkPixbuf *thumbnail_missing;

    GtkOrientation  orientation;

    GtkTreeModel   *model;

    gboolean        show_text;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoIconBar, rstto_icon_bar, GTK_TYPE_CONTAINER)



static void
rstto_icon_bar_class_init (RsttoIconBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = rstto_icon_bar_finalize;
    gobject_class->get_property = rstto_icon_bar_get_property;
    gobject_class->set_property = rstto_icon_bar_set_property;

    gtkwidget_class->realize = rstto_icon_bar_realize;
    gtkwidget_class->unrealize = rstto_icon_bar_unrealize;
    gtkwidget_class->get_preferred_width = rstto_icon_bar_get_preferred_width;
    gtkwidget_class->get_preferred_height = rstto_icon_bar_get_preferred_height;
    gtkwidget_class->size_allocate = rstto_icon_bar_size_allocate;
    gtkwidget_class->draw = rstto_icon_bar_draw;
    gtkwidget_class->enter_notify_event = rstto_icon_bar_enter;
    gtkwidget_class->leave_notify_event = rstto_icon_bar_leave;
    gtkwidget_class->motion_notify_event = rstto_icon_bar_motion;
    gtkwidget_class->button_press_event = rstto_icon_bar_button_press;
    gtkwidget_class->button_release_event = rstto_icon_bar_button_release;
    gtkwidget_class->destroy = rstto_icon_bar_destroy;

    /**
     * RsttoIconBar:orientation:
     *
     * The orientation of the icon bar.
     *
     * Default value: %GTK_ORIENTATION_VERTICAL
     **/
    g_object_class_install_property (gobject_class,
            PROP_ORIENTATION,
            g_param_spec_enum ("orientation",
                _("Orientation"),
                _("The orientation of the iconbar"),
                GTK_TYPE_ORIENTATION,
                GTK_ORIENTATION_VERTICAL,
                G_PARAM_READWRITE));

    /**
     * RsttoIconBar:model:
     *
     * The model for the icon bar.
     **/
    g_object_class_install_property (gobject_class,
            PROP_MODEL,
            g_param_spec_object ("model",
                _("Icon Bar Model"),
                _("Model for the icon bar"),
                GTK_TYPE_TREE_MODEL,
                G_PARAM_READWRITE));

    /**
     * RsttoIconBar:active:
     *
     * The item which is currently active.
     *
     * Allowed values: >= -1
     *
     * Default value: -1
     **/
    g_object_class_install_property (gobject_class,
            PROP_ACTIVE,
            g_param_spec_int ("active",
                _("Active"),
                _("Active item index"),
                -1, G_MAXINT, -1,
                G_PARAM_READWRITE));

    /**
     * RsttoIconBar:show-text:
     *
     * Show text under icon.
     *
     * Allowed values: TRUE, FALSE
     *
     * Default value: TRUE
     **/
    g_object_class_install_property (gobject_class,
            PROP_SHOW_TEXT,
            g_param_spec_boolean ("show-text",
                _("Show Text"),
                _("Show Text"),
                TRUE,
                G_PARAM_READWRITE));

    /**
     * RsttoIconBar:s_window:
     *
     * The scrolled window icon bar is placed into.
     **/
    g_object_class_install_property (gobject_class,
            PROP_SCROLLED_WINDOW,
            g_param_spec_object ("scrolled-window",
                _("Scrolled window"),
                _("Scrolled window icon bar is placed into"),
                GTK_TYPE_WIDGET,
                G_PARAM_READWRITE));

    gtk_widget_class_install_style_property (gtkwidget_class,
            g_param_spec_boxed ("active-item-fill-color",
                _("Active item fill color"),
                _("Active item fill color"),
                GDK_TYPE_RGBA,
                G_PARAM_READABLE));

    gtk_widget_class_install_style_property (gtkwidget_class,
            g_param_spec_boxed ("active-item-border-color",
                _("Active item border color"),
                _("Active item border color"),
                GDK_TYPE_RGBA,
                G_PARAM_READABLE));

    gtk_widget_class_install_style_property (gtkwidget_class,
            g_param_spec_boxed ("active-item-text-color",
                _("Active item text color"),
                _("Active item text color"),
                GDK_TYPE_RGBA,
                G_PARAM_READABLE));

    gtk_widget_class_install_style_property (gtkwidget_class,
            g_param_spec_boxed ("cursor-item-fill-color",
                _("Cursor item fill color"),
                _("Cursor item fill color"),
                GDK_TYPE_RGBA,
                G_PARAM_READABLE));

    gtk_widget_class_install_style_property (gtkwidget_class,
            g_param_spec_boxed ("cursor-item-border-color",
                _("Cursor item border color"),
                _("Cursor item border color"),
                GDK_TYPE_RGBA,
                G_PARAM_READABLE));

    gtk_widget_class_install_style_property (gtkwidget_class,
            g_param_spec_boxed ("cursor-item-text-color",
                _("Cursor item text color"),
                _("Cursor item text color"),
                GDK_TYPE_RGBA,
                G_PARAM_READABLE));

    /**
     * RsttoIconBar::selection-changed:
     * @icon_bar  : The #RsttoIconBar.
     *
     * This signal is emitted whenever the currently selected icon
     * changes.
     **/
    icon_bar_signals[SELECTION_CHANGED] =
        g_signal_new ("selection-changed",
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}



static void
rstto_icon_bar_init (RsttoIconBar *icon_bar)
{
    icon_bar->priv = rstto_icon_bar_get_instance_private (icon_bar);

    /* if the icon bar is hidden, this applies to the window icon, or other atomic
     * thumbnail request */
    icon_bar->priv->n_visible_items = 1;

    icon_bar->priv->orientation = GTK_ORIENTATION_VERTICAL;
    icon_bar->priv->show_text = TRUE;
    icon_bar->priv->settings = rstto_settings_new ();
    icon_bar->priv->thumbnailer = rstto_thumbnailer_new ();

    icon_bar->priv->thumbnail_size = rstto_settings_get_uint_property (
            icon_bar->priv->settings,
            "thumbnail-size");

    gtk_widget_set_can_focus (GTK_WIDGET (icon_bar), FALSE);

    g_signal_connect (icon_bar->priv->settings, "notify::thumbnail-size",
                      G_CALLBACK (cb_rstto_thumbnail_size_changed), icon_bar);
    g_signal_connect_swapped (icon_bar->priv->settings, "notify::bgcolor",
                              G_CALLBACK (gtk_widget_queue_draw), icon_bar);
    g_signal_connect_swapped (icon_bar->priv->settings, "notify::bgcolor-override",
                              G_CALLBACK (gtk_widget_queue_draw), icon_bar);
    rstto_icon_bar_set_scale_factor (icon_bar);
    g_signal_connect (icon_bar, "notify::scale-factor",
                      G_CALLBACK (rstto_icon_bar_set_scale_factor), NULL);
}



static void
rstto_icon_bar_destroy (GtkWidget *widget)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);

    rstto_icon_bar_set_model (icon_bar, NULL);

    GTK_WIDGET_CLASS (rstto_icon_bar_parent_class)->destroy (widget);
}



static void
rstto_icon_bar_finalize (GObject *object)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (object);

    g_object_unref (icon_bar->priv->settings);
    g_object_unref (icon_bar->priv->thumbnailer);
    if (icon_bar->priv->thumbnail_missing != NULL)
        g_object_unref (icon_bar->priv->thumbnail_missing);

    G_OBJECT_CLASS (rstto_icon_bar_parent_class)->finalize (object);
}



static void
rstto_icon_bar_get_property (
        GObject    *object,
        guint       prop_id,
        GValue     *value,
        GParamSpec *pspec)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (object);

    switch (prop_id)
    {
        case PROP_ORIENTATION:
            g_value_set_enum (value, icon_bar->priv->orientation);
            break;

        case PROP_MODEL:
            g_value_set_object (value, icon_bar->priv->model);
            break;

        case PROP_ACTIVE:
            g_value_set_int (value, rstto_icon_bar_get_active (icon_bar));
            break;

        case PROP_SHOW_TEXT:
            g_value_set_boolean (value, rstto_icon_bar_get_show_text (icon_bar));
            break;

        case PROP_SCROLLED_WINDOW:
            g_value_set_object (value, icon_bar->priv->s_window);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
rstto_icon_bar_set_property (
        GObject      *object,
        guint         prop_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (object);
    GtkScrolledWindow *swindow;

    switch (prop_id)
    {
        case PROP_ORIENTATION:
            rstto_icon_bar_set_orientation (icon_bar, g_value_get_enum (value));
            break;

        case PROP_MODEL:
            rstto_icon_bar_set_model (icon_bar, g_value_get_object (value));
            break;

        case PROP_ACTIVE:
            rstto_icon_bar_set_active (icon_bar, g_value_get_int (value));
            break;

        case PROP_SHOW_TEXT:
            rstto_icon_bar_set_show_text (icon_bar, g_value_get_boolean (value));
            break;

        case PROP_SCROLLED_WINDOW:
            icon_bar->priv->s_window = g_value_get_object (value);
            swindow = GTK_SCROLLED_WINDOW (icon_bar->priv->s_window);
            icon_bar->priv->hadjustment = gtk_scrolled_window_get_hadjustment (swindow);
            icon_bar->priv->vadjustment = gtk_scrolled_window_get_vadjustment (swindow);
            g_signal_connect_swapped (swindow, "scroll-event",
                                      G_CALLBACK (rstto_icon_bar_scroll), icon_bar);
            g_signal_connect_swapped (icon_bar->priv->hadjustment, "value-changed",
                                      G_CALLBACK (gtk_widget_queue_draw), icon_bar);
            g_signal_connect_swapped (icon_bar->priv->vadjustment, "value-changed",
                                      G_CALLBACK (gtk_widget_queue_draw), icon_bar);
            g_signal_connect (icon_bar->priv->hadjustment, "changed",
                              G_CALLBACK (cb_rstto_icon_bar_adjustment_changed), icon_bar);
            g_signal_connect (icon_bar->priv->vadjustment, "changed",
                              G_CALLBACK (cb_rstto_icon_bar_adjustment_changed), icon_bar);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
rstto_icon_bar_set_scale_factor (RsttoIconBar *icon_bar)
{
    GtkWidget *widget = GTK_WIDGET (icon_bar);
    GtkRequisition requisition;
    RsttoIconBarItem *item;
    RsttoFile *file;
    RsttoThumbnailFlavor flavor;

    /* reset thumbnail states */
    for (GList *lp = icon_bar->priv->items; lp != NULL; lp = lp->next)
    {
        item = lp->data;
        gtk_tree_model_get (icon_bar->priv->model, &item->iter, 0, &file, -1);
        for (flavor = RSTTO_THUMBNAIL_FLAVOR_NORMAL; flavor < RSTTO_THUMBNAIL_FLAVOR_COUNT; flavor++)
            rstto_file_set_thumbnail_state (file, flavor, RSTTO_THUMBNAIL_STATE_UNPROCESSED);
    }

    /* scale the thumbnails with the rest of the window */
    rstto_util_set_scale_factor (gtk_widget_get_scale_factor (widget));
    rstto_icon_bar_update_missing_icon (icon_bar);
    rstto_icon_bar_size_request (widget, &requisition);

    gtk_widget_queue_resize (widget);
}



static void
rstto_icon_bar_realize (GtkWidget *widget)
{
    GdkWindowAttr  attributes;
    RsttoIconBar  *icon_bar = RSTTO_ICON_BAR (widget);
    gint           attributes_mask;
    GtkAllocation  allocation;
    GdkWindow     *window;

    gtk_widget_set_realized (widget, TRUE);

    gtk_widget_get_allocation (widget, &allocation);

    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.event_mask = gtk_widget_get_events (widget)
            | GDK_EXPOSURE_MASK
            | GDK_VISIBILITY_NOTIFY_MASK;
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);
    gdk_window_set_user_data (window, widget);

    attributes.x = 0;
    attributes.y = 0;
    attributes.width = MAX (icon_bar->priv->width, allocation.width);
    attributes.height = MAX (icon_bar->priv->height, allocation.height);
    attributes.event_mask = (GDK_SCROLL_MASK
            | GDK_EXPOSURE_MASK
            | GDK_ENTER_NOTIFY_MASK
            | GDK_LEAVE_NOTIFY_MASK
            | GDK_POINTER_MOTION_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK
            | GDK_KEY_PRESS_MASK
            | GDK_KEY_RELEASE_MASK)
            | gtk_widget_get_events (widget);
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    icon_bar->priv->bin_window = gdk_window_new (window, &attributes, attributes_mask);
    gdk_window_set_user_data (icon_bar->priv->bin_window, widget);
    gdk_window_show (icon_bar->priv->bin_window);
}



static void
rstto_icon_bar_unrealize (GtkWidget *widget)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);

    if (icon_bar->priv->bin_window)
    {
        gdk_window_set_user_data (icon_bar->priv->bin_window, NULL);
        gdk_window_destroy (icon_bar->priv->bin_window);
        icon_bar->priv->bin_window = NULL;
    }

    /* GtkWidget::unrealize destroys children and widget->window */
    GTK_WIDGET_CLASS (rstto_icon_bar_parent_class)->unrealize (widget);
}



static void
rstto_icon_bar_size_request (
        GtkWidget      *widget,
        GtkRequisition *requisition)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);
    gint focus_width, focus_pad, n_items;

    if (icon_bar->priv->model == NULL || icon_bar->priv->items == NULL)
    {
        icon_bar->priv->width = requisition->width = 0;
        icon_bar->priv->height = requisition->height = 0;
        icon_bar->priv->item_size = 0;

        return;
    }

    gtk_widget_style_get (widget, "focus-line-width", &focus_width,
                          "focus-padding", &focus_pad, NULL);

    /* calculate item size: there is a focus padding both inside and outside the item */
    icon_bar->priv->item_size = 2 * (focus_width + 2 * focus_pad)
                                + rstto_util_get_thumbnail_n_pixels (icon_bar->priv->thumbnail_size)
                                  / gtk_widget_get_scale_factor (widget);

    n_items = rstto_image_list_get_n_images (RSTTO_IMAGE_LIST (icon_bar->priv->model));
    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
        icon_bar->priv->width = requisition->width = icon_bar->priv->item_size;
        icon_bar->priv->height = requisition->height = n_items * icon_bar->priv->item_size;
    }
    else
    {
        icon_bar->priv->width = requisition->width = n_items * icon_bar->priv->item_size;
        icon_bar->priv->height = requisition->height = icon_bar->priv->item_size;
    }
}
static void
rstto_icon_bar_get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width)
{
    GtkRequisition requisition;

    rstto_icon_bar_size_request (widget, &requisition);
    *minimal_width = *natural_width = requisition.width;
}
static void
rstto_icon_bar_get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height)
{
    GtkRequisition requisition;

    rstto_icon_bar_size_request (widget, &requisition);
    *minimal_height = *natural_height = requisition.height;
}



static void
rstto_icon_bar_size_allocate (GtkWidget *widget,
                              GtkAllocation *allocation)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);

    gtk_widget_set_allocation (widget, allocation);

    if (!icon_bar->priv->active_item)
        g_warning ("thumbnail bar shown when no images are available");

    if (gtk_widget_get_realized (widget))
    {
        gdk_window_move_resize (gtk_widget_get_window (widget), allocation->x, allocation->y,
                                allocation->width, allocation->height);
        gdk_window_resize (icon_bar->priv->bin_window, allocation->width, allocation->height);
    }
}



static gboolean
rstto_icon_bar_draw (GtkWidget *widget,
                     cairo_t *cr)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);
    GtkAdjustment *adjustment;
    GdkRectangle rect;
    GList *lp;
    gint first_visible, last_visible, offset, n_items, n;

    rstto_util_paint_background_color (widget, icon_bar->priv->settings, cr);

    gdk_cairo_get_clip_rectangle (cr, &rect);
    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
        adjustment = icon_bar->priv->vadjustment;
        offset = rect.y / icon_bar->priv->item_size;
        n_items = rect.height / icon_bar->priv->item_size + 2;
    }
    else
    {
        adjustment = icon_bar->priv->hadjustment;
        offset = rect.x / icon_bar->priv->item_size;
        n_items = rect.width / icon_bar->priv->item_size + 2;
    }

    /* the right limits for the visible area are given by the adjustemts, and this is
     * what we use to set the thumbnailer queue size */
    first_visible = gtk_adjustment_get_value (adjustment) / icon_bar->priv->item_size;
    last_visible = first_visible + icon_bar->priv->n_visible_items - 1;
    offset = MIN (MAX (offset, first_visible), last_visible);
    n_items = MIN (n_items, last_visible - offset + 1);

    /* skip items before the drawing area */
    for (lp = icon_bar->priv->items, n = 0; lp != NULL && n < offset; lp = lp->next, n++);

    /* only draw items in the drawing area, skip those who are after */
    for (n = 0; lp != NULL && n < n_items; lp = lp->next, n++)
        rstto_icon_bar_paint_item (icon_bar, lp->data, cr);

    return TRUE;
}



static void
rstto_icon_bar_update_cursor_item (RsttoIconBar *icon_bar,
                                   RsttoIconBarItem *item)
{
    if (icon_bar->priv->cursor_item != NULL)
        rstto_icon_bar_queue_draw_item (icon_bar, icon_bar->priv->cursor_item);

    icon_bar->priv->cursor_item = item;
    if (icon_bar->priv->cursor_item != NULL)
        rstto_icon_bar_queue_draw_item (icon_bar, icon_bar->priv->cursor_item);
}



static gboolean
rstto_icon_bar_enter (GtkWidget *widget,
                      GdkEventCrossing *event)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);
    RsttoIconBarItem *item;

    item = rstto_icon_bar_get_item_at_pos (icon_bar, event->x, event->y);
    rstto_icon_bar_update_cursor_item (icon_bar, item);

    return FALSE;
}



static gboolean
rstto_icon_bar_leave (GtkWidget *widget,
                      GdkEventCrossing *event)
{
    rstto_icon_bar_update_cursor_item (RSTTO_ICON_BAR (widget), NULL);

    return FALSE;
}



static gboolean
rstto_icon_bar_motion (GtkWidget *widget,
                       GdkEventMotion *event)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);
    RsttoIconBarItem *item;

    item = rstto_icon_bar_get_item_at_pos (icon_bar, event->x, event->y);
    rstto_icon_bar_update_cursor_item (icon_bar, item);

    return FALSE;
}

static gboolean
rstto_icon_bar_scroll (GtkWidget *widget,
                       GdkEventScroll *event)
{
    RsttoIconBar *icon_bar = RSTTO_ICON_BAR (widget);
    RsttoIconBarItem *item;
    GtkAdjustment *adjustment;
    gdouble val, old_val, step_size, max_value;

    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
        adjustment = icon_bar->priv->vadjustment;
    else
        adjustment = icon_bar->priv->hadjustment;

    step_size = icon_bar->priv->item_size / 2.0;
    val = old_val = gtk_adjustment_get_value (adjustment);
    max_value = gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment);

    switch (event->direction)
    {
        case GDK_SCROLL_UP:
        case GDK_SCROLL_LEFT:
            val -= step_size;
            val = MAX (val, 0);
            break;
        case GDK_SCROLL_DOWN:
        case GDK_SCROLL_RIGHT:
            val += step_size;
            val = MIN (val, max_value);
            break;

        default: /* GDK_SCROLL_SMOOTH */
            if (event->delta_y < 0)
            {
                val -= step_size;
                val = MAX (val, 0);
            }
            else if (event->delta_y > 0)
            {
                val += step_size;
                val = MIN (val, max_value);
            }
            break;
    }

    gtk_adjustment_set_value (adjustment, val);
    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
        item = rstto_icon_bar_get_item_at_pos (icon_bar, event->x, event->y + (val - old_val));
    else
        item = rstto_icon_bar_get_item_at_pos (icon_bar, event->x + (val - old_val), event->y);

    rstto_icon_bar_update_cursor_item (icon_bar, item);

    return TRUE;
}

static gboolean
rstto_icon_bar_button_press (
        GtkWidget      *widget,
        GdkEventButton *event)
{
    RsttoIconBar *icon_bar;
    RsttoIconBarItem *item;

    icon_bar = RSTTO_ICON_BAR (widget);

    if (G_UNLIKELY (!gtk_widget_has_focus (widget)))
        gtk_widget_grab_focus (widget);

    if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
        item = rstto_icon_bar_get_item_at_pos (icon_bar, event->x, event->y);
        icon_bar->priv->single_click_item = item;
    }
    return TRUE;
}

static gboolean
rstto_icon_bar_button_release (
        GtkWidget      *widget,
        GdkEventButton *event)
{
    RsttoIconBar *icon_bar;
    RsttoIconBarItem *item;

    icon_bar = RSTTO_ICON_BAR (widget);

    if (event->button == 1 && event->type == GDK_BUTTON_RELEASE)
    {
        item = rstto_icon_bar_get_item_at_pos (icon_bar, event->x, event->y);
        if (G_LIKELY (item != NULL && item != icon_bar->priv->active_item && item == icon_bar->priv->single_click_item))
            rstto_icon_bar_set_active (icon_bar, item->index);
    }
    return TRUE;
}



static RsttoIconBarItem *
rstto_icon_bar_get_item_at_pos (
        RsttoIconBar *icon_bar,
        gint          x,
        gint          y)
{
    GList *lp;

    if (G_UNLIKELY (icon_bar->priv->item_size == 0))
        return NULL;

    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
        lp = g_list_nth (icon_bar->priv->items, y / icon_bar->priv->item_size);
    else
        lp = g_list_nth (icon_bar->priv->items, x / icon_bar->priv->item_size);

    return (lp != NULL) ? lp->data : NULL;
}



static void
rstto_icon_bar_queue_draw_item (
        RsttoIconBar     *icon_bar,
        RsttoIconBarItem *item)
{
    GdkRectangle area;

    if (gtk_widget_get_realized (GTK_WIDGET (icon_bar)))
    {
        if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
            area.x = 0;
            area.y = icon_bar->priv->item_size * item->index;
        }
        else
        {
            area.x = icon_bar->priv->item_size * item->index;
            area.y = 0;
        }

        area.height = area.width = icon_bar->priv->item_size;

        gdk_window_invalidate_rect (icon_bar->priv->bin_window, &area, TRUE);
    }
}



static void
rstto_icon_bar_paint_item (
        RsttoIconBar     *icon_bar,
        RsttoIconBarItem *item,
        cairo_t          *cr)
{
    RsttoFile       *file;
    const GdkPixbuf *pixbuf = NULL;
    GdkRGBA         *border_color, *fill_color;
    GdkRGBA          tmp_color;
    gdouble          px, py, offset, size;
    gint             x, y, focus_width, focus_pad, scale_factor;
    gint             pixbuf_width = 0, pixbuf_height = 0;

    if (icon_bar->priv->model == NULL)
        return;

    gtk_widget_style_get (GTK_WIDGET (icon_bar),
            "focus-line-width", &focus_width,
            "focus-padding", &focus_pad,
            NULL);
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (icon_bar));

    gtk_tree_model_get (icon_bar->priv->model, &item->iter, 0, &file, -1);
    pixbuf = rstto_file_get_thumbnail (file, icon_bar->priv->thumbnail_size);

    if (NULL == pixbuf)
        pixbuf = icon_bar->priv->thumbnail_missing;

    if (pixbuf)
    {
        pixbuf_width = gdk_pixbuf_get_width (pixbuf) / scale_factor;
        pixbuf_height = gdk_pixbuf_get_height (pixbuf) / scale_factor;
    }

    /* calculate pixbuf / layout location */
    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
        x = 0;
        y = icon_bar->priv->item_size * item->index;

        px = (icon_bar->priv->item_size - pixbuf_width) / 2.0;
        py = y + (icon_bar->priv->item_size - pixbuf_height) / 2.0;
    }
    else
    {
        x = icon_bar->priv->item_size * item->index;
        y = 0;

        px = x + (icon_bar->priv->item_size - pixbuf_width) / 2.0;
        py = (icon_bar->priv->item_size - pixbuf_height) / 2.0;
    }

    if (icon_bar->priv->active_item == item || icon_bar->priv->cursor_item == item)
    {
        if (icon_bar->priv->active_item == item)
        {
            gtk_widget_style_get (GTK_WIDGET (icon_bar),
                    "active-item-fill-color", &fill_color,
                    "active-item-border-color", &border_color,
                    NULL);

            if (fill_color == NULL)
            {
                gdk_rgba_parse (&tmp_color, "#c1d2ee");
                fill_color = gdk_rgba_copy (&tmp_color);
            }

            if (border_color == NULL)
            {
                gdk_rgba_parse (&tmp_color, "#316ac5");
                border_color = gdk_rgba_copy (&tmp_color);
            }
        }
        else
        {
            gtk_widget_style_get (GTK_WIDGET (icon_bar),
                    "cursor-item-fill-color", &fill_color,
                    "cursor-item-border-color", &border_color,
                    NULL);

            if (fill_color == NULL)
            {
                gdk_rgba_parse (&tmp_color, "#e0e8f6");
                fill_color = gdk_rgba_copy (&tmp_color);
            }

            if (border_color == NULL)
            {
                gdk_rgba_parse (&tmp_color, "#98b4e2");
                border_color = gdk_rgba_copy (&tmp_color);
            }
        }

        cairo_save (cr);

        cairo_set_source_rgb (cr, fill_color->red, fill_color->green, fill_color->blue);
        offset = focus_pad + focus_width;
        size = icon_bar->priv->item_size - 2 * offset;
        cairo_rectangle (cr, x + offset, y + offset, size, size);
        cairo_fill (cr);

        cairo_set_source_rgb (cr, border_color->red, border_color->green, border_color->blue);
        cairo_set_line_width (cr, focus_width);
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);

        offset = focus_pad + focus_width / 2.0;
        size = icon_bar->priv->item_size - 2 * offset;
        cairo_rectangle (cr, x + offset, y + offset, size, size);
        cairo_stroke (cr);

        cairo_restore (cr);

        gdk_rgba_free (border_color);
        gdk_rgba_free (fill_color);
    }

    if (NULL != pixbuf)
    {
        cairo_surface_t *surface;

        cairo_save (cr);
        rstto_util_set_source_pixbuf (cr, pixbuf, px, py);
        cairo_pattern_get_surface (cairo_get_source (cr), &surface);
        cairo_surface_set_device_scale (surface, scale_factor, scale_factor);
        cairo_paint (cr);
        cairo_restore (cr);
    }
}



static RsttoIconBarItem *
rstto_icon_bar_item_new (void)
{
    return g_slice_new0 (RsttoIconBarItem);
}



static void
rstto_icon_bar_item_free (gpointer item)
{
    g_slice_free (RsttoIconBarItem, item);
}



static void
rstto_icon_bar_build_items (RsttoIconBar *icon_bar)
{
    RsttoIconBarItem *item;
    GtkTreeIter     iter;
    GList          *items = NULL;
    gint            i = 0;

    if (!gtk_tree_model_get_iter_first (icon_bar->priv->model, &iter))
        return;

    do
    {
        item = rstto_icon_bar_item_new ();
        item->iter = iter;
        item->index = i++;

        items = g_list_prepend (items, item);
    }
    while (gtk_tree_model_iter_next (icon_bar->priv->model, &iter));

    icon_bar->priv->items = g_list_reverse (items);
}



static void
rstto_icon_bar_row_changed (
        GtkTreeModel *model,
        GtkTreePath  *path,
        GtkTreeIter  *iter,
        RsttoIconBar *icon_bar)
{
    RsttoIconBarItem  *item;
    gint             idx;

    idx = gtk_tree_path_get_indices (path)[0];
    item = g_list_nth_data (icon_bar->priv->items, idx);

    /* the image list is loaded by file blocks, whereas the icon bar is filled at the
     * end of this process, we can therefore receive wrong information during this time */
    if (item == NULL)
        return;

    rstto_icon_bar_queue_draw_item (icon_bar, item);
}



static void
rstto_icon_bar_row_inserted (
        GtkTreeModel *model,
        GtkTreePath  *path,
        GtkTreeIter  *iter,
        RsttoIconBar *icon_bar)
{
    RsttoIconBarItem *item;
    GList *lp;

    item = rstto_icon_bar_item_new ();
    item->iter = *iter;
    item->index = gtk_tree_path_get_indices (path)[0];

    icon_bar->priv->items = g_list_insert (icon_bar->priv->items, item, item->index);

    for (lp = g_list_nth (icon_bar->priv->items, item->index + 1); lp != NULL; lp = lp->next)
    {
        item = lp->data;
        item->index++;
    }

    gtk_widget_queue_resize (GTK_WIDGET (icon_bar));
}



static void
rstto_icon_bar_row_deleted (
        GtkTreeModel *model,
        GtkTreePath  *path,
        RsttoIconBar *icon_bar)
{
    RsttoIconBarItem *item;
    gboolean        active = FALSE;
    GList          *lnext;
    GList          *lp;
    gint            idx;

    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));

    idx = gtk_tree_path_get_indices (path)[0];
    lp = g_list_nth (icon_bar->priv->items, idx);

    /* the image list is loaded by file blocks, whereas the icon bar is filled at the
     * end of this process, we can therefore receive wrong information during this time */
    if (lp == NULL)
        return;

    item = lp->data;
    if (item == icon_bar->priv->active_item)
    {
        icon_bar->priv->active_item = NULL;
        active = TRUE;
    }

    if (item == icon_bar->priv->cursor_item)
        icon_bar->priv->cursor_item = NULL;

    rstto_icon_bar_item_free (item);

    for (lnext = lp->next; lnext != NULL; lnext = lnext->next)
    {
        item = lnext->data;
        item->index--;
    }

    icon_bar->priv->items = g_list_delete_link (icon_bar->priv->items, lp);

    if (active && icon_bar->priv->items != NULL)
        icon_bar->priv->active_item = icon_bar->priv->items->data;

    gtk_widget_queue_resize (GTK_WIDGET (icon_bar));

    if (active)
        rstto_icon_bar_set_active (icon_bar, -1);
}


static void
rstto_icon_bar_rows_reordered (
        GtkTreeModel *model,
        GtkTreePath  *path,
        GtkTreeIter  *iter,
        gint         *new_order,
        RsttoIconBar *icon_bar)
{
    RsttoIconBarItem **item_array;
    GList           *items = NULL;
    GList           *lp;
    gint            *inverted_order;
    gint             length;
    gint             i;

    length = gtk_tree_model_iter_n_children (model, NULL);
    inverted_order = g_newa (gint, length);
    item_array = g_newa (RsttoIconBarItem *, length);

    /* initialize arrays for security and so that static analysis tools like scan-build
     * do not report warnings */
    for (i = 0; i < length; ++i)
    {
        inverted_order[i] = 0;
        item_array[i] = icon_bar->priv->items->data;
    }

    /* invert the array */
    for (i = 0; i < length; ++i)
        inverted_order[new_order[i]] = i;

    for (i = 0, lp = icon_bar->priv->items; i < length && lp != NULL; ++i, lp = lp->next)
        item_array[inverted_order[i]] = lp->data;

    for (i = 0; i < length; ++i)
    {
        item_array[i]->index = i;
        items = g_list_prepend (items, item_array[i]);
    }

    g_list_free (icon_bar->priv->items);
    icon_bar->priv->items = g_list_reverse (items);

    gtk_widget_queue_draw (GTK_WIDGET (icon_bar));
}



static void
rstto_icon_bar_list_sorted (RsttoImageList *list,
                            RsttoIconBar *icon_bar)
{
    RsttoImageListIter *iter;
    GtkWidget *window;
    gint idx;

    /* get the current image index */
    window = gtk_widget_get_ancestor (GTK_WIDGET (icon_bar), RSTTO_TYPE_MAIN_WINDOW);
    iter = rstto_main_window_get_iter (RSTTO_MAIN_WINDOW (window));
    idx = rstto_image_list_iter_get_position (iter);

    /* reload the list to reflect the new sorting order */
    rstto_icon_bar_set_model (icon_bar, NULL);
    rstto_icon_bar_set_model (icon_bar, GTK_TREE_MODEL (list));

    /* re-select the current image */
    rstto_icon_bar_set_active (icon_bar, idx);
}



static void
rstto_icon_bar_list_remove_all (RsttoImageList *list,
                                RsttoIconBar *icon_bar)
{
    /* reload empty list */
    rstto_icon_bar_set_model (icon_bar, NULL);
    rstto_icon_bar_set_model (icon_bar, GTK_TREE_MODEL (list));
}



/**
 * rstto_icon_bar_new:
 * @s_window : A #GtkScrolledWindow.
 *
 * Creates a new #RsttoIconBar without model.
 *
 * Returns: a newly allocated #RsttoIconBar.
 **/
GtkWidget *
rstto_icon_bar_new (GtkWidget *s_window)
{
    g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (s_window), NULL);

    return g_object_new (RSTTO_TYPE_ICON_BAR,
            "scrolled-window", s_window,
            NULL);
}



/**
 * rstto_icon_bar_new_with_model:
 * @model : A #GtkTreeModel.
 *
 * Creates a new #RsttoIconBar and associates it with
 * @model.
 *
 * Returns: a newly allocated #RsttoIconBar, which is associated with @model.
 **/
GtkWidget *
rstto_icon_bar_new_with_model (GtkTreeModel *model)
{
    g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

    return g_object_new (RSTTO_TYPE_ICON_BAR,
            "model", model,
            NULL);
}



/**
 * rstto_icon_bar_get_model:
 * @icon_bar  : A #RsttoIconBar.
 *
 * Returns the model the #RsttoIconBar is based on. Returns %NULL if
 * the model is unset.
 *
 * Returns: A #GtkTreeModel, or %NULL if none is currently being used.
 **/
GtkTreeModel *
rstto_icon_bar_get_model (RsttoIconBar *icon_bar)
{
    g_return_val_if_fail (RSTTO_IS_ICON_BAR (icon_bar), NULL);
    return icon_bar->priv->model;
}



/**
 * rstto_icon_bar_set_model:
 * @icon_bar  : A #RsttoIconBar.
 * @model     : A #GtkTreeModel or %NULL.
 *
 * Sets the model for a #RsttoIconBar. If the @icon_bar already has a model
 * set, it will remove it before settings the new model. If @model is %NULL,
 * then it will unset the old model.
 **/
void
rstto_icon_bar_set_model (
        RsttoIconBar *icon_bar,
        GtkTreeModel *model)
{
    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));
    g_return_if_fail (GTK_IS_TREE_MODEL (model) || model == NULL);

    if (G_UNLIKELY (model == icon_bar->priv->model))
        return;

    if (icon_bar->priv->model)
    {
        g_signal_handlers_disconnect_by_func (icon_bar->priv->model,
                rstto_icon_bar_row_changed,
                icon_bar);
        g_signal_handlers_disconnect_by_func (icon_bar->priv->model,
                rstto_icon_bar_row_inserted,
                icon_bar);
        g_signal_handlers_disconnect_by_func (icon_bar->priv->model,
                rstto_icon_bar_row_deleted,
                icon_bar);
        g_signal_handlers_disconnect_by_func (icon_bar->priv->model,
                rstto_icon_bar_rows_reordered,
                icon_bar);
        g_signal_handlers_disconnect_by_func (icon_bar->priv->model,
                rstto_icon_bar_list_sorted,
                icon_bar);
        g_signal_handlers_disconnect_by_func (icon_bar->priv->model,
                rstto_icon_bar_list_remove_all,
                icon_bar);

        g_object_unref (icon_bar->priv->model);

        g_list_free_full (icon_bar->priv->items, rstto_icon_bar_item_free);
        icon_bar->priv->active_item = NULL;
        icon_bar->priv->cursor_item = NULL;
        icon_bar->priv->items = NULL;
    }

    icon_bar->priv->model = model;

    if (model != NULL)
    {
        g_object_ref (model);

        g_signal_connect (model, "row-changed",
                G_CALLBACK (rstto_icon_bar_row_changed), icon_bar);
        g_signal_connect (model, "row-inserted",
                G_CALLBACK (rstto_icon_bar_row_inserted), icon_bar);
        g_signal_connect (model, "row-deleted",
                G_CALLBACK (rstto_icon_bar_row_deleted), icon_bar);
        g_signal_connect (model, "rows-reordered",
                G_CALLBACK (rstto_icon_bar_rows_reordered), icon_bar);
        g_signal_connect (model, "sorted",
                G_CALLBACK (rstto_icon_bar_list_sorted), icon_bar);
        g_signal_connect (model, "remove-all",
                G_CALLBACK (rstto_icon_bar_list_remove_all), icon_bar);

        rstto_icon_bar_build_items (icon_bar);
    }

    gtk_widget_queue_resize (GTK_WIDGET (icon_bar));

    g_object_notify (G_OBJECT (icon_bar), "model");
}


/**
 * rstto_icon_bar_get_orientation:
 * @icon_bar  : An #RsttoIconBar.
 *
 * Retrieves the current orientation of the toolbar. See
 * rstto_icon_bar_set_orientation ().
 *
 * Returns: The orientation of @icon_bar.
 **/
GtkOrientation
rstto_icon_bar_get_orientation (RsttoIconBar *icon_bar)
{
    g_return_val_if_fail (RSTTO_IS_ICON_BAR (icon_bar), GTK_ORIENTATION_VERTICAL);
    return icon_bar->priv->orientation;
}



/**
 * rstto_icon_bar_set_orientation:
 * @icon_bar    : An #RsttoIconBar.
 * @orientation : A new #GtkOrientation.
 *
 * Sets whether the @icon_bar should appear horizontally
 * or vertically.
 **/
void
rstto_icon_bar_set_orientation (
        RsttoIconBar    *icon_bar,
        GtkOrientation   orientation)
{
    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));

    if (icon_bar->priv->orientation != orientation)
    {
        /* Unset the cursor-item */
        icon_bar->priv->cursor_item = NULL;

        icon_bar->priv->orientation = orientation;
        gtk_widget_queue_resize (GTK_WIDGET (icon_bar));

        g_object_notify (G_OBJECT (icon_bar), "orientation");
    }
}



/**
 * rstto_icon_bar_get_active:
 * @icon_bar  : An #RsttoIconBar.
 *
 * Returns the index of the currently active item, or -1 if there's no
 * active item.
 *
 * Returns: An integer which is the index of the currently active item,
 *          or -1 if there's no active item.
 **/
gint
rstto_icon_bar_get_active (RsttoIconBar *icon_bar)
{
    g_return_val_if_fail (RSTTO_IS_ICON_BAR (icon_bar), -1);

    return (icon_bar->priv->active_item != NULL)
        ? icon_bar->priv->active_item->index
        : -1;
}



/**
 * rstto_icon_bar_set_active:
 * @icon_bar  : An #RsttoIconBar.
 * @idx       : An index in the model passed during construction,
 *              or -1 to have no active item.
 *
 * Sets the active item of @icon_bar to be the item at @idx.
 **/
void
rstto_icon_bar_set_active (
        RsttoIconBar *icon_bar,
        gint          idx)
{
    GList *item;

    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));

    if ((idx >= 0 && (item = g_list_nth (icon_bar->priv->items, idx)) == NULL)
        || (icon_bar->priv->active_item == NULL && idx == -1)
        || (icon_bar->priv->active_item != NULL && idx == icon_bar->priv->active_item->index))
        return;

    if (G_LIKELY (idx >= 0))
        icon_bar->priv->active_item = item->data;
    else
        icon_bar->priv->active_item = NULL;

    g_signal_emit (icon_bar, icon_bar_signals[SELECTION_CHANGED], 0);
    g_object_notify (G_OBJECT (icon_bar), "active");
    gtk_widget_queue_draw (GTK_WIDGET (icon_bar));
}



/**
 * rstto_icon_bar_get_active_iter:
 * @icon_bar  : An #RsttoIconBar.
 * @iter      : An uninitialized #GtkTreeIter.
 *
 * Sets @iter to point to the current active item, if it exists.
 *
 * Returns: %TRUE if @iter was set.
 **/
gboolean
rstto_icon_bar_get_active_iter (
        RsttoIconBar  *icon_bar,
        GtkTreeIter   *iter)
{
    RsttoIconBarItem *item;

    g_return_val_if_fail (RSTTO_IS_ICON_BAR (icon_bar), FALSE);
    g_return_val_if_fail (iter != NULL, FALSE);

    item = icon_bar->priv->active_item;
    if (item == NULL)
        return FALSE;

    *iter = item->iter;

    return TRUE;
}



/**
 * rstto_icon_bar_set_active_iter:
 * @icon_bar  : An #RsttoIconBar.
 * @iter      : The #GtkTreeIter.
 *
 * Sets the current active item to be the one referenced by @iter. @iter
 * must correspond to a path of depth one.
 *
 * This can only be called if @icon_bar is associated with #GtkTreeModel.
 **/
void
rstto_icon_bar_set_active_iter (
        RsttoIconBar *icon_bar,
        GtkTreeIter  *iter)
{
    GtkTreePath *path;

    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));
    g_return_if_fail (icon_bar->priv->model != NULL);
    g_return_if_fail (iter != NULL);

    path = gtk_tree_model_get_path (icon_bar->priv->model, iter);
    if (G_LIKELY (path != NULL))
    {
        rstto_icon_bar_set_active (icon_bar, gtk_tree_path_get_indices (path)[0]);
        gtk_tree_path_free (path);
    }
}

gint
rstto_icon_bar_get_n_visible_items (RsttoIconBar *icon_bar)
{
    return icon_bar->priv->n_visible_items;
}

/**
 * rstto_icon_bar_set_show_text:
 * @icon_bar  : An #RsttoIconBar.
 * @show_text : TRUE if text should be visible
 *              or FALSE if text is hidden
 *
 * Toggles the visibility of the text-label
 **/
void
rstto_icon_bar_set_show_text (
        RsttoIconBar *icon_bar,
        gboolean show_text)
{
    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));
    icon_bar->priv->show_text = show_text;
}

/**
 * rstto_icon_bar_get_show_text:
 * @icon_bar  : An #RsttoIconBar.
 *
 * Returns: TRUE if text is visible.
 **/
gboolean
rstto_icon_bar_get_show_text (RsttoIconBar  *icon_bar)
{
    g_return_val_if_fail (RSTTO_IS_ICON_BAR (icon_bar), FALSE);
    return icon_bar->priv->show_text;
}

/**
 * rstto_icon_bar_show_active:
 * @icon_bar  : An #RsttoIconBar.
 **/
void
rstto_icon_bar_show_active (RsttoIconBar *icon_bar)
{
    GtkAdjustment *adjustment;

    g_return_if_fail (RSTTO_IS_ICON_BAR (icon_bar));

    if (icon_bar->priv->active_item == NULL)
        return;

    if (icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL)
        adjustment = icon_bar->priv->vadjustment;
    else
        adjustment = icon_bar->priv->hadjustment;

    gtk_adjustment_set_value (adjustment, (icon_bar->priv->active_item->index + 0.5)
                                          * icon_bar->priv->item_size
                                          - gtk_adjustment_get_page_size (adjustment) / 2);
}

static void
cb_rstto_thumbnail_size_changed (
        GObject *settings,
        GParamSpec *pspec,
        gpointer user_data)
{
    RsttoIconBar *icon_bar = user_data;

    g_object_get (settings, "thumbnail-size", &icon_bar->priv->thumbnail_size, NULL);

    rstto_icon_bar_update_missing_icon (icon_bar);
    gtk_widget_queue_resize (GTK_WIDGET (icon_bar));
}

static void
cb_rstto_icon_bar_adjustment_changed (GtkAdjustment *adjustment,
                                      RsttoIconBar *icon_bar)
{
    gdouble page_size, upper;

    static gdouble s_page_size = 0, s_upper = 0;

    if ((icon_bar->priv->orientation == GTK_ORIENTATION_VERTICAL
         && adjustment != icon_bar->priv->vadjustment)
        || (icon_bar->priv->orientation == GTK_ORIENTATION_HORIZONTAL
            && adjustment != icon_bar->priv->hadjustment))
        return;

    page_size = gtk_adjustment_get_page_size (adjustment);
    upper = gtk_adjustment_get_upper (adjustment);

    /* center on the active item when there is a real size change: with the allocation mode
     * applied in get_preferred_width/height(), 'page_size' corresponds to the visible part
     * of the icon bar, while 'upper' corresponds to its allocated size */
    if (s_page_size != page_size || s_upper != upper)
    {
        s_page_size = page_size;
        s_upper = upper;
        icon_bar->priv->n_visible_items = 2 + page_size / icon_bar->priv->item_size;
        rstto_icon_bar_show_active (icon_bar);
    }
}

static void
rstto_icon_bar_update_missing_icon (RsttoIconBar *icon_bar)
{
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (icon_bar));

    if (icon_bar->priv->thumbnail_missing != NULL)
        g_object_unref (icon_bar->priv->thumbnail_missing);

    icon_bar->priv->thumbnail_missing =
        gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (), "image-missing",
                                            rstto_util_get_thumbnail_n_pixels (icon_bar->priv->thumbnail_size) / scale_factor,
                                            scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
}
