/**
 * Plugin for the lxpanel.
 *
 * Displays several monitors in the panel.
 *
 * A lot of code in this plugin comes from the CPU plugin (that only displays a
 * CPU monitor), that is distributed under the following terms :
 *
 * Copyright (C) 2010 Cyril Roelandt <steap@users.sourceforge.net>
 *               2012-2014 Henry Gebhardt <hsggebhardt@googlemail.com>
 *               2012 Rafał Mużyło <galtgendo@gmail.com>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2015 Rafał Mużyło <galtgendo@gmail.com>
 *               2018 Mamoru TASAKA <mtasaka@fedoraproject.org>
 *               2019 Carles Pina i Estany <carles@pina.cat>
 *
 * <terms>
 * Copyright (c) 2008-2014 LxDE Developers, see the file AUTHORS for details.
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
 * </terms>
 *
 */

/*
 * HOWTO : Add your own monitor for the resource "foo".
 *
 * 1) Write the foo_update() function, that fills in the stats.
 * 2) Write the foo_tooltip_update() function, that updates your tooltip. This
 *    is optional, but recommended.
 * 3) Add a #define FOO_POSITION, and increment N_MONITORS.
 * 4) Add :
 *     - the default color of your plugin ("default_colors" table)
 *     - the update function ("update_functions" table)
 *     - the tooltip update function ("tooltip_update" table)
 * 5) Configuration :
 *     - edit the monitors_config() function so that a "Display FOO usage"
 *     checkbox appears in the prefs dialog.
 *     - edit the monitors_save() function so that a "DisplayFOO" string appears
 *     in the config file ("~/.config/lxpanel/<profile>/config")
 *     - edit the monitors_config() function so that a "FOO color" entry appears
 *       in the prefs dialog.
 *     - edit the monitors_save() function so that a "FOOColor" string appears
 *       in the config file.
 *     - edit the monitors_constructor() function so that options are correctly
 *     aplied. Adding something like :
 *
 *     else if (g_ascii_strcasecmp(s.t[0], "DisplayFOO") == 0)
 *         mp->displayed_monitors[FOO_POSITION] = atoi(s.t[1])
 *     else if (g_ascii_strcasecmp(s.t[0], "FOOColor") == 0)
 *         colors[FOO_POSITION] = g_strndup(s.t[1], COLOR_SIZE-1);
 *
 *     should be enough.
 * 6) Enjoy.
 */

/*
 * FIXME : known BUGS :
 *     - when removing a monitor and re-adding it, it is drawn with a white
 *     border of BORDER_SIZE pixels around it.
 */

#include <stdlib.h>
#include <glib/gi18n.h>
#include <errno.h>
#include <libfm/fm-gtk.h>

#include "plugin.h"

#include "dbg.h"


#define PLUGIN_NAME      "MonitorsPlugin"
#define BORDER_SIZE      2                  /* Pixels               */
#define DEFAULT_WIDTH    40                 /* Pixels               */
#define UPDATE_PERIOD    1                  /* Seconds              */
#define COLOR_SIZE       8                  /* In chars : #xxxxxx\0 */

#ifndef ENTER
#define ENTER fprintf(stderr, "Entering %s\n", __func__);
#endif

/*
 * Stats are stored in a circular buffer.
 * Newest values are on the left of the ring cursor.
 * Oldest values are on the right of the ring cursor.
 */
typedef float stats_set;

struct Monitor {
    GdkColor     foreground_color;  /* Foreground color for drawing area      */
    GtkWidget    *da;               /* Drawing area                           */
    cairo_surface_t    *pixmap;     /* Pixmap to be drawn on drawing area     */
    gint         pixmap_width;      /* Width and size of the buffer           */
    gint         pixmap_height;     /* Does not include border size           */
    stats_set    *stats;            /* Circular buffer of values              */
    stats_set    total;             /* Maximum possible value, as in mem_total*/
    gint         ring_cursor;       /* Cursor for ring/circular buffer        */
    gchar        *color;            /* Color of the graph                     */
    gboolean     (*update) (struct Monitor *); /* Update function             */
    void         (*update_tooltip) (struct Monitor *);
};

