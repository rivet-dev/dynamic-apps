/*
 *  xfconfd
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfconf-backend-factory.h"
#include "xfconf-backend.h"
#include "xfconf-daemon.h"

/* i'm not sure i like this method.  perhaps each backend could be a
 * GTypeModule.  i also want the ability to multiplex multiple backends.
 * for example, i'd like to write a MCS backend that can read the old MCS
 * config files to ease migration to the new system, but of course the 'new'
 * backend should be the one that gets written to all the time.
 */

#ifdef BUILD_XFCONF_BACKEND_PERCHANNEL_XML
#include "xfconf-backend-perchannel-xml.h"
#endif

static GHashTable *backends = NULL;

static void
xfconf_backend_factory_ensure_backends(void)
{
    if(backends)
        return;
    
    backends = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     NULL, (GDestroyNotify)g_free);
    
#ifdef BUILD_XFCONF_BACKEND_PERCHANNEL_XML
    {
        GType *gtype = g_new(GType, 1);
        *gtype = XFCONF_TYPE_BACKEND_PERCHANNEL_XML;
        g_hash_table_insert(backends,
                            (gpointer)XFCONF_BACKEND_PERCHANNEL_XML_TYPE_ID,
                            gtype);
    }
#endif
}


XfconfBackend *
xfconf_backend_factory_get_backend(const gchar *type,
                                   GError **error)
{
    XfconfBackend *backend = NULL;
    GType *backend_gtype;
    
    xfconf_backend_factory_ensure_backends();
    
    backend_gtype = g_hash_table_lookup(backends, type);
    if(!backend_gtype) {
        if(error) {
            g_set_error(error, XFCONF_ERROR, 0,
                        _("Unable to find Xfconf backend of type \"%s\""),
                        type);
        }
        return NULL;
    }
    
    backend = g_object_new(*backend_gtype, NULL);
    if(!xfconf_backend_initialize(backend, error)) {
        g_object_unref(G_OBJECT(backend));
        return NULL;
    }
    
    return backend;
}


void
xfconf_backend_factory_cleanup (void)
{
  if(backends) {
      g_hash_table_destroy(backends);
      backends = NULL;
  }
}
