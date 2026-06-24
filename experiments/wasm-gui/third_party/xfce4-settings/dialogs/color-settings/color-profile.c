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

#include "color-profile.h"

struct _ColorProfile
{
    GtkListBoxRow parent_instance;

    GtkWidget   *box;
    CdDevice    *device;
    CdProfile   *profile;
    gboolean     is_default;
    GtkWidget   *widget_description;
    GtkWidget   *widget_image;
    guint        device_changed_id;
    guint        profile_changed_id;
};


#define IMAGE_WIDGET_PADDING 9

G_DEFINE_TYPE (ColorProfile, color_profile, GTK_TYPE_LIST_BOX_ROW)

enum
{
    PROP_0,
    PROP_DEVICE,
    PROP_PROFILE,
    PROP_IS_DEFAULT,
    PROP_LAST
};

static gchar *
color_profile_get_profile_date (CdProfile *profile)
{
    gint64 created;
    g_autoptr(GDateTime) dt = NULL;

    /* get profile age */
    created = cd_profile_get_created (profile);
    if (created == 0)
        return NULL;
    dt = g_date_time_new_from_unix_utc (created);
    if (dt)
        return g_date_time_format (dt, "%x");
    else
        return NULL;
}

static gchar *
gcm_prefs_get_profile_title (CdProfile *profile)
{
    CdColorspace colorspace;
    const gchar *tmp;
    GString *str;

    str = g_string_new ("");

    /* add date only if it's a calibration profile or the profile has
     * not been tagged with this data */
    tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_DATA_SOURCE);
    if (tmp == NULL || g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_CALIB) == 0) {
        tmp = color_profile_get_profile_date (profile);
        if (tmp != NULL)
          g_string_append_printf (str, "%s - ", tmp);
    }
    else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0) {
        /* TRANSLATORS: standard spaces are well known colorspaces like
         * sRGB, AdobeRGB and ProPhotoRGB */
        g_string_append_printf (str, "%s - ", _("Standard Space"));
    }
    else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0) {
        /* TRANSLATORS: test profiles do things like changing the screen
         * a different color, or swap the red and green channels */
        g_string_append_printf (str, "%s - ", _("Test Profile"));
    }
    else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0) {
        /* TRANSLATORS: automatic profiles are generated automatically
         * by the color management system based on manufacturing data,
         * for instance the default monitor profile is created from the
         * primaries specified in the monitor EDID */
        g_string_append_printf (str, "%s - ", C_("Automatically generated profile", "Automatic"));
    }

    /* add quality if it exists */
    tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_QUALITY);
    if (g_strcmp0 (tmp, CD_PROFILE_METADATA_QUALITY_LOW) == 0) {
        /* TRANSLATORS: the profile quality - low quality profiles take
         * much less time to generate but may be a poor reflection of the
         * device capability */
        g_string_append_printf (str, "%s - ", C_("Profile quality", "Low Quality"));
    }
    else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_QUALITY_MEDIUM) == 0) {
        /* TRANSLATORS: the profile quality */
        g_string_append_printf (str, "%s - ", C_("Profile quality", "Medium Quality"));
    }
    else if (g_strcmp0 (tmp, CD_PROFILE_METADATA_QUALITY_HIGH) == 0) {
        /* TRANSLATORS: the profile quality - high quality profiles take
         * a *long* time, and have the best calibration and
         * characterisation data. */
        g_string_append_printf (str, "%s - ", C_("Profile quality", "High Quality"));
    }

    /* add profile description */
    tmp = cd_profile_get_title (profile);
    if (tmp != NULL) {
        g_string_append (str, tmp);
        goto out;
    }

    /* some meta profiles do not have ICC profiles */
    colorspace = cd_profile_get_colorspace (profile);
    if (colorspace == CD_COLORSPACE_RGB) {
        /* TRANSLATORS: this default RGB space is used for printers that
         * do not have additional printer profiles specified in the PPD */
        g_string_append (str, C_("Colorspace fallback", "Default RGB"));
        goto out;
    }
    if (colorspace == CD_COLORSPACE_CMYK) {
        /* TRANSLATORS: this default CMYK space is used for printers that
         * do not have additional printer profiles specified in the PPD */
        g_string_append (str, C_("Colorspace fallback", "Default CMYK"));
        goto out;
    }
    if (colorspace == CD_COLORSPACE_GRAY) {
        /* TRANSLATORS: this default gray space is used for printers that
         * do not have additional printer profiles specified in the PPD */
        g_string_append (str, C_("Colorspace fallback", "Default Gray"));
        goto out;
    }

    /* fall back to ID, ick */
    tmp = g_strdup (cd_profile_get_id (profile));
    g_string_append (str, tmp);
out:
    return g_string_free (str, FALSE);
}

static void
color_profile_refresh (ColorProfile *color_profile)
{
    g_autofree gchar *title = NULL;

    /* show the image if the profile is default */
    gtk_widget_set_visible (color_profile->widget_image, color_profile->is_default);
    gtk_widget_set_margin_start (color_profile->widget_description,
                                 color_profile->is_default ? 0 : IMAGE_WIDGET_PADDING * 2 + 16);

    /* set the title */
    title = gcm_prefs_get_profile_title (color_profile->profile);
    gtk_label_set_markup (GTK_LABEL (color_profile->widget_description), title);
}

CdDevice *
color_profile_get_device (ColorProfile *color_profile)
{
    g_return_val_if_fail (SETTINGS_IS_COLOR_PROFILE (color_profile), NULL);
    return color_profile->device;
}

