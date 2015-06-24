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

#define LOG_CTXT  pSource->logCtxt

//#define DEBUG_MIXER_TIMING 1

static void source_reset(MIXER_SOURCE_T *pSource, int lock) {

  LOG(X_DEBUG("mixer source reset sourceid:%d, lock:%d"), pSource->id, lock);

  pSource->discardedSamplesOffset = 0;

  ringbuf_reset(&pSource->buf, lock);

  if(pSource->pOutput) {

    ringbuf_reset(&pSource->pOutput->buf, 1);

    //
    // Reset the vad output buffer
    //
    if(pSource->pOutput->vad_buffer) {
      pSource->pOutput->priorVadIdx = -1;
      memset(pSource->pOutput->vad_buffer, 0,  pSource->pOutput->vad_bufsz);
    }
  }

}

//
// addsamples needs to maintain the same function signature as addbuffered
//
int addsamples(MIXER_SOURCE_T *pSource, 
                      const int16_t *pSamples, 
                      unsigned int numSamples,
                      unsigned int channels, 
                      u_int64_t tsHz,
                      int vad,
                      int *pvad,
                      int lock) {


  int rc = 0;
  unsigned int idx = 0;
  int was_set;
  int overwrote = 0;

  if(numSamples <= 0) {
    return 0;
  }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  if(pSource->id==1) 
    LOG(X_DEBUG("---mixer addsamples sourceid:%d numSamples:%d (in src buffer:%d) tsHz:%lldHz (clockoffset:%lldHz, discardoffset:%lldHz) (%.3fs), buf.tsHz:%llu (haveTsHz:%d, active:%d)"), pSource->id, numSamples, pSource->buf.numSamples, tsHz, pSource->clockOffset, pSource->discardedSamplesOffset, (double)(tsHz + pSource->clockOffset)/pSource->clockHz,  pSource->buf.tsHz, pSource->buf.haveTsHz, pSource->active);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  if(lock) {
    pthread_mutex_lock(&pSource->buf.mtx);
  }

  if(!pSource->active) {

    was_set = pSource->buf.haveTsHz; 

    // pSource->buf.haveTsHz will be set to 0 here
    source_reset(pSource, !lock);
    pSource->active = 1;

    if(was_set) {
      LOG(X_DEBUG("mixer setting sourceid:%d active tsHz:%lluHz)"), pSource->id, !pSource->buf.haveTsHz ? tsHz : pSource->buf.tsHz);
    }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
     LOG(X_DEBUG("source_reset done, haveTsHz:%d buf.tsHz:%lluHz, tsHz:%lluHz, source.numS:%d"), pSource->buf.haveTsHz, pSource->buf.tsHz, tsHz, pSource->buf.numSamples); mixer_dumpLog(S_DEBUG, pSource->pMixer, !lock, "src reset"); 
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  }

  if(!pSource->buf.haveTsHz) {

    //
    // Set tsHz as the time of the first stored sample in the buffer
    //
    pSource->buf.tsHz = tsHz;
    pSource->buf.haveTsHz = 1;

  } else if(tsHz < pSource->buf.tsHz) {

    //
    // We likely previously skipped these input samples as they were late arriving and have already
    // been replaced with null noise
    //
    idx += (pSource->buf.tsHz - tsHz);
    LOG(X_WARNING("mixer_addsamples sourceid:%d clipping first %d (max:%d) samples %lld < %lld"), 
             pSource->id, idx, numSamples, tsHz, pSource->buf.tsHz);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    mixer_dumpLog(S_DEBUG, pSource->pMixer, !lock, "addsamples clipping");
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  }
 
  //
  // Copy the input samples into the ring buffer
  //
  if(numSamples > idx) {

    rc = numSamples - idx;

    if((overwrote = ringbuf_add(&pSource->buf, &pSamples[idx], numSamples - idx)) > 0) {

      pSource->buf.tsHz += overwrote;
      LOG(X_WARNING("mixer_addsamples sourceid:%d overwrote %d, rdidx:%d, wridx:%d / %d.  Will reset source."), 
            pSource->id, overwrote, pSource->buf.samplesRdIdx, pSource->buf.samplesWrIdx, pSource->buf.samplesSz);

      pSource->active = 0;
      pSource->needInputReset = 1;
    }
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  if(pSource->id==1) 
    LOG(X_DEBUG("---mixer ringbuf_add was called with %d - %d samples.  buf.tsHz:%lluHz (tsHz:%lluHz), buf.numSamples:%d"), numSamples, idx, pSource->buf.tsHz, tsHz, pSource->buf.numSamples);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  }

  if(lock) {
    pthread_mutex_unlock(&pSource->buf.mtx);
  }

  return rc;
}

