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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

#include <thunar-sbr/thunar-sbr-replace-renamer.h>

#include <libxfce4util/libxfce4util.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CASE_SENSITIVE,
  PROP_PATTERN,
  PROP_REPLACEMENT,
  PROP_REGEXP,
};



static void   thunar_sbr_replace_renamer_finalize     (GObject                      *object);
static void   thunar_sbr_replace_renamer_get_property (GObject                      *object,
                                                       guint                         prop_id,
                                                       GValue                       *value,
                                                       GParamSpec                   *pspec);
static void   thunar_sbr_replace_renamer_set_property (GObject                      *object,
                                                       guint                         prop_id,
                                                       const GValue                 *value,
                                                       GParamSpec                   *pspec);
static void   thunar_sbr_replace_renamer_realize      (GtkWidget                    *widget);
static gchar *thunar_sbr_replace_renamer_process      (ThunarxRenamer               *renamer,
                                                       ThunarxFileInfo              *file,
                                                       const gchar                  *text,
                                                       guint                         idx);
#ifdef HAVE_PCRE2
static gchar *thunar_sbr_replace_renamer_pcre_exec    (ThunarSbrReplaceRenamer      *replace_renamer,
                                                       const gchar                  *text);
static void   thunar_sbr_replace_renamer_pcre_update  (ThunarSbrReplaceRenamer      *replace_renamer);
#endif



struct _ThunarSbrReplaceRenamerClass
{
  ThunarxRenamerClass __parent__;
};

struct _ThunarSbrReplaceRenamer
{
  ThunarxRenamer __parent__;
  GtkWidget     *pattern_entry;
  gboolean       case_sensitive;
  gboolean       regexp;
  gchar         *pattern;
  gchar         *replacement;

  /* TRUE if PCRE is available and supports UTF-8 */
  gint           utf8_regexp_supported;

  /* PCRE compiled pattern */
#ifdef HAVE_PCRE2
  pcre2_code    *pcre_pattern;
  gint           pcre_capture_count;
#endif
};



THUNARX_DEFINE_TYPE (ThunarSbrReplaceRenamer, thunar_sbr_replace_renamer, THUNARX_TYPE_RENAMER);



