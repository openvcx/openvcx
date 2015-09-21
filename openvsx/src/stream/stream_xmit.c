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

#if defined(VSX_HAVE_STREAMER)

//static int g_sr;
//static int g_delme;

//#define DUMP_RTP  1
//#define DROP_RTP 1
//#define DUP_RTP 1
//#define OO_RTP 1
//#define DTX_RTP 1
//#define DTMF_RTP 1

/*
static void testrecord(STREAM_XMIT_NODE_T *pStream, unsigned char *pData, unsigned int len) {
  static int fd;
  char path[256];
  if(!fd) { sprintf(path, "test%d.m2t", getpid());fd = open(path, O_CREAT | O_RDWR, 0666); }

  write(fd, pData, len);

}
*/

static int sendRtcpSr(STREAM_XMIT_NODE_T *pStream, unsigned int idxDest) {
  int rc = 0;
  OUTFMT_FRAME_DATA_T frameData;
  TIME_VAL tvNow = timer_GetTime();
  unsigned char buf[256];
  char tmp[128];
  int len = 0;
  STREAM_RTP_DEST_T *pdest = &pStream->pRtpMulti->pdests[idxDest];

  if(!pStream->pXmitAction->do_output_rtphdr || !pdest->sendrtcpsr) {
    return 0;
  }

  //
  // Ensure that the very first RTCP SR (before any RTP was even sent) contains a valid timestamp
  //
  if(pdest->rtcp.sr.rtpts == 0 && pStream->pRtpMulti->pRtp->timeStamp != 0) {
    pdest->rtcp.sr.rtpts = pStream->pRtpMulti->pRtp->timeStamp;
  }

  if((len = stream_rtcp_createsr(pStream->pRtpMulti, idxDest, buf, sizeof(buf))) < 0) {
    LOG(X_ERROR("Unable to create rtcp sr for destination[%d]"), idxDest);
    return -1;
  }

  VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - sendto rtcp sr pt:%d [dest:%d] %s:%d len:%d pkt:%u,%u ts:%u"), 
               (pStream->pRtpMulti->pRtp->pt & RTP_PT_MASK),idxDest, 
               FORMAT_NETADDR(pdest->saDstsRtcp, tmp, sizeof(tmp)), ntohs(INET_PORT(pdest->saDstsRtcp)), len, 
               htonl(pdest->rtcp.sr.pktcnt), htonl(pdest->rtcp.sr.octetcnt),  htonl(pdest->rtcp.sr.rtpts)));

  pdest->rtcp.tmLastSrSent = tvNow;

  if(pdest->noxmit) { 

    pthread_mutex_lock(&pStream->pRtpMulti->mtx);
    if(pdest->outCb.pliveQIdx) {

      //
      // outidx should always be 0, as each outidx will have its own dedicated packetizer streamer
      //
      memset(&frameData, 0, sizeof(frameData));
      frameData.xout.outidx = 0;
      //
      // The timestamp here is for pktqueue stats tracking
      //
      OUTFMT_PTS(&frameData) = tvNow * 90 / TIME_VAL_MS;
      OUTFMT_DATA(&frameData) = (unsigned char *) buf;
      OUTFMT_LEN(&frameData) = len;
      //
      // In the case of RTSP interleaved, the port is actually the chnanel id
      //
      frameData.channelId = ntohs(INET_PORT(pdest->saDstsRtcp));
      liveq_addpkt(pStream->pLiveQ2, *pdest->outCb.pliveQIdx, &frameData);
    }
    pthread_mutex_unlock(&pStream->pRtpMulti->mtx);

    rc = len;

  } else {

#define DUMP_RTCP_XMIT_DEFS \
  //uint32_t ntptm = htonl((htonl( ((RTCP_PKT_SR_T *)buf)->ntp_msw) & 0xffff) << 16 | (htonl( ((RTCP_PKT_SR_T *)buf)->ntp_lsw) >> 16)); 
  uint32_t factor = 0xffffffff / TIME_VAL_US; \
  uint32_t sec = htonl(((RTCP_PKT_SR_T *)buf)->ntp_msw); \
  uint32_t usec = htonl(((RTCP_PKT_SR_T *)buf)->ntp_lsw) / factor; 

#define DUMP_RTCP_XMIT_FMT "RTP - rtcp-xmt(0x%lx:%d): pt:%3d, len:%4d, ts:%9u, pkt:%4d, bytecnt:%9u, ntp:%9us,%6uus (0x%x,0x%x)"
#define DUMP_RTCP_XMIT_ARGS  (long unsigned int) (long unsigned int *)pStream, idxDest, pStream->pRtpMulti->init.pt, len, htonl(((RTCP_PKT_SR_T *)buf)->rtpts), htonl(((RTCP_PKT_SR_T *)buf)->pktcnt), htonl(((RTCP_PKT_SR_T *)buf)->octetcnt), sec, usec, htonl(((RTCP_PKT_SR_T *)buf)->ntp_msw), htonl(((RTCP_PKT_SR_T *)buf)->ntp_lsw)

  VSX_DEBUG_RTP( 
    DUMP_RTCP_XMIT_DEFS
    LOG(X_DEBUG(DUMP_RTCP_XMIT_FMT), DUMP_RTCP_XMIT_ARGS); 
  );
#if defined(DUMP_RTP)
    DUMP_RTCP_XMIT_DEFS
    VSX_DEBUGLOG_TIME
    fprintf(stderr, DUMP_RTCP_XMIT_FMT, DUMP_RTCP_XMIT_ARGS);
#endif // 0


    if((rc = streamxmit_sendto(pdest, buf, len, 1, 0, 0)) < 0) {
      return -1;
    }

    //if(pdest->pstreamStats) {
    //  stream_stats_addsample(pdest->pstreamStats, len, 0);
    //}

  }

  return rc;
}