int addbuffered(MIXER_SOURCE_T *pSource, 
                       const int16_t *pSamples, 
                       unsigned int numSamples,
                       unsigned int channels, 
                       u_int64_t tsHz,
                       int vad,
                       int *pvad,
                       int lock) {
  int rc = 0;
  int rc_vad = 0;
  //int vadTot = 0, vadCnt = 0;
  unsigned int vadIdx;
  unsigned int idx = 0;
  unsigned int copy;
  AUDIO_PREPROC_T *pPreproc = &pSource->preproc;

  if(!pPreproc->active) {

    rc = addsamples(pSource, pSamples, numSamples, channels, tsHz, vad, NULL, lock);

    return rc;
  }

  LOG(X_DEBUGV("addbuffered sourceid:%d, active:%d %lluHz (lastWr:%lluHz), samples:%d, buffer starts at %lluHz, previously had %u samples"),pSource->id, pSource->active, tsHz, pSource->lastWrittenTsHz, numSamples, pSource->buf.tsHz, pSource->buf.numSamples);

  if(pSource->active && pSource->buf.haveTsHz && tsHz < pSource->buf.tsHz) {
    //
    // We likely previously skipped these input samples as they were late arriving and have already
    // been replaced with null noise
    //
    idx += (pSource->buf.tsHz - tsHz);
    LOG(X_WARNING("mixer_addbuffered samples sourceid:%d clipping first %d (max:%d) samples %lldHz < %lldHz, "
                "has %d samples"),
                pSource->id, idx, numSamples, tsHz, pSource->buf.tsHz, pSource->buf.numSamples);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    mixer_dumpLog(S_DEBUG, pSource->pMixer, !lock, "addbuffered clipping");
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

/*
    if(pSource->active && idx >= numSamples && tsHz < pSource->buf.tsHz && 
       (pSource->buf.tsHz - tsHz) > pSource->thresholdHz) {
      pSource->active = 0;
      pSource->needInputReset = 1;
      source_reset(pSource, !lock);
//#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    fprintf(stderr, "addbuffered resetting source id:%d, haveTsHz:%d, tsHz:%lluHz, buf.numSamples:%d, tsHz:%lluHz, idx:%d, active:%d\n", pSource->id, pSource->buf.haveTsHz, pSource->buf.tsHz, pSource->buf.numSamples, tsHz - idx, idx, pSource->active);
    
//#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      return 0;
    }
*/

    tsHz += idx;

  }

  if(!pSource->active && pPreproc->bufferIdx > 0) {
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    LOG(X_DEBUG("---mixer preproc setting bufferIdx=0 because source id:%d is not active"), pSource->id);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    pPreproc->bufferIdx = 0;
  }

  //
  // Run the input samples through the preprocessor
  //
  while(idx < numSamples) {

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    if(pSource->id==1) 
      LOG(X_DEBUG("---mixer preproc source id:%d, %d/%d, pPreproc->tsHz:%lluHz, tsHz:%lluHz (discarded offset:%lldHz), pPreproc->bufferIdx:%d, source->active:%d"), pSource->id, idx, numSamples, pPreproc->tsHz, tsHz, pSource->discardedSamplesOffset, pPreproc->bufferIdx, pSource->active);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    if(pPreproc->bufferIdx == 0) {
      pPreproc->tsHz = tsHz;
    }

    copy = MIN(pPreproc->chunkHz - pPreproc->bufferIdx, numSamples - idx);

//fprintf(stderr, "addbuffered sourceid:%dcopy:%d -> [%d]/%d, pPreproc->tsHz:%llu, tsHz:%llu\n", pSource->id, copy, pPreproc->bufferIdx, pPreproc->chunkHz, pPreproc->tsHz, tsHz);


    if(!pSamples) {
      //
      // Fill with blank samples
      //
      memset(&pPreproc->buffer[pPreproc->bufferIdx], 0x00, copy * sizeof(int16_t));
    } else if(pSamples != pSource->buf.buffer) {
      memcpy(&pPreproc->buffer[pPreproc->bufferIdx], &pSamples[idx], copy * sizeof(int16_t));
    }

    pPreproc->bufferIdx += copy;
    idx += copy; 
    tsHz += copy;

    if(pPreproc->bufferIdx >= pPreproc->chunkHz) {

      if(pPreproc->cfg_vad || pPreproc->cfg_denoise || pPreproc->cfg_agc) {

        //  
        // Run the audio preprocessor on the entire chunk of samples
        //
        if((rc_vad = audio_preproc_exec(pPreproc,  pPreproc->buffer, pPreproc->bufferIdx)) < 0) {
          rc_vad = 0;
        }
        //rc_vad = random()%2;

        if(pPreproc->cfg_vad_smooth) {
          //int rc_vad_orig = rc_vad;
#define VAD_SMOOTHING_RAISE_FACTOR 4.0f
#define VAD_SMOOTHING_LOWER_FACTOR 2.5f
#define VAD_SMOOTHING_THRESHOLD  0.65f
          pPreproc->vad_smooth_avg += ((float)rc_vad - pPreproc->vad_smooth_avg) / 
                         (rc_vad > 0 ? VAD_SMOOTHING_RAISE_FACTOR : VAD_SMOOTHING_LOWER_FACTOR);
          if(pPreproc->vad_smooth_avg >= VAD_SMOOTHING_THRESHOLD) {
            rc_vad = 1;
          } else {
            rc_vad = 0;
          }
        //if(((int) pthread_self()& 0xffff) != 0x5960) fprintf(stderr, "tid:0x%x rc_vad_orig:%d (smoothed:%d) vad_smooth_avg:%.3f\n", pthread_self(), rc_vad_orig, rc_vad, pPreproc->vad_smooth_avg);
        } 

      } 

      if(!pPreproc->cfg_vad) {
        //
        // The VAD value comes form the input, as it may have already been computed at
        // an up-stream source
        //
        rc_vad = vad ? 1 : 0;
      }

      //vadTot += pSamples ? rc_vad : 0;
      //vadCnt++;

      //
      // Store the returned VAD confidence value
      //
      if(pPreproc->vad_buffer) {


        //
        // Algorithm to persist VAD flag up to cfg_vad_persist samples after the last speex vad 
        // non-zero value was obtained
        //
        if(rc_vad > 0) {
          pPreproc->vad_persist = pPreproc->cfg_vad_persist;
        } else if(pPreproc->vad_persist > 0) {
          pPreproc->vad_persist--;
          rc_vad = 2;
        }

        vadIdx = pSource->buf.samplesWrIdx / pPreproc->chunkHz;
        pPreproc->vad_buffer[vadIdx] = rc_vad;
      }
      
      //fprintf(stderr, "audio_preproc_exec: vadIdx[%d], vad:%d (persist:%d/%d), tsHz:%llu\n", vadIdx, rc_vad, pPreproc->vad_persist, pPreproc->cfg_vad_persist, pPreproc->tsHz);

     //TOD: after reset pPreproc->tsHz corresponds to old buf.tsHz, should take effect of new input param tsHz

      rc = addsamples(pSource, pPreproc->buffer, pPreproc->bufferIdx, channels, pPreproc->tsHz, vad, NULL, lock);

      pPreproc->bufferIdx = 0;

      if(rc < 0 || !pSource->active) {
        break;
      }

    }

  }

  if(pvad) {
    //*pvad = vadCnt > 0 ? vadTot / vadCnt : 0;
    *pvad = rc_vad ? 1 : 0;
  }

  return rc;
}

