/*
 * Copyright (C) 2008 Fred Chien <fred@lxde.org>
 *               2008 Jim Huang <jserv.tw@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <sys/time.h>
#include <iwlib.h>
#include "netstat.h"
#include "wireless.h"

/*
static const char * iw_ie_cypher_name[] = {
    "none",
    "WEP-40",
    "TKIP",
    "WRAP",
    "CCMP",
    "WEP-104",
};

static const char * iw_ie_key_mgmt_name[] = {
    "none",
    "802.1x",
    "PSK",
};
*/

void wireless_aplist_free(void *aplist, GObject *dummy)
{
    APLIST *ptr;
    APLIST *delptr;

    if (aplist!=NULL) {
        ptr = aplist;
        do {
            g_free(ptr->info->essid);
            g_free(ptr->info->apaddr);
            g_free(ptr->info);

            delptr = ptr;
            ptr = ptr->next;
            g_free(delptr);
        } while(ptr!=NULL);
    }
}

void wireless_gen_ie(ap_info *info, unsigned char *buffer, int ielen)
{
	int offset = 2;
	int count;
	int i;
	unsigned char wpa1_oui[3] = {0x00, 0x50, 0xf2};
	unsigned char wpa2_oui[3] = {0x00, 0x0f, 0xac};
	unsigned char *wpa_oui;

	/* check IE type */
	switch(buffer[0]) {
		case 0xdd: /* WPA or else */
			wpa_oui = wpa1_oui;

			if((ielen < 8)
				|| (memcmp(&buffer[offset], wpa_oui, 3) != 0)
				|| (buffer[offset + 3] != 0x01)) {
				if (info->haskey)
					info->en_method = NS_WIRELESS_AUTH_WEP;
				else
					info->en_method = NS_WIRELESS_AUTH_OFF;

				info->key_mgmt = NS_IW_IE_KEY_MGMT_NONE;
				info->group = NS_IW_IE_CIPHER_NONE;
				info->pairwise = NS_IW_IE_CIPHER_NONE;

				return;
			}

			/* OUI and 0x01 */
			offset += 4;
			break;

		case 0x30: /* IEEE 802.11i/WPA2 */
			wpa_oui = wpa2_oui;
			break;

		default: /* Unknown */
			if (info->haskey)
				info->en_method = NS_WIRELESS_AUTH_WEP;
			else
				info->en_method = NS_WIRELESS_AUTH_OFF;

			info->key_mgmt = NS_IW_IE_KEY_MGMT_NONE;
			info->group = NS_IW_IE_CIPHER_NONE;
			info->pairwise = NS_IW_IE_CIPHER_NONE;
			return;
	}

	/* assume TKIP */
	info->en_method = NS_WIRELESS_AUTH_WPA;
	info->key_mgmt = NS_IW_IE_KEY_MGMT_NONE;
	info->group = NS_IW_IE_CIPHER_TKIP;
	info->pairwise = NS_IW_IE_CIPHER_TKIP;

	/* 2 bytes for version number (little endian) */
	offset += 2;

	/* check group cipher for short IE */
	if ((offset+4) > ielen) {
		/* this is a short IE, we can assume TKIP/TKIP. */
		info->group = NS_IW_IE_CIPHER_TKIP;
		info->pairwise = NS_IW_IE_CIPHER_TKIP;
		return;
	}

	/* 4 Bytes for group cipher information [3 bytes][1 Byte] */
	if(memcmp(&buffer[offset], wpa_oui, 3)!=0) {
		/* the group cipher is proprietary */
		info->group = NS_IW_IE_CIPHER_NONE;
	} else {
		/* pick a byte for type of group cipher */
		info->group = buffer[offset+3];
	}
	offset += 4;

	/* check pairwise cipher for short IE */
	if ((offset+2) > ielen) {
		/* this is a short IE, we can assume TKIP. */
		info->pairwise = NS_IW_IE_CIPHER_TKIP;
		return;
	}

	/* 2 bytes for number of pairwise ciphers (little endian) */
	count = buffer[offset] | (buffer[offset + 1] << 8);
	offset += 2;

	/* if we are done */
	if ((offset+4*count) > ielen) {
		return;
	}

	/* choose first cipher of pairwise ciphers to use,
	 * FIXME: Let user decide the cipher is the best way. */
	for(i=0;i<count;i++) {
		if(memcmp(&buffer[offset], wpa_oui, 3)==0) {
			/* pick a byte for type of group cipher */
			info->pairwise = buffer[offset+3];
		}
		offset += 4;
    }

	/* check authentication suites */
	if ((offset+2) > ielen) {
		/* this is a short IE, we can assume TKIP. */
		info->key_mgmt = NS_IW_IE_KEY_MGMT_NONE;
		return;
	}

	/* 2 bytes for number of authentication suites (little endian) */
	count = buffer[offset] | (buffer[offset + 1] << 8);
	offset += 2;

	/* if we are done */
	if ((offset+4*count) > ielen) {
		return;
	}

	/* choose first key_mgmt of authentication suites to use,
	 * FIXME: Let user decide the key_mgmt is the best way. */
	for(i=0;i<count;i++) {
		if(memcmp(&buffer[offset], wpa_oui, 3)==0) {
			/* pick a byte for type of key_mgmt */
			info->key_mgmt = buffer[offset+3];
		}
		offset += 4;
    }
}

