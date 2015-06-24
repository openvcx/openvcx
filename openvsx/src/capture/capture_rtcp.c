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


#if defined(VSX_HAVE_CAPTURE)


CAPTURE_STREAM_T *capture_rtcp_lookup(const CAPTURE_STATE_T *pState, const COLLECT_STREAM_KEY_T *pKey) {

  unsigned int idx;
  //uint16_t dstPort = pKey->dstPort;

  //if(dstPort > 0 && dstPort % 2 == 1) {
  //  dstPort--;
  //}

  //TODO: since we're not capable of using arbitrary rtcp ports.. need to do a proper match than above
 // TODO: this should be a hash

  for(idx = 0; idx < pState->maxStreams; idx++) {
    //fprintf(stderr, "lookup rtcp key ssrc: 0x%x 0x%x dst:%d args ssrc: 0x%x 0x%x dst:%d\n", pState->pStreams[idx].hdr.key.ssrc, pState->pStreams[idx].hdr.key.pair_dstipv4.s_addr,  pState->pStreams[idx].hdr.key.dstPort, pKey->ssrc, pKey->pair_dstipv4.s_addr, dstPort);

     if(pState->pStreams[idx].hdr.key.ssrc == pKey->ssrc &&
        //pState->pStreams[idx].hdr.key.dstPort == dstPort &&
        !memcmp(&pState->pStreams[idx].hdr.key.ipAddr, &pKey->ipAddr, 
                pState->pStreams[idx].hdr.key.lenIp)) {
      return &pState->pStreams[idx];
    }
  }

  return NULL;
}

