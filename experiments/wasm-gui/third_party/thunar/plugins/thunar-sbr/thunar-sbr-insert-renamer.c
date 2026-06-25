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

#include <thunar-sbr/thunar-sbr-insert-renamer.h>

#include <libxfce4util/libxfce4util.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_MODE,
  PROP_OFFSET,
  PROP_OFFSET_MODE,
  PROP_TEXT,
};



static void   thunar_sbr_insert_renamer_finalize      (GObject                      *object);
static void   thunar_sbr_insert_renamer_get_property  (GObject                      *object,
                                                       guint                         prop_id,
                                                       GValue                       *value,
                                                       GParamSpec                   *pspec);
static void   thunar_sbr_insert_renamer_set_property  (GObject                      *object,
                                                       guint                         prop_id,
                                                       const GValue                 *value,
                                                       GParamSpec                   *pspec);
static gchar *thunar_sbr_insert_renamer_process       (ThunarxRenamer               *renamer,
                                                       ThunarxFileInfo              *file,
                                                       const gchar                  *text,
                                                       guint                         idx);



struct _ThunarSbrInsertRenamerClass
{
  ThunarxRenamerClass __parent__;
};

struct _ThunarSbrInsertRenamer
{
  ThunarxRenamer      __parent__;
  ThunarSbrInsertMode mode;
  guint               offset;
  ThunarSbrOffsetMode offset_mode;
  gchar              *text;
};



THUNARX_DEFINE_TYPE (ThunarSbrInsertRenamer, thunar_sbr_insert_renamer, THUNARX_TYPE_RENAMER);



static void
thunar_sbr_insert_renamer_class_init (ThunarSbrInsertRenamerClass *klass)
{
  ThunarxRenamerClass *thunarxrenamer_class;
  GObjectClass        *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_sbr_insert_renamer_finalize;
  gobject_class->get_property = thunar_sbr_insert_renamer_get_property;
  gobject_class->set_property = thunar_sbr_insert_renamer_set_property;

  thunarxrenamer_class = THUNARX_RENAMER_CLASS (klass);
  thunarxrenamer_class->process = thunar_sbr_insert_renamer_process;

  /**
   * ThunarSbrInsertRenamer:mode:
   *
   * The #ThunarSbrInsertMode to use.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode", "mode", "mode",
                                                      THUNAR_SBR_TYPE_INSERT_MODE,
                                                      THUNAR_SBR_INSERT_MODE_INSERT,
                                                      G_PARAM_READWRITE));

  /**
   * ThunarSbrInsertRenamer:offset:
   *
   * The starting offset at which to insert/overwrite.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_OFFSET,
                                   g_param_spec_uint ("offset",
                                                      "offset",
                                                      "offset",
                                                      0, G_MAXUINT, 1,
                                                      G_PARAM_READWRITE));

  /**
   * ThunarSbrInsertRenamer:offset-mode:
   *
   * The offset mode for the renamer.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_OFFSET_MODE,
                                   g_param_spec_enum ("offset-mode",
                                                      "offset-mode",
                                                      "offset-mode",
                                                      THUNAR_SBR_TYPE_OFFSET_MODE,
                                                      THUNAR_SBR_OFFSET_MODE_LEFT,
                                                      G_PARAM_READWRITE));

  /**
   * ThunarSbrInsertRenamer:text:
   *
   * The text to insert/overwrite.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "text",
                                                        "text",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}



static void
thunar_sbr_insert_renamer_init (ThunarSbrInsertRenamer *insert_renamer)
{
  AtkRelationSet *relations;
  GtkAdjustment  *adjustment;
  AtkRelation    *relation;
  GEnumClass     *klass;
  AtkObject      *object;
  GtkWidget      *spinner;
  GtkWidget      *combo;
  GtkWidget      *entry;
  GtkWidget      *label;
  GtkWidget      *grid;
  GtkWidget      *hbox;
  guint           n;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_box_pack_start (GTK_BOX (insert_renamer), grid, TRUE, TRUE, 0);
  gtk_widget_show (grid);

  combo = gtk_combo_box_text_new ();
  klass = g_type_class_ref (THUNAR_SBR_TYPE_INSERT_MODE);
  for (n = 0; n < klass->n_values; ++n)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(klass->values[n].value_nick));
  g_object_bind_property (G_OBJECT (insert_renamer), "mode", G_OBJECT (combo), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), combo, 0, 0, 1, 1);
  g_type_class_unref (klass);
  gtk_widget_show (combo);

  label = gtk_label_new_with_mnemonic (_("_Text:"));
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_object_bind_property (G_OBJECT (entry), "text", G_OBJECT (insert_renamer), "text", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 2, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_show (entry);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  label = gtk_label_new_with_mnemonic (_("_At position:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 2, 1);
  gtk_widget_show (label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_grid_attach (GTK_GRID (grid), hbox, 2, 1, 1, 1);
  gtk_widget_show (hbox);

  spinner = gtk_spin_button_new_with_range (0u, G_MAXUINT, 1u);
  gtk_entry_set_width_chars (GTK_ENTRY (spinner), 4);
  gtk_entry_set_alignment (GTK_ENTRY (spinner), 1.0f);
  gtk_entry_set_activates_default (GTK_ENTRY (spinner), TRUE);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinner), 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinner), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spinner), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinner);
  gtk_widget_show (spinner);

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (spinner));
  g_object_bind_property (G_OBJECT (insert_renamer), "offset", G_OBJECT (adjustment), "value", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (spinner);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  combo = gtk_combo_box_text_new ();
  klass = g_type_class_ref (THUNAR_SBR_TYPE_OFFSET_MODE);
  for (n = 0; n < klass->n_values; ++n)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(klass->values[n].value_nick));
  g_object_bind_property (G_OBJECT (insert_renamer), "offset-mode", G_OBJECT (combo), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  g_type_class_unref (klass);
  gtk_widget_show (combo);
}



static void
thunar_sbr_insert_renamer_finalize (GObject *object)
{
  ThunarSbrInsertRenamer *insert_renamer = THUNAR_SBR_INSERT_RENAMER (object);

  /* release the text */
  g_free (insert_renamer->text);

  (*G_OBJECT_CLASS (thunar_sbr_insert_renamer_parent_class)->finalize) (object);
}



