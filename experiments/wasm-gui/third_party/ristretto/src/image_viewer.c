/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "util.h"
#include "image_viewer.h"
#include "main_window.h"

#include <math.h>



/*
 * A buffer size of 1 MiB makes it possible to load a lot of current images in only
 * a few iterations, which can significantly improve performance for some formats
 * like GIF, without being too big, which on the contrary would degrade performance.
 * On the other hand, it ensures that a cancelled load will end fairly quickly at
 * the end of the current iteration, instead of continuing almost indefinitely as
 * can be the case for some images, again in GIF format.
 * See https://gitlab.xfce.org/apps/ristretto/-/issues/16 for an example of such an
 * image.
 */
#define LOADER_BUFFER_SIZE 1048576

#define BACKGROUND_ICON_SIZE 128

#define MAX_OVER_VISIBLE 1.5
#define MIN_VIEW_PERCENT 0.1
#define RSTTO_LINE_WIDTH 1.0

#define ERROR_UNDETERMINED "Undetermined error"

enum
{
    PROP_0,
    PROP_SHOW_CLOCK,

    /* For scrollable interface */
    PROP_HADJUSTMENT,
    PROP_VADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY
};

typedef enum
{
    RSTTO_IMAGE_VIEWER_MOTION_STATE_NORMAL = 0,
    RSTTO_IMAGE_VIEWER_MOTION_STATE_BOX_ZOOM,
    RSTTO_IMAGE_VIEWER_MOTION_STATE_MOVE
} RsttoImageViewerMotionState;

typedef struct _RsttoImageViewerTransaction RsttoImageViewerTransaction;



#define RSTTO_GET_CLOCK_WIDTH(w, h) (40 + MIN (w, h) * 0.07)
#define RSTTO_GET_CLOCK_OFFSET(w)   (w * 0.15);

static void
rstto_image_viewer_finalize (GObject *object);
static void
rstto_image_viewer_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec);
static void
rstto_image_viewer_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec);


static gboolean
rstto_image_viewer_draw (GtkWidget *widget,
                         cairo_t *ctx);
static void
rstto_image_viewer_realize (GtkWidget *widget);
static void
rstto_image_viewer_get_preferred_width (GtkWidget *widget,
                                        gint *minimal_width,
                                        gint *natural_width);
static void
rstto_image_viewer_get_preferred_height (GtkWidget *widget,
                                         gint *minimal_height,
                                         gint *natural_height);
static void
rstto_image_viewer_size_allocate (GtkWidget *widget,
                                  GtkAllocation *allocation);
static gboolean
rstto_scroll_event (GtkWidget *widget,
                    GdkEventScroll *event);
static gboolean
rstto_button_press_event (GtkWidget *widget,
                          GdkEventButton *event);
static gboolean
rstto_button_release_event (GtkWidget *widget,
                            GdkEventButton *event);
static gboolean
rstto_motion_notify_event (GtkWidget *widget,
                           GdkEventMotion *event);
static gboolean
rstto_popup_menu (GtkWidget *widget);


static gboolean
set_scale (RsttoImageViewer *viewer,
           gdouble scale);
static void
set_scale_factor (RsttoImageViewer *viewer,
                  GParamSpec *pspec,
                  gpointer data);
static void
set_adjustments (RsttoImageViewer *viewer,
                 gdouble h_value,
                 gdouble v_value);
static gboolean
compute_selection_box_dimensions (RsttoImageViewer *viewer,
                                  gdouble *box_x,
                                  gdouble *box_y,
                                  gdouble *box_width,
                                  gdouble *box_height);
static void
paint_background_icon (RsttoImageViewer *viewer,
                       GdkPixbuf *pixbuf,
                       gdouble alpha,
                       cairo_t *ctx);
static void
rstto_image_viewer_paint (GtkWidget *widget,
                          cairo_t *ctx);

static void
cb_rstto_image_viewer_file_changed (RsttoFile *r_file,
                                    RsttoImageViewer *viewer);
static void
rstto_image_viewer_set_motion_state (RsttoImageViewer *viewer,
                                     RsttoImageViewerMotionState state);
static void
cb_rstto_image_viewer_read_file_ready (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data);
static void
cb_rstto_image_viewer_read_input_stream_ready (GObject *source_object,
                                               GAsyncResult *result,
                                               gpointer user_data);
static void
cb_rstto_image_loader_image_ready (GdkPixbufLoader *loader,
                                   RsttoImageViewerTransaction *transaction);
static void
cb_rstto_image_loader_size_prepared (GdkPixbufLoader *loader,
                                     gint width,
                                     gint height,
                                     RsttoImageViewerTransaction *transaction);
static void
cb_rstto_image_loader_closed (GdkPixbufLoader *loader,
                              RsttoImageViewerTransaction *transaction);
static gboolean
cb_rstto_image_viewer_update_pixbuf (gpointer user_data);
static void
cb_rstto_image_viewer_dnd (GtkWidget *widget,
                           GdkDragContext *context,
                           gint x,
                           gint y,
                           GtkSelectionData *data,
                           guint info,
                           guint time_,
                           RsttoImageViewer *viewer);
static void
cb_rstto_limit_quality_changed (GObject *settings,
                                GParamSpec *pspec,
                                gpointer user_data);
static void
cb_rstto_bgcolor_changed (GObject *settings,
                          GParamSpec *pspec,
                          gpointer user_data);
static void
cb_rstto_zoom_direction_changed (GObject *settings,
                                 GParamSpec *pspec,
                                 gpointer user_data);
static void
rstto_image_viewer_load_image (RsttoImageViewer *viewer,
                               RsttoFile *file,
                               gdouble scale);
static void
rstto_image_viewer_transaction_free (gpointer data);



struct _RsttoImageViewerTransaction
{
    RsttoImageViewer *viewer;
    RsttoFile        *file;
    GCancellable     *cancellable;
    GdkPixbufLoader  *loader;
    guint             loader_closed_id;

    GError           *error;

    gint              image_width;
    gint              image_height;
    gdouble           image_scale;
    gdouble           scale;

    /*
     * GDK uses windowing system specific APIs to retrieve this, and X11 requires
     * special processing to be used in multi-threaded context. So this must be
     * retrieved before we start asynchronous image loading, so that we don't have
     * to distinguish between windowing systems, or use X11-specific APIs in our code.
     */
    gint              monitor_width;
    gint              monitor_height;

    /* File I/O data */
    /*****************/
    guchar           *buffer;
    gssize            read_bytes;
};

struct _RsttoImageViewerPrivate
{
    RsttoFile                   *file;
    RsttoSettings               *settings;

    GtkIconTheme                *icon_theme;
    GdkPixbuf                   *missing_icon;
    GdkPixbuf                   *bg_icon;

    gboolean                     limit_quality;

    GError                      *error;

    guint                        hscroll_policy : 1;
    guint                        vscroll_policy : 1;

    RsttoImageViewerTransaction *transaction;
    RsttoImageOrientation        orientation;
    gchar                      **excluded_mime_types;
    struct
    {
        cairo_pattern_t *pattern;
        gboolean has_alpha;
        gint width;
        gint height;
    } pixbuf;

    struct
    {
        gint x_offset;
        gint y_offset;
        gint width;
        gint height;
    } rendering;

    struct
    {
        gboolean show_clock;
    } props;

    GtkMenu                     *menu;
    gboolean                     invert_zoom_direction;

    /* Animation data for animated images (like .gif/.mng) */
    /*******************************************************/
    GdkPixbufAnimationIter *iter;
    guint                   animation_id;
    gdouble                 scale;
    RsttoScale              auto_scale;
    GtkAdjustment          *vadjustment;
    GtkAdjustment          *hadjustment;

    gdouble                 image_scale;
    gint                    original_image_width;
    gint                    original_image_height;
    gint                    image_width;
    gint                    image_height;

    struct
    {
        gdouble x;
        gdouble y;
        gdouble current_x;
        gdouble current_y;
        gint h_val;
        gint v_val;
        RsttoImageViewerMotionState state;
    } motion;
};



G_DEFINE_TYPE_WITH_CODE (RsttoImageViewer, rstto_image_viewer, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (RsttoImageViewer)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))



