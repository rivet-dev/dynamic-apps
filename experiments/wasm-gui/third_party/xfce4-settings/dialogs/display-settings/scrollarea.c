/* Copyright 2006, 2007, 2008, Soren Sandmann <sandmann@daimi.au.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gdk/gdkprivate.h> /* For GDK_PARENT_RELATIVE_BG */
#include "scrollarea.h"
#include "foo-marshal.h"

G_DEFINE_TYPE_WITH_CODE (FooScrollArea, foo_scroll_area, GTK_TYPE_CONTAINER, G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));

static GtkWidgetClass *parent_class;

typedef struct BackingStore BackingStore;

typedef void (* ExposeFunc) (cairo_t *cr, GdkRegion *region, gpointer data);

typedef struct InputPath InputPath;
typedef struct InputRegion InputRegion;
typedef struct AutoScrollInfo AutoScrollInfo;

struct InputPath
{
    gboolean                is_stroke;
    cairo_fill_rule_t       fill_rule;
    double                  line_width;
    cairo_path_t           *path;       /* In canvas coordinates */

    FooScrollAreaEventFunc  func;
    gpointer                data;

    InputPath              *next;
};

/* InputRegions are mutually disjoint */
struct InputRegion
{
    GdkRegion *region;      /* the boundary of this area in canvas coordinates */
    InputPath *paths;
};

struct AutoScrollInfo
{
    int     dx;
    int     dy;
    int     timeout_id;
    int     begin_x;
    int     begin_y;
    double  res_x;
    double  res_y;
    GTimer *timer;
};

struct FooScrollAreaPrivate
{
    GdkWindow              *input_window;

    int                     width;
    int                     height;

    GtkAdjustment          *hadj;
    GtkAdjustment          *vadj;
    GtkScrollablePolicy     hscroll_policy;
    GtkScrollablePolicy     vscroll_policy;
    int                     x_offset;
    int                     y_offset;

    int                     min_width;
    int                     min_height;

    GPtrArray              *input_regions;

    AutoScrollInfo         *auto_scroll_info;

    /* During expose, this region is set to the region
     * being exposed. At other times, it is NULL
     *
     * It is used for clipping of input areas
     */
    InputRegion            *current_input;

    gboolean                grabbed;
    FooScrollAreaEventFunc  grab_func;
    gpointer                grab_data;

    cairo_surface_t        *surface;
    GdkRegion              *update_region;      /* In canvas coordinates */
};

enum
{
    VIEWPORT_CHANGED,
    PAINT,
    INPUT,
    LAST_SIGNAL,
};

enum {
    PROP_0,
    PROP_VADJUSTMENT,
    PROP_HADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY
};

static guint scrollarea_signals [LAST_SIGNAL] = { 0 };

static void     foo_scroll_area_get_preferred_width  (GtkWidget      *widget,
                                                      gint           *minimum,
                                                      gint           *natural);
static void     foo_scroll_area_get_preferred_height (GtkWidget      *widget,
                                                      gint           *minimum,
                                                      gint           *natural);
static gboolean foo_scroll_area_draw                 (GtkWidget      *widget,
                                                      cairo_t        *cr);
static void     foo_scroll_area_size_allocate        (GtkWidget      *widget,
                                                      GtkAllocation  *allocation);
static void     foo_scroll_area_set_hadjustment      (FooScrollArea  *scroll_area,
                                                      GtkAdjustment  *hadjustment);
static void     foo_scroll_area_set_vadjustment      (FooScrollArea  *scroll_area,
                                                      GtkAdjustment  *vadjustment);
static void     foo_scroll_area_realize              (GtkWidget      *widget);
static void     foo_scroll_area_unrealize            (GtkWidget      *widget);
static void     foo_scroll_area_map                  (GtkWidget      *widget);
static void     foo_scroll_area_unmap                (GtkWidget      *widget);
static gboolean foo_scroll_area_button_press         (GtkWidget      *widget,
                                                      GdkEventButton *event);
static gboolean foo_scroll_area_button_release       (GtkWidget       *widget,
                                                      GdkEventButton *event);
static gboolean foo_scroll_area_motion               (GtkWidget      *widget,
                                                      GdkEventMotion *event);

static void
foo_scroll_area_map (GtkWidget *widget)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);

    GTK_WIDGET_CLASS (parent_class)->map (widget);

    if (area->priv->input_window)
        gdk_window_show (area->priv->input_window);
}

static void
foo_scroll_area_unmap (GtkWidget *widget)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);

    if (area->priv->input_window)
        gdk_window_hide (area->priv->input_window);

    GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
foo_scroll_area_finalize (GObject *object)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (object);

    g_object_unref (scroll_area->priv->hadj);
    g_object_unref (scroll_area->priv->vadj);

    g_ptr_array_free (scroll_area->priv->input_regions, TRUE);
    gdk_region_destroy (scroll_area->priv->update_region);

    g_free (scroll_area->priv);

    G_OBJECT_CLASS (foo_scroll_area_parent_class)->finalize (object);
}

