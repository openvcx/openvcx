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
#include "util/netio.h"

#if defined(VSX_HAVE_CAPTURE)

static int pass_filter_ip6(const CAPTURE_FILTER_T *pFilter,
                              const struct ip6_hdr *pip6) {

  //TODO: ipv6 filter not implemented

  if(pFilter->ip != 0 || pFilter->dstIp != 0 || pFilter->srcIp != 0) {
    return 0;
  }

  return 1;
}

static int pass_filter_ip(const CAPTURE_FILTER_T *pFilter,
                              const struct ip *pip,
                              const struct ip6_hdr *pip6) {

  if(pip6) {
    return pass_filter_ip6(pFilter, pip6);
  }

  if(pip->ip_v != 4) {
    return 0;
  }

  if(pFilter->ip != 0) {

    if(pFilter->ip != pip->ip_dst.s_addr && pFilter->ip != pip->ip_src.s_addr) {
      return 0;
    }

  } else {

    if(pFilter->dstIp != 0 && pFilter->dstIp != pip->ip_dst.s_addr) {
      return 0;
    }

    if(pFilter->srcIp != 0 && pFilter->srcIp != pip->ip_src.s_addr) {
      return 0;
    }
 
  }

  return 1;
}


static int pass_filter_udpraw(const CAPTURE_FILTER_T *pFilter,
                              const struct ip *pip,
                              const struct ip6_hdr *pip6, 
                              const struct udphdr *pudp) {

  if(!pass_filter_ip(pFilter, pip, pip6)) {
     return 0; 
  } 

  if(pFilter->port != 0) {

    if(pFilter->port != pudp->dest && pFilter->port != pudp->source) {
      return 0; 
    }

  } else {

    if(pFilter->dstPort != 0 && pFilter->dstPort != pudp->dest) {
      return 0; 
    }
    if(pFilter->srcPort != 0 && pFilter->srcPort != pudp->source) {
      return 0;
    }
  }

  return 1;
}

static int pass_filter_tcpraw(const CAPTURE_FILTER_T *pFilter,
                              const struct ip *pip,
                              const struct ip6_hdr *pip6,
                              const struct tcphdr *ptcp) {

  if(!pass_filter_ip(pFilter, pip, pip6)) {
     return 0; 
  } 

  if(pFilter->port != 0) {

    if(pFilter->port != ptcp->dest && pFilter->port != ptcp->source) {
      return 0;
    }

  } else {

    if(pFilter->dstPort != 0 && pFilter->dstPort != ptcp->dest) {
      return 0;
    }
    if(pFilter->srcPort != 0 && pFilter->srcPort != ptcp->source) {
      return 0;
    }
  }

  return 1; 
}


static int pass_filter_rtp(const CAPTURE_FILTER_T *pFilter,
                           const COLLECT_STREAM_PKT_T *pkt) {

  if(pFilter->haveRtpSsrcFilter && 
     pFilter->ssrcFilter != pkt->hdr.key.ssrc) {
    return 0;
  }
  if(pFilter->haveRtpPtFilter &&
     pFilter->payloadType != pkt->hdr.payloadType) {
    return 0;
  } 

  return 1;
}

static const CAPTURE_FILTER_T *get_filter_tcpraw(const CAPTURE_STATE_T *pState,
                                                 const struct ip *pip,
                                                 const struct ip6_hdr *pip6,
                                                 const struct tcphdr *ptcp) {
  unsigned int idx;

  for(idx = 0; idx < pState->filt.numFilters; idx++) {

    if(!pass_filter_tcpraw(&pState->filt.filters[idx], pip, pip6, ptcp)) {
      continue;
    }
    return &pState->filt.filters[idx];
  }
  return NULL;
}


static const CAPTURE_FILTER_T *get_filter_udpraw(const CAPTURE_STATE_T *pState, 
                                                 const struct ip *pip,
                                                 const struct ip6_hdr *pip6,
                                                 const struct udphdr *pudp) {
  unsigned int idx;

  for(idx = 0; idx < pState->filt.numFilters; idx++) {

    if(!pass_filter_udpraw(&pState->filt.filters[idx], pip, pip6, pudp)) {
      continue;
    }
    return &pState->filt.filters[idx];
  }
  return NULL;
}

