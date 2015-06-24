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


#ifndef __STREAM_NET_MPG4V_H__
#define __STREAM_NET_MPG4V_H__

#include "stream/stream_net.h"
#include "util/fileutil.h"
#include "codecs/mpg4v.h"



typedef struct STREAM_MPG4V_FRAME {
  unsigned int                 numSlices;
  uint32_t                     szTot;
  uint64_t                     frameId;
  FILE_STREAM_T               *pFileStream;
  unsigned int                 idxInSlice;  // includes idxInAnnexBHdr if
                                            // includeAnnexBHdr is set
  unsigned int                 idxSlice;
  MPG4V_SAMPLE_T              *pSlice;
} STREAM_MPG4V_FRAME_T;

typedef struct STREAM_MPG4V {
  MPG4V_DESCR_T               *pMpg4v;
  unsigned int                 idxSlice;
  STREAM_MPG4V_FRAME_T         frame;
  MPG4V_SAMPLE_T               slice;
  MPG4V_SAMPLE_T               seqHdrs;
  int                          includeSeqHdrs;

  int                          loop;
} STREAM_MPG4V_T;



void stream_net_mpg4v_init(STREAM_NET_T *pNet);
float stream_net_mpg4v_jump(STREAM_MPG4V_T *pNet, float fStartSec, int syncframe);


#endif // __STREAM_NET_MPG4V_H__
