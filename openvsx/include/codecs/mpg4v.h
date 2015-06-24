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


#ifndef __MPG4V_H__
#define __MPG4V_H__

#include "vsxconfig.h"
#include "mp4.h"
#include "fileutil.h"

#define MPEG4_MAX_FPS_SANITY      100

#define MPEG4_DEFAULT_CLOCKHZ     90000

#define MPEG4_HDR32_VOS           0x000001b0  // Visual Object Sequence start
#define MPEG4_HDR32_VOL           0x000001b1  // Visual Object Sequence end
#define MPEG4_HDR32_USERDATA      0x000001b2
#define MPEG4_HDR32_GOP           0x000001b3  // Group of VOP start
#define MPEG4_HDR32_VSE           0x000001b4  // Video Session Error
#define MPEG4_HDR32_VO            0x000001b5  // Visual Object start
#define MPEG4_HDR32_VOP           0x000001b6  // Visual Object Plane start
//                                0x00000120 - 0x0000012f // VOL
#define MPEG4_ISVOL(t)            ((t) >= 0x20 && (t) <= 0x2f)

#define MPEG4_HDR8_VOS            0xb0
#define MPEG4_HDR8_VOL            0xb1
#define MPEG4_HDR8_USERDATA       0xb2
#define MPEG4_HDR8_GOP            0xb3
#define MPEG4_HDR8_VSE            0xb4
#define MPEG4_HDR8_VO             0xb5
#define MPEG4_HDR8_VOP            0xb6

#define MPG4V_DECODER_FLAG_HDR          0x01
#define MPG4V_DECODER_FLAG_VID          0x02
#define MPG4V_DECODER_FLAG_VIDGOPSTART  0x04
#define MPG4V_DECODER_FLAG_UNKNOWN      0x08

typedef MP4_MDAT_CONTENT_NODE_T MPG4V_SAMPLE_T;

typedef struct CBDATA_ATTACH_MP4V_SAMPLE {
  void *pMpg4v;
  unsigned char arena[64];
} CBDATA_ATTACH_MP4V_SAMPLE_T;

typedef enum MPG4V_FRAME_TYPE {
  MPG4V_FRAME_TYPE_INVALID = 0,
  MPG4V_FRAME_TYPE_I       = 1,
  MPG4V_FRAME_TYPE_P       = 2,
  MPG4V_FRAME_TYPE_B       = 3,
} MPG4V_FRAME_TYPE_T;

typedef struct MPG4V_DECODER_CTXT {
  uint32_t               frameIdx;
  uint16_t               flag;
  uint8_t                storingHdr;
  uint8_t                startCode;
  int                    storedHdr;
  MPG4V_FRAME_TYPE_T     lastFrameType;
} MPG4V_DECODER_CTXT_T;

typedef struct MPG4V_SEQ_HDRS {
  // avoiding using pHdrs to avoid shallow copy woes (sdp mpg4v seqHdrs)
  //unsigned char              *pHdrs;
  uint8_t                     profile;
  uint8_t                     level;
  uint16_t                    have_profile_level;
  unsigned int                hdrsBufSz;
  unsigned int                hdrsLen;
  unsigned char               hdrsBuf[ESDS_STARTCODES_SZMAX];
} MPG4V_SEQ_HDRS_T;

typedef struct MPG4V_DESCR_PARAM {
  unsigned int                frameWidthPx;
  unsigned int                frameHeightPx;
  unsigned int                clockHz;
  unsigned int                frameDeltaHz;
} MPG4V_DESCR_PARAM_T;

typedef struct MPG4V_DESCR {
  MPG4V_DESCR_PARAM_T         param;
  FILE_STREAM_T              *pStreamIn;
  MP4_MDAT_CONTENT_T          content;
  unsigned int                numSamples;
  unsigned int                lastFrameId;
  MPG4V_SEQ_HDRS_T            seqHdrs;
  uint32_t                    clockHzOverride; 
  uint64_t                    lastPlayOffsetHz;
  CBDATA_ATTACH_MP4V_SAMPLE_T mp4vCbData;
  int                         haveGopStarts;

  MPG4V_DECODER_CTXT_T   ctxt;
} MPG4V_DESCR_T;

void mpg4v_free(MPG4V_DESCR_T *pMpg4v);
int mpg4v_getSamples(MPG4V_DESCR_T *pMpg4v);
MP4_MDAT_CONTENT_NODE_T *mpg4v_cbGetNextSample(void *pArg, int idx, int advanceFp);



typedef struct MPG4V_CBDATA_WRITE_SAMPLE {
  FILE_STREAM_T *pStreamOut;
  unsigned char arena[4096];
} MPG4V_CBDATA_WRITE_SAMPLE_T;


int mpg4v_writeStart(FILE_STREAM_T *pStreamOut, const unsigned char *pData,
                     unsigned int len);

int mpg4v_cbWriteSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz);

int mpg4v_createSampleListFromMp4(MPG4V_DESCR_T *pMpg4v, MP4_CONTAINER_T *pMp4,
                                  CHUNK_SZ_T *pStchk, float fStartSec);

int mpg4v_decodeVOL(BIT_STREAM_T *pStream, MPG4V_DESCR_PARAM_T *pMpg4vParam);
int mpg4v_decodeObj(const STREAM_CHUNK_T *pStream, MPG4V_DESCR_T *pMpg4v);
int mpg4v_appendStartCodes(MPG4V_DESCR_T *pMpg4v, unsigned char *pData, unsigned int len);



#endif //__MPG4V_H__