typedef struct Monitor Monitor;
typedef gboolean (*update_func) (Monitor *);
typedef void (*tooltip_update_func) (Monitor *);

/*
 * Position of our monitors : monitor 0 will always be on the left of the
 * plugin, monitor 1 on the right of monitor 0 (or on the left of the plugin if
 * monitor 0 is not displayed), etc.
 */
#define CPU_POSITION    0
#define MEM_POSITION    1
#define N_MONITORS      2

/* Our plugin */
typedef struct {
    LXPanel *panel;
    config_setting_t *settings;
    Monitor  *monitors[N_MONITORS];          /* Monitors                      */
    int      displayed_monitors[N_MONITORS]; /* Booleans                      */
    char     *action;                        /* What to do on click           */
    guint    timer;                          /* Timer for regular updates     */
} MonitorsPlugin;

/*
 * Prototypes
 */
static void monitor_set_foreground_color(MonitorsPlugin *, Monitor *, const gchar *);

/* CPU Monitor */
static gboolean cpu_update(Monitor *);
static void     cpu_tooltip_update (Monitor *m);

/* RAM Monitor */
static gboolean mem_update(Monitor *);
static void     mem_tooltip_update (Monitor *m);


static gboolean configure_event(GtkWidget*, GdkEventConfigure*, gpointer);
#if !GTK_CHECK_VERSION(3, 0, 0)
static gboolean expose_event(GtkWidget *, GdkEventExpose *, Monitor *);
#else
static gboolean draw(GtkWidget *, cairo_t *, Monitor *);
#endif
static void redraw_pixmap (Monitor *m);

/* Monitors functions */
static void monitors_destructor(gpointer);
static gboolean monitors_apply_config(gpointer);


/******************************************************************************
 *                              Monitor functions                             *
 ******************************************************************************/
static Monitor*
monitor_init(MonitorsPlugin *mp, Monitor *m, gchar *color)
{
    ENTER;

    m->da = gtk_drawing_area_new();
    gtk_widget_add_events(m->da, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                 GDK_BUTTON_MOTION_MASK);
    gtk_widget_set_size_request(m->da, DEFAULT_WIDTH, panel_get_height(mp->panel));

    monitor_set_foreground_color(mp, m, color);

    /* Signals */
    g_signal_connect(G_OBJECT(m->da), "configure-event",
        G_CALLBACK(configure_event), (gpointer) m);
#if !GTK_CHECK_VERSION(3, 0, 0)
    g_signal_connect (G_OBJECT(m->da), "expose-event",
        G_CALLBACK(expose_event), (gpointer) m);
#else
    g_signal_connect (G_OBJECT(m->da), "draw",
        G_CALLBACK(draw), (gpointer) m);
#endif

    return m;
}

static void
monitor_free(Monitor *m)
{
    if (!m)
        return;

    g_free(m->color);
    if (m->pixmap)
        cairo_surface_destroy(m->pixmap);
    if (m->stats)
        g_free(m->stats);
    g_free(m);

    return;
}

static void
monitor_set_foreground_color(MonitorsPlugin *mp, Monitor *m, const gchar *color)
{
    g_free(m->color);
    m->color = g_strndup(color, COLOR_SIZE - 1);
    gdk_color_parse(color, &m->foreground_color);
}
/******************************************************************************
 *                          End of monitor functions                          *
 ******************************************************************************/

/******************************************************************************
 *                                 CPU monitor                                *
 ******************************************************************************/
typedef unsigned long long CPUTick;/* Value from /proc/stat                   */
typedef float CPUSample;	   /* Saved CPU utilization value as 0.0..1.0 */

struct cpu_stat {
    CPUTick u, n, s, i;		  /* User, nice, system, idle */
};

