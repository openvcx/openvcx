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


#ifndef __FRAME_Q_H__
#define __FRAME_Q_H__

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

#include <pthread.h>
#include "mixer/util.h"


typedef struct FRAME_NODE {
  void                    *pData;     // data buffer
  unsigned int             lenData;   // length of data
  unsigned int             lenAlloc;  // allocation size of pData
  unsigned int             sequence;  // stream sequence # (from RTP)
  unsigned int             ts;        // stream timestamp (from RTP)
  TIME_STAMP_T             captureTm; // capture timestamp 
  struct FRAME_NODE       *pnext;
} FRAME_NODE_T;

typedef struct FRAME_Q {
  FRAME_NODE_T            *pHead;
  FRAME_NODE_T            *pUsed;
  pthread_mutex_t          mtx;
  pthread_mutex_t          mtxCond;
  pthread_cond_t           cond;
  unsigned int             size;
  unsigned int             max;
} FRAME_Q_T;

FRAME_Q_T *frameq_create(unsigned int max);
void frameq_destroy(FRAME_Q_T *pQ);
int frameq_push(FRAME_Q_T *pQ, 
                const void *pData, 
                unsigned int lenData, 
                unsigned int sequence,
                unsigned int ts,
                TIME_STAMP_T captureTm);
FRAME_NODE_T *frameq_pop(FRAME_Q_T *pQ);
FRAME_NODE_T *frameq_popWait(FRAME_Q_T *pQ);
int frameq_recycle(FRAME_Q_T *pQ, FRAME_NODE_T *pNode);
void frameq_freeNode(FRAME_NODE_T *pNode);
int frameq_size(FRAME_Q_T *pQ);

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

#endif // __FRAME_Q_H__
