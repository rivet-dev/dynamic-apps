/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar-sbr/thunar-sbr-remove-renamer.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_END_OFFSET,
  PROP_END_OFFSET_MODE,
  PROP_START_OFFSET,
  PROP_START_OFFSET_MODE,
};



static void   thunar_sbr_remove_renamer_get_property  (GObject                      *object,
                                                       guint                         prop_id,
                                                       GValue                       *value,
                                                       GParamSpec                   *pspec);
static void   thunar_sbr_remove_renamer_set_property  (GObject                      *object,
                                                       guint                         prop_id,
                                                       const GValue                 *value,
                                                       GParamSpec                   *pspec);
static void   thunar_sbr_remove_renamer_realize       (GtkWidget                    *widget);
static gchar *thunar_sbr_remove_renamer_process       (ThunarxRenamer               *renamer,
                                                       ThunarxFileInfo              *file,
                                                       const gchar                  *text,
                                                       guint                         idx);
static void   thunar_sbr_remove_renamer_update        (ThunarSbrRemoveRenamer       *remove_renamer);



struct _ThunarSbrRemoveRenamerClass
{
  ThunarxRenamerClass __parent__;
};

struct _ThunarSbrRemoveRenamer
{
  ThunarxRenamer      __parent__;
  GtkWidget          *end_spinner;
  GtkWidget          *start_spinner;
  guint               end_offset;
  ThunarSbrOffsetMode end_offset_mode;
  guint               start_offset;
  ThunarSbrOffsetMode start_offset_mode;
};



THUNARX_DEFINE_TYPE (ThunarSbrRemoveRenamer, thunar_sbr_remove_renamer, THUNARX_TYPE_RENAMER);



