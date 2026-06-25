/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __TEX_OPEN_TERMINAL_H__
#define __TEX_OPEN_TERMINAL_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

typedef struct _TexOpenTerminalClass TexOpenTerminalClass;
typedef struct _TexOpenTerminal      TexOpenTerminal;

#define TEX_TYPE_OPEN_TERMINAL            (tex_open_terminal_get_type ())
#define TEX_OPEN_TERMINAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEX_TYPE_OPEN_TERMINAL, TexOpenTerminal))
#define TEX_OPEN_TERMINAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TEX_TYPE_OPEN_TERMINAL, TexOpenTerminalClass))
#define TEX_IS_OPEN_TERMINAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEX_TYPE_OPEN_TERMINAL))
#define TEX_IS_OPEN_TERMINAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEX_TYPE_OPEN_TERMINAL))
#define TEX_OPEN_TERMINAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEX_TYPE_OPEN_TERMINAL, TexOpenTerminalClass))

GType tex_open_terminal_get_type      (void) G_GNUC_CONST;
void  tex_open_terminal_register_type (ThunarxProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__TEX_OPEN_TERMINAL_H__ */
