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

//#define DROP_RTP 1

#if defined(VSX_HAVE_CAPTURE)

#define RTP_SEQ_INCR_MIN          1

static int capture_flushJtBuf(CAPTURE_STATE_T *pState, CAPTURE_STREAM_T *pStream, const COLLECT_STREAM_PKT_T *pPkt);

static CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *jtbufpool_get(CAPTURE_JTBUF_PKTDATA_POOL_T *pPool) {
  CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *pEntry  = NULL;

  if(pPool->numAllocEntries >= (int) pPool->maxEntries) {
    return NULL;
  }

  if(pPool->pEntries) {
    pEntry = pPool->pEntries;
    pPool->pEntries = pPool->pEntries->pnext;
    pEntry->pnext = NULL;
  } else if((pEntry = (CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *) 
                       avc_calloc(1, sizeof(CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T)))) {

    if((pEntry->pBuf = avc_calloc(1, pPool->entryBufSz)) == NULL) {       
      free(pEntry);
      return NULL;
    }
    pPool->numAllocEntries++;
  }
  return pEntry;
}

static int jtbufpool_put(CAPTURE_JTBUF_PKTDATA_POOL_T *pPool, CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *pEntry) {
  if(!pEntry) {
    return -1;
  }
  if(pPool->pEntries) {
    pEntry->pnext = pPool->pEntries;
  }
  pPool->pEntries = pEntry;
  return 0;
}

static int jtbufpool_free(CAPTURE_JTBUF_PKTDATA_POOL_T *pPool) {
  CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *pEntry = NULL;

  while(pPool->pEntries) {
    pEntry = pPool->pEntries->pnext;
    if(pPool->pEntries->pBuf) {
      free(pPool->pEntries->pBuf);
    }
    free(pPool->pEntries);
    pPool->pEntries = pEntry;
  }
  pPool->numAllocEntries = 0;

  return 0;
}

static void jtbuf_close(CAPTURE_JTBUF_T *pjtBuf) {

  if(pjtBuf->pPkts) {
    //LOG(X_DEBUG("JTBUF - freeing 0x%x pool size %d%d sizePkts:%d"), pjtBuf, pjtBuf->pool.maxEntries, pjtBuf->pool.entryBufSz, pjtBuf->sizePkts);
    jtbufpool_free(&pjtBuf->pool);
    avc_free((void **) &pjtBuf->pPkts);
  }
  pjtBuf->pool.maxEntries = 0;
  pjtBuf->pool.entryBufSz = 0;
  pjtBuf->sizePkts = 0;

}

void rtp_captureFree(CAPTURE_STATE_T *pState) {
  CAPTURE_STREAM_T *pStream;
  unsigned int idxStream;

  if(!pState) {
    return;
  }

  jtbuf_close(&pState->jtBufVid);
  pState->pjtBufVid = NULL;

  jtbuf_close(&pState->jtBufAud);
  pState->pjtBufAud = NULL;

  capture_abr_close(&pState->rembAbr);

  if(pState->pStreams) {
    for(idxStream = 0; idxStream < pState->maxStreams; idxStream++) {
      pStream = &pState->pStreams[idxStream];
    //if(pState->pStreams[idxStream].jtBuf.pPkts) {
    //  jtbufpool_free(&pState->pStreams[idxStream].jtBuf.pool);
    //  free(pState->pStreams[idxStream].jtBuf.pPkts);
    //}

      if(pStream->pFilter) {
        srtp_closeInputStream((SRTP_CTXT_T *) &pStream->pFilter->srtps[0]);
        srtp_closeInputStream((SRTP_CTXT_T *) &pStream->pFilter->srtps[1]);
      }

    }
  }


  free(pState->pStreams);
  pState->pStreams = NULL;
  pthread_mutex_destroy(&pState->mutexStreams);
  free(pState);

}


static int jtbuf_init(CAPTURE_JTBUF_T *pjtBuf, unsigned int jtBufSzPkts, unsigned int jtBufPktBufSz) {

  jtbuf_close(pjtBuf);

  if(jtBufSzPkts == 0 || jtBufPktBufSz == 0) {
    return 0;
  }

  if((pjtBuf->pPkts = (CAPTURE_JTBUF_PKT_T *) avc_calloc(jtBufSzPkts, sizeof(CAPTURE_JTBUF_PKT_T))) == NULL) {
    return -1;
  }

  pjtBuf->pool.maxEntries = jtBufSzPkts;
  pjtBuf->pool.entryBufSz = jtBufPktBufSz;
  pjtBuf->sizePkts = jtBufSzPkts;

  //LOG(X_DEBUG("JTBUF - created 0x%x pool size %d%d sizePkts:%d"), pjtBuf, pjtBuf->pool.maxEntries, pjtBuf->pool.entryBufSz, pjtBuf->sizePkts);

  return 0;
}


CAPTURE_STATE_T *rtp_captureCreate(unsigned int maxStreams, 
                                   unsigned int jtBufSzPktsVid,
                                   unsigned int jtBufSzPktsAud,
                                   unsigned int jtBufPktBufSz,
                                   unsigned int cfgMaxVidRtpGapWaitTmMs,
                                   unsigned int cfgMaxAudRtpGapWaitTmMs) {
  CAPTURE_STATE_T *pState = NULL;
  //unsigned int idxStream;

  if(maxStreams < 1) {
    return NULL;
  }

  if(!(pState = (CAPTURE_STATE_T *) avc_calloc(1, sizeof(CAPTURE_STATE_T)))) {
    rtp_captureFree(pState);
    return NULL;
  }

  if((pState->pStreams = (CAPTURE_STREAM_T *) avc_calloc(maxStreams, sizeof(CAPTURE_STREAM_T))) == NULL) {
    rtp_captureFree(pState);
    return NULL;
  }

  if(jtBufPktBufSz > 0) {
    if((jtBufSzPktsVid > 0 && jtbuf_init(&pState->jtBufVid, jtBufSzPktsVid, jtBufPktBufSz) < 0) ||
       (jtBufSzPktsAud > 0 && jtbuf_init(&pState->jtBufAud, jtBufSzPktsAud, jtBufPktBufSz) < 0)) {
      rtp_captureFree(pState);
      return NULL;
    }
  } 

  if(pState->jtBufVid.pPkts) {
    pState->pjtBufVid = &pState->jtBufVid;
  }
  if(pState->jtBufAud.pPkts) {
    pState->pjtBufAud = &pState->jtBufAud;
  }

  pState->maxStreams = maxStreams;
  pState->cfgMaxVidRtpGapWaitTmMs = cfgMaxVidRtpGapWaitTmMs; 
  pState->cfgMaxAudRtpGapWaitTmMs = cfgMaxAudRtpGapWaitTmMs; 

  pthread_mutex_init(&pState->mutexStreams, NULL);

  if(jtBufPktBufSz > 0) {
    LOG(X_DEBUG("Capture max flows set to: %u, packet reorder queue: %u x %u, audio: %u x %u"),
             pState->maxStreams, 
            jtBufSzPktsVid, (jtBufSzPktsVid > 0 ? jtBufPktBufSz : 0),
            jtBufSzPktsAud, (jtBufSzPktsAud > 0 ? jtBufPktBufSz : 0));
  }

  return pState;
}


int capture_delete_stream(CAPTURE_STATE_T *pState, CAPTURE_STREAM_T *pStream) {

  char tmp[32];

  if(pStream) {

    //fprintf(stderr, "DELETE_STREAM... ssrc:0x%x\n", pStream->hdr.key.ssrc);

    if(IS_CAPTURE_FILTER_TRANSPORT_RTP(pStream->pFilter->transType)) {
      snprintf(tmp, sizeof(tmp), " ssrc:0x%x",  (pStream->hdr.key.ssrc));
    } else {
      tmp[0] = '\0';
    }
    LOG(X_DEBUG("Removing input stream %s %s%s"), pStream->strSrcDst,
         codecType_getCodecDescrStr(pStream->pFilter->mediaType), tmp);

    if(pStream->cbOnStreamEnd) {
      pStream->cbOnStreamEnd(pStream);
    }

    srtp_closeInputStream((SRTP_CTXT_T *) &pStream->pFilter->srtps[0]);
    srtp_closeInputStream((SRTP_CTXT_T *) &pStream->pFilter->srtps[1]);

    memset(&pStream->hdr, 0, sizeof(COLLECT_STREAM_HDR_T));

    if(!pState->pjtBufVid && pStream->pjtBuf == &pState->jtBufVid) {
      pState->pjtBufVid = pStream->pjtBuf;
    } else if(!pState->pjtBufAud && pStream->pjtBuf == &pState->jtBufAud) {
      pState->pjtBufAud = pStream->pjtBuf;
    }
    pStream->pjtBuf = NULL;
    //jtbufpool_free(&pStream->jtBuf.pool);

    pStream->tmLastPkt = 0;
    pStream->pFilter = NULL;
    pStream->pCbUserData = NULL;
    pStream->pRembAbr = NULL;

    return 0;
  }

/*
  unsigned int idx;

  // TODO: this should be a hash
  for(idx = 0; idx < pState->maxStreams; idx++) {
    if(memcmp(&pState->pStreams[idx].hdr.key, &pStream->hdr.key, pStream->hdr.key.lenKey) == 0) {
      memset(&pState->pStreams[idx].hdr, 0, sizeof(COLLECT_STREAM_HDR_T));
      jtbufpool_free(&pState->pStreams[idx].jtBuf.pool);
      return 0;
    }
  }
*/
  return -1;
}


