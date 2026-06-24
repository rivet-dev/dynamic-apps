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
 *    Joe Marcus Clarke  <marcus@freebsd.org>
 */

#include <config.h>

#include "netstatus-sysdeps.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <glib/gi18n.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <dev/an/if_aironet_ieee.h>
#include <dev/wi/if_wavelan_ieee.h>
#endif

static inline gboolean
parse_stats (char    *buf,
	     int      prx_idx,
	     int      ptx_idx,
	     gulong  *in_packets,
	     gulong  *out_packets,
	     int      brx_idx,
	     int      btx_idx,
	     gulong  *in_bytes,
	     gulong  *out_bytes)
{
  char *p;
  int   i;

  p = strtok (buf, " \t\n");
  for (i = 0; p; i++, p = strtok (NULL, " \t\n"))
    {
      if (i == prx_idx)
	*in_packets = g_ascii_strtoull (p, NULL, 10);
      if (i == ptx_idx)
	*out_packets = g_ascii_strtoull (p, NULL, 10);
      if (i == brx_idx)
	*in_bytes = g_ascii_strtoull (p, NULL, 10);
      if (i == btx_idx)
	*out_bytes = g_ascii_strtoull (p, NULL, 10);
    }

  if (i <= prx_idx || i <= ptx_idx || i <= brx_idx || i <=btx_idx)
    return FALSE;

  return TRUE;
}

#if !defined (__FreeBSD__)

static inline char *
parse_iface_name (const char *buf)
{
  char *p1;

  if ((p1 = strchr (buf, ':')))
    {
      char *p2;

      p2 = strchr (p1, ':');
      if (p2)
	*p2++ = '\0';
      else
	*p1++ = '\0';

      return p2 ? p2 : p1;
    }
  else if ((p1 = strchr (buf, ' ')))
    {
      *p1++ = '\0';
      return p1;
    }

  return NULL;
}

static inline void
parse_stats_header (char *buf,
		    int  *prx_idx,
		    int  *ptx_idx,
		    int  *brx_idx,
		    int  *btx_idx)
{
  char *p;
  int   i;

  *prx_idx = *ptx_idx = -1;
  *brx_idx = *btx_idx = -1;

  p = strtok (buf, "| \t\n");
  p = strtok (NULL, "| \t\n"); /* Skip the first one */
  for (i = 0; p; i++, p = strtok (NULL, "| \t\n"))
    {
      if (!strcmp (p, "packets"))
	{
	  if (*prx_idx == -1)
	    *prx_idx = i;
	  else
	    *ptx_idx = i;
	}
      else if (!strcmp (p, "bytes"))
	{
	  if (*brx_idx == -1)
	    *brx_idx = i;
	  else
	    *btx_idx = i;
	}
    }
}

static inline FILE *
get_proc_net_dev_fh (void)
{
  static FILE *retval = NULL;

  if (retval != NULL)
    return retval;

  return retval = fopen ("/proc/net/dev", "r");
}

char *
netstatus_sysdeps_read_iface_statistics (const char  *iface,
					 gulong      *in_packets,
					 gulong      *out_packets,
					 gulong      *in_bytes,
					 gulong      *out_bytes)
{
  FILE *fh;
  char  buf [512];
  int   prx_idx, ptx_idx;
  int   brx_idx, btx_idx;
  char *error_message = NULL;

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (in_packets != NULL, NULL);
  g_return_val_if_fail (out_packets != NULL, NULL);
  g_return_val_if_fail (in_bytes != NULL, NULL);
  g_return_val_if_fail (out_bytes != NULL, NULL);

  *in_packets  = -1;
  *out_packets = -1;
  *in_bytes    = -1;
  *out_bytes   = -1;

  fh = get_proc_net_dev_fh ();
  if (!fh)
    return g_strdup_printf (_("Cannot open /proc/net/dev: %s"),
			    g_strerror (errno));

  if (fgets (buf, sizeof (buf), fh) == NULL ||
      fgets (buf, sizeof (buf), fh) == NULL)
    return g_strdup (_("Could not parse /proc/net/dev. No data."));

  parse_stats_header (buf, &prx_idx, &ptx_idx, &brx_idx, &btx_idx);
  if (prx_idx == -1 || ptx_idx == -1 ||
      brx_idx == -1 || btx_idx == -1)
    return g_strdup (_("Could not parse /proc/net/dev. Unknown format."));

  while (fgets (buf, sizeof (buf), fh))
    {
      char *stats;
      char *name;

      name = buf;
      while (g_ascii_isspace (name [0]))
	name++;

      stats = parse_iface_name (name);
      if (!stats)
	{
	  if (!error_message)
	    error_message = g_strdup_printf (_("Could not parse interface name from '%s'"), buf);
	  continue;
	}

      if (strcmp (name, iface) != 0)
	continue;

      if (!parse_stats (stats,
			prx_idx, ptx_idx, in_packets, out_packets,
			brx_idx, btx_idx, in_bytes, out_bytes))
	{
	  if (error_message)
	    g_free (error_message);
	  error_message = g_strdup_printf (_("Could not parse interface statistics from '%s'. "
					     "prx_idx = %d; ptx_idx = %d; brx_idx = %d; btx_idx = %d;"),
					   buf, prx_idx, ptx_idx, brx_idx, btx_idx);
	  continue;
	}

      break;
    }

  if ((*in_packets == (gulong) -1 || *out_packets == (gulong) -1 || *in_bytes == (gulong) -1 || *out_bytes == (gulong) -1) && !error_message)
    error_message = g_strdup_printf ("Could not find information on interface '%s' in /proc/net/dev", iface);

  rewind (fh);
  fflush (fh);

  return error_message;
}

