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


#define SAMPLE_LEN_ROLLUP  0xffffffff

#if 1
void burstmeter_dump(const BURSTMETER_SAMPLE_SET_T *pSamples) {
  unsigned int idx;
  uint32_t bytes = 0;
  uint32_t pkts = 0;

  for(idx = 0; idx < pSamples->countPeriods + 1; idx++) {
    bytes += pSamples->samples[idx].bytes;
    pkts += pSamples->samples[idx].packets;
    fprintf(stderr, "[%d].pkts:%d, bytes:%d\n", idx, pSamples->samples[idx].packets, pSamples->samples[idx].bytes);
  }
  fprintf(stderr, "idxLatest:%d, period:%d, range:%d, tot-bytes:%u, tot-pkts:%u, meter-cur-bytes:%u, meter-cur-pkts:%u\n", pSamples->idxLatest, pSamples->meter.periodMs, pSamples->meter.rangeMs, bytes, pkts, pSamples->meter.cur.bytes, pSamples->meter.cur.packets);

}
#endif // 0

int burstmeter_copy(BURSTMETER_SAMPLE_SET_T *pSamples, const BURSTMETER_SAMPLE_SET_T *pFrom) {
  int rc = 0;

  if(!pSamples || !pFrom || !pFrom->meter.periodMs <= 0 || !pFrom->meter.rangeMs <= 0) {
    return -1;
  }

  memset(pSamples, 0, sizeof(BURSTMETER_SAMPLE_SET_T));
  pSamples->countPeriods = pFrom->meter.rangeMs / pFrom->meter.periodMs;
  if(pFrom->meter.periodMs * pSamples->countPeriods != pFrom->meter.rangeMs) {
    return -1;
  }
  
  pSamples->meter.periodMs = pFrom->meter.periodMs;
  pSamples->meter.rangeMs = pFrom->meter.rangeMs;

  if(!(pSamples->samples = avc_calloc(pSamples->countPeriods + 1, sizeof(BURSTMETER_SAMPLE_T)))) {
    return -1;
  }

  pSamples->nolock = 1;

  return rc;
}

int burstmeter_init(BURSTMETER_SAMPLE_SET_T *pSamples,
                 unsigned int periodMs,
                 unsigned int rangeMs) {
  int rc = 0;

  if(!pSamples || periodMs <= 0 || rangeMs <= 0) {
    return -1;
  }

  memset(pSamples, 0, sizeof(BURSTMETER_SAMPLE_SET_T));

  pSamples->countPeriods = rangeMs / periodMs;
  if(periodMs * pSamples->countPeriods != rangeMs) {
    return -1;
  }

  pSamples->meter.periodMs = periodMs;
  pSamples->meter.rangeMs = rangeMs;

  if(!(pSamples->samples = avc_calloc(pSamples->countPeriods + 1, sizeof(BURSTMETER_SAMPLE_T)))) {
    return -1;
  }

  pthread_mutex_init(&pSamples->mtx, NULL);

  //fprintf(stderr, "INIT COUNT:%d, %d,%d\n", pSamples->countPeriods, pSamples->periodMs, pSamples->rangeMs);

  return rc;
}

void burstmeter_close(BURSTMETER_SAMPLE_SET_T *pSamples) {

  if(pSamples) {

    if(pSamples->samples) {
      avc_free((void *) &pSamples->samples);
    }

    if(!pSamples->nolock) {
      pthread_mutex_destroy(&pSamples->mtx);
    }
  }
}

int burstmeter_reset(BURSTMETER_SAMPLE_SET_T *pSamples) {
  int rc = 0;

  if(!pSamples || !pSamples->samples) {
    return -1;
  }
 
  pSamples->tmLatestSample.tv_sec = 0;
  pSamples->tmLatestSample.tv_usec = 0;
  pSamples->offsetBoundary = 0;
  pSamples->idxLatest = 0;
  memset(&pSamples->meter.cur, 0, sizeof(pSamples->meter.cur));
  memset(&pSamples->meter.max, 0, sizeof(pSamples->meter.max));
  memset(pSamples->samples, 0, (pSamples->countPeriods + 1) * sizeof(BURSTMETER_SAMPLE_T));

  return rc;
}