int capture_check_rtp_bye(const CAPTURE_STATE_T *pState) {

  unsigned int idx;
  int numBye = 0;

  //
  // Check if all input streams have received RTCP BYE
  //
  for(idx = 0; idx < pState->maxStreams; idx++) {
    if(pState->pStreams[idx].numPkts > 0 && pState->pStreams[idx].pFilter &&
       IS_CAPTURE_FILTER_TRANSPORT_RTP(pState->pStreams[idx].pFilter->transType) &&
       pState->pStreams[idx].haveRtcpBye) {
      numBye++;
    }
  }

  return numBye;
}

static CAPTURE_STREAM_T *capture_rtp_lookup(CAPTURE_STATE_T *pState, 
                                            const COLLECT_STREAM_KEY_T *pKey) {

  unsigned int idx;

   // TODO: this should be a hash
  //avc_dumpHex(stderr, pKey, pKey->lenKey, 1);

  for(idx = 0; idx < pState->maxStreams; idx++) {

    //fprintf(stderr, "rtp_lookup[%d]\n", idx);
    //avc_dumpHex(stderr, &pState->pStreams[idx].hdr.key, pKey->lenKey, 1);

    if(memcmp(&pState->pStreams[idx].hdr.key, pKey, pKey->lenKey) == 0) {
      return &pState->pStreams[idx];
    }
  }

  return NULL;
}

static void init_stream(CAPTURE_STATE_T *pState, CAPTURE_STREAM_T *pStream, 
                        const COLLECT_STREAM_PKT_T *pPkt, int isvid) {

  char tmp[128];
  int sz;

  memcpy(&pStream->hdr, &pPkt->hdr, sizeof(COLLECT_STREAM_HDR_T));

  pStream->numPkts = 0;
  pStream->numBytes = 0;
  pStream->numBytesPayload = 0;
  pStream->numSeqIncrOk = 0;
  pStream->numSeqIncrInvalid = 0;
  pStream->numSeqnoNacked = 0;
  pStream->frameDeltaHz = 0;
  pStream->lastTsChangeTv.tv_sec = 0;
  pStream->lastTsChangeTv.tv_usec = 0;
  pStream->ts0 = pPkt->data.u.rtp.ts;
  pStream->tsRolls = 0;
  pStream->clockHz = 0;
  pStream->haveRtcpBye = 0;
  pStream->hdr.tvStart.tv_sec = pPkt->data.tv.tv_sec;
  pStream->hdr.tvStart.tv_usec = pPkt->data.tv.tv_usec;
  pStream->tmLastPkt = ((TIME_VAL)pPkt->data.tv.tv_sec * TIME_VAL_US) + 
                                  pPkt->data.tv.tv_usec;
  capture_rtcprr_reset(&pStream->rtcpRR, htonl(pPkt->hdr.key.ssrc), htonl(CREATE_RTP_SSRC()));

  if(isvid && pState->pjtBufVid) {
    pStream->pjtBuf = pState->pjtBufVid;
    pState->pjtBufVid = NULL;
  } else if(!isvid && pState->pjtBufAud) {
    pStream->pjtBuf = pState->pjtBufAud;
    pState->pjtBufAud = NULL;
  }

  if(pStream->pjtBuf && pStream->pjtBuf->pPkts) {
    memset(pStream->pjtBuf->pPkts, 0, pStream->pjtBuf->sizePkts * sizeof(CAPTURE_JTBUF_PKT_T));
     pStream->pjtBuf->idxPkt = 0;
     pStream->pjtBuf->idxPktStored = 0;
   }
   pStream->pFilter = NULL;

  pStream->strSrcDst[0] = '\0';

  if((sz = snprintf(pStream->strSrcDst, sizeof(pStream->strSrcDst), "%s%s%s:%d",
       pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? "[" : "",
       inet_ntop(pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? AF_INET6 : AF_INET, (pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? 
         (const void *) &pPkt->hdr.key.pair_srcipv6 : (const void *) &pPkt->hdr.key.pair_srcipv4), tmp, sizeof(tmp)), 
       pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? "]" : "", pPkt->hdr.key.srcPort)) > 0) {
      snprintf(&pStream->strSrcDst[sz], sizeof(pStream->strSrcDst) - sz, " -> %s%s%s:%d",
       pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? "[" : "",
       inet_ntop(pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? AF_INET6 : AF_INET, pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? 
                   (const void *) &pPkt->hdr.key.pair_dstipv6 : (const void *) &pPkt->hdr.key.pair_dstipv4,
                   tmp, sizeof(tmp)), 
       pPkt->hdr.key.lenIp == ADDR_LEN_IPV6 ? "]" : "", pPkt->hdr.key.dstPort);
  }

}


static CAPTURE_STREAM_T *add_stream(CAPTURE_STATE_T *pState, 
                                    const COLLECT_STREAM_PKT_T *pPkt,
                                    const CAPTURE_FILTER_T *pFilter) {
  unsigned int idx;
  CAPTURE_STREAM_T *pStream = NULL;
  int isvid = 0;
  TIME_VAL tmPkt = ((TIME_VAL)pPkt->data.tv.tv_sec * TIME_VAL_US) + pPkt->data.tv.tv_usec;

  for(idx = 0; idx < pState->maxStreams; idx++) {
    if(pState->pStreams[idx].tmLastPkt + (RTP_STREAM_IDLE_EXPIRE_MS * TIME_VAL_MS) < tmPkt) {

      pStream = &pState->pStreams[idx];

      if(pState->pStreams[idx].tmLastPkt != 0) {
        LOG(X_WARNING("Expiring %s, ssrc:0x%x, pt:%u"), 
            pStream->strSrcDst, pStream->hdr.key.ssrc, pStream->hdr.payloadType);

        capture_delete_stream(pState, pStream);
      }
      break;
    }
  }

  if(pStream) {
    if(pFilter && IS_CAP_FILTER_AUD(pFilter->mediaType)) {
      pStream->maxRtpGapWaitTmMs = pState->cfgMaxAudRtpGapWaitTmMs;
    } else {
      isvid = 1;
      pStream->maxRtpGapWaitTmMs = pState->cfgMaxVidRtpGapWaitTmMs;
    }
    init_stream(pState, pStream, pPkt, isvid);
  }

  return pStream;
}

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

#if 0
static void dump_jtbuf(CAPTURE_STREAM_T *pStream) {
  unsigned int count, idxPkt;

  idxPkt = pStream->pjtBuf->idxPkt;

  fprintf(stderr, "jtbuf: idxPkt:%d, idxPktStored:%d, startingSeq:%d, sizePkts:%d\n", 
          pStream->pjtBuf->idxPkt, pStream->pjtBuf->idxPktStored, pStream->pjtBuf->startingSeq, pStream->pjtBuf->sizePkts);

  for(count = 0; count < pStream->pjtBuf->sizePkts; count++) {

    if(++idxPkt >= pStream->pjtBuf->sizePkts) {
      idxPkt = 0;
    }

    fprintf(stderr, "[%d] seq:%d, ts:%u, len:%d, tv:%u.%u flag:0x%x %s%s  nack.tvLastNack:%u.%u", idxPkt, 
       pStream->pjtBuf->pPkts[idxPkt].pktData.u.rtp.seq,
       pStream->pjtBuf->pPkts[idxPkt].pktData.u.rtp.ts,
       PKTCAPLEN(pStream->pjtBuf->pPkts[idxPkt].pktData.payload),
       pStream->pjtBuf->pPkts[idxPkt].pktData.tv.tv_sec, pStream->pjtBuf->pPkts[idxPkt].pktData.tv.tv_usec,
       pStream->pjtBuf->pPkts[idxPkt].rcvFlag,
       pStream->pjtBuf->pPkts[idxPkt].rcvFlag & CAPTURE_JTBUF_FLAG_IN_STREAM ? "in-stream " : "", 
       pStream->pjtBuf->pPkts[idxPkt].rcvFlag & CAPTURE_JTBUF_FLAG_RX ? "rcvd " : "",
       pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack.tv_sec, pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack.tv_usec);
  fprintf(stderr, "\n");
  }

}
#endif // 0

#define NACK_RETRANSMIT_INTERVAL_MS    20

