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


#ifndef __VORBIS_H__
#define __VORBIS_H__

#include "vsxconfig.h"

typedef MP4_MDAT_CONTENT_NODE_T VORBIS_SAMPLE_T;

typedef struct VORBIS_DESCR {
  MP4_MDAT_CONTENT_NODE_T     lastFrame;
  unsigned int                frameDeltaHz;
  unsigned int                numSamples;
  MKV_CONTAINER_T            *pMkv;
  unsigned int                lastFrameId;

  uint32_t                    clockHz;
  unsigned int                channels;
  unsigned int                bitrate_min;
  unsigned int                bitrate_avg;
  unsigned int                bitrate_max;
  unsigned int                blocksize[2];
  unsigned int                framesize;

  unsigned char              *codecPrivData;
  unsigned int                codecPrivLen;
} VORBIS_DESCR_T;


void vorbis_free(VORBIS_DESCR_T *pVorbis);
int vorbis_loadFramesFromMkv(VORBIS_DESCR_T *pVorbis, MKV_CONTAINER_T *pMkv);
MP4_MDAT_CONTENT_NODE_T *vorbis_cbGetNextSample(void *pArg, int idx, int advanceFp);
int vorbis_parse_hdr(const unsigned char *pbuf, unsigned int len, VORBIS_DESCR_T *pVorbis);


#endif // __VORBIS_H__
