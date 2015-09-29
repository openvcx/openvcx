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

//#define BITRATE_RESTRICT 1


#if defined BITRATE_RESTRICT
static BURSTMETER_SAMPLE_SET_T g_bs_v;
static BURSTMETER_SAMPLE_SET_T g_bs_a;
static int g_bs_init;
static float g_bs_usleep;
static TIME_VAL gtv0, gtv1;
static unsigned int g_bytes_v, g_bytes_a;
void burstmeter_dump(const BURSTMETER_SAMPLE_SET_T *pSamples);
#endif //BITRATE_RESTRICT
         

static int outfmt_invokeCbsUnbuffured(STREAMER_OUTFMT_LIST_T *pLiveFmts, const OUTFMT_FRAME_DATA_T *pFrame);

static void gop_history_onFrame(GOP_HISTORY_CTXT_T *pCtxt, int keyframe, uint64_t pts) {
  unsigned int idx, cnt;

  if(keyframe) {

    if(pCtxt->keyframeCnt++ > 0) {

      //
      // Calculate the average GOP size over the past GOP_HISTORY_COUNT GOPs
      //
      pCtxt->gopAvg = 0;
      pCtxt->gopAvgPtsDuration = 0;
      pCtxt->gopTms[pCtxt->gopIdx] = pts - pCtxt->lastKeyframePts;
      idx = pCtxt->gopIdx;
      for(cnt = MIN(pCtxt->keyframeCnt - 1, GOP_HISTORY_COUNT); cnt > 0; cnt--) {
        //LOG(X_DEBUG("gop[%d]:%d, duration:%.3f pCtxt->gopIdx:%d"), idx, pCtxt->gops[idx], PTSF(pCtxt->gopTms[idx]), pCtxt->gopIdx);
        pCtxt->gopAvg += pCtxt->gops[idx];
        pCtxt->gopAvgPtsDuration += pCtxt->gopTms[idx];
        idx = idx > 0 ? idx -1 : GOP_HISTORY_COUNT - 1;
      }
      pCtxt->gopAvg /= (MIN(pCtxt->keyframeCnt - 1, GOP_HISTORY_COUNT));
      pCtxt->gopAvgPtsDuration /= (MIN(pCtxt->keyframeCnt - 1, GOP_HISTORY_COUNT));

      //LOG(X_DEBUG("GOP gop[%d]:%d, avg:%.3f, tm:%.3f (avg:%.3f) keyframeCnt:%d"), pCtxt->gopIdx, pCtxt->gops[pCtxt->gopIdx], pCtxt->gopAvg, PTSF(pCtxt->gopTms[pCtxt->gopIdx]), PTSF(pCtxt->gopAvgPtsDuration), pCtxt->keyframeCnt);

      if(++pCtxt->gopIdx >= GOP_HISTORY_COUNT) {
        pCtxt->gopIdx = 0;
      }

    }

    pCtxt->gops[pCtxt->gopIdx] = 1;
    pCtxt->lastKeyframePts = pts;

  } else {
    pCtxt->gops[pCtxt->gopIdx]++;
  }

}

#define FRAME_THIN_HIGH_WATERMARK  .50f
#define FRAME_THIN_HIGH_WATERMARK2 .34f
#define FRAME_THIN_HIGH_WATERMARK3 .24f
#define FRAME_THIN_LOW_WATERMARK   .08f
#define FRAME_THIN_Q_LATENCY_MS    4000

typedef struct FRAME_THIN_CTXT {
  int isinit;
  float targetGopSz;
  unsigned int gopSz;
  BURSTMETER_SAMPLE_SET_T burstMeterVid;
  BURSTMETER_SAMPLE_SET_T burstMeterAud;
  TIME_VAL tmLastTargetGopChange;
  unsigned int keyframeCntLastTargetGopChange;
  int dropUntilKeyframe;
  int lastGopSkipped;
  uint64_t ptsLastLowWatermark;
  uint64_t ptsLastHighWatermark;
  int64_t deltaLastPtsContents;
  const OUTFMT_CFG_T *pOutFmt;
  const GOP_HISTORY_CTXT_T *pGOPHistory;
} FRAME_THIN_CTXT_T;

static int framethin_init(FRAME_THIN_CTXT_T *pCtxt) {
  int rc = 0;

  if(!pCtxt) {
    return -1;
  }

  if((rc = burstmeter_init(&pCtxt->burstMeterVid, 500, 6000)) < 0 ||
     (rc = burstmeter_init(&pCtxt->burstMeterAud, 500, 6000) < 0)) {
  }

  return rc;
}

static void framethin_close(FRAME_THIN_CTXT_T *pCtxt) {

  if(pCtxt) {
    burstmeter_close(&pCtxt->burstMeterVid);
    burstmeter_close(&pCtxt->burstMeterAud);
  }

}

FRAME_THIN_CTXT_T *framethin_alloc() {
  FRAME_THIN_CTXT_T *pCtxt = NULL;

  if(!(pCtxt = (FRAME_THIN_CTXT_T *) avc_calloc(1, sizeof(FRAME_THIN_CTXT_T)))) {
    return NULL;
  }

  if(framethin_init(pCtxt) < 0) {
    avc_free((void **) &pCtxt);
    pCtxt = NULL; 
  }

  return pCtxt;
}

void framethin_destroy(FRAME_THIN_CTXT_T *pCtxt) {

  if(pCtxt) {
    framethin_close(pCtxt);
    avc_free((void **) &pCtxt);
  }
}

static void framethin_newTargetGOP(FRAME_THIN_CTXT_T *pCtxt, float targetGopSz) {
  TIME_VAL tvNow;
 
  tvNow = timer_GetTime();

  if((tvNow - pCtxt->tmLastTargetGopChange) / TIME_VAL_MS < 700) {
    return;
  } else if(pCtxt->keyframeCntLastTargetGopChange == pCtxt->pGOPHistory->keyframeCnt && 
            pCtxt->targetGopSz > 0 && targetGopSz > pCtxt->targetGopSz) {
    return;
  }

  if(targetGopSz == 0 || pCtxt->targetGopSz != targetGopSz) {

    if(targetGopSz >= 1.0f && targetGopSz < pCtxt->targetGopSz) {
      pCtxt->dropUntilKeyframe = 1;
    } else {
      pCtxt->dropUntilKeyframe = 0;
    }
    if(pCtxt->targetGopSz != targetGopSz) {
      LOG(X_DEBUG("framethin set target-GOP from %.2f -> %.2f (avg-GOP:%.1f), keys:%d, drop-until-key:%d, delta-last-pts-contents:%.3f sec"), pCtxt->targetGopSz, targetGopSz, pCtxt->pGOPHistory->gopAvg, pCtxt->pGOPHistory->keyframeCnt, pCtxt->dropUntilKeyframe, PTSF(pCtxt->deltaLastPtsContents));
    }
  }

  pCtxt->targetGopSz = targetGopSz;
  pCtxt->tmLastTargetGopChange = tvNow;
  pCtxt->keyframeCntLastTargetGopChange = pCtxt->pGOPHistory->keyframeCnt;
}


static int gewTargetGopSz(FRAME_THIN_CTXT_T *pCtxt, float factor) {
  float targetGopSz = 0.0f;

  if(pCtxt->targetGopSz > 1.0f) {
    if((targetGopSz = pCtxt->targetGopSz * factor) > pCtxt->pGOPHistory->gopAvg * factor) {
      targetGopSz = pCtxt->pGOPHistory->gopAvg * factor;
    }

  } else {

    targetGopSz = pCtxt->pGOPHistory->gopAvg * factor;
    if(pCtxt->targetGopSz > 0 && targetGopSz > pCtxt->targetGopSz) {
      targetGopSz = 0;
    }
  }

  return targetGopSz;
}

