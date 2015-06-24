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

typedef struct MONITOR_START_WRAP {
  STREAM_STATS_MONITOR_T  *pMonitor;
  char                     tid_tag[LOGUTIL_TAG_LENGTH];
} MONITOR_START_WRAP_T;


static void stream_monitor_dump(FILE *fp, STREAM_STATS_MONITOR_T *pMonitor);
static int stream_monitor_detach(STREAM_STATS_MONITOR_T *pMonitor, STREAM_STATS_T *pStats);


int stream_stats_destroy(STREAM_STATS_T **ppStats, pthread_mutex_t *pmtx) {
  unsigned int idx;
  unsigned int throughputIdx;

  if(pmtx) {
    pthread_mutex_lock(pmtx);
  }

  if(!ppStats || !(*ppStats)) {
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    }
    return -1;
  }

  pthread_mutex_lock(&(*ppStats)->mtx);

  (*ppStats)->active = 0;

  // 
  // Automatically detach from monitor linked list
  //
  if((*ppStats)->pMonitor) {
    stream_monitor_detach( (*ppStats)->pMonitor, *ppStats);
  }

  for(idx = 0; idx < THROUGHPUT_STATS_BURSTRATES_MAX; idx++) {
    for(throughputIdx = 0; throughputIdx < 2; throughputIdx++) {
      burstmeter_close(&(*ppStats)->throughput_rt[throughputIdx].bitratesWr[idx]);
      burstmeter_close(&(*ppStats)->throughput_rt[throughputIdx].bitratesRd[idx]);
    }
  }

  pthread_mutex_unlock(&(*ppStats)->mtx);
  pthread_mutex_destroy(&(*ppStats)->mtx);
  avc_free((void **) ppStats);

  if(pmtx) {
    pthread_mutex_unlock(pmtx);
  }

  return 0;
}

static STREAM_STATS_T *stream_stats_create(const struct sockaddr_in *psaRemote, 
                                           unsigned int numWr, unsigned int numRd,
                                           int rangeMs1, int rangeMs2) {
  STREAM_STATS_T *pStats = NULL;
  unsigned int idx;
  unsigned int periodMs;
  int rangeMs;
  int rc = 0;

  if(!(pStats = (STREAM_STATS_T *) avc_calloc(1, sizeof(STREAM_STATS_T)))) {
    return NULL;
  }

  pStats->active = 1;

  for(idx = 0; idx < THROUGHPUT_STATS_BURSTRATES_MAX; idx++) {

    rangeMs = idx == 0 ? rangeMs1 : rangeMs2;
    if(rangeMs <= 0) {
      continue;
    }

    periodMs = rangeMs / 10;
    rangeMs = periodMs * 10;

    if(rc >= 0 && numWr > 0) {
      // This is used for UDP / RTP output or general TCP based flow queue writing 
      rc = burstmeter_init(&pStats->throughput_rt[0].bitratesWr[idx], periodMs, rangeMs);
    }
    if(rc >= 0 && numWr > 1) { 
      // This is used for UDP / RTCP output
      rc = burstmeter_init(&pStats->throughput_rt[1].bitratesWr[idx], periodMs, rangeMs);
    }
    if(rc >= 0 && numRd > 0) { 
      // This is used for general TCP based flow queue reading 
       rc = burstmeter_init(&pStats->throughput_rt[0].bitratesRd[idx], periodMs, rangeMs);
    }
    if(rc < 0) {
      stream_stats_destroy(&pStats, NULL);
      return NULL;
    }

  }

  if(psaRemote) {
    memcpy(&pStats->saRemote, psaRemote, sizeof(pStats->saRemote));
  }
  pStats->numWr = numWr;
  pStats->numRd  = numRd;

  pthread_mutex_init(&pStats->mtx, NULL);

  return pStats;

}

