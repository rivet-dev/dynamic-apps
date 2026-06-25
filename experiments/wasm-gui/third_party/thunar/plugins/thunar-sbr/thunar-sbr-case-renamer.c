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

#include <thunar-sbr/thunar-sbr-case-renamer.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_MODE,
};



static void   thunar_sbr_case_renamer_get_property  (GObject                    *object,
                                                     guint                       prop_id,
                                                     GValue                     *value,
                                                     GParamSpec                 *pspec);
static void   thunar_sbr_case_renamer_set_property  (GObject                    *object,
                                                     guint                       prop_id,
                                                     const GValue               *value,
                                                     GParamSpec                 *pspec);
static gchar *thunar_sbr_case_renamer_process       (ThunarxRenamer             *renamer,
                                                     ThunarxFileInfo            *file,
                                                     const gchar                *text,
                                                     guint                       idx);



struct _ThunarSbrCaseRenamerClass
{
  ThunarxRenamerClass __parent__;
};

struct _ThunarSbrCaseRenamer
{
  ThunarxRenamer           __parent__;
  ThunarSbrCaseRenamerMode mode;
};



THUNARX_DEFINE_TYPE (ThunarSbrCaseRenamer, thunar_sbr_case_renamer, THUNARX_TYPE_RENAMER);



static void
thunar_sbr_case_renamer_class_init (ThunarSbrCaseRenamerClass *klass)
{
  ThunarxRenamerClass *thunarxrenamer_class;
  GObjectClass        *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = thunar_sbr_case_renamer_get_property;
  gobject_class->set_property = thunar_sbr_case_renamer_set_property;

  thunarxrenamer_class = THUNARX_RENAMER_CLASS (klass);
  thunarxrenamer_class->process = thunar_sbr_case_renamer_process;

  /**
   * ThunarSbrCaseRenamer:mode:
   *
   * The #ThunarSbrCaseRenamerMode for this
   * #ThunarSbrCaseRenamer instance.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode", "mode", "mode",
                                                      THUNAR_SBR_TYPE_CASE_RENAMER_MODE,
                                                      THUNAR_SBR_CASE_RENAMER_MODE_LOWER,
                                                      G_PARAM_READWRITE));
}



static void
thunar_sbr_case_renamer_init (ThunarSbrCaseRenamer *case_renamer)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  GEnumClass     *klass;
  AtkObject      *object;
  GtkWidget      *combo;
  GtkWidget      *label;
  GtkWidget      *hbox;
  guint           n;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (case_renamer), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("Con_vert to:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_text_new ();
  klass = g_type_class_ref (THUNAR_SBR_TYPE_CASE_RENAMER_MODE);
  for (n = 0; n < klass->n_values; ++n)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _(klass->values[n].value_nick));
  g_object_bind_property (G_OBJECT (case_renamer), "mode", G_OBJECT (combo), "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  g_type_class_unref (klass);
  gtk_widget_show (combo);

  /* set Atk label relation for the combo */
  object = gtk_widget_get_accessible (combo);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));
}



static void
thunar_sbr_case_renamer_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ThunarSbrCaseRenamer *case_renamer = THUNAR_SBR_CASE_RENAMER (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, thunar_sbr_case_renamer_get_mode (case_renamer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_sbr_case_renamer_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ThunarSbrCaseRenamer *case_renamer = THUNAR_SBR_CASE_RENAMER (object);

  switch (prop_id)
    {
    case PROP_MODE:
      thunar_sbr_case_renamer_set_mode (case_renamer, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gchar*
tscr_utf8_strcase (const gchar *text,
                   gboolean     title_case)
{
  const gchar *t;
  gboolean     upper = TRUE;
  gunichar     c;
  GString     *result;

  /* allocate a new string for the result */
  result = g_string_sized_new (32);

  /* convert the input text */
  for (t = text; *t != '\0'; t = g_utf8_next_char (t))
    {
      /* check the next char */
      c = g_utf8_get_char (t);
      if (title_case && g_unichar_isspace (c))
        {
          upper = TRUE;
        }
      else if (upper && g_unichar_isalpha (c))
        {
          c = g_unichar_toupper (c);
          upper = FALSE;
        }
      else
        {
          c = g_unichar_tolower (c);
        }

      /* append the char to the result */
      g_string_append_unichar (result, c);
    }

  return g_string_free (result, FALSE);
}



static gchar*
thunar_sbr_case_renamer_process (ThunarxRenamer  *renamer,
                                 ThunarxFileInfo *file,
                                 const gchar     *text,
                                 guint            idx)
{
  ThunarSbrCaseRenamer *case_renamer = THUNAR_SBR_CASE_RENAMER (renamer);

  switch (case_renamer->mode)
    {
    case THUNAR_SBR_CASE_RENAMER_MODE_LOWER:
      return g_utf8_strdown (text, -1);

    case THUNAR_SBR_CASE_RENAMER_MODE_UPPER:
      return g_utf8_strup (text, -1);

    case THUNAR_SBR_CASE_RENAMER_MODE_TITLE:
      return tscr_utf8_strcase (text, TRUE);

   case THUNAR_SBR_CASE_RENAMER_MODE_FIRST_UPPER:
      return tscr_utf8_strcase (text, FALSE);

    default:
      g_assert_not_reached ();
      return NULL;
    }
}



/**
 * thunar_sbr_case_renamer_new:
 *
 * Allocates a new #ThunarSbrCaseRenamer instance.
 *
 * Return value: the newly allocated #ThunarSbrCaseRenamer.
 **/
ThunarSbrCaseRenamer*
thunar_sbr_case_renamer_new (void)
{
  return g_object_new (THUNAR_SBR_TYPE_CASE_RENAMER,
                       "name", _("Uppercase / Lowercase"),
                       NULL);
}



/**
 * thunar_sbr_case_renamer_get_mode:
 * @case_renamer : a #ThunarSbrCaseRenamer.
 *
 * Returns the current #ThunarSbrCaseRenamerMode
 * for the @case_renamer.
 *
 * Return value: the current mode of @case_renamer.
 **/
ThunarSbrCaseRenamerMode
thunar_sbr_case_renamer_get_mode (ThunarSbrCaseRenamer *case_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_CASE_RENAMER (case_renamer), THUNAR_SBR_CASE_RENAMER_MODE_LOWER);
  return case_renamer->mode;
}



/**
 * thunar_sbr_case_renamer_set_mode:
 * @case_renamer : a #ThunarSbrCaseRenamer.
 * @mode         : the new #ThunarSbrCaseRenamerMode.
 *
 * Sets the #ThunarSbrCaseRenamerMode of the @case_renamer
 * to the specified @mode.
 **/
void
thunar_sbr_case_renamer_set_mode (ThunarSbrCaseRenamer    *case_renamer,
                                  ThunarSbrCaseRenamerMode mode)
{
  g_return_if_fail (THUNAR_SBR_IS_CASE_RENAMER (case_renamer));

  /* check if we already use that mode */
  if (G_UNLIKELY (case_renamer->mode == mode))
    return;

  /* apply the new mode */
  case_renamer->mode = mode;

  /* notify listeners */
  g_object_notify (G_OBJECT (case_renamer), "mode");

  /* emit the "changed" signal */
  thunarx_renamer_changed (THUNARX_RENAMER (case_renamer));
}