static const CAPTURE_FILTER_T *get_filter_udprtp(const CAPTURE_STATE_T *pState, 
                                                 const COLLECT_STREAM_PKT_T *pkt) {
  unsigned int idx;
  struct ip pktIp;
  struct udphdr pktUdp;

  pktIp.ip_v = (unsigned char) pkt->hdr.key.lenIp;
  pktIp.ip_src = pkt->hdr.key.ipAddr.ip_un.ipv4.srcIp;
  pktIp.ip_dst = pkt->hdr.key.ipAddr.ip_un.ipv4.dstIp;

  pktUdp.source = pkt->hdr.key.srcPort;
  pktUdp.dest = pkt->hdr.key.dstPort;

  for(idx = 0; idx < pState->filt.numFilters; idx++) {

    if(!pass_filter_rtp(&pState->filt.filters[idx], pkt)) {
      continue;
    }

    if(!pass_filter_udpraw(&pState->filt.filters[idx], &pktIp, NULL, &pktUdp)) {
      continue;
    }

    return &pState->filt.filters[idx];

  }
  return NULL;
}

static int have_filter_rtp(CAPTURE_STATE_T *pState) {
  unsigned int idx;

  for(idx = 0; idx < pState->filt.numFilters; idx++) {
    if(pState->filt.filters[idx].haveRtpPtFilter ||
      pState->filt.filters[idx].haveRtpSsrcFilter != 0 ||
      IS_CAPTURE_FILTER_TRANSPORT_RTP(pState->filt.filters[idx].transType)) {
      return 1;
    }
  }
  return 0;
}


