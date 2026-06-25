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
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-window.h>



#define mousepad_view_get_buffer(view) (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)))



/* GObject virtual functions */
static void      mousepad_view_finalize                      (GObject            *object);
static void      mousepad_view_set_property                  (GObject            *object,
                                                              guint               prop_id,
                                                              const GValue       *value,
                                                              GParamSpec         *pspec);

/* GtkWidget virtual functions */
static gboolean  mousepad_view_drag_motion                   (GtkWidget          *widget,
                                                              GdkDragContext     *context,
                                                              gint                x,
                                                              gint                y,
                                                              guint               timestamp);

/* GtkTextView virtual functions */
static void      mousepad_view_cut_clipboard                 (GtkTextView        *text_view);
static void      mousepad_view_delete_from_cursor            (GtkTextView        *text_view,
                                                              GtkDeleteType       type,
                                                              int                 count);
static void      mousepad_view_paste_clipboard               (GtkTextView        *text_view);

/* GtkSourceView virtual functions */
#if GTK_SOURCE_MAJOR_VERSION >= 4
static void      mousepad_view_move_lines                    (GtkSourceView      *source_view,
                                                              gboolean            down);
#else
static void      mousepad_view_move_lines                    (GtkSourceView      *source_view,
                                                              gboolean            copy,
                                                              gint                count);
#endif
static void      mousepad_view_move_words                    (GtkSourceView      *source_view,
                                                              gint                count);
static void      mousepad_view_redo                          (GtkSourceView      *source_view);
static void      mousepad_view_undo                          (GtkSourceView      *source_view);

/* MousepadView own functions */
static void      mousepad_view_transpose_range               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *start_iter,
                                                              GtkTextIter         *end_iter);
static void      mousepad_view_transpose_lines               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *start_iter,
                                                              GtkTextIter         *end_iter);
static void      mousepad_view_transpose_words               (GtkTextBuffer       *buffer,
                                                              GtkTextIter         *iter);
static void      mousepad_view_set_font                      (MousepadView        *view,
                                                              const gchar         *font);
static void      mousepad_view_set_show_whitespace           (MousepadView        *view,
                                                              gboolean             show);
static void      mousepad_view_set_space_location_flags      (MousepadView        *view,
                                                              gulong               flags);
static void      mousepad_view_set_show_line_endings         (MousepadView        *view,
                                                              gboolean             show);
static void      mousepad_view_set_color_scheme              (MousepadView        *view,
                                                              const gchar         *color_scheme);
static void      mousepad_view_set_word_wrap                 (MousepadView        *view,
                                                              gboolean             enabled);
static void      mousepad_view_set_match_braces              (MousepadView        *view,
                                                              gboolean             enabled);



struct _MousepadView
{
  GtkSourceView         __parent__;

  /* property related */
  GBinding                    *font_binding;
  gboolean                     show_whitespace;
  GtkSourceSpaceLocationFlags  space_location_flags;
  gboolean                     show_line_endings;
  gchar                       *color_scheme;
  gboolean                     match_braces;
};



enum
{
  PROP_0,
  PROP_FONT,
  PROP_SHOW_WHITESPACE,
  PROP_SPACE_LOCATION,
  PROP_SHOW_LINE_ENDINGS,
  PROP_COLOR_SCHEME,
  PROP_WORD_WRAP,
  PROP_MATCH_BRACES,
  NUM_PROPERTIES
};



G_DEFINE_TYPE (MousepadView, mousepad_view, GTK_SOURCE_TYPE_VIEW)