static void
rstto_image_viewer_init (RsttoImageViewer *viewer)
{
    GSList *list, *li;
    gchar **strv;
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (viewer));

    viewer->priv = rstto_image_viewer_get_instance_private (viewer);
    viewer->priv->settings = rstto_settings_new ();
    viewer->priv->pixbuf.pattern = NULL;
    viewer->priv->animation_id = 0;
    viewer->priv->scale = RSTTO_SCALE_NONE;
    viewer->priv->auto_scale = RSTTO_SCALE_NONE;
    viewer->priv->image_width = viewer->priv->original_image_width = 0;
    viewer->priv->image_height = viewer->priv->original_image_height = 0;

    viewer->priv->icon_theme = gtk_icon_theme_get_default ();
    viewer->priv->bg_icon = gtk_icon_theme_load_icon_for_scale (
            viewer->priv->icon_theme,
            RISTRETTO_APP_ID,
            BACKGROUND_ICON_SIZE,
            scale_factor,
            GTK_ICON_LOOKUP_FORCE_SIZE,
            NULL);
    viewer->priv->missing_icon = gtk_icon_theme_load_icon_for_scale (
            viewer->priv->icon_theme,
            "image-missing",
            BACKGROUND_ICON_SIZE,
            scale_factor,
            GTK_ICON_LOOKUP_FORCE_SIZE,
            NULL);
    if (viewer->priv->bg_icon != NULL)
    {
        gdk_pixbuf_saturate_and_pixelate (
                viewer->priv->bg_icon,
                viewer->priv->bg_icon,
                0, FALSE);
    }

    g_signal_connect (viewer->priv->settings, "notify::bgcolor",
                      G_CALLBACK (cb_rstto_bgcolor_changed), viewer);
    g_signal_connect (viewer->priv->settings, "notify::bgcolor-override",
                      G_CALLBACK (cb_rstto_bgcolor_changed), viewer);
    g_signal_connect (viewer->priv->settings, "notify::limit-quality",
                      G_CALLBACK (cb_rstto_limit_quality_changed), viewer);
    g_signal_connect (viewer->priv->settings, "notify::invert-zoom-direction",
                      G_CALLBACK (cb_rstto_zoom_direction_changed), viewer);
    g_signal_connect (viewer, "drag-data-received",
                      G_CALLBACK (cb_rstto_image_viewer_dnd), viewer);
    g_signal_connect (viewer, "notify::scale-factor", G_CALLBACK (set_scale_factor), NULL);

    gtk_widget_set_events (GTK_WIDGET (viewer),
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_BUTTON1_MOTION_MASK |
                           GDK_ENTER_NOTIFY_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_SCROLL_MASK);

    gtk_drag_dest_set (GTK_WIDGET (viewer), GTK_DEST_DEFAULT_ALL, NULL, 0,
                      GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_PRIVATE);
    gtk_drag_dest_add_uri_targets (GTK_WIDGET (viewer));

    /*
     * Mime types for which we will use the default pixbuf loader, instead of the
     * mime-type-specific one, typically because it is broken. This is different from using
     * gdk_pixbuf_format_set_disabled(), which would totally disable the corresponding library,
     * whereas some of its features could still be used here.
     * This should apply to as few mime types as possible, but all RAW types should be excluded
     * for now, see https://gitlab.freedesktop.org/libopenraw/libopenraw/-/issues/7
    */
    viewer->priv->excluded_mime_types = NULL;
    list = gdk_pixbuf_get_formats ();
    for (li = list; li != NULL; li = li->next)
    {
        /* not sure we can count on the stability of the format name or description, so better
         * to look for a mime type that we know must be present */
        strv = gdk_pixbuf_format_get_mime_types (li->data);
        if (g_strv_contains ((const gchar *const *) strv, "image/x-canon-cr2"))
        {
            viewer->priv->excluded_mime_types = strv;
            break;
        }
        else
            g_strfreev (strv);
    }

    g_slist_free (list);
}

/**
 * rstto_image_viewer_class_init:
 * @viewer_class:
 *
 * Initialize imageviewer class
 */
static void
rstto_image_viewer_class_init (RsttoImageViewerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = rstto_image_viewer_finalize;
    object_class->set_property = rstto_image_viewer_set_property;
    object_class->get_property = rstto_image_viewer_get_property;

    widget_class->draw = rstto_image_viewer_draw;
    widget_class->realize = rstto_image_viewer_realize;
    widget_class->get_preferred_width = rstto_image_viewer_get_preferred_width;
    widget_class->get_preferred_height = rstto_image_viewer_get_preferred_height;
    widget_class->size_allocate = rstto_image_viewer_size_allocate;
    widget_class->scroll_event = rstto_scroll_event;
    widget_class->button_press_event = rstto_button_press_event;
    widget_class->button_release_event = rstto_button_release_event;
    widget_class->motion_notify_event = rstto_motion_notify_event;
    widget_class->popup_menu = rstto_popup_menu;

    g_signal_new (
            "size-ready",
            G_TYPE_FROM_CLASS (object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    g_signal_new ("scale-changed",
            G_TYPE_FROM_CLASS (object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    g_signal_new ("status-changed",
            G_TYPE_FROM_CLASS (object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    g_signal_new ("files-dnd",
            G_TYPE_FROM_CLASS (object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE, 1,
            G_TYPE_POINTER);

    g_object_class_install_property (
            G_OBJECT_CLASS (object_class),
            PROP_SHOW_CLOCK,
            g_param_spec_boolean (
                    "show-clock",
                    "",
                    "",
                    FALSE,
                    G_PARAM_READWRITE));

    /* Scrollable interface properties */
    g_object_class_override_property (
            object_class,
            PROP_HADJUSTMENT,
            "hadjustment");
    g_object_class_override_property (
            object_class,
            PROP_VADJUSTMENT,
            "vadjustment");
    g_object_class_override_property (
            object_class,
            PROP_HSCROLL_POLICY,
            "hscroll-policy");
    g_object_class_override_property (
            object_class,
            PROP_VSCROLL_POLICY,
            "vscroll-policy");
}

/**
 * rstto_image_viewer_realize:
 * @widget:
 *
 */
static void
rstto_image_viewer_realize (GtkWidget *widget)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);
    GtkAllocation allocation;
    GdkWindowAttr attributes;
    GdkWindow *window;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (RSTTO_IS_IMAGE_VIEWER (widget));

    gtk_widget_set_realized (widget, TRUE);

    gtk_widget_get_allocation (widget, &allocation);
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;
    attributes.visual = gtk_widget_get_visual (widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
    window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);
    gdk_window_set_user_data (window, widget);
    g_object_ref (window);

    g_object_get (viewer->priv->settings, "limit-quality",
                  &(viewer->priv->limit_quality), NULL);
    g_object_get (viewer->priv->settings, "invert-zoom-direction",
                  &(viewer->priv->invert_zoom_direction), NULL);
}

/**
 * rstto_image_viewer_get_preferred_width / height:
 *
 * Request a default size of 300 by 400 pixels
 */
static void
rstto_image_viewer_get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width)
{
    *minimal_width = 100;
    *natural_width = 400;
}

static void
rstto_image_viewer_get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height)
{
    *minimal_height = 100;
    *natural_height = 300;
}

/**
 * rstto_image_viewer_size_allocate:
 * @widget:
 * @allocation:
 *
 */
static void
rstto_image_viewer_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);

    gtk_widget_set_allocation (widget, allocation);

    if (gtk_widget_get_realized (widget))
    {
        gdk_window_move_resize (gtk_widget_get_window (widget), allocation->x, allocation->y,
                                allocation->width, allocation->height);

        if (viewer->priv->auto_scale != RSTTO_SCALE_NONE)
            set_scale (viewer, viewer->priv->auto_scale);

        set_adjustments (viewer,
                         gtk_adjustment_get_value (viewer->priv->hadjustment),
                         gtk_adjustment_get_value (viewer->priv->vadjustment));
    }
}

/**
 * rstto_image_viewer_draw:
 * @widget:
 * @cairo_t:
 *
 */
static gboolean
rstto_image_viewer_draw (GtkWidget *widget, cairo_t *ctx)
{
    cairo_save (ctx);

    rstto_image_viewer_paint (widget, ctx);

    cairo_restore (ctx);

    return TRUE;
}

static void
rstto_image_viewer_finalize (GObject *object)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (object);

    if (viewer->priv->transaction && viewer->priv->transaction->loader_closed_id != 0)
        g_source_remove (viewer->priv->transaction->loader_closed_id);
    if (viewer->priv->settings)
    {
        g_object_unref (viewer->priv->settings);
        viewer->priv->settings = NULL;
    }
    if (viewer->priv->bg_icon)
    {
        g_object_unref (viewer->priv->bg_icon);
        viewer->priv->bg_icon = NULL;
    }
    if (viewer->priv->missing_icon)
    {
        g_object_unref (viewer->priv->missing_icon);
        viewer->priv->missing_icon = NULL;
    }
    if (viewer->priv->pixbuf.pattern)
    {
        cairo_pattern_destroy (viewer->priv->pixbuf.pattern);
        viewer->priv->pixbuf.pattern = NULL;
    }
    if (viewer->priv->iter)
    {
        g_object_unref (viewer->priv->iter);
        viewer->priv->iter = NULL;
    }
    if (viewer->priv->excluded_mime_types)
    {
        g_strfreev (viewer->priv->excluded_mime_types);
        viewer->priv->excluded_mime_types = NULL;
    }
    if (viewer->priv->file)
    {
        g_object_unref (viewer->priv->file);
        viewer->priv->file = NULL;
    }
    if (viewer->priv->error)
    {
        g_error_free (viewer->priv->error);
        viewer->priv->error = NULL;
    }

    G_OBJECT_CLASS (rstto_image_viewer_parent_class)->finalize (object);
}

/**
 * Set scale.
 * @viewer: the image viewer structure
 * @scale: 0 means 'fit to view', 1 means 100%/1:1 scale
 *
 * Return value: FALSE if there is no scale change (already at min, max or
                 fit-to-view), TRUE otherwise
 */
