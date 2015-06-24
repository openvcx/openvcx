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


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "logutil.h"
#include "mixer/mixer_int.h"
#include "mixer/mixer.h"

//#define  DEBUG_MIXER_TIMING 1

#define LOG_CTXT  pMixer->logCtxt

#define ADVANCE_RDR(rb) if(++(rb).samplesRdIdx >= (rb).samplesSz) { \
                               (rb).samplesRdIdx = 0; } \
                             (rb).tsHz++; 

#define ADVANCE_WR(rb) if((rb).samplesWrIdx + 1 == (rb).samplesRdIdx || \
                         ((rb).samplesWrIdx + 1 >= (rb).samplesSz && (rb).samplesRdIdx == 0)) { \
                            (rb).overwrote++; (rb).skipped++; ADVANCE_RDR(rb); } \
                       if(++(rb).samplesWrIdx >= (rb).samplesSz)  \
                            (rb).samplesWrIdx = 0; \
                       if((rb).numSamples <= (rb).samplesSz) \
                            (rb).numSamples++;  \
                       

#if defined(MIXER_DUMP_VAD)

static unsigned int s_vadIdx, s_vad, s_sampleIdx;

static void dump_vad(MIXER_SOURCE_T *pSource) {
  unsigned int idx, vadIdx;
  unsigned int vad_bufsz = pSource->buf.samplesSz / pSource->preproc.chunkHz + 1;

  vadIdx = pSource->buf.samplesRdIdx / pSource->preproc.chunkHz;


  for(idx = 0; idx < vad_bufsz; idx++) {
    fprintf(stderr, "vad[%2d]-> %d  %s", idx, pSource->preproc.vad_buffer[idx], idx % 6 == 5 ? "\n" : "");
  }
  fprintf(stderr, "\nvadIdx:%d\n", vadIdx);

}
#endif // (MIXER_DUMP_VAD)

static void mixchannels_noself(MIXER_T *pMixer,
                               MIXER_SOURCE_T *pSources[], 
                               unsigned int numSources,
                               MIXER_SOURCE_T *pSourceSelf) {
  int vad;
  int vad_tot = 0;
  unsigned int mixedSources = 0;
  unsigned int idxSource, vadIdx;
  float val = 0;
  float gain;

  for(idxSource = 0; idxSource < numSources; idxSource++) {

    if(!pSources[idxSource]->active || 
       (pSources[idxSource] == pSourceSelf && pSourceSelf->includeSelfChannel == 0)) {

      //
      // Set VAD to 0 to prevent source audio from being mixed into the output
      //
      vad = 0;

    } else {

      if(pSources[idxSource]->preproc.vad_buffer) {

        vadIdx = pSources[idxSource]->buf.samplesRdIdx / pSources[idxSource]->preproc.chunkHz;
        vad = pSources[idxSource]->preproc.vad_buffer[vadIdx];

#if defined(MIXER_DUMP_VAD)
//TODO: this doesn't work for more than 2 sources
//fprintf(stderr, "vad:%d, vadIdx[%d] %uHz\n", vad, vadIdx, s_sampleIdx);
if(vad != s_vad) { fprintf(stderr, "mixing source id:%d change in vad[%d]:%d (samplesRdidx:%d/%d chunkHz:%d) %uHz\n", pSources[idxSource]->id, vadIdx, vad, pSources[idxSource]->buf.samplesRdIdx, pSources[idxSource]->buf.samplesSz, pSources[idxSource]->preproc.chunkHz, s_sampleIdx); /*dump_vad(pSources[idxSource]); */ } s_vadIdx = vadIdx; s_vad = vad; 
#endif // MIXER__DUMP_VAD
//s_sampleIdx++; 

      } else {
        vad = 1;
      }
    }

    if(vad > 0) {
      val += (pSources[idxSource]->buf.buffer[pSources[idxSource]->buf.samplesRdIdx] / 32768.0f *
              pSources[idxSource]->gain );
      mixedSources++;
      vad_tot += vad;
    }

  }

  //
  // Determine a reasonable gain multiplier for the output channel
  //
  if(mixedSources > 1) {
    gain = 0.9f;
    val *= gain;
  }

  if(val > 1.0f) {
    val = 1.0f;
  } else if(val < -1.0f) {
    val = -1.0f;
  }

//fprintf(stderr, "vad: %d val:%.3f  %d,   ", vad, val, (int16_t) (val * 32768.0f));

  if(pSourceSelf->pOutput->buf.buffer) {
    pSourceSelf->pOutput->buf.buffer[pSourceSelf->pOutput->buf.samplesWrIdx] = (int16_t) (val * 32768.0f);
    //fprintf(stderr, "SAMPLE[%d] %d, gain:%.3f, vad:%d\n", pSourceSelf->pOutput->buf.samplesWrIdx, (int16_t) (val * 32768.0f), pSources[idxSource]->gain, vad);

    //
    // Set the output channel VAD confidence value
    //
    if(pSourceSelf->pOutput->vad_buffer) {
      unsigned int vadIdx = pSourceSelf->pOutput->buf.samplesWrIdx / pSourceSelf->preproc.chunkHz;

      if(vadIdx != pSourceSelf->pOutput->priorVadIdx) {

        pSourceSelf->pOutput->vad_buffer[vadIdx] = mixedSources > 0 ? (vad_tot / mixedSources) : 0;

        pSourceSelf->pOutput->priorVadIdx = vadIdx;
      }

    }

    ADVANCE_WR(pSourceSelf->pOutput->buf);
  }

}