static void
mousepad_view_class_init (MousepadViewClass *klass)
{
  GObjectClass       *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass     *widget_class = GTK_WIDGET_CLASS (klass);
  GtkTextViewClass   *textview_class = GTK_TEXT_VIEW_CLASS (klass);
  GtkSourceViewClass *sourceview_class = GTK_SOURCE_VIEW_CLASS (klass);

  gobject_class->finalize = mousepad_view_finalize;
  gobject_class->set_property = mousepad_view_set_property;

  widget_class->drag_motion = mousepad_view_drag_motion;

  textview_class->cut_clipboard = mousepad_view_cut_clipboard;
  textview_class->delete_from_cursor = mousepad_view_delete_from_cursor;
  textview_class->paste_clipboard = mousepad_view_paste_clipboard;

  sourceview_class->move_lines = mousepad_view_move_lines;
  sourceview_class->move_words = mousepad_view_move_words;
  sourceview_class->redo = mousepad_view_redo;
  sourceview_class->undo = mousepad_view_undo;

  g_object_class_install_property (gobject_class, PROP_FONT,
    g_param_spec_string ("font", "Font", "The font to use in the view",
                         NULL, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_SHOW_WHITESPACE,
    g_param_spec_boolean ("show-whitespace", "ShowWhitespace",
                          "Whether whitespace is visualized in the view",
                          FALSE, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_SPACE_LOCATION,
    g_param_spec_flags ("space-location", "SpaceLocation",
                        "The space locations to show in the view",
                        GTK_SOURCE_TYPE_SPACE_LOCATION_FLAGS, GTK_SOURCE_SPACE_LOCATION_ALL,
                        G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_SHOW_LINE_ENDINGS,
    g_param_spec_boolean ("show-line-endings", "ShowLineEndings",
                          "Whether line-endings are visualized in the view",
                          FALSE, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_COLOR_SCHEME,
    g_param_spec_string ("color-scheme", "ColorScheme",
                         "The id of the syntax highlighting color scheme to use",
                         NULL, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_WORD_WRAP,
    g_param_spec_boolean ("word-wrap", "WordWrap",
                          "Whether to virtually wrap long lines in the view",
                          FALSE, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_MATCH_BRACES,
    g_param_spec_boolean ("match-braces", "MatchBraces",
                          "Whether to highlight matching braces, parens, brackets, etc.",
                          FALSE, G_PARAM_WRITABLE));
}



static void
mousepad_view_buffer_changed (MousepadView *view,
                              GParamSpec   *pspec,
                              gpointer      user_data)
{
  GtkSourceBuffer *buffer;
  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

  if (buffer != NULL)
    {
      GtkSourceStyleSchemeManager *manager;
      GtkSourceStyleScheme        *scheme;
      gboolean enable_highlight = TRUE;

      manager = gtk_source_style_scheme_manager_get_default ();
      scheme = gtk_source_style_scheme_manager_get_scheme (manager,
                 view->color_scheme ? view->color_scheme : "");

      if (scheme == NULL)
        {
          scheme = gtk_source_style_scheme_manager_get_scheme (manager, "classic");
          enable_highlight = FALSE;
        }

      gtk_source_buffer_set_style_scheme (buffer, scheme);
      gtk_source_buffer_set_highlight_syntax (buffer, enable_highlight);
      gtk_source_buffer_set_highlight_matching_brackets (buffer, view->match_braces);
    }
}



static void
mousepad_view_use_default_font (MousepadView *view)
{
  /* default font is used: unbind from GSettings, bind to application property */
  if (MOUSEPAD_SETTING_GET_BOOLEAN (USE_DEFAULT_FONT))
    {
      g_settings_unbind (view, "font");
      view->font_binding = g_object_bind_property (g_application_get_default (), "default-font",
                                                   view, "font", G_BINDING_SYNC_CREATE);
    }
  /* default font is not used: unbind from application property, bind to GSettings */
  else
    {
      if (view->font_binding != NULL)
        {
          g_binding_unbind (view->font_binding);
          view->font_binding = NULL;
        }

      MOUSEPAD_SETTING_BIND (FONT, view, "font", G_SETTINGS_BIND_GET);
    }
}



static void
mousepad_view_init (MousepadView *view)
{
  GApplication *application;

  /* initialize properties variables */
  view->font_binding = NULL;
  view->show_whitespace = FALSE;
  view->space_location_flags = GTK_SOURCE_SPACE_LOCATION_ALL;
  view->show_line_endings = FALSE;
  view->color_scheme = g_strdup ("none");
  view->match_braces = FALSE;

  /* make sure any buffers set on the view get the color scheme applied to them */
  g_signal_connect (view, "notify::buffer",
                    G_CALLBACK (mousepad_view_buffer_changed), NULL);

  /* bind Gsettings */
#define BIND_(setting, prop) \
  MOUSEPAD_SETTING_BIND (setting, view, prop, G_SETTINGS_BIND_GET)

  BIND_ (AUTO_INDENT,            "auto-indent");
  BIND_ (INDENT_ON_TAB,          "indent-on-tab");
  BIND_ (INDENT_WIDTH,           "indent-width");
  BIND_ (TAB_WIDTH,              "tab-width");
  BIND_ (INSERT_SPACES,          "insert-spaces-instead-of-tabs");
  BIND_ (SMART_BACKSPACE,        "smart-backspace");
  BIND_ (SMART_HOME_END,         "smart-home-end");
  BIND_ (SHOW_WHITESPACE,        "show-whitespace");
  BIND_ (SHOW_LINE_ENDINGS,      "show-line-endings");
  BIND_ (SHOW_LINE_MARKS,        "show-line-marks");
  BIND_ (SHOW_LINE_NUMBERS,      "show-line-numbers");
  BIND_ (SHOW_RIGHT_MARGIN,      "show-right-margin");
  BIND_ (RIGHT_MARGIN_POSITION,  "right-margin-position");
  BIND_ (HIGHLIGHT_CURRENT_LINE, "highlight-current-line");
  BIND_ (COLOR_SCHEME,           "color-scheme");
  BIND_ (WORD_WRAP,              "word-wrap");
  BIND_ (MATCH_BRACES,           "match-braces");

#undef BIND_

  /* bind the "font" property conditionally */
  mousepad_view_use_default_font (view);
  MOUSEPAD_SETTING_CONNECT_OBJECT (USE_DEFAULT_FONT, mousepad_view_use_default_font,
                                   view, G_CONNECT_SWAPPED);

  /* bind the whitespace display property to that of the application */
  application = g_application_get_default ();
  g_object_bind_property (application, "space-location", view, "space-location", G_BINDING_SYNC_CREATE);
}



static void
mousepad_view_finalize (GObject *object)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  /* cleanup color scheme name */
  g_free (view->color_scheme);

  (*G_OBJECT_CLASS (mousepad_view_parent_class)->finalize) (object);
}



static void
mousepad_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  MousepadView *view = MOUSEPAD_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT:
      mousepad_view_set_font (view, g_value_get_string (value));
      break;
    case PROP_SHOW_WHITESPACE:
      mousepad_view_set_show_whitespace (view, g_value_get_boolean (value));
      break;
    case PROP_SPACE_LOCATION:
      mousepad_view_set_space_location_flags (view, g_value_get_flags (value));
      break;
    case PROP_SHOW_LINE_ENDINGS:
      mousepad_view_set_show_line_endings (view, g_value_get_boolean (value));
      break;
    case PROP_COLOR_SCHEME:
      mousepad_view_set_color_scheme (view, g_value_get_string (value));
      break;
    case PROP_WORD_WRAP:
      mousepad_view_set_word_wrap (view, g_value_get_boolean (value));
      break;
    case PROP_MATCH_BRACES:
      mousepad_view_set_match_braces (view, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
mousepad_view_drag_motion (GtkWidget      *widget,
                           GdkDragContext *context,
                           gint            x,
                           gint            y,
                           guint           timestamp)
{
  GtkTargetList *target_list;
  gboolean       drop_zone;

  /* chain up to parent */
  drop_zone = GTK_WIDGET_CLASS (mousepad_view_parent_class)->drag_motion (widget, context,
                                                                          x, y, timestamp);

  /* enforce acceptance of our targets, especially when hovering over selections */
  target_list = gtk_target_list_new (drop_targets, G_N_ELEMENTS (drop_targets));
  if (gtk_drag_dest_find_target (widget, context, target_list) != GDK_NONE)
  {
    gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), timestamp);
    drop_zone = TRUE;
  }

  gtk_target_list_unref (target_list);

  return drop_zone;
}



static void
mousepad_view_cut_clipboard (GtkTextView *text_view)
{
  /* let GTK do the main job */
  GTK_TEXT_VIEW_CLASS (mousepad_view_parent_class)->cut_clipboard (text_view);

  /* scroll to cursor in our way */
  mousepad_view_scroll_to_cursor (MOUSEPAD_VIEW (text_view));
}



static void
mousepad_view_delete_from_cursor (GtkTextView   *text_view,
                                  GtkDeleteType  type,
                                  int            count)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter, start, end;
  GtkTextMark   *start_mark, *end_mark;
  gchar         *text = NULL, *eol;
  gint           line, column, n_lines;

  /* override only GTK_DELETE_PARAGRAPHS to make "win.edit.delete-line" work as expected */
  if (type == GTK_DELETE_PARAGRAPHS)
    {
      /* get iter at cursor */
      buffer = mousepad_view_get_buffer (MOUSEPAD_VIEW (text_view));
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));

      /* store current line and column numbers, treating end iter as a special case, to have
       * consistent cursor positions at undo/redo (the undo manager enforces this cursor
       * position at redo, so the best we can do is to enforce it also at undo) */
      line = gtk_text_iter_get_line (&iter);
      if (gtk_text_iter_is_end (&iter))
        column = -1;
      else
        column = mousepad_util_get_real_line_offset (&iter);

      /* begin a user action, playing with the undo manager to preserve cursor position */
      g_object_freeze_notify (G_OBJECT (buffer));
      gtk_text_buffer_begin_user_action (buffer);

      /* insert fake text, so that cursor position is restored when undoing the action */
      gtk_text_buffer_insert (buffer, &iter, "c", 1);

      /* delimit the line to delete, paragraph delimiter excluded */
      start = iter;
      gtk_text_iter_set_line_offset (&start, 0);
      end = start;
      gtk_text_iter_forward_to_line_end (&end);

      /* handle paragraph delimiter and final cursor position */
      if (G_LIKELY ((n_lines = gtk_text_buffer_get_line_count (buffer)) > 1))
        {
          /* reduce the last line case to the general case by reversing the last two lines */
          if (line == n_lines - 1)
            {
              /* mark deletion positions */
              start_mark = gtk_text_buffer_create_mark (buffer, NULL, &start, FALSE);
              end_mark = gtk_text_buffer_create_mark (buffer, NULL, &end, TRUE);

              /* copy the penultimate line, paragraph delimiter aside */
              gtk_text_buffer_get_iter_at_line (buffer, &start, line - 1);
              end = start;
              if (! gtk_text_iter_ends_line (&end))
                gtk_text_iter_forward_to_line_end (&end);

              text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
              iter = end;
              gtk_text_iter_forward_char (&end);
              eol = gtk_text_buffer_get_text (buffer, &iter, &end, TRUE);

              /* delete the penultimate line, paragraph delimiter included */
              gtk_text_buffer_delete (buffer, &start, &end);

              /* paste the penultimate line as last line (paragraph delimiter switched) */
              gtk_text_buffer_get_end_iter (buffer, &iter);
              gtk_text_buffer_insert (buffer, &iter, eol, -1);
              gtk_text_buffer_insert (buffer, &iter, text, -1);
              g_free (text);
              g_free (eol);

              /* restore deletion positions at their marks */
              gtk_text_buffer_get_iter_at_mark (buffer, &start, start_mark);
              gtk_text_buffer_get_iter_at_mark (buffer, &end, end_mark);
            }

          /* add the paragraph delimiter to the line to delete */
          gtk_text_iter_forward_char (&end);

          /* place cursor at its position after line deletion, preserving old position
           * as far as possible */
          mousepad_util_place_cursor (buffer, line + 1, column);

          /* finally, add the text portion up to the future cursor position to the text
           * to delete, so that restoring it afterwards will restore cursor position when
           * redoing the action */
          iter = end;
          gtk_text_buffer_get_iter_at_mark (buffer, &end, gtk_text_buffer_get_insert (buffer));
          text = gtk_text_buffer_get_text (buffer, &iter, &end, TRUE);
        }

      /* delete delimited text */
      gtk_text_buffer_delete (buffer, &start, &end);

      /* restore text to the left of the cursor */
      if (text != NULL)
        {
          gtk_text_buffer_insert_at_cursor (buffer, text, -1);
          g_free (text);
        }

      /* end user action */
      gtk_text_buffer_end_user_action (buffer);
      g_object_thaw_notify (G_OBJECT (buffer));

      return;
    }

  /* let GTK handle other cases */
  GTK_TEXT_VIEW_CLASS (mousepad_view_parent_class)->delete_from_cursor (text_view, type, count);
}



static void
mousepad_view_paste_clipboard (GtkTextView *text_view)
{
  /* let GTK do the main job */
  GTK_TEXT_VIEW_CLASS (mousepad_view_parent_class)->paste_clipboard (text_view);

  /* scroll to cursor in our way */
  mousepad_view_scroll_to_cursor (MOUSEPAD_VIEW (text_view));
}



static void
_mousepad_view_move_lines (GtkSourceView *source_view,
                           gboolean       down)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start, end, iter;
  gint           n_lines, start_line, end_line, start_char, end_char;
  gboolean       cursor_start = FALSE, inserted = FALSE;

  /* get selection lines and character offsets */
  buffer = mousepad_view_get_buffer (MOUSEPAD_VIEW (source_view));
  n_lines = gtk_text_buffer_get_line_count (buffer);
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  start_line = gtk_text_iter_get_line (&start);
  end_line = gtk_text_iter_get_line (&end);
  start_char = gtk_text_iter_get_line_offset (&start);
  end_char = gtk_text_iter_get_line_offset (&end);

  /* determine cursor position */
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
  if (gtk_text_iter_equal (&iter, &start))
    cursor_start = TRUE;

  /* begin a user action */
  g_object_freeze_notify (G_OBJECT (buffer));
  gtk_text_buffer_begin_user_action (buffer);

  /* insert fake text on last line if needed, to not make the last empty line a special case */
  if ((down && end_line == n_lines - 2 && (end_char != 0 || start_line == end_line))
      || (! down && start_line == n_lines - 1))
    {
      gtk_text_buffer_get_end_iter (buffer, &end);
      if (gtk_text_iter_get_chars_in_line (&end) == 0)
        {
          gtk_text_buffer_insert (buffer, &end, "c", 1);
          inserted = TRUE;
        }
    }

  /* compute new start/end lines */
  if (! down && start_line != 0)
    {
      start_line--;
      end_line--;
    }
  else if (down && end_line != n_lines - 1)
    {
      start_line++;
      end_line++;
    }

  /* let GSV move lines */
#if GTK_SOURCE_MAJOR_VERSION >= 4
  GTK_SOURCE_VIEW_CLASS (mousepad_view_parent_class)->move_lines (source_view, down);
#else
  GTK_SOURCE_VIEW_CLASS (mousepad_view_parent_class)->move_lines (source_view, FALSE, down ? 1 : -1);
#endif

  /* delete fake text */
  if (inserted)
    {
      down = !! down;
      gtk_text_buffer_get_iter_at_line_offset (buffer, &start, start_line - down, 0);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &end, start_line - down, 1);
      gtk_text_buffer_delete (buffer, &start, &end);
    }

  /* restore selection */
  gtk_text_buffer_get_iter_at_line_offset (buffer, &start, start_line, start_char);
  gtk_text_buffer_get_iter_at_line_offset (buffer, &end, end_line, end_char);
  if (cursor_start)
    gtk_text_buffer_select_range (buffer, &start, &end);
  else
    gtk_text_buffer_select_range (buffer, &end, &start);

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
  g_object_thaw_notify (G_OBJECT (buffer));
}