static gboolean
cpu_update(Monitor * c)
{
    static struct cpu_stat previous_cpu_stat = { 0, 0, 0, 0 };

    if ((c->stats != NULL) && (c->pixmap != NULL))
    {
        /* Open statistics file and scan out CPU usage. */
        struct cpu_stat cpu;
        FILE * stat = fopen("/proc/stat", "r");
        if (stat == NULL)
            return TRUE;
        int fscanf_result = fscanf(stat, "cpu %llu %llu %llu %llu",
                                    &cpu.u, &cpu.n, &cpu.s, &cpu.i);
        fclose(stat);

        /* Ensure that fscanf succeeded. */
        if (fscanf_result == 4)
        {
            /* Comcolors delta from previous statistics. */
            struct cpu_stat cpu_delta;
            cpu_delta.u = cpu.u - previous_cpu_stat.u;
            cpu_delta.n = cpu.n - previous_cpu_stat.n;
            cpu_delta.s = cpu.s - previous_cpu_stat.s;
            cpu_delta.i = cpu.i - previous_cpu_stat.i;

            /* Copy current to previous. */
            memcpy(&previous_cpu_stat, &cpu, sizeof(struct cpu_stat));

            /* Comcolors user+nice+system as a fraction of total.
             * Introduce this sample to ring buffer, increment and wrap ring
             * buffer cursor. */
            float cpu_uns = cpu_delta.u + cpu_delta.n + cpu_delta.s;
            c->stats[c->ring_cursor] = cpu_uns / (cpu_uns + cpu_delta.i);
            c->ring_cursor += 1;
            if (c->ring_cursor >= c->pixmap_width)
                c->ring_cursor = 0;

            /* Redraw with the new sample. */
            redraw_pixmap(c);
        }
    }
    return TRUE;
}

static void
cpu_tooltip_update (Monitor *m)
{
    if (m && m->stats) {
        gchar *tooltip_text;
        gint ring_pos = (m->ring_cursor == 0)
            ? m->pixmap_width - 1 : m->ring_cursor - 1;
        tooltip_text = g_strdup_printf(_("CPU usage: %.2f%%"),
                m->stats[ring_pos] * 100);
        gtk_widget_set_tooltip_text(m->da, tooltip_text);
        g_free(tooltip_text);
    }
}

/******************************************************************************
 *                            End of CPU Monitor                              *
 ******************************************************************************/

/******************************************************************************
 *                               RAM Monitor                                  *
 ******************************************************************************/
static gboolean
mem_update(Monitor * m)
{
    ENTER;

    FILE *meminfo;
    char buf[80];
    long int mem_total = 0;
    long int mem_free  = 0;
    long int mem_buffers = 0;
    long int mem_cached = 0;
    long int mem_sreclaimable = 0;
    unsigned int readmask = 0x10 | 0x8 | 0x4 | 0x2 | 0x1;

    if (!m->stats || !m->pixmap)
        RET(TRUE);

    meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        g_warning("monitors: Could not open /proc/meminfo: %d, %s",
                  errno, strerror(errno));
        RET(FALSE);
    }

    while (readmask && fgets(buf, sizeof(buf), meminfo)) {
        if (sscanf(buf, "MemTotal: %ld kB\n", &mem_total) == 1) {
            readmask ^= 0x1;
            continue;
        }
        if (sscanf(buf, "MemFree: %ld kB\n", &mem_free) == 1) {
            readmask ^= 0x2;
            continue;
        }
        if (sscanf(buf, "Buffers: %ld kB\n", &mem_buffers) == 1) {
            readmask ^= 0x4;
            continue;
        }
        if (sscanf(buf, "Cached: %ld kB\n", &mem_cached) == 1) {
            readmask ^= 0x8;
            continue;
        }
        if (sscanf(buf, "SReclaimable: %ld kB\n", &mem_sreclaimable) == 1) {
            readmask ^= 0x10;
            continue;
        }
    }

    fclose(meminfo);

    if (readmask) {
        g_warning("monitors: Couldn't read all values from /proc/meminfo: "
                  "readmask %x", readmask);
        RET(FALSE);
    }

    m->total = mem_total;

    /* Adding stats to the buffer:
     * It is debatable if 'mem_buffers' counts as free or not. I'll go with
     * 'free', because it can be flushed fairly quickly, and generally
     * isn't necessary to keep in memory.
     * It is hard to draw the line, which caches should be counted as free,
     * and which not. 'free' command line utility from procps counts
     * SReclaimable as free so it's counted it here as well (note that
     * 'man free' doesn't specify this)
     * 'mem_cached' definitely counts as 'free' because it is immediately
     * released should any application need it. */
    m->stats[m->ring_cursor] = (mem_total - mem_buffers - mem_free -
            mem_cached - mem_sreclaimable) / (float)mem_total;

    m->ring_cursor++;
    if (m->ring_cursor >= m->pixmap_width)
        m->ring_cursor = 0;

    /* Redraw the pixmap, with the new sample */
    redraw_pixmap (m);

    RET(TRUE);
}

