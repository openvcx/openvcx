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


#ifndef __STREAM_NET_IMAGE_H__
#define __STREAM_NET_IMAGE_H__

#include "stream/stream_net.h"
#include "util/fileutil.h"
#include "formats/image.h"


typedef struct STREAM_IMAGE {
  void                        *pCbData;
  IMAGE_GENERIC_DESCR_T       *pImage;
  MP4_MDAT_CONTENT_NODE_T      node;
  unsigned int                 idxFrame;
  unsigned int                 idxInFrame;

  uint64_t                     frameId;
  TIME_VAL                     tvStart;
  unsigned int                 clockHz;
  unsigned int                 frameDeltaHz;

  int                          loop;
} STREAM_IMAGE_T;


void stream_net_image_init(STREAM_NET_T *pNet);
//float stream_net_h264_jump(STREAM_H264_T *pNet, float fStartSec, int syncframe);


#endif // __STREAM_NET_IMAGE_H__