static inline gboolean
parse_wireless (char  *buf,
		int    link_idx,
		int   *link)
{
  char *p;
  int   i;

  p = strtok (buf, " \t\n");
  for (i = 0; p; i++, p = strtok (NULL, " \t\n"))
    {
      if (i == link_idx)
	*link = g_ascii_strtoull (p, NULL, 10);
    }

  if (i <= link_idx)
    return FALSE;

  return TRUE;
}

static inline int
parse_wireless_header (char *buf)
{
  char *p;
  int   i;

  p = strtok (buf, "| \t\n");
  p = strtok (NULL, "| \t\n"); /* Skip the first one */
  for (i = 0; p; i++, p = strtok (NULL, "| \t\n"))
    {
      if (!strcmp (p, "link"))
	{
	  return i;
	}
    }

  return -1;
}

static inline FILE *
get_proc_net_wireless_fh (void)
{
  static FILE *retval = NULL;

  if (retval != NULL)
    return retval;

  return retval = fopen ("/proc/net/wireless", "r");
}

char *
netstatus_sysdeps_read_iface_wireless_details (const char *iface,
					       gboolean   *is_wireless,
					       int        *signal_strength)
{
  FILE *fh;
  char  buf [512];
  int   link_idx;
  char *error_message = NULL;

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (is_wireless != NULL, NULL);
  g_return_val_if_fail (signal_strength != NULL, NULL);

  if (is_wireless)
    *is_wireless = FALSE;
  if (signal_strength)
    *signal_strength = 0;

  fh = get_proc_net_wireless_fh ();
  if (!fh)
    return NULL;

  if (fgets (buf, sizeof (buf), fh) == NULL ||
      fgets (buf, sizeof (buf), fh) == NULL)
    return g_strdup (_("Could not parse /proc/net/wireless. No data."));

  link_idx = parse_wireless_header (buf);
  if (link_idx == -1)
    return g_strdup (_("Could not parse /proc/net/wireless. Unknown format."));

  while (fgets (buf, sizeof (buf), fh))
    {
      char *details;
      char *name;
      int   link = 0;

      name = buf;
      while (g_ascii_isspace (name [0]))
	name++;

      details = parse_iface_name (name);
      if (!details)
	{
	  if (!error_message)
	    error_message = g_strdup_printf (_("Could not parse interface name from '%s'"), buf);
	  continue;
	}

      if (strcmp (name, iface) != 0)
	continue;

      if (!parse_wireless (details, link_idx, &link))
	{
	  if (error_message)
	    g_free (error_message);
	  error_message = g_strdup_printf (_("Could not parse wireless details from '%s'. link_idx = %d;"),
					   buf, link_idx);
	  continue;
	}

      /* Stolen from the wireless applet */
      *signal_strength = (int) rint ((log (link) / log (92)) * 100.0);
      *signal_strength = CLAMP (*signal_strength, 0, 100);
      *is_wireless = TRUE;

      break;
    }

  rewind (fh);
  fflush (fh);

  return error_message;
}

#else /* defined(__FreeBSD__) */

