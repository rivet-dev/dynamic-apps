/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010 Lionel Le Folgoc <lionel@lefolgoc.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <math.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <xfconf/xfconf.h>
#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <common/xfce-randr.h>
#include "common/display-profiles.h"
#include "display-dialog_ui.h"
#include "confirmation-dialog_ui.h"
#include "minimal-display-dialog_ui.h"
#include "profile-changed-dialog_ui.h"
#include "identity-popup_ui.h"

#include "scrollarea.h"

#define MARGIN  16
#define NOTIFY_PROP_DEFAULT 1

enum
{
    COLUMN_OUTPUT_NAME,
    COLUMN_OUTPUT_ID,
    N_OUTPUT_COLUMNS
};

enum
{
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_HASH,
    N_COLUMNS
};

enum
{
    RESOLUTION_COLUMN_COMBO_NAME,
    RESOLUTION_COLUMN_COMBO_MARKUP,
    RESOLUTION_COLUMN_COMBO_VALUE,
    N_RESOLUTION_COMBO_COLUMNS
};

enum
{
    COLUMN_COMBO_NAME,
    COLUMN_COMBO_VALUE,
    N_COMBO_COLUMNS
};



typedef struct _XfceRotation XfceRotation;



struct _XfceRotation
{
    Rotation     rotation;
    const gchar *name;
};



/* Xrandr rotation name conversion */
static const XfceRotation rotation_names[] =
{
    { RR_Rotate_0,   N_("None") },
    { RR_Rotate_90,  N_("Left") },
    { RR_Rotate_180, N_("Inverted") },
    { RR_Rotate_270, N_("Right") }
};



/* Xrandr reflection name conversion */
static const XfceRotation reflection_names[] =
{
    { 0,                         N_("None") },
    { RR_Reflect_X,              N_("Horizontal") },
    { RR_Reflect_Y,              N_("Vertical") },
    { RR_Reflect_X|RR_Reflect_Y, N_("Horizontal and Vertical") }
};



/* Confirmation dialog data */
typedef struct
{
    GtkBuilder *builder;
    gint count;
    guint source_id;
} ConfirmationDialog;


/* Option entries */
static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static gboolean minimal = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "minimal", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &minimal, N_("Minimal interface to set up an external output"), NULL},
    { NULL }
};

/* Global xfconf channel */
static XfconfChannel *display_channel;

/* output currently selected in the combobox */
static guint active_output;

/* Pointer to the used randr structure */
static XfceRandr *xfce_randr = NULL;

/* event base for XRandR notifications */
static gint randr_event_base;

/* Used to identify the display */
static GHashTable *display_popups = NULL;
gboolean show_popups = FALSE;

gboolean supports_alpha = FALSE;

/* Keep track of the initially active profile */
gchar *active_profile = NULL;

/* Graphical randr */
GtkWidget *randr_gui_area = NULL;
GList *current_outputs = NULL;

/* Outputs Combobox TODO Use App() to store constant widgets once the cruft is cleaned */
GtkWidget *randr_outputs_combobox = NULL;
GtkWidget *apply_button = NULL;

/* Show nice representation of the display ratio */
typedef struct _XfceRatio XfceRatio;

struct _XfceRatio
{
    gboolean precise;
    gdouble ratio;
    const gchar *desc;
};

static GHashTable *display_ratio = NULL;
/* adding the +0.5 to result in "rounding" instead of trunc() */
#define _ONE_DIGIT_PRECISION(x) ((gdouble)((gint)((x)*10.0+0.5))/10.0)
#define _TWO_DIGIT_PRECISION(x) ((gdouble)((gint)((x)*100.0+0.5))/100.0)

/* most prominent ratios */
/* adding in least exact order to find most precise */
static XfceRatio ratio_table[] = {
    { FALSE, _ONE_DIGIT_PRECISION(16.0/9.0), "<span font_style='italic'>≈16:9</span>" },
    { FALSE, _TWO_DIGIT_PRECISION(16.0/9.0), "<span font_style='italic'>≈16:9</span>" },
    { TRUE, 16.0/9.0, "16:9" },
    { FALSE, _ONE_DIGIT_PRECISION(16.0/10.0), "<span font_style='italic'>≈16:10</span>" },
    { FALSE, _TWO_DIGIT_PRECISION(16.0/10.0), "<span font_style='italic'>≈16:10</span>" },
    { TRUE, 16.0/10.0, "16:10" },
    /* _ONE_DIGIT_PRECISION(4.0/3.0) would be mixed up with 5/4 */
    { FALSE, _TWO_DIGIT_PRECISION(4.0/3.0), "<span font_style='italic'>≈4:3</span>" },
    { TRUE, 4.0/3.0, "4:3" },
    { FALSE, _ONE_DIGIT_PRECISION(21.0/9.0), "<span font_style='italic'>≈21:9</span>" },
    { FALSE, _TWO_DIGIT_PRECISION(21.0/9.0), "<span font_style='italic'>≈21:9</span>" },
    { TRUE, 21.0/9.0, "21:9" },
    { FALSE, 0.0 , NULL }
};

static void display_settings_minimal_only_display1_toggled   (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_mirror_displays_toggled (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_extend_right_toggled    (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_only_display2_toggled   (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static gboolean display_setting_primary_toggled              (GtkWidget       *widget,
                                                              gboolean         primary,
                                                              GtkBuilder      *builder);

static void display_setting_mirror_displays_populate         (GtkBuilder *builder);

static void display_settings_profile_apply                   (GtkWidget       *widget,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_profile_apply           (GtkToggleButton *widget,
                                                              GtkBuilder      *builder);

static GList *list_connected_outputs                         (gint            *total_w,
                                                              gint            *total_h);

static void
display_settings_changed (void)
{
    gtk_widget_set_sensitive (GTK_WIDGET (apply_button), TRUE);
}

static XfceOutputInfo*
get_nth_xfce_output_info (gint id)
{
    XfceOutputInfo *output = NULL;
    GList * entry = NULL;

    if (!current_outputs)
        current_outputs = list_connected_outputs (NULL, NULL);

    entry = g_list_nth (current_outputs, id);

    if (entry)
        output = entry->data;

    return output;
}

static void
initialize_connected_outputs (void)
{
    if (current_outputs)
    {
        g_list_free(current_outputs);
        current_outputs = NULL;
    }
}

static guint
display_settings_get_n_active_outputs (void)
{
    guint n, count = 0;

    g_assert (xfce_randr != NULL);

    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] != None)
            ++count;
    }
    return count;
}

static gboolean
display_setting_combo_box_get_value (GtkComboBox *combobox,
                                     gint        *value,
                                     gboolean     resolution)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        model = gtk_combo_box_get_model (combobox);
        if (resolution)
            gtk_tree_model_get (model, &iter, RESOLUTION_COLUMN_COMBO_VALUE, value, -1);
        else
            gtk_tree_model_get (model, &iter, COLUMN_COMBO_VALUE, value, -1);

        return TRUE;
    }

    return FALSE;
}

static gboolean
display_settings_update_time_label (gpointer user_data)
{
    ConfirmationDialog *confirmation_dialog = user_data;
    GObject *dialog;

    dialog = gtk_builder_get_object (confirmation_dialog->builder, "dialog1");
    confirmation_dialog->count--;

    if (confirmation_dialog->count <= 0)
    {
        gtk_dialog_response (GTK_DIALOG (dialog), 1);
        confirmation_dialog->source_id = 0;

        return FALSE;
    }
    else
    {
        gchar   *label_string;

        label_string = g_strdup_printf (_("The previous configuration will be restored in <b>%i"
                                          " seconds</b> if you do not reply to this question."),
                                        confirmation_dialog->count);

        gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), "%s", label_string);
        g_free (label_string);

        return TRUE;
    }

    return TRUE;
}

/* Returns true if the configuration is to be kept or false if it is to
 * be reverted */
static gboolean
display_setting_timed_confirmation (GtkBuilder *main_builder)
{
    GtkBuilder *builder;
    GObject    *main_dialog;
    GError     *error = NULL;
    gint        response_id;

    /* Lock the main UI */
    main_dialog = gtk_builder_get_object (main_builder, "display-dialog");

    builder = gtk_builder_new ();

    if (gtk_builder_add_from_string (builder, confirmation_dialog_ui,
                                     confirmation_dialog_ui_length, &error) != 0)
    {
        GObject *dialog;
        ConfirmationDialog *confirmation_dialog;

        confirmation_dialog = g_new0 (ConfirmationDialog, 1);
        confirmation_dialog->builder = builder;
        confirmation_dialog->count = 10;

        dialog = gtk_builder_get_object (builder, "dialog1");

        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_dialog));
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        confirmation_dialog->source_id =
            g_timeout_add_seconds (1, display_settings_update_time_label, confirmation_dialog);

        response_id = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (GTK_WIDGET (dialog));
        if (confirmation_dialog->source_id != 0)
            g_source_remove (confirmation_dialog->source_id);
        g_free (confirmation_dialog);
    }
    else
    {
        response_id = 2;
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    g_object_unref (G_OBJECT (builder));

    return ((response_id == 2) ? TRUE : FALSE);
}

/*
 * Encapsulates display_setting_timed_confirmation, automatically uses Fallback on FALSE
 * Returns TRUE if the configuration was kept, FALSE if the configuration was replaced with the Fallback
 */
static gboolean
display_setting_ask_fallback (GtkBuilder *builder)
{
    guint i = 0;

    /* Ask user confirmation (or recover to'Fallback on timeout') */
    if (display_setting_timed_confirmation (builder))
    {
        /* Update the Fallback */
        for (i = 0; i < xfce_randr->noutput; i++)
            xfce_randr_save_output (xfce_randr, "Fallback", display_channel, i);
        return TRUE;
    }
    else
    {
        /* Recover to Fallback (will as well overwrite default xfconf settings) */
        xfce_randr_apply (xfce_randr, "Fallback", display_channel);
        foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
        return FALSE;
    }
}

static void
display_setting_custom_scale_changed (GtkSpinButton *spinbutton,
                                      gpointer       user_data)
{
    gdouble scale;

    scale = gtk_spin_button_get_value (spinbutton);
    xfce_randr->scalex[active_output] = scale;
    xfce_randr->scaley[active_output] = scale;

    display_settings_changed ();
}

static void
display_setting_scale_changed (GtkComboBox *combobox,
                               GtkBuilder  *builder)
{
    GObject      *revealer, *spin_scalex, *spin_scaley;
    GValue        prop = { 0, };
    gdouble       scale;
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        return;

    revealer = gtk_builder_get_object (builder, "revealer-scale");
    spin_scalex = gtk_builder_get_object (builder, "spin-scale-x");
    spin_scaley = gtk_builder_get_object (builder, "spin-scale-y");

    gtk_combo_box_get_active_iter (combobox, &iter);
    model = gtk_combo_box_get_model (combobox);
    gtk_tree_model_get_value (model, &iter, COLUMN_COMBO_VALUE, &prop);
    scale = g_value_get_double (&prop);

    /* Show the spinbuttons if the combobox is set to "Custom:" */
    if (scale == -1.0)
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
    }
    else
    {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scalex), scale);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scaley), scale);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
    }

    g_value_unset (&prop);
    display_settings_changed ();
}

static gboolean
display_setting_scale_set_active (GtkTreeModel *model,
                                  GtkTreePath  *path,
                                  GtkTreeIter  *iter,
                                  gpointer      data)
{
    GValue    prop = { 0, };
    GObject  *combobox = data;
    gboolean  found = FALSE;

    gtk_tree_model_get_value (model, iter, COLUMN_COMBO_VALUE, &prop);

    if (g_value_get_double (&prop) == xfce_randr->scalex[active_output])
    {
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), iter);
        found = TRUE;
    }
    else
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), -1);

    g_value_unset (&prop);

    return found;
}

static void
display_setting_scale_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox, *label, *revealer, *spin_scalex, *spin_scaley;
    guint         n;

    if (!xfce_randr)
        return;

    combobox = gtk_builder_get_object (builder, "randr-scale");
    label = gtk_builder_get_object (builder, "label-scale");
    revealer = gtk_builder_get_object (builder, "revealer-scale");

    /* disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), -1);
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Sync the current scale value to the spinbuttons */
    spin_scalex = gtk_builder_get_object (builder, "spin-scale-x");
    spin_scaley = gtk_builder_get_object (builder, "spin-scale-y");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scalex), xfce_randr->scalex[active_output]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scaley), xfce_randr->scaley[active_output]);

    /* Block the "changed" signal while determining the active item */
    g_signal_handlers_block_by_func (combobox, display_setting_scale_changed,
                                     builder);

    /* If the current scale is part of the presets set it as active */
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_tree_model_foreach (model, display_setting_scale_set_active, combobox);

    /* If the current scale is not found in the presets we select "Custom:", which
       is the last element of the liststore */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
    {
        GtkTreePath *path;
        GtkTreeIter  iter;

        n = gtk_tree_model_iter_n_children (model, NULL);
        path = gtk_tree_path_new_from_indices (n - 1, -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
        gtk_tree_path_free (path);
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
    }
    else
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_scale_changed,
                                       builder);
}

static void
display_setting_reflections_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value, FALSE))
        return;

    /* Remove existing reflection */
    xfce_randr->rotation[active_output] &= ~XFCE_RANDR_REFLECTIONS_MASK;

    /* Set the new one */
    xfce_randr->rotation[active_output] |= value;

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
}

static void
display_setting_reflections_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox, *label;
    Rotation      reflections;
    Rotation      active_reflection;
    guint         n;
    GtkTreeIter   iter;

    if (!xfce_randr)
        return;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-reflection");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
    label = gtk_builder_get_object (builder, "label-reflection");

    /* disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_reflections_changed,
                                     builder);

    /* Load only supported reflections */
    reflections = xfce_randr->rotations[active_output] & XFCE_RANDR_REFLECTIONS_MASK;
    active_reflection = xfce_randr->rotation[active_output] & XFCE_RANDR_REFLECTIONS_MASK;

    /* Try to insert the reflections */
    for (n = 0; n < G_N_ELEMENTS (reflection_names); n++)
    {
        if ((reflections & reflection_names[n].rotation) == reflection_names[n].rotation)
        {
            /* Insert */
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, _(reflection_names[n].name),
                                COLUMN_COMBO_VALUE, reflection_names[n].rotation, -1);

            /* Select active reflection */
            if (xfce_randr && xfce_randr->mode[active_output] != None)
            {
                if ((reflection_names[n].rotation & active_reflection) == reflection_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_reflections_changed,
                                       builder);
}

static void
display_setting_rotations_changed (GtkComboBox *combobox,
                                   GtkBuilder  *builder)
{
    XfceOutputInfo *output;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value, FALSE))
        return;

    /* Set new rotation */
    xfce_randr->rotation[active_output] &= ~XFCE_RANDR_ROTATIONS_MASK;
    xfce_randr->rotation[active_output] |= value;
    output = get_nth_xfce_output_info(active_output);
    output->rotation = xfce_randr->rotation[active_output];

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
}