static gboolean
set_scale (RsttoImageViewer *viewer, gdouble scale)
{
    RsttoScale default_zoom;
    GtkAllocation allocation;
    gdouble h_scale, v_scale, max_scale;
    gboolean auto_scale = RSTTO_SCALE_NONE;

    /* no usable image, no scale change (avoids in particular priv->file == NULL below) */
    if (viewer->priv->image_width == 0)
        return FALSE;

    gtk_widget_get_allocation (GTK_WIDGET (viewer), &allocation);

    switch (viewer->priv->orientation)
    {
        case RSTTO_IMAGE_ORIENT_90:
        case RSTTO_IMAGE_ORIENT_270:
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            h_scale = (gdouble) allocation.width / viewer->priv->image_height;
            v_scale = (gdouble) allocation.height / viewer->priv->image_width;
            break;
        case RSTTO_IMAGE_ORIENT_NONE:
        case RSTTO_IMAGE_ORIENT_180:
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
        default:
            h_scale = (gdouble) allocation.width / viewer->priv->image_width;
            v_scale = (gdouble) allocation.height / viewer->priv->image_height;
            break;
    }

    if (scale == RSTTO_SCALE_NONE)
    {
        default_zoom = rstto_settings_get_int_property (viewer->priv->settings, "default-zoom");
        if (default_zoom != RSTTO_SCALE_NONE)
        {
            scale = default_zoom;
        }
        else if (h_scale > 1 && v_scale > 1)
        {
            /* for small images fitting scale to 1:1 size */
            scale = RSTTO_SCALE_REAL_SIZE;
        }
        else
        {
            /* for large images exceeding window size, limit-to-view */
            scale = RSTTO_SCALE_LIMIT_TO_VIEW;
        }
    }

    if (scale == RSTTO_SCALE_LIMIT_TO_VIEW)
    {
        auto_scale = RSTTO_SCALE_LIMIT_TO_VIEW;
        scale = MIN (RSTTO_SCALE_REAL_SIZE, MIN (h_scale, v_scale));
    }
    else if (scale == RSTTO_SCALE_FIT_TO_VIEW)
    {
        auto_scale = RSTTO_SCALE_FIT_TO_VIEW;
        scale = MIN (h_scale, v_scale);
    }
    else if (scale < RSTTO_SCALE_REAL_SIZE)
    {
        /* minimum scale is a percent of display area, unless image is smaller */
        if (scale * MAX (viewer->priv->image_width, viewer->priv->image_height)
            < MIN_VIEW_PERCENT * MIN (allocation.width, allocation.height))
        {
            scale = viewer->priv->scale;
        }
    }
    else
    {
        /* maximum scale depends on the sizes of the display area and the image */
        max_scale = MAX (1.0,
                         MAX_OVER_VISIBLE
                         * MAX (allocation.width, allocation.height)
                         / MIN (viewer->priv->image_width, viewer->priv->image_height));
        scale = MIN (scale, max_scale);
    }

    viewer->priv->auto_scale = auto_scale;
    rstto_file_set_auto_scale (viewer->priv->file, auto_scale);
    if (viewer->priv->scale != scale)
    {
        viewer->priv->scale = scale;
        rstto_file_set_scale (viewer->priv->file, scale);
        g_signal_emit_by_name (viewer, "scale-changed");

        return TRUE;
    }

    return FALSE;
}

static void
set_scale_factor (RsttoImageViewer *viewer,
                  GParamSpec *pspec,
                  gpointer data)
{
    cairo_surface_t *surface;
    gint scale_factor;

    if (viewer->priv->image_width > 0)
    {
        /* do not scale the image with the rest of the window */
        scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (viewer));
        viewer->priv->image_width = viewer->priv->original_image_width / scale_factor;
        viewer->priv->image_height = viewer->priv->original_image_height / scale_factor;
        cairo_pattern_get_surface (viewer->priv->pixbuf.pattern, &surface);
        cairo_surface_set_device_scale (surface, scale_factor, scale_factor);

        if (pspec != NULL)
            gtk_widget_queue_resize (GTK_WIDGET (viewer));
    }
}

static void
set_adjustments (RsttoImageViewer *viewer,
                 gdouble h_value,
                 gdouble v_value)
{
    GtkAllocation alloc;
    gdouble width, height;

    gtk_widget_get_allocation (GTK_WIDGET (viewer), &alloc);

    /* if there is no usable image, reset values */
    if (viewer->priv->image_width == 0)
    {
        viewer->priv->rendering.x_offset = 0;
        viewer->priv->rendering.y_offset = 0;
        viewer->priv->rendering.width = 0;
        viewer->priv->rendering.height = 0;

        gtk_adjustment_configure (viewer->priv->hadjustment, 0, 0, 0, 0, 0, 0);
        gtk_adjustment_configure (viewer->priv->vadjustment, 0, 0, 0, 0, 0, 0);

        return;
    }

    /* set rendering geometry */
    switch (viewer->priv->orientation)
    {
        case RSTTO_IMAGE_ORIENT_90:
        case RSTTO_IMAGE_ORIENT_270:
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            width = viewer->priv->image_height * viewer->priv->scale;
            height = viewer->priv->image_width * viewer->priv->scale;
            break;
        case RSTTO_IMAGE_ORIENT_NONE:
        case RSTTO_IMAGE_ORIENT_180:
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
        default:
            width = viewer->priv->image_width * viewer->priv->scale;
            height = viewer->priv->image_height * viewer->priv->scale;
            break;
    }

    viewer->priv->rendering.width = width;
    viewer->priv->rendering.height = height;
    viewer->priv->rendering.x_offset = MAX (0, (alloc.width - width) / 2.0);
    viewer->priv->rendering.y_offset = MAX (0, (alloc.height - height) / 2.0);

    /* set adjustemts */
    gtk_adjustment_configure (viewer->priv->hadjustment, h_value, 0,
                              MAX (viewer->priv->rendering.width, alloc.width),
                              0, 0, alloc.width);
    gtk_adjustment_configure (viewer->priv->vadjustment, v_value, 0,
                              MAX (viewer->priv->rendering.height, alloc.height),
                              0, 0, alloc.height);
}

static gboolean
compute_selection_box_dimensions (RsttoImageViewer *viewer,
                                  gdouble *box_x,
                                  gdouble *box_y,
                                  gdouble *box_width,
                                  gdouble *box_height)
{
    gdouble x_offset = viewer->priv->rendering.x_offset;
    gdouble y_offset = viewer->priv->rendering.y_offset;
    gdouble image_width = viewer->priv->rendering.width;
    gdouble image_height = viewer->priv->rendering.height;

    /* a selection box can be created moving the cursor from left to right, aswell
     * as from right to left: calculate the box dimensions accordingly */
    *box_x = MIN (viewer->priv->motion.x, viewer->priv->motion.current_x);
    *box_width = MAX (viewer->priv->motion.x, viewer->priv->motion.current_x) - *box_x;

    /* same thing vertically */
    *box_y = MIN (viewer->priv->motion.y, viewer->priv->motion.current_y);
    *box_height = MAX (viewer->priv->motion.y, viewer->priv->motion.current_y) - *box_y;

    /* constrain the selection box to the left and top sides of the image */
    if (*box_x < x_offset)
    {
        *box_width -= x_offset - *box_x;
        *box_x = x_offset;
    }
    if (*box_y < y_offset)
    {
        *box_height -= y_offset - *box_y;
        *box_y = y_offset;
    }

    if (x_offset + image_width < *box_x + *box_width)
    {
        *box_width = x_offset + image_width - *box_x - 1;
    }
    if (y_offset + image_height < *box_y + *box_height)
    {
        *box_height = y_offset + image_height - *box_y - 1;
    }

    return *box_width > 0 && *box_height > 0;
}

static void
paint_background_icon (RsttoImageViewer *viewer,
                       GdkPixbuf *pixbuf,
                       gdouble alpha,
                       cairo_t *ctx)
{
    GtkAllocation alloc;
    gdouble bg_scale;

    gtk_widget_get_allocation (GTK_WIDGET (viewer), &alloc);

    /* calculate the icon-size */
    bg_scale = (alloc.width < alloc.height)
               ? 1.2 * BACKGROUND_ICON_SIZE / alloc.width
               : 1.2 * BACKGROUND_ICON_SIZE / alloc.height;

    /* move the cairo context in position so the background-image is painted in
     * the center of the widget */
    cairo_translate (ctx,
                     (alloc.width - BACKGROUND_ICON_SIZE / bg_scale) / 2.0,
                     (alloc.height - BACKGROUND_ICON_SIZE / bg_scale) / 2.0);

    /* scale the context so the image fills the same part of the cairo-context */
    cairo_scale (ctx, 1.0 / bg_scale, 1.0 / bg_scale);

    /* draw the pixbuf on the cairo-context */
    if (pixbuf != NULL)
    {
        cairo_surface_t *surface;
        gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (viewer));

        rstto_util_set_source_pixbuf (ctx, pixbuf, 0, 0);
        cairo_pattern_get_surface (cairo_get_source (ctx), &surface);
        cairo_surface_set_device_scale (surface, scale_factor, scale_factor);
        cairo_paint_with_alpha (ctx, alpha);
    }
}

