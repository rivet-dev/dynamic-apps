/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *    Erwann Chenede  <erwann.chenede@sun.com>
 *    Mark McLoughlin  <mark@skynet.ie>
 *
 * Support for the various hardware types is adapted from the
 * net-tools package (version 1.60) which is also distributed
 * under the GNU General Public License. The code used below
 * is Copyright (C) 1993 MicroWalt Corporation.
 */

#include <config.h>

#include "netstatus-iface.h"

#include <glib/gi18n.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif /* HAVE_SYS_SOCKIO_H */
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "netstatus-sysdeps.h"
#include "netstatus-enums.h"

#define NETSTATUS_IFACE_POLL_DELAY       500  /* milliseconds between polls */
#define NETSTATUS_IFACE_POLLS_IN_ERROR   10   /* no. of polls in error before increasing delay */
#define NETSTATUS_IFACE_ERROR_POLL_DELAY 5000 /* delay to use when in error state */

enum
{
  PROP_0,
  PROP_NAME,
  PROP_STATE,
  PROP_STATS,
  PROP_WIRELESS,
  PROP_SIGNAL_STRENGTH,
  PROP_ERROR
};

struct _NetstatusIfacePrivate
{
  char           *name;

  NetstatusState  state;
  NetstatusStats  stats;
  int             signal_strength;
  GError         *error;

  int             sockfd;
  guint           monitor_id;

  guint           error_polling : 1;
  guint           is_wireless : 1;
};

static void     netstatus_iface_instance_init   (NetstatusIface      *iface,
						 NetstatusIfaceClass *klass);