static void
display_setting_rotations_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox, *label;
    Rotation      rotations;
    Rotation      active_rotation;
    guint         n;
    GtkTreeIter   iter;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-rotation");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
    label = gtk_builder_get_object (builder, "label-rotation");

    /* Disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_rotations_changed,
                                     builder);

    /* Load only supported rotations */
    rotations = xfce_randr->rotations[active_output] & XFCE_RANDR_ROTATIONS_MASK;
    active_rotation = xfce_randr->rotation[active_output] & XFCE_RANDR_ROTATIONS_MASK;

    /* Try to insert the rotations */
    for (n = 0; n < G_N_ELEMENTS (rotation_names); n++)
    {
        if ((rotations & rotation_names[n].rotation) == rotation_names[n].rotation)
        {
            /* Insert */
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, _(rotation_names[n].name),
                                COLUMN_COMBO_VALUE, rotation_names[n].rotation, -1);

            /* Select active rotation */
            if (xfce_randr && (xfce_randr->mode[active_output] != None))
            {
                if ((rotation_names[n].rotation & active_rotation) == rotation_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_rotations_changed,
                                       builder);
}

static void
display_setting_refresh_rates_changed (GtkComboBox *combobox,
                                       GtkBuilder  *builder)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value, FALSE))
        return;

    /* Set new mode */
    xfce_randr->mode[active_output] = value;

    /* In any case, check if we're now in mirror mode */
    display_setting_mirror_displays_populate (builder);

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
}

static void
display_setting_refresh_rates_populate (GtkBuilder *builder)
{
    GtkTreeModel     *model;
    GObject          *combobox, *label;
    GtkTreeIter       iter;
    gchar            *name = NULL;
    gint              nmode, n;
    GObject          *res_combobox;
    const XfceRRMode *modes, *current_mode;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
    label = gtk_builder_get_object (builder, "label-refresh-rate");

    /* Disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_refresh_rates_changed,
                                     builder);

    /* Fetch the selected resolution */
    res_combobox = gtk_builder_get_object (builder, "randr-resolution");
    if (!display_setting_combo_box_get_value (GTK_COMBO_BOX (res_combobox), &n, TRUE))
        return;

    current_mode = xfce_randr_find_mode_by_id (xfce_randr, active_output, n);
    if (!current_mode)
        return;

    /* Walk all supported modes */
    modes = xfce_randr_get_modes (xfce_randr, active_output, &nmode);
    for (n = 0; n < nmode; ++n)
    {
        /* The mode resolution does not match the selected one */
        if (modes[n].width != current_mode->width
            || modes[n].height != current_mode->height)
            continue;

        /* Insert the mode */
        name = g_strdup_printf (_("%.2f Hz"), modes[n].rate);
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COLUMN_COMBO_NAME, name,
                            COLUMN_COMBO_VALUE, modes[n].id, -1);
        g_free (name);

        /* Select the active mode */
        if (modes[n].id == xfce_randr->mode[active_output])
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* If a new resolution was selected, set a refresh rate */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

    /* In any case, check if we're now in mirror mode */
    display_setting_mirror_displays_populate (builder);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_refresh_rates_changed,
                                       builder);
}

static void
display_setting_resolutions_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    XfceOutputInfo *output;
    const XfceRRMode *mode;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value, TRUE))
        return;

    /* Set new resolution */
    xfce_randr->mode[active_output] = value;

    /* Apply resolution to gui */
    output = get_nth_xfce_output_info (active_output);
    mode = xfce_randr_find_mode_by_id (xfce_randr, active_output, value);
    output->width = xfce_randr_mode_width(mode, 0);
    output->height = xfce_randr_mode_height(mode, 0);

    /* Update refresh rates */
    display_setting_refresh_rates_populate (builder);

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
}

/* Greatest common divisor */
static guint
gcd (guint a,
     guint b)
{
    if (b == 0)
      return a;

    return gcd (b, a % b);
}

/* Initialize valid display aspect ratios */
static void
display_settings_aspect_ratios_populate (void)
{
    XfceRatio *i;

    display_ratio = g_hash_table_new (g_double_hash, g_double_equal);
    for (i = ratio_table; i->ratio != 0.0; i++)
    {
        g_hash_table_insert (display_ratio, &i->ratio, (gpointer) i);
    }
}

static void
display_setting_resolutions_populate (GtkBuilder *builder)
{
    GtkTreeModel     *model;
    GObject          *combobox, *label;
    gint              nmode, n;
    gchar            *name;
    gchar            *rratio;
    GtkTreeIter       iter;
    const XfceRRMode *modes;
    XfceOutputInfo   *output;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    label = gtk_builder_get_object (builder, "label-resolution");

    output = get_nth_xfce_output_info (active_output);

    /* Disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        display_setting_refresh_rates_populate (builder);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_resolutions_changed,
                                     builder);

    /* Walk all supported modes */
    modes = xfce_randr_get_modes (xfce_randr, active_output, &nmode);
    for (n = 0; n < nmode; ++n)
    {
        /* Try to avoid duplicates */
        if (n == 0 || (n > 0 && (modes[n].width != modes[n - 1].width
            || modes[n].height != modes[n - 1].height)))
        {
            /* Insert mode and ratio */
            gdouble    ratio = (double) modes[n].width / (double) modes[n].height;
            gdouble    rough_ratio;
            gchar     *ratio_text = NULL;
            XfceRatio *ratio_info = g_hash_table_lookup (display_ratio, &ratio);

            /* Highlight the preferred mode with an asterisk */
            if (output->pref_width == modes[n].width
                && output->pref_height == modes[n].height)
                name = g_strdup_printf ("%dx%d*", modes[n].width,
                                        modes[n].height);
            else
                name = g_strdup_printf ("%dx%d", modes[n].width,
                                        modes[n].height);

            if (ratio_info)
                ratio_text = g_strdup (ratio_info->desc);

            if (!ratio_info)
            {
                rough_ratio = _TWO_DIGIT_PRECISION (ratio);
                ratio_info = g_hash_table_lookup (display_ratio, &rough_ratio);
                if (ratio_info)
                {
                    /* if the lookup finds a precise ratio
                     * although we did round the current ratio
                     * we also mark this as not precise */
                    if (ratio_info->precise)
                        ratio_text = g_strdup_printf ("<span font_style='italic'>≈%s</span>", ratio_info->desc);
                    else
                        ratio_text = g_strdup (ratio_info->desc);
                }
            }

            if (!ratio_info)
            {
                rough_ratio = _ONE_DIGIT_PRECISION (ratio);
                ratio_info = g_hash_table_lookup (display_ratio, &rough_ratio);
                if (ratio_info)
                {
                    if (ratio_info->precise)
                        ratio_text = g_strdup_printf ("<span font_style='italic'>≈%s</span>", ratio_info->desc);
                    else
                        ratio_text = g_strdup (ratio_info->desc);
                }
            }

            if (!ratio_info)
            {
                guint gcd_tmp = gcd (modes[n].width, modes[n].height);
                guint format_x = modes[n].width / gcd_tmp;
                guint format_y = modes[n].height / gcd_tmp;
                rratio = g_strdup_printf ("<span fgalpha='50%%'>%d:%d</span>", format_x, format_y);
            }
            else
            {
                rratio = g_strdup_printf ("<span fgalpha='50%%'>%s</span>", ratio_text);
            }
            g_free (ratio_text);

            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                RESOLUTION_COLUMN_COMBO_NAME, name,
                                RESOLUTION_COLUMN_COMBO_MARKUP, rratio,
                                RESOLUTION_COLUMN_COMBO_VALUE, modes[n].id, -1);
            g_free (name);
            g_free (rratio);
        }

        /* Select the active mode */
        if (modes[n].id == xfce_randr->mode[active_output])
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_resolutions_changed,
                                       builder);
}

static void
display_setting_screen_changed (GtkWidget *widget,
                                GdkScreen *old_screen,
                                gpointer   userdata)
{
    GdkScreen *screen = gtk_widget_get_screen (widget);
    GdkVisual *visual = gdk_screen_get_rgba_visual (screen);

    if (gdk_screen_is_composited (screen))
        supports_alpha = TRUE;
    else
    {
        visual = gdk_screen_get_system_visual (screen);
        supports_alpha = FALSE;
    }

    gtk_widget_set_visual (widget, visual);
}

static gboolean
display_setting_identity_popup_draw (GtkWidget      *popup,
                                     cairo_t *cr,
                                     GtkBuilder     *builder)
{
    cairo_pattern_t *vertical_gradient, *innerstroke_gradient, *selected_gradient, *selected_innerstroke_gradient;
    gint             radius;
    gboolean         selected = (g_hash_table_lookup (display_popups, GINT_TO_POINTER (active_output)) == popup);

    GtkAllocation *allocation = g_new0 (GtkAllocation, 1);
    gtk_widget_get_allocation (GTK_WIDGET (popup), allocation);

    radius = 10;
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    /* Create the various gradients */
    vertical_gradient = cairo_pattern_create_linear (0, 0, 0, allocation->height);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0, 0.25, 0.25, 0.25);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0.24, 0.15, 0.15, 0.15);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0.6, 0.0, 0.0, 0.0);

    innerstroke_gradient = cairo_pattern_create_linear (0, 0, 0, allocation->height);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0, 0.35, 0.35, 0.35);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.4, 0.25, 0.25, 0.25);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.7, 0.15, 0.15, 0.15);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.85, 0.0, 0.0, 0.0);

    selected_gradient = cairo_pattern_create_linear (0, 0, 0, allocation->height);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0, 0.05, 0.20, 0.46);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.4, 0.05, 0.12, 0.25);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.6, 0.05, 0.10, 0.20);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.8, 0.0, 0.02, 0.05);

    selected_innerstroke_gradient = cairo_pattern_create_linear (0, 0, 0, allocation->height);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0, 0.15, 0.45, 0.75);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0.7, 0.0, 0.15, 0.25);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0.85, 0.0, 0.0, 0.0);

    /* Compositing is not available, so just set the background color. */
    if (!supports_alpha)
    {
        /* Draw a filled rectangle with outline */
        cairo_set_line_width (cr, 1.0);
        cairo_set_source (cr, vertical_gradient);
        if (selected)
            cairo_set_source (cr, selected_gradient);
        cairo_paint (cr);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_rectangle (cr, 0.5, 0.5, allocation->width-0.5, allocation->height-0.5);
        cairo_stroke (cr);

        /* Draw the inner stroke */
        cairo_set_source_rgb (cr, 0.35, 0.35, 0.35);
        if (selected)
            cairo_set_source_rgb (cr, 0.15, 0.45, 0.75);
        cairo_move_to (cr, 1.5, 1.5);
        cairo_line_to (cr, allocation->width-1, 1.5);
        cairo_stroke (cr);
        cairo_set_source (cr, innerstroke_gradient);
        if (selected)
            cairo_set_source (cr, selected_innerstroke_gradient);
        cairo_move_to (cr, 1.5, 1.5);
        cairo_line_to (cr, 1.5, allocation->height-1.0);
        cairo_move_to (cr, allocation->width-1.5, 1.5);
        cairo_line_to (cr, allocation->width-1.5, allocation->height-1.0);
        cairo_stroke (cr);
    }
    /* Draw rounded corners. */
    else
    {
        cairo_set_source_rgba (cr, 0, 0, 0, 0);
        cairo_paint (cr);

        /* Draw a filled rounded rectangle with outline */
        cairo_set_line_width (cr, 1.0);
        cairo_move_to (cr, 0.5, allocation->height+0.5);
        cairo_line_to (cr, 0.5, radius+0.5);
        cairo_arc (cr, radius+0.5, radius+0.5, radius, 3.14, 3.0*3.14/2.0);
        cairo_line_to (cr, allocation->width-0.5 - radius, 0.5);
        cairo_arc (cr, allocation->width-0.5 - radius, radius+0.5, radius, 3.0*3.14/2.0, 0.0);
        cairo_line_to (cr, allocation->width-0.5, allocation->height+0.5);
        cairo_set_source (cr, vertical_gradient);
        if (selected)
            cairo_set_source (cr, selected_gradient);
        cairo_fill_preserve (cr);
        cairo_set_source_rgb (cr, 0.05, 0.05, 0.05);
        cairo_stroke (cr);

        /* Draw the inner stroke */
        cairo_set_source_rgb (cr, 0.35, 0.35, 0.35);
        if (selected)
            cairo_set_source_rgb (cr, 0.15, 0.45, 0.75);
        cairo_arc (cr, radius+1.5, radius+1.5, radius, 3.14, 3.0*3.14/2.0);
        cairo_line_to (cr, allocation->width-1.5 - radius, 1.5);
        cairo_arc (cr, allocation->width-1.5 - radius, radius+1.5, radius, 3.0*3.14/2.0, 0.0);
        cairo_stroke (cr);
        cairo_set_source (cr, innerstroke_gradient);
        if (selected)
            cairo_set_source (cr, selected_innerstroke_gradient);
        cairo_move_to (cr, 1.5, radius+1.0);
        cairo_line_to (cr, 1.5, allocation->height-1.0);
        cairo_move_to (cr, allocation->width-1.5, radius+1.0);
        cairo_line_to (cr, allocation->width-1.5, allocation->height-1.0);
        cairo_stroke (cr);

        cairo_close_path (cr);
    }

    cairo_pattern_destroy (vertical_gradient);
    cairo_pattern_destroy (innerstroke_gradient);
    cairo_pattern_destroy (selected_gradient);
    cairo_pattern_destroy (selected_innerstroke_gradient);

    g_free (allocation);

    return FALSE;
}

static GtkWidget *
display_setting_identity_display (gint display_id)
{
    GtkBuilder       *builder;
    GtkWidget        *popup = NULL;
    GObject          *display_number, *display_name, *display_details;
    const XfceRRMode *current_mode;
    gchar            *color_hex = "#FFFFFF", *number_label, *name_label, *details_label;
    gint              screen_pos_x, screen_pos_y;
    gint              window_width, window_height, screen_width, screen_height;

    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, identity_popup_ui,
                                     identity_popup_ui_length, NULL) != 0)
    {
        popup = GTK_WIDGET (gtk_builder_get_object (builder, "popup"));
        gtk_widget_set_name (popup, "XfceDisplayDialogPopup");

        gtk_widget_set_app_paintable (popup, TRUE);
        g_signal_connect (G_OBJECT (popup), "draw", G_CALLBACK (display_setting_identity_popup_draw), builder);
        g_signal_connect (G_OBJECT (popup), "screen-changed", G_CALLBACK (display_setting_screen_changed), NULL);

        display_number = gtk_builder_get_object (builder, "display_number");
        display_name = gtk_builder_get_object (builder, "display_name");
        display_details = gtk_builder_get_object (builder, "display_details");

        if (display_settings_get_n_active_outputs() > 1)
        {
            current_mode = xfce_randr_find_mode_by_id (xfce_randr, display_id,
                                                       xfce_randr->mode[display_id]);
            if (!xfce_randr_get_positions (xfce_randr, display_id,
                                           &screen_pos_x, &screen_pos_y))
            {
                screen_pos_x = 0;
                screen_pos_y = 0;
            }
            screen_width = xfce_randr_mode_width (current_mode, xfce_randr->rotation[display_id]);
            screen_height = xfce_randr_mode_height (current_mode, xfce_randr->rotation[display_id]);
        }
        else
        {
            screen_pos_x = 0;
            screen_pos_y = 0;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            screen_width = gdk_screen_width ();
            screen_height = gdk_screen_height ();
G_GNUC_END_IGNORE_DEPRECATIONS
        }

        if (xfce_randr->noutput > 1) {
            number_label = g_markup_printf_escaped ("<span foreground='%s' font='Bold 28'>%d</span>",
                                                    color_hex, display_id + 1);
            gtk_label_set_markup (GTK_LABEL (display_number), number_label);
            g_free (number_label);
        }
        else {
            gtk_label_set_text (GTK_LABEL (display_number), NULL);
            gtk_widget_set_margin_start (GTK_WIDGET (display_number), 0);
            gtk_widget_set_margin_end (GTK_WIDGET (display_number), 0);
        }

        name_label = g_markup_printf_escaped ("<span foreground='%s' font='Bold 10'>%s %s</span>",
                                              color_hex, _("Display:"), xfce_randr->friendly_name[display_id]);
        gtk_label_set_markup (GTK_LABEL (display_name), name_label);
        g_free (name_label);

        details_label = g_markup_printf_escaped ("<span foreground='%s' font='Light 10'>%s %i x %i</span>", color_hex,
                                                 _("Resolution:"), screen_width, screen_height);
        gtk_label_set_markup (GTK_LABEL (display_details), details_label);
        g_free (details_label);

        gtk_window_get_size (GTK_WINDOW (popup), &window_width, &window_height);

        gtk_window_move (GTK_WINDOW (popup),
                         screen_pos_x + (screen_width - window_width)/2,
                         screen_pos_y + screen_height - window_height);

        display_setting_screen_changed (GTK_WIDGET (popup), NULL, NULL);

        gtk_window_present (GTK_WINDOW (popup));
    }

    /* Release the builder */
    g_object_unref (G_OBJECT (builder));

    return popup;
}