static int capture_rtcp_create_fci_fir(STREAM_RTCP_RR_T *pRtcp,
                                       unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int lentot = 0;

/*
  RTCP 5104
  4.3.1.1.  Message Format

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Seq nr.       |    Reserved                                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

  pRtcp->fb_fir.seqno++;

  lentot = 4 + (ntohs(pRtcp->fb_fir.hdr.len) * 4);
  if(lentot > lenbuf) {
    return -1;
  }
  memcpy(pBuf, &pRtcp->fb_fir, lentot);

  return lentot;
}

static int capture_rtcp_create_fci_pli(STREAM_RTCP_RR_T *pRtcp,
                                       unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int lentot = 0;

  lentot = 4 + (ntohs(pRtcp->fb_pli.hdr.len) * 4);
  if(lentot > lenbuf) {
    return -1;
  }
  memcpy(pBuf, &pRtcp->fb_pli, lentot);

  return lentot;
}

static int capture_rtcp_create_fci_nack(STREAM_RTCP_RR_T *pRtcp,
                                       unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int lenpkt= 0;
  unsigned int idxbuf = 0;
  unsigned int idxNackPkt;

/*
  RTCP 4585
  6.2.1.  Generic NACK

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            PID                |             BLP               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/

  for(idxNackPkt = 0; idxNackPkt < pRtcp->countNackPkts; idxNackPkt++) {
    
    lenpkt = 4 + (ntohs(pRtcp->fbs_nack[idxNackPkt].hdr.len) * 4);
    if(idxbuf + lenpkt > lenbuf) {
      return -1;
    }
    memcpy(&pBuf[idxbuf], &pRtcp->fbs_nack[idxNackPkt], lenpkt);
    idxbuf += lenpkt;

  }

  pRtcp->countNackPkts = 0;

  return idxbuf;
}

static int capture_rtcp_create_fci_app_remb(STREAM_RTCP_RR_T *pRtcp,
                                            unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int i;
  const unsigned int lentot = 24;
  uint64_t bitrate = 0;
  uint8_t exponent = 0;
  uint32_t mantissa;
  RTCP_PKT_HDR_T *pHdr = (RTCP_PKT_HDR_T *) pBuf;

  if(lenbuf < 24) {
    return -1;
  }

  bitrate = pRtcp->rembBitrateBps;

  //
  // Build RTCP FCI NACK (Negative acknowledgement) packet static values
  //
  pHdr->hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pHdr->hdr |= 0 & 0x40; // padding 0
  pHdr->hdr |= RTCP_FCI_PSFB_APP & 0x3f;
  pHdr->pt = RTCP_PT_PSFB;
  pHdr->len = htons(5);   // in 4 byte words
  pHdr->ssrc = pRtcp->rr.hdr.ssrc;
  
  *((uint32_t *) &(pBuf[8])) = 0; // remote SSRC field always 0 
  *((uint32_t *) &(pBuf[12])) = *((uint32_t *) "REMB");
  pBuf[16] = 0x01;

  for(i = 0; i < 8; i++) {
    if(bitrate <= ((uint32_t) 263143 << i)) {
      exponent = i;
      break;
    }
  }

  mantissa = (bitrate >> exponent);
  pBuf[17] = (uint8_t) ((exponent << 2) + ((mantissa >> 16) & 0x03));
  pBuf[18] = (uint8_t) (mantissa >> 8);
  pBuf[19] = (uint8_t) mantissa;

  *((uint32_t *) &(pBuf[20])) = pRtcp->fb_fir.ssrc;

  VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_rtcp_create_fci_app_remb created REMB packet with bitrate: %llu bps for ssrc: 0x%x, %d bytes"), bitrate, htonl(pRtcp->rr.hdr.ssrc), lentot));
  VSX_DEBUG_REMB( LOGHEX_DEBUG(pBuf, lentot));

  return lentot;
}


static int capture_rtcp_create_sdes(STREAM_RTCP_RR_T *pRtcp,
                                    unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int lentmp;
  unsigned int lentot = 0;

  lentmp = 4 + (ntohs(pRtcp->sdes.hdr.len) * 4);
  if(lentot + lentmp > lenbuf) {
    return -1;
  }
  memcpy(&pBuf[lentot], &pRtcp->sdes, lentmp);
  lentot += lentmp;

  return lentot;
}

static int capture_rtcp_create_rr(STREAM_RTCP_RR_T *pRtcp,
                                  unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int lentot = 0;
  uint32_t tmp;
  TIME_VAL tmnow = timer_GetTime();

  /*
  RFC3550
  6.4.2 RR: Receiver Report RTCP Packet

          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  header |V=2|P|    RC   |   PT=RR=201   |             length            |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                     SSRC of packet sender                     |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  report |                 SSRC_1 (SSRC of first source)                 |
  block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    1    | fraction lost |       cumulative number of packets lost       |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |           extended highest sequence number received           |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                      interarrival jitter                      |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                         last SR (LSR)                         |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                   delay since last SR (DLSR)                  |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  report |                 SSRC_2 (SSRC of second source)                |
  block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    2    :                               ...                             :
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
         |                  profile-specific extensions                  |
  */

  pRtcp->pktdroptot += pRtcp->pktdropcnt;
  pRtcp->rr.fraclost = (uint8_t) (float) roundf((float) pRtcp->pktdropcnt / 
                        (float) (pRtcp->pktcnt + pRtcp->pktdropcnt) * 256.0);
  tmp = htonl(pRtcp->pktdroptot << 8);
  memcpy(pRtcp->rr.cumulativelost, &tmp, 3);
  pRtcp->rr.seqcycles = htons(pRtcp->seqRolls);
  pRtcp->rr.seqhighest = htons(pRtcp->rr.seqhighest);
  pRtcp->rr.jitter = htonl(round(pRtcp->rfc3550jitter));
  if(pRtcp->tmLastSrRcvd > 0) {
    pRtcp->rr.lsr = htonl((htonl(pRtcp->lastSrRcvd.ntp_msw) & 0xffff) << 16 |
                          (htonl(pRtcp->lastSrRcvd.ntp_lsw) >> 16)); 

    pRtcp->rr.dlsr = htonl( (float) (tmnow - pRtcp->tmLastSrRcvd) * 
                           (65536.0f / TIME_VAL_US_FLOAT) );
//fprintf(stderr, "DLSR: 0x%x %llu %llu \n", htonl(pRtcp->rr.dlsr), tmnow, pRtcp->tmLastSrRcvd); 
  }

  lentot = 4 + (ntohs(pRtcp->rr.hdr.len) * 4);
  if(lentot > lenbuf) {
    return -1;
  }

  memcpy(pBuf, &pRtcp->rr, lentot);

  pRtcp->pktcnt = 0;
  pRtcp->pktdropcnt = 0;

  return lentot;
}


