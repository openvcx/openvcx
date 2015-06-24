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
#include "mixer/ringbuf.h"

#define LOG_CTXT  (pBuf ? pBuf->logCtxt : NULL)

int ringbuf_init(LOG_CTXT_T *logCtxt,
                 RING_BUF_T *pBuf, 
                 unsigned int sz) {

  if(!pBuf) {
    return -1;
  }

  pBuf->logCtxt = logCtxt;
  pthread_mutex_init(&pBuf->mtx, NULL);

  pBuf->samplesSz = sz;

  if(pBuf->samplesSz > 0 && !(pBuf->buffer = (int16_t *) calloc(sizeof(int16_t), pBuf->samplesSz))) {
    return -1;
  }

  ringbuf_reset(pBuf, 0);

  return 0;
}

void ringbuf_free(RING_BUF_T *pBuf) {

  pthread_mutex_destroy(&pBuf->mtx);

  pBuf->numSamples = 0;
  pBuf->samplesSz = 0;

  if(pBuf->buffer) {
    free(pBuf->buffer);
    pBuf->buffer = NULL;
  }

}

void ringbuf_reset(RING_BUF_T *pBuf, int lock) {

  if(lock) {
    pthread_mutex_lock(&pBuf->mtx);
  }

  pBuf->skipped += pBuf->numSamples;
  pBuf->numSamples = 0;
  pBuf->samplesWrIdx = 0;
  pBuf->samplesRdIdx = 0;
  pBuf->tsHz = 0;
  pBuf->haveTsHz = 0;
  pBuf->haveRdr = 0;
  pBuf->overwrote = 0;

  if(lock) {
    pthread_mutex_unlock(&pBuf->mtx);
  }

}

int ringbuf_add(RING_BUF_T *pBuf, const int16_t *pSamples, unsigned int numSamples) {
  unsigned int idx = 0;
  int overwrote = 0;
  unsigned int copy;

  //
  // Copy the input samples into the ring buffer
  //
  while(idx < numSamples) {

    copy = MIN((numSamples - idx), (pBuf->samplesSz - pBuf->samplesWrIdx));

    //fprintf(stderr, "copy size:%d  idx:%d, sz:%d\n", copy, idx, numSamples);

    if(pBuf->numSamples > 0 && pBuf->samplesRdIdx >= pBuf->samplesWrIdx &&
       pBuf->samplesRdIdx < pBuf->samplesWrIdx + copy) {

      overwrote += pBuf->samplesWrIdx + copy - pBuf->samplesRdIdx;
      //fprintf(stderr, "increment overwrote to %d by %d\n", overwrote, pBuf->samplesWrIdx + copy - pBuf->samplesRdIdx);
    }

    if(pBuf->buffer) {

      if(pSamples) {

        memcpy(&pBuf->buffer[pBuf->samplesWrIdx], &pSamples[idx], copy * sizeof(int16_t));

      } else {

        //
        // Fill with blank samples
        //
        memset(&pBuf->buffer[pBuf->samplesWrIdx], 0x00, copy * sizeof(int16_t));

      }

    }

    idx += copy;

    if((pBuf->numSamples += copy) > pBuf->samplesSz) {
      pBuf->numSamples = pBuf->samplesSz;
    }

    if((pBuf->samplesWrIdx += copy) >= pBuf->samplesSz) {
      pBuf->samplesWrIdx = 0;
    }

    //fprintf(stderr, "tid:0x%x ringbuf_add now idx:[%d]/%d wridx:[%d]/%d, rdidx:[%d], numSamples:%d\n", pthread_self(), idx, numSamples, pBuf->samplesWrIdx, pBuf->samplesSz, pBuf->samplesRdIdx, pBuf->numSamples);

  }

  if(overwrote > 0) {
    if((pBuf->samplesRdIdx = pBuf->samplesWrIdx) >= pBuf->samplesSz) {
      pBuf->samplesRdIdx = 0;
    }
    pBuf->overwrote += overwrote;
    pBuf->skipped += overwrote;
  }

  return overwrote;
}

int ringbuf_get(RING_BUF_T *pBuf, int16_t *pSamples, unsigned int numSamples, u_int64_t *pTsHz, int lock) {
  unsigned int idx = 0;
  unsigned int copy;

  if(!pBuf) {
    return -1; 
  }

  //fprintf(stderr, "ringbuf_get %d/%d numS:%d rd:%d wr:%d\n", idx, numSamples, pBuf->numSamples, pBuf->samplesRdIdx, pBuf->samplesWrIdx);

  if(lock) {
    pthread_mutex_lock(&pBuf->mtx);
  }

  if(!pBuf->buffer || pBuf->numSamples < numSamples) {
    if(lock) {
      pthread_mutex_unlock(&pBuf->mtx);
    }
    return 0;
  }

  if(pTsHz) {
    *pTsHz = pBuf->tsHz;
  }

  while(idx < numSamples) {

    copy = MIN((numSamples - idx), (pBuf->samplesSz - pBuf->samplesRdIdx));

    if(copy <= 0) {
      break;
    }

    if(pSamples) {
      memcpy(&pSamples[idx], &pBuf->buffer[pBuf->samplesRdIdx], copy * sizeof(int16_t));
    }

    if((pBuf->samplesRdIdx += copy) >= pBuf->samplesSz) {
      pBuf->samplesRdIdx = 0;
    }
    pBuf->numSamples -= copy;
    pBuf->tsHz += copy;

    idx +=copy;
  }

  if(lock) {
    pthread_mutex_unlock(&pBuf->mtx);
  }

  return idx;
}

