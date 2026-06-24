/*
 * Copyright (C) 2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "private.h"
#include "space.h"

gboolean _lxpanel_button_release(GtkWidget *widget, GdkEventButton *event)
{
    Panel *p = PLUGIN_PANEL(widget)->priv;

    if (event->device == p->move_device && event->button == 2 &&
        p->move_state != PANEL_MOVE_STOP)
    {
        if (p->move_state == PANEL_MOVE_MOVING)
        {
            /* ungrab device and return back previous cursor */
#if GTK_CHECK_VERSION(3, 0, 0)
            gdk_device_ungrab(event->device, event->time);
#else
            gdk_pointer_ungrab(event->time);
#endif
        }
        p->move_state = PANEL_MOVE_STOP;
        p->move_device = NULL;
        return TRUE;
    }
    return FALSE;
}

gboolean _lxpanel_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    Panel *p = PLUGIN_PANEL(widget)->priv;
    GList *plugins, *l;
    GtkAllocation alloc;
    GdkGrabStatus s;
    int x, y, old_pos, new_pos;
    gboolean expand, rtl;
    config_setting_t *cfg;
    PanelPluginMoveData *in, *out;

    if (event->device != p->move_device)
        return FALSE;

    if (p->move_state == PANEL_MOVE_DETECT)
    {
        gdk_window_get_origin(event->window, &x, &y);
        x += event->x - p->ax;
        y += event->y - p->ay;
        /* check threshold, start moving */
        if (gtk_drag_check_threshold(widget, p->move_x, p->move_y, x, y))
        {
            plugins = gtk_container_get_children(GTK_CONTAINER(p->box));
            for (l = plugins; l; l = l->next)
            {
                gtk_widget_get_allocation(l->data, &alloc);
                if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                {
                    if (p->move_x >= alloc.x && p->move_x < alloc.x + alloc.width)
                        break;
                }
                else
                {
                    if (p->move_y >= alloc.y && p->move_y < alloc.y + alloc.height)
                        break;
                }
            }
            if (l == NULL || PANEL_IS_SPACE(l->data))
            {
                p->move_state = PANEL_MOVE_STOP;
                p->move_device = NULL;
                g_list_free(plugins);
                return TRUE;
            }
            /* grab pointer, use cursor "move" */
#if GTK_CHECK_VERSION(3, 0, 0)
            s = gdk_device_grab(event->device, gtk_widget_get_window(widget),
                                GDK_OWNERSHIP_NONE, FALSE,
                                GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                gdk_cursor_new_from_name(p->display, "move"),
                                event->time);
#else
            s = gdk_pointer_grab(gtk_widget_get_window(widget), FALSE,
                                 GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                 NULL, gdk_cursor_new_from_name(p->display, "move"),
                                 event->time);
#endif
            if (s == GDK_GRAB_SUCCESS)
            {
                p->move_state = PANEL_MOVE_MOVING;
                /* load all drag data into panel data */
                p->move_plugin = l->data;
                p->move_before.space = NULL;
                p->move_before.plugin = NULL;
                p->move_after.space = NULL;
                p->move_after.plugin = NULL;
                if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                    p->move_diff = alloc.x + alloc.width / 2 - p->move_x;
                else
                    p->move_diff = alloc.y + alloc.height / 2 - p->move_y;
                /* g_debug("move_diff is %d",p->move_diff); */
                if (l->prev)
                {
                    GList *save = l;

                    l = l->prev;
                    if (PANEL_IS_SPACE(l->data))
                    {
                        p->move_before.space = l->data;
                        gtk_container_child_get(GTK_CONTAINER(p->box), l->data,
                                                "expand", &expand, NULL);
                        if (expand)
                        {
                            gtk_widget_get_allocation(l->data, &alloc);
                            if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                                p->move_before.space_size = alloc.width;
                            else
                                p->move_before.space_size = alloc.height;
                        }
                        else
                            p->move_before.space_size = -1;
                        l = l->prev;
                    }
                    if (l)
                    {
                        p->move_before.plugin = l->data;
                        gtk_widget_get_allocation(l->data, &alloc);
                        if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                            p->move_before.plugin_center = alloc.x + alloc.width / 2;
                        else
                            p->move_before.plugin_center = alloc.y + alloc.height / 2;
                    }
                    l = save;
                }
                if (l->next)
                {
                    l = l->next;
                    if (PANEL_IS_SPACE(l->data))
                    {
                        p->move_after.space = l->data;
                        gtk_container_child_get(GTK_CONTAINER(p->box), l->data,
                                                "expand", &expand, NULL);
                        if (expand)
                        {
                            gtk_widget_get_allocation(l->data, &alloc);
                            if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                                p->move_after.space_size = alloc.width;
                            else
                                p->move_after.space_size = alloc.height;
                        }
                        else
                            p->move_after.space_size = -1;
                        l = l->next;
                    }
                    if (l)
                    {
                        p->move_after.plugin = l->data;
                        gtk_widget_get_allocation(l->data, &alloc);
                        if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                            p->move_after.plugin_center = alloc.x + alloc.width / 2;
                        else
                            p->move_after.plugin_center = alloc.y + alloc.height / 2;
                    }
                }
                g_list_free(plugins);
                return TRUE;
            }
            g_list_free(plugins);
        }
    }
    else if (p->move_state == PANEL_MOVE_MOVING)
    {
        /* calculate current and new positions of moving widget center */
        gdk_window_get_origin(event->window, &x, &y);
        gtk_widget_get_allocation(p->move_plugin, &alloc);
        if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
            old_pos = alloc.x + alloc.width / 2;
            new_pos = x + event->x - p->ax + p->move_diff;
            rtl = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);
        }
        else
        {
            old_pos = alloc.y + alloc.height / 2;
            new_pos = y + event->y - p->ay + p->move_diff;
            rtl = FALSE;
        }
        /* actually move widget, changing spaces or swapping with others */
        if (new_pos > old_pos)
        {
            x = new_pos - old_pos;
            /* cursor is right/down */
            if (rtl)
                goto _split_or_swap_before;
            else
                goto _split_or_swap_after;
        }
        else if (new_pos < old_pos)
        {
            x = old_pos - new_pos;
            /* cursor is left/up */
            if (rtl)
            {
                rtl = FALSE; /* it now means before => after */
_split_or_swap_after:
                in = &p->move_before;
                out = &p->move_after;
            }
            else
            {
                rtl = TRUE; /* it now means after => before */
_split_or_swap_before:
                in = &p->move_after;
                out = &p->move_before;
            }
            /* g_debug("moving plugin by %d", new_pos - old_pos); */
            if (out->space)
            {
                /* split space and move plugin */
                if (in->space)
                {
                    /* between spaces */
                    if (out->space_size < 0)
                    {
                        /* fixed size, decrease out */
                        y = _panel_space_get_size(out->space) - x;
                        /* g_debug("next is fixed space of %d -> %d", x+y, y); */
                        if (y >= 2)
                        {
                            _panel_space_resize(out->space, y);
                            if (in->space_size > 0 &&
                                y > in->space_size - x)
                            {
                                /* fixed became bigger than expanded, let swap */
                                /* g_debug("swap 'expand' on spaces"); */
                                gtk_container_child_set(GTK_CONTAINER(p->box),
                                                        out->space,
                                                        "expand", TRUE, NULL);
                                cfg = g_object_get_qdata(G_OBJECT(out->space),
                                                         lxpanel_plugin_qconf);
                                config_group_set_int(cfg, "expand", 1);
                                out->space_size = y;
                                gtk_container_child_set(GTK_CONTAINER(p->box),
                                                        in->space,
                                                        "expand", FALSE, NULL);
                                cfg = g_object_get_qdata(G_OBJECT(in->space),
                                                         lxpanel_plugin_qconf);
                                config_group_set_int(cfg, "expand", 0);
                                _panel_space_resize(in->space, in->space_size + x);
                                in->space_size = -1;
                            }
                            /* in->space_size < 0 will be handled below */
                        }
                        else
                        {
                            /* remove empty space plugin */
                            /* g_debug("removing next space %p", out->space); */
                            lxpanel_remove_plugin(p->topgwin, out->space);
                            out->space = NULL;
                            y += x + gtk_box_get_spacing(GTK_BOX(p->box));
                            if (in->space_size >= 0)
                                /* correct size of expanded */
                                in->space_size += y;
                            else
                            {
                                /* both were fixed size - correct size of which left */
                                y += _panel_space_get_size(in->space);
                                _panel_space_resize(in->space, y);
                                /* g_debug("change prev size to %d", y); */
                            }
                            /* both spaces were handled so may return */
                            return TRUE;
                        }
                    }
                    else
                    {
                        if (out->space_size < x)
                            /* expandable size exhausted */
                            x = out->space_size;
                            //FIXME: if in->space_size >= 0 then remove out->space
                        out->space_size -= x;
                    }
                    if (in->space_size < 0)
                    {
                        /* fixed size, increase before */
                        y = _panel_space_get_size(in->space) + x;
                        _panel_space_resize(in->space, y);
                        /* g_debug("prev is fixed space of %d -> %d", y-x, y); */
                        if (out->space_size >= 0 &&
                            y > out->space_size)
                        {
                            /* g_debug("swap 'expand' on spaces"); */
                            /* fixed became bigger than expanded, let swap */
                            gtk_container_child_set(GTK_CONTAINER(p->box),
                                                    in->space,
                                                    "expand", TRUE, NULL);
                            cfg = g_object_get_qdata(G_OBJECT(in->space),
                                                     lxpanel_plugin_qconf);
                            config_group_set_int(cfg, "expand", 1);
                            in->space_size = y;
                            if (out->space_size >= 2)
                            {
                                gtk_container_child_set(GTK_CONTAINER(p->box),
                                                        out->space,
                                                        "expand", FALSE, NULL);
                                cfg = g_object_get_qdata(G_OBJECT(out->space),
                                                         lxpanel_plugin_qconf);
                                config_group_set_int(cfg, "expand", 0);
                                _panel_space_resize(out->space, out->space_size);
                                out->space_size = -1;
                            }
                            else
                            {
                                /* g_debug("removing next space"); */
                                /* remove empty space plugin */
                                lxpanel_remove_plugin(p->topgwin, out->space);
                                out->space = NULL;
                                in->space_size += out->space_size;
                                in->space_size += gtk_box_get_spacing(GTK_BOX(p->box));
                            }
                        }
                    }
                    else
                        in->space_size += x;
                    /* end of between spaces case */
                }
                else if (x >= 2)
                {
                    /* there is only space after */
                    /* ensure out->space size is at least x+2 */
                    if (out->space_size < 0)
                    {
                        y = _panel_space_get_size(out->space);
                        if (y - x < 2)
                            goto _swap_next_space;
                        /* decrement in->space size by x */
                        _panel_space_resize(out->space, y - x);
                        goto _add_prev_space;
                    }
                    else if (out->space_size - x < 2)
                    {
_swap_next_space:
                        /* too big change, just swap */
                        gtk_container_child_get(GTK_CONTAINER(p->box),
                                                p->move_plugin,
                                                "position", &y, NULL);
                        /* reposition space from next to prev */
                        in->space = out->space;
                        in->space_size = out->space_size;
                        out->space = NULL;
                        gtk_container_child_set(GTK_CONTAINER(p->box),
                                                in->space, "position",
                                                y, NULL);
                        cfg = g_object_get_qdata(G_OBJECT(in->space),
                                                 lxpanel_plugin_qconf);
                        config_setting_move_elem(cfg, config_setting_get_parent(cfg),
                                                 y + 1);
                        /* g_debug("swapped with next space %p", in->space); */
                    }
                    else
                    {
                        out->space_size -= x;
_add_prev_space:
                        /* create PanelSpace of size x before plugin on move */
                        gtk_container_child_get(GTK_CONTAINER(p->box),
                                                p->move_plugin,
                                                "position", &y, NULL);
                        cfg = config_group_add_subgroup(config_root_setting(p->config),
                                                        "Plugin");
                        config_group_set_string(cfg, "type", "space");
                        in->space_size = -1;
                        if (rtl)
                            /* create space after moving one */
                            y++;
                            /* else create space before moving one, i.e. in place */
                        in->space = lxpanel_add_plugin(p->topgwin, "space", cfg, y);
                        config_setting_move_elem(cfg, config_setting_get_parent(cfg),
                                                 y + 1);
                        if (in->space == NULL)
                            //FIXME: is it ever possible?
                            config_setting_destroy(cfg);
                        else
                            _panel_space_resize(in->space, x);
                        /* g_debug("added space %d before plugin", x); */
                    }
                }
            }
            else if (out->plugin)
            {
                /* no space after, check if need swap */
                if (new_pos < old_pos)
                {
                    /* going down */
                    if (new_pos < out->plugin_center)
                    {
                        /* need swap */
                        gtk_widget_get_allocation(p->move_plugin, &alloc);
                        if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                            x = alloc.width;
                        else
                            x = alloc.height;
                        x += gtk_box_get_spacing(GTK_BOX(p->box));
                        goto _swap_next;
                    }
                }
                else if (new_pos > out->plugin_center)
                {
                    /* going up, need swap */
                    gtk_widget_get_allocation(p->move_plugin, &alloc);
                    if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                        x = -alloc.width;
                    else
                        x = -alloc.height;
                    x -= gtk_box_get_spacing(GTK_BOX(p->box));
_swap_next:
                    /* swap with next plugin and reload all data */
                    plugins = gtk_container_get_children(GTK_CONTAINER(p->box));
                    l = g_list_find(plugins, p->move_plugin);
                    y = g_list_position(plugins, l);
                    if (rtl)
                    {
                        /* going down in list */
                        g_assert(l && l->prev);
                        l = l->prev->prev;
                    }
                    else
                    {
                        /* going up in list */
                        g_assert(l && l->next);
                        l = l->next->next;
                    }
                    /* g_debug("swapping with next plugin %p", out->plugin); */
                    in->space = NULL;
                    in->plugin = out->plugin;
                    in->plugin_center = out->plugin_center + x;
                    /* swap next plugin with one being moved */
                    gtk_container_child_set(GTK_CONTAINER(p->box),
                                            in->plugin, "position", y, NULL);
                    cfg = g_object_get_qdata(G_OBJECT(in->plugin),
                                             lxpanel_plugin_qconf);
                    config_setting_move_elem(cfg, config_setting_get_parent(cfg),
                                             y + 1);
                    /* check and set out->plugin and out->space */
                    if (l && PANEL_IS_SPACE(l->data))
                    {
                        out->space = l->data;
                        gtk_container_child_get(GTK_CONTAINER(p->box), l->data,
                                                "expand", &expand, NULL);
                        if (expand)
                        {
                            gtk_widget_get_allocation(l->data, &alloc);
                            if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                                out->space_size = alloc.width;
                            else
                                out->space_size = alloc.height;
                        }
                        else
                            out->space_size = -1;
                        if (rtl)
                            l = l->prev;
                        else
                            l = l->next;
                    }
                    else
                        out->space = NULL;
                    if (l)
                    {
                        out->plugin = l->data;
                        gtk_widget_get_allocation(l->data, &alloc);
                        if (p->orientation == GTK_ORIENTATION_HORIZONTAL)
                            out->plugin_center = alloc.x + alloc.width / 2;
                        else
                            out->plugin_center = alloc.y + alloc.height / 2;
                    }
                    else
                        out->plugin = NULL;
                    g_list_free(plugins);
                }
            }
        }
        return TRUE;
    }
    return FALSE;
}