int capture_rtcp_create(STREAM_RTCP_RR_T *pRtcp,
                        int do_rr, int do_fir, int do_pli, int do_nack, int do_remb,
                        unsigned char *pBuf, unsigned int lenbuf) {
  int rc;
  unsigned int lentot = 0;
  int do_sdes = 1;

  if(!do_rr && !do_fir && !do_pli && !do_nack && !do_remb) {
    return 0;
  }

  if(do_rr) {
    if((rc = capture_rtcp_create_rr(pRtcp, &pBuf[lentot], lenbuf - lentot)) > 0) {
      lentot +=  rc;
    }
  }

  if(do_fir) {
    if((rc = capture_rtcp_create_fci_fir(pRtcp, &pBuf[lentot], lenbuf - lentot)) > 0) {
      lentot +=  rc;
    }
  }

  if(do_pli) {
    if((rc = capture_rtcp_create_fci_pli(pRtcp, &pBuf[lentot], lenbuf - lentot)) > 0) {
      lentot +=  rc;
    }
  }

  if(do_nack) {
    if((rc = capture_rtcp_create_fci_nack(pRtcp, &pBuf[lentot], lenbuf - lentot)) > 0) {
      lentot +=  rc;
    }
  }

  if(do_remb) {
    if((rc = capture_rtcp_create_fci_app_remb(pRtcp, &pBuf[lentot], lenbuf - lentot)) > 0) {
      lentot +=  rc;
    }
  }

  if(do_sdes) {
    if((rc = capture_rtcp_create_sdes(pRtcp, &pBuf[lentot], lenbuf - lentot)) > 0) {
      lentot +=  rc;
    }
  }

  return lentot;
}

static void rtcprr_reset_fir(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc) {
  RTCP_PKT_PSFB_FIR_T *pFir;

  pFir = &pRtcp->fb_fir;

  //
  // Build RTCP FCI FIR (Full Intra Refresh) packet static values
  //
  pFir->hdr.hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pFir->hdr.hdr |= 0 & 0x40; // padding 0
  pFir->hdr.hdr |= RTCP_FCI_PSFB_FIR & 0x3f; 
  pFir->hdr.pt = RTCP_PT_PSFB;
  pFir->hdr.len = htons(4);   // in 4 byte words
  pFir->hdr.ssrc = pRtcp->rr.hdr.ssrc;

  pFir->ssrc_mediasrc = ssrc;
  pFir->ssrc = ssrc;

  pFir->seqno = 0;

}

static void rtcprr_reset_pli(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc) {
  RTCP_PKT_PSFB_PLI_T *pPli;

  pPli = &pRtcp->fb_pli;

  //
  // Build RTCP FCI PLI (Picture Loss Indication) packet static values
  //
  pPli->hdr.hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pPli->hdr.hdr |= 0 & 0x40; // padding 0
  pPli->hdr.hdr |= RTCP_FCI_PSFB_PLI & 0x3f; 
  pPli->hdr.pt = RTCP_PT_PSFB;
  pPli->hdr.len = htons(2);   // in 4 byte words
  pPli->hdr.ssrc = pRtcp->rr.hdr.ssrc;

  pPli->ssrc_mediasrc = ssrc;

}

static void rtcprr_reset_nack(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc) {
  RTCP_PKT_RTPFB_NACK_T *pNack;
  unsigned int idx;

  for(idx = 0; idx < RTCP_NACK_PKTS_MAX; idx++) {

    pNack = &pRtcp->fbs_nack[idx];

    //
    // Build RTCP FCI NACK (Negative acknowledgement) packet static values
    //
    pNack->hdr.hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
    pNack->hdr.hdr |= 0 & 0x40; // padding 0
    pNack->hdr.hdr |= RTCP_FCI_RTPFB_NACK & 0x3f;
    pNack->hdr.pt = RTCP_PT_RTPFB;
    pNack->hdr.len = htons(3);   // in 4 byte words
    pNack->hdr.ssrc = pRtcp->rr.hdr.ssrc;

    pNack->ssrc_mediasrc = ssrc;

    pNack->pid = 0;
    pNack->blp = 0;

  }

}