static void
foo_scroll_area_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (object);

    switch (property_id)
    {
        case PROP_VADJUSTMENT:
            g_value_set_object (value, &scroll_area->priv->vadj);
            break;
        case PROP_HADJUSTMENT:
            g_value_set_object (value, &scroll_area->priv->hadj);
            break;
        case PROP_HSCROLL_POLICY:
            g_value_set_enum (value, scroll_area->priv->hscroll_policy);
            break;
        case PROP_VSCROLL_POLICY:
            g_value_set_enum (value, scroll_area->priv->vscroll_policy);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
foo_scroll_area_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (object);

    switch (property_id)
    {
        case PROP_VADJUSTMENT:
            foo_scroll_area_set_vadjustment (FOO_SCROLL_AREA (object), g_value_get_object (value));
            break;
        case PROP_HADJUSTMENT:
            foo_scroll_area_set_hadjustment (FOO_SCROLL_AREA (object), g_value_get_object (value));
            break;
        case PROP_HSCROLL_POLICY:
            scroll_area->priv->hscroll_policy = g_value_get_enum (value);
            break;
        case PROP_VSCROLL_POLICY:
            scroll_area->priv->vscroll_policy = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
foo_scroll_area_class_init (FooScrollAreaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = foo_scroll_area_finalize;
    object_class->set_property = foo_scroll_area_set_property;
    object_class->get_property = foo_scroll_area_get_property;
    widget_class->draw = foo_scroll_area_draw;
    widget_class->get_preferred_width = foo_scroll_area_get_preferred_width;
    widget_class->get_preferred_height = foo_scroll_area_get_preferred_height;
    widget_class->size_allocate = foo_scroll_area_size_allocate;
    widget_class->realize = foo_scroll_area_realize;
    widget_class->unrealize = foo_scroll_area_unrealize;
    widget_class->button_press_event = foo_scroll_area_button_press;
    widget_class->button_release_event = foo_scroll_area_button_release;
    widget_class->motion_notify_event = foo_scroll_area_motion;
    widget_class->map = foo_scroll_area_map;
    widget_class->unmap = foo_scroll_area_unmap;

    parent_class = g_type_class_peek_parent (klass);

    /* Scrollable interface properties */
    g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
    g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
    g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

    scrollarea_signals[VIEWPORT_CHANGED] =
        g_signal_new ("viewport_changed",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (FooScrollAreaClass,
                                       viewport_changed),
                      NULL, NULL,
                      foo_marshal_VOID__BOXED_BOXED,
                      G_TYPE_NONE, 2,
                      GDK_TYPE_RECTANGLE,
                      GDK_TYPE_RECTANGLE);

    scrollarea_signals[PAINT] =
        g_signal_new ("paint",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (FooScrollAreaClass,
                                       paint),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1,
                      G_TYPE_POINTER);
}

static GtkAdjustment *
new_adjustment (void)
{
    return GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
}

static void
foo_scroll_area_init (FooScrollArea *scroll_area)
{
    GtkWidget *widget;

    widget = GTK_WIDGET (scroll_area);

    gtk_widget_set_has_window (widget, FALSE);
    gtk_widget_set_redraw_on_allocate (widget, FALSE);

    scroll_area->priv = g_new0 (FooScrollAreaPrivate, 1);
    scroll_area->priv->width = 0;
    scroll_area->priv->height = 0;
    scroll_area->priv->hadj = g_object_ref_sink (new_adjustment());
    scroll_area->priv->vadj = g_object_ref_sink (new_adjustment());
    scroll_area->priv->x_offset = 0.0;
    scroll_area->priv->y_offset = 0.0;
    scroll_area->priv->min_width = 0;
    scroll_area->priv->min_height = 0;
    scroll_area->priv->auto_scroll_info = NULL;
    scroll_area->priv->input_regions = g_ptr_array_new ();
    scroll_area->priv->surface = NULL;
    scroll_area->priv->update_region = gdk_region_new ();
}

typedef void (* PathForeachFunc) (double  *x,
                                  double  *y,
                                  gpointer data);

typedef struct
{
    double x1, y1, x2, y2;
} Box;

static void
input_path_free_list (InputPath *paths)
{
    if (!paths)
        return;

    input_path_free_list (paths->next);
    cairo_path_destroy (paths->path);
    g_free (paths);
}

static void
input_region_free (InputRegion *region)
{
    input_path_free_list (region->paths);
    gdk_region_destroy (region->region);

    g_free (region);
}

static void
get_viewport (FooScrollArea *scroll_area,
              GdkRectangle  *viewport)
{
    GtkAllocation allocation;
    GtkWidget *widget = GTK_WIDGET (scroll_area);

    gtk_widget_get_allocation (widget, &allocation);

    viewport->x = scroll_area->priv->x_offset;
    viewport->y = scroll_area->priv->y_offset;
    viewport->width = allocation.width;
    viewport->height = allocation.height;
}

static void
allocation_to_canvas (FooScrollArea *area,
                      int           *x,
                      int           *y)
{
    *x += area->priv->x_offset;
    *y += area->priv->y_offset;
}

static void
clear_exposed_input_region (FooScrollArea *area,
                            GdkRegion     *exposed) /* in canvas coordinates */
{
    guint i;
    GdkRegion *viewport;
    GdkRectangle allocation;

    gtk_widget_get_allocation (GTK_WIDGET (area), &allocation);
    allocation.x = 0;
    allocation.y = 0;
    allocation_to_canvas (area, &allocation.x, &allocation.y);
    viewport = gdk_region_rectangle (&allocation);
    gdk_region_subtract (viewport, exposed);

    for (i = 0; i < area->priv->input_regions->len; ++i)
    {
        InputRegion *region = area->priv->input_regions->pdata[i];

        gdk_region_intersect (region->region, viewport);

        if (gdk_region_empty (region->region))
        {
            input_region_free (region);
            g_ptr_array_remove_index_fast (area->priv->input_regions, i--);
        }
    }

    gdk_region_destroy (viewport);
}

static void
setup_background_cr (GdkWindow *window,
                     cairo_t   *cr,
                     int        x_offset,
                     int        y_offset)
{
    cairo_translate (cr, - x_offset, - y_offset);
    cairo_set_source_rgb (cr, 0.98, 0.98, 0.98);
    cairo_translate (cr, x_offset, y_offset);
}

static void
initialize_background (GtkWidget *widget,
                       cairo_t   *cr)
{
    setup_background_cr (gtk_widget_get_window (widget), cr, 0, 0);

    cairo_paint (cr);
}

static gboolean
foo_scroll_area_draw (GtkWidget *widget,
                      cairo_t   *widget_cr)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (widget);
    GdkRegion *region;
    GtkAllocation widget_allocation;

    /* Setup input areas */
    clear_exposed_input_region (scroll_area, scroll_area->priv->update_region);

    scroll_area->priv->current_input = g_new0 (InputRegion, 1);
    scroll_area->priv->current_input->region = gdk_region_copy (scroll_area->priv->update_region);
    scroll_area->priv->current_input->paths = NULL;
    g_ptr_array_add (scroll_area->priv->input_regions, scroll_area->priv->current_input);

    region = scroll_area->priv->update_region;
    scroll_area->priv->update_region = gdk_region_new ();

    initialize_background (widget, widget_cr);

    g_signal_emit (widget, scrollarea_signals[PAINT], 0, widget_cr);

    scroll_area->priv->current_input = NULL;

    gtk_widget_get_allocation (widget, &widget_allocation);

    /* Finally draw the backing surface */
    cairo_set_source_surface (widget_cr, scroll_area->priv->surface,
                              widget_allocation.x, widget_allocation.y);
    cairo_fill (widget_cr);

    gdk_region_destroy (region);

    return TRUE;
}

void
foo_scroll_area_get_viewport (FooScrollArea *scroll_area,
                              GdkRectangle  *viewport)
{
    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));

    if (!viewport)
        return;

    get_viewport (scroll_area, viewport);
}