static void
display_setting_identity_popups_populate (void)
{
    guint n;

    g_assert (xfce_randr);

    display_popups = g_hash_table_new_full (g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            (GDestroyNotify) gtk_widget_destroy);

    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] != None)
            g_hash_table_insert (display_popups,
                                 GINT_TO_POINTER (n),
                                 display_setting_identity_display (n));
    }
}

static void
display_setting_mirror_displays_toggled (GtkToggleButton *togglebutton,
                                         GtkBuilder      *builder)
{
    XfceOutputInfo *output;
    guint    n, pos = 0;
    RRMode  *clonable_modes;

    if (!xfce_randr)
        return;

    /* reset the inconsistent state, since the mirror checkbutton is being toggled */
    if (gtk_toggle_button_get_inconsistent (togglebutton))
        gtk_toggle_button_set_inconsistent (togglebutton, FALSE);

    if (gtk_toggle_button_get_active (togglebutton))
    {
        /* Activate mirror-mode with a single mode for all of them */
        clonable_modes = xfce_randr_clonable_modes (xfce_randr);
        /* Apply mirror settings to each output */
        for (n = 0; n < xfce_randr->noutput; n++)
        {
            if (xfce_randr->mode[n] == None)
                continue;

            if (clonable_modes != NULL)
                xfce_randr->mode[n] = clonable_modes[n];
            xfce_randr->rotation[n] = RR_Rotate_0;
            xfce_randr->mirrored[n] = TRUE;
            xfce_randr->position[n].x = 0;
            xfce_randr->position[n].y = 0;
        }
        g_free (clonable_modes);
    }
    else
    {
        /* Deactivate mirror-mode, use the preferred mode of each output */
        for (n = 0; n < xfce_randr->noutput; n++)
        {
            xfce_randr->mode[n] = xfce_randr_preferred_mode (xfce_randr, n);
            xfce_randr->mirrored[n] = FALSE;
            xfce_randr->position[n].x = pos;
            xfce_randr->position[n].y = 0;

            pos += xfce_randr_mode_width (xfce_randr_find_mode_by_id (xfce_randr, n, xfce_randr->mode[n]), 0);
        }
    }

    /* Apply resolution to gui */
    for (n = 0; n < xfce_randr->noutput; n++)
    {
        output = get_nth_xfce_output_info (n);
        if (output) {
            output->rotation = xfce_randr->rotation[n];
            output->x = xfce_randr->position[n].x;
            output->y = xfce_randr->position[n].y;
            output->mirrored = xfce_randr->mirrored[n];
            output->width = xfce_randr_mode_width (xfce_randr_find_mode_by_id (xfce_randr, n, xfce_randr->mode[n]), 0);
            output->height = xfce_randr_mode_height (xfce_randr_find_mode_by_id (xfce_randr, n, xfce_randr->mode[n]), 0);
        } /* else: some kind of racecondition during re-connect? - just ignore */
    }

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
}

static void
display_setting_mirror_displays_populate (GtkBuilder *builder)
{
    GObject *check;
    RRMode  *clonable_modes = NULL;
    guint    n;
    gboolean cloned = TRUE;
    gboolean mirrored = FALSE;

    if (!xfce_randr)
        return;

    check = gtk_builder_get_object (builder, "mirror-displays");

    if (xfce_randr->noutput > 1)
        gtk_widget_show (GTK_WIDGET (check));
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        return;
    }

    /* Can outputs be cloned? */
    if (display_settings_get_n_active_outputs () > 1)
        clonable_modes = xfce_randr_clonable_modes (xfce_randr);

    gtk_widget_set_sensitive (GTK_WIDGET (check), clonable_modes != NULL);
    if (clonable_modes == NULL)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
        return;
    }

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_mirror_displays_toggled,
                                     builder);

    /* Check if mirror settings are on */
    for (n = 0; n < xfce_randr->noutput; n++)
    {
        if (xfce_randr->mode[n] == None)
            continue;

        cloned &= (clonable_modes != NULL &&
                   xfce_randr->mode[n] == clonable_modes[n] &&
                   xfce_randr->mirrored[n]);
        mirrored = xfce_randr->mirrored[n];

        if (!cloned)
            break;
    }
    g_free (clonable_modes);

    /* if two displays are 'mirrored', i.e. their x and y positions are the same
       we set the checkbutton to the inconsistent state */
    if (mirrored == TRUE && cloned == FALSE)
    {
        gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (check), 1);
    }
    else
    {
        if (gtk_toggle_button_get_inconsistent (GTK_TOGGLE_BUTTON (check)))
            gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (check), 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cloned);
    }


    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (check, display_setting_mirror_displays_toggled,
                                       builder);
}

static gboolean
display_setting_primary_toggled (GtkWidget *widget,
                                 gboolean   primary,
                                 GtkBuilder *builder)
{
    guint m;

    if (!xfce_randr)
        return FALSE;

    if (primary)
    {
        /* Set currently active display as primary */
        xfce_randr->status[active_output]=XFCE_OUTPUT_STATUS_PRIMARY;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output);
        /* and all others as secondary */
        for (m = 0; m < xfce_randr->noutput; ++m)
        {
            if (m != active_output)
            {
                xfce_randr->status[m]=XFCE_OUTPUT_STATUS_SECONDARY;
                xfce_randr_save_output (xfce_randr, "Default", display_channel, m);
            }
        }
    }
    else
    {
        xfce_randr->status[active_output]=XFCE_OUTPUT_STATUS_SECONDARY;
        xfce_randr_save_output (xfce_randr, "Default", display_channel, active_output);
    }

    /* Apply the changes */
    xfce_randr_apply (xfce_randr, "Default", display_channel);
    gtk_switch_set_state (GTK_SWITCH (widget), primary);

    return TRUE;
}

static void
display_setting_primary_populate (GtkBuilder *builder)
{
    GObject *check, *label, *primary_indicator, *primary_info;
    gboolean output_on = TRUE;
    gboolean multiple_displays = TRUE;
    gboolean primary;

    if (!xfce_randr)
        return;
    primary = xfce_randr->status[active_output] != XFCE_OUTPUT_STATUS_SECONDARY;
    if (xfce_randr->noutput <= 1)
        multiple_displays = FALSE;
    check = gtk_builder_get_object (builder, "primary");
    label = gtk_builder_get_object (builder, "label-primary");
    primary_info = gtk_builder_get_object (builder, "primary-info-button");
    primary_indicator = gtk_builder_get_object (builder, "primary-indicator");

    /* If there's only one display we hide the primary option as it is meaningless */
    gtk_widget_set_visible (GTK_WIDGET (check), multiple_displays);
    gtk_widget_set_visible (GTK_WIDGET (label), multiple_displays);
    gtk_widget_set_visible (GTK_WIDGET (primary_info), multiple_displays);
    gtk_widget_set_visible (GTK_WIDGET (primary_indicator), multiple_displays);
    if (!multiple_displays)
        return;

    if (xfce_randr->mode[active_output] == None)
        output_on = FALSE;
    gtk_widget_set_sensitive (GTK_WIDGET (check), output_on);
    gtk_widget_set_sensitive (GTK_WIDGET (label), output_on);
    gtk_widget_set_visible (GTK_WIDGET (primary_indicator), primary);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_primary_toggled,
                                     builder);
    gtk_switch_set_state (GTK_SWITCH (check), primary);
    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (check, display_setting_primary_toggled,
                                       builder);
}

static gboolean
display_setting_output_toggled (GtkSwitch       *widget,
                                gboolean         output_on,
                                GtkBuilder      *builder)
{
    if (!xfce_randr)
        return FALSE;

    if (xfce_randr->noutput <= 1)
        return FALSE;

    if (output_on)
        xfce_randr->mode[active_output] =
            xfce_randr_preferred_mode (xfce_randr, active_output);
    else
    {
        if (display_settings_get_n_active_outputs () == 1)
        {
            xfce_dialog_show_warning (NULL,
                                      _("The last active output must not be disabled, the system would"
                                        " be unusable."),
                                      _("Selected output not disabled"));
            return FALSE;
        }
        xfce_randr->mode[active_output] = None;
    }

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, active_output);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));

    return display_setting_ask_fallback (builder);
}

static void
display_setting_output_status_populate (GtkBuilder *builder)
{
    GObject *check;

    if (!xfce_randr)
        return;

    check = gtk_builder_get_object (builder, "output-on");

    if (xfce_randr->noutput > 1)
        gtk_widget_show (GTK_WIDGET (check));
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        return;
    }

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_output_toggled,
                                     builder);
    gtk_switch_set_state (GTK_SWITCH (check),
                          xfce_randr->mode[active_output] != None);
    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (check, display_setting_output_toggled,
                                       builder);
}

static void
display_settings_combobox_selection_changed (GtkComboBox *combobox,
                                             GtkBuilder  *builder)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    GtkWidget    *popup;
    gint          active_id, previous_id;

    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        /* Get the output info */
        model = gtk_combo_box_get_model (combobox);
        gtk_tree_model_get (model, &iter, COLUMN_OUTPUT_ID, &active_id, -1);

        /* Get the new active screen or output */
        previous_id = active_output;
        active_output = active_id;

        /* Update the combo boxes */
        display_setting_output_status_populate (builder);
        display_setting_primary_populate (builder);
        display_setting_mirror_displays_populate (builder);
        display_setting_resolutions_populate (builder);
        display_setting_refresh_rates_populate (builder);
        display_setting_rotations_populate (builder);
        display_setting_reflections_populate (builder);
        display_setting_scale_populate (builder);

        /* redraw the two (old active, new active) popups */
        popup = g_hash_table_lookup (display_popups, GINT_TO_POINTER (previous_id));
        if (popup)
            gtk_widget_queue_draw (popup);
        popup = g_hash_table_lookup (display_popups, GINT_TO_POINTER (active_id));
        if (popup)
            gtk_widget_queue_draw (popup);

        if (randr_gui_area)
            foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
    }
}

static gchar **
display_settings_get_display_infos (void)
{
    gchar   **display_infos;
    guint     m;

    display_infos = g_new0 (gchar *, xfce_randr->noutput + 1);
    /* get all display edids, to only query randr once */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        display_infos[m] = g_strdup_printf ("%s", xfce_randr_get_edid (xfce_randr, m));
    }

    return display_infos;
}

static void
display_settings_minimal_profile_populate (GtkBuilder *builder)
{
    GObject  *profile_box, *profile_display1;
    GList    *profiles = NULL;
    GList    *current;
    gchar   **display_infos;

    profile_box  = gtk_builder_get_object (builder, "profile-box");
    profile_display1  = gtk_builder_get_object (builder, "display1");

    display_infos = display_settings_get_display_infos ();
    profiles = display_settings_get_profiles (display_infos, display_channel);
    g_strfreev (display_infos);

    current = g_list_first (profiles);
    while (current)
    {
        GtkWidget *box, *profile_radio, *label, *image;
        gchar *property;
        gchar *profile_name;

        /* use the display string value of the profile hash property */
        property = g_strdup_printf ("/%s", (gchar *)current->data);
        profile_name = xfconf_channel_get_string (display_channel, property, NULL);

        label = gtk_label_new (profile_name);
        image = gtk_image_new_from_icon_name ("xfce-display-profile", 128);
        gtk_image_set_pixel_size (GTK_IMAGE (image), 128);

        profile_radio = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (profile_display1));
        gtk_container_add (GTK_CONTAINER (profile_radio), image);
        g_object_set_data (G_OBJECT (profile_radio), "profile", (gchar *)current->data);
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (profile_radio), FALSE);
        gtk_widget_set_size_request (GTK_WIDGET (profile_radio), 128, 128);

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start (GTK_BOX (box), profile_radio, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 3);
        gtk_widget_set_margin_start (GTK_WIDGET (box), 24);
        gtk_box_pack_start (GTK_BOX (profile_box), box, FALSE, FALSE, 0);

        g_signal_connect (profile_radio, "toggled", G_CALLBACK (display_settings_minimal_profile_apply),
                          builder);

        current = g_list_next (current);
        g_free (property);
        g_free (profile_name);
    }

    gtk_widget_show_all (GTK_WIDGET (profile_box));
}

static void
display_settings_profile_list_init (GtkBuilder *builder)
{
    GtkListStore      *store;
    GObject           *treeview;
    GtkCellRenderer   *renderer;
    GtkTreeViewColumn *column;

    store = gtk_list_store_new (N_COLUMNS,
                                G_TYPE_ICON,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

    treeview = gtk_builder_get_object (builder, "randr-profile");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    /* Setup Profile name column */
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "gicon", COLUMN_ICON, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    /* Setup Profile name column */
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, _("Profiles matching the currently connected displays"));
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    /* Setup Profile hash column */
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_HASH, NULL);
    gtk_tree_view_column_set_visible (column, FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    g_object_unref (G_OBJECT (store));
}

