/*
 * ACPI battery monitor plugin for LXPanel
 *
 * Copyright (C) 2007 by Greg McNew <gmcnew@gmail.com>
 * Copyright (C) 2008 by Hong Jen Yee <pcman.tw@gmail.com>
 * Copyright (C) 2009 by Juergen Hoetzel <juergen@archlinux.org>
 * Copyright (C) 2014-2019 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2015 Balló György <ballogyor@gmail.com>
 *               2015 Stanislav Kozina, Ersin <xersin@users.sf.net>
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
 *
 * This plugin monitors battery usage on ACPI-enabled systems by reading the
 * battery information found in /sys/class/power_supply. The update interval is
 * user-configurable and defaults to 3 second.
 *
 * The battery's remaining life is estimated from its current charge and current
 * rate of discharge. The user may configure an alarm command to be run when
 * their estimated remaining battery life reaches a certain level.
 */

/* FIXME:
 *  Here are somethings need to be improvec:
 *  1. Replace pthread stuff with gthread counterparts for portability.
 *  4. Handle failure gracefully under systems other than Linux.
*/

#include <glib.h>
#include <glib/gi18n.h>
#include <pthread.h> /* used by pthread_create() and alarmThread */
#include <semaphore.h> /* used by update() and alarmProcess() for alarms */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "dbg.h" /* for ENTER and RET macros */
#include "batt_sys.h"
#include "plugin.h" /* all other APIs including panel configuration */

/* The last MAX_SAMPLES samples are averaged when charge rates are evaluated.
   This helps prevent spikes in the "time left" values the user sees. */
#define MAX_SAMPLES 10

typedef struct {
    char *alarmCommand,
        *backgroundColor,
        *chargingColor1,
        *chargingColor2,
        *dischargingColor1,
        *dischargingColor2;
    GdkColor background,
        charging1,
        charging2,
        discharging1,
        discharging2;
    cairo_surface_t *pixmap;
    GtkContainer *box;
    GtkWidget *drawingArea;
    GtkOrientation orientation;
    unsigned int alarmTime,
        border,
        height,
        length,
        numSamples,
        requestedBorder,
        *rateSamples,
        rateSamplesSum,
        thickness,
        timer,
        state_elapsed_time,
        info_elapsed_time,
        wasCharging,
        width,
        hide_if_no_battery;
    int battery_number;
    sem_t alarmProcessLock;
    battery* b;
    gboolean has_ac_adapter;
    gboolean show_extended_information;
    LXPanel *panel;
    config_setting_t *settings;
} lx_battery;


typedef struct {
    char *command;
    sem_t *lock;
} Alarm;

static void destructor(gpointer data);
static void update_display(lx_battery *lx_b, gboolean repaint);

/* alarmProcess takes the address of a dynamically allocated alarm struct (which
   it must free). It ensures that alarm commands do not run concurrently. */
static void * alarmProcess(void *arg) {
    Alarm *a = (Alarm *) arg;

    sem_wait(a->lock);
    if (system(a->command) != 0)
        g_warning("plugin batt: failed to execute alarm command \"%s\"", a->command);
    sleep(51); /* do not spam messages more often than once a minute */
    sem_post(a->lock);

    g_free(a);
    return NULL;
}


static void append(gchar **tooltip, gchar *fmt, ...)
{
    gchar *old = *tooltip;
    gchar *new;
    va_list va;

    va_start(va, fmt);
    new = g_strdup_vprintf(fmt, va);
    va_end(va);

    *tooltip = g_strconcat(old, new, NULL);

    g_free(old);
    g_free(new);
}


/* Make a tooltip string, and display remaining charge time if the battery
   is charging or remaining life if it's discharging */