static int jtbuf_createNACKs(CAPTURE_STREAM_T *pStream, const struct timeval *ptv) {
  int rc = 0;
  unsigned int count, idxPkt;
  unsigned int seqNumOffset;
  int haveStartSeqnum = 0;
  uint16_t seqnum;
  uint16_t seqnumNackStart = 0;
  const struct timeval *ptvlastRcvd = NULL;
  uint32_t *plastRcvdTs = NULL;
  RTCP_PKT_RTPFB_NACK_T *pnackPkt = NULL;

  if(!pStream->pjtBuf || !pStream->pjtBuf->pPkts) {
    return -1;
  }

  ptvlastRcvd = &pStream->pjtBuf->pPkts[pStream->pjtBuf->idxPkt].pktData.tv;
  plastRcvdTs = &pStream->pjtBuf->pPkts[pStream->pjtBuf->idxPkt].pktData.u.rtp.ts;

  //memset(pStream->rtcpRR.fbs_nack, 0, sizeof(pStream->rtcpRR.fbs_nack));
  for(count = 0; count <= RTCP_NACK_PKTS_MAX; count++) {
    pStream->rtcpRR.fbs_nack[count].pid = 0;
    pStream->rtcpRR.fbs_nack[count].blp = 0;
  }
  pStream->rtcpRR.countNackPkts = 0;
  pnackPkt = &pStream->rtcpRR.fbs_nack[0];

  idxPkt = pStream->pjtBuf->idxPkt;

  //LOG(X_DEBUG("jtbuf_createNACK tv:%u.%u"), ptv->tv_sec, ptv->tv_usec);
  //fprintf(stderr, "jtbuf: idxPkt:%d, idxPktStored:%d, startingSeq:%d, sizePkts:%d\n", pStream->pjtBuf->idxPkt, pStream->pjtBuf->idxPktStored, pStream->pjtBuf->startingSeq, pStream->pjtBuf->sizePkts);
  seqnum = pStream->pjtBuf->startingSeq;

  for(count = 0; count < pStream->pjtBuf->sizePkts; count++) {

    if(++idxPkt >= pStream->pjtBuf->sizePkts) {
      idxPkt = 0;
    }

    seqnum++;

    //LOG(X_DEBUG("jtbuf_donack checking jtBuf[%d].rcvFlag:0x%x, seq:%d"), idxPkt, pStream->pjtBuf->pPkts[idxPkt].rcvFlag, seqnum);

    if(!(pStream->pjtBuf->pPkts[idxPkt].rcvFlag & (CAPTURE_JTBUF_FLAG_IN_STREAM))) {
      //LOG(X_DEBUG("jtbuf_donack jtBuf[%d] breaking"), idxPkt);

      //
      // The ringbuffer packet slot is invalid or was already processed
      //
      break;

    } else if((pStream->pjtBuf->pPkts[idxPkt].rcvFlag & (CAPTURE_JTBUF_FLAG_RX))) {
      //LOG(X_DEBUG("jtbuf_donack jtBuf[%d] continue cuz its been received"), idxPkt);

      //
      // The ringbuffer packet slot was received ok
      //
      ptvlastRcvd = &pStream->pjtBuf->pPkts[idxPkt].pktData.tv;
      plastRcvdTs = &pStream->pjtBuf->pPkts[idxPkt].pktData.u.rtp.ts;
      continue;

    } else if(pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack.tv_sec > 0 && 
              TIME_TV_DIFF_MS(*ptv, pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack) < NACK_RETRANSMIT_INTERVAL_MS) {
              //TIME_TV_DIFF_MS(*ptv, pStream->pjtBuf->pPkts[idxPkt].pktData.tv) < NACK_RETRANSMIT_INTERVAL_MS) {
      //LOG(X_DEBUG("jtbuf_donack jtBuf[%d] continue cuz its too frsh"), idxPkt,  TIME_TV_DIFF_MS(*ptv, pStream->pjtBuf->pPkts[idxPkt].pktData.tv));

      //
      // We had previously send a NACK for this sequence number within the NACK_RETRANSMIT_INTERVAL_MS threshold
      //
      continue;

    //} else if(plastRcvdTs && *plastRcvdTs == pStream->highestRcvdTs) {
    //  LOG(X_DEBUG("Not yet sending a NACK request for seq: %d since the packet may just be out of order"), seqnum);
    //  continue;
    } else if(TIME_TV_DIFF_MS(*ptv, *ptvlastRcvd) < 60) {
      //LOG(X_DEBUG("Not yet sending a NACK request for seq: %d since the packet may still be inbound (%d ms)"), seqnum, TIME_TV_DIFF_MS(*ptv, *ptvlastRcvd));
      continue;
    }

    if(!haveStartSeqnum) {

      //
      // This missing sequence number is the first packet of the RTCP NACK
      //
      pnackPkt->pid = htons(seqnum);
      haveStartSeqnum = 1;
      seqnumNackStart = seqnum;
      pStream->rtcpRR.countNackPkts++;
      //LOG(X_DEBUG("jtbuf_donack jtBuf[%d] set nack[%d].seqnum to %d"), idxPkt, pStream->rtcpRR.countNackPkts, seqnum);

    } else {

      if((seqNumOffset = seqnum - seqnumNackStart) > 16) {

        if(pStream->rtcpRR.countNackPkts >= RTCP_NACK_PKTS_MAX) {
          LOG(X_WARNING("Maximum number of %d RTCP NACK compound packets reached for rtp seq:%d."), RTCP_NACK_PKTS_MAX, seqnum);
          break;
        }

        //
        // This missing sequence number is the first packet of a compound RTCP NACK packet
        //
        pnackPkt = &pStream->rtcpRR.fbs_nack[pStream->rtcpRR.countNackPkts++];
        pnackPkt->pid = htons(seqnum);
        seqnumNackStart = seqnum;
        //LOG(X_DEBUG("jtbuf_donack jtBuf[%d] set nack[%d].seqnum to %d"), idxPkt, pStream->rtcpRR.countNackPkts, seqnum);

      } else if(seqNumOffset > 0) {

        //
        // This missing sequence number is a non-first packet of a RTCP NACK packet which is indicated by 
        // the 16bit bitmask field
        //
        pnackPkt->blp |= (1 << (seqNumOffset - 1));

        //LOG(X_DEBUG("jtbuf_donack jtBuf[%d] set nack[%d].blp bit with offset %d"), idxPkt, pStream->rtcpRR.countNackPkts, seqNumOffset);

      }
    }

    if(pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack.tv_sec == 0) {
      pStream->numSeqnoNacked++;
    }
    TIME_TV_SET(pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack, *ptv);
   
/*
    fprintf(stderr, "[%d] seq:%d, ts:%u, len:%d, tv:%u.%u flag:0x%x %s%s  ", idxPkt, 
       pStream->pjtBuf->pPkts[idxPkt].pktData.u.rtp.seq,
       pStream->pjtBuf->pPkts[idxPkt].pktData.u.rtp.ts,
       PKTCAPLEN(pStream->pjtBuf->pPkts[idxPkt].pktData.payload),
       pStream->pjtBuf->pPkts[idxPkt].pktData.tv.tv_sec, pStream->pjtBuf->pPkts[idxPkt].pktData.tv.tv_usec,
       pStream->pjtBuf->pPkts[idxPkt].rcvFlag,
       pStream->pjtBuf->pPkts[idxPkt].rcvFlag & CAPTURE_JTBUF_FLAG_IN_STREAM ? "in-stream " : "", 
       pStream->pjtBuf->pPkts[idxPkt].rcvFlag & CAPTURE_JTBUF_FLAG_RX ? "rcvd " : "");
  fprintf(stderr, "\n");
*/
  }

  for(count = 0; count < pStream->rtcpRR.countNackPkts; count++) {
    if(pStream->rtcpRR.fbs_nack[count].blp != 0) {
      pStream->rtcpRR.fbs_nack[count].blp = htons(pStream->rtcpRR.fbs_nack[count].blp);
    }

    LOG(X_DEBUG("Created RTCP NACK request for ssrc: 0x%x starting seq: %d, blp: 0x%x, jtbuf:[%d]/%d :seq:%d"), 
      pStream->hdr.key.ssrc, htons(pStream->rtcpRR.fbs_nack[count].pid), htons(pStream->rtcpRR.fbs_nack[count].blp),
      pStream->pjtBuf->idxPkt, pStream->pjtBuf->sizePkts, pStream->pjtBuf->startingSeq);
  }

  //if(pStream->pRembAbr) {
  //  capture_abr_notifyBitrate(pStream, NULL);
  //}

//pStream->rtcpRR.countNackPkts = 0;

  return rc;
}