void stream_stats_addPktSample(STREAM_STATS_T *pStats, pthread_mutex_t *pmtx, unsigned int len, int rtp) {
  unsigned int idx;
  struct timeval tv;
  THROUGHPUT_STATS_T *pThroughput = NULL;
  BURSTMETER_SAMPLE_SET_T *pbitrates;

  if(pmtx) {
    pthread_mutex_lock(pmtx);
  }

  if(!pStats || !pStats->active) {
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    }
    return;
  }

  //LOG(X_DEBUG("stream_stats_addPktSample len:%d, rtp:%d"), len, rtp);

  if(pStats->numWr > 1 && !rtp) {
    // This is for RTCP packet accounting
    pThroughput = &pStats->throughput_rt[1];
  } else {
    // This is for RTP packet accounting or general TCP based flow
    pThroughput = &pStats->throughput_rt[0];
  }
  pbitrates = pThroughput->bitratesWr;

  gettimeofday(&tv, NULL);

  pthread_mutex_lock(&pStats->mtx);

  if(pStats->active) {

    if(pThroughput->written.slots == 0) {
      TIME_TV_SET(pStats->tvstart, tv);
    }
    pThroughput->written.bytes += len;
    pThroughput->written.slots++;

    for(idx = 0; idx < THROUGHPUT_STATS_BURSTRATES_MAX; idx++) {
      if(pbitrates[idx].meter.rangeMs > 0) {
        burstmeter_AddSample(&pbitrates[idx], len, &tv);
      } 
    }

  }

  pthread_mutex_unlock(&pStats->mtx);

  if(pmtx) {
    pthread_mutex_unlock(pmtx);
  }

}

static void stream_stats_resetctrs(STREAM_STATS_T *pStats) {

  pStats->ctr_rt.ctr_countseqRr = 0;
  pStats->ctr_rt.ctr_rrdelayMsTot = 0;
  pStats->ctr_rt.ctr_rrdelaySamples = 0;
  pStats->ctr_rt.ctr_fracLostTot = 0;
  pStats->ctr_rt.ctr_fracLostSamples = 0;
  pStats->ctr_rt.ctr_cumulativeLost = 0;
  pStats->ctr_rt.ctr_cumulativeSeqNum = 0;
  pStats->ctr_rt.ctr_rrRcvd = 0;

}

/*
int stream_stats_addRTCPNACK(STREAM_STATS_T *pStats, const RTCP_PKT_RTPFB_NACK_T *pNack) {
  int rc = 0;

  return rc;
}
*/

int stream_stats_addRTCPRR(STREAM_STATS_T *pStats, pthread_mutex_t *pmtx, const STREAM_RTCP_SR_T *pSr) {
  int rc = 0;
  int seqDeltaRr = 0;
  int rrdelayUs;
  int lostDelta;
  float fracLost;
  uint16_t seqhighestRr;
  const RTCP_PKT_RR_T *pRr;

  if(pmtx) {
    pthread_mutex_lock(pmtx);
  } 

  if(!pStats || !pSr) {
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    } 
    return -1;
  }

  pthread_mutex_lock(&pStats->mtx);

  if(pStats->active) {

    pRr = &pSr->lastRrRcvd;

    fracLost = ((float) pRr->fraclost / 0xff) * 100.0f;
    pStats->ctr_rt.ctr_fracLostTot += fracLost;
    pStats->ctr_rt.ctr_fracLostSamples++;

    rrdelayUs = (pSr->tmLastRrRcvd - pSr->tmLastSrSent) - (int32_t) (htonl(pRr->dlsr) * 
                             (TIME_VAL_US / 65536.0f));
    if(rrdelayUs > 10 * TIME_VAL_US) {
      //
      // Assume invalid value
      //
      rrdelayUs = 0;
    } else if(rrdelayUs > 0) {
      pStats->ctr_rt.ctr_rrdelayMsTot += rrdelayUs / TIME_VAL_MS;
      pStats->ctr_rt.ctr_rrdelaySamples++;
    }
    seqhighestRr = htons(pRr->seqhighest);
  
    if(pStats->ctr_rt.numRrTot > 0) {
      seqDeltaRr = RTP_SEQ_DELTA(seqhighestRr, pStats->ctr_rt.seqhighestRrPrev);
    }
    pStats->ctr_rt.ctr_countseqRr += seqDeltaRr;
    pStats->ctr_rt.seqhighestRrPrev = seqhighestRr;

    if((lostDelta = pSr->prevcumulativelost - pStats->ctr_rt.cumulativeLostPrev) > 10000) {
      //
      // Assume invalid value
      //
      lostDelta = 0; 
    } else if(lostDelta > 0) {
      pStats->ctr_rt.ctr_cumulativeLost += lostDelta;
    }
    pStats->ctr_rt.cumulativeLostPrev = pSr->prevcumulativelost;

    //fprintf(stderr, "ctr:%d = rr:%llu - prior ctr:%d\n", pStats->ctr_rt.ctr_cumulativeSeqNum, pSr->reportedSeqNumTot, pStats->ctr_rt.ctr_cumulativeSeqNum);
    pStats->ctr_rt.ctr_cumulativeSeqNum = pSr->reportedSeqNumTot - pStats->ctr_rt.ctr_cumulativeSeqNum; 
    pStats->ctr_rt.cumulativeSeqNum = pSr->reportedSeqNumTot;


  //fprintf(stderr, "stream_stats_addrr pt:%d, numRrTot:%d, fraclost:%.2f, cumlost:%d (%d), rrdelay:%d us, seq SR:%d, RR:%d, seqDelta:%d\n", pStats->pRtpInit ? pStats->pRtpInit->pt : 0, pStats->ctr_rt.numRrTot, fracLost, pSr->prevcumulativelost, lostDelta, rrdelayUs, htons(pSr->seqhighest), seqhighestRr, seqDeltaRr);

    pStats->ctr_rt.numRrTot++;
    pStats->ctr_rt.ctr_rrRcvd++;

  } // endo of if(pStats->active)

  pthread_mutex_unlock(&pStats->mtx);

  if(pmtx) {
    pthread_mutex_unlock(pmtx);
  } 

  return rc;
}