#if GTK_SOURCE_MAJOR_VERSION >= 4

static void
mousepad_view_move_lines (GtkSourceView *source_view,
                          gboolean       down)
{
  _mousepad_view_move_lines (source_view, down);
}

#else

static void
mousepad_view_move_lines (GtkSourceView *source_view,
                          gboolean       copy,
                          gint           count)
{
  _mousepad_view_move_lines (source_view, copy);
}

#endif



static void
mousepad_view_move_words (GtkSourceView *source_view,
                          gint           count)
{
  GtkTextBuffer *buffer, *test_buffer;
  GtkWidget     *test_view;
  GtkTextIter    start, end;
  gchar         *text;
  gint           n_chars, start_offset, end_offset;
  gboolean       succeed;

  /*
   * GSV sometimes fails to move words correctly, and removes content in these cases:
   * see https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/190.
   * Below, we first test in a virtual view if the operation succeeds, before letting
   * GSV operate on the real view.
   */

  /* get data to build the test view */
  buffer = mousepad_view_get_buffer (MOUSEPAD_VIEW (source_view));
  n_chars = gtk_text_buffer_get_char_count (buffer);
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

  gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_get_iter_at_mark (buffer, &end, gtk_text_buffer_get_selection_bound (buffer));
  start_offset = gtk_text_iter_get_offset (&start);
  end_offset = gtk_text_iter_get_offset (&end);

  /* build the test view */
  test_view = g_object_ref_sink (gtk_source_view_new ());
  test_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (test_view));
  gtk_text_buffer_set_text (test_buffer, text, -1);
  gtk_text_buffer_get_iter_at_offset (test_buffer, &start, start_offset);
  gtk_text_buffer_get_iter_at_offset (test_buffer, &end, end_offset);
  gtk_text_buffer_select_range (test_buffer, &start, &end);

  /* exit if GSV fails to move words correctly */
  g_signal_emit_by_name (test_view, "move-words", count);
  succeed = (gtk_text_buffer_get_char_count (test_buffer) == n_chars);
  g_object_unref (test_view);
  g_free (text);
  if (! succeed)
    return;

  /* let GSV move words */
  GTK_SOURCE_VIEW_CLASS (mousepad_view_parent_class)->move_words (source_view, count);
}