static int addSample(BURSTMETER_SAMPLE_SET_T *pSamples, unsigned int sampleLen,
                      const struct timeval *pTv) {

  int usOffsetCur;
  int idxOffsetCur;
  unsigned int ui;
  unsigned int idxLatest;
  unsigned int idxEarliest;
  int doReset = 0;
  struct timeval tv;

  if(!pTv) {
    gettimeofday(&tv, NULL);
    pTv = &tv;
  }

  //fprintf(stderr, "burst meter addSample  pTvLatest:%u:%6d pTv: %u:%6d\n", pSamples->tmLatestSample.tv_sec, pSamples->tmLatestSample.tv_usec , pTv->tv_sec, pTv->tv_usec);

  if(pSamples->tmLatestSample.tv_sec == 0) {

    pSamples->idxLatest = 0;
    idxOffsetCur = 0;

    pSamples->tmLatestSample.tv_sec = pTv->tv_sec;
    pSamples->tmLatestSample.tv_usec = pTv->tv_usec;

    // This is the offset of the current packet from the start of the latest 
    // bandwidth sample boundary
    pSamples->offsetBoundary = 0;
    
  } else {

    TIME_VAL tv64 = (TIME_VAL) (pTv->tv_sec * TIME_VAL_US) + pTv->tv_usec;
    TIME_VAL tv64Latest = (TIME_VAL) (pSamples->tmLatestSample.tv_sec * TIME_VAL_US) + pSamples->tmLatestSample.tv_usec;

    if(tv64 > tv64Latest) {
      usOffsetCur = tv64 - tv64Latest; 
    } else {
      usOffsetCur = 0;
    }

    //if((uint64_t)(pTv->tv_sec * TIME_VAL_US) + pTv->tv_usec <
    //   (uint64_t)(pSamples->tmLatestSample.tv_sec * TIME_VAL_US) + pSamples->tmLatestSample.tv_usec) {
    //  fprintf(stderr, "burst meter time goes backwards, pTvLatest:%u:%6d pTv: %u:%6d\n", pSamples->tmLatestSample.tv_sec, pSamples->tmLatestSample.tv_usec , pTv->tv_sec, pTv->tv_usec);
    //}
  
    // 0x7fffffff usec /1000 / 1000 = 2147 sec
    //if(pTv->tv_sec - pSamples->tmLatestSample.tv_sec > 2147) {
    //  LOG(X_WARNING("burst sample time difference too large: %"SECT" - %"USECT), 
    //      pTv->tv_sec, pSamples->tmLatestSample.tv_sec);
    //  usOffsetCur = 0x7fffffff;
    //} else {
    //  usOffsetCur = ((pTv->tv_sec - pSamples->tmLatestSample.tv_sec) * 1000000) +
    //                 (pTv->tv_usec - pSamples->tmLatestSample.tv_usec + pSamples->offsetBoundary);
    //}
  
    pSamples->offsetBoundary = (unsigned short) (usOffsetCur % (1000 * pSamples->meter.periodMs)); 

    idxOffsetCur = (usOffsetCur / 1000 / pSamples->meter.periodMs);

  }

  //fprintf(stderr, "burst len:%d, %u:%6d tmLatest:(%u:%6d), idxOffsetCur:%d, boundary:%d, usOffsetCur:%d, idxLatest:%d\n", sampleLen, pTv->tv_sec, pTv->tv_usec, pSamples->tmLatestSample.tv_sec, pSamples->tmLatestSample.tv_usec, idxOffsetCur, pSamples->offsetBoundary, usOffsetCur, pSamples->idxLatest);

  //
  // If the latest sample data fits into a new sample slot, expire
  // any sample slots from history that are being overwritten
  //
  if(idxOffsetCur > 0) {

    //if((idxEarliest = pSamples->idxLatest + 1) >= pSamples->countPeriods) {
    if((idxEarliest = pSamples->idxLatest + 1) > pSamples->countPeriods) {
      idxEarliest = 0u;
    }


    idxLatest = pSamples->idxLatest;
    for(ui = 0u; ui < (unsigned int) idxOffsetCur; ui++) {

      //fprintf(stderr, "EXPIRING SLOT[earliest:%d,latest:%d], ui:%d/%d  %d bytes, %d inserts, periods:%d\n", idxEarliest, idxLatest, ui, idxOffsetCur, pSamples->samples[idxEarliest].bytes, pSamples->samples[idxEarliest].packets, pSamples->countPeriods); if(ui == 0) burstmeter_dump(pSamples);

      if(ui > pSamples->countPeriods) {
        doReset = 1;
        break;
      }

      pSamples->meter.cur.bytes += pSamples->samples[idxLatest].bytes;
      pSamples->meter.cur.packets += pSamples->samples[idxLatest].packets;

      pSamples->meter.cur.bytes -= pSamples->samples[idxEarliest].bytes;
      pSamples->meter.cur.packets -= pSamples->samples[idxEarliest].packets;

      pSamples->samples[idxEarliest].bytes = 0;
      pSamples->samples[idxEarliest].packets = 0;

      if(pSamples->meter.cur.bytes > pSamples->meter.max.bytes) {
        pSamples->meter.max.bytes = pSamples->meter.cur.bytes;
      }

      if(pSamples->meter.cur.packets > pSamples->meter.max.packets) {
        pSamples->meter.max.packets = pSamples->meter.cur.packets;
      }

      if(++idxEarliest > pSamples->countPeriods) {
        idxEarliest = 0u;
      }
      if(++idxLatest > pSamples->countPeriods) {
        idxLatest = 0u;
      }
    }

  }

  if(idxOffsetCur > 0) {

    pSamples->idxLatest = (pSamples->idxLatest + idxOffsetCur) % (pSamples->countPeriods + 1);

    if(doReset) {
      //fprintf(stderr, "burstmeter doReset %u:%6d, %u:%6d\n\n\n\n\n\n", pSamples->tmLatestSample.tv_sec, pSamples->tmLatestSample.tv_usec , pTv->tv_sec, pTv->tv_usec);
      pSamples->tmLatestSample.tv_sec = pTv->tv_sec;
      pSamples->tmLatestSample.tv_usec = pTv->tv_usec;
    } else {
      TV_INCREMENT_MS(pSamples->tmLatestSample, idxOffsetCur * pSamples->meter.periodMs); 
    }

    //fprintf(stderr, "idxLatest now: %d, tmLatest:(%u:%6d), idxOffsetCur:%d, doReset:%d\n", pSamples->idxLatest, pSamples->tmLatestSample.tv_sec, pSamples->tmLatestSample.tv_usec, idxOffsetCur, doReset);
  }

/*
  //
  // For each bandwidth meter, increment the latest bandwidth sample
  //
  if(numMeters > 0) {
    for(idxMeter = 0u; idxMeter < numMeters; idxMeter++) {

      if((pMeters[idxMeter].cur.bytes += sampleLen) > pMeters[idxMeter].max.bytes)
        pMeters[idxMeter].max.bytes = pMeters[idxMeter].cur.bytes;

      if((++pMeters[idxMeter].cur.packets) > pMeters[idxMeter].max.packets)
        pMeters[idxMeter].max.packets = pMeters[idxMeter].cur.packets;
    }
  }
*/

  //
  // Increment the latest bandwidth sample
  //
  if(sampleLen != SAMPLE_LEN_ROLLUP) {
    pSamples->samples[pSamples->idxLatest].bytes += sampleLen;
    pSamples->samples[pSamples->idxLatest].packets++;
  }

//int totB=0, totP=0; for(ui=0; ui<pSamples->countPeriods + 1;ui++) { if(ui!=pSamples->idxLatest) totB+= pSamples->samples[ui].bytes; if(ui!=pSamples->idxLatest) totP+= pSamples->samples[ui].packets; }
//  fprintf(stderr, "INSERTED %d bytes INTO 0x%x SLOT[%d] %d bytes, %d inserts, cur_byte:%d, cur_pkts:%d, max_bytes:%d, max_pkts:%d, totB:%d, totP:%d\n", sampleLen, pSamples, pSamples->idxLatest, pSamples->samples[pSamples->idxLatest].bytes, pSamples->samples[pSamples->idxLatest].packets, pSamples->meter.cur.bytes, pSamples->meter.cur.packets, pSamples->meter.max.bytes, pSamples->meter.max.packets, totB, totP);//burstmeter_dump(pSamples);

  return 0;
}