static void
process_event (FooScrollArea          *scroll_area,
               FooScrollAreaEventType  input_type,
               int                     x,
               int                     y);

static void
emit_viewport_changed (FooScrollArea *scroll_area,
                       GdkRectangle  *new_viewport,
                       GdkRectangle  *old_viewport)
{
    int px, py;
    GdkSeat *seat;
    GdkDevice *device;

    g_signal_emit (scroll_area, scrollarea_signals[VIEWPORT_CHANGED], 0,
                   new_viewport, old_viewport);

    if (scroll_area->priv->input_window == NULL)
        return;

    seat = gdk_display_get_default_seat (gdk_window_get_display (scroll_area->priv->input_window));
    device = gdk_seat_get_pointer (seat);
    gdk_window_get_device_position (scroll_area->priv->input_window, device, &px, &py, NULL);

    process_event (scroll_area, FOO_MOTION, px, py);
}

static void
clamp_adjustment (GtkAdjustment *adj)
{
    if (gtk_adjustment_get_upper (adj) >= gtk_adjustment_get_page_size (adj))
        gtk_adjustment_set_value (adj, CLAMP (gtk_adjustment_get_value (adj), 0.0,
                                              gtk_adjustment_get_upper (adj)
                                               - gtk_adjustment_get_page_size (adj)));
    else
        gtk_adjustment_set_value (adj, 0.0);
}