static int mixchannels(MIXER_T *pMixer,
                       MIXER_SOURCE_T *pSources[], 
                       unsigned int numSources, 
                       unsigned int numSamples,
                       u_int64_t tsHz) {

  unsigned int idxSample, idxSource;

  //for(idxSource = 0; idxSource < numSources; idxSource++) {
    //fprintf(stderr, "mixer_channels startsource[%d]/%d, numSamples:%d/%d, tsHz:%llu, offsetHz:%lld\n", idxSource, numSources, numSamples, pSources[idxSource]->numSamples, pSources[idxSource]->tsHz, pSources[idxSource]->clockOffset);
  //}

//fprintf(stderr, "MIX START rd:%d, wr:%d, numS:%d\n", pSources[0]->output.samplesRdIdx, pSources[0]->output.samplesWrIdx, pSources[0]->output.numSamples);

  for(idxSample = 0; idxSample < numSamples; idxSample++) {

    for(idxSource = 0; idxSource < numSources; idxSource++) {

      if(!pSources[idxSource]->pOutput) {
        continue;
      }

      if(!pSources[idxSource]->pOutput->buf.haveTsHz) {

        //
        // Set tsHz as the time of the first stored sample in the buffer
        //
        pSources[idxSource]->pOutput->buf.tsHz = tsHz;
        pSources[idxSource]->pOutput->buf.haveTsHz = 1;
      }

      mixchannels_noself(pMixer, pSources, numSources, pSources[idxSource]);
    }

    for(idxSource = 0; idxSource < numSources; idxSource++) {
      //
      // Increment the buf.samplesRdIdx
      //
      ADVANCE_RDR(pSources[idxSource]->buf);
    }

  }


  for(idxSource = 0; idxSource < numSources; idxSource++) {
    pSources[idxSource]->buf.numSamples -= MIN(pSources[idxSource]->buf.numSamples, numSamples);

    //fprintf(stderr, "mixer_channels source[%d]/%d, numSamples:%d/%d, tsHz:%lld\n", idxSource, numSources, numSamples, pSources[idxSource]->buf.numSamples,  pSources[idxSource]->tsHz);

  }

  //fprintf(stderr, "MIX END rd:%d, wr:%d, numS:%d\n", pSources[0]->pOutput->buf.samplesRdIdx, pSources[0]->pOutput->buf.samplesWrIdx, pSources[0]->pOutput->buf.numSamples);

  return idxSample;
}

