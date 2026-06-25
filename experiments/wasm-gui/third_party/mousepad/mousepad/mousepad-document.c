/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-close-button.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-window.h>



static void      mousepad_document_finalize                (GObject                *object);
static void      mousepad_document_notify_cursor_position  (MousepadDocument       *document);
static void      mousepad_document_encoding_changed        (MousepadFile           *file,
                                                            MousepadEncoding        encoding,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_language         (GtkSourceBuffer        *buffer,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_overwrite        (GtkTextView            *textview,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_location_changed        (MousepadDocument       *document,
                                                            GFile                  *file);
static void      mousepad_document_style_label             (MousepadDocument       *document);
static void      mousepad_document_tab_button_clicked      (GtkWidget              *widget,
                                                            MousepadDocument       *document);
static void      mousepad_document_search_completed        (GObject                *object,
                                                            GAsyncResult           *result,
                                                            gpointer                data);
static void      mousepad_document_emit_search_signal      (MousepadDocument       *document,
                                                            GParamSpec             *pspec,
                                                            GtkSourceSearchContext *search_context);
static void      mousepad_document_search_widget_visible   (MousepadDocument       *document,
                                                            GParamSpec             *pspec,
                                                            MousepadWindow         *window);



enum
{
  CLOSE_TAB,
  CURSOR_CHANGED,
  ENCODING_CHANGED,
  LANGUAGE_CHANGED,
  OVERWRITE_CHANGED,
  SEARCH_COMPLETED,
  LAST_SIGNAL
};

enum
{
  INIT,
  VISIBLE,
  HIDDEN,
};

struct _MousepadDocumentPrivate
{
  /* the tab label and its tooltip ebox */
  GtkWidget              *ebox;
  GtkWidget              *label;

  /* utf-8 valid document names */
  gchar                  *utf8_filename;
  gchar                  *utf8_basename;

  /* search related */
  GtkSourceSearchContext *search_context, *selection_context;
  GtkSourceBuffer        *selection_buffer;
  gint                    prev_search_state;
  guint                   search_id;
  gint                    cur_match;
};



static guint document_signals[LAST_SIGNAL];



MousepadDocument *
mousepad_document_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_DOCUMENT, NULL);
}



G_DEFINE_TYPE_WITH_PRIVATE (MousepadDocument, mousepad_document, GTK_TYPE_SCROLLED_WINDOW)



static void
mousepad_document_class_init (MousepadDocumentClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_document_finalize;

  document_signals[CLOSE_TAB] =
    g_signal_new (I_("close-tab"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  document_signals[CURSOR_CHANGED] =
    g_signal_new (I_("cursor-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, _mousepad_marshal_VOID__INT_INT_INT,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  document_signals[ENCODING_CHANGED] =
    g_signal_new (I_("encoding-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  document_signals[LANGUAGE_CHANGED] =
    g_signal_new (I_("language-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_SOURCE_TYPE_LANGUAGE);

  document_signals[OVERWRITE_CHANGED] =
    g_signal_new (I_("overwrite-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  document_signals[SEARCH_COMPLETED] =
    g_signal_new (I_("search-completed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, _mousepad_marshal_VOID__INT_INT_STRING_FLAGS,
                  G_TYPE_NONE, 4, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, MOUSEPAD_TYPE_SEARCH_FLAGS);
}



static void
mousepad_document_hierarchy_changed (MousepadDocument *document,
                                     GtkWidget        *prev_window)
{
  GtkWidget *window;
  gboolean   visible;

  /* disconnect from previous window signals in case of a drag and drop */
  if (prev_window != NULL)
    mousepad_disconnect_by_func (prev_window, mousepad_document_search_widget_visible, document);

  /* get the ancestor MousepadWindow */
  window = gtk_widget_get_ancestor (GTK_WIDGET (document), MOUSEPAD_TYPE_WINDOW);

  /* apart from the cases where the document is removed (perhaps temporarily during a
   * drag and drop), there might also not be a MousepadWindow ancestor, e.g. when the
   * document is packed in a MousepadEncodingDialog */
  if (window == NULL)
    return;

  /* initialize autosave only for documents in a MousepadWindow, and only once */
  if (prev_window == NULL)
    mousepad_file_autosave_init (document->file);

  /* bind some search properties to the "search-widget-visible" window property */
  g_signal_connect_object (window, "notify::search-widget-visible",
                           G_CALLBACK (mousepad_document_search_widget_visible),
                           document, G_CONNECT_SWAPPED);

  /* get the window property */
  g_object_get (window, "search-widget-visible", &visible, NULL);

  /* block search context handlers, so that they are unblocked below without warnings */
  if (visible && document->priv->prev_search_state != VISIBLE)
    {
      g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                       g_signal_lookup ("insert-text", GTK_TYPE_TEXT_BUFFER),
                                       0, NULL, NULL, document->priv->search_context);
      g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                       g_signal_lookup ("delete-range", GTK_TYPE_TEXT_BUFFER),
                                       0, NULL, NULL, document->priv->search_context);
    }

  /* initialize binding */
  mousepad_document_search_widget_visible (document, NULL, MOUSEPAD_WINDOW (window));
}



static void
mousepad_document_init (MousepadDocument *document)
{
  GtkTargetList           *target_list;
  GtkSourceSearchSettings *search_settings;

  /* we will complete initialization when the document is anchored */
  g_signal_connect (document, "hierarchy-changed",
                    G_CALLBACK (mousepad_document_hierarchy_changed), NULL);

  /* private structure */
  document->priv = mousepad_document_get_instance_private (document);

  /* initialize the variables */
  document->priv->utf8_filename = NULL;
  document->priv->utf8_basename = NULL;
  document->priv->label = NULL;
  document->priv->selection_context = NULL;
  document->priv->selection_buffer = NULL;
  document->priv->prev_search_state = INIT;

  /* setup the scrolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (document),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (document), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (document), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (document), NULL);

  /* create a textbuffer and associated search context */
  document->buffer = GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
  document->priv->search_context = gtk_source_search_context_new (
                                     GTK_SOURCE_BUFFER (document->buffer), NULL);
  document->priv->search_id = 0;
  document->priv->cur_match = 0;

  /* bind search settings to Mousepad settings, except "regex-enabled" to prevent prohibitive
   * computation times in some situations (see
   * mousepad_document_prevent_endless_scanning() below) */
  search_settings = gtk_source_search_context_get_settings (document->priv->search_context);
  MOUSEPAD_SETTING_BIND (SEARCH_WRAP_AROUND, search_settings,
                         "wrap-around", G_SETTINGS_BIND_GET);
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_CASE, search_settings,
                         "case-sensitive", G_SETTINGS_BIND_GET);
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_WHOLE_WORD, search_settings,
                         "at-word-boundaries", G_SETTINGS_BIND_GET);

  /* forward the search context signal */
  g_signal_connect_swapped (document->priv->search_context, "notify::occurrences-count",
                            G_CALLBACK (mousepad_document_emit_search_signal), document);

  /* initialize the file */
  document->file = mousepad_file_new (document->buffer);
  g_signal_connect_swapped (document->file, "location-changed",
                            G_CALLBACK (mousepad_document_location_changed), document);

  /* setup the textview */
  document->textview = g_object_new (MOUSEPAD_TYPE_VIEW, "buffer", document->buffer, NULL);
  gtk_container_add (GTK_CONTAINER (document), GTK_WIDGET (document->textview));
  gtk_widget_show (GTK_WIDGET (document->textview));

  /* also allow dropping of uris and tabs in the textview */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (document->textview));
  gtk_target_list_add_table (target_list, drop_targets, G_N_ELEMENTS (drop_targets));

  /* connect handlers to the document attribute signals */
  g_signal_connect_swapped (document->file, "readonly-changed",
                            G_CALLBACK (mousepad_document_style_label), document);
  g_signal_connect_swapped (document->textview, "notify::editable",
                            G_CALLBACK (mousepad_document_style_label), document);

  /* forward some document attribute signals more or less directly */
  g_signal_connect_swapped (document->buffer, "notify::cursor-position",
                            G_CALLBACK (mousepad_document_notify_cursor_position), document);
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH, mousepad_document_notify_cursor_position,
                                   document, G_CONNECT_SWAPPED);
  g_signal_connect (document->file, "encoding-changed",
                    G_CALLBACK (mousepad_document_encoding_changed), document);
  g_signal_connect (document->buffer, "notify::language",
                    G_CALLBACK (mousepad_document_notify_language), document);
  g_signal_connect (document->textview, "notify::overwrite",
                    G_CALLBACK (mousepad_document_notify_overwrite), document);
}



static void
mousepad_document_finalize (GObject *object)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (object);

  /* cleanup */
  g_free (document->priv->utf8_filename);
  g_free (document->priv->utf8_basename);

  /* release the file */
  g_object_unref (document->file);

  /* search related */
  g_object_unref (document->priv->search_context);
  g_object_unref (document->buffer);
  if (document->priv->selection_buffer != NULL)
    {
      g_object_unref (document->priv->selection_context);
      g_object_unref (document->priv->selection_buffer);
    }

  G_OBJECT_CLASS (mousepad_document_parent_class)->finalize (object);
}



static void
mousepad_document_notify_cursor_position (MousepadDocument *document)
{
  GtkTextIter iter;
  gint        line, column, selection;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* get the current iter position */
  gtk_text_buffer_get_iter_at_mark (document->buffer, &iter,
                                    gtk_text_buffer_get_insert (document->buffer));

  /* get the current line number */
  line = gtk_text_iter_get_line (&iter) + 1;

  /* get the column */
  column = mousepad_util_get_real_line_offset (&iter);

  /* get length of the selection */
  selection = mousepad_view_get_selection_length (document->textview);

  /* clear search index if set */
  if (document->priv->cur_match != 0)
    {
      document->priv->cur_match = 0;
      g_object_notify (G_OBJECT (document->priv->search_context), "occurrences-count");
    }

  /* emit the signal */
  g_signal_emit (document, document_signals[CURSOR_CHANGED], 0, line, column, selection);
}



static void
mousepad_document_encoding_changed (MousepadFile     *file,
                                    MousepadEncoding  encoding,
                                    MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* emit the signal */
  g_signal_emit (document, document_signals[ENCODING_CHANGED], 0, encoding);
}



static void
mousepad_document_notify_language (GtkSourceBuffer  *buffer,
                                   GParamSpec       *pspec,
                                   MousepadDocument *document)
{
  GtkSourceLanguage *language;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

  /* the new language */
  language = gtk_source_buffer_get_language (buffer);

  /* emit the signal */
  g_signal_emit (document, document_signals[LANGUAGE_CHANGED], 0, language);
}



static void
mousepad_document_notify_overwrite (GtkTextView      *textview,
                                    GParamSpec       *pspec,
                                    MousepadDocument *document)
{
  gboolean overwrite;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));

  /* whether overwrite is enabled */
  overwrite = gtk_text_view_get_overwrite (textview);

  /* emit the signal */
  g_signal_emit (document, document_signals[OVERWRITE_CHANGED], 0, overwrite);
}



void
mousepad_document_send_signals (MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* re-send the cursor changed signal */
  mousepad_document_notify_cursor_position (document);

  /* re-send the encoding signal */
  mousepad_document_encoding_changed (document->file,
                                      mousepad_file_get_encoding (document->file), document);

  /* re-send the language signal */
  mousepad_document_notify_language (GTK_SOURCE_BUFFER (document->buffer), NULL, document);

  /* re-send the overwrite signal */
  mousepad_document_notify_overwrite (GTK_TEXT_VIEW (document->textview), NULL, document);
}



static void
mousepad_document_location_changed (MousepadDocument *document,
                                    GFile            *file)
{
  gchar       *utf8_filename, *utf8_short_filename, *utf8_basename;
  const gchar *home;
  size_t       home_len;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (file != NULL);

  /* get the display filename */
  utf8_filename = mousepad_util_get_display_path (file);

  /* create a shorter display filename: replace $HOME with a tilde if user is not root */
  if (geteuid () && (home = g_get_home_dir ()) && (home_len = strlen (home))
      && g_str_has_prefix (utf8_filename, home))
    {
      utf8_short_filename = g_strconcat ("~", utf8_filename + home_len, NULL);
      g_free (utf8_filename);
      utf8_filename = utf8_short_filename;
    }

  /* create the display name */
  utf8_basename = g_filename_display_basename (utf8_filename);

  /* remove the old names */
  g_free (document->priv->utf8_filename);
  g_free (document->priv->utf8_basename);

  /* set the new names */
  document->priv->utf8_filename = utf8_filename;
  document->priv->utf8_basename = utf8_basename;

  /* update the tab label and tooltip */
  if (G_UNLIKELY (document->priv->label))
    {
      /* set the tab label */
      gtk_label_set_text (GTK_LABEL (document->priv->label), utf8_basename);

      /* set the tab tooltip */
      gtk_widget_set_tooltip_text (document->priv->ebox, utf8_filename);

      /* update label style */
      mousepad_document_style_label (document);
    }
}



static void
mousepad_document_style_label (MousepadDocument *document)
{
  GtkStyleContext *context;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (document->buffer));
  g_return_if_fail (MOUSEPAD_IS_FILE (document->file));

  if (document->priv->label)
    {
      context = gtk_widget_get_style_context (document->priv->label);

      /* grey out the label text */
      if (mousepad_file_get_read_only (document->file)
          || ! gtk_text_view_get_editable (GTK_TEXT_VIEW (document->textview)))
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_DIM_LABEL);
      else
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_DIM_LABEL);
    }
}



