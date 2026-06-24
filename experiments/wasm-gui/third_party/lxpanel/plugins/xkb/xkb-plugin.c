/*
 * Copyright (C) 2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2012-2013 Giuseppe Penone <giuspen@gmail.com>
 *               2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

/* Originally derived from xfce4-xkb-plugin, Copyright 2004 Alexander Iliev,
 * which credits Michael Glickman. */

/* Modified by Giuseppe Penone <giuspen@gmail.com> starting from 2012-07 and lxpanel 0.5.10 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xkb.h"
#include "gtk-compat.h"

enum
{
    COLUMN_ICON,
    COLUMN_LAYOUT,
    COLUMN_VARIANT,
    NUM_COLUMNS
};

enum
{
    COLUMN_ADD_ICON,
    COLUMN_ADD_LAYOUT,
    COLUMN_ADD_DESC,
    NUM_ADD_COLUMNS
};

enum
{
    COLUMN_MODEL_ID,
    COLUMN_MODEL_DESC,
    NUM_MODEL_COLUMNS
};

enum
{
    COLUMN_CHANGE_ID,
    COLUMN_CHANGE_DESC,
    COLUMN_CHANGE_INCL,
    COLUMN_CHANGE_WEIGHT,
    NUM_CHANGE_COLUMNS
};

static void  xkb_destructor(gpointer user_data);
static void  xkb_settings_fill_layout_tree_model_with_config(XkbPlugin *p_xkb);
static void  xkb_update_layouts_n_variants(XkbPlugin *p_xkb);
static void  xkb_add_layout(XkbPlugin *p_xkb, gchar *layout, gchar*variant);
static int   xkb_get_flag_size(XkbPlugin *p_xkb);

static void      on_xkb_fbev_active_window_event(FbEv *ev, gpointer p_data);
static gboolean  on_xkb_button_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer p_data);
static void      on_radiobutton_disp_type_image_toggled(GtkToggleButton *p_radiobutton, gpointer p_data);
static void      on_radiobutton_disp_type_image_cust_toggled(GtkToggleButton *p_radiobutton, gpointer p_data);
static void      on_radiobutton_disp_type_text_toggled(GtkToggleButton *p_radiobutton, gpointer p_data);
static void      on_xkb_checkbutton_per_app_toggled(GtkToggleButton *tb, gpointer p_data);
static void      on_xkb_checkbutton_no_reset_opt_toggled(GtkToggleButton *tb, gpointer p_data);
static void      on_xkb_checkbutton_keep_system_layouts_toggled(GtkToggleButton *tb, gpointer p_data);
static void      on_dialog_config_response(GtkDialog *p_dialog, gint response, gpointer p_data);
static void      on_xkb_entry_advanced_opt_icon_press(GtkEntry             *p_entry,
                                                      GtkEntryIconPosition  icon_pos,
                                                      GdkEvent             *p_event,
                                                      gpointer              p_data);

static unsigned char  user_active = FALSE;
static const gchar    flag_filepath_generator[] = "%s/%s.png";

static int xkb_get_flag_size(XkbPlugin *p_xkb)
{
    int  size = panel_get_icon_size(p_xkb->panel);
    switch(p_xkb->flag_size)
    {
        case 1: return size*0.5;
        case 2: return size*0.6;
        case 3: return size*0.7;
        case 4: return size*0.8;
        case 5: return size*0.9;
        default: break;
    }
    return size;
}

/* Redraw the graphics. */
void xkb_redraw(XkbPlugin *p_xkb)
{
    /* Set the image. */
    gboolean valid_image = FALSE;
    int  size = xkb_get_flag_size(p_xkb);
    if( (p_xkb->display_type == DISP_TYPE_IMAGE) || (p_xkb->display_type == DISP_TYPE_IMAGE_CUST) )
    {
        char * group_name = (char *)xkb_get_current_symbol_name_lowercase(p_xkb);
        if(group_name != NULL)
        {
            gchar *flag_filepath = NULL;
            gchar *flags_dir = (p_xkb->cust_dir_exists && (p_xkb->display_type == DISP_TYPE_IMAGE_CUST)) ? g_strdup(FLAGSCUSTDIR):g_strdup(FLAGSDIR);
            if(strchr(group_name, '/') != NULL)
            {
                gchar *layout_mod = g_strdup(group_name);
                layout_mod = g_strdelimit(layout_mod, "/", '-');
                flag_filepath = g_strdup_printf(flag_filepath_generator, flags_dir, layout_mod);
                g_free(layout_mod);
            }
            else
            {
                flag_filepath = g_strdup_printf(flag_filepath_generator, flags_dir, group_name);
            }
            GdkPixbuf * unscaled_pixbuf = gdk_pixbuf_new_from_file(flag_filepath, NULL);
            g_free(flag_filepath);
            g_free(flags_dir);
            g_free(group_name);

            if(unscaled_pixbuf != NULL)
            {
                /* Loaded successfully. */
                int width = gdk_pixbuf_get_width(unscaled_pixbuf);
                int height = gdk_pixbuf_get_height(unscaled_pixbuf);
                GdkPixbuf * pixbuf = gdk_pixbuf_scale_simple(unscaled_pixbuf, size * width / height, size, GDK_INTERP_BILINEAR);
                if(pixbuf != NULL)
                {
                    gtk_image_set_from_pixbuf(GTK_IMAGE(p_xkb->p_image), pixbuf);
                    g_object_unref(G_OBJECT(pixbuf));
                    gtk_widget_hide(p_xkb->p_label);
                    gtk_widget_show(p_xkb->p_image);
                    gtk_widget_set_tooltip_text(p_xkb->p_plugin, xkb_get_current_group_name(p_xkb));
                    valid_image = TRUE;
                }
                g_object_unref(unscaled_pixbuf);
            }
        }
    }

    /* Set the label. */
    if( (p_xkb->display_type == DISP_TYPE_TEXT) || ( ! valid_image) )
    {
        char *group_name = (char *)xkb_get_current_symbol_name(p_xkb);
        if (group_name != NULL)
        {
            lxpanel_draw_label_text(p_xkb->panel, p_xkb->p_label, group_name,
                    TRUE, size*4/(5*10.0), TRUE);
            gtk_widget_hide(p_xkb->p_image);
            gtk_widget_show(p_xkb->p_label);
            gtk_widget_set_tooltip_text(p_xkb->p_plugin, xkb_get_current_group_name(p_xkb));
        }
    }
}

/* Handler for "active_window" event on root window listener. */
static void on_xkb_fbev_active_window_event(FbEv * ev, gpointer p_data)
{
    XkbPlugin * xkb = (XkbPlugin *)p_data;
    if (xkb->enable_perwin)
    {
        Window * win = fb_ev_active_window(ev);
        if (*win != None)
        {
            xkb_active_window_changed(xkb, *win);
            xkb_redraw(xkb);
        }
    }
}

/* Handler for "scroll-event" on drawing area. */
static gboolean on_xkb_button_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer p_data)
{
    XkbPlugin * xkb = (XkbPlugin *)p_data;

    /* Change to next or previous group. */
    xkb_change_group(xkb,
        (((event->direction == GDK_SCROLL_UP) || (event->direction == GDK_SCROLL_RIGHT)) ? 1 : -1));
    return TRUE;
}

/* Handler for button-press-event on top level widget. */
static gboolean on_xkb_button_press_event(GtkWidget * widget,  GdkEventButton * event, LXPanel * panel)
{
    /* Change to next group. */
    if (event->button != 1)
        return FALSE;
    xkb_change_group(lxpanel_plugin_get_data(widget), 1);
    return TRUE;
}

static void on_xkb_entry_advanced_opt_icon_press(GtkEntry             *p_entry,
                                                 GtkEntryIconPosition  icon_pos,
                                                 GdkEvent             *p_event,
                                                 gpointer              p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    g_free(p_xkb->kbd_advanced_options);
    p_xkb->kbd_advanced_options = g_strdup(gtk_entry_get_text(p_entry));
    config_group_set_string(p_xkb->settings, "AdvancedOpt", p_xkb->kbd_advanced_options);
    xkb_setxkbmap(p_xkb);
}