static void
mem_tooltip_update (Monitor *m)
{
    if (m && m->stats) {
        gchar *tooltip_text;
        gint ring_pos = (m->ring_cursor == 0)
            ? m->pixmap_width - 1 : m->ring_cursor - 1;
        tooltip_text = g_strdup_printf(_("RAM usage: %.1fMB (%.2f%%)"),
                m->stats[ring_pos] * m->total / 1024,
                m->stats[ring_pos] * 100);
        gtk_widget_set_tooltip_text(m->da, tooltip_text);
        g_free(tooltip_text);
    }
}
/******************************************************************************
 *                             End of RAM Monitor                             *
 ******************************************************************************/

/******************************************************************************
 *                            Basic events handlers                           *
 ******************************************************************************/
static gboolean
configure_event(GtkWidget* widget, GdkEventConfigure* dummy, gpointer data)
{
    (void) dummy;
    GtkAllocation allocation;

    int new_pixmap_width, new_pixmap_height;

    gtk_widget_get_allocation(widget, &allocation);
    new_pixmap_width = allocation.width - BORDER_SIZE * 2;
    new_pixmap_height = allocation.height - BORDER_SIZE *2;
    Monitor *m;

    m = (Monitor *) data;

    if (new_pixmap_width > 0 && new_pixmap_height > 0)
    {
        /*
         * If the stats buffer does not exist (first time we get inside this
         * function) or its size changed, reallocate the buffer and preserve
         * existing data.
         */
        if (!m->stats || (new_pixmap_width != m->pixmap_width))
        {
            stats_set *new_stats = g_new0(stats_set, new_pixmap_width);

            if (!new_stats)
                return TRUE;

            if (m->stats)
            {
                /* New allocation is larger.
                 * Add new "oldest" samples of zero following the cursor*/
                if (new_pixmap_width > m->pixmap_width)
                {
                    /* Number of values between the ring cursor and the end of
                     * the buffer */
                    int nvalues = m->pixmap_width - m->ring_cursor;

                    memcpy(new_stats,
                           m->stats,
                           m->ring_cursor * sizeof (stats_set));
                    memcpy(new_stats + nvalues,
                           m->stats + m->ring_cursor,
                           nvalues * sizeof(stats_set));
                }
                /* New allocation is smaller, but still larger than the ring
                 * buffer cursor */
                else if (m->ring_cursor <= new_pixmap_width)
                {
                    /* Numver of values that can be stored between the end of
                     * the new buffer and the ring cursor */
                    int nvalues = new_pixmap_width - m->ring_cursor;
                    memcpy(new_stats,
                           m->stats,
                           m->ring_cursor * sizeof(stats_set));
                    memcpy(new_stats + m->ring_cursor,
                           m->stats + m->pixmap_width - nvalues,
                           nvalues * sizeof(stats_set));
                }
                /* New allocation is smaller, and also smaller than the ring
                 * buffer cursor.  Discard all oldest samples following the ring
                 * buffer cursor and additional samples at the beginning of the
                 * buffer. */
                else
                {
                    memcpy(new_stats,
                           m->stats + m->ring_cursor - new_pixmap_width,
                           new_pixmap_width * sizeof(stats_set));
                }
                g_free(m->stats);
            }
            m->stats = new_stats;
        }

        m->pixmap_width = new_pixmap_width;
        m->pixmap_height = new_pixmap_height;
        if (m->pixmap)
            cairo_surface_destroy(m->pixmap);
        m->pixmap = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
                                   m->pixmap_width,
                                   m->pixmap_height);
        check_cairo_surface_status(&m->pixmap);
        redraw_pixmap(m);
    }

    return TRUE;
}

