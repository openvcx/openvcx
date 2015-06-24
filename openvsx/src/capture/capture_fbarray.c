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

#if defined(VSX_HAVE_CAPTURE)


CAPTURE_FBARRAY_T *fbarray_create(unsigned int sz, unsigned int clockHz) {
  CAPTURE_FBARRAY_T *pCtxt = NULL;
  unsigned int szAlloc;

  if(sz <= 0) {
    return NULL;
  }

  szAlloc  = sizeof(CAPTURE_FBARRAY_T) + (sz * sizeof(CAPTURE_FBARRAY_ELEM_T));

  if(!(pCtxt = (CAPTURE_FBARRAY_T *) calloc(1, szAlloc))) {
    return NULL;
  }
  pCtxt->pElemArr = (CAPTURE_FBARRAY_ELEM_T *) ((unsigned char *) pCtxt + sizeof(CAPTURE_FBARRAY_T));
  pCtxt->szElem = sz;
  pCtxt->clockHz = clockHz;

  return pCtxt;
}

void fbarray_destroy(CAPTURE_FBARRAY_T *pCtxt) {

  //fprintf(stderr, "fbarray destroy\n");
  if(pCtxt) {
    pCtxt->szElem = 0;
    pCtxt->pElemArr = NULL;
    free(pCtxt);
  }
}

void fbarray_reset(CAPTURE_FBARRAY_T *pCtxt) {

  if(pCtxt) {
    //memset(pCtxt->pElemArr, 0, pCtxt->szElem * sizeof(CAPTURE_FBARRAY_ELEM_T));
    pCtxt->countElem = 0;
    pCtxt->tsPrior = 0;
    pCtxt->havePriorKey = 0;
  }
}

int fbarray_add(CAPTURE_FBARRAY_T *pCtxt, unsigned int ts, uint16_t seqnum, int keyframe, int flags) {
  int rc = 0;

  if(!pCtxt) {
    return -1;
  }
  
  if(++pCtxt->idxElem >= pCtxt->szElem) {
    pCtxt->idxElem = 0;
  }

  pCtxt->pElemArr[pCtxt->idxElem].flags = flags;
  pCtxt->pElemArr[pCtxt->idxElem].ts = ts;
  pCtxt->pElemArr[pCtxt->idxElem].seqnum = seqnum;
  pCtxt->pElemArr[pCtxt->idxElem].keyframe = keyframe;
  pCtxt->tsPrior = ts;
  
  if(keyframe) {
    pCtxt->havePriorKey = 1;
    pCtxt->tsPriorKey = ts;
  }

  if(pCtxt->countElem < pCtxt->szElem) {
    pCtxt->countElem++;
  }

  return rc;
}

void fbarray_dump(const CAPTURE_FBARRAY_T *pCtxt) {
  unsigned int idxElem;
  unsigned int iter = 0;

  if(!pCtxt) {
    return;
  }

  idxElem = pCtxt->idxElem;

  LOG(X_DEBUG("fb elements:%d, %uHz, havePriorKey:%d, tsPriorKey:%u, tsPrior:%u"), pCtxt->countElem, pCtxt->clockHz, pCtxt->havePriorKey, pCtxt->tsPriorKey, pCtxt->tsPrior);

  for(iter = 0; iter < MIN(pCtxt->szElem, pCtxt->countElem); iter++) {

    LOG(X_DEBUG("fb elem[%d (%d)]: ts:%u/%uHz (%.3f), flags: 0x%x"), iter, idxElem, pCtxt->pElemArr[idxElem].ts, pCtxt->clockHz, (float) pCtxt->pElemArr[idxElem].ts/pCtxt->clockHz, pCtxt->pElemArr[idxElem].flags);

    idxElem =  FBARRAY_ELEM_PRIOR(pCtxt, idxElem);
  }
  
}


#endif // VSX_HAVE_CAPTURE