static void
paint_clock (GtkWidget *widget, cairo_t *ctx)
{
    GtkAllocation allocation;
    gdouble width, height, offset;
    time_t t = time (NULL);
    struct tm *lt = localtime (&t);
    gdouble hour_angle = M_PI * 2 / 12 * (lt->tm_hour % 12 + 6 + (M_PI * 2 / 720.0 * lt->tm_min));
    gint i;

    gtk_widget_get_allocation (widget, &allocation);

    width = RSTTO_GET_CLOCK_WIDTH (allocation.width, allocation.height);
    height = width;
    offset = RSTTO_GET_CLOCK_OFFSET (width);

    cairo_save (ctx);

    cairo_translate (
        ctx,
        allocation.width - offset - width,
        allocation.height - offset - height);

    cairo_set_source_rgba (ctx, 0.0, 0.0, 0.0, 0.4);
    cairo_save (ctx);
    cairo_translate (
        ctx,
        width / 2,
        height / 2);
    cairo_arc (
        ctx,
        00, 00,
        width / 2,
        0,
        2 * M_PI);
    cairo_fill (ctx);

    cairo_set_source_rgba (ctx, 1.0, 1.0, 1.0, 0.8);

    cairo_save (ctx);
    for (i = 0; i < 12; ++i)
    {
        cairo_rotate (
            ctx,
            30 * M_PI / 180);
        cairo_arc (
            ctx,
            00, -1 * (width / 2 - 5),
            width / 20,
            0,
            2 * M_PI);
        cairo_fill (ctx);
    }
    cairo_restore (ctx);

/***/
    cairo_save (ctx);
    cairo_set_line_width (ctx, width / 15);
    cairo_set_line_cap (ctx, CAIRO_LINE_CAP_ROUND);
    cairo_rotate (
        ctx,
        hour_angle);
    cairo_move_to (ctx, 0, 0);
    cairo_line_to (
        ctx,
        0,
        height * 0.2);
    cairo_stroke (
        ctx);
    cairo_restore (ctx);
/***/
    cairo_set_line_width (ctx, width / 15);
    cairo_set_line_cap (ctx, CAIRO_LINE_CAP_ROUND);
    cairo_rotate (
        ctx,
        M_PI * 2 / 60 * (lt->tm_min % 60 + 30));
    cairo_move_to (ctx, 0, 0);
    cairo_line_to (
        ctx,
        0,
        height * 0.3);
    cairo_stroke (
        ctx);

    cairo_restore (ctx);
    cairo_restore (ctx);
}

static void
paint_image (GtkWidget *widget, cairo_t *ctx)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);
    cairo_matrix_t transform_matrix;
    gdouble x_scale, y_scale, block_width, block_height;
    gint i, j, h_adjust, v_adjust;
    gint x_offset = viewer->priv->rendering.x_offset;
    gint y_offset = viewer->priv->rendering.y_offset;
    gint width = viewer->priv->rendering.width;
    gint height = viewer->priv->rendering.height;
    gint scale_factor = gtk_widget_get_scale_factor (widget);
    gdouble nx_squares = scale_factor * width / 10.0;
    gdouble ny_squares = scale_factor * height / 10.0;
    gdouble square_size = 10.0 / scale_factor;

    cairo_save (ctx);

    /* paint checkered background */
    if (viewer->priv->pixbuf.has_alpha)
    {
        cairo_set_source_rgba (ctx, 0.8, 0.8, 0.8, 1.0);
        cairo_rectangle (ctx, x_offset, y_offset, width, height);
        cairo_fill (ctx);

        cairo_set_source_rgba (ctx, 0.7, 0.7, 0.7, 1.0);
        for (i = 0; i < nx_squares; ++i)
        {
            block_width = (i <= nx_squares - 1) ? square_size
                                                : width - floor (nx_squares) * square_size;
            for (j = (i % 2) ? 1 : 0; j < ny_squares; j += 2)
            {
                block_height = (j <= ny_squares - 1) ? square_size
                                                     : height - floor (ny_squares) * square_size;
                cairo_rectangle (ctx, x_offset + i * square_size, y_offset + j * square_size,
                                 block_width, block_height);
                cairo_fill (ctx);
            }
        }
    }

    cairo_restore (ctx);

    h_adjust = gtk_adjustment_get_value (viewer->priv->hadjustment);
    v_adjust = gtk_adjustment_get_value (viewer->priv->vadjustment);
    switch (viewer->priv->orientation)
    {
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
            cairo_translate (ctx, - h_adjust, - v_adjust);
            cairo_translate (ctx, width, 0);
            cairo_translate (ctx, x_offset, y_offset);
            cairo_matrix_init_identity (&transform_matrix);
            transform_matrix.xx = -1.0;
            cairo_transform (ctx, &transform_matrix);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
            cairo_translate (ctx, - h_adjust, - v_adjust);
            cairo_translate (ctx, 0, height);
            cairo_translate (ctx, x_offset, y_offset);
            cairo_matrix_init_identity (&transform_matrix);
            transform_matrix.yy = -1.0;
            cairo_transform (ctx, &transform_matrix);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
            cairo_rotate (ctx, M_PI * 1.5);
            cairo_translate (ctx, v_adjust, - h_adjust);
            cairo_translate (ctx, - y_offset, x_offset);
            cairo_matrix_init_identity (&transform_matrix);
            transform_matrix.xx = -1.0;
            cairo_transform (ctx, &transform_matrix);
            break;
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            cairo_rotate (ctx, M_PI * 0.5);
            cairo_translate (ctx, - v_adjust, h_adjust);
            cairo_translate (ctx, height, - width);
            cairo_translate (ctx, y_offset, - x_offset);
            cairo_matrix_init_identity (&transform_matrix);
            transform_matrix.xx = -1.0;
            cairo_transform (ctx, &transform_matrix);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            cairo_rotate (ctx, M_PI * 0.5);
            cairo_translate (ctx, - v_adjust, h_adjust);
            cairo_translate (ctx, 0, - width);
            cairo_translate (ctx, y_offset, - x_offset);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            cairo_rotate (ctx, M_PI * 1.5);
            cairo_translate (ctx, v_adjust, - h_adjust);
            cairo_translate (ctx, - height, 0);
            cairo_translate (ctx, - y_offset, x_offset);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            cairo_rotate (ctx, M_PI);
            cairo_translate (ctx, h_adjust, v_adjust);
            cairo_translate (ctx, - width, - height);
            cairo_translate (ctx, - x_offset, - y_offset);
            break;
        case RSTTO_IMAGE_ORIENT_NONE:
        default:
            cairo_translate (ctx, - h_adjust, - v_adjust);
            cairo_translate (ctx, x_offset, y_offset);
            break;
    }

    switch (viewer->priv->orientation)
    {
        case RSTTO_IMAGE_ORIENT_90:
        case RSTTO_IMAGE_ORIENT_270:
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSPOSE:
        case RSTTO_IMAGE_ORIENT_FLIP_TRANSVERSE:
            /* adjusted scales for painting, so that the painted image is strictly
             * included in the rendering rectangle, and the drawing area restriction
             * in gdk_window_invalidate_*() is really taken into account (typical
             * example where it is necessary: when redrawing the background in
             * cb_rstto_bgcolor_changed()) */
            x_scale = (1 - DBL_EPSILON) * width / viewer->priv->image_height;
            y_scale = (1 - DBL_EPSILON) * height / viewer->priv->image_width;
            break;
        case RSTTO_IMAGE_ORIENT_NONE:
        case RSTTO_IMAGE_ORIENT_180:
        case RSTTO_IMAGE_ORIENT_FLIP_HORIZONTAL:
        case RSTTO_IMAGE_ORIENT_FLIP_VERTICAL:
        default:
            x_scale = (1 - DBL_EPSILON) * width / viewer->priv->image_width;
            y_scale = (1 - DBL_EPSILON) * height / viewer->priv->image_height;
            break;
    }

    cairo_scale (ctx, x_scale / viewer->priv->image_scale, y_scale / viewer->priv->image_scale);
    cairo_set_source (ctx, viewer->priv->pixbuf.pattern);
    cairo_paint (ctx);
}

static void
paint_selection_box (GtkWidget *widget, cairo_t *ctx)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);
    gdouble box_x, box_y, box_width, box_height;

    /* make sure the box dimensions are not negative, this results in rendering-artifacts */
    if (! compute_selection_box_dimensions (viewer, &box_x, &box_y, &box_width, &box_height))
        return;

    cairo_rectangle (
        ctx,
        box_x + RSTTO_LINE_WIDTH / 2.0, box_y + RSTTO_LINE_WIDTH / 2.0,
        box_width, box_height);

    cairo_set_source_rgba (ctx, 0.9, 0.9, 0.9, 0.2);
    cairo_fill_preserve (ctx);
    cairo_set_source_rgba (ctx, 0.2, 0.2, 0.2, 1.0);
    cairo_set_line_width (ctx, RSTTO_LINE_WIDTH);
    cairo_stroke (ctx);
}

static void
rstto_image_viewer_paint (GtkWidget *widget, cairo_t *ctx)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);

    if (gtk_widget_get_realized (widget))
    {
        cairo_rectangle (
                ctx,
                0.0,
                0.0,
                gtk_widget_get_allocated_width (widget),
                gtk_widget_get_allocated_height (widget));
        cairo_clip (ctx);

        rstto_util_paint_background_color (widget, viewer->priv->settings, ctx);

        /* Check if a file should be rendered */
        /**************************************/
        if (NULL == viewer->priv->file)
        {
            cairo_save (ctx);

            /* Paint the background-image (ristretto icon) */
            /***********************************************/
            paint_background_icon (viewer, viewer->priv->bg_icon, 0.1, ctx);

            cairo_restore (ctx);

            if (viewer->priv->props.show_clock)
            {
                cairo_save (ctx);
                paint_clock (widget, ctx);
                cairo_restore (ctx);
            }
        }
        else
        {
            cairo_save (ctx);

            /* image and rendering are set */
            if (viewer->priv->image_width > 0 && viewer->priv->rendering.width > 0)
                paint_image (widget, ctx);
            else if (viewer->priv->error != NULL)
                paint_background_icon (viewer, viewer->priv->missing_icon, 1.0, ctx);

            cairo_restore (ctx);

            if (viewer->priv->motion.state == RSTTO_IMAGE_VIEWER_MOTION_STATE_BOX_ZOOM)
            {
                cairo_save (ctx);
                paint_selection_box (widget, ctx);
                cairo_restore (ctx);
            }

            if (viewer->priv->props.show_clock)
            {
                cairo_save (ctx);
                paint_clock (widget, ctx);
                cairo_restore (ctx);
            }
        }
    }
}

