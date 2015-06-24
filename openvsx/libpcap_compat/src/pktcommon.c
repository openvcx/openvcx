/** <!--
 *
 *  Copyright (C) 2014 OpenVCX openvcx@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like this software to be made available to you under an 
 *  alternate license please email openvcx@gmail.com for more information.
 *
 * -->
 */


#include <stdio.h>
#include "unixcompat.h"
#include "pthread_compat.h"

#if defined(WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif // WIN32

#include <string.h>

#if defined(WIN32) || defined(__MINGW32__)
#include <sys/types.h>
#else
#include <arpa/inet.h>
#endif // WIN32


#include "pcap_compat.h"
#include "logutil.h"
#include "pkttypes.h"
#include "pktcapture.h"
#include "pktcommon.h"

#if defined(DISABLE_PCAP)


const char *pkt_FindDevDescr(const char *iface)  {
  return "";
}

int pkt_ListInterfaces(FILE* file) {
  return -1;
}

#else // DISABLE_PCAP


const char *pkt_FindDevDescr(const char *iface) {

	pcap_if_t *alldevs = NULL;
	pcap_if_t *d = NULL;
	static char buf[PCAP_ERRBUF_SIZE];

  if(!iface) {
    return "";
  }

	/* Retrieve the device list */
	if(pcap_findalldevs(&alldevs, buf) == -1) {
		return "";
	}

  memset(buf, 0, sizeof(buf));

  for(d=alldevs; d; d=d->next) {
    if(d->name && !strcmp(d->name, iface)) {
      if(d->description) {
        strncpy(buf, d->description, sizeof(buf) -1);
      } else {
        strncpy(buf, "(No Description)", sizeof(buf) -1);
      }
      break;
    }
  }

  pcap_freealldevs(alldevs);

  return buf;
}

int pkt_ListInterfaces(FILE *fp) {

	pcap_if_t *alldevs = NULL;
	pcap_if_t *d = NULL;
	char errbuf[PCAP_ERRBUF_SIZE];
  int i = 0;

	/* Retrieve the device list */
	if(pcap_findalldevs(&alldevs, errbuf) == -1) {
		LOG(X_ERROR("Error in pcap_findalldevs: %s"), errbuf);
		return -1;
	}

  for(d=alldevs; d; d=d->next) {
    fprintf(fp, "%d. %s", ++i, d->name);
    if (d->description) {
      fprintf(fp, " (%s)\n", d->description);
    } else {
      fprintf(fp, " (No description available)\n");
    }
  }

  pcap_freealldevs(alldevs);

  if(i==0) {
#ifdef WIN32
    LOG(X_ERROR("No interfaces found! Make sure WinPcap is installed."));
#else
    LOG(X_ERROR("No interfaces found! You must have root privileges."));
#endif // WIN32
    return -1;
  }

  return 0;
}

#endif // DISABLE_PCAP



char *inet6_ntoa(struct in6_addr in6) {
  static char buf[64];
  sprintf(buf, "%2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x",
   ((uint16_t *) &in6.s6_addr)[0],
   ((uint16_t *) &in6.s6_addr)[2],
   ((uint16_t *) &in6.s6_addr)[4],
   ((uint16_t *) &in6.s6_addr)[6],
   ((uint16_t *) &in6.s6_addr)[8],
   ((uint16_t *) &in6.s6_addr)[10]);

  return buf;
}

