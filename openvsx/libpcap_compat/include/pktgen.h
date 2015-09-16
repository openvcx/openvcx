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


#ifndef __PACKET_GEN_H__
#define __PACKET_GEN_H__


#ifndef WIN32
#include <netinet/in.h>
#endif // WIN32

#include "pktcommon.h"
#include "pkttypes.h"


#define PACKETGEN_PKT_UDP_DATA_SZ    4096

typedef struct PACKETGEN_PKT_UDP {
    struct ether_header *peth;
    union {
      struct ip *pip4;
      struct ip6_hdr *pip6;
    } u_ip;
    struct udphdr *pudp;
    unsigned char data[PACKETGEN_PKT_UDP_DATA_SZ];
} PACKETGEN_PKT_UDP_T;

 
int pktgen_InitUdpPacketIpv4(PACKETGEN_PKT_UDP_T *pkt,
                          const unsigned char srcMac[],
                          const unsigned char dstMac[],
                          unsigned short haveVlan,
                          unsigned short vlan,
                          unsigned char tos,
                          unsigned int srcIp,
                          unsigned int dstIp,
                          unsigned short srcPort,
                          unsigned short dstPort,
                          unsigned short lenPayload);

int pktgen_ChecksumUdpPacketIpv4(PACKETGEN_PKT_UDP_T *pkt);


#define pktgen_ListInterfaces      pkt_ListInterfaces
extern int pkt_ListInterfaces(FILE *fp);

int pktgen_GetLocalAdapterInfo(const char *dev,
                               unsigned char *pmacaddr, 
                               in_addr_t *pnetaddr);

int pktgen_SetInterface(const char *iface);
const char *pktgen_GetInterface();
const unsigned char *pktgen_GetLocalMac();
unsigned int pktgen_GetLocalIp();
int pktgen_Init();
int pktgen_Close();

int pktgen_Send(const unsigned char *packet, unsigned int len);

int pktgen_Queue(const unsigned char *packet, unsigned int len);
int pktgen_SendQueued();

#ifndef WIN32
//
// Used to send a single raw packet
// On WIN32, pcap_sendpacket should be used instead
//
int pktgen_SendPacket(const char *dev, unsigned char *packet, int len);

#endif // WIN32

#endif //__PACKET_GEN_H__