static void
display_settings_profile_list_populate (GtkBuilder *builder)
{
    GtkListStore     *store;
    GObject          *treeview;
    GtkTreeIter       iter;
    GList            *profiles = NULL;
    GList            *current;
    gchar           **display_infos;

    /* create a new list store */
    store = gtk_list_store_new (N_COLUMNS,
                                G_TYPE_ICON,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

    /* set up the new combobox which will replace the above combobox */
    treeview = gtk_builder_get_object (builder, "randr-profile");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

    display_infos = display_settings_get_display_infos ();
    profiles = display_settings_get_profiles (display_infos, display_channel);
    g_strfreev (display_infos);

    /* Populate treeview */
    current = g_list_first (profiles);
    while (current)
    {
        gchar *property;
        gchar *profile_name;
        gchar *active_profile_hash;
        GIcon *icon = NULL;

        /* use the display string value of the profile hash property */
        property = g_strdup_printf ("/%s", (gchar *)current->data);
        profile_name = xfconf_channel_get_string (display_channel, property, NULL);
        active_profile_hash = xfconf_channel_get_string (display_channel, "/ActiveProfile", "Default");

        /* highlight the currently active profile */
        if (g_strcmp0 ((gchar *)current->data, active_profile_hash) == 0)
        {
            icon = g_themed_icon_new_with_default_fallbacks ("object-select-symbolic");
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_ICON, icon,
                            COLUMN_NAME, profile_name,
                            COLUMN_HASH, (gchar *)current->data,
                            -1);

        current = g_list_next (current);
        g_free (property);
        g_free (profile_name);
        g_free (active_profile_hash);
        if (icon)
            g_object_unref (icon);
    }

    /* Release the store */
    g_list_free (profiles);
    g_object_unref (G_OBJECT (store));
}

static void
display_settings_combobox_populate (GtkBuilder *builder)
{
    guint             m;
    GtkListStore     *store;
    GObject          *combobox;
    GtkTreeIter       iter;
    gboolean          selected = FALSE;

    /* Create a new list store */
    store = gtk_list_store_new (N_OUTPUT_COLUMNS,
                                G_TYPE_STRING, /* COLUMN_OUTPUT_NAME */
                                G_TYPE_INT);   /* COLUMN_OUTPUT_ID */

    /* Set up the new combobox which will replace the above combobox */
    combobox = gtk_builder_get_object (builder, "randr-outputs");
    gtk_combo_box_set_model (GTK_COMBO_BOX (combobox), GTK_TREE_MODEL (store));

    /* Walk all the connected outputs */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        gchar *friendly_name;

        /* Insert the output in the store */
        if (xfce_randr->noutput > 1)
            friendly_name = g_strdup_printf ("%d - %s", m + 1, xfce_randr->friendly_name[m]);
        else
            friendly_name = xfce_randr->friendly_name[m];
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_OUTPUT_NAME, friendly_name,
                            COLUMN_OUTPUT_ID, m, -1);

        /* Select active output */
        if (m == active_output)
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), m);
            selected = TRUE;
        }
        if (xfce_randr->noutput > 1)
            g_free (friendly_name);
    }

    /* If nothing was selected the active output is no longer valid,
     * select the last display in the list. */
    if (!selected)
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), m);

    /* Release the store */
    g_object_unref (G_OBJECT (store));
}

static void
display_settings_combo_box_create (GtkComboBox *combobox,
                                   gboolean     resolution)
{
    GtkCellRenderer *renderer;
    GtkListStore    *store;

    /* Create and set the combobox model */
    if (resolution)
        store = gtk_list_store_new (N_RESOLUTION_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    else
        store = gtk_list_store_new (N_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));

    /* Setup renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
    if (resolution)
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", RESOLUTION_COLUMN_COMBO_NAME);
    else
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", COLUMN_COMBO_NAME);

    /* Add another column for the resolution combobox to display the ratio */
    if (resolution)
    {
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "markup", RESOLUTION_COLUMN_COMBO_MARKUP);
        gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
    }
}

static void
display_settings_dialog_response (GtkDialog  *dialog,
                                  gint        response_id,
                                  GtkBuilder *builder)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "display",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else if (response_id == GTK_RESPONSE_CLOSE)
    {
        gchar *new_active_profile = xfconf_channel_get_string (display_channel, "/ActiveProfile", NULL);
        gchar *property = g_strdup_printf ("/%s", active_profile);
        gchar *profile_name = xfconf_channel_get_string (display_channel, property, NULL);

        if (g_strcmp0 (active_profile, new_active_profile) != 0 &&
            profile_name != NULL &&
            g_strcmp0 (active_profile, "Default") != 0)
        {
            GtkBuilder *profile_changed_builder;
            GError     *error = NULL;
            gint        profile_response_id;

            profile_changed_builder = gtk_builder_new ();

            if (gtk_builder_add_from_string (profile_changed_builder, profile_changed_dialog_ui,
                                             profile_changed_dialog_ui_length, &error) != 0)
            {
                GObject *profile_changed_dialog, *label, *button;
                const char *str;
                const char *format = "<big><b>\%s</b></big>";
                char *markup;
                gchar *button_label;

                profile_changed_dialog = gtk_builder_get_object (profile_changed_builder, "profile-changed-dialog");

                gtk_window_set_transient_for (GTK_WINDOW (profile_changed_dialog), GTK_WINDOW (dialog));
                gtk_window_set_modal (GTK_WINDOW (profile_changed_dialog), TRUE);

                label = gtk_builder_get_object (profile_changed_builder, "header");
                str = g_strdup_printf(_("Update changed display profile '%s'?"), profile_name);
                markup = g_markup_printf_escaped (format, str);
                gtk_label_set_markup (GTK_LABEL (label), markup);

                button = gtk_builder_get_object (profile_changed_builder, "button-update");
                button_label = g_strdup_printf (_("_Update '%s'"), profile_name);
                gtk_button_set_label (GTK_BUTTON (button), button_label);

                profile_response_id = gtk_dialog_run (GTK_DIALOG (profile_changed_dialog));
                gtk_widget_destroy (GTK_WIDGET (profile_changed_dialog));
                g_free (markup);
                g_free (button_label);
            }
            else
            {
                profile_response_id = 2;
                g_error ("Failed to load the UI file: %s.", error->message);
                g_error_free (error);
            }

            /* update the profile */
            if (profile_response_id == GTK_RESPONSE_OK)
            {
                guint i;

                for (i = 0; i < xfce_randr->noutput; i++)
                    xfce_randr_save_output (xfce_randr, active_profile, display_channel, i);

                xfconf_channel_set_string (display_channel, "/ActiveProfile", active_profile);
            }

            g_object_unref (G_OBJECT (profile_changed_builder));
        }
        g_free (profile_name);
        g_free (property);
        g_free (new_active_profile);
        g_free (active_profile);
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
set_display_popups_visible(gboolean visible)
{
    GHashTableIter iter;
    gpointer key, value;
    GtkWidget *popup;

    g_hash_table_iter_init (&iter, display_popups);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        popup = (GtkWidget *) value;
        gtk_widget_set_visible (popup, visible);
    }
}

static gboolean
on_identify_displays_toggled (GtkWidget *widget,
                              gboolean   state,
                              GtkBuilder *builder)
{
    set_display_popups_visible (state);
    gtk_switch_set_state (GTK_SWITCH (widget), state);

    return TRUE;
}

static void
display_setting_apply (GtkWidget *widget, GtkBuilder *builder)
{
    guint i = 0;

    for (i=0; i < xfce_randr->noutput; i++)
        xfce_randr_save_output (xfce_randr, "Default", display_channel, i);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    display_setting_ask_fallback (builder);

    gtk_widget_set_sensitive(widget, FALSE);
}

static void
display_settings_profile_changed (GtkTreeSelection *selection, GtkBuilder *builder)
{
    GObject *button;
    GtkTreeModel      *model;
    GtkTreeIter        iter;
    gboolean selected;

    selected = gtk_tree_selection_get_selected (selection, &model, &iter);

    button = gtk_builder_get_object (builder, "button-profile-save");
    gtk_widget_set_sensitive (GTK_WIDGET (button), selected);
    button = gtk_builder_get_object (builder, "button-profile-delete");
    gtk_widget_set_sensitive (GTK_WIDGET (button), selected);
    button = gtk_builder_get_object (builder, "button-profile-apply");
    gtk_widget_set_sensitive (GTK_WIDGET (button), selected);
}

static void
display_settings_minimal_profile_apply (GtkToggleButton *widget, GtkBuilder *builder)
{
    gchar  *profile_hash;

    profile_hash = (gchar *) g_object_get_data (G_OBJECT (widget), "profile");
    xfce_randr_apply (xfce_randr, profile_hash, display_channel);
}

static void
display_settings_profile_save (GtkWidget *widget, GtkBuilder *builder)
{
    GObject           *treeview;
    GtkTreeSelection  *selection;
    GtkTreeModel      *model;
    GtkTreeIter        iter;

    treeview = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        guint i = 0;
        gchar *property;
        gchar *profile_hash;
        gchar *profile_name;

        gtk_tree_model_get (model, &iter, COLUMN_NAME, &profile_name, COLUMN_HASH, &profile_hash, -1);
        property = g_strdup_printf ("/%s", profile_hash);

        for (i = 0; i < xfce_randr->noutput; i++)
            xfce_randr_save_output (xfce_randr, profile_hash, display_channel, i);

        /* save the human-readable name of the profile as string value */
        xfconf_channel_set_string (display_channel, property, profile_name);
        xfconf_channel_set_string (display_channel, "/ActiveProfile", profile_hash);

        display_settings_profile_list_populate (builder);
        gtk_widget_set_sensitive (widget, FALSE);

        g_free (property);
        g_free (profile_hash);
        g_free (profile_name);
    }
    else
        gtk_widget_set_sensitive (widget, TRUE);
}

/* reset the widget states if the user starts editing the profile name */
static void
display_settings_profile_entry_text_changed (GtkEditable *entry,
                                             GtkBuilder  *builder)
{
    GObject *infobar, *button;

    button = gtk_builder_get_object (builder, "button-profile-create-cb");
    infobar = gtk_builder_get_object (builder, "profile-exists");

    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), "error");
    gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
    gtk_widget_hide (GTK_WIDGET (infobar));
}

static void
display_settings_profile_create_cb (GtkWidget *widget, GtkBuilder *builder)
{
    const gchar *profile_name;
    GtkWidget *popover;
    GObject *infobar, *entry, *button;

    entry = gtk_builder_get_object (builder, "entry-profile-create");
    profile_name = gtk_entry_get_text (GTK_ENTRY (entry));

    /* check if the profile name is already taken */
    if (!display_settings_profile_name_exists (display_channel, profile_name))
    {
        button = gtk_builder_get_object (builder, "button-profile-create-cb");
        infobar = gtk_builder_get_object (builder, "profile-exists");

        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), "error");
        gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
        gtk_widget_show_all (GTK_WIDGET (infobar));

        g_signal_connect (G_OBJECT (entry), "changed",
                          G_CALLBACK (display_settings_profile_entry_text_changed), builder);
        return;
    }

    if (profile_name)
    {
        guint i = 0;
        gchar *property;
        gchar *profile_hash;

        profile_hash = g_compute_checksum_for_string (G_CHECKSUM_SHA1, profile_name, strlen(profile_name));
        property = g_strdup_printf ("/%s", profile_hash);
        for (i = 0; i < xfce_randr->noutput; i++)
            xfce_randr_save_output (xfce_randr, profile_hash, display_channel, i);

        /* save the human-readable name of the profile as string value */
        xfconf_channel_set_string (display_channel, property, profile_name);
        xfconf_channel_set_string (display_channel, "/ActiveProfile", profile_hash);
        display_settings_profile_list_populate (builder);

        g_free (property);
        g_free (profile_hash);
    }
    popover = gtk_widget_get_ancestor (widget, GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
display_settings_profile_create (GtkWidget *widget, GtkBuilder *builder)
{
    GObject *popover, *entry, *button, *infobar;

    /* Create a popover dialog for saving a new profile */
    popover = gtk_builder_get_object (builder, "popover-create-profile");
    entry = gtk_builder_get_object (builder, "entry-profile-create");
    button = gtk_builder_get_object (builder, "button-profile-create-cb");
    infobar = gtk_builder_get_object (builder, "profile-exists");

    gtk_widget_show (GTK_WIDGET (popover));
    gtk_widget_hide (GTK_WIDGET (infobar));
    gtk_widget_grab_focus (GTK_WIDGET (entry));
    gtk_widget_grab_default (GTK_WIDGET (button));

    g_signal_connect (button, "clicked", G_CALLBACK (display_settings_profile_create_cb), builder);
}

static void
display_settings_profile_apply (GtkWidget *widget, GtkBuilder *builder)
{
    GObject           *treeview;
    GtkTreeSelection  *selection;
    GtkTreeModel      *model;
    GtkTreeIter        iter;

    treeview = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gchar *profile_hash;
        gchar *old_profile_hash;

        old_profile_hash = xfconf_channel_get_string (display_channel, "/ActiveProfile", "Default");
        gtk_tree_model_get (model, &iter, COLUMN_HASH, &profile_hash, -1);
        xfce_randr_apply (xfce_randr, profile_hash, display_channel);
        xfconf_channel_set_string (display_channel, "/ActiveProfile", profile_hash);

        if (!display_setting_timed_confirmation (builder))
        {
            xfce_randr_apply (xfce_randr, old_profile_hash, display_channel);
            xfconf_channel_set_string (display_channel, "/ActiveProfile", old_profile_hash);

            foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));
        }
        display_settings_profile_list_populate (builder);

        g_free (profile_hash);
    }
}

static void
display_settings_profile_row_activated (GtkTreeView       *tree_view,
                                        GtkTreePath       *path,
                                        GtkTreeViewColumn *column,
                                        gpointer           user_data)
{
    GtkBuilder *builder = user_data;
    display_settings_profile_apply (NULL, builder);
}

static void
display_settings_profile_delete (GtkWidget *widget, GtkBuilder *builder)
{
    GObject           *treeview;
    GtkTreeSelection  *selection;
    GtkTreeModel      *model;
    GtkTreeIter        iter;

    treeview = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gchar *profile_name;
        gchar *profile_hash;
        gint   response;
        gchar *primary_message;

        gtk_tree_model_get (model, &iter, COLUMN_NAME, &profile_name, COLUMN_HASH, &profile_hash, -1);
        primary_message = g_strdup_printf (_("Do you want to delete the display profile '%s'?"), profile_name);

        response = xfce_message_dialog (NULL, _("Delete Profile"),
                                        "user-trash",
                                        primary_message,
                                        _("Once a display profile is deleted it cannot be restored."),
                                        _("Cancel"), GTK_RESPONSE_NO,
                                        _("Delete"), GTK_RESPONSE_YES,
                                        NULL);

        g_free (primary_message);

        if (response == GTK_RESPONSE_YES)
        {
            GString *property;

            property = g_string_new (profile_hash);
            g_string_prepend_c (property, '/');

            xfconf_channel_reset_property (display_channel, property->str, True);
            xfconf_channel_set_string (display_channel, "/ActiveProfile", "Default");
            display_settings_profile_list_populate (builder);
            g_free (profile_name);
        }
        else
        {
            g_free (profile_name);
            return;
        }
    }
}

static void
display_setting_minimal_autoconnect_mode_changed (GtkComboBox *combobox,
                                                  GtkBuilder  *builder)
{
    gint value;
    gboolean state = TRUE;
    GObject *auto_enable_profiles;

    value = gtk_combo_box_get_active (combobox);
    /* On "Do nothing" disable the "auto-enable-profiles" option */
    if (value == 0)
      state = FALSE;

    auto_enable_profiles = gtk_builder_get_object (builder, "auto-enable-profiles");
    gtk_widget_set_sensitive (GTK_WIDGET (auto_enable_profiles), state);
    auto_enable_profiles = gtk_builder_get_object (builder, "auto-enable-profiles-label");
    gtk_widget_set_sensitive (GTK_WIDGET (auto_enable_profiles), state);
}

static void
display_settings_launch_settings_dialogs (GtkButton *button,
                                          gpointer   user_data)
{
    gchar    *command = user_data;
    GAppInfo *app_info = NULL;
    GError   *error = NULL;

    app_info = g_app_info_create_from_commandline (command, "Xfce Settings", G_APP_INFO_CREATE_NONE, &error);

    if (G_UNLIKELY (app_info == NULL)) {
        g_warning ("Could not find application %s", error->message);
        return;
    }
    if (error != NULL)
        g_error_free (error);

    if (!g_app_info_launch (app_info, NULL, NULL, &error))
        g_warning ("Could not launch the application %s", error->message);
    if (error != NULL)
        g_error_free (error);
}