static const CAPTURE_STREAM_T *capture_onRawPkt_int(unsigned char *pArg, 
                                                    const struct pcap_pkthdr *pkthdr, 
                                                    const unsigned char *packet, 
                                                    const NETIO_SOCK_T *pnetsock) {

  uint16_t *pus;
  uint16_t ethType = 0;
  struct ip *pip;
  struct udphdr *pudp;
  struct tcphdr *ptcp;
  struct ip6_hdr *pip6 = NULL;
  unsigned int iphl = 0u;
  int ethlen = 14;
  int haveRtpFilter;
  int isRtp = 0;
  COLLECT_STREAM_PKT_T pkt;
  const CAPTURE_STREAM_T *pStream = NULL;
  const CAPTURE_FILTER_T *pFilter = NULL;
  CAPTURE_STATE_T *pState = (CAPTURE_STATE_T *) pArg;

  char delme[2][64];

  if(pState) {
    // maybe move this to inside of mutex lock
    pState->numPkts++;
  }

  if(pkthdr == NULL || packet == NULL) {
    return NULL;
  }

  if(pkthdr->caplen < 16 ||   
    (packet[0] & 0x01)) {     // ethernet non-multicast  
    return NULL;
  }

  if(packet[0] == 0x02) {

    // loopback type - 4 byte header
    pus = (uint16_t *) &packet[2];  
    ethType = 0x0800;
    ethlen = 4;

  } else if(packet[0] == 0x1e) {

    // null / loopback ipv6 - not sure about this
    pus = (unsigned short *) &packet[2];
    ethType = 0x86DD;

  } else {

    pus = (uint16_t *) &packet[12];
    if((ethType = ntohs(*pus)) == 0x8100) { // VLAN 

      pus = (unsigned short *) &packet[16];
      ethType = ntohs(*pus);

    } else if(ethType == 0x0000) {
      //
      // linux cooked capture - not sure how to properly id
      // DLT_LINUX_SLL - 16 byte header 
      //
      pus = (uint16_t *) &packet[14];
      ethType = ntohs(*pus);
      ethlen = 16;
    }
    if(ethType != 0x0800 && ethType != 0x86DD)  {
      // type IPv4 or IPv6 
      return NULL;
    }

  }

  if(pkthdr->caplen < ethlen + 40) {  // (14eth) + 20ip + 20 min([udp + rtp |tcp]) 
    return NULL;
  }

  pip = (struct ip *) ((unsigned char *)pus + 2);
  if(pip->ip_v == 4) {
    iphl = pip->ip_hl * 4;
  } else if(ethType == 0x86DD || pip->ip_v == 6) {
    //(ntohl(pip6->ip6_ctlun.ip6_un1.ip6_un1_flow) >> 28) == 0x06) {
    iphl = IPV6_HDR_SIZE;
    pip6 = (struct ip6_hdr *) pip;
  }

  if(iphl == 0 || 
     ((int) (((unsigned char *)pip) - ((unsigned char *)packet)) + iphl + UDP_HEADER_LEN) >= pkthdr->caplen ||
     !(pip->ip_v == 6 || (pip->ip_v == 4 && !(ntohs(pip->ip_off) & IP_OFFMASK)))  ) {   
     // discard fragments
    return NULL;
  }

  pkt.pnetsock = pnetsock;

  if((pip6 == NULL && pip->ip_p == 0x06) ||
     (pip6 && pip6->ip6_ctlun.ip6_un1.ip6_un1_nxt == 0x06)) {     
     // tcp 

    ptcp = (struct tcphdr *) ((unsigned char *) pip + iphl);
    ptcp->source = ntohs(ptcp->source);
    ptcp->dest = ntohs(ptcp->dest);
    //pip6 = pip6;

    if(pip6) {
      sprintf(delme[0], "%s:%d", inet6_ntoa(pip6->ip6_src),ptcp->source);
      sprintf(delme[1], "%s:%d", inet6_ntoa(pip6->ip6_dst),ptcp->dest);
    } else {
      sprintf(delme[0], "%s:%d", inet_ntoa(pip->ip_src),ptcp->source);
      sprintf(delme[1], "%s:%d", inet_ntoa(pip->ip_dst),ptcp->dest);
    }


    if(pState->filt.numFilters > 0 &&
       (pFilter = get_filter_tcpraw(pState, pip, pip6, ptcp)) == NULL) {
//fprintf(stderr, "discarded tcp pkt %s->%s\n", delme[0], delme[1]);
      return NULL;
    }

    if(capture_decodeTcp((struct ether_header *) packet,
                         pip, 
                         ptcp, 
                         pkthdr,
                         &pkt) == 0) {

//fprintf(stderr, "tcp %s -> %s\n", delme[0], delme[1]);
//fprintf(stderr, "tcp %s:%d sz:%d caplen:%d len:%d \n", inet_ntoa(pkt.hdr.key.pqos_dstipv4), pkt.hdr.key.dstPort, pkt.data.u.tcp.sz, pkt.data.payload.caplen,pkt.data.payload.len);

      if(PKTWIRELEN(pkt.data.payload) > 0) {
        capture_processTcpRaw(pState, &pkt, pFilter);
      }


    } else {
      //fprintf(stderr, "bad decode tcp %s -> %s\n", delme[0], delme[1]);
    }



  } else if((pip6 == NULL && pip->ip_p == IPPROTO_UDP) ||
     (pip6 && pip6->ip6_ctlun.ip6_un1.ip6_un1_nxt == IPPROTO_UDP)) {     
     // udp 


    pudp = (struct udphdr *) ((unsigned char *)pip + iphl);
    pudp->source = ntohs(pudp->source);
    pudp->dest = ntohs(pudp->dest);

//sprintf(delme[0], "%s:%d", inet_ntoa(pip->ip_src),pudp->source);
//sprintf(delme[1], "%s:%d", inet_ntoa(pip->ip_dst),pudp->dest);
//fprintf(stdout, "%s->%s\n", delme[0], delme[1]);

    haveRtpFilter = have_filter_rtp(pState);

    //if(pState->filt.numFilters > 0 && !haveRtpFilter && 
    //  (pFilter = capture_checkUdpRawFilters(pState, pip, pudp)) == NULL) {
    //  return NULL;
    //}

    if(pState->filt.numFilters > 0 && 
       (pFilter = get_filter_udpraw(pState, pip, pip6, pudp)) == NULL) {
//fprintf(stdout, "discarded pkt %s->%s\n", delme[0], delme[1]);
      return NULL;
    } 
                      
    if(pState->filt.numFilters == 0 || haveRtpFilter) {


      if((isRtp = !capture_decodeRtp((struct ether_header *) packet,
                         pip, 
                         pudp, 
                         (rtp_t *) (((unsigned char *)pudp) + UDP_HEADER_LEN),
                         //pkthdr->caplen - (  (((unsigned char *)pudp) - packet) + UDP_HEADER_LEN),
                         pkthdr,
                         &pkt))) {

        //if(pState->filt.numFilters > 0) {
        pFilter = get_filter_udprtp(pState, &pkt);      

        if(pState->filt.numFilters == 0 || pFilter != NULL) {
//fprintf(stdout, "filtered rtp pkt %s->%s pt:%u,ssrc=0x%x\n", delme[0], delme[1], pkt.hdr.payloadType,pkt.hdr.key.ssrc);
          pStream = capture_processRtp(pState, &pkt, pFilter);
        }else {
//fprintf(stdout, "discarded rtp pkt %s->%s pt:%u,ssrc=0x%x\n", delme[0], delme[1], pkt.hdr.payloadType,pkt.hdr.key.ssrc);
        }

      } else {
//fprintf(stdout, "discarded non-rtp pkt %s->%s pt:%u,ssrc=0x%x\n", delme[0], delme[1], pkt.hdr.payloadType,pkt.hdr.key.ssrc);

      }

    } else if(pFilter) {
//fprintf(stdout, "filtered udp pkt %s->%s\n", delme[0], delme[1]);
      // TODO: implement non RTP onStreamAdd and packet cb invocation
      // should we do basic mpeg2-ts continuity reording?
      if(capture_decodeUdpRaw((struct ether_header *) packet,
                         pip, 
                         pudp, 
                         pkthdr,
                         &pkt) == 0) {

          capture_processUdpRaw(pState, &pkt, pFilter);
//fprintf(stdout, "decode ok on udp pkt %s->%s\n", delme[0], delme[1]);
      } else {

//fprintf(stdout, "decode failed on udp pkt %s->%s\n", delme[0], delme[1]);
      }

    } else {
//fprintf(stdout, "non-filtered udp pkt %s->%s\n", delme[0], delme[1]);

    }

  } // end of udp check

  return pStream;
}


