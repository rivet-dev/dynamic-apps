/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-compact-view.h>



static AtkObject   *thunar_compact_view_get_accessible (GtkWidget               *widget);



struct _ThunarCompactViewClass
{
  ThunarAbstractIconViewClass __parent__;
};

struct _ThunarCompactView
{
  ThunarAbstractIconView __parent__;
};



G_DEFINE_TYPE (ThunarCompactView, thunar_compact_view, THUNAR_TYPE_ABSTRACT_ICON_VIEW)



static void
thunar_compact_view_class_init (ThunarCompactViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_accessible = thunar_compact_view_get_accessible;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->zoom_level_property_name = "last-compact-view-zoom-level";

  /* override ThunarAbstractIconView default row spacing */
  gtk_widget_class_install_style_property(gtkwidget_class, g_param_spec_int (
          "row-spacing",                //name
          "row-spacing",                //nick
          "space between rows in px",   //blurb
          0,                            //min
          100,                          //max
          0,                            //default
          G_PARAM_READWRITE));         //flags
}



static void
thunar_compact_view_init (ThunarCompactView *compact_view)
{
  gboolean max_chars;

 /* initialize the icon view properties */
  exo_icon_view_set_margin (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (compact_view))), 3);
  exo_icon_view_set_layout_mode (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (compact_view))), EXO_ICON_VIEW_LAYOUT_COLS);
  exo_icon_view_set_orientation (EXO_ICON_VIEW (gtk_bin_get_child (GTK_BIN (compact_view))), GTK_ORIENTATION_HORIZONTAL);

  /* setup the icon renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (compact_view)->icon_renderer),
                "ypad", 2u,
                NULL);

  g_object_get (G_OBJECT (THUNAR_STANDARD_VIEW (compact_view)->preferences), "misc-compact-view-max-chars", &max_chars, NULL);

  /* setup the name renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (compact_view)->name_renderer),
                "xalign", 0.0f,
                "yalign", 0.5f,
                NULL);

  /* setup ellipsization */
  if (max_chars > 0)
    g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (compact_view)->name_renderer),
                  "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                  "width-chars", max_chars,
                  NULL);
}



static AtkObject*
thunar_compact_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the icon view class */
  object = (*GTK_WIDGET_CLASS (thunar_compact_view_parent_class)->get_accessible) (widget);

  /* set custom Atk properties for the icon view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_description (object, _("Compact directory listing"));
      atk_object_set_name (object, _("Compact view"));
      atk_object_set_role (object, ATK_ROLE_DIRECTORY_PANE);
    }

  return object;
}