static int capture_reorderPkt(CAPTURE_STATE_T *pState, 
                              CAPTURE_STREAM_T *pStream, 
                              const COLLECT_STREAM_PKT_T *pPkt) {
  unsigned short seq;
  int seqDelta = 0;
  int seqTmp;
  int seqRoll;
  int rc;
  unsigned int idxPkt = pStream->pjtBuf->idxPkt;
  unsigned int idxPktStore;
  CAPTURE_JTBUF_PKT_T *pJtBufPkt = &pStream->pjtBuf->pPkts[idxPkt];
  
  pStream->numBytesPayload += PKTWIRELEN(pPkt->data.payload);
  pStream->numBytes += (pPkt->data.u.rtp.marksz & PKTDATA_RTP_MASK_SZ);

  if(pStream->pRembAbr) {
    burstmeter_AddSample(&pStream->pRembAbr->payloadBitrate, PKTWIRELEN(pPkt->data.payload), &pPkt->data.tv);  
  }

  if(pStream->numPkts == 1) {  
    memcpy(&pJtBufPkt->pktData, &pPkt->data, sizeof(COLLECT_STREAM_PKTDATA_T));
    //LOG(X_DEBUG("---calling cbOnPkt(reorder:1) pStream 0x%x pUserD:0x%x, numPkts:%d,cbOnPkt:0x%x"), pStream, pStream->pCbUserData, pStream->numPkts, pStream->cbOnPkt);
    //pStream->highestRcvdTs = pJtBufPkt->pktData.u.rtp.ts;
    if(pStream->cbOnPkt) {
      pStream->cbOnPkt(pStream->pCbUserData, &pJtBufPkt->pktData);
    }
    pStream->pjtBuf->idxPktStored = idxPkt;
    pStream->pjtBuf->startingSeq = pPkt->data.u.rtp.seq;
    pStream->rtcpRR.pktcnt++;
    pStream->rtcpRR.rr.seqhighest = pPkt->data.u.rtp.seq;
    return 0;
  }
    
  seqRoll = DID_UINT16_ROLL(pPkt->data.u.rtp.seq, pJtBufPkt->pktData.u.rtp.seq);
  if(seqRoll) {
    pStream->rtcpRR.seqRolls++; 
  }
  pStream->rtcpRR.rr.seqhighest = pPkt->data.u.rtp.seq;

  seqDelta = seqTmp = RTP_SEQ_DELTA(pPkt->data.u.rtp.seq, pJtBufPkt->pktData.u.rtp.seq);

//  if(pPkt->hdr.payloadType==100) LOG(X_DEBUG("rtp pt:%d, ssrc:0x%x, ts:%d seq:%d delta:%d tsdelta:%d/%d, seqDelta:%d"), pPkt->hdr.payloadType, pPkt->hdr.key.ssrc, pPkt->data.u.rtp.ts, pPkt->data.u.rtp.seq, seqDelta,RTP_TS_DELTA(pPkt->data.u.rtp.ts, pJtBufPkt->pktData.u.rtp.ts), pStream->clockHz, seqDelta);

  if(seqDelta < 0) {

    if(pPkt->data.u.rtp.seq < pStream->pjtBuf->startingSeq) {
      LOG(X_WARNING("Late %spacket will be discarded seq: %d < seq: %d, ssrc: 0x%x, ts: %u"), 
          CAPTURE_STREAM_TYPESTR(pState, pStream),
          pPkt->data.u.rtp.seq, pJtBufPkt->pktData.u.rtp.seq, pPkt->hdr.key.ssrc, pPkt->data.u.rtp.ts);
      return 0;
    }

  } else if(seqDelta > (int) pStream->pjtBuf->sizePkts) {
    seqTmp = pStream->pjtBuf->sizePkts;
  }
    
  for(seq = 0; seq < seqTmp; seq++) {
    if(++idxPkt >= pStream->pjtBuf->sizePkts) {
      idxPkt = 0;
    }
    if(seq < seqTmp - 1 && !(pStream->pjtBuf->pPkts[idxPkt].rcvFlag & CAPTURE_JTBUF_FLAG_IN_STREAM)) {
      pStream->pjtBuf->pPkts[idxPkt].rcvFlag |= CAPTURE_JTBUF_FLAG_IN_STREAM;
      pStream->pjtBuf->pPkts[idxPkt].nack.tvLastNack.tv_sec = 0;
    }
  }

  idxPktStore = idxPkt;

  if(seqDelta >= (int) pStream->pjtBuf->sizePkts || 
     (seqDelta > 1 && pStream->clockHz > 0 && pStream->maxRtpGapWaitTmMs > 0 && 
    (float) (1000.0 * RTP_TS_DELTA(pPkt->data.u.rtp.ts, pJtBufPkt->pktData.u.rtp.ts) / pStream->clockHz) > 
    (float) pStream->maxRtpGapWaitTmMs)) { 

    LOG(X_WARNING("Flushing %s RTP jitter buffer at seq:%d (%d/%d) after waiting %.2f(/%u) ms"), 
        IS_CAP_FILTER_AUD(pStream->pFilter->mediaType) ? "audio" : "video",
        pPkt->data.u.rtp.seq, seqDelta, pStream->pjtBuf->sizePkts, 
        pStream->clockHz > 0 ? (float) (1000.0 * RTP_TS_DELTA(pPkt->data.u.rtp.ts, 
     pJtBufPkt->pktData.u.rtp.ts) / pStream->clockHz) : 0, pStream->maxRtpGapWaitTmMs);

    //TODO: when flushign w/ NACK, upon iterating if encountering a loss gap < expiration time, do not flush entire jtbuf, because those may need to be nacked still
    //
    // If this input stream supports NACKs, then finish flushing the jitter buffer upon encountering a reception 
    // gap within a threshold of the maxRtpGapWaitTmMs.  This is to allow possible retransmissions to reach
    // us when there are multiple lost gaps within the jitter buffer.
    //
    rc = capture_flushJtBuf(pState, pStream, ((pStream->pFilter->enableNack &&
                                              seqDelta < (int) pStream->pjtBuf->sizePkts) ? pPkt : NULL));

    if(rc + 1 >= pStream->pjtBuf->sizePkts) {
      rc = seqDelta - 1;
    } 
    LOG(X_WARNING("Detected RTP %d packets lost pt:%d at seq:%u, ts:%u"), rc, 
        pPkt->hdr.payloadType, pJtBufPkt->pktData.u.rtp.seq, pJtBufPkt->pktData.u.rtp.ts);
    pStream->rtcpRR.pktdropcnt += rc;
    if(pStream->pjtBuf->flushedAll) {
      idxPktStore = idxPkt = 0;
      pStream->pjtBuf->idxPkt = pStream->pjtBuf->sizePkts - 1;
    } else {
      idxPktStore = idxPkt;
      idxPkt = pStream->pjtBuf->idxPkt;
      // preserve idxPkt and pStream->pjtBuf->idxPkt, pStream->pjtBuf->idxPktStored
    }
    //LOG(X_DEBUG("cbOnPkt(flush)... after flushedAll:%d, flushing rc: %d, idxPkt:%d, idxPktStore:%d, seqDelta:%d, pjtBuf->idxPkt:%d, pjtBuf->idxPktStored:%d"), pStream->pjtBuf->flushedAll, rc, idxPkt, idxPktStore, seqDelta, pStream->pjtBuf->idxPkt, pStream->pjtBuf->idxPktStored);    

  //} else if(seqDelta > 1 && pStream->pFilter->enableNack > 0) {
  //  jtbuf_createNACKs(pStream, &pPkt->data.tv);
  }
  
  memcpy(&pStream->pjtBuf->pPkts[idxPktStore].pktData, &pPkt->data, 
         sizeof(COLLECT_STREAM_PKTDATA_T));
  pStream->pjtBuf->pPkts[idxPktStore].rcvFlag = CAPTURE_JTBUF_FLAG_RX | CAPTURE_JTBUF_FLAG_IN_STREAM;
  pStream->pjtBuf->pPkts[idxPktStore].nack.tvLastNack.tv_sec = 0;
  pStream->pjtBuf->idxPktStored = idxPktStore;

  if(seqDelta > 1 && pStream->pFilter->enableNack > 0) {
    //dump_jtbuf(pStream);
    jtbuf_createNACKs(pStream, &pPkt->data.tv);
  }

  //if(pStream->pFilter->nackFbSize) dump_jtbuf(pStream);

  if((idxPkt = pStream->pjtBuf->idxPkt + 1) >= pStream->pjtBuf->sizePkts) {
      idxPkt = 0;
  }

  while((pStream->pjtBuf->pPkts[idxPkt].rcvFlag & CAPTURE_JTBUF_FLAG_RX)) {

    pJtBufPkt = &pStream->pjtBuf->pPkts[idxPkt];
    //pStream->highestRcvdTs = pJtBufPkt->pktData.u.rtp.ts;

    if(pStream->cbOnPkt) {

      capture_calc_jitter(pJtBufPkt, pStream);

      //LOG(X_DEBUG("---calling cbOnPkt(reorder:2) seq:%u, sz:%u rtp ts:%u caplen:%d len:%d, tm:%u,%u, %uHz, numPkts:%d"),pJtBufPkt->pktData.u.rtp.seq, (pJtBufPkt->pktData.u.rtp.marksz & PKTDATA_RTP_MASK_SZ), pJtBufPkt->pktData.u.rtp.ts, PKTCAPLEN(pJtBufPkt->pktData.payload), PKTWIRELEN(pJtBufPkt->pktData.payload), pJtBufPkt->pktData.tv.tv_sec, pJtBufPkt->pktData.tv.tv_usec, pStream->clockHz, pStream->numPkts);

      pStream->cbOnPkt(pStream->pCbUserData, &(pJtBufPkt->pktData));
    }
    pStream->pjtBuf->startingSeq = pJtBufPkt->pktData.u.rtp.seq;
    pStream->rtcpRR.pktcnt++;

    if(pJtBufPkt->pPoolBuf) {
      jtbufpool_put(&pStream->pjtBuf->pool, pJtBufPkt->pPoolBuf);
      pJtBufPkt->pPoolBuf = NULL;
    }
    pJtBufPkt->rcvFlag = 0;
    pJtBufPkt->nack.tvLastNack.tv_sec = 0;
    pStream->pjtBuf->idxPkt = idxPkt;

    if(++idxPkt >= pStream->pjtBuf->sizePkts) {
      idxPkt = 0;
    }

  }

  //
  // Need to preserve the packet payload since this (future) packet is being kept around
  // to preserve sequence order
  //
  if(pStream->pjtBuf->pPkts[pStream->pjtBuf->idxPktStored].rcvFlag != 0) {

    pJtBufPkt = &pStream->pjtBuf->pPkts[pStream->pjtBuf->idxPktStored];

    if(PKTCAPLEN(pJtBufPkt->pktData.payload) > pStream->pjtBuf->pool.entryBufSz) {
      LOG(X_WARNING("Payload payload too large for jitter buffer %d > %d"), 
               PKTCAPLEN(pJtBufPkt->pktData.payload), pStream->pjtBuf->pool.entryBufSz);
      return 0;
    } else if(pJtBufPkt->pPoolBuf == NULL) {
      if((pJtBufPkt->pPoolBuf = jtbufpool_get(&pStream->pjtBuf->pool)) == NULL) {
        LOG(X_ERROR("Failed to get reordering packet buffer from pool of size %d/%d."), 
                    pStream->pjtBuf->pool.numAllocEntries, pStream->pjtBuf->pool.maxEntries);
        return -1;
      }
    }
    memcpy(pJtBufPkt->pPoolBuf->pBuf, pJtBufPkt->pktData.payload.pData, 
          PKTCAPLEN(pJtBufPkt->pktData.payload));
    pJtBufPkt->pktData.payload.pData = pJtBufPkt->pPoolBuf->pBuf;
    pJtBufPkt->pktData.payload.pRtp = NULL;
   
  }

  return 0;
}