void
mousepad_document_set_overwrite (MousepadDocument *document,
                                 gboolean          overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  gtk_text_view_set_overwrite (GTK_TEXT_VIEW (document->textview), overwrite);
}



void
mousepad_document_focus_textview (MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* focus the textview */
  gtk_widget_grab_focus (GTK_WIDGET (document->textview));
}



static void
mousepad_document_expand_tabs_changed (MousepadDocument *document)
{
  gboolean expand;

  expand = MOUSEPAD_SETTING_GET_BOOLEAN (EXPAND_TABS);

  gtk_widget_set_hexpand (document->priv->label, expand);
  gtk_label_set_ellipsize (GTK_LABEL (document->priv->label),
                           expand ? PANGO_ELLIPSIZE_MIDDLE : PANGO_ELLIPSIZE_NONE);
}



GtkWidget *
mousepad_document_get_tab_label (MousepadDocument *document)
{
  GtkWidget  *hbox, *button;

  /* create the box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (hbox);

  /* the ebox */
  document->priv->ebox = g_object_new (GTK_TYPE_EVENT_BOX, "border-width", 2,
                                       "visible-window", FALSE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), document->priv->ebox, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (document->priv->ebox, document->priv->utf8_filename);
  gtk_widget_show (document->priv->ebox);

  /* create the label */
  document->priv->label = gtk_label_new (mousepad_document_get_basename (document));
  mousepad_document_expand_tabs_changed (document);
  MOUSEPAD_SETTING_CONNECT_OBJECT (EXPAND_TABS, mousepad_document_expand_tabs_changed,
                                   document, G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (document->priv->ebox), document->priv->label);
  gtk_widget_show (document->priv->label);

  /* set label style */
  mousepad_document_style_label (document);

  /* create the button */
  button = mousepad_close_button_new (document->buffer);
  gtk_widget_show (button);

  /* pack button, add signal and tooltip */
  gtk_widget_set_tooltip_text (button, _("Close this tab"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (mousepad_document_tab_button_clicked), document);

  return hbox;
}



static void
mousepad_document_tab_button_clicked (GtkWidget        *widget,
                                      MousepadDocument *document)
{
  g_signal_emit (document, document_signals[CLOSE_TAB], 0);
}



const gchar *
mousepad_document_get_basename (MousepadDocument *document)
{
  static gint untitled_counter = 0;

  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  /* check if there is a filename set */
  if (document->priv->utf8_basename == NULL)
    {
      /* create an unique untitled document name */
      document->priv->utf8_basename = g_strdup_printf ("%s %d", _("Untitled"), ++untitled_counter);
    }

  return document->priv->utf8_basename;
}



const gchar *
mousepad_document_get_filename (MousepadDocument *document)
{
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  return document->priv->utf8_filename;
}



static gboolean
mousepad_document_search_completed_idle (gpointer data)
{
  MousepadDocument        *document = data;
  MousepadSearchFlags      flags;
  GtkSourceSearchContext  *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextBuffer           *selection_buffer;
  GtkTextIter             *start, *end;
  GtkTextIter              iter;
  gchar                   *selected_text;
  const gchar             *string, *replace;
  gboolean                 found;

  /* retrieve the first stage data */
  search_context = mousepad_object_get_data (document, "search-context");
  flags = GPOINTER_TO_INT (mousepad_object_get_data (search_context, "flags"));
  replace = mousepad_object_get_data (search_context, "replace");
  start = mousepad_object_get_data (search_context, "start");
  end = mousepad_object_get_data (search_context, "end");
  found = GPOINTER_TO_INT (mousepad_object_get_data (search_context, "found"));
  search_settings = gtk_source_search_context_get_settings (search_context);
  string = gtk_source_search_settings_get_search_text (search_settings);

  if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START)
    gtk_text_buffer_get_selection_bounds (document->buffer, &iter, NULL);
  else
    gtk_text_buffer_get_selection_bounds (document->buffer, NULL, &iter);

  /* handle the action */
  if (found && (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT)
      && ! (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    {
      gtk_text_buffer_select_range (document->buffer, start, end);
      document->priv->cur_match =
        gtk_source_search_context_get_occurrence_position (search_context, start, end);
    }
  else if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
    {
      if (found && ! (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA))
        {
          /* replace selected occurrence */
#if GTK_SOURCE_MAJOR_VERSION >= 4
          gtk_source_search_context_replace (search_context, start, end, replace, -1, NULL);
#else
          gtk_source_search_context_replace2 (search_context, start, end, replace, -1, NULL);
#endif

          /* select next occurrence */
          flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;
          flags &= ~ MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE;
          mousepad_document_search (document, string, NULL, flags);
        }
      else if (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA)
        {
          /* replace all occurrences in the buffer in use */
          gtk_source_search_context_replace_all (search_context, replace, -1, NULL);

          if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
            {
              /* get the text in the virtual selection buffer */
              selection_buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (search_context));
              gtk_text_buffer_get_bounds (selection_buffer, start, end);
              selected_text = gtk_text_buffer_get_text (selection_buffer, start, end, FALSE);

              /* replace selection in the real buffer by the text in the virtual selection buffer */
              gtk_text_buffer_get_selection_bounds (document->buffer, start, end);
              gtk_text_buffer_begin_user_action (document->buffer);
              gtk_text_buffer_delete (document->buffer, start, end);
              gtk_text_buffer_insert (document->buffer, start, selected_text, -1);
              gtk_text_buffer_end_user_action (document->buffer);
              g_free (selected_text);
            }
        }
    }
  /* deselect previous result when the new search fails or the search field is reset */
  else if (! (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_NONE)
           && ! (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    gtk_text_buffer_place_cursor (document->buffer, &iter);

  /* force the signal emission, to cover cases where Mousepad search settings change without
   * changing GtkSourceView settings (e.g. when switching between single-document mode and
   * multi-document mode, or if search index changed) */
  if (gtk_source_search_context_get_occurrences_count (search_context) != -1)
    g_object_notify (G_OBJECT (search_context), "occurrences-count");

  document->priv->search_id = 0;

  return FALSE;
}



static gboolean
mousepad_document_unref (gpointer data)
{
  g_object_unref (data);

  return FALSE;
}



static void
mousepad_document_search_completed (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      data)
{
  MousepadDocument        *document = data;
  MousepadSearchFlags      flags;
  GtkSourceSearchContext  *search_context = GTK_SOURCE_SEARCH_CONTEXT (object);
  GtkTextIter              start, end;
  GError                  *error = NULL;
  gboolean                 found;

  /* exit if the document was removed during the search process */
  if (gtk_widget_get_parent (GTK_WIDGET (document)) == NULL)
    {
      /* let the cancellable operation reach its end before removing the last reference */
      g_idle_add (mousepad_document_unref, document);

      return;
    }

  /* remove extra reference kept at first stage */
  g_object_unref (document);

  /* get the search result */
  flags = GPOINTER_TO_INT (mousepad_object_get_data (search_context, "flags"));
#if GTK_SOURCE_MAJOR_VERSION >= 4
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    found = gtk_source_search_context_backward_finish (search_context, result,
                                                       &start, &end, NULL, &error);
  else
    found = gtk_source_search_context_forward_finish (search_context, result,
                                                      &start, &end, NULL, &error);
#else
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    found = gtk_source_search_context_backward_finish2 (search_context, result,
                                                        &start, &end, NULL, &error);
  else
    found = gtk_source_search_context_forward_finish2 (search_context, result,
                                                       &start, &end, NULL, &error);
#endif

  /* exit if the operation was cancelled */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }
  else if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  /* now we need first to exit this async task callback to prevent any warning,
   * but we launch the rest as soon as possible to preserve real-time behavior */
  mousepad_object_set_data (search_context, "found", GINT_TO_POINTER (found));
  mousepad_object_set_data_full (search_context, "start",
                                 gtk_text_iter_copy (&start), gtk_text_iter_free);
  mousepad_object_set_data_full (search_context, "end",
                                 gtk_text_iter_copy (&end), gtk_text_iter_free);
  mousepad_object_set_data (document, "search-context", search_context);
  if (document->priv->search_id != 0)
    g_source_remove (document->priv->search_id);

  document->priv->search_id =
    g_idle_add_full (G_PRIORITY_HIGH, mousepad_document_search_completed_idle,
                     mousepad_util_source_autoremove (document), NULL);
}



void
mousepad_document_search (MousepadDocument    *document,
                          const gchar         *string,
                          const gchar         *replace,
                          MousepadSearchFlags  flags)
{
  GtkSourceSearchContext  *search_context;
  GtkSourceSearchSettings *search_settings, *search_settings_doc;
  GtkTextIter              iter, start, end;
  GCancellable            *cancellable;
  gchar                   *selected_text;
  const gchar             *reference = "";
  gboolean                 has_references;

  /* get the search iter */
  if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START)
    gtk_text_buffer_get_selection_bounds (document->buffer, &iter, NULL);
  else
    gtk_text_buffer_get_selection_bounds (document->buffer, NULL, &iter);

  /* search in selected text only: substitute a virtual selection buffer to the real buffer
   * and work inside */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
    {
      /* initialize selection buffer and search context if needed */
      if (document->priv->selection_buffer == NULL)
        {
          document->priv->selection_buffer = gtk_source_buffer_new (NULL);
          document->priv->selection_context =
            gtk_source_search_context_new (document->priv->selection_buffer, NULL);
          g_signal_connect_swapped (document->priv->selection_context, "notify::occurrences-count",
                                    G_CALLBACK (mousepad_document_emit_search_signal), document);
          gtk_source_search_context_set_highlight (document->priv->selection_context, FALSE);
        }

      /* get the text in the selection area */
      gtk_text_buffer_get_selection_bounds (document->buffer, &start, &end);
      selected_text = gtk_text_buffer_get_text (document->buffer, &start, &end, FALSE);

      /* put the text in the selection buffer */
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (document->priv->selection_buffer),
                                selected_text, -1);
      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (document->priv->selection_buffer), &iter);
      g_free (selected_text);

      /* copy search settings from those of the document */
      search_context = document->priv->selection_context;
      search_settings = gtk_source_search_context_get_settings (search_context);
      search_settings_doc = gtk_source_search_context_get_settings (document->priv->search_context);

      gtk_source_search_settings_set_case_sensitive (
        search_settings, gtk_source_search_settings_get_case_sensitive (search_settings_doc));
      gtk_source_search_settings_set_at_word_boundaries (
        search_settings, gtk_source_search_settings_get_at_word_boundaries (search_settings_doc));
      gtk_source_search_settings_set_regex_enabled (
        search_settings, gtk_source_search_settings_get_regex_enabled (search_settings_doc));
    }
  /* search in the whole document */
  else
    search_context = document->priv->search_context;

  /* set the string to search for */
  search_settings = gtk_source_search_context_get_settings (search_context);
  gtk_source_search_settings_set_search_text (search_settings, string);

  /* set wrap around: always true for the search bar, bound to GSettings otherwise */
  gtk_source_search_settings_set_wrap_around (search_settings,
                                              (flags & MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND)
                                              || MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_WRAP_AROUND));

  /* special treatments if regex search is enabled */
  if (gtk_source_search_settings_get_regex_enabled (search_settings))
    {
      /* disable highlight to prevent prohibitive computation times in some situations
       * (see mousepad_document_prevent_endless_scanning() below) */
      gtk_source_search_context_set_highlight (search_context, FALSE);

      /* trick gtk_source_search_context_replace_all() into thinking its replacement text
       * contains back references even if it does not, so that g_regex_replace() is always
       * used to replace text in this function, which finally behaves the same as
       * gtk_source_search_context_replace(): see
       * https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/172 */
      if (replace != NULL && (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
          && (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA)
          && g_regex_check_replacement (replace, &has_references, NULL)
          && ! has_references)
        reference = "\\g<1>";
    }

  /* attach some data for the second stage */
  mousepad_object_set_data (search_context, "flags", GINT_TO_POINTER (flags));
  mousepad_object_set_data_full (search_context, "replace",
                                 g_strconcat (reference, replace, NULL), g_free);

  /* keep the document alive during the search process */
  g_object_ref (document);

  /* search the string */
  cancellable = g_cancellable_new ();
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    gtk_source_search_context_backward_async (search_context, &iter, cancellable,
                                              mousepad_document_search_completed, document);
  else
    gtk_source_search_context_forward_async (search_context, &iter, cancellable,
                                             mousepad_document_search_completed, document);
  g_object_unref (cancellable);
}