int burstmeter_AddSample(BURSTMETER_SAMPLE_SET_T *pSamples, 
                      unsigned int sampleLen,
                      const struct timeval *pTv) {
  int rc = 0;

  if(!pSamples || !pSamples->samples || pSamples->meter.periodMs <= 0) {
    return -1;
  }

  if(!pSamples->nolock) {
    pthread_mutex_lock(&pSamples->mtx);
  }

  rc = addSample(pSamples, sampleLen, pTv);

  if(!pSamples->nolock) {
    pthread_mutex_unlock(&pSamples->mtx);
  }

  return rc;
}

int burstmeter_AddDecoderSample(BURSTMETER_DECODER_SET_T *pSet,
                               unsigned int sampleLen,
                               unsigned int frameId) {

  //unsigned int idx;                                 
  struct timeval tv;
  //BURSTMETER_METER_T *pExtraMeters[(sizeof(pSet->extraMeters) / sizeof(pSet->extraMeters[0]))];
  

  if(!pSet || pSet->clockHz == 0 || pSet->frameDeltaHz == 0) {
    //pSet->numExtraMeters >= (sizeof(pSet->extraMeters) / sizeof(pSet->extraMeters[0]))) {
    return -1;
  }

  tv.tv_sec = (long) (((long long)frameId * pSet->frameDeltaHz) / pSet->clockHz) + 1;
  tv.tv_usec =  (long) (((double)(((long long)frameId * pSet->frameDeltaHz) % 
                          pSet->clockHz) / pSet->clockHz) * 1000000);

  //for(idx = 0; idx < pSet->numExtraMeters; idx++) {
  //  pExtraMeters[idx] = &pSet->extraMeters[idx];
  //}

  //return burstmeter_AddSample(&pSet->set, sampleLen, &tv, *pExtraMeters, pSet->numExtraMeters);
  return burstmeter_AddSample(&pSet->set, sampleLen, &tv);
}