static void     netstatus_iface_class_init      (NetstatusIfaceClass *klass);
static void     netstatus_iface_finalize        (GObject             *object);
static void     netstatus_iface_set_property    (GObject             *object,
						 guint                property_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void     netstatus_iface_get_property    (GObject             *object,
						 guint                property_id,
						 GValue              *value,
						 GParamSpec          *pspec);
static gboolean netstatus_iface_monitor_timeout (NetstatusIface      *iface);
static void     netstatus_iface_init_monitor    (NetstatusIface      *iface);

static GObjectClass *parent_class;

GType
netstatus_iface_get_type (void)
{
  static GType type = 0;
  if (!type)
    {
      static const GTypeInfo info =
      {
        sizeof (NetstatusIfaceClass),
        NULL,
        NULL,
        (GClassInitFunc) netstatus_iface_class_init,
        NULL,
        NULL,
        sizeof (NetstatusIface),
        0,
        (GInstanceInitFunc) netstatus_iface_instance_init,
        NULL
      };
      type = g_type_register_static (G_TYPE_OBJECT, "NetstatusIface", &info, 0);
    }
  return type;
}

static void
netstatus_iface_instance_init (NetstatusIface      *iface,
			       NetstatusIfaceClass *klass __attribute__((unused)))
{
  iface->priv = g_new0 (NetstatusIfacePrivate, 1);
  iface->priv->state = NETSTATUS_STATE_DISCONNECTED;
}

static void
netstatus_iface_class_init (NetstatusIfaceClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize     = netstatus_iface_finalize;
  gobject_class->get_property = netstatus_iface_get_property;
  gobject_class->set_property = netstatus_iface_set_property;

  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
							_("Name"),
							_("The interface name"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_STATE,
				   g_param_spec_enum ("state",
						      _("State"),
						      _("The interface state"),
						      NETSTATUS_TYPE_STATE,
						      NETSTATUS_STATE_DISCONNECTED,
						      G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_STATS,
				   g_param_spec_boxed ("stats",
						       _("Stats"),
						       _("The interface packets/bytes statistics"),
						       NETSTATUS_TYPE_STATS,
						       G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_WIRELESS,
				   g_param_spec_boolean ("wireless",
							 _("Wireless"),
							 _("Whether the interface is a wireless interface"),
							 FALSE,
							 G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_SIGNAL_STRENGTH,
				   g_param_spec_int ("signal-strength",
						     _("Signal"),
						     _("Wireless signal strength percentage"),
						     0,
						     100,
						     0,
						     G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_ERROR,
				   g_param_spec_boxed ("error",
						       _("Error"),
						       _("The current error condition"),
						       NETSTATUS_TYPE_G_ERROR,
						       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
netstatus_iface_finalize (GObject *object)
{
  NetstatusIface *iface = (NetstatusIface *) object;

  if (iface->priv->error)
    g_error_free (iface->priv->error);
  iface->priv->error = NULL;

  if (iface->priv->monitor_id)
    g_source_remove (iface->priv->monitor_id);
  iface->priv->monitor_id = 0;

  if (iface->priv->sockfd)
    close (iface->priv->sockfd);
  iface->priv->sockfd = 0;

  g_free (iface->priv->name);
  iface->priv->name = NULL;

  g_free (iface->priv);
  iface->priv = NULL;

  parent_class->finalize (object);
}

static void 
netstatus_iface_set_property (GObject      *object,
			      guint         property_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  NetstatusIface *iface = (NetstatusIface *) object;

  switch (property_id)
    {
    case PROP_NAME:
      netstatus_iface_set_name (iface, g_value_get_string (value));
      break;
    case PROP_ERROR:
      netstatus_iface_set_error (iface, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
netstatus_iface_get_property (GObject    *object,
			      guint       property_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  NetstatusIface *iface = (NetstatusIface *) object;

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, iface->priv->name);
      break;
    case PROP_STATE:
      g_value_set_enum (value, iface->priv->state);
      break;
    case PROP_STATS:
      g_value_set_boxed (value, &iface->priv->stats);
      break;
    case PROP_WIRELESS:
      g_value_set_boolean (value, iface->priv->is_wireless);
      break;
    case PROP_SIGNAL_STRENGTH:
      g_value_set_int (value, iface->priv->signal_strength);
      break;
    case PROP_ERROR:
      g_value_set_boxed (value, iface->priv->error);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

NetstatusIface *
netstatus_iface_new (const char *name)
{
  return g_object_new (NETSTATUS_TYPE_IFACE,
		       "name", name,
		       NULL);
}

void
netstatus_iface_set_name (NetstatusIface *iface,
			  const char     *name)
{
  g_return_if_fail (NETSTATUS_IS_IFACE (iface));

  if (iface->priv->name && name &&
      !strcmp (iface->priv->name, name))
    return;

  if (name && strlen (name) >= IF_NAMESIZE)
    {
      g_warning (G_STRLOC ": interface name '%s' is too long\n", name);
      return;
    }

  if (iface->priv->name)
    g_free (iface->priv->name);
  iface->priv->name = g_strdup (name);

  netstatus_iface_init_monitor (iface);

  g_object_notify (G_OBJECT (iface), "name");
}

const char *
netstatus_iface_get_name (NetstatusIface *iface)
{
  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), NULL);

  return iface->priv->name;
}

NetstatusState
netstatus_iface_get_state (NetstatusIface *iface)
{
  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), NETSTATUS_STATE_DISCONNECTED);

  return iface->priv->state;
}

void
netstatus_iface_get_statistics (NetstatusIface *iface,
				NetstatusStats *stats)
{
  g_return_if_fail (NETSTATUS_IS_IFACE (iface));

  if (stats)
    *stats  = iface->priv->stats;
}

gboolean
netstatus_iface_get_is_wireless (NetstatusIface *iface)
{
  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), FALSE);

  return iface->priv->is_wireless;
}

int
netstatus_iface_get_signal_strength (NetstatusIface *iface)
{
  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), 0);

  return iface->priv->signal_strength;
}

void
netstatus_iface_set_error (NetstatusIface *iface,
			   const GError   *error)
{
  g_return_if_fail (NETSTATUS_IS_IFACE (iface));

  if (iface->priv->state != NETSTATUS_STATE_ERROR && error)
    {
      g_assert (iface->priv->error == NULL);

      iface->priv->state = NETSTATUS_STATE_ERROR;
      iface->priv->error = g_error_copy (error);

      g_object_notify (G_OBJECT (iface), "state");
      g_object_notify (G_OBJECT (iface), "error");
    }
}

const GError *
netstatus_iface_get_error (NetstatusIface *iface)
{
  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), NULL);

  return iface->priv->error;
}

void
netstatus_iface_clear_error (NetstatusIface *iface,
			     NetstatusError  code)
{
  g_return_if_fail (NETSTATUS_IS_IFACE (iface));

  if (iface->priv->state == NETSTATUS_STATE_ERROR &&
      g_error_matches (iface->priv->error, NETSTATUS_ERROR, code))
    {
      iface->priv->state = NETSTATUS_STATE_DISCONNECTED;

      g_error_free (iface->priv->error);
      iface->priv->error = NULL;

      g_object_notify (G_OBJECT (iface), "state");
      g_object_notify (G_OBJECT (iface), "error");
    }
}

static void
netstatus_iface_set_polling_error (NetstatusIface *iface,
				   NetstatusError  code,
				   const char     *format,
				   ...)
{
  GError  *error;
  va_list  args;
#if !GLIB_CHECK_VERSION(2, 22, 0)
  char    *error_message;
#endif

  va_start (args, format);

#if GLIB_CHECK_VERSION(2, 22, 0)
  error = g_error_new_valist (NETSTATUS_ERROR, code, format, args);
#else
  error_message = g_strdup_vprintf (format, args);
  error = g_error_new (NETSTATUS_ERROR, code, "%s", error_message);
  g_free (error_message);
#endif

  dprintf (POLLING, "ERROR: %s\n", error->message);
  netstatus_iface_set_error (iface, error);

  g_error_free (error);

  va_end (args);
}

static int
netstatus_iface_get_sockfd (NetstatusIface *iface)
{
  int fd;

  if (iface->priv->sockfd)
    return iface->priv->sockfd;

  if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      netstatus_iface_set_polling_error (iface,
					 NETSTATUS_ERROR_SOCKET,
					 _("Unable to open socket: %s"),
					 g_strerror (errno));
      return 0;
    }

  dprintf (POLLING, "Successfully opened socket for polling\n");
  netstatus_iface_clear_error (iface, NETSTATUS_ERROR_SOCKET);

  iface->priv->sockfd = fd;

  return iface->priv->sockfd;
}

