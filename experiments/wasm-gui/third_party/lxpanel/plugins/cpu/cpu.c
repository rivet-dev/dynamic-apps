/*
 * CPU usage plugin to lxpanel
 *
 * Copyright (C) 2006-2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2009 Marty Jack <martyj19@comcast.net>
 *               2009 Jürgen Hötzel <juergen@archlinux.org>
 *               2012 Rafał Mużyło <galtgendo@gmail.com>
 *               2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2013 Marko Rauhamaa <marko@pacujo.net>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2015 Rafał Mużyło <galtgendo@gmail.com>
 *
 * This file is a part of LXPanel project.
 *
 * Copyright (C) 2004 by Alexandre Pereira da Silva <alexandre.pereira@poli.usp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
/*A little bug fixed by Mykola <mykola@2ka.mipt.ru>:) */

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "plugin.h"

#define BORDER_SIZE 2
#define PANEL_HEIGHT_DEFAULT 26 /* from panel defaults */

/* #include "../../dbg.h" */

typedef unsigned long long CPUTick;		/* Value from /proc/stat */
typedef float CPUSample;			/* Saved CPU utilization value as 0.0..1.0 */

struct cpu_stat {
    CPUTick u, n, s, i;				/* User, nice, system, idle */
};

/* Private context for CPU plugin. */
typedef struct {
    GdkColor foreground_color;			/* Foreground color for drawing area */
    GtkWidget * da;				/* Drawing area */
    cairo_surface_t * pixmap;				/* Pixmap to be drawn on drawing area */

    guint timer;				/* Timer for periodic update */
    CPUSample * stats_cpu;			/* Ring buffer of CPU utilization values */
    unsigned int ring_cursor;			/* Cursor for ring buffer */
    guint pixmap_width;				/* Width of drawing area pixmap; also size of ring buffer; does not include border size */
    guint pixmap_height;			/* Height of drawing area pixmap; does not include border size */
    struct cpu_stat previous_cpu_stat;		/* Previous value of cpu_stat */
} CPUPlugin;

static void redraw_pixmap(CPUPlugin * c);
static gboolean cpu_update(CPUPlugin * c);
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c);
#if !GTK_CHECK_VERSION(3, 0, 0)
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c);
#else
static gboolean draw(GtkWidget * widget, cairo_t * cr, CPUPlugin * c);
#endif

static void cpu_destructor(gpointer user_data);

/* Redraw after timer callback or resize. */
static void redraw_pixmap(CPUPlugin * c)
{
    cairo_t * cr = cairo_create(c->pixmap);
    GtkStyle * style = gtk_widget_get_style(c->da);
    cairo_set_line_width (cr, 1.0);
    /* Erase pixmap. */
    cairo_rectangle(cr, 0, 0, c->pixmap_width, c->pixmap_height);
    gdk_cairo_set_source_color(cr, &style->black);
    cairo_fill(cr);

    /* Recompute pixmap. */
    unsigned int i;
    unsigned int drawing_cursor = c->ring_cursor;
    gdk_cairo_set_source_color(cr, &c->foreground_color);
    for (i = 0; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the CPU usage graph. */
        if (c->stats_cpu[drawing_cursor] != 0.0)
        {
            cairo_move_to(cr, i + 0.5, c->pixmap_height);
            cairo_line_to(cr, i + 0.5, c->pixmap_height - c->stats_cpu[drawing_cursor] * c->pixmap_height);
            cairo_stroke(cr);
        }

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
        if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    /* check_cairo_status(cr); */
    cairo_destroy(cr);

    /* Redraw pixmap. */
    gtk_widget_queue_draw(c->da);
}

/* Periodic timer callback. */
static gboolean cpu_update(CPUPlugin * c)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    if ((c->stats_cpu != NULL) && (c->pixmap != NULL))
    {
        /* Open statistics file and scan out CPU usage. */
        struct cpu_stat cpu;
        FILE * stat = fopen("/proc/stat", "r");
        if (stat == NULL)
            return TRUE;
        int fscanf_result = fscanf(stat, "cpu %llu %llu %llu %llu", &cpu.u, &cpu.n, &cpu.s, &cpu.i);
        fclose(stat);

        /* Ensure that fscanf succeeded. */
        if (fscanf_result == 4)
        {
            /* Compute delta from previous statistics. */
            struct cpu_stat cpu_delta;
            cpu_delta.u = cpu.u - c->previous_cpu_stat.u;
            cpu_delta.n = cpu.n - c->previous_cpu_stat.n;
            cpu_delta.s = cpu.s - c->previous_cpu_stat.s;
            cpu_delta.i = cpu.i - c->previous_cpu_stat.i;

            /* Copy current to previous. */
            memcpy(&c->previous_cpu_stat, &cpu, sizeof(struct cpu_stat));

            /* Compute user+nice+system as a fraction of total.
             * Introduce this sample to ring buffer, increment and wrap ring buffer cursor. */
            float cpu_uns = cpu_delta.u + cpu_delta.n + cpu_delta.s;
            c->stats_cpu[c->ring_cursor] = cpu_uns / (cpu_uns + cpu_delta.i);
            c->ring_cursor += 1;
            if (c->ring_cursor >= c->pixmap_width)
                c->ring_cursor = 0;

            /* Redraw with the new sample. */
            redraw_pixmap(c);
        }
    }
    return TRUE;
}