static gchar* make_tooltip(lx_battery* lx_b, gboolean isCharging)
{
    gchar * tooltip;
    gchar * indent = "  ";
    battery *b = lx_b->b;

    if (b == NULL)
        return NULL;

    if (isCharging) {
        if (lx_b->b->seconds > 0) {
            int hours = lx_b->b->seconds / 3600;
            int left_seconds = lx_b->b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            tooltip = g_strdup_printf(
                    _("Battery %d: %d%% charged, %d:%02d until full"),
                    lx_b->battery_number, lx_b->b->percentage,
                    hours,
                    minutes );
        }
        else
            goto _charged;
    } else {
        /* if we have enough rate information for battery */
        if (lx_b->b->percentage != 100) {
            int hours = lx_b->b->seconds / 3600;
            int left_seconds = lx_b->b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            tooltip = g_strdup_printf(
                    _("Battery %d: %d%% charged, %d:%02d left"),
                    lx_b->battery_number, lx_b->b->percentage,
                    hours,
                    minutes );
        } else {
_charged:
            tooltip = g_strdup_printf(
                    _("Battery %d: %d%% charged"),
                    lx_b->battery_number, lx_b->b->percentage);
        }
    }

    if (!lx_b->show_extended_information) {
        return tooltip;
    }

    if (b->energy_full_design != -1)
        append(&tooltip, _("\n%sEnergy full design:\t\t%5d mWh"), indent, b->energy_full_design);
    if (b->energy_full != -1)
        append(&tooltip, _("\n%sEnergy full:\t\t\t%5d mWh"), indent, b->energy_full);
    if (b->energy_now != -1)
        append(&tooltip, _("\n%sEnergy now:\t\t\t%5d mWh"), indent, b->energy_now);
    if (b->power_now != -1)
        append(&tooltip, _("\n%sPower now:\t\t\t%5d mW"), indent, b->power_now);

    if (b->charge_full_design != -1)
        append(&tooltip, _("\n%sCharge full design:\t%5d mAh"), indent, b->charge_full_design);
    if (b->charge_full != -1)
        append(&tooltip, _("\n%sCharge full:\t\t\t%5d mAh"), indent, b->charge_full);
    if (b->charge_now != -1)
        append(&tooltip, _("\n%sCharge now:\t\t\t%5d mAh"), indent, b->charge_now);
    if (b->current_now != -1)
        append(&tooltip, _("\n%sCurrent now:\t\t\t%5d mA"), indent, b->current_now);

    if (b->voltage_now != -1)
        append(&tooltip, _("\n%sCurrent Voltage:\t\t%.3lf V"), indent, b->voltage_now / 1000.0);

    return tooltip;
}

static void set_tooltip_text(lx_battery* lx_b)
{
    if (lx_b->b == NULL)
        return;
    gboolean isCharging = battery_is_charging(lx_b->b);
    gchar *tooltip = make_tooltip(lx_b, isCharging);
    gtk_widget_set_tooltip_text(lx_b->drawingArea, tooltip);
    g_free(tooltip);
}

/* FIXME:
   Don't repaint if percentage of remaining charge and remaining time aren't changed. */