static void
display_settings_primary_status_info_populate (GtkBuilder *builder)
{
    GObject          *widget;
    GtkWidget        *image;
    XfconfChannel    *channel;
    gchar            *primary_status_panel;
    gint              primary_status;
    gint              panels = 0;
    gint              panels_with_primary = 0;
    gchar            *property;

    widget = gtk_builder_get_object (builder, "primary-info-button");
    image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_BUTTON);
    gtk_container_add (GTK_CONTAINER (widget), image);
    gtk_widget_show (image);

    channel = xfconf_channel_new ("xfce4-panel");
    widget = gtk_builder_get_object (builder, "panel-ok");
    property = g_strdup_printf ("/panels/panel-%u/output-name", panels);
    /* Check all panels and show the ok icon on the first occurence of a panel set to "Primary" */
    for (panels = 0; xfconf_channel_has_property (channel, property); panels++)
    {
        primary_status_panel = xfconf_channel_get_string (channel, property, "Automatic");
        if (g_strcmp0 (primary_status_panel, "Primary") == 0)
        {
            gtk_widget_show (GTK_WIDGET (widget));
            panels_with_primary++;
        }
        else
            gtk_widget_hide (GTK_WIDGET (widget));
        property = g_strdup_printf ("/panels/panel-%u/output-name", panels + 1);
        g_free (primary_status_panel);
    }
    if (panels_with_primary > 1)
    {
        gchar *label;
        widget = gtk_builder_get_object (builder, "panel-label");
        label = g_strdup_printf (_("%d Xfce Panels"), panels_with_primary);
        gtk_label_set_text (GTK_LABEL (widget), label);
        g_free (label);
    }
    g_free (property);
    g_object_unref (G_OBJECT (channel));
    widget = gtk_builder_get_object (builder, "panel-configure");
    g_signal_connect (widget, "clicked", G_CALLBACK (display_settings_launch_settings_dialogs), "xfce4-panel --preferences");

    channel = xfconf_channel_new ("xfce4-desktop");
    primary_status = xfconf_channel_get_bool (channel, "/desktop-icons/primary", FALSE);
    widget = gtk_builder_get_object (builder, "desktop-ok");
    gtk_widget_set_visible (GTK_WIDGET (widget), primary_status);
    g_object_unref (G_OBJECT (channel));
    widget = gtk_builder_get_object (builder, "desktop-configure");
    g_signal_connect (widget, "clicked", G_CALLBACK (display_settings_launch_settings_dialogs), "xfdesktop-settings");

    channel = xfconf_channel_new ("xfce4-notifyd");
    primary_status = xfconf_channel_get_uint (channel, "/primary-monitor", 0);
    widget = gtk_builder_get_object (builder, "notifications-ok");
    gtk_widget_set_visible (GTK_WIDGET (widget), primary_status);
    g_object_unref (G_OBJECT (channel));
    widget = gtk_builder_get_object (builder, "notifications-configure");
    g_signal_connect (widget, "clicked", G_CALLBACK (display_settings_launch_settings_dialogs), "xfce4-notifyd-config");
}

static GtkWidget *
display_settings_dialog_new (GtkBuilder *builder)
{
    GObject          *combobox;
    GtkCellRenderer  *renderer;
    GObject          *label, *check, *primary, *mirror, *identify, *primary_indicator;
    GObject          *revealer, *spinbutton;
    GtkWidget        *button;
    GtkTreeSelection *selection;

    /* Get the combobox */
    combobox = gtk_builder_get_object (builder, "randr-outputs");

    /* Text renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), renderer, "text", COLUMN_OUTPUT_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Identification popups */
    display_setting_identity_popups_populate ();
    identify = gtk_builder_get_object (builder, "identify-displays");
    g_signal_connect (G_OBJECT (identify), "state-set", G_CALLBACK (on_identify_displays_toggled), builder);
    xfconf_g_property_bind (display_channel, "/IdentityPopups", G_TYPE_BOOLEAN, identify,
                            "active");
    set_display_popups_visible (gtk_switch_get_active (GTK_SWITCH (identify)));

    /* Display selection combobox */
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_settings_combobox_selection_changed), builder);

    /* Setup the combo boxes */
    check = gtk_builder_get_object (builder, "output-on");
    primary = gtk_builder_get_object (builder, "primary");
    mirror = gtk_builder_get_object (builder, "mirror-displays");
    g_signal_connect (G_OBJECT (check), "state-set", G_CALLBACK (display_setting_output_toggled), builder);
    g_signal_connect (G_OBJECT (primary), "state-set", G_CALLBACK (display_setting_primary_toggled), builder);
    g_signal_connect (G_OBJECT (mirror), "toggled", G_CALLBACK (display_setting_mirror_displays_toggled), builder);
    if (xfce_randr->noutput > 1)
    {
        gtk_widget_show (GTK_WIDGET (check));
        gtk_widget_show (GTK_WIDGET (mirror));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        gtk_widget_hide (GTK_WIDGET (mirror));
    }

    /* Set up primary status info button */
    display_settings_primary_status_info_populate (builder);
    primary_indicator = gtk_builder_get_object (builder, "primary-indicator");
    gtk_widget_set_visible (GTK_WIDGET (primary_indicator), gtk_switch_get_active (GTK_SWITCH (primary)));

    label = gtk_builder_get_object (builder, "label-reflection");
    gtk_widget_show (GTK_WIDGET (label));

    combobox = gtk_builder_get_object (builder, "randr-scale");
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_scale_changed), builder);
    revealer = gtk_builder_get_object (builder, "revealer-scale");
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
    else
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);

    spinbutton = gtk_builder_get_object (builder, "spin-scale-x");
    g_signal_connect (G_OBJECT (spinbutton), "value-changed", G_CALLBACK (display_setting_custom_scale_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-reflection");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), FALSE);
    gtk_widget_show (GTK_WIDGET (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_reflections_changed), builder);

    display_settings_aspect_ratios_populate ();
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), TRUE);
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), FALSE);
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-rotation");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), FALSE);
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (combobox));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (combobox), FALSE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (display_settings_profile_changed), builder);
    g_signal_connect (G_OBJECT (combobox), "row-activated", G_CALLBACK (display_settings_profile_row_activated), builder);

    combobox = gtk_builder_get_object (builder, "autoconnect-mode");
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_minimal_autoconnect_mode_changed), builder);
    xfconf_g_property_bind (display_channel, "/Notify", G_TYPE_INT, combobox,
                            "active");
    /* Correctly initialize the state of the auto-enable-profiles setting based on autoconnect-mode */
    if (xfconf_channel_get_int (display_channel, "/Notify", -1) == -1)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), NOTIFY_PROP_DEFAULT);
        display_setting_minimal_autoconnect_mode_changed ((GTK_COMBO_BOX (combobox)), builder);
    }

    apply_button = GTK_WIDGET (gtk_builder_get_object (builder, "apply"));
    g_signal_connect (G_OBJECT (apply_button), "clicked", G_CALLBACK (display_setting_apply), builder);
    gtk_widget_set_sensitive (apply_button, FALSE);

    check = gtk_builder_get_object (builder, "auto-enable-profiles");
    xfconf_g_property_bind (display_channel, "/AutoEnableProfiles", G_TYPE_BOOLEAN, check,
                            "active");

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-save"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_save), builder);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-delete"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_delete), builder);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-apply"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_apply), builder);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-create"));
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_create), builder);

    /* Populate the combobox */
    display_settings_combobox_populate (builder);
    display_settings_profile_list_init (builder);
    display_settings_profile_list_populate (builder);

    return GTK_WIDGET (gtk_builder_get_object (builder, "display-dialog"));
}

static void
display_settings_minimal_only_display1_toggled (GtkToggleButton *button,
                                                GtkBuilder      *builder)
{
    GObject *buttons;

    if (!gtk_toggle_button_get_active (button))
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Put Display1 in its preferred mode and deactivate Display2 */
    xfce_randr->mode[0] = xfce_randr_preferred_mode (xfce_randr, 0);
    xfce_randr->mode[1] = None;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0);
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_only_display2_toggled (GtkToggleButton *button,
                                                GtkBuilder      *builder)
{
    GObject *buttons;

    if (!gtk_toggle_button_get_active(button) )
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Put Display2 in its preferred mode and deactivate Display1 */
    xfce_randr->mode[1] = xfce_randr_preferred_mode (xfce_randr, 1);
    xfce_randr->mode[0] = None;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0);
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_mirror_displays_toggled (GtkToggleButton *button,
                                                  GtkBuilder      *builder)
{
    GObject *buttons;
    guint    n;
    RRMode  *clonable_modes;

    if (!gtk_toggle_button_get_active(button))
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Activate mirror-mode with a single mode for all of them */
    clonable_modes = xfce_randr_clonable_modes (xfce_randr);
    /* Configure each available display for mirroring */
    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] == None)
            continue;

        if (clonable_modes != NULL)
            xfce_randr->mode[n] = clonable_modes[n];
        xfce_randr->mirrored[n] = TRUE;
        xfce_randr->rotation[n] = RR_Rotate_0;
        xfce_randr->position[n].x = 0;
        xfce_randr->position[n].y = 0;
        xfce_randr_save_output (xfce_randr, "Default", display_channel, n);
    }
    g_free (clonable_modes);

    /* Apply all changes */
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_extend_right_toggled (GtkToggleButton *button,
                                               GtkBuilder      *builder)
{
    const XfceRRMode *mode;
    GObject *buttons;
    guint    n;

    if (!gtk_toggle_button_get_active(button))
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET (buttons), FALSE);

    /* Activate all inactive displays */
    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] == None)
        {
            xfce_randr->mode[n] = xfce_randr_preferred_mode (xfce_randr, n);
        }
    }

    /* (Re)set Display1 to 0x0 */
    xfce_randr->position[0].x = 0;
    xfce_randr->position[0].y = 0;

    /* Move Display2 right of Display1 */
    mode = xfce_randr_find_mode_by_id (xfce_randr, 0, xfce_randr->mode[0]);
    xfce_randr->position[1].x = xfce_randr_mode_width(mode, 0);
    xfce_randr->position[1].y = 0;

    /* Save changes to both displays */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0);
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1);

    /* Apply all changes */
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET (buttons), TRUE);
}

static GdkFilterReturn
screen_on_event (GdkXEvent *xevent,
                 GdkEvent  *event,
                 gpointer   data)
{
    GtkBuilder *builder = data;
    XEvent     *e = xevent;
    gint        event_num;

    if (!e)
        return GDK_FILTER_CONTINUE;

    event_num = e->type - randr_event_base;

    if (event_num == RRScreenChangeNotify)
    {
        xfce_randr_reload (xfce_randr);
        display_settings_combobox_populate (builder);
        display_settings_profile_list_populate (builder);

        /* recreate the identify display popups */
        g_hash_table_destroy (display_popups);
        g_hash_table_destroy (display_ratio);
        display_setting_identity_popups_populate ();
        display_settings_aspect_ratios_populate ();
        set_display_popups_visible(show_popups);
    }

    initialize_connected_outputs();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (randr_gui_area));

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}

/* Xfce RANDR GUI **TODO** Place these functions in a sensible location */
/* This function checks the status quo of more than one display with respect to
   cloning and mirroring and returns:
      0: not cloned
      1: cloned (same x/y, same resolution)
      2: mirrored (same x/y, different resolution)
*/
static gint
get_mirrored_configuration (void)
{
    gboolean cloned = TRUE;
    gboolean mirrored = FALSE;
    RRMode  *clonable_modes = NULL;
    guint    n;

    if (!xfce_randr)
        return FALSE;

    if (xfce_randr->noutput <= 1)
        return FALSE;

    /* Can outputs be cloned? */
    if (display_settings_get_n_active_outputs () > 1)
        clonable_modes = xfce_randr_clonable_modes (xfce_randr);

    if (clonable_modes == NULL)
        return 0;

    /* Check if mirror settings are on */
    for (n = 0; n < xfce_randr->noutput; n++)
    {
        if (xfce_randr->mode[n] == None)
            continue;

        cloned &= (xfce_randr->mode[n] == clonable_modes[n] &&
                   xfce_randr->mirrored[n]);
        mirrored = xfce_randr->mirrored[n];

        if (!cloned)
            break;
    }
    g_free (clonable_modes);

    if (mirrored == TRUE && cloned == FALSE)
        return 2;
    else
        return cloned;
}

static XfceOutputInfo *
convert_xfce_output_info (gint output_id)
{
    XfceOutputInfo *output;
    const XfceRRMode *mode, *preferred;
    RRMode preferred_mode;
    gint x, y;

    xfce_randr_get_positions(xfce_randr, output_id, &x, &y);
    mode = xfce_randr_find_mode_by_id (xfce_randr, output_id, xfce_randr->mode[output_id]);
    preferred_mode = xfce_randr_preferred_mode (xfce_randr, output_id);
    preferred = xfce_randr_find_mode_by_id (xfce_randr, output_id, preferred_mode);
    output = g_new0 (XfceOutputInfo, 1);
    output->id = output_id;
    output->x = x;
    output->y = y;
    output->scalex = xfce_randr->scalex[output_id];
    output->scaley = xfce_randr->scaley[output_id];
    output->user_data = NULL;
    output->display_name = xfce_randr->friendly_name[output_id];
    output->connected = TRUE;
    output->on = xfce_randr->mode[output_id] != None;

    if (preferred != NULL) {
        output->pref_width = preferred->width;
        output->pref_height = preferred->height;
    } else {
        // Fallback on 640x480 if randr detection fails (Xfce #12580)
        output->pref_width = 640;
        output->pref_height = 480;
    }

    if (output->on) {
        output->rotation = xfce_randr->rotation[output_id];
        if (mode != NULL) {
            output->width = mode->width;
            output->height = mode->height;
            output->rate = mode->rate;
        } else if (preferred != NULL) {
            output->width = preferred->width;
            output->height = preferred->height;
            output->rate = preferred->rate;
        } else {
            output->width = 640;
            output->height = 480;
            output->rate = 0.0;
        }
    } else {
        output->rotation = 0;
        output->width = output->pref_width;
        output->height = output->pref_height;
        output->rate = 0.0;
    }

    return output;
}

typedef struct App App;
typedef struct GrabInfo GrabInfo;

struct App
{
    XfceOutputInfo     *current_output;

    GtkWidget          *dialog;
};

static gboolean output_overlaps (XfceOutputInfo *output);
static void get_geometry (XfceOutputInfo *output, int *w, int *h);

static void
lay_out_outputs_horizontally (void)
{
    gint x, y, temp_x;
    GList *list;

    /* Lay out all the monitors horizontally when "mirror screens" is turned
     * off, to avoid having all of them overlapped initially.  We put the
     * outputs turned off on the right-hand side.
     */

    x = 0;
    y = 0;

    /* First pass, all "on" outputs */
    for (list = current_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *output;

        output = list->data;
        if (output->connected && output->on)
        {
            y = MAX(output->y, y);
            temp_x = output->x + output->width;
            x = MAX(temp_x, x);
        }
    }

    /* Second pass, all the black screens */
    for (list = current_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *output;

        output = list->data;
        if (!(output->connected && output->on))
        {
            output->x = x;
            output->y = y;
            x += output->width;
        }
    }
}

static void
on_viewport_changed (FooScrollArea *scroll_area,
                     GdkRectangle  *old_viewport,
                     GdkRectangle  *new_viewport)
{
    foo_scroll_area_set_size (scroll_area,
                              new_viewport->width,
                              new_viewport->height);

    foo_scroll_area_invalidate (scroll_area);
}

static void
layout_set_font (PangoLayout *layout, const char *font)
{
    PangoFontDescription *desc =
        pango_font_description_from_string (font);

    if (desc)
    {
        pango_layout_set_font_description (layout, desc);

        pango_font_description_free (desc);
    }
}