static gboolean
netstatus_iface_poll_iface_statistics (NetstatusIface *iface,
				       gulong         *in_packets,
				       gulong         *out_packets,
				       gulong         *in_bytes,
				       gulong         *out_bytes)
{
  char *error_message;

  error_message = netstatus_sysdeps_read_iface_statistics (iface->priv->name,
							   in_packets,
							   out_packets,
							   in_bytes,
							   out_bytes);
  if (error_message)
    {
      netstatus_iface_set_polling_error (iface,
					 NETSTATUS_ERROR_STATISTICS,
					 error_message);
      g_free (error_message);

      return FALSE;
    }

  netstatus_iface_clear_error (iface, NETSTATUS_ERROR_STATISTICS);

  return TRUE;
}

static NetstatusState
netstatus_iface_poll_state (NetstatusIface *iface)
{
  NetstatusState state;
  struct ifreq   if_req;
  gboolean       tx, rx;
  int            fd;
  gulong         in_packets, out_packets;
  gulong         in_bytes, out_bytes;

  if (!(fd = netstatus_iface_get_sockfd (iface)))
    return NETSTATUS_STATE_DISCONNECTED;

  memset (&if_req, 0, sizeof (struct ifreq));
  strcpy (if_req.ifr_name, iface->priv->name);

  if (ioctl (fd, SIOCGIFFLAGS, &if_req) < 0)
    {
      netstatus_iface_set_polling_error (iface,
					 NETSTATUS_ERROR_IOCTL_IFFLAGS,
					 _("SIOCGIFFLAGS error: %s"),
					 g_strerror (errno));
      return NETSTATUS_STATE_DISCONNECTED;
    }

  netstatus_iface_clear_error (iface, NETSTATUS_ERROR_IOCTL_IFFLAGS);

  dprintf (POLLING, "Interface is %sup and %srunning\n",
	   if_req.ifr_flags & IFF_UP ? "" : "not ",
	   if_req.ifr_flags & IFF_RUNNING ? "" : "not ");

  if (!(if_req.ifr_flags & IFF_UP) || !(if_req.ifr_flags & IFF_RUNNING))
    return NETSTATUS_STATE_DISCONNECTED;

  if (!netstatus_iface_poll_iface_statistics (iface, &in_packets, &out_packets, &in_bytes, &out_bytes))
    return NETSTATUS_STATE_IDLE;

  dprintf (POLLING, "Packets in: %ld out: %ld. Prev in: %ld out: %ld\n",
	   in_packets, out_packets,
	   iface->priv->stats.in_packets, iface->priv->stats.out_packets);
  dprintf (POLLING, "Bytes in: %ld out: %ld. Prev in: %ld out: %ld\n",
	   in_bytes, out_bytes,
	   iface->priv->stats.in_bytes, iface->priv->stats.out_bytes);

  rx = in_packets  > iface->priv->stats.in_packets;
  tx = out_packets > iface->priv->stats.out_packets;

  if (!tx && !rx)
    state = NETSTATUS_STATE_IDLE;
  else if (tx && rx)
    state = NETSTATUS_STATE_TX_RX;
  else if (tx)
    state = NETSTATUS_STATE_TX;
  else /* if (rx) */
    state = NETSTATUS_STATE_RX;

  dprintf (POLLING, "State: %s\n", netstatus_get_state_string (state));

  if (tx || rx)
    {
      iface->priv->stats.in_packets  = in_packets;
      iface->priv->stats.out_packets = out_packets;
      iface->priv->stats.in_bytes    = in_bytes;
      iface->priv->stats.out_bytes   = out_bytes;

      g_object_notify (G_OBJECT (iface), "stats");
    }

  return state;
}