/* Handler for configure_event on drawing area. */
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, CPUPlugin * c)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(widget, &allocation);
    /* Allocate pixmap and statistics buffer without border pixels. */
    guint new_pixmap_width = MAX(allocation.width - BORDER_SIZE * 2, 0);
    guint new_pixmap_height = MAX(allocation.height - BORDER_SIZE * 2, 0);
    if ((new_pixmap_width > 0) && (new_pixmap_height > 0))
    {
        /* If statistics buffer does not exist or it changed size, reallocate and preserve existing data. */
        if ((c->stats_cpu == NULL) || (new_pixmap_width != c->pixmap_width))
        {
            CPUSample * new_stats_cpu = g_new0(typeof(*c->stats_cpu), new_pixmap_width);
            if (c->stats_cpu != NULL)
            {
                if (new_pixmap_width > c->pixmap_width)
                {
                    /* New allocation is larger.
                     * Introduce new "oldest" samples of zero following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[new_pixmap_width - c->pixmap_width + c->ring_cursor],
                        &c->stats_cpu[c->ring_cursor], (c->pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else if (c->ring_cursor <= new_pixmap_width)
                {
                    /* New allocation is smaller, but still larger than the ring buffer cursor.
                     * Discard the oldest samples following the cursor. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[0], c->ring_cursor * sizeof(CPUSample));
                    memcpy(&new_stats_cpu[c->ring_cursor],
                        &c->stats_cpu[c->pixmap_width - new_pixmap_width + c->ring_cursor], (new_pixmap_width - c->ring_cursor) * sizeof(CPUSample));
                }
                else
                {
                    /* New allocation is smaller, and also smaller than the ring buffer cursor.
                     * Discard all oldest samples following the ring buffer cursor and additional samples at the beginning of the buffer. */
                    memcpy(&new_stats_cpu[0],
                        &c->stats_cpu[c->ring_cursor - new_pixmap_width], new_pixmap_width * sizeof(CPUSample));
                    c->ring_cursor = 0;
                }
                g_free(c->stats_cpu);
            }
            c->stats_cpu = new_stats_cpu;
        }

        /* Allocate or reallocate pixmap. */
        c->pixmap_width = new_pixmap_width;
        c->pixmap_height = new_pixmap_height;
        if (c->pixmap)
            cairo_surface_destroy(c->pixmap);
        c->pixmap = cairo_image_surface_create(CAIRO_FORMAT_RGB24, c->pixmap_width, c->pixmap_height);
        /* check_cairo_surface_status(&c->pixmap); */

        /* Redraw pixmap at the new size. */
        redraw_pixmap(c);
    }
    return TRUE;
}

/* Handler for expose_event on drawing area. */
#if !GTK_CHECK_VERSION(3, 0, 0)
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, CPUPlugin * c)
#else
static gboolean draw(GtkWidget * widget, cairo_t * cr, CPUPlugin * c)
#endif
{
    /* Draw the requested part of the pixmap onto the drawing area.
     * Translate it in both x and y by the border size. */
    if (c->pixmap != NULL)
    {
#if !GTK_CHECK_VERSION(3, 0, 0)
        cairo_t * cr = gdk_cairo_create(gtk_widget_get_window(widget));
        GtkStyle * style = gtk_widget_get_style(c->da);
        gdk_cairo_region(cr, event->region);
        cairo_clip(cr);
        gdk_cairo_set_source_color(cr, &style->black);
#else
        cairo_set_source_rgb(cr, 0, 0, 0); // FIXME: use black color from style
#endif
        cairo_set_source_surface(cr, c->pixmap,
              BORDER_SIZE, BORDER_SIZE);
        cairo_paint(cr);
        /* check_cairo_status(cr); */
#if !GTK_CHECK_VERSION(3, 0, 0)
        cairo_destroy(cr);
#endif
    }
    return FALSE;
}

/* Plugin constructor. */
static GtkWidget *cpu_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    CPUPlugin * c = g_new0(CPUPlugin, 1);
    GtkWidget * p;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_event_box_new();
    gtk_widget_set_has_window(p, FALSE);
    lxpanel_plugin_set_data(p, c, cpu_destructor);

    /* Allocate drawing area as a child of top level widget. */
    c->da = gtk_drawing_area_new();
    gtk_widget_add_events(c->da, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                 GDK_BUTTON_MOTION_MASK);
    gtk_widget_set_size_request(c->da, 40, PANEL_HEIGHT_DEFAULT);
    gtk_container_add(GTK_CONTAINER(p), c->da);

    /* Clone a graphics context and set "green" as its foreground color.
     * We will use this to draw the graph. */
    gdk_color_parse("green",  &c->foreground_color);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(c->da), "configure-event", G_CALLBACK(configure_event), (gpointer) c);
#if !GTK_CHECK_VERSION(3, 0, 0)
    g_signal_connect(G_OBJECT(c->da), "expose-event", G_CALLBACK(expose_event), (gpointer) c);
#else
    g_signal_connect(G_OBJECT(c->da), "draw", G_CALLBACK(draw), (gpointer) c);
#endif

    /* Show the widget.  Connect a timer to refresh the statistics. */
    gtk_widget_show(c->da);
    c->timer = g_timeout_add(1500, (GSourceFunc) cpu_update, (gpointer) c);
    return p;
}

/* Plugin destructor. */
static void cpu_destructor(gpointer user_data)
{
    CPUPlugin * c = (CPUPlugin *)user_data;

    /* Disconnect the timer. */
    g_source_remove(c->timer);

    /* Deallocate memory. */
    cairo_surface_destroy(c->pixmap);
    g_free(c->stats_cpu);
    g_free(c);
}

FM_DEFINE_MODULE(lxpanel_gtk, cpu)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("CPU Usage Monitor"),
    .description = N_("Display CPU usage"),
    .new_instance = cpu_constructor,
};