int mixer_addbuffered(MIXER_SOURCE_T *pSource, 
                       const int16_t *pSamples, 
                       unsigned int numSamples,
                       unsigned int channels, 
                       u_int64_t tsHz,
                       int vad,
                       int *pvad) {
  int rc = 0;

  if(!pSource || channels > 1) {
    return -1;
  } else if(numSamples <= 0) {
    return 0; 
  } else if(!pSource->buf.buffer) {

    //
    // We don't ingest any input data, may only opt to receive mixed output data
    //
    return 0; 

  } else if(!pSource->preproc.active) {
    //fprintf(stderr, "mixer_addbuffered preprec inactive\n");
    return mixer_add(pSource, pSamples, numSamples, channels, tsHz);
  }

  //fprintf(stderr, "mixer_addbuffered preprec active\n");

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  if(pSource->id==1) 
    LOG(X_DEBUG("---mixer_addbuffered source id:%d, numSamples:%d, tsHz:%lluHz, active:%d, haveTsHz:%d, buf.tsHz:%lluHz, buf.numS:%d, mixer tsHz:%lluHz, preproc.tsHz:%lluHz"), pSource->id, numSamples, tsHz, pSource->active, pSource->buf.haveTsHz, pSource->buf.tsHz, pSource->buf.numSamples, pSource->pMixer->tsHz, pSource->preproc.tsHz);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  pSource->isbuffered = 1;

  if(pSource->active && pSource->discardedSamplesOffset) {
    tsHz += pSource->discardedSamplesOffset;
  }

  pSource->lastWrittenTsHz = tsHz + numSamples;

  rc = addbuffered(pSource, pSamples, numSamples, channels, tsHz, vad, pvad, 1);

  return rc;

}