ap_info *
wireless_parse_scanning_event(struct iw_event *event, ap_info *oldinfo)
{
	ap_info *info;

	/* found a new AP */
	if (event->cmd==SIOCGIWAP) {
		char buf[128];
		info = g_new0(ap_info, 1);
		info->apaddr = g_strdup(iw_sawap_ntop(&event->u.ap_addr, buf));
		info->en_method = NS_WIRELESS_AUTH_OFF;
		info->haskey = FALSE;
		info->key_mgmt = NS_IW_IE_KEY_MGMT_NONE;
		info->group = NS_IW_IE_CIPHER_TKIP;
		info->pairwise = NS_IW_IE_CIPHER_TKIP;
	} else {
		info = oldinfo;
	}

    switch (event->cmd) {
        case SIOCGIWESSID: /* ESSID */
			if (!event->u.essid.flags
				|| event->u.essid.length==0
				|| strlen(event->u.essid.pointer)==0) {
				info->essid = NULL;
			} else {
				info->essid = g_strndup(event->u.essid.pointer, event->u.essid.length);
			}
            break;
		case IWEVQUAL: /* Signal Quality */
				info->quality = (int)rint((log (event->u.qual.qual) / log (92)) * 100.0);
            break;
        case SIOCGIWENCODE: /* Encryption */
			if (!event->u.data.pointer)
				event->u.data.flags |= IW_ENCODE_NOKEY;

			if (!(event->u.data.flags & IW_ENCODE_DISABLED)) {
				info->haskey = TRUE;
				/* assume WEP */
				info->en_method = NS_WIRELESS_AUTH_WEP;
			} else {
				info->haskey = FALSE;
				info->en_method = NS_WIRELESS_AUTH_OFF;
			}
            break;
		case IWEVGENIE: /* Extra information */
		{
			int offset = 0;
			int ielen = event->u.data.length;
			unsigned char *iebuf = event->u.data.pointer;

			while(offset <= (ielen - 2)) {
				/* check IE type */
				switch(iebuf[offset]) {
					case 0xdd: /* WPA or else */
					case 0x30: /* IEEE 802.11i/WPA2 */
						wireless_gen_ie(info, iebuf, ielen);
						break;
				}
				offset += iebuf[offset+1] + 2;
			}
		}
			break;
	}

    return info;
}

/* when we have some workaround problems,
 * we need this function to rescanning access-point.
 * */
gboolean wireless_refresh(int iwsockfd, const char *ifname)
{
	struct iwreq wrq;
	struct iw_range range;
	struct timeval tv;
	fd_set rfds; /* File descriptors for select */
	int selfd;
	char buffer[IW_SCAN_MAX_DATA];

	/* setting interfaces name */
	strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

	/* Getting range */
	iw_get_range_info(iwsockfd, ifname, &range);

	/* check scanning support */
	if (range.we_version_compiled < 14)
		return FALSE;

	/* Initiate Scanning */
	wrq.u.data.pointer = buffer;
	wrq.u.data.length = IW_SCAN_MAX_DATA;
	wrq.u.data.flags = 0;

	if (ioctl(iwsockfd, SIOCSIWSCAN, &wrq) < 0) {
		if (errno!=EPERM)
			return FALSE;
	}

	/* Init timeout value -> 250ms */
	tv.tv_sec = 0;
	tv.tv_usec = 250000;

	/* Scanning APs */
	while(1) {
		if (ioctl(iwsockfd, SIOCGIWSCAN, &wrq) < 0) {
			if (errno == EAGAIN) { /* not yet ready */
				FD_ZERO(&rfds);
				selfd = -1;

				if (select(selfd + 1, &rfds, NULL, NULL, &tv)==0)
					continue; /* timeout */
			} else {
				break;
			}
		}

		if (wrq.u.data.length <= 0)
			break;
	}

	return TRUE;
}

