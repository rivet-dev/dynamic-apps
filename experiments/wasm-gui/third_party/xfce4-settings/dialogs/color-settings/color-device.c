/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <colord.h>
#include "color-device.h"

struct _ColorDevice
{
    GtkListBoxRow parent_instance;

    CdDevice    *device;
    gboolean     enabled;
    gchar       *sortable;
    GtkWidget   *widget_description;
    GtkWidget   *widget_icon;
    GtkWidget   *widget_switch;
    guint        device_changed_id;
};

G_DEFINE_TYPE (ColorDevice, color_device, GTK_TYPE_LIST_BOX_ROW)

enum
{
    SIGNAL_ENABLED_CHANGED,
    SIGNAL_LAST
};

enum
{
    PROP_0,
    PROP_DEVICE,
    PROP_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

gchar *
color_device_get_kind (CdDevice *device)
{
    if (cd_device_get_kind (device) == CD_DEVICE_KIND_DISPLAY)
        /* TRANSLATORS: an externally connected display, where %s is either the
         * model, vendor or ID, e.g. 'LP2480zx Monitor' */
        return _("Monitor");
    else if (cd_device_get_kind (device) == CD_DEVICE_KIND_SCANNER)
        /* TRANSLATORS: a flatbed scanner device, e.g. 'Epson Scanner' */
        return _("Scanner");
    else if (cd_device_get_kind (device) == CD_DEVICE_KIND_CAMERA)
        /* TRANSLATORS: a camera device, e.g. 'Nikon D60 Camera' */
        return _("Camera");
    else if (cd_device_get_kind (device) == CD_DEVICE_KIND_PRINTER)
        /* TRANSLATORS: a printer device, e.g. 'Epson Photosmart Printer' */
        return _("Printer");
    else if (cd_device_get_kind (device) == CD_DEVICE_KIND_WEBCAM)
        /* TRANSLATORS: a webcam device, e.g. 'Philips HiDef Camera' */
        return _("Webcam");
    else
        return NULL;
}

gchar *
color_device_get_title (CdDevice *device)
{
    const gchar *tmp;
    GString *string;

    string = g_string_new ("");

    /* is laptop panel */
    if (cd_device_get_kind (device) == CD_DEVICE_KIND_DISPLAY &&
        cd_device_get_embedded (device)) {
        /* TRANSLATORS: This refers to the TFT display on a laptop */
        g_string_append (string, _("Laptop Screen"));
        goto out;
    }

    /* is internal webcam */
    if (cd_device_get_kind (device) == CD_DEVICE_KIND_WEBCAM &&
        cd_device_get_embedded (device)) {
        /* TRANSLATORS: This refers to the embedded webcam on a laptop */
        g_string_append (string, _("Built-in Webcam"));
        goto out;
    }

    /* get the display model, falling back to something sane */
    tmp = cd_device_get_model (device);
    if (tmp == NULL)
        tmp = cd_device_get_vendor (device);
    if (tmp == NULL)
        tmp = cd_device_get_id (device);

    if (color_device_get_kind (device))
        g_string_append_printf (string, "%s %s", tmp, color_device_get_kind (device));
    else
        g_string_append (string, tmp);

out:
    return g_string_free (string, FALSE);
}

static const gchar *
color_device_kind_to_sort (CdDevice *device)
{
    CdDeviceKind kind = cd_device_get_kind (device);
    if (kind == CD_DEVICE_KIND_DISPLAY)
        return "4";
    if (kind == CD_DEVICE_KIND_SCANNER)
        return "3";
    if (kind == CD_DEVICE_KIND_CAMERA)
        return "2";
    if (kind == CD_DEVICE_KIND_WEBCAM)
        return "1";
    if (kind == CD_DEVICE_KIND_PRINTER)
        return "0";
    return "9";
}

const gchar *
color_device_get_type_icon (CdDevice *device)
{
    CdDeviceKind kind = cd_device_get_kind (device);
    if (kind == CD_DEVICE_KIND_DISPLAY)
        return "video-display";
    if (kind == CD_DEVICE_KIND_SCANNER)
        return "scanner";
    if (kind == CD_DEVICE_KIND_CAMERA)
        return "camera-photo";
    if (kind == CD_DEVICE_KIND_WEBCAM)
        return "camera-web";
    if (kind == CD_DEVICE_KIND_PRINTER)
        return "printer";
    return "dialog-question";
}

gchar *
color_device_get_sortable_base (CdDevice *device)
{
    g_autofree gchar *title = color_device_get_title (device);
    return g_strdup_printf ("%s-%s-%s",
                            color_device_kind_to_sort (device),
                            cd_device_get_id (device),
                            title);
}

static void
color_device_refresh (ColorDevice *color_device)
{
    g_autofree gchar *title = NULL;
    g_autoptr(GPtrArray) profiles = NULL;
    AtkObject *accessible;
    g_autofree gchar *name1 = NULL;
    g_autofree gchar *device_icon = NULL;

    /* add switch and expander if there are profiles, otherwise use a label */
    profiles = cd_device_get_profiles (color_device->device);
    if (profiles == NULL)
        return;

    title = color_device_get_title (color_device->device);
    gtk_label_set_label (GTK_LABEL (color_device->widget_description), title);
    gtk_widget_set_visible (color_device->widget_description, TRUE);

    device_icon = g_strdup_printf ("%s-symbolic", color_device_get_type_icon (color_device->device));
    gtk_image_set_from_icon_name (GTK_IMAGE (color_device->widget_icon),
                                  device_icon,
                                  GTK_ICON_SIZE_MENU);
    gtk_widget_set_visible (color_device->widget_icon, TRUE);
    gtk_widget_set_visible (color_device->widget_switch, profiles->len > 0);

    gtk_switch_set_active (GTK_SWITCH (color_device->widget_switch),
                           cd_device_get_enabled (color_device->device));

    accessible = gtk_widget_get_accessible (color_device->widget_switch);
    name1 = g_strdup_printf (_("Enable color management for %s"), title);
    atk_object_set_name (accessible, name1);
}

CdDevice *
color_device_get_device (ColorDevice *color_device)
{
    g_return_val_if_fail (SETTINGS_IS_COLOR_DEVICE (color_device), NULL);
    return color_device->device;
}

const gchar *
color_device_get_sortable (ColorDevice *color_device)
{
    g_return_val_if_fail (SETTINGS_IS_COLOR_DEVICE (color_device), NULL);
    return color_device->sortable;
}

static void
color_device_get_property (GObject *object, guint param_id,
                           GValue *value, GParamSpec *pspec)
{
    ColorDevice *color_device = SETTINGS_COLOR_DEVICE (object);
    switch (param_id)
    {
        case PROP_DEVICE:
            g_value_set_object (value, color_device->device);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}

static void
color_device_set_property (GObject *object, guint param_id,
                              const GValue *value, GParamSpec *pspec)
{
    ColorDevice *color_device = SETTINGS_COLOR_DEVICE (object);

    switch (param_id)
    {
        case PROP_DEVICE:
            color_device->device = g_value_dup_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}

static void
color_device_finalize (GObject *object)
{
    ColorDevice *color_device = SETTINGS_COLOR_DEVICE (object);

    if (color_device->device_changed_id > 0)
        g_signal_handler_disconnect (color_device->device, color_device->device_changed_id);

    g_free (color_device->sortable);
    g_object_unref (color_device->device);

    G_OBJECT_CLASS (color_device_parent_class)->finalize (object);
}

void
color_device_set_enabled (ColorDevice *color_device,
                              gboolean enabled)
{
    /* same as before */
    if (color_device->enabled == enabled)
        return;

    /* refresh */
    color_device->enabled = enabled;

    g_signal_emit (color_device,
                   signals[SIGNAL_ENABLED_CHANGED], 0,
                   color_device->enabled);
    color_device_refresh (color_device);
}

static void
color_device_notify_enable_device_cb (GtkSwitch *sw,
                                         GParamSpec *pspec,
                                         gpointer user_data)
{
    ColorDevice *color_device = SETTINGS_COLOR_DEVICE (user_data);
    gboolean enable;
    gboolean ret;
    g_autoptr(GError) error = NULL;

    enable = gtk_switch_get_active (sw);
    g_debug ("Set %s to %i", cd_device_get_id (color_device->device), enable);
    ret = cd_device_set_enabled_sync (color_device->device,
                                      enable, NULL, &error);
    if (!ret)
      g_warning ("failed to %s to the device: %s",
                 enable ? "enable" : "disable", error->message);

    /* if enabled, close */
    color_device_set_enabled (color_device, enable);
}

static void
color_device_changed_cb (CdDevice *device,
                                   ColorDevice *color_device)
{
    color_device_refresh (color_device);
}

static void
color_device_constructed (GObject *object)
{
    ColorDevice *color_device = SETTINGS_COLOR_DEVICE (object);
    g_autofree gchar *sortable_tmp = NULL;

    /* watch the device for changes */
    color_device->device_changed_id =
      g_signal_connect (color_device->device, "changed",
                        G_CALLBACK (color_device_changed_cb), color_device);

    /* calculate sortable -- FIXME: we have to hack this as EggListBox
     * does not let us specify a GtkSortType:
     * https://bugzilla.gnome.org/show_bug.cgi?id=691341 */
    sortable_tmp = color_device_get_sortable_base (color_device->device);
    color_device->sortable = g_strdup_printf ("%sXX", sortable_tmp);

    color_device->enabled = cd_device_get_enabled (color_device->device);

    color_device_refresh (color_device);

    /* watch to see if the user flicked the switch */
    g_signal_connect (G_OBJECT (color_device->widget_switch), "notify::active",
                      G_CALLBACK (color_device_notify_enable_device_cb),
                      color_device);
}

static void
color_device_class_init (ColorDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = color_device_get_property;
    object_class->set_property = color_device_set_property;
    object_class->constructed = color_device_constructed;
    object_class->finalize = color_device_finalize;

    g_object_class_install_property (object_class, PROP_DEVICE,
                                     g_param_spec_object ("device", NULL,
                                                          NULL,
                                                          CD_TYPE_DEVICE,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    signals [SIGNAL_ENABLED_CHANGED] =
        g_signal_new ("enabled-changed",
                      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
color_device_init (ColorDevice *color_device)
{
    GtkWidget *box;

    /* description */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 9);
    color_device->widget_icon = gtk_image_new ();
    gtk_widget_set_margin_start (color_device->widget_icon, 12);
    gtk_widget_set_margin_end (color_device->widget_icon, 3);
    gtk_box_pack_start (GTK_BOX (box), color_device->widget_icon, FALSE , FALSE, 0);

    color_device->widget_description = gtk_label_new ("");
    gtk_widget_set_margin_top (color_device->widget_description, 12);
    gtk_widget_set_margin_bottom (color_device->widget_description, 12);
    gtk_widget_set_halign (color_device->widget_description, GTK_ALIGN_START);
    gtk_label_set_ellipsize (GTK_LABEL (color_device->widget_description), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign (GTK_LABEL (color_device->widget_description), 0);
    gtk_box_pack_start (GTK_BOX (box), color_device->widget_description, TRUE, TRUE, 0);

    /* switch */
    color_device->widget_switch = gtk_switch_new ();
    gtk_widget_set_valign (color_device->widget_switch, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end (color_device->widget_switch, 12);
    gtk_box_pack_start (GTK_BOX (box), color_device->widget_switch, FALSE, FALSE, 0);

    /* refresh */
    gtk_container_add (GTK_CONTAINER (color_device), box);
    gtk_widget_set_visible (box, TRUE);
}

GtkWidget *
color_device_new (CdDevice *device)
{
    return g_object_new (TYPE_COLOR_DEVICE,
                         "device", device,
                         NULL);
}
