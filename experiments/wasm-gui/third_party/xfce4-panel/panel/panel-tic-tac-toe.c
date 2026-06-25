/*
 * Copyright (C) 2010 Nick Schermer <nick@xfce.org>
 *
 * The game code has been taken (with adjustments) from the javascript
 * game of Stephen Ostermiller: http://ostermiller.org/calc/tictactoe.html.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include <common/panel-private.h>
#include <panel/panel-tic-tac-toe.h>



static void panel_tic_tac_toe_response       (GtkDialog      *dialog,
                                              gint            response_id);
static void panel_tic_tac_toe_move           (PanelTicTacToe *dialog);
static void panel_tic_tac_toe_button_clicked (GtkWidget      *button,
                                              PanelTicTacToe *dialog);
static void panel_tic_tac_toe_new_game       (PanelTicTacToe *dialog);



#define cells_to_hex(c1,c2,c3,c4,c5,c6,c7,c8,c9)  (c1 << 0 | c2 << 1 | c3 << 2 | \
                                                   c4 << 3 | c5 << 4 | c6 << 5 | \
                                                   c7 << 6 | c8 << 7 | c9 << 8)
#define cells_to_hex2(c1,c2,c3,c4,c5,c6,c7,c8,c9) (c1 << 0 | c2 << 2 | c3 << 4 | \
                                                   c4 << 6 | c5 << 8 | c6 << 10 | \
                                                   c7 << 12 | c8 << 14 | c9 << 16)
#define winner_hex(winner,state)                  ((((winner) - 1) << 18) | (state))



struct _PanelTicTacToeClass
{
  XfceTitledDialogClass __parent__;
};

struct _PanelTicTacToe
{
  XfceTitledDialog  __parent__;

  GtkWidget *buttons[9];
  GtkWidget *labels[9];
  GtkWidget *level;
};

enum
{
  LEVEL_NOVICE,
  LEVEL_INTERMEDIATE,
  LEVEL_EXPERIENCED,
  LEVEL_EXPERT
};

enum
{
  ONE      = 1,
  PLAYER_O = 2,
  PLAYER_X = 3,
  TIE      = 4
};



G_DEFINE_TYPE (PanelTicTacToe, panel_tic_tac_toe, XFCE_TYPE_TITLED_DIALOG)



static void
panel_tic_tac_toe_class_init (PanelTicTacToeClass *klass)
{
  GtkDialogClass *gtkdialog_class;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = panel_tic_tac_toe_response;
}



static void
panel_tic_tac_toe_init (PanelTicTacToe *dialog)
{
  GtkWidget *button;
  GtkWidget *grid;
  GtkWidget *separator;
  guint      i;
  GtkWidget *label;
  guint      row, col;
  GtkWidget *vbox;
  GtkWidget *combo;
  GtkWidget *hbox;

  gtk_window_set_title (GTK_WINDOW (dialog), "Tic-tac-toe");
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "applications-games");

  xfce_titled_dialog_create_action_area (XFCE_TITLED_DIALOG (dialog));

  button = xfce_gtk_button_new_mixed ("document-new", _("_New Game"));
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);
  xfce_titled_dialog_add_button (XFCE_TITLED_DIALOG (dialog), _("_Close"), GTK_RESPONSE_CLOSE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("_Level:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  dialog->level = combo = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Novice"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Intermediate"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Experienced"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Expert"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), LEVEL_EXPERIENCED);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 1);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 1);
  gtk_container_add (GTK_CONTAINER (vbox), grid);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 0, 1, 5, 1);
  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 0, 3, 5, 1);
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 1, 0, 1, 5);
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 3, 0, 1, 5);

  for (i = 0; i < 9; i++)
    {
      button = dialog->buttons[i] = gtk_button_new ();
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_widget_set_size_request (button, 70, 70);
      gtk_widget_set_can_default (button, FALSE);
      gtk_widget_set_can_focus (button, FALSE);
      g_signal_connect (G_OBJECT (button), "clicked",
          G_CALLBACK (panel_tic_tac_toe_button_clicked), dialog);

      label = dialog->labels[i] = gtk_label_new ("");
      gtk_container_add (GTK_CONTAINER (button), label);

      row = (i / 3) * 2;
      col = (i % 3) * 2;

      gtk_grid_attach (GTK_GRID (grid), button,
                       col, row, 1, 1);
    }

  /* set label attributes */
  panel_tic_tac_toe_new_game (dialog);
}



static void
panel_tic_tac_toe_response (GtkDialog *gtk_dialog,
                            gint       response_id)
{
  panel_return_if_fail (PANEL_IS_TIC_TAC_TOE (gtk_dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      /* new game */
      panel_tic_tac_toe_new_game (PANEL_TIC_TAC_TOE (gtk_dialog));
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (gtk_dialog));
    }
}