int framethin_onFrame(FRAME_THIN_CTXT_T *pCtxt, const OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;
  int havePtsLastOverwrite = 0;
  int havePtsLastHighWatermark = 0;
  int havePtsLastLowWatermark = 0;
  int64_t deltaPtsLastOverwrite = 0;
  int64_t deltaPtsLastHighWatermark = 0;
  int64_t deltaPtsLastLowWatermark = 0;
  int64_t deltaPtsContents = 0;
  uint64_t ptsLowWatermark = 0;
  uint64_t ptsHighWatermark = 0;
  uint64_t ptsHighWatermark2 = 0;
  uint64_t ptsHighWatermark3 = 0;
  uint64_t ptsLastWr = 0;
  int maxRTlatencyMs;
  float raiseFactor;
  float targetGopSz = 0;

  if(!pCtxt || !pFrame || !pCtxt->pOutFmt || !pCtxt->pOutFmt->pQ || !pCtxt->pGOPHistory) {
    return -1;
  }

  if(!pCtxt->isinit && (rc = framethin_init(pCtxt)) == 0) {
    pCtxt->isinit = 1;
  } else if(rc != 0) {
    return rc;
  }

  if(!pFrame->isvid) {
    burstmeter_AddSample(&pCtxt->burstMeterAud, OUTFMT_LEN(pFrame), NULL);
    return 0;
  }

  //
  // Get the current queue buffered data contents duration
  //
  deltaPtsContents = pktqueue_getAgePts(pCtxt->pOutFmt->pQ, &ptsLastWr);
  if((maxRTlatencyMs = pCtxt->pOutFmt->pQ->cfg.maxRTlatencyMs) == 0) {
    maxRTlatencyMs = FRAME_THIN_Q_LATENCY_MS;
  }
  ptsHighWatermark = FRAME_THIN_HIGH_WATERMARK * ((float) PTS_HZ_MS * maxRTlatencyMs);
  ptsHighWatermark2 = FRAME_THIN_HIGH_WATERMARK2 * ((float) PTS_HZ_MS * maxRTlatencyMs);
  ptsHighWatermark3 = FRAME_THIN_HIGH_WATERMARK3 * ((float) PTS_HZ_MS * maxRTlatencyMs);
  ptsLowWatermark = FRAME_THIN_LOW_WATERMARK * ((float) PTS_HZ_MS * maxRTlatencyMs);

  //LOG(X_DEBUG("deltaPtsContenst:%.3f, deltaLastPtsContents:%.3f, lowWatermarkThreshold:%.3f"), PTSF(deltaPtsContents), PTSF(pCtxt->deltaLastPtsContents), PTSF(.15f * ((float)PTS_HZ_MS * maxRTlatencyMs)));


  //
  // Check if we're above the high-water mark of queue buffered contents
  // This means that we're reading pretty slow, which may soon lead to data overwrites
  //
  if(deltaPtsContents > ptsHighWatermark) {

    pCtxt->ptsLastHighWatermark = ptsLastWr;

  } else if(deltaPtsContents < ptsLowWatermark && pCtxt->deltaLastPtsContents >= ptsLowWatermark) {

    //
    // Check if we've just gone below the low-water mark of queue buffered contents
    // This means that we're now reading faster and have been flushing the queue
    //
    pCtxt->ptsLastLowWatermark = ptsLastWr;

    //LOG(X_DEBUG("SET ptsLastLowWatermark pts:%.3f, deltaLastPtsContents:%.3f"), PTSF(pCtxt->ptsLastLowWatermark), PTSF(pCtxt->deltaLastPtsContents));
  }

  if(pCtxt->ptsLastHighWatermark > 0) {
    deltaPtsLastHighWatermark = ptsLastWr - pCtxt->ptsLastHighWatermark;
    havePtsLastHighWatermark = 1;
  }

  if(pCtxt->ptsLastLowWatermark > 0) {
    deltaPtsLastLowWatermark = ptsLastWr - pCtxt->ptsLastLowWatermark;
    havePtsLastLowWatermark = 1;
  }

  if(pCtxt->pOutFmt->pQ->ptsLastOverwrite > 0) {
    deltaPtsLastOverwrite = ptsLastWr - pCtxt->pOutFmt->pQ->ptsLastOverwrite;
    havePtsLastOverwrite = 1;
  }

  pCtxt->deltaLastPtsContents = deltaPtsContents;

  if(OUTFMT_KEYFRAME(pFrame)) {

    LOG(X_DEBUG("framethin last-GOP-sz:%d, skipped:%d, target-GOP:%.1f (avg-GOP:%.1f), keys:%d, vid: %.1fKb/s, aud: %.1fKb/s, high-water-ago:%d, %.3fsec, low-water-ago:%d, %.3fsec, Q: {delta-pts:%.3fsec, rd:[%d]/%d, wr:[%d]}"), pCtxt->gopSz, pCtxt->lastGopSkipped, pCtxt->targetGopSz, pCtxt->pGOPHistory->gopAvg, pCtxt->pGOPHistory->keyframeCnt, burstmeter_getBitrateBps(&pCtxt->burstMeterVid)/1000.0f, burstmeter_getBitrateBps(&pCtxt->burstMeterAud)/1000.0f, havePtsLastHighWatermark, PTSF(deltaPtsLastHighWatermark), havePtsLastLowWatermark, PTSF(deltaPtsLastLowWatermark), PTSF(deltaPtsContents), pCtxt->pOutFmt->pQ->idxRd, pCtxt->pOutFmt->pQ->cfg.maxPkts, pCtxt->pOutFmt->pQ->idxWr);


#if defined BITRATE_RESTRICT
    LOG(X_DEBUG("framethin output bitrate vid: %.3f kb/s, aud: %.3f Kb/s"), burstmeter_getBitrateBps(&g_bs_v)/1000.0f, burstmeter_getBitrateBps(&g_bs_a)/1000.0f);
#endif // BITRATE_RESTRICT

    //
    // Always let a key-frame through
    // TODO: if we're still not reading fast enough, should go audio-only from time-to-time
    //
    rc = 0;
    pCtxt->gopSz = 1;
    pCtxt->dropUntilKeyframe = 0;
    pCtxt->lastGopSkipped = 0;

  } else {

    pCtxt->gopSz++;

    //LOG(X_DEBUG("non keyframe, keyframeCnt:%d, gopAvgPtsDuration:%.3f"), pCtxt->pGOPHistory->keyframeCnt, pCtxt->pGOPHistory->gopAvgPtsDuration);
    if(pCtxt->pGOPHistory->keyframeCnt > 1 && pCtxt->pGOPHistory->gopAvgPtsDuration < PTS_HZ_SEC * 20) {
  
      if(havePtsLastOverwrite && deltaPtsLastOverwrite < PTS_HZ_SEC * 5) {

        framethin_newTargetGOP(pCtxt, 1);

      } else if(havePtsLastHighWatermark && deltaPtsLastHighWatermark < PTS_HZ_SEC * 4) {

        if(pCtxt->targetGopSz > 1.0f) {
          targetGopSz = pCtxt->targetGopSz * .15f;
          //LOG(X_DEBUG("lower by 3/4.. targetGopSz now :%.3f, from targetGopSz"), targetGopSz);
        } else if(pCtxt->targetGopSz != 0) {
          targetGopSz = pCtxt->pGOPHistory->gopAvg * .15f;
          //LOG(X_DEBUG("lower by 3/4.. targetGopSz now :%.3f, from gopAvg"), targetGopSz);
        }
        framethin_newTargetGOP(pCtxt, targetGopSz);

      } else if(deltaPtsContents > ptsHighWatermark2) {

        targetGopSz = gewTargetGopSz(pCtxt, .50f);
        framethin_newTargetGOP(pCtxt, targetGopSz);

      } else if(deltaPtsContents > ptsHighWatermark3) {

        targetGopSz = gewTargetGopSz(pCtxt, .75f);
        framethin_newTargetGOP(pCtxt, targetGopSz);

      } else if(pCtxt->targetGopSz != 0 && 
	        deltaPtsContents < ptsLowWatermark &&
                (!havePtsLastHighWatermark || (havePtsLastLowWatermark &&  deltaPtsLastLowWatermark > PTS_HZ_SEC * 2))) {

        if(pCtxt->targetGopSz != 0) {
          raiseFactor = .25f;
          if(raiseFactor * pCtxt->targetGopSz < pCtxt->pGOPHistory->gopAvg * .25f) {
            //LOG(X_DEBUG("raise raiseFactor:%.3f, targetGopSz: %.3f from current%.3f, gopAvg:%.3f add..."), raiseFactor, targetGopSz, pCtxt->targetGopSz, pCtxt->pGOPHistory->gopAvg); 
            targetGopSz = pCtxt->targetGopSz + (pCtxt->pGOPHistory->gopAvg * .25);    
          } else {
            //LOG(X_DEBUG("raise raiseFactor:%.3f, targetGopSz from %.3f multiply..."), raiseFactor, targetGopSz); 
            targetGopSz = pCtxt->targetGopSz * (1.0f + raiseFactor);
          }
          framethin_newTargetGOP(pCtxt, targetGopSz);
        }
      }

      if(pCtxt->dropUntilKeyframe || (pCtxt->targetGopSz > 0 && pCtxt->gopSz > pCtxt->targetGopSz)) {
        //LOG(X_DEBUG("Skip video non-key frame pts:%.3f cur-gopsz:%d, target-gopsz:%.2f, dropUntilKey:%d, q-delta-rd/wr:%.3f sec, keyframeCnt:%d, gopAvgPtsDuration:%.3f"), PTSF(OUTFMT_PTS(pFrame)), pCtxt->gopSz, pCtxt->targetGopSz,  pCtxt->dropUntilKeyframe, PTSF(deltaPtsContents), pCtxt->pGOPHistory->keyframeCnt, PTSF(pCtxt->pGOPHistory->gopAvgPtsDuration));
        pCtxt->lastGopSkipped++;
        rc = 1;
      }
    }

  } // end of if(OUTFMT_KEYFRAME ...

  if(rc == 0) {
    burstmeter_AddSample(&pCtxt->burstMeterVid, OUTFMT_LEN(pFrame), NULL);
  }

  return rc;
}