static void
thunar_sbr_replace_renamer_class_init (ThunarSbrReplaceRenamerClass *klass)
{
  ThunarxRenamerClass *thunarxrenamer_class;
  GtkWidgetClass      *gtkwidget_class;
  GObjectClass        *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_sbr_replace_renamer_finalize;
  gobject_class->get_property = thunar_sbr_replace_renamer_get_property;
  gobject_class->set_property = thunar_sbr_replace_renamer_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_sbr_replace_renamer_realize;

  thunarxrenamer_class = THUNARX_RENAMER_CLASS (klass);
  thunarxrenamer_class->process = thunar_sbr_replace_renamer_process;

  /**
   * ThunarSbrReplaceRenamer:case-sensitive:
   *
   * Whether to use case sensitive search and replace.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CASE_SENSITIVE,
                                   g_param_spec_boolean ("case-sensitive",
                                                         "case-sensitive",
                                                         "case-sensitive",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * ThunarSbrReplaceRenamer:pattern:
   *
   * The search pattern.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PATTERN,
                                   g_param_spec_string ("pattern",
                                                        "pattern",
                                                        "pattern",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * ThunarSbrReplaceRenamer:replacement:
   *
   * The replacement text.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REPLACEMENT,
                                   g_param_spec_string ("replacement",
                                                        "replacement",
                                                        "replacement",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * ThunarSbrReplaceRenamer:regexp:
   *
   * Whether to use regular expressions.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REGEXP,
                                   g_param_spec_boolean ("regexp",
                                                         "regexp",
                                                         "regexp",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}



static void
thunar_sbr_replace_renamer_init (ThunarSbrReplaceRenamer *replace_renamer)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *grid;
  GtkWidget      *label;
  GtkWidget      *entry;
  GtkWidget      *button;
  uint32_t        pcre2_compiled_widths;

  replace_renamer->utf8_regexp_supported = FALSE;

#ifdef HAVE_PCRE2
  /* check if PCRE supports UTF-8 */
  if (pcre2_config (PCRE2_CONFIG_COMPILED_WIDTHS, &pcre2_compiled_widths) >= 0)
  {
    /* bit0 indicates 8-bit support. bit1 and bit2 indicate 16-bit and 32-bit support respectively. */
    if ((pcre2_compiled_widths & (1 << 0)) != 0)
      replace_renamer->utf8_regexp_supported = TRUE;
  }
#endif

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_box_pack_start (GTK_BOX (replace_renamer), grid, FALSE, FALSE, 0);
  gtk_widget_show (grid);

  label = gtk_label_new_with_mnemonic (_("_Search For:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  replace_renamer->pattern_entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (replace_renamer->pattern_entry), TRUE);
  g_object_bind_property (G_OBJECT (replace_renamer->pattern_entry), "text", G_OBJECT (replace_renamer), "pattern", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (replace_renamer->pattern_entry, _("Enter the text to search for in the file names."));
  gtk_widget_set_hexpand (replace_renamer->pattern_entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), replace_renamer->pattern_entry, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), replace_renamer->pattern_entry);
  gtk_widget_show (replace_renamer->pattern_entry);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (replace_renamer->pattern_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  button = gtk_check_button_new_with_mnemonic (_("Regular _Expression"));
  g_object_bind_property (G_OBJECT (button), "active", G_OBJECT (replace_renamer), "regexp", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("If you enable this option, the pattern will be treated as a regular expression and "
                                         "matched using the Perl-compatible regular expressions (PCRE). Check the documentation "
                                         "for details about the regular expression syntax."));
  gtk_grid_attach (GTK_GRID (grid), button, 2, 0, 1, 1);
  gtk_widget_set_sensitive (button, replace_renamer->utf8_regexp_supported);
  gtk_widget_show (button);

  label = gtk_label_new_with_mnemonic (_("Replace _With:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_object_bind_property (G_OBJECT (entry), "text", G_OBJECT (replace_renamer), "replacement", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (entry, _("Enter the text that should be used as replacement for the pattern above."));
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_show (entry);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  button = gtk_check_button_new_with_mnemonic (_("C_ase Sensitive Search"));
  g_object_bind_property (G_OBJECT (button), "active", G_OBJECT (replace_renamer), "case-sensitive", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_widget_set_tooltip_text (button, _("If you enable this option, the pattern will be searched in a case-sensitive manner. "
                                         "The default is to use a case-insensitive search."));
  gtk_grid_attach (GTK_GRID (grid), button, 2, 1, 1, 1);
  gtk_widget_show (button);
}



static void
thunar_sbr_replace_renamer_finalize (GObject *object)
{
  ThunarSbrReplaceRenamer *replace_renamer = THUNAR_SBR_REPLACE_RENAMER (object);

  /* release the PCRE pattern (if any) */
#ifdef HAVE_PCRE2
  if (G_UNLIKELY (replace_renamer->pcre_pattern != NULL))
    pcre2_code_free (replace_renamer->pcre_pattern);
#endif

  /* release the strings */
  g_free (replace_renamer->replacement);
  g_free (replace_renamer->pattern);

  (*G_OBJECT_CLASS (thunar_sbr_replace_renamer_parent_class)->finalize) (object);
}



static void
thunar_sbr_replace_renamer_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ThunarSbrReplaceRenamer *replace_renamer = THUNAR_SBR_REPLACE_RENAMER (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, thunar_sbr_replace_renamer_get_case_sensitive (replace_renamer));
      break;

    case PROP_PATTERN:
      g_value_set_string (value, thunar_sbr_replace_renamer_get_pattern (replace_renamer));
      break;

    case PROP_REPLACEMENT:
      g_value_set_string (value, thunar_sbr_replace_renamer_get_replacement (replace_renamer));
      break;

    case PROP_REGEXP:
      g_value_set_boolean (value, thunar_sbr_replace_renamer_get_regexp (replace_renamer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_sbr_replace_renamer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ThunarSbrReplaceRenamer *replace_renamer = THUNAR_SBR_REPLACE_RENAMER (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      thunar_sbr_replace_renamer_set_case_sensitive (replace_renamer, g_value_get_boolean (value));
      break;

    case PROP_PATTERN:
      thunar_sbr_replace_renamer_set_pattern (replace_renamer, g_value_get_string (value));
      break;

    case PROP_REPLACEMENT:
      thunar_sbr_replace_renamer_set_replacement (replace_renamer, g_value_get_string (value));
      break;

    case PROP_REGEXP:
      thunar_sbr_replace_renamer_set_regexp (replace_renamer, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_sbr_replace_renamer_realize (GtkWidget *widget)
{
  /* realize the widget */
  (*GTK_WIDGET_CLASS (thunar_sbr_replace_renamer_parent_class)->realize) (widget);

#ifdef HAVE_PCRE2
  /* update the PCRE pattern */
  thunar_sbr_replace_renamer_pcre_update (THUNAR_SBR_REPLACE_RENAMER (widget));
#endif
}



static gchar*
tsrr_replace (const gchar *text,
              const gchar *pattern,
              const gchar *replacement,
              gboolean     case_sensitive)
{
  const gchar *p;
  const gchar *t;
  gunichar     pc;
  gunichar     tc;
  GString     *result = g_string_sized_new (32);

  while (*text != '\0')
    {
      /* compare the pattern to this part of the text */
      for (p = pattern, t = text; *p != '\0' && *t != '\0'; p = g_utf8_next_char (p), t = g_utf8_next_char (t))
        {
          /* determine the next unichars */
          pc = g_utf8_get_char (p);
          tc = g_utf8_get_char (t);

          /* check if the chars don't match */
          if (pc != tc && (case_sensitive || g_unichar_toupper (pc) != g_unichar_toupper (tc)))
            break;
        }

      /* check if the pattern matches */
      if (G_UNLIKELY (*p == '\0'))
        {
          /* append the replacement to the result... */
          g_string_append (result, replacement);

          /* ...and skip to the text after the pattern */
          text = t;
        }
      else
        {
          /* just append the text char */
          g_string_append_unichar (result, g_utf8_get_char (text));
          text = g_utf8_next_char (text);
        }
    }

  return g_string_free (result, FALSE);
}



static gchar*
thunar_sbr_replace_renamer_process (ThunarxRenamer  *renamer,
                                    ThunarxFileInfo *file,
                                    const gchar     *text,
                                    guint            idx)
{
  ThunarSbrReplaceRenamer *replace_renamer = THUNAR_SBR_REPLACE_RENAMER (renamer);

  /* nothing to replace if we don't have a pattern */
  if (G_UNLIKELY (replace_renamer->pattern == NULL || *replace_renamer->pattern == '\0'))
    return g_strdup (text);

  /* check if we should use regular expression */
  if (G_UNLIKELY (replace_renamer->regexp))
    {
#ifdef HAVE_PCRE2
      /* check if the pattern failed to compile */
      if (G_UNLIKELY (replace_renamer->pcre_pattern == NULL))
        return g_strdup (text);

      /* just execute the pattern */
      return thunar_sbr_replace_renamer_pcre_exec (replace_renamer, text);
#endif
    }

  /* perform the replace operation */
  return tsrr_replace (text, replace_renamer->pattern, replace_renamer->replacement, replace_renamer->case_sensitive);
}



#ifdef HAVE_PCRE2
static gchar*
thunar_sbr_replace_renamer_pcre_exec (ThunarSbrReplaceRenamer *replace_renamer,
                                      const gchar             *subject)
{
  GString     *result;
  int          error;
  PCRE2_SIZE   erroffset;
  gchar        output[1024];
  pcre2_code  *compiled_pattern;
  PCRE2_SIZE   outlen;
  int          n_substitutions;  /* number of substitutions that were carried out */

  result = g_string_sized_new (32);

  compiled_pattern = pcre2_compile_8 ((PCRE2_SPTR)replace_renamer->pattern,
                                      PCRE2_ZERO_TERMINATED,
                                      0,
                                      &error,
                                      &erroffset,
                                      0);
  if (compiled_pattern == NULL)
    return g_strdup (subject);

  pcre2_jit_compile (compiled_pattern, PCRE2_JIT_COMPLETE);

  outlen = sizeof (output) / sizeof(PCRE2_UCHAR);
  
  n_substitutions = pcre2_substitute (compiled_pattern,
                                      (PCRE2_SPTR)subject,
                                      PCRE2_ZERO_TERMINATED,
                                      0,
                                      PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_EXTENDED,
                                      0,
                                      0,
                                      (PCRE2_SPTR)replace_renamer->replacement,
                                      PCRE2_ZERO_TERMINATED,
                                      (PCRE2_UCHAR *)output,
                                      &outlen);

  if (n_substitutions < 0)
    {
      PCRE2_UCHAR buffer[256];
      pcre2_get_error_message (error, buffer, sizeof(buffer));
      g_warning ("PCRE2 substitution failed at offset %d: %s\n", (int)erroffset, buffer);
      return g_strdup (subject);
    }

  pcre2_code_free (compiled_pattern);

  for (size_t i = 0; i < outlen; i++)
    g_string_append_c (result, output[i]);

  return g_string_free (result, FALSE);
}



static void
thunar_sbr_replace_renamer_pcre_update (ThunarSbrReplaceRenamer *replace_renamer)
{
  const gchar *error_message = NULL;
  gchar       *tooltip;
  gchar       *message;
  glong        offset;
  gint         error_offset = -1;
  int          error;
  PCRE2_SIZE   erroffset;

  /* pre-compile the pattern if regexp is enabled */
  if (G_UNLIKELY (replace_renamer->regexp))
    {
      /* release the previous pattern (if any) */
      if (G_LIKELY (replace_renamer->pcre_pattern != NULL))
        pcre2_code_free (replace_renamer->pcre_pattern);

      /* try to compile the new pattern */
      replace_renamer->pcre_pattern = pcre2_compile ((PCRE2_SPTR)replace_renamer->pattern, PCRE2_ZERO_TERMINATED, 0, &error, &erroffset, 0);

      if (replace_renamer->pcre_pattern == NULL)
        {
          PCRE2_UCHAR buffer[256];
          pcre2_get_error_message (error, buffer, sizeof(buffer));
          g_warning ("PCRE2 compilation failed at offset %d: %s\n", (int)erroffset, buffer);
        }
    }

  /* check if there was an error compiling the pattern */
  if (G_UNLIKELY (error_message != NULL))
    {
      /* convert the message to UTF-8 */
      message = g_locale_to_utf8 (error_message, -1, NULL, NULL, NULL);
      if (G_LIKELY (message != NULL))
        {
          /* determine the UTF-8 char offset */
          offset = g_utf8_pointer_to_offset (replace_renamer->pattern, replace_renamer->pattern + error_offset);

          /* setup a tooltip with the error message */
          tooltip = g_strdup_printf (_("Invalid regular expression, at character position %ld: %s"), offset, message);
          gtk_widget_set_tooltip_text (replace_renamer->pattern_entry, tooltip);
          g_free (tooltip);
        }
      g_free (message);

      /* check if the entry is realized */
      if (gtk_widget_get_realized (replace_renamer->pattern_entry))
        {
          /* highlight invalid input by using theme-specific colors */
          gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (replace_renamer->pattern_entry)), "error");
        }
    }
  else
    {
      /* check if the entry is realized */
      if (gtk_widget_get_realized (replace_renamer->pattern_entry))
        {
          /* stop highlight of invalid input if any */
          gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (replace_renamer->pattern_entry)), "error");
        }

      /* reset to default tooltip */
      gtk_widget_set_tooltip_text (replace_renamer->pattern_entry, _("Enter the text to search for in the file names."));
    }
}
#endif



/**
 * thunar_sbr_replace_renamer_new:
 *
 * Allocates a new #ThunarSbrReplaceRenamer instance.
 *
 * Return value: the newly allocated #ThunarSbrReplaceRenamer.
 **/
ThunarSbrReplaceRenamer*
thunar_sbr_replace_renamer_new (void)
{
  return g_object_new (THUNAR_SBR_TYPE_REPLACE_RENAMER,
                       "name", _("Search & Replace"),
                       NULL);
}



/**
 * thunar_sbr_replace_renamer_get_case_sensitive:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 *
 * Returns %TRUE if @replace_renamer<!---->s search should
 * be case sensitive.
 *
 * Return value: %TRUE if @replace_renamer is case sensitive.
 **/
gboolean
thunar_sbr_replace_renamer_get_case_sensitive (ThunarSbrReplaceRenamer *replace_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer), FALSE);
  return replace_renamer->case_sensitive;
}



/**
 * thunar_sbr_replace_renamer_set_case_sensitive:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 * @case_sensitive  : %TRUE to use case sensitive search.
 *
 * If @case_sensitive is %TRUE the search of @replace_renamer
 * will be case sensitive.
 **/
void
thunar_sbr_replace_renamer_set_case_sensitive (ThunarSbrReplaceRenamer *replace_renamer,
                                               gboolean                 case_sensitive)
{
  g_return_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer));

  /* normalize the value */
  case_sensitive = !!case_sensitive;

  /* check if we have a new setting */
  if (G_LIKELY (replace_renamer->case_sensitive != case_sensitive))
    {
      /* apply the new value */
      replace_renamer->case_sensitive = case_sensitive;

#ifdef HAVE_PCRE2
      /* pre-compile the pattern */
      thunar_sbr_replace_renamer_pcre_update (replace_renamer);
#endif

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (replace_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (replace_renamer), "case-sensitive");
    }
}



/**
 * thunar_sbr_replace_renamer_get_pattern:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 *
 * Returns the search pattern for @replace_renamer.
 *
 * Return value: the search pattern for @replace_renamer.
 **/
const gchar*
thunar_sbr_replace_renamer_get_pattern (ThunarSbrReplaceRenamer *replace_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer), NULL);
  return replace_renamer->pattern;
}