static void
thunar_sbr_remove_renamer_class_init (ThunarSbrRemoveRenamerClass *klass)
{
  ThunarxRenamerClass *thunarxrenamer_class;
  GtkWidgetClass      *gtkwidget_class;
  GObjectClass        *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = thunar_sbr_remove_renamer_get_property;
  gobject_class->set_property = thunar_sbr_remove_renamer_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_sbr_remove_renamer_realize;

  thunarxrenamer_class = THUNARX_RENAMER_CLASS (klass);
  thunarxrenamer_class->process = thunar_sbr_remove_renamer_process;

  /**
   * ThunarSbrRemoveRenamer:end-offset:
   *
   * The end offset for the character removal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_END_OFFSET,
                                   g_param_spec_uint ("end-offset",
                                                      "end-offset",
                                                      "end-offset",
                                                      0, G_MAXUINT, 1,
                                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  /**
   * ThunarSbrRemoveRenamer:end-offset-mode:
   *
   * The end offset mode for the character removal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_END_OFFSET_MODE,
                                   g_param_spec_enum ("end-offset-mode",
                                                      "end-offset-mode",
                                                      "end-offset-mode",
                                                      THUNAR_SBR_TYPE_OFFSET_MODE,
                                                      THUNAR_SBR_OFFSET_MODE_LEFT,
                                                      G_PARAM_READWRITE));

  /**
   * ThunarSbrRemoveRenamer:start-offset:
   *
   * The start offset for the character removal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_START_OFFSET,
                                   g_param_spec_uint ("start-offset",
                                                      "start-offset",
                                                      "start-offset",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE));

  /**
   * ThunarSbrRemoveRenamer:start-offset-mode:
   *
   * The start offset mode for the character removal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_START_OFFSET_MODE,
                                   g_param_spec_enum ("start-offset-mode",
                                                      "start-offset-mode",
                                                      "start-offset-mode",
                                                      THUNAR_SBR_TYPE_OFFSET_MODE,
                                                      THUNAR_SBR_OFFSET_MODE_LEFT,
                                                      G_PARAM_READWRITE));
}



static void
thunar_sbr_remove_renamer_init (ThunarSbrRemoveRenamer *remove_renamer)
{
  AtkRelationSet *relations;
  GtkAdjustment  *adjustment;
  AtkRelation    *relation;
  GEnumClass     *klass;
  AtkObject      *object;
  GtkWidget      *combo;
  GtkWidget      *label;
  GtkWidget      *grid;
  guint           n;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_box_pack_start (GTK_BOX (remove_renamer), grid, TRUE, TRUE, 0);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("Remove _From Position:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  remove_renamer->start_spinner = gtk_spin_button_new_with_range (0u, G_MAXUINT, 1u);
  gtk_entry_set_width_chars (GTK_ENTRY (remove_renamer->start_spinner), 4);
  gtk_entry_set_alignment (GTK_ENTRY (remove_renamer->start_spinner), 1.0f);
  gtk_entry_set_activates_default (GTK_ENTRY (remove_renamer->start_spinner), TRUE);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (remove_renamer->start_spinner), 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (remove_renamer->start_spinner), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (remove_renamer->start_spinner), TRUE);
  gtk_grid_attach (GTK_GRID (grid), remove_renamer->start_spinner, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), remove_renamer->start_spinner);
  gtk_widget_show (remove_renamer->start_spinner);

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (remove_renamer->start_spinner));
  g_object_bind_property (G_OBJECT (remove_renamer), "start-offset", G_OBJECT (adjustment), "value", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (remove_renamer->start_spinner);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  combo = gtk_combo_box_text_new ();
  klass = g_type_class_ref (THUNAR_SBR_TYPE_OFFSET_MODE);
  for (n = 0; n < klass->n_values; ++n)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(klass->values[n].value_nick));
  g_object_bind_property (G_OBJECT (remove_renamer), "start-offset-mode", G_OBJECT (combo), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), combo, 2, 0, 1, 1);
  g_type_class_unref (klass);
  gtk_widget_show (combo);

  label = gtk_label_new_with_mnemonic (_("_To Position:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  remove_renamer->end_spinner = gtk_spin_button_new_with_range (0u, G_MAXUINT, 1u);
  gtk_entry_set_width_chars (GTK_ENTRY (remove_renamer->end_spinner), 4);
  gtk_entry_set_alignment (GTK_ENTRY (remove_renamer->end_spinner), 1.0f);
  gtk_entry_set_activates_default (GTK_ENTRY (remove_renamer->end_spinner), TRUE);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (remove_renamer->end_spinner), 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (remove_renamer->end_spinner), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (remove_renamer->end_spinner), TRUE);
  gtk_grid_attach (GTK_GRID (grid), remove_renamer->end_spinner, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), remove_renamer->end_spinner);
  gtk_widget_show (remove_renamer->end_spinner);

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (remove_renamer->end_spinner));
  g_object_bind_property (G_OBJECT (remove_renamer), "end-offset", G_OBJECT (adjustment), "value", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (remove_renamer->end_spinner);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  combo = gtk_combo_box_text_new ();
  klass = g_type_class_ref (THUNAR_SBR_TYPE_OFFSET_MODE);
  for (n = 0; n < klass->n_values; ++n)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(klass->values[n].value_nick));
  g_object_bind_property (G_OBJECT (remove_renamer), "end-offset-mode", G_OBJECT (combo), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), combo, 2, 1, 1, 1);
  g_type_class_unref (klass);
  gtk_widget_show (combo);
}



static void
thunar_sbr_remove_renamer_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ThunarSbrRemoveRenamer *remove_renamer = THUNAR_SBR_REMOVE_RENAMER (object);

  switch (prop_id)
    {
    case PROP_END_OFFSET:
      g_value_set_uint (value, thunar_sbr_remove_renamer_get_end_offset (remove_renamer));
      break;

    case PROP_END_OFFSET_MODE:
      g_value_set_enum (value, thunar_sbr_remove_renamer_get_end_offset_mode (remove_renamer));
      break;

    case PROP_START_OFFSET:
      g_value_set_uint (value, thunar_sbr_remove_renamer_get_start_offset (remove_renamer));
      break;

    case PROP_START_OFFSET_MODE:
      g_value_set_enum (value, thunar_sbr_remove_renamer_get_start_offset_mode (remove_renamer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_sbr_remove_renamer_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ThunarSbrRemoveRenamer *remove_renamer = THUNAR_SBR_REMOVE_RENAMER (object);

  switch (prop_id)
    {
    case PROP_END_OFFSET:
      thunar_sbr_remove_renamer_set_end_offset (remove_renamer, g_value_get_uint (value));
      break;

    case PROP_END_OFFSET_MODE:
      thunar_sbr_remove_renamer_set_end_offset_mode (remove_renamer, g_value_get_enum (value));
      break;

    case PROP_START_OFFSET:
      thunar_sbr_remove_renamer_set_start_offset (remove_renamer, g_value_get_uint (value));
      break;

    case PROP_START_OFFSET_MODE:
      thunar_sbr_remove_renamer_set_start_offset_mode (remove_renamer, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_sbr_remove_renamer_realize (GtkWidget *widget)
{
  /* realize the renamer widget */
  (*GTK_WIDGET_CLASS (thunar_sbr_remove_renamer_parent_class)->realize) (widget);

  /* update the renamer state */
  thunar_sbr_remove_renamer_update (THUNAR_SBR_REMOVE_RENAMER (widget));
}



