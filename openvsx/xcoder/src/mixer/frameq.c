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
#include "logutil.h"
#include "mixer/frameq.h"


#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

static void frameq_destroyNode(FRAME_NODE_T **ppNode) {

  if((*ppNode)->pData) {
    free((*ppNode)->pData);
    (*ppNode)->pData = NULL;
  }
  (*ppNode)->lenData = 0;
  (*ppNode)->lenAlloc = 0;

  free(*ppNode);
  *ppNode = NULL;

}

void frameq_freeNode(FRAME_NODE_T *pNode) {
  if(pNode) {
    frameq_destroyNode(&pNode);
  }
}

static int frameq_attach(FRAME_Q_T *pQ, FRAME_NODE_T *pNode) {
  FRAME_NODE_T *pTmp;

  if(!pNode) {
    return - 1;
  }

  pTmp = pQ->pHead;

  pNode->pnext = NULL;

  if(!pTmp) {
    pQ->pHead = pNode;
  } else {
    while(pTmp->pnext) {
      pTmp = pTmp->pnext;
    }

    pTmp->pnext = pNode;
    
  }

  pQ->size++;

  return 0;
}

#define ALLOC_CHUNK 64

static FRAME_NODE_T *frameq_createNode(FRAME_NODE_T *pNode, 
                                       const void *pData, 
                                       unsigned int lenData,
                                       unsigned int sequence,
                                       unsigned int ts,
                                       TIME_STAMP_T captureTm) {

  if(!pNode && !(pNode = (FRAME_NODE_T *) calloc(1, sizeof(FRAME_NODE_T)))) {
    return NULL; 
  }

  if(pNode->lenAlloc < lenData) {

    pNode->lenAlloc = (lenData / ALLOC_CHUNK + 1) * (ALLOC_CHUNK);

    if(pNode->pData) {
      free(pNode->pData);
    }

    if(!(pNode->pData = malloc(pNode->lenAlloc))) {
      free(pNode);
      return NULL;
    }

  }

  memcpy(pNode->pData, pData, lenData);
  pNode->lenData = lenData;
  pNode->sequence = sequence;
  pNode->ts = ts;
  pNode->captureTm = captureTm;

  return pNode;
}

static void frameq_removeAll(FRAME_Q_T *pQ, FRAME_NODE_T **ppHead) {
  FRAME_NODE_T *pNode;

  while((pNode = *ppHead)) {
    *ppHead = (*ppHead)->pnext;

    frameq_destroyNode(&pNode);

  }

}

FRAME_Q_T *frameq_create(unsigned int max) {
  FRAME_Q_T *pQ;

  if(!(pQ = (FRAME_Q_T *) calloc(1, sizeof(FRAME_Q_T)))) {
    return NULL;
  }

  pthread_mutex_init(&pQ->mtx, NULL);
  pthread_mutex_init(&pQ->mtxCond, NULL);
  pthread_cond_init(&pQ->cond, NULL);

  pQ->max = max;

  return pQ;
}

void frameq_destroy(FRAME_Q_T *pQ) {

  if(pQ) {

    pthread_mutex_lock(&pQ->mtx);
    frameq_removeAll(pQ, &pQ->pHead);
    frameq_removeAll(pQ, &pQ->pUsed);
    pthread_mutex_unlock(&pQ->mtx);

    pthread_cond_destroy(&pQ->cond);
    pthread_mutex_destroy(&pQ->mtxCond);
    pthread_mutex_destroy(&pQ->mtx);
    free(pQ);
  }

}

int frameq_push(FRAME_Q_T *pQ, 
                const void *pData, 
                unsigned int lenData,
                unsigned int sequence,
                unsigned int ts,
                TIME_STAMP_T captureTm) {
  int rc = 0;
  FRAME_NODE_T *pNode, *pTmp;

  pthread_mutex_lock(&pQ->mtx);

  if(pQ->max > 0 && pQ->size >= pQ->max) {
    pthread_mutex_unlock(&pQ->mtx);
    return -1;
  }

  pTmp = NULL;
  pNode = pQ->pUsed;
  while(pNode) {
    if(pNode->lenAlloc >= lenData) {
      if(pTmp) {
        pTmp->pnext = pNode->pnext;
      } else {
        pQ->pUsed = pNode->pnext;
      }
      break;
    }
    pTmp = pNode;
    pNode = pNode->pnext;
  }

  if(!pNode && pQ->pUsed) {
    pNode = pQ->pUsed; 
    pQ->pUsed = pQ->pUsed->pnext;
  }

  if(!(pTmp = frameq_createNode(pNode, pData, lenData, sequence, ts, captureTm))) {
    frameq_destroyNode(&pTmp);
  }

  rc = frameq_attach(pQ, pTmp);

  pthread_mutex_unlock(&pQ->mtx);

  //
  // wake up any listeners
  //
  mixer_cond_broadcast(&pQ->cond, &pQ->mtxCond);

  return rc;
}

FRAME_NODE_T *frameq_pop(FRAME_Q_T *pQ) {
  FRAME_NODE_T *pNode;

  if(!pQ) {
    return NULL;
  }

  pthread_mutex_lock(&pQ->mtx);

  if((pNode = pQ->pHead)) {
    pQ->pHead = pNode->pnext;
    pNode->pnext = NULL;
    if(pQ->size > 0) {
      pQ->size--;
    }
  }

  pthread_mutex_unlock(&pQ->mtx);

  return pNode;
}

FRAME_NODE_T *frameq_popWait(FRAME_Q_T *pQ) {
  FRAME_NODE_T *pNode = NULL;
  int index = 0;
  int rc;

  if(!pQ) {
    return NULL;
  }

  while(!(pNode = frameq_pop(pQ)) && index++ < 2) {
    if((rc = mixer_cond_wait(&pQ->cond, &pQ->mtxCond)) != 0) {
      break;
    }
  }

  return pNode;
}

int frameq_recycle(FRAME_Q_T *pQ, FRAME_NODE_T *pNode) {

  if(!pQ || !pNode) {
    return -1;
  }

  pthread_mutex_lock(&pQ->mtx);

  if(!pQ->pUsed) {
    pQ->pUsed = pNode;
    pNode->pnext = NULL;
  } else {
    pNode->pnext = pQ->pUsed;
    pQ->pUsed = pNode;
  }

  pthread_mutex_unlock(&pQ->mtx);

  return 0;
}

int frameq_size(FRAME_Q_T *pQ) {
  int size;

  if(!pQ) {
    return 0;
  }

  pthread_mutex_lock(&pQ->mtx);

  size = pQ->size;

  pthread_mutex_unlock(&pQ->mtx);

  return size;
}

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
