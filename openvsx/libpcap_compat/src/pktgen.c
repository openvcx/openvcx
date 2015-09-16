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


#include "pcap_compat.h"

#if defined(WIN32) 
//defined(__MINGW32__)

#if defined(_MSC_VER) && (_MSC_VER < 1500)
#include "Win32-Extensions.h"
#endif // _MSC_VER

#include <windows.h>

#ifndef DISABLE_PCAP

#include "ntddndis.h"
#undef _WS2IPDEF_ 
#include <Iphlpapi.h>

#endif // DISABLE_PCAP

#include "unixcompat.h"
#include "pthread_compat.h"

//mingw does not define in_addr_t
typedef uint32_t in_addr_t;


#define PCAP_SENDQUEUE_ELEMENTS         64
#define PCAP_SENDQUEUE_SIZE             65535

#ifndef DISABLE_PCAP

static pcap_send_queue *g_pSendQueue = NULL;
static struct pcap_pkthdr g_pktHdrs[PCAP_SENDQUEUE_ELEMENTS];
static unsigned int g_queuedPkts;

static pcap_t *g_pcapgen_fp = NULL;

#endif // DISABLE_PCAP

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

#else

#if !defined(IPHONE)
#include <net/route.h>
#endif // IPHONE

#include <net/if.h>

#endif // __linux__
#include <errno.h>

#ifdef __linux__
static struct sockaddr_ll g_sall;
#endif // __linux__
static int g_fd = 0;

#endif // WIN32


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pktgen.h"
#include "pktcommon.h"
#include "logutil.h"

static char g_device_gen[256];
static unsigned char g_device_gen_mac[ETH_ALEN];
static in_addr_t g_device_gen_ipv4;

#define CKSUM_CARRY(x) (x = (x >> 16) + (x & 0xffff), (~(x + (x >> 16)) & 0xffff))

static int cksum_work(const unsigned short *addr, int len) {
  int sum = 0;
  int nleft = len;
  unsigned short ans = 0;
  const unsigned short *w = addr;

  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  if (nleft == 1) {
    *(unsigned char *)(&ans) = *(unsigned char *)w;
    sum += ans;
  }

  return sum;
}

static int cksum(u_char *buf, int protocol, int len) {
  struct ip *iph_p = (struct ip *) buf;
  struct udphdr *udph_p;
  int ip_hl = iph_p->ip_hl << 2;
  int sum = 0;

  switch(protocol) {
    case IPPROTO_UDP:
      udph_p = (struct udphdr *) (buf + ip_hl);
      udph_p->check = 0;
      sum = cksum_work((u_short *)&iph_p->ip_src, 8);
      sum += ntohs(IPPROTO_UDP + (unsigned short) len);
      sum += cksum_work((u_short *)udph_p, len);
      udph_p->check = (unsigned short) CKSUM_CARRY(sum);
      break;
    case IPPROTO_IP:
      iph_p->ip_sum = 0;
      sum = cksum_work((u_short *)iph_p, len);
      iph_p->ip_sum = (unsigned short) CKSUM_CARRY(sum);
      break;
    default:
      return -1;
   }

  return 0;
}

int pktgen_SetInterface(const char *iface) {
  if(!iface || strlen(iface) > sizeof(g_device_gen) -1) {
    return -1;
  }

  strcpy(g_device_gen, iface);

  return 0;
}

const char *pktgen_GetInterface() {
  return g_device_gen;
}

const unsigned char *pktgen_GetLocalMac() {
  return g_device_gen_mac;
}

unsigned int pktgen_GetLocalIp() {
  return g_device_gen_ipv4;
}

