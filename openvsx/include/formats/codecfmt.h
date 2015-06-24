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


#ifndef __CODEC_FMT_H__
#define __CODEC_FMT_H__

#include <stdio.h>
#include "unixcompat.h"
#include "codecs/avcc.h"
#include "codecs/esds.h"
#include "codecs/aac.h"


typedef struct CODEC_VID_CTXTDESCR {
  unsigned int             width; 
  unsigned int             height; 
  unsigned int             clockHz;
  unsigned int             frameDeltaHz; 

  const unsigned char     *pstartHdrs;
  unsigned int             startHdrsLen;

} CODEC_VID_CTXTDESCR_T;

typedef struct CODEC_VID_CTXT_H264 {
  AVC_DECODER_CFG_T        avcc;
  unsigned int             avccRawLen;
  unsigned char            avccRaw[16 + AVCC_SPS_MAX_LEN + AVCC_PPS_MAX_LEN];

  uint8_t                  profile;
  uint8_t                  level;

} CODEC_VID_CTXT_H264_T;

typedef struct CODEC_VID_CTXT_VP8 {
  int fill;
} CODEC_VID_CTXT_VP8_T;

typedef struct CODEC_AUD_CTXTDESCR {
  unsigned int             clockHz;
  unsigned int             channels; 

  const unsigned char     *pstartHdrs;
  unsigned int             startHdrsLen;

} CODEC_AUD_CTXTDESCR_T;

typedef struct CODEC_AUD_CTXT_AAC {
  ESDS_DECODER_CFG_T       esds;
  AAC_ADTS_FRAME_T         adtsFrame;
  uint8_t                  adtsHdr[7];
  uint8_t                  audHdr;
  uint8_t                  spbuf[16];
} CODEC_AUD_CTXT_AAC_T;

typedef struct CODEC_AUD_CTXT_VORBIS {
  const char              *pData;
  unsigned int             lenData;
} CODEC_AUD_CTXT_VORBIS_T;

typedef struct CODEC_VID_CTXT {
  XC_CODEC_TYPE_T          codecType;
  struct STREAMER_CFG      *pStreamerCfg;
  uint64_t                 tm0;
  uint64_t                 tmprev;
  uint64_t                 tsLastWrittenFrame;  // output format ts
  uint8_t                  haveSeqHdr;
  uint8_t                  sentSeqHdr;
  uint8_t                  haveKeyFrame;
  CODEC_VID_CTXTDESCR_T    ctxt;
  unsigned int             nonKeyFrames;
  BYTE_STREAM_T            tmpFrame;

  union {    
    CODEC_VID_CTXT_H264_T    h264;
    CODEC_VID_CTXT_VP8_T     vp8;
  } codecCtxt;

  union {
    SPSPPS_RAW_T           h264;
  } seqhdr;

} CODEC_VID_CTXT_T;


typedef struct CODEC_AUD_CTXT {
  XC_CODEC_TYPE_T          codecType;
  uint64_t                 tm0;
  struct STREAMER_CFG      *pStreamerCfg;
  uint64_t                 tmprev;             // pts 90Khz
  uint64_t                 tsLastWrittenFrame;  // output format ts
  uint8_t                  haveSeqHdr;
  CODEC_AUD_CTXTDESCR_T    ctxt;

  BYTE_STREAM_T            tmpFrame;

  union {    
    CODEC_AUD_CTXT_AAC_T    aac;
    CODEC_AUD_CTXT_VORBIS_T vorbis;
  } codecCtxt;

} CODEC_AUD_CTXT_T;

typedef struct CODEC_AV_CTXT {

  CODEC_VID_CTXT_T           vid;
  CODEC_AUD_CTXT_T           aud;

} CODEC_AV_CTXT_T;

void codec_nonkeyframes_warn(CODEC_VID_CTXT_T *pVidCtxt, const char *descr);
void codecfmt_close(CODEC_AV_CTXT_T *pAv);
int codecfmt_init(CODEC_AV_CTXT_T *pAv, unsigned int vidTmpFrameSz, unsigned int audTmpFrameSz);

#endif // __CODEC_FMT_H__