static gboolean
netstatus_iface_poll_wireless_details (NetstatusIface *iface,
				       int            *signal_strength)
{
  char     *error_message;
  gboolean  is_wireless;

  error_message = netstatus_sysdeps_read_iface_wireless_details (iface->priv->name,
								 &is_wireless,
								 signal_strength);

  if (error_message)
    {
      netstatus_iface_set_polling_error (iface,
					 NETSTATUS_ERROR_WIRELESS_DETAILS,
					 error_message);
      g_free (error_message);

      return FALSE;
    }

  netstatus_iface_clear_error (iface, NETSTATUS_ERROR_WIRELESS_DETAILS);

  return is_wireless;
}

static void
netstatus_iface_increase_poll_delay_in_error (NetstatusIface *iface)
{
  static int polls_in_error = 0;

  if (iface->priv->state == NETSTATUS_STATE_ERROR)
    {
      dprintf (POLLING, "Interface in error state\n");

      if (!iface->priv->error_polling &&
	  ++polls_in_error >= NETSTATUS_IFACE_POLLS_IN_ERROR)
	{
	  dprintf (POLLING, "Increasing polling delay after too many errors\n");
	  iface->priv->error_polling = TRUE;
	  g_source_remove (iface->priv->monitor_id);
	  iface->priv->monitor_id = g_timeout_add (NETSTATUS_IFACE_ERROR_POLL_DELAY,
						   (GSourceFunc) netstatus_iface_monitor_timeout,
						   iface);
	}
    }
  else if (iface->priv->error_polling)
    {
      dprintf (POLLING, "Recovered from errors. Increasing polling delay again\n");

      iface->priv->error_polling = FALSE;
      polls_in_error = 0;

      g_source_remove (iface->priv->monitor_id);
      iface->priv->monitor_id = g_timeout_add (NETSTATUS_IFACE_POLL_DELAY,
					       (GSourceFunc) netstatus_iface_monitor_timeout,
					       iface);
    }
}

static gboolean
netstatus_iface_monitor_timeout (NetstatusIface *iface)
{
  NetstatusState state;
  int            signal_strength;
  gboolean       is_wireless;

  if (g_source_is_destroyed(g_main_current_source()))
    return FALSE;

  state = netstatus_iface_poll_state (iface);

  if (iface->priv->state != state &&
      iface->priv->state != NETSTATUS_STATE_ERROR)
    {
      iface->priv->state = state;
      g_object_notify (G_OBJECT (iface), "state");
    }

  is_wireless = netstatus_iface_poll_wireless_details (iface, &signal_strength);
  if (iface->priv->is_wireless != is_wireless)
    {
      iface->priv->is_wireless = is_wireless;
      g_object_notify (G_OBJECT (iface), "wireless");
    }

  if (iface->priv->signal_strength != signal_strength)
    {
      iface->priv->signal_strength = signal_strength;
      g_object_notify (G_OBJECT (iface), "signal-strength");
    }

  netstatus_iface_increase_poll_delay_in_error (iface);

  return TRUE;
}

static void
netstatus_iface_init_monitor (NetstatusIface *iface)
{
  iface->priv->stats.in_packets  = 0;
  iface->priv->stats.out_packets = 0;
  iface->priv->stats.in_bytes    = 0;
  iface->priv->stats.out_bytes   = 0;
  iface->priv->signal_strength   = 0;
  iface->priv->is_wireless       = FALSE;

  g_object_freeze_notify (G_OBJECT (iface));
  g_object_notify (G_OBJECT (iface), "state");
  g_object_notify (G_OBJECT (iface), "wireless");
  g_object_notify (G_OBJECT (iface), "signal-strength");
  g_object_thaw_notify (G_OBJECT (iface));

  if (iface->priv->monitor_id)
    {
      dprintf (POLLING, "Removing existing monitor\n");
      g_source_remove (iface->priv->monitor_id);
      iface->priv->monitor_id = 0;
    }

  if (iface->priv->name)
    {
      dprintf (POLLING, "Initialising monitor with delay of %d\n", NETSTATUS_IFACE_POLL_DELAY);
      iface->priv->monitor_id = g_timeout_add (NETSTATUS_IFACE_POLL_DELAY,
					       (GSourceFunc) netstatus_iface_monitor_timeout,
					       iface);

      /* netstatus_iface_monitor_timeout (iface); */
    }
}

