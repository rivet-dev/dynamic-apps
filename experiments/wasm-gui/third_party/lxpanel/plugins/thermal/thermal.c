/*
 * Thermal plugin to lxpanel
 *
 * Copyright (C) 2007 by Daniel Kesler <kesler.daniel@gmail.com>
 *               2014-2015 by Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include <string.h>

#include "plugin.h"
#include "misc.h"

#include "dbg.h"

#define PROC_THERMAL_DIRECTORY "/proc/acpi/thermal_zone/" /* must be slash-terminated */
#define PROC_THERMAL_TEMPF  "temperature"
#define PROC_THERMAL_TRIP  "trip_points"
#define PROC_TRIP_CRITICAL "critical (S5):"

#define SYSFS_THERMAL_DIRECTORY "/sys/class/thermal/" /* must be slash-terminated */
#define SYSFS_THERMAL_SUBDIR_PREFIX "thermal_zone"
#define SYSFS_THERMAL_TEMPF  "temp"
#define SYSFS_THERMAL_TRIP  "trip_point_0_temp"

#define MAX_NUM_SENSORS 10
#define MAX_AUTOMATIC_CRITICAL_TEMP 150 /* in degrees Celsius */

#if !GLIB_CHECK_VERSION(2, 40, 0)
# define g_info(...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, __VA_ARGS__)
#endif

typedef gint (*GetTempFunc)(char const *);

typedef struct thermal {
    LXPanel *panel;
    config_setting_t *settings;
    GtkWidget *namew;
    GString *tip;
    int warning1;
    int warning2;
    int not_custom_levels, auto_sensor;
    char *sensor,
         *str_cl_normal,
         *str_cl_warning1,
         *str_cl_warning2;
    unsigned int timer;
    GdkColor cl_normal,
             cl_warning1,
             cl_warning2;
    int numsensors;
    char *sensor_array[MAX_NUM_SENSORS];
    char *sensor_name[MAX_NUM_SENSORS];
    GetTempFunc get_temperature[MAX_NUM_SENSORS];
    GetTempFunc get_critical[MAX_NUM_SENSORS];
    gint temperature[MAX_NUM_SENSORS];
    gint critical[MAX_NUM_SENSORS];
} thermal;


static gint
proc_get_critical(char const* sensor_path){
    FILE *state;
    char buf[ 256 ], sstmp [ 100 ];
    char* pstr;

    if(sensor_path == NULL) return -1;

    snprintf(sstmp,sizeof(sstmp),"%s%s",sensor_path,PROC_THERMAL_TRIP);

    if (!(state = fopen( sstmp, "r"))) {
        g_warning("thermal: cannot open %s", sstmp);
        return -1;
    }

    while( fgets(buf, 256, state) &&
            ! ( pstr = strstr(buf, PROC_TRIP_CRITICAL) ) );
    if( pstr )
    {
        pstr += strlen(PROC_TRIP_CRITICAL);
        while( *pstr && *pstr == ' ' )
            ++pstr;

        pstr[strlen(pstr)-3] = '\0';
        fclose(state);
        return atoi(pstr);
    }

    fclose(state);
    return -1;
}

static gint
proc_get_temperature(char const* sensor_path){
    FILE *state;
    char buf[ 256 ], sstmp [ 100 ];
    char* pstr;

    if(sensor_path == NULL) return -1;

    snprintf(sstmp,sizeof(sstmp),"%s%s",sensor_path,PROC_THERMAL_TEMPF);

    if (!(state = fopen( sstmp, "r"))) {
        g_warning("thermal: cannot open %s", sstmp);
        return -1;
    }

    while( fgets(buf, 256, state) &&
            ! ( pstr = strstr(buf, "temperature:") ) );
    if( pstr )
    {
        pstr += 12;
        while( *pstr && *pstr == ' ' )
            ++pstr;

        pstr[strlen(pstr)-3] = '\0';
        fclose(state);
        return atoi(pstr);
    }

    fclose(state);
    return -1;
}

static gint _get_reading(const char *path, gboolean quiet)
{
    FILE *state;
    char buf[256];
    char* pstr;

    if (!(state = fopen(path, "r"))) {
        if (!quiet)
            g_warning("thermal: cannot open %s", path);
        return -1;
    }

    while( fgets(buf, 256, state) &&
            ! ( pstr = buf ) );
    if( pstr )
    {
        fclose(state);
        return atoi(pstr)/1000;
    }

    fclose(state);
    return -1;
}

static gint
sysfs_get_critical(char const* sensor_path){
    char sstmp [ 100 ];

    if(sensor_path == NULL) return -1;

    snprintf(sstmp,sizeof(sstmp),"%s%s",sensor_path,SYSFS_THERMAL_TRIP);

    return _get_reading(sstmp, TRUE);
}

