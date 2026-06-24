/*
 * Copyright (C) 2008 Fred Chien <fred@lxde.org>
 *               2012 Michael Rawson <michaelrawson76@gmail.com>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <pthread.h>
#include <iwlib.h>
#include "nsconfig.h"
#include "netstat.h"
#include "lxnm_client.h"
#include "statusicon.h"
#include "passwd_gui.h"
#include "devproc.h"
#include "wireless.h"
#include "plugin.h"
#include "misc.h"
#include "dbg.h"

/* 1 second */
#define NETSTAT_IFACE_POLL_DELAY 3000

static void* actionProcess(void *arg)
{
    ENTER;
    RET(GINT_TO_POINTER(system((char *)arg)));
}

/* menu handlers */
static void ethernet_repair(GtkWidget *widget, netdev_info *ni)
{
    if (ni->ns->fixcmd) {
        pthread_t actionThread;
        pthread_attr_t attr;
        char *fixcmd;

        fixcmd = g_strdup_printf(ni->ns->fixcmd, ni->netdev_list->info.ifname);

        /* Note: Better would be to join the thread, and thus get a status
         * message. But detaching is currently simpler to implement. */
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&actionThread, &attr, actionProcess, fixcmd);
        pthread_attr_destroy(&attr);
    } else {
        lxnm_send_command(ni->ns->fnetd->lxnmchannel, LXNM_ETHERNET_REPAIR, ni->netdev_list->info.ifname);
    }
}

static void ethernet_down(GtkWidget *widget, netdev_info *ni)
{
    lxnm_send_command(ni->ns->fnetd->lxnmchannel, LXNM_ETHERNET_DOWN, ni->netdev_list->info.ifname);
}

static void wireless_connect(GtkWidget *widget, ap_setting *aps)
{
    char *cmdargs;

    /* without encryption */
    if (!aps->apinfo->haskey) {
        cmdargs = lxnm_wireless_command_make(aps->ifname, aps->apinfo->essid, aps->apinfo->apaddr, "",
                                   aps->apinfo->en_method, aps->apinfo->key_mgmt,
                                   aps->apinfo->group, aps->apinfo->pairwise);
        lxnm_send_command(aps->gio, LXNM_WIRELESS_CONNECT, cmdargs);
        g_free(cmdargs);
    } else {
        /* with encryption */
        if (aps->ni->netdev_list->info.pg!=NULL)
            passwd_gui_destroy(aps->ni->netdev_list->info.pg);

        /* record information for password dialog */
        ap_setting *aps_new;
        ap_info *apinfo;
	apinfo = malloc(sizeof(ap_info));
	if (aps->apinfo->essid==NULL)
		apinfo->essid = NULL;
	else
		apinfo->essid = g_strdup(aps->apinfo->essid);

	apinfo->apaddr = g_strdup(aps->apinfo->apaddr);
	apinfo->quality = aps->apinfo->quality;
	apinfo->en_method = aps->apinfo->en_method;
	apinfo->pairwise = aps->apinfo->pairwise;
	apinfo->group = aps->apinfo->group;
	apinfo->key_mgmt = aps->apinfo->key_mgmt;
	apinfo->haskey = aps->apinfo->haskey;

        aps_new = g_new0(ap_setting, 1);
        aps_new->ni = aps->ni;
        aps_new->gio = aps->gio;
        aps_new->ifname = g_strdup(aps->ifname);
        aps_new->apinfo = apinfo;
        /* create dialog window for typing password */
        aps->ni->netdev_list->info.pg = passwd_gui_new(aps_new);
        //passwd_gui_set_style(aps->ni->netdev_list->info.pg, gtk_style_new());
    }
}

static void
g_free_weaknotify(void *ptr, GObject *dummy)
{
    g_free(ptr);
}