#ifdef DISABLE_PCAP
int pktgen_Init() {
  return -1;
}
#else // DISABLE_PCAP
int pktgen_Init() {

#ifdef WIN32
  char errbuf[PCAP_ERRBUF_SIZE];
#endif // WIN32

  if(pktgen_GetLocalAdapterInfo(g_device_gen, 
                                g_device_gen_mac, 
                                &g_device_gen_ipv4) != 0) {
#ifdef WIN32
    LOG(X_ERROR("Unable to initialize the adapter '%s'"), g_device_gen);
#else // WIN32
    if(getuid() != 0) {
      LOG(X_ERROR("You must have root access!"));
    } else {
      LOG(X_ERROR("Unable to open interface '%s'."), g_device_gen);
    }
#endif // WIN32
    return -1;
  }

  LOG(X_DEBUG("%s, ip:%s."),
      g_device_gen, inet_ntoa(*((struct in_addr *)&g_device_gen_ipv4)));

  LOG(X_DEBUG("%s MAC: (%02x:%02x:%02x:%02x:%02x:%02x)"), g_device_gen,
       g_device_gen_mac[0], g_device_gen_mac[1], g_device_gen_mac[2],
       g_device_gen_mac[3], g_device_gen_mac[4], g_device_gen_mac[5]);

#ifdef WIN32

  if ((g_pcapgen_fp = pcap_open_live(g_device_gen,
              65536, // portion of the packet to capture. It doesn't matter in this case 
              0,     // promiscuous mode (nonzero means promiscuous)
              1000, 
              errbuf)) == NULL) {
	  LOG(X_CRITICAL("Unable to open the adapter. '%s' is not supported by WinPcap\n"),
                       g_device_gen);
     return -1;
  }



  if((g_pSendQueue = pcap_sendqueue_alloc(PCAP_SENDQUEUE_SIZE)) == NULL) {
    LOG(X_ERROR("pcap_sendqueue_alloc %d failed"), PCAP_SENDQUEUE_SIZE);
		pcap_close(g_pcapgen_fp);	
		g_pcapgen_fp = NULL;
    return -1;
  }

  memset(g_pktHdrs, 0, sizeof(g_pktHdrs));

  g_queuedPkts = 0;


  LOG(X_INFO("Using interface '%s' %s %s %02x:%02x:%02x:%02x:%02x:%02x"), 
              g_device_gen, pkt_FindDevDescr(g_device_gen),
              inet_ntoa(*((struct in_addr *)&g_device_gen_ipv4)), g_device_gen_mac[0],g_device_gen_mac[1],
              g_device_gen_mac[2],g_device_gen_mac[3],g_device_gen_mac[4],g_device_gen_mac[5]);      

  LOG(X_INFO("%s"), pcap_lib_version());

#else // WIN32

#ifdef __linux__
  struct ifreq ifr;

  if((g_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
    LOG(X_ERROR("socket PF_PACKET SOCK_RAW failed"), strerror(errno));
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, g_device_gen, sizeof(ifr.ifr_name) - 1);
  if(ioctl(g_fd, SIOCGIFINDEX, &ifr) == -1) {
    LOG(X_ERROR("ioctl SIOCGIFINDEX failed on %s: %s"), g_device_gen, strerror(errno));
    close(g_fd);
    g_fd = 0;
  }

  memset(&g_sall, 0, sizeof(g_sall));
  g_sall.sll_family = AF_PACKET;
  g_sall.sll_ifindex = ifr.ifr_ifindex;
  g_sall.sll_protocol = htons(ETH_P_ALL);

#else // __linux__
  return -1;
#endif // __linux__

#endif // WIN32

  return 0;
}
#endif // DISABLE_PCAP

int pktgen_Close() {

#ifdef WIN32
#ifdef DISABLE_PCAP
  return -1;
#else // DISABLE_PCAP
  if(g_pcapgen_fp) {
    pcap_sendqueue_destroy(g_pSendQueue);
    g_pSendQueue = NULL;
    pcap_close(g_pcapgen_fp);	
    g_pcapgen_fp = NULL;
    return 0;
  }
  return -1;

#endif // DISABLE_PCAP
#else // WIN32

  if(g_fd) {
    close(g_fd);
    g_fd = 0;
  }
  return -1;
#endif // WIN32
}

int pktgen_Send(const unsigned char *packet, unsigned int len) {

#ifdef WIN32
#ifdef DISABLE_PCAP
  return -1;
#else // DISABLE_PCAP

  if(g_pcapgen_fp == NULL) {
    return -1;
  }
  /* Send down the packet */
  if (pcap_sendpacket(g_pcapgen_fp,	packet, len) != 0) {
    LOG(X_ERROR("Error sending packet: %s"), pcap_geterr(g_pcapgen_fp));
    return -1;
  }

#endif // DISABLE_PCAP
#else // WIN32

#ifdef __linux__
  int rc;

  if(g_fd == 0) {
    return -1;
  }

  if((rc = sendto(g_fd, packet, len, 0, (struct sockaddr *) &g_sall, sizeof(g_sall))) != len) {
    LOG(X_ERROR("Error sending packet: %d / %d %s"), rc, len, strerror(errno));
    return -1;
  }
#else // __linux__
  return -1;
#endif // __linux__

#endif // WIN32

  return 0;
}

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
                             unsigned short lenPayload) {


  unsigned short *pus = NULL;                            
  struct ip *pip4 = NULL;
  unsigned short totLen = 14 + 20 + 8 + lenPayload;
  
  if(!pkt) {
    return -1;
  }

  memset(pkt->data, 0, sizeof(pkt->data)); 

  // build the ethernet header
  pkt->peth = (struct ether_header *) pkt->data;
  memcpy(pkt->peth->ether_shost, srcMac, ETH_ALEN);
  memcpy(pkt->peth->ether_dhost, dstMac, ETH_ALEN);

  if(haveVlan) {

    pkt->peth->ether_type = htons(ETHERTYPE_VLAN);

    pus = (unsigned short *) ((unsigned char *)pkt->peth + 14);
    *pus = htons(vlan);
    //memcpy((unsigned char *)pkt->peth + 14, &vlan, 2);

    pus = (unsigned short *) ((unsigned char *)pkt->peth + 16);
    *pus = htons(ETHERTYPE_IP);

    pip4 = pkt->u_ip.pip4 = (struct ip *) ((unsigned char *)pkt->peth + 18);
    totLen += 4;
  } else {
    pkt->peth->ether_type = htons(ETHERTYPE_IP);
    pip4 = pkt->u_ip.pip4 = (struct ip *) ((unsigned char *)pkt->peth + 14);
  }

  // build the ip header
  pip4->ip_v = 4;
  pip4->ip_hl = 5;
  pip4->ip_tos = tos;
  pip4->ip_len = htons(totLen - (unsigned short) ((unsigned char *) pip4 - (unsigned char *) pkt->peth));
  pip4->ip_id = htons((unsigned short) random() % 0xffff);
  pip4->ip_off = 0;
  pip4->ip_ttl = PKTGEN_DEFAULT_TTL;
  pip4->ip_p = IPPROTO_UDP;
  pip4->ip_sum = 0;
  pip4->ip_src.s_addr = srcIp;
  pip4->ip_dst.s_addr = dstIp;

  // build the udp header
  pkt->pudp = (struct udphdr *) ((unsigned char *) pip4 + ( pip4->ip_hl * 4));
  pkt->pudp->source = htons(srcPort);
  pkt->pudp->dest = htons(dstPort);
  pkt->pudp->len =  htons(totLen - (unsigned short) ((unsigned char *) pkt->pudp - (unsigned char *) pkt->peth));
  pkt->pudp->check = 0;

  return 0;
}