static gint
sysfs_get_temperature(char const* sensor_path){
    char sstmp [ 100 ];

    if(sensor_path == NULL) return -1;

    snprintf(sstmp,sizeof(sstmp),"%s%s",sensor_path,SYSFS_THERMAL_TEMPF);

    return _get_reading(sstmp, FALSE);
}

static gint
hwmon_get_critical(char const* sensor_path)
{
    char sstmp [ 100 ];
    int spl;

    if(sensor_path == NULL) return -1;

    spl = strlen(sensor_path) - 6;
    if (spl < 17 || spl > 94)
        return -1;

    snprintf(sstmp, sizeof(sstmp), "%.*s_crit", spl, sensor_path);

    return _get_reading(sstmp, TRUE);
}

static gint
hwmon_get_temperature(char const* sensor_path)
{
    if(sensor_path == NULL) return -1;

    return _get_reading(sensor_path, FALSE);
}

static gint get_temperature(thermal *th, gint *warn)
{
    gint max = -273;
    gint cur, i, w = 0;

    for(i = 0; i < th->numsensors; i++){
        cur = th->get_temperature[i](th->sensor_array[i]);
        if (w == 2) ; /* already warning2 */
        else if (th->not_custom_levels &&
                 th->critical[i] > 0 && cur >= th->critical[i] - 5)
            w = 2;
        else if ((!th->not_custom_levels || th->critical[i] < 0) &&
                 cur >= th->warning2)
            w = 2;
        else if (w == 1) ; /* already warning1 */
        else if (th->not_custom_levels &&
                 th->critical[i] > 0 && cur >= th->critical[i] - 10)
            w = 1;
        else if ((!th->not_custom_levels || th->critical[i] < 0) &&
                 cur >= th->warning1)
            w = 1;
        if (cur > max)
            max = cur;
        th->temperature[i] = cur;
    }
    *warn = w;

    return max;
}

static gint get_critical(thermal *th)
{
    gint min = MAX_AUTOMATIC_CRITICAL_TEMP;
    gint i;

    for(i = 0; i < th->numsensors; i++){
        th->critical[i] = th->get_critical[i](th->sensor_array[i]);
        if (th->critical[i] > 0 && th->critical[i] < min)
            min = th->critical[i];
    }

    return min;
}

static void
update_display(thermal *th)
{
    char buffer [60];
    int i;
    int temp;
    GdkColor color;
    gchar *separator;

    temp = get_temperature(th, &i);
    if (i >= 2)
        color = th->cl_warning2;
    else if (i >= 1)
        color = th->cl_warning1;
    else
        color = th->cl_normal;

    if(temp == -1)
        lxpanel_draw_label_text(th->panel, th->namew, "NA", TRUE, 1, TRUE);
    else
    {
        snprintf(buffer, sizeof(buffer), "%02d", temp);
        lxpanel_draw_label_text_with_color(th->panel, th->namew, buffer, TRUE, 1, &color);
    }

    g_string_truncate(th->tip, 0);
    separator = "";
    for (i = 0; i < th->numsensors; i++){
        g_string_append_printf(th->tip, "%s%s:\t%2d°C", separator, th->sensor_name[i], th->temperature[i]);
        separator = "\n";
    }
    gtk_widget_set_tooltip_text(th->namew, th->tip->str);
}

static gboolean update_display_timeout(gpointer user_data)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    update_display(user_data);
    return TRUE; /* repeat later */
}

static int
add_sensor(thermal* th, char const* sensor_path, const char *sensor_name,
           GetTempFunc get_temp, GetTempFunc get_crit)
{
    if (th->numsensors + 1 > MAX_NUM_SENSORS){
        g_warning("thermal: Too many sensors (max %d), ignoring '%s'",
                MAX_NUM_SENSORS, sensor_path);
        return -1;
    }

    th->sensor_array[th->numsensors] = g_strdup(sensor_path);
    th->sensor_name[th->numsensors] = g_strdup(sensor_name);
    th->get_critical[th->numsensors] = get_crit;
    th->get_temperature[th->numsensors] = get_temp;
    th->numsensors++;

    g_debug("thermal: Added sensor %s", sensor_path);

    return 0;
}

/* find_sensors():
 *      - Get the sensor directory, and store it in '*sensor'.
 *      - It is searched for in 'directory'.
 *      - Only the subdirectories starting with 'subdir_prefix' are accepted as sensors.
 *      - 'subdir_prefix' may be NULL, in which case any subdir is considered a sensor. */
