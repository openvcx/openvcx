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


#define PKTQUEUE_TMP_PKT                   1

#define PKTQ_ADVANCE_WR(pQ)   if(++((pQ)->idxWr) >= (pQ)->cfg.maxPkts) { \
                                    (pQ)->idxWr = 0; } (pQ)->idxInWr = 0;
#define PKTQ_ADVANCE_RD(pQ)   if(++((pQ)->idxRd) >= (pQ)->cfg.maxPkts) { \
                                    (pQ)->idxRd = 0; } (pQ)->idxInRd = 0;
#define PKTQ_PREV_WR(pQ) (pQ->idxWr > 0 ? pQ->idxWr - 1 : pQ->cfg.maxPkts - 1);



extern pthread_cond_t *g_proc_exit_cond; 
static enum PKTQUEUE_RC framebuf_add(PKTQUEUE_T *pFb, const unsigned char *pData,
                                     unsigned int len, PKTQ_EXTRADATA_T *pXtra);
static int framebuf_read(PKTQUEUE_T *pFb, unsigned char *pData, unsigned int len,
                         PKTQ_EXTRADATA_T *pXtra);
static const PKTQUEUE_PKT_T *framebuf_readdirect(PKTQUEUE_T *pQ);
static int framebuf_readdirect_done(PKTQUEUE_T *pQ);

static int ringbuf_write(PKTQUEUE_T *pQ, const unsigned char *pBuf, unsigned int sz);
static int ringbuf_read(PKTQUEUE_T *pQ, unsigned char *pBuf, unsigned int sz);


/*
static void pktqueue_destroystats(THROUGHPUT_STATS_T **ppStats) {
  THROUGHPUT_STATS_T *pStats = *ppStats;
  unsigned int idx;

  if(!pStats) {
    return; 
  }

  for(idx = 0; idx < THROUGHPUT_STATS_BURSTRATES_MAX; idx++) {
    burstmeter_close(&pStats->bitratesWr[idx]);
    burstmeter_close(&pStats->bitratesRd[idx]);
  }

  avc_free((void *) ppStats);
}


int pktqueue_createstats(PKTQUEUE_T *pQ, unsigned int rangeMs1, unsigned int rangeMs2) {
  unsigned int idx;
  unsigned int periodMs;
  unsigned int rangeMs ;

  if(!pQ) {
    return -1;
  }

  if(pQ->pstats) {
    pktqueue_destroystats(&pQ->pstats);
  }

  if(!(pQ->pstats = avc_calloc(1, sizeof(THROUGHPUT_STATS_T)))) {
    return -1;
  }

  for(idx = 0; idx < THROUGHPUT_STATS_BURSTRATES_MAX; idx++) {

    rangeMs = idx == 0 ? rangeMs1 : rangeMs2;
    if(rangeMs == 0) {
      continue;
    }

    periodMs = rangeMs / 10;
    rangeMs = periodMs * 10;
    if(burstmeter_init(&pQ->pstats->bitratesWr[idx], periodMs, rangeMs) != 0 ||
       burstmeter_init(&pQ->pstats->bitratesRd[idx], periodMs, rangeMs) != 0) {
      pktqueue_destroystats(&pQ->pstats);
      return -1;
    }
  }

  return 0;
}
*/

static int pktqueue_addburstsample(BURSTMETER_SAMPLE_SET_T *pbitrates,  
                                   THROUGHPUT_STATS_TM_T *pTm,
                                   unsigned int len,
                                   const PKTQ_EXTRADATA_T *pXtra) {
  struct timeval tv;
  unsigned int idx;

  if(!pXtra) {
    gettimeofday(&tv, NULL);
    if(pTm) {
      pTm->tm = (tv.tv_sec * 90000) + (tv.tv_usec * 9 / 100);
    }
  } else {
    tv.tv_sec = pXtra->tm.pts / 90000;
    tv.tv_usec = (pXtra->tm.pts % 90000) * 100 / 9;
    if(pTm) {
      pTm->tm =  pXtra->tm.pts;
    }
  }

  //fprintf(stderr, "ADD SAMPLE pts %lldHz %.6fsec %d:%d\n", pXtra->tm.pts, PTSF(pXtra->tm.pts), tv.tv_sec, tv.tv_usec);
  for(idx = 0; idx < 2; idx++) {
    if(pbitrates[idx].meter.rangeMs > 0) {
      burstmeter_AddSample(&pbitrates[idx], len, &tv);
     }
  }
 
  return 0;
}

static void qlock(PKTQUEUE_T *pQ, int lock) {
  if(pQ->cfg.uselock) {
    if(lock) {
      pthread_mutex_lock(&pQ->mtx);
    } else {
      pthread_mutex_unlock(&pQ->mtx);
    }
  }
}

static unsigned char *pktqueue_getUserData(PKTQUEUE_PKT_T *pPkt, const PKTQUEUE_CONFIG_T *pCfg) {
  unsigned int alignoffset = 0;
  unsigned char *p = pPkt->pBuf + pCfg->prebufOffset;

  if(pCfg->userDataLen > 0 && ((long)p & 0x0f) > 0) {
     alignoffset = 16 - ((long)p & 0x0f);
  }

  return p + pPkt->len + alignoffset;

}

static int alloc_slot(PKTQUEUE_T *pQ, unsigned int idx) {

  if(!(pQ->pkts[idx].pBuf = avc_calloc(1, pQ->cfg.prebufOffset + pQ->cfg.maxPktLen +
                             (pQ->cfg.userDataLen > 0 ? 16 : 0) + pQ->cfg.userDataLen))) {
    return -1;
  }

  pQ->pkts[idx].pData = pQ->pkts[idx].pBuf + pQ->cfg.prebufOffset;
  pQ->pkts[idx].allocSz = pQ->cfg.maxPktLen;

  return 0;
}

PKTQUEUE_T *pktqueue_create(unsigned int maxPkts, unsigned int szPkt,
                            unsigned int growMaxPkts, unsigned int growSzPkt,
                            unsigned int prebufOffset, unsigned int userDataLen,
                            int uselock) {
  PKTQUEUE_T *pQ;
  unsigned int idx;
  //unsigned int alignoffset = 0;

  if(maxPkts <= 0 || szPkt <= 0) {
    return NULL;
  }

  if(growMaxPkts < maxPkts) {
    growMaxPkts = maxPkts;
  }
  if(growMaxPkts > maxPkts) {
    LOG(X_WARNING("Queue growMaxPkts:%d > %d not yet implemented"), growMaxPkts, maxPkts);
    growMaxPkts = maxPkts;
  }

  if(!(pQ = (PKTQUEUE_T *) avc_calloc(1, sizeof(PKTQUEUE_T)))) {
    return NULL;
  }

  if(!(pQ->pkts = (PKTQUEUE_PKT_T *) avc_calloc(growMaxPkts + PKTQUEUE_TMP_PKT, sizeof(PKTQUEUE_PKT_T)))) {
    free(pQ);
    return NULL;
  }

  pQ->cfg.maxPkts = maxPkts;
  pQ->cfg.maxPktLen = szPkt;
  pQ->cfg.growMaxPkts = growMaxPkts;
  pQ->cfg.growMaxPktLen = growSzPkt;
  pQ->cfg.prebufOffset = prebufOffset;
  pQ->cfg.userDataLen = userDataLen;

  //fprintf(stderr, "Q 0x%x MTX INIT\n", pQ);

  if((pQ->cfg.uselock = uselock)) {
    pthread_mutex_init(&pQ->mtx, NULL);
    pthread_mutex_init(&pQ->mtxRd, NULL);
    pthread_mutex_init(&pQ->mtxWr, NULL);
  }

  pthread_mutex_init(&pQ->notify.mtx, NULL);
  pthread_cond_init(&pQ->notify.cond, NULL);

  for(idx = 0; idx < maxPkts + PKTQUEUE_TMP_PKT; idx++) {

    if(alloc_slot(pQ, idx) < 0) {
      pktqueue_destroy(pQ);
      return NULL;
    }
/*
    if(!(pQ->pkts[idx].pBuf = avc_calloc(1, pQ->cfg.prebufOffset + pQ->cfg.maxPktLen +
                             (pQ->cfg.userDataLen > 0 ? 16 : 0) + pQ->cfg.userDataLen))) {
      pktqueue_destroy(pQ);
      return NULL;
    }

    pQ->pkts[idx].pData = pQ->pkts[idx].pBuf + pQ->cfg.prebufOffset;
    pQ->pkts[idx].allocSz = pQ->cfg.maxPktLen;
*/
  }

  if(PKTQUEUE_TMP_PKT) {
    pQ->pktTmp = &pQ->pkts[maxPkts];
  }

  pktqueue_reset(pQ, 0);

  return pQ;
}

void pktqueue_destroy(PKTQUEUE_T *pQ) {
  unsigned int idx;

  if(!pQ) {
    return;
  }

  for(idx = 0; idx < pQ->cfg.growMaxPkts + PKTQUEUE_TMP_PKT; idx++) {
    if(pQ->pkts[idx].pBuf) {
      free(pQ->pkts[idx].pBuf);
    }
  }
  if(pQ->pktTmp && pQ->pktTmp->pBuf && pQ->ownTmpPkt) {
    free(pQ->pktTmp->pBuf);
  }

  //if(pQ->pstats) {
  //  pktqueue_destroystats(&pQ->pstats);
  //}

  //fprintf(stderr, "Q 0x%x MTX DESTROY\n", pQ);

  if(pQ->cfg.uselock) {
    pthread_mutex_destroy(&pQ->mtx);
    pthread_mutex_destroy(&pQ->mtxRd);
    pthread_mutex_destroy(&pQ->mtxWr);
  }

  pthread_cond_destroy(&pQ->notify.cond);
  pthread_mutex_destroy(&pQ->notify.mtx);

  if(pQ->pkts) {
    free(pQ->pkts);
  }

  free(pQ);
}