int pktgen_ChecksumUdpPacketIpv4(PACKETGEN_PKT_UDP_T *pkt) {

  if(cksum((unsigned char *) pkt->u_ip.pip4, IPPROTO_IP, (pkt->u_ip.pip4->ip_hl * 4)) != 0) {
    return -1;
  }

  if(cksum((unsigned char *) pkt->u_ip.pip4, 
       IPPROTO_UDP, ntohs(pkt->u_ip.pip4->ip_len) - (pkt->u_ip.pip4->ip_hl * 4)) != 0) {
    return -1;
  }
  
  return 0;
}


#ifdef WIN32

int pktgen_Queue(const unsigned char *packet, unsigned int len) {
#ifdef DISABLE_PCAP
  return -1;
#else // DISABLE_PCAP

  if(g_pSendQueue == NULL || g_queuedPkts >= PCAP_SENDQUEUE_ELEMENTS) {
    return -1;
  }

  if(g_pSendQueue->len + sizeof(struct pcap_pkthdr) + len > g_pSendQueue->maxlen) {
    return -1;
  }
     
  g_pktHdrs[g_queuedPkts].len = len;
  g_pktHdrs[g_queuedPkts].caplen = len;

  if(pcap_sendqueue_queue(g_pSendQueue, &g_pktHdrs[g_queuedPkts], packet) == -1) {
    LOG(X_ERROR("Error queing packet idx %d (len: %d).  Queue len: %d / %d"), 
          g_queuedPkts, len, g_pSendQueue->len, PCAP_SENDQUEUE_SIZE);
    return -1;
  }
  g_queuedPkts++;

  return 0;
#endif // DISABLE_PCAP
}

int pktgen_SendQueued() {
#ifdef DISABLE_PCAP
  return -1;
#else // DISABLE_PCAP
  unsigned int rc;

  if(g_pcapgen_fp == NULL || g_pSendQueue == NULL || g_queuedPkts == 0) {
    return 0;
  }

  if((rc = pcap_sendqueue_transmit(g_pcapgen_fp, g_pSendQueue, 0)) != g_pSendQueue->len) {
    LOG(X_ERROR("pcap_sendqueue_transmit returned %d / %d , %d packets %s"), 
      rc, g_pSendQueue->len, g_queuedPkts, pcap_geterr(g_pcapgen_fp));
  }

  g_pSendQueue->len = 0;
  rc = g_queuedPkts;
  g_queuedPkts = 0;

  return rc;
#endif // DISABLE_PCAP
}

#else // WIN32

int pktgen_Queue(const unsigned char *packet, unsigned int len) {
  return pktgen_Send(packet, len);
}

int pktgen_SendQueued() {
  return 0;
}

int pktgen_SendPacket(const char *dev, unsigned char *packet, int len) {
#ifdef __linux__
  struct sockaddr_ll sall;
  struct ifreq ifr;
  int fd;
  int rc = 0;

  if((fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);
  if(ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
    close(fd);
    return -1;
  }

  memset(&sall, 0, sizeof(sall));
  sall.sll_family = AF_PACKET;
  sall.sll_ifindex = ifr.ifr_ifindex;
  sall.sll_protocol = htons(ETH_P_ALL);

  if((rc = sendto(fd, packet, len, 0, (struct sockaddr *) &sall, sizeof(sall))) != len) {
    close(fd);
    return -1;
  }

  close(fd);

  return 0;
#else //__linux__
  return -1;
#endif // __linux__
}

#endif // WIN32


#if 0
int main(int argc, char *argv[]) {

  return 0;
}
#endif // 0