static int capture_flushJtBuf(CAPTURE_STATE_T *pState, CAPTURE_STREAM_T *pStream, const COLLECT_STREAM_PKT_T *pPkt) {

  unsigned int count;
  CAPTURE_JTBUF_PKT_T *pJtBufPkt = NULL;
  COLLECT_STREAM_PKTDATA_T pkt;
  int numlost = 0;
  int stopFlushingOnGap = -1;
  float maxRtpGapWaitTmMs = 0;

  if(pPkt && pStream->clockHz > 0 && pStream->maxRtpGapWaitTmMs > 0) {
    maxRtpGapWaitTmMs = (float) pStream->maxRtpGapWaitTmMs - (.2f * (float) pStream->maxRtpGapWaitTmMs);
  }

  for(count = 0; count < pStream->pjtBuf->sizePkts; count++) {

    if(++pStream->pjtBuf->idxPkt >= pStream->pjtBuf->sizePkts) {
      pStream->pjtBuf->idxPkt = 0;
    }
    pJtBufPkt = &pStream->pjtBuf->pPkts[pStream->pjtBuf->idxPkt];

    if(pJtBufPkt->rcvFlag & CAPTURE_JTBUF_FLAG_IN_STREAM) {

      //pStream->highestRcvdTs = pJtBufPkt->pktData.u.rtp.ts;

      if(pJtBufPkt->rcvFlag & CAPTURE_JTBUF_FLAG_RX) {

        if(pStream->cbOnPkt) {
          //LOG(X_DEBUG("---calling cbOnPkt(flush) seq:%u, sz:%u rtp ts:%u caplen:%d len:%d"),pJtBufPkt->pktData.u.rtp.seq, (pJtBufPkt->pktData.u.rtp.marksz & PKTDATA_RTP_MASK_SZ), pJtBufPkt->pktData.u.rtp.ts, PKTCAPLEN(pJtBufPkt->pktData.payload), PKTWIRELEN(pJtBufPkt->pktData.payload));

          pStream->cbOnPkt(pStream->pCbUserData, &pJtBufPkt->pktData);
        }
        pStream->pjtBuf->startingSeq = pJtBufPkt->pktData.u.rtp.seq;
        pStream->rtcpRR.pktcnt++;
        if(stopFlushingOnGap == 0 && maxRtpGapWaitTmMs > 0.0f &&
           (float) (1000.0 * RTP_TS_DELTA(pPkt->data.u.rtp.ts, pJtBufPkt->pktData.u.rtp.ts) / pStream->clockHz) <
            maxRtpGapWaitTmMs) {
          stopFlushingOnGap = 1;
        }

      } else {

        if(stopFlushingOnGap == -1) {
          stopFlushingOnGap = 0;
        } if(stopFlushingOnGap == 1) {
          stopFlushingOnGap = 2;
          if(pStream->pjtBuf->idxPkt > 0) {
            pStream->pjtBuf->idxPkt--;
          } else {
            pStream->pjtBuf->idxPkt = pStream->pjtBuf->sizePkts - 1;
          }
          //LOG(X_DEBUG("---not calling cbOnPkt(flush) pJtBuf->idxPkt back to: %d"), pStream->pjtBuf->idxPkt);
          break;
        }

        numlost++;
        if(pStream->cbOnPkt) {
          memset(&pkt, 0, sizeof(pkt));
          //LOG(X_DEBUG("---calling cbOnPkt(flush) FLAG_XR not set"));
          pStream->cbOnPkt(pStream->pCbUserData, &pkt);
        }
      }

      pJtBufPkt->rcvFlag = 0;        
      pJtBufPkt->nack.tvLastNack.tv_sec = 0;
      if(pJtBufPkt->pPoolBuf) {
        jtbufpool_put(&pStream->pjtBuf->pool, pJtBufPkt->pPoolBuf);
        pJtBufPkt->pPoolBuf = NULL;
      }
    }

  } 

  if(stopFlushingOnGap != 2) {
    pStream->pjtBuf->idxPkt = 0;
    pStream->pjtBuf->flushedAll = 1;
  } else {
    pStream->pjtBuf->flushedAll = 0;
  }

  return numlost;
}

int rtp_captureFlush(CAPTURE_STATE_T *pState) {
  unsigned int idx;
  for(idx = 0; idx < pState->maxStreams; idx++) {

    capture_flushJtBuf(pState, &pState->pStreams[idx], NULL);
  }
  return 0;
}

int capture_processTcpRaw(CAPTURE_STATE_T *pState, 
                          const COLLECT_STREAM_PKT_T *pPkt,
                          const CAPTURE_FILTER_T *pFilter) {
  CAPTURE_STREAM_T *pStream = NULL;

  pthread_mutex_lock(&pState->mutexStreams);

  if((pStream = capture_rtp_lookup(pState, &pPkt->hdr.key)) == NULL) {
    if((pStream = add_stream(pState, pPkt, pFilter)) == NULL) {
      pthread_mutex_unlock(&pState->mutexStreams);
      return -1;
    }

    pStream->pFilter = (CAPTURE_FILTER_T *) pFilter;
    if(pState->cbOnStreamAdd) {
      pState->cbOnStreamAdd(pState->pCbUserData, pStream, &pPkt->hdr, NULL);
    }
  }

  pthread_mutex_unlock(&pState->mutexStreams);

  pStream->numPkts++;
  pStream->tmLastPkt = ((TIME_VAL)pPkt->data.tv.tv_sec * TIME_VAL_US) + 
                       pPkt->data.tv.tv_usec;

  if(pStream->cbOnPkt) {
    //TODO: need to do tcp resequencing

    //fprintf(stderr, "tcp seq:%u len:%d (%u)\n", pPkt->data.u.tcp.seq, pPkt->data.payload.caplen, pPkt->data.tv.tv_sec);

    if(((CAPTURE_CBDATA_SP_T *)pStream->pCbUserData)->lastPktSeq > pPkt->data.u.tcp.seq) {
      LOG(X_WARNING("Dropping tcp (retransmitted?) seq:%u"), pPkt->data.u.tcp.seq);
      return 0;
    }
    ((CAPTURE_CBDATA_SP_T *)pStream->pCbUserData)->lastPktSeq = pPkt->data.u.tcp.seq;

    pStream->cbOnPkt(pStream->pCbUserData, &pPkt->data);
  }
  return 0;
}

int capture_processUdpRaw(CAPTURE_STATE_T *pState, 
                          const COLLECT_STREAM_PKT_T *pPkt,
                          const CAPTURE_FILTER_T *pFilter) {
  CAPTURE_STREAM_T *pStream = NULL;

  pthread_mutex_lock(&pState->mutexStreams);

  pState->numUdprawPkts++;

  if((pStream = capture_rtp_lookup(pState, &pPkt->hdr.key)) == NULL) {
    if((pStream = add_stream(pState, pPkt, pFilter)) == NULL) {
      pthread_mutex_unlock(&pState->mutexStreams);
      return -1;
    }

    pStream->pFilter = (CAPTURE_FILTER_T *) pFilter;
    if(pState->cbOnStreamAdd) {
      pState->cbOnStreamAdd(pState->pCbUserData, pStream, &pPkt->hdr, NULL);
    }
    //fprintf(stderr, "new udp stream %s filter:0x%x\n", pStream->strSrcDst, pFilter);

  }

  pthread_mutex_unlock(&pState->mutexStreams);

  pStream->numPkts++;
  pStream->tmLastPkt = ((TIME_VAL)pPkt->data.tv.tv_sec * TIME_VAL_US) + 
                       pPkt->data.tv.tv_usec;

  if(pStream->cbOnPkt) {
    pStream->cbOnPkt(pStream->pCbUserData, &pPkt->data);
  }
  return 0;
}