static void mixer_discardSamples(MIXER_T *pMixer,
                                 MIXER_SOURCE_T *pSources[], 
                                 unsigned int numSources,
                                 u_int64_t tsHz) {
  unsigned int idxSource;
  int consume;
  unsigned int numSamples;
  int idx;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  int discarded = 0;
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  for(idxSource = 0; idxSource < numSources; idxSource++) {

    if(pSources[idxSource]->active && !pSources[idxSource]->buf.haveRdr) {

      //fprintf(stderr, "BUF %lluHz now %lluHz\n", pSources[idxSource]->buf.tsHz, tsHz);

      //
      // If this is the first read, clip any samples prior to the current master mixer clock so as to start
      // output as close to real-time as possible
      //
      if(pSources[idxSource]->active &&
         pSources[idxSource]->buf.haveTsHz && 
         pSources[idxSource]->buf.tsHz + pSources[idxSource]->clockOffset < tsHz) {

        consume = tsHz - pSources[idxSource]->buf.tsHz - pSources[idxSource]->clockOffset;
        numSamples = pSources[idxSource]->buf.numSamples; 

        idx = ringbuf_get(&pSources[idxSource]->buf, 
                          NULL, 
                          MIN(consume, pSources[idxSource]->buf.numSamples), 
                          NULL,
                          0);

        LOG(X_WARNING("mixer_discard samples sourceid:%d (source[%d]) discarded %d/%d samples mixer at %lluHz"), pSources[idxSource]->id, idxSource, consume, numSamples, tsHz);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        discarded = 1;
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

        //for(idx = 0; idx < consume && pSources[idxSource]->buf.numSamples > 0; idx++) {
        //  ADVANCE_RDR(pSources[idxSource]->buf);
        //  pSources[idxSource]->buf.numSamples--;
        //}

        if(pSources[idxSource]->buf.numSamples == 0 && idx < consume) {
          pSources[idxSource]->discardedSamplesOffset = (consume - idx);
          pSources[idxSource]->buf.tsHz += (consume - idx);
        }

      }

      pSources[idxSource]->buf.haveRdr = 1;
    }

  }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  if(discarded) {
    mixer_dumpLog(S_DEBUG, pMixer, 0, "after discard");
  }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

}

int mixer_check_idle(MIXER_T *pMixer, 
                     u_int64_t tsHz) {
  //int rc = 0;
  unsigned int idx;
  MIXER_SOURCE_T *pSource;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  int haveIdle = 0;
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  if(!pMixer) {
    return -1;
  }

  pthread_mutex_lock(&pMixer->mtx);

  if(pMixer->idleSourceThresholdHz <= 0) {
    pthread_mutex_unlock(&pMixer->mtx);
    return 0;
  }

  for(idx = 0; idx < (sizeof(pMixer->p_sources) / sizeof(pMixer->p_sources[0])); idx++) {

    if((pSource = pMixer->p_sources[idx]) &&
       pSource->active &&
       pSource->buf.haveTsHz) {

      pthread_mutex_lock(&pSource->buf.mtx);

      //
      // Disable the the source input audio if has been idle > pMixer->idleSourceThresholdHz
      //
      if(pSource->buf.haveTsHz &&
         pSource->lastWrittenTsHz + pMixer->idleSourceThresholdHz < tsHz) {

        LOG(X_DEBUG("mixer setting sourceid:%d inactive after %lldHz > %lldHz idle (lastWrittenTs:%lluHz, tsHz:%lluHz"),
          pSource->id, tsHz - pSource->lastWrittenTsHz, pMixer->idleSourceThresholdHz, pSource->lastWrittenTsHz, tsHz);

        pSource->active = 0;
        pSource->needInputReset = 1;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        haveIdle = 1;
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      }

      pthread_mutex_unlock(&pSource->buf.mtx);

    }

  }

  pthread_mutex_unlock(&pMixer->mtx);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  if(haveIdle) {
    mixer_dumpLog(S_DEBUG, pMixer, 1, "after idle");
  }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  return 0;
}

