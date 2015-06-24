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

//#undef DISABLE_PCAP

#include "pcap_compat.h"

#if defined(WIN32) 

#if defined(_MSC_VER) && (_MSC_VER < 1500)
#include "Win32-Extensions.h"
#endif // _MSC_VER

#include <windows.h>

#ifndef DISABLE_PCAP

#include "ntddndis.h"
#undef _WS2IPDEF_ 

#endif // DISABLE_PCAP

#include <Iphlpapi.h>

#include "unixcompat.h"
#include "pthread_compat.h"

//mingw does not define in_addr_t
typedef uint32_t in_addr_t;




#else // WIN32

#include <unistd.h>
#include <stdint.h>

//#if !defined(__MINGW32__)
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
//#endif // __MINGW32__

#include <sys/types.h>
#if defined(__linux__)
#include <linux/sockios.h>
#include <linux/netdevice.h>
#include <linux/filter.h>
//#elif !defined(__MINGW32__)
#else

#if !defined(IPHONE)
#include <net/route.h>
#include <net/if.h>
#endif //IPHONE

#endif // __linux__

#if defined(__APPLE__)
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#endif // __APPLE__

#include <errno.h>

//#ifdef __linux__
//static struct sockaddr_ll g_sall;
//#endif // __linux__

#endif // WIN32

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pktgen.h"
#include "pktcommon.h"
#include "logutil.h"


#if defined(WIN32)

int pktgen_GetLocalAdapterInfo(const char *dev,
                               unsigned char *pmacaddr, 
                               in_addr_t *pnetaddr) {
//#ifdef DISABLE_PCAP
//  return -1;
//#else // DISABLE_PCAP

  IP_ADAPTER_INFO adapterInfo[10];
  IP_ADAPTER_INFO *pAdapterInfo = NULL;
  unsigned long adaptersLen = sizeof(adapterInfo);
  unsigned int rc;

  memset(adapterInfo, 0, sizeof(adapterInfo));

  if((rc = GetAdaptersInfo(adapterInfo, &adaptersLen)) != ERROR_SUCCESS) {
    LOG(X_ERROR("GetAdaptersInfo Failed with code: %d"),  rc);
    return -1;
  }

  pAdapterInfo = adapterInfo;

  do {

    if(pAdapterInfo->Type == MIB_IF_TYPE_ETHERNET) {
      if(!dev || (strstr(dev, pAdapterInfo->AdapterName) &&
         pAdapterInfo->AddressLength == ETH_ALEN)) {
        
        if(pmacaddr) {
          memcpy(pmacaddr, pAdapterInfo->Address, ETH_ALEN);
        }

        if(pnetaddr) {
          if((*pnetaddr = 
               inet_addr(pAdapterInfo->IpAddressList.IpAddress.String)) == INADDR_NONE) {
            LOG(X_ERROR("Cannot determinte IP address for adapter '%s'"), 
                pAdapterInfo->AdapterName);
            return -1;  
          } else if(dev || *pnetaddr != INADDR_ANY) {
            return 0;
          }
        } else {
          return 0;
        }
      }
    }

    pAdapterInfo = pAdapterInfo->Next;

  } while(pAdapterInfo);

  LOG(X_ERROR("Unable to find network adapter.  None matching name '%s'"), 
              dev ? dev : "<any>");

  return -1;
//#endif // DISABLE_PCAP
}

#elif defined(__APPLE__) 

#define MAX(x,y)  ((x) > (y) ? (x) : (y))
#define MIN(x,y)  ((x) < (y) ? (x) : (y))

