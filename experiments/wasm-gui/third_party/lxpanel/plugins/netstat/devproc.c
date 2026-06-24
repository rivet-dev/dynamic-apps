/*
 * Copyright (C) 2008 Fred Chien <fred@lxde.org>
 *               2012 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2014-2015 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <string.h>
#include <glib.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <linux/types.h>
#include <linux/ethtool.h>
#include <iwlib.h>
#include "nsconfig.h"
#include "netstat.h"
#include "statusicon.h"
#include "devproc.h"
#include "dbg.h"

/* network device list */
static void netproc_netdevlist_add(NETDEVLIST_PTR *netdev_list,
                                   const char *ifname,
                                   gulong recv_bytes,
                                   gulong recv_packets,
                                   gulong trans_bytes,
                                   gulong trans_packets,
                                   gboolean wireless)
{
	NETDEVLIST_PTR new_dev;

	new_dev = malloc(sizeof(NETDEVLIST));
	new_dev->info.ifname = g_strdup(ifname);
	new_dev->info.mac = NULL;
	new_dev->info.ipaddr = NULL;
	new_dev->info.dest = NULL;
	new_dev->info.bcast = NULL;
	new_dev->info.mask = NULL;
	new_dev->info.alive = TRUE;
	new_dev->info.enable = FALSE;
	new_dev->info.updated = TRUE;
	new_dev->info.plug = TRUE;
	new_dev->info.connected = TRUE;
	new_dev->info.wireless = wireless;
	new_dev->info.status = NETDEV_STAT_NORMAL;
	new_dev->info.recv_bytes = recv_bytes;
	new_dev->info.recv_packets = recv_packets;
	new_dev->info.trans_bytes = trans_bytes;
	new_dev->info.trans_packets = trans_packets;
	new_dev->info.status_icon = NULL;
	new_dev->info.pg = NULL;
	new_dev->prev = NULL;
	new_dev->next = *netdev_list;
	if (new_dev->next!=NULL) {
		new_dev->next->prev = new_dev;
	}
	*netdev_list = new_dev;
}

static void netproc_netdevlist_destroy(NETDEVLIST_PTR netdev_list)
{
	g_free(netdev_list->info.ifname);
	g_free(netdev_list->info.mac);
	g_free(netdev_list->info.ipaddr);
	g_free(netdev_list->info.dest);
	g_free(netdev_list->info.bcast);
	g_free(netdev_list->info.mask);
	statusicon_destroy(netdev_list->info.status_icon);
}

int netproc_netdevlist_clear(NETDEVLIST_PTR *netdev_list)
{
	NETDEVLIST_PTR ptr;
	NETDEVLIST_PTR ptr_del;

	if (*netdev_list==NULL)
		return 0;

	ptr = *netdev_list;
	while(ptr->next!=NULL) {
		ptr_del = ptr;
		ptr = ptr->next;
		netproc_netdevlist_destroy(ptr_del);
		free(ptr_del);
	}

	*netdev_list = NULL;

	return 0;
}


static NETDEVLIST_PTR netproc_netdevlist_find(
		NETDEVLIST_PTR netdev_list, const char *ifname)
{
	NETDEVLIST_PTR ptr;

	if (netdev_list==NULL)
		return NULL;

	ptr = netdev_list;
	do {

		if (strcmp(ptr->info.ifname, ifname)==0)
			return ptr;

		ptr = ptr->next;
	} while(ptr!=NULL);

	return NULL;
}

static char *netproc_parse_ifname(const char *buffer)
{
	char *ptr;
	if ((ptr = strchr(buffer, ':'))) {
		*ptr++ = '\0';
	}

	return ptr;
}

static void netproc_parse_stats_header(char *buffer,
                                       int *prx_idx,
                                       int *ptx_idx,
                                       int *brx_idx,
                                       int *btx_idx)
{
	char *p;
	int i;

	*prx_idx = *ptx_idx = -1;
	*brx_idx = *btx_idx = -1;

	p = strtok (buffer, "| \t\n");
	p = strtok (NULL, "| \t\n");
	for (i = 0; p; i++, p = strtok (NULL, "| \t\n")) {
		if (!strcmp (p, "packets")) {
			if (*prx_idx == -1)
				*prx_idx = i;
			else
				*ptx_idx = i;
		} else if (!strcmp (p, "bytes")) {
			if (*brx_idx == -1)
				*brx_idx = i;
			else
				*btx_idx = i;
		}
	}
}

