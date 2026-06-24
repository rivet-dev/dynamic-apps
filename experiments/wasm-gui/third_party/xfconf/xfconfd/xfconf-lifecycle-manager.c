/*
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

#include <gio/gio.h>

#include <xfconf-lifecycle-manager.h>



#define SHUTDOWN_TIMEOUT_SECONDS 300



enum
{
    SIGNAL_SHUTDOWN,
    N_SIGNALS,
};



static void xfconf_lifecycle_manager_finalize (GObject *object);



struct _XfconfLifecycleManagerPrivate
{
    GMutex lock;

    guint timeout_id;
    guint use_count;
    gboolean shutdown_emitted;
};



static guint lifecycle_manager_signals[N_SIGNALS];



G_DEFINE_TYPE_WITH_PRIVATE (XfconfLifecycleManager, xfconf_lifecycle_manager, G_TYPE_OBJECT)



static void
xfconf_lifecycle_manager_class_init (XfconfLifecycleManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = xfconf_lifecycle_manager_finalize;

    lifecycle_manager_signals[SIGNAL_SHUTDOWN] =
        g_signal_new ("shutdown", XFCONF_TYPE_LIFECYCLE_MANAGER, G_SIGNAL_RUN_LAST, 0,
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}



static void
xfconf_lifecycle_manager_init (XfconfLifecycleManager *manager)
{
    manager->priv = xfconf_lifecycle_manager_get_instance_private (manager);
    g_mutex_init (&manager->priv->lock);
    manager->priv->timeout_id = 0;
    manager->priv->use_count = 0;
    manager->priv->shutdown_emitted = FALSE;
}



static void
xfconf_lifecycle_manager_finalize (GObject *object)
{
    XfconfLifecycleManager *manager = XFCONF_LIFECYCLE_MANAGER (object);

    g_mutex_clear (&manager->priv->lock);

    G_OBJECT_CLASS (xfconf_lifecycle_manager_parent_class)->finalize (object);
}



static gboolean
xfconf_lifecycle_manager_timeout (gpointer user_data)
{
    XfconfLifecycleManager *manager = user_data;

    g_mutex_lock (&manager->priv->lock);

    /* reschedule the timeout if the daemon is currently in use */
    if (manager->priv->use_count > 0)
    {
        g_mutex_unlock (&manager->priv->lock);
        return TRUE;
    }

    /* reset the timeout id */
    manager->priv->timeout_id = 0;

    /* emit the shutdown signal */
    g_signal_emit (manager, lifecycle_manager_signals[SIGNAL_SHUTDOWN], 0);

    /* set the shutdown emitted flag to force other threads not to reschedule
     * the timeout */
    manager->priv->shutdown_emitted = TRUE;

    g_mutex_unlock (&manager->priv->lock);

    return FALSE;
}



XfconfLifecycleManager *
xfconf_lifecycle_manager_new (void)
{
    return g_object_new (XFCONF_TYPE_LIFECYCLE_MANAGER, NULL);
}



void
xfconf_lifecycle_manager_start (XfconfLifecycleManager *manager)
{
    g_return_if_fail (XFCONF_IS_LIFECYCLE_MANAGER (manager));

    g_mutex_lock (&manager->priv->lock);

    /* ignore if there already is a timeout scheduled */
    if (manager->priv->timeout_id != 0)
    {
        g_mutex_unlock (&manager->priv->lock);
        return;
    }

    manager->priv->timeout_id =
        g_timeout_add_seconds (SHUTDOWN_TIMEOUT_SECONDS,
                               xfconf_lifecycle_manager_timeout,
                               manager);

    g_mutex_unlock (&manager->priv->lock);
}



gboolean
xfconf_lifecycle_manager_keep_alive (XfconfLifecycleManager *manager)
{
    g_return_val_if_fail (XFCONF_IS_LIFECYCLE_MANAGER (manager), G_DBUS_METHOD_INVOCATION_UNHANDLED);

    g_mutex_lock (&manager->priv->lock);

    /* if the shutdown signal has been emitted, there's nothing we can do to prevent
     * a shutdown anymore */
    if (manager->priv->shutdown_emitted)
    {
        g_mutex_unlock (&manager->priv->lock);
        return G_DBUS_METHOD_INVOCATION_UNHANDLED;
    }

    /* drop existing timeout, if any */
    if (manager->priv->timeout_id != 0)
        g_source_remove (manager->priv->timeout_id);

    /* reschedule the shutdown timeout */
    manager->priv->timeout_id =
        g_timeout_add_seconds (SHUTDOWN_TIMEOUT_SECONDS,
                               xfconf_lifecycle_manager_timeout,
                               manager);

    g_mutex_unlock (&manager->priv->lock);

    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}



gboolean
xfconf_lifecycle_manager_increment_use_count (XfconfLifecycleManager *manager)
{
    g_return_val_if_fail (XFCONF_IS_LIFECYCLE_MANAGER (manager), G_DBUS_METHOD_INVOCATION_UNHANDLED);

    g_mutex_lock (&manager->priv->lock);

    manager->priv->use_count++;

    g_mutex_unlock (&manager->priv->lock);

    return G_DBUS_METHOD_INVOCATION_UNHANDLED;
}



gboolean
xfconf_lifecycle_manager_decrement_use_count (XfconfLifecycleManager *manager)
{
    g_return_val_if_fail (XFCONF_IS_LIFECYCLE_MANAGER (manager), G_DBUS_METHOD_INVOCATION_HANDLED);

    g_mutex_lock (&manager->priv->lock);

    /* decrement the use count, make sure not to drop below zero */
    if (manager->priv->use_count > 0)
        manager->priv->use_count--;

    g_mutex_unlock (&manager->priv->lock);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}