GtkWidget *
rstto_image_viewer_new (void)
{
    GtkWidget *widget;

    widget = g_object_new (RSTTO_TYPE_IMAGE_VIEWER, NULL);

    return widget;
}

/*
 * rstto_image_viewer_set_file:
 * @viewer:
 * @file:
 *
 * Set the file that should be displayed in the image_viewer.
 *
 * TODO:
 *  - cancellable...
 */
void
rstto_image_viewer_set_file (RsttoImageViewer *viewer,
                             RsttoFile *file,
                             gdouble scale,
                             RsttoScale auto_scale,
                             RsttoImageOrientation orientation)
{
    GtkWidget *widget = GTK_WIDGET (viewer);

    /*
     * Set the image-orientation
     */
    viewer->priv->orientation = orientation;

    /*
     * Check if a file is set, or unset.
     */
    if (file)
    {
        /*
         * Should the 'old' file be cleaned up?
         */
        if (viewer->priv->file)
        {
            /*
             * If the old, and new file are equal, do nothing.
             */
            if (viewer->priv->file != file)
            {
                /*
                 * This will first need to return to the 'main' loop before it cleans up after itself.
                 * We can forget about the transaction, once it's cancelled, it will clean-up itself. -- (it should)
                 */
                if (viewer->priv->transaction)
                {
                    if (! g_cancellable_is_cancelled (viewer->priv->transaction->cancellable))
                    {
                        g_cancellable_cancel (viewer->priv->transaction->cancellable);
                    }
                    viewer->priv->transaction = NULL;
                }

                g_object_ref (file);
                g_signal_connect (
                        file,
                        "changed",
                        G_CALLBACK (cb_rstto_image_viewer_file_changed),
                        viewer);
                g_signal_handlers_disconnect_by_func (
                        viewer->priv->file,
                        cb_rstto_image_viewer_file_changed,
                        viewer);
                g_object_unref (viewer->priv->file);

                viewer->priv->file = file;
                if (viewer->priv->error)
                {
                    g_error_free (viewer->priv->error);
                    viewer->priv->error = NULL;
                }
                viewer->priv->image_scale = 1.0;
                viewer->priv->image_width = viewer->priv->original_image_width = 0;
                viewer->priv->image_height = viewer->priv->original_image_height = 0;

                rstto_image_viewer_load_image (viewer, viewer->priv->file,
                                               auto_scale != RSTTO_SCALE_NONE ? auto_scale : scale);
            }
        }
        else
        {
            g_signal_connect (
                    file,
                    "changed",
                    G_CALLBACK (cb_rstto_image_viewer_file_changed),
                    viewer);
            g_object_ref (file);
            viewer->priv->file = file;
            rstto_image_viewer_load_image (viewer, viewer->priv->file,
                                           auto_scale != RSTTO_SCALE_NONE ? auto_scale : scale);
        }
    }
    else
    {
        if (viewer->priv->animation_id != 0)
            REMOVE_SOURCE (viewer->priv->animation_id);
        if (viewer->priv->iter)
        {
            g_object_unref (viewer->priv->iter);
            viewer->priv->iter = NULL;
        }
        if (viewer->priv->pixbuf.pattern)
        {
            cairo_pattern_destroy (viewer->priv->pixbuf.pattern);
            viewer->priv->pixbuf.pattern = NULL;
        }
        if (viewer->priv->transaction)
        {
            if (! g_cancellable_is_cancelled (viewer->priv->transaction->cancellable))
            {
                g_cancellable_cancel (viewer->priv->transaction->cancellable);
            }
            viewer->priv->transaction = NULL;
        }
        if (viewer->priv->file)
        {
            g_signal_handlers_disconnect_by_func (
                    viewer->priv->file,
                    cb_rstto_image_viewer_file_changed,
                    viewer);
            g_object_unref (viewer->priv->file);
            viewer->priv->file = NULL;

            /* Reset the image-size to 0 */
            viewer->priv->image_width = viewer->priv->original_image_width = 0;
            viewer->priv->image_height = viewer->priv->original_image_height = 0;

            set_adjustments (viewer, 0, 0);
            gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

            gtk_widget_set_tooltip_text (widget, NULL);
        }
    }
}

static void
rstto_image_viewer_load_image (RsttoImageViewer *viewer, RsttoFile *file, gdouble scale)
{
    RsttoImageViewerTransaction *transaction = g_new0 (RsttoImageViewerTransaction, 1);
    const gchar *mime_type;

    /*
     * This will first need to return to the 'main' loop before it cleans up after itself.
     * We can forget about the transaction, once it's cancelled, it will clean-up itself. -- (it should)
     */
    if (viewer->priv->transaction)
    {
        g_cancellable_cancel (viewer->priv->transaction->cancellable);
        viewer->priv->transaction = NULL;
    }

    mime_type = rstto_file_get_content_type (file);
    if (viewer->priv->excluded_mime_types == NULL
        || ! g_strv_contains ((const gchar *const *) viewer->priv->excluded_mime_types, mime_type))
        transaction->loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, NULL);

    /* fallback in case of an excluded mime type, or any error */
    if (transaction->loader == NULL)
        transaction->loader = gdk_pixbuf_loader_new ();

    transaction->cancellable = g_cancellable_new ();
    transaction->buffer = g_new0 (guchar, LOADER_BUFFER_SIZE);
    transaction->file = file;
    transaction->viewer = viewer;
    transaction->scale = scale;

    if (viewer->priv->limit_quality)
    {
        GdkMonitor *monitor;
        GdkRectangle rect;
        gint scale_factor;

        monitor = gdk_display_get_monitor_at_window (gdk_display_get_default (),
                                                     gtk_widget_get_window (GTK_WIDGET (viewer)));
        gdk_monitor_get_geometry (monitor, &rect);
        scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (viewer));
        transaction->monitor_width = rect.width * scale_factor;
        transaction->monitor_height = rect.height * scale_factor;
    }

    g_signal_connect (transaction->loader, "size-prepared", G_CALLBACK (cb_rstto_image_loader_size_prepared), transaction);
    g_signal_connect (transaction->loader, "closed", G_CALLBACK (cb_rstto_image_loader_closed), transaction);

    viewer->priv->transaction = transaction;

    g_file_read_async (rstto_file_get_file (transaction->file),
                       G_PRIORITY_DEFAULT,
                       transaction->cancellable,
                       cb_rstto_image_viewer_read_file_ready,
                       transaction);
}

static void
rstto_image_viewer_transaction_free (gpointer data)
{
    RsttoImageViewerTransaction *tr = data;

    /*
     * Check if this transaction is current,
     * if so, remove the reference from the viewer.
     */
    if (tr->viewer->priv->transaction == tr)
    {
        tr->viewer->priv->transaction = NULL;
    }
    if (tr->error)
    {
        g_error_free (tr->error);
    }
    g_object_unref (tr->cancellable);
    g_object_unref (tr->loader);
    g_free (tr->buffer);
    g_free (tr);
}

void
rstto_image_viewer_set_scale (RsttoImageViewer *viewer, gdouble scale)
{
    gdouble tmp_x, tmp_y, h_page_size, v_page_size;

    h_page_size = gtk_adjustment_get_page_size (viewer->priv->hadjustment);
    v_page_size = gtk_adjustment_get_page_size (viewer->priv->vadjustment);

    tmp_x = (gtk_adjustment_get_value (viewer->priv->hadjustment) + h_page_size / 2
             - viewer->priv->rendering.x_offset) / viewer->priv->scale;
    tmp_y = (gtk_adjustment_get_value (viewer->priv->vadjustment) + v_page_size / 2
             - viewer->priv->rendering.y_offset) / viewer->priv->scale;

    if (set_scale (viewer, scale))
    {
        set_adjustments (viewer,
                         tmp_x * viewer->priv->scale - h_page_size / 2,
                         tmp_y * viewer->priv->scale - v_page_size / 2);
        gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (viewer)), NULL, FALSE);
    }
}

GdkPixbuf *
rstto_image_viewer_get_pixbuf (RsttoImageViewer *viewer)
{
    cairo_surface_t *surface;

    if (viewer->priv->pixbuf.pattern == NULL)
        return NULL;

    cairo_pattern_get_surface (viewer->priv->pixbuf.pattern, &surface);

    return gdk_pixbuf_get_from_surface (surface, 0, 0,
                                        viewer->priv->pixbuf.width,
                                        viewer->priv->pixbuf.height);
}

gdouble
rstto_image_viewer_get_scale (RsttoImageViewer *viewer)
{
    return viewer->priv->scale;
}

static void
rstto_image_viewer_set_motion_state (RsttoImageViewer *viewer, RsttoImageViewerMotionState state)
{
    viewer->priv->motion.state = state;
}

