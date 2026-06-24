/**
 * CPUFreq plugin to lxpanel
 *
 * Copyright (C) 2009 by Daniel Kesler <kesler.daniel@gmail.com>
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
#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#include "plugin.h"
#include "misc.h"

#include "dbg.h"

#define PROC_ICON           "cpufreq-icon"
#define SYSFS_CPU_DIRECTORY "/sys/devices/system/cpu"
#define SCALING_GOV         "scaling_governor"
#define SCALING_AGOV        "scaling_available_governors"
#define SCALING_AFREQ       "scaling_available_frequencies"
#define SCALING_CUR_FREQ    "scaling_cur_freq"
#define SCALING_SETFREQ     "scaling_setspeed"
#define SCALING_MAX         "scaling_max_freq"
#define SCALING_MIN         "scaling_min_freq"


typedef struct {
    GtkWidget *main;
    config_setting_t *settings;
    GList *governors;
    GList *cpus;
    int has_cpufreq;
    char* cur_governor;
    int   cur_freq;
    unsigned int timer;
    //gboolean remember;
} cpufreq;

typedef struct {
    char *data;
    cpufreq *cf;
} Param;

static void cpufreq_destructor(gpointer user_data);

static void
get_cur_governor(cpufreq *cf){
    FILE *fp;
    char buf[ 100 ], sstmp [ 256 ];

    snprintf(sstmp, sizeof(sstmp), "%s/%s", (char*)cf->cpus->data, SCALING_GOV);
    if ((fp = fopen( sstmp, "r")) != NULL) {
        if(cf->cur_governor)
        {
          g_free(cf->cur_governor);
          cf->cur_governor = NULL;
        }
        if (fgets(buf, 100, fp))
        {
            buf[strlen(buf)-1] = '\0';
            cf->cur_governor = strdup(buf);
        }
        fclose(fp);
    }
}

static void
get_cur_freq(cpufreq *cf){
    FILE *fp;
    char buf[ 100 ], sstmp [ 256 ];

    snprintf(sstmp, sizeof(sstmp), "%s/%s", (char*)cf->cpus->data, SCALING_CUR_FREQ);
    if ((fp = fopen( sstmp, "r")) != NULL) {
        if (fgets(buf, 100, fp))
        {
            buf[strlen(buf)-1] = '\0';
            cf->cur_freq = atoi(buf);
        }
        fclose(fp);
    }
}

/*static void
get_governors(cpufreq *cf){
    FILE *fp;
    GList *l;
    char buf[ 100 ], sstmp [ 256 ], c, bufl = 0;

    g_list_free(cf->governors);
    cf->governors = NULL;

    get_cur_governor(cf);

    if(cf->cpus == NULL){
        cf->governors = NULL;
        return;
    }
    sprintf(sstmp,"%s/%s",cf->cpus->data, SCALING_AGOV);

    if (!(fp = fopen( sstmp, "r"))) {
        printf("cpufreq: cannot open %s\n",sstmp);
        return;
    }

    while((c = fgetc(fp)) != EOF){
        if(c == ' '){
            if(bufl > 1){
                buf[bufl] = '\0';
                cf->governors = g_list_append(cf->governors, strdup(buf));
            }
            bufl = 0;
            buf[0] = '\0';
        }else{
            buf[bufl++] = c;
        }
    }

    fclose(fp);
}

static void
cpufreq_set_freq(GtkWidget *widget, Param* p){
    FILE *fp;
    char buf[ 100 ], sstmp [ 256 ];

    if(strcmp(p->cf->cur_governor, "userspace")) return;

    sprintf(sstmp,"%s/%s",p->cf->cpus->data, SCALING_SETFREQ);
    if ((fp = fopen( sstmp, "w")) != NULL) {
        fprintf(fp,"%s",p->data);
        fclose(fp);
    }
}

static GtkWidget *
frequency_menu(cpufreq *cf){
    FILE *fp;
    Param* param;
    char buf[ 100 ], sstmp [ 256 ], c, bufl = 0;

    sprintf(sstmp,"%s/%s",cf->cpus->data, SCALING_AFREQ);

    if (!(fp = fopen( sstmp, "r"))) {
        printf("cpufreq: cannot open %s\n",sstmp);
        return 0;
    }

    GtkMenu* menu = GTK_MENU(gtk_menu_new());
    GtkWidget* menuitem;

    while((c = fgetc(fp)) != EOF){
        if(c == ' '){
            if(bufl > 1){
                buf[bufl] = '\0';
                menuitem = gtk_menu_item_new_with_label(strdup(buf));
                gtk_menu_append (GTK_MENU_SHELL (menu), menuitem);
                gtk_widget_show (menuitem);
                param = g_new0(Param, 1);
                param->data = strdup(buf);
                param->cf = cf;
                g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(cpufreq_set_freq), param);
                g_object_weak_ref(G_OBJECT(menuitem), (GWeakNotify)g_free, param);
            }
            bufl = 0;
            buf[0] = '\0';
        }else{
            buf[bufl++] = c;
        }
    }

    fclose(fp);
    return GTK_WIDGET(menu);
}*/