void update_display(lx_battery *lx_b, gboolean repaint) {
    cairo_t *cr;
    battery *b = lx_b->b;
    /* unit: mW */
    int rate;
    gboolean isCharging;

    if (! lx_b->pixmap )
        return;

    cr = cairo_create(lx_b->pixmap);
    cairo_set_line_width (cr, 1.0);

    /* draw background */
    gdk_cairo_set_source_color(cr, &lx_b->background);
    cairo_rectangle(cr, 0, 0, lx_b->width, lx_b->height);
    cairo_fill(cr);

    /* no battery is found */
    if( b == NULL )
    {
        gtk_widget_set_tooltip_text( lx_b->drawingArea, _("No batteries found") );
        if (lx_b->hide_if_no_battery)
        {
            gtk_widget_hide(gtk_widget_get_parent(lx_b->drawingArea));
            repaint = FALSE;
        }
        goto update_done;
    }

    /* fixme: only one battery supported */

    rate = lx_b->b->current_now;
    if (lx_b->b->percentage == 100)
        isCharging = TRUE; /* if battery is fully charged then draw it green */
    else
        isCharging = battery_is_charging ( b );

    /* Consider running the alarm command */
    if ( !isCharging &&
        ( ( battery_get_remaining( b ) / 60 ) < (int)lx_b->alarmTime ) )
    {
        /* FIXME: this should be done using glibs process functions */
        /* FIXME: see bug #463: it should not spawn process all the time */
        /* Alarms should not run concurrently; determine whether an alarm is
           already running */
        int alarmCanRun;
        sem_getvalue(&(lx_b->alarmProcessLock), &alarmCanRun);

        /* Run the alarm command if it isn't already running */
        if (alarmCanRun) {

            Alarm *a = (Alarm *) malloc(sizeof(Alarm));
            a->command = lx_b->alarmCommand;
            a->lock = &(lx_b->alarmProcessLock);

            /* Manage the alarm process in a new thread, which which will be
               responsible for freeing the alarm struct it's given */
            pthread_t alarmThread;
            pthread_create(&alarmThread, NULL, alarmProcess, a);
        }
    }

    set_tooltip_text(lx_b);

    int chargeLevel = lx_b->b->percentage * lx_b->length / 100;

    if (lx_b->orientation == GTK_ORIENTATION_HORIZONTAL) {

        /* Draw the battery bar vertically, using color 1 for the left half and
           color 2 for the right half */
        gdk_cairo_set_source_color(cr,
                isCharging ? &lx_b->charging1 : &lx_b->discharging1);
        cairo_rectangle(cr, 0, lx_b->height - chargeLevel,
                        lx_b->width / 2, chargeLevel);
        cairo_fill(cr);
        gdk_cairo_set_source_color(cr,
                isCharging ? &lx_b->charging2 : &lx_b->discharging2);
        cairo_rectangle(cr, lx_b->width / 2, lx_b->height - chargeLevel,
                        (lx_b->width + 1) / 2, chargeLevel);
        cairo_fill(cr);

    }
    else {

        /* Draw the battery bar horizontally, using color 1 for the top half and
           color 2 for the bottom half */
        gdk_cairo_set_source_color(cr,
                isCharging ? &lx_b->charging1 : &lx_b->discharging1);
        cairo_rectangle(cr, 0, 0, chargeLevel, lx_b->height / 2);
        cairo_fill(cr);
        gdk_cairo_set_source_color(cr,
                isCharging ? &lx_b->charging2 : &lx_b->discharging2);
        cairo_rectangle(cr, 0, (lx_b->height + 1) / 2,
                        chargeLevel, lx_b->height / 2);
        cairo_fill(cr);

    }
    gtk_widget_show(gtk_widget_get_parent(lx_b->drawingArea));

update_done:
    if( repaint )
        gtk_widget_queue_draw( lx_b->drawingArea );

    check_cairo_status(cr);
    cairo_destroy(cr);
}

/* This callback is called every 3 seconds */
static int update_timout(lx_battery *lx_b) {
    battery *bat;
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    GDK_THREADS_ENTER();
    lx_b->state_elapsed_time++;
    lx_b->info_elapsed_time++;

    bat = battery_update( lx_b->b );
    if (bat == NULL)
    {
        battery_free(lx_b->b);

        /* maybe in the mean time a battery has been inserted. */
        lx_b->b = battery_get(lx_b->battery_number);
    }

    update_display( lx_b, TRUE );

    GDK_THREADS_LEAVE();
    return TRUE;
}

/* An update will be performed whenever the user clicks on the charge bar */
static gboolean buttonPressEvent(GtkWidget *p, GdkEventButton *event,
                                 LXPanel *panel)
{
    lx_battery *lx_b = lxpanel_plugin_get_data(p);

    update_display(lx_b, TRUE);
    /* FIXME: open some application for lid/power management may be? */

    return FALSE;
}

static gint configureEvent(GtkWidget *widget, GdkEventConfigure *event,
        lx_battery *lx_b)
{
    GtkAllocation allocation;

    ENTER;

    gtk_widget_get_allocation(widget, &allocation);
    if (allocation.width <= 1 && allocation.height <= 1)
    {
        /* If plugin is hidden currently then we get 1x1 here */
        RET(TRUE);
    }

    if (lx_b->pixmap)
        cairo_surface_destroy(lx_b->pixmap);

    /* Update the plugin's dimensions */
    lx_b->width = allocation.width;
    lx_b->height = allocation.height;
    if (lx_b->orientation == GTK_ORIENTATION_HORIZONTAL) {
        lx_b->length = lx_b->height;
    }
    else {
        lx_b->length = lx_b->width;
    }

    lx_b->pixmap = cairo_image_surface_create (CAIRO_FORMAT_RGB24, allocation.width,
                                               allocation.height);
    check_cairo_surface_status(&lx_b->pixmap);

    /* Perform an update so the bar will look right in its new orientation */
    update_display(lx_b, FALSE);

    /* we enforce border width here because panel sets it to 0 */
    gtk_container_set_border_width(lx_b->box, lx_b->border);

    RET(TRUE);
}