static void
mousepad_document_emit_search_signal (MousepadDocument       *document,
                                      GParamSpec             *pspec,
                                      GtkSourceSearchContext *search_context)
{
  GtkSourceSearchSettings *search_settings;
  MousepadSearchFlags      flags;
  gint                     n_matches;
  const gchar             *string;

  /* retrieve data */
  flags = GPOINTER_TO_INT (mousepad_object_get_data (search_context, "flags"));
  n_matches = gtk_source_search_context_get_occurrences_count (search_context);
  search_settings = gtk_source_search_context_get_settings (search_context);
  string = gtk_source_search_settings_get_search_text (search_settings);

  /* emit the signal */
  g_signal_emit (document, document_signals[SEARCH_COMPLETED], 0, document->priv->cur_match,
                 n_matches, string, flags);
}



static void
mousepad_document_scanning_started (MousepadDocument *document)
{
  gtk_source_search_context_set_highlight (document->priv->search_context, FALSE);
}



static void
mousepad_document_scanning_completed (MousepadDocument *document)
{
  gtk_source_search_context_set_highlight (document->priv->search_context, TRUE);
}



/*
 * Disable highlight during regex searches: a workaround to prevent prohibitive computation
 * times in some situations (see also mousepad_document_search() and mousepad_document_init()
 * above).
 * See https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/164
 */