const CAPTURE_STREAM_T *capture_processRtp(CAPTURE_STATE_T *pState, 
                                           const COLLECT_STREAM_PKT_T *pPkt,
                                           const CAPTURE_FILTER_T *pFilter) {

  CAPTURE_STREAM_T *pStream = NULL;
  COLLECT_STREAM_PKTDATA_T *pPktDataLast = NULL;  // unordered, last rx packet
  SRTP_CTXT_T *pSrtp = NULL;
  int seqDelta;
  int tsDelta;
  int rc;

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - rtp-recv tv:%lu:%u ssrc:0x%x, pt:%u, seq:%u, mark:%u rtp ts:%u, len:%d, numPkts:%llu"), 
            pPkt->data.tv.tv_sec, pPkt->data.tv.tv_usec, pPkt->hdr.key.ssrc,pPkt->hdr.payloadType,
            pPkt->data.u.rtp.seq, (pPkt->data.u.rtp.marksz & PKTDATA_RTP_MASK_SZ)?1:0, pPkt->data.u.rtp.ts, 
            PKTCAPLEN(pPkt->data.payload), pState->numRtpPkts); 
    if(pPkt) {
      LOGHEX_DEBUG(&pPkt->hdr.key, pPkt->hdr.key.lenKey); 
    }
  );

#if defined(DROP_RTP)
  static int rcv_seq0_v, rcv_seq0_a;
  static int rcv_tot_v, rcv_tot_a, rcv_drop_tot_v, rcv_drop_tot_a, rcv_drop_cnt_v, rcv_drop_cnt_a;
  static float pkt_drop_percent_v, pkt_drop_percent_a;
  int dropped_v=0, dropped_a=0;
  if(1&&((pPkt->hdr.payloadType & RTP_PT_MASK) == 100 || (pPkt->hdr.payloadType & RTP_PT_MASK) == 112 ||
      (pPkt->hdr.payloadType & RTP_PT_MASK) == 120)) {
    if(rcv_seq0_v==0) rcv_seq0_v = pPkt->data.u.rtp.seq;
//if(pPkt->data.u.rtp.seq-rcv_seq0_v <700) {
    if(rcv_tot_v++ % 40 == 10)rcv_drop_cnt_v=(random()%2 + 1);
    if(rcv_drop_cnt_v>0){ rcv_drop_cnt_v--; rcv_drop_tot_v++; dropped_v=1; pkt_drop_percent_v=(float)rcv_drop_tot_v/rcv_tot_v;}
//}
  } else if((pPkt->hdr.payloadType & RTP_PT_MASK) == 111) {
    if(rcv_seq0_a==0) rcv_seq0_a = pPkt->data.u.rtp.seq;
    if(rcv_tot_a++ % 30 == 10)rcv_drop_cnt_a=(random()%3 + 1);
    if(rcv_drop_cnt_a>0){ rcv_drop_cnt_a--; rcv_drop_tot_a++; dropped_a=1; pkt_drop_percent_a=(float)rcv_drop_tot_a/rcv_tot_a;}

  }
  if(dropped_v||dropped_a) { LOG(X_DEBUG("DROP CAPTURE pt:%d, seq:%u (%d), ts:%u, vid-loss:%.2f%%, aud-loss;%.2f%%"),  (pPkt->hdr.payloadType & RTP_PT_MASK),  pPkt->data.u.rtp.seq, pPkt->data.u.rtp.seq - (dropped_v ? rcv_seq0_v : rcv_seq0_a), pPkt->data.u.rtp.ts, pkt_drop_percent_v *100.0f, pkt_drop_percent_a * 100.0f); return NULL;}
#endif // (DROP_RTP)
  
  pthread_mutex_lock(&pState->mutexStreams);

  pState->numRtpPkts++;

  //
  // Lookup the stream with the computed key
  //
  if(!(pStream = capture_rtp_lookup(pState, &pPkt->hdr.key))) {

    if(!(pStream = add_stream(pState, pPkt, pFilter))) {
      pthread_mutex_unlock(&pState->mutexStreams);
      return NULL;
    }

    if(!pStream->pjtBuf || !pStream->pjtBuf->pPkts) {
      pthread_mutex_unlock(&pState->mutexStreams);
      return NULL;
    }

    pStream->pFilter = (CAPTURE_FILTER_T *) pFilter;
    pStream->id = pPkt->hdr.key.ssrc;  
    if(pFilter && codectype_isVid(pStream->pFilter->mediaType)) {
      pStream->fir_send_intervalms = FIR_INTERVAL_MS_XMIT_RTCP;
      pStream->appremb_send_intervalms = APPREMB_INTERVAL_MS_XMIT_RTCP;
      capture_abr_init(&pState->rembAbr);
      pStream->pRembAbr = &pState->rembAbr;
    }
   
    //
    // Initialize SRTP reception processing for the SRTP RTP context
    //      
    if(pFilter->srtps[0].kt.k.lenKey > 0) {

      LOG(X_DEBUG("Initializing SRTP capture context key length:%d (0x%x, 0x%x)"), 
         pFilter->srtps[0].kt.k.lenKey, &pFilter->srtps[0], pFilter->srtps[0].pSrtpShared);

      if(srtp_initInputStream((SRTP_CTXT_T *) &pFilter->srtps[0], htonl(pPkt->hdr.key.ssrc), 1) < 0) {
        pthread_mutex_unlock(&pState->mutexStreams);
        return NULL;
      }
    }
/*
    //... the following is done in capture_socket since rtcp can arrive before the first rtp packet
    //
    // If we're using SRTP-DTLS, and the DTLS handshake has assigned unique keys to the RTCP context (non rtcp-mux case) then
    // we should init a unique SRTP RTCP context for unprotecting incoming RTCP packets
    //
    if(pFilter->srtps[1].pSrtpShared && pFilter->srtps[1].pSrtpShared == &pFilter->srtps[1] && pFilter->srtps[1].kt.k.lenKey > 0) {
      LOG(X_DEBUG("Initializing SRTP RTCP capture context key length:%d (0x%x, 0x%x)"), pFilter->srtps[1].kt.k.lenKey, &pFilter->srtps[1], pFilter->srtps[1].pSrtpShared);
      if(srtp_initInputStream((SRTP_CTXT_T *) &pFilter->srtps[1], htonl(pPkt->hdr.key.ssrc), 1) < 0) {
        pthread_mutex_unlock(&pState->mutexStreams);
        return NULL;
      }
    }
*/

    if(pState->cbOnStreamAdd) {
      pState->cbOnStreamAdd(pState->pCbUserData, pStream, &pPkt->hdr, NULL);
    }

    //fprintf(stderr, "new rtp stream %s filter:0x%x, ssrc:0x%x\n", pStream->strSrcDst, pFilter, pPkt->hdr.key.ssrc);
  }

  pthread_mutex_unlock(&pState->mutexStreams);

  //fprintf(stderr, "pStream: 0x%x pFilter: 0x%x numPkts:%llu, rtp-seq:%d, ssrc: 0x%x\n", pStream, pStream->pFilter, pStream->numPkts, pPkt->data.u.rtp.seq, pPkt->hdr.key.ssrc);

  pStream->numPkts++;
  pStream->tmLastPkt = ((TIME_VAL)pPkt->data.tv.tv_sec * TIME_VAL_US) + pPkt->data.tv.tv_usec;

  if(pStream->numPkts > 1) {

    pPktDataLast  = &pStream->pjtBuf->pPkts[pStream->pjtBuf->idxPktStored].pktData;
    seqDelta = RTP_SEQ_DELTA(pPkt->data.u.rtp.seq, pPktDataLast->u.rtp.seq);

    //pStream->lastRcvdTs = pPktDataLast->u.rtp.ts;

    //
    // Auto validate RTP type stream by checking correct sequence # increment
    //
    if(pStream->numSeqIncrOk < RTP_SEQ_INCR_MIN) {

      if(seqDelta == 1) {
        pStream->numSeqIncrOk++;
      } else {
        pStream->numSeqIncrInvalid++;
      }

      if(seqDelta < 0 || pStream->numSeqIncrInvalid > 2) {

        LOG(X_ERROR("Looks like an invalid RTP stream pt:%d, ssrc:0x%x, seq:%u (delta:%d), rtp-ts:%u"),
                  pPkt->hdr.payloadType, pPkt->hdr.key.ssrc, pPkt->data.u.rtp.seq, seqDelta, pPkt->data.u.rtp.ts);

        capture_delete_stream(pState, pStream);
        return pStream;
      }
    }

    if(seqDelta == 0) {

      LOG(X_WARNING("RTP duplicate packet at seq:%u, ts:%u, srrc: 0x%x"), 
          pPkt->data.u.rtp.seq, pPkt->data.u.rtp.ts, pPkt->hdr.key.ssrc);
      return pStream;

    } else if(seqDelta < 1) {


      LOG(X_WARNING("RTP out of order reception gap seq:%u, ts:%u, srrc: 0x%x, gap:%d"), 
          pPkt->data.u.rtp.seq, pPkt->data.u.rtp.ts, pPkt->hdr.key.ssrc,
         -1 * RTP_SEQ_DELTA(pPktDataLast->u.rtp.seq,  pPkt->data.u.rtp.seq));

      //dump_jtbuf(pStream);
    }

    if(seqDelta == 1 && pPkt->data.u.rtp.ts != pPktDataLast->u.rtp.ts) {

      //
      // Auto compute the Hz increment for new frames
      //
      if(pStream->frameDeltaHz == 0) {
        tsDelta = RTP_TS_DELTA(pPkt->data.u.rtp.ts, pPktDataLast->u.rtp.ts);
        if(tsDelta > 0) {
          pStream->frameDeltaHz = tsDelta;
        }
      }
      
      if(pPkt->data.u.rtp.ts < UINT32_ROLL_MIN && pPktDataLast->u.rtp.ts > UINT32_ROLL_MAX) {
        pStream->tsRolls++;
      } else if(pPkt->data.u.rtp.ts > UINT32_ROLL_MAX && pPktDataLast->u.rtp.ts < UINT32_ROLL_MIN) {
        // Possible w/ out of order reception, shortly after ts rolled
        pStream->tsRolls--;
      }

      pStream->lastTsChangeTv.tv_sec = pPkt->data.tv.tv_sec;
      pStream->lastTsChangeTv.tv_usec = pPkt->data.tv.tv_usec;

    }

    //
    // Check RTP payload type consitency
    //
    if((pPkt->hdr.payloadType & RTP_PT_MASK) != pStream->hdr.payloadType) {

      //
      // Check if the payload type is for a telephone-event notification such as DTMF
      //
      if(pStream->pFilter && pStream->pFilter->telephoneEvt.available &&
         (pPkt->hdr.payloadType & RTP_PT_MASK) == pStream->pFilter->telephoneEvt.payloadType) {
        
        LOG(X_DEBUG("RTP %s payload type %d (stream payload type %d)"), pStream->pFilter->telephoneEvt.encodingName, 
            pPkt->hdr.payloadType, pStream->hdr.payloadType);
        ((COLLECT_STREAM_PKT_T *) pPkt)->data.flags |= PKT_FLAGS_DTMF2833;
      } else {
        LOG(X_WARNING("RTP payload type change %d -> %d"), pStream->hdr.payloadType, pPkt->hdr.payloadType);
        return NULL;
      }

    }

  } // end of   if(pStream->numPkts > 1) 

  //
  // SRTP decrypt the RTP packet
  //
  pSrtp = CAPTURE_RTP_SRTP_CTXT(pFilter);
  if(pPkt->pnetsock && (pPkt->pnetsock->flags & NETIO_FLAG_SRTP) && pSrtp->kt.k.lenKey == 0) {
    LOG(X_ERROR("Capture SRTP input key not set for RTP packet length: %d"), PKTCAPLEN(pPkt->data.payload));
    LOG(X_DEBUG("SRTP capture context key length:%d (0x%x, 0x%x)"), pFilter->srtps[0].kt.k.lenKey, &pFilter->srtps[0], pFilter->srtps[0].pSrtpShared);
    return NULL;
  } else if(pSrtp->kt.k.lenKey > 0 && pPkt->data.payload.pRtp) {
    unsigned int rtpHdrLen = pPkt->data.payload.pData - pPkt->data.payload.pRtp;
    unsigned int srtpLen = rtpHdrLen + PKTCAPLEN(pPkt->data.payload);
    int capLenDelta = PKTWIRELEN(pPkt->data.payload) - PKTCAPLEN(pPkt->data.payload);
    //fprintf(stderr, "SRTP RCV (before) keylen:%d, caplen:%d, len:%d, ssrc:0x%x, rtpHdrLen:%d, srtpLen:%d, capLenDelta:%d\n", pFilter->srtp.kt.k.lenKey, pPkt->data.payload.ul.l.caplen, pPkt->data.payload.ul.l.len, pPkt->hdr.key.ssrc, rtpHdrLen, srtpLen, capLenDelta);
    if((rc = srtp_decryptPacket(pSrtp, pPkt->data.payload.pRtp, &srtpLen, 0)) < 0) {
      return NULL;
    } else if(rc >= rtpHdrLen) {
      PKTCAPLEN(((COLLECT_STREAM_PKT_T * ) pPkt)->data.payload) = rc - rtpHdrLen;
      if(capLenDelta > 0) {
        PKTWIRELEN(((COLLECT_STREAM_PKT_T *) pPkt)->data.payload) = PKTCAPLEN(pPkt->data.payload) + capLenDelta;
      }
    }
    //fprintf(stderr, "SRTP RCV keylen:%d, caplen:%d, len:%d, ssrc:0x%x\n", pSrtp->kt.k.lenKey, pPkt->data.payload.ul.l.caplen, pPkt->data.payload.ul.l.len, pPkt->hdr.key.ssrc);
  }

  if(capture_reorderPkt(pState, pStream, pPkt) < 0) {
    return NULL;
  }

  return pStream;
}