/* Plugin constructor. */
static GtkWidget *xkb_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    XkbPlugin * p_xkb = g_new0(XkbPlugin, 1);
    GtkWidget * p;
    const char *tmp;
    int tmp_int;

    p_xkb->panel = panel;
    p_xkb->settings = settings;

    /* Initialize to defaults. */
    p_xkb->display_type = DISP_TYPE_IMAGE;
    //p_xkb->enable_perwin = FALSE;
    //p_xkb->do_not_reset_opt = FALSE;
    p_xkb->keep_system_layouts = TRUE;
    //p_xkb->kbd_model = NULL;
    //p_xkb->kbd_layouts = NULL;
    //p_xkb->kbd_variants = NULL;
    //p_xkb->kbd_change_option = NULL;
    //p_xkb->kbd_advanced_options = NULL;
    p_xkb->flag_size = 3;
    p_xkb->cust_dir_exists = g_file_test(FLAGSCUSTDIR,  G_FILE_TEST_IS_DIR);

    /* Load parameters from the configuration file. */
    config_setting_lookup_int(settings, "DisplayType", &p_xkb->display_type);
    if (config_setting_lookup_int(settings, "PerWinLayout", &tmp_int))
        p_xkb->enable_perwin = tmp_int != 0;
    if (config_setting_lookup_int(settings, "NoResetOpt", &tmp_int))
        p_xkb->do_not_reset_opt = tmp_int != 0;
    if (config_setting_lookup_int(settings, "KeepSysLayouts", &tmp_int))
        p_xkb->keep_system_layouts = tmp_int != 0;
    if (config_setting_lookup_string(settings, "Model", &tmp))
        p_xkb->kbd_model = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "LayoutsList", &tmp))
        p_xkb->kbd_layouts = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "VariantsList", &tmp))
        p_xkb->kbd_variants = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "ToggleOpt", &tmp))
        p_xkb->kbd_change_option = g_strdup(tmp);
    if (config_setting_lookup_string(settings, "AdvancedOpt", &tmp))
        p_xkb->kbd_advanced_options = g_strdup(tmp);
    config_setting_lookup_int(settings, "FlagSize", &p_xkb->flag_size);

    /* Allocate top level widget and set into Plugin widget pointer. */
    p_xkb->p_plugin = p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, p_xkb, xkb_destructor);

    /* Create a horizontal box as the child of the button. */
    GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 3);
    gtk_container_add(GTK_CONTAINER(p), hbox);
    gtk_widget_show(hbox);

    /* Create a label and an image as children of the horizontal box.
     * Only one of these is visible at a time, controlled by user preference
     * and the successful loading of the image. */
    p_xkb->p_label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(hbox), p_xkb->p_label);
    p_xkb->p_image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(hbox), p_xkb->p_image);

    /* Check for first run */
    if( (p_xkb->kbd_model == NULL) || (p_xkb->kbd_layouts == NULL) ||
        (p_xkb->kbd_variants == NULL) || (p_xkb->kbd_change_option == NULL) )
    {
        /* This is a first run, read the current layout */
        xkb_mechanism_constructor(p_xkb);

        if(p_xkb->kbd_model != NULL) g_free(p_xkb->kbd_model);
        if(p_xkb->kbd_layouts != NULL) g_free(p_xkb->kbd_layouts);
        if(p_xkb->kbd_variants != NULL) g_free(p_xkb->kbd_variants);
        if(p_xkb->kbd_change_option != NULL) g_free(p_xkb->kbd_change_option);

        p_xkb->kbd_model = g_strdup("pc105");
        gchar *symbol_name_lowercase = (char *)xkb_get_current_symbol_name_lowercase(p_xkb);
        p_xkb->kbd_layouts = g_strdup(symbol_name_lowercase);
        g_free(symbol_name_lowercase);
        p_xkb->kbd_variants = g_strdup(",");
        p_xkb->kbd_change_option = g_strdup("grp:shift_caps_toggle");
        config_group_set_string(p_xkb->settings, "Model", p_xkb->kbd_model);
        config_group_set_string(p_xkb->settings, "LayoutsList", p_xkb->kbd_layouts);
        config_group_set_string(p_xkb->settings, "VariantsList", p_xkb->kbd_variants);
        config_group_set_string(p_xkb->settings, "ToggleOpt", p_xkb->kbd_change_option);

        xkb_mechanism_destructor(p_xkb);
    }

    /* Initialize the XKB interface. */
    xkb_setxkbmap(p_xkb);
    xkb_mechanism_constructor(p_xkb);

    /* Connect signals. */
    g_signal_connect(p, "scroll-event", G_CALLBACK(on_xkb_button_scroll_event), p_xkb);
    g_signal_connect(G_OBJECT(fbev), "active-window", G_CALLBACK(on_xkb_fbev_active_window_event), p_xkb);

    /* Show the widget and return. */
    xkb_redraw(p_xkb);
    return p;
}

/* Plugin destructor. */
static void xkb_destructor(gpointer user_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)user_data;

    /* Disconnect root window event handler. */
    g_signal_handlers_disconnect_by_func(G_OBJECT(fbev), on_xkb_fbev_active_window_event, p_xkb);

    /* Disconnect from the XKB mechanism. */
    xkb_mechanism_destructor(p_xkb);

    /* Deallocate all memory. */
    g_free(p_xkb->kbd_model);
    g_free(p_xkb->kbd_layouts);
    g_free(p_xkb->kbd_variants);
    g_free(p_xkb->kbd_change_option);
    g_free(p_xkb->kbd_advanced_options);
    g_free(p_xkb);
}

/* Handler for "toggled" event on per-application check box of configuration dialog. */
static void on_xkb_checkbutton_per_app_toggled(GtkToggleButton *tb, gpointer p_data)
{
    if(user_active == TRUE)
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * xkb = (XkbPlugin *)p_data;
        xkb->enable_perwin = gtk_toggle_button_get_active(tb);
        if(!xkb->enable_perwin)
        {
            /* at deactivation clear the hash table */
            if(xkb->p_hash_table_group != NULL)
                g_hash_table_destroy(xkb->p_hash_table_group);
            xkb->p_hash_table_group = g_hash_table_new(g_direct_hash, NULL);
        }
        config_group_set_int(xkb->settings, "PerWinLayout", xkb->enable_perwin);
        xkb_redraw(xkb);
    }
}

/* Handler for "toggled" event on no reset options check box of configuration dialog. */
static void on_xkb_checkbutton_no_reset_opt_toggled(GtkToggleButton *tb, gpointer p_data)
{
    if(user_active == TRUE)
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * xkb = (XkbPlugin *)p_data;
        xkb->do_not_reset_opt = gtk_toggle_button_get_active(tb);
        config_group_set_int(xkb->settings, "NoResetOpt", xkb->do_not_reset_opt);
        xkb_redraw(xkb);
    }
}

/* Handler for "toggled" event on keep system layouts check box of configuration dialog. */
static void on_xkb_checkbutton_keep_system_layouts_toggled(GtkToggleButton *tb, gpointer p_data)
{
    if(user_active == TRUE)
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->keep_system_layouts = gtk_toggle_button_get_active(tb);
        config_group_set_int(p_xkb->settings, "KeepSysLayouts", p_xkb->keep_system_layouts);
        xkb_redraw(p_xkb);

        gtk_widget_set_sensitive(p_xkb->p_frame_kbd_model, !p_xkb->keep_system_layouts);
        gtk_widget_set_sensitive(p_xkb->p_frame_kbd_layouts, !p_xkb->keep_system_layouts);
        gtk_widget_set_sensitive(p_xkb->p_frame_change_layout, !p_xkb->keep_system_layouts);
        gtk_widget_set_sensitive(p_xkb->p_entry_advanced_opt, !p_xkb->keep_system_layouts);
        gtk_widget_set_sensitive(p_xkb->p_checkbutton_no_reset_opt, !p_xkb->keep_system_layouts);

        if(!p_xkb->keep_system_layouts)
        {
            gtk_entry_set_icon_from_stock(GTK_ENTRY(p_xkb->p_entry_advanced_opt), GTK_ENTRY_ICON_SECONDARY, "gtk-save");
            xkb_update_layouts_n_variants(p_xkb);
        }
        else
        {
            GtkWidget *dialog;
            dialog = gtk_message_dialog_new(p_xkb->p_dialog_config,
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_WARNING,
                                            GTK_BUTTONS_OK,
                                            _("New session is required for this option to take effect"));
            gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
    }
}

static void on_radiobutton_flag_size_1_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->flag_size = 1;
        config_group_set_int(p_xkb->settings, "FlagSize", 1);
        xkb_redraw(p_xkb);
    }
}

static void on_radiobutton_flag_size_2_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->flag_size = 2;
        config_group_set_int(p_xkb->settings, "FlagSize", 2);
        xkb_redraw(p_xkb);
    }
}