static gint
panel_tic_tac_toe_has_winner (gint state)
{
  guint i;
  gint  player_x, player_o, tie;
  gint  matches[] =
  {
    /* horizontal */
    cells_to_hex2 (ONE, ONE, ONE, 0, 0, 0, 0, 0, 0),
    cells_to_hex2 (0, 0, 0, ONE, ONE, ONE, 0, 0, 0),
    cells_to_hex2 (0, 0, 0, 0, 0, 0, ONE, ONE, ONE),

    /* vertical */
    cells_to_hex2 (ONE, 0, 0, ONE, 0, 0, ONE, 0, 0),
    cells_to_hex2 (0, ONE, 0, 0, ONE, 0, 0, ONE, 0),
    cells_to_hex2 (0, 0, ONE, 0, 0, ONE, 0, 0, ONE),

    /* diagonal */
    cells_to_hex2 (ONE, 0, 0, 0, ONE, 0, 0, 0, ONE),
    cells_to_hex2 (0, 0, ONE, 0, ONE, 0, ONE, 0, 0),
  };

  for (i = 0; i < G_N_ELEMENTS (matches); i++)
    {
      /* x wins */
      player_x = matches[i] * PLAYER_X;
      if ((state & player_x) == player_x)
        return winner_hex (PLAYER_X, player_x);

      /* o wins */
      player_o = matches[i] * PLAYER_O;
      if ((state & player_x) == player_o)
        return winner_hex (PLAYER_O, player_o);
    }

  /* tie */
  tie = cells_to_hex2 (PLAYER_O, PLAYER_O, PLAYER_O,
                       PLAYER_O, PLAYER_O, PLAYER_O,
                       PLAYER_O, PLAYER_O, PLAYER_O);
  if ((state & tie) == tie)
    return winner_hex (TIE, 0);

  /* no winner or tie */
  return 0;
}



static gint
panel_tic_tac_toe_best_opening (gint state,
                                gint possible_initial_moves)
{
  gint mask;

  /* no moves yet, return the valid initial moves */
  if (state == 0)
    return possible_initial_moves;

  mask = state & cells_to_hex2 (PLAYER_O, PLAYER_O, PLAYER_O,
                                PLAYER_O, PLAYER_O, PLAYER_O,
                                PLAYER_O, PLAYER_O, PLAYER_O);

  /* user took the center, pick a corner */
  if (mask == cells_to_hex2 (0, 0, 0, 0, PLAYER_O, 0, 0, 0, 0))
    return cells_to_hex (ONE, 0, ONE, 0, 0, 0, ONE, 0, ONE);

  /* user took a corner, pick the center */
  if (mask == cells_to_hex2 (PLAYER_O, 0, 0, 0, 0, 0, 0, 0, 0)
      || mask == cells_to_hex2 (0, 0, PLAYER_O, 0, 0, 0, 0, 0, 0)
      || mask == cells_to_hex2 (0, 0, 0, 0, 0, 0, PLAYER_O, 0, 0)
      || mask == cells_to_hex2 (0, 0, 0, 0, 0, 0, 0, 0, PLAYER_O))
    return cells_to_hex (0, 0, 0, 0, ONE, 0, 0, 0, 0);

  /* user took an edge, pick something in the same diagonal */
  if (mask == cells_to_hex2 (0, PLAYER_O, 0, 0, 0, 0, 0, 0, 0))
    return cells_to_hex (ONE, 0, ONE, 0, ONE, 0, 0, ONE, 0);
  if (mask == cells_to_hex2 (0, 0, 0, PLAYER_O, 0, 0, 0, 0, 0))
    return cells_to_hex (ONE, 0, 0, 0, ONE, ONE, ONE, 0, 0);
  if (mask == cells_to_hex2 (0, 0, 0, 0, 0, PLAYER_O, 0, 0, 0))
    return cells_to_hex (0, 0, ONE, ONE, ONE, 0, 0, 0, ONE);
  if (mask == cells_to_hex2 (0, 0, 0, 0, 0, 0, 0, PLAYER_O, 0))
    return cells_to_hex (0, ONE, 0, 0, ONE, 0, ONE, 0, ONE);

  /* no best opening move possible */
  return 0;
}