static void stream_monitor_proc(void *pArg) {
  MONITOR_START_WRAP_T startWrap;
  STREAM_STATS_MONITOR_T *pMonitor = NULL;
  FILE *fp = NULL;
  unsigned int intervalMs;
  TIME_VAL tvNext, tv;

  memcpy(&startWrap, pArg, sizeof(startWrap));
  pMonitor = startWrap.pMonitor;

  logutil_tid_add(pthread_self(), startWrap.tid_tag);

  pMonitor->runMonitor = 1;
  if((intervalMs = pMonitor->dumpIntervalMs) <= 0) {
    intervalMs = STREAM_STATS_DUMP_MS;
  }

  //
  // Set the output file path 
  //
  if(pMonitor->statfilepath) {
    if(!strcasecmp(pMonitor->statfilepath, "stdout")) {
      fp = stdout;
    } else if(!strcasecmp(pMonitor->statfilepath, "stderr")) {
      fp = stderr;
    } else if(!(fp = fopen(pMonitor->statfilepath, "w"))) {
      LOG(X_ERROR("Failed top open stats file %s for writing"), pMonitor->statfilepath);
      pMonitor->runMonitor = -1;
    }
  } else {
    //fp = stdout;
  }

  if(pMonitor->runMonitor == 1) {
    tvNext = timer_GetTime() + (intervalMs * TIME_VAL_MS);
    LOG(X_INFO("Started %sstream output monitor %s%s"), 
                pMonitor->pAbr ? "ABR " : "",
                pMonitor->statfilepath ? "to " : "", 
                pMonitor->statfilepath ? pMonitor->statfilepath : "");
  }

  while(pMonitor->runMonitor == 1 && !g_proc_exit) {

    if((tv = timer_GetTime()) >= tvNext) {

      stream_monitor_dump(fp, pMonitor);

      tvNext += (intervalMs * TIME_VAL_MS);
    }

    usleep(100000);

  }

  if(fp) {
    fclose(fp);
  }

  pMonitor->runMonitor = -1;
  pthread_mutex_destroy(&pMonitor->mtx);
  pMonitor->active = -1;

  logutil_tid_remove(pthread_self());

}