//static uint32_t delmeprevvid,delmeprevaud;
//static int delme_drop;

static int sendPktUdpRtp(STREAM_XMIT_NODE_T *pStream, unsigned int idxDest,
                         const unsigned char *pData, unsigned int len) {
  int rc = 0;
  OUTFMT_FRAME_DATA_T frameData;

  if(!pStream->pXmitAction->do_output_rtphdr) {

    if(len >= RTP_HEADER_LEN) {
      pData += RTP_HEADER_LEN;
      len -= RTP_HEADER_LEN;
    } else {
      len = 0;
    }

  }

  if(len == 0) {
    LOG(X_WARNING("sendto called with 0 length"));
    return 0;
  }

  pStream->pRtpMulti->pdests[idxDest].rtcp.sr.rtpts = pStream->pRtpMulti->pRtp->timeStamp;
  pStream->pRtpMulti->pdests[idxDest].rtcp.pktcnt++;
  pStream->pRtpMulti->pdests[idxDest].rtcp.octetcnt += len;
  pStream->pRtpMulti->pdests[idxDest].rtcp.seqhighest = pStream->pRtpMulti->pRtp->sequence_num;
 
  //if(pStream->pRtpMulti->pdests[idxDest].rtcp.pktcnt % 50 == 49) return len;

  //if(ntohs(pStream->pRtpMulti->pdests[idxDest].saDsts.sin_port)==5006) fprintf(stderr, "%llu sendto[dst:%d] %s %s:%d %d bytes pt:%u mk:%d seq:%u ts:%u ssrc:0x%x\n", timer_GetTime(), idxDest, (pStream->pXmitAction->do_output_rtphdr?"rtp":"udp"),inet_ntoa(pStream->pRtpMulti->pdests[idxDest].saDsts.sin_addr), ntohs(pStream->pRtpMulti->pdests[idxDest].saDsts.sin_port), len, pStream->pRtpMulti->pRtp->pt&0x7f, (pStream->pRtpMulti->pRtp->pt&0x80)?1:0, htons(pStream->pRtpMulti->pRtp->sequence_num), htonl(pStream->pRtpMulti->pRtp->timeStamp), pStream->pRtpMulti->pRtp->ssrc);

  int dropped=0;
#if defined(DROP_RTP)
  static int pkt_tot_v, pkt_tot_a, drop_tot_v, drop_tot_a, drop_cnt_v,drop_cnt_a;
  static float pkt_drop_percent;
  if(1&&((pStream->pRtpMulti->pRtp->pt&0x7f) == 112 || (pStream->pRtpMulti->pRtp->pt&0x7f) == 120 ||
     (pStream->pRtpMulti->pRtp->pt&0x7f) == 100)) {
    //if(pkt_tot_v++ == 100) drop_cnt_v=1;
    if(pkt_tot_v++ % 30 == 1)drop_cnt_v=(random()%1 + 2); 
    //if(1||htons(pStream->pRtpMulti->pRtp->sequence_num) == 0) drop_cnt_v=10;
    if(drop_cnt_v>0){ drop_cnt_v--; drop_tot_v++; dropped=1; pkt_drop_percent=(float)drop_tot_v/pkt_tot_v;} 
  } else if(0&&(pStream->pRtpMulti->pRtp->pt&0x7f) == 96 || (pStream->pRtpMulti->pRtp->pt&0x7f) == 111) {
    if(pkt_tot_a++ % 30 == 1)drop_cnt_a=(random()%2 +1); 
    if(drop_cnt_a>0){ drop_cnt_a--; drop_tot_a++; dropped=1; pkt_drop_percent=(float)drop_tot_a/pkt_tot_a;}
  }
  if(dropped) { LOG(X_DEBUG("DROP RTP pt:%d, seq:%u, ts:%u, loss:%.2f%%"), pStream->pRtpMulti->pRtp->pt&0x7f, htons(pStream->pRtpMulti->pRtp->sequence_num), htonl(pStream->pRtpMulti->pRtp->timeStamp), pkt_drop_percent *100.0f); }
  //if(dropped && len > 30) { fprintf(stderr, "CORRUPT SRTP pt:%d, seq;%u\n", pStream->pRtpMulti->pRtp->pt&0x7f, htons(pStream->pRtpMulti->pRtp->sequence_num)); *((char *) &pData[MIN(len-30,90)+30]) = 0x05; }
#endif // (DROP_RTP)

#if defined(DTX_RTP)
  static int seq_last_silence=0;
  static int silence_pkts = 0;
  if((pStream->pRtpMulti->pRtp->pt&0x7f) == 0 || (pStream->pRtpMulti->pRtp->pt&0x7f) == 8) {
    if(htons(pStream->pRtpMulti->pRtp->sequence_num) >= 400 &&
      htons(pStream->pRtpMulti->pRtp->sequence_num) % 100 == 99) {
      seq_last_silence = htons(pStream->pRtpMulti->pRtp->sequence_num);
      silence_pkts = 40;
      fprintf(stderr, "INSERTING DTX silence of %d pkts at seq:%d at ts:%u\n", silence_pkts, seq_last_silence, htonl(pStream->pRtpMulti->pRtp->timeStamp));
    }
    if(silence_pkts > 0) {
      if(--silence_pkts > 0) {
        return len;
      } else {
        pStream->pRtpMulti->pRtp->sequence_num = ntohs(seq_last_silence);
        fprintf(stderr, "RESUMING audio after DTX at seq:%d at ts:%u\n", seq_last_silence, htonl(pStream->pRtpMulti->pRtp->timeStamp));
      }
    }
  }

#endif // DTX_RTP

#if defined(DTMF_RTP)
  static int dtmf_pkts;
  int pt=96;
  if((pStream->pRtpMulti->pRtp->pt&0x7f) == pt || (pStream->pRtpMulti->pRtp->pt&0x7f) == 101) {
    if(htons(pStream->pRtpMulti->pRtp->sequence_num) >= 100 &&
      htons(pStream->pRtpMulti->pRtp->sequence_num) % 50 == 49) {
      dtmf_pkts=6;
    }
    
    if(dtmf_pkts> 0) {
      pStream->pRtpMulti->pRtp->pt = 101; 
      --dtmf_pkts;
      len=MIN(len,16);
    } else {
      pStream->pRtpMulti->pRtp->pt = pt; 
    }
  }

#endif // DTX_RTP


    //fprintf(stderr, "sendto idxDest:%d noxmit:%d pRtpMulti: 0x%x %s:%d for %d bytes\n", idxDest, pStream->pRtpMulti->pdests[idxDest].noxmit, pStream->pRtpMulti, inet_ntoa(pStream->pRtpMulti->pdests[idxDest].saDsts.sin_addr), ntohs(pStream->pRtpMulti->pdests[idxDest].saDsts.sin_port), len);

  //
  // noxmit may be set for a program such as RTSP interleaved output
  //
  //fprintf(stderr, "NOXMIT:%d pRtpMulti: 0x%x, pDest: 0x%x,  idxDest: %d, data len:%d\n", pStream->pRtpMulti->pdests[idxDest].noxmit, pStream->pRtpMulti, &pStream->pRtpMulti->pdests[idxDest], idxDest, len);

  if(pStream->pRtpMulti->pdests[idxDest].noxmit) { 

    pthread_mutex_lock(&pStream->pRtpMulti->mtx);
    if(pStream->pRtpMulti->pdests[idxDest].outCb.pliveQIdx) {

      //
      // outidx should always be 0, as each outidx will have its own dedicated packetizer streamer
      //
      memset(&frameData, 0, sizeof(frameData));
      frameData.xout.outidx = 0;
      //
      // The timestamp here is for pktqueue stats tracking
      //
      OUTFMT_PTS(&frameData) = timer_GetTime() * 90 / TIME_VAL_MS;
      OUTFMT_DATA(&frameData) = (unsigned char *) pData;
      OUTFMT_LEN(&frameData) = len;
      OUTFMT_KEYFRAME(&frameData) = pStream->cur_iskeyframe;
      //
      // In the case of RTSP interleaved, the port is actually the channel id
      //
      frameData.channelId = ntohs(INET_PORT(pStream->pRtpMulti->pdests[idxDest].saDsts));
      liveq_addpkt(pStream->pLiveQ2, 
                   *pStream->pRtpMulti->pdests[idxDest].outCb.pliveQIdx,
                   &frameData);
    }
    pthread_mutex_unlock(&pStream->pRtpMulti->mtx);

    rc = len;

  } else {

//static int drop_frame; uint32_t s_last_ts; static uint32_t s_keyframe_ts;if(pStream->cur_iskeyframe) s_keyframe_ts= pStream->pRtpMulti->pRtp->timeStamp;
//if(!drop_frame && s_keyframe_ts == pStream->pRtpMulti->pRtp->timeStamp && delme_drop % 4 == 3) drop_frame=1;else if(s_keyframe_ts != pStream->pRtpMulti->pRtp->timeStamp) drop_frame=0;
//if(s_last_ts != pStream->pRtpMulti->pRtp->timeStamp) delme_drop++;
//s_last_ts=pStream->pRtpMulti->pRtp->timeStamp;

//if(pStream->pRtpMulti->init.pt == 112 && drop_frame) { rc = len; fprintf(stderr, "drop ts:%u len:%d\n", htonl(pStream->pRtpMulti->pRtp->timeStamp), len);} else {
//if((delme_drop++ % 25 == 24)) { rc = len; } else {
    //fprintf(stderr, "sendto len:%d, 0x%x 0x%x 0x%x 0x%x\n", len, pData[0], pData[1], pData[2], pData[3]);

#define DUMP_RTP_XMIT_FMT "RTP - rtp-xmit(0x%lx:%d): pt:%3d len:%4d, ts:%9u, seq:%5u, marker:%d, keyfr:%d"
#define DUMP_RTP_XMIT_ARGS  (long unsigned int) (long unsigned int *) pStream, idxDest, pStream->pRtpMulti->init.pt, len, htonl(pStream->pRtpMulti->pRtp->timeStamp), htons(pStream->pRtpMulti->pRtp->sequence_num), (pStream->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK) ? 1 : 0, pStream->cur_iskeyframe

  //VSX_DEBUGLOG(DUMP_RTP_XMIT);
  VSX_DEBUG_RTP( LOG(X_DEBUG(DUMP_RTP_XMIT_FMT), DUMP_RTP_XMIT_ARGS));

#if defined(DUMP_RTP)
    VSX_DEBUGLOG_TIME
    fprintf(stderr, DUMP_RTP_XMIT_FMT, DUMP_RTP_XMIT_ARGS);
#endif // 0

  //fprintf(stderr, "sendPktUdpRtp len:%d idxDest:%d srtp:%d,%d, pStream: 0x%x, idxProg:%d\n", len, idxDest, pStream->pRtpMulti->pdests[idxDest].srtp.lenKey, pStream->pRtpMulti->pdests[idxDest].srtp.lenKey, pStream, pStream->idxProg);

#if defined(DUP_RTP)
  static int pkt_dup_v, pkt_dup_a, dup_tot_v, dup_tot_a, dup_cnt_v,dup_cnt_a;
  static float pkt_dup_percent;
  int dupped=0;
  if((pStream->pRtpMulti->pRtp->pt&0x7f) == 112 || (pStream->pRtpMulti->pRtp->pt&0x7f) == 97) {
    if(pkt_dup_v++ % 20 == 1)dup_cnt_v=(random()%6 + 1);
    if(dup_cnt_v>0){ dup_cnt_v--; dup_tot_v++; dupped=1; pkt_dup_percent=(float)dup_tot_v/pkt_dup_v;}
  } else if(0&&(pStream->pRtpMulti->pRtp->pt&0x7f) == 96) {
    if(pkt_dup_a++ % 30 == 1)dup_cnt_a=(random()%3 +1);
    if(dup_cnt_a>0){ dup_cnt_a--; dup_tot_a++; dupped=1; pkt_dup_percent=(float)dup_tot_a/pkt_dup_a;}
  }
  if(dupped) {
    fprintf(stderr, "DUP RTP pt:%d, seq:%u, ts:%u, loss:%.2f%%\n", pStream->pRtpMulti->pRtp->pt&0x7f, htons(pStream->pRtpMulti->pRtp->sequence_num), htonl(pStream->pRtpMulti->pRtp->timeStamp), pkt_dup_percent *100.0f); 
     netio_sendto(STREAM_RTP_PNETIOSOCK(pStream->pRtpMulti->pdests[idxDest]),   
                  (struct sockaddr *) &pStream->pRtpMulti->pdests[idxDest].saDsts,
                  (void *) pData, len, NULL);
  }
#endif // OO_RTP

#if defined(OO_RTP)
  #define OO_LEN  8
  static unsigned int pkt_oo_v, pkt_oo_a;
  static unsigned char oo_buf[2048][OO_LEN];
  static unsigned int oo_lens[OO_LEN];
  static unsigned int oo_ages[OO_LEN];
  unsigned int lensrtp;
  int oo_queue=0;
  int oo_check_q = 0;
  unsigned int oo_buf_idx = 0;
  if((pStream->pRtpMulti->pRtp->pt&0x7f) == 112 || (pStream->pRtpMulti->pRtp->pt&0x7f) == 97 ||
     (pStream->pRtpMulti->pRtp->pt&0x7f) == 100) {
    oo_check_q = 1;
    if(oo_lens[oo_buf_idx] == 0 && pkt_oo_v++ % 40 == 1) { oo_queue=1; }
  } else if(0 && (pStream->pRtpMulti->pRtp->pt&0x7f) == 96) {
    oo_check_q = 1;
    if(oo_lens[oo_buf_idx] == 0 && pkt_oo_v++ % 20 == 1) { oo_queue=1; }
  }

  if(oo_queue) {
    LOG(X_DEBUG("QUEING OUT-Of_ORDER RTP pt:%d, seq:%u, ts:%u, len:%d"), pStream->pRtpMulti->pRtp->pt&0x7f, htons(pStream->pRtpMulti->pRtp->sequence_num), htonl(pStream->pRtpMulti->pRtp->timeStamp), len);
    //avc_dumpHex(stderr, pData, MIN(16, len), 1);

    if(pStream->pRtpMulti->pdests[idxDest].srtp.k.lenKey > 0) {
      lensrtp = 2048;
      if(srtp_encryptPacket(&pStream->pRtpMulti->pdests[idxDest].srtp, (const unsigned char *) pData, len, &oo_buf[0][oo_buf_idx], &lensrtp, 0) < 0) {
        return -1;
      }
      len = lensrtp;
    } else {
      memcpy(&oo_buf[0][oo_buf_idx], pData, len);
    }

    oo_lens[oo_buf_idx] = len;
    oo_ages[oo_buf_idx] = 20;
    return len;
  }
  if(oo_check_q) {
    if(oo_ages[oo_buf_idx] > 0) {
      oo_ages[oo_buf_idx]--;
    } else if(oo_lens[oo_buf_idx] > 0) {
      LOG(X_DEBUG("SENDING OUT-Of_ORDER RTP len:%d at seq:%d"), oo_lens[oo_buf_idx], htons(pStream->pRtpMulti->pRtp->sequence_num));
      oo_lens[oo_buf_idx] = 0;
    }
  }
#endif // OO_RTP

    //LOG(X_DEBUG("SENDING ON idxDest[%d] pdest: 0x%x, len:%d"), idxDest, &pStream->pRtpMulti->pdests[idxDest], len);

    if((rc = streamxmit_sendto(&pStream->pRtpMulti->pdests[idxDest], pData, len, 0, 
                               pStream->cur_iskeyframe, dropped)) < 0) {
      return -1;
    }

    //if(pStream->pRtpMulti->pdests[idxDest].pstreamStats) {
    //  stream_stats_addsample(pStream->pRtpMulti->pdests[idxDest].pstreamStats, len, 
    //                         htons(pStream->pRtpMulti->pRtp->sequence_num));
    //}

  }

  return rc;
}