/*
 * rstto_image_viewer_set_orientation:
 * @viewer:
 * @orientation:
 *
 * Set orientation for the image shown here.
 */
void
rstto_image_viewer_set_orientation (RsttoImageViewer *viewer, RsttoImageOrientation orientation)
{
    viewer->priv->orientation = orientation;
    rstto_file_set_orientation (viewer->priv->file, orientation);

    if (viewer->priv->auto_scale != RSTTO_SCALE_NONE)
        set_scale (viewer, viewer->priv->auto_scale);

    set_adjustments (viewer,
                     gtk_adjustment_get_value (viewer->priv->hadjustment),
                     gtk_adjustment_get_value (viewer->priv->vadjustment));
    gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (viewer)), NULL, FALSE);
}

RsttoImageOrientation
rstto_image_viewer_get_orientation (RsttoImageViewer *viewer)
{
    if (viewer)
    {
        return viewer->priv->orientation;
    }
    return RSTTO_IMAGE_ORIENT_NONE;
}

gint
rstto_image_viewer_get_width (RsttoImageViewer *viewer)
{
    if (viewer)
    {
        return viewer->priv->original_image_width;
    }
    return 0;
}
gint
rstto_image_viewer_get_height (RsttoImageViewer *viewer)
{
    if (viewer)
    {
        return viewer->priv->original_image_height;
    }
    return 0;
}

void
rstto_image_viewer_set_menu (RsttoImageViewer *viewer, GtkMenu *menu)
{
    if (viewer->priv->menu)
    {
        gtk_menu_detach (viewer->priv->menu);
        gtk_widget_destroy (GTK_WIDGET (viewer->priv->menu));
    }

    viewer->priv->menu = menu;

    if (viewer->priv->menu)
    {
        gtk_menu_attach_to_widget (
                viewer->priv->menu,
                GTK_WIDGET (viewer),
                NULL);
    }

}


/************************/
/** CALLBACK FUNCTIONS **/
/************************/
static void
cb_rstto_image_viewer_read_file_ready (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    RsttoImageViewerTransaction *transaction = user_data;
    GFile *file = G_FILE (source_object);
    GFileInputStream *file_input_stream;

    if (rstto_main_window_get_app_exited ())
        return;

    file_input_stream = g_file_read_finish (file, result, &transaction->error);
    if (file_input_stream == NULL)
    {
        gdk_pixbuf_loader_close (transaction->loader, NULL);
        return;
    }

    g_input_stream_read_async (G_INPUT_STREAM (file_input_stream),
                               transaction->buffer,
                               LOADER_BUFFER_SIZE,
                               G_PRIORITY_DEFAULT,
                               transaction->cancellable,
                               cb_rstto_image_viewer_read_input_stream_ready,
                               transaction);
}

static void
loader_write_async (GTask *task,
                    gpointer source_object,
                    gpointer task_data,
                    GCancellable *cancellable)
{
    RsttoImageViewerTransaction *transaction = task_data;

    if (! gdk_pixbuf_loader_write (transaction->loader, transaction->buffer,
                                   transaction->read_bytes, &transaction->error)
        || g_cancellable_is_cancelled (transaction->cancellable))
    {
        gdk_pixbuf_loader_close (transaction->loader, NULL);
        g_object_unref (source_object);
    }
    else
        g_input_stream_read_async (source_object, transaction->buffer, LOADER_BUFFER_SIZE,
                                   G_PRIORITY_DEFAULT, transaction->cancellable,
                                   cb_rstto_image_viewer_read_input_stream_ready, transaction);
}

static void
cb_rstto_image_viewer_read_input_stream_ready (GObject *source_object,
                                               GAsyncResult *result,
                                               gpointer user_data)
{
    RsttoImageViewerTransaction *transaction = user_data;
    GTask *task;

    if (rstto_main_window_get_app_exited ())
        return;

    transaction->read_bytes = g_input_stream_read_finish (G_INPUT_STREAM (source_object),
                                                          result, &transaction->error);
    if (transaction->read_bytes <= 0)
    {
        gdk_pixbuf_loader_close (transaction->loader,
                                 transaction->read_bytes == 0 ? &transaction->error : NULL);
        g_object_unref (source_object);
    }
    else
    {
        task = g_task_new (source_object, transaction->cancellable, NULL, NULL);
        g_task_set_task_data (task, transaction, NULL);
        g_task_run_in_thread (task, loader_write_async);
        g_object_unref (task);
    }
}


static void
cb_rstto_image_loader_image_ready (GdkPixbufLoader *loader, RsttoImageViewerTransaction *transaction)
{
    RsttoImageViewer *viewer = transaction->viewer;
    GdkPixbuf *pixbuf;
    gint timeout = 0;

    if (viewer->priv->transaction == transaction)
    {
        if (viewer->priv->animation_id != 0)
            REMOVE_SOURCE (viewer->priv->animation_id);

        if (viewer->priv->iter)
        {
            g_object_unref (viewer->priv->iter);
            viewer->priv->iter = NULL;
        }

        if (viewer->priv->pixbuf.pattern)
        {
            cairo_pattern_destroy (viewer->priv->pixbuf.pattern);
            viewer->priv->pixbuf.pattern = NULL;
        }

        viewer->priv->iter =
            gdk_pixbuf_animation_get_iter (gdk_pixbuf_loader_get_animation (loader), NULL);

        /* set pixbuf data */
        pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (viewer->priv->iter);
        viewer->priv->pixbuf.pattern = rstto_util_set_source_pixbuf (NULL, pixbuf, 0, 0);
        viewer->priv->pixbuf.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
        viewer->priv->pixbuf.width = gdk_pixbuf_get_width (pixbuf);
        viewer->priv->pixbuf.height = gdk_pixbuf_get_height (pixbuf);

        /* schedule next frame diplay if needed */
        timeout = gdk_pixbuf_animation_iter_get_delay_time (viewer->priv->iter);
        if (timeout > 0)
            viewer->priv->animation_id = g_timeout_add (timeout,
                                                        cb_rstto_image_viewer_update_pixbuf,
                                                        rstto_util_source_autoremove (viewer));
        else
        {
            /* this is a single-frame image, we only need to keep the pixbuf as a pattern */
            g_object_unref (viewer->priv->iter);
            viewer->priv->iter = NULL;
        }
    }
}

static void
cb_rstto_image_loader_size_prepared (GdkPixbufLoader *loader, gint width, gint height, RsttoImageViewerTransaction *transaction)
{
    /*
     * By default, the image-size won't be limited to screen-size (since it's smaller)
     * or, because we don't want to reduce it.
     * Set the image_scale to 1.0 (100%)
     */
    transaction->image_scale = 1.0;

    transaction->image_width = width;
    transaction->image_height = height;

    if (transaction->viewer->priv->limit_quality)
    {
        /*
         * Set the maximum size of the loaded image to the screen-size.
         * TODO: Add some 'smart-stuff' here
         */
        if (transaction->monitor_width < width || transaction->monitor_height < height)
        {
            gdouble s_width = transaction->monitor_width, s_height = transaction->monitor_height;

            /*
             * The image is loaded at the screen_size, calculate how this fits best.
             *  scale = MIN (width / screen_width, height / screen_height)
             */
            if (width / s_width < height / s_height)
            {
                transaction->image_scale = s_width / width;
                gdk_pixbuf_loader_set_size (loader, s_width, s_width * height / width);
            }
            else
            {
                transaction->image_scale = s_height / height;
                gdk_pixbuf_loader_set_size (loader, s_height * width / height, s_height);
            }
        }
    }
}

static gboolean
cb_rstto_image_loader_closed_idle (gpointer data)
{
    RsttoImageViewerTransaction *transaction = data;
    RsttoImageViewer *viewer = transaction->viewer;
    GdkPixbuf *pixbuf;
    GtkWidget *widget = GTK_WIDGET (viewer);

    if (viewer->priv->transaction == transaction)
    {
        pixbuf = gdk_pixbuf_loader_get_pixbuf (transaction->loader);

        /* cover any case where no pixbuf is available and no error is set:
         * see e.g. https://gitlab.gnome.org/GNOME/gdk-pixbuf/-/issues/204 */
        if (transaction->error == NULL && pixbuf == NULL)
            g_set_error (&transaction->error, GDK_PIXBUF_ERROR,
                         GDK_PIXBUF_ERROR_FAILED, ERROR_UNDETERMINED);

        if (transaction->error == NULL || (
                g_error_matches (transaction->error, GDK_PIXBUF_ERROR,
                                 GDK_PIXBUF_ERROR_CORRUPT_IMAGE)
                && pixbuf != NULL
            ))
        {
            gtk_widget_set_tooltip_text (widget, NULL);
            cb_rstto_image_loader_image_ready (transaction->loader, transaction);
            viewer->priv->image_scale = transaction->image_scale;
            viewer->priv->image_width = viewer->priv->original_image_width
                                      = transaction->image_width;
            viewer->priv->image_height = viewer->priv->original_image_height
                                       = transaction->image_height;
            set_scale_factor (viewer, NULL, NULL);
            set_scale (viewer, transaction->scale);
        }
        else
        {
            viewer->priv->image_scale = 1.0;
            viewer->priv->image_width = viewer->priv->original_image_width = 0;
            viewer->priv->image_height = viewer->priv->original_image_height = 0;
            if (viewer->priv->pixbuf.pattern)
            {
                cairo_pattern_destroy (viewer->priv->pixbuf.pattern);
                viewer->priv->pixbuf.pattern = NULL;
            }

            gtk_widget_set_tooltip_text (widget, transaction->error->message);
        }

        viewer->priv->error = transaction->error;
        transaction->error = NULL;
        viewer->priv->transaction = NULL;

        set_adjustments (viewer,
                         gtk_adjustment_get_value (viewer->priv->hadjustment),
                         gtk_adjustment_get_value (viewer->priv->vadjustment));
        gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

        g_signal_emit_by_name (transaction->viewer, "size-ready");
    }

    transaction->loader_closed_id = 0;

    return FALSE;
}