static void
mousepad_view_redo (GtkSourceView *source_view)
{
  /* let GSV do the main job */
  GTK_SOURCE_VIEW_CLASS (mousepad_view_parent_class)->redo (source_view);

  /* scroll to cursor in our way */
  mousepad_view_scroll_to_cursor (MOUSEPAD_VIEW (source_view));
}



static void
mousepad_view_undo (GtkSourceView *source_view)
{
  /* let GSV do the main job */
  GTK_SOURCE_VIEW_CLASS (mousepad_view_parent_class)->undo (source_view);

  /* scroll to cursor in our way */
  mousepad_view_scroll_to_cursor (MOUSEPAD_VIEW (source_view));
}



gboolean
mousepad_view_scroll_to_cursor (gpointer data)
{
  MousepadView  *view = data;
  GtkTextBuffer *buffer;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* scroll to visible area */
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
                                gtk_text_buffer_get_insert (buffer),
                                0.02, FALSE, 0.0, 0.0);

  return FALSE;
}



static void
mousepad_view_transpose_range (GtkTextBuffer *buffer,
                               GtkTextIter   *start_iter,
                               GtkTextIter   *end_iter)
{
  gchar *string, *reversed;
  gint   offset;

  /* store start iter line offset */
  offset = gtk_text_iter_get_offset (start_iter);

  /* get selected text */
  string = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);
  if (G_LIKELY (string))
    {
      /* reverse the string */
      reversed = g_utf8_strreverse (string, -1);

      /* only change the buffer then the string changed */
      if (G_LIKELY (reversed && strcmp (reversed, string) != 0))
        {
          /* delete the text between the iters */
          gtk_text_buffer_delete (buffer, start_iter, end_iter);

          /* insert the reversed string */
          gtk_text_buffer_insert (buffer, end_iter, reversed, -1);

          /* restore start iter */
          gtk_text_buffer_get_iter_at_offset (buffer, start_iter, offset);
        }

      /* cleanup */
      g_free (string);
      g_free (reversed);
    }
}



