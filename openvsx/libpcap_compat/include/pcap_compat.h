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


#ifndef __PCAP_COMPAT_H__
#define __PCAP_COMPAT_H__

#ifdef DISABLE_PCAP

#if defined(WIN32) && !defined(__MINGW32__)

#include <windows.h>
#include "unixcompat.h"

#if defined(_MSC_VER) && (_MSC_VER < 1600)


struct in6_addr
  {
    union
      {
        uint8_t u6_addr8[16];
        uint16_t u6_addr16[8];
        uint32_t u6_addr32[4];
      } in6_u;

#define s6_addr                 in6_u.u6_addr8
#define s6_addr16               in6_u.u6_addr16
#define s6_addr32               in6_u.u6_addr32
} IN6_ADDR, *PIN6_ADDR, FAR *LPIN6_ADDR;

#else // _MSC_VER

#include "In6addr.h"

#endif // _MSC_VER


#else // WIN32

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#endif // WIN32

#define PCAP_ERRBUF_SIZE                128

//#ifndef u_int32_t
//typedef unsigned int u_int32_t;
//#endif // u_int32_t

typedef int pcap_t;

struct pcap_pkthdr {
  struct timeval ts;    
  uint32_t caplen; 
  uint32_t len;     
};


struct pcap_stat {
  unsigned int ps_recv;          /* number of packets received */
  unsigned int ps_drop;          /* number of packets dropped */
  unsigned int ps_ifdrop;        /* drops by interface XXX not yet supported */
};



#else  // DISABLE_PCAP

#include <pcap.h>

#endif // DISABLE_PCAP

#endif //__PCAP_COMPAT_H__