#if GTK_CHECK_VERSION(3, 0, 0)
static gint draw(GtkWidget *widget, cairo_t *cr, lx_battery *lx_b) {
#else
static gint exposeEvent(GtkWidget *widget, GdkEventExpose *event, lx_battery *lx_b) {
#endif

    ENTER;

#if GTK_CHECK_VERSION(3, 0, 0)
    cairo_set_source_rgb(cr, 0, 0, 0); // FIXME: set black color from the style
#else
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
    GtkStyle *style = gtk_widget_get_style(lx_b->drawingArea);

    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);

    gdk_cairo_set_source_color(cr, &style->black);
#endif
    cairo_set_source_surface(cr, lx_b->pixmap, 0, 0);
    cairo_paint(cr);

    check_cairo_status(cr);
#if !GTK_CHECK_VERSION(3, 0, 0)
    cairo_destroy(cr);
#endif

    RET(FALSE);
}

/* updates length, border, and height/width appropriate to orientation */
static void updateSizes(lx_battery *b)
{
    b->length = panel_get_height(b->panel);
    b->border = MIN(b->requestedBorder, (MAX(1, b->length) - 1) / 2);
    b->length -= 2 * b->border;
    if (b->orientation == GTK_ORIENTATION_HORIZONTAL)
        b->height = b->length;
    else
        b->width = b->length;
}


static GtkWidget * constructor(LXPanel *panel, config_setting_t *settings)
{
    ENTER;

    lx_battery *lx_b;
    GtkWidget *p;
    const char *str;
    int tmp_int;

    lx_b = g_new0(lx_battery, 1);

    /* get requested battery */
    if (config_setting_lookup_int(settings, "BatteryNumber", &tmp_int))
        lx_b->battery_number = MAX(0, tmp_int);
    lx_b->b = battery_get(lx_b->battery_number);

    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, lx_b, destructor);
    gtk_widget_set_has_window(p, FALSE);

    lx_b->box = GTK_CONTAINER(p);
    lx_b->drawingArea = gtk_drawing_area_new();
    gtk_widget_add_events(lx_b->drawingArea, GDK_BUTTON_PRESS_MASK |
                                             GDK_BUTTON_RELEASE_MASK |
                                             GDK_BUTTON_MOTION_MASK);

    gtk_container_add(lx_b->box, lx_b->drawingArea);

    lx_b->orientation = panel_get_orientation(panel);

    gtk_widget_show(lx_b->drawingArea);

    sem_init(&(lx_b->alarmProcessLock), 0, 1);

    lx_b->alarmCommand = lx_b->backgroundColor = lx_b->chargingColor1 = lx_b->chargingColor2
            = lx_b->dischargingColor1 = lx_b->dischargingColor2 = NULL;

    /* Set default values for integers */
    lx_b->alarmTime = 5;
    lx_b->requestedBorder = 1;
    lx_b->thickness = 8;

    /* remember instance data */
    lx_b->panel = panel;
    lx_b->settings = settings;

    lx_b->show_extended_information = FALSE;

    if (config_setting_lookup_int(settings, "HideIfNoBattery", &tmp_int))
        lx_b->hide_if_no_battery = (tmp_int != 0);
    if (config_setting_lookup_string(settings, "AlarmCommand", &str))
        lx_b->alarmCommand = g_strdup(str);
    if (config_setting_lookup_string(settings, "BackgroundColor", &str))
        lx_b->backgroundColor = g_strdup(str);
    if (config_setting_lookup_string(settings, "ChargingColor1", &str))
        lx_b->chargingColor1 = g_strdup(str);
    if (config_setting_lookup_string(settings, "ChargingColor2", &str))
        lx_b->chargingColor2 = g_strdup(str);
    if (config_setting_lookup_string(settings, "DischargingColor1", &str))
        lx_b->dischargingColor1 = g_strdup(str);
    if (config_setting_lookup_string(settings, "DischargingColor2", &str))
        lx_b->dischargingColor2 = g_strdup(str);
    if (config_setting_lookup_int(settings, "AlarmTime", &tmp_int))
        lx_b->alarmTime = MAX(0, tmp_int);
    if (config_setting_lookup_int(settings, "BorderWidth", &tmp_int))
        lx_b->requestedBorder = CLAMP(tmp_int, 0, 6);
    if (config_setting_lookup_int(settings, "Size", &tmp_int))
        lx_b->thickness = MAX(1, tmp_int);
    if (config_setting_lookup_int(settings, "ShowExtendedInformation", &tmp_int))
        lx_b->show_extended_information = (tmp_int != 0);

    /* Make sure the border value is acceptable */
    updateSizes(lx_b);
    if (lx_b->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        lx_b->width = lx_b->thickness;
        gtk_widget_set_size_request(lx_b->drawingArea, lx_b->width, -1);
    }
    else
    {
        lx_b->height = lx_b->thickness;
        gtk_widget_set_size_request(lx_b->drawingArea, -1, lx_b->height);
    }

    g_signal_connect (G_OBJECT (lx_b->drawingArea),"configure-event",
          G_CALLBACK (configureEvent), (gpointer) lx_b);