static void
mousepad_view_transpose_lines (GtkTextBuffer *buffer,
                               GtkTextIter   *start_iter,
                               GtkTextIter   *end_iter)
{
  GString *string;
  gint     start_line, end_line;
  gint     i;
  gchar   *slice;

  /* make sure the order is ok */
  gtk_text_iter_order (start_iter, end_iter);

  /* get the line numbers */
  start_line = gtk_text_iter_get_line (start_iter);
  end_line = gtk_text_iter_get_line (end_iter);

  /* new string */
  string = g_string_new (NULL);

  /* add the lines in reversed order to the string */
  for (i = start_line; i <= end_line && i < G_MAXINT; i++)
    {
      /* get start iter */
      gtk_text_buffer_get_iter_at_line (buffer, start_iter, i);

      /* set end iter */
      *end_iter = *start_iter;

      /* only prepend when the iters won't be equal */
      if (!gtk_text_iter_ends_line (end_iter))
        {
          /* move the iter to the end of this line */
          gtk_text_iter_forward_to_line_end (end_iter);

          /* prepend line */
          slice = gtk_text_buffer_get_slice (buffer, start_iter, end_iter, FALSE);
          string = g_string_prepend (string, slice);
          g_free (slice);
        }

      /* prepend new line */
      if (i < end_line)
        string = g_string_prepend_c (string, '\n');
    }

  /* get start iter again */
  gtk_text_buffer_get_iter_at_line (buffer, start_iter, start_line);

  /* delete selection */
  gtk_text_buffer_delete (buffer, start_iter, end_iter);

  /* insert reversed lines */
  gtk_text_buffer_insert (buffer, end_iter, string->str, string->len);

  /* cleanup */
  g_string_free (string, TRUE);

  /* restore start iter */
  gtk_text_buffer_get_iter_at_line (buffer, start_iter, start_line);
}



static void
mousepad_view_transpose_words (GtkTextBuffer *buffer,
                               GtkTextIter   *iter)
{
  GtkTextMark *left_mark, *right_mark;
  GtkTextIter  start_left, end_left, start_right, end_right;
  gchar       *word_left, *word_right;
  gboolean     restore_cursor;

  /* left word end */
  end_left = *iter;
  if (!mousepad_util_iter_backward_text_start (&end_left))
    return;

  /* left word start */
  start_left = end_left;
  if (!mousepad_util_iter_backward_word_start (&start_left))
    return;

  /* right word start */
  start_right = *iter;
  if (!mousepad_util_iter_forward_text_start (&start_right))
    return;

  /* right word end */
  end_right = start_right;
  if (!mousepad_util_iter_forward_word_end (&end_right))
    return;

  /* check if we selected something usefull */
  if (gtk_text_iter_get_line (&start_left) == gtk_text_iter_get_line (&end_right)
      && !gtk_text_iter_equal (&start_left, &end_left)
      && !gtk_text_iter_equal (&start_right, &end_right)
      && !gtk_text_iter_equal (&end_left, &start_right))
    {
      /* get the words */
      word_left = gtk_text_buffer_get_slice (buffer, &start_left, &end_left, FALSE);
      word_right = gtk_text_buffer_get_slice (buffer, &start_right, &end_right, FALSE);

      /* check if we need to restore the cursor afterwards */
      restore_cursor = gtk_text_iter_equal (iter, &start_right);

      /* insert marks at the start and end of the right word */
      left_mark = gtk_text_buffer_create_mark (buffer, NULL, &start_right, TRUE);
      right_mark = gtk_text_buffer_create_mark (buffer, NULL, &end_right, FALSE);

      /* delete the left word and insert the right word there */
      gtk_text_buffer_delete (buffer, &start_left, &end_left);
      gtk_text_buffer_insert (buffer, &start_left, word_right, -1);
      g_free (word_right);

      /* restore the right word iters */
      gtk_text_buffer_get_iter_at_mark (buffer, &start_right, left_mark);
      gtk_text_buffer_get_iter_at_mark (buffer, &end_right, right_mark);

      /* delete the right words and insert the left word there */
      gtk_text_buffer_delete (buffer, &start_right, &end_right);
      gtk_text_buffer_insert (buffer, &end_right, word_left, -1);
      g_free (word_left);

      /* restore the cursor if needed */
      if (restore_cursor)
        {
          /* restore the iter from the left mark (left gravity) */
          gtk_text_buffer_get_iter_at_mark (buffer, &start_right, left_mark);
          gtk_text_buffer_place_cursor (buffer, &start_right);
        }

      /* cleanup the marks */
      gtk_text_buffer_delete_mark (buffer, left_mark);
      gtk_text_buffer_delete_mark (buffer, right_mark);
    }
}