static void
get_geometry (XfceOutputInfo *output, int *w, int *h)
{
    if (output->on)
    {
        if (output->scalex > 0 && output->scalex != 1.0
            && output->scaley > 0 && output->scaley != 1.0)
        {
            *h = output->height * output->scaley;
            *w = output->width * output->scalex;
        }
        else
        {
            *h = output->height;
            *w = output->width;
        }
    }
    else
    {
        *h = output->pref_height;
        *w = output->pref_width;
    }
    if ((output->rotation == RR_Rotate_90) || (output->rotation == RR_Rotate_270))
    {
        int tmp;
        tmp = *h;
        *h = *w;
        *w = tmp;
    }
}

static void
initialize_connected_outputs_at_zero(void)
{
    GList *list = NULL;
    gint start_x, start_y;

    start_x = G_MAXINT;
    start_y = G_MAXINT;

    /* Get the left-most and top-most coordinates */
    for (list = current_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *output = list->data;

        start_x = MIN(start_x, output->x);
        start_y = MIN(start_y, output->y);
    }

    /* Realign at zero */
    for (list = current_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *output = list->data;

        output->y = output->y - start_y;
        output->x = output->x - start_x;

        /* Update the Xfce Randr */
        xfce_randr->position[output->id].x = output->x;
        xfce_randr->position[output->id].y = output->y;
    }
}

static GList *
list_connected_outputs (gint *total_w, gint *total_h)
{
    gint dummy;
    guint m;
    GList *list = NULL;

    if (!total_w)
        total_w = &dummy;
    if (!total_h)
        total_h = &dummy;

    /* Do we need to initialize the current outputs? */
    if (!current_outputs)
    {
        for (m = 0; m < xfce_randr->noutput; ++m)
        {
            XfceOutputInfo *output = convert_xfce_output_info(m);

            current_outputs = g_list_prepend (current_outputs, output);
        }
        current_outputs = g_list_reverse (current_outputs);

        lay_out_outputs_horizontally();
    }

    *total_w = 0;
    *total_h = 0;

    for (list = current_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *output = list->data;

        int w, h;

        get_geometry (output, &w, &h);

        *total_w = MAX(*total_w, w + output->x);
        *total_h = MAX(*total_h, h + output->y);
    }

    return current_outputs;
}

static int
get_n_connected (void)
{
    return xfce_randr->noutput;
}

static double
compute_scale (void)
{
    int available_w, available_h;
    gint total_w, total_h;
    GdkRectangle viewport;

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (randr_gui_area), &viewport);

    list_connected_outputs (&total_w, &total_h);

    available_w = viewport.width - 2 * MARGIN;
    available_h = viewport.height - 2 * MARGIN;

    return MIN ((double)available_w / (double)total_w, (double)available_h / (double)total_h);
}

typedef struct Edge
{
    XfceOutputInfo *output;
    int x1, y1;
    int x2, y2;
} Edge;

typedef struct Snap
{
    Edge *snapper;      /* Edge that should be snapped */
    Edge *snappee;
    int dy, dx;
} Snap;

static void
add_edge (XfceOutputInfo *output, int x1, int y1, int x2, int y2, GArray *edges)
{
    Edge e;

    e.x1 = x1;
    e.x2 = x2;
    e.y1 = y1;
    e.y2 = y2;
    e.output = output;

    g_array_append_val (edges, e);
}

static void
list_edges_for_output (XfceOutputInfo *output, GArray *edges)
{
    int x, y, w, h;

    x = output->x;
    y = output->y;
    get_geometry (output, &w, &h);

    /* Top, Bottom, Left, Right */
    add_edge (output, x, y, x + w, y, edges);
    add_edge (output, x, y + h, x + w, y + h, edges);
    add_edge (output, x, y, x, y + h, edges);
    add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (GArray *edges)
{
    GList *connected_outputs = NULL;
    GList *list;

    connected_outputs = list_connected_outputs (NULL, NULL);

    for (list = connected_outputs; list != NULL; list = list->next)
    {
        list_edges_for_output (list->data, edges);
    }
}

static gboolean
overlap (int s1, int e1, int s2, int e2)
{
    return (!(e1 < s2 || s1 >= e2));
}

static gboolean
horizontal_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->y1 != snapper->y2 || snappee->y1 != snappee->y2)
        return FALSE;

    return overlap (snapper->x1, snapper->x2, snappee->x1, snappee->x2);
}

static gboolean
vertical_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->x1 != snapper->x2 || snappee->x1 != snappee->x2)
        return FALSE;

    return overlap (snapper->y1, snapper->y2, snappee->y1, snappee->y2);
}

static void
add_snap (GArray *snaps, Snap snap)
{
    if (ABS (snap.dx) <= 200 || ABS (snap.dy) <= 200)
        g_array_append_val (snaps, snap);
}

static void
add_edge_snaps (Edge *snapper, Edge *snappee, GArray *snaps)
{
    Snap snap;

    snap.snapper = snapper;
    snap.snappee = snappee;

    if (horizontal_overlap (snapper, snappee))
    {
        snap.dx = 0;
        snap.dy = snappee->y1 - snapper->y1;

        add_snap (snaps, snap);
    }
    else if (vertical_overlap (snapper, snappee))
    {
        snap.dy = 0;
        snap.dx = snappee->x1 - snapper->x1;

        add_snap (snaps, snap);
    }

    /* Corner snaps */
    /* 1->1 */
    snap.dx = snappee->x1 - snapper->x1;
    snap.dy = snappee->y1 - snapper->y1;

    add_snap (snaps, snap);

    /* 1->2 */
    snap.dx = snappee->x2 - snapper->x1;
    snap.dy = snappee->y2 - snapper->y1;

    add_snap (snaps, snap);

    /* 2->2 */
    snap.dx = snappee->x2 - snapper->x2;
    snap.dy = snappee->y2 - snapper->y2;

    add_snap (snaps, snap);

    /* 2->1 */
    snap.dx = snappee->x1 - snapper->x2;
    snap.dy = snappee->y1 - snapper->y2;

    add_snap (snaps, snap);
}

static void
list_snaps (XfceOutputInfo *output, GArray *edges, GArray *snaps)
{
    guint i;

    for (i = 0; i < edges->len; ++i)
    {
        Edge *output_edge = &(g_array_index (edges, Edge, i));

        if (output_edge->output == output)
        {
            guint j;

            for (j = 0; j < edges->len; ++j)
            {
                Edge *edge = &(g_array_index (edges, Edge, j));

                if (edge->output != output)
                    add_edge_snaps (output_edge, edge, snaps);
            }
        }
    }
}

static gboolean
corner_on_edge (int x, int y, Edge *e)
{
    if (x == e->x1 && x == e->x2 && y >= e->y1 && y <= e->y2)
        return TRUE;

    if (y == e->y1 && y == e->y2 && x >= e->x1 && x <= e->x2)
        return TRUE;

    return FALSE;
}

static gboolean
edges_align (Edge *e1, Edge *e2)
{
    if (corner_on_edge (e1->x1, e1->y1, e2))
        return TRUE;

    if (corner_on_edge (e2->x1, e2->y1, e1))
        return TRUE;

    return FALSE;
}

static gboolean
output_is_aligned (XfceOutputInfo *output, GArray *edges)
{
    gboolean result = FALSE;
    guint i;

    for (i = 0; i < edges->len; ++i)
    {
        Edge *output_edge = &(g_array_index (edges, Edge, i));

        if (output_edge->output == output)
        {
            guint j;

            for (j = 0; j < edges->len; ++j)
            {
                Edge *edge = &(g_array_index (edges, Edge, j));

                /* We are aligned if an output edge matches
                 * an edge of another output
                 */
                if (edge->output != output_edge->output)
                {
                    if (edges_align (output_edge, edge))
                    {
                        result = TRUE;
                        goto done;
                    }
                }
            }
        }
    }
done:
    return result;
}

static void
get_output_rect (XfceOutputInfo *output, GdkRectangle *rect)
{
    int w, h;

    get_geometry (output, &w, &h);

    rect->width = w;
    rect->height = h;
    rect->x = output->x;
    rect->y = output->y;
}

static gboolean
output_overlaps (XfceOutputInfo *output)
{
    GList         *connected_outputs = NULL;
    GList         *list;
    gboolean       overlaps = FALSE;
    GdkRectangle   output_rect;

    get_output_rect (output, &output_rect);

    connected_outputs = list_connected_outputs (NULL, NULL);

    for (list = connected_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *other = list->data;
        if (other != output)
        {
            GdkRectangle other_rect;

            get_output_rect (other, &other_rect);
            if (gdk_rectangle_intersect (&output_rect, &other_rect, NULL))
            {
                overlaps = TRUE;
                break;
            }
        }
    }

    return overlaps;
}

static gboolean
xfce_rr_config_is_aligned (GArray *edges)
{
    GList     *connected_outputs = NULL;
    GList     *list;
    gboolean   aligned = TRUE;

    connected_outputs = list_connected_outputs (NULL, NULL);

    for (list = connected_outputs; list != NULL; list = list->next)
    {
        XfceOutputInfo *output = list->data;
        if (!output_is_aligned (output, edges) || output_overlaps (output))
        {
            aligned = FALSE;
            break;
        }
    }

    return aligned;
}

struct GrabInfo
{
    int grab_x;
    int grab_y;
    int output_x;
    int output_y;
};

static gboolean
is_corner_snap (const Snap *s)
{
    return s->dx != 0 && s->dy != 0;
}

static int
compare_snaps (gconstpointer v1, gconstpointer v2)
{
    const Snap *s1 = v1;
    const Snap *s2 = v2;
    int sv1 = MAX (ABS (s1->dx), ABS (s1->dy));
    int sv2 = MAX (ABS (s2->dx), ABS (s2->dy));
    int d;

    d = sv1 - sv2;

    /* This snapping algorithm is good enough for rock'n'roll, but
     * this is probably a better:
     *
     *    First do a horizontal/vertical snap, then
     *    with the new coordinates from that snap,
     *    do a corner snap.
     *
     * Right now, it's confusing that corner snapping
     * depends on the distance in an axis that you can't actually see.
     *
     */
    if (d == 0)
    {
        if (is_corner_snap (s1) && !is_corner_snap (s2))
            return -1;
        else if (is_corner_snap (s2) && !is_corner_snap (s1))
            return 1;
        else
            return 0;
    }
    else
    {
        return d;
    }
}

/* Sets a mouse cursor for a widget's window.  As a hack, you can pass
 * GDK_BLANK_CURSOR to mean "set the cursor to NULL" (i.e. reset the widget's
 * window's cursor to its default).
 */
static void
set_cursor (GtkWidget *widget, GdkCursorType type)
{
    GdkCursor *cursor;
    GdkWindow *window;

    if (type == GDK_BLANK_CURSOR)
        cursor = NULL;
    else
        cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), type);

    window = gtk_widget_get_window (widget);

    if (window)
        gdk_window_set_cursor (window, cursor);

    if (cursor)
        g_object_unref (cursor);
}

static void
set_monitors_tooltip (gchar *tooltip_text)
{
    const char *text;

    if (tooltip_text)
        text = g_strdup (tooltip_text);

    else
        text = _("Select a monitor to change its properties; drag it to rearrange its placement.");

    gtk_widget_set_tooltip_text (randr_gui_area, text);
}

static void
on_output_event (FooScrollArea      *area,
                 FooScrollAreaEvent *event,
                 gpointer            data)
{
    XfceOutputInfo *output = data;
    gint            mirrored;

    //App *app = g_object_get_data (G_OBJECT (area), "app");

    mirrored = get_mirrored_configuration ();
    /* If the mouse is inside the outputs, set the cursor to "you can move me".  See
     * on_canvas_event() for where we reset the cursor to the default if it
     * exits the outputs' area.
     */
    if (event->type == FOO_MOTION_OUTSIDE)
        return;

    if (!mirrored && get_n_connected () > 1)
        set_cursor (GTK_WIDGET (area), GDK_FLEUR);

    if (event->type == FOO_BUTTON_PRESS)
    {
        GrabInfo *info;
        gchar *tooltip_text;

        gtk_combo_box_set_active (GTK_COMBO_BOX (randr_outputs_combobox), output->id);

        if (!mirrored && get_n_connected () > 1)
        {
            foo_scroll_area_begin_grab (area, on_output_event, data);

            info = g_new0 (GrabInfo, 1);
            info->grab_x = event->x;
            info->grab_y = event->y;
            info->output_x = output->x;
            info->output_y = output->y;

            tooltip_text = g_strdup_printf(_("(%i, %i)"), output->x, output->y);
            set_monitors_tooltip (tooltip_text);
            g_free (tooltip_text);

            output->user_data = info;
        }

        foo_scroll_area_invalidate (area);
    }
    else
    {
        if (foo_scroll_area_is_grabbed (area))
        {
            GrabInfo *info = output->user_data;
            double scale = compute_scale();
            int new_x, new_y;
            guint i;
            GArray *edges, *snaps;

            new_x = info->output_x + (event->x - info->grab_x) / scale;
            new_y = info->output_y + (event->y - info->grab_y) / scale;

            output->x = new_x;
            output->y = new_y;

            edges = g_array_new (TRUE, TRUE, sizeof (Edge));
            snaps = g_array_new (TRUE, TRUE, sizeof (Snap));

            list_edges (edges);
            list_snaps (output, edges, snaps);

            g_array_sort (snaps, compare_snaps);

            output->x = info->output_x;
            output->y = info->output_y;

            for (i = 0; i < snaps->len; ++i)
            {
                Snap *snap = &(g_array_index (snaps, Snap, i));
                GArray *new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

                output->x = new_x + snap->dx;
                output->y = new_y + snap->dy;

                g_array_set_size (new_edges, 0);
                list_edges (new_edges);

                if (xfce_rr_config_is_aligned (new_edges))
                {
                    g_array_free (new_edges, TRUE);
                    break;
                }
                else
                {
                    output->x = info->output_x;
                    output->y = info->output_y;
                    g_array_free (new_edges, TRUE);
                }
            }

            g_array_free (snaps, TRUE);
            g_array_free (edges, TRUE);

            if (event->type == FOO_BUTTON_RELEASE)
            {
                foo_scroll_area_end_grab (area);
                set_monitors_tooltip (NULL);

                g_free (output->user_data);
                output->user_data = NULL;

                initialize_connected_outputs_at_zero ();
                display_settings_changed ();
            }
            else
            {
                set_monitors_tooltip (g_strdup_printf(_("(%i, %i)"), output->x, output->y) );
            }

            foo_scroll_area_invalidate (area);
        }
    }
}

static void
on_canvas_event (FooScrollArea      *area,
                 FooScrollAreaEvent *event,
                 gpointer            data)
{
    /* If the mouse exits the outputs, reset the cursor to the default.  See
     * on_output_event() for where we set the cursor to the movement cursor if
     * it is over one of the outputs.
     */
    set_cursor (GTK_WIDGET (area), GDK_BLANK_CURSOR);
}

static void
paint_background (FooScrollArea *area,
                  cairo_t       *cr)
{
    GdkRectangle viewport;
    GtkWidget *widget;
    GtkStyleContext *ctx;

    widget = GTK_WIDGET (area);

    foo_scroll_area_get_viewport (area, &viewport);
    ctx = gtk_widget_get_style_context (widget);
    gtk_style_context_add_class (ctx, "view");

    foo_scroll_area_add_input_from_fill (area, cr, on_canvas_event, NULL);
}

