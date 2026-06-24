/* Copyright 2006, 2007, 2008, Soren Sandmann <sandmann@daimi.au.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <cairo.h>
#include <gtk/gtk.h>

#define GdkRegion cairo_region_t
#define gdk_region_new cairo_region_create
#define gdk_region_destroy cairo_region_destroy
#define gdk_region_rectangle cairo_region_create_rectangle
#define gdk_region_subtract cairo_region_subtract
#define gdk_region_intersect cairo_region_intersect
#define gdk_region_empty cairo_region_is_empty
#define gdk_region_copy cairo_region_copy
#define gdk_region_get_clipbox cairo_region_get_extents
#define gdk_region_offset cairo_region_translate
#define gdk_region_point_in cairo_region_contains_point
#define gdk_region_union cairo_region_union

#define FOO_TYPE_SCROLL_AREA            (foo_scroll_area_get_type ())
#define FOO_SCROLL_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FOO_TYPE_SCROLL_AREA, FooScrollArea))
#define FOO_SCROLL_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  FOO_TYPE_SCROLL_AREA, FooScrollAreaClass))
#define FOO_IS_SCROLL_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FOO_TYPE_SCROLL_AREA))
#define FOO_IS_SCROLL_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  FOO_TYPE_SCROLL_AREA))
#define FOO_SCROLL_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  FOO_TYPE_SCROLL_AREA, FooScrollAreaClass))

typedef struct FooScrollArea FooScrollArea;
typedef struct FooScrollAreaClass FooScrollAreaClass;
typedef struct FooScrollAreaPrivate FooScrollAreaPrivate;
typedef struct FooScrollAreaEvent FooScrollAreaEvent;

typedef enum
{
    FOO_BUTTON_PRESS,
    FOO_BUTTON_RELEASE,
    FOO_MOTION,
    FOO_MOTION_OUTSIDE
} FooScrollAreaEventType;

struct FooScrollAreaEvent
{
    FooScrollAreaEventType type;
    int                    x;
    int                    y;
};

typedef void (* FooScrollAreaEventFunc) (FooScrollArea      *area,
                                         FooScrollAreaEvent *event,
                                         gpointer            data);

struct FooScrollArea
{
    GtkContainer parent_instance;

    FooScrollAreaPrivate *priv;
};

struct FooScrollAreaClass
{
    GtkContainerClass parent_class;

    void (*set_scroll_adjustments) (FooScrollArea *scroll_area,
                                    GtkAdjustment *hadjustment,
                                    GtkAdjustment *vadjustment);

    void (*viewport_changed)       (FooScrollArea *scroll_area,
                                    GdkRectangle  *old_viewport,
                                    GdkRectangle  *new_viewport);

    void (*paint)                  (FooScrollArea *scroll_area,
                                    cairo_t       *cr,
                                    GdkRectangle  *extents,
                                    GdkRegion     *region);
};

GType foo_scroll_area_get_type (void);

FooScrollArea *foo_scroll_area_new (void);

/* Set the requisition for the widget. */
void foo_scroll_area_set_min_size          (FooScrollArea          *scroll_area,
                                            int                     min_width,
                                            int                     min_height);

/* Set how much of the canvas can be scrolled into view */
void foo_scroll_area_set_size              (FooScrollArea          *scroll_area,
                                            int                     width,
                                            int                     height);
void foo_scroll_area_set_size_fixed_y      (FooScrollArea          *scroll_area,
                                            int                     width,
                                            int                     height,
                                            int                     old_y,
                                            int                     new_y);
void foo_scroll_area_set_viewport_pos      (FooScrollArea          *scroll_area,
                                            int                     x,
                                            int                     y);
void foo_scroll_area_get_viewport          (FooScrollArea          *scroll_area,
                                            GdkRectangle           *viewport);
void foo_scroll_area_add_input_from_stroke (FooScrollArea          *scroll_area,
                                            cairo_t	               *cr,
                                            FooScrollAreaEventFunc  func,
                                            gpointer                data);
void foo_scroll_area_add_input_from_fill   (FooScrollArea          *scroll_area,
                                            cairo_t                *cr,
                                            FooScrollAreaEventFunc  func,
                                            gpointer                data);
void foo_scroll_area_invalidate_region     (FooScrollArea          *area,
                                            GdkRegion              *region);
void foo_scroll_area_invalidate            (FooScrollArea          *scroll_area);
void foo_scroll_area_invalidate_rect       (FooScrollArea          *scroll_area,
                                            int                     x,
                                            int                     y,
                                            int                     width,
                                            int                     height);
void     foo_scroll_area_begin_grab        (FooScrollArea          *scroll_area,
                                            FooScrollAreaEventFunc  func,
                                            gpointer                input_data);
void     foo_scroll_area_end_grab          (FooScrollArea          *scroll_area);
gboolean foo_scroll_area_is_grabbed        (FooScrollArea          *scroll_area);
void     foo_scroll_area_begin_auto_scroll (FooScrollArea          *scroll_area);
void     foo_scroll_area_auto_scroll       (FooScrollArea          *scroll_area,
                                            FooScrollAreaEvent     *event);
void     foo_scroll_area_end_auto_scroll   (FooScrollArea          *scroll_area);