static void
cb_rstto_image_loader_closed (GdkPixbufLoader *loader,
                              RsttoImageViewerTransaction *transaction)
{
    /* this signal handler may run before the view has reached its final size, resulting
     * in incorrect scaling: the thumbnail bar is already shown at this time, but the size
     * request to adjust the view may not have taken place yet */
    transaction->loader_closed_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                                     cb_rstto_image_loader_closed_idle,
                                                     transaction,
                                                     rstto_image_viewer_transaction_free);
}

static gboolean
cb_rstto_image_viewer_update_pixbuf (gpointer user_data)
{
    RsttoImageViewer *viewer = user_data;
    GdkPixbuf *pixbuf;
    GdkRectangle rect;
    gint timeout = 0;

    if (gdk_pixbuf_animation_iter_advance (viewer->priv->iter, NULL))
    {
        /* update pixbuf data (a copy of the pixbuf is kept as a pattern, so we can access
         * it safely regardless of the iter state) */
        cairo_pattern_destroy (viewer->priv->pixbuf.pattern);
        pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (viewer->priv->iter);
        viewer->priv->pixbuf.pattern = rstto_util_set_source_pixbuf (NULL, pixbuf, 0, 0);
        viewer->priv->pixbuf.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
        viewer->priv->pixbuf.width = gdk_pixbuf_get_width (pixbuf);
        viewer->priv->pixbuf.height = gdk_pixbuf_get_height (pixbuf);
        set_scale_factor (viewer, NULL, NULL);

        /* redraw only the image */
        rect.x = viewer->priv->rendering.x_offset;
        rect.y = viewer->priv->rendering.y_offset;
        rect.width = viewer->priv->rendering.width;
        rect.height = viewer->priv->rendering.height;
        gdk_window_invalidate_rect (gtk_widget_get_window (user_data), &rect, FALSE);

        /* schedule next frame display */
        timeout = gdk_pixbuf_animation_iter_get_delay_time (viewer->priv->iter);
        if (timeout > 0)
            viewer->priv->animation_id = g_timeout_add (timeout,
                                                        cb_rstto_image_viewer_update_pixbuf,
                                                        rstto_util_source_autoremove (viewer));
    }

    if (timeout <= 0)
    {
        /* if the animation stops, we only need to keep the pixbuf as a pattern */
        g_object_unref (viewer->priv->iter);
        viewer->priv->iter = NULL;

        /* reset the timeout id only in this case */
        viewer->priv->animation_id = 0;
    }

    return FALSE;
}

static gboolean
rstto_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);
    gboolean invert_zoom_direction = viewer->priv->invert_zoom_direction;
    gdouble x_offset = viewer->priv->rendering.x_offset;
    gdouble y_offset = viewer->priv->rendering.y_offset;
    gdouble scale = viewer->priv->scale;
    gdouble tmp_x, tmp_y;

    if (event->state & (GDK_CONTROL_MASK))
    {
        if (NULL != viewer->priv->file)
        {
            tmp_x = (gtk_adjustment_get_value (viewer->priv->hadjustment) + event->x - x_offset)
                    / viewer->priv->scale;
            tmp_y = (gtk_adjustment_get_value (viewer->priv->vadjustment) + event->y - y_offset)
                    / viewer->priv->scale;

            switch (event->direction)
            {
                case GDK_SCROLL_UP:
                case GDK_SCROLL_LEFT:
                    scale = invert_zoom_direction ? viewer->priv->scale / RSTTO_SCALE_FACTOR
                                                  : viewer->priv->scale * RSTTO_SCALE_FACTOR;
                    break;
                case GDK_SCROLL_DOWN:
                case GDK_SCROLL_RIGHT:
                    scale = invert_zoom_direction ? viewer->priv->scale * RSTTO_SCALE_FACTOR
                                                  : viewer->priv->scale / RSTTO_SCALE_FACTOR;
                    break;
                case GDK_SCROLL_SMOOTH:
                    /* TODO */
                    break;
            }

            if (set_scale (viewer, scale))
            {
                set_adjustments (viewer, tmp_x * scale - event->x, tmp_y * scale - event->y);
                gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);
            }
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean
rstto_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);
    GdkRectangle previous_box, current_box, union_box;
    gdouble previous_x, previous_y;

    if (event->state & GDK_BUTTON1_MASK)
    {
        previous_x = viewer->priv->motion.current_x;
        previous_y = viewer->priv->motion.current_y;
        viewer->priv->motion.current_x = event->x;
        viewer->priv->motion.current_y = event->y;

        switch (viewer->priv->motion.state)
        {
            case RSTTO_IMAGE_VIEWER_MOTION_STATE_MOVE:
                if (viewer->priv->motion.x != viewer->priv->motion.current_x
                    || viewer->priv->motion.y != viewer->priv->motion.current_y)
                {
                    set_adjustments (viewer,
                                     viewer->priv->motion.h_val + viewer->priv->motion.x
                                        - viewer->priv->motion.current_x,
                                     viewer->priv->motion.v_val + viewer->priv->motion.y
                                        - viewer->priv->motion.current_y);
                    gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);
                }
                break;
            case RSTTO_IMAGE_VIEWER_MOTION_STATE_BOX_ZOOM:
                /* redraw only the union of the previous and current selection boxes */
                previous_box.x = MIN (viewer->priv->motion.x, previous_x);
                previous_box.y = MIN (viewer->priv->motion.y, previous_y);
                previous_box.width = ceil (MAX (viewer->priv->motion.x, previous_x))
                                     - previous_box.x + 2 * RSTTO_LINE_WIDTH;
                previous_box.height = ceil (MAX (viewer->priv->motion.y, previous_y))
                                      - previous_box.y + 2 * RSTTO_LINE_WIDTH;

                current_box.x = MIN (viewer->priv->motion.x, event->x);
                current_box.y = MIN (viewer->priv->motion.y, event->y);
                current_box.width = ceil (MAX (viewer->priv->motion.x, event->x))
                                    - current_box.x + 2 * RSTTO_LINE_WIDTH;
                current_box.height = ceil (MAX (viewer->priv->motion.y, event->y))
                                     - current_box.y + 2 * RSTTO_LINE_WIDTH;

                gdk_rectangle_union (&previous_box, &current_box, &union_box);
                gdk_window_invalidate_rect (gtk_widget_get_window (widget), &union_box, FALSE);

                /* Only change the cursor when hovering over the image
                 */
                if ((event->x > viewer->priv->rendering.x_offset) &&
                    (event->y > viewer->priv->rendering.y_offset) &&
                    (event->y < (viewer->priv->rendering.y_offset + viewer->priv->rendering.height)) &&
                    (event->x < (viewer->priv->rendering.x_offset + viewer->priv->rendering.width)))
                {
                    GdkCursor *cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_UL_ANGLE);
                    gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
                    g_object_unref (cursor);
                }
                else
                {
                    /* Set back to default when moving over the
                     * background.
                     */
                    gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
                }
                break;
            default:
                break;
        }
    }
    return FALSE;
}