/**
 * thunar_sbr_replace_renamer_set_pattern:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 * @pattern         : the new pattern for @replace_renamer.
 *
 * Sets the search pattern of @replace_renamer to @pattern.
 **/
void
thunar_sbr_replace_renamer_set_pattern (ThunarSbrReplaceRenamer *replace_renamer,
                                        const gchar             *pattern)
{
  g_return_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer));
  g_return_if_fail (g_utf8_validate (pattern, -1, NULL));

  /* check if we have a new pattern */
  if (g_strcmp0 (replace_renamer->pattern, pattern) != 0)
    {
      /* apply the new value */
      g_free (replace_renamer->pattern);
      replace_renamer->pattern = g_strdup (pattern);

#ifdef HAVE_PCRE2
      /* pre-compile the pattern */
      thunar_sbr_replace_renamer_pcre_update (replace_renamer);
#endif

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (replace_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (replace_renamer), "pattern");
    }
}



/**
 * thunar_sbr_replace_renamer_get_regexp:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 *
 * Returns %TRUE if @replace_renamer should use a regular
 * expression.
 *
 * Return value: %TRUE if @replace_renamer uses a regexp.
 **/
gboolean
thunar_sbr_replace_renamer_get_regexp (ThunarSbrReplaceRenamer *replace_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer), FALSE);
  return replace_renamer->regexp;
}