static inline void
parse_header (char *buf,
	      int  *prx_idx,
	      int  *ptx_idx,
	      int  *brx_idx,
	      int  *btx_idx)
{
   char *p;
   int   i;

   *prx_idx = *ptx_idx = -1;
   *brx_idx = *btx_idx = -1;

   p = strtok (buf, " \n\t");
   for (i = 0; p; i++, p = strtok (NULL, " \t\n"))
     {
        if (!strcmp (p, "Ipkts"))
	  {
	     *prx_idx = i;
	  }
	else if (!strcmp (p, "Ibytes"))
	  {
	     *brx_idx = i;
	  }
	else if (!strcmp (p, "Opkts"))
	  {
	     *ptx_idx = i;
	  }
	else if (!strcmp (p, "Obytes"))
	  {
	     *btx_idx = i;
	  }
     }
}

static inline gboolean
wireless_getval (const char      *iface,
		 gpointer         req,
		 unsigned long    req_type,
		 char           **error)
{
  struct ifreq ifr;
  int          s;

  memset (&ifr, 0, sizeof (ifr));

  strlcpy (ifr.ifr_name, iface, sizeof (ifr.ifr_name));
  ifr.ifr_data = (caddr_t) req;

  s = socket (AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    {
      *error = g_strdup_printf (_("Could not connect to interface, '%s'"), iface);
      return FALSE;
    }

  if (ioctl (s, req_type, &ifr) == -1)
    {
      *error = g_strdup_printf (_("Could not send ioctl to interface, '%s'"), iface);
      close (s);
      return FALSE;
    }

  close (s);
  return TRUE;
}

static inline char *
get_an_data (const char *iface,
	     int        *signal_strength)
{
#ifdef AN_RID_RSSI_MAP
  struct an_ltv_rssi_map  an_rssimap;
#endif
  struct an_req         areq;
  struct an_ltv_status *sts;
  int                   level;
  char                 *error = NULL;
  gboolean              rssimap_valid = FALSE;

#ifdef AN_RID_RSSI_MAP
  an_rssimap.an_len = sizeof (an_rssimap);
  an_rssimap.an_type = AN_RID_RSSI_MAP;
  rssimap_valid = wireless_getval (iface, (gpointer) &an_rssimap, SIOCGAIRONET, &error);
#endif

  areq.an_len = sizeof (areq);
  areq.an_type = AN_RID_STATUS;

  if (!wireless_getval (iface, (gpointer) &areq, SIOCGAIRONET, &error))
    return error;

  sts = (struct an_ltv_status *) &areq;

#ifdef AN_RID_RSSI_MAP
  if (rssimap_valid)
    level = (int) (an_rssimap.an_entries[sts->an_normalized_strength].an_rss_pct);
  else
    level = (int) (sts->an_normalized_strength);
#else
  level = (int) (sts->an_normalized_rssi);
#endif

  memcpy (signal_strength, &level, sizeof (signal_strength));

  return error;
}

static inline char *
get_wi_data (const char *iface,
	     int        *signal_strength)
{
  struct wi_req  wreq;
  int            level;
  char          *error = NULL;

  memset (&wreq, 0, sizeof (wreq));

  wreq.wi_len  = WI_MAX_DATALEN;
  wreq.wi_type = WI_RID_COMMS_QUALITY;

  if (!wireless_getval (iface, &wreq, SIOCGWAVELAN, &error))
    return error;

  level = (int) wreq.wi_val[1];

#ifdef WI_RID_READ_APS
  if (signal_strength <= 0)
    {
      /* we fail to get signal strength by usual means, try another way */
      static time_t   last_scan;
      static long int cached;
      time_t          now;

      now = time (NULL);

      /* XXX: this is long operation, and we will scan station not often then one in 5 secs */
      if (now > last_scan + 5)
        {
          struct wi_apinfo *w;
          int              nstations;

          bzero (&wreq, sizeof (wreq));
          wreq.wi_len  = WI_MAX_DATALEN;
          wreq.wi_type = WI_RID_READ_APS;
          if (!wireless_getval (iface, &wreq, SIOCGWAVELAN, &error))
            return error;
          nstations = *(int *) wreq.wi_val;
          if (nstations > 0)
	    {
              w = (struct wi_apinfo *)(((char *) &wreq.wi_val) + sizeof (int));
              signal_strength = (long int) w->signal;
            }

            cached = signal_strength;
            last_scan = now;
          }
        else
	  {
            signal_strength = cached;
          }
    }
#endif

  memcpy (signal_strength, &level, sizeof (signal_strength));

  return error;
}

char *
netstatus_sysdeps_read_iface_wireless_details (const char *iface,
					       gboolean   *is_wireless,
					       int        *signal_strength)
{
  char *error_message = NULL;

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (is_wireless != NULL, NULL);
  g_return_val_if_fail (signal_strength != NULL, NULL);

  if (is_wireless)
    *is_wireless = FALSE;
  if (signal_strength)
    *signal_strength = 0;

  if (g_strncasecmp (iface, "an",   2) &&
      g_strncasecmp (iface, "wi",   2) &&
      g_strncasecmp (iface, "ath",  3) &&
      g_strncasecmp (iface, "ndis", 4) &&
      g_strncasecmp (iface, "ipw",  3) &&
      g_strncasecmp (iface, "iwi",  3) &&
      g_strncasecmp (iface, "acx",  3))
    return error_message;

  if (g_strncasecmp (iface, "an", 2) == 0)
    {
      error_message = get_an_data (iface, signal_strength);
      *is_wireless = TRUE;
    }
  else
    {
      error_message = get_wi_data (iface, signal_strength);
      *is_wireless = TRUE;
    }

  return error_message;
}

char *
netstatus_sysdeps_read_iface_statistics (const char *iface,
					 gulong     *in_packets,
					 gulong     *out_packets,
					 gulong     *in_bytes,
					 gulong     *out_bytes)
{
  GError  *error;
  char    *command_line;
  char   **argv;
  char    *error_message = NULL;
  int      pipe_out;

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (in_packets != NULL, NULL);
  g_return_val_if_fail (out_packets != NULL, NULL);
  g_return_val_if_fail (in_bytes != NULL, NULL);
  g_return_val_if_fail (out_bytes != NULL, NULL);

  *in_packets  = -1;
  *out_packets = -1;
  *in_bytes    = -1;
  *out_bytes   = -1;

  error = NULL;
  command_line = g_strdup_printf ("/usr/bin/netstat -n -I %s -b -f inet", iface);
  if (!g_shell_parse_argv (command_line, NULL, &argv, &error))
    {
      error_message = g_strdup_printf (_("Could not parse command line '%s': %s"),
				       command_line,
				       error->message);
      g_error_free (error);
      g_free (command_line);

      return error_message;
    }
  g_free (command_line);

  error = NULL;
  if (g_spawn_async_with_pipes (NULL,
				argv,
				NULL,
				0,
				NULL,
				NULL,
				NULL,
				NULL,
				&pipe_out,
				NULL,
				&error))
    {
      GIOChannel *channel;
      char       *buf;
      int         prx_idx, ptx_idx;
      int         brx_idx, btx_idx;

      channel = g_io_channel_unix_new (pipe_out);

      g_io_channel_read_line (channel, &buf, NULL, NULL, NULL);
      parse_header (buf, &prx_idx, &ptx_idx, &brx_idx, &btx_idx);
      g_free (buf);

      if (prx_idx == -1 || ptx_idx == -1 ||
	  brx_idx == -1 || btx_idx == -1)
	{
	  error_message = g_strdup (_("Could not parse 'netstat' output. Unknown format"));
	  goto error_shutdown;
	}

      g_io_channel_read_line (channel, &buf, NULL, NULL, NULL);

      if (!parse_stats (buf,
			prx_idx, ptx_idx, in_packets, out_packets,
			brx_idx, btx_idx, in_bytes, out_bytes))
	{
	  error_message = g_strdup_printf (_("Could not parse interface statistics from '%s'. "
					     "prx_idx = %d; ptx_idx = %d; brx_idx = %d; btx_idx = %d;"),
					   buf, prx_idx, ptx_idx, brx_idx, btx_idx);
	}
      else if (*in_packets == -1 || *out_packets == -1 || *in_bytes == -1 || *out_bytes == -1)
	{
	  error_message = g_strdup_printf ("Could not obtain information on interface '%s' from netstat",
					   iface);
	}

      g_free (buf);

    error_shutdown:
      g_io_channel_unref (channel);
      close (pipe_out);
    }
  else
    {
      error_message = g_strdup_printf ("Error running /usr/bin/netstat for '%s': %s",
				       iface, error->message);
      g_error_free (error);
    }

  g_strfreev (argv);

  return error_message;
}

#endif /* !defined(__FreeBSD__) */