static void
get_cpus(cpufreq *cf)
{

    const char *cpu;
    char cpu_path[100];

    GDir * cpuDirectory = g_dir_open(SYSFS_CPU_DIRECTORY, 0, NULL);
    if (cpuDirectory == NULL)
    {
        cf->cpus = NULL;
        printf("cpufreq: no cpu found\n");
        return;
    }

    while ((cpu = g_dir_read_name(cpuDirectory)))
    {
        /* Look for directories of the form "cpu<n>", where "<n>" is a decimal integer. */
        if ((strncmp(cpu, "cpu", 3) == 0) && (cpu[3] >= '0') && (cpu[3] <= '9'))
        {
            snprintf(cpu_path, sizeof(cpu_path), "%s/%s/cpufreq", SYSFS_CPU_DIRECTORY, cpu);

            GDir * cpufreqDir = g_dir_open(SYSFS_CPU_DIRECTORY, 0, NULL);
            if (cpufreqDir == NULL)
            {
                cf->cpus = NULL;
                cf->has_cpufreq = 0;
                break;
            }

            cf->has_cpufreq = 1;
            cf->cpus = g_list_append(cf->cpus, strdup(cpu_path));
        }
    }
    g_dir_close(cpuDirectory);
}

/*static void
cpufreq_set_governor(GtkWidget *widget, Param* p){
    FILE *fp;
    char buf[ 100 ], sstmp [ 256 ];

    sprintf(sstmp, "%s/%s", p->cf->cpus->data, SCALING_GOV);
    if ((fp = fopen( sstmp, "w")) != NULL) {
        fprintf(fp,"%s",p->data);
        fclose(fp);
    }
}

static GtkWidget *
cpufreq_menu(cpufreq *cf){
    GList *l;
    GSList *group;
    char buff[100];
    GtkMenuItem* menuitem;
    Param* param;

    GtkMenu* menu = GTK_MENU(gtk_menu_new());
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

    get_governors(cf);
    group = NULL;

    if((cf->governors == NULL) || (!cf->has_cpufreq) || (cf->cur_governor == NULL)){
        menuitem = GTK_MENU_ITEM(gtk_menu_item_new_with_label("CPUFreq not supported"));
        gtk_menu_append (GTK_MENU_SHELL (menu), GTK_WIDGET (menuitem));
        gtk_widget_show (GTK_WIDGET (menuitem));
        return GTK_WIDGET(menu);
    }

    if(strcmp(cf->cur_governor, "userspace") == 0){
        menuitem = GTK_MENU_ITEM(gtk_menu_item_new_with_label("  Frequency"));
        gtk_menu_append (GTK_MENU_SHELL (menu), GTK_WIDGET (menuitem));
        gtk_widget_show (GTK_WIDGET (menuitem));
        gtk_menu_item_set_submenu(menuitem, frequency_menu(cf));
        menuitem = GTK_MENU_ITEM(gtk_separator_menu_item_new());
        gtk_menu_append (GTK_MENU_SHELL (menu), GTK_WIDGET (menuitem));
        gtk_widget_show (GTK_WIDGET(menuitem));
    }

    for( l = cf->governors; l; l = l->next )
    {
      if(strcmp((char*)l->data, cf->cur_governor) == 0){
        sprintf(buff,"> %s", l->data);
        menuitem = GTK_MENU_ITEM(gtk_menu_item_new_with_label(strdup(buff)));
      }else{
        sprintf(buff,"   %s", l->data);
        menuitem = GTK_MENU_ITEM(gtk_menu_item_new_with_label(strdup(buff)));
      }

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (menuitem));
      gtk_widget_show (GTK_WIDGET (menuitem));
      param = g_new0(Param, 1);
      param->data = l->data;
      param->cf = cf;
      g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(cpufreq_set_governor), param);
      g_object_weak_ref(G_OBJECT(menuitem), (GWeakNotify) g_free, param);
    }

    return GTK_WIDGET (menu);
}*/