int outfmt_getoutidx(const char *url, const char **ppend) {
  int idx = 0;
  int isvalid = 1;
  size_t pos, pos0, sz;

  if(url) {
    sz = pos = strlen(url);

    if(ppend) {
      *ppend = &url[pos];
    }

    //
    // Extract the outfmt stream index which follows the stream uri
    // TODO: '/2' is allowed anywhere in the URI, which could be the name of a dir when accessed via mgr
    // '/httplive/leading-dir/test.sdp/2'
    // Should set ppend to '/2'
    // The return value is [ outidx - 1 ]
    //
    while(pos > 1) {

      if(url[pos - 1] == '/') {

        idx = pos0 = pos;
        if(!strncmp(VSX_URI_PROFILE_PARAM, &url[pos], strlen(VSX_URI_PROFILE_PARAM))) {
          idx += 5;        
          pos0 = idx;
        }

        //
        // Ensure that the outidx string only consists of digits 
        //
        while(url[idx] != '\0' && idx < sz) {
          isvalid = 1;
          if(url[idx] < '0' || url[idx] > '9') {
            isvalid = 0;
            idx = 0;
            break;
          } 
          idx++;
        }

        if(isvalid && (idx = atoi(&url[pos0])) > 0) {
          idx--;
          if(ppend) {
            *ppend = &url[pos - 1];
          }
          break;
        }
        sz = pos - 1;
      }
      pos--;
    } 
  }

  return idx;
}

void outfmt_pause(OUTFMT_CFG_T *pOutFmt, int paused) {
  STREAMER_OUTFMT_T *pLiveFmt;

  if(!pOutFmt || !pOutFmt->pLiveFmt) {
    return;
  }

  pLiveFmt = pOutFmt->pLiveFmt;

  if(pLiveFmt) {
    pthread_mutex_lock(&pLiveFmt->mtx);
    pOutFmt->paused = paused;
    pthread_mutex_unlock(&pLiveFmt->mtx);
  }

}

OUTFMT_CFG_T *outfmt_setCb(STREAMER_OUTFMT_T *pLiveFmt, 
                           OUTFMT_CB_ONFRAME cbOnFrame,
                           void *pOutFmtCbData,
                           const PKTQUEUE_CONFIG_T *pQCfg,
                           STREAM_STATS_T *pstats,
                           int paused,
                           int frameThin,
                           unsigned int *pNumUsed) {

  OUTFMT_CFG_T *pOutFmt = NULL;
  int rc;
  unsigned int idx;

  if(!pLiveFmt || !pLiveFmt->poutFmts || !pQCfg) {
    LOG(X_ERROR("outfmt not initialized!"));
    return NULL;
  }

  if(pNumUsed) {
    *pNumUsed = 0;
  }

  pthread_mutex_lock(&pLiveFmt->mtx);

  for(idx = 0; idx < pLiveFmt->max; idx++) {

    if(!pLiveFmt->poutFmts[idx].do_outfmt) {

      pOutFmt = &pLiveFmt->poutFmts[idx];

      memset(&pOutFmt->cbCtxt, 0, sizeof(pOutFmt->cbCtxt));
      if(!(pOutFmt->pQ = (PKTQUEUE_T *) pktqueue_create(pQCfg->maxPkts, pQCfg->maxPktLen,
                                                        pQCfg->maxPkts, pQCfg->growMaxPktLen, 
                                                        0,  sizeof(OUTFMT_FRAME_DATA_T), 1))) {
        LOG(X_ERROR("Failed to create queue for async frame listener"));
        pOutFmt = NULL;
        break; 
      }
      pOutFmt->pQ->cfg.id = pQCfg->id + idx;
      pOutFmt->pQ->cfg.overwriteType = PKTQ_OVERWRITE_FIND_KEYFRAME;
      //pOutFmt->pQ->cfg.overwriteType = PKTQ_OVERWRITE_FIND_PTS; pOutFmt->pQ->cfg.overwriteVal = 0;
      pOutFmt->pQ->cfg.growSzOnlyAsNeeded = 20; // grow by extra 20% 
      pOutFmt->pQ->cfg.maxRTlatencyMs = FRAME_THIN_Q_LATENCY_MS; // TODO: this should not be smaller than any GOP

      LOG(X_DEBUG("Created outfmt pktq[%d] %d x slotsz:%d, slotszmax:%d"), 
          pOutFmt->pQ->cfg.id, pQCfg->maxPkts, pOutFmt->pQ->cfg.maxPktLen, pOutFmt->pQ->cfg.growMaxPktLen);

      //
      // Attach any (reader/writer) stats metrics counters to the queue instance
      //
      if((pOutFmt->pstats = pstats)) {
        pOutFmt->pQ->pstats = &pstats->throughput_rt[0];
      }

      //
      // If the queue does not have a reader, each insert will overwrite the prior, possibly missing the
      // first n packets, such as keyframe
      //
      pktqueue_setrdr(pOutFmt->pQ, 0); 

      pthread_mutex_init(&pOutFmt->mtx, NULL);
      pOutFmt->cbCtxt.ppQ = &pOutFmt->pQ;
      pOutFmt->cbCtxt.pMtx = &pOutFmt->mtx;
      pOutFmt->cbCtxt.cbOnFrame = cbOnFrame;
      pOutFmt->cbCtxt.pCbData = pOutFmtCbData;
      pOutFmt->cbCtxt.id = NULL;
      pOutFmt->cbCtxt.idx = idx;
      //pOutFmt->cbCtxt.pGOPHistory = NULL;

      if(frameThin && (pOutFmt->cbCtxt.pFrameThinCtxt = framethin_alloc())) {
        LOG(X_DEBUG("framethin enabled for output queue (id:%d)"), pOutFmt->pQ->cfg.id);
        pOutFmt->cbCtxt.pFrameThinCtxt->pOutFmt = pOutFmt;
        pOutFmt->cbCtxt.pFrameThinCtxt->pGOPHistory = &pLiveFmt->gopHistory;
        pOutFmt->cbCtxt.pGOPHistory = &pLiveFmt->gopHistory;
      }

      if((rc = liveq_start_cb(&pOutFmt->cbCtxt)) < 0) {
        LOG(X_ERROR("Unable to start liveq cb thread"));
        pktqueue_destroy(pOutFmt->pQ);
        pOutFmt->pQ = NULL;
        pOutFmt->cbCtxt.ppQ = NULL;
        framethin_destroy(pOutFmt->cbCtxt.pFrameThinCtxt);
        pOutFmt->cbCtxt.pGOPHistory = NULL;
        pthread_mutex_destroy(&pOutFmt->mtx);
        pOutFmt = NULL;
        break;
      }

      pOutFmt->pLiveFmt = pLiveFmt;
      pOutFmt->paused = paused;
      pOutFmt->do_outfmt = 1;
      pLiveFmt->numActive++;
      break;
    } else {
      if(pNumUsed) {
        (*pNumUsed)++;
      }
    }
  }

  pthread_mutex_unlock(&pLiveFmt->mtx);

  return pOutFmt;
}

