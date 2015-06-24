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


#define RTP_RETRANSMIT_HISTORY_MS     1000
#define RTP_RETRANSMIT_INTERVAL_MS    200 

typedef struct STREAMXMIT_PKT_HDR {
  uint16_t                 flags;
  uint16_t                 seqNum;
  uint32_t                 timeStamp;
  int                      rtcp;
  int                      keyframe;

  int                      doRetransmit; 
  TIME_VAL                 tvCreate;
  TIME_VAL                 tvXmit;
  TIME_VAL                 tvLastRetransmit;
} STREAMXMIT_PKT_HDR_T;

static int streamxmit_async_start(STREAM_RTP_MULTI_T *pRtp, STREAM_STATS_MONITOR_T *pMonitor, int lock);

/*
void pktqueue_cb_streamxmit_dump_pkthdr(void *p) {
  STREAMXMIT_PKT_HDR_T *pHdr = (STREAMXMIT_PKT_HDR_T *) p;

  fprintf(stderr, " flags:0x%x, seq:%u, ts:%u, rtcp:%d, keyframe:%d, tvXmit:%llums", 
          pHdr->flags, pHdr->seqNum, pHdr->timeStamp, pHdr->rtcp, pHdr->keyframe, 
          pHdr->tvXmit /TIME_VAL_MS);

}
*/

int streamxmit_init(STREAM_RTP_DEST_T *pDest, const STREAM_DEST_CFG_T *pDestCfg) {
  unsigned int numPkts = 0;
  unsigned int szPkt;
  int iTmp;
  STREAM_XMIT_QUEUE_T *pAsyncQ = NULL;
  PKTQUEUE_T *pQ = NULL;
  STREAM_STATS_MONITOR_T *pMonitor = NULL;

  if(!pDest || !pDestCfg) {
    return -1;
  }

  pAsyncQ = &pDest->asyncQ;

  if(pDestCfg->pFbReq && pDestCfg->pFbReq->nackRtpRetransmit) {
    pAsyncQ->doRtcpNack = 1; 
  }

  pAsyncQ->doAsyncXmit = 0;  // TODO: implement for packet output smoothing
  //
  // We run if we are to do async RTP xmit 
  // or we support RTP-retransmissions for video (using RTCP NACK notifications) 
  //
  if(!pAsyncQ->doAsyncXmit && (!pAsyncQ->doRtcpNack || pDest->pRtpMulti->isaud)) {
    return 0;
  }

  if(pAsyncQ->doRtcpNack) {

    //
    // Compute an RTP retransmission queue size
    //
    if(pDest->pRtpMulti->pStreamerCfg->xcode.vid.common.cfgDo_xcode &&
      pDest->pRtpMulti->pStreamerCfg->xcode.vid.out[pDestCfg->xcodeOutidx].cfgBitRateOut > 0 &&
      pDest->pRtpMulti->init.maxPayloadSz > 0) {

      iTmp = (pDest->pRtpMulti->pStreamerCfg->xcode.vid.out[pDestCfg->xcodeOutidx].cfgBitRateOut / 
       (8 * 10 * pDest->pRtpMulti->init.maxPayloadSz) + 2) * 10;

      numPkts = MIN(150, MAX(50, iTmp));
    } else {
      numPkts = 100;
    }

//TODO: really only need burstmeter when capping retransmission traffic
    burstmeter_init(&pAsyncQ->retransmissionMeter, 1, 1000);

//pAsyncQ->retransmissionKbpsMax = 30;

  } else {
    numPkts = 40;
  }
  szPkt = pDest->pRtpMulti->init.maxPayloadSz + RTP_HEADER_LEN + DTLS_OVERHEAD_SIZE + 64;

  // No locking!... should rely on pAsyncQ->mtx
  if(!(pQ = pktqueue_create(numPkts, szPkt, 0, 0, 0, sizeof(STREAMXMIT_PKT_HDR_T), 0))) {
    return -1;
  }

  LOG(X_DEBUG("Created RTP %soutput packet queue size: %dB x %dpkts"), pAsyncQ->doRtcpNack ? "NACK " : "", 
              szPkt, numPkts);

  pQ->cfg.overwriteType = PKTQ_OVERWRITE_OK;
  pQ->cfg.id = STREAMER_QID_NACK;
  pktqueue_setrdr(pQ, 0);
  pthread_mutex_init(&pAsyncQ->mtx, NULL);
  pAsyncQ->nackHistoryMs = RTP_RETRANSMIT_HISTORY_MS;
  pAsyncQ->pQ = pQ;

  //
  // Start the RTP output queue processor thread
  //
  if(pDestCfg->pMonitor && pDestCfg->pMonitor->active) {
    pMonitor = pDestCfg->pMonitor; 
  }
  streamxmit_async_start(pDest->pRtpMulti, pMonitor, 0);

  return 0;
}

