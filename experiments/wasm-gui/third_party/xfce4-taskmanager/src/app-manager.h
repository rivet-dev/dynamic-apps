/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

typedef struct _App App;
struct _App
{
	WnckApplication *	application;
	GPid			pid;
	gchar			name[1024];
	cairo_surface_t *	surface;
};

#define XTM_TYPE_APP_MANAGER			(xtm_app_manager_get_type ())
#define XTM_APP_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_APP_MANAGER, XtmAppManager))
#define XTM_APP_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_APP_MANAGER, XtmAppManagerClass))
#define XTM_IS_APP_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_APP_MANAGER))
#define XTM_IS_APP_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_APP_MANAGER))
#define XTM_APP_MANAGER_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_APP_MANAGER, XtmAppManagerClass))

typedef struct _XtmAppManager XtmAppManager;

GType			xtm_app_manager_get_type			(void);
XtmAppManager *		xtm_app_manager_new				(void);
App *			xtm_app_manager_get_app_from_pid		(XtmAppManager *manager, GPid pid);

#endif /* !APP_MANAGER_H */
