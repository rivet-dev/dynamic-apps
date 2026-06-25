/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2023 Brian Tarricone <brian@tarricone.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfce-notify-common.h"
#include "xfce-notify-enum-types.h"

typedef struct {
    GString *sanitized;
    gboolean a_has_href;
} MarkupState;

static void
markup_start_elem(GMarkupParseContext *context,
                  const gchar *element_name,
                  const gchar **attribute_names,
                  const gchar **attribute_values,
                  gpointer user_data,
                  GError **error)
{
    MarkupState *state = user_data;

    if (strcmp(element_name, "b") == 0 ||
        strcmp(element_name, "i") == 0 ||
        strcmp(element_name, "u") == 0)
    {
        g_string_append_c(state->sanitized, '<');
        g_string_append(state->sanitized, element_name);
        g_string_append_c(state->sanitized, '>');
    } else if (strcmp(element_name, "a") == 0) {
        // XXX: this method of tracking that the <a> tag has a href= attr
        // doesn't work if the client nests another <a> inside this one.
        state->a_has_href = FALSE;
        for (gint i = 0; attribute_names[i] != NULL; ++i) {
            if (strcmp(attribute_names[i], "href") == 0) {
                g_string_append_printf(state->sanitized, "<a href=\"%s\">", attribute_values[i]);
                state->a_has_href = TRUE;
                break;
            }
        }
    } else if (strcmp(element_name, "img") == 0) {
        // We don't support <img>, but if there's an alt= attr, use it.
        for (gint i = 0; attribute_names[i] != NULL; ++i) {
            if (strcmp(attribute_names[i], "alt") == 0) {
                g_string_append_printf(state->sanitized, " [%s] ", attribute_values[i]);
            }
        }
    }
}

static void
markup_end_elem(GMarkupParseContext *context,
                const gchar *element_name,
                gpointer user_data,
                GError **error)
{
    MarkupState *state = user_data;

    if (strcmp(element_name, "b") == 0 ||
        strcmp(element_name, "i") == 0 ||
        strcmp(element_name, "u") == 0 ||
        (strcmp(element_name, "a") == 0 && state->a_has_href))
    {
        g_string_append(state->sanitized, "</");
        g_string_append(state->sanitized, element_name);
        g_string_append_c(state->sanitized, '>');
    }
}

static void
markup_text(GMarkupParseContext *context,
            const gchar *text,
            gsize text_len,
            gpointer user_data,
            GError **error)
{
    MarkupState *state = user_data;
    gchar *escaped = g_markup_escape_text(text, text_len);
    g_string_append(state->sanitized, escaped);
    g_free(escaped);
}

// We can't use pango_parse_markup(), as that does not support hyperlinks.
gchar *
xfce_notify_sanitize_markup(const gchar *markup) {
    if (G_LIKELY(markup != NULL)) {
        const GMarkupParser parser = {
            .start_element = markup_start_elem,
            .end_element = markup_end_elem,
            .text = markup_text,
            .passthrough = NULL,
            .error = NULL,
        };
        GMarkupParseContext *ctx;
        MarkupState state = { NULL, FALSE };
        gchar *p;
        gboolean needs_root;
        gboolean valid;

        p = (gchar *)markup;
        while (*p != '\0' && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
            ++p;
        }
        needs_root = strncmp(p, "<markup>", 8) != 0;

        state.sanitized = g_string_sized_new(strlen(markup));
        ctx = g_markup_parse_context_new(&parser, 0, &state, NULL);

        valid = (!needs_root || g_markup_parse_context_parse(ctx, "<markup>", -1, NULL))
            && g_markup_parse_context_parse(ctx, markup, -1, NULL)
            && (!needs_root || g_markup_parse_context_parse(ctx, "</markup>", -1, NULL))
            && g_markup_parse_context_end_parse(ctx, NULL);

        g_markup_parse_context_free(ctx);

        if (valid) {
            return g_string_free(state.sanitized, FALSE);
        } else {
            g_string_free(state.sanitized, TRUE);
            return g_markup_escape_text(p, -1);
        }
    } else {
        return NULL;
    }
}

GtkWidget *
xfce_notify_create_placeholder_label(const gchar *markup) {
    GtkWidget *label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_sensitive(label, FALSE);
    gtk_widget_set_margin_start(label, 24);
    gtk_widget_set_margin_end(label, 24);
    gtk_widget_set_margin_top(label, 24);
    gtk_widget_set_margin_bottom(label, 24);
    return label;
}