static gboolean netproc_parse_status(char *buffer,
                                     int prx_idx,
                                     int ptx_idx,
                                     gulong *in_packets,
                                     gulong *out_packets,
                                     int brx_idx,
                                     int btx_idx,
                                     gulong *in_bytes,
                                     gulong *out_bytes)
{
	char *ptr;
	int i;
	ptr = strtok(buffer, " \t\n");
	for (i=0;ptr;i++, ptr = strtok(NULL, " \t\n")) {
		if (i==prx_idx)
			*in_packets = g_ascii_strtoull(ptr, NULL, 10);
		else if (i==ptx_idx)
			*out_packets = g_ascii_strtoull(ptr, NULL, 10);
		else if (i==brx_idx)
			*in_bytes = g_ascii_strtoull(ptr, NULL, 10);
		else if (i==btx_idx)
			*out_bytes = g_ascii_strtoull(ptr, NULL, 10);
	}

	if (i <= prx_idx || i <= ptx_idx || i <= brx_idx || i <=btx_idx)
		return FALSE;

	return TRUE;
}

FILE *netproc_open()
{
	return fopen("/proc/net/dev", "r");
}

void netproc_close(FILE *fp)
{
	fclose(fp);
}

int netproc_scandevice(int sockfd, int iwsockfd, FILE *fp, NETDEVLIST_PTR *netdev_list)
{
	char buffer[512];
	int count = 0;
	int prx_idx, ptx_idx, brx_idx, btx_idx;
	gulong in_packets, out_packets, in_bytes, out_bytes;
	NETDEVLIST_PTR devptr = NULL;

	/* interface information */
	struct ifreq ifr;
	struct ethtool_test edata;
	iwstats iws;
	char *status;
	char *name;
	struct iw_range iwrange;
	int has_iwrange = 0;

	status = fgets (buffer, sizeof(buffer), fp);
	if (!status)
		g_warning("netstat: netproc_scnadevice(): Error reading first line from stream!");
	status = fgets (buffer, sizeof(buffer), fp);
	if (!status)
		g_warning("netstat: netproc_scnadevice(): Error reading second line from stream!");
	netproc_parse_stats_header(buffer, &prx_idx, &ptx_idx, &brx_idx, &btx_idx);

	while (fgets(buffer, sizeof(buffer), fp)) {
		/* getting interface name */
		name = buffer;
		while (g_ascii_isspace(name[0])) {
			name++;
		}

		/* reading packet infomation */
		in_packets = out_packets = in_bytes = out_bytes = 0L;
		status = netproc_parse_ifname(name);
		netproc_parse_status(status, prx_idx, ptx_idx, &in_packets, &out_packets,
				     brx_idx, btx_idx, &in_bytes, &out_bytes);

		/* check interface hw_type */
		bzero(&ifr, sizeof(ifr));
		strncpy(ifr.ifr_name, name, strlen(name));
		ifr.ifr_name[strlen(name)+1] = '\0';
		if (ioctl(sockfd, SIOCGIFHWADDR, &ifr)<0)
			continue;

		/* hw_types is not Ethernet and PPP */
		if (ifr.ifr_hwaddr.sa_family!=ARPHRD_ETHER&&ifr.ifr_hwaddr.sa_family!=ARPHRD_PPP)
			continue;

		/* detecting new interface */
		if ((devptr = netproc_netdevlist_find(*netdev_list, name))==NULL) {
			/* check wireless device */
			has_iwrange = (iw_get_range_info(iwsockfd, name, &iwrange)>=0);
			if (!(has_iwrange) || (iwrange.we_version_compiled < 14))
				netproc_netdevlist_add(netdev_list, name, in_bytes, in_packets, out_bytes, out_packets, FALSE);
			else
				netproc_netdevlist_add(netdev_list, name, in_bytes, in_packets, out_bytes, out_packets, TRUE);

			devptr = netproc_netdevlist_find(*netdev_list, name);

			/* MAC Address */
			devptr->info.mac = g_strdup_printf ("%02X:%02X:%02X:%02X:%02X:%02X",
					ifr.ifr_hwaddr.sa_data[0] & 0377,
					ifr.ifr_hwaddr.sa_data[1] & 0377,
					ifr.ifr_hwaddr.sa_data[2] & 0377,
					ifr.ifr_hwaddr.sa_data[3] & 0377,
					ifr.ifr_hwaddr.sa_data[4] & 0377,
					ifr.ifr_hwaddr.sa_data[5] & 0377);
		} else {
			/* Setting device status and update flags */
			if (devptr->info.recv_packets!=in_packets&&devptr->info.trans_packets!=out_packets) {
				if (devptr->info.status!=NETDEV_STAT_BOTHRS)
					devptr->info.updated = TRUE;

				devptr->info.status = NETDEV_STAT_BOTHRS;
			} else if (devptr->info.recv_packets!=in_packets) {
				if (devptr->info.status!=NETDEV_STAT_RECVDATA)
					devptr->info.updated = TRUE;

				devptr->info.status = NETDEV_STAT_RECVDATA;
			} else if (devptr->info.trans_packets!=out_packets) {
				if (devptr->info.status!=NETDEV_STAT_SENDDATA)
					devptr->info.updated = TRUE;

				devptr->info.status = NETDEV_STAT_SENDDATA;
			} else {
				if (devptr->info.status!=NETDEV_STAT_NORMAL)
					devptr->info.updated = TRUE;

				devptr->info.status = NETDEV_STAT_NORMAL;
			}

			/* Recording r/t information */
			devptr->info.recv_bytes = in_bytes;
			devptr->info.recv_packets = in_packets;
			devptr->info.trans_bytes = out_bytes;
			devptr->info.trans_packets = out_packets;

			/* give device a life */
			devptr->info.alive = TRUE;
		}

		/* Enable */
		bzero(&ifr, sizeof(ifr));
		strcpy(ifr.ifr_name, devptr->info.ifname);
		ifr.ifr_name[IF_NAMESIZE - 1] = '\0';
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr)>=0) {
			devptr->info.flags = ifr.ifr_flags;
			if (ifr.ifr_flags & IFF_UP) {
				devptr->info.enable = TRUE;
				devptr->info.updated = TRUE;
			} else {
				devptr->info.enable = FALSE;
				devptr->info.updated = TRUE;
			}

			if (devptr->info.enable) {
				/* Workaround for Atheros Cards */
				if (strncmp(devptr->info.ifname, "ath", 3)==0)
					wireless_refresh(iwsockfd, devptr->info.ifname);

				/* plug */
				bzero(&ifr, sizeof(ifr));
				strcpy(ifr.ifr_name, devptr->info.ifname);
				ifr.ifr_name[IF_NAMESIZE - 1] = '\0';

				edata.cmd = 0x0000000a;
				ifr.ifr_data = (caddr_t)&edata;
				if (ioctl(sockfd, SIOCETHTOOL, &ifr)<0) {
					/* using IFF_RUNNING instead due to system doesn't have ethtool or working in non-root */
					if (devptr->info.flags & IFF_RUNNING) {
						if (!devptr->info.plug) {
							devptr->info.plug = TRUE;
							devptr->info.updated = TRUE;
						}
					} else if (devptr->info.plug) {
						devptr->info.plug = FALSE;
						devptr->info.updated = TRUE;
					}
				} else {
					if (edata.data) {
						if (!devptr->info.plug) {
							devptr->info.plug = TRUE;
							devptr->info.updated = TRUE;
						}
					} else if (devptr->info.plug) {
						devptr->info.plug = FALSE;
						devptr->info.updated = TRUE;
					}
				}

				/* get network information */
				if (devptr->info.enable&&devptr->info.plug) {
					if (devptr->info.flags & IFF_RUNNING) {
						/* release old information */
						g_free(devptr->info.ipaddr);
						g_free(devptr->info.bcast);
						g_free(devptr->info.mask);

						/* IP Address */
						bzero(&ifr, sizeof(ifr));
						strcpy(ifr.ifr_name, devptr->info.ifname);
						ifr.ifr_name[IF_NAMESIZE - 1] = '\0';
						if (ioctl(sockfd, SIOCGIFADDR, &ifr)<0)
							devptr->info.ipaddr = g_strdup("0.0.0.0");
						else
							devptr->info.ipaddr = g_strdup(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));

						/* Point-to-Porint Address */
						if (devptr->info.flags & IFF_POINTOPOINT) {
							bzero(&ifr, sizeof(ifr));
							strcpy(ifr.ifr_name, devptr->info.ifname);
							ifr.ifr_name[IF_NAMESIZE - 1] = '\0';
							if (ioctl(sockfd, SIOCGIFDSTADDR, &ifr)<0)
								devptr->info.dest = NULL;
							else
								devptr->info.dest = g_strdup(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_dstaddr)->sin_addr));
						}

						/* Broadcast */
						if (devptr->info.flags & IFF_BROADCAST) {
							bzero(&ifr, sizeof(ifr));
							strcpy(ifr.ifr_name, devptr->info.ifname);
							ifr.ifr_name[IF_NAMESIZE - 1] = '\0';
							if (ioctl(sockfd, SIOCGIFBRDADDR, &ifr)<0)
								devptr->info.bcast = NULL;
							else
								devptr->info.bcast = g_strdup(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_broadaddr)->sin_addr));
						}

						/* Netmask */
						bzero(&ifr, sizeof(ifr));
						strcpy(ifr.ifr_name, devptr->info.ifname);
						ifr.ifr_name[IF_NAMESIZE - 1] = '\0';
						if (ioctl(sockfd, SIOCGIFNETMASK, &ifr)<0)
							devptr->info.mask = NULL;
						else
							devptr->info.mask = g_strdup(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));

						/* Wireless Information */
						if (devptr->info.wireless) {
							struct wireless_config wconfig;

							/* get wireless config */
							if (iw_get_basic_config(iwsockfd, devptr->info.ifname, &wconfig)>=0) {
								/* Protocol */
								devptr->info.protocol = g_strdup(wconfig.name);
								/* ESSID */
								devptr->info.essid = g_strdup(wconfig.essid);

								/* Signal Quality */
								iw_get_stats(iwsockfd, devptr->info.ifname, &iws, &iwrange, has_iwrange);
								devptr->info.quality = rint((log (iws.qual.qual) / log (92)) * 100.0);
							}
						}

						/* check problem connection */
						if (strcmp(devptr->info.ipaddr, "0.0.0.0")==0) {
							devptr->info.status = NETDEV_STAT_PROBLEM;
							/* has connection problem  */
							if (devptr->info.connected) {
								devptr->info.connected = FALSE;
								devptr->info.updated = TRUE;
							}
						} else if (!devptr->info.connected) {
								devptr->info.status = NETDEV_STAT_NORMAL;
								devptr->info.connected = TRUE;
								devptr->info.updated = TRUE;
						}
					} else {
						/* has connection problem  */
						devptr->info.status = NETDEV_STAT_PROBLEM;
						if (devptr->info.connected) {
							devptr->info.connected = FALSE;
							devptr->info.updated = TRUE;
						}
					}
				}
			}
		}

		devptr = NULL;
		count++;
	}

	rewind(fp);
	fflush(fp);

	return count;
}

