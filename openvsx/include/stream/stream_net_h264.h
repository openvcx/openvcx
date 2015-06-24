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


#ifndef __STREAM_NET_H264_H__
#define __STREAM_NET_H264_H__

#include "stream/stream_net.h"
#include "util/fileutil.h"
#include "codecs/h264avc.h"



typedef struct STREAM_H264_FRAME {
  unsigned int                 numSlices;
  uint32_t                     szTot;
  uint64_t                     frameId;
  FILE_STREAM_T               *pFileStream;
  unsigned int                 idxInSlice;  // includes idxInAnnexBHdr if
                                            // includeAnnexBHdr is set
  unsigned int                 idxInAnnexBHdr;
  unsigned int                 idxSlice;
  H264_NAL_T                   *pSlice;
} STREAM_H264_FRAME_T;

typedef struct STREAM_H264 {
  void                        *pCbData;
  H264_AVC_DESCR_T            *pH264;
  unsigned int                 idxNal;
  STREAM_H264_FRAME_T          frame;
  H264_NAL_T                   nal;

  H264_NAL_T                   spsSlice;
  H264_NAL_T                   ppsSlice;
  int                          includeAnnexBHdr;
  int                          includeSeqHdrs;
  int                          loop;
} STREAM_H264_T;


void stream_net_h264_init(STREAM_NET_T *pNet);
float stream_net_h264_jump(STREAM_H264_T *pNet, float fStartSec, int syncframe);


#endif // __STREAM_NET_H264_H__