int mixer_add(MIXER_SOURCE_T *pSource, 
              const int16_t *pSamples, 
              unsigned int numSamples,
              unsigned int channels, 
              u_int64_t tsHz) {
  int rc;

  if(!pSource || channels > 1) {
    return -1;
  } else if(numSamples <= 0) {
    return 0; 
  }

  pSource->isbuffered = 0;

  pSource->lastWrittenTsHz = tsHz + numSamples;

  rc = addsamples(pSource, pSamples, numSamples, channels, tsHz, 0, NULL, 1);

  return rc;
}

static int s_mixer_source_id = 0;
static pthread_mutex_t s_mixer_mtx = PTHREAD_MUTEX_INITIALIZER;

MIXER_SOURCE_T *mixer_source_init(LOG_CTXT_T *logCtxt,
                                  float durationSec,
                                  float gain,
                                  unsigned int sampleRateHz,
                                  int64_t thresholdHz,
                                  int vad,
                                  int vad_mode,
                                  int denoise,
                                  int agc,
                                  int haveOutput,
                                  int includeSelfChannel,
                                  //int setOutputVad,
                                  int acceptInputVad) {

  MIXER_SOURCE_T *pSource;

  if(!haveOutput && durationSec <= 0.0f) {
    //
    // The mixer source has no input and output properties set
    //
    return NULL;
  } if(!(pSource = (MIXER_SOURCE_T *) calloc(1, sizeof(MIXER_SOURCE_T)))) {
    return NULL;
  }

  pSource->logCtxt = logCtxt;
  pSource->gain = gain;
  pSource->clockHz = sampleRateHz;
  pSource->thresholdHz = thresholdHz;
  pSource->includeSelfChannel = includeSelfChannel;
  //pSource->setOutputVad = setOutputVad;
  pSource->acceptInputVad = acceptInputVad;

  //
  // durationSec can be 0 to indicate NULL input source, which may only want audio output
  //
  if(ringbuf_init(logCtxt, &pSource->buf, pSource->clockHz * durationSec) < 0) {
    mixer_source_free(pSource);
    return NULL;
  }

  //
  // Always set the desired chunk size in Hz
  // since it's currently being referenced outside of the preproc context... no no
  //
  pSource->preproc.chunkHz = pSource->clockHz * MIXER_PREPROC_CHUNKSZ_MS / 1000;   // 20 ms chunks

  if(pSource->buf.buffer && (vad || denoise || agc || acceptInputVad)) {

    pSource->preproc.cfg_vad_mode = vad_mode;

    if(audio_preproc_init(&pSource->preproc, 
                          pSource->clockHz, 
                          pSource->buf.samplesSz,
                          vad,
                          denoise,
                          agc) < 0) {
      mixer_source_free(pSource);
      return NULL; 
    }
  }

  if(haveOutput) {
    if(!(pSource->pOutput = (MIXER_OUTPUT_T *) calloc(1, sizeof(MIXER_OUTPUT_T))) ||
      (ringbuf_init(logCtxt, &pSource->pOutput->buf, pSource->clockHz) < 0)) {
      mixer_source_free(pSource);
      return NULL;
    }
    if(vad) {
      pSource->pOutput->vad_bufsz = pSource->buf.samplesSz / pSource->preproc.chunkHz + 1;
      pSource->pOutput->vad_buffer = (unsigned char *) calloc(1, pSource->pOutput->vad_bufsz);
      pSource->pOutput->priorVadIdx = -1;
    }
  }

  pthread_mutex_lock(&s_mixer_mtx);
  pSource->id = ++s_mixer_source_id;
  pthread_mutex_unlock(&s_mixer_mtx);

  return pSource;
}

void mixer_source_free(MIXER_SOURCE_T *pSource) {

  if(!pSource) {
    return;
  }

  pSource->active = 0;

  pthread_mutex_lock(&s_mixer_mtx);
  if(pSource->id > 0 && s_mixer_source_id > 0) {
    s_mixer_source_id--;
  }
  pthread_mutex_unlock(&s_mixer_mtx);

  audio_preproc_free(&pSource->preproc);

  ringbuf_free(&pSource->buf);
  if(pSource->pOutput) {
    ringbuf_free(&pSource->pOutput->buf);

    if(pSource->pOutput->vad_buffer) {
      free(pSource->pOutput->vad_buffer);
      pSource->pOutput->vad_buffer = NULL;
    }

    free(pSource->pOutput);
    pSource->pOutput = NULL;
  }

  free(pSource);
}