static void on_radiobutton_flag_size_3_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->flag_size = 3;
        config_group_set_int(p_xkb->settings, "FlagSize", 3);
        xkb_redraw(p_xkb);
    }
}

static void on_radiobutton_flag_size_4_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->flag_size = 4;
        config_group_set_int(p_xkb->settings, "FlagSize", 4);
        xkb_redraw(p_xkb);
    }
}

static void on_radiobutton_flag_size_5_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->flag_size = 5;
        config_group_set_int(p_xkb->settings, "FlagSize", 5);
        xkb_redraw(p_xkb);
    }
}

static void on_radiobutton_flag_size_6_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->flag_size = 6;
        config_group_set_int(p_xkb->settings, "FlagSize", 6);
        xkb_redraw(p_xkb);
    }
}

/* Handler for "toggled" event on display type image radiobutton of configuration dialog. */
static void on_radiobutton_disp_type_image_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->display_type =  DISP_TYPE_IMAGE;
        config_group_set_int(p_xkb->settings, "DisplayType", p_xkb->display_type);
        xkb_redraw(p_xkb);
    }
}

/* Handler for "toggled" event on display type image radiobutton of configuration dialog. */
static void on_radiobutton_disp_type_image_cust_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->display_type =  DISP_TYPE_IMAGE_CUST;
        config_group_set_int(p_xkb->settings, "DisplayType", p_xkb->display_type);
        xkb_redraw(p_xkb);
    }
}

/* Handler for "toggled" event on display type image radiobutton of configuration dialog. */
static void on_radiobutton_disp_type_text_toggled(GtkToggleButton *p_radiobutton, gpointer p_data)
{
    if( (user_active == TRUE) && gtk_toggle_button_get_active(p_radiobutton) )
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->display_type =  DISP_TYPE_TEXT;
        config_group_set_int(p_xkb->settings, "DisplayType", p_xkb->display_type);
        xkb_redraw(p_xkb);
    }
}

/* Handler for "response" event on configuration dialog. */
static void on_dialog_config_response(GtkDialog *p_dialog, gint response, gpointer data)
{
    XkbPlugin * p_xkb = (XkbPlugin *)data;

    /* Reset pointer because dialog will be destroyed */
    p_xkb->p_dialog_config = NULL;
}

static void  on_cell_renderer_layout_change_incl_toggled(GtkCellRendererToggle *cell,
                                                        gchar                 *path_str,
                                                        gpointer               data)
{
    GtkTreeModel *p_model = (GtkTreeModel *)data;
    GtkTreeIter  tree_iter;
    GtkTreePath *p_tree_path = gtk_tree_path_new_from_string(path_str);
    gboolean     included;

    /* get toggled iter */
    gtk_tree_model_get_iter(p_model, &tree_iter, p_tree_path);
    gtk_tree_model_get(p_model, &tree_iter, COLUMN_CHANGE_INCL, &included, -1);

    /* do something with the value */
    included = !included;

    /* set new value */
    gtk_list_store_set(GTK_LIST_STORE(p_model), &tree_iter, COLUMN_CHANGE_INCL, included, -1);
    gtk_list_store_set(GTK_LIST_STORE(p_model), &tree_iter, COLUMN_CHANGE_WEIGHT, included ? PANGO_WEIGHT_ULTRAHEAVY : PANGO_WEIGHT_NORMAL, -1);

    /* clean up */
    gtk_tree_path_free(p_tree_path);
}

static gboolean  on_treeviews_lists_button_press_event(GtkWidget *p_widget,
                                                       GdkEventButton *p_event,
                                                       gpointer p_data)
{
    if(p_event->button == 1)
    {
        if(p_event->type == GDK_2BUTTON_PRESS)
        {
            GtkButton *p_button_ok = (GtkButton *)p_data;
            gtk_button_clicked(p_button_ok);
        }
    }
    else if(p_event->button == 2)
    {
        GtkTreePath  *p_tree_path;
        if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(p_widget),
                                         p_event->x, p_event->y,
                                         &p_tree_path,
                                         NULL, NULL, NULL))
        {
            if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(p_widget), p_tree_path))
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(p_widget), p_tree_path);
            else
                gtk_tree_view_expand_row(GTK_TREE_VIEW(p_widget), p_tree_path, FALSE);
            gtk_tree_path_free(p_tree_path);
        }
    }
    return FALSE;
}

static void on_button_kbd_model_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    // dialog
    GtkWidget *p_dialog = gtk_dialog_new_with_buttons(_("Select Keyboard Model"),
                            p_xkb->p_dialog_config,
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    // scrolledwindow
    GtkWidget * p_scrolledwindow_kbd_model = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_kbd_model),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(p_dialog))),
                       p_scrolledwindow_kbd_model, TRUE, TRUE, 2);

    // liststore
    GtkListStore *p_liststore_kbd_model = gtk_list_store_new(NUM_MODEL_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *p_treeview_kbd_model = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_liststore_kbd_model));
    g_object_unref(G_OBJECT(p_liststore_kbd_model));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(p_treeview_kbd_model), TRUE);
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_kbd_model), p_treeview_kbd_model);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // model desc
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Description"), p_renderer, "text", COLUMN_MODEL_DESC, NULL);
    gtk_tree_view_column_set_sort_column_id(p_column, COLUMN_MODEL_DESC);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_model), p_column);
    // model id
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Id"), p_renderer, "text", COLUMN_MODEL_ID, NULL);
    gtk_tree_view_column_set_sort_column_id(p_column, COLUMN_MODEL_ID);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_model), p_column);


    // populate model
    GKeyFile *p_keyfile = g_key_file_new();
    gchar *xkbcfg_filepath = g_strdup_printf("%s/models.cfg", XKBCONFDIR);
    if(g_key_file_load_from_file(p_keyfile, xkbcfg_filepath, 0, NULL))
    {
        gchar **keys_models = g_key_file_get_keys(p_keyfile, "MODELS", NULL, NULL);
        guint   model_idx = 0;
        GtkTreeIter  tree_iter;
        gchar *p_model_desc;
        while(keys_models[model_idx] != NULL)
        {
            p_model_desc = g_key_file_get_string(p_keyfile, "MODELS", keys_models[model_idx], NULL);
            gtk_list_store_append(p_liststore_kbd_model, &tree_iter);
            gtk_list_store_set(p_liststore_kbd_model, &tree_iter,
                                COLUMN_MODEL_ID, keys_models[model_idx],
                                COLUMN_MODEL_DESC, g_dgettext("xkeyboard-config", p_model_desc),
                                -1);
            g_free(p_model_desc);
            model_idx++;
        }
        g_strfreev(keys_models);
        g_key_file_free(p_keyfile);
    }
    g_free(xkbcfg_filepath);

    // callback for double click
    g_signal_connect(p_treeview_kbd_model, "button-press-event",
                     G_CALLBACK(on_treeviews_lists_button_press_event),
                     gtk_dialog_get_widget_for_response(GTK_DIALOG(p_dialog), GTK_RESPONSE_OK));
    // sort for description
    gtk_tree_view_column_clicked(p_column);

    gtk_widget_set_size_request(p_dialog, 700, 500);
    gtk_widget_show_all(GTK_WIDGET(p_scrolledwindow_kbd_model));
    gint  response = gtk_dialog_run(GTK_DIALOG(p_dialog));
    if(response == GTK_RESPONSE_OK)
    {
        GtkTreeIter  tree_iter_sel;
        GtkTreeSelection *p_treeselection_kbd_model = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_treeview_kbd_model));
        if(gtk_tree_selection_get_selected(p_treeselection_kbd_model,
                                           (GtkTreeModel **)(&p_liststore_kbd_model),
                                           &tree_iter_sel))
        {
            gchar *kbd_model_new;
            gtk_tree_model_get(GTK_TREE_MODEL(p_liststore_kbd_model),
                               &tree_iter_sel, COLUMN_MODEL_ID, &kbd_model_new, -1);
            g_free(p_xkb->kbd_model);
            p_xkb->kbd_model = g_strdup(kbd_model_new);
            config_group_set_string(p_xkb->settings, "Model", kbd_model_new);
            gtk_button_set_label(GTK_BUTTON(p_xkb->p_button_kbd_model), p_xkb->kbd_model);
            g_free(kbd_model_new);
            xkb_setxkbmap(p_xkb);
            xkb_redraw(p_xkb);
        }
    }
    gtk_widget_destroy(p_dialog);
}