gint
xfce_notify_enum_value_from_nick(GType enum_type, const gchar *nick, gint default_value) {
    gint value = default_value;

    if (nick != NULL) {
        GEnumClass *klass = g_type_class_ref(enum_type);
        GEnumValue *enum_value = g_enum_get_value_by_nick(klass, nick);

        if (enum_value != NULL) {
            value = enum_value->value;
        }

        g_type_class_unref(klass);
    }

    return value;
}

gchar *
xfce_notify_enum_nick_from_value(GType enum_type, gint value) {
    gchar *nick = NULL;
    GEnumClass *klass = g_type_class_ref(enum_type);
    GEnumValue *enum_value = g_enum_get_value(klass, value);

    if (enum_value != NULL) {
        nick = g_strdup(enum_value->value_nick);
    }

    g_type_class_unref(klass);

    return nick;
}

gint
xfce_notify_xfconf_channel_get_enum(XfconfChannel *channel,
                                    const gchar *property_name,
                                    gint default_value,
                                    GType enum_type)
{
    const gchar *nick = xfconf_channel_get_string(channel, property_name, NULL);
    if (nick == NULL) {
        return default_value;
    } else {
        return xfce_notify_enum_value_from_nick(enum_type, nick, default_value);
    }
}


static void
xfce_notify_migrate_log_max_size_setting(XfconfChannel *channel) {
    if (!xfconf_channel_has_property(channel, LOG_MAX_SIZE_ENABLED_PROP)) {
        guint value = xfconf_channel_get_uint(channel, LOG_MAX_SIZE_PROP, LOG_MAX_SIZE_DEFAULT);
        xfconf_channel_set_bool(channel, LOG_MAX_SIZE_ENABLED_PROP, value > 0);
        if (value == 0) {
            xfconf_channel_set_uint(channel, LOG_MAX_SIZE_PROP, LOG_MAX_SIZE_DEFAULT);
        }
    }
}

static void
xfce_notify_migrate_show_notifications_on_setting(XfconfChannel *channel) {
    if (xfconf_channel_has_property(channel, "/primary-monitor")) {
        guint value = xfconf_channel_get_uint(channel, "/primary-monitor", 0);
        gchar *new_value = xfce_notify_enum_nick_from_value(XFCE_TYPE_NOTIFY_SHOW_ON,
                                                            value == 1
                                                            ? XFCE_NOTIFY_SHOW_ON_PRIMARY_MONITOR
                                                            : SHOW_NOTIFICATIONS_ON_DEFAULT);
        if (G_LIKELY(new_value != NULL)) {
            xfconf_channel_set_string(channel, SHOW_NOTIFICATIONS_ON_PROP, new_value);
            xfconf_channel_reset_property(channel, "/primary-monitor", FALSE);
            g_free(new_value);
        }
    }
}

static void
xfce_notify_migrate_enum_setting(XfconfChannel *channel, const gchar *property_name, GType enum_type) {
    if (xfconf_channel_has_property(channel, property_name)) {
        GValue value = G_VALUE_INIT;

        xfconf_channel_get_property(channel, property_name, &value);
        if (G_VALUE_HOLDS_UINT(&value)) {
            gchar *nick = xfce_notify_enum_nick_from_value(enum_type, g_value_get_uint(&value));

            if (nick != NULL) {
                xfconf_channel_reset_property(channel, property_name, FALSE);
                xfconf_channel_set_string(channel, property_name, nick);
                g_free(nick);
            }
        }

        g_value_unset(&value);
    }
}

void
xfce_notify_migrate_settings(XfconfChannel *channel) {
    xfce_notify_migrate_log_max_size_setting(channel);
    xfce_notify_migrate_show_notifications_on_setting(channel);
    xfce_notify_migrate_enum_setting(channel, DATETIME_FORMAT_PROP, XFCE_TYPE_NOTIFY_DATETIME_FORMAT);
    xfce_notify_migrate_enum_setting(channel, LOG_LEVEL_PROP, XFCE_TYPE_LOG_LEVEL);
    xfce_notify_migrate_enum_setting(channel, LOG_LEVEL_APPS_PROP, XFCE_TYPE_LOG_LEVEL_APPS);
    xfce_notify_migrate_enum_setting(channel, NOTIFY_LOCATION_PROP, XFCE_TYPE_NOTIFY_POSITION);
}