void
mousepad_view_transpose (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    sel_start, sel_end;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (gtk_text_buffer_get_selection_bounds (buffer, &sel_start, &sel_end))
    {
      /* if the selection is not on the same line, include the whole lines */
      if (gtk_text_iter_get_line (&sel_start) == gtk_text_iter_get_line (&sel_end))
        {
          /* reverse selection */
          mousepad_view_transpose_range (buffer, &sel_start, &sel_end);
        }
      else
        {
          /* reverse lines */
          mousepad_view_transpose_lines (buffer, &sel_start, &sel_end);
        }

      /* restore selection */
      gtk_text_buffer_select_range (buffer, &sel_end, &sel_start);
    }
  else
    {
      /* get cursor iter */
      gtk_text_buffer_get_iter_at_mark (buffer, &sel_start, gtk_text_buffer_get_insert (buffer));

      /* set end iter */
      sel_end = sel_start;

      if (gtk_text_iter_starts_line (&sel_start))
        {
          /* swap this line with the line above */
          if (gtk_text_iter_backward_line (&sel_end))
            {
              mousepad_view_transpose_lines (buffer, &sel_start, &sel_end);

              /* place cursor in its original position */
              gtk_text_iter_set_line_offset (&sel_end, 0);
              gtk_text_buffer_place_cursor (buffer, &sel_end);
            }
        }
      else if (gtk_text_iter_ends_line (&sel_start))
        {
          /* swap this line with the line below */
          if (gtk_text_iter_forward_line (&sel_end) || gtk_text_iter_starts_line (&sel_end))
            {
              mousepad_view_transpose_lines (buffer, &sel_start, &sel_end);

              /* for empty line place cursor at the start of just swapped line */
              if (gtk_text_iter_ends_line (&sel_start))
                gtk_text_iter_forward_line (&sel_start);
              /* for non-empty line place cursor in its original position */
              else
                gtk_text_iter_forward_to_line_end (&sel_start);

              gtk_text_buffer_place_cursor (buffer, &sel_start);
            }
        }
      else if (mousepad_util_iter_inside_word (&sel_start))
        {
          /* reverse the characters before and after the cursor */
          if (gtk_text_iter_backward_char (&sel_start) && gtk_text_iter_forward_char (&sel_end))
            {
              mousepad_view_transpose_range (buffer, &sel_start, &sel_end);

              /* place cursor in its original position */
              gtk_text_iter_backward_char (&sel_end);
              gtk_text_buffer_place_cursor (buffer, &sel_end);
            }
        }
      else
        {
          /* swap the words left and right of the cursor */
          mousepad_view_transpose_words (buffer, &sel_start);
        }
    }

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);
}



void
mousepad_view_custom_paste (MousepadView *view,
                            const gchar  *string)
{
  GtkClipboard   *clipboard;
  GtkTextBuffer  *buffer;
  GtkTextMark    *mark;
  GtkTextIter     iter, start_iter, end_iter;
  gchar         **pieces;
  gchar          *text = NULL;
  gint            i, column;

  /* leave when the view is not editable */
  if (! gtk_text_view_get_editable (GTK_TEXT_VIEW (view)))
    return;

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin user action */
  gtk_text_buffer_begin_user_action (buffer);

  if (string == NULL)
    {
      /* get the clipboard */
      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD);

      /* get the clipboard text */
      text = gtk_clipboard_wait_for_text (clipboard);

      /* leave when the text is null */
      if (G_UNLIKELY (text == NULL))
        return;

      /* chop the string into pieces */
      pieces = g_strsplit (text, "\n", -1);

      /* get iter at cursor position */
      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

      /* get the iter column */
      column = mousepad_util_get_real_line_offset (&iter);

      /* insert the pieces in the buffer */
      for (i = 0; pieces[i] != NULL; i++)
        {
          /* insert the text in the buffer */
          gtk_text_buffer_insert (buffer, &iter, pieces[i], -1);

          /* break if the next piece is null */
          if (G_UNLIKELY (pieces[i+1] == NULL))
            break;

          /* move the iter to the next line */
          if (!gtk_text_iter_forward_line (&iter))
            {
              /* no new line, insert a new line */
              gtk_text_buffer_insert (buffer, &iter, "\n", 1);
            }
          else
            {
              /* set the iter at the column offset */
              mousepad_util_set_real_line_offset (&iter, column, FALSE);
            }
        }

      /* cleanup */
      g_free (text);
      g_strfreev (pieces);

      /* set the cursor to the last iter position */
      gtk_text_buffer_place_cursor (buffer, &iter);
    }
  else
    {
      /* get selection bounds */
      gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);

      /* remove the existing selection if the iters are not equal */
      if (!gtk_text_iter_equal (&start_iter, &end_iter))
        gtk_text_buffer_delete (buffer, &start_iter, &end_iter);

      /* insert string */
      gtk_text_buffer_insert (buffer, &start_iter, string, -1);
    }

  /* end user action */
  gtk_text_buffer_end_user_action (buffer);

  /* put cursor on screen */
  mousepad_view_scroll_to_cursor (view);
}