static gboolean  change_opt_tree_model_foreach(GtkTreeModel *p_model,
                                               GtkTreePath *p_path,
                                               GtkTreeIter *p_iter,
                                               gpointer p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    gboolean  included;

    gtk_tree_model_get(p_model, p_iter, COLUMN_CHANGE_INCL, &included,  -1);
    if(included)
    {
        gchar  *change_opt_id;
        gtk_tree_model_get(p_model, p_iter, COLUMN_CHANGE_ID, &change_opt_id,  -1);
        if(strlen(p_xkb->p_gstring_change_opt_partial->str))
        {
            g_string_append_c(p_xkb->p_gstring_change_opt_partial, ',');
        }
        g_string_append(p_xkb->p_gstring_change_opt_partial, change_opt_id);

        //g_printf("\npartial change opt = '%s'\n", p_xkb->p_gstring_change_opt_partial->str);

        g_free(change_opt_id);
    }
    return FALSE;
}

static void on_button_kbd_change_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    // dialog
    GtkWidget *p_dialog = gtk_dialog_new_with_buttons(_("Select Layout Change Type"),
                            p_xkb->p_dialog_config,
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    // scrolledwindow
    GtkWidget * p_scrolledwindow_kbd_change = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_kbd_change),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(p_dialog))),
                       p_scrolledwindow_kbd_change, TRUE, TRUE, 2);

    // liststore
    GtkListStore *p_liststore_kbd_change = gtk_list_store_new(NUM_CHANGE_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT);
    GtkWidget *p_treeview_kbd_change = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_liststore_kbd_change));
    g_object_unref(G_OBJECT(p_liststore_kbd_change));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(p_treeview_kbd_change), TRUE);
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_kbd_change), p_treeview_kbd_change);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // change included
    p_renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(p_renderer, "toggled", G_CALLBACK(on_cell_renderer_layout_change_incl_toggled), p_liststore_kbd_change);
    p_column = gtk_tree_view_column_new_with_attributes("", p_renderer, "active", COLUMN_CHANGE_INCL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_change), p_column);
    // change desc
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Description"), p_renderer, "text", COLUMN_CHANGE_DESC,
                                                                                      "weight", COLUMN_CHANGE_WEIGHT,
                                                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(p_column, COLUMN_CHANGE_DESC);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_change), p_column);
    // change id
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Id"), p_renderer, "text", COLUMN_CHANGE_ID,
                                                                             "weight", COLUMN_CHANGE_WEIGHT,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(p_column, COLUMN_CHANGE_ID);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_change), p_column);

    // populate model
    GKeyFile *p_keyfile = g_key_file_new();
    gchar *xkbcfg_filepath = g_strdup_printf("%s/toggle.cfg", XKBCONFDIR);
    if(g_key_file_load_from_file(p_keyfile, xkbcfg_filepath, 0, NULL))
    {
        char **change_opts = g_strsplit_set(p_xkb->kbd_change_option, ",", 0);
        int    num_change_opts;
        gchar **keys_changes = g_key_file_get_keys(p_keyfile, "TOGGLE", NULL, NULL);
        guint   change_idx = 0;
        GtkTreeIter  tree_iter;
        gchar *p_change_desc;
        while(keys_changes[change_idx] != NULL)
        {
            p_change_desc = g_key_file_get_string(p_keyfile, "TOGGLE", keys_changes[change_idx], NULL);
            gtk_list_store_append(p_liststore_kbd_change, &tree_iter);
            gboolean  included = FALSE;
            num_change_opts = 0;
            while(change_opts[num_change_opts] != NULL)
            {
                if(strcmp(change_opts[num_change_opts], keys_changes[change_idx]) == 0)
                {
                    included = TRUE;
                    break;
                }
                num_change_opts++;
            }
            gtk_list_store_set(p_liststore_kbd_change, &tree_iter,
                                COLUMN_CHANGE_ID, keys_changes[change_idx],
                                COLUMN_CHANGE_DESC, g_dgettext("xkeyboard-config", p_change_desc),
                                COLUMN_CHANGE_INCL, included,
                                COLUMN_CHANGE_WEIGHT, included ? PANGO_WEIGHT_ULTRAHEAVY : PANGO_WEIGHT_NORMAL,
                                -1);
            g_free(p_change_desc);
            change_idx++;
        }
        g_strfreev(keys_changes);
        g_key_file_free(p_keyfile);
        g_strfreev(change_opts);
    }
    g_free(xkbcfg_filepath);

    // callback for double click
    //g_signal_connect(p_treeview_kbd_change, "button-press-event",
                     //G_CALLBACK(on_treeviews_lists_button_press_event),
                     //gtk_dialog_get_widget_for_response(GTK_DIALOG(p_dialog), GTK_RESPONSE_OK));
    gtk_widget_set_size_request(p_dialog, 700, 500);
    gtk_widget_show_all(GTK_WIDGET(p_scrolledwindow_kbd_change));
    gint  response = gtk_dialog_run(GTK_DIALOG(p_dialog));
    if(response == GTK_RESPONSE_OK)
    {
        p_xkb->p_gstring_change_opt_partial = g_string_new("");
        gtk_tree_model_foreach(GTK_TREE_MODEL(p_liststore_kbd_change),
                               change_opt_tree_model_foreach,
                               p_xkb);
        if(!strlen(p_xkb->p_gstring_change_opt_partial->str))
        {
            g_string_append_c(p_xkb->p_gstring_change_opt_partial, ',');
        }
        g_free(p_xkb->kbd_change_option);
        p_xkb->kbd_change_option = g_strdup(p_xkb->p_gstring_change_opt_partial->str);
        config_group_set_string(p_xkb->settings, "ToggleOpt", p_xkb->kbd_change_option);
        g_string_free(p_xkb->p_gstring_change_opt_partial, TRUE/*free also gstring->str*/);

        gtk_button_set_label(GTK_BUTTON(p_xkb->p_button_change_layout), p_xkb->kbd_change_option);
        xkb_setxkbmap(p_xkb);
        xkb_redraw(p_xkb);
    }
    gtk_widget_destroy(p_dialog);
}

static void on_button_up_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    GtkTreeIter  tree_iter_prev;
    if(gtk_tree_selection_get_selected(p_xkb->p_treeselection_layout,
                                       (GtkTreeModel **)(&p_xkb->p_liststore_layout),
                                       &tree_iter_sel))
    {
        GtkTreePath *p_tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                                                           &tree_iter_sel);
        if( (gtk_tree_path_prev(p_tree_path)) &&
            (gtk_tree_model_get_iter(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                                     &tree_iter_prev,
                                     p_tree_path)))
        {
            gtk_list_store_swap(p_xkb->p_liststore_layout, &tree_iter_sel, &tree_iter_prev);
            xkb_update_layouts_n_variants(p_xkb);
        }
        gtk_tree_path_free(p_tree_path);
    }
}

static void on_button_down_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    GtkTreeIter  tree_iter_next;
    if(gtk_tree_selection_get_selected(p_xkb->p_treeselection_layout,
                                       (GtkTreeModel **)(&p_xkb->p_liststore_layout),
                                       &tree_iter_sel))
    {
        tree_iter_next = tree_iter_sel;
        if(gtk_tree_model_iter_next(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                                    &tree_iter_next))
        {
            gtk_list_store_swap(p_xkb->p_liststore_layout, &tree_iter_sel, &tree_iter_next);
            xkb_update_layouts_n_variants(p_xkb);
        }
    }
}

static void on_button_rm_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    if(gtk_tree_selection_get_selected(p_xkb->p_treeselection_layout,
                                       (GtkTreeModel **)(&p_xkb->p_liststore_layout),
                                       &tree_iter_sel))
    {
        gtk_list_store_remove(p_xkb->p_liststore_layout, &tree_iter_sel);
        xkb_update_layouts_n_variants(p_xkb);
        gtk_widget_set_sensitive(p_xkb->p_button_rm_layout, p_xkb->num_layouts > 1);
    }
}