int stream_monitor_start(STREAM_STATS_MONITOR_T *pMonitor, 
                         const char *statfilepath,
                         unsigned int intervalMs) {
  int rc = 0;
  pthread_t ptdMonitor;
  pthread_attr_t attrMonitor;
  const char *s;
  MONITOR_START_WRAP_T startWrap;

  if(!pMonitor) {
    return -1;
  } else if(pMonitor->active) {
    return 0;
  }

  if(statfilepath && statfilepath[0] == '\0') {
    statfilepath = NULL;
  }

  pthread_mutex_init(&pMonitor->mtx, NULL);

  pMonitor->active = 1;
  pMonitor->statfilepath = statfilepath;
  if(pMonitor->rangeMs1 >= 0) {
    pMonitor->rangeMs1 = STREAM_STATS_RANGE_MS_LOW;
  }
  if(pMonitor->rangeMs2 >= 0) {
    pMonitor->rangeMs2 =  STREAM_STATS_RANGE_MS_HIGH;
  }

  memset(&startWrap, 0, sizeof(startWrap));
  startWrap.pMonitor = pMonitor;
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(startWrap.tid_tag, sizeof(startWrap.tid_tag), "%s-mon", s);
  }

  pMonitor->runMonitor = 2;
  pMonitor->dumpIntervalMs = intervalMs;
  pthread_attr_init(&attrMonitor);
  pthread_attr_setdetachstate(&attrMonitor, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdMonitor,
                    &attrMonitor,
                    (void *) stream_monitor_proc,
                    (void *) &startWrap) != 0) {
    LOG(X_ERROR("Unable to create stream output statistics monitor thread"));
    pMonitor->active = 0;
    pMonitor->runMonitor = 0;
    pthread_mutex_destroy(&pMonitor->mtx);
    return -1;
  }

  while(pMonitor->runMonitor != 1 && pMonitor->runMonitor != -1) {
    usleep(5000);
  }

  return rc;
}

int stream_monitor_stop(STREAM_STATS_MONITOR_T *pMonitor) {
  int rc = 0;

  if(!pMonitor) {
    return -1;
  }

  pMonitor->active = 0;

  if(pMonitor->runMonitor > 0) {
    pMonitor->runMonitor = -2;
    while(pMonitor->runMonitor != -1) {
      usleep(5000);
    }
  }

  if(pMonitor->pAbr) {
    stream_abr_close(pMonitor->pAbr);
    avc_free((void **) &pMonitor->pAbr);
  }

  return rc;
}

static STREAM_STATS_T *monitor_find(STREAM_STATS_MONITOR_T *pMonitor, STREAM_STATS_T *pStats) {

  STREAM_STATS_T *pNode = pMonitor->plist;

  while(pNode) {
    if(pNode == pStats) {
      return pNode;
    }
    pNode = pNode->pnext;
  }
  return NULL;
}

static int stream_monitor_attach(STREAM_STATS_MONITOR_T *pMonitor, STREAM_STATS_T *pStats) {
  int rc = 0;
  STREAM_STATS_T *pNode = NULL;
  
  if(!pMonitor || !pMonitor->active || !pStats) {
    return -1;
  }

  pthread_mutex_lock(&pMonitor->mtx);

  if(monitor_find(pMonitor, pStats)) {
    pthread_mutex_unlock(&pMonitor->mtx);
    return -1;
  }

  if(!pMonitor->plist) {
    pMonitor->plist = pStats;
  } else {
    pNode = pMonitor->plist; 
    pMonitor->plist = pStats;
  }
  pStats->pnext = pNode;
  gettimeofday(&pStats->tvstart, NULL);
  pStats->pMonitor = pMonitor;
  pMonitor->count++;
  //fprintf(stderr, "ATTACH COUNT:%d\n", pMonitor->count);

  pthread_mutex_unlock(&pMonitor->mtx);

  return rc;
}