APLIST *wireless_scanning(int iwsockfd, const char *ifname)
{
	APLIST *ap = NULL;
	APLIST *newap;

	struct iwreq wrq;
	int scanflags = 0;		/* Flags for scan */
	unsigned char *	buffer = NULL;	/* Results */
	int buflen = IW_SCAN_MAX_DATA;	/* Min for compat WE < 17 */
	struct iw_range	range;
	int has_range;
	struct timeval tv;		/* select timeout */
	int timeout = 15000000;		/* 15s */

	/* Get range stuff */
	has_range = (iw_get_range_info(iwsockfd, ifname, &range) >= 0);

	/* Check if the interface could support scanning. */
	if ((!has_range) || (range.we_version_compiled < 14)) {
		fprintf(stderr, "%-8.16s  Interface doesn't support scanning.\n\n",
				ifname);
		return NULL;
	}

	/* Init timeout value -> 250ms between set and first get */
	tv.tv_sec = 0;
	tv.tv_usec = 250000;

	wrq.u.data.pointer = NULL;
	wrq.u.data.flags = 0;
	wrq.u.data.length = 0;

	/* Initiate Scanning */
	if (iw_set_ext(iwsockfd, ifname, SIOCSIWSCAN, &wrq) < 0) {
		if ((errno != EPERM) || (scanflags != 0)) {
			fprintf(stderr, "%-8.16s  Interface doesn't support "
				"scanning : %s\n\n", ifname, strerror(errno));
			return NULL;
		}
		tv.tv_usec = 0;
	}
	timeout -= tv.tv_usec;

	/* Forever */
	while (1) {
		fd_set rfds;		/* File descriptors for select */
		int last_fd;	/* Last fd */
		int ret;

		/* Guess what ? We must re-generate rfds each time */
		FD_ZERO(&rfds);
		last_fd = -1;

		/* In here, add the rtnetlink fd in the list */

		/* Wait until something happens */
		ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

		/* Check if there was an error */
		if (ret < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fprintf(stderr, "Unhandled signal - exiting...\n");
			return NULL;
		}

		/* Check if there was a timeout */
		if (ret == 0) {
			unsigned char *newbuf;

realloc:
			/* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
			newbuf = realloc(buffer, buflen);
			if (newbuf == NULL) {
				if (buffer)
					free(buffer);
				fprintf(stderr, "%s: Allocation failed\n", __FUNCTION__);
				return NULL;
			}
			buffer = newbuf;

			/* Try to read the results */
			wrq.u.data.pointer = buffer;
			wrq.u.data.flags = 0;
			wrq.u.data.length = buflen;
			if (iw_get_ext(iwsockfd, ifname, SIOCGIWSCAN, &wrq) < 0)	{
				/* Check if buffer was too small (WE-17 only) */
				if ((errno == E2BIG) &&
				    (range.we_version_compiled > 16)) {
					/* Check if the driver gave us any hints. */
					if (wrq.u.data.length > buflen)
						buflen = wrq.u.data.length;
					else
						buflen *= 2;
					/* Try again */
					goto realloc;
				}

				/* Check if results not available yet */
				if(errno == EAGAIN) {
					/* Restart timer for only 100ms*/
					tv.tv_sec = 0;
					tv.tv_usec = 100000;
					timeout -= tv.tv_usec;
					if (timeout > 0)
						continue; /* Try again later */
				}

				/* Bad error */
				free(buffer);
				fprintf(stderr,
				"%-8.16s  Failed to read scan data : %s\n\n",
						ifname, strerror(errno));
				return NULL;
			}
			else
				/* We have the results, go to process them */
				break;
		}

		/* In here, check if event and event type
		 * if scan event, read results. All errors bad & no reset timeout */
	}

	if(wrq.u.data.length) {
		struct iw_event           iwe;
		struct stream_descr       stream;
		int                       ret;

		iw_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
		do {
			/* Extract an event and print it */
			ret = iw_extract_event_stream(&stream, &iwe, range.we_version_compiled);
			if (iwe.cmd==SIOCGIWAP) {
				newap = malloc(sizeof(APLIST));
				newap->info = NULL;
				newap->next = ap;
				ap = newap;
			}
			ap->info = wireless_parse_scanning_event(&iwe, ap->info);
		}
		while (ret > 0);
		printf("\n");
	}
	else
		printf("%-8.16s  No scan results\n\n", ifname);

	free(buffer);
	return ap;
}