static void on_button_add_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    // dialog
    GtkWidget *p_dialog = gtk_dialog_new_with_buttons(_("Add Keyboard Layout"),
                            p_xkb->p_dialog_config,
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    // scrolledwindow
    GtkWidget * p_scrolledwindow_add_layout = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_add_layout),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(p_dialog))),
                       p_scrolledwindow_add_layout, TRUE, TRUE, 2);

    // treestore
    GtkTreeStore *p_treestore_add_layout = gtk_tree_store_new(NUM_ADD_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *p_treeview_add_layout = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_treestore_add_layout));
    g_object_unref(G_OBJECT(p_treestore_add_layout));
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(p_treeview_add_layout), TRUE);
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_add_layout), p_treeview_add_layout);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column, *p_column_desc;
    // icon
    p_renderer = gtk_cell_renderer_pixbuf_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Flag"), p_renderer, "pixbuf", COLUMN_ADD_ICON, NULL);
    gtk_tree_view_column_set_sort_column_id(p_column, COLUMN_ADD_DESC);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_add_layout), p_column);
    // layout
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Layout"), p_renderer, "text", COLUMN_ADD_LAYOUT, NULL);
    gtk_tree_view_column_set_sort_column_id(p_column, COLUMN_ADD_LAYOUT);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_add_layout), p_column);
    // desc
    p_renderer = gtk_cell_renderer_text_new();
    p_column_desc = gtk_tree_view_column_new_with_attributes(_("Description"), p_renderer, "text", COLUMN_ADD_DESC, NULL);
    gtk_tree_view_column_set_sort_column_id(p_column_desc, COLUMN_ADD_DESC);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_add_layout), p_column_desc);
    // search column
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(p_treeview_add_layout), COLUMN_ADD_DESC);

    // populate model
    GKeyFile *p_keyfile = g_key_file_new();
    gchar *xkbcfg_filepath = g_strdup_printf("%s/layouts.cfg", XKBCONFDIR);
    if(g_key_file_load_from_file(p_keyfile, xkbcfg_filepath, 0, NULL))
    {
        gchar **keys_layouts = g_key_file_get_keys(p_keyfile, "LAYOUTS", NULL, NULL);
        guint   layout_idx = 0;
        GtkTreeIter  tree_top, tree_child;
        gchar *p_layout_desc;
        while(keys_layouts[layout_idx] != NULL)
        {
            p_layout_desc = g_key_file_get_string(p_keyfile, "LAYOUTS", keys_layouts[layout_idx], NULL);
            if(strchr(keys_layouts[layout_idx], '(') == NULL)
            {
                gchar *flag_filepath = NULL;
                gchar *flags_dir = (p_xkb->cust_dir_exists && (p_xkb->display_type == DISP_TYPE_IMAGE_CUST)) ? g_strdup(FLAGSCUSTDIR):g_strdup(FLAGSDIR);
                if(strchr(keys_layouts[layout_idx], '/') != NULL)
                {
                    gchar *layout_mod = g_strdup(keys_layouts[layout_idx]);
                    layout_mod = g_strdelimit(layout_mod, "/", '-');
                    flag_filepath = g_strdup_printf(flag_filepath_generator, flags_dir, layout_mod);
                    g_free(layout_mod);
                }
                else
                {
                    flag_filepath = g_strdup_printf(flag_filepath_generator, flags_dir, keys_layouts[layout_idx]);
                }
                GdkPixbuf *p_pixbuf = gdk_pixbuf_new_from_file_at_size(flag_filepath, -1, 16, NULL);
                gtk_tree_store_append(p_treestore_add_layout, &tree_top, NULL);
                if(p_pixbuf != NULL)
                {
                    gtk_tree_store_set(p_treestore_add_layout, &tree_top,
                                        COLUMN_ADD_ICON, p_pixbuf,
                                        COLUMN_ADD_LAYOUT, keys_layouts[layout_idx],
                                        COLUMN_ADD_DESC, g_dgettext("xkeyboard-config", p_layout_desc),
                                        -1);
                    g_object_unref(G_OBJECT(p_pixbuf));
                }
                else
                {
                    gtk_tree_store_set(p_treestore_add_layout, &tree_top,
                                        COLUMN_ADD_LAYOUT, keys_layouts[layout_idx],
                                        COLUMN_ADD_DESC, p_layout_desc,
                                        -1);
                }
                g_free(flag_filepath);
                g_free(flags_dir);
            }
            else
            {

                gtk_tree_store_append(p_treestore_add_layout, &tree_child, &tree_top);
                gtk_tree_store_set(p_treestore_add_layout, &tree_child,
                                    COLUMN_ADD_LAYOUT, keys_layouts[layout_idx],
                                    COLUMN_ADD_DESC, p_layout_desc,
                                    -1);
            }
            g_free(p_layout_desc);
            layout_idx++;
        }
        g_strfreev(keys_layouts);
        g_key_file_free(p_keyfile);
    }
    g_free(xkbcfg_filepath);

    // callback for double click
    g_signal_connect(p_treeview_add_layout, "button-press-event",
                     G_CALLBACK(on_treeviews_lists_button_press_event),
                     gtk_dialog_get_widget_for_response(GTK_DIALOG(p_dialog), GTK_RESPONSE_OK));
    // sort for description
    gtk_tree_view_column_clicked(p_column_desc);

    gtk_widget_set_size_request(p_dialog, 700, 500);
    gtk_widget_show_all(GTK_WIDGET(p_scrolledwindow_add_layout));
    gint  response = gtk_dialog_run(GTK_DIALOG(p_dialog));
    if(response == GTK_RESPONSE_OK)
    {
        GtkTreeIter  tree_iter_sel;
        GtkTreeSelection *p_treeselection_add_layout = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_treeview_add_layout));
        if(gtk_tree_selection_get_selected(p_treeselection_add_layout,
                                           (GtkTreeModel **)(&p_treestore_add_layout),
                                           &tree_iter_sel))
        {
            gchar *layout_add;
            GString *p_gstring_new_layout = g_string_new("");
            GString *p_gstring_new_variant = g_string_new("");
            gtk_tree_model_get(GTK_TREE_MODEL(p_treestore_add_layout),
                               &tree_iter_sel, COLUMN_ADD_LAYOUT, &layout_add, -1);

            if(strchr(layout_add, '(') == NULL)
            {
                g_string_append(p_gstring_new_layout, layout_add);
            }
            else
            {
                gboolean  parsing_variant = FALSE;
                guchar  i;
                for(i=0; layout_add[i]; i++)
                {
                    if(!parsing_variant)
                    {
                        if(layout_add[i] == '(')
                            parsing_variant = TRUE;
                        else
                            g_string_append_c(p_gstring_new_layout, layout_add[i]);
                    }
                    else
                    {
                        if(layout_add[i] == ')')
                            break;
                        else
                            g_string_append_c(p_gstring_new_variant, layout_add[i]);
                    }
                }
            }
            xkb_add_layout(p_xkb, p_gstring_new_layout->str, p_gstring_new_variant->str);
            xkb_update_layouts_n_variants(p_xkb);

            gtk_widget_set_sensitive(p_xkb->p_button_rm_layout, p_xkb->num_layouts > 1);

            g_free(layout_add);
            g_string_free(p_gstring_new_layout, TRUE/*free also gstring->str*/);
            g_string_free(p_gstring_new_variant, TRUE/*free also gstring->str*/);
        }
    }
    gtk_widget_destroy(p_dialog);
}

void xkb_setxkbmap(XkbPlugin *p_xkb)
{
    GString *p_gstring_syscmd;
    int rc;

    if(p_xkb->keep_system_layouts) return;

    p_gstring_syscmd = g_string_new("");
    g_string_printf(p_gstring_syscmd,
                    "setxkbmap -model %s -layout %s -variant %s -option %s",
                    p_xkb->kbd_model, p_xkb->kbd_layouts, p_xkb->kbd_variants,
                    p_xkb->kbd_change_option);
    if( (p_xkb->kbd_advanced_options != NULL) && strlen(p_xkb->kbd_advanced_options) )
    {
        g_string_append(p_gstring_syscmd, " ");
        g_string_append(p_gstring_syscmd, p_xkb->kbd_advanced_options);
    }
    if(!p_xkb->do_not_reset_opt)
    {
        rc = system("setxkbmap -option"); // reset options
        if(rc)
        {
            g_warning("xkb: system(setxkbmap -option) returned %d", rc);
        }
    }
    rc = system(p_gstring_syscmd->str); // set new map
    if(rc)
    {
        g_warning("xkb: system(%s) returned %d", p_gstring_syscmd->str, rc);
    }
    //g_printf("\n%s\n", p_gstring_syscmd->str);
    g_string_free(p_gstring_syscmd, TRUE/*free also gstring->str*/);
}