STREAM_STATS_T *stream_monitor_createattach(STREAM_STATS_MONITOR_T *pMonitor, 
                                            const struct sockaddr_in *psaRemote,
                                            STREAM_METHOD_T method,
                                            STREAM_MONITOR_ABR_TYPE_T abrEnabled) {
  unsigned int numWr = 1;
  unsigned int numRd = 1;
  STREAM_STATS_T *pStats;

  if(!pMonitor || !pMonitor->active || !psaRemote) {
    return NULL;
  }

  //
  // If this monitor is for use with Adaptive Bitrate control then we don't care about stream
  // data sources which do not account for the bitrate we intend to control.
  //
  if(pMonitor->pAbr && abrEnabled == STREAM_MONITOR_ABR_NONE) {
    return NULL;
  }

  if(abrEnabled == STREAM_MONITOR_ABR_ENABLED) {
    LOG(X_DEBUG("ABR is enabled for output -> %s://%s:%d"), 
        devtype_methodstr(method), inet_ntoa(psaRemote->sin_addr), htons(psaRemote->sin_port));
  }

  if(method == STREAM_METHOD_UDP) {

    //
    // payload over UDP w/o RTP header (MPEG-2 ts)
    //
    numRd = 0;

  } else if(method == STREAM_METHOD_UDP_RTP) {

    //
    // We try to account RTP and RTCP output independently
    //
    numWr = 2;

    //
    // For RTP output there is no TCP flow based in/out queing disparity
    //
    numRd = 0;
  }

  if(!(pStats = stream_stats_create(psaRemote, numWr, numRd, pMonitor->rangeMs1, pMonitor->rangeMs2))) {
    return NULL;
  }

  pStats->method = method;
  pStats->abrEnabled = abrEnabled;

  if(stream_monitor_attach(pMonitor, pStats) < 0) {
    stream_stats_destroy(&pStats, NULL);
  }

  return pStats;
}

static int stream_monitor_detach(STREAM_STATS_MONITOR_T *pMonitor, STREAM_STATS_T *pStats) {
  int rc = -1;
  STREAM_STATS_T *pNode;
  STREAM_STATS_T *pPrev = NULL;

  if(!pMonitor || !pStats) {
    return -1;
  }

  pthread_mutex_lock(&pMonitor->mtx);

  pNode = pMonitor->plist;
    
  while(pNode) {
    if(pNode == pStats) {
      if(pPrev) {
        pPrev->pnext = pNode->pnext;
      } else {
        pMonitor->plist = pNode->pnext;
      }
      pNode->pnext = NULL;
      rc = 0;
      break;
    } 

    pPrev = pNode;
    pNode = pNode->pnext;
  }

  //fprintf(stderr, "DETACH COUNT:%d\n", pMonitor->count);

  if(pMonitor->count > 0) {
    pMonitor->count--;
  }

  pthread_mutex_unlock(&pMonitor->mtx);

  return rc;
}


//#define THROUGHPUT_BYTES_IN_KILO F          1000.0f
//#define THROUGHPUT_BYTES_IN_KILO_F          1024.0f
//#define THROUGHPUT_BYTES_IN_MEG_F           (THROUGHPUT_BYTES_IN_KILO_F * THROUGHPUT_BYTES_IN_KILO_F) 

static char *printDuration(char *buf, unsigned int szbuf, float ms) {
  int rc = 0;

  if(ms > 1000) {
    rc = snprintf(buf, szbuf, "%.1fs", ms/1000.0f);
  } else {
    rc = snprintf(buf, szbuf, "%dms", (int) ms);
  }

  if(rc <= 0 && szbuf > 0) {
    buf[0] = '\0'; 
  }

  return buf;
}

static void stream_stats_newinterval(STREAM_STATS_T *pStats, struct timeval *ptv) {

  pthread_mutex_lock(&pStats->mtx);

  //
  // Copy the current real-time snapshot stats
  //

  TIME_TV_SET(pStats->tv_last, *ptv);

  memcpy(&pStats->ctr_last, &pStats->ctr_rt, sizeof(pStats->ctr_last));

  //
  // intentional shallow copy only
  //
  memcpy(pStats->throughput_last, pStats->throughput_rt, sizeof(pStats->throughput_last));

  //
  // Reset any running counters
  //
  stream_stats_resetctrs(pStats);

  pthread_mutex_unlock(&pStats->mtx);

}

