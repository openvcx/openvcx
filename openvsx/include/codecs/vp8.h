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


#ifndef __CODEC_VP8_H__
#define __CODEC_VP8_H__

#include "vsxconfig.h"
#include "fileutil.h"

#define VP8_DEFAULT_CLOCKHZ                90000
#define VP8_PD_EXTENDED_CTRL               0x80
#define VP8_PD_RESERVED_BIT                0x40
#define VP8_PD_START_PARTITION             0x10
#define VP8_PD_X_START_PICID_PRESENT       0x80
#define VP8_PD_X_START_TLOPICIDX_PRESENT   0x40
#define VP8_PD_X_START_TID_PRESENT         0x20
#define VP8_PD_X_START_TID_KEYIDX_PRESENT  0x10
#define VP8_PD_I_START_PICID_16BIT         0x80

#define VP8_PARTITIONS_COUNT               8
#define VP8_PARTITIONS_MASK                0x0f

typedef MP4_MDAT_CONTENT_NODE_T VP8_SAMPLE_T;

#define VP8_ISKEYFRAME(data) (!(*(data) & 0x01))

typedef struct VP8_DESCR {
  unsigned int                frameWidthPx;
  unsigned int                frameHeightPx;
  MP4_MDAT_CONTENT_NODE_T     lastFrame;
  uint32_t                    clockHz; 
  unsigned int                frameDeltaHz;
  unsigned int                numSamples;
  MKV_CONTAINER_T            *pMkv;
  unsigned int                lastFrameId;
} VP8_DESCR_T;

void vp8_free(VP8_DESCR_T *pVp8);
int vp8_loadFramesFromMkv(VP8_DESCR_T *pVp8, MKV_CONTAINER_T *pMkv);
MP4_MDAT_CONTENT_NODE_T *vp8_cbGetNextSample(void *pArg, int idx, int advanceFp);
int vp8_parse_hdr(const unsigned char *pbuf, unsigned int len, VP8_DESCR_T *pVp8);




#endif //__CODEC_VP8_H__