int capture_rtcp_set_local_ssrc(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc) {
  unsigned int idx;

  pRtcp->rr.hdr.ssrc = ssrc;
  pRtcp->sdes.hdr.ssrc = ssrc;
  pRtcp->fb_fir.hdr.ssrc = ssrc;
  pRtcp->fb_pli.hdr.ssrc = ssrc;

  for(idx = 0; idx < RTCP_NACK_PKTS_MAX; idx++) {
    pRtcp->fbs_nack[idx].hdr.ssrc = ssrc;
  }

  return 0;
}

void capture_rtcprr_reset(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc, uint32_t localSSRC) {

  memset(&pRtcp->rr, 0, sizeof(pRtcp->rr));

  //
  // Build RTCP RR packet static values
  //
  pRtcp->rr.hdr.hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pRtcp->rr.hdr.hdr |= 0 & 0x40; // padding 0
  pRtcp->rr.hdr.hdr |= 1 & 0x3f; // rr count 1
  pRtcp->rr.hdr.pt = RTCP_PT_RR;
  pRtcp->rr.hdr.len = htons(7);   // in 4 byte words

  //TODO: the RR ssrc needs to be the SSRC of any packets being streamed out of this instance
  pRtcp->rr.hdr.ssrc = localSSRC;

  //
  //The RR id (ssrc) needs to correspond to the SSRC of the remote sender
  //
  pRtcp->rr.ssrc_mediasrc = ssrc;

  rtcp_init_sdes(&pRtcp->sdes, pRtcp->rr.hdr.ssrc);

  pRtcp->pktcnt = 0;
  pRtcp->pktdropcnt = 0;
  pRtcp->pktdroptot = 0;

  pRtcp->rfc3550jitter = 0;

  rtcprr_reset_fir(pRtcp, ssrc);
  rtcprr_reset_pli(pRtcp, ssrc);
  rtcprr_reset_nack(pRtcp, ssrc);

}


