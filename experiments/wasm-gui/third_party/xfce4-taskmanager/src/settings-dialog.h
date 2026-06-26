/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

void		xtm_settings_dialog_run				(GtkWidget *parent_window);

#endif /* !SETTINGS_DIALOG_H */