static gboolean
set_adjustment_values (FooScrollArea *scroll_area)
{
    GtkAllocation allocation;

    GtkAdjustment *hadj = scroll_area->priv->hadj;
    GtkAdjustment *vadj = scroll_area->priv->vadj;

    /* Horizontal */
    gtk_widget_get_allocation (GTK_WIDGET (scroll_area), &allocation);
    g_object_freeze_notify (G_OBJECT (hadj));
    gtk_adjustment_set_page_size (hadj, allocation.width);
    gtk_adjustment_set_step_increment (hadj, 0.1 * allocation.width);
    gtk_adjustment_set_page_increment (hadj, 0.9 * allocation.width);
    gtk_adjustment_set_lower (hadj, 0.0);
    gtk_adjustment_set_upper (hadj, scroll_area->priv->width);
    g_object_thaw_notify (G_OBJECT (hadj));

    /* Vertical */
    g_object_freeze_notify (G_OBJECT (vadj));
    gtk_adjustment_set_page_size (vadj, allocation.height);
    gtk_adjustment_set_step_increment (vadj, 0.1 * allocation.height);
    gtk_adjustment_set_page_increment (vadj, 0.9 * allocation.height);
    gtk_adjustment_set_lower (vadj, 0.0);
    gtk_adjustment_set_upper (vadj, scroll_area->priv->height);
    g_object_thaw_notify (G_OBJECT (vadj));

    clamp_adjustment (hadj);
    clamp_adjustment (vadj);

    return TRUE;
}

static void
foo_scroll_area_realize (GtkWidget *widget)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);
    GdkWindowAttr attributes;
    GtkAllocation widget_allocation;
    GdkWindow *window;
    gint attributes_mask;
    cairo_t *cr;

    gtk_widget_get_allocation (widget, &widget_allocation);
    gtk_widget_set_realized (widget, TRUE);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget_allocation.x;
    attributes.y = widget_allocation.y;
    attributes.width = widget_allocation.width;
    attributes.height = widget_allocation.height;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK |
                              GDK_BUTTON1_MOTION_MASK |
                              GDK_BUTTON2_MOTION_MASK |
                              GDK_BUTTON3_MOTION_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_ENTER_NOTIFY_MASK |
                              GDK_LEAVE_NOTIFY_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y;

    window = gtk_widget_get_parent_window (widget);
    gtk_widget_set_window (widget, window);
    g_object_ref (window);

    area->priv->input_window = gdk_window_new (window, &attributes, attributes_mask);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    cr = gdk_cairo_create (gtk_widget_get_window (widget));
G_GNUC_END_IGNORE_DEPRECATIONS
    area->priv->surface = cairo_surface_create_similar (cairo_get_target (cr), CAIRO_CONTENT_COLOR,
                                                        widget_allocation.width, widget_allocation.height);
    cairo_destroy (cr);
    gdk_window_set_user_data (area->priv->input_window, area);
}

static void
foo_scroll_area_unrealize (GtkWidget *widget)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);

    if (area->priv->input_window)
    {
        gdk_window_set_user_data (area->priv->input_window, NULL);
        gdk_window_destroy (area->priv->input_window);
        area->priv->input_window = NULL;
    }

    GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static cairo_surface_t *
create_new_surface (GtkWidget       *widget,
                    cairo_surface_t *old)
{
    GtkAllocation widget_allocation;
    cairo_t *cr;
    cairo_surface_t *new;

    gtk_widget_get_allocation (widget, &widget_allocation);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    cr = gdk_cairo_create (gtk_widget_get_window (widget));
G_GNUC_END_IGNORE_DEPRECATIONS

    new = cairo_surface_create_similar (cairo_get_target (cr),
                                        CAIRO_CONTENT_COLOR,
                                        widget_allocation.width,
                                        widget_allocation.height);
    cairo_destroy (cr);

    /* Unfortunately we don't know in which direction we were resized,
     * so we just assume we were dragged from the south-east corner.
     *
     * Although, maybe we could get the root coordinates of the input-window?
     * That might just work, actually. We need to make sure marco uses
     * static gravity for the window before this will be useful.
     */

    cr = cairo_create (new);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface (cr, old, 0, 0);

    cairo_paint (cr);
    cairo_destroy (cr);

    return new;
}

static void
allocation_to_canvas_region (FooScrollArea *area,
                             GdkRegion     *region)
{
    gdk_region_offset (region, area->priv->x_offset, area->priv->y_offset);
}

static void
_cairo_region_xor (cairo_region_t       *dst,
                   const cairo_region_t *src)
{
    cairo_region_t *trb;

    trb = cairo_region_copy (src);

    cairo_region_subtract (trb, dst);
    cairo_region_subtract (dst, src);
    cairo_region_union (dst, trb);
    cairo_region_destroy (trb);
}

static void
foo_scroll_area_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (widget);
    GdkRectangle new_viewport;
    GdkRectangle old_viewport;
    GdkRegion *old_allocation;
    GdkRegion *invalid;
    GtkAllocation widget_allocation;

    get_viewport (scroll_area, &old_viewport);

    gtk_widget_get_allocation (widget, &widget_allocation);
    old_allocation = gdk_region_rectangle (&widget_allocation);
    gdk_region_offset (old_allocation, -widget_allocation.x, -widget_allocation.y);
    invalid = gdk_region_rectangle (allocation);
    gdk_region_offset (invalid, -allocation->x, -allocation->y);
    _cairo_region_xor (invalid, old_allocation);
    allocation_to_canvas_region (scroll_area, invalid);
    foo_scroll_area_invalidate_region (scroll_area, invalid);
    gdk_region_destroy (old_allocation);
    gdk_region_destroy (invalid);

    gtk_widget_set_allocation (widget, allocation);

    if (scroll_area->priv->input_window)
    {
        cairo_surface_t *new_surface;

        gdk_window_move_resize (scroll_area->priv->input_window,
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);

        new_surface = create_new_surface (widget, scroll_area->priv->surface);
        cairo_surface_destroy (scroll_area->priv->surface);
        scroll_area->priv->surface = new_surface;
    }

    get_viewport (scroll_area, &new_viewport);

    emit_viewport_changed (scroll_area, &new_viewport, &old_viewport);
}