static void
find_sensors(thermal* th, char const* directory, char const* subdir_prefix,
             GetTempFunc get_temp, GetTempFunc get_crit)
{
    GDir *sensorsDirectory;
    const char *sensor_name;
    char sensor_path[100];

    if (! (sensorsDirectory = g_dir_open(directory, 0, NULL)))
        return;

    /* Scan the thermal_zone directory for available sensors */
    while ((sensor_name = g_dir_read_name(sensorsDirectory))) {
        if (sensor_name[0] == '.')
            continue;
        if (subdir_prefix) {
            if (strncmp(sensor_name, subdir_prefix, strlen(subdir_prefix)) != 0)
                continue;
        }
        snprintf(sensor_path,sizeof(sensor_path),"%s%s/", directory, sensor_name);
        add_sensor(th, sensor_path, sensor_name, get_temp, get_crit);
    }
    g_dir_close(sensorsDirectory);
}

static gboolean try_hwmon_sensors(thermal* th, const char *path)
{
    GDir *sensorsDirectory;
    const char *sensor_name;
    char sensor_path[100], buf[256];
    FILE *fp;
    gboolean found = FALSE;

    if (!(sensorsDirectory = g_dir_open(path, 0, NULL)))
        return found;

    while ((sensor_name = g_dir_read_name(sensorsDirectory)))
    {
        if (strncmp(sensor_name, "temp", 4) == 0 &&
            strcmp(&sensor_name[5], "_input") == 0)
        {
            snprintf(sensor_path, sizeof(sensor_path), "%s/temp%c_label", path,
                     sensor_name[4]);
            fp = fopen(sensor_path, "r");
            buf[0] = '\0';
            if (fp)
            {
                if (fgets(buf, 256, fp))
                {
                    char *pp = strchr(buf, '\n');
                    if (pp)
                        *pp = '\0';
                }
                fclose(fp);
            }
            snprintf(sensor_path, sizeof(sensor_path), "%s/%s", path, sensor_name);
            add_sensor(th, sensor_path, buf[0] ? buf : sensor_name,
                       hwmon_get_temperature, hwmon_get_critical);
            found = TRUE;
        }
    }
    g_dir_close(sensorsDirectory);
    return found;
}

static void find_hwmon_sensors(thermal* th)
{
    char dir_path[100];
    char *c;
    int i; /* sensor type num, we'll try up to 4 */

    for (i = 0; i < 4; i++)
    {
        snprintf(dir_path, sizeof(dir_path), "/sys/class/hwmon/hwmon%d/device", i);
        if (try_hwmon_sensors(th, dir_path))
            continue;
        /* no sensors found under device/, try parent dir */
        c = strrchr(dir_path, '/');
        *c = '\0';
        try_hwmon_sensors(th, dir_path);
    }
}


static void
remove_all_sensors(thermal *th)
{
    int i;

    g_debug("thermal: Removing all sensors (%d)", th->numsensors);

    for (i = 0; i < th->numsensors; i++)
    {
        g_free(th->sensor_array[i]);
        g_free(th->sensor_name[i]);
    }

    th->numsensors = 0;
}

static void
check_sensors( thermal *th )
{
    // FIXME: scan in opposite order
    find_sensors(th, PROC_THERMAL_DIRECTORY, NULL, proc_get_temperature, proc_get_critical);
    find_sensors(th, SYSFS_THERMAL_DIRECTORY, SYSFS_THERMAL_SUBDIR_PREFIX, sysfs_get_temperature, sysfs_get_critical);
    if (th->numsensors == 0)
        find_hwmon_sensors(th);
    g_info("thermal: Found %d sensors", th->numsensors);
}


static gboolean applyConfig(gpointer p)
{
    thermal *th = lxpanel_plugin_get_data(p);
    int critical;
    ENTER;

    if (th->str_cl_normal) gdk_color_parse(th->str_cl_normal, &th->cl_normal);
    if (th->str_cl_warning1) gdk_color_parse(th->str_cl_warning1, &th->cl_warning1);
    if (th->str_cl_warning2) gdk_color_parse(th->str_cl_warning2, &th->cl_warning2);

    remove_all_sensors(th);
    /* FIXME: support wildcards in th->sensor */
    if(th->sensor == NULL) th->auto_sensor = TRUE;
    if(th->auto_sensor) check_sensors(th);
    else if (strncmp(th->sensor, "/sys/", 5) != 0)
        add_sensor(th, th->sensor, th->sensor, proc_get_temperature, proc_get_critical);
    else if (strncmp(th->sensor, "/sys/class/hwmon/", 17) != 0)
        add_sensor(th, th->sensor, th->sensor, sysfs_get_temperature, sysfs_get_critical);
    else
        add_sensor(th, th->sensor, th->sensor, hwmon_get_temperature, hwmon_get_critical);

    critical = get_critical(th);

    if(th->not_custom_levels){
        th->warning1 = critical - 10;
        th->warning2 = critical - 5;
    }

    config_group_set_string(th->settings, "NormalColor", th->str_cl_normal);
    config_group_set_string(th->settings, "Warning1Color", th->str_cl_warning1);
    config_group_set_string(th->settings, "Warning2Color", th->str_cl_warning2);
    config_group_set_int(th->settings, "AutomaticLevels", th->not_custom_levels);
    /* TODO: clean obsolete setting
    config_setting_remove(th->settings, "CustomLevels"); */
    config_group_set_int(th->settings, "Warning1Temp", th->warning1);
    config_group_set_int(th->settings, "Warning2Temp", th->warning2);
    config_group_set_int(th->settings, "AutomaticSensor", th->auto_sensor);
    config_group_set_string(th->settings, "Sensor", th->sensor);
    RET(FALSE);
}