#if 0
static int capture_calc_jitter(const CAPTURE_JTBUF_PKT_T *pJtBufPkt, CAPTURE_STREAM_T *pStream) {
  int rc = 0;
  uint64_t tvElapsedHz;
  uint64_t tsElapsedHz; 
  int64_t deltaHz;
  uint64_t tvElapsedUs0;
  double tsElapsedMs0;
  //double deltaUs;
  //double tsElapsedMs;
  //double j;

  tvElapsedUs0 = ((pJtBufPkt->pktData.tv.tv_sec - pStream->hdr.tvStart.tv_sec) * TIME_VAL_US) + 
                 (pJtBufPkt->pktData.tv.tv_usec - pStream->hdr.tvStart.tv_usec);

  if(pStream->clockHz > 0) {
    tsElapsedMs0 = (((uint64_t )pJtBufPkt->pktData.u.rtp.ts - pStream->ts0) +
                ((uint64_t ) pStream->tsRolls << 32)) * 1000 / pStream->clockHz;
    pStream->rtcpRR.deviationMs0 = (int64_t)(tvElapsedUs0/1000 - tsElapsedMs0);
  }  
  //fprintf(stderr, "tvElapsedUs0: %llu, tsElapsedMs0: %.3f, delta0: %lld ms\n", tvElapsedUs0, tsElapsedMs0, (int64_t) (tvElapsedUs0/1000 - tsElapsedMs0));

  //fprintf(stderr, "calling cbOnPkt seq:%u, sz:%u rtp ts:%u caplen:%d len:%d, tm:%u,%u, %uHz, delta:%lld ms (mark:%lld ms)\n",pJtBufPkt->pktData.u.rtp.seq, (pJtBufPkt->pktData.u.rtp.marksz & PKTDATA_RTP_MASK_SZ), pJtBufPkt->pktData.u.rtp.ts, PKTCAPLEN(pJtBufPkt->pktData.payload), PKTWIRELEN(pJtBufPkt->pktData.payload), pJtBufPkt->pktData.tv.tv_sec, pJtBufPkt->pktData.tv.tv_usec, pStream->clockHz, pStream->rtcpRR.deviationMs0, pStream->rtcpRR.deviationMs0Mark);

  if((pStream->rtcpRR.deviationMs0 - pStream->rtcpRR.deviationMs0Mark) > MAX(100,pStream->maxRtpGapWaitTmMs) &&
     pStream->rtcpRR.deviationMs0 > MAX(100,pStream->maxRtpGapWaitTmMs)) {

    LOG(X_WARNING("RTP ssrc:0x%x, seq:%u, ts:%uHz (clock: %uHz) is late by %llu ms"), pStream->hdr.key.ssrc, pJtBufPkt->pktData.u.rtp.seq, pJtBufPkt->pktData.u.rtp.ts, pStream->clockHz, pStream->rtcpRR.deviationMs0);

    pStream->rtcpRR.deviationMs0Mark = MAX(0, pStream->rtcpRR.deviationMs0);
  } else if(pStream->rtcpRR.deviationMs0 < pStream->rtcpRR.deviationMs0Mark) {
    pStream->rtcpRR.deviationMs0Mark = MAX(0, pStream->rtcpRR.deviationMs0);
  }

  //
  // Compute rfc-3550 jitter for each packet, even video packets of the same frame
  // since these can arrive late as well
  //
  if(pStream->rtcpRR.tvPriorPkt.tv_sec > 0) {
    //&&(tsElapsedHz = RTP_TS_DELTA(pJtBufPkt->pktData.u.rtp.ts, pStream->rtcpRR.tsPriorPkt)) > 0) {

    tsElapsedHz = RTP_TS_DELTA(pJtBufPkt->pktData.u.rtp.ts, pStream->rtcpRR.tsPriorPkt);
    tvElapsedHz = ((pJtBufPkt->pktData.tv.tv_sec - pStream->rtcpRR.tvPriorPkt.tv_sec) * TIME_VAL_US) + 
                  (pJtBufPkt->pktData.tv.tv_usec - pStream->rtcpRR.tvPriorPkt.tv_usec);
    tvElapsedHz = tvElapsedHz * pStream->clockHz / TIME_VAL_US;
    deltaHz = tvElapsedHz - tsElapsedHz;
    if(deltaHz < 0) {
      deltaHz = -1 * deltaHz;
    }
    //fprintf(stderr, "DELTAHZ:%u %uHz\n", deltaHz, pStream->clockHz);
    pStream->rtcpRR.rfc3550jitter += ((double)deltaHz - pStream->rtcpRR.rfc3550jitter) / 16.0f;

    //fprintf(stderr, "tvElapsedHz:%llu, tsElapsedHz:%llu, %dHz, deltaHz:%lld, j:%.3fHz\n", tvElapsedHz, tsElapsedHz, pStream->clockHz, (tvElapsedHz - tsElapsedHz), pStream->rtcpRR.rfc3550jitter);
    //fprintf(stderr, "tvElapsedUs0: %llu, tsElapsedMs0: %.3f, delta0: %lld ms\n", tvElapsedUs0, tsElapsedMs0, (int64_t) (tvElapsedUs0/1000 - tsElapsedMs0));

  }

  if(pStream->rtcpRR.tvPriorPkt.tv_sec == 0 ||
     pStream->rtcpRR.tsPriorPkt != pJtBufPkt->pktData.u.rtp.ts) {
    memcpy(&pStream->rtcpRR.tvPriorPkt, &pJtBufPkt->pktData.tv, sizeof(pStream->rtcpRR.tvPriorPkt));
    pStream->rtcpRR.tsPriorPkt = pJtBufPkt->pktData.u.rtp.ts;
    pStream->rtcpRR.clockHz = pStream->clockHz;
  }

  return rc;
}
#endif // 0



#endif // VSX_HAVE_CAPTURE
