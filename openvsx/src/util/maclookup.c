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

#ifdef WIN32

#include "winsock2.h"

//#define  _WS2IPDEF_  1
//#include "pcap_compat.h"
#include <windows.h>

#include "unixcompat.h"
#include "pthread_compat.h"

#include "Iphlpapi.h"

typedef uint32_t in_addr_t;

#else  // WIN32

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pcap_compat.h"

#endif // WIN32

#include <stdio.h>
#include <string.h>

#include "logutil.h"
#include "pktgen.h"
#include "util/getmacbyarp.h"

int maclookup_GetMac(long ipv4, unsigned char mac[], unsigned short vlan) {


  int rc = -1;
  char *pMac = NULL;

#ifdef WIN32

  unsigned long sz;
  MIB_IPNETTABLE *pIpNetTable = NULL;
  MIB_IPNETROW  *pIpNetRow = NULL;
  MIB_IPFORWARDROW ipForwardRow;
  char buf[4096];

#endif // WIN32

  if(mac == NULL) {
    return -1;
  }

  //
  // Return MAC of local interface
  //
  if((long) pktgen_GetLocalIp() == ipv4) {
    if((pMac = (char *) pktgen_GetLocalMac()) == NULL) {
      return -1;
    }

    memcpy(mac, pktgen_GetLocalMac(), ETH_ALEN);
    return 0;
  }

#ifdef WIN32

  //
  // Look through the list of system arp entries for an exact match of 
  // the destination ip
  //
  sz = sizeof(buf);
  if((rc = GetIpNetTable((MIB_IPNETTABLE *) buf, &sz, TRUE)) == NO_ERROR) {
    pIpNetTable = (MIB_IPNETTABLE *) buf;
    for(sz = 0; sz < pIpNetTable->dwNumEntries; sz++) {
      pIpNetRow = &pIpNetTable->table[sz];

      if(pIpNetRow->dwAddr == (unsigned long) ipv4) {
        memcpy(mac, pIpNetRow->bPhysAddr, ETH_ALEN);
        LOG(X_DEBUG("Found mac in system arp table %x:%x:%x:%x:%x:%x for %s"),
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], inet_ntoa(*(struct in_addr *)&ipv4));
        return 0;
      }
    }

    // If no exact match was found

  } else {
    LOG(X_ERROR("GetIpNetTable returned %d"), rc);
    rc = -1;
  }

#else // WIN32

  if(mac == NULL) {
    return -1;
  }

#endif // WIN32

  //
  // If there is no exact match in the system arp table, attempt to arp
  // for the mac manually.  A found mac will be stored in the internal 
  // cache available for future lookups.
  //
  if((rc = arpmac_GetMac(ipv4, mac, vlan)) == 0) {
    return rc;
  }

  LOG(X_WARNING("Unable to arp for %s"), inet_ntoa(*((struct in_addr *)&ipv4)));


#ifdef WIN32

  //
  // Attempt to obtain the next hop best route (router).  
  // And then the mac address via arp of the destination router
  //
  rc = 0;
  memset(&ipForwardRow, 0, sizeof(ipForwardRow));

  if((rc = GetBestRoute(ipv4, pktgen_GetLocalIp(), &ipForwardRow)) != NO_ERROR) {
    LOG(X_ERROR("Unable to find best route to %s from interface: %s"), 
        inet_ntoa(*(struct in_addr *)&ipv4), pktgen_GetLocalIp());
    rc = -1;
  }

  if(rc != -1 && ipForwardRow.dwForwardNextHop == pktgen_GetLocalIp()) {
    LOG(X_ERROR("Route to %s next hop matches local interface"), 
      inet_ntoa(*((struct in_addr *)&ipForwardRow.dwForwardNextHop)));
    rc = -1;
  }

  if(rc != -1 && (rc = arpmac_GetMac(ipForwardRow.dwForwardNextHop, mac, vlan)) == 0) {
    LOG(X_INFO("Using gateway %s %x:%x:%x:%x:%x:%x as destination"),
          inet_ntoa(*(struct in_addr *)&ipForwardRow.dwForwardNextHop), 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return rc;
  }


  //
  // Attempt to return the MAC of the gateway
  //
  //if(pIpNetTable && pIpNetTable->dwNumEntries > 0) {
  //  pIpNetRow = &pIpNetTable->table[0];
  //  memcpy(mac, pIpNetRow->bPhysAddr, ETH_ALEN);
  //  LOG(X_WARNING("Warning using unknown gateway mac %x:%x:%x:%x:%x:%x for %s"),
  //    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], inet_ntoa(*((struct in_addr *) &ipv4)));
  //  return 1;
  //}

#endif // WIN32

  return rc;
}

int maclookup_Init(unsigned int expirationSec) {
  int rc;

  if((rc = arpmac_Init(expirationSec)) != 0) {
    return rc;
  }
  return 0;
}