int outfmt_removeCb(OUTFMT_CFG_T *pOutFmt) {
  STREAMER_OUTFMT_T *pLiveFmt = NULL;
  int rc = 0;
  //pthread_t tid;

  if(!pOutFmt || !pOutFmt->pLiveFmt) {
    return -1;
  }

  pLiveFmt = pOutFmt->pLiveFmt;

  //
  // Stop the liveq callback thread
  //
  pthread_mutex_lock(&pLiveFmt->mtx);
  pOutFmt->cbCtxt.ppQ = NULL;
  pktqueue_wakeup(pOutFmt->pQ);
  pthread_mutex_unlock(&pLiveFmt->mtx);

  if(PTHREAD_T_TOINT(pOutFmt->cbCtxt.tid) > 0 && 
    PTHREAD_SELF_TOINT(pthread_self()) == PTHREAD_T_TOINT(pOutFmt->cbCtxt.tid)) {

    pOutFmt->do_outfmt = 0;
    LOG(X_ERROR("Cannot call outfmt_removeCb from same cb thread"));
    return -1;
  }

  liveq_stop_cb(&pOutFmt->cbCtxt);

  pthread_mutex_lock(&pLiveFmt->mtx);
  pthread_mutex_lock(&pOutFmt->mtx);
  pktqueue_destroy(pOutFmt->pQ);
  if(pOutFmt->pstats) {
    stream_stats_destroy(&pOutFmt->pstats, NULL);
  }
  if(pOutFmt->cbCtxt.pFrameThinCtxt) {
    framethin_destroy(pOutFmt->cbCtxt.pFrameThinCtxt);
    pOutFmt->cbCtxt.pFrameThinCtxt = NULL;
  }
  pOutFmt->cbCtxt.pGOPHistory = NULL;
  pOutFmt->cbCtxt.cbOnFrame = NULL;
  pOutFmt->cbCtxt.pCbData = NULL;
  pOutFmt->pQ = NULL;
  pOutFmt->pstats = NULL;
  pOutFmt->do_outfmt = 0;
  pOutFmt->pLiveFmt = NULL;
  if(pLiveFmt->numActive > 0) {
    pLiveFmt->numActive--;
  }

  pthread_mutex_unlock(&pOutFmt->mtx);
  pthread_mutex_destroy(&pOutFmt->mtx);

  pthread_mutex_unlock(&pLiveFmt->mtx);

  return rc;
}

int outfmt_init(STREAMER_OUTFMT_LIST_T *pLiveFmts, int orderingQSz) {
  int rc = 0;
  PKTQUEUE_T *pQ = NULL;
  unsigned int growMaxPktLen = STREAMER_RTMPQ_SZSLOT_MAX_LIMIT;

  if(!pLiveFmts) {
    return -1;
  }

  if(orderingQSz > 0) {

    outfmt_close(pLiveFmts);

    //
    // no locking queue for ordering / buffering future timestamped audio / video, as when applying a/v sync
    //
    if(!(pQ = pktqueue_create(orderingQSz, 1024, orderingQSz, growMaxPktLen, 0, sizeof(OUTFMT_FRAME_DATA_T), 0))) {
      return -1;
    }
    LOG(X_DEBUG("Created outfmt ordering pktq[%d] %d x slotsz:%d, slotszmax:%d"),
        pQ->cfg.id, pQ->cfg.maxPkts, pQ->cfg.maxPktLen, pQ->cfg.growMaxPktLen);

    pLiveFmts->ordering.pOrderingQ = pQ;
    pQ->cfg.id = STREAMER_QID_ORDERING;
    pQ->cfg.overwriteType = PKTQ_OVERWRITE_FIND_KEYFRAME;
    pQ->cfg.growSzOnlyAsNeeded = 1;
    pktqueue_setrdr(pQ, 0);
    pLiveFmts->ordering.haveAudQueued = 0;
    pLiveFmts->ordering.haveVidQueued = 0;
    pLiveFmts->ordering.haveAud = 0;
    pLiveFmts->ordering.haveVid = 0;
    pLiveFmts->ordering.active = 0;
  }

  return rc;
}

void outfmt_close(STREAMER_OUTFMT_LIST_T *pLiveFmts) {

  if(pLiveFmts) {
    if(pLiveFmts->ordering.pOrderingQ) {
      pktqueue_destroy(pLiveFmts->ordering.pOrderingQ);
      pLiveFmts->ordering.pOrderingQ = NULL;
    }
  }

}

static int addpkt(PKTQUEUE_T *pQ, const OUTFMT_FRAME_DATA_T *pFrame) {
                  
  int rc = 0;
  unsigned int outidx;
  unsigned int len = 0;
  int tmp;
  unsigned int idx = 0;
  unsigned char *p, *p_prior;
  unsigned char *p0;
  PKTQ_EXTRADATA_T xtra;

  if(pQ->cfg.userDataLen < sizeof(OUTFMT_FRAME_DATA_T)) {
    LOG(X_ERROR("outfmt write queue id: %d user-data-len:%d < %d"), 
          pQ->cfg.id, pQ->cfg.userDataLen, sizeof(OUTFMT_FRAME_DATA_T));
    return -1;
  }

  xtra.tm.pts = OUTFMT_PTS(pFrame);
  xtra.tm.dts = OUTFMT_DTS(pFrame);
  xtra.pQUserData = (void *) pFrame;

  p0 = p_prior = OUTFMT_DATA_IDX(pFrame, 0); 
  len  = OUTFMT_LEN_IDX(pFrame, 0);

  //
  // increment idx from p0 to the start of the last outidx frame body
  //
  for(outidx = 1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if((tmp = OUTFMT_LEN_IDX(pFrame, outidx)) < 0) {
      break;
    } else if(tmp > 0 && (p = OUTFMT_DATA_IDX(pFrame, outidx))) {

      len = tmp;

      if(p >= p_prior) {
        idx += (p - p_prior);

        if(idx + len > pQ->cfg.growMaxPktLen) {
          break; 
        }

      } else {
        // The pFrame buffer space should be contiguous
        return -1; 
      }
   
      p_prior = p;

    }

  }

  //
  // set 'idx' to the end of the last frame so we copy the entire contiguous bufferspace into the queue slot
  //
  if(len > 0) {
    idx += len;
  }

  VSX_DEBUG_OUTFMT( LOG(X_DEBUG("OUTFMT - addpkt queue id:%d addpkt data: 0x%x, len:%d"), pQ->cfg.id, p0, idx) );

  if(idx > pQ->cfg.growMaxPktLen) {
    LOG(X_WARNING("Output method slot max size slot %d too small for %d"), pQ->cfg.growMaxPktLen, idx);
    idx = pQ->cfg.growMaxPktLen;
  }

  rc = pktqueue_addpkt(pQ, p0, idx, &xtra, OUTFMT_KEYFRAME(pFrame));

  return rc;
}

