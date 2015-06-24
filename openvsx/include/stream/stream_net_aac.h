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


#ifndef __STREAM_NET_AAC_H__
#define __STREAM_NET_AAC_H__

#include "stream/stream_net.h"
#include "util/fileutil.h"
#include "codecs/aac.h"



typedef struct STREAM_AAC {
  void                        *pCbData;
  MP4_MDAT_CONTENT_NODE_T     *pSample;
  AAC_DESCR_T                  aac;
  unsigned int                 idxSample;
  unsigned int                 idxSampleInFrame;
  unsigned int                 idxInSample;
  unsigned char                adtsHdrBuf[64];
  unsigned int                 lenAdtsHdr;

  int                          loop;
} STREAM_AAC_T;

void stream_net_aac_init(STREAM_NET_T *pNet);
float stream_net_aac_jump(STREAM_AAC_T *pNet, float fStartSec);

#endif // __STREAM_NET_AAC_H__