static void
emit_input (FooScrollArea         *scroll_area,
            FooScrollAreaEventType type,
            int                    x,
            int                    y,
            FooScrollAreaEventFunc func,
            gpointer               data)
{
    FooScrollAreaEvent event;

    if (!func)
        return;

    if (type != FOO_MOTION && type != FOO_MOTION_OUTSIDE)
        emit_input (scroll_area, FOO_MOTION, x, y, func, data);

    event.type = type;
    event.x = x;
    event.y = y;

    func (scroll_area, &event, data);
}

static void
process_event (FooScrollArea         *scroll_area,
               FooScrollAreaEventType input_type,
               int                    x,
               int                    y)
{
    GtkWidget *widget = GTK_WIDGET (scroll_area);
    guint i;

    allocation_to_canvas (scroll_area, &x, &y);

    if (scroll_area->priv->grabbed)
    {
        emit_input (scroll_area, input_type, x, y,
                    scroll_area->priv->grab_func,
                    scroll_area->priv->grab_data);
        return;
    }

    for (i = 0; i < scroll_area->priv->input_regions->len; ++i)
    {
        InputRegion *region = scroll_area->priv->input_regions->pdata[i];

        if (gdk_region_point_in (region->region, x, y))
        {
            InputPath *path;

            path = region->paths;
            while (path)
            {
                cairo_t *cr;
                gboolean inside;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                cr = gdk_cairo_create (gtk_widget_get_window (widget));
G_GNUC_END_IGNORE_DEPRECATIONS
                cairo_set_fill_rule (cr, path->fill_rule);
                cairo_set_line_width (cr, path->line_width);
                cairo_append_path (cr, path->path);

                if (path->is_stroke)
                    inside = cairo_in_stroke (cr, x, y);
                else
                    inside = cairo_in_fill (cr, x, y);

                cairo_destroy (cr);

                if (inside)
                {
                    emit_input (scroll_area, input_type,
                                x, y,
                                path->func,
                                path->data);
                    return;
                }
                else if (input_type == FOO_MOTION) {
                    emit_input (scroll_area, FOO_MOTION_OUTSIDE,
                                x, y,
                                path->func,
                                path->data);
                }

                path = path->next;
            }

            /* Since the regions are all disjoint, no other region
             * can match. Of course we could be clever and try and
             * sort the regions, but so far I have been unable to
             * make this loop show up on a profile.
             */
            return;
        }
    }
}

static void
process_gdk_event (FooScrollArea *scroll_area,
                   int            x,
                   int            y,
                   GdkEvent      *event)
{
    FooScrollAreaEventType input_type;

    if (event->type == GDK_BUTTON_PRESS)
        input_type = FOO_BUTTON_PRESS;
    else if (event->type == GDK_BUTTON_RELEASE)
        input_type = FOO_BUTTON_RELEASE;
    else if (event->type == GDK_MOTION_NOTIFY)
        input_type = FOO_MOTION;
    else
        return;

    process_event (scroll_area, input_type, x, y);
}

static gboolean
foo_scroll_area_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);

    process_gdk_event (area, event->x, event->y, (GdkEvent *)event);

    return TRUE;
}

static gboolean
foo_scroll_area_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);

    process_gdk_event (area, event->x, event->y, (GdkEvent *)event);

    return FALSE;
}

static gboolean
foo_scroll_area_motion (GtkWidget      *widget,
                        GdkEventMotion *event)
{
    FooScrollArea *area = FOO_SCROLL_AREA (widget);

    process_gdk_event (area, event->x, event->y, (GdkEvent *)event);

    return TRUE;
}

void
foo_scroll_area_set_size_fixed_y (FooScrollArea *scroll_area,
                                  int            width,
                                  int            height,
                                  int            old_y,
                                  int            new_y)
{
    scroll_area->priv->width = width;
    scroll_area->priv->height = height;

    g_object_thaw_notify (G_OBJECT (scroll_area->priv->vadj));
    gtk_adjustment_set_value (scroll_area->priv->vadj, new_y);

    set_adjustment_values (scroll_area);
    g_object_thaw_notify (G_OBJECT (scroll_area->priv->vadj));
}