int burstmeter_updateCounters2(BURSTMETER_SAMPLE_SET_T *pSamples) {
  return burstmeter_updateCounters(pSamples, NULL);
}

int burstmeter_updateCounters(BURSTMETER_SAMPLE_SET_T *pSamples, const struct timeval *pTv) {
  int rc = 0;
  struct timeval tvnow;
  int64_t msdiff;
  struct timeval tv;
  //const unsigned int thresholdMs = 50;

  //
  // This should be called before attempting to read any of the rate based counter stats
  // such as burstmeter_getBitrateBps, burstmeter_getPacketrateBps
  //

  if(!pSamples || !pSamples->samples || pSamples->meter.periodMs <= 0) {
    return -1;
  } else if(pSamples->tmLatestSample.tv_sec == 0) {
    return 0; 
  }

  if(!pSamples->nolock) {
    pthread_mutex_lock(&pSamples->mtx);
  }

  if(!pTv) {
    tv.tv_sec = pSamples->tmLatestSample.tv_sec;
    tv.tv_usec = pSamples->tmLatestSample.tv_usec;
    pTv = &tv;
  }
  gettimeofday(&tvnow, NULL);
  msdiff = (int64_t) TIME_TV_DIFF_MS(tvnow, *pTv);

  if(msdiff <= 0) {
    pthread_mutex_unlock(&pSamples->mtx);
    return 0;
  }

  //
  // If a time greater than our threshold has expired between now
  // and the last time a sample was added, add an empty sample
  // to roll-up (pop) any sample data, otherwise, the counters will always
  // be stuck at the latest value for each request to get the packet / bit rate.
  //
  //if(TIME_TV_DIFF_MS(*pTv, pSamples->tmLatestSample) > thresholdMs) {
    //LOG(X_DEBUG("*pTv: %u:%u, tmLatestSample: %u:%u, %lld > thresholdMs"), pTv->tv_sec, pTv->tv_usec, pSamples->tmLatestSample.tv_sec, pSamples->tmLatestSample.tv_usec, msdiff);
    rc = addSample(pSamples, SAMPLE_LEN_ROLLUP, pTv);
  //}

  if(!pSamples->nolock) {
    pthread_mutex_unlock(&pSamples->mtx);
  }

  return rc;
}

int burstmeter_printBytes(char *buf, unsigned int szbuf, int64_t b) {
  int rc = 0;

  if(b >= 1048576) {
    rc = snprintf(buf, szbuf, "%.2fMB", (float)b/1048576.0f );
  } else if(b >= 1024) {
    rc = snprintf(buf, szbuf, "%.2fKB", (float)b/1024.0f );
  } else {
    rc = snprintf(buf, szbuf, "%lldB", b );
  }

  if(rc <= 0 && szbuf > 0) {
    buf[0] = '\0';
    rc = 0;
  }

  return rc;
}
char *burstmeter_printBytesStr(char *buf, unsigned int szbuf, int64_t b) {
  burstmeter_printBytes(buf, szbuf, b);
  return buf;
}

float burstmeter_getBitrateBps(const BURSTMETER_SAMPLE_SET_T *pBurstSet) {
  float bps = 0;

  if(!pBurstSet) {
    return 0;
  }

  if(!pBurstSet->nolock) {
    pthread_mutex_lock(&((BURSTMETER_SAMPLE_SET_T *) pBurstSet)->mtx);
  }

  if(pBurstSet->meter.rangeMs > 0) {
    bps = pBurstSet->meter.cur.bytes * (8000.0f / pBurstSet->meter.rangeMs);
  }

  if(!pBurstSet->nolock) {
    pthread_mutex_unlock(&((BURSTMETER_SAMPLE_SET_T *) pBurstSet)->mtx);
  }

  return bps;
}