static void netproc_alive(NETDEVLIST_PTR netdev_list)
{
	NETDEVLIST_PTR ptr;

	if (netdev_list==NULL) {
		return;
	}

	ptr = netdev_list;
	do {
		ptr->info.alive = FALSE;
		ptr = ptr->next;
	} while(ptr!=NULL);
}

void netproc_devicelist_clear(NETDEVLIST_PTR *netdev_list)
{
	NETDEVLIST_PTR ptr;
	NETDEVLIST_PTR next_ptr;

	for (ptr = *netdev_list; ptr != NULL; ptr = next_ptr) {
		next_ptr = ptr->next;
		if (!ptr->info.alive) { /* if device was removed */
			if (ptr->prev != NULL)
				ptr->prev->next = ptr->next;
			if (ptr->next != NULL)
				ptr->next->prev = ptr->prev;
			if (ptr == *netdev_list)
				*netdev_list = ptr->next;

			netproc_netdevlist_destroy(ptr);
			g_free(ptr);
		}
	}
}

void netproc_listener(FNETD *fnetd)
{
	if (fnetd->sockfd) {
		netproc_alive(fnetd->netdevlist);
		fnetd->netdev_fp = netproc_open();
		netproc_scandevice(fnetd->sockfd, fnetd->iwsockfd, fnetd->netdev_fp, &fnetd->netdevlist);
		netproc_close(fnetd->netdev_fp);
	}
}

#ifdef DEBUG
void netproc_print(NETDEVLIST_PTR netdev_list)
{
	NETDEVLIST_PTR ptr;

	if (netdev_list==NULL) {
		return;
	}

	ptr = netdev_list;
	do {
		printf("%s: %d\n", ptr->info.ifname, ptr->info.status);
		ptr = ptr->next;
	} while(ptr!=NULL);
}
#endif