int mixer_mix(MIXER_T *pMixer, u_int64_t tsHz) {
  int rc = 0;
  unsigned int idx;
  MIXER_SOURCE_T *pSource;
  MIXER_SOURCE_T *pSources[MAX_MIXER_SOURCES];
  unsigned int numSources = 0;
  //unsigned int thresholdHz;
  int availableSamples;
  int64_t fillHz;
  FUNC_ADD_SAMPLES fAddSamples;
  int mixerSamples = -1;
  int mixerSamplesChunk = 0;
  //TIME_STAMP_T tm;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  int filledEmptyFrames[(sizeof(pMixer->p_sources) / sizeof(pMixer->p_sources[0]))];
  int idx2;
  memset(filledEmptyFrames, 0, sizeof(filledEmptyFrames));
  int numSamplesArr[MAX_MIXER_SOURCES];
  memset(numSamplesArr, 0, sizeof(numSamplesArr));
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  if(!pMixer) {
    return -1;
  }

  memset(pSources, 0, sizeof(pSources));

  pthread_mutex_lock(&pMixer->mtx);

  //
  // Get the number of active source audio streams 
  //
  //for(idx = 0; idx < (sizeof(pMixer->p_sources) / sizeof(pMixer->p_sources[0])); idx++) {
  for(idx = 0; idx < MAX_MIXER_SOURCES; idx++) {

    if((pSource = pMixer->p_sources[idx]) && 
       pSource->active && 
       pSource->buf.haveTsHz) {

      pthread_mutex_lock(&pSource->buf.mtx);
      if(pSource->pOutput) {
        pthread_mutex_lock(&pSource->pOutput->buf.mtx);
      }

      pSources[numSources++] = pSource;

    }

  }

  //
  // Discard any source input streams which came before the current clock
  //
  mixer_discardSamples(pMixer, &pSources[0], numSources, tsHz);

  //if(numSources > 0 ) {
    //tsHz -= (pSources[0].clockHz /50);
  //}

  for(idx = 0; idx < numSources; idx++) {

    pSource = pSources[idx];

    //
    // Determine whether to add null filler samples for any audio which is late beyond
    // the mixer thresholdHz  
    //

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  //fprintf(stderr, "---mixer_mix check fill empty thresholdHz:%lld, tsHz:%llu, source[%d].tsHz:%llu, numS:%d, clockOffset:%lldHz\n", pMixer->thresholdHz, tsHz, idx, pSource->buf.tsHz, pSource->buf.numSamples, pSource->clockOffset);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    if(pMixer->thresholdHz > 0 && pSource->active && tsHz > pMixer->thresholdHz && 
       pSource->buf.tsHz + pSource->clockOffset + pSource->buf.numSamples < tsHz - pMixer->thresholdHz) {

      fillHz = tsHz -  pSource->buf.tsHz - pSource->clockOffset - pSource->buf.numSamples, 

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      filledEmptyFrames[idx] = 1;
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

      LOG(X_WARNING("mixer sourceid:%d filling src idx[%d]/%d with %llu empty samples %lluHz (%llu + %u) < %lluHz (threshold: %lluHz)"), 
        pSource->id, idx, numSources,
        fillHz,
        pSource->buf.tsHz + pSource->clockOffset + pSource->buf.numSamples,
        pSource->buf.tsHz + pSource->clockOffset, pSource->buf.numSamples, 
        tsHz, pMixer->thresholdHz);

      if(pSource->isbuffered) {
        fAddSamples = addbuffered;
      } else {
        fAddSamples = addsamples;
      }

      fAddSamples(pSource, 
                 NULL, 
                 fillHz,
                 1, 
                 tsHz,
                 0,
                 NULL,
                 0);


    }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    for(idx2 = 0; idx2 < numSources; idx2++) {
      if(filledEmptyFrames[idx2]) {
        mixer_dumpLog(S_DEBUG, pMixer, 0, "after empty fill");
        break;
      }
    }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    //
    // Determine the number of input samples to be mixed
    //
    if(pSource->active) {
      availableSamples = pSource->buf.numSamples;

      if(mixerSamples == -1) {
        mixerSamples = availableSamples;
      } else {
        mixerSamples = MIN(mixerSamples, availableSamples); 
      }
//#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
//  numSamplesArr[idx] = pSource->active ? availableSamples : -1;
//#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    }

  }

  LOG(X_DEBUGV("mixer_mix active:%d %lluHz, %d / %d samples"), numSources, tsHz, pMixer->outChunkHz > 0 ? (mixerSamples >= pMixer->outChunkHz ? pMixer->outChunkHz : (0)) : mixerSamples, mixerSamples);

  mixerSamplesChunk = mixerSamples;

  //
  // If outChunkHz has been configured ensure the mixer output always produces consistently
  // sized chunks.
  //
  if(pMixer->outChunkHz > 0) {
    if(mixerSamplesChunk >= pMixer->outChunkHz) {
      mixerSamplesChunk = pMixer->outChunkHz;
    } else {
      mixerSamplesChunk = 0;
    }
  }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  numSamplesArr[0] = pSources[0] ? pSources[0]->buf.numSamples : -1;
  numSamplesArr[1] = pSources[1] ? pSources[1]->buf.numSamples : -1;
  numSamplesArr[2] = pSources[2] ? pSources[2]->buf.numSamples : -1;
  numSamplesArr[3] = pSources[3] ? pSources[3]->buf.numSamples : -1;
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  //
  // Mix the input audio channels 
  //
  if(mixerSamplesChunk > 0) {
    rc = mixchannels(pMixer, pSources, numSources, mixerSamplesChunk, tsHz);
  }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  //LOG(X_DEBUG("---mixer_mix active:%d, rc:%d, %lluHz -> %lluHz, mixerSamplesChunk:%d (%d)(outChunkHz:%d)"), numSources, rc, tsHz, tsHz + rc, mixerSamplesChunk, mixerSamples, pMixer->outChunkHz);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  if(rc > 0) {

    pMixer->tsHzPrior = tsHz;
    pMixer->tsHz = tsHz + rc;

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  LOG(X_DEBUG("---mixer_mix output active:%d, rc:%d, %lluHz -> %lluHz, mixerSamplesChunk:%d (%d)(outChunkHz:%d) [0].numS:%d->%d, [1].numS:%d->%d, [2].numS:%d->%d, [3].numS:%d->%d"), numSources, rc, pMixer->tsHzPrior, pMixer->tsHz, mixerSamplesChunk, mixerSamples, pMixer->outChunkHz, numSamplesArr[0], pSources[0] ? pSources[0]->buf.numSamples : -1, numSamplesArr[1], pSources[1] ? pSources[1]->buf.numSamples : -1, numSamplesArr[2], pSources[2] ? pSources[2]->buf.numSamples : -1, numSamplesArr[3], pSources[3] ? pSources[3]->buf.numSamples : -1);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  }

  //fprintf(stderr, "%llu mixer_mix given mixerSamples:%d -> out:%d %lluHz, numSources:%d, mixer output: %lluHz\n", time_getTime()/1000, mixerSamples, rc, tsHz, numSources, pMixer->tsHz);

  //
  // unlock the mixer input streams
  //
  for(idx = 0; idx < numSources; idx++) {

    if(pSources[idx]->pOutput) {
      pthread_mutex_unlock(&pSources[idx]->pOutput->buf.mtx);
    }
    pthread_mutex_unlock(&pSources[idx]->buf.mtx);

  }

  pthread_mutex_unlock(&pMixer->mtx);

  return rc;
}