static char *dump_throughput(char *buf, unsigned int szbuf, const char *prfx, 
                             THROUGHPUT_STATS_T *pS, int urlidx) {
  int rc;
  unsigned int idx;
  unsigned int idxbuf = 0;
  char buftmp[256];

  if(szbuf > 0) {
    buf[0] = '\0';
  }

  if(!pS) {
    return buf;
  }

  if(pS->written.bytes > 0) {

    burstmeter_printBytes(buftmp, sizeof(buftmp), pS->written.bytes);

    if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&out%d=%s", urlidx, buftmp)) > 0) ||
       (!urlidx &&  (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", %swrote: %s", prfx, buftmp)) > 0)) {
      idxbuf += rc;
    }

    for(idx = 0; idx < sizeof(pS->bitratesWr) / sizeof(pS->bitratesWr[0]); idx++) {

      if(pS->bitratesWr[idx].meter.rangeMs > 0) {
  
        //
        // Roll up any real-time rate based counters
        //
        burstmeter_updateCounters(&pS->bitratesWr[idx], NULL);

        burstmeter_printThroughput(buftmp, sizeof(buftmp), &pS->bitratesWr[idx]);

        rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", %.1f pkts/s", burstmeter_getPacketratePs(&pS->bitratesWr[idx]));
        idxbuf += rc;

        if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&out%.0fs%d=%s",
                      (float) pS->bitratesWr[idx].meter.rangeMs/1000.0f, urlidx, buftmp)) > 0) ||
           (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", %.0fs: %s", 
                     (float) pS->bitratesWr[idx].meter.rangeMs/1000.0f, buftmp)) > 0)) {
          idxbuf += rc;
        }
      }

    }
  }

  if(pS->read.bytes > 0) {

    burstmeter_printBytes(buftmp, sizeof(buftmp), pS->read.bytes);

    if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&in%d=%s", urlidx, buftmp)) > 0) ||
       (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", read: %s", buftmp)) > 0)) {
      idxbuf += rc;
    }

    for(idx = 0; idx < sizeof(pS->bitratesRd) / sizeof(pS->bitratesRd[0]); idx++) {

      if(pS->bitratesRd[idx].meter.rangeMs > 0) {

        burstmeter_printThroughput(buftmp, sizeof(buftmp), &pS->bitratesRd[idx]);

        if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&in%.0fs%d=%s",
                        (float) pS->bitratesRd[idx].meter.rangeMs/1000.0f, urlidx, buftmp)) > 0) ||
           (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", %.0fs: %s", 
                      (float) pS->bitratesRd[idx].meter.rangeMs/1000.0f, buftmp)) > 0)) {
          idxbuf += rc;
        }

      }
    }
  }

  if(pS->skipped.bytes > 0) {
    burstmeter_printBytes(buftmp, sizeof(buftmp), pS->skipped.bytes);

    if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&skip%d=%s", urlidx, buftmp)) > 0) ||
       (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", skipped: %s", buftmp)) > 0)) {
      idxbuf += rc;
    }
  }

  if(pS->read.bytes > 0) {
    burstmeter_printBytes(buftmp, sizeof(buftmp), pS->written.bytes - pS->read.bytes - pS->skipped.bytes);

    if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&queue%d=%s", urlidx, buftmp)) > 0) ||
       (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", queued: %s", buftmp)) > 0)) {
      idxbuf += rc;
    }

    //fprintf(fp, " %s", printDuration(buftmp, sizeof(buftmp), PTSF_MS(pS->tmLastWr.tm - pS->tmLastRd.tm)));
  }



  /*  
  fprintf(fp, "\nwritten:%lld,%lld, read:%lld,%lld, skipped:%lld,%lld { delta:%lld,%lld %.3fms, tmLastWr:%.3f, tmLastRd:%.3f\n", pS->written.bytes, pS->written.slots, pS->read.bytes, pS->read.slots, pS->skipped.bytes, pS->skipped.slots, (pS->written.bytes - pS->read.bytes - pS->skipped.bytes), (pS->written.slots - pS->read.slots - pS->skipped.slots),PTSF_MS(pS->tmLastWr.tm- pS->tmLastRd.tm), PTSF(pS->tmLastWr.tm), PTSF(pS->tmLastRd.tm));

    for(idx = 0; idx < sizeof(pS->bitratesWr) / sizeof(pS->bitratesWr[0]); idx++) {
      if(pS->bitratesWr[idx].meter.rangeMs > 0) {
        fprintf(fp, "writer - [%d] %dms cur: %.2fKb/s max: %.2fKb/s ", idx, pS->bitratesWr[idx].meter.rangeMs, pS->bitratesWr[idx].meter.cur.bytes * (1000.0f / pS->bitratesWr[idx].meter.rangeMs) / 125.0f, pS->bitratesWr[idx].meter.max.bytes * (1000.0f / pS->bitratesWr[idx].meter.rangeMs) / 125.0f);
      }
   }
   fprintf(fp, "\n");

   for(idx = 0; idx < sizeof(pS->bitratesWr) / sizeof(pS->bitratesWr[0]); idx++) {
     if(pS->bitratesRd[idx].meter.rangeMs > 0) {
        fprintf(fp, "reader - [%d] %dms cur: %.2fKb/s max: %.2fKb/s ", idx, pS->bitratesRd[idx].meter.rangeMs, pS->bitratesRd[idx].meter.cur.bytes * (1000.0f / pS->bitratesRd[idx].meter.rangeMs) / 125.0f, pS->bitratesRd[idx].meter.max.bytes * (1000.0f / pS->bitratesRd[idx].meter.rangeMs) / 125.0f);
      }
    }
    fprintf(fp, "\n");
*/

  return buf;
}