void capture_onRawPkt(unsigned char *pArg, 
                      const struct pcap_pkthdr *pkthdr, 
                      const unsigned char *packet) {

  (void) capture_onRawPkt_int(pArg, pkthdr, packet, NULL);
}

const CAPTURE_STREAM_T *capture_onUdpSockPkt(CAPTURE_STATE_T *pState, 
                                             unsigned char *pData, 
                                             unsigned int len, 
                                             unsigned int lenPrepend,
                                             struct sockaddr_in *pSaSrc, 
                                             struct sockaddr_in *pSaDst,
                                             struct timeval *ptv,
                                             const NETIO_SOCK_T *pnetsock) {

  struct udphdr *pudp = (struct udphdr *) (pData - 8);
  struct ip *pip = (struct ip *) (((unsigned char *) pudp) - 20);
  struct ether_header *peth = (struct ether_header *) (((unsigned char *) pip) - 14);
  struct pcap_pkthdr pkthdr;

  // pre-pend assumed eth, ip, udp headers
  peth->ether_dhost[0] = 0x00;
  peth->ether_type = htons(0x0800);

  pip->ip_v = 4;
  pip->ip_hl = 5;
  pip->ip_off = 0;
  pip->ip_p = IPPROTO_UDP; // udp
  pip->ip_len = 28 + len;
  pip->ip_off = 0;
  pip->ip_src = pSaSrc->sin_addr;
  pip->ip_dst = pSaDst->sin_addr;

  pudp->source = pSaSrc->sin_port;
  pudp->dest = pSaDst->sin_port;
  pudp->len = 8 + len;

  pkthdr.caplen = 42 + len;
  pkthdr.len = pkthdr.caplen;
  pkthdr.ts.tv_sec = ptv->tv_sec;
  pkthdr.ts.tv_usec = ptv->tv_usec;
  
  return capture_onRawPkt_int((unsigned char *) pState, &pkthdr, (unsigned char *) peth, pnetsock); 
}

#endif // VSX_HAVE_CAPTURE