void
mousepad_view_convert_spaces_and_tabs (MousepadView *view,
                                       gint          type)
{
  GtkTextBuffer *buffer;
  GtkTextMark   *mark;
  GtkTextIter    start_iter, end_iter;
  GtkTextIter    iter;
  gint           tab_size;
  gboolean       in_range = FALSE;
  gboolean       no_forward;
  gunichar       c;
  gint           offset;
  gint           n_spaces = 0;
  gint           start_offset = -1;
  gchar         *string;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get the tab size */
  tab_size = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));

  /* get the start and end iter */
  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      /* move to the start of the line when replacing spaces */
      if (type == SPACES_TO_TABS && !gtk_text_iter_starts_line (&start_iter))
        gtk_text_iter_set_line_offset (&start_iter, 0);

      /* offset for restoring the selection afterwards */
      start_offset = gtk_text_iter_get_offset (&start_iter);
    }
  else
    {
      /* get the document bounds */
      gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
    }

  /* leave when the iters are equal (empty docs) */
  if (gtk_text_iter_equal (&start_iter, &end_iter))
    return;

  /* begin a user action and free notifications */
  g_object_freeze_notify (G_OBJECT (buffer));
  gtk_text_buffer_begin_user_action (buffer);

  /* create a mark to restore the end iter after modifieing the buffer */
  mark = gtk_text_buffer_create_mark (buffer, NULL, &end_iter, FALSE);

  /* walk the text between the iters */
  for (;;)
    {
      /* get the character */
      c = gtk_text_iter_get_char (&start_iter);

      /* reset */
      no_forward = FALSE;

      if (type == SPACES_TO_TABS)
        {
          if (c == ' ' || in_range)
            {
              if (! in_range)
                {
                  /* set the start iter */
                  iter = start_iter;

                  /* get the real offset of the start iter */
                  offset = mousepad_util_get_real_line_offset (&iter);

                  /* the number of spaces to inline with the tabs */
                  n_spaces = tab_size - offset % tab_size;
                }

              /* check if we can already insert a tab */
              if (n_spaces == 0)
                {
                  /* delete the selected spaces */
                  gtk_text_buffer_delete (buffer, &iter, &start_iter);
                  gtk_text_buffer_insert (buffer, &start_iter, "\t", 1);

                  /* restore the end iter */
                  gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, mark);

                  /* stop being inside a range */
                  in_range = FALSE;

                  /* no need to forward (iter moved after the insert */
                  no_forward = TRUE;
                }
              else
                {
                  /* check whether we're still in a range */
                  in_range = (c == ' ');
                }

              /* decrease counter */
              n_spaces--;
            }

          /* go to next line if we reached text */
          if (!g_unichar_isspace (c))
            {
              /* continue to the next line if we hit non spaces */
              gtk_text_iter_forward_line (&start_iter);

              /* make sure there is no valid range anymore */
              in_range = FALSE;

              /* no need to forward */
              no_forward = TRUE;
            }
        }
      else if (type == TABS_TO_SPACES && c == '\t')
        {
          /* get the real offset of the iter */
          offset = mousepad_util_get_real_line_offset (&start_iter);

          /* the number of spaces to inline with the tabs */
          n_spaces = tab_size - offset % tab_size;

          /* move one character forwards */
          iter = start_iter;
          gtk_text_iter_forward_char (&start_iter);

          /* delete the tab */
          gtk_text_buffer_delete (buffer, &iter, &start_iter);

          /* create a string with the number of spaces */
          string = g_strnfill (n_spaces, ' ');

          /* insert the spaces */
          gtk_text_buffer_insert (buffer, &start_iter, string, n_spaces);

          /* cleanup */
          g_free (string);

          /* restore the end iter */
          gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, mark);

          /* iter already moved by the insert */
          no_forward = TRUE;
        }

      /* break when the iters are equal (or start is bigger) */
      if (gtk_text_iter_compare (&start_iter, &end_iter) >= 0)
        break;

      /* forward the iter */
      if (G_LIKELY (! no_forward))
        gtk_text_iter_forward_char (&start_iter);
    }

  /* delete our mark */
  gtk_text_buffer_delete_mark (buffer, mark);

  /* restore the selection if needed */
  if (start_offset > -1)
    {
      /* restore iter and select range */
      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start_offset);
      gtk_text_buffer_select_range (buffer, &end_iter, &start_iter);
    }

  /* end the user action */
  gtk_text_buffer_end_user_action (buffer);
  g_object_thaw_notify (G_OBJECT (buffer));
}



void
mousepad_view_strip_trailing_spaces (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter, needle;
  gint           start, end, i;
  gunichar       c;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* get range in line numbers */
  if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
    {
      start = gtk_text_iter_get_line (&start_iter);
      end = gtk_text_iter_get_line (&end_iter) + 1;
    }
  else
    {
      start = 0;
      end = gtk_text_buffer_get_line_count (buffer);
    }

  /* begin a user action and free notifications */
  g_object_freeze_notify (G_OBJECT (buffer));
  gtk_text_buffer_begin_user_action (buffer);

  /* walk all the selected lines */
  for (i = start; i < end; i++)
    {
      /* get the iter at the line */
      gtk_text_buffer_get_iter_at_line (buffer, &end_iter, i);

      /* continue if the line is empty */
      if (gtk_text_iter_ends_line (&end_iter))
        continue;

      /* move the iter to the end of the line */
      gtk_text_iter_forward_to_line_end (&end_iter);

      /* initialize the iters */
      needle = start_iter = end_iter;

      /* walk backwards until we hit something else then a space or tab */
      while (gtk_text_iter_backward_char (&needle))
        {
          /* get the character */
          c = gtk_text_iter_get_char (&needle);

          /* set the start iter of the spaces range or stop searching */
          if (c == ' ' || c == '\t')
            start_iter = needle;
          else
            break;
        }

      /* remove the spaces/tabs if the iters are not equal */
      if (!gtk_text_iter_equal (&start_iter, &end_iter))
        gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
    }

  /* end the user action */
  gtk_text_buffer_end_user_action (buffer);
  g_object_thaw_notify (G_OBJECT (buffer));
}