#if GTK_CHECK_VERSION(3, 0, 0)
    g_signal_connect (G_OBJECT (lx_b->drawingArea), "draw",
          G_CALLBACK(draw), (gpointer) lx_b);
#else
    g_signal_connect (G_OBJECT (lx_b->drawingArea), "expose-event",
          G_CALLBACK (exposeEvent), (gpointer) lx_b);
#endif

    /* Apply more default options */
    if (! lx_b->alarmCommand)
        lx_b->alarmCommand = g_strconcat("notify-send \"", _("Battery low"), "\" --icon=battery-caution", NULL);
    if (! lx_b->backgroundColor)
        lx_b->backgroundColor = g_strdup("black");
    if (! lx_b->chargingColor1)
        lx_b->chargingColor1 = g_strdup("#28f200");
    if (! lx_b->chargingColor2)
        lx_b->chargingColor2 = g_strdup("#22cc00");
    if (! lx_b->dischargingColor1)
        lx_b->dischargingColor1 = g_strdup("#ffee00");
    if (! lx_b->dischargingColor2)
        lx_b->dischargingColor2 = g_strdup("#d9ca00");

    gdk_color_parse(lx_b->backgroundColor, &lx_b->background);
    gdk_color_parse(lx_b->chargingColor1, &lx_b->charging1);
    gdk_color_parse(lx_b->chargingColor2, &lx_b->charging2);
    gdk_color_parse(lx_b->dischargingColor1, &lx_b->discharging1);
    gdk_color_parse(lx_b->dischargingColor2, &lx_b->discharging2);

    /* Start the update loop */
    lx_b->timer = g_timeout_add_seconds( 9, (GSourceFunc) update_timout, (gpointer) lx_b);

    RET(p);
}


static void
destructor(gpointer data)
{
    ENTER;

    lx_battery *b = (lx_battery *)data;

    if (b->b != NULL)
        battery_free(b->b);

    if (b->pixmap)
        cairo_surface_destroy(b->pixmap);

    g_free(b->alarmCommand);
    g_free(b->backgroundColor);
    g_free(b->chargingColor1);
    g_free(b->chargingColor2);
    g_free(b->dischargingColor1);
    g_free(b->dischargingColor2);

    g_free(b->rateSamples);
    sem_destroy(&(b->alarmProcessLock));
    if (b->timer)
        g_source_remove(b->timer);
    g_free(b);

    RET();

}


static void orientation(LXPanel *panel, GtkWidget *p) {

    ENTER;

    lx_battery *b = lxpanel_plugin_get_data(p);

    if (b->orientation != panel_get_orientation(panel)) {
        b->orientation = panel_get_orientation(panel);
        updateSizes(b);
        if (b->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
            b->width = b->thickness;
            gtk_widget_set_size_request(b->drawingArea, b->width, -1);
        }
        else
        {
            b->height = b->thickness;
            gtk_widget_set_size_request(b->drawingArea, -1, b->height);
        }
    }

    RET();
}