void
foo_scroll_area_set_size (FooScrollArea *scroll_area,
                          int            width,
                          int            height)
{
    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));

    /* FIXME: Default scroll algorithm should probably be to
     * keep the same *area* outside the screen as before.
     *
     * For wrapper widgets that will do something roughly
     * right. For widgets that don't change size, it
     * will do the right thing. Except for idle-layouting
     * widgets.
     *
     * Maybe there should be some generic support for those
     * widgets. Can that even be done?
     *
     * Should we have a version of this function using
     * fixed points?
     */

    scroll_area->priv->width = width;
    scroll_area->priv->height = height;

    set_adjustment_values (scroll_area);
}

static void
foo_scroll_area_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (widget);

    if (minimum != NULL) {
        *minimum = scroll_area->priv->min_width;
    }
    if (natural != NULL) {
        *natural = scroll_area->priv->min_width;
    }
}

static void
foo_scroll_area_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
    FooScrollArea *scroll_area = FOO_SCROLL_AREA (widget);

    if (minimum != NULL) {
        *minimum = scroll_area->priv->min_height;
    }
    if (natural != NULL) {
        *natural = scroll_area->priv->min_height;
    }
}

static void
foo_scroll_area_scroll (FooScrollArea *area,
                        gint           dx,
                        gint           dy)
{
    GdkRectangle allocation;
    GdkRectangle src_area;
    GdkRectangle move_area;
    GdkRegion *invalid_region;

    gtk_widget_get_allocation (GTK_WIDGET (area), &allocation);
    allocation.x = 0;
    allocation.y = 0;

    src_area = allocation;
    src_area.x -= dx;
    src_area.y -= dy;

    invalid_region = gdk_region_rectangle (&allocation);

    if (gdk_rectangle_intersect (&allocation, &src_area, &move_area))
    {
        GdkRegion *move_region;
        cairo_t *cr;

        cr = cairo_create (area->priv->surface);

        /* Cairo doesn't allow self-copies, so we do this little trick instead:
         * 1) Clip so the group size is small.
         * 2) Call push_group() which creates a temporary pixmap as a workaround
         */
        gdk_cairo_rectangle (cr, &move_area);
        cairo_clip (cr);
        cairo_push_group (cr);

        cairo_set_source_surface (cr, area->priv->surface, dx, dy);
        gdk_cairo_rectangle (cr, &move_area);
        cairo_fill (cr);

        cairo_pop_group_to_source (cr);
        cairo_paint (cr);

        cairo_destroy (cr);

        gtk_widget_queue_draw (GTK_WIDGET (area));

        move_region = gdk_region_rectangle (&move_area);
        gdk_region_offset (move_region, dx, dy);
        gdk_region_subtract (invalid_region, move_region);
        gdk_region_destroy (move_region);
    }

    allocation_to_canvas_region (area, invalid_region);

    foo_scroll_area_invalidate_region (area, invalid_region);

    gdk_region_destroy (invalid_region);
}

static void
foo_scrollbar_adjustment_changed (GtkAdjustment *adj,
                                  FooScrollArea *scroll_area)
{
    GtkWidget *widget = GTK_WIDGET (scroll_area);
    gint dx = 0;
    gint dy = 0;
    GdkRectangle old_viewport, new_viewport;

    get_viewport (scroll_area, &old_viewport);

    if (adj == scroll_area->priv->hadj)
    {
        /* FIXME: do we treat the offset as int or double, and,
         * if int, how do we round?
         */
        dx = (int)gtk_adjustment_get_value (adj) - scroll_area->priv->x_offset;
        scroll_area->priv->x_offset = gtk_adjustment_get_value (adj);
    }
    else if (adj == scroll_area->priv->vadj)
    {
        dy = (int)gtk_adjustment_get_value (adj) - scroll_area->priv->y_offset;
        scroll_area->priv->y_offset = gtk_adjustment_get_value (adj);
    }
    else
    {
        g_assert_not_reached ();
    }

    if (gtk_widget_get_realized (widget))
    {
        foo_scroll_area_scroll (scroll_area, -dx, -dy);
    }

    get_viewport (scroll_area, &new_viewport);

    emit_viewport_changed (scroll_area, &new_viewport, &old_viewport);
}

static void
set_one_adjustment (FooScrollArea *scroll_area,
                    GtkAdjustment *adjustment,
                    GtkAdjustment **location)
{
    g_return_if_fail (location != NULL);

    if (adjustment == *location)
        return;

    if (!adjustment)
        adjustment = new_adjustment ();

    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

    if (*location)
    {
        g_signal_handlers_disconnect_by_func (*location, foo_scrollbar_adjustment_changed, scroll_area);
        g_object_unref (*location);
    }

    *location = adjustment;

    g_object_ref_sink (*location);

    g_signal_connect (*location, "value_changed",
                      G_CALLBACK (foo_scrollbar_adjustment_changed),
                      scroll_area);
}

static void
foo_scroll_area_set_hadjustment (FooScrollArea *scroll_area,
                                 GtkAdjustment *hadjustment)
{
    set_one_adjustment (scroll_area, hadjustment, &scroll_area->priv->hadj);

    set_adjustment_values (scroll_area);
}

