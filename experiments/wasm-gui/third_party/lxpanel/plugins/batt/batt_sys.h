/*
 *      batt_sys.h
 *
 *      Copyright 2009 Juergen Hötzel <juergen@archlinux.org>
 *                2015 Stanislav Kozina, Ersin <xersin@users.sf.net>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifndef BATT_SYS_H
#define BATT_SYS_H


#define BUF_SIZE 1024
#define ACPI_PATH_SYS_POWER_SUPPLY  "/sys/class/power_supply"
#define ACPI_BATTERY_DEVICE_NAME    "BAT"
#define MIN_CAPACITY	 0.01
#define MIN_PRESENT_RATE 0.01
#define BATTERY_DESC	"Battery"

#include <glib.h>

typedef struct battery {
    int battery_num;
    /* path to battery dir */
    gchar *path;
    /* sysfs file contents */
    int charge_now;
    int energy_now;
    int current_now;
    int power_now;
    int voltage_now;
    int charge_full_design;
    int energy_full_design;
    int charge_full;
    int energy_full;
    /* extra info */
    int seconds;
    int percentage;
    char *state;
    //const char *poststr;
    //const char *capacity_unit;
    int type_battery;
} battery;

battery *battery_get(int);
battery *battery_update( battery *b );
//void battery_print(battery *b, int show_capacity);
gboolean battery_is_charging( battery *b );
gint battery_get_remaining( battery *b );
void battery_free(battery* bat);

#endif