static void frameFromQ(const PKTQUEUE_T *pQ, const PKTQUEUE_PKT_T *pPkt, OUTFMT_FRAME_DATA_T *pFrame) {

  unsigned int outidx;
  int tmp;
  unsigned char *p_prev, *p_next, *p;

  if(pPkt->xtra.pQUserData) {

    memcpy(pFrame, pPkt->xtra.pQUserData, sizeof(OUTFMT_FRAME_DATA_T));

    //
    // Restore the outidx data pointers to the new buffer space
    //
    p_prev = OUTFMT_DATA_IDX(pFrame, 0);
    OUTFMT_DATA_IDX(pFrame, 0) = p_next = pPkt->pData;
    
    for(outidx = 1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if((p = OUTFMT_DATA_IDX(pFrame, outidx))) {
        if((tmp = (p - p_prev)) > 0) {
          p_next += tmp;
        }
        p_prev = p;

        if((p_next - pPkt->pData) > pQ->cfg.growMaxPktLen) {
          p_next = pPkt->pData + pQ->cfg.growMaxPktLen;
        }
        OUTFMT_DATA_IDX(pFrame, outidx) = p_next;

      } else { 
        OUTFMT_DATA_IDX(pFrame, outidx) = NULL;
      }

    }

    //
    // Adjust outidx frame body sizes if not all outidx frame body contents 
    //were able to fit into the queue slot space
    //
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      if(OUTFMT_DATA_IDX(pFrame, outidx) && OUTFMT_LEN_IDX(pFrame, outidx) > 0 &&
         (tmp = (OUTFMT_DATA_IDX(pFrame, outidx) - pPkt->pData + OUTFMT_LEN_IDX(pFrame, outidx))) > 
         pQ->cfg.growMaxPktLen) {
         if((tmp -= pQ->cfg.growMaxPktLen) < 0) {
           tmp = 0;
         }
         LOG(X_WARNING("Output read queue id: %d [outidx:%d].size:%d -> truncated to: %d"), 
                       pQ->cfg.id, outidx, OUTFMT_LEN_IDX(pFrame, outidx), tmp);
         OUTFMT_LEN_IDX(pFrame, outidx) = tmp;
      }
    }

  } else {
    // Does not restore outidx > 0
    OUTFMT_LEN_IDX(pFrame, 0) = pPkt->len;
    OUTFMT_DATA_IDX(pFrame, 0) = pPkt->pData;
    OUTFMT_PTS_IDX(pFrame, 0) = pPkt->xtra.tm.pts;
    OUTFMT_DTS_IDX(pFrame, 0) = pPkt->xtra.tm.dts;
    OUTFMT_KEYFRAME_IDX(pFrame, 0) = (pPkt->flags & PKTQUEUE_FLAG_KEYFRAME) ? 1 : 0;
    for(outidx = 1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      OUTFMT_LEN_IDX(pFrame, outidx) = 0;
    }
  }

  VSX_DEBUG_OUTFMT(
    LOG(X_DEBUG("OUTFMT - read queue id: %d, buf: 0x%x, lenbuf: %d, 0:{0x%x, len:%d}, 1:{0x%x, len:%d}, 2:{0x%x, len:%d}, 3:{0x%x, len:%d}"), pQ->cfg.id, pFrame->xout.outbuf.buf, pFrame->xout.outbuf.lenbuf, pFrame->xout.outbuf.poffsets[0], pFrame->xout.outbuf.lens[0],  pFrame->xout.outbuf.poffsets[1], pFrame->xout.outbuf.lens[1], pFrame->xout.outbuf.poffsets[2], pFrame->xout.outbuf.lens[2],pFrame->xout.outbuf.poffsets[3], pFrame->xout.outbuf.lens[3]);

  );

}

static void ordering_advanceQRdr(PKTQUEUE_T *pQ) {
  pQ->pkts[pQ->idxRd].flags = 0;
  if(++(pQ->idxRd) >= pQ->cfg.maxPkts) {
    pQ->idxRd = 0; 
  } 
  pQ->idxInRd = 0;
}