int capture_decodeUdpRaw(const struct ether_header *peth,
                         const struct ip *pip,
                         const struct udphdr *pudp,
                         const struct pcap_pkthdr *pkthdr,
                         COLLECT_STREAM_PKT_T *pkt) {

  const struct ip6_hdr *pip6 = (const struct ip6_hdr *) pip;


  pkt->hdr.key.srcPort = pudp->source;
  pkt->hdr.key.dstPort = pudp->dest;

  if(pkt->hdr.key.dstPort < RTP_MIN_PORT || pkt->hdr.key.srcPort < RTP_MIN_PORT) {
    return -1;
  } 

  pkt->hdr.key.ssrc = 0;
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
  }

  pkt->hdr.extCodec = 0;
  pkt->data.u.rtp.marksz = (pkthdr->len & PKTDATA_RTP_MASK_SZ);
  PKTCAPLEN(pkt->data.payload) = (unsigned short) pkthdr->caplen - 
              ((((const unsigned char *)pudp) - (const unsigned char *)peth) + 8);
  PKTWIRELEN(pkt->data.payload) = PKTCAPLEN(pkt->data.payload) + (pkthdr->len - pkthdr->caplen);
  pkt->data.payload.pData = (((unsigned char *) pudp) + 8);
  pkt->data.payload.pRtp = NULL;
  pkt->data.tv.tv_sec = pkthdr->ts.tv_sec;
  pkt->data.tv.tv_usec = pkthdr->ts.tv_usec;

  return 0;
}

int capture_decodeRtp(const struct ether_header *peth,
                              const struct ip *pip,
                              const struct udphdr *pudp,
                              const rtp_t *rtp,
                              const struct pcap_pkthdr *pkthdr,
                              COLLECT_STREAM_PKT_T *pkt) {

  uint32_t rtp_hdr;
  const struct ip6_hdr *pip6 = (const struct ip6_hdr *) pip;
  uint16_t marked_hdr = 0;
  uint16_t rtpHdrLen = RTP_HEADER_LEN;
  int rtpCapLen;

  if((rtpCapLen = pkthdr->caplen - (  (((const unsigned char *)pudp) - 
                         (const unsigned char *)peth) + 8)) < RTP_HEADER_LEN) {
    return -1;
  }

  pkt->hdr.key.srcPort = pudp->source;
  pkt->hdr.key.dstPort = pudp->dest;

  if(pkt->hdr.key.dstPort < RTP_MIN_PORT || pkt->hdr.key.srcPort < RTP_MIN_PORT) {
    VSX_DEBUG_RTP( LOG(X_ERROR("Bad RTP srcport:%d, dstport:%d"), pkt->hdr.key.srcPort, pkt->hdr.key.dstPort) );
    return -1;
  } 

  rtp_hdr = ntohl(rtp->header);
  if(((rtp_hdr & RTPHDR32_VERSION_MASK) >> 30) != RTP_VERSION) {
    VSX_DEBUG_RTP( LOG(X_ERROR("Bad RTP Version 0x%x"), rtp_hdr >> 24) );
    return -1;
  }

  if(rtp_hdr & RTPHDR32_CSRC_MASK) {
    rtpHdrLen += (uint16_t) (4 * ntohl(rtp_hdr & RTPHDR32_CSRC_MASK));
    if(rtpCapLen < rtpHdrLen) {
      VSX_DEBUG_RTP( LOG(X_ERROR("Bad RTP capture length:%d %d < RTP header length: %d"), rtpCapLen, rtpHdrLen) );
      return -1;
    }
  }

  //
  // Skip RTP header extensions 
  //
  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |      0xBE     |      0xDE     |            length=2           |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |  ID   | len=6 |                RTT                            |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                     send timestamp  (t_i)                     |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // Figure 1: RTP Sender Header Extension Block
  //
  //
  if(rtp_hdr & RTPHDR32_EXT_MASK) {
    rtpHdrLen += (4 + (4 * ntohs(*((uint16_t *)(((char *) rtp) + rtpHdrLen + 2)))));
  }

  pkt->hdr.payloadType = (rtp_hdr & RTPHDR32_TYPE_MASK) >> 16;

  if(pkt->hdr.payloadType < 96 || pkt->hdr.payloadType > 127) {
    // Automatically Accept dynamic payload types

    switch(pkt->hdr.payloadType) {
      case RTP_PAYLOAD_TYPE_PCMU:
      case RTP_PAYLOAD_TYPE_PCMA:
      case RTP_PAYLOAD_TYPE_G722:
      case RTP_PAYLOAD_TYPE_JPG:
      case RTP_PAYLOAD_TYPE_MPV:
      case RTP_PAYLOAD_TYPE_MP2T:
      case RTP_PAYLOAD_TYPE_H261:
      case RTP_PAYLOAD_TYPE_H263:
        break;
      default:
        VSX_DEBUG_RTP(LOG(X_ERROR("Bad RTP payload type: %d"), pkt->hdr.payloadType) );
        return -1;
    }
  }

  marked_hdr = (uint16_t) ((rtp_hdr & RTPHDR32_MARKED_MASK) >> 23);
  pkt->data.u.rtp.seq = (uint16_t) (rtp_hdr & RTPHDR32_SEQ_NUM_MASK);
  pkt->data.u.rtp.ts = ntohl(rtp->timeStamp);
  pkt->hdr.key.ssrc = ntohl(rtp->ssrc);

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
  }

  pkt->hdr.extCodec = 0;
  pkt->data.u.rtp.marksz = (marked_hdr << 15) | (pkthdr->len & PKTDATA_RTP_MASK_SZ);
  PKTCAPLEN(pkt->data.payload) = (uint16_t) (rtpCapLen - rtpHdrLen);
  PKTWIRELEN(pkt->data.payload) = PKTCAPLEN(pkt->data.payload) + (pkthdr->len - pkthdr->caplen);
  pkt->data.payload.pRtp = (unsigned char *) rtp;
  pkt->data.payload.pData = ((unsigned char *)rtp) + rtpHdrLen;
  pkt->data.tv.tv_sec = pkthdr->ts.tv_sec;
  pkt->data.tv.tv_usec = pkthdr->ts.tv_usec;
  pkt->data.flags = 0;

  return 0;
}

