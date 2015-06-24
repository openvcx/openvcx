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


#ifndef __STREAM_NET_VORBIS_H__
#define __STREAM_NET_VORBIS_H__

#include "stream/stream_net.h"
#include "util/fileutil.h"
#include "codecs/vorbis.h"



typedef struct STREAM_VORBIS_FRAME {
  uint32_t                     szTot;
  uint64_t                     frameId;
  FILE_STREAM_T               *pFileStream;
  unsigned int                 idxInFrame; 
  unsigned int                 idxSlice; 
  VORBIS_SAMPLE_T              *pSample;
} STREAM_VORBIS_FRAME_T;

typedef struct STREAM_VORBIS {
  VORBIS_DESCR_T              *pVorbis;
  STREAM_VORBIS_FRAME_T           frame;
  unsigned int                 idxFrame;

  int                          loop;
} STREAM_VORBIS_T;



void stream_net_vorbis_init(STREAM_NET_T *pNet);
float stream_net_vorbis_jump(STREAM_VORBIS_T *pNet, float fStartSec);


#endif // __STREAM_NET_VORBIS_H__