#if !GTK_CHECK_VERSION(3, 0, 0)
static gboolean
expose_event(GtkWidget * widget, GdkEventExpose * event, Monitor *m)
#else
static gboolean
draw(GtkWidget * widget, cairo_t * cr, Monitor *m)
#endif
{
    /* Draw the requested part of the pixmap onto the drawing area.
     * Translate it in both x and y by the border size. */
    if (m->pixmap != NULL)
    {
#if !GTK_CHECK_VERSION(3, 0, 0)
        cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
        GtkStyle *style = gtk_widget_get_style(m->da);
        gdk_cairo_region(cr, event->region);
        cairo_clip(cr);
        gdk_cairo_set_source_color(cr, &style->black);
#else
        cairo_set_source_rgb(cr, 0, 0, 0); // FIXME: set the color from style
#endif
        cairo_set_source_surface(cr, m->pixmap, BORDER_SIZE, BORDER_SIZE);
        cairo_paint(cr);
        check_cairo_status(cr);
#if !GTK_CHECK_VERSION(3, 0, 0)
        cairo_destroy(cr);
#endif
    }

    return FALSE;
}


static gboolean monitors_button_press_event(GtkWidget* widget, GdkEventButton* evt, LXPanel *panel)
{
    MonitorsPlugin* mp;

    if (evt->button != 1)
        return FALSE;

    mp = lxpanel_plugin_get_data(widget);
    if (mp->action != NULL)
        fm_launch_command_simple(NULL, NULL, 0, mp->action, NULL);
    else
        fm_launch_command_simple(NULL, NULL, 0, "lxtask", NULL);

    return TRUE;
}
/******************************************************************************
 *                       End of basic events handlers                         *
 ******************************************************************************/

static void
redraw_pixmap (Monitor *m)
{
    int i;
    cairo_t *cr = cairo_create(m->pixmap);
    GtkStyle *style = gtk_widget_get_style(m->da);

    cairo_set_line_width (cr, 1.0);

    /* Erase pixmap */
    gdk_cairo_set_source_color(cr, &style->black);
    cairo_paint(cr);

    gdk_cairo_set_source_color(cr, &m->foreground_color);
    for (i = 0; i < m->pixmap_width; i++)
    {
        unsigned int drawing_cursor = (m->ring_cursor + i) % m->pixmap_width;

        /* Draw one bar of the graph */
        cairo_move_to(cr, i + 0.5, m->pixmap_height);
        cairo_line_to(cr, i + 0.5, (1.0 - m->stats[drawing_cursor]) * m->pixmap_height);
        cairo_stroke(cr);
    }

    check_cairo_status(cr);
    cairo_destroy(cr);
    /* Redraw pixmap */
    gtk_widget_queue_draw(m->da);
}


static update_func update_functions [N_MONITORS] = {
    [CPU_POSITION] = cpu_update,
    [MEM_POSITION] = mem_update
};

static char *default_colors[N_MONITORS] = {
    [CPU_POSITION] = "#0000FF",
    [MEM_POSITION] = "#FF0000"
};


static tooltip_update_func tooltip_update[N_MONITORS] = {
    [CPU_POSITION] = cpu_tooltip_update,
    [MEM_POSITION] = mem_tooltip_update
};