float burstmeter_getPacketratePs(const BURSTMETER_SAMPLE_SET_T *pBurstSet) {
  float pktps = 0;

  if(!pBurstSet) {
    return 0;
  }

  if(!pBurstSet->nolock) {
    pthread_mutex_lock(&((BURSTMETER_SAMPLE_SET_T *) pBurstSet)->mtx);
  }

  if(pBurstSet->meter.rangeMs > 0) {
    pktps = pBurstSet->meter.cur.packets * (1000.0f / pBurstSet->meter.rangeMs);
  }

  if(!pBurstSet->nolock) {
    pthread_mutex_unlock(&((BURSTMETER_SAMPLE_SET_T *) pBurstSet)->mtx);
  }

  return pktps;
}

static int printBitrate(char *buf, unsigned int szbuf, float bps) {
  int rc = 0;
  //float _bps = Bps * 8;

  if(bps >= THROUGHPUT_BYTES_IN_MEG_F  ) {
    rc = snprintf(buf, szbuf, "%.2fMb/s", (float) bps/THROUGHPUT_BYTES_IN_MEG_F );
  } else if(bps >= THROUGHPUT_BYTES_IN_KILO_F ) {
    rc = snprintf(buf, szbuf, "%.2fKb/s", (float) bps/THROUGHPUT_BYTES_IN_KILO_F );
  } else {
    rc = snprintf(buf, szbuf, "%.2fb/s", bps);
  }

  if(rc <= 0 && szbuf > 0) {
    buf[0] = '\0';
    rc = 0;
  }

  return rc;
}

char *burstmeter_printBitrateStr(char *buf, unsigned int szbuf, float bps) {
  printBitrate(buf, szbuf, bps);
  return buf;
}

int burstmeter_printThroughput(char *buf, unsigned int szbuf, const BURSTMETER_SAMPLE_SET_T *pBurstSet) {

  if(pBurstSet->meter.rangeMs > 0) {

    return printBitrate(buf, szbuf, burstmeter_getBitrateBps(pBurstSet));

  } else {

    if(szbuf > 0) {
      buf[0] = '\0';
    }

  }
  return 0;
}

char *burstmeter_printThroughputStr(char *buf, unsigned int szbuf, const BURSTMETER_SAMPLE_SET_T *pBurstSet) {
  burstmeter_printThroughput(buf, szbuf, pBurstSet);
  return buf;
}

#if 0
void testbw() {

/*
#define ADD_US_TO_TV(us, tv) { tv.tv_usec += (us); \
                              if((us) > 0) { \
                                while(tv.tv_usec > 999999) { tv.tv_usec -= 1000000; tv.tv_sec ++; } \
                              } else { \
                                while(tv.tv_usec < 0) { tv.tv_usec += 1000000; tv.tv_sec --; }   \
                              } }
#define ADD_MS_TO_TV(ms, tv) ADD_US_TO_TV(((ms) * 1000), (tv))
*/

  int idx;
  BURSTMETER_DECODER_SET_T decoderBw;

  memset(&decoderBw, 0, sizeof(decoderBw));
  decoderBw.clockHz = 90000;
  decoderBw.frameDeltaHz = 30000;
  decoderBw.set.meter.periodMs = 1000;
  decoderBw.extraMeters[0].periodMs = 250;
  decoderBw.extraMeters[1].periodMs = 3000;
  decoderBw.numExtraMeters = 2;

  bandwidth_AddDecoderSample(&decoderBw, 100, 0);
  bandwidth_AddDecoderSample(&decoderBw, 100, 1);
  bandwidth_AddDecoderSample(&decoderBw, 111, 1);
  bandwidth_AddDecoderSample(&decoderBw, 150, 2);
  bandwidth_AddDecoderSample(&decoderBw, 140, 29);
  bandwidth_AddDecoderSample(&decoderBw, 140, 31);

  fprintf(stderr, "1sec: %.2f bytes/s %.2f pkts/s  \n",
    (decoderBw.set.meter.max.bytes * (1000.0f / decoderBw.set.meter.periodMs)),
    (decoderBw.set.meter.max.packets * (1000.0f / decoderBw.set.meter.periodMs)));

  for(idx = 0; idx < decoderBw.numExtraMeters; idx++) {

    fprintf(stderr, "%u ms: %.2f bytes/s %.2f pkts/s  \n",
      decoderBw.extraMeters[idx].periodMs,
      (decoderBw.extraMeters[idx].max.bytes * (1000.0f / decoderBw.extraMeters[idx].periodMs)),
      (decoderBw.extraMeters[idx].max.packets * (1000.0f / decoderBw.extraMeters[idx].periodMs)));
    }

}
#endif // 0