static gchar*
thunar_sbr_remove_renamer_process (ThunarxRenamer  *renamer,
                                   ThunarxFileInfo *file,
                                   const gchar     *text,
                                   guint            idx)
{
  ThunarSbrRemoveRenamer *remove_renamer = THUNAR_SBR_REMOVE_RENAMER (renamer);
  const gchar            *start_pointer;
  const gchar            *end_pointer;
  GString                *result;
  guint                   start_offset;
  guint                   text_length;
  guint                   end_offset;

  /* determine the text length */
  text_length = g_utf8_strlen (text, -1);

  /* determine the offsets for this file name */
  end_offset = (remove_renamer->end_offset_mode == THUNAR_SBR_OFFSET_MODE_LEFT)
             ? remove_renamer->end_offset : (text_length - remove_renamer->end_offset);
  start_offset = (remove_renamer->start_offset_mode == THUNAR_SBR_OFFSET_MODE_LEFT)
               ? remove_renamer->start_offset : (text_length - remove_renamer->start_offset);

  /* check if anything should be removed */
  if (G_UNLIKELY (start_offset >= end_offset || end_offset > text_length))
    return g_strdup (text);

  /* determine start and end pointers */
  end_pointer = g_utf8_offset_to_pointer (text, end_offset);
  start_pointer = g_utf8_offset_to_pointer (text, start_offset);

  /* allocate the resulting string */
  result = g_string_sized_new (text_length);
  g_string_append_len (result, text, start_pointer - text);
  g_string_append (result, end_pointer);
  return g_string_free (result, FALSE);
}



static void
thunar_sbr_remove_renamer_update (ThunarSbrRemoveRenamer *remove_renamer)
{
  guint start_offset;
  guint end_offset;

  /* check if the renamer is realized */
  if (gtk_widget_get_realized (GTK_WIDGET (remove_renamer)))
    {
      /* check if start and end offset make sense */
      end_offset = (remove_renamer->end_offset_mode == THUNAR_SBR_OFFSET_MODE_LEFT)
                 ? remove_renamer->end_offset : (G_MAXUINT - remove_renamer->end_offset);
      start_offset = (remove_renamer->start_offset_mode == THUNAR_SBR_OFFSET_MODE_LEFT)
                   ? remove_renamer->start_offset : (G_MAXUINT - remove_renamer->start_offset);

      /* highlight invalid input by using theme specific colors */
      if (G_UNLIKELY (start_offset >= end_offset))
        {
          gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (remove_renamer->start_spinner)), "error");
          gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (remove_renamer->end_spinner)), "error");
        }
      else
        {
          gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (remove_renamer->start_spinner)), "error");
          gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (remove_renamer->end_spinner)), "error");
        }
    }

  /* tell the bulk renamer that we have a new state */
  thunarx_renamer_changed (THUNARX_RENAMER (remove_renamer));
}



/**
 * thunar_sbr_remove_renamer_new:
 *
 * Allocates a new #ThunarSbrRemoveRenamer instance.
 *
 * Return value: the newly allocated #ThunarSbrRemoveRenamer.
 **/
ThunarSbrRemoveRenamer*
thunar_sbr_remove_renamer_new (void)
{
  return g_object_new (THUNAR_SBR_TYPE_REMOVE_RENAMER,
                       "name", _("Remove Characters"),
                       NULL);
}



/**
 * thunar_sbr_remove_renamer_get_end_offset:
 * @remove_renamer : a #ThunarSbrRemoveRenamer.
 *
 * Returns the end offset of the character removal.
 *
 * Return value: the end offset for @remove_renamer.
 **/
guint
thunar_sbr_remove_renamer_get_end_offset (ThunarSbrRemoveRenamer *remove_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer), 0);
  return remove_renamer->end_offset;
}



/**
 * thunar_sbr_remove_renamer_set_end_offset:
 * @remove_renamer : a #ThunarSbrRemoveRenamer.
 * @end_offset     : the new end offset.
 *
 * Sets the end offset of @remove_renamer to @end_offset.
 **/
void
thunar_sbr_remove_renamer_set_end_offset (ThunarSbrRemoveRenamer *remove_renamer,
                                          guint                   end_offset)
{
  g_return_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer));

  /* check if we have a new value */
  if (G_LIKELY (remove_renamer->end_offset != end_offset))
    {
      /* apply the new setting */
      remove_renamer->end_offset = end_offset;

      /* update the renamer */
      thunar_sbr_remove_renamer_update (remove_renamer);

      /* notify listeners */
      g_object_notify (G_OBJECT (remove_renamer), "end-offset");
    }
}