static  gboolean
clicked(GtkWidget *widget, GdkEventButton *evt, LXPanel *panel)
{
    ENTER;

    /* Standard right-click handling. */
    if( evt->button == 1 )
    {
// Setting governor can't work without root privilege
//      gtk_menu_popup( cpufreq_menu((cpufreq*)plugin->priv), NULL, NULL, NULL, NULL,
//                      evt->button, evt->time );
      return TRUE;
    }

    RET(FALSE);
}

static gboolean
_update_tooltip(cpufreq *cf)
{
    char *tooltip;

    get_cur_freq(cf);
    get_cur_governor(cf);

    ENTER;

    tooltip = g_strdup_printf(_("Frequency: %d MHz\nGovernor: %s"),
                              cf->cur_freq / 1000, cf->cur_governor);
    gtk_widget_set_tooltip_text(cf->main, tooltip);
    g_free(tooltip);
    RET(TRUE);
}

static gboolean update_tooltip(gpointer user_data)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    return _update_tooltip(user_data);
}

static GtkWidget *cpufreq_constructor(LXPanel *panel, config_setting_t *settings)
{
    cpufreq *cf;
    //GtkWidget *button;

    ENTER;
    cf = g_new0(cpufreq, 1);
    g_return_val_if_fail(cf != NULL, NULL);
    cf->governors = NULL;
    cf->cpus = NULL;
    cf->settings = settings;

    cf->main = lxpanel_button_new_for_icon(panel, PROC_ICON, NULL, NULL);
    lxpanel_plugin_set_data(cf->main, cf, cpufreq_destructor);

    cf->has_cpufreq = 0;

    get_cpus(cf);

    //if (config_setting_lookup_int(settings, "Remember", &tmp_int)) cf->remember = tmp_int != 0;
    //if (config_setting_lookup_int(settings, "Governor", &tmp_str)) cf->cur_governor = g_strdup(tmp_str);
    //config_setting_lookup_int(settings, "Frequency", &cf->cur_freq);

    _update_tooltip(cf);
    cf->timer = g_timeout_add_seconds(2, update_tooltip, (gpointer)cf);

    RET(cf->main);
}

/*
static gboolean applyConfig(gpointer user_data)
{
    cpufreq *cf = lxpanel_plugin_get_data(user_data);

    config_group_set_int(cf->settings, "Remember", cf->remember);
    return FALSE;
}

static GtkWidget *config(LXPanel *panel, GtkWidget *p, GtkWindow *parent)
{
    cpufreq *cf = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("CPUFreq frontend"), panel, applyConfig, p,
            _("Remember governor and frequency"), &cf->remember, CONF_TYPE_BOOL,
            NULL);
}
*/

static void
cpufreq_destructor(gpointer user_data)
{
    cpufreq *cf = (cpufreq *)user_data;
    g_list_free ( cf->cpus );
    g_list_free ( cf->governors );
    g_source_remove(cf->timer);
    g_free(cf);
}


FM_DEFINE_MODULE(lxpanel_gtk, cpufreq)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("CPUFreq frontend"),
    .description = N_("Display CPU frequency and allow one to change governors and frequency"),

    .new_instance = cpufreq_constructor,
    //.config      = config,
    .button_press_event = clicked
};