static gint
panel_tic_tac_toe_get_random_move (gint moves)
{
  gint i;
  gint n_moves;
  gint seed;

  /* count number of moves in the state */
  for (i = 0, n_moves = 0; i < 9; i++)
    if ((moves & (1 << i)) != 0)
      n_moves++;

  if (n_moves > 0)
    {
      /* return one random move from the state */
      seed = g_random_int_range (0, n_moves);
      for (i = 0, n_moves = 0; i < 9; i++)
        {
          if ((moves & (1 << i)) != 0
              && n_moves++ == seed)
            return i;
        }

      panel_assert_not_reached ();
    }

  /* no move possible */
  return -1;
}



static gint
panel_tic_tac_toe_get_legal_moves (gint state)
{
  gint i, moves = 0;

  for (i = 0; i < 9; i++)
    if ((state & (1 << (i * 2 + 1))) == 0)
      moves |= 1 << i;

  return moves;
}



static gint
panel_tic_tac_toe_get_winner_move (gint state,
                                   gint player)
{
  gint i;

  for (i = 0; i < 9; i++)
    if ((state & (1 << (i * 2 + 1))) == 0)
      if (panel_tic_tac_toe_has_winner (state | (player << (i * 2))) != 0)
        return 1 << i;

  return 0;
}



static gint
panel_tic_tac_toe_get_move_rate (gint state,
                                 gint move,
                                 gint move_for,
                                 gint next_turn,
                                 gint limit,
                                 gint depth)
{
  gint new_state;
  gint winner;
  gint rate;
  gint legal_moves, i;
  gint value;

  new_state = state | (next_turn << (move * 2));

  /* check if this new state has a winner or tie */
  winner = panel_tic_tac_toe_has_winner (new_state);
  if (winner == winner_hex (TIE, 0))
    return 0;
  if (winner != 0)
    {
      if (move_for == next_turn)
        return 10 - depth;
      else
        return depth - 10;
    }

  rate = move_for == next_turn ? 999 : -999;
  if (depth == limit)
    return rate;

  legal_moves = panel_tic_tac_toe_get_legal_moves (new_state);
  for (i = 0; i < 9; i++)
    {
      if ((legal_moves & (1 << i)) != 0)
        {
          value = panel_tic_tac_toe_get_move_rate (new_state, i, move_for,
                                                   next_turn == PLAYER_O ? PLAYER_X : PLAYER_O,
                                                   10 - ABS (rate), depth + 1);
          if (ABS (value) != 999)
            {
              if ((move_for == next_turn && value < rate)
                  || (move_for != next_turn && value > rate))
                rate = value;
            }
        }
    }

  return rate;
}



static gint
panel_tic_tac_toe_get_move (gint state,
                            gint player,
                            gint level)
{
  gint winner;
  gint moves = 0, legal_moves;
  gint first_moves;
  gint rate, value;
  gint i;
  gint move = -1;

  winner = panel_tic_tac_toe_has_winner (state);
  if (winner == 0)
    {
      legal_moves = panel_tic_tac_toe_get_legal_moves (state);

      switch (level)
        {
        case LEVEL_EXPERT:
          /* just take a random position on the first move */
          first_moves = cells_to_hex (ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE);
          moves = panel_tic_tac_toe_best_opening (state, first_moves);
          if (moves != 0)
            break;

          /* find the moves with the best rate */
          rate = -999;
          for (i = 0; i < 9; i++)
            {
               if ((legal_moves & (1 << i)) == 0)
                 continue;

               value = panel_tic_tac_toe_get_move_rate (state, i, player, player, 15, 1);
               if (value > rate)
                 {
                   /* found a better game, reset moves */
                   rate = value;
                   moves = 0;
                 }

               /* add the moves with the current rating */
               if (rate == value)
                 moves |= (1 << i);

            }
          break;

        case LEVEL_EXPERIENCED:
          /* take a corner on the first move */
          first_moves = cells_to_hex (ONE, 0, ONE, 0, 0, 0, ONE, 0, ONE);
          moves = panel_tic_tac_toe_best_opening (state, first_moves);
          if (moves != 0)
            break;
          /* else fall through */

        case LEVEL_INTERMEDIATE:
          /* try to find a winning move */
          moves = panel_tic_tac_toe_get_winner_move (state, PLAYER_X);
          if (moves != 0)
            break;
          /* else fall through */

          /* try to find a blocking move */
          moves = panel_tic_tac_toe_get_winner_move (state, PLAYER_O);
          if (moves != 0)
            break;
          /* else fall through */

        case LEVEL_NOVICE:
          moves = legal_moves;
          break;
        }

      move = panel_tic_tac_toe_get_random_move (moves);
    }

  return move;
}