static OUTFMT_FRAME_DATA_T *ordering_peekFrame(PKTQUEUE_T *pQ, OUTFMT_FRAME_DATA_T *pFrame) {

  if(!(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
    return NULL;
  }

  frameFromQ(pQ, &pQ->pkts[pQ->idxRd], pFrame); 

  return pFrame;
}

static int ordering_flushFrames(STREAMER_OUTFMT_LIST_T *pLiveFmts) {
  int rc = 0;
  OUTFMT_FRAME_DATA_T frameData;
  OUTFMT_FRAME_DATA_T *pFrameTmp = NULL;
  STREAMER_OUTFMT_BUFFER_T *pOrdering = &pLiveFmts->ordering;
  PKTQUEUE_T *pQ = pOrdering->pOrderingQ;

  while((pFrameTmp = ordering_peekFrame(pQ, &frameData))) {
    ordering_advanceQRdr(pQ);
    rc = outfmt_invokeCbsUnbuffured(pLiveFmts, pFrameTmp);
  }

  pOrdering->haveVidQueued = 0;
  pOrdering->haveAudQueued = 0;
  pOrdering->haveVid = 0;
  pOrdering->haveAud = 0;

  return 0;
}

static int ordering_bufferFrame(STREAMER_OUTFMT_LIST_T *pLiveFmts, OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;
  OUTFMT_FRAME_DATA_T frameData;
  OUTFMT_FRAME_DATA_T *pFrameTmp = NULL;
  STREAMER_OUTFMT_BUFFER_T *pOrdering = &pLiveFmts->ordering;
  PKTQUEUE_T *pQ = pOrdering->pOrderingQ;

  //LOG(X_DEBUG("WILL QUEUE TO ORDER %s pts:%.3f, len:%d, lastVid:%.3f, lastAud:%.3f"), pFrame->isvid ? "video" : "audio", PTSF(OUTFMT_PTS(pFrame)), OUTFMT_LEN(pFrame),  PTSF(pOrdering->lastVidPts), PTSF(pOrdering->lastAudPts));

  //
  // If the queue is full then we need to pop a node to make room
  //
  if((pQ->pkts[pQ->idxWr].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {

    LOG(X_WARNING("LiveQ frame ordering queue is full.")); 
    //pktqueue_dump(pQ, NULL);
    pFrameTmp = ordering_peekFrame(pQ, &frameData);

    if(pFrameTmp && OUTFMT_PTS(pFrame) > PTS_HZ_SEC + OUTFMT_PTS(pFrameTmp)) {
      LOG(X_WARNING("Disabling LiveQ frame ordering and flushing contents.")); 
      pOrdering->active = 0; 
      ordering_flushFrames(pLiveFmts);
      return outfmt_invokeCbsUnbuffured(pLiveFmts, pFrame);
    }

    ordering_advanceQRdr(pQ);

    if(pFrameTmp && (rc = outfmt_invokeCbsUnbuffured(pLiveFmts, pFrameTmp)) < 0) {
      return rc;
    }

  }

  if(OUTFMT_LEN(pFrame) > pQ->cfg.growMaxPktLen) {
    LOG(X_WARNING("Output method slot max size slot %d too small for %d.  Increase %s"),
      pQ->cfg.growMaxPktLen, OUTFMT_LEN(pFrame), SRV_CONF_KEY_RTMPQSZSLOTMAX);
  }

  rc = addpkt(pQ, pFrame);

  if(pFrame->isvid) {
    pOrdering->haveVidQueued = 1;
    pOrdering->haveAudQueued = 0;
  } else if(pFrame->isaud) {
    pOrdering->haveAudQueued = 1;
    pOrdering->haveVidQueued = 0;
  }

  return rc;
}

static int ordering_invokeCbs(STREAMER_OUTFMT_LIST_T *pLiveFmts, OUTFMT_FRAME_DATA_T *pFrameIn) {
  int rc = 0;
  int do_queue = 0;
  int read_queued_vid = 0;
  int read_queued_aud = 0;
  OUTFMT_FRAME_DATA_T *pFrameTmp = NULL;
  OUTFMT_FRAME_DATA_T frameData;
  STREAMER_OUTFMT_BUFFER_T *pOrdering = &pLiveFmts->ordering;
  PKTQUEUE_T *pQ = pOrdering->pOrderingQ;
  OUTFMT_FRAME_DATA_T *pFrameOut = pFrameIn;

  //LOG(X_DEBUG("-------- outfmt_invokeCbs pLiveFmts: 0x%x len:%d, pts:%.3f, %s, lastVid:%.3f, lastAud:%.3f, haveVidQ:%d, haveAQudQ:%d"), pLiveFmts, OUTFMT_LEN(pFrameIn), PTSF(OUTFMT_PTS(pFrameIn)), pFrameIn->isvid ? "video" : "audio", PTSF(pOrdering->lastVidPts), PTSF(pOrdering->lastAudPts), pOrdering->haveVidQueued, pOrdering->haveAudQueued);

  if(pFrameIn->isvid && !pOrdering->haveVid) {
    pOrdering->haveVid = 1;
  } else if(pFrameIn->isaud && !pOrdering->haveAud) {
    pOrdering->haveAud = 1;
  }

  //
  // Frame ordering may have been auto-disabled if the ordering queue filled up and the time gap 
  // between audio & video exceeded our threshold
  //
  if(pOrdering->haveVid && pOrdering->haveAud && !pOrdering->active) {
    LOG(X_DEBUG("Enabling LiveQ frame ordering.")); 
    pOrdering->active = 1;
  }

  if(pFrameIn->isvid) {

    if(pOrdering->haveAud && OUTFMT_PTS(pFrameIn) > pOrdering->lastAudPts && !pOrdering->haveAudQueued) {
      do_queue = 1;
      pFrameOut = NULL;
    }

  } else if(pFrameIn->isaud) {

    if(pOrdering->haveVid && OUTFMT_PTS(pFrameIn) > pOrdering->lastVidPts && !pOrdering->haveVidQueued) {
      do_queue = 1;
      pFrameOut = NULL;
    }

  }

  //LOG(X_DEBUG("set do_queue:%d"), do_queue);

  //
  // Queue the possibly future dated frame to see if any frames with prior timestamps will arrive next 
  //
  if(do_queue && (rc = ordering_bufferFrame(pLiveFmts, pFrameIn)) < 0) {
    return rc;
  } 
 
  //
  // Flush the queue from any frame timestamps which have already past in both the audio and video streams
  //
  pFrameTmp = NULL;
  rc = 0;
  while(rc >= 0 && (pFrameTmp = ordering_peekFrame(pQ, &frameData))) {

    //
    // If we were called for a video frane, we only want to flush audio frames,
    // and vice versa if we were called for an audio frame
    //
    if(pFrameIn->isvid == pFrameTmp->isvid && pFrameIn->isaud == pFrameTmp->isaud) {
      break;
    }

    if(pFrameTmp->isvid) {
      pOrdering->lastVidPts = OUTFMT_PTS(pFrameTmp);
    } else if(pFrameTmp->isaud) {
      pOrdering->lastAudPts = OUTFMT_PTS(pFrameTmp);
    }

    if(OUTFMT_PTS(pFrameIn) > OUTFMT_PTS(pFrameTmp)) {

        //LOG(X_DEBUG("GOT QUEUED frame %s pts:%.3f, pFrameIn pts:%.3f, lastVid:%.3f, lastAud:%.3f"), pFrameTmp->isvid ? "video" : "audio", PTSF(OUTFMT_PTS(pFrameTmp)), PTSF(OUTFMT_PTS(pFrameIn)), PTSF(pOrdering->lastVidPts), PTSF(pOrdering->lastAudPts));
      ordering_advanceQRdr(pQ);
 
      rc = outfmt_invokeCbsUnbuffured(pLiveFmts, pFrameTmp);

      if(pFrameTmp->isvid) {
        read_queued_vid = 1;
      } else if(pFrameTmp->isaud) {
        read_queued_aud = 1;
      }

    } else {
      break;
    }

  } 

  //
  // After flushing any frames, we may need to now buffer this current frame if the ordering queue has
  // becoe empty
  //
  if(pFrameOut && 
     ((((pFrameOut->isvid && read_queued_vid) || (pFrameOut->isaud && read_queued_aud))) ||
      ((read_queued_vid || read_queued_aud) && !(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA)))) {
    if((rc = ordering_bufferFrame(pLiveFmts, pFrameOut)) < 0) {
      return rc;
    }
    pFrameOut = NULL;
  } 
  
  if(!(pQ->pkts[pQ->idxRd].flags & PKTQUEUE_FLAG_HAVEPKTDATA)) {
    pOrdering->haveVidQueued = 0;
    pOrdering->haveAudQueued = 0;
  } 

  //
  // Process the frame that we were originally called for, possibly after flushing any frames
  // from the ordering queue, which had been previously buffered
  //
  if(pFrameOut) {
    if(!do_queue) {
      if(pFrameOut->isvid) {
        pOrdering->lastVidPts = OUTFMT_PTS(pFrameOut);
        //LOG(X_DEBUG("Set lastVidPts: %.3f"), PTSF(pOrdering->lastVidPts));
      } else if(pFrameOut->isaud) {
        pOrdering->lastAudPts = OUTFMT_PTS(pFrameOut);
        //LOG(X_DEBUG("Set lastAudPts: %.3f"), PTSF(pOrdering->lastAudPts));
      }
    }
    rc = outfmt_invokeCbsUnbuffured(pLiveFmts, pFrameOut);
  }

  return rc;
}

int outfmt_invokeCbs(STREAMER_OUTFMT_LIST_T *pLiveFmts, OUTFMT_FRAME_DATA_T *pFrameIn) {
  int rc;

  if(!pLiveFmts || !pFrameIn) {
    return -1;
  } else if(OUTFMT_LEN(pFrameIn) <= 0) {
    return 0;
  }

  if(pLiveFmts->ordering.pOrderingQ) {
    //
    // Attempt to buffer any future dated frames to maintain interleaved frame ordering
    // for future stream specific callback invocation.  For eg., to maintain proper flv, 
    // mkv audio and video frame time stamp ordering regardless of audio sync applied.
    //
    rc = ordering_invokeCbs(pLiveFmts, pFrameIn);
  } else {
    rc = outfmt_invokeCbsUnbuffured(pLiveFmts, pFrameIn);
  }

  return rc;
}

static int outfmt_invokeCbsUnbuffured(STREAMER_OUTFMT_LIST_T *pLiveFmts, const OUTFMT_FRAME_DATA_T *pFrame) {
  STREAMER_OUTFMT_T *pOutFmt;
  PKTQUEUE_T *pQ;
  //PKTQ_EXTRADATA_T xtra;
  int rc;
  int didGopHistory = 0;
  unsigned int idx, idxFmts;

  if(!pLiveFmts || !pFrame) {
    return -1;
  } else if(OUTFMT_LEN(pFrame) <= 0) {
    return 0;
  }

  //LOG(X_DEBUG("    called outfmt_invokeCbsUnbuffured pLiveFmts: 0x%x len:%d, pts:%.3f, isvid:%d, isaud:%d"), pLiveFmts, OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), pFrame->isvid, pFrame->isaud);

  for(idxFmts = 0; idxFmts < STREAMER_OUTFMT_IDX_MAX; idxFmts++) {

    pOutFmt = &pLiveFmts->out[idxFmts];

    if(!pOutFmt->do_outfmt || pOutFmt->numActive <= 0) {
    //fprintf(stderr, "pOutFmt[%d].do_outfmt:%d, numActive:%d, idxFmts\n", idxFmts, pOutFmt->do_outfmt, pOutFmt->numActive);
      continue; 
    }

    pthread_mutex_lock(&pOutFmt->mtx);

    for(idx = 0; idx < pOutFmt->max; idx++) {

      //fprintf(stderr, "idxFmts[%d].POUTFMT[%d] do_outfmt:%d  .cbOnFrame:0x%x, .pCbData:0x%x\n", idxFmts, idx, pOutFmt->poutFmts[idx].do_outfmt, pOutFmt->poutFmts[idx].cbCtxt.cbOnFrame, pOutFmt->poutFmts[idx].cbCtxt.pCbData);

      if(pOutFmt->poutFmts &&
         pOutFmt->poutFmts[idx].do_outfmt &&
         pOutFmt->poutFmts[idx].cbCtxt.cbOnFrame &&
         pOutFmt->poutFmts[idx].cbCtxt.pCbData &&
         !pOutFmt->poutFmts[idx].paused) {

        pQ = NULL;
        if(pOutFmt->poutFmts[idx].cbCtxt.ppQ && 
           (pQ = *(pOutFmt->poutFmts[idx].cbCtxt.ppQ))) {
          
          //
          // Since outfmt_setCb is internally creating the pktqueue, we don't really 
          // need to check to make sure the userData contents storage size is adequate
          // The user data contents are actually copied inline w/ the queue data storage
          // buffer
          //
          //if(xtra.pQUserData && pQ->cfg.userDataLen < sizeof(OUTFMT_FRAME_DATA_T)) {
          //  continue;
          //}

          //TODO: create rtp packetizer from outfmt... to be used by pip video conf output reader & sender

          if(OUTFMT_LEN(pFrame) > pQ->cfg.growMaxPktLen) {
            LOG(X_WARNING("Output method slot max size slot %d too small for %d.  Increase %s"), 
              pQ->cfg.growMaxPktLen, OUTFMT_LEN(pFrame), SRV_CONF_KEY_RTMPQSZSLOTMAX);
          }

          //static int greatest_sz; if(OUTFMT_LEN(pFrame) > greatest_sz) greatest_sz = OUTFMT_LEN(pFrame); fprintf(stderr, "tid: 0x%x outfmt_invokeCbs pktqueue_addpkt len:%d(/%d, max:%d), greatest_sz:%d, rc:%d \n", pthread_self(), OUTFMT_LEN(pFrame), pQ->cfg.maxPktLen, pQ->cfg.growMaxPktLen, greatest_sz, rc); //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), MIN(OUTFMT_LEN(pFrame), 16), 0); 

          //
          // If frame thinning is enabled, then do GOP accounting on the queue insertion
          //
          if(!didGopHistory && pOutFmt->poutFmts[idx].cbCtxt.pGOPHistory && pFrame->isvid) {
            gop_history_onFrame(pOutFmt->poutFmts[idx].cbCtxt.pGOPHistory, OUTFMT_KEYFRAME(pFrame), OUTFMT_PTS(pFrame));
            didGopHistory = 1;
          }

          VSX_DEBUG_OUTFMT( 
             LOG(X_DEBUG("OUTFMT - write queue id: %d, buf: 0x%x, lenbuf: %d, 0:{0x%x, len:%d}, 1:{0x%x, len:%d}, 2:{0x%x, len:%d}, 3:{0x%x, len:%d}"), pQ->cfg.id, pFrame->xout.outbuf.buf, pFrame->xout.outbuf.lenbuf, pFrame->xout.outbuf.poffsets[0], pFrame->xout.outbuf.lens[0], pFrame->xout.outbuf.poffsets[1], pFrame->xout.outbuf.lens[1], pFrame->xout.outbuf.poffsets[2], pFrame->xout.outbuf.lens[2],pFrame->xout.outbuf.poffsets[3], pFrame->xout.outbuf.lens[3]);
          ); 

          //
          // Insert the frame contents into the output specific queue for async output method client consumption
          //
          rc = addpkt(pQ, pFrame);
 
        }

      }
    }

    pthread_mutex_unlock(&pOutFmt->mtx);

  }

  return 0;
}


typedef struct LIVEQ_CB_CTXT_WRAP {
  LIVEQ_CB_CTXT_T *pCtxt;
  char             tid_tag[LOGUTIL_TAG_LENGTH];
} LIVEQ_CB_CTXT_WRAP_T;

static void liveq_proc(void *pArg) {
  LIVEQ_CB_CTXT_WRAP_T wrap;
  LIVEQ_CB_CTXT_T *pCtxt = NULL;
  const PKTQUEUE_PKT_T *pPkt;
  PKTQUEUE_T *pQ;
  int rc = 0;
  OUTFMT_FRAME_DATA_T frameData;
  int skipFrame = 0;
  char buf[32];
  char *pid = NULL;

  memcpy(&wrap, (LIVEQ_CB_CTXT_WRAP_T *) pArg, sizeof(wrap));
  logutil_tid_add(pthread_self(), wrap.tid_tag);
  pCtxt = wrap.pCtxt;
  pCtxt->running = 1;
  pid = pCtxt->id;

#if defined WIN32
  PTHREAD_T_TOINT(pCtxt->tid) = PTHREAD_SELF_TOINT(pthread_self());
#else
  pCtxt->tid = pthread_self();
#endif // WIN32


  if(!pid) {
    snprintf(buf, sizeof(buf), "%u", pCtxt->idx);
    pid = buf;
  }

  LOG(X_DEBUG("LiveQ reader %s thread started"), pid); 

  while(rc >= 0 && pCtxt->running == 1 && !g_proc_exit) {

    pthread_mutex_lock(pCtxt->pMtx);

    if(!pCtxt->ppQ || !(*pCtxt->ppQ)) {
      pthread_mutex_unlock(pCtxt->pMtx);
      break;
    }

    pQ = *(pCtxt->ppQ);
    pPkt = NULL;

    if(pQ->quitRequested) {

      pthread_mutex_unlock(pCtxt->pMtx);
      break;

    } else if(pktqueue_havepkt(pQ)) {

      if((pPkt = pktqueue_readpktdirect(pQ))) {

        //
        // Avoid memcpy of the frame payload data by swapping pointers with swap read slot
        //
        if(pktqueue_swapreadslots(pQ, &pCtxt->pSwappedSlot) < 0) {
          LOG(X_ERROR("Failed to swap slot in queue id:%d"), pQ->cfg.id);
          pktqueue_readpktdirect_done(pQ);
          break;
        } else {
          pPkt = pCtxt->pSwappedSlot;
        }

        pktqueue_readpktdirect_done(pQ);

        //fprintf(stderr, "liveq_proc (qid:%d) read rd:%d, wr:%d/%d, pquserdata:0x%x\n", pQ->cfg.id, pQ->idxRd, pQ->idxWr, pQ->cfg.maxPkts, pPkt->xtra.pQUserData);

        frameFromQ(pQ, pPkt, &frameData);
        skipFrame = 0;
      
        //
        // If this output format queue supports frame thinning, check if we should drop the frame
        // to reduce output bandwidth
        //
        if(pCtxt->pFrameThinCtxt && framethin_onFrame(pCtxt->pFrameThinCtxt, &frameData) == 1) {
          skipFrame = 1; 
        }

#if defined BITRATE_RESTRICT
        if(!g_bs_init) {
          burstmeter_init(&g_bs_v, 200, 4000);
          burstmeter_init(&g_bs_a, 200, 4000);
          g_bs_init = 1;
          g_bs_usleep = 35.0f;
          gtv0 = timer_GetTime();
        }

        if(frameData.isvid) {
          g_bytes_v += OUTFMT_LEN(&frameData);
        } else {
          g_bytes_a += OUTFMT_LEN(&frameData);
        }

        if(!skipFrame) {

          if(frameData.isvid) {
            burstmeter_AddSample(&g_bs_v, OUTFMT_LEN(&frameData), NULL); 
          } else {
            burstmeter_AddSample(&g_bs_a, OUTFMT_LEN(&frameData), NULL); 
          }
          float bps = burstmeter_getBitrateBps(&g_bs_v) +  burstmeter_getBitrateBps(&g_bs_a);
          float bs_usleep = 0;
           bs_usleep = OUTFMT_LEN(&frameData) * g_bs_usleep;
           if(bs_usleep > 10) {
             usleep(bs_usleep);
           }
        
          //if(timer_GetTime()/TIME_VAL_US != gtv1/TIME_VAL_US) { fprintf(stderr, "BM vid %lld.%lld, bytes:%d, range:%d\n", timer_GetTime()/TIME_VAL_US, timer_GetTime()/TIME_VAL_MS, g_bs_v.meter.cur.bytes, g_bs_v.meter.rangeMs); burstmeter_dump(&g_bs_v); }
          gtv1 = timer_GetTime();

          //if(frameData.isvid) LOG(X_DEBUG("livefmt->cbOnFrame: pts:%.3f, len:%d %s%s, vid: %.3f Kb/s, aud:%.3f Kb/s, sleep:%.3fms, vid-rate:%.3f, aud-rate:%.3f"), PTSF(OUTFMT_PTS(&frameData)), OUTFMT_LEN(&frameData), frameData.isvid ? "video" : "audio", OUTFMT_KEYFRAME(&frameData) ? " KEY" : "", burstmeter_getBitrateBps(&g_bs_v)/1000.0f, burstmeter_getBitrateBps(&g_bs_a)/1000.0f, bs_usleep / 1000.0f, (8 * (float)g_bytes_v/((gtv1-gtv0)/1000.0f)), (8 * (float)g_bytes_a/((gtv1-gtv0)/1000.0f)));
        }
#else
        //if(frameData.isvid) LOG(X_DEBUG("livefmt->cbOnFrame: pts:%.3f, len:%d %s%s"), PTSF(OUTFMT_PTS(&frameData)), OUTFMT_LEN(&frameData), frameData.isvid ? "video" : "audio", OUTFMT_KEYFRAME(&frameData) ? " KEY" : "");

#endif // BITRATE_RESTRICT

        if(!skipFrame && (rc = pCtxt->cbOnFrame(pCtxt->pCbData, &frameData)) < 0) {
          LOG(X_ERROR("LiveQ id:%d callback failed with error: %d"), pQ->cfg.id, rc);
        }

      }

      pthread_mutex_unlock(pCtxt->pMtx);

    } else {

      pthread_mutex_unlock(pCtxt->pMtx);

      pktqueue_waitforunreaddata(pQ);
    }

  } // end of while(rc >= 0 && ...

  LOG(X_DEBUG("LiveQ reader %s thread exiting (rc:%d) "), pid, rc);

  if(pCtxt->pSwappedSlot && pCtxt->pSwappedSlot->pBuf) {
    free(pCtxt->pSwappedSlot->pBuf);
    pCtxt->pSwappedSlot->pBuf = NULL;
  }
  pCtxt->pSwappedSlot = NULL;

  pCtxt->running = -1;

  logutil_tid_remove(pthread_self());
}


enum PKTQUEUE_RC liveq_addpkt(STREAMER_LIVEQ_T *pLiveQ, unsigned int idxQ, OUTFMT_FRAME_DATA_T *pFrame) {

  enum PKTQUEUE_RC rc = PKTQUEUE_RC_ERROR;
  //PKTQ_EXTRADATA_T xtra;
  PKTQUEUE_T *pQ = NULL;

  if(!pLiveQ) {
    return -1;
  }

  //fprintf(stderr, "addpkt pQ: 0x%x len:%d, isvid:%d, pts:%.3f, dts:%.3f\n", pQ, OUTFMT_LEN(pFrame), pFrame->isvid, PTSF(OUTFMT_PTS(pFrame)), PTSF(OUTFMT_PTS(pFrame)));

  pthread_mutex_lock(&pLiveQ->mtx);

  if(pLiveQ->pQs && (pQ = pLiveQ->pQs[idxQ])) {

    //
    // The user data contents are actually copied inline w/ the queue data storage
    // buffer
    //
    rc = addpkt(pQ, pFrame);
  }

  pthread_mutex_unlock(&pLiveQ->mtx);

  //fprintf(stderr, "addpkt done pQ: 0x%x rc:%d\n", pQ, rc);

  return rc;
}


int liveq_start_cb(LIVEQ_CB_CTXT_T *pCtxt) {
  int rc = 0;
  pthread_t ptd;
  pthread_attr_t ptdAttr;
  LIVEQ_CB_CTXT_WRAP_T wrap;
  const char *s;

  if(!pCtxt || !pCtxt->ppQ || !pCtxt->pMtx || !pCtxt->cbOnFrame) {
    return -1;
  }

  wrap.pCtxt = pCtxt;
  wrap.tid_tag[0] = '\0';
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(wrap.tid_tag, sizeof(wrap.tid_tag), "%s-livq-%d-%d", s, 
             (pCtxt->ppQ ? (*pCtxt->ppQ)->cfg.id : 0), pCtxt->idx);

  }
  pCtxt->running = 2;
  pthread_attr_init(&ptdAttr);
  pthread_attr_setdetachstate(&ptdAttr, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptd,
                    &ptdAttr,
                  (void *) liveq_proc,
                  (void *) &wrap) != 0) {

    LOG(X_ERROR("Unable to create liveQ callback thread"));
    pCtxt->running = 0;
    return -1;
  }

  while(pCtxt->running == 2) {
    usleep(5000);
  }

  return rc;
}

int liveq_stop_cb(LIVEQ_CB_CTXT_T *pCtxt) {

  char buf[32];
  char *pid;
  
  if(!pCtxt) {
    return -1;
  }

  if(!(pid = pCtxt->id)) {
    snprintf(buf, sizeof(buf), "%u", pCtxt->idx);
    pid = buf;
  }

  //fprintf(stderr, "LIVEQ_STOP %s running:%d\n", pid, pCtxt->running);

  if(pCtxt->running == 0 || pCtxt->running == -1) {
    //fprintf(stderr, "LIVEQ_STOP DONE %s running:%d\n", pid, pCtxt->running);
    return 0;
  }

  pCtxt->running = -2;

  while(pCtxt->running != -1) {
    usleep(5000);
  }
  
  //fprintf(stderr, "LIVEQ_STOP DONE %s running:%d\n", pid, pCtxt->running);

  return 0;
}

static OUTFMT_FRAME_DATA_WRAP_T *outfmt_dupframe(const OUTFMT_FRAME_DATA_T *pFrame,
                                     OUTFMT_FRAME_DATA_WRAP_T *pCloneArg) {
  OUTFMT_FRAME_DATA_WRAP_T *pClone = pCloneArg;

  if(!pClone && !(pClone = (OUTFMT_FRAME_DATA_WRAP_T *) avc_calloc(1, sizeof(OUTFMT_FRAME_DATA_WRAP_T)))) {
    return NULL;
  }

  memcpy(&pClone->data, pFrame, sizeof(OUTFMT_FRAME_DATA_T));
  if(!(pClone->data.xout.outbuf.buf = avc_calloc(1, OUTFMT_LEN(pFrame)))) {
    if(!pCloneArg) {
      avc_free((void **) pClone);
    }
    return NULL;
  }
  memcpy(pClone->data.xout.outbuf.buf, OUTFMT_DATA(pFrame), OUTFMT_LEN(pFrame));
  pClone->data.xout.outbuf.lenbuf = OUTFMT_LEN(pFrame);
  //OUTFMT_DATA(&pClone->data) = pClone->data.xout.outbuf.buf;

  return pClone;
}

static void outfmt_freeframe(OUTFMT_FRAME_DATA_WRAP_T *pFrame) {
  if(pFrame->data.xout.outbuf.buf) {
    avc_free((void **) &pFrame->data.xout.outbuf.buf);
    pFrame->data.xout.outbuf.lenbuf = 0;
    //OUTFMT_DATA(&pFrame->data) = NULL;
  }
}

int outfmt_queueframe(OUTFMT_QUEUED_FRAMES_T *pQueued, const OUTFMT_FRAME_DATA_T *pFrame) {
  OUTFMT_FRAME_DATA_WRAP_T *pCloned = NULL;

  if(!pQueued || !pFrame) {
    return -1;
  }

  if(!(pCloned = outfmt_dupframe(pFrame, NULL))) {
    return -1;
  }

  if(!pQueued->phead) {
    pQueued->phead = pCloned;
  } else {
    pQueued->ptail->pnext = pCloned;
  }
  pQueued->ptail = pCloned;
  pQueued->count++;

  return 0;
}

void outfmt_freequeuedframes(OUTFMT_QUEUED_FRAMES_T *pQueued) {
  OUTFMT_FRAME_DATA_WRAP_T *pNode;

  pNode = pQueued->phead;

  while(pNode) {
    pQueued->phead = pNode->pnext;
    outfmt_freeframe(pNode);
    pNode = pQueued->phead;
  }

  pQueued->phead = pQueued->ptail = NULL;
  pQueued->count = 0;
  pQueued->skipped = 0;

}


#endif // VSX_HAVE_STREAMER