/* Colors currently used. We cannot store them in the "struct Monitor"s where
 * they belong, because we free these when the user removes them. And since we
 * want the colors to stay the same even after removing/adding a widget... */
static char *colors[N_MONITORS] = {
    NULL,
    NULL
};

/*
 * This function is called every UPDATE_PERIOD seconds. It updates all
 * monitors.
 */
static gboolean
monitors_update(gpointer data)
{
    MonitorsPlugin *mp;
    int i;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    mp = (MonitorsPlugin *) data;
    if (!mp)
        RET(FALSE);

    for (i = 0; i < N_MONITORS; i++)
    {
        if (mp->monitors[i])
        {
            mp->monitors[i]->update(mp->monitors[i]);
            if (mp->monitors[i]->update_tooltip)
                mp->monitors[i]->update_tooltip(mp->monitors[i]);
        }
    }

    return TRUE;
}

static Monitor*
monitors_add_monitor (GtkWidget *p, MonitorsPlugin *mp, update_func update,
             tooltip_update_func update_tooltip, gchar *color)
{
    ENTER;

    Monitor *m;

    m = g_new0(Monitor, 1);
    m = monitor_init(mp, m, color);
    m->update = update;
    m->update_tooltip = update_tooltip;
    gtk_box_pack_start(GTK_BOX(p), m->da, FALSE, FALSE, 0);
    gtk_widget_show(m->da);

    RET(m);
}

static GtkWidget *
monitors_constructor(LXPanel *panel, config_setting_t *settings)
{
    ENTER;
    int i;
    MonitorsPlugin *mp;
    GtkWidget *p;
    const char *tmp;

    mp = g_new0(MonitorsPlugin, 1);
    mp->panel = panel;
    mp->settings = settings;

    p = gtk_hbox_new(TRUE, 2);
    lxpanel_plugin_set_data(p, mp, monitors_destructor);

    /* First time we use this plugin : only display CPU usage */
    mp->displayed_monitors[CPU_POSITION] = 1;

    /* Apply options */
    config_setting_lookup_int(settings, "DisplayCPU",
                              &mp->displayed_monitors[CPU_POSITION]);
    config_setting_lookup_int(settings, "DisplayRAM",
                              &mp->displayed_monitors[MEM_POSITION]);
    if (config_setting_lookup_string(settings, "Action", &tmp))
        mp->action = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "CPUColor", &tmp))
        colors[CPU_POSITION] = g_strndup(tmp, COLOR_SIZE-1);
    if (config_setting_lookup_string(settings, "RAMColor", &tmp))
        colors[MEM_POSITION] = g_strndup(tmp, COLOR_SIZE-1);

    /* Initializing monitors */
    for (i = 0; i < N_MONITORS; i++)
    {
        if (!colors[i])
            colors[i] = g_strndup(default_colors[i], COLOR_SIZE-1);

        if (mp->displayed_monitors[i])
        {
            mp->monitors[i] = monitors_add_monitor(p, mp,
                                                   update_functions[i],
                                                   tooltip_update[i],
                                                   colors[i]);
        }
    }

    /* Adding a timer : monitors will be updated every UPDATE_PERIOD
     * seconds */
    mp->timer = g_timeout_add_seconds(UPDATE_PERIOD, (GSourceFunc) monitors_update,
                              (gpointer) mp);
    RET(p);
}

static void
monitors_destructor(gpointer user_data)
{
    ENTER;
    int            i;
    MonitorsPlugin *mp;

    mp = (MonitorsPlugin *) user_data;

    /* Removing timer */
    g_source_remove(mp->timer);

    /* Freeing all monitors */
    for (i = 0; i < N_MONITORS; i++)
    {
        if (mp->monitors[i])
            monitor_free(mp->monitors[i]);
    }

    g_free(mp->action);
    g_free(mp);

    RET();
}