void
mousepad_view_duplicate (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start_iter, end_iter;
  gboolean       has_selection;
  gboolean       insert_eol = FALSE;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* get the buffer */
  buffer = mousepad_view_get_buffer (view);

  /* begin a user action */
  gtk_text_buffer_begin_user_action (buffer);

  /* get iters */
  has_selection = gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);

  /* select entire line */
  if (! has_selection)
    {
      /* set to the start of the line */
      if (!gtk_text_iter_starts_line (&start_iter))
        gtk_text_iter_set_line_offset (&start_iter, 0);

      /* move the other iter to the start of the next line */
      insert_eol = !gtk_text_iter_forward_line (&end_iter);
    }

  /* insert a dupplicate of the text before the iter */
  gtk_text_buffer_insert_range (buffer, &start_iter, &start_iter, &end_iter);

  /* insert a new line if needed */
  if (insert_eol)
    gtk_text_buffer_insert (buffer, &start_iter, "\n", 1);

  /* end the action */
  gtk_text_buffer_end_user_action (buffer);
}



gint
mousepad_view_get_selection_length (MousepadView *view)
{
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  gint           length = 0;

  g_return_val_if_fail (MOUSEPAD_IS_VIEW (view), FALSE);

  /* get the text buffer */
  buffer = mousepad_view_get_buffer (view);

  /* calculate the selection length from the iter offset (absolute) */
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    length = ABS (gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&start));

  return length;
}



static void
mousepad_view_set_font (MousepadView *view,
                        const gchar  *font)
{
  PangoFontDescription *font_desc;
  GtkCssProvider       *provider;
  gchar                *css_font_desc, *css_string;

  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  /* from font string to css string through pango description */
  font_desc = pango_font_description_from_string (font);
  css_font_desc = mousepad_util_pango_font_description_to_css (font_desc);
  css_string = g_strdup_printf ("textview { %s }", css_font_desc);

  /* set font */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css_string, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (view)),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* cleanup */
  g_object_unref (provider);
  pango_font_description_free (font_desc);
  g_free (css_font_desc);
  g_free (css_string);
}



static void
mousepad_view_update_draw_spaces (MousepadView *view)
{
  GtkSourceSpaceTypeFlags      type_flags = GTK_SOURCE_SPACE_TYPE_NONE;
  GtkSourceSpaceLocationFlags  flag;
  GtkSourceSpaceDrawer        *drawer;
  gboolean                     enable_matrix = FALSE;

  drawer = gtk_source_view_get_space_drawer (GTK_SOURCE_VIEW (view));

  if (view->show_whitespace)
    {
      type_flags |= GTK_SOURCE_SPACE_TYPE_SPACE | GTK_SOURCE_SPACE_TYPE_TAB
                    | GTK_SOURCE_SPACE_TYPE_NBSP;
      enable_matrix = TRUE;

      for (flag = 1; flag < GTK_SOURCE_SPACE_LOCATION_ALL; flag <<= 1)
        if (view->space_location_flags & flag)
          gtk_source_space_drawer_set_types_for_locations (drawer, flag, type_flags);
        else
          gtk_source_space_drawer_set_types_for_locations (drawer, flag, GTK_SOURCE_SPACE_TYPE_NONE);
    }
  else
    gtk_source_space_drawer_set_types_for_locations (drawer, GTK_SOURCE_SPACE_LOCATION_ALL,
                                                     GTK_SOURCE_SPACE_TYPE_NONE);

  if (view->show_line_endings)
    {
      enable_matrix = TRUE;
      if (view->space_location_flags & GTK_SOURCE_SPACE_LOCATION_TRAILING)
        type_flags |= GTK_SOURCE_SPACE_TYPE_NEWLINE;
      else
        type_flags = GTK_SOURCE_SPACE_TYPE_NEWLINE;

      gtk_source_space_drawer_set_types_for_locations (drawer, GTK_SOURCE_SPACE_LOCATION_TRAILING,
                                                       type_flags);
    }

  gtk_source_space_drawer_set_enable_matrix (drawer, enable_matrix);
}



static void
mousepad_view_set_show_whitespace (MousepadView *view,
                                   gboolean      show)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->show_whitespace = show;
  mousepad_view_update_draw_spaces (view);
}



static void
mousepad_view_set_space_location_flags (MousepadView *view,
                                        gulong        flags)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->space_location_flags = flags;
  mousepad_view_update_draw_spaces (view);
}



static void
mousepad_view_set_show_line_endings (MousepadView *view,
                                     gboolean      show)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->show_line_endings = show;
  mousepad_view_update_draw_spaces (view);
}



static void
mousepad_view_set_color_scheme (MousepadView *view,
                                const gchar  *color_scheme)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  if (g_strcmp0 (color_scheme, view->color_scheme) != 0)
    {
      g_free (view->color_scheme);
      view->color_scheme = g_strdup (color_scheme);

      /* update the buffer if there is one */
      mousepad_view_buffer_changed (view, NULL, NULL);
    }
}



static void
mousepad_view_set_word_wrap (MousepadView *view,
                             gboolean      enabled)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view),
                               enabled ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
}



static void
mousepad_view_set_match_braces (MousepadView *view,
                                gboolean      enabled)
{
  g_return_if_fail (MOUSEPAD_IS_VIEW (view));

  view->match_braces = enabled;

  mousepad_view_buffer_changed (view, NULL, NULL);
}
