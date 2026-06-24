/*
 *  xfce4-settings-editor
 *
 *  Copyright (c) 2012      Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfce-settings-cell-renderer.h"
#include "xfce-settings-marshal.h"



struct _XfceSettingsCellRendererClass
{
    GtkCellRendererClass __parent__;
};

struct _XfceSettingsCellRenderer
{
    GtkCellRenderer __parent__;

    GValue           cell_value;

    guint            locked : 1;

    GtkCellRenderer *renderer_text;
    GtkCellRenderer *renderer_toggle;
};

enum
{
    PROP_0,
    PROP_VALUE,
    PROP_LOCKED
};

enum
{
    VALUE_CHANGED,
    LAST_SIGNAL
};

static GQuark edit_data_quark = 0;

static guint  renderer_signals[LAST_SIGNAL];



static void             xfce_settings_cell_renderer_set_property  (GObject              *object,
                                                                   guint                 prop_id,
                                                                   const GValue         *value,
                                                                   GParamSpec           *pspec);
static void             xfce_settings_cell_renderer_get_property  (GObject              *object,
                                                                   guint                 prop_id,
                                                                   GValue               *value,
                                                                   GParamSpec           *pspec);
static void             xfce_settings_cell_renderer_finalize      (GObject              *object);
static void             xfce_settings_cell_renderer_render        (GtkCellRenderer      *cell,
                                                                   cairo_t              *cr,
                                                                   GtkWidget            *widget,
                                                                   const GdkRectangle   *background_area,
                                                                   const GdkRectangle   *cell_area,
                                                                   GtkCellRendererState  flags);
static gint             xfce_settings_cell_renderer_activate      (GtkCellRenderer      *cell,
                                                                   GdkEvent             *event,
                                                                   GtkWidget            *widget,
                                                                   const gchar          *path,
                                                                   const GdkRectangle   *background_area,
                                                                   const GdkRectangle   *cell_area,
                                                                   GtkCellRendererState  flags);
static GtkCellEditable *xfce_settings_cell_renderer_start_editing (GtkCellRenderer      *cell,
                                                                   GdkEvent             *event,
                                                                   GtkWidget            *widget,
                                                                   const gchar          *path,
                                                                   const GdkRectangle   *background_area,
                                                                   const GdkRectangle   *cell_area,
                                                                   GtkCellRendererState  flags);
static void             xfce_settings_strv_to_string              (const GValue         *src_value,
                                                                   GValue               *dest_value);


G_DEFINE_TYPE (XfceSettingsCellRenderer, xfce_settings_cell_renderer, GTK_TYPE_CELL_RENDERER)



static void
xfce_settings_cell_renderer_class_init (XfceSettingsCellRendererClass *klass)
{
    GObjectClass         *gobject_class;
    GtkCellRendererClass *gtkcellrenderer_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = xfce_settings_cell_renderer_set_property;
    gobject_class->get_property = xfce_settings_cell_renderer_get_property;
    gobject_class->finalize = xfce_settings_cell_renderer_finalize;

    gtkcellrenderer_class = GTK_CELL_RENDERER_CLASS (klass);
    gtkcellrenderer_class->render = xfce_settings_cell_renderer_render;
    gtkcellrenderer_class->activate = xfce_settings_cell_renderer_activate;
    gtkcellrenderer_class->start_editing = xfce_settings_cell_renderer_start_editing;

    g_object_class_install_property (gobject_class,
                                     PROP_VALUE,
                                     g_param_spec_boxed ("value",
                                                         NULL, NULL,
                                                         G_TYPE_VALUE,
                                                         G_PARAM_READWRITE
                                                         | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_LOCKED,
                                     g_param_spec_boolean ("locked",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READWRITE
                                                           | G_PARAM_STATIC_STRINGS));

    renderer_signals[VALUE_CHANGED] = g_signal_new (g_intern_static_string ("value-changed"),
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0, NULL, NULL,
                                                    _xfce_settings_marshal_VOID__STRING_BOXED,
                                                    G_TYPE_NONE, 2,
                                                    G_TYPE_STRING,
                                                    G_TYPE_VALUE);

    edit_data_quark = g_quark_from_static_string ("path");

    g_value_register_transform_func (G_TYPE_STRV, G_TYPE_STRING,
                                     xfce_settings_strv_to_string);
}



static void
xfce_settings_cell_renderer_init (XfceSettingsCellRenderer *renderer)
{
    renderer->renderer_text = gtk_cell_renderer_text_new ();
    g_object_ref_sink (G_OBJECT (renderer->renderer_text));
    g_object_set (renderer->renderer_text, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    renderer->renderer_toggle = gtk_cell_renderer_toggle_new ();
    g_object_ref_sink (G_OBJECT (renderer->renderer_toggle));
}



static void
xfce_settings_cell_renderer_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
    XfceSettingsCellRenderer *renderer = XFCE_SETTINGS_CELL_RENDERER (object);
    GValue                   *real_value;
    GtkCellRendererMode       cell_mode;
    GType                     cell_type;

    switch (prop_id)
    {
        case PROP_VALUE:
            if (G_IS_VALUE (&renderer->cell_value))
                g_value_unset (&renderer->cell_value);

            real_value = g_value_get_boxed (value);
            cell_mode = GTK_CELL_RENDERER_MODE_INERT;
            if (G_IS_VALUE (real_value))
            {
                cell_type = G_VALUE_TYPE (real_value);
                g_value_init (&renderer->cell_value, cell_type);
                g_value_copy (real_value, &renderer->cell_value);

                switch (cell_type)
                {
                    case G_TYPE_STRING:
                    case G_TYPE_INT:
                    case G_TYPE_UINT:
                    case G_TYPE_INT64:
                    case G_TYPE_UINT64:
                    case G_TYPE_DOUBLE:
                        cell_mode = GTK_CELL_RENDERER_MODE_EDITABLE;
                        break;

                    case G_TYPE_BOOLEAN:
                        cell_mode = GTK_CELL_RENDERER_MODE_ACTIVATABLE;
                        break;
                }
            }

            g_object_set (object, "mode", cell_mode, NULL);
            break;

        case PROP_LOCKED:
            renderer->locked = g_value_get_boolean (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
xfce_settings_cell_renderer_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
    XfceSettingsCellRenderer *renderer = XFCE_SETTINGS_CELL_RENDERER (object);

    switch (prop_id)
    {
        case PROP_VALUE:
            if (G_IS_VALUE (&renderer->cell_value))
                g_value_set_boxed (value, &renderer->cell_value);
            else
                g_value_set_boxed (value, NULL);
            break;

        case PROP_LOCKED:
            g_value_set_boolean (value, renderer->locked);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
xfce_settings_cell_renderer_finalize (GObject *object)
{
    XfceSettingsCellRenderer *renderer = XFCE_SETTINGS_CELL_RENDERER (object);

    if (G_IS_VALUE (&renderer->cell_value))
        g_value_unset (&renderer->cell_value);

    g_object_unref (G_OBJECT (renderer->renderer_text));
    g_object_unref (G_OBJECT (renderer->renderer_toggle));

    G_OBJECT_CLASS (xfce_settings_cell_renderer_parent_class)->finalize (object);
}



static GtkCellRenderer *
xfce_settings_cell_renderer_prepare (XfceSettingsCellRenderer *renderer)
{
    const GValue *value = &renderer->cell_value;
    GValue        str_value = { 0, };

    if (G_VALUE_TYPE (value) == xfce_settings_array_type ()
        || G_VALUE_TYPE (value) == G_TYPE_STRV)
        goto transform_value;

    switch (G_VALUE_TYPE (value))
    {
        case G_TYPE_NONE:
        case G_TYPE_INVALID:
            g_object_set (G_OBJECT (renderer->renderer_text),
                          "text", NULL, NULL);
            break;

        case G_TYPE_STRING:
            g_object_set (G_OBJECT (renderer->renderer_text),
                          "text", g_value_get_string (value), NULL);
            break;

        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
        case G_TYPE_DOUBLE:
            transform_value:

            g_value_init (&str_value, G_TYPE_STRING);
            if (g_value_transform (value, &str_value))
            {
                g_object_set (G_OBJECT (renderer->renderer_text),
                              "text", g_value_get_string (&str_value), NULL);
            }
            g_value_unset (&str_value);
            break;

        case G_TYPE_BOOLEAN:
            g_object_set (G_OBJECT (renderer->renderer_toggle),
                          "xalign", 0.0,
                          "active", g_value_get_boolean (value), NULL);

            return renderer->renderer_toggle;

        default:
            g_object_set (G_OBJECT (renderer->renderer_text),
                          "text", "<unknown>", NULL);
            break;
    }

    return renderer->renderer_text;
}



static void
xfce_settings_cell_renderer_render (GtkCellRenderer      *cell,
                                    cairo_t              *cr,
                                    GtkWidget            *widget,
                                    const GdkRectangle   *background_area,
                                    const GdkRectangle   *cell_area,
                                    GtkCellRendererState  flags)
{
    XfceSettingsCellRenderer *renderer = XFCE_SETTINGS_CELL_RENDERER (cell);
    GtkCellRenderer          *cell_renderer;

    cell_renderer = xfce_settings_cell_renderer_prepare (renderer);
    gtk_cell_renderer_render (cell_renderer,
                              cr, widget,
                              background_area, cell_area,
                              flags);
}



static gint
xfce_settings_cell_renderer_activate (GtkCellRenderer      *cell,
                                      GdkEvent             *event,
                                      GtkWidget            *widget,
                                      const gchar          *path,
                                      const GdkRectangle   *background_area,
                                      const GdkRectangle   *cell_area,
                                      GtkCellRendererState  flags)
{
    XfceSettingsCellRenderer *renderer = XFCE_SETTINGS_CELL_RENDERER (cell);
    GValue                    new_value = { 0, };

    if (renderer->locked)
        return FALSE;

    switch (G_VALUE_TYPE (&renderer->cell_value))
    {
        case G_TYPE_NONE:
        case G_TYPE_INVALID:
            return FALSE;

        case G_TYPE_BOOLEAN:
            g_value_init (&new_value, G_TYPE_BOOLEAN);
            g_value_set_boolean (&new_value, !g_value_get_boolean (&renderer->cell_value));

            g_signal_emit (G_OBJECT (renderer),
                           renderer_signals[VALUE_CHANGED], 0,
                           path, &new_value);

            g_value_unset (&new_value);
            return TRUE;

        default:
            g_assert_not_reached ();
            break;
    }

    return FALSE;
}



typedef struct
{
    gchar *path;
    GType  dest_type;
}
EditData;



static void
xfce_settings_cell_renderer_done_editing (GtkCellEditable          *entry,
                                          XfceSettingsCellRenderer *renderer)
{
    EditData    *data;
    const gchar *text;
    GValue       value = { 0, };
    gdouble      dval;

    data = g_object_get_qdata (G_OBJECT (entry), edit_data_quark);
    g_value_init (&value, data->dest_type);

    text = gtk_entry_get_text (GTK_ENTRY (entry));
    if (G_UNLIKELY (text == NULL))
      text = "";

    switch (data->dest_type)
    {
        case G_TYPE_INT:
            dval = g_ascii_strtod (text, NULL);
            g_value_set_int (&value, CLAMP (dval, G_MININT, G_MAXINT));
            break;

        case G_TYPE_UINT:
            dval = g_ascii_strtod (text, NULL);
            g_value_set_uint (&value, CLAMP (dval, 0, G_MAXUINT));
            break;

        case G_TYPE_INT64:
            g_value_set_int64 (&value, g_ascii_strtoll (text, NULL, 0));
            break;

        case G_TYPE_UINT64:
            g_value_set_uint64 (&value, g_ascii_strtoull (text, NULL, 0));
            break;

        case G_TYPE_DOUBLE:
            g_value_set_double (&value, g_ascii_strtod (text, NULL));
            break;

        case G_TYPE_STRING:
            g_value_set_static_string (&value, text);
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    g_signal_emit (G_OBJECT (renderer),
                   renderer_signals[VALUE_CHANGED], 0,
                   data->path, &value);

    g_value_unset (&value);
}



static void
xfce_settings_cell_renderer_edit_free (gpointer user_data)
{
    EditData *data = user_data;
    g_free (data->path);
    g_slice_free (EditData, data);
}



static GtkCellEditable *
xfce_settings_cell_renderer_start_editing (GtkCellRenderer      *cell,
                                           GdkEvent             *event,
                                           GtkWidget            *widget,
                                           const gchar          *path,
                                           const GdkRectangle   *background_area,
                                           const GdkRectangle   *cell_area,
                                           GtkCellRendererState  flags)
{
    XfceSettingsCellRenderer *renderer = XFCE_SETTINGS_CELL_RENDERER (cell);
    GtkWidget                *entry;
    GValue                    str_value = { 0, };
    const gchar              *text;
    EditData                 *data;

    if (renderer->locked)
        return NULL;

    switch (G_VALUE_TYPE (&renderer->cell_value))
    {
        case G_TYPE_NONE:
        case G_TYPE_INVALID:
            return NULL;

        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
        case G_TYPE_DOUBLE:
        case G_TYPE_STRING:
            g_value_init (&str_value, G_TYPE_STRING);
            if (g_value_transform (&renderer->cell_value, &str_value))
            {
                entry = gtk_entry_new ();
                text = g_value_get_string (&str_value);
                gtk_entry_set_text (GTK_ENTRY (entry), text == NULL ? "" : text);
                gtk_entry_set_has_frame (GTK_ENTRY (entry), FALSE);
                gtk_widget_show (entry);

                data = g_slice_new0 (EditData);
                data->path = g_strdup (path);
                data->dest_type = G_VALUE_TYPE (&renderer->cell_value);

                g_object_set_qdata_full (G_OBJECT (entry), edit_data_quark, data,
                    xfce_settings_cell_renderer_edit_free);

                g_signal_connect (G_OBJECT (entry), "editing-done",
                    G_CALLBACK (xfce_settings_cell_renderer_done_editing), cell);

                g_value_unset (&str_value);

                return GTK_CELL_EDITABLE (entry);
            }
            else
            {
                g_warning ("Unable to transform value from %s to %s",
                           G_VALUE_TYPE_NAME (&renderer->cell_value),
                           G_VALUE_TYPE_NAME (&str_value));
            }
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    return NULL;
}



static void
xfce_settings_array_to_string (const GValue *src_value,
                               GValue       *dest_value)
{
    GString      *str;
    GPtrArray    *array = g_value_get_boxed (src_value);
    guint         i;
    const GValue *val;
    GValue        str_val = { 0, };

    g_return_if_fail (G_VALUE_HOLDS_STRING (dest_value));
    g_return_if_fail (array != NULL);

    str = g_string_new ("[ ");

    for (i = 0; i < array->len; i++)
    {
        val = g_ptr_array_index (array, i);

        if (val == NULL)
        {
            g_string_append (str, "Null");
        }
        else if (G_VALUE_HOLDS_STRING (val))
        {
            g_string_append_printf (str, "\"%s\"",
                g_value_get_string (val));
        }
        else
        {
            g_value_init (&str_val, G_TYPE_STRING);
            if (g_value_transform (val, &str_val))
                g_string_append (str, g_value_get_string (&str_val));
            else
                g_string_append (str, "?");
            g_value_unset (&str_val);
        }

        if (i < array->len - 1)
            g_string_append (str, ", ");
    }

    g_string_append (str, " ]");

    g_value_take_string (dest_value, g_string_free (str, FALSE));
}



static void
xfce_settings_strv_to_string (const GValue *src_value,
                              GValue       *dest_value)
{
    gchar   **array = g_value_get_boxed (src_value);
    GString  *str;
    guint     i;

    g_return_if_fail (G_VALUE_HOLDS_STRING (dest_value));
    g_return_if_fail (array != NULL);

    str = g_string_new ("[ ");

    for (i = 0; array[i] != NULL; i++)
    {
        if (i > 0)
          g_string_append (str, ", ");
        g_string_append_printf (str, "\"%s\"", array[i]);
    }

    g_string_append (str, " ]");

    g_value_take_string (dest_value, g_string_free (str, FALSE));
}




GtkCellRenderer *
xfce_settings_cell_renderer_new (void)
{
    return g_object_new (XFCE_TYPE_SETTINGS_CELL_RENDERER, NULL);
}



GType
xfce_settings_array_type (void)
{
    static GType type = 0;

    if (type == 0)
    {
        type = g_type_from_name ("GPtrArray");
        g_value_register_transform_func (type, G_TYPE_STRING,
                                         xfce_settings_array_to_string);
    }

    return type;
}