int pktgen_GetLocalAdapterInfo(const char *dev,
                               unsigned char *pmacaddr,
                               in_addr_t *pnetaddr) {
  int fd;
  int rc = -1;
  struct ifreq *ifr;
  struct ifconf ifc;
  struct sockaddr_in *sin;
  struct sockaddr_dl *sdl;
  const char *curdev = NULL;
  char *cp, *cplim;
  int valid_mac = 0;
  int stored_mac = 0;
  int stored_ip = 0;
  int valid_ip = 0;
  char tmp[80];
  int tmpi[6];
  char buf[4000];

  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
    return -1;
  }

  memset(&ifc, 0, sizeof(ifc));
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;

  if(ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
    LOG(X_ERROR("ioctl SIOCGIFCONF failed"));
    close(fd);
    return -1;
  }

  //fprintf(stderr, "DEV:'%s'\n", dev ? dev : "NULL");

  cplim = buf + ifc.ifc_len;
  for(cp = buf; 
     cp < cplim; 
     cp += sizeof(ifr->ifr_name) + MAX(sizeof(ifr->ifr_addr), ifr->ifr_addr.sa_len)) {

    ifr = (struct ifreq *) cp;

    if(dev && strncmp(ifr->ifr_name, dev, strlen(dev))) {
      continue;
    }

    curdev = ifr->ifr_name;

    if(ifr->ifr_addr.sa_family == AF_LINK) {

      sdl = (struct sockaddr_dl *) &ifr->ifr_addr;
      strncpy(tmp,  ether_ntoa((const struct ether_addr *) LLADDR(sdl)), sizeof(tmp));
      sscanf(tmp,"%x:%x:%x:%x:%x:%x", 
              &tmpi[0], &tmpi[1], &tmpi[2], &tmpi[3], &tmpi[4], &tmpi[5]);

      tmp[0] = tmpi[0];
      tmp[1] = tmpi[1];
      tmp[2] = tmpi[2];
      tmp[3] = tmpi[3];
      tmp[4] = tmpi[4];
      tmp[5] = tmpi[5];

      if(!memcmp(tmp, (unsigned char[]) { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, ETH_ALEN)) {
        continue;
      }

      //fprintf(stderr, "'%s' MAC: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", curdev, tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);

      if(pmacaddr) {
        if(dev || !valid_mac || !stored_mac) {  
          memcpy(pmacaddr, tmp, ETH_ALEN);

          // If dev is not specified, only store the first 'enX' interface 
          if(!dev && !strncmp(curdev, "en", 2)) {
            stored_mac = 1;
          }
        }
      }

      if(!valid_mac) {
        valid_mac = 1;
      }

    } else if(ifr->ifr_addr.sa_family == AF_INET) {

      if((ifr->ifr_flags & IFF_UP) == 0) {
        sin = (struct sockaddr_in *) &ifr->ifr_addr;
        if(((htonl(sin->sin_addr.s_addr) & 0xff000000) >> 24) == 127) {
          continue;
        }
        //fprintf(stderr, "IP: 0x%x\n", sin->sin_addr.s_addr);

        if(pnetaddr) {
          if(dev || !valid_ip || !stored_ip) {
            *pnetaddr = sin->sin_addr.s_addr;
          }
          if(!dev) {
            stored_ip = 1;
          }
        }

        if(!valid_ip) {
          valid_ip = 1;
        }
      }

    }

  }

  close(fd);

  if((!pmacaddr || valid_mac) && (!pnetaddr || valid_ip)) {
    rc = 0;
  }

  return rc;
}

#else // __linux__

int pktgen_GetLocalAdapterInfo(const char *dev,
                               unsigned char *pmacaddr,
                               in_addr_t *pnetaddr) {
  int fd;
  unsigned int idx;
  struct ifreq ifr;
  struct ifconf ifc;
  struct sockaddr_in *local_ip;
  char buf[1024];

  //if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
    //LOG(X_ERROR("Failed to open SOCK_RAW, IPPROTO_RAW"));
    return -1;
  }

  if(!dev) {
    memset(&ifc, 0, sizeof(ifc));
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
      LOG(X_ERROR("ioctl SIOCGIFCONF failed"));
      close(fd);
      return -1;
    }
    for(idx = 0; idx < ifc.ifc_len / sizeof(struct ifreq); idx++) {
      memset(&ifr, 0, sizeof(ifr));
      strncpy(ifr.ifr_name, ((struct ifreq *) ifc.ifc_req)[idx].ifr_name,
              sizeof(ifr.ifr_name) - 1);
      if(ioctl(fd, SIOCGIFFLAGS, &ifr) == 0 &&
           !(ifr.ifr_flags & IFF_LOOPBACK)) {
        strncpy(buf, ifr.ifr_name, sizeof(buf) - 1);
        dev = buf;
        break;
      }
    } 
  }

  if(!dev) {
    LOG(X_WARNING("No matching network interface found."));
    close(fd);
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy (ifr.ifr_name, dev, sizeof(ifr.ifr_name) -1);

  if(pnetaddr) {
    if(ioctl(fd, SIOCGIFADDR, &ifr) != 0) {
      LOG(X_ERROR("SIOCGIFADDR failed:%s (%s)."), strerror(errno), dev);
      close(fd);
      return -1;
    }
    local_ip = (struct sockaddr_in *) &ifr.ifr_addr;
    *pnetaddr = local_ip->sin_addr.s_addr;
  }

  if(pmacaddr) {
#if defined(__linux__)
    if(ioctl (fd, SIOCGIFHWADDR, &ifr) != 0) {
      LOG(X_DEBUG("SIOCGIFHWADDR failed!:%s"), strerror (errno));
      close(fd);
      return -1;
    }
    memcpy(pmacaddr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
  }
#else // __linux__
    LOG(X_ERROR("ioctl SIOCGIFHWADDR not available"));
    close(fd);
    return -1;
  }
#endif // __linux
  close (fd);

  return 0;
}


#endif // WIN32

