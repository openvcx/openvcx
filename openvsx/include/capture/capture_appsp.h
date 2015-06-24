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


#ifndef __CAPTURE_APPSP_H__
#define __CAPTURE_APPSP_H__


#include "capture.h"
#include "codecs/aac.h"
#include "formats/mp2pes.h"
#include "util/pktqueue.h"


typedef struct CAPTURE_CBDATA_SP_PCM {
  int                          bytesPerHz;
} CAPTURE_CBDATA_SP_PCM_T;

typedef struct CAPTURE_CBDATA_SP_AMR {
  int                          lastFrameType;
} CAPTURE_CBDATA_SP_AMR_T;


typedef struct CAPTURE_VP8_PARITITION {
  unsigned int                   byteIdx;
} CAPTURE_VP8_PARTITION_T;

typedef struct CAPTURE_CBDATA_SP_VP8{
  CAPTURE_VP8_PARTITION_T        partitions[VP8_PARTITIONS_COUNT];
} CAPTURE_CBDATA_SP_VP8_T;

typedef struct CAPTURE_CBDATA_SP_H263 {
  XC_CODEC_TYPE_T                codecType;
  H263_DESCR_T                   descr;
} CAPTURE_CBDATA_SP_H263_T;


#define CAPTURE_SP_FLAG_STREAM_STARTED      0x0001
#define CAPTURE_SP_FLAG_KEYFRAME            0x0002
#define CAPTURE_SP_FLAG_HAVEFRAGMENTSTART   0x0004
#define CAPTURE_SP_FLAG_INFRAGMENT          0x0008
#define CAPTURE_SP_FLAG_DAMAGEDFRAME        0x0010
#define CAPTURE_SP_FLAG_DROPFRAME           0x0020
#define CAPTURE_SP_FLAG_PREVLOST            0x0040

#define CAPTURE_SP_FLAG_H264_HAVESPS        0x0100
#define CAPTURE_SP_FLAG_H264_HAVEPPS        0x0200

//
// Callback context for each packet of a processed stream
//
typedef struct CAPTURE_CBDATA_SP {
  uint16_t                      inuse;
  FILE_STREAM_T                 stream1;

  //
  // For RTP, this is the prior received RTP sequence #
  // For TCP, this is the prior received TCP sequence #
  //
  uint32_t                      lastPktSeq;
  uint32_t                      islastPktLost;
  uint32_t                      lastPktTs;
  int                           frameHasError;
  int                           spFlags;
  CAPTURE_PKT_ACTION_DESCR_T   *pCapAction;
  const CAPTURE_STREAM_T       *pStream;
  CAPTURE_FBARRAY_T            *pCaptureFbArray;
  struct CAPTURE_CBDATA        *pAllStreams;
  
  union {
    MPG4V_DESCR_T               mpg4v;
    RTMP_RECORD_T               rtmp;
    CAPTURE_CBDATA_SP_H263_T    h263;
    //CAP_CBDATA_SP_H264_T        h264;
    CAPTURE_CBDATA_SP_PCM_T     pcm;
    CAPTURE_CBDATA_SP_AMR_T     amr;
    CAPTURE_CBDATA_SP_VP8_T     vp8;
  } u;

} CAPTURE_CBDATA_SP_T;

#define CAPTURE_MAX_STREAMS               4

//
// Callback context for each new stream being added.
// The stream's pCbUserData pointer should become set to an available
// CAPTURE_CBDATA_SP_T available stream specific entry
//
typedef struct CAPTURE_CBDATA {
  unsigned int maxStreams;
  CAPTURE_CBDATA_SP_T sp[CAPTURE_MAX_STREAMS];
  CAP_ASYNC_DESCR_T *pCfg;   // Can optionally point back to CAP_ASYNC_DESCR... with purpose
                             // of referring back to STREAMER_CFG_T VID_ENCODER_FBREQUEST_T
  const char *outDir;
  int overWriteOutput;
} CAPTURE_CBDATA_T;



#endif // __CAPTURE_APPSP_H__