static gboolean  layouts_tree_model_foreach(GtkTreeModel *p_model,
                                            GtkTreePath *p_path,
                                            GtkTreeIter *p_iter,
                                            gpointer p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    gchar *layout_val;
    gchar *variant_val;

    gtk_tree_model_get(p_model, p_iter, COLUMN_LAYOUT, &layout_val,  -1);
    gtk_tree_model_get(p_model, p_iter, COLUMN_VARIANT, &variant_val,  -1);

    if(strlen(p_xkb->p_gstring_layouts_partial->str))
    {
        g_string_append_c(p_xkb->p_gstring_layouts_partial, ',');
        g_string_append_c(p_xkb->p_gstring_variants_partial, ',');
    }
    g_string_append(p_xkb->p_gstring_layouts_partial, layout_val);
    g_string_append(p_xkb->p_gstring_variants_partial, variant_val);

    //g_printf("\npartial layouts = '%s'\n", p_xkb->p_gstring_layouts_partial->str);
    //g_printf("partial variants = '%s'\n", p_xkb->p_gstring_variants_partial->str);

    g_free(layout_val);
    g_free(variant_val);
    p_xkb->num_layouts++;
    return FALSE;
}

static void xkb_update_layouts_n_variants(XkbPlugin *p_xkb)
{
    p_xkb->p_gstring_layouts_partial = g_string_new("");
    p_xkb->p_gstring_variants_partial = g_string_new("");
    p_xkb->num_layouts = 0;
    gtk_tree_model_foreach(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                           layouts_tree_model_foreach,
                           p_xkb);
    if(!strlen(p_xkb->p_gstring_variants_partial->str))
    {
        g_string_append_c(p_xkb->p_gstring_variants_partial, ',');
    }
    g_free(p_xkb->kbd_layouts);
    g_free(p_xkb->kbd_variants);
    p_xkb->kbd_layouts = g_strdup(p_xkb->p_gstring_layouts_partial->str);
    p_xkb->kbd_variants = g_strdup(p_xkb->p_gstring_variants_partial->str);
    g_string_free(p_xkb->p_gstring_layouts_partial, TRUE/*free also gstring->str*/);
    g_string_free(p_xkb->p_gstring_variants_partial, TRUE/*free also gstring->str*/);
    xkb_setxkbmap(p_xkb);
    xkb_mechanism_destructor(p_xkb);
    xkb_mechanism_constructor(p_xkb);
    config_group_set_string(p_xkb->settings, "LayoutsList", p_xkb->kbd_layouts);
    config_group_set_string(p_xkb->settings, "VariantsList", p_xkb->kbd_variants);
    xkb_redraw(p_xkb);
}

static void xkb_add_layout(XkbPlugin *p_xkb, gchar *layout, gchar*variant)
{
    GtkTreeIter  tree_iter;
    gtk_list_store_append(p_xkb->p_liststore_layout, &tree_iter);
    gchar *flag_filepath = NULL;
    gchar *flags_dir = (p_xkb->cust_dir_exists && (p_xkb->display_type == DISP_TYPE_IMAGE_CUST)) ? g_strdup(FLAGSCUSTDIR):g_strdup(FLAGSDIR);
    if(strchr(layout, '/') != NULL)
    {
        gchar *layout_mod = g_strdup(layout);
        layout_mod = g_strdelimit(layout_mod, "/", '-');
        flag_filepath = g_strdup_printf(flag_filepath_generator, flags_dir, layout_mod);
        g_free(layout_mod);
    }
    else
    {
        flag_filepath = g_strdup_printf(flag_filepath_generator, flags_dir, layout);
    }
    GdkPixbuf *p_pixbuf = gdk_pixbuf_new_from_file_at_size(flag_filepath, -1, 20, NULL);
    if(p_pixbuf != NULL)
    {
        gtk_list_store_set(p_xkb->p_liststore_layout, &tree_iter,
                           COLUMN_ICON, p_pixbuf,
                           COLUMN_LAYOUT, layout,
                           COLUMN_VARIANT, variant,
                           -1);
        g_object_unref(G_OBJECT(p_pixbuf));
    }
    else
    {
        gtk_list_store_set(p_xkb->p_liststore_layout, &tree_iter,
                           COLUMN_LAYOUT, layout,
                           COLUMN_VARIANT, variant,
                           -1);
    }
    g_free(flag_filepath);
    g_free(flags_dir);
}