static GtkWidget *
wireless_menu(netdev_info *ni)
{
    APLIST *aplist;
    APLIST *ptr;
    GtkWidget *menu;
    GtkWidget *menu_item;
    GtkWidget *wireless_label;

    /* AP status widget */
    GtkWidget *item_box;
    GtkWidget *essid_label;
    GtkWidget *lockicon;
    GtkWidget *signal_quality;
    gdouble quality_per;
    ap_setting *aps;

    /* create menu */
    menu = gtk_menu_new();
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

    /* Scanning AP */
    aplist = wireless_scanning(ni->ns->fnetd->iwsockfd, ni->netdev_list->info.ifname);
    if (aplist!=NULL) {
        /* release AP list after menu */
        g_object_weak_ref(G_OBJECT(menu), wireless_aplist_free, aplist);
        ptr = aplist;
        do {
            /* skip hidden AP with Encryption */
            if (ptr->info->haskey&&ptr->info->essid==NULL) {
                /* handle next AP */
                ptr = ptr->next;
				continue;
            }

            aps = g_new0(ap_setting, 1);
            aps->ni = ni;
            aps->gio = ni->ns->fnetd->lxnmchannel;
            aps->ifname = ni->netdev_list->info.ifname;
			aps->apinfo = ptr->info;

            /* create a new item */
            menu_item = gtk_menu_item_new();
            item_box = gtk_hbox_new(FALSE, 0);

            /* Encryption */
            if (aps->apinfo->haskey) {
                lockicon = lxpanel_image_new_for_icon(NULL, ICONS_WL_LOCK, 18, NULL);
                gtk_box_pack_start(GTK_BOX(item_box), lockicon, FALSE, FALSE, 0);
            }

            /* ESSID */
            if (aps->apinfo->essid==NULL)
                essid_label = gtk_label_new(_("<Hidden Access Point>"));
            else
                essid_label = gtk_label_new(aps->apinfo->essid);

            gtk_label_set_justify(GTK_LABEL(essid_label), GTK_JUSTIFY_LEFT);
            gtk_misc_set_padding(GTK_MISC(essid_label), 2, 0);
            gtk_box_pack_start(GTK_BOX(item_box), essid_label, TRUE, FALSE, 0);

            /* Quality */
            quality_per = (gdouble)((double)aps->apinfo->quality/100);
            if (quality_per>1)
                quality_per = 1;
            else if (quality_per<0)
                quality_per = 0;

            signal_quality = gtk_progress_bar_new();
            gtk_widget_set_size_request(signal_quality, 100, -1);
#if GTK_CHECK_VERSION(3, 0, 0)
            gtk_orientable_set_orientation(GTK_ORIENTABLE(signal_quality), GTK_ORIENTATION_HORIZONTAL);
#else
            gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(signal_quality), GTK_PROGRESS_LEFT_TO_RIGHT);
#endif
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(signal_quality), quality_per);
            gtk_box_pack_start(GTK_BOX(item_box), signal_quality, FALSE, FALSE, 0);

            /* add this item to menu */
            gtk_container_add(GTK_CONTAINER(menu_item), item_box);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
            g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(wireless_connect), aps);
            g_object_weak_ref(G_OBJECT(menu_item), g_free_weaknotify, aps);

            /* handle next AP */
            ptr = ptr->next;
        } while(ptr!=NULL);
    } else {
        /* we do not found any wireless networks */
        menu_item = gtk_menu_item_new();
        wireless_label = gtk_label_new(_("Wireless Networks not found in range"));
        gtk_label_set_justify(GTK_LABEL(wireless_label), GTK_JUSTIFY_LEFT);
        gtk_widget_set_sensitive(GTK_WIDGET(wireless_label), FALSE);
        gtk_container_add(GTK_CONTAINER(menu_item), wireless_label);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    }

    gtk_widget_show_all(menu);

    return menu;
}