static int stream_stats_dump(char *buf, unsigned int szbuf, STREAM_STATS_T *pStats, int urlidx) {
  int rc;
  uint32_t durationms;
  float fracLost, f;
  unsigned int idxbuf = 0;
  STREAM_RTP_INIT_T *pRtpInit;
  char buftmp[2048];

  if(pStats->tvstart.tv_sec > 0) {
    durationms = TIME_TV_DIFF_MS(pStats->tv_last, pStats->tvstart);
  } else {
    durationms = 0;
  }

  printDuration(buftmp, sizeof(buftmp), durationms);

  if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&method%d=%s&duration%d=%s&host%d=%s:%d", 
                 urlidx, devtype_methodstr(pStats->method), urlidx, buftmp, urlidx,
                 inet_ntoa(pStats->saRemote.sin_addr), htons(pStats->saRemote.sin_port))) > 0) ||
     (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "%7s %s -> %15s:%d", 
       devtype_methodstr(pStats->method), buftmp, 
       inet_ntoa(pStats->saRemote.sin_addr), htons(pStats->saRemote.sin_port))) > 0)) {
    idxbuf += rc;
  }

  if(pStats->pRtpDest && pStats->pRtpDest->pRtpMulti) {
    pRtpInit = &pStats->pRtpDest->pRtpMulti->init;
    if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&pt%d=%d", urlidx, pRtpInit->pt)) > 0) ||
       (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, " pt:%3d", pRtpInit->pt)) > 0)) {
      idxbuf += rc;
    } 
  }

  if((rc = snprintf(&buf[idxbuf], szbuf - idxbuf, 
           "%s", dump_throughput(buftmp, sizeof(buftmp), "", &pStats->throughput_last[0], urlidx))) > 0) {
    idxbuf += rc;
  }

  if(pStats->numWr > 1 && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, 
           "%s", dump_throughput(buftmp, sizeof(buftmp), "rtcp ", &pStats->throughput_last[1], urlidx))) > 0) {
    idxbuf += rc;
  }

  if(pStats->ctr_last.ctr_rrRcvd > 0) {
   //fprintf(fp, " ctr_last.ctr_rrRcvd:%d", pStats->ctr_last.ctr_rrRcvd);
    if(pStats->ctr_last.ctr_rrdelaySamples > 0) {
      f = (float) pStats->ctr_last.ctr_rrdelayMsTot / pStats->ctr_last.ctr_rrdelaySamples;

      if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&rtdelay%d=%.1f", urlidx, f)) > 0) ||
         (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", rtdelay: %.1f", f)) > 0)) {
        idxbuf += rc;
      }
    }

    if(pStats->ctr_last.ctr_fracLostSamples > 0) {
      fracLost = pStats->ctr_last.ctr_fracLostTot / pStats->ctr_last.ctr_fracLostSamples;

      if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&fraclost%d=%.2f", urlidx, fracLost)) > 0) ||
         (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, ", frac-lost: %.2f%%", fracLost)) > 0)) {
        idxbuf += rc;
      }
    }

    if(pStats->ctr_last.cumulativeSeqNum > 0) {
      fracLost = (float) 100.0f * pStats->ctr_last.cumulativeLostPrev / pStats->ctr_last.cumulativeSeqNum;
    } else {   
      fracLost = 0;
    }

    if((urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "&cml-lost%d=%d&cml-pkts%d=%u&tot-lost%d=%u&tot%d=%u&fractotlost%d=%.2f", 
                   urlidx, pStats->ctr_last.ctr_cumulativeLost,
                   urlidx, pStats->ctr_last.ctr_countseqRr,
                   urlidx, pStats->ctr_last.cumulativeLostPrev, 
                   urlidx, pStats->ctr_last.cumulativeSeqNum,
                   urlidx, fracLost)) > 0) ||

       (!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, 
                    ", cml-lost: %d/%d (all:%d/%d, %.2f%%)", pStats->ctr_last.ctr_cumulativeLost, 
                    pStats->ctr_last.ctr_countseqRr, pStats->ctr_last.cumulativeLostPrev, pStats->ctr_last.cumulativeSeqNum, fracLost)) > 0)) {

      idxbuf += rc;
    }
  }

  if(!urlidx && (rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "\n")) > 0) {
    idxbuf += rc;
  }

  return (int) idxbuf;
}

