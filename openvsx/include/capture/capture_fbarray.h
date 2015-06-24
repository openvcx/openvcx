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


#ifndef __CAPTURE_FBARRAY_H__
#define __CAPTURE_FBARRAY_H__

#define CAPTURE_FBARRAY_FLAG_KEYFRAME    0x8000

typedef struct CAPTURE_FBARRAY_ELEM {
  int                            flags;
  uint32_t                       ts;
  uint16_t                       seqnum;
  uint16_t                       keyframe;
} CAPTURE_FBARRAY_ELEM_T;

typedef struct CAPTURE_FBARRAY {
  unsigned int                   szElem;
  unsigned int                   idxElem;
  unsigned int                   countElem;
  uint32_t                       tsPrior;
  int                            havePriorKey;
  uint32_t                       tsPriorKey;
  unsigned int                   clockHz;
  
  struct VID_ENCODER_FBREQUEST   *pFbReq;
  CAPTURE_FBARRAY_ELEM_T         *pElemArr;
} CAPTURE_FBARRAY_T;

#define FBARRAY_ELEM_PRIOR(p, idx) ((idx) == 0 ? (p)->szElem - 1 : (idx) - 1)

CAPTURE_FBARRAY_T *fbarray_create(unsigned int sz, unsigned int clockHz);
void fbarray_destroy(CAPTURE_FBARRAY_T *pCtxt);
int fbarray_add(CAPTURE_FBARRAY_T *pCtxt, unsigned int ts, uint16_t seqnum, int keyframe, int flags);
void fbarray_reset(CAPTURE_FBARRAY_T *pCtxt);
void fbarray_dump(const CAPTURE_FBARRAY_T *pCtxt);

#endif // __CAPTURE_FBARRAY_H__