static gint menupopup(GtkWidget *widget, GdkEvent *event, netdev_info *ni)
{
    GdkEventButton *event_button;

    g_return_val_if_fail (event != NULL, FALSE);

//    if (event->type == GDK_BUTTON_PRESS) {
        event_button = (GdkEventButton *) event;
    if (event->type == GDK_BUTTON_PRESS && event_button->button == 1) {
//        if (event_button->button == 1) {
            /* wireless device */
            if (ni->netdev_list->info.wireless) {
                gtk_menu_popup(GTK_MENU(wireless_menu(ni)), NULL, NULL, NULL, NULL, event_button->button, event_button->time);
//            }
            return TRUE;
        } else {
//        } else if (event_button->button == 3) {
            GtkWidget *menu;
            GtkWidget *menu_item;

            /* create menu */
            menu = gtk_menu_new();

            /* Repair */
            menu_item = gtk_menu_item_new_with_label(_("Repair"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
            g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(ethernet_repair), ni);

            /* interface down */
            menu_item = gtk_menu_item_new_with_label(_("Disable"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
            g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(ethernet_down), ni);

            gtk_widget_show_all(menu);

            gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event_button->button, event_button->time);
            return TRUE;
        }
    }

    return FALSE;
}

static char *select_icon(gboolean plug, gboolean connected, int stat)
{
    if (!plug)
        return ICONS_DISCONNECT;

    switch(stat) {
        case NETDEV_STAT_NORMAL:
            return ICONS_CONNECTED;
            break;
        case NETDEV_STAT_PROBLEM:
            return ICONS_PROBLEM;
            break;
        case NETDEV_STAT_RENEW:
            return ICONS_RENEW;
            break;
        case NETDEV_STAT_BOTHRS:
            return ICONS_BOTHRS;
            break;
        case NETDEV_STAT_SENDDATA:
            return ICONS_SENDDATA;
            break;
        case NETDEV_STAT_RECVDATA:
            return ICONS_RECVDATA;
            break;
        default:
            return NULL;
    }
}

static char *select_icon_theme(gboolean plug, gboolean connected, int stat)
{
    if (!plug)
        return ICONS_DISCONNECT_THEME;

    switch(stat) {
        case NETDEV_STAT_NORMAL:
            return ICONS_CONNECTED_THEME;
            break;
        case NETDEV_STAT_PROBLEM:
            return ICONS_PROBLEM_THEME;
            break;
        case NETDEV_STAT_RENEW:
            return ICONS_RENEW_THEME;
            break;
        case NETDEV_STAT_BOTHRS:
            return ICONS_BOTHRS_THEME;
            break;
        case NETDEV_STAT_SENDDATA:
            return ICONS_SENDDATA_THEME;
            break;
        case NETDEV_STAT_RECVDATA:
            return ICONS_RECVDATA_THEME;
            break;
        default:
            return NULL;
    }
}

static void refresh_systray(netstat *ns, NETDEVLIST_PTR netdev_list)
{
    NETDEVLIST_PTR ptr;
    char *tooltip;
    const char *icon, *theme_icon;

    if (netdev_list==NULL) {
        return;
    }

    ptr = netdev_list;
    do {
        if (!ptr->info.enable) {
            if (ptr->info.status_icon!=NULL) {
                set_statusicon_visible(ptr->info.status_icon, FALSE);
            }
        } else if (ptr->info.updated) {
            if (!ptr->info.plug) {
                if (ptr->info.wireless)
                    tooltip = g_strdup_printf("%s\n  %s", ptr->info.ifname, _("Wireless Connection has no connectivity"));
				else
                    tooltip = g_strdup_printf("%s\n  %s", ptr->info.ifname, _("Network cable is plugged out"));
            } else if (!ptr->info.connected)
                tooltip = g_strdup_printf("%s\n  %s", ptr->info.ifname, _("Connection has limited or no connectivity"));
            else if (ptr->info.flags & IFF_POINTOPOINT)
                tooltip = g_strdup_printf("%s\n  %s\t%s\n  %s\t%s\n  %s\t%s\n\n %s(%s/%s)\n   %lu/%lu %s\n   %lu/%lu %s",
                                                                 ptr->info.ifname,
                                                                _("IP Address:"), ptr->info.ipaddr,
                                                                _("Remote IP:"), ptr->info.dest,
                                                                _("Netmask:"), ptr->info.mask,
                                                                _("Activity"), _("Sent"),_("Received"),
                                                                ptr->info.trans_bytes, ptr->info.recv_bytes, _("bytes"),
                                                                ptr->info.trans_packets, ptr->info.recv_packets, _("packets"));
            else if (ptr->info.wireless)
                tooltip = g_strdup_printf("%s(%s) - %s(%d%%)\n  %s\t%s\n  %s\t%s\n  %s\t%s\n  %s\t%s\n  %s\t%s\n\n %s(%s/%s)\n   %lu/%lu %s\n   %lu/%lu %s",
                                                                ptr->info.ifname, _("Wireless"),
                                                                ptr->info.essid, ptr->info.quality,
                                                                _("Protocol:"), ptr->info.protocol,
                                                                _("IP Address:"), ptr->info.ipaddr,
                                                                _("Broadcast:"), ptr->info.bcast,
                                                                _("Netmask:"), ptr->info.mask,
                                                                _("HW Address:"), ptr->info.mac,
                                                                _("Activity"), _("Sent"), _("Received"),
                                                                ptr->info.trans_bytes, ptr->info.recv_bytes, _("bytes"),
                                                                ptr->info.trans_packets, ptr->info.recv_packets, _("packets"));

            else
                tooltip = g_strdup_printf("%s\n  %s\t%s\n  %s\t%s\n  %s\t%s\n  %s\t%s\n\n %s(%s/%s)\n   %lu/%lu %s\n   %lu/%lu %s",
                                                                ptr->info.ifname,
                                                                _("IP Address:"), ptr->info.ipaddr,
                                                                _("Broadcast:"), ptr->info.bcast,
                                                                _("Netmask:"), ptr->info.mask,
                                                                _("HW Address:"), ptr->info.mac,
                                                                _("Activity"), _("Sent"), _("Received"),
                                                                ptr->info.trans_bytes, ptr->info.recv_bytes, _("bytes"),
                                                                ptr->info.trans_packets, ptr->info.recv_packets, _("packets"));

            icon = select_icon(ptr->info.plug, ptr->info.connected, ptr->info.status);
            if (ns->use_theme)
                theme_icon = select_icon_theme(ptr->info.plug, ptr->info.connected, ptr->info.status);
            else
                theme_icon = icon;
            /* status icon doesn't exist  */
            if (ptr->info.status_icon==NULL) {
                netdev_info *ni;
                ni = malloc(sizeof(netdev_info));
                ni->ns = ns;
                ni->netdev_list = ptr;

                ptr->info.status_icon = create_statusicon(ns->panel, ns->mainw, icon, tooltip, theme_icon);
                g_signal_connect(ptr->info.status_icon->main, "button-press-event", G_CALLBACK(menupopup), ni);
                g_object_weak_ref(G_OBJECT(ptr->info.status_icon->main), g_free_weaknotify, ni);
            } else {
                set_statusicon_tooltips(ptr->info.status_icon, tooltip);
                update_statusicon(ptr->info.status_icon, icon, theme_icon);
                set_statusicon_visible(ptr->info.status_icon, TRUE);
            }
            g_free(tooltip);
        }
        ptr = ptr->next;
    } while(ptr!=NULL);
}

static gboolean refresh_devstat(netstat *ns)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    netproc_listener(ns->fnetd);
#ifdef DEBUG
    netproc_print(ns->fnetd->netdevlist);
#endif
    refresh_systray(ns, ns->fnetd->netdevlist);
    netproc_devicelist_clear(&ns->fnetd->netdevlist);
    return TRUE;
}