int streamxmit_close(STREAM_RTP_DEST_T *pDest, int lock) {
  int rc = 0;
  STREAM_XMIT_QUEUE_T *pAsyncQ;

  if(!pDest) {
    return -1;
  }

  pAsyncQ = &pDest->asyncQ;

  if(lock) {
    pthread_mutex_lock(&pDest->pRtpMulti->mtx);
  }
 
  if(pAsyncQ->pQ) {
    pktqueue_destroy(pAsyncQ->pQ);
    pAsyncQ->doAsyncXmit = 0;
    pAsyncQ->doRtcpNack = 0;
    pAsyncQ->pQ = NULL;
    pthread_mutex_destroy(&pAsyncQ->mtx);
  }

  burstmeter_close(&pAsyncQ->retransmissionMeter);

  if(lock) {
    pthread_mutex_unlock(&pDest->pRtpMulti->mtx);
  }

  return rc;
}

int streamxmit_sendto(STREAM_RTP_DEST_T *pDest, const unsigned char *data, unsigned int len, 
                      int rtcp, int keyframe, int drop) {
  int rc = 0;
  NETIO_SOCK_T *pnetsock = NULL;
  const unsigned char *pData = data;
  PKTQ_EXTRADATA_T qXtra;
  STREAMXMIT_PKT_HDR_T pktHdr;
  unsigned char encBuf[PACKETGEN_PKT_UDP_DATA_SZ];

  //
  // If we're using DTLS-SRTP over audio/video mux (on same RTP port) then we need to choose
  // the right socket which has completed the DTLS handshake
  //
  if(rtcp) {
    pnetsock = STREAM_RTCP_PNETIOSOCK(*pDest);
  } else {
    pnetsock = STREAM_RTP_PNETIOSOCK(*pDest);
  }

  //LOG(X_DEBUG("STREAMXMIT_SENDTO len:%d, rtcp:%d, drop:%d, pDest: 0x%x, srtp:0x%x, pSrtpShared: 0x%x, 0x%x, lenK:%d"), len, rtcp, drop, pDest, &pDest->srtps[rtcp ? 1 : 0], pDest->srtps[rtcp ? 1 : 0].pSrtpShared, SRTP_CTXT_PTR(&pDest->srtps[rtcp ? 1 : 0]), SRTP_CTXT_PTR(&pDest->srtps[rtcp ? 1 : 0])->k.lenKey);LOGHEX_DEBUG(data, MIN(16, len));

  if((rc = srtp_dtls_protect(pnetsock, data, len, encBuf, sizeof(encBuf),
                      rtcp ? &pDest->saDstsRtcp : &pDest->saDsts, 
                      pDest->srtps[rtcp ? 1 : 0].pSrtpShared,
                      rtcp ? SENDTO_PKT_TYPE_RTCP : SENDTO_PKT_TYPE_RTP)) < 0) {
    if(rc == -2) {
      // DTLS handshake not complete
      return 0;
    } else {
      return -1;
    }
  } else if(rc > 0) {
    pData = encBuf;
    len = rc;
  }

  //
  // Queue the packet for async sending
  //
  if(pDest->asyncQ.pQ && (pDest->asyncQ.doAsyncXmit || (!rtcp && pDest->asyncQ.doRtcpNack))) {

    memset(&qXtra, 0, sizeof(qXtra));
    memset(&pktHdr, 0, sizeof(pktHdr));
    pktHdr.flags = 0;
    pktHdr.tvCreate = timer_GetTime();
    if(!pDest->asyncQ.doAsyncXmit) {
      pktHdr.tvXmit = pktHdr.tvCreate;
    }
    if(!(pktHdr.rtcp = rtcp)) {
      pktHdr.seqNum = htons(pDest->pRtpMulti->pRtp->sequence_num);
      pktHdr.timeStamp = htonl(pDest->pRtpMulti->pRtp->timeStamp);
      pktHdr.keyframe = keyframe;
    }
    qXtra.pQUserData = &pktHdr;

    pthread_mutex_lock(&pDest->asyncQ.mtx);

    if(!pDest->asyncQ.isCurPktKeyframe && keyframe) {
      pDest->asyncQ.haveLastKeyframeSeqNumStart = 1;
      pDest->asyncQ.lastKeyframeSeqNumStart = pktHdr.seqNum;
      //LOG(X_DEBUG("NACK - GOP start at packet %d"), pktHdr.seqNum);
    }

    if(pktqueue_addpkt(pDest->asyncQ.pQ, pData, len, &qXtra,
       (!pDest->asyncQ.isCurPktKeyframe && keyframe ? 1 : 0)) != PKTQUEUE_RC_OK) {
      pthread_mutex_unlock(&pDest->asyncQ.mtx);
      return -1;
    }
    //LOG(X_DEBUG("NACK q rd:[%d]/%d, wr:%d"), pDest->asyncQ.pQ->idxRd, pDest->asyncQ.pQ->cfg.maxPkts, pDest->asyncQ.pQ->idxWr);
    pDest->asyncQ.isCurPktKeyframe = keyframe;

    pthread_mutex_unlock(&pDest->asyncQ.mtx);
    rc = len;

    //pktqueue_dump(pDest->asyncQ.pQ,  pktqueue_cb_streamxmit_dump_pkthdr);
  } 

  if(!pDest->asyncQ.doAsyncXmit && !drop) {
    if((rc = srtp_sendto(pnetsock, (void *) pData, len, 0, 
                         rtcp ? &pDest->saDstsRtcp : &pDest->saDsts, NULL, 
                         rtcp ? SENDTO_PKT_TYPE_RTCP : SENDTO_PKT_TYPE_RTP, 1)) < 0) {
      return -1;
    }

  } else if(drop) {
    rc = len;
  }

  if(pDest->pstreamStats) {
    stream_stats_addPktSample(pDest->pstreamStats, &pDest->streamStatsMtx, len, !rtcp);
  }

  return rc;
}