gboolean
netstatus_iface_get_inet4_details (NetstatusIface  *iface,
				   char           **addr,
				   char           **dest,
				   char           **bcast,
				   char           **mask)
{
  struct ifreq if_req;
  int          fd;
  int          flags;

  if (addr)
    *addr = NULL;
  if (dest)
    *dest = NULL;
  if (mask)
    *mask = NULL;

  if (!iface->priv->name)
    return FALSE;

  if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      g_warning (G_STRLOC ": unable to open AF_INET socket: %s\n",
		 g_strerror (errno));
      return FALSE;
    }

  if_req.ifr_addr.sa_family = AF_INET;

  strncpy (if_req.ifr_name, iface->priv->name, IF_NAMESIZE - 1);
  if_req.ifr_name [IF_NAMESIZE - 1] = '\0';
  if (addr && ioctl (fd, SIOCGIFADDR, &if_req) == 0)
    *addr = g_strdup (inet_ntoa (((struct sockaddr_in *) &if_req.ifr_addr)->sin_addr));

  if (addr && !*addr)
    {
      close (fd);
      return FALSE;
    }

  strncpy (if_req.ifr_name, iface->priv->name, IF_NAMESIZE - 1);
  if_req.ifr_name [IF_NAMESIZE - 1] = '\0';
  if (ioctl (fd, SIOCGIFFLAGS, &if_req) < 0)
    {
      close (fd);
      return TRUE;
    }

  flags = if_req.ifr_flags;

  strncpy (if_req.ifr_name, iface->priv->name, IF_NAMESIZE - 1);
  if_req.ifr_name [IF_NAMESIZE - 1] = '\0';
  if (dest && flags & IFF_POINTOPOINT &&
      ioctl (fd, SIOCGIFDSTADDR, &if_req) == 0)
    *dest = g_strdup (inet_ntoa (((struct sockaddr_in *) &if_req.ifr_dstaddr)->sin_addr));

  strncpy (if_req.ifr_name, iface->priv->name, IF_NAMESIZE - 1);
  if_req.ifr_name [IF_NAMESIZE - 1] = '\0';
  if (bcast && flags & IFF_BROADCAST &&
      ioctl (fd, SIOCGIFBRDADDR, &if_req) == 0)
    *bcast = g_strdup (inet_ntoa (((struct sockaddr_in *) &if_req.ifr_broadaddr)->sin_addr));

  strncpy (if_req.ifr_name, iface->priv->name, IF_NAMESIZE - 1);
  if_req.ifr_name [IF_NAMESIZE - 1] = '\0';
  if (mask && ioctl (fd, SIOCGIFNETMASK, &if_req) == 0)
    *mask = g_strdup (inet_ntoa (((struct sockaddr_in *) &if_req.ifr_addr)->sin_addr));

  close (fd);

  return TRUE;
}

static char *
print_mac_addr (guchar *p)
{
  return g_strdup_printf ("%02X:%02X:%02X:%02X:%02X:%02X",
			  p [0] & 0377,
			  p [1] & 0377,
			  p [2] & 0377,
			  p [3] & 0377,
			  p [4] & 0377,
			  p [5] & 0377);
}

static char *
print_ash_addr (guchar *p)
{
#define ASH_ALEN 64

  GString *str;
  char    *retval;
  int      i = 0;

  str = g_string_new ("[");

  while (p [i] != 0xc9 && p [i] != 0xff && (i < ASH_ALEN))
    g_string_append_printf (str, "%1x", p [i++]);

  g_string_append_c (str, ']');

  retval = str->str;
  g_string_free (str, FALSE);

  return retval;

#undef ASH_ALEN
}

static char *
print_ax25_addr (guchar *p)
{
  GString *str;
  char    *retval;
  int      i;

  str = g_string_new (NULL);

  for (i = 0; i < 6; i++)
    {
      char c = (p [i] & 0377) >> 1;

      if (c == ' ')
	{
	  retval = str->str;
	  g_string_free (str, FALSE);

	  return retval;
	}

      g_string_append_c (str, c);
    }

  i = (p [6] & 0x1E) >> 1;
  if (i != 0)
    g_string_append_printf (str, "-%d", i);

  retval = str->str;
  g_string_free (str, FALSE);

  return retval;
}

static char *
print_rose_addr (guchar *p)
{
  return g_strdup_printf ("%02x%02x%02x%02x%02x", p [0], p [1], p [2], p [3], p [4]);
}

static char *
print_x25_addr (guchar *p)
{
  return g_strdup ((const gchar *) p);
}