static void
foo_scroll_area_set_vadjustment (FooScrollArea *scroll_area,
                                 GtkAdjustment *vadjustment)
{
    set_one_adjustment (scroll_area, vadjustment, &scroll_area->priv->vadj);

    set_adjustment_values (scroll_area);
}

FooScrollArea *
foo_scroll_area_new (void)
{
    return g_object_new (FOO_TYPE_SCROLL_AREA, NULL);
}

void
foo_scroll_area_set_min_size (FooScrollArea *scroll_area,
                              int            min_width,
                              int            min_height)
{
    scroll_area->priv->min_width = min_width;
    scroll_area->priv->min_height = min_height;

    /* FIXME: think through invalidation.
     *
     * Goals: - no repainting everything on size_allocate(),
     *        - make sure input boxes are invalidated when
     *          needed
     */
    gtk_widget_queue_resize (GTK_WIDGET (scroll_area));
}

static InputPath *
make_path (FooScrollArea         *area,
           cairo_t               *cr,
           gboolean               is_stroke,
           FooScrollAreaEventFunc func,
           gpointer               data)
{
    InputPath *path = g_new0 (InputPath, 1);

    path->is_stroke = is_stroke;
    path->fill_rule = cairo_get_fill_rule (cr);
    path->line_width = cairo_get_line_width (cr);
    path->path = cairo_copy_path (cr);
    path->func = func;
    path->data = data;
    path->next = area->priv->current_input->paths;
    area->priv->current_input->paths = path;
    return path;
}

/* FIXME: we probably really want a
 *
 *      foo_scroll_area_add_input_from_fill (area, cr, ...);
 * and
 *      foo_scroll_area_add_input_from_stroke (area, cr, ...);
 * as well.
 */
void
foo_scroll_area_add_input_from_fill (FooScrollArea         *scroll_area,
                                     cairo_t               *cr,
                                     FooScrollAreaEventFunc func,
                                     gpointer               data)
{
    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));
    g_return_if_fail (cr != NULL);
    g_return_if_fail (scroll_area->priv->current_input);

    make_path (scroll_area, cr, FALSE, func, data);
}

void
foo_scroll_area_add_input_from_stroke (FooScrollArea         *scroll_area,
                                       cairo_t               *cr,
                                       FooScrollAreaEventFunc func,
                                       gpointer               data)
{
    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));
    g_return_if_fail (cr != NULL);
    g_return_if_fail (scroll_area->priv->current_input);

    make_path (scroll_area, cr, TRUE, func, data);
}

void
foo_scroll_area_invalidate (FooScrollArea *scroll_area)
{
    GtkAllocation allocation;
    GtkWidget *widget = GTK_WIDGET (scroll_area);

    gtk_widget_get_allocation (widget, &allocation);
    foo_scroll_area_invalidate_rect (scroll_area,
                                     scroll_area->priv->x_offset,
                                     scroll_area->priv->y_offset,
                                     allocation.width,
                                     allocation.height);
}

static void
canvas_to_window (FooScrollArea *area,
                  GdkRegion     *region)
{
    GtkAllocation allocation;
    GtkWidget *widget = GTK_WIDGET (area);

    gtk_widget_get_allocation (widget, &allocation);
    gdk_region_offset (region,
                       -area->priv->x_offset + allocation.x,
                       -area->priv->y_offset + allocation.y);
}

static void
window_to_canvas (FooScrollArea *area,
                  GdkRegion     *region)
{
    GtkAllocation allocation;
    GtkWidget *widget = GTK_WIDGET (area);

    gtk_widget_get_allocation (widget, &allocation);
    gdk_region_offset (region,
                       area->priv->x_offset - allocation.x,
                       area->priv->y_offset - allocation.y);
}

void
foo_scroll_area_invalidate_region (FooScrollArea *area,
                                   GdkRegion     *region)
{
    GtkWidget *widget;

    g_return_if_fail (FOO_IS_SCROLL_AREA (area));

    widget = GTK_WIDGET (area);

    gdk_region_union (area->priv->update_region, region);

    if (gtk_widget_get_realized (widget))
    {
        canvas_to_window (area, region);
        gdk_window_invalidate_region (gtk_widget_get_window (widget),
                                      region, TRUE);
        window_to_canvas (area, region);
    }
}

void
foo_scroll_area_invalidate_rect (FooScrollArea *scroll_area,
                                 int            x,
                                 int            y,
                                 int            width,
                                 int            height)
{
    GdkRectangle rect = { x, y, width, height };
    GdkRegion *region;

    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));

    region = gdk_region_rectangle (&rect);

    foo_scroll_area_invalidate_region (scroll_area, region);

    gdk_region_destroy (region);
}