static int rtp_capturePrint2Buf(const CAPTURE_STATE_T *pState, char *buf, unsigned int sz) {

  int rc = 0;
  unsigned int idx, tmp;
  const COLLECT_STREAM_HDR_T  *pPktHdr;
  const COLLECT_STREAM_PKTDATA_T *pPktDataLast;
  uint64_t tsDelta;
  unsigned int idxbuf = 0;
  char tmpBuf[64];
  char tmpBuf2[64];
  char tmpBuf3[64];
  //uint64_t tvElapsedUs0;
  uint64_t tsElapsedHz0;
  double tsElapsedMs0;
  int64_t deltaMs;
  double durationUs;
  double clockHz;
  double fps;
  double bwNet;
  double bwPayload;
  double rfc3550jitterMs; 
  float f;

  pthread_mutex_lock(&((CAPTURE_STATE_T *) pState)->mutexStreams);

  if((rc = snprintf(&buf[idxbuf], sz - idxbuf, 
                    "Input stream statistics:  Packets received: %"LL64"u, RTP: %"LL64"u", 
                    pState->numPkts, pState->numRtpPkts)) > 0) {
    idxbuf += rc;
  }

  for(idx = 0; idx < pState->maxStreams; idx++) {

    tmpBuf[0] = '\0';
    tmpBuf2[0] = '\0';
    tmpBuf3[0] = '\0';

    if(pState->pStreams[idx].numSeqIncrOk >= RTP_SEQ_INCR_MIN) {
      pPktHdr = &pState->pStreams[idx].hdr;
      pPktDataLast = &pState->pStreams[idx].pjtBuf->pPkts[pState->pStreams[idx].pjtBuf->idxPktStored].pktData;

      if((pState->pStreams[idx].rtcpRR.pktdroptot > 0 ||
         pState->pStreams[idx].rtcpRR.pktdropcnt > 0) && pState->pStreams[idx].numPkts > 0) {
      
        tmp = MAX(pState->pStreams[idx].rtcpRR.pktdroptot, pState->pStreams[idx].rtcpRR.pktdropcnt);
        f = (float) tmp * 100.0f / pState->pStreams[idx].numPkts;
        snprintf(tmpBuf, sizeof(tmpBuf), " lost:%u, %.2f%%,", tmp, f);
      }

      if(pState->pStreams[idx].numSeqnoNacked > 0 && pState->pStreams[idx].numPkts > 0) {
        f = (float) pState->pStreams[idx].numSeqnoNacked * 100.0f / pState->pStreams[idx].numPkts;
        snprintf(tmpBuf3, sizeof(tmpBuf3), " nacked:%u, %.2f%%,", pState->pStreams[idx].numSeqnoNacked, f);
      }

      durationUs = ((double)(pState->pStreams[idx].lastTsChangeTv.tv_sec - 
                     pPktHdr->tvStart.tv_sec) * TIME_VAL_US) +
               (pState->pStreams[idx].lastTsChangeTv.tv_usec - pPktHdr->tvStart.tv_usec);

      if(durationUs > TIME_VAL_US) {
        tsDelta = ((uint64_t )pPktDataLast->u.rtp.ts - pState->pStreams[idx].ts0) + 
           ((uint64_t ) pState->pStreams[idx].tsRolls << 32);
        clockHz = (double) tsDelta / (durationUs / TIME_VAL_US_FLOAT);
        //fprintf(stderr, "tsD:%llu Hz, duration: %.3f us\n", tsDelta, durationUs);
        fps = clockHz / pState->pStreams[idx].frameDeltaHz;
        bwNet = ((double) pState->pStreams[idx].numBytes / (durationUs * 128.0 / 
                                                                TIME_VAL_US_FLOAT));
        bwPayload = ((double) pState->pStreams[idx].numBytesPayload / 
                                         (durationUs*128.0 / TIME_VAL_US_FLOAT));


        //tvElapsedUs0 = ((pPktDataLast->tv.tv_sec - pState->pStreams[idx].hdr.tvStart.tv_sec) * TIME_VAL_US) +
        //                (pPktDataLast->tv.tv_usec - pState->pStreams[idx].hdr.tvStart.tv_usec);

        if(pState->pStreams[idx].clockHz > 0) {
          tsElapsedHz0 = ((uint64_t )pPktDataLast->u.rtp.ts - pState->pStreams[idx].ts0) +
                          ((uint64_t ) pState->pStreams[idx].tsRolls << 32);
          tsElapsedMs0 = (double) 1000.0 * tsElapsedHz0 / pState->pStreams[idx].clockHz;
          deltaMs = (durationUs/1000 - tsElapsedMs0);
          if(deltaMs > 0) {
            snprintf(tmpBuf2, sizeof(tmpBuf2), " late");
          }
          rfc3550jitterMs = 1000 * pState->pStreams[idx].rtcpRR.rfc3550jitter / pState->pStreams[idx].clockHz;
        } else {
          rfc3550jitterMs = 0.0f; 
          deltaMs= 0.0f;
        }
         //fprintf(stderr, "tvElapsedUs0: %llu, tsElapsedMs0: %.3f, delta0: %lld ms\n", durationUs, tsElapsedMs0, (int64_t) (durationUs/1000 - tsElapsedMs0));

      } else {
        clockHz = 0.0f;
        fps = 0.0f;
        bwNet = 0.0f;
        bwPayload = 0.0f;
        deltaMs= 0.0f;
        rfc3550jitterMs = 0.0f; 
      }

      if((rc = snprintf(&buf[idxbuf], sz - idxbuf, "\nRTP %s, ssrc:0x%x, pt:%u, tos:0x%x, pkts:%"LL64"u,%s%s"
          " net:%.3fKbit/s, data:%.3fKbit/s,\nduration:%.3fs %.1fHz %.1ffps, seq:%u, ts:%uHz"
          ", rfc-jitter: %.1fms, arrival delta:%lldms%s",
            pState->pStreams[idx].strSrcDst, 
            pPktHdr->key.ssrc,  pPktHdr->payloadType, pPktHdr->ipTos, 
            pState->pStreams[idx].numPkts, tmpBuf, tmpBuf3, bwNet, bwPayload, 
            durationUs / TIME_VAL_US_FLOAT, clockHz, fps, pPktDataLast->u.rtp.seq, pPktDataLast->u.rtp.ts,
            rfc3550jitterMs, deltaMs, tmpBuf2)) >= 0) {

        idxbuf += rc;
      }

    }
  }

  pthread_mutex_unlock(&((CAPTURE_STATE_T *) pState)->mutexStreams);

  return idxbuf;
}

void rtp_capturePrintLog(const CAPTURE_STATE_T *pState, int logLevel) {
  int rc;
  char buf[2048];

  if((rc = rtp_capturePrint2Buf(pState, buf, sizeof(buf))) > 0) {
    LOG(logLevel, "%s\n", buf);
  }
}

void rtp_capturePrint(const CAPTURE_STATE_T *pState, FILE *fp) {
  int rc;
  char buf[2048];

  if((rc = rtp_capturePrint2Buf(pState, buf, sizeof(buf))) > 0) {
    fprintf(fp, "%s", buf);
  }
}

#endif // VSX_HAVE_CAPTURE
