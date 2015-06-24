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


#include "vsx_common.h"
#include "pktcapture.h"
#include "pktgen.h"

#if defined(VSX_HAVE_CAPTURE)


#define TCP_HDR_LEN       20

int capture_decodeTcp(const struct ether_header *peth,
                              const struct ip *pip,
                              const struct tcphdr *ptcp,
                              const struct pcap_pkthdr *pkthdr,
                              COLLECT_STREAM_PKT_T *pkt) {

  const struct ip6_hdr *pip6 = (const struct ip6_hdr *) pip;
  unsigned short tcpHdrLen = 0;
  int payloadCapLen;
  
  if((payloadCapLen = pkthdr->caplen - 
   (  (((const unsigned char *)ptcp) - (const unsigned char *)peth))) < TCP_HDR_LEN) {
fprintf(stderr, "bad tcp caplen:%d (pkt len:%d)\n", payloadCapLen, pkthdr->len);
    return -1;
  }

  pkt->hdr.key.srcPort = ptcp->source;
  pkt->hdr.key.dstPort = ptcp->dest;

//fprintf(stderr, "%d %d\n", ptcp->doffres, (ptcp->doffres >> 4));

  if((tcpHdrLen = (ptcp->doffres >> 4) * 4) < TCP_HDR_LEN ||
     (tcpHdrLen > TCP_HDR_LEN && tcpHdrLen - TCP_HDR_LEN > payloadCapLen)) {
fprintf(stderr, "bad tcp hdr len tcphdrlen:%d, caplen:%d\n", tcpHdrLen, payloadCapLen);
    return -1;
  }
  

  pkt->data.u.tcp.seq =  (uint32_t) htonl(ptcp->seq);
  pkt->data.u.tcp.flags = ptcp->flags & 0x3f;

  if(pip->ip_v == 4) {

    //
    //  Handle IPv4
    //

    pkt->hdr.key.lenIp = ADDR_LEN_IPV4;
    pkt->hdr.key.lenKey = COLLECT_STREAM_KEY_SZ_NO_IP + (pkt->hdr.key.lenIp * 2); 
    pkt->hdr.key.pair_srcipv4.s_addr = pip->ip_src.s_addr;
    pkt->hdr.key.pair_dstipv4.s_addr = pip->ip_dst.s_addr;
    pkt->hdr.ipTos = pip->ip_tos;


  } else {

    //
    // Handle IPv6
    //

    pkt->hdr.key.lenIp = ADDR_LEN_IPV6;
    pkt->hdr.key.lenKey = COLLECT_STREAM_KEY_SZ_NO_IP + (pkt->hdr.key.lenIp * 2); 
    memcpy(&pkt->hdr.key.pair_srcipv6, pip6->ip6_src.s6_addr, ADDR_LEN_IPV6);
    memcpy(&pkt->hdr.key.pair_dstipv6, pip6->ip6_dst.s6_addr, ADDR_LEN_IPV6);
    pkt->hdr.ipTos = 0;
//fprintf(stderr, "ipv6-%s-\n", inet6_ntoa(pkt->hdr.key.pair_dstipv6));
//fprintf(stderr, "%d %d %d\n", sizeof(struct in_addr), sizeof(struct in6_addr), ADDR_LEN_IPV6);
  }

  pkt->data.u.tcp.sz = pkthdr->len;
  PKTCAPLEN(pkt->data.payload) = (unsigned short) (payloadCapLen - tcpHdrLen);
  PKTWIRELEN(pkt->data.payload) = PKTCAPLEN(pkt->data.payload) + (pkthdr->len - pkthdr->caplen);
  pkt->data.payload.pData = ((unsigned char *)ptcp) + tcpHdrLen;
  pkt->data.tv.tv_sec = pkthdr->ts.tv_sec;
  pkt->data.tv.tv_usec = pkthdr->ts.tv_usec;
  pkt->data.flags = 0;

  return 0;
}

#endif // VSX_HAVE_CAPTURE