static void stream_report(STREAM_XMIT_NODE_T *pStream) {
  VSXLIB_STREAM_REPORT_T *pReport = &pStream->pBwDescr->pReportCfg->reportCbData;

  pReport->reportedPktsLost = pStream->pRtpMulti->pdests[0].rtcp.reportedLost;
  pReport->reportedPktsLostTot = pStream->pRtpMulti->pdests[0].rtcp.reportedLostTot;
  pReport->reportedPkts = pStream->pRtpMulti->pdests[0].rtcp.reportedSeqNum;
  pReport->reportedPktsTot = pStream->pRtpMulti->pdests[0].rtcp.reportedSeqNumTot;

  pReport->pktsSent = pStream->pBwDescr->pkts;
  pReport->bytesSent = pStream->pBwDescr->bytes;
  pReport->pktsSentTot = pStream->pBwDescr->pktsTot;
  pReport->bytesSentTot = pStream->pBwDescr->bytesTot;

  pStream->pBwDescr->pReportCfg->reportCb(pStream->pBwDescr->pReportCfg->pCbData, 
                                          pReport);

}

int streamxmit_sendpkt(STREAM_XMIT_NODE_T *pStream) {
  int rc = 0;
  int triedXmit;
  int sz;
  int szPkt;
  TIME_VAL tvNow;
  unsigned int idxDest;
  unsigned int szData;
  const unsigned char *pData;
  unsigned int szDataPayload;
  const unsigned char *pDataPayload;
  COLLECT_STREAM_PKTDATA_T collectPkt;
  unsigned int bytes = 0;
  unsigned int pkts = 0;

  tvNow = timer_GetTime();
  pData = stream_rtp_data(pStream->pRtpMulti);
  szData = stream_rtp_datalen(pStream->pRtpMulti);

  if(szData >= RTP_HEADER_LEN) {
    pDataPayload = pData + RTP_HEADER_LEN;
    szDataPayload = szData - RTP_HEADER_LEN;
  } else {
    pDataPayload = NULL;
    szDataPayload = 0;
  }

  if(pStream->pXmitAction->do_stream) {
    triedXmit = 1;
    pkts++;

    //LOG(X_DEBUG("STREAM_SENDPKT pRtpMulti:0x%x do_output:%d numD:%d liveQ:0x%x,%d"), pStream->pRtpMulti, pStream->pXmitAction->do_output, pStream->pRtpMulti->numDests,pStream->pLiveQ,  pStream->pLiveQ ? pStream->pLiveQ->numActive : 0);

    //TODO: numDest / isactive checking should be mutex protected
    if(pStream->pXmitAction->do_output && pStream->pRtpMulti->numDests > 0) {

      szPkt = 0;
      for(idxDest = 0; idxDest < pStream->pRtpMulti->maxDests; idxDest++) {

        if(!pStream->pRtpMulti->pdests[idxDest].isactive || pStream->pXmitDestRc[idxDest] != 0) {
         continue;
        }

        if(pStream->rawSend) {
          if(pktgen_Queue(pData, szData) != 0) {
            pStream->pXmitDestRc[idxDest] = -1;
            sz = -1;
          } else {
            sz = szData;
          }
        } else {

          //fprintf(stderr, "PRTPMULTI: 0x%x, idxDest[%d/%d] port:%d\n", pStream->pRtpMulti, idxDest, pStream->pRtpMulti->numDests, ntohs(pStream->pRtpMulti->pdests[idxDest].saDsts.sin_port));

          //
          // Check and send an rtcp sender report
          //
          if(*pStream->pfrtcp_sr_intervalsec > 0 &&
              (tvNow - pStream->pRtpMulti->pdests[idxDest].tmLastRtcpSr) / TIME_VAL_MS  >
              *pStream->pfrtcp_sr_intervalsec * 1000) {

            sendRtcpSr(pStream, idxDest);
            pStream->pRtpMulti->pdests[idxDest].tmLastRtcpSr = tvNow;
          }
       
          if((sz = sendPktUdpRtp(pStream, idxDest, pData, szData)) < 0) {
            pStream->pXmitDestRc[idxDest] = sz;
          }
        }
        if(szPkt == 0 && sz > 0) {
          szPkt = sz;
        }
      } // end of for

      // Exit if there are no good transmitters in the list
      if(pStream->pRtpMulti->numDests > 0 && pStream->pXmitAction->do_output && szPkt == 0) {
        return -1;
      }

    } else {    // if do_output
      szPkt = szData;
    }

    if(!pStream->pXmitAction->do_output_rtphdr && szPkt > RTP_HEADER_LEN) {
      szPkt -= RTP_HEADER_LEN;
    }

    bytes += szPkt;

    //
    // Add the packet data to any outbound ts readers (ts / http)
    //
    // TODO: do not hardcode szData > RTP_HEADER_LEN rtp hdr len
    if(pStream->pLiveQ && pStream->pLiveQ->numActive > 0 && szDataPayload > 0) {
      pthread_mutex_lock(&pStream->pLiveQ->mtx);
      for(sz = 0; (unsigned int) sz < pStream->pLiveQ->max; sz++) {
        if(pStream->pLiveQ->pQs[sz]) {
          pktqueue_addpkt(pStream->pLiveQ->pQs[sz], pDataPayload, szDataPayload, NULL, 0);
        }
      }
      pthread_mutex_unlock(&pStream->pLiveQ->mtx);
    }

  }

  //testrecord(pStream, pDataPayload, szDataPayload);

  if(!pStream->disableActions) {

    //
    // Record output stream
    //
    if(pStream->pXmitAction->do_record_post && pStream->pXmitCbs->cbRecord &&
       pStream->pXmitCbs->pCbRecordData) {

      memset(&collectPkt, 0, sizeof(collectPkt));
      collectPkt.payload.pData = (unsigned char *) pDataPayload;
      PKTCAPLEN(collectPkt.payload) = szDataPayload;
      if((sz = pStream->pXmitCbs->cbRecord(pStream->pXmitCbs->pCbRecordData,
                                              &collectPkt)) < 0) {
        return -1;
      }
      if(triedXmit == 0) {
        triedXmit = 1;
      }
    }

    //
    // Call post processing function, such as http live streaming
    // callback to segment and package output ts files
    //
    if(pStream->pXmitAction->do_httplive && pStream->pXmitCbs->cbPostProc &&
       pStream->pXmitCbs->pCbPostProcData &&
       pStream->pXmitAction->do_stream) {

//fprintf(stderr, "CALLING CBPOSTPROC len:%d bwdescr:0x%x, pliveQ:0x%x\n", szDataPayload, pStream->pBwDescr, pStream->pLiveQ);
      if((sz = pStream->pXmitCbs->cbPostProc(pStream->pXmitCbs->pCbPostProcData,
                                                 pDataPayload, szDataPayload)) < 0) {
        return -1;
      }
      if(triedXmit == 0) {
        triedXmit = 1;
      }
    }

  } // end of disableActions


  //
  // Update any bandwidth description status
  //
  if(pStream->pBwDescr) {

    //TODO: this may be broken for multiple output...
    if(pStream->pBwDescr->tmpPkts == 0 && pStream->pBwDescr->pktsTot == 0) {
      pStream->pRtpMulti->pdests[0].rtcp.prevseqhighest = 
                                 htons(pStream->pRtpMulti->pRtp->sequence_num);
    }

    pStream->pBwDescr->tmpPkts += pkts;
    pStream->pBwDescr->tmpBytes += bytes;
    if(pStream->pBwDescr->tmpLastUpdateTm == 0) {
      pStream->pBwDescr->tmpLastUpdateTm = tvNow;
    }

    if((tvNow / TIME_VAL_US) > (pStream->pBwDescr->tmpLastUpdateTm / TIME_VAL_US) + 1) {
      pStream->pBwDescr->pkts = pStream->pBwDescr->tmpPkts;
      pStream->pBwDescr->bytes = pStream->pBwDescr->tmpBytes;
      pStream->pBwDescr->pktsTot += pStream->pBwDescr->tmpPkts;
      pStream->pBwDescr->bytesTot += pStream->pBwDescr->tmpBytes;
      pStream->pBwDescr->intervalMs = (float)
                (tvNow - pStream->pBwDescr->tmpLastUpdateTm)/ TIME_VAL_MS;
      TV_FROM_TIMEVAL(pStream->pBwDescr->updateTv, tvNow);

      if(pStream->pBwDescr->pReportCfg && pStream->pBwDescr->pReportCfg->reportCb) {

        stream_report(pStream);

//fprintf(stderr, "BW D xmit pt:%d pkts:%d(%lld) bytes:%d rr pkts:%d(%lld), lost:%d(%d)\n", (pStream->pRtpMulti->pRtp->pt & RTP_PT_MASK), pStream->pBwDescr->pkts,  pStream->pBwDescr->pktsTot, pStream->pBwDescr->bytes, pStream->pRtpMulti->pdests[0].rtcp.reportedSeqNum, pStream->pRtpMulti->pdests[0].rtcp.reportedSeqNumTot, pStream->pRtpMulti->pdests[0].rtcp.reportedLost, pStream->pRtpMulti->pdests[0].rtcp.reportedLostTot);

      }
      pStream->pBwDescr->tmpLastUpdateTm = tvNow;
      pStream->pBwDescr->tmpPkts = 0;
      pStream->pBwDescr->tmpBytes = 0;

    }

  }


  pStream->pRtpMulti->payloadLen = 0;

  return rc;
}

int streamxmit_writepkt(STREAM_XMIT_NODE_T *pStream) {
  int rc = 0;
  FILE_STREAM_T *pFile = pStream->pXmitCbs->pCbRecordData;

  if(pStream->pRtpMulti->payloadLen > 0) {
    if((rc = WriteFileStream(pFile, pStream->pRtpMulti->pPayload,
          pStream->pRtpMulti->payloadLen)) != pStream->pRtpMulti->payloadLen) {
      return -1;
    }
  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