MIXER_T *mixer_init(LOG_CTXT_T *logCtxt,
                    unsigned int thresholdHz, 
                    unsigned int outChunkHz) {

  MIXER_T *pMixer;

  if(!(pMixer = (MIXER_T *) calloc(1, sizeof(MIXER_T)))) {
    return NULL;
  }

  pMixer->logCtxt = logCtxt;
  pthread_mutex_init(&pMixer->mtx, NULL);
  pMixer->thresholdHz = thresholdHz;
  pMixer->idleSourceThresholdHz = thresholdHz;
  pMixer->outChunkHz = outChunkHz;

  return pMixer;
}

void mixer_free(MIXER_T *pMixer) {

  if(pMixer) {
    pthread_mutex_destroy(&pMixer->mtx);
    free(pMixer);
  }

}

int mixer_addsource(MIXER_T *pMixer, 
                    MIXER_SOURCE_T *pSource) {
  int rc = -1;
  unsigned int idx;

  if(!pMixer || !pSource) {
    return -1;
  }

  pSource->active = 0;

  pthread_mutex_lock(&pMixer->mtx);

  for(idx = 0; idx < MAX_MIXER_SOURCES; idx++) {

    if(!(pMixer->p_sources[idx])) {

      pMixer->p_sources[idx] = pSource;
      pMixer->numSources++;
      //pSource->active = 1;
      rc = 0;
      break;
    }

  }

  pthread_mutex_unlock(&pMixer->mtx);

  return rc;
}

int mixer_removesource(MIXER_T *pMixer, 
                       MIXER_SOURCE_T *pSource) {
  int rc = -1;
  unsigned int idx;

  if(!pMixer || !pSource) {
    return -1;
  }

  pSource->active = 0;

  pthread_mutex_lock(&pMixer->mtx);

  for(idx = 0; idx < MAX_MIXER_SOURCES; idx++) {

    if((pMixer->p_sources[idx]) == pSource) {

      pMixer->p_sources[idx] = NULL;
      if(pMixer->numSources > 0) {
        pMixer->numSources--;
      }
      rc = 0;
      break;
    }

  }

  pthread_mutex_unlock(&pMixer->mtx);

  return rc;
}

int mixer_reset_sources(MIXER_T *pMixer) {
  int rc = 0;
  unsigned int idx;

  pthread_mutex_lock(&pMixer->mtx);

  for(idx = 0; idx < (sizeof(pMixer->p_sources) / sizeof(pMixer->p_sources[0])); idx++) {

    if(pMixer->p_sources[idx] && pMixer->p_sources[idx]->active) {

      pthread_mutex_lock(&pMixer->p_sources[idx]->buf.mtx);
      pMixer->p_sources[idx]->active = 0;
      pthread_mutex_lock(&pMixer->p_sources[idx]->buf.mtx);

      rc++;
    }
  }

  pthread_mutex_unlock(&pMixer->mtx);

  return rc;
}