static void
paint_output (cairo_t *cr, int i, gint scale_factor, double *snap_x, double *snap_y)
{
    int w, h;
    double scale = compute_scale();
    double x, y, end_x, end_y;
    gint total_w, total_h;
    GList *connected_outputs = list_connected_outputs (&total_w, &total_h);
    XfceOutputInfo *output = NULL;
    GList *entry = NULL;
    PangoLayout *layout;
    PangoRectangle ink_extent, log_extent;
    GdkRectangle viewport;
    cairo_pattern_t *pat_lin = NULL, *pat_radial = NULL;
    double alpha = 1.0;
    double available_w;
    double factor = 1.0;
    const char *text;
    gint    mirrored;

    mirrored = get_mirrored_configuration ();

    cairo_save (cr);

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (randr_gui_area), &viewport);

    entry = g_list_nth (connected_outputs, i);
    if (entry)
        output = entry->data;
    if (output)
        get_geometry (output, &w, &h);
    else
        return;

    viewport.height -= 2 * MARGIN;
    viewport.width -= 2 * MARGIN;

    /* Center the displayed outputs in the viewport */
    x = ceil (output->x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0);
    y = ceil (output->y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0);

    /* Align endpoints */
    end_x = x + ceil (w * scale);
    end_y = y + ceil (h * scale);
    if ( abs((int)end_x-(int)*snap_x) <= 1 )
    {
        end_x = *snap_x;
    }
    if ( abs((int)end_y-(int)*snap_y) <= 1 )
    {
        end_y = *snap_y;
    }
    *snap_x = end_x;
    *snap_y = end_y;

    cairo_translate (cr,
                     x + (w * scale) / 2,
                     y + (h * scale) / 2);

    /* rotation is already applied in get_geometry */

    if (output->rotation == RR_Reflect_X)
        cairo_scale (cr, -1, 1);

    if (output->rotation == RR_Reflect_Y)
        cairo_scale (cr, 1, -1);

    cairo_translate (cr,
                     - x - (w * scale) / 2,
                     - y - (h * scale) / 2);

    cairo_rectangle (cr, x, y, end_x - x, end_y - y);
    cairo_clip_preserve (cr);

    foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (randr_gui_area),
                                         cr, on_output_event, output);

    cairo_set_line_width (cr, 1.0);

    /* Make overlapping displays ('mirrored') more transparent so both displays can
       be recognized more easily */
    if (output->id != active_output && mirrored == 2)
        alpha = 0.5;
    /* When displays are mirrored it makes no sense to make them semi-transparent
       because they overlay each other completely */
    else if (mirrored == 1)
        alpha = 1.0;
    /* the inactive display should be more transparent and the overlapping one as
       well */
    else if (output->id != active_output || mirrored == 2)
        alpha = 0.7;

    if (output->on)
    {
        /* Background gradient for active display */
        pat_lin = cairo_pattern_create_linear (x, y, x, y + (h * scale));
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.0, 0.56, 0.85, 0.92, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.2, 0.33, 0.75, 0.92, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.7, 0.25, 0.57, 0.77, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 1.0, 0.17, 0.39, 0.63, alpha);
        cairo_set_source (cr, pat_lin);
        cairo_fill_preserve (cr);

        cairo_set_source_rgba (cr, 0.11, 0.26, 0.52, alpha);
        cairo_stroke (cr);
    }
    else
    {
        /* Background gradient for disabled display */
        pat_lin = cairo_pattern_create_linear (x, y, x, y + (h * scale));
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.0, 0.24, 0.3, 0.31, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.2, 0.17, 0.20, 0.22, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.7, 0.14, 0.16, 0.18, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 1.0, 0.07, 0.07, 0.07, alpha);
        cairo_set_source (cr, pat_lin);
        cairo_fill_preserve (cr);

        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, alpha);
        cairo_stroke (cr);
    }

    /* Draw inner stroke */
    cairo_rectangle (cr, x + 1.5, y + 1.5, end_x - x - 3, end_y - y - 3);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha - 0.75);
    cairo_stroke (cr);

    /* Draw reflection as radial gradient on a polygon */
    pat_radial = cairo_pattern_create_radial ((end_x -x) /2 + x, y, 1, (end_x -x) /2 + x, y, h * scale);
    cairo_pattern_add_color_stop_rgba (pat_radial, 0.0, 1.0, 1.0, 1.0, 0.4);
    cairo_pattern_add_color_stop_rgba (pat_radial, 0.5, 1.0, 1.0, 1.0, 0.15);
    cairo_pattern_add_color_stop_rgba (pat_radial, 0.8, 1.0, 1.0, 1.0, 0.0);

    cairo_move_to (cr, x + 1.5, y + 1.5);
    cairo_line_to (cr, end_x - 1.5, y + 1.5);
    cairo_line_to (cr, end_x - 1.5, y + (end_y - y) / 3);
    cairo_line_to (cr, x + 1.5, y + (end_y - y) / 1.5);
    cairo_close_path (cr);
    cairo_set_source (cr, pat_radial);
    cairo_fill (cr);

    /* Draw a panel type rectangle to show which monitor is primary */
    if (xfce_randr->status[output->id] == XFCE_OUTPUT_STATUS_PRIMARY) {
        GdkPixbuf   *pixbuf;
        GtkIconInfo *icon_info;
        GdkRGBA      fg;
        gint         icon_size;

        gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, &icon_size);
        icon_info = gtk_icon_theme_lookup_icon_for_scale (gtk_icon_theme_get_default (),
                                                          "help-about-symbolic",
                                                          icon_size,
                                                          scale_factor,
                                                          GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                                          | GTK_ICON_LOOKUP_FORCE_SIZE);

        gdk_rgba_parse (&fg, "#000000");
        pixbuf = gtk_icon_info_load_symbolic (icon_info, &fg, NULL, NULL, NULL, NULL, NULL);
        if (G_LIKELY (pixbuf != NULL))
        {
            cairo_save (cr);
            cairo_translate (cr, x + scale_factor, y + scale_factor);
            cairo_scale (cr, 1.0 / scale_factor, 1.0 / scale_factor);
            gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
            cairo_paint (cr);
            cairo_restore (cr);

            g_object_unref (pixbuf);
        }
    }

    /* Display name label*/
    if (mirrored == 1)
    {
    /* Translators:  this is the feature where what you see on your laptop's
     * screen is the same as your external monitor.  Here, "Mirror" is being
     * used as an adjective, not as a verb.  For example, the Spanish
     * translation could be "Pantallas en Espejo", *not* "Espejar Pantallas".
     */
        text = _("Mirror Screens");
    }
    else
    {
        text = output->display_name;
    }
    layout = gtk_widget_create_pango_layout (GTK_WIDGET (randr_gui_area), text);
    layout_set_font (layout, "Sans Bold 12");
    pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);

    available_w = w * scale + 0.5 - 6; /* Same as the inner rectangle's width, minus 1 pixel of padding on each side */

    cairo_scale (cr, factor, factor);

    if (available_w < ink_extent.width)
    {
        pango_layout_set_width (layout, available_w * PANGO_SCALE);
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
        log_extent.width = available_w;
    }

    cairo_move_to (cr,
                   x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                   y + ((h * scale + 0.5) - factor * log_extent.height) / 2 - 1);
    /* Try to make the text as readable as possible for overlapping displays */
    if (output->id == active_output && mirrored == 2)
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, alpha);
    else
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, alpha - 0.6);

    pango_cairo_show_layout (cr, layout);

    cairo_move_to (cr,
                   x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                   y + ((h * scale + 0.5) - factor * log_extent.height) / 2);

    /* Try to make the text as readable as possible for overlapping displays - the
       currently selected one could be painted below the other display*/
    if (output->id == active_output && mirrored == 2)
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
    else
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha);

    pango_cairo_show_layout (cr, layout);

    /* Display state label */
    if (!output->on)
    {
        PangoLayout *display_state;

        display_state = gtk_widget_create_pango_layout (GTK_WIDGET (randr_gui_area), _("(Disabled)"));
        layout_set_font (display_state, "Sans 8");
        pango_layout_get_pixel_extents (display_state, &ink_extent, &log_extent);

        available_w = w * scale + 0.5 - 6;
        if (available_w < ink_extent.width)
            factor = available_w / ink_extent.width;
        else
            factor = 1.0;
        cairo_move_to (cr,
                       x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                       y + ((h * scale + 0.5) - factor * log_extent.height) / 2 + 18);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
        pango_cairo_show_layout (cr, display_state);
        g_object_unref (display_state);
    }

    /* Show display number in the left bottom corner if there's more than 1*/
    if (xfce_randr->noutput > 1)
    {
        PangoLayout *display_number;
        gchar *display_num;


        display_num = g_strdup_printf ("%d", i + 1);
        display_number = gtk_widget_create_pango_layout (GTK_WIDGET (randr_gui_area), display_num);
        layout_set_font (display_number, "Mono Bold 9");
        pango_layout_get_pixel_extents (display_number, &ink_extent, &log_extent);

        available_w = w * scale + 0.5 - 6;
        if (available_w < ink_extent.width)
            factor = available_w / ink_extent.width;
        else
            factor = 1.0;

        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.4);
        cairo_arc (cr,
                   x + (w * scale + 0.5) / 2,
                   y + ((h * scale + 0.5)) - (factor * log_extent.height / 2) - 3.5,
                   factor * log_extent.height / 2 + 2.5, 0.0, 2 * M_PI);
        cairo_fill (cr);
        cairo_move_to (cr,
                       x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                       y + ((h * scale + 0.5) - factor * log_extent.height) - 3.5);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
        pango_cairo_show_layout (cr, display_number);
        g_object_unref (display_number);
        g_free (display_num);
    }

    cairo_restore (cr);

    if (pat_lin)
        cairo_pattern_destroy (pat_lin);
    if (pat_radial)
        cairo_pattern_destroy (pat_radial);

    g_object_unref (layout);
}

static void
on_area_paint (FooScrollArea  *area,
               cairo_t        *cr,
               gpointer        data)
{
    GList *connected_outputs = NULL;
    GList *list;
    double x = 0.0, y = 0.0;
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (area));

    paint_background (area, cr);

    connected_outputs = list_connected_outputs (NULL, NULL);

    for (list = connected_outputs; list != NULL; list = list->next)
    {
        gint i;

        i = g_list_position (connected_outputs, list);
        /* Always paint the currently selected display last, i.e. on top, so it's
           visible and the name is readable */
        if (i >= 0 && (guint)i == active_output) {
            continue;
        }
        paint_output (cr, i, scale_factor, &x, &y);

        if (get_mirrored_configuration () == 1)
            break;
    }
    /* Finally also paint the active output */
    paint_output (cr, active_output, scale_factor, &x, &y);
}

static XfceOutputInfo *
get_nearest_output (gint x, gint y)
{
    int nearest_index;
    guint nearest_dist;
    guint m;

    nearest_index = -1;
    nearest_dist = G_MAXINT;

    /* Walk all the connected outputs */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        XfceOutputInfo *output;
        guint dist_x, dist_y;

        output = convert_xfce_output_info (m);

        if (!(output->connected && output->on))
            continue;

        if (x < output->x)
            dist_x = output->x - x;
        else if (x >= output->x + (gint)output->width)
            dist_x = x - (output->x + output->width) + 1;
        else
            dist_x = 0;

        if (y < output->y)
            dist_y = output->y - y;
        else if (y >= output->y + (gint)output->height)
            dist_y = y - (output->y + output->height) + 1;
        else
            dist_y = 0;

        if (MIN (dist_x, dist_y) < nearest_dist)
        {
            nearest_dist = MIN (dist_x, dist_y);
            nearest_index = m;
        }
    }

    if (nearest_index != -1)
        return convert_xfce_output_info (nearest_index);
    else
        return NULL;
}

/* Gets the output that contains the largest intersection with the window.
 * Logic stolen from gdk_screen_get_monitor_at_window().
 */
static XfceOutputInfo *
get_output_for_window (GdkWindow *window)
{
    GdkRectangle win_rect;
    int largest_area;
    int largest_index;
    guint m;

    gdk_window_get_geometry (window, &win_rect.x, &win_rect.y, &win_rect.width, &win_rect.height);
    gdk_window_get_origin (window, &win_rect.x, &win_rect.y);

    largest_area = 0;
    largest_index = -1;

    /* Walk all the connected outputs */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        XfceOutputInfo *output;
        GdkRectangle output_rect, intersection;

        output = convert_xfce_output_info (m);

        output_rect.x      = output->x;
        output_rect.y      = output->y;
        output_rect.width  = output->width;
        output_rect.height = output->height;

        if (xfce_randr->mode[m] != None)
        {
            if (gdk_rectangle_intersect (&win_rect, &output_rect, &intersection))
            {
                int area;

                area = intersection.width * intersection.height;
                if (area > largest_area)
                {
                    largest_area = area;
                    largest_index = m;
                }
            }
        }
    }

    if (largest_index != -1)
        return convert_xfce_output_info (largest_index);
    else
        return get_nearest_output ( win_rect.x + win_rect.width / 2,
                                    win_rect.y + win_rect.height / 2);
}

/* We select the current output, i.e. select the one being edited, based on
 * which output is showing the configuration dialog.
 */
static void
select_current_output_from_dialog_position (App *app)
{
    if (gtk_widget_get_realized (app->dialog))
        app->current_output = get_output_for_window (gtk_widget_get_window (app->dialog));
    else
        app->current_output = NULL;
}

/* This is a GtkWidget::map-event handler.  We wait for the display-properties
 * dialog to be mapped, and then we select the output which corresponds to the
 * monitor on which the dialog is being shown.
 */
static gboolean
dialog_map_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    App *app = data;

    select_current_output_from_dialog_position (app);
    return FALSE;
}

static GtkWidget*
_gtk_builder_get_widget (GtkBuilder *builder, const gchar *name)
{
    return GTK_WIDGET (gtk_builder_get_object (builder, name));
}
/* Xfce RANDR GUI */

static void
display_settings_show_main_dialog (GdkDisplay *display)
{
    GtkBuilder  *builder;
    GtkWidget   *dialog, *plug;
    GObject     *plug_child;
    GError      *error = NULL;
    GtkWidget   *gui_container;
    App *app;

    /* Load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, display_dialog_ui,
                                     display_dialog_ui_length, &error) != 0)
    {
        /* Build the dialog */
        dialog = display_settings_dialog_new (builder);

        /* Set up notifications */
        XRRSelectInput (gdk_x11_display_get_xdisplay (display),
                        GDK_WINDOW_XID (gdk_get_default_root_window ()),
                        RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (display,
                                              randr_event_base,
                                              RRNotify + 1);
        gdk_window_add_filter (gdk_get_default_root_window (), screen_on_event, builder);

        app = g_new0 (App, 1);

        initialize_connected_outputs();

        app->dialog = _gtk_builder_get_widget (builder, "display-dialog");
        g_signal_connect_after (app->dialog, "map-event",
                    G_CALLBACK (dialog_map_event_cb), app);

        /* Scroll Area */
        randr_gui_area = (GtkWidget *)foo_scroll_area_new ();
        randr_outputs_combobox = _gtk_builder_get_widget (builder, "randr-outputs");

        g_object_set_data (G_OBJECT (randr_gui_area), "app", app);

        set_monitors_tooltip (NULL);

        /* FIXME: this should be computed dynamically */
        foo_scroll_area_set_min_size (FOO_SCROLL_AREA (randr_gui_area), -1, 200);
        gtk_widget_show (randr_gui_area);
        g_signal_connect (randr_gui_area, "paint",
                  G_CALLBACK (on_area_paint), app);
        g_signal_connect (randr_gui_area, "viewport_changed",
                  G_CALLBACK (on_viewport_changed), app);

        gui_container = GTK_WIDGET (gtk_builder_get_object (builder, "randr-dnd"));
        gtk_container_add (GTK_CONTAINER (gui_container), GTK_WIDGET (randr_gui_area));
        gtk_widget_show_all (gui_container);

        /* Keep track of the profile that was active when the dialog was launched */
        active_profile = xfconf_channel_get_string (display_channel, "/ActiveProfile", "Default");

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            g_signal_connect (G_OBJECT (dialog), "response",
                              G_CALLBACK (display_settings_dialog_response), builder);
            g_signal_connect (G_OBJECT (dialog), "destroy",
                              G_CALLBACK (gtk_main_quit), builder);
            /* Show the dialog */
            gtk_window_present (GTK_WINDOW (dialog));
        }
        else
        {
            /* Create plug widget */
            plug = gtk_plug_new (opt_socket_id);
            g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
            gtk_widget_show (plug);

            /* Get plug child widget */
            plug_child = gtk_builder_get_object (builder, "plug-child");
            xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));
        }

        /* To prevent the settings dialog to be saved in the session */
        gdk_x11_set_sm_client_id ("FAKE ID");

        /* Enter the main loop */
        gtk_main ();

        gtk_widget_destroy (dialog);
        g_free (app);
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    gdk_window_remove_filter (gdk_get_default_root_window (), screen_on_event, builder);

    /* Release the builder */
    g_object_unref (G_OBJECT (builder));
}