static gboolean
rstto_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);

    if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
        viewer->priv->motion.x = event->x;
        viewer->priv->motion.y = event->y;
        viewer->priv->motion.current_x = event->x;
        viewer->priv->motion.current_y = event->y;
        viewer->priv->motion.h_val = gtk_adjustment_get_value (viewer->priv->hadjustment);
        viewer->priv->motion.v_val = gtk_adjustment_get_value (viewer->priv->vadjustment);

        if (viewer->priv->file != NULL)
        {
            if (! (event->state & (GDK_CONTROL_MASK)))
            {
                if ((event->x > viewer->priv->rendering.x_offset) &&
                    (event->y > viewer->priv->rendering.y_offset) &&
                    (event->y < (viewer->priv->rendering.y_offset + viewer->priv->rendering.height)) &&
                    (event->x < (viewer->priv->rendering.x_offset + viewer->priv->rendering.width)))
                {
                    GdkCursor *cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_FLEUR);
                    gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
                    g_object_unref (cursor);
                    rstto_image_viewer_set_motion_state (viewer, RSTTO_IMAGE_VIEWER_MOTION_STATE_MOVE);
                }
            }

            if (event->state & GDK_CONTROL_MASK)
            {
                /* Only change the cursor when hovering over the image
                 */
                if ((event->x > viewer->priv->rendering.x_offset) &&
                    (event->y > viewer->priv->rendering.y_offset) &&
                    (event->y < (viewer->priv->rendering.y_offset + viewer->priv->rendering.height)) &&
                    (event->x < (viewer->priv->rendering.x_offset + viewer->priv->rendering.width)))
                {
                    GdkCursor *cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_UL_ANGLE);
                    gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
                    g_object_unref (cursor);
                }

                /* Set the zoom-state even if not hovering over the
                 * image, this allows for easier selection.
                 * Dragging from / to somewhere outside the image to
                 * make sure the border is selected too.
                 */
                rstto_image_viewer_set_motion_state (viewer, RSTTO_IMAGE_VIEWER_MOTION_STATE_BOX_ZOOM);
            }
        }
        return TRUE;
    }
    else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
        /* toggle fullscreen mode only when double-clicking on the image */
        if (viewer->priv->file != NULL
            && event->x >= viewer->priv->rendering.x_offset
            && event->x <= (viewer->priv->rendering.x_offset + viewer->priv->rendering.width)
            && event->y >= viewer->priv->rendering.y_offset
            && event->y <= (viewer->priv->rendering.y_offset + viewer->priv->rendering.height))
        {
            GtkWidget *window;

            window = gtk_widget_get_ancestor (widget, RSTTO_TYPE_MAIN_WINDOW);
            if (window != NULL)
                rstto_main_window_fullscreen (widget, RSTTO_MAIN_WINDOW (window));
        }

        return TRUE;
    }
    else if (event->button == 3)
    {
        if (viewer->priv->menu)
        {
            gtk_widget_show_all (GTK_WIDGET (viewer->priv->menu));
            gtk_menu_popup_at_pointer (viewer->priv->menu, NULL);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean
rstto_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);
    gdouble box_x, box_y, box_width, box_height, tmp_x, tmp_y, scale,
            h_page_size, v_page_size;

    gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);

    switch (viewer->priv->motion.state)
    {
        case RSTTO_IMAGE_VIEWER_MOTION_STATE_BOX_ZOOM:
            if (compute_selection_box_dimensions (viewer, &box_x, &box_y, &box_width, &box_height))
            {
                /* calculate the center of the selection-box */
                tmp_x = (gtk_adjustment_get_value (viewer->priv->hadjustment)
                         + box_x + box_width / 2 - viewer->priv->rendering.x_offset)
                         / viewer->priv->scale;
                tmp_y = (gtk_adjustment_get_value (viewer->priv->vadjustment)
                         + box_y + box_height / 2 - viewer->priv->rendering.y_offset)
                         / viewer->priv->scale;

                /* calculate the new scale */
                h_page_size = gtk_adjustment_get_page_size (viewer->priv->hadjustment);
                v_page_size = gtk_adjustment_get_page_size (viewer->priv->vadjustment);
                scale = viewer->priv->scale * MIN (h_page_size / box_width,
                                                   v_page_size / box_height);

                if (set_scale (viewer, scale))
                {
                    set_adjustments (viewer,
                                     tmp_x * scale - h_page_size / 2,
                                     tmp_y * scale - v_page_size / 2);
                    gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);
                }
            }
            break;

        default:
            break;
    }

    rstto_image_viewer_set_motion_state (viewer, RSTTO_IMAGE_VIEWER_MOTION_STATE_NORMAL);

    return FALSE;
}

static void
cb_rstto_limit_quality_changed (GObject *settings, GParamSpec *pspec, gpointer user_data)
{
    RsttoImageViewer *viewer = user_data;

    g_object_get (viewer->priv->settings, "limit-quality", &(viewer->priv->limit_quality), NULL);

    if (NULL != viewer->priv->file)
        rstto_image_viewer_load_image (viewer, viewer->priv->file, viewer->priv->scale);
}

static void
cb_rstto_bgcolor_changed (GObject *settings, GParamSpec *pspec, gpointer user_data)
{
    RsttoImageViewer *viewer = user_data;
    cairo_region_t *region;
    cairo_status_t status;
    GtkAllocation alloc;
    const gchar *message = ERROR_UNDETERMINED;

    /* do not redraw the image */
    gtk_widget_get_allocation (user_data, &alloc);
    region = cairo_region_create_rectangle (&alloc);
    alloc.x = viewer->priv->rendering.x_offset;
    alloc.y = viewer->priv->rendering.y_offset;
    alloc.width = viewer->priv->rendering.width;
    alloc.height = viewer->priv->rendering.height;

    status = cairo_region_subtract_rectangle (region, &alloc);
    if (status != CAIRO_STATUS_SUCCESS)
    {
        if (status == CAIRO_STATUS_NO_MEMORY)
            message = "Not enough memory";

        g_warning ("Failed to optimize image drawing: %s", message);
    }

    gdk_window_invalidate_region (gtk_widget_get_window (user_data), region, FALSE);
    cairo_region_destroy (region);
}

static void
cb_rstto_zoom_direction_changed (GObject *settings, GParamSpec *pspec, gpointer user_data)
{
    RsttoImageViewer *viewer = user_data;
    viewer->priv->invert_zoom_direction = rstto_settings_get_boolean_property (RSTTO_SETTINGS (settings), "invert-zoom-direction");
}

static gboolean
rstto_popup_menu (GtkWidget *widget)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (widget);

    if (viewer->priv->menu)
    {
        gtk_widget_show_all (GTK_WIDGET (viewer->priv->menu));
        gtk_menu_popup_at_pointer (viewer->priv->menu, NULL);
        return TRUE;
    }
    return FALSE;
}

static void
cb_rstto_image_viewer_dnd (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data,
                           guint info, guint time_, RsttoImageViewer *viewer)
{
    g_return_if_fail (RSTTO_IS_IMAGE_VIEWER (viewer));

    if ((gtk_selection_data_get_length (data) >= 0) && (gtk_selection_data_get_format (data) == 8))
    {
        gchar **uris;

        uris = g_uri_list_extract_uris ((const gchar *) gtk_selection_data_get_data (data));

        g_signal_emit_by_name (viewer, "files-dnd", uris);

        gtk_drag_finish (context, TRUE, FALSE, time_);
        g_strfreev (uris);
    }
    else
    {
        gtk_drag_finish (context, FALSE, FALSE, time_);
    }
}

GError *
rstto_image_viewer_get_error (RsttoImageViewer *viewer)
{
    if (viewer->priv->error)
    {
        return g_error_copy (viewer->priv->error);
    }
    return NULL;
}

static void
rstto_image_viewer_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (object);

    switch (property_id)
    {
        case PROP_SHOW_CLOCK:
            viewer->priv->props.show_clock = g_value_get_boolean (value);
            break;
        case PROP_HADJUSTMENT:
            if (viewer->priv->hadjustment)
                g_object_unref (viewer->priv->hadjustment);

            viewer->priv->hadjustment = g_value_get_object (value);
            if (viewer->priv->hadjustment)
                g_object_ref (viewer->priv->hadjustment);
            break;
        case PROP_VADJUSTMENT:
            if (viewer->priv->vadjustment)
                g_object_unref (viewer->priv->vadjustment);

            viewer->priv->vadjustment = g_value_get_object (value);
            if (viewer->priv->vadjustment)
                g_object_ref (viewer->priv->vadjustment);
            break;
        case PROP_HSCROLL_POLICY:
            viewer->priv->hscroll_policy = g_value_get_enum (value);
            gtk_widget_queue_resize (GTK_WIDGET (viewer));
            break;
        case PROP_VSCROLL_POLICY:
            viewer->priv->vscroll_policy = g_value_get_enum (value);
            gtk_widget_queue_resize (GTK_WIDGET (viewer));
            break;
    }
}

static void
rstto_image_viewer_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER (object);

    switch (property_id)
    {
        case PROP_SHOW_CLOCK:
            g_value_set_boolean (value, viewer->priv->props.show_clock);
            break;
        case PROP_HADJUSTMENT:
            g_value_set_object (value, viewer->priv->hadjustment);
            break;
        case PROP_VADJUSTMENT:
            g_value_set_object (value, viewer->priv->vadjustment);
            break;
        case PROP_HSCROLL_POLICY:
            g_value_set_enum (value, viewer->priv->hscroll_policy);
            break;
        case PROP_VSCROLL_POLICY:
            g_value_set_enum (value, viewer->priv->vscroll_policy);
            break;
    }
}

static gboolean
cb_rstto_image_viewer_refresh (gpointer user_data)
{
    GtkAllocation alloc;
    gdouble width, offset;

    /* redraw only the clock square */
    gtk_widget_get_allocation (user_data, &alloc);
    width = RSTTO_GET_CLOCK_WIDTH (alloc.width, alloc.height);
    offset = RSTTO_GET_CLOCK_OFFSET (width);
    alloc.x = alloc.width - width - offset;
    alloc.y = alloc.height - width - offset;
    alloc.width = alloc.height = ceil (width);

    gdk_window_invalidate_rect (gtk_widget_get_window (user_data), &alloc, FALSE);

    return TRUE;
}

void
rstto_image_viewer_set_show_clock (RsttoImageViewer *viewer, gboolean value)
{
    static guint id = 0;

    viewer->priv->props.show_clock = value;

    if (value)
        id = g_timeout_add (15000, cb_rstto_image_viewer_refresh,
                            rstto_util_source_autoremove (viewer));
    else if (id != 0)
        REMOVE_SOURCE (id);
}

gboolean
rstto_image_viewer_is_busy (RsttoImageViewer *viewer)
{
    if (viewer->priv->transaction != NULL)
    {
        return TRUE;
    }
    return FALSE;
}

static void
cb_rstto_image_viewer_file_changed (RsttoFile *r_file, RsttoImageViewer *viewer)
{
    rstto_image_viewer_load_image (viewer, r_file, viewer->priv->auto_scale);
    g_signal_emit_by_name (viewer, "status-changed");
}