static GtkWidget *
monitors_config (LXPanel *panel, GtkWidget *p)
{
    ENTER;

    GtkWidget *dialog;
    MonitorsPlugin *mp;

    mp = lxpanel_plugin_get_data(p);

    dialog = lxpanel_generic_config_dlg(_("Resource monitors"),
        panel, monitors_apply_config, p,
        _("Display CPU usage"), &mp->displayed_monitors[0], CONF_TYPE_BOOL,
        _("CPU color"), &colors[CPU_POSITION], CONF_TYPE_STR,
        _("Display RAM usage"), &mp->displayed_monitors[1], CONF_TYPE_BOOL,
        _("RAM color"), &colors[MEM_POSITION], CONF_TYPE_STR,
        _("Action when clicked (default: lxtask)"), &mp->action, CONF_TYPE_STR,
        NULL);

    RET(dialog);
}

static gboolean
monitors_apply_config (gpointer user_data)
{
    ENTER;
    GtkWidget *p = user_data;
    MonitorsPlugin *mp;
    mp = lxpanel_plugin_get_data(p);

    int i;
    int current_n_monitors = 0;

start:
    for (i = 0; i < N_MONITORS; i++)
    {
        /* User may remove color value. In such case, reset to the default */
        if (!colors[i])
            colors[i] = g_strndup(default_colors[i], COLOR_SIZE-1);

        if (mp->displayed_monitors[i])
            current_n_monitors++;

        if (mp->displayed_monitors[i] && !mp->monitors[i])
        {
            /* We've just activated monitor<i> */
            mp->monitors[i] = monitors_add_monitor(p, mp,
                                                   update_functions[i],
                                                   tooltip_update[i],
                                                   colors[i]);
            /*
             * It is probably best for users if their monitors are always
             * displayed in the same order : the CPU monitor always on the left,
             * the RAM monitor always on the right of the CPU monitor (if the
             * CPU monitor is displayed), etc. That's why we do not just use
             * gtk_box_pack_start/gtk_box_pack_end, and use
             * gtk_box_reorder_child.
             */
            gtk_box_reorder_child(GTK_BOX(p),
                                  mp->monitors[i]->da,current_n_monitors-1);
        }
        else if (!mp->displayed_monitors[i] && mp->monitors[i])
        {
            /* We've just removed monitor<i> */
            gtk_widget_destroy(mp->monitors[i]->da);
            monitor_free(mp->monitors[i]);
            mp->monitors[i] = NULL;
        }
        if (mp->monitors[i] &&
            strncmp(mp->monitors[i]->color, colors[i], COLOR_SIZE) != 0)
        {
            /* We've changed the color */
            monitor_set_foreground_color(mp, mp->monitors[i], colors[i]);
        }
    }

    /* Workaround meant to prevent users to display no monitor at all.
     * FIXME : write something clean. When there is only one monitor displayed,
     * its toggle button should not be clickable in the prefs. */
    if (current_n_monitors == 0)
    {
        mp->displayed_monitors[0] = 1;
        goto start;
    }
    config_group_set_int(mp->settings, "DisplayCPU", mp->displayed_monitors[CPU_POSITION]);
    config_group_set_int(mp->settings, "DisplayRAM", mp->displayed_monitors[MEM_POSITION]);
    config_group_set_string(mp->settings, "Action", mp->action);
    config_group_set_string(mp->settings, "CPUColor",
                            mp->monitors[CPU_POSITION] ? colors[CPU_POSITION] : NULL);
    config_group_set_string(mp->settings, "RAMColor",
                            mp->monitors[MEM_POSITION] ? colors[MEM_POSITION] : NULL);

    RET(FALSE);
}


FM_DEFINE_MODULE(lxpanel_gtk, monitors)

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Resource monitors"),
    .description = N_("Display monitors (CPU, RAM)"),
    .new_instance = monitors_constructor,
    .config = monitors_config,
    .button_press_event = monitors_button_press_event
};

/* vim: set sw=4 sts=4 et : */