static void
thunar_sbr_insert_renamer_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ThunarSbrInsertRenamer *insert_renamer = THUNAR_SBR_INSERT_RENAMER (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, thunar_sbr_insert_renamer_get_mode (insert_renamer));
      break;

    case PROP_OFFSET:
      g_value_set_uint (value, thunar_sbr_insert_renamer_get_offset (insert_renamer));
      break;

    case PROP_OFFSET_MODE:
      g_value_set_enum (value, thunar_sbr_insert_renamer_get_offset_mode (insert_renamer));
      break;

    case PROP_TEXT:
      g_value_set_string (value, thunar_sbr_insert_renamer_get_text (insert_renamer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_sbr_insert_renamer_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ThunarSbrInsertRenamer *insert_renamer = THUNAR_SBR_INSERT_RENAMER (object);

  switch (prop_id)
    {
    case PROP_MODE:
      thunar_sbr_insert_renamer_set_mode (insert_renamer, g_value_get_enum (value));
      break;

    case PROP_OFFSET:
      thunar_sbr_insert_renamer_set_offset (insert_renamer, g_value_get_uint (value));
      break;

    case PROP_OFFSET_MODE:
      thunar_sbr_insert_renamer_set_offset_mode (insert_renamer, g_value_get_enum (value));
      break;

    case PROP_TEXT:
      thunar_sbr_insert_renamer_set_text (insert_renamer, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gchar*
thunar_sbr_insert_renamer_process (ThunarxRenamer  *renamer,
                                   ThunarxFileInfo *file,
                                   const gchar     *text,
                                   guint            idx)
{
  ThunarSbrInsertRenamer *insert_renamer = THUNAR_SBR_INSERT_RENAMER (renamer);
  const gchar            *t;
  const gchar            *s;
  GString                *result;
  guint                   text_length;
  guint                   offset;

  /* check if we have any text to insert/overwrite */
  if (G_UNLIKELY (insert_renamer->text == NULL || *insert_renamer->text == '\0'))
    return g_strdup (text);

  /* determine the input text length */
  text_length = g_utf8_strlen (text, -1);

  /* determine the real offset and check if it's valid */
  offset = (insert_renamer->offset_mode == THUNAR_SBR_OFFSET_MODE_LEFT) ? insert_renamer->offset : (text_length - insert_renamer->offset);
  if (G_UNLIKELY (offset > text_length))
    return g_strdup (text);

  /* allocate space for the result */
  result = g_string_sized_new (2 * text_length);

  /* determine the text pointer for the offset */
  s = g_utf8_offset_to_pointer (text, offset);

  /* add the text before the insert/overwrite offset */
  g_string_append_len (result, text, s - text);

  /* add the text to insert */
  g_string_append (result, insert_renamer->text);

  /* skip over the input text if overwriting */
  if (insert_renamer->mode == THUNAR_SBR_INSERT_MODE_OVERWRITE)
    {
      /* skip over the number of characters in the overwrite text */
      for (t = insert_renamer->text; *s != '\0' && *t != '\0'; s = g_utf8_next_char (s), t = g_utf8_next_char (t))
        ;
    }

  /* append the remaining text */
  g_string_append (result, s);

  /* return the result */
  return g_string_free (result, FALSE);
}



/**
 * thunar_sbr_insert_renamer_new:
 *
 * Allocates a new #ThunarSbrInsertRenamer instance.
 *
 * Return value: the newly allocated #ThunarSbrInsertRenamer.
 **/
ThunarSbrInsertRenamer*
thunar_sbr_insert_renamer_new (void)
{
  return g_object_new (THUNAR_SBR_TYPE_INSERT_RENAMER,
                       "name", _("Insert / Overwrite"),
                       NULL);
}



/**
 * thunar_sbr_insert_renamer_get_mode:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 *
 * Returns the mode of the @insert_renamer.
 *
 * Return value: the mode of @insert_renamer.
 **/
ThunarSbrInsertMode
thunar_sbr_insert_renamer_get_mode (ThunarSbrInsertRenamer *insert_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer), THUNAR_SBR_INSERT_MODE_INSERT);
  return insert_renamer->mode;
}



/**
 * thunar_sbr_insert_renamer_set_mode:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 * @mode           : the new #ThunarSbrInsertMode for @insert_renamer.
 *
 * Sets the mode of @insert_renamer to @mode.
 **/
void
thunar_sbr_insert_renamer_set_mode (ThunarSbrInsertRenamer *insert_renamer,
                                    ThunarSbrInsertMode     mode)
{
  g_return_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer));

  /* check if we have a new mode */
  if (G_LIKELY (insert_renamer->mode != mode))
    {
      /* apply the new mode */
      insert_renamer->mode = mode;

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (insert_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (insert_renamer), "mode");
    }
}