int streamxmit_onRTCPNACK(STREAM_RTP_DEST_T *pDest, const RTCP_PKT_RTPFB_NACK_T *pNack) {
  STREAMXMIT_PKT_HDR_T *pktHdr;
  TIME_VAL tvNow;
  uint16_t nackSeqNumStart;
  uint16_t nackBlp;
  unsigned int seqNumOffset;
  unsigned int idxWr;
  unsigned int count = 0;
  int doRetransmission = 0;
  STREAM_XMIT_QUEUE_T *pAsyncQ = NULL;

  if(!pDest || !pNack) {
    return -1;
  } else if(!(pAsyncQ = &pDest->asyncQ) || !pAsyncQ->doRtcpNack || !pAsyncQ->pQ) {
    return 0;
  }

//TODO: compute incoming NACK rate to outgoing RTP rate for ABR algorithm

  tvNow = timer_GetTime();
  nackSeqNumStart = htons(pNack->pid);
  nackBlp = htons(pNack->blp);

  pthread_mutex_lock(&pAsyncQ->mtx);

  //LOG(X_DEBUG("streamxmit_onRTCPNACK nackSeqNumStart:%d"), nackSeqNumStart);
  //fprintf(stderr, "treamxmit_onRTCPNACK nackSeqNumStart... nackSeqNumStart:%d\n", nackSeqNumStart); pktqueue_dump(pDest->asyncQ.pQ,  pktqueue_cb_streamxmit_dump_pkthdr);

  //
  // If the highest possible NACK sequence num came before the first packet of the keyframe of the current GOP,
  // then disregard it because we are assuming the keyframe is an IDR.
  //
  if(pAsyncQ->haveLastKeyframeSeqNumStart && nackSeqNumStart + 16 < pAsyncQ->lastKeyframeSeqNumStart) {
    pthread_mutex_unlock(&pAsyncQ->mtx);
    LOG(X_DEBUG("RTP NACK for sequence: %d ignored because it pertains to a prior GOP sequence:%d"), nackSeqNumStart, 
                pDest->asyncQ.lastKeyframeSeqNumStart);
    return 0;
  }

  //
  // Iterate the stored packet list backwards starting with the most recently 
  // transmitted packet
  //
  idxWr = pAsyncQ->pQ->idxWr;
  //LOG(X_DEBUG("streamxmit_onRTCPNACK starting at idxWr:%d / %d"), idxWr-1, pAsyncQ->pQ->cfg.maxPkts);
  while(count++ < pAsyncQ->pQ->cfg.maxPkts) {

    if(idxWr > 0) {
      idxWr--;
    } else {
      idxWr = pAsyncQ->pQ->cfg.maxPkts - 1;
    }

    if(!(pAsyncQ->pQ->pkts[idxWr].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
      //
      // The write queue slot is empty
      //
      //LOG(X_DEBUG("streamxmit_onRTCPNACK idxWr:%d / %d is empty.. break"), idxWr, pAsyncQ->pQ->cfg.maxPkts);
      break;

    } else if((pktHdr = (STREAMXMIT_PKT_HDR_T *) pAsyncQ->pQ->pkts[idxWr].xtra.pQUserData) && !pktHdr->rtcp) {

      if(pktHdr->seqNum < nackSeqNumStart) {

        //
        // The queue slot RTP sequence number came before the NACK starting sequence number
        // so the current queue content is before the scope of the NACK
        //
        //LOG(X_DEBUG("streamxmit_onRTCPNACK idxWr:%d / %d is empty.. break.. seqNuM:%d came before nackSeqNumStart:%d"), idxWr, pAsyncQ->pQ->cfg.maxPkts, pktHdr->seqNum, nackSeqNumStart);
        break;

      } else if(pktHdr->tvXmit + (pAsyncQ->nackHistoryMs * TIME_VAL_MS) < tvNow) {

        //
        // The queue slot packet time is too old and beyond our threshold for RTP retransmission
        //
        //LOG(X_DEBUG("streamxmit_onRTCPNACK idxWr:%d / %d is empty.. seqNuM:%d nack too old to honor"), idxWr, pAsyncQ->pQ->cfg.maxPkts, pktHdr->seqNum);
        if(pktHdr->doRetransmit >= 0) {
          LOG(X_DEBUG("RTP NACK for sequence: %d ignored because it has age %d ms > %d ms"), nackSeqNumStart, pAsyncQ->nackHistoryMs,
               (tvNow - pktHdr->tvXmit) / TIME_VAL_MS);
        }
        pktHdr->doRetransmit = -1;
        break;

//TODO: handle seq roll... should not be a big problem here...
      } else if(pktHdr->doRetransmit >= 0 && pktHdr->seqNum <= nackSeqNumStart + 16) {

        //LOG(X_DEBUG("streamxmit_onRTCPNACK idxWr:%d / %d seqNum:%d, nackSeqNumStart:%d is within range. seqNumOffset:%d, nackBlp:0x%x"), idxWr, pAsyncQ->pQ->cfg.maxPkts, pktHdr->seqNum, nackSeqNumStart, (pktHdr->seqNum - nackSeqNumStart), nackBlp);

        if((seqNumOffset = pktHdr->seqNum - nackSeqNumStart) == 0 || (nackBlp & (1 << (seqNumOffset - 1)))) {

          //
          // The packet sequence number matches a packet which is being negatively-acknowledged so mark
          // it for retransmission
          //
          if(pktHdr->doRetransmit >= 0 && 
             (!pAsyncQ->haveLastKeyframeSeqNumStart || pktHdr->seqNum >= pAsyncQ->lastKeyframeSeqNumStart)) {

            LOG(X_DEBUGV("RTP NACK marked pt:%d sequence:%d for retransmission"), 
                pDest->pRtpMulti->init.pt, pktHdr->seqNum);

            if(pktHdr->doRetransmit == 0) {

              if(pDest->pstreamStats) {
                stream_abr_notifyBitrate(pDest->pstreamStats, &pDest->streamStatsMtx, 
                                         STREAM_ABR_UPDATE_REASON_NACK_REQ, .5f);
              }

            }

            pktHdr->doRetransmit++;

          } else {
            LOG(X_DEBUG("RTP NACK not marking pt:%d sequence:%d for retransmission"), pDest->pRtpMulti->init.pt, pktHdr->seqNum);
          }


       
          doRetransmission = 1;
        }

      }
    }

  }

  if(doRetransmission) {
    if(!pAsyncQ->haveRequestedRetransmission) {
      pAsyncQ->haveRequestedRetransmission = 1;
      //
      // Signal the output retransmission thread
      //
      vsxlib_cond_signal(&pDest->pRtpMulti->asyncRtpCond.cond, &pDest->pRtpMulti->asyncRtpCond.mtx);
    }
  }

  pthread_mutex_unlock(&pAsyncQ->mtx);

  return 0;
}

static int streamxmit_retransmitRtp(STREAM_RTP_DEST_T *pDest) {
  STREAMXMIT_PKT_HDR_T *pktHdr;
  unsigned int idxRd;
  unsigned int count = 0;
  int lastWasExpired = 0;
  int rc;
  int haveRequestedRetransmission = 0;
  const PKTQUEUE_PKT_T *pQPkt = NULL;
  STREAM_XMIT_QUEUE_T *pAsyncQ = &pDest->asyncQ;
  TIME_VAL tvNow;
  struct timeval tv;
  float kbps;
  //char buf[256];

  tvNow = timer_GetTime();
  TV_FROM_TIMEVAL(tv, tvNow);

  pthread_mutex_lock(&pAsyncQ->mtx);
  idxRd = pAsyncQ->pQ->idxRd;
  //LOG(X_DEBUG("NACK streamxmit_iterate ... idxRd:%d, idxWr:%d / %d"), pAsyncQ->pQ->idxRd, pAsyncQ->pQ->idxWr, pAsyncQ->pQ->cfg.maxPkts);
  while(count++ < pAsyncQ->pQ->cfg.maxPkts) {

    pQPkt = &pAsyncQ->pQ->pkts[idxRd];

    if(!(pQPkt->flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
      //LOG(X_DEBUG("NACK streamxmit_iterate break at qid[%d]"), idxRd);
      break;
    }
    //LOG(X_DEBUG("NACK streamxmit_iterate check qid[%d] seqno:%d, %lld ms ago"), idxRd, ((STREAMXMIT_PKT_HDR_T *) pQPkt->xtra.pQUserData)->seqNum, (tvNow - ((STREAMXMIT_PKT_HDR_T *) pQPkt->xtra.pQUserData)->tvxmit)/1000);

    lastWasExpired = 0;

    if(!(pktHdr = (STREAMXMIT_PKT_HDR_T *) pQPkt->xtra.pQUserData)) {

      pAsyncQ->pQ->idxRd = idxRd;
      fprintf(stderr, "streamxmit_itertate should never get here... idxRd:%d\n", idxRd);

    } else if(pktHdr->tvXmit + (pAsyncQ->nackHistoryMs * TIME_VAL_MS) < tvNow) {
      //
      // The queue slot RTP sequence number came before the NACK starting sequence number
      // so the current queue content is before the scope of the NACK
      //
      if(pktHdr->doRetransmit > 0) {
        pktHdr->doRetransmit = -1;

        if(pDest->pstreamStats) {
          stream_abr_notifyBitrate(pDest->pstreamStats, &pDest->streamStatsMtx, 
                                   STREAM_ABR_UPDATE_REASON_NACK_AGED, .5f);
        }

      }
      pAsyncQ->pQ->idxRd = idxRd;
      pAsyncQ->pQ->pkts[idxRd].flags = 0;
      lastWasExpired = 1;
      //LOG(X_DEBUG("NACK streamxmit_iterate old here marked to -1 ... idxRd:%d, seqNum:%d"), idxRd, pktHdr->seqNum);

    } else if(pktHdr->doRetransmit > 0 && !pktHdr->rtcp) {

      if(pktHdr->tvLastRetransmit == 0 || 
         pktHdr->tvLastRetransmit + (RTP_RETRANSMIT_INTERVAL_MS * TIME_VAL_MS) < tvNow) {

        //
        // Ensure that the retransmission will not exceed our retransmission bitrate limit
        //
        if(pAsyncQ->retransmissionKbpsMax > 0 &&
          burstmeter_updateCounters(&pAsyncQ->retransmissionMeter, &tv) == 0 &&
          (kbps = (burstmeter_getBitrateBps(&pAsyncQ->retransmissionMeter)/THROUGHPUT_BYTES_IN_KILO_F)) >= pAsyncQ->retransmissionKbpsMax) {

          LOG(X_WARNING("RTP NACK retransmission bitrate %.3f >= %.3f  throttled pt:%d sequence:%d to %s:%d, for original %d ms ago."), 
                     kbps, pAsyncQ->retransmissionKbpsMax, pDest->pRtpMulti->init.pt, 
                      pktHdr->seqNum, inet_ntoa(pDest->saDstsRtcp.sin_addr), ntohs(pDest->saDstsRtcp.sin_port),
                     (tvNow - pktHdr->tvXmit) / TIME_VAL_MS);

          if(pDest->pstreamStats) {
            stream_abr_notifyBitrate(pDest->pstreamStats, &pDest->streamStatsMtx,
                                     STREAM_ABR_UPDATE_REASON_NACK_THROTTLED, .5f);
          }

          pktHdr->doRetransmit = 0;
          pktHdr->tvLastRetransmit = tvNow; 

        } else {

          burstmeter_AddSample(&pAsyncQ->retransmissionMeter, pQPkt->len, &tv);

          //LOG(X_DEBUG("RTP NACK Retransmission bitrate: %.3f %s (%.3f bps)"),  (float) pAsyncQ->retransmissionMeter.meter.rangeMs/1000, burstmeter_printThroughputStr(buf, sizeof(buf), &pAsyncQ->retransmissionMeter), (float) burstmeter_getBitrateBps(&pAsyncQ->retransmissionMeter));

          //
          // Retransmit the RTP packet
          //
          LOG(X_DEBUG("RTP NACK retransmitting pt:%d sequence:%d, len:%d to %s:%d, original sent %d ms ago.  "
                      "Rate: %.3f pkts/s, %.3f b/s"), 
                    pDest->pRtpMulti->init.pt, pktHdr->seqNum, pQPkt->len, 
                    inet_ntoa(pDest->saDstsRtcp.sin_addr), ntohs(pDest->saDstsRtcp.sin_port), (tvNow - pktHdr->tvXmit) / TIME_VAL_MS,
                    (float) burstmeter_getPacketratePs(&pAsyncQ->retransmissionMeter),
                    (float) burstmeter_getBitrateBps(&pAsyncQ->retransmissionMeter));

          if((rc = srtp_sendto(STREAM_RTP_PNETIOSOCK(*pDest), (void *) pQPkt->pData, pQPkt->len, 0,
                              &pDest->saDsts, NULL, SENDTO_PKT_TYPE_RTP, 1)) < 0) {
          
            pktHdr->doRetransmit = -1;
          } else {
            pktHdr->doRetransmit = 0;
          }

          pktHdr->tvLastRetransmit = tvNow; 
          if(pDest->pstreamStats) {
            stream_abr_notifyBitrate(pDest->pstreamStats, &pDest->streamStatsMtx,
                                     STREAM_ABR_UPDATE_REASON_NACK_RETRANSMITTED, .5f);
          }

        }

      } else {
        haveRequestedRetransmission = 1;
      }
   
    }

    if(++idxRd >= pAsyncQ->pQ->cfg.maxPkts) {
      idxRd = 0;
    }
    if(lastWasExpired) {
      pAsyncQ->pQ->idxRd = idxRd;
    } 

  } // end of while...

  pAsyncQ->haveRequestedRetransmission = haveRequestedRetransmission;

  pthread_mutex_unlock(&pAsyncQ->mtx);

  return haveRequestedRetransmission;
}

typedef struct STREAMXMIT_WRAP {
  STREAM_RTP_MULTI_T        *pRtp;
  STREAM_STATS_MONITOR_T    *pMonitor;
  char                       tid_tag[LOGUTIL_TAG_LENGTH];
} STREAMXMIT_WRAP_T;

void streamxmit_async_proc(void *pArg) {
  STREAMXMIT_WRAP_T wrap;
  int rc;
  STREAM_RTP_MULTI_T *pRtp = NULL;
  STREAM_RTP_DEST_T *pDest;
  unsigned int idx;
  struct timespec ts;
  struct timeval tv;
  int haveRequestedRetransmission = 0;

  memcpy(&wrap, (STREAMXMIT_WRAP_T *) pArg, sizeof(wrap));
  logutil_tid_add(pthread_self(), wrap.tid_tag);
  pRtp = wrap.pRtp;
  pRtp->asyncRtpRunning = 1;

  LOG(X_DEBUG("RTP output thread started"));

  while(pRtp->asyncRtpRunning == 1 && !g_proc_exit) {

    gettimeofday(&tv, NULL);
    if(haveRequestedRetransmission) {
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = tv.tv_usec + 20000;
      //LOG(X_DEBUG("RTP output thread going to wait for 20ms.."));
    } else {
      ts.tv_sec = tv.tv_sec + 2;
      ts.tv_nsec = tv.tv_usec;
      //LOG(X_DEBUG("RTP output thread going to wait for 2 sec.."));
    }

    if((rc = pthread_cond_timedwait(&pRtp->asyncRtpCond.cond, &pRtp->asyncRtpCond.mtx, &ts)) < 0) {
      LOG(X_WARNING("RTP output thread pthread_cond_timedwait failed"));
      usleep(20000);
    }

    if(pRtp->asyncRtpRunning != 1 || g_proc_exit) {
      break;
    }

    haveRequestedRetransmission = 0;
    pthread_mutex_lock(&pRtp->mtx);

    //LOG(X_DEBUG("RTP output thread doing its thing.."));
    if(pRtp->numDests > 0) {

      for(idx = 0; idx < pRtp->maxDests; idx++) {
        pDest = &pRtp->pdests[idx];

//if(pDest->isactive) { LOG(X_DEBUG("ACTIVE[%d]"), idx);  stream_abr_notifyitrate(pDest->pstreamStats, &pDest->streamStatsMtx, STREAM_ABR_UPDATE_REASON_NACK_REQ, .5f); }

        if(pDest->isactive && pDest->asyncQ.haveRequestedRetransmission) {
          if((rc = streamxmit_retransmitRtp(pDest)) > 0) {
            haveRequestedRetransmission = 1;
          }
        }
      }

    }

    pthread_mutex_unlock(&pRtp->mtx);

  }

  LOG(X_DEBUG("RTP output thread ending"));
  pRtp->asyncRtpRunning = -1;

  logutil_tid_remove(pthread_self());
}

int streamxmit_async_stop(STREAM_RTP_MULTI_T *pRtp, int lock) {

  int rc = 0;

  if(!pRtp) {
    return -1; 
  }

  if(lock) {
    pthread_mutex_lock(&pRtp->mtx);
  }

  //LOG(X_DEBUG("streamxmit_async_stop called %d"), pRtp->asyncRtpRunning);

  if(pRtp->asyncRtpRunning <= 0) {
    if(lock) {
      pthread_mutex_unlock(&pRtp->mtx);
    }
    return 0;
  } else {
    pRtp->asyncRtpRunning = 0;
  }

  if(lock) {
    pthread_mutex_unlock(&pRtp->mtx);
  }


  do {
    vsxlib_cond_signal(&pRtp->asyncRtpCond.cond, &pRtp->asyncRtpCond.mtx);
    usleep(20000);
  } while(pRtp->asyncRtpRunning != -1);

  pthread_cond_destroy(&pRtp->asyncRtpCond.cond);
  pthread_mutex_destroy(&pRtp->asyncRtpCond.mtx);

  return rc;
}

static int streamxmit_async_start(STREAM_RTP_MULTI_T *pRtp, STREAM_STATS_MONITOR_T *pMonitor, int lock) {
  int rc = 0;
  pthread_t ptdCap;
  pthread_attr_t attrCap;
  STREAMXMIT_WRAP_T wrap;
  const char *s;

  if(lock) {
    pthread_mutex_lock(&pRtp->mtx);
  }

  if(pRtp->asyncRtpRunning > 0) {
    if(lock) {
      pthread_mutex_unlock(&pRtp->mtx);
    }
    return 0;
  }

  pthread_mutex_init(&pRtp->asyncRtpCond.mtx, NULL);
  pthread_cond_init(&pRtp->asyncRtpCond.cond, NULL);

  wrap.pRtp = pRtp;
  wrap.pMonitor = pMonitor;
  wrap.tid_tag[0] = '\0';
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(wrap.tid_tag, sizeof(wrap.tid_tag), "%s-rtpxmit", s);
  }

  pRtp->asyncRtpRunning = 2;
  pthread_attr_init(&attrCap);
  pthread_attr_setdetachstate(&attrCap, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdCap,
                    &attrCap,
                    (void *) streamxmit_async_proc,
                    (void *) &wrap) != 0) {

    LOG(X_ERROR("Unable to create RTP output thread"));
    pRtp->asyncRtpRunning = 0;
    pthread_cond_destroy(&pRtp->asyncRtpCond.cond);
    pthread_mutex_destroy(&pRtp->asyncRtpCond.mtx);
    if(lock) {
      pthread_mutex_unlock(&pRtp->mtx);
    }
    return -1;
  }

  if(lock) {
    pthread_mutex_unlock(&pRtp->mtx);
  }

  while(pRtp->asyncRtpRunning == 2) {
    usleep(5000);
  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