static gint
panel_tic_tac_toe_get_state (PanelTicTacToe *dialog)
{
  gint         state = 0;
  gint         i;
  const gchar *text;
  gint         value;

  panel_return_val_if_fail (PANEL_IS_TIC_TAC_TOE (dialog), 0);

  for (i = 0; i < 9; i++)
    {
      text = gtk_label_get_text (GTK_LABEL (dialog->labels[i]));

      if (text != NULL && *text == 'X')
        value = PLAYER_X;
      else if (text != NULL && *text == 'O')
        value = PLAYER_O;
      else
        continue;

      state |= value << (i * 2);
    }

  return state;
}



static void
panel_tic_tac_toe_move (PanelTicTacToe *dialog)
{
  gint move;
  gint state;
  gint level;

  panel_return_if_fail (PANEL_IS_TIC_TAC_TOE (dialog));

  state = panel_tic_tac_toe_get_state (dialog);
  level = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->level));
  move = panel_tic_tac_toe_get_move (state, PLAYER_X, level);
  if (move != -1)
    {
      gtk_widget_set_sensitive (dialog->buttons[move], FALSE);
      gtk_label_set_text (GTK_LABEL (dialog->labels[move]), "X");
    }


}



static void
panel_tic_tac_toe_highlight_winner (PanelTicTacToe *dialog,
                                    gint            winner)
{
  PangoAttribute *attr;
  PangoAttrList  *attrs;
  gint            i;
  gint            tie;

  panel_return_if_fail (PANEL_IS_TIC_TAC_TOE (dialog));

  tie = winner_hex (TIE, 0);
  if ((winner & tie) == winner_hex (PLAYER_X, 0))
    {
      attr = pango_attr_foreground_new (0xffff, 0, 0);
    }
  else if ((winner & tie) == winner_hex (PLAYER_O, 0))
    {
      attr = pango_attr_foreground_new (0, 0, 0xffff);
    }
  else
    {
      /* grey out all the cells */
      attr = pango_attr_foreground_new (0x4444, 0x4444, 0x4444);
      winner |= cells_to_hex2 (PLAYER_O, PLAYER_O, PLAYER_O,
                               PLAYER_O, PLAYER_O, PLAYER_O,
                               PLAYER_O, PLAYER_O, PLAYER_O);
    }

  for (i = 0; i < 9; i++)
    {
      gtk_widget_set_sensitive (dialog->buttons[i], FALSE);

      if ((winner & (1 << (i * 2 + 1))) != 0)
        {
          attrs = gtk_label_get_attributes (GTK_LABEL (dialog->labels[i]));
          pango_attr_list_insert (attrs, pango_attribute_copy (attr));
          gtk_label_set_attributes (GTK_LABEL (dialog->labels[i]), attrs);
        }
    }

  pango_attribute_destroy (attr);
}



static void
panel_tic_tac_toe_button_clicked (GtkWidget      *button,
                                  PanelTicTacToe *dialog)
{
  GtkWidget *label;
  gint       state;
  gint       winner;

  panel_return_if_fail (PANEL_IS_TIC_TAC_TOE (dialog));

  label = gtk_bin_get_child (GTK_BIN (button));
  gtk_widget_set_sensitive (button, FALSE);
  gtk_label_set_text (GTK_LABEL (label), "O");

  panel_tic_tac_toe_move (dialog);

  state = panel_tic_tac_toe_get_state (dialog);
  winner = panel_tic_tac_toe_has_winner (state);
  if (winner != 0)
    panel_tic_tac_toe_highlight_winner (dialog, winner);
}



static void
panel_tic_tac_toe_new_game (PanelTicTacToe *dialog)
{
  PangoAttrList  *attrs, *attrs_dup;
  PangoAttribute *attr;
  gint            i;

  attrs = pango_attr_list_new ();
  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  pango_attr_list_insert (attrs, attr);
  attr = pango_attr_size_new (36 * PANGO_SCALE);
  pango_attr_list_insert (attrs, attr);
  attr = pango_attr_foreground_new (0, 0, 0);
  pango_attr_list_insert (attrs, attr);

  for (i = 0; i < 9; i++)
    {
      gtk_label_set_text (GTK_LABEL (dialog->labels[i]), "");
      gtk_widget_set_sensitive (dialog->buttons[i], TRUE);

      attrs_dup = pango_attr_list_copy (attrs);
      gtk_label_set_attributes (GTK_LABEL (dialog->labels[i]), attrs_dup);
      pango_attr_list_unref (attrs_dup);
    }

  pango_attr_list_unref (attrs);

  /* 50/50 the panel will start the game */
  if (g_random_int_range (0, 2) == 0)
    panel_tic_tac_toe_move (dialog);
}



void
panel_tic_tac_toe_show (void)
{
  GtkWidget *dialog;

  dialog = g_object_new (PANEL_TYPE_TIC_TAC_TOE, NULL);
  gtk_widget_show_all (dialog);
}