static gboolean
display_settings_minimal_dialog_key_press_event(GtkWidget *widget,
                                                GdkEventKey *event,
                                                gpointer user_data)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

static void
display_settings_minimal_advanced_clicked (GtkButton  *button,
                                           GtkBuilder *builder)
{
    GtkWidget *dialog;

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
    gtk_widget_hide (dialog);

    display_settings_show_main_dialog (gdk_display_get_default ());

    gtk_main_quit ();
}

static void
display_settings_minimal_get_positions (GtkWidget    *dialog,
                                        GdkRectangle *monitor_rect,
                                        GdkRectangle *window_rect)
{
    GdkDisplay *display;
    GdkSeat    *seat;
    GdkMonitor *monitor;
    gint cursorx, cursory;

    display = gdk_display_get_default ();
    seat = gdk_display_get_default_seat (display);
    gdk_window_get_device_position (gdk_get_default_root_window (),
                                    gdk_seat_get_pointer (seat),
                                    &cursorx, &cursory, NULL);

    monitor = gdk_display_get_monitor_at_point (display, cursorx, cursory);
    gdk_monitor_get_geometry (monitor, monitor_rect);
    gtk_window_get_position (GTK_WINDOW (dialog), &window_rect->x, &window_rect->y);
    gtk_window_get_size (GTK_WINDOW (dialog), &window_rect->width, &window_rect->height);
}

static gboolean
display_settings_minimal_center (gpointer user_data)
{
    GdkRectangle monitor_rect, window_rect;
    GtkWidget *dialog = user_data;

    display_settings_minimal_get_positions (dialog, &monitor_rect, &window_rect);

    gtk_window_move (GTK_WINDOW (dialog),
                     monitor_rect.x + monitor_rect.width / 2 - window_rect.width / 2,
                     monitor_rect.y + monitor_rect.height / 2 - window_rect.height / 2);

    return FALSE;
}

static void
display_settings_minimal_cycle (GtkWidget  *dialog,
                                GtkBuilder *builder)
{
    GtkToggleButton *only_display1, *mirror_displays, *extend_right, *only_display2;

    only_display1 = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "display1"));
    mirror_displays = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "mirror"));
    extend_right = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "extend_right"));
    only_display2 = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "display2"));

    if (gtk_toggle_button_get_active (only_display1))
        if (gtk_widget_get_sensitive (GTK_WIDGET (mirror_displays)))
            gtk_toggle_button_set_active (mirror_displays, TRUE);
        else
            gtk_toggle_button_set_active (extend_right, TRUE);
    else if (gtk_toggle_button_get_active (mirror_displays))
        gtk_toggle_button_set_active (extend_right, TRUE);
    else if (gtk_toggle_button_get_active (extend_right))
        gtk_toggle_button_set_active (only_display2, TRUE);
    else
        gtk_toggle_button_set_active (only_display1, TRUE);

    g_timeout_add_seconds (1, display_settings_minimal_center, dialog);
}

static void
display_settings_minimal_activated (GApplication *application,
                                    gpointer      user_data)
{
    GtkBuilder *builder = user_data;
    GtkWidget  *dialog;
    GdkRectangle monitor_rect, window_rect;

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
    display_settings_minimal_get_positions (dialog, &monitor_rect, &window_rect);

    /* Check if dialog is already at current monitor (where cursor is at) */
    if (gdk_rectangle_intersect (&monitor_rect, &window_rect, NULL))
    {
        /* Select next preset if dialog is already at current monitor */
        display_settings_minimal_cycle (dialog, builder);
    }
    else
    {
        /* Center at current monitor if displayed elsewhere */
        display_settings_minimal_center (dialog);
    }

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
display_settings_minimal_load_icon (GtkBuilder  *builder,
                                    const gchar *img_name,
                                    const gchar *icon_name)
{
    GObject      *dialog;
    GtkImage     *img;
    GtkIconTheme *icon_theme;
    GdkPixbuf    *icon;
    cairo_surface_t *surface = NULL;
    gint scale_factor;

    dialog = gtk_builder_get_object (builder, "dialog");
    img = GTK_IMAGE (gtk_builder_get_object (builder, img_name));
    g_return_if_fail (dialog && img);
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (dialog));

    icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (dialog)));
    icon = gtk_icon_theme_load_icon_for_scale (icon_theme, icon_name, 128, scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    if (G_LIKELY (icon != NULL))
    {
        surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, NULL);
        g_object_unref (icon);
    }
    gtk_image_set_from_surface (GTK_IMAGE (img), surface);

    if (G_LIKELY (surface != NULL))
        cairo_surface_destroy (surface);
}

static void
display_settings_show_minimal_dialog (GdkDisplay *display)
{
    GtkBuilder *builder;
    GtkWidget  *dialog, *cancel;
    GObject    *only_display1, *only_display2, *mirror_displays, *mirror_displays_label;
    GObject    *extend_right, *advanced, *fake_button, *label, *profile_box;
    GError     *error = NULL;
    gboolean    found = FALSE;
    RRMode     *clonable_modes;
    GtkApplication *app;

    builder = gtk_builder_new ();

    if (gtk_builder_add_from_string (builder, minimal_display_dialog_ui,
                                     minimal_display_dialog_ui_length, &error) != 0)
    {
        gchar *only_display1_label, *only_display2_label;

        /* Build the minimal dialog */
        dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        cancel = GTK_WIDGET (gtk_builder_get_object (builder, "cancel_button"));

        g_signal_connect (dialog, "key-press-event", G_CALLBACK (display_settings_minimal_dialog_key_press_event), NULL);
        g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (cancel, "clicked", G_CALLBACK (gtk_main_quit), NULL);

        display_settings_minimal_load_icon (builder, "image1", "xfce-display-internal");
        display_settings_minimal_load_icon (builder, "image2", "xfce-display-mirror");
        display_settings_minimal_load_icon (builder, "image3", "xfce-display-extend");
        display_settings_minimal_load_icon (builder, "image4", "xfce-display-external");

        only_display1 = gtk_builder_get_object (builder, "display1");
        mirror_displays = gtk_builder_get_object (builder, "mirror");
        mirror_displays_label = gtk_builder_get_object (builder, "label2");
        extend_right = gtk_builder_get_object (builder, "extend_right");
        only_display2 = gtk_builder_get_object (builder, "display2");
        advanced = gtk_builder_get_object (builder, "advanced_button");
        fake_button = gtk_builder_get_object (builder, "fake_button");

        /* Create the profile radiobuttons */
        display_settings_minimal_profile_populate (builder);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fake_button), TRUE);

        label = gtk_builder_get_object (builder, "label1");
        only_display1_label = g_strdup_printf (_("Only %s (1)"), xfce_randr->friendly_name[0]);
        gtk_label_set_text (GTK_LABEL (label), only_display1_label);
        gtk_widget_set_tooltip_text (GTK_WIDGET (label), only_display1_label);
        g_free (only_display1_label);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display1),
                                      xfce_randr->mode[0] != None);

        if (xfce_randr->noutput > 1)
        {
            label = gtk_builder_get_object (builder, "label4");
            only_display2_label = g_strdup_printf (_("Only %s (2)"), xfce_randr->friendly_name[1]);
            gtk_label_set_text (GTK_LABEL (label), only_display2_label);
            gtk_widget_set_tooltip_text (GTK_WIDGET (label), only_display2_label);
            g_free (only_display2_label);
            /* Can outputs be cloned? */
            if (display_settings_get_n_active_outputs () > 1)
                clonable_modes = xfce_randr_clonable_modes (xfce_randr);
            else
                clonable_modes = NULL;

            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays), clonable_modes != NULL);
            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays_label), clonable_modes != NULL);
            g_free (clonable_modes);

            if (xfce_randr->mode[0] != None)
            {
                if (xfce_randr->mode[1] != None)
                {
                    /* Check for mirror */
                    if (xfce_randr->mirrored[0] && xfce_randr->mirrored[1])
                    {
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mirror_displays), TRUE);
                        found = TRUE;
                    }
                    /* Check for Extend Right */
                    if (!found && (gint)xfce_randr->position[1].x == (gint)xfce_randr->position[0].x + (gint)xfce_randr_mode_width(xfce_randr_find_mode_by_id (xfce_randr, 0, xfce_randr->mode[0]), 0))
                    {
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (extend_right), TRUE);
                        found = TRUE;
                    }
                }
                /* Toggle Primary */
                if (!found)
                {
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display1), TRUE);
                }
            }
            /* Toggle Secondary */
            else
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display2), TRUE);
            }
        }
        else
        {
            /* Only one output, disable other buttons */
            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (extend_right), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (only_display2), FALSE);
        }

        /* Initialize application to ensure single instance */
        app = gtk_application_new ("org.xfce.display.settings", 0);

        g_application_register (G_APPLICATION (app), NULL, &error);
        if (error != NULL)
          {
            g_warning ("Unable to register GApplication: %s", error->message);
            g_error_free (error);
            error = NULL;
          }

        if (g_application_get_is_remote (G_APPLICATION (app)))
          {
            g_application_activate (G_APPLICATION (app));
            g_object_unref (app);
            return;
          }

        g_signal_connect (only_display1, "toggled", G_CALLBACK (display_settings_minimal_only_display1_toggled),
                          builder);
        g_signal_connect (mirror_displays, "toggled", G_CALLBACK (display_settings_minimal_mirror_displays_toggled),
                          builder);
        g_signal_connect (extend_right, "toggled", G_CALLBACK (display_settings_minimal_extend_right_toggled),
                          builder);
        g_signal_connect (only_display2, "toggled", G_CALLBACK (display_settings_minimal_only_display2_toggled),
                          builder);
        g_signal_connect (advanced, "clicked", G_CALLBACK (display_settings_minimal_advanced_clicked),
                          builder);

        g_signal_connect (app, "activate", G_CALLBACK (display_settings_minimal_activated), builder);

        /* Auto-apply the first profile in the list */
        if (xfconf_channel_get_bool (display_channel, "/AutoEnableProfiles", TRUE))
        {
            /* Walz down the widget hierarchy: profile-box -> gtkbox -> gtkradiobutton */
            profile_box  = gtk_builder_get_object (builder, "profile-box");
            if (GTK_IS_CONTAINER (profile_box))
            {
                GList *children = NULL;
                GList *first_profile_box;

                children = gtk_container_get_children (GTK_CONTAINER (profile_box));
                first_profile_box = g_list_first (children);
                if (first_profile_box)
                {
                    GList *grand_children = NULL;
                    GList *current;
                    GtkWidget *box = GTK_WIDGET (first_profile_box->data);

                    grand_children = gtk_container_get_children (GTK_CONTAINER (box));
                    current = g_list_first (grand_children);
                    if (current)
                    {
                        GtkWidget* widget = GTK_WIDGET (grand_children->data);

                        if (GTK_IS_TOGGLE_BUTTON (widget))
                        {
                            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
                        }
                    }
                }
            }
        }

        /* Show the minimal dialog and start the main loop */
        gtk_window_present (GTK_WINDOW (dialog));
        gtk_main ();
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    g_object_unref (G_OBJECT (builder));
}


gint
main (gint argc, gchar **argv)
{
    GdkDisplay  *display;
    GError      *error = NULL;
    gboolean     succeeded = TRUE;
    gint         error_base;
    gchar       *command;
    const gchar *alternative = NULL;
    const gchar *alternative_icon = NULL;
    gint         response;
    guint        i = 0;

    /* Setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* Initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* Print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* Cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* Print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2023");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* Get the default display */
    display = gdk_display_get_default ();

    /* Check if the randr extension is avaible on the system */
    if (!XRRQueryExtension (gdk_x11_display_get_xdisplay (display), &randr_event_base, &error_base))
    {
        g_set_error (&error, 0, 0, _("Unable to query the version of the RandR extension being used"));
        xfce_dialog_show_error (NULL, error, _("Unable to start the Xfce Display Settings"));
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* Initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* Print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* Open the xsettings channel */
    display_channel = xfconf_channel_new ("displays");
    if (G_LIKELY (display_channel))
    {
        /* Create a new xfce randr (>= 1.2) for this display
         * this will only work if there is 1 screen on this display
         * As GTK 3.10, the number of screens is always 1 */
        xfce_randr = xfce_randr_new (display, &error);

        if (!xfce_randr)
        {
            succeeded = FALSE;
            command = g_find_program_in_path ("amdcccle");

            if (command != NULL)
            {
                alternative = _("ATI Settings");
                alternative_icon = "ccc_small";
            }

            response = xfce_message_dialog (NULL, NULL, "dialog-error",
                                            _("Unable to start the Xfce Display Settings"),
                                            error ? error->message : NULL,
                                            _("_Close"), GTK_RESPONSE_CLOSE,
                                            alternative != NULL ?XFCE_BUTTON_TYPE_MIXED : NULL,
                                            alternative_icon, alternative, GTK_RESPONSE_OK, NULL);
            g_clear_error (&error);

            if (response == GTK_RESPONSE_OK
                && !g_spawn_command_line_async (command, &error))
            {
                xfce_dialog_show_error (NULL, error, _("Unable to launch the proprietary driver settings"));
                g_error_free (error);
            }

            g_free (command);

            goto cleanup;
        }

        /* Hook to make sure the libxfce4ui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
        {
            succeeded = FALSE;
            goto cleanup;
        }

        /* Store a Fallback of the current settings */
        for (i = 0; i < xfce_randr->noutput; i++)
            xfce_randr_save_output (xfce_randr, "Fallback", display_channel, i);

        if (xfce_randr->noutput <= 1 || !minimal)
            display_settings_show_main_dialog (display);
        else
            display_settings_show_minimal_dialog (display);

cleanup:
        /* Release the channel */
        g_object_unref (G_OBJECT (display_channel));
    }

    /* Free the randr 1.2 backend */
    if (xfce_randr)
        xfce_randr_free (xfce_randr);

    /* Shutdown xfconf */
    xfconf_shutdown ();

    return (succeeded ? EXIT_SUCCESS : EXIT_FAILURE);
}
