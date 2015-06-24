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


#ifndef __H263_H__
#define __H263_H__

#include "vsxconfig.h"
#include "bits.h"

#define H263_DEFAULT_CLOCKHZ     90000


typedef enum H263_FRAME_TYPE {
  H263_FRAME_TYPE_INVALID = 0,
  H263_FRAME_TYPE_I       = 1,
  H263_FRAME_TYPE_P       = 2,
  H263_FRAME_TYPE_B       = 3
} H263_FRAME_TYPE_T;

typedef struct H263_FRAME {
  uint32_t                    picNum;
  H263_FRAME_TYPE_T           frameType;
} H263_FRAME_T;


typedef struct H263_DESCR_PARAM {
  unsigned int                frameWidthPx;
  unsigned int                frameHeightPx;
  unsigned int                clockHz;
  unsigned int                frameDeltaHz;
} H263_DESCR_PARAM_T;

typedef struct H263_PLUS_DESCR{
  int                        h263plus;
  int                        advIntraCoding;
  int                        deblocking;
  int                        sliceStructured;
} H263_PLUS_DESCR_T;

typedef struct H263_DESCR {
  H263_PLUS_DESCR_T          plus;
  H263_FRAME_T               frame;
  H263_DESCR_PARAM_T         param;
} H263_DESCR_T;




int h263_decodeHeader(BIT_STREAM_T *pStream, H263_DESCR_T *pH263, int rfc4629);



#endif //__H263_H__