/**
 * thunar_sbr_remove_renamer_get_end_offset_mode:
 * @remove_renamer : a #ThunarSbrRemoveRenamer.
 *
 * Returns the end offset mode for @remove_renamer.
 *
 * Return value: the end offset mode for @remove_renamer.
 **/
ThunarSbrOffsetMode
thunar_sbr_remove_renamer_get_end_offset_mode (ThunarSbrRemoveRenamer *remove_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer), THUNAR_SBR_OFFSET_MODE_LEFT);
  return remove_renamer->end_offset_mode;
}



/**
 * thunar_sbr_remove_renamer_set_end_offset_mode:
 * @remove_renamer  : a #ThunarSbrRemoveRenamer.
 * @end_offset_mode : the new end #ThunarSbrOffsetMode.
 *
 * Sets the end offset mode for @remove_renamer to @end_offset_mode.
 **/
void
thunar_sbr_remove_renamer_set_end_offset_mode (ThunarSbrRemoveRenamer *remove_renamer,
                                               ThunarSbrOffsetMode     end_offset_mode)
{
  g_return_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer));

  /* check if we have a new value */
  if (G_LIKELY (remove_renamer->end_offset_mode != end_offset_mode))
    {
      /* apply the new setting */
      remove_renamer->end_offset_mode = end_offset_mode;

      /* update the renamer */
      thunar_sbr_remove_renamer_update (remove_renamer);

      /* notify listeners */
      g_object_notify (G_OBJECT (remove_renamer), "end-offset-mode");
    }
}



/**
 * thunar_sbr_remove_renamer_get_start_offset:
 * @remove_renamer : a #ThunarSbrRemoveRenamer.
 *
 * Returns the start offset of the character removal.
 *
 * Return value: the start offset for @remove_renamer.
 **/
guint
thunar_sbr_remove_renamer_get_start_offset (ThunarSbrRemoveRenamer *remove_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer), 0);
  return remove_renamer->start_offset;
}



/**
 * thunar_sbr_remove_renamer_set_start_offset:
 * @remove_renamer : a #ThunarSbrRemoveRenamer.
 * @start_offset   : the new start offset.
 *
 * Sets the start offset of @remove_renamer to @start_offset.
 **/
void
thunar_sbr_remove_renamer_set_start_offset (ThunarSbrRemoveRenamer *remove_renamer,
                                            guint                   start_offset)
{
  g_return_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer));

  /* check if we have a new value */
  if (G_LIKELY (remove_renamer->start_offset != start_offset))
    {
      /* apply the new setting */
      remove_renamer->start_offset = start_offset;

      /* update the renamer */
      thunar_sbr_remove_renamer_update (remove_renamer);

      /* notify listeners */
      g_object_notify (G_OBJECT (remove_renamer), "start-offset");
    }
}



/**
 * thunar_sbr_remove_renamer_get_start_offset_mode:
 * @remove_renamer : a #ThunarSbrRemoveRenamer.
 *
 * Returns the start offset mode for @remove_renamer.
 *
 * Return value: the start offset mode for @remove_renamer.
 **/
ThunarSbrOffsetMode
thunar_sbr_remove_renamer_get_start_offset_mode (ThunarSbrRemoveRenamer *remove_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer), THUNAR_SBR_OFFSET_MODE_LEFT);
  return remove_renamer->start_offset_mode;
}



/**
 * thunar_sbr_remove_renamer_set_start_offset_mode:
 * @remove_renamer    : a #ThunarSbrRemoveRenamer.
 * @start_offset_mode : the new start #ThunarSbrOffsetMode.
 *
 * Sets the start offset mode for @remove_renamer to @start_offset_mode.
 **/
void
thunar_sbr_remove_renamer_set_start_offset_mode (ThunarSbrRemoveRenamer *remove_renamer,
                                                 ThunarSbrOffsetMode     start_offset_mode)
{
  g_return_if_fail (THUNAR_SBR_IS_REMOVE_RENAMER (remove_renamer));

  /* check if we have a new value */
  if (G_LIKELY (remove_renamer->start_offset_mode != start_offset_mode))
    {
      /* apply the new setting */
      remove_renamer->start_offset_mode = start_offset_mode;

      /* update the renamer */
      thunar_sbr_remove_renamer_update (remove_renamer);

      /* notify listeners */
      g_object_notify (G_OBJECT (remove_renamer), "start-offset-mode");
    }
}