/**
 * thunar_sbr_replace_renamer_set_regexp:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 * @regexp          : %TRUE to use regular expressions.
 *
 * If @regexp is %TRUE a regular expression should be used
 * for @replace_renamer.
 **/
void
thunar_sbr_replace_renamer_set_regexp (ThunarSbrReplaceRenamer *replace_renamer,
                                       gboolean                 regexp)
{
  g_return_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer));

  /* normalize the value */
  regexp = (!!regexp && replace_renamer->utf8_regexp_supported);

  /* check if we have a new value */
  if (G_LIKELY (replace_renamer->regexp != regexp))
    {
      /* apply the new value */
      replace_renamer->regexp = regexp;

#ifdef HAVE_PCRE2
      /* pre-compile the pattern */
      thunar_sbr_replace_renamer_pcre_update (replace_renamer);
#endif

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (replace_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (replace_renamer), "regexp");
    }
}



/**
 * thunar_sbr_replace_renamer_get_replacement:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 *
 * Returns the replacement for the @replace_renamer.
 *
 * Return value: the replacement for @replace_renamer.
 **/
const gchar*
thunar_sbr_replace_renamer_get_replacement (ThunarSbrReplaceRenamer *replace_renamer)
{
  g_return_val_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer), NULL);
  return replace_renamer->replacement;
}



/**
 * thunar_sbr_replace_renamer_set_replacement:
 * @replace_renamer : a #ThunarSbrReplaceRenamer.
 * @replacement     : the new replacement.
 *
 * Sets the replacement of @replace_renamer to @replacement.
 **/
void
thunar_sbr_replace_renamer_set_replacement (ThunarSbrReplaceRenamer *replace_renamer,
                                            const gchar             *replacement)
{
  g_return_if_fail (THUNAR_SBR_IS_REPLACE_RENAMER (replace_renamer));
  g_return_if_fail (g_utf8_validate (replacement, -1, NULL));

  /* check if we have a new replacement */
  if (g_strcmp0 (replace_renamer->replacement, replacement) != 0)
    {
      /* apply the setting */
      g_free (replace_renamer->replacement);
      replace_renamer->replacement = g_strdup (replacement);

      /* update the renamer */
      thunarx_renamer_changed (THUNARX_RENAMER (replace_renamer));

      /* notify listeners */
      g_object_notify (G_OBJECT (replace_renamer), "replacement");
    }
}



