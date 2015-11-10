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

#if 1
void pool_dump(POOL_T *pPool) {
  POOL_ELEMENT_T *p;
  
  fprintf(stderr, "pool: 0x%x, num:%u inuse: ", (ptrdiff_t) (long unsigned int) pPool, pPool->numElements);
  p = pPool->pInUse;
  while(p) {
    fprintf(stderr, "0x%x ",  (ptrdiff_t) (long unsigned int)  p);
    p = p->pNext;
  }
  fprintf(stderr, "\nfree: ");
  p = pPool->pFree;
  while(p) {
    fprintf(stderr, "0x%x ", (ptrdiff_t) (long unsigned int) p);
    p = p->pNext;
  }
  fprintf(stderr, "\n");

}
#endif // 0

int pool_open(POOL_T *pPool, unsigned int numElements, unsigned int szElement,
              int lock) {
  unsigned int idx = 0;
  POOL_ELEMENT_T *pCur, *pPrev;

  if(!pPool || numElements <= 0 || szElement <= 0) {
    return -1;
  }

  VSX_DEBUG_MEM( LOG(X_DEBUG("MEM - pool_open %d x %d"), numElements, szElement); );

  if(!(pPool->pElements = (POOL_ELEMENT_T *) avc_calloc(numElements, szElement))) {
    return -1;
  }

  pPool->lock = lock;
  pPool->szElement = szElement;
  pPool->numElements = numElements;
  pPool->pInUse =  NULL;
  pPool->freeElements = numElements;
  pPool->destroy_onempty = 0;

  for(idx = 0; idx < numElements; idx++) {
    pCur = (POOL_ELEMENT_T *) (((char *) pPool->pElements) + (idx * szElement));
    pCur->id = idx + 1;

    if(idx == 0) {
      pPool->pFree = pPool->pElements;
    } else {
      //pCur = (POOL_ELEMENT_T *) (((char *) pPool->pElements) + (idx * szElement));
      pPrev = (POOL_ELEMENT_T *) (((char *) pPool->pElements) + ((idx  - 1) * szElement));
      pPrev->pNext = pCur;
      pCur->pPrev = pPrev;
    }
  }

  if(pPool->lock) {
    pthread_mutex_init(&pPool->mtx, NULL);
  }

  return 0;
}

POOL_ELEMENT_T *pool_get(POOL_T *pPool) {
  POOL_ELEMENT_T *pElement = NULL;

  if(!pPool) {
    return NULL;
  }

  if(pPool->lock) {
    pthread_mutex_lock(&pPool->mtx);
  }

  if(pPool->pFree) {
    pElement = pPool->pFree;
    pPool->pFree = pPool->pFree->pNext;
    if(pPool->pFree) {
      pPool->pFree->pPrev = NULL;
    }
  }

  if(pElement) {
    pElement->pPrev = NULL;
    pElement->pNext = NULL;
    if(pPool->pInUse) {
      pElement->pNext = pPool->pInUse;
      pPool->pInUse->pPrev = pElement;
    } 
    pPool->pInUse = pElement;
    pElement->inuse = 1;
    pPool->freeElements--;
  }

  if(pPool->lock) {
    pthread_mutex_unlock(&pPool->mtx);
  }

  //fprintf(stderr, "pool_get called 0x%x free:%d/%d\n", pElement, pPool->freeElements, pPool->numElements);

  return pElement;
}

static void pool_free(POOL_T *pPool) {

  //fprintf(stderr, "pool_free called\n");

  pPool->pFree = pPool->pInUse = NULL;

  if(pPool->pElements) {
    VSX_DEBUG_MEM( LOG(X_DEBUG("MEM - pool_freed")); );
    avc_free((void **) &pPool->pElements); 
  }

  if(pPool->lock) {
    pPool->lock = 0;
    pthread_mutex_destroy(&pPool->mtx);
  }

}

int pool_return(POOL_T *pPool, POOL_ELEMENT_T *pElement) {

  if(!pPool || !pElement) {
    return -1;
  } else if(!pElement->inuse) {
    LOG(X_ERROR("pool_returned called with invalid element 0x%x"), pElement);
    pool_dump(pPool);
    return -1;
  }

  if(pPool->lock) {
    pthread_mutex_lock(&pPool->mtx);
  }

  if(pElement->pPrev) {
    pElement->pPrev->pNext = pElement->pNext;
  }
  if(pElement->pNext) {
    pElement->pNext->pPrev = pElement->pPrev;
  }
  if(pElement == pPool->pInUse) {
    pPool->pInUse = pPool->pInUse->pNext;
  }

  pElement->pPrev = pElement->pNext = NULL;
  pElement->inuse = 0;
  pPool->freeElements++;

  if(pPool->pFree) {
    pPool->pFree->pPrev = pElement;
    pElement->pNext = pPool->pFree;
  }

  //fprintf(stderr, "pool_return called 0x%x free:%d/%d\n", pElement, pPool->freeElements, pPool->numElements);

  pPool->pFree = pElement;

  if(pPool->lock) {
    pthread_mutex_unlock(&pPool->mtx);
  }

  if(pPool->destroy_onempty && !pPool->pInUse) {
    pool_free(pPool);
    return 0;
  }

  return 0;
}


void pool_close(POOL_T *pPool, int wait_for_ret) {

  TIME_VAL tv0 = timer_GetTime();

  if(!pPool) {
    return;
  }

  if(pPool->pInUse) {
    pPool->destroy_onempty = 1;

    if(wait_for_ret) {
      LOG(X_DEBUG("pool_close %s waiting for resources to be returned"), (pPool->descr ? pPool->descr : ""));
      while(pPool->pInUse) {
        if(wait_for_ret > 0 && (timer_GetTime() - tv0) / TIME_VAL_MS > wait_for_ret) {
          LOG(X_WARNING("pool_close %s aborting wait for resources to be returned"), (pPool->descr ? pPool->descr : ""));
          break;
        }
        usleep(50000);
      }
      LOG(X_DEBUG("pool_close %s done waiting for resources to be returned"), (pPool->descr ? pPool->descr : ""));
    } else {
      LOG(X_DEBUG("pool_close %s delaying deallocation until resources returned"), 
        (pPool->descr ? pPool->descr : ""));
      return;
    }

  }

  pool_free(pPool);

}