static void stream_monitor_dump(FILE *fp, STREAM_STATS_MONITOR_T *pMonitor) {
  STREAM_STATS_T *pStats;
  time_t tm;
  struct timeval tv;
  char buf[1024];

  if(!pMonitor) {
    return ;
  }

  gettimeofday(&tv, NULL);

  pthread_mutex_lock(&pMonitor->mtx);

  if(fp) {
    tm = time(NULL);
    strftime(buf, sizeof(buf) -1, "%H:%M:%S %m/%d/%Y", localtime(&tm));
    fprintf(fp, "\n======= %s %d Output Streams =======\n", buf, pMonitor->count);
  }

  pStats = pMonitor->plist;
    
  while(pStats) {

    if(pStats->active) {

      stream_stats_newinterval(pStats, &tv);

      //
      // Invoke the ABR calback
      //
      if(pMonitor->pAbr && pStats->abrEnabled == STREAM_MONITOR_ABR_ENABLED) {
        stream_abr_cbOnStream(pStats);
      }

      if(fp) {
        if(stream_stats_dump(buf, sizeof(buf), pStats, 0) > 0) {
          fprintf(fp, "%s", buf);
        }
      }

    }

    pStats = pStats->pnext;
  }

  pthread_mutex_unlock(&pMonitor->mtx);

  if(fp) {
    fflush(fp);
  }

  return;
}


int stream_monitor_dump_url(char *buf, unsigned int szbuf, STREAM_STATS_MONITOR_T *pMonitor) {
  STREAM_STATS_T *pNode;
  unsigned int idxbuf = 0;
  unsigned int urlidx = 0;
  int rc;
  time_t tm;
  struct timeval tv;
  char tmbuf[128];

  if(!pMonitor || !buf) {
    return -1;
  }

  gettimeofday(&tv, NULL);

  pthread_mutex_lock(&pMonitor->mtx);

  tm = time(NULL);
  strftime(tmbuf, sizeof(tmbuf)-1, "%H:%M:%S_%m/%d/%Y", localtime(&tm));
  if((rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "date=%s&count=%d", tmbuf, pMonitor->count)) > 0) {
    idxbuf += rc;
  }

  pNode = pMonitor->plist;
    
  while(pNode) {

    if(pNode->active) {

      if((rc = stream_stats_dump(&buf[idxbuf], szbuf - idxbuf, pNode, ++urlidx)) > 0) {
        idxbuf += rc;
      }

    }

    pNode = pNode->pnext;
  }

  pthread_mutex_unlock(&pMonitor->mtx);

  return (int) idxbuf;
}



#endif // VSX_HAVE_STREAMER