void
foo_scroll_area_begin_grab (FooScrollArea         *scroll_area,
                            FooScrollAreaEventFunc func,
                            gpointer               input_data)
{
    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));
    g_return_if_fail (!scroll_area->priv->grabbed);

    scroll_area->priv->grabbed = TRUE;
    scroll_area->priv->grab_func = func;
    scroll_area->priv->grab_data = input_data;

    /* FIXME: we should probably take a server grab */
    /* Also, maybe there should be support for setting the grab cursor */
}

void
foo_scroll_area_end_grab (FooScrollArea *scroll_area)
{
    g_return_if_fail (FOO_IS_SCROLL_AREA (scroll_area));

    scroll_area->priv->grabbed = FALSE;
    scroll_area->priv->grab_func = NULL;
    scroll_area->priv->grab_data = NULL;
}

gboolean
foo_scroll_area_is_grabbed (FooScrollArea *scroll_area)
{
    return scroll_area->priv->grabbed;
}

void
foo_scroll_area_set_viewport_pos (FooScrollArea *scroll_area,
                                  int            x,
                                  int            y)
{
    g_object_freeze_notify (G_OBJECT (scroll_area->priv->hadj));
    g_object_freeze_notify (G_OBJECT (scroll_area->priv->vadj));
    gtk_adjustment_set_value (scroll_area->priv->hadj, x);
    gtk_adjustment_set_value (scroll_area->priv->vadj, y);

    set_adjustment_values (scroll_area);
    g_object_thaw_notify (G_OBJECT (scroll_area->priv->hadj));
    g_object_thaw_notify (G_OBJECT (scroll_area->priv->vadj));
}

static gboolean
rect_contains (const GdkRectangle *rect,
               int                 x,
               int                 y)
{
    return (x >= rect->x               &&
            y >= rect->y               &&
            x  < rect->x + rect->width &&
            y  < rect->y + rect->height);
}

static void
stop_scrolling (FooScrollArea *area)
{
    if (area->priv->auto_scroll_info)
    {
        g_source_remove (area->priv->auto_scroll_info->timeout_id);
        g_timer_destroy (area->priv->auto_scroll_info->timer);
        g_free (area->priv->auto_scroll_info);
        area->priv->auto_scroll_info = NULL;
    }
}

static gboolean
scroll_idle (gpointer data)
{
    GdkRectangle viewport, new_viewport;
    FooScrollArea *area = data;
    AutoScrollInfo *info = area->priv->auto_scroll_info;
    int new_x, new_y;
    double elapsed;

    get_viewport (area, &viewport);

    elapsed = g_timer_elapsed (info->timer, NULL);

    info->res_x = elapsed * info->dx / 0.2;
    info->res_y = elapsed * info->dy / 0.2;

    new_x = viewport.x + info->res_x;
    new_y = viewport.y + info->res_y;

    foo_scroll_area_set_viewport_pos (area, new_x, new_y);

    get_viewport (area, &new_viewport);

    if (viewport.x == new_viewport.x &&
        viewport.y == new_viewport.y &&
        (info->res_x > 1.0  ||
         info->res_y > 1.0  ||
         info->res_x < -1.0 ||
         info->res_y < -1.0))
    {
        stop_scrolling (area);

        /* stop scrolling if it didn't have an effect */
        return FALSE;
    }

    return TRUE;
}

static void
ensure_scrolling (FooScrollArea *area,
                  int            dx,
                  int            dy)
{
    if (!area->priv->auto_scroll_info)
    {
        area->priv->auto_scroll_info = g_new0 (AutoScrollInfo, 1);
        area->priv->auto_scroll_info->timeout_id =
            g_idle_add (scroll_idle, area);
        area->priv->auto_scroll_info->timer = g_timer_new ();
    }

    area->priv->auto_scroll_info->dx = dx;
    area->priv->auto_scroll_info->dy = dy;
}

void
foo_scroll_area_auto_scroll (FooScrollArea      *scroll_area,
                             FooScrollAreaEvent *event)
{
    GdkRectangle viewport;

    get_viewport (scroll_area, &viewport);

    if (rect_contains (&viewport, event->x, event->y))
    {
        stop_scrolling (scroll_area);
    }
    else
    {
        int dx, dy;

        dx = dy = 0;

        if (event->y < viewport.y)
        {
            dy = event->y - viewport.y;
            dy = MIN (dy + 2, 0);
        }
        else if (event->y >= viewport.y + viewport.height)
        {
            dy = event->y - (viewport.y + viewport.height - 1);
            dy = MAX (dy - 2, 0);
        }

        if (event->x < viewport.x)
        {
            dx = event->x - viewport.x;
            dx = MIN (dx + 2, 0);
        }
        else if (event->x >= viewport.x + viewport.width)
        {
            dx = event->x - (viewport.x + viewport.width - 1);
            dx = MAX (dx - 2, 0);
        }

        ensure_scrolling (scroll_area, dx, dy);
    }
}

void
foo_scroll_area_begin_auto_scroll (FooScrollArea *scroll_area)
{
    /* noop for now */
}

void
foo_scroll_area_end_auto_scroll (FooScrollArea *scroll_area)
{
    stop_scrolling (scroll_area);
}