static void xkb_settings_fill_layout_tree_model_with_config(XkbPlugin *p_xkb)
{
    p_xkb->num_layouts = 0;
    if(strlen(p_xkb->kbd_layouts) && strlen(p_xkb->kbd_variants))
    {
        char **layouts = g_strsplit_set(p_xkb->kbd_layouts, ",", 0);
        char **variants = g_strsplit_set(p_xkb->kbd_variants, ",", 0);

        while(layouts[p_xkb->num_layouts] != NULL)
        {
            xkb_add_layout(p_xkb, layouts[p_xkb->num_layouts], variants[p_xkb->num_layouts]);
            p_xkb->num_layouts++;
        }
        gtk_widget_set_sensitive(p_xkb->p_button_rm_layout, p_xkb->num_layouts > 1);

        g_strfreev(layouts);
        g_strfreev(variants);
    }
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *xkb_configure(LXPanel *panel, GtkWidget *p)
{
    XkbPlugin * p_xkb = lxpanel_plugin_get_data(p);
    gchar       markup_str[MAX_MARKUP_LEN];

    user_active = FALSE;

    // configuration dialog
    GtkWidget * dlg = gtk_dialog_new_with_buttons(
        _("Keyboard Layout Handler"),
        NULL,
        GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CLOSE,
        GTK_RESPONSE_OK,
        NULL);
    p_xkb->p_dialog_config = GTK_WINDOW(dlg);
    panel_apply_icon(p_xkb->p_dialog_config);

    // main vbox of the config dialog
    GtkWidget * p_hbox_main = gtk_hbox_new(TRUE, 0);
    GtkWidget * p_vbox_left = gtk_vbox_new(FALSE, 0);
    GtkWidget * p_vbox_right = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
                      p_hbox_main);
    gtk_box_pack_start(GTK_BOX(p_hbox_main), p_vbox_left, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_main), p_vbox_right, FALSE, TRUE, 0);


    // 'KEYBOARD MODEL' frame
    p_xkb->p_frame_kbd_model = gtk_frame_new(NULL);
    gtk_widget_set_sensitive(p_xkb->p_frame_kbd_model, !p_xkb->keep_system_layouts);
    GtkWidget * p_label_kbd_model = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Keyboard Model"));
    gtk_label_set_markup(GTK_LABEL(p_label_kbd_model), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_kbd_model), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_xkb->p_frame_kbd_model), p_label_kbd_model);
    gtk_frame_set_shadow_type(GTK_FRAME(p_xkb->p_frame_kbd_model), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_left), p_xkb->p_frame_kbd_model, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_xkb->p_frame_kbd_model), 3);

    // frame alignment
    GtkWidget * p_alignment_kbd_model = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_xkb->p_frame_kbd_model), p_alignment_kbd_model);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_kbd_model), 4, 4, 10, 10);
    p_xkb->p_button_kbd_model = gtk_button_new_with_label(p_xkb->kbd_model);
    g_signal_connect(p_xkb->p_button_kbd_model, "clicked", G_CALLBACK(on_button_kbd_model_clicked), p_xkb);
    gtk_container_add(GTK_CONTAINER(p_alignment_kbd_model), p_xkb->p_button_kbd_model);


    // 'KEYBOARD LAYOUTS' frame
    p_xkb->p_frame_kbd_layouts = gtk_frame_new(NULL);
    gtk_widget_set_sensitive(p_xkb->p_frame_kbd_layouts, !p_xkb->keep_system_layouts);
    GtkWidget * p_label_kbd_layouts = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Keyboard Layouts"));
    gtk_label_set_markup(GTK_LABEL(p_label_kbd_layouts), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_kbd_layouts), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_xkb->p_frame_kbd_layouts), p_label_kbd_layouts);
    gtk_frame_set_shadow_type(GTK_FRAME(p_xkb->p_frame_kbd_layouts), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_left), p_xkb->p_frame_kbd_layouts, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_xkb->p_frame_kbd_layouts), 3);
    gtk_widget_set_size_request(GTK_WIDGET(p_xkb->p_frame_kbd_layouts), 300, 180);

    // frame alignment
    GtkWidget * p_alignment_kbd_layouts = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_xkb->p_frame_kbd_layouts), p_alignment_kbd_layouts);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_kbd_layouts), 4, 4, 10, 10);
    GtkWidget * p_hbox_kbd_layouts = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(p_alignment_kbd_layouts), p_hbox_kbd_layouts);

    // scrolledwindow and buttons
    GtkWidget * p_scrolledwindow_kbd_layouts = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_kbd_layouts),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(p_hbox_kbd_layouts), p_scrolledwindow_kbd_layouts, TRUE, TRUE, 2);
    GtkWidget * p_vbox_kbd_layouts = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(p_hbox_kbd_layouts), p_vbox_kbd_layouts, FALSE, TRUE, 4);
    GtkWidget * p_button_add_layout = gtk_button_new_from_stock(GTK_STOCK_ADD);
    GtkWidget * p_button_up_layout = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    GtkWidget * p_button_down_layout = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    p_xkb->p_button_rm_layout = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(p_button_add_layout, "clicked", G_CALLBACK(on_button_add_layout_clicked), p_xkb);
    g_signal_connect(p_xkb->p_button_rm_layout, "clicked", G_CALLBACK(on_button_rm_layout_clicked), p_xkb);
    g_signal_connect(p_button_up_layout, "clicked", G_CALLBACK(on_button_up_layout_clicked), p_xkb);
    g_signal_connect(p_button_down_layout, "clicked", G_CALLBACK(on_button_down_layout_clicked), p_xkb);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_button_add_layout, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_xkb->p_button_rm_layout, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_button_up_layout, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_button_down_layout, FALSE, TRUE, 0);

    // liststore
    p_xkb->p_liststore_layout = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    p_xkb->p_treeview_layout = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_xkb->p_liststore_layout));
    g_object_unref(G_OBJECT(p_xkb->p_liststore_layout));
    p_xkb->p_treeselection_layout = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_xkb->p_treeview_layout));
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_kbd_layouts), p_xkb->p_treeview_layout);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // icon
    p_renderer = gtk_cell_renderer_pixbuf_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Flag"), p_renderer, "pixbuf", COLUMN_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_xkb->p_treeview_layout), p_column);
    // layout
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Layout"), p_renderer, "text", COLUMN_LAYOUT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_xkb->p_treeview_layout), p_column);
    // variant
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Variant"), p_renderer, "text", COLUMN_VARIANT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_xkb->p_treeview_layout), p_column);
    xkb_settings_fill_layout_tree_model_with_config(p_xkb);


    // 'CHANGE LAYOUT OPTION' frame
    p_xkb->p_frame_change_layout = gtk_frame_new(NULL);
    gtk_widget_set_sensitive(p_xkb->p_frame_change_layout, !p_xkb->keep_system_layouts);
    GtkWidget * p_label_change_layout = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Change Layout Option"));
    gtk_label_set_markup(GTK_LABEL(p_label_change_layout), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_change_layout), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_xkb->p_frame_change_layout), p_label_change_layout);
    gtk_frame_set_shadow_type(GTK_FRAME(p_xkb->p_frame_change_layout), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_left), p_xkb->p_frame_change_layout, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_xkb->p_frame_change_layout), 3);

    // frame alignment
    GtkWidget * p_alignment_change_layout = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_xkb->p_frame_change_layout), p_alignment_change_layout);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_change_layout), 4, 4, 10, 10);
    p_xkb->p_button_change_layout = gtk_button_new_with_label(p_xkb->kbd_change_option);
    g_signal_connect(p_xkb->p_button_change_layout, "clicked", G_CALLBACK(on_button_kbd_change_layout_clicked), p_xkb);
    gtk_container_add(GTK_CONTAINER(p_alignment_change_layout), p_xkb->p_button_change_layout);


    // 'ADVANCED setxkbmap OPTIONS' frame
    GtkWidget * p_frame_advanced_opt = gtk_frame_new(NULL);
    GtkWidget * p_label_advanced_opt = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Advanced setxkbmap Options"));
    gtk_label_set_markup(GTK_LABEL(p_label_advanced_opt), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_advanced_opt), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_advanced_opt), p_label_advanced_opt);
    gtk_frame_set_shadow_type(GTK_FRAME(p_frame_advanced_opt), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_right), p_frame_advanced_opt, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_advanced_opt), 3);

    // frame alignment
    GtkWidget * p_alignment_advanced_opt = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_advanced_opt), p_alignment_advanced_opt);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_advanced_opt), 4, 4, 10, 10);
    GtkWidget * p_vbox_advanced_opt = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(p_alignment_advanced_opt), p_vbox_advanced_opt);
    p_xkb->p_entry_advanced_opt = gtk_entry_new();
    gtk_widget_set_sensitive(p_xkb->p_entry_advanced_opt, !p_xkb->keep_system_layouts);
    if (p_xkb->kbd_advanced_options)
        gtk_entry_set_text(GTK_ENTRY(p_xkb->p_entry_advanced_opt), p_xkb->kbd_advanced_options);
    gtk_entry_set_icon_from_stock(GTK_ENTRY(p_xkb->p_entry_advanced_opt), GTK_ENTRY_ICON_SECONDARY, "gtk-save");
    g_signal_connect(p_xkb->p_entry_advanced_opt, "icon-press", G_CALLBACK(on_xkb_entry_advanced_opt_icon_press), p_xkb);
    gtk_box_pack_start(GTK_BOX(p_vbox_advanced_opt), p_xkb->p_entry_advanced_opt, FALSE, TRUE, 0);
    p_xkb->p_checkbutton_no_reset_opt = gtk_check_button_new_with_mnemonic(_("Do _not reset existing options"));
    gtk_widget_set_sensitive(p_xkb->p_checkbutton_no_reset_opt, !p_xkb->keep_system_layouts);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_xkb->p_checkbutton_no_reset_opt), p_xkb->do_not_reset_opt);
    g_signal_connect(p_xkb->p_checkbutton_no_reset_opt, "toggled", G_CALLBACK(on_xkb_checkbutton_no_reset_opt_toggled), p_xkb);
    gtk_box_pack_start(GTK_BOX(p_vbox_advanced_opt), p_xkb->p_checkbutton_no_reset_opt, FALSE, TRUE, 0);
    GtkWidget *p_checkbutton_keep_system_layouts = gtk_check_button_new_with_mnemonic(_("Keep _system layouts"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_checkbutton_keep_system_layouts), p_xkb->keep_system_layouts);
    g_signal_connect(p_checkbutton_keep_system_layouts, "toggled", G_CALLBACK(on_xkb_checkbutton_keep_system_layouts_toggled), p_xkb);
    gtk_box_pack_start(GTK_BOX(p_vbox_advanced_opt), p_checkbutton_keep_system_layouts, FALSE, TRUE, 0);


    // 'PER WINDOW SETTINGS' frame
    GtkWidget * p_frame_perapp_layout = gtk_frame_new(NULL);
    GtkWidget * p_label_perapp_layout = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Per Window Settings"));
    gtk_label_set_markup(GTK_LABEL(p_label_perapp_layout), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_perapp_layout), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_perapp_layout), p_label_perapp_layout);
    gtk_frame_set_shadow_type(GTK_FRAME(p_frame_perapp_layout), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_right), p_frame_perapp_layout, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_perapp_layout), 3);

    // frame alignment
    GtkWidget * p_alignment_perapp_layout = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_perapp_layout), p_alignment_perapp_layout);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_perapp_layout), 4, 4, 10, 10);
    GtkWidget *p_checkbutton_per_app = gtk_check_button_new_with_mnemonic(_("_Remember layout for each window"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_checkbutton_per_app), p_xkb->enable_perwin);
    g_signal_connect(p_checkbutton_per_app, "toggled", G_CALLBACK(on_xkb_checkbutton_per_app_toggled), p_xkb);
    gtk_container_add(GTK_CONTAINER(p_alignment_perapp_layout), p_checkbutton_per_app);


    // 'SHOW LAYOUT AS' frame
    GtkWidget * p_frame_display_type = gtk_frame_new(NULL);
    GtkWidget * p_label_show_layout_as = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Show Layout as"));
    gtk_label_set_markup(GTK_LABEL(p_label_show_layout_as), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_show_layout_as), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_display_type), p_label_show_layout_as);
    gtk_frame_set_shadow_type(GTK_FRAME(p_frame_display_type), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_right), p_frame_display_type, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_display_type), 3);

    // frame alignment
    GtkWidget * p_alignment_display_type = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_display_type), p_alignment_display_type);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_display_type), 4, 4, 10, 10);
    GtkWidget *p_table = gtk_table_new(3, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(p_table), 10);
    gtk_container_add(GTK_CONTAINER(p_alignment_display_type), p_table);

    // radiobuttons
    GtkWidget * p_image_disp_type_image = gtk_image_new();
    GtkWidget * p_image_disp_type_image_cust = NULL;
    gchar *symbol_name_lowercase = (char *)xkb_get_current_symbol_name_lowercase(p_xkb);
    gchar *flag_filepath = NULL;
    gchar *flag_filepath_cust = NULL;
    if(strchr(symbol_name_lowercase, '/') != NULL)
    {
        gchar *layout_mod = g_strdup(symbol_name_lowercase);
        layout_mod = g_strdelimit(layout_mod, "/", '-');
        flag_filepath = g_strdup_printf(flag_filepath_generator, FLAGSDIR, layout_mod);
        if(p_xkb->cust_dir_exists)
            flag_filepath_cust = g_strdup_printf(flag_filepath_generator, FLAGSCUSTDIR, layout_mod);
        g_free(layout_mod);
    }
    else
    {
        flag_filepath = g_strdup_printf(flag_filepath_generator, FLAGSDIR, symbol_name_lowercase);
        if(p_xkb->cust_dir_exists)
            flag_filepath_cust = g_strdup_printf(flag_filepath_generator, FLAGSCUSTDIR, symbol_name_lowercase);
    }
    GdkPixbuf *p_pixbuf = gdk_pixbuf_new_from_file_at_size(flag_filepath, -1, 16, NULL);
    if(p_pixbuf != NULL)
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(p_image_disp_type_image), p_pixbuf);
        g_object_unref(G_OBJECT(p_pixbuf));
    }
    if(flag_filepath_cust != NULL)
    {
        p_image_disp_type_image_cust = gtk_image_new();
        GdkPixbuf *p_pixbuf_cust = gdk_pixbuf_new_from_file_at_size(flag_filepath_cust, -1, 16, NULL);
        if(p_pixbuf_cust != NULL)
        {
            gtk_image_set_from_pixbuf(GTK_IMAGE(p_image_disp_type_image_cust), p_pixbuf_cust);
            g_object_unref(G_OBJECT(p_pixbuf_cust));
        }
    }
    g_free(flag_filepath);
    if(flag_filepath_cust != NULL)
        g_free(flag_filepath_cust);
    g_free(symbol_name_lowercase);
    GtkWidget * p_label_disp_type_text = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<span font='%d' font_weight='heavy'>%s</span>", 16, xkb_get_current_symbol_name(p_xkb));
    gtk_label_set_markup(GTK_LABEL(p_label_disp_type_text), markup_str);
    GtkWidget * p_radiobutton_disp_type_image = gtk_radio_button_new_with_label(NULL, (const gchar *)_("Image"));
    GtkWidget * p_radiobutton_disp_type_image_cust = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_disp_type_image), (const gchar *)_("Custom Image"));
    GtkWidget * p_radiobutton_disp_type_text = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_disp_type_image), (const gchar *)_("Text"));

    gtk_table_attach(GTK_TABLE(p_table), p_image_disp_type_image, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(p_table), p_radiobutton_disp_type_image, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    if(p_xkb->cust_dir_exists)
    {
        gtk_table_attach(GTK_TABLE(p_table), p_image_disp_type_image_cust, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
        gtk_table_attach(GTK_TABLE(p_table), p_radiobutton_disp_type_image_cust, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    }
    gtk_table_attach(GTK_TABLE(p_table), p_label_disp_type_text, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(p_table), p_radiobutton_disp_type_text, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);

    g_signal_connect(p_radiobutton_disp_type_image, "toggled", G_CALLBACK(on_radiobutton_disp_type_image_toggled), p_xkb);
    if(p_xkb->cust_dir_exists)
        g_signal_connect(p_radiobutton_disp_type_image_cust, "toggled", G_CALLBACK(on_radiobutton_disp_type_image_cust_toggled), p_xkb);
    g_signal_connect(p_radiobutton_disp_type_text, "toggled", G_CALLBACK(on_radiobutton_disp_type_text_toggled), p_xkb);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_disp_type_image), (p_xkb->display_type == DISP_TYPE_IMAGE) ||
                                                                                   ( (p_xkb->display_type == DISP_TYPE_IMAGE_CUST) && (!p_xkb->cust_dir_exists) ) );
    if(p_xkb->cust_dir_exists)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_disp_type_image_cust), p_xkb->display_type == DISP_TYPE_IMAGE_CUST);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_disp_type_text), p_xkb->display_type == DISP_TYPE_TEXT);


    // 'PANEL ICON SIZE' frame
    GtkWidget * p_frame_flag_size = gtk_frame_new(NULL);
    GtkWidget * p_label_flag_size = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Panel Icon Size"));
    gtk_label_set_markup(GTK_LABEL(p_label_flag_size), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_flag_size), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_flag_size), p_label_flag_size);
    gtk_frame_set_shadow_type(GTK_FRAME(p_frame_flag_size), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(p_vbox_right), p_frame_flag_size, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_flag_size), 3);

    // frame alignment
    GtkWidget * p_alignment_flag_size = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_flag_size), p_alignment_flag_size);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_flag_size), 4, 4, 10, 10);
    GtkWidget *p_hbox_flag_size = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(p_alignment_flag_size), p_hbox_flag_size);

    GtkWidget *p_radiobutton_flag_size_1 = gtk_radio_button_new_with_label(NULL, "1");
    GtkWidget *p_radiobutton_flag_size_2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_flag_size_1), "2");
    GtkWidget *p_radiobutton_flag_size_3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_flag_size_1), "3");
    GtkWidget *p_radiobutton_flag_size_4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_flag_size_1), "4");
    GtkWidget *p_radiobutton_flag_size_5 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_flag_size_1), "5");
    GtkWidget *p_radiobutton_flag_size_6 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_flag_size_1), "6");
    g_signal_connect(p_radiobutton_flag_size_1, "toggled", G_CALLBACK(on_radiobutton_flag_size_1_toggled), p_xkb);
    g_signal_connect(p_radiobutton_flag_size_2, "toggled", G_CALLBACK(on_radiobutton_flag_size_2_toggled), p_xkb);
    g_signal_connect(p_radiobutton_flag_size_3, "toggled", G_CALLBACK(on_radiobutton_flag_size_3_toggled), p_xkb);
    g_signal_connect(p_radiobutton_flag_size_4, "toggled", G_CALLBACK(on_radiobutton_flag_size_4_toggled), p_xkb);
    g_signal_connect(p_radiobutton_flag_size_5, "toggled", G_CALLBACK(on_radiobutton_flag_size_5_toggled), p_xkb);
    g_signal_connect(p_radiobutton_flag_size_6, "toggled", G_CALLBACK(on_radiobutton_flag_size_6_toggled), p_xkb);
    gtk_box_pack_start(GTK_BOX(p_hbox_flag_size), p_radiobutton_flag_size_1, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(p_hbox_flag_size), p_radiobutton_flag_size_2, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(p_hbox_flag_size), p_radiobutton_flag_size_3, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(p_hbox_flag_size), p_radiobutton_flag_size_4, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(p_hbox_flag_size), p_radiobutton_flag_size_5, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(p_hbox_flag_size), p_radiobutton_flag_size_6, TRUE, TRUE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_flag_size_1), p_xkb->flag_size == 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_flag_size_2), p_xkb->flag_size == 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_flag_size_3), p_xkb->flag_size == 3);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_flag_size_4), p_xkb->flag_size == 4);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_flag_size_5), p_xkb->flag_size == 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_flag_size_6), p_xkb->flag_size == 6);

    /* Connect signals. */
    g_signal_connect(dlg, "response", G_CALLBACK(on_dialog_config_response), p_xkb);

    /* Display the dialog. */
    gtk_widget_show_all(p_hbox_main);

    user_active = TRUE;
    return dlg;
}

/* Callback when panel configuration changes. */
static void xkb_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    /* Do a full redraw. */
    xkb_redraw(lxpanel_plugin_get_data(p));
}

FM_DEFINE_MODULE(lxpanel_gtk, xkb)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Keyboard Layout Handler"),
    .description = N_("Handle keyboard layouts"),

    .new_instance = xkb_constructor,
    .config = xkb_configure,
    .reconfigure = xkb_panel_configuration_changed,
    .button_press_event = on_xkb_button_press_event
};