static char *
print_arcnet_addr (guchar *p)
{
  return g_strdup_printf ("%02X", p [0] & 0377);
}

static char *
print_dlci_addr (guchar *p)
{
  return g_strdup_printf ("%i", *(short *) p);
}

static char *
print_irda_addr (guchar *p)
{
  return g_strdup_printf ("%02x:%02x:%02x:%02x", p [3], p [2], p [1], p [0]);
}

static char *
print_econet_addr (guchar *p)
{
  /* This should really be:
   *   #include <neteconet/ec.h>
   *
   *   struct ec_addr *ec = (struct ec_addr *) p;
   *   return g_strdup_printf ("%d.%d, ec->net, ec->station);
   *
   * But I think the hack is safe enough.
   */
  return g_strdup_printf ("%d.%d", p [0], p [1]);
}

static struct HwType
{
  int   hw_type;
  char *hw_name;
  char *(*print_hw_addr) (guchar *p);
} hw_types [] =
  {
#ifdef ARPHRD_NETROM
    { ARPHRD_NETROM, N_("AMPR NET/ROM"), print_ax25_addr },
#endif
#ifdef ARPHRD_ETHER
    { ARPHRD_ETHER, N_("Ethernet"), print_mac_addr },
#endif
#ifdef ARPHRD_EETHER
    { ARPHRD_EETHER, NULL, NULL },
#endif
#ifdef ARPHRD_AX25
    { ARPHRD_AX25, N_("AMPR AX.25"), NULL },
#endif
#ifdef ARPHRD_PRONET
    { ARPHRD_PRONET, NULL, NULL },
#endif
#ifdef ARPHRD_CHAOS
    { ARPHRD_CHAOS, NULL, NULL },
#endif
#ifdef ARPHRD_IEEE802
    { ARPHRD_IEEE802, N_("16/4 Mbps Token Ring"), print_mac_addr },
#endif
#ifdef ARPHRD_ARCNET
    { ARPHRD_ARCNET, N_("ARCnet"), print_arcnet_addr },
#endif
#ifdef ARPHRD_APPLETLK
    { ARPHRD_APPLETLK, NULL, NULL },
#endif
#ifdef ARPHRD_DLCI
    { ARPHRD_DLCI, N_("Frame Relay DLCI"), print_dlci_addr },
#endif
#ifdef ARPHRD_ATM
    { ARPHRD_ATM, NULL, NULL },
#endif
#ifdef ARPHRD_METRICOM
    { ARPHRD_METRICOM, N_("Metricom Starmode IP"), NULL },
#endif
#ifdef ARPHRD_SLIP
    { ARPHRD_SLIP, N_("Serial Line IP"), NULL },
#endif
#ifdef ARPHRD_CSLIP
    { ARPHRD_CSLIP, N_("VJ Serial Line IP"), NULL },
#endif
#ifdef ARPHRD_SLIP6
    { ARPHRD_SLIP6, N_("6-bit Serial Line IP"), NULL },
#endif
#ifdef ARPHRD_CSLIP6
    { ARPHRD_CSLIP6, N_("VJ 6-bit Serial Line IP"), NULL },
#endif
#ifdef ARPHRD_RSRVD
    { ARPHRD_RSRVD, NULL, NULL },
#endif
#ifdef ARPHRD_ADAPT
    { ARPHRD_ADAPT, N_("Adaptive Serial Line IP"), NULL },
#endif
#ifdef ARPHRD_ROSE
    { ARPHRD_ROSE, N_("AMPR ROSE"), print_rose_addr },
#endif
#ifdef ARPHRD_X25
    { ARPHRD_X25, N_("Generic X.25"), print_x25_addr },
#endif
#ifdef ARPHRD_PPP
    { ARPHRD_PPP, N_("Point-to-Point Protocol"), NULL },
#endif
#ifdef ARPHRD_CISCO
    { ARPHRD_CISCO, NULL, NULL },
#endif
#ifdef ARPHRD_HDLC
    { ARPHRD_HDLC, N_("(Cisco)-HDLC"), NULL },
#endif
#ifdef ARPHRD_LAPB
    { ARPHRD_LAPB, N_("LAPB"), NULL },
#endif
#ifdef ARPHRD_DDCMP
    { ARPHRD_DDCMP, NULL, NULL },
#endif
#ifdef ARPHRD_RAWHDLC
    { ARPHRD_RAWHDLC, NULL, NULL },
#endif
#ifdef ARPHRD_TUNNEL
    { ARPHRD_TUNNEL, N_("IPIP Tunnel"), NULL },
#endif
#ifdef ARPHRD_TUNNEL6
    { ARPHRD_TUNNEL6, NULL, NULL },
#endif
#ifdef ARPHRD_FRAD
    { ARPHRD_FRAD, N_("Frame Relay Access Device"), NULL },
#endif
#ifdef ARPHRD_SKIP
    { ARPHRD_SKIP, NULL, NULL },
#endif
#ifdef ARPHRD_LOOPBACK
    { ARPHRD_LOOPBACK, N_("Local Loopback"), print_mac_addr },
#endif
#ifdef ARPHRD_LOCALTLK
    { ARPHRD_LOCALTLK, NULL, NULL },
#endif
#ifdef ARPHRD_FDDI
    { ARPHRD_FDDI, N_("Fiber Distributed Data Interface"), print_mac_addr },
#endif
#ifdef ARPHRD_BIF
    { ARPHRD_BIF, NULL, NULL },
#endif
#ifdef ARPHRD_SIT
    { ARPHRD_SIT, N_("IPv6-in-IPv4"), NULL },
#endif
#ifdef ARPHRD_IPDDP
    { ARPHRD_IPDDP, NULL, NULL },
#endif
#ifdef ARPHRD_IPGRE
    { ARPHRD_IPGRE, NULL, NULL },
#endif
#ifdef ARPHRD_PIMREG
    { ARPHRD_PIMREG, NULL, NULL },
#endif
#ifdef ARPHRD_HIPPI
    { ARPHRD_HIPPI, N_("HIPPI"), print_mac_addr },
#endif
#ifdef ARPHRD_ASH
    { ARPHRD_ASH, N_("Ash"), print_ash_addr },
#endif
#ifdef ARPHRD_ECONET
    { ARPHRD_ECONET, N_("Econet"), print_econet_addr },
#endif
#ifdef ARPHRD_IRDA
    { ARPHRD_IRDA, N_("IrLAP"), print_irda_addr },
#endif
#ifdef ARPHRD_FCPP
    { ARPHRD_FCPP, NULL, NULL },
#endif
#ifdef ARPHRD_FCAL
    { ARPHRD_FCAL, NULL, NULL },
#endif
#ifdef ARPHRD_FCPL
    { ARPHRD_FCPL, NULL, NULL },
#endif
#ifdef ARPHRD_FCPFABRIC
    { ARPHRD_FCPFABRIC, NULL, NULL },
#endif
#ifdef ARPHRD_IEEE802_TR
    { ARPHRD_IEEE802_TR, N_("16/4 Mbps Token Ring"), print_mac_addr },
#endif
#ifdef ARPHRD_IEEE80211
    { ARPHRD_IEEE80211, NULL, NULL },
#endif
  };