int mixer_count_active(MIXER_T *pMixer) {
  unsigned int idx;
  unsigned int numSources = 0;

  if(!pMixer) {
    return 0;
  }

  pthread_mutex_lock(&pMixer->mtx);

  //numSources = pMixer->numSources;

  for(idx = 0; idx < (sizeof(pMixer->p_sources) / sizeof(pMixer->p_sources[0])); idx++) {

    if(pMixer->p_sources[idx] && pMixer->p_sources[idx]->active) {
      numSources++;
    }

  }

  pthread_mutex_unlock(&pMixer->mtx);

  return (int) numSources;
}

static int mixer_dump2buf(char *buf, unsigned int sz, MIXER_T *pMixer, int lock, const char *descr) {
  unsigned int numSources = 0;
  unsigned int idx;
  int rc;
  unsigned int idxbuf = 0;
  int64_t tsDeltaHz;
  MIXER_SOURCE_T *pSource;
  MIXER_SOURCE_T *pSources[MAX_MIXER_SOURCES];

  if(!buf || !pMixer) {
    return -1;
  }

  if((rc = snprintf(&buf[idxbuf], sz - idxbuf, "========= Begin dumping mixer state %s =========\n", 
                    descr ? descr : "")) > 0) {
    idxbuf += rc;
  }

  if(lock) {
    pthread_mutex_lock(&pMixer->mtx);
  }

  if((rc = snprintf(&buf[idxbuf], sz - idxbuf, "mixer tsHz:%lluHz, late threshold:%lluHz, idle threshold:%lluHz\n", pMixer->tsHz, pMixer->thresholdHz, pMixer->idleSourceThresholdHz)) > 0) {
    idxbuf += rc;
  }

  for(idx = 0; idx < (sizeof(pMixer->p_sources) / sizeof(pMixer->p_sources[0])); idx++) {

    if((pSource = pMixer->p_sources[idx])) {

      pSources[numSources++] = pSource;
      if(lock) {
        pthread_mutex_lock(&pSource->buf.mtx);
      }

      tsDeltaHz = pSource->buf.tsHz - pMixer->tsHz;

      if((rc = snprintf(&buf[idxbuf], sz - idxbuf, 
          "source[%d].id:%d, active:%d rd[%d] wr[%d]/%d, haveTsHz:%d, %lluHz (%s%lldHz), numS:%d, overwrote:%d, skipped:%d\n", 
        idx, pSource->id, pSource->active, pSource->buf.samplesRdIdx, pSource->buf.samplesWrIdx, 
        pSource->buf.samplesSz, pSource->buf.haveTsHz, pSource->buf.tsHz, tsDeltaHz > 0 ? "+" : "", 
        tsDeltaHz, pSource->buf.numSamples, pSource->buf.overwrote, pSource->buf.skipped)) > 0) {
        idxbuf += rc;
      }

    }

  }

  for(idx = 0; idx < numSources; idx++) {

    pSource = pSources[idx];

    if((rc = snprintf(&buf[idxbuf], sz - idxbuf, 
        "output[%d] rd[%d] wr[%d]/%d, %lluHz, numS:%d, overwrote:%d\n", 
       idx, pSource->pOutput->buf.samplesRdIdx, pSource->pOutput->buf.samplesWrIdx, 
       pSource->pOutput->buf.samplesSz, pSource->pOutput->buf.tsHz, pSource->pOutput->buf.numSamples, 
       pSource->pOutput->buf.overwrote)) > 0) {
      idxbuf += rc;
    }

    if(lock) {
      pthread_mutex_unlock(&pSource->buf.mtx);
    }

  }

  if(lock) {
    pthread_mutex_unlock(&pMixer->mtx);
  }

  if((rc = snprintf(&buf[idxbuf], sz - idxbuf, "========= End dumping mixer state =========\n")) > 0) {
    idxbuf += rc;
  }

  return idxbuf;
}

void mixer_dump(FILE *fp, MIXER_T *pMixer, int lock, const char *descr) {
  int rc;
  char buf[4096];

  if((rc = mixer_dump2buf(buf, sizeof(buf), pMixer, lock, descr)) > 0)  {
    fprintf(fp, "%s", buf); 
  }

}

void mixer_dumpLog(int logLevel, MIXER_T *pMixer, int lock, const char *descr) {
  int rc;
  char buf[4096];

  if((rc = mixer_dump2buf(buf, sizeof(buf), pMixer, lock, descr)) > 0)  {
    LOG(logLevel, "%s\n", buf);
  }

}
