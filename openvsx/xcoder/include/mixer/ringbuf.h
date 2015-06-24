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

#ifndef __RING_BUF_H__
#define __RING_BUF_H__

#include "mixer/util.h"
#include "logutil.h"


typedef struct RING_BUF {
  LOG_CTXT_T                  *logCtxt;
  unsigned int                 samplesSz;    // int16_t based size of buffer
  unsigned int                 numSamples;   // buffer fullness in terms of samples
  unsigned int                 samplesWrIdx; // int16_t based index within buffer
  unsigned int                 samplesRdIdx; // int16_t based index within buffer
  u_int64_t                    tsHz;         // timestamp of first sample
  int                          haveTsHz;
  int                          haveRdr;
  pthread_mutex_t              mtx;
  int16_t                     *buffer;       // ring buffer for storing PCM samples
  int                          overwrote;
  unsigned int                 skipped;
} RING_BUF_T;

int ringbuf_init(LOG_CTXT_T *logCtxt,
                 RING_BUF_T *pBuf, 
                 unsigned int sz);

void ringbuf_free(RING_BUF_T *pBuf);

void ringbuf_reset(RING_BUF_T *pBuf,
                   int lock);

int ringbuf_add(RING_BUF_T *pBuf, 
                const int16_t *pSamples, 
                unsigned int numSamples);

int ringbuf_get(RING_BUF_T *pBuf, 
                int16_t *pSamples, 
                unsigned int numSamples, 
                u_int64_t *pTsHz,
                int lock);

#endif // __RING_BUF_H__