static struct HwType *
netstatus_iface_get_hw_details (NetstatusIface  *iface,
				char           **hw_addr)
{
#ifdef SIOCGIFHWADDR
  static struct HwType *hw_type = NULL;
  struct ifreq          if_req;
  int                   fd;
  unsigned int                   i;

  if (hw_addr)
    *hw_addr = NULL;

  if (!iface->priv->name)
    return NULL;

  if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      g_warning (G_STRLOC ": unable to open AF_INET socket: %s\n",
		 g_strerror (errno));
      return NULL;
    }

  strncpy (if_req.ifr_name, iface->priv->name, IF_NAMESIZE - 1);
  if_req.ifr_name [IF_NAMESIZE - 1] = '\0';
  if (ioctl (fd, SIOCGIFHWADDR, &if_req) < 0)
    {
      g_warning (G_STRLOC ": unable to obtain hardware address: %s\n",
		 g_strerror (errno));
      close (fd);
      return NULL;
    }

  close (fd);

  if (hw_type && hw_type->hw_type != if_req.ifr_hwaddr.sa_family)
    hw_type = NULL;

  for (i = 0; !hw_type && i < G_N_ELEMENTS (hw_types); i++)
    if (hw_types [i].hw_type == if_req.ifr_hwaddr.sa_family)
      hw_type = &hw_types [i];

  if (!hw_type || !hw_type->hw_name)
    {
      g_warning (G_STRLOC ": no support for hardware type %d\n",
		 if_req.ifr_hwaddr.sa_family);
      return NULL;
    }

  if (hw_addr && if_req.ifr_hwaddr.sa_data && hw_type->print_hw_addr)
    *hw_addr = hw_type->print_hw_addr ((guchar *) if_req.ifr_hwaddr.sa_data);

  return hw_type;

#else /* !defined(SIOCGIFHWADDR) */
  return NULL;
#endif
}