void pktqueue_dumpstats(THROUGHPUT_STATS_T *pS) {
  unsigned int idx;

  if(pS) {
    fprintf(stderr, "written:%lld,%lld, read:%lld,%lld, skipped:%lld,%lld { delta:%lld,%lld %.3fms, tmLastWr:%.3f, tmLastRd:%.3f\n", pS->written.bytes, pS->written.slots, pS->read.bytes, pS->read.slots, pS->skipped.bytes, pS->skipped.slots, 
(pS->written.bytes - pS->read.bytes - pS->skipped.bytes), (pS->written.slots - pS->read.slots - pS->skipped.slots),PTSF_MS(pS->tmLastWr.tm- pS->tmLastRd.tm), PTSF(pS->tmLastWr.tm), PTSF(pS->tmLastRd.tm));

    for(idx = 0; idx < sizeof(pS->bitratesWr) / sizeof(pS->bitratesWr[0]); idx++) {
      if(pS->bitratesWr[idx].meter.rangeMs > 0) {
        fprintf(stderr, "writer - [%d] %dms cur: %.2fKb/s max: %.2fKb/s ", idx, pS->bitratesWr[idx].meter.rangeMs, pS->bitratesWr[idx].meter.cur.bytes * (1000.0f / pS->bitratesWr[idx].meter.rangeMs) / 125.0f, pS->bitratesWr[idx].meter.max.bytes * (1000.0f / pS->bitratesWr[idx].meter.rangeMs) / 125.0f);
      }
   }
   fprintf(stderr, "\n");

   for(idx = 0; idx < sizeof(pS->bitratesWr) / sizeof(pS->bitratesWr[0]); idx++) {
     if(pS->bitratesRd[idx].meter.rangeMs > 0) {
        fprintf(stderr, "reader - [%d] %dms cur: %.2fKb/s max: %.2fKb/s ", idx, pS->bitratesRd[idx].meter.rangeMs, pS->bitratesRd[idx].meter.cur.bytes * (1000.0f / pS->bitratesRd[idx].meter.rangeMs) / 125.0f, pS->bitratesRd[idx].meter.max.bytes * (1000.0f / pS->bitratesRd[idx].meter.rangeMs) / 125.0f);
      }
    }
    fprintf(stderr, "\n");

  }


}

