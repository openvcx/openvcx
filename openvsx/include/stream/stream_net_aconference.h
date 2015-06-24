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


#ifndef __STREAM_NET_ACONFERENCE_H__
#define __STREAM_NET_ACONFERENCE_H__

#include "stream/stream_net.h"
#include "util/fileutil.h"
#include "codecs/vp8.h"

#define STREAM_ACONFERENCE_ID   0x01020304
#define STREAM_ACONFERENCE_PTIME    20 

typedef int STREAM_ACONFERENCE_DESCR_T;


typedef struct STREAM_ACONFERENCE_FRAME {
  uint32_t                     szTot;
  uint64_t                     frameId;
  unsigned int                 idxInSlice; 
  unsigned int                 idxSlice;
  MP4_MDAT_CONTENT_NODE_T      content;
} STREAM_ACONFERENCE_FRAME_T;

typedef struct STREAM_ACONFERENCE {
  int                           id;
  TIME_VAL                      tvStart;
  STREAM_ACONFERENCE_FRAME_T    frame;
  int64_t                       ptsOffset;
  unsigned int                  idxSlice;
  unsigned int                  clockHz;
  unsigned int                  frameDeltaHz;
  struct STREAM_VCONFERENCE    *pConferenceV;
} STREAM_ACONFERENCE_T;



void stream_net_aconference_init(STREAM_NET_T *pNet);


#endif // __STREAM_NET_ACONFERENCE_H__