/**
 * thunar_sbr_insert_renamer_get_offset:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 *
 * Returns the offset for the @insert_renamer.
 *
 * Return value: the offset for @insert_renamer.
 **/
guint
thunar_sbr_insert_renamer_get_offset (ThunarSbrInsertRenamer *insert_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer), 0);
  return insert_renamer->offset;
}



/**
 * thunar_sbr_insert_renamer_set_offset:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 * @offset         : the new offset for @insert_renamer.
 *
 * Sets the offset for @insert_renamer to @offset.
 **/
void
thunar_sbr_insert_renamer_set_offset (ThunarSbrInsertRenamer *insert_renamer,
                                      guint                   offset)
{
  g_return_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer));

  /* check if we have a new offset */
  if (G_LIKELY (insert_renamer->offset != offset))
    {
      /* apply the new offset */
      insert_renamer->offset = offset;

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (insert_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (insert_renamer), "offset");
    }
}



/**
 * thunar_sbr_insert_renamer_get_offset_mode:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 *
 * Returns the offset mode for the @insert_renamer.
 *
 * Return value: the offset mode for @insert_renamer.
 **/
ThunarSbrOffsetMode
thunar_sbr_insert_renamer_get_offset_mode (ThunarSbrInsertRenamer *insert_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer), THUNAR_SBR_OFFSET_MODE_LEFT);
  return insert_renamer->offset_mode;
}



/**
 * thunar_sbr_insert_renamer_set_offset_mode:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 * @offset_mode    : the new offset mode for @insert_renamer.
 *
 * Sets the offset mode for @insert_renamer to @offset_mode.
 **/
void
thunar_sbr_insert_renamer_set_offset_mode (ThunarSbrInsertRenamer *insert_renamer,
                                           ThunarSbrOffsetMode     offset_mode)
{
  g_return_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer));

  /* check if we have a new setting */
  if (G_LIKELY (insert_renamer->offset_mode != offset_mode))
    {
      /* apply the new setting */
      insert_renamer->offset_mode = offset_mode;

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (insert_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (insert_renamer), "offset-mode");
    }
}



/**
 * thunar_sbr_insert_renamer_get_text:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 *
 * Returns the text for the @insert_renamer.
 *
 * Return value: the text for @insert_renamer.
 **/
const gchar*
thunar_sbr_insert_renamer_get_text (ThunarSbrInsertRenamer *insert_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer), NULL);
  return insert_renamer->text;
}



/**
 * thunar_sbr_insert_renamer_set_text:
 * @insert_renamer : a #ThunarSbrInsertRenamer.
 * @text           : the new text for @insert_renamer.
 *
 * Sets the text for @insert_renamer to @text.
 **/
void
thunar_sbr_insert_renamer_set_text (ThunarSbrInsertRenamer *insert_renamer,
                                    const gchar            *text)
{
  g_return_if_fail (THUNAR_SBR_IS_INSERT_RENAMER (insert_renamer));

  /* check if we have a new text */
  if (G_LIKELY (g_strcmp0 (insert_renamer->text, text) != 0))
    {
      /* apply the new text */
      g_free (insert_renamer->text);
      insert_renamer->text = g_strdup (text);

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (insert_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (insert_renamer), "text");
    }
}