static void
mousepad_document_prevent_endless_scanning (MousepadDocument *document,
                                            gboolean          visible)
{
  if (visible && MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_HIGHLIGHT_ALL)
      && MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_ENABLE_REGEX))
    {
      g_signal_connect_swapped (document->buffer, "insert-text",
                                G_CALLBACK (mousepad_document_scanning_started), document);
      g_signal_connect_swapped (document->buffer, "delete-range",
                                G_CALLBACK (mousepad_document_scanning_started), document);
      g_signal_connect_swapped (document->priv->search_context, "notify::occurrences-count",
                                G_CALLBACK (mousepad_document_scanning_completed), document);
    }
  else
    {
      mousepad_disconnect_by_func (document->buffer, mousepad_document_scanning_started, document);
      mousepad_disconnect_by_func (document->priv->search_context,
                                   mousepad_document_scanning_completed, document);

      /* re-enable highlighting if disconnection occurs between the two switches above */
      gtk_source_search_context_set_highlight (document->priv->search_context,
                                               MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_HIGHLIGHT_ALL));
    }
}



static void
mousepad_document_search_widget_visible (MousepadDocument *document,
                                         GParamSpec       *pspec,
                                         MousepadWindow   *window)
{
  GtkSourceSearchSettings *search_settings;
  gboolean                 visible;

  /* get the window property */
  g_object_get (window, "search-widget-visible", &visible, NULL);

  /* get the search settings */
  search_settings = gtk_source_search_context_get_settings (document->priv->search_context);

  if (visible && document->priv->prev_search_state != VISIBLE)
    {
      /* update previous search state */
      document->priv->prev_search_state = VISIBLE;

      /* unblock search context handlers */
      g_signal_handlers_unblock_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                         g_signal_lookup ("insert-text", GTK_TYPE_TEXT_BUFFER),
                                         0, NULL, NULL, document->priv->search_context);
      g_signal_handlers_unblock_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                         g_signal_lookup ("delete-range", GTK_TYPE_TEXT_BUFFER),
                                         0, NULL, NULL, document->priv->search_context);

      /* activate the workaround to prevent endless buffer scanning */
      mousepad_document_prevent_endless_scanning (document, visible);
      MOUSEPAD_SETTING_CONNECT_OBJECT (SEARCH_HIGHLIGHT_ALL,
                                       mousepad_document_prevent_endless_scanning,
                                       document, G_CONNECT_SWAPPED);
      MOUSEPAD_SETTING_CONNECT_OBJECT (SEARCH_ENABLE_REGEX,
                                       mousepad_document_prevent_endless_scanning,
                                       document, G_CONNECT_SWAPPED);

      /* bind "highlight" and "regex-enabled" search settings to Mousepad settings */
      MOUSEPAD_SETTING_BIND (SEARCH_HIGHLIGHT_ALL, document->priv->search_context,
                             "highlight", G_SETTINGS_BIND_GET);
      MOUSEPAD_SETTING_BIND (SEARCH_ENABLE_REGEX, search_settings,
                             "regex-enabled", G_SETTINGS_BIND_GET);
    }
  else if (! visible && document->priv->prev_search_state != HIDDEN)
    {
      /* update previous search state */
      document->priv->prev_search_state = HIDDEN;

      /* block search context handlers */
      g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                       g_signal_lookup ("insert-text", GTK_TYPE_TEXT_BUFFER),
                                       0, NULL, NULL, document->priv->search_context);
      g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                       g_signal_lookup ("delete-range", GTK_TYPE_TEXT_BUFFER),
                                       0, NULL, NULL, document->priv->search_context);

      /* deactivate the workaround to prevent endless buffer scanning */
      mousepad_document_prevent_endless_scanning (document, visible);
      MOUSEPAD_SETTING_DISCONNECT (SEARCH_HIGHLIGHT_ALL,
                                   G_CALLBACK (mousepad_document_prevent_endless_scanning),
                                   document);
      MOUSEPAD_SETTING_DISCONNECT (SEARCH_ENABLE_REGEX,
                                   G_CALLBACK (mousepad_document_prevent_endless_scanning),
                                   document);

      /* unbind "highlight" and "regex-enabled" search settings and turn them off */
      g_settings_unbind (document->priv->search_context, "highlight");
      g_settings_unbind (search_settings, "regex-enabled");
      gtk_source_search_context_set_highlight (document->priv->search_context, FALSE);
      gtk_source_search_settings_set_regex_enabled (search_settings, FALSE);
    }
}



GType
mousepad_document_search_flags_get_type (void)
{
  static GType type = G_TYPE_NONE;

  if (G_UNLIKELY (type == G_TYPE_NONE))
    {
      /* use empty values table */
      static const GFlagsValue values[] =
      {
        { 0, NULL, NULL }
      };

      /* register the type */
      type = g_flags_register_static (I_("MousepadSearchFlags"), values);
    }

  return type;
}