CdProfile *
color_profile_get_profile (ColorProfile *color_profile)
{
    g_return_val_if_fail (SETTINGS_IS_COLOR_PROFILE (color_profile), NULL);
    return color_profile->profile;
}

gboolean
color_profile_get_is_default (ColorProfile *color_profile)
{
    g_return_val_if_fail (SETTINGS_IS_COLOR_PROFILE (color_profile), 0);
    return color_profile->is_default;
}

void
color_profile_set_is_default (ColorProfile *color_profile, gboolean is_default)
{
    g_return_if_fail (SETTINGS_IS_COLOR_PROFILE (color_profile));
    color_profile->is_default = is_default;
    color_profile_refresh (color_profile);
}

static void
color_profile_get_property (GObject *object, guint param_id,
                               GValue *value, GParamSpec *pspec)
{
    ColorProfile *color_profile = SETTINGS_COLOR_PROFILE (object);
    switch (param_id)
    {
        case PROP_DEVICE:
            g_value_set_object (value, color_profile->device);
            break;
        case PROP_PROFILE:
            g_value_set_object (value, color_profile->profile);
            break;
        case PROP_IS_DEFAULT:
            g_value_set_boolean (value, color_profile->is_default);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}

static void
color_profile_set_property (GObject *object, guint param_id,
                               const GValue *value, GParamSpec *pspec)
{
    ColorProfile *color_profile = SETTINGS_COLOR_PROFILE (object);

    switch (param_id)
    {
        case PROP_DEVICE:
            color_profile->device = g_value_dup_object (value);
            break;
        case PROP_PROFILE:
            color_profile->profile = g_value_dup_object (value);
            break;
        case PROP_IS_DEFAULT:
            color_profile->is_default = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}

static void
color_profile_finalize (GObject *object)
{
    ColorProfile *color_profile = SETTINGS_COLOR_PROFILE (object);

    if (color_profile->device_changed_id > 0)
        g_signal_handler_disconnect (color_profile->device, color_profile->device_changed_id);
    if (color_profile->profile_changed_id > 0)
        g_signal_handler_disconnect (color_profile->profile, color_profile->profile_changed_id);

    g_object_unref (color_profile->device);
    g_object_unref (color_profile->profile);

    G_OBJECT_CLASS (color_profile_parent_class)->finalize (object);
}

static void
color_profile_changed_cb (CdDevice *device,
                          ColorProfile *color_profile)
{
    g_autoptr(CdProfile) profile = NULL;

    /* check to see if the default has changed */
    profile = cd_device_get_default_profile (device);
    if (profile != NULL)
        color_profile->is_default = g_strcmp0 (cd_profile_get_object_path (profile),
                                               cd_profile_get_object_path (color_profile->profile)) == 0;

    color_profile_refresh (color_profile);
}

static void
color_profile_constructed (GObject *object)
{
    ColorProfile *color_profile = SETTINGS_COLOR_PROFILE (object);

    /* watch to see if the default changes */
    color_profile->device_changed_id =
        g_signal_connect (color_profile->device, "changed",
                          G_CALLBACK (color_profile_changed_cb), color_profile);
    color_profile->profile_changed_id =
        g_signal_connect (color_profile->profile, "changed",
                          G_CALLBACK (color_profile_changed_cb), color_profile);

    color_profile_refresh (color_profile);
}

static void
color_profile_class_init (ColorProfileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = color_profile_get_property;
    object_class->set_property = color_profile_set_property;
    object_class->constructed = color_profile_constructed;
    object_class->finalize = color_profile_finalize;

    g_object_class_install_property (object_class, PROP_DEVICE,
                                     g_param_spec_object ("device", NULL,
                                                          NULL,
                                                          CD_TYPE_DEVICE,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_PROFILE,
                                     g_param_spec_object ("profile", NULL,
                                                          NULL,
                                                          CD_TYPE_PROFILE,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_IS_DEFAULT,
                                     g_param_spec_boolean ("is-default", NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
color_profile_init (ColorProfile *color_profile)
{
    GtkWidget *box;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 9);

    /* default tick */
    color_profile->widget_image = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_margin_start (color_profile->widget_image, IMAGE_WIDGET_PADDING);
    //gtk_widget_set_margin_end (color_profile->widget_image, IMAGE_WIDGET_PADDING);
    gtk_box_pack_start (GTK_BOX (box), color_profile->widget_image, FALSE, FALSE, 0);

    /* description */
    color_profile->widget_description = gtk_label_new ("");
    gtk_widget_set_margin_top (color_profile->widget_description, 9);
    gtk_widget_set_margin_bottom (color_profile->widget_description, 9);
    gtk_widget_set_halign (color_profile->widget_description, GTK_ALIGN_START);
    gtk_label_set_ellipsize (GTK_LABEL (color_profile->widget_description), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign (GTK_LABEL (color_profile->widget_description), 0);
    gtk_box_pack_start (GTK_BOX (box), color_profile->widget_description, TRUE, TRUE, 0);
    gtk_widget_show (color_profile->widget_description);

    /* refresh */
    gtk_container_add (GTK_CONTAINER (color_profile), box);
    gtk_widget_set_visible (box, TRUE);
}

GtkWidget *
color_profile_new (CdDevice *device,
                   CdProfile *profile,
                   gboolean is_default)
{
    return g_object_new (TYPE_COLOR_PROFILE,
                         "device", device,
                         "profile", profile,
                         "is-default", is_default,
                         NULL);
}