gboolean
netstatus_iface_get_is_loopback (NetstatusIface *iface)
{
  struct HwType *hw_type;

  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), FALSE);

  if (!(hw_type = netstatus_iface_get_hw_details (iface, NULL)))
    return FALSE;

#ifdef ARPHRD_LOOPBACK
  return hw_type->hw_type == ARPHRD_LOOPBACK ? TRUE : FALSE;
#else
  return FALSE;
#endif
}

gboolean
netstatus_iface_get_device_details (NetstatusIface  *iface,
				    const char     **hw_name,
				    char           **hw_addr)
{
  struct HwType *hw_type;

  g_return_val_if_fail (NETSTATUS_IS_IFACE (iface), FALSE);

  if (hw_name)
    *hw_name = NULL;
  if (hw_addr)
    *hw_addr = NULL;

  if (!(hw_type = netstatus_iface_get_hw_details (iface, hw_addr)))
    return FALSE;

  g_assert (hw_type->hw_name != NULL);

  if (hw_name)
    *hw_name = _(hw_type->hw_name);

  return TRUE;
}

/* Taken From R. Stevens Unix Network Programming Vol. 1.
 *
 * SIOCGIFCONF does not return an error on all systems if
 * the buffer is not large enough for all available
 * interfaces. This loop portably ensures that we get the
 * information for all interfaces.
 */
static struct ifconf *
get_ifconf (int      fd,
	    GError **error)
{
  struct ifconf  if_conf;
  struct ifconf *retval;
  int            len, lastlen;

  lastlen = 0;
  len = 10 * sizeof (struct ifreq);

  while (TRUE)
    {
      if_conf.ifc_len = len;
      if_conf.ifc_buf = g_malloc0 (len);

      if (ioctl (fd, SIOCGIFCONF, &if_conf) < 0)
	{
	  if (errno != EINVAL || lastlen != 0)
	    {
	      g_free (if_conf.ifc_buf);

	      if (error)
		*error = g_error_new (NETSTATUS_ERROR,
				      NETSTATUS_ERROR_IOCTL_IFCONF,
				      _("SIOCGIFCONF error: %s"),
				      g_strerror (errno));

	      return NULL;
	    }
	}
      else
	{
	  if (if_conf.ifc_len == lastlen)
	    break;
	  lastlen = if_conf.ifc_len;
	}

      g_free (if_conf.ifc_buf);
      if_conf.ifc_buf = NULL;

      len *= 2;
    }

  retval = g_new0 (struct ifconf, 1);

  retval->ifc_len = if_conf.ifc_len;
  retval->ifc_buf = if_conf.ifc_buf;

  return retval;
}

GList *
netstatus_list_interface_names (GError **error)
{

  struct ifconf *if_conf;
  GList         *interfaces;
  GList         *loopbacks;
  char          *p;
  int            fd;

  if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      if (error)
	*error = g_error_new (NETSTATUS_ERROR,
			      NETSTATUS_ERROR_SOCKET,
			      _("Unable to open socket: %s"),
			      g_strerror (errno));
      return NULL;
    }

  if ((if_conf = get_ifconf (fd, error)) == NULL)
    {
      close (fd);
      return NULL;
    }

  interfaces = NULL;
  loopbacks  = NULL;

  for (p = if_conf->ifc_buf; p < if_conf->ifc_buf + if_conf->ifc_len;)
    {
      struct ifreq *if_req = (struct ifreq *) p;
      gboolean      loopback = FALSE;

      p = (char *) (if_req + 1);

      if (ioctl (fd, SIOCGIFFLAGS, if_req) < 0)
	{
	  if (error)
	    *error = g_error_new (NETSTATUS_ERROR,
				  NETSTATUS_ERROR_IOCTL_IFFLAGS,
				  _("SIOCGIFFLAGS error: %s"),
				  g_strerror (errno));
	}
      else
	{
	  loopback = (if_req->ifr_flags & IFF_LOOPBACK);
	}

      if (!loopback)
	interfaces = netstatus_list_insert_unique (interfaces,
						   g_strdup (if_req->ifr_name));
      else
	loopbacks  = netstatus_list_insert_unique (loopbacks,
						   g_strdup (if_req->ifr_name));
    }

  interfaces = g_list_concat (interfaces, loopbacks);

  g_free (if_conf->ifc_buf);
  g_free (if_conf);
  close (fd);

  if (!interfaces && error)
    *error = g_error_new (NETSTATUS_ERROR,
			  NETSTATUS_ERROR_NO_INTERFACES,
			  _("No network devices found"));

  return interfaces;
}