void pktqueue_dump(PKTQUEUE_T *pQ, PKTQUEUE_DUMP_USERDATA cbDumpUserData) {
  unsigned int i; 
  PKTQUEUE_STATS_REPORT_T report;

  pktqueue_getstats(pQ, &report);
 
  for(i = 0; i < pQ->cfg.growMaxPkts; i++) {
    fprintf(stderr, "[%d]", i);
    if(pQ->pkts[i].pBuf) {
      fprintf(stderr, " len:%d, pts: %.3f, flags:0x%x %s%s%s", pQ->pkts[i].len, PTSF(pQ->pkts[i].xtra.tm.pts), pQ->pkts[i].flags,
            (pQ->pkts[i].flags & PKTQUEUE_FLAG_KEYFRAME) ? " keyframe" : "",
            pQ->idxWr == i ? " writer " : "", pQ->idxRd == i ? " reader " : "");
      if(cbDumpUserData && pQ->pkts[i].xtra.pQUserData) {
        cbDumpUserData(pQ->pkts[i].xtra.pQUserData);
      }
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "Q[id:%d] wr:[%d][%d], rd:[%d][%d] uniqueWrIdx:%u, haveRdr:%d, overwritten:%d\n", pQ->cfg.id, pQ->idxInWr, pQ->idxWr, pQ->idxInRd, pQ->idxRd, pQ->uniqueWrIdx+1,pQ->haveRdr, pQ->numOverwritten);

  pktqueue_dumpstats(&report.stats);

}

static int reset(PKTQUEUE_T *pQ) {
  unsigned int i;

  for(i = 0; i < pQ->cfg.maxPkts; i++) {

    if(pQ->pstats && pQ->pkts[i].pBuf) {
      if((pQ->pkts[i].flags & PKTQUEUE_FLAG_HAVEPKTDATA) &&
         (i != pQ->idxRd || pQ->idxInRd == 0)) {
        pQ->pstats->skipped.slots++;
      }
      pQ->pstats->skipped.bytes += pQ->pkts[i].len;
      if(i == pQ->idxRd && pQ->idxInRd > 0) {
        pQ->pstats->skipped.bytes -= pQ->idxInRd;
      }
    }

    pQ->pkts[i].len = 0;
    pQ->pkts[i].flags = 0;
    pQ->pkts[i].idx = 1;
    memset(&pQ->pkts[i].xtra, 0, sizeof(pQ->pkts[i].xtra));
  }

  pQ->idxWr = 0;
  pQ->idxInWr = 0;
  pQ->idxRd = 0;
  pQ->idxInRd = 0;
  pQ->haveData = 0;
  pQ->isInit = 0;
  pQ->numOverwritten = 0;
  pQ->consecOverwritten = 0;
  pQ->uniqueWrIdx = 1;

  return 0;
}

static int realloc_slot(PKTQUEUE_T *pQ, unsigned int idx, unsigned int reqlen) {
  unsigned char *pBuf;
  unsigned int growMaxPktLen;

  if(pQ->cfg.growMaxPktLen > pQ->cfg.maxPktLen && pQ->pkts[idx].allocSz < pQ->cfg.growMaxPktLen) {

    if(pQ->cfg.growSzOnlyAsNeeded > 0) {

      if(pQ->cfg.growSzOnlyAsNeeded == 1) {
        growMaxPktLen = reqlen;
      } else {
        growMaxPktLen =  (unsigned int) ((float)(reqlen * (100 + pQ->cfg.growSzOnlyAsNeeded)) / 100.0f);
      }
      growMaxPktLen = MIN(growMaxPktLen, pQ->cfg.growMaxPktLen);

    } else {
      growMaxPktLen = pQ->cfg.growMaxPktLen;
    }

    if(growMaxPktLen < reqlen) {
      LOG(X_ERROR("queue (id:%d) resizing of wr[%d].sz:%d -> length:%d exceeds slot length:%d, (max:%d)"), 
          pQ->cfg.id, idx, pQ->pkts[idx].allocSz, reqlen, growMaxPktLen, pQ->cfg.growMaxPktLen);
      return -1;
    } else if(!(pBuf = (unsigned char *) avc_calloc(1, pQ->cfg.prebufOffset + 
       growMaxPktLen + (pQ->cfg.userDataLen > 0 ? 16 : 0) + pQ->cfg.userDataLen))) {
      return -1;
    }

    LOG(X_DEBUGV("Increased queue (id:%d) slot[%d] size %d -> %d, data len:%u"), 
                 pQ->cfg.id, idx, pQ->pkts[idx].allocSz, growMaxPktLen, reqlen);

    memcpy(pBuf, pQ->pkts[idx].pBuf, pQ->cfg.prebufOffset + pQ->pkts[idx].len + 
           (pQ->cfg.userDataLen > 0 ? 16 : 0) + pQ->cfg.userDataLen);
    free(pQ->pkts[idx].pBuf);
    pQ->pkts[idx].pBuf = pBuf;
    pQ->pkts[idx].pData = pBuf + pQ->cfg.prebufOffset;
    pQ->pkts[idx].allocSz = growMaxPktLen;

    return growMaxPktLen;
  }
  
  return 0;
}

static int move_rdr(PKTQUEUE_T *pQ, unsigned int numSkip) {
  unsigned int idx = 0;

  if(numSkip == 0) {
    numSkip = 1;
  } else if(numSkip > pQ->cfg.maxPkts) {
    numSkip = pQ->cfg.maxPkts;
  }

  //fprintf(stderr, "MOVE_RDR\n");

  if(numSkip > 0 && pQ->pstats) {
    //fprintf(stderr, "SKIPPING0 [%d], %d bytes\n", pQ->idxRd, pQ->pkts[pQ->idxRd].len);
    if((pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA) && pQ->idxInRd == 0) {
      pQ->pstats->skipped.slots++;
    }
    pQ->pstats->skipped.bytes += (pQ->pkts[pQ->idxRd].len - pQ->idxInRd);
  }

  pQ->idxInRd = 0;

  for(idx = 0; idx < numSkip; idx++) {
    //fprintf(stderr, "move_rdr idx[%d], numSkip:%d, numPkts:%d, idxRd:%d, idxWr:%d\n", idx, numSkip, numPkts, pQ->idxRd, pQ->idxWr);

    if(pQ->pstats) {
      //fprintf(stderr, "SKIPPING [%d], %d bytes\n", pQ->idxRd, pQ->pkts[pQ->idxRd].len);
      if((pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
        pQ->pstats->skipped.slots++;
      }
      pQ->pstats->skipped.bytes += pQ->pkts[pQ->idxRd].len;
    }

    if(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA) {
      pQ->consecOverwritten++;
      pQ->numOverwritten++;
      pQ->pkts[pQ->idxRd].flags = 0;
      pQ->pkts[pQ->idxRd].len = 0;
    }

    PKTQ_ADVANCE_RD(pQ);
  }
  //fprintf(stderr, "move_rdr finished idxRd:%d, idxWr:%d (was called w/ numPkts:%d)\n", pQ->idxRd, pQ->idxWr, numPkts);

  return 0;
}

static int testQMoveToPts(PKTQUEUE_T *pQ, uint64_t pts,  const PKTQ_EXTRADATA_T *pXtra, int lock) {
  int maxPkts;
  int rc = 0;
  unsigned int numOverwritten = 0;
  int havePts0 = 0;
  uint64_t pts0 = 0;

  if(!pQ->haveRdr) {
    return rc;
  }

  //LOG(X_DEBUG("DUMPING Q complement before repositioning... pts:%.3f"), PTSF(pts)); pktqueue_dump(pQ, NULL);

  if(lock) {
    qlock(pQ, 1);
  }

  maxPkts = pQ->cfg.maxPkts;

  while(maxPkts-- > 0) {

    if(!(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
      //fprintf(stderr, "BREAK idxRd[%d] empty\n", pQ->idxRd);
      break;
    } else if(numOverwritten > 0 && pts <= pQ->pkts[pQ->idxRd].xtra.tm.pts) {
      //fprintf(stderr, "BREAK pts idxRd[%d] \n", pQ->idxRd);
      break;
    } else if(numOverwritten > 0 && pQ->idxRd == pQ->idxWr) {
      //fprintf(stderr, "BREAK idxRd == idxWr:%d \n", pQ->idxRd);
      break;
    }

    if(!havePts0) {
      pts0 = pQ->pkts[pQ->idxRd].xtra.tm.pts;
      havePts0 = 1;
    }
    pQ->pkts[pQ->idxRd].flags = 0;
    pQ->pkts[pQ->idxRd].len = 0;
    numOverwritten++;
    pQ->numOverwritten++;
    pQ->consecOverwritten++;
    //fprintf(stderr, "pQComplement cleared idxRd[%d]\n", pQ->idxRd);

    PKTQ_ADVANCE_RD(pQ);

  }

  LOG(X_WARNING("Setting queue (id:%d) reader to pts: %.3f -> %.3f at rd:[%d]/%d, wr:[%d], overwrote: %d (%d), pts{pkt:%.3f, q:%.3f(peer-q:%.3f)}"),
                pQ->cfg.id, PTSF(pts0), PTSF(pts), pQ->idxRd, pQ->cfg.maxPkts, pQ->idxWr, numOverwritten, pQ->numOverwritten,
                (pXtra ? PTSF(pXtra->tm.pts) : 0), PTSF(pQ->pkts[pQ->idxRd].xtra.tm.pts), 
                pQ->pQComplement ? PTSF(((PKTQUEUE_T *)pQ->pQComplement)->pkts[((PKTQUEUE_T *)pQ->pQComplement)->idxRd].xtra.tm.pts) : 0);

  if(lock) {
    qlock(pQ, 0);
  }

  //LOG(X_DEBUG("DUMPING Q complement after repositioning... pts:%.3f"), PTSF(pts)); pktqueue_dump(pQ, NULL);

  return numOverwritten;
}

static uint64_t getOverwritePtsTolerance(const PKTQUEUE_T *pQ) {
  uint64_t ptsTolerance = 0;

  if(pQ->cfg.overwriteType == PKTQ_OVERWRITE_FIND_PTS) {
    ptsTolerance = pQ->cfg.overwriteVal;
  } else {
    ptsTolerance = PKTQUEUE_OVERWRITE_PTS_TOLERANCE;
  }

  return ptsTolerance;
}

static int testQMoveToKeyframe(PKTQUEUE_T *pQ, const PKTQ_EXTRADATA_T *pXtra) {
  unsigned int idxWr;
  int maxPkts;
  int setRdr = -1;
  uint64_t ptsLatest = 0;
  //uint64_t ptsTolerance = PKTQUEUE_OVERWRITE_PTS_TOLERANCE;
  uint64_t ptsTolerance = 0;
  uint64_t ptsComplement = 0;
  int64_t ptsDelta;
  int havePtsLatest = 0;
  unsigned int numOverwritten = 0;
  uint64_t pts0 = 0;
  PKTQ_EXTRADATA_T xtra;
  int rc = 0;

  if(!pQ->haveRdr) {
    return rc;
  }

  //LOG(X_DEBUG("DUMPING Q before repositioning...")); pktqueue_dump(pQ, NULL);

  maxPkts = pQ->cfg.maxPkts;
  idxWr = pQ->idxWr;
  pts0 = pQ->pkts[pQ->idxRd].xtra.tm.pts;

  while(maxPkts-- > 0) {

    if(idxWr > 0) {
      idxWr--;
    } else {
      idxWr = pQ->cfg.maxPkts - 1;
    }

    if(!(pQ->pkts[idxWr].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
      //fprintf(stderr, "BREAK idxWr[%d] empty\n", idxWr);
      break;
    } else if(setRdr == -1 && idxWr == pQ->idxRd) {
      //fprintf(stderr, "BREAK idxWr == %d\n", idxWr);
      break;
    }

    //pts = pQ->pkts[idxWr].xtra.tm.pts;
    if(!havePtsLatest) {
      ptsLatest = pQ->pkts[idxWr].xtra.tm.pts;
      havePtsLatest = 1;

      //if(pQ->cfg.overwriteType == PKTQ_OVERWRITE_FIND_PTS) {
      //  ptsTolerance = pQ->cfg.overwriteVal;
      //}

      if(pQ->cfg.overwriteType != PKTQ_OVERWRITE_FIND_KEYFRAME) {
        break;
      }
    }

    //LOG(X_DEBUG("qid[%d]. idxWr[%d]/%d pts %.3f, len:%d %s"), pQ->cfg.id, idxWr, pQ->cfg.maxPkts, PTSF(pQ->pkts[idxWr].xtra.tm.pts), pQ->pkts[idxWr].len, pQ->pkts[idxWr].flags & PKTQUEUE_FLAG_KEYFRAME ? "KEYFRAME" : "");

    if(setRdr == -1 && (pQ->pkts[idxWr].flags & PKTQUEUE_FLAG_KEYFRAME)) {
      setRdr = idxWr;
    } else if(setRdr != -1) {
      pQ->pkts[idxWr].flags = 0;
      pQ->pkts[idxWr].len = 0;
      numOverwritten++;
      pQ->numOverwritten++;
      pQ->consecOverwritten++;
    }

  }

  ptsTolerance = getOverwritePtsTolerance(pQ);

  if(setRdr != -1) {
    pQ->idxRd = setRdr;
    ptsLatest = pQ->pkts[pQ->idxRd].xtra.tm.pts;
    havePtsLatest = 1;
    LOG(X_WARNING("Setting queue (id:%d) reader to keyframe at rd:[%d]/%d (pts: %.3f -> %.3f), wr:[%d], overwrote: %d (%d), pts{pkt:%.3f, q:%.3f(peer-q:%.3f)}"),
                pQ->cfg.id, setRdr, pQ->cfg.maxPkts, PTSF(pts0), PTSF(ptsLatest), pQ->idxWr, numOverwritten, pQ->numOverwritten,
                (pXtra ? PTSF(pXtra->tm.pts) : 0), PTSF(pQ->pkts[pQ->idxRd].xtra.tm.pts), 
                pQ->pQComplement ? PTSF(((PKTQUEUE_T *)pQ->pQComplement)->pkts[((PKTQUEUE_T *)pQ->pQComplement)->idxRd].xtra.tm.pts) : 0);

    rc = 1;
  } else if(havePtsLatest && ptsLatest >= ptsTolerance) {
    ptsLatest -= ptsTolerance;
    testQMoveToPts(pQ, ptsLatest,  pXtra, 0);
    rc = 2;
  }

  //LOG(X_DEBUG("DUMPING Q after repositioning...")); pktqueue_dump(pQ, NULL);

  if(pQ->pQComplement && havePtsLatest) {

    //
    // The peer queue may not be ts base aligned w/ our queue, so reset by timing offset
    // from the latest known time.
    //
    if(pXtra && (ptsDelta = pXtra->tm.pts - ptsLatest) < pQ->pQComplement->ptsLastWr) {
      ptsComplement = pQ->pQComplement->ptsLastWr - ptsDelta;
      if(pXtra) {
        memcpy(&xtra, pXtra, sizeof(xtra));
        xtra.tm.pts = ptsComplement;
        pXtra = &xtra;
      }
    } else {
      ptsComplement = ptsLatest;
    }

    testQMoveToPts(pQ->pQComplement, ptsComplement, pXtra, pQ->pQComplement->cfg.uselock);
  }

  return rc;
}

static void skip_overwritten_slots(PKTQUEUE_T *pQ, const PKTQ_EXTRADATA_T *pXtra) {
  unsigned int numskip = 20; 
  unsigned int numOverwritten = pQ->numOverwritten;

  if(pQ->cfg.maxPkts <= numskip) pQ->consecOverwritten=pQ->cfg.maxPkts + 1;

  if((unsigned int) pQ->consecOverwritten >= pQ->cfg.maxPkts) {

    LOG(X_DEBUG("Resetting pkt queue(id:%d) sz: %d - consec overwritten:%d"),
           pQ->cfg.id, pQ->cfg.maxPkts, pQ->consecOverwritten);
    reset(pQ);
    if(!pQ->haveRdrSetManually) {
      pQ->haveRdr = 0;
    }

    numOverwritten = pQ->cfg.maxPkts;

  } else {

    if(pQ->idxWr == pQ->idxRd) {

      if(numskip >= pQ->cfg.maxPkts) {
        if((numskip = pQ->cfg.maxPkts /2) <= 2) {
          numskip = 2;
        }
      }

      move_rdr(pQ, numskip);

      numOverwritten = pQ->numOverwritten - numOverwritten;

      //LOG(X_DEBUG("pkt queue(id:%d) skipped %d slots wr:[%d] rd:[%d] / %d (%d), consec:%d"),
      //     pQ->cfg.id, numskip, pQ->idxWr, pQ->idxRd, pQ->cfg.maxPkts, pQ->cfg.maxPktLen,
      //    pQ->consecOverwritten);

    }
  }

  LOG(X_WARNING("Setting queue(id:%d) reader at rd:[%d]/%d, wr:[%d], overwrote:%d (%d), pts{pkt:%.3f, q:%.3f(peer-q:%.3f)}"),
      pQ->cfg.id, pQ->idxRd, pQ->cfg.maxPkts, pQ->idxWr, numOverwritten, pQ->numOverwritten,
      (pXtra ? PTSF(pXtra->tm.pts) : 0),
      PTSF(pQ->pkts[pQ->idxRd].xtra.tm.pts), 
      pQ->pQComplement ? PTSF(((PKTQUEUE_T *)pQ->pQComplement)->pkts[((PKTQUEUE_T *)pQ->pQComplement)->idxRd].xtra.tm.pts) : 0);

}

static int check_overwrite(PKTQUEUE_T *pQ, int haveRdr, const PKTQ_EXTRADATA_T *pXtra) {
  int rc = 0;
  int64_t ptsDelta;
  int64_t ptsTolerance;
  int doOverwrite = 0;

  if(haveRdr && !pQ->haveRdr) {
    return rc;
  }

  if(pXtra && pQ->cfg.overwriteType != PKTQ_OVERWRITE_OK &&pQ->cfg.maxRTlatencyMs > 0 && 
     pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {

    ptsDelta = pXtra->tm.pts - pQ->pkts[pQ->idxRd].xtra.tm.pts;
    ptsTolerance = getOverwritePtsTolerance(pQ);

    if(ptsDelta > ptsTolerance + (pQ->cfg.maxRTlatencyMs * 90)) {

      LOG(X_DEBUG("queue (id:%d) exceeded latency threshold %.3f > %.3f + %.3f sec, rd:[%d]/%d,pts:%.3f, 0x%x, wr:[%d],pts:%.3f"), 
                  pQ->cfg.id, PTSF(ptsDelta), PTSF(ptsTolerance), (float)pQ->cfg.maxRTlatencyMs/1000.0f, 
                  pQ->idxRd, pQ->cfg.maxPkts, PTSF(pQ->pkts[pQ->idxRd].xtra.tm.pts), pQ->pkts[pQ->idxRd].flags,
                  pQ->idxWr, PTSF(pXtra->tm.pts));

      doOverwrite = 1;
    }
  }

  if(!doOverwrite && pQ->pkts[pQ->idxWr].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    if(pQ->cfg.overwriteType != PKTQ_OVERWRITE_OK) {
      LOG(X_DEBUG("queue (id:%d) is full and write will overwrite, rd:[%d]/%d,pts:%.3f, 0x%x, wr:[%d],pts:%.3f"), 
                  pQ->cfg.id, pQ->idxRd, pQ->cfg.maxPkts, PTSF(pQ->pkts[pQ->idxRd].xtra.tm.pts), pQ->pkts[pQ->idxRd].flags,
                  pQ->idxWr, PTSF(pXtra->tm.pts));
    }

    doOverwrite = 1;
  }

  if(doOverwrite) {

    if(pXtra) {
      pQ->ptsLastOverwrite = pXtra->tm.pts;
    }

    if(pQ->cfg.overwriteType == PKTQ_OVERWRITE_OK) {

      move_rdr(pQ, 0);

    } else if(pQ->cfg.overwriteType == PKTQ_OVERWRITE_FIND_KEYFRAME ||
              pQ->cfg.overwriteType == PKTQ_OVERWRITE_FIND_PTS) {

      testQMoveToKeyframe(pQ, pXtra);
      rc = 1;

    } else {

      skip_overwritten_slots(pQ, pXtra);
      rc = 1;
    }

  } else if(pQ->consecOverwritten > 0) {
    pQ->consecOverwritten = 0;
  }
 
  return rc;
}

int pktqueue_syncToRT(PKTQUEUE_T *pQ) {
  int rc = 0;
  PKTQ_EXTRADATA_T xtra;

  if(!pQ) {
    return -1;
  }

  memset(&xtra, 0, sizeof(xtra));

  qlock(pQ, 1);

  xtra.tm.pts = pQ->ptsLastWr;
  testQMoveToKeyframe(pQ, &xtra);

  qlock(pQ, 0);

  return rc;
}

int64_t pktqueue_getAgePts(PKTQUEUE_T *pQ, uint64_t *ptsLastWr) {
  int64_t ptsDelta = 0;

  if(!pQ) {
    return -1;
  } 

  qlock(pQ, 1);

  if(!(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE)) {
    ptsDelta = 0;
  } else if((ptsDelta = pQ->ptsLastWr - pQ->pkts[pQ->idxRd].xtra.tm.pts) < 0) {
    ptsDelta = 0;
  }

  if(ptsLastWr) {
    *ptsLastWr = pQ->ptsLastWr;
  }

  qlock(pQ, 0);

  return ptsDelta;
}

static void wakeup_listeners(PKTQUEUE_T *pQ) {
  vsxlib_cond_broadcast(&pQ->notify.cond, &pQ->notify.mtx);
  if(pQ->pnotifyComplement) {
    vsxlib_cond_broadcast(&pQ->pnotifyComplement->cond, &pQ->pnotifyComplement->mtx);
  }
}

void pktqueue_wakeup(PKTQUEUE_T *pQ) {
  if(pQ) {
    wakeup_listeners(pQ);
  }
}

enum PKTQUEUE_RC pktqueue_addpartialpkt(PKTQUEUE_T *pQ, const unsigned char *pData, 
                           unsigned int len, PKTQ_EXTRADATA_T *pXtra, int endingPkt,
                           int overwrite) {

  int addTm = 0;
  int newslot = 0;
  unsigned int idxWr;
  int rc;

  if(!pQ || !pData) {
    return PKTQUEUE_RC_ERROR;
  }

  if(pQ->cfg.type != PKTQ_TYPE_DEFAULT) {
    // not implemented
    return PKTQUEUE_RC_ERROR;
  }

  qlock(pQ, 1);

  if(!pQ->isInit) {

    pQ->idxInWr = 0;
    pQ->pkts[pQ->idxWr].len = 0;
    pQ->pkts[pQ->idxWr].flags = 0;
    pQ->isInit = 1;
    addTm = 1;

  } else if(pQ->prevWrWasEnd || 
            (pXtra && pXtra->tm.pts != pQ->pkts[pQ->idxWr].xtra.tm.pts)) {

  //fprintf(stderr, "queue[%d] new pts: %llu last {wr:%u len:%d idx:%u}\n", pQ->cfg.id, pXtra->tm.pts, pQ->idxWr, pQ->pkts[pQ->idxWr].len, pQ->uniqueWrIdx);

    pQ->pkts[pQ->idxWr].flags |= PKTQUEUE_FLAG_HAVECOMPLETE;

    // Check if the next slot will be overwritten
    if(!overwrite) {
      idxWr = pQ->idxWr;
      if(++idxWr >= pQ->cfg.maxPkts) { 
        idxWr = 0; 
      } 
      if(pQ->pkts[idxWr].flags & PKTQUEUE_FLAG_HAVEPKTDATA) {
        qlock(pQ, 0);
        return PKTQUEUE_RC_WOULDOVERWRITE;
      }
    }


    PKTQ_ADVANCE_WR(pQ);
    newslot = 1;
    check_overwrite(pQ, 0, pXtra);
    pQ->pkts[pQ->idxWr].idx = ++pQ->uniqueWrIdx;
    pQ->idxInWr  = 0;
    pQ->pkts[pQ->idxWr].len = 0;
    pQ->pkts[pQ->idxWr].flags = 0;
    pQ->haveData = 1;
    addTm = 1;
  } else if(endingPkt) {
    pQ->pkts[pQ->idxWr].flags |= PKTQUEUE_FLAG_HAVECOMPLETE;
    pQ->haveData = 1;
  }

  if(pQ->cfg.prebufOffset + pQ->idxInWr + len > pQ->pkts[pQ->idxWr].allocSz) {
//fprintf(stderr, "slot size: %d + %d > %d\n", pQ->idxInWr , len , pQ->pkts[pQ->idxWr].allocSz);
    rc = 0;
    if(pQ->idxInWr + len > pQ->cfg.growMaxPktLen || 
       (rc = realloc_slot(pQ, pQ->idxWr, pQ->idxInWr + len))  <= 0) {

      LOG(X_WARNING("pkt queue(id:%d) wr[%u] too small %u + %u / %u, resize %s"), 
            pQ->cfg.id, pQ->idxWr, pQ->idxInWr, len, pQ->pkts[pQ->idxWr].allocSz,
            (rc > 0 ? "ok" : (rc < 0 ? "error" : "none")));
      qlock(pQ, 0);
      return PKTQUEUE_RC_SIZETOOBIG;
    }
  }

  //fprintf(stderr, "queue[%d] partial copied %d into wr:[%d](%d/%d), rd:[%d], haveRdr:%d pts:%.3f dts:%.3f\n", pQ->cfg.id, len, pQ->idxWr, pQ->idxInWr, pQ->cfg.maxPktLen, pQ->idxRd, pQ->haveRdr, PTSF(pXtra->tm.pts), PTSF(pXtra->tm.dts));

  pQ->prevWrWasEnd = endingPkt;
  memcpy(&pQ->pkts[pQ->idxWr].pData[pQ->idxInWr], pData, len);
  pQ->pkts[pQ->idxWr].len += len;
  pQ->pkts[pQ->idxWr].flags |= PKTQUEUE_FLAG_HAVEPKTDATA;
  pQ->idxInWr += len;

  if(pQ->pstats) {
    pQ->pstats->written.bytes += len;
    if(newslot) {
      pQ->pstats->written.slots++;
    }
  }

  if(addTm && pXtra) {
    memcpy(&pQ->pkts[pQ->idxWr].xtra, pXtra, sizeof(pQ->pkts[pQ->idxWr].xtra));
    pQ->ptsLastWr = pXtra->tm.pts;
    if(pQ->pstats) {
      memcpy(&pQ->pstats->tmLastWr, &pXtra->tm, sizeof(pQ->pstats->tmLastWr));
    }
  }

  if(pQ->haveData && pQ->cfg.uselock) {
    wakeup_listeners(pQ);
  }

  qlock(pQ, 0);

  return PKTQUEUE_RC_OK;
}

/*
int pktqueue_haveopenslots(PKTQUEUE_T *pQ, unsigned int num) {
  unsigned int idxWr;
  int rc = 1;

  if(!pQ) {
    return PKTQUEUE_RC_ERROR;
  }
  qlock(pQ, 1);

  idxWr = pQ->idxWr;
  while(num > 0) {
    if(++idxWr >= pQ->cfg.maxPkts) { 
      idxWr = 0; 
    } 
    if(pQ->pkts[idxWr].flags & PKTQUEUE_FLAG_HAVEPKTDATA) {
      rc = 0;
      break;
    }
    num--;
  }

  qlock(pQ, 0);

  return rc;
}
*/

enum PKTQUEUE_RC pktqueue_addpkt(PKTQUEUE_T *pQ, 
                                 const unsigned char *pData, 
                                 unsigned int len, 
                                 PKTQ_EXTRADATA_T *pXtra, 
                                 int keyframe) {

  int overwrote = 0;
  unsigned int idxWrPrev;
  unsigned int sz;
  int rc = 0;

  if(!pQ || !pData) {
    return PKTQUEUE_RC_ERROR;
  }

  //if(pQ->cfg.id==10||pQ->cfg.id==11){  LOG(S_DEBUG,"pQ:%d pts:%llu %.3f, dts:%lld len:%d) ", pQ->cfg.id, pXtra ? pXtra->tm.pts : 0, pXtra ? PTSF(pXtra->tm.pts) : 0, pXtra ? pXtra->tm.dts : 0, len); LOGHEX_DEBUG(pData, MIN(16, len)); }

  if(pQ->cfg.type == PKTQ_TYPE_FRAMEBUF) {
    return framebuf_add(pQ, pData, len, pXtra);
  } else if(pQ->cfg.type == PKTQ_TYPE_RINGBUF) {
    return ringbuf_write(pQ, pData, len);
  } else if(pQ->cfg.type != PKTQ_TYPE_DEFAULT) {
    return PKTQUEUE_RC_ERROR;
  }

  // No need to check pQ->pkts[pQ->idxWr].allocSz (or pQ->cfg.growMaxPktLen)
  // since resizing is only utilized in addpartialpkt
  //if(len > pQ->cfg.maxPktLen) {
  //  LOG(X_ERROR("Unable to add %u to queue with max slot size %u"), 
  //            len, pQ->cfg.maxPktLen);
  //  return -1;
  //}

  qlock(pQ, 1);

  overwrote = check_overwrite(pQ, 1, pXtra);
/*
  if(!pQ->cfg.allowOverwrite) {
    overwrote = check_overwrite(pQ, 1, pXtra);
  } else if(pQ->pkts[pQ->idxWr].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    //pQ->numOverwritten++;
    move_rdr(pQ, 0);
  }
*/

  //
  // If concataddenum is set, try to stuff the current (still unread) 
  // slot to max capacity
  //
  if(pQ->cfg.concataddenum && !overwrote && pQ->idxRd != pQ->idxWr) {

    idxWrPrev = PKTQ_PREV_WR(pQ);

    if(pQ->pkts[idxWrPrev].len > 0 &&
       (pQ->pkts[idxWrPrev].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {

      if((sz = ((pQ->pkts[idxWrPrev].allocSz - pQ->pkts[idxWrPrev].len) / 
           pQ->cfg.concataddenum) * pQ->cfg.concataddenum) > 0) {
        if(sz > len) {
         sz = len;
        }
        memcpy(&pQ->pkts[idxWrPrev].pData[pQ->pkts[idxWrPrev].len], pData, sz);
        pQ->pkts[idxWrPrev].len += sz;

        pData += sz;
        len -= sz;
        if(pQ->pstats) {
          pQ->pstats->written.bytes += sz;
        }

      }
    }

  }

  if(len > 0) {

    if(pQ->cfg.prebufOffset + len > pQ->pkts[pQ->idxWr].allocSz) {
      rc = 0;
      pQ->pkts[pQ->idxWr].len = 0;
      if(len > pQ->cfg.growMaxPktLen || (rc = realloc_slot(pQ, pQ->idxWr, len))  <= 0) {

        LOG(X_WARNING("pkt queue(id:%d) wr[%u] too small  %u / %u, resize %s"),
              pQ->cfg.id, pQ->idxWr, len, pQ->pkts[pQ->idxWr].allocSz,
              (rc > 0 ? "ok" : (rc < 0 ? "error" : "none")));
        qlock(pQ, 0);
        return PKTQUEUE_RC_SIZETOOBIG;
      }
    }

    memcpy(pQ->pkts[pQ->idxWr].pData, pData, len);
    pQ->pkts[pQ->idxWr].len = len;
    pQ->pkts[pQ->idxWr].flags = (PKTQUEUE_FLAG_HAVEPKTDATA | PKTQUEUE_FLAG_HAVECOMPLETE);
    if(keyframe) {
      pQ->pkts[pQ->idxWr].flags |= PKTQUEUE_FLAG_KEYFRAME;
    }

    if(pQ->haveRdr && pQ->pstats) {
      pQ->pstats->written.bytes += len;
      pktqueue_addburstsample(pQ->pstats->bitratesWr, &pQ->pstats->tmLastWr, len, pXtra);
    }
    if(pXtra) {
      memcpy(&pQ->pkts[pQ->idxWr].xtra, pXtra, sizeof(pQ->pkts[pQ->idxWr].xtra));
      pQ->pkts[pQ->idxWr].xtra.pQUserData = NULL;
      pQ->ptsLastWr = pXtra->tm.pts;
      
      if(pQ->cfg.userDataLen > 0 && pXtra->pQUserData) {
        pQ->pkts[pQ->idxWr].xtra.pQUserData = pktqueue_getUserData(&pQ->pkts[pQ->idxWr], &pQ->cfg);
        memcpy(pQ->pkts[pQ->idxWr].xtra.pQUserData, pXtra->pQUserData,  pQ->cfg.userDataLen);
      }
    }

  }

  if(len > 0) {
    if(pQ->haveRdr) {
      if(pQ->pstats) {
        pQ->pstats->written.slots++;
      }
      PKTQ_ADVANCE_WR(pQ);
    }
    pQ->pkts[pQ->idxWr].idx = ++pQ->uniqueWrIdx;
  } 
  pQ->haveData = 1;

  //LOG(X_DEBUG("queue id:%d copied %d into wr:[%d] (%d) rdr:[%d] (%d), uniqueWrIdx:%u, haveRdr:%d"), pQ->cfg.id, len, pQ->idxWr, pQ->idxInWr, pQ->idxRd, pQ->idxInRd, pQ->uniqueWrIdx+1,pQ->haveRdr); if(pQ->pstats) LOG(X_DEBUG("written:%d,%d, read:%d,%d"), pQ->pstats->written.bytes, pQ->pstats->written.slots, pQ->pstats->read.bytes, pQ->pstats->read.slots);

  if(pQ->cfg.uselock) {
    wakeup_listeners(pQ);
  }

  qlock(pQ, 0);

  return PKTQUEUE_RC_OK;
}

static void set_rdr_pos(PKTQUEUE_T *pQ) {

  pQ->idxRd = pQ->idxWr;
  pQ->haveRdr = 1;

  //fprintf(stderr, "set_rdr_pos q[%d] idxRd:%d\n", pQ->cfg.id, pQ->idxRd);

  if(pQ->pkts[pQ->idxWr].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    PKTQ_ADVANCE_WR(pQ);
  }

}

void pktqueue_setrdr(PKTQUEUE_T *pQ, int force) {
  
  if(pQ) {
    qlock(pQ, 1);

    if(!pQ->haveRdr) {
      set_rdr_pos(pQ);
    } else if(force) {

      // Leave up to the last 'force' elements in the queue
      int count = 0;
      if(pQ->idxWr > pQ->idxRd) {
        count = pQ->idxWr - pQ->idxRd;
      } else if(pQ->idxWr < pQ->idxRd) {
        count = pQ->cfg.maxPkts - pQ->idxRd + pQ->idxWr;
      }
      while(count-- > force) {
        pQ->pkts[pQ->idxRd].flags = 0;
        PKTQ_ADVANCE_RD(pQ);
      }
    }

    pQ->haveRdrSetManually = 1;

    qlock(pQ, 0);
  }
}

const PKTQUEUE_PKT_T *pktqueue_readpktdirect(PKTQUEUE_T *pQ) {
  int newrdr = 0;
  const PKTQUEUE_PKT_T *pPkt = NULL;

  //fprintf(stderr, "readpktdirect qid:%d rd:%d wr:%d haveRdr:%d qtype:%d\n", pQ->cfg.id, pQ->idxRd, pQ->idxWr, pQ->haveRdr, pQ->cfg.type);

  if(!pQ || pQ->quitRequested || pQ->inDirectRd) {
    return NULL;
  }

  if(pQ->cfg.type == PKTQ_TYPE_FRAMEBUF) {
    return framebuf_readdirect(pQ);
  } else if(pQ->cfg.type != PKTQ_TYPE_DEFAULT) {
    return NULL;
  }

  qlock(pQ, 1);

  if(!pQ->haveRdr) {
    newrdr = 1;
    set_rdr_pos(pQ);
  }

  if(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    pPkt = &pQ->pkts[pQ->idxRd];  
    pQ->inDirectRd = 1;

    if(pQ->pstats) {
      if(newrdr) {
        pQ->pstats->written.bytes += pPkt->len;
        pQ->pstats->written.slots++;
      }
      pQ->pstats->read.bytes += pPkt->len;
      pktqueue_addburstsample(pQ->pstats->bitratesRd, &pQ->pstats->tmLastRd, pPkt->len, &pPkt->xtra);
    }

  } else {

   LOG(X_ERROR("readpktdirect nothing to read, q(id:%d) rd[%d], idxInRd:%d/%d, wr[%d]"
               ", haveData:%d, uniq:%d"),
       pQ->cfg.id, pQ->idxRd, pQ->idxInRd, pQ->pkts[pQ->idxRd].len, pQ->idxWr, 
       pQ->haveData, pQ->uniqueWrIdx);

    qlock(pQ, 0);
  }

  return pPkt;
}

int pktqueue_readpktdirect_done(PKTQUEUE_T *pQ) {

  if(!pQ || !pQ->inDirectRd) {
    return -1;
  }

  if(pQ->cfg.type == PKTQ_TYPE_FRAMEBUF) {
    return framebuf_readdirect_done(pQ);
  } else if(pQ->cfg.type != PKTQ_TYPE_DEFAULT) {
    return PKTQUEUE_RC_ERROR;
  }

  pQ->pkts[pQ->idxRd].flags = 0;
  if(pQ->pstats) {
    pQ->pstats->read.slots++;
  }
  PKTQ_ADVANCE_RD(pQ);

  if(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    pQ->haveData = 1;
  } else if(pQ->haveData) {
    pQ->haveData = 0;
  }

  pQ->inDirectRd = 0;
  qlock(pQ, 0);

  return 0;
}

int pktqueue_swapreadslots(PKTQUEUE_T *pQ, PKTQUEUE_PKT_T **ppSlot) {
  PKTQUEUE_PKT_T *pSlotGoingIn = NULL;
  PKTQUEUE_PKT_T pkt;

  if(!pQ || !ppSlot) {
    return -1;
  }

  if(!pQ->inDirectRd) {
    qlock(pQ, 1);
  }

  if(!(*ppSlot)) {

    if(!pQ->pktTmp) {
      LOG(X_ERROR("Unable to swap pkt slots"));
      if(!pQ->inDirectRd) {
        qlock(pQ, 0);
      }
      return -1;
    }
    pSlotGoingIn = *ppSlot = pQ->pktTmp;
    pQ->ownTmpPkt = 0;
  } else {
    pSlotGoingIn = *ppSlot;
  }

  if(!pSlotGoingIn->pBuf) {
    if(!pQ->inDirectRd) {
      qlock(pQ, 0);
    }
    return -1;
  } 

  memcpy(&pkt, &pQ->pkts[pQ->idxRd], sizeof(PKTQUEUE_PKT_T));
  pSlotGoingIn->len = 0;
  pSlotGoingIn->flags = 0;
  memcpy(&pQ->pkts[pQ->idxRd], pSlotGoingIn, sizeof(PKTQUEUE_PKT_T));
  memcpy(*ppSlot, &pkt, sizeof(PKTQUEUE_PKT_T));

  if(!pQ->inDirectRd) {
    qlock(pQ, 0);
  }

  return 0;
}

int pktqueue_lock(PKTQUEUE_T *pQ, int lock) {

  if(!pQ) {
    return -1;
  }

  qlock(pQ, lock);

  return 0;
}
  
int pktqueue_readpkt(PKTQUEUE_T *pQ, unsigned char *pData, 
                                  unsigned int len, PKTQ_EXTRADATA_T *pXtra, 
                                  unsigned int *pNumOverwritten) {

  int newrdr = 0;
  unsigned int lenPkt = 0;
  PKTQUEUE_PKT_T *pPkt = NULL;

  if(!pQ || !pData || pQ->quitRequested) {
    return (int) PKTQUEUE_RC_ERROR;
  }

  //fprintf(stderr, "pktqueue_readpkt qid:%d rd:%d wr:%d haveRdr:%d qtype:%d\n", pQ->cfg.id, pQ->idxRd, pQ->idxWr, pQ->haveRdr, pQ->cfg.type);

  if(pQ->cfg.type == PKTQ_TYPE_FRAMEBUF) {

    if(pNumOverwritten) {
      *pNumOverwritten = 0;
    }
    return framebuf_read(pQ, pData, len, pXtra);

  } else if(pQ->cfg.type == PKTQ_TYPE_RINGBUF) {

    if(pNumOverwritten) {
      *pNumOverwritten = pQ->numOverwritten;
    }

    return ringbuf_read(pQ, pData, len);

  } else if(pQ->cfg.type != PKTQ_TYPE_DEFAULT) {
    return PKTQUEUE_RC_ERROR;
  }

  qlock(pQ, 1);

  if(!pQ->haveRdr) {
    newrdr = 1;
    set_rdr_pos(pQ);
  }

  pPkt = &pQ->pkts[pQ->idxRd];

  if(pPkt->flags & PKTQUEUE_FLAG_HAVECOMPLETE) {

    if((lenPkt = pPkt->len) > len) {
      LOG(X_ERROR("queue len request for %d < %d"), len, pPkt->len);
      qlock(pQ, 0);
      return (int) PKTQUEUE_RC_ERROR;
    }
    //fprintf(stderr, "pktqueue_readpkt len:%d pts:%.3f %s\n", pPkt->len, PTSF(pPkt->xtra.tm.pts), (pPkt->flags & PKTQUEUE_FLAG_KEYFRAME ? "KEYFRAME" : ""));
    memcpy(pData, pPkt->pData, lenPkt);
    if(pQ->pstats) {
      if(newrdr) {
        pQ->pstats->written.bytes += lenPkt;
        pQ->pstats->written.slots++;
      }
      pQ->pstats->read.bytes += lenPkt;
      pQ->pstats->read.slots++;
      pktqueue_addburstsample(pQ->pstats->bitratesRd, &pQ->pstats->tmLastRd, lenPkt, pXtra ? &pPkt->xtra : NULL);
    }
    pPkt->flags = 0;
    pPkt->len = 0;

    if(pXtra) {
      if(pXtra->pQUserData && pPkt->xtra.pQUserData && pQ->cfg.userDataLen > 0) {
        memcpy(pXtra->pQUserData, pPkt->xtra.pQUserData, pQ->cfg.userDataLen);
      } else {
        pXtra->pQUserData = NULL;
      }
      memcpy(pXtra, &pPkt->xtra, sizeof(PKTQ_EXTRADATA_T) - sizeof(void *));
    }

    if(pNumOverwritten) {
      *pNumOverwritten = pQ->numOverwritten;
    }

    PKTQ_ADVANCE_RD(pQ);

    if(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
      pQ->haveData = 1;
    } else if(pQ->haveData) {
      pQ->haveData = 0;
    }

  }

  qlock(pQ, 0);
 
  return lenPkt;
}


int pktqueue_readexact(PKTQUEUE_T *pQ, unsigned char *pData, unsigned int lenTot) {

  unsigned int idxData = 0;
  unsigned int lenReadPkt = 0;
  unsigned int lenToRead = lenTot;

  if(!pQ || !pData || pQ->quitRequested) {
    return -1;
  }

  if(pQ->cfg.type != PKTQ_TYPE_DEFAULT) {
    return PKTQUEUE_RC_ERROR;
  }

  qlock(pQ, 1);

  if(!pQ->haveRdr) {
    set_rdr_pos(pQ);
  }

  while(lenToRead > 0) {

    if(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    
      lenReadPkt = lenToRead;
      if(lenReadPkt > pQ->pkts[pQ->idxRd].len - pQ->idxInRd) {
        lenReadPkt = pQ->pkts[pQ->idxRd].len - pQ->idxInRd;
      }

      memcpy(&pData[idxData], &pQ->pkts[pQ->idxRd].pData[pQ->idxInRd], lenReadPkt);

//fprintf(stderr, "queue read_exact copied %d from [%d] to buf[%d], idxInRd:%d, sz:%d/%d, requested:%d\n", lenReadPkt, pQ->idxRd, idxData, pQ->idxInRd, pQ->idxInRd, pQ->pkts[pQ->idxRd].len, lenTot);
      lenToRead -= lenReadPkt;
      idxData += lenReadPkt;
      pQ->idxInRd += lenReadPkt;


      if(pQ->idxInRd >= pQ->pkts[pQ->idxRd].len) {
        pQ->pkts[pQ->idxRd].flags = 0;
        pQ->pkts[pQ->idxRd].len = 0;
        PKTQ_ADVANCE_RD(pQ);

        if(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
          pQ->haveData = 1;
        } else {
          pQ->haveData = 0;
        }
      }

//fprintf(stderr, "queue read is now [%d]\n", pQ->idxRd);

    } else {
      pQ->haveData = 0;
      break;
    }

  }

  //pQ->haveData = 0;


//fprintf(stderr, "readexact returning %d/%d rd:%d(%d) wr:%d(%d)\n", lenTot - lenToRead, lenTot, pQ->idxRd, pQ->idxInRd, pQ->idxWr, pQ->idxInWr);
//if(lenTot-lenToRead>0 && pData[0] !=0x47)fprintf(stderr, "NO START CODE 0x%x 0x%x\n", pData[0], pData[1]);


  qlock(pQ, 0);

  return (lenTot - lenToRead);
}

//static TIME_VAL sl1;
//static TIME_VAL sl2;
int pktqueue_waitforunreaddata(PKTQUEUE_T *pQ) {

  qlock(pQ, 1);

  if(pQ->haveData) {
    qlock(pQ, 0);
    return 1;
  }  else if(pQ->quitRequested) {
    qlock(pQ, 0);
    return 1;
  }

  qlock(pQ, 0);

  g_proc_exit_cond = &pQ->notify.cond;
  vsxlib_cond_wait(&pQ->notify.cond, &pQ->notify.mtx);
  g_proc_exit_cond = NULL;

  return 1;
}

int pktqueue_reset(PKTQUEUE_T *pQ, int lock) {

  if(lock) {
    qlock(pQ, 1);
  }

  reset(pQ);

  if(lock) {
    qlock(pQ, 0);
  }

  return 0;
}

int pktqueue_havepkt(PKTQUEUE_T *pQ) {
  int rc = 0;

  if(!pQ) {
    return 0;
  }
  qlock(pQ, 1);

  if(pQ->haveData) {
    if(pQ->cfg.type == PKTQ_TYPE_RINGBUF) {
      rc = pQ->pkts[0].len;
    } else {
      //rc = pQ->uniqueWrIdx - 1;
      rc = pQ->uniqueWrIdx;
    }
  }

  qlock(pQ, 0);

  return rc;
}

int pktqueue_getstats(PKTQUEUE_T *pQ, PKTQUEUE_STATS_REPORT_T *pReport) {
  int rc = 0;

  if(!pQ || !pQ->pstats || !pReport) {
    return -1;
  }

  qlock(pQ, 1);

  memcpy(&pReport->stats, pQ->pstats, sizeof(pReport->stats));
  //memcpy(&pReport->tmEarliest, &pQ->pkts[pQ->idxRd].xtra.tm, sizeof(pReport->tmEarliest));
  //memcpy(&pReport->tmLatest, &pQ->pkts[pQ->idxWr].xtra.tm, sizeof(pReport->tmLatest));

  qlock(pQ, 0);

  return rc;
}

PKTQUEUE_T *framebuf_create(unsigned int sz, unsigned int prebufOffset) {
  PKTQUEUE_T *pFb;
                          
  if(!(pFb = pktqueue_create(2, sz, 0, 0, prebufOffset, 0, 0))) {
    return NULL;
  }

  pFb->cfg.type = PKTQ_TYPE_FRAMEBUF;

  return pFb;
}

PKTQUEUE_T *ringbuf_create(unsigned int sz) {
  PKTQUEUE_T *pRb;

  if(!(pRb = pktqueue_create(1, sz, 0, 0, 0, 0, 1))) {
    return NULL;
  }

  pRb->cfg.type = PKTQ_TYPE_RINGBUF;

  return pRb;
}



static enum PKTQUEUE_RC framebuf_add(PKTQUEUE_T *pFb, const unsigned char *pData, 
                                    unsigned int len, PKTQ_EXTRADATA_T *pXtra) {
  int overwrote = 0;

  if(!pFb) {
    return PKTQUEUE_RC_ERROR;
  }

  pthread_mutex_lock(&pFb->mtxWr);

  pthread_mutex_lock(&pFb->mtx);

  if(pFb->idxRd == pFb->idxWr) {
    PKTQ_ADVANCE_WR(pFb);
  }

  if(pFb->pkts[pFb->idxWr].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    overwrote = 1;
  }

  pFb->pkts[pFb->idxWr].flags = 0;
  pFb->pkts[pFb->idxWr].len = 0;

  //fprintf(stderr, "framebuf_add len:%d flags:0x%x 0x%x wr:%d, rd:%d\n", len, pFb->pkts[0].flags, pFb->pkts[1].flags, pFb->idxWr, pFb->idxRd);

  pthread_mutex_unlock(&pFb->mtx);

  if(len > MAX(pFb->cfg.maxPktLen, pFb->cfg.growMaxPktLen)) {
    LOG(X_WARNING("framebuf(id:%d) wr[%u] too small  %u / %u, resize %s"),
      pFb->cfg.id, pFb->idxWr, len, pFb->pkts[pFb->idxWr].allocSz,
      (pFb->cfg.growMaxPktLen > 0 ? "ok" : "none"));
    pthread_mutex_unlock(&pFb->mtxWr);
    return PKTQUEUE_RC_SIZETOOBIG;
  }

  memcpy(pFb->pkts[pFb->idxWr].pData, pData, len);
  pFb->pkts[pFb->idxWr].len = len;
  pFb->pkts[pFb->idxWr].idx = ++pFb->uniqueWrIdx;
  if(pXtra) {
    memcpy(&pFb->pkts[pFb->idxWr].xtra, pXtra, sizeof(pFb->pkts[pFb->idxWr].xtra));
  }

  pthread_mutex_lock(&pFb->mtx);
  pFb->pkts[pFb->idxWr].flags |= PKTQUEUE_FLAG_HAVECOMPLETE;
  pFb->haveData = 1;
  pthread_mutex_unlock(&pFb->mtx);

  VSX_DEBUGLOG3("framebuf_write idx:%d haveD:%d flg:0x%x 0x%x uniq:%d\n", pFb->idxWr, pFb->haveData, pFb->pkts[0].flags, pFb->pkts[1].flags, pFb->uniqueWrIdx);

  wakeup_listeners(pFb);

  pthread_mutex_unlock(&pFb->mtxWr);

  if(overwrote) {
    //fprintf(stderr, "%lld framebuf overwrote slot\n",  timer_GetTime() / TIME_VAL_MS);
    LOG(X_DEBUG("framebuf(id:%d) overwrote slot"), pFb->cfg.id);
  }

  return PKTQUEUE_RC_OK;
}

static const PKTQUEUE_PKT_T *framebuf_readdirect(PKTQUEUE_T *pFb) {
  const PKTQUEUE_PKT_T *pPkt = NULL;
  int isreading = 0;

  if(!pFb || pFb->quitRequested || pFb->inDirectRd) {
    return NULL;
  }

  pthread_mutex_lock(&pFb->mtxRd);

  if(!pFb->haveRdr) {
    pFb->haveRdr = 1;
    //set_rdr_pos(pQ);
  }

  pthread_mutex_lock(&pFb->mtx);

  if(pFb->pkts[pFb->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    isreading = 1;
  } else {
    PKTQ_ADVANCE_RD(pFb);
    if(pFb->pkts[pFb->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
      isreading = 1;
    }
  }

  //fprintf(stderr, "framebuf_readdirect flags:0x%x 0x%x wr:%d, rd:%d havedata:%d isreading:%d\n", pFb->pkts[0].flags, pFb->pkts[1].flags, pFb->idxWr, pFb->idxRd, pFb->haveData, isreading);

  pthread_mutex_unlock(&pFb->mtx);

  if(isreading) {
    pPkt = &pFb->pkts[pFb->idxRd];
    pFb->inDirectRd = 1;
  
    //fprintf(stderr, "%lld framebuf read direct\n", timer_GetTime() / TIME_VAL_MS);
  } else {
    LOG(X_WARNING("framebuf_readdirect nothing to read,  framebuf(id:%d) rd[%d], idxInRd:%d/%d, wr[%d]"), 
      pFb->cfg.id, pFb->idxRd, pFb->idxInRd, pFb->pkts[pFb->idxRd].len, pFb->idxWr);
    pthread_mutex_unlock(&pFb->mtxRd);
  }

  return pPkt;
}

static int framebuf_readdirect_done(PKTQUEUE_T *pFb) {

  if(!pFb || !pFb->inDirectRd) {
    return -1;
  }

  pFb->pkts[pFb->idxRd].flags = 0;

  if((pFb->pkts[0].flags & PKTQUEUE_FLAG_HAVECOMPLETE) ||
     (pFb->pkts[1].flags & PKTQUEUE_FLAG_HAVECOMPLETE)) {
    pFb->haveData = 1;
  } else if(pFb->haveData) {
    pFb->haveData = 0;
  }

  VSX_DEBUGLOG3("framebuf_readdirect_done idx:%d haveD:%d flg:0x%x 0x%x\n", pFb->idxRd, pFb->haveData, pFb->pkts[0].flags, pFb->pkts[1].flags);

  pFb->inDirectRd = 0;
  pthread_mutex_unlock(&pFb->mtxRd);

  return 0;
}

static int framebuf_read(PKTQUEUE_T *pFb, unsigned char *pData, unsigned int len,
                          PKTQ_EXTRADATA_T *pXtra) {
  unsigned int lenPkt = 0;
  int isreading = 0;

  if(!pFb) {
    return -1;
  }

  pthread_mutex_lock(&pFb->mtxRd);

  if(!pFb->haveRdr) {
    pFb->haveRdr = 1;
  }

  pthread_mutex_lock(&pFb->mtx);

  if(pFb->pkts[pFb->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
    isreading = 1;
  } else {
    PKTQ_ADVANCE_RD(pFb);
    if(pFb->pkts[pFb->idxRd].flags & PKTQUEUE_FLAG_HAVECOMPLETE) {
      isreading = 1;
    }
  }

  if((pFb->pkts[pFb->idxRd == 0 ? 1 : 0].flags & PKTQUEUE_FLAG_HAVECOMPLETE)) { 
    pFb->haveData = 1;
  } else if(pFb->haveData) {
    pFb->haveData = 0;
  }

  pthread_mutex_unlock(&pFb->mtx);

  if(!isreading) {
    pthread_mutex_unlock(&pFb->mtxRd);
    return 0;
  }

  if((lenPkt = pFb->pkts[pFb->idxRd].len) > len) {
    LOG(X_ERROR("framebuf len request for %d < %d"), len, pFb->pkts[pFb->idxRd].len);
    pthread_mutex_unlock(&pFb->mtxRd);
    return -1;
  }

  memcpy(pData, pFb->pkts[pFb->idxRd].pData, lenPkt);
  pFb->pkts[pFb->idxRd].flags = 0;
  pFb->pkts[pFb->idxRd].len = 0;

  if(pXtra) {
    memcpy(pXtra, &pFb->pkts[pFb->idxRd].xtra, sizeof(PKTQ_EXTRADATA_T));
  }

  // VSX_DEBUGLOG3("read framebuf idx:%d haveD:%d flg:0x%x 0x%x\n", pFb->idxRd, pFb->haveData, pFb->pkts[0].flags, pFb->pkts[1].flags);

  pthread_mutex_unlock(&pFb->mtxRd);

  //fprintf(stderr, "framebuf read slot\n");

  return lenPkt;
}


static int ringbuf_write(PKTQUEUE_T *pQ, const unsigned char *pBuf, 
                         unsigned int szin) {
  unsigned int idxStart = 0;
  unsigned int szleft;
  unsigned int numoverwritten = 0;
  unsigned int numwritten = 0;
  unsigned int sz = szin;

  if(!pQ || !pBuf) {
    return -1;
  } else if(sz == 0) {
    return 0;
  }

  qlock(pQ, 1);

  if(!pQ->haveRdr) {
    pQ->pkts[0].len = 0;
    pQ->idxInWr = 0; 
    pQ->idxInRd = 0; 
  }

  //LOG(X_DEBUG("ringbuf_write sz:%d, rd:%d, wr:%d"), szin, pQ->idxInRd, pQ->idxInWr);

  if(sz > pQ->cfg.maxPktLen) {
    LOG(X_WARNING("ringbuf (id:%d) write size %d truncated to max size %d"), 
        pQ->cfg.id, sz, pQ->cfg.maxPktLen);
    idxStart = sz - pQ->cfg.maxPktLen;
    sz = pQ->cfg.maxPktLen;
    pQ->idxInWr = 0;
  }

  if((szleft = pQ->cfg.maxPktLen - pQ->idxInWr) > sz) {
    szleft = sz;
  }
  memcpy(&pQ->pkts[0].pData[pQ->idxInWr], &pBuf[idxStart], szleft);
  numwritten = szleft;
  if(pQ->pkts[0].len > 0 && 
     pQ->idxInRd >= pQ->idxInWr && pQ->idxInRd <= pQ->idxInWr + szleft) {
    numoverwritten += szleft - (pQ->idxInRd - pQ->idxInWr);
    if((pQ->idxInRd += szleft - (pQ->idxInRd - pQ->idxInWr)) >= pQ->cfg.maxPktLen) {
      pQ->idxInRd = 0;
    }
  }
  if((pQ->pkts[0].len += szleft) > pQ->cfg.maxPktLen) {
    pQ->pkts[0].len = pQ->cfg.maxPktLen;
  }
  if((pQ->idxInWr += szleft) >= pQ->cfg.maxPktLen) {
    pQ->idxInWr = 0;
  }
  sz -= szleft;

  if(sz > 0) {
    memcpy(pQ->pkts[0].pData, &pBuf[idxStart + numwritten], sz);
    numwritten += sz;
    pQ->pkts[0].len += sz;
    if(pQ->idxInRd < sz) {
      numoverwritten += (sz - pQ->idxInRd);
      pQ->idxInRd = sz;
      if((pQ->pkts[0].len += sz) > pQ->cfg.maxPktLen) {
        pQ->pkts[0].len = pQ->cfg.maxPktLen;
      }
    }
    pQ->idxInWr = sz;
  }

  if(sz > pQ->cfg.maxPktLen) {
    pQ->idxInRd = 0; 
  }
  
  if(!pQ->haveData) {
    pQ->haveData = 1;
    if(pQ->cfg.uselock) {
      wakeup_listeners(pQ);
    }
  }

  qlock(pQ, 0);

  if(numoverwritten > 0 && pQ->haveRdr) {
    pQ->numOverwritten += numoverwritten;
    LOG(X_WARNING("ringbuf (id:%d) overwrote %d/%d bytes, rd:%d, wr:%d, max:%d"), 
        pQ->cfg.id, numoverwritten, szin, 
        pQ->idxInRd, pQ->idxInWr, pQ->cfg.maxPktLen);
  }

  return (int) numwritten;
}

static int ringbuf_read(PKTQUEUE_T *pQ, unsigned char *pBuf, unsigned int sz) {
  unsigned int szread;
  unsigned int idxread = 0;

  qlock(pQ, 1);

  //LOG(X_DEBUG("ringbuf_read sz:%d, rd:%d, wr:%d"), sz, pQ->idxInRd, pQ->idxInWr);

  if(!pQ->haveRdr) {
    pQ->haveRdr = 1;
  }

  if(pQ->pkts[0].len == 0) {
    qlock(pQ, 0);
    return 0;
  }

  if(sz > pQ->pkts[0].len) {
    sz = pQ->pkts[0].len;
  }

  szread = sz;
  if(pQ->idxInRd + szread > pQ->cfg.maxPktLen) {
    szread = pQ->cfg.maxPktLen - pQ->idxInRd;
  }
  if(szread > pQ->pkts[0].len) {
    szread = pQ->pkts[0].len;
  }
  memcpy(&pBuf[idxread], &pQ->pkts[0].pData[pQ->idxInRd], szread);
  pQ->pkts[0].len -= szread;
  if((pQ->idxInRd += szread) >= pQ->cfg.maxPktLen) {
    pQ->idxInRd = 0;
  }
  sz -= szread;
  idxread += szread;
  //LOG(X_DEBUG("read %d from ending rdr at[%d] wr at[%d] len:%d"), szread, pQ->idxInRd, pQ->idxInWr, pQ->pkts[0].len);

  if(pQ->pkts[0].len > 0 && sz > 0) {
    szread = sz;
    if(szread > pQ->pkts[0].len) {
      szread = pQ->pkts[0].len;
    }
    memcpy(&pBuf[idxread], pQ->pkts[0].pData, szread);
    pQ->pkts[0].len -= szread;
    pQ->idxInRd = szread;
    idxread += szread;
  //fprintf(stderr, "reading [%d] from wrap-around %d %d rdr at [%d]\n", idxread, sz, pQ->pkts[0].len, pQ->idxInRd);
  }

  if(pQ->pkts[0].len > 0) {
    pQ->haveData = 1;
  } else {
    pQ->haveData = 0;
  }

  //LOG(X_DEBUG("ringbuf_read returning idxread[%d] pktlen now %d rd:%d wr:%d"), idxread, pQ->pkts[0].len, pQ->idxInRd, pQ->idxInWr);

  qlock(pQ, 0);

  return (int) idxread;
}

#if 0

void printBuf(unsigned char *p, unsigned int len) {
  int i;

  fprintf(stderr, "read:%d\n", len);
  for(i = 0; i < len; i++)  {
    fprintf(stderr, "%3d ", p[i]);
    if(i % 8 == 7)
      fprintf(stderr, "\n");
  }
}

void printQ(PKTQUEUE_T *pQ, int rc) {
  int i;

  fprintf(stderr, "wrote:%d rd[%d]:%d, wr[%d]:%d len:%d\n", rc, pQ->idxRd, pQ->idxInRd, pQ->idxWr, pQ->idxInWr, pQ->pkts[0].len);
  for(i = 0; i < pQ->cfg.maxPktLen; i++)  {
    fprintf(stderr, "%3d ", pQ->pkts[0].pData[i]);
    if(i % 8 == 7)
      fprintf(stderr, "\n");
  }
}

int testringbuf() {
  unsigned char buf[1024];
  unsigned char rdbuf[1024];
  int idx=0;
  int idxrd=0;
  int i;
  PKTQUEUE_T *pQ;  

  for(i = 0; i < sizeof(buf); i++) buf[i] = i; 
  
  pQ = ringbuf_create(32);  

  i = ringbuf_write(pQ, &buf[idx], 33);
  idx += 33;
  printQ(pQ, i);

  i = ringbuf_write(pQ, &buf[idx], 6);
  idx += 6;
  printQ(pQ, i);

  i = ringbuf_read(pQ, &rdbuf[idxrd], 24);
  printBuf(&rdbuf[idxrd], i);
  i = ringbuf_read(pQ, &rdbuf[idxrd], 4);
  printBuf(&rdbuf[idxrd], i);

  i = ringbuf_write(pQ, &buf[idx], 1);
  idx += 1;
  printQ(pQ, i);

  i = ringbuf_read(pQ, &rdbuf[idxrd], 6);
  printBuf(&rdbuf[idxrd], i);

  i = ringbuf_write(pQ, &buf[idx], 1);
  idx += 1;
  printQ(pQ, i);


  pktqueue_destroy(pQ);

  return 0;
}

#endif // 0