static void
thermal_destructor(gpointer user_data)
{
  thermal *th = (thermal *)user_data;

  ENTER;
  remove_all_sensors(th);
  g_string_free(th->tip, TRUE);
  g_free(th->sensor);
  g_free(th->str_cl_normal);
  g_free(th->str_cl_warning1);
  g_free(th->str_cl_warning2);
  g_source_remove(th->timer);
  g_free(th);
  RET();
}

static GtkWidget *
thermal_constructor(LXPanel *panel, config_setting_t *settings)
{
    thermal *th;
    GtkWidget *p;
    const char *tmp;

    ENTER;
    th = g_new0(thermal, 1);
    th->panel = panel;
    th->settings = settings;

    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, th, thermal_destructor);
    gtk_widget_set_has_window(p, FALSE);

    th->namew = gtk_label_new("ww");
    gtk_container_add(GTK_CONTAINER(p), th->namew);

    th->tip = g_string_new(NULL);

    /* By default, use automatic, that is, "not custom" temperature levels. If
     * we were using custom levels, they would be 0°C at startup, so we would
     * display in warning colors by default. */
    th->not_custom_levels = TRUE;

    if (config_setting_lookup_string(settings, "NormalColor", &tmp))
        th->str_cl_normal = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "Warning1Color", &tmp))
        th->str_cl_warning1 = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "Warning2Color", &tmp))
        th->str_cl_warning2 = g_strdup(tmp);
    config_setting_lookup_int(settings, "AutomaticSensor", &th->auto_sensor);
    /* backward compatibility for wrong variable */
    config_setting_lookup_int(settings, "CustomLevels", &th->not_custom_levels);
    config_setting_lookup_int(settings, "AutomaticLevels", &th->not_custom_levels);
    if (config_setting_lookup_string(settings, "Sensor", &tmp))
        th->sensor = g_strdup(tmp);
    config_setting_lookup_int(settings, "Warning1Temp", &th->warning1);
    config_setting_lookup_int(settings, "Warning2Temp", &th->warning2);

    if(!th->str_cl_normal)
        th->str_cl_normal = g_strdup("#00ff00");
    if(!th->str_cl_warning1)
        th->str_cl_warning1 = g_strdup("#fff000");
    if(!th->str_cl_warning2)
        th->str_cl_warning2 = g_strdup("#ff0000");

    applyConfig(p);

    gtk_widget_show(th->namew);

    update_display(th);
    th->timer = g_timeout_add_seconds(3, (GSourceFunc) update_display_timeout, (gpointer)th);

    RET(p);
}

static GtkWidget *config(LXPanel *panel, GtkWidget *p)
{
    ENTER;

    GtkWidget *dialog;
    thermal *th = lxpanel_plugin_get_data(p);
    dialog = lxpanel_generic_config_dlg(_("Temperature Monitor"),
            panel, applyConfig, p,
            _("Normal color"), &th->str_cl_normal, CONF_TYPE_STR,
            _("Warning1 color"), &th->str_cl_warning1, CONF_TYPE_STR,
            _("Warning2 color"), &th->str_cl_warning2, CONF_TYPE_STR,
            _("Automatic sensor location"), &th->auto_sensor, CONF_TYPE_BOOL, // FIXME: if off, disable next one
            _("Sensor"), &th->sensor, CONF_TYPE_STR, // FIXME: create a list to select instead
            _("Automatic temperature levels"), &th->not_custom_levels, CONF_TYPE_BOOL, // FIXME: if off, disable two below
            _("Warning1 temperature"), &th->warning1, CONF_TYPE_INT,
            _("Warning2 temperature"), &th->warning2, CONF_TYPE_INT,
            NULL);

    RET(dialog);
}

FM_DEFINE_MODULE(lxpanel_gtk, thermal)

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Temperature Monitor"),
    .description = N_("Display system temperature"),

    .new_instance = thermal_constructor,
    .config = config,
};


/* vim: set sw=4 sts=4 et : */