static gboolean applyConfig(gpointer user_data)
{
    ENTER;

    lx_battery *b = lxpanel_plugin_get_data(user_data);

    /* Update the battery we monitor */
    battery_free(b->b);
    b->b = battery_get(b->battery_number);

    /* Update colors */
    if (b->backgroundColor &&
            gdk_color_parse(b->backgroundColor, &b->background))
        config_group_set_string(b->settings, "BackgroundColor", b->backgroundColor);
    if (b->chargingColor1 && gdk_color_parse(b->chargingColor1, &b->charging1))
        config_group_set_string(b->settings, "ChargingColor1", b->chargingColor1);
    if (b->chargingColor2 && gdk_color_parse(b->chargingColor2, &b->charging2))
        config_group_set_string(b->settings, "ChargingColor2", b->chargingColor2);
    if (b->dischargingColor1 &&
            gdk_color_parse(b->dischargingColor1, &b->discharging1))
        config_group_set_string(b->settings, "DischargingColor1", b->dischargingColor1);
    if (b->dischargingColor2 &&
            gdk_color_parse(b->dischargingColor2, &b->discharging2))
        config_group_set_string(b->settings, "DischargingColor2", b->dischargingColor2);

    /* Make sure it is at least 1 px */
    if (b->thickness < 1)
        b->thickness = 1;

    /* Make sure the border value is acceptable */
    b->requestedBorder = MIN(b->requestedBorder, 6);
    updateSizes(b);

    /* Resize the widget */
    gtk_container_set_border_width(b->box, b->border);
    if (b->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        b->width = b->thickness;
        gtk_widget_set_size_request(b->drawingArea, b->width, -1);
    }
    else
    {
        b->height = b->thickness;
        gtk_widget_set_size_request(b->drawingArea, -1, b->height);
    }
    /* ensure visibility if requested */
    if (!b->hide_if_no_battery)
        gtk_widget_show(user_data);
    else if (b->b == NULL)
        gtk_widget_hide(user_data);

    if (b->alarmCommand == NULL)
        b->alarmCommand = g_strconcat("xmessage ", _("Battery low"), NULL);

    /* update tooltip */
    set_tooltip_text(b);

    /* update settings */
    config_group_set_int(b->settings, "HideIfNoBattery", b->hide_if_no_battery);
    config_group_set_string(b->settings, "AlarmCommand", b->alarmCommand);
    config_group_set_int(b->settings, "AlarmTime", b->alarmTime);
    config_group_set_int(b->settings, "BorderWidth", b->requestedBorder);
    config_group_set_int(b->settings, "Size", b->thickness);
    config_group_set_int(b->settings, "ShowExtendedInformation",
                         b->show_extended_information);
    config_group_set_int(b->settings, "BatteryNumber", b->battery_number);

    update_display(b, TRUE);

    RET(FALSE);
}


static GtkWidget *config(LXPanel *panel, GtkWidget *p) {
    lx_battery *b = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Battery Monitor"),
            panel, applyConfig, p,
            _("Hide if there is no battery"), &b->hide_if_no_battery, CONF_TYPE_BOOL,
            _("Alarm command"), &b->alarmCommand, CONF_TYPE_STR,
            _("Alarm time (minutes left)"), &b->alarmTime, CONF_TYPE_INT,
            _("Background color"), &b->backgroundColor, CONF_TYPE_STR,
            _("Charging color 1"), &b->chargingColor1, CONF_TYPE_STR,
            _("Charging color 2"), &b->chargingColor2, CONF_TYPE_STR,
            _("Discharging color 1"), &b->dischargingColor1, CONF_TYPE_STR,
            _("Discharging color 2"), &b->dischargingColor2, CONF_TYPE_STR,
            "", panel_config_int_button_new(_("Border width"), (int *)&b->requestedBorder,
                                            0, 6), CONF_TYPE_EXTERNAL,
            "", panel_config_int_button_new(_("Size"), (int *)&b->thickness,
                                            1, 50), CONF_TYPE_EXTERNAL,
            _("Show Extended Information"), &b->show_extended_information, CONF_TYPE_BOOL,
            _("Number of battery to monitor"), &b->battery_number, CONF_TYPE_INT,
            NULL);
}


FM_DEFINE_MODULE(lxpanel_gtk, batt)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name        = N_("Battery Monitor"),
    .description = N_("Display battery status using ACPI"),

    .new_instance = constructor,
    .config      = config,
    .reconfigure = orientation,
    .button_press_event = buttonPressEvent
};


/* vim: set sw=4 sts=4 : */