/* Plugin constructor */
static void netstat_destructor(gpointer user_data)
{
    netstat *ns = (netstat *) user_data;

    ENTER;
    g_source_remove(ns->ttag);
    netproc_netdevlist_clear(&ns->fnetd->netdevlist);
    /* The widget is destroyed in plugin_stop().
    gtk_widget_destroy(ns->mainw);
    */
    lxnm_close(ns->fnetd->lxnmchannel);
    close(ns->fnetd->sockfd);
    close(ns->fnetd->iwsockfd);
    g_free(ns->fnetd);
    g_free(ns->fixcmd);
    g_free(ns);
    RET();
}

static GtkWidget *netstat_constructor(LXPanel *panel, config_setting_t *settings)
{
    netstat *ns;
    const char *tmp;
    int tmp_int;
    GtkWidget *p;

    ENTER;
    ns = g_new0(netstat, 1);
    g_return_val_if_fail(ns != NULL, NULL);
    ns->panel = panel;
    /* apply config */
    if (config_setting_lookup_string(settings, "FixCommand", &tmp))
        ns->fixcmd = g_strdup(tmp);
    if (config_setting_lookup_int(settings, "UseTheme", &tmp_int))
        ns->use_theme = !!tmp_int;

    /* initializing */
    ns->fnetd = malloc(sizeof(FNETD));
    ns->fnetd->netdevlist = NULL;
    ns->fnetd->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    ns->fnetd->iwsockfd = iw_sockets_open();
    ns->fnetd->lxnmchannel = lxnm_socket();

    /* main */
    ns->mainw = panel_box_new(panel, FALSE, 1);
    gtk_widget_show_all(ns->mainw);

    /* Initializing network device list*/
    ns->fnetd->netdev_fp = netproc_open();
    ns->fnetd->dev_count = netproc_netdevlist_clear(&ns->fnetd->netdevlist);
    ns->fnetd->dev_count = netproc_scandevice(ns->fnetd->sockfd, ns->fnetd->iwsockfd, ns->fnetd->netdev_fp, &ns->fnetd->netdevlist);
    netproc_close(ns->fnetd->netdev_fp);
    refresh_systray(ns, ns->fnetd->netdevlist);

    ns->ttag = g_timeout_add(NETSTAT_IFACE_POLL_DELAY, (GSourceFunc)refresh_devstat, ns);

    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, ns, netstat_destructor);
    gtk_widget_set_has_window(p, FALSE);
    gtk_container_add((GtkContainer*)p, ns->mainw);

    RET(p);
}

static void orientation_changed(LXPanel *panel, GtkWidget *p)
{
    netstat *ns = lxpanel_plugin_get_data(p);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(ns->mainw),
                                   panel_get_orientation(panel));
}

FM_DEFINE_MODULE(lxpanel_gtk, netstat)

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Manage Networks"),
    .description = N_("Monitor and Manage networks"),

    .new_instance = netstat_constructor,
    .reconfigure = orientation_changed,
};

/* vim: set sw=4 sts=4 et : */
