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


#ifndef __XCODE_TYPES_H__
#define __XCODE_TYPES_H__

#include "codecs/aac.h"
#include "codecs/h264.h"
#include "codecs/mpeg2.h"
#include "xcode_ipc.h"
#include "stream/stream_fb.h"

#define XCODE_CFR_OUT_TOLERANCE_DEFAULT   36000 
#define XCODE_VID_LOOKAHEAD_MIN_FPS       23
#define XCODE_VID_LOOKAHEAD_AUTO_FLAG     0x80000000
#define XCODE_VID_THREADS_DEC_NOTSET      0x80000000
#define XCODE_IDR_VIA_FIR                 1

typedef struct AV_OFFSET {
  int64_t                       offset0;
  int64_t                       offsetCur;
} AV_OFFSET_T;

typedef struct XCODE_H264_OUT {
  H264_DECODER_CTXT_T           h264;
  SPSPPS_RAW_T                  *pSpspps;
  unsigned int                  expectedPoc;
  unsigned int                  lastPoc;
  int                           pocDelta;
} XCODE_H264_OUT_T;

typedef struct XCODE_MPG4V_OUT {
  MPG4V_DESCR_T                 mpg4v;
} XCODE_MPG4V_OUT_T;


typedef struct XCODE_H264_IN {
  SPSPPS_RAW_T                  spsppsprev;
  SPSPPS_RAW_T                  spspps;
  H264_DECODER_CTXT_T           h264;
} XCODE_H264_IN_T;

typedef struct XCODE_MPG4V_IN {
  MPG4V_DESCR_T                   mpg4v;
} XCODE_MPG4V_IN_T;


typedef struct VID_UDATA_OUT {

  union {
    XCODE_H264_OUT_T             xh264;
    XCODE_MPG4V_OUT_T            xmpg4v; 
  } uout;

  uint64_t                       payloadByteCntTot;
  unsigned int                   payloadByteCnt;
  unsigned int                   payloadFrameCnt;
  struct timeval                 tv0;
  struct timeval                 tv1;
  int                            slowEncodePeriods;
  int64_t                        tvLateFromRTPrior;

  float                          lastQuantizerAvg;

  VID_ENCODER_FBREQUEST_T        fbReq;

} VID_UDATA_OUT_T;

enum STREAM_XCODE_FLAGS {
  STREAM_XCODE_FLAGS_NONE          = 0x00,
  STREAM_XCODE_FLAGS_RESET         = 0x01,    // should perform reset on next frame
  STREAM_XCODE_FLAGS_UPDATE        = 0x02,    // should perform configuration update on next frame
  STREAM_XCODE_FLAGS_LOAD_XCFG     = 0x04     // do (delayed) load of xcode config string
                                              // such as when processing conditional xcode config
                                              // based on input video resolution
};

typedef struct STREAM_XCODE_VID_UDATA {

  enum XC_CODEC_TYPE             type;
  struct STREAM_XCODE_DATA      *pVidData;

  PKTQUEUE_PKT_T                 tmpFrame;

  //
  // Obtained from input feed
  //

  union {
    MPEG2_SEQ_HDR_T              xmpeg2;
    XCODE_H264_IN_T              xh264;
    XCODE_MPG4V_IN_T             xmpg4v;
  } uin;

  unsigned int                   seqHdrWidth;
  unsigned int                   seqHdrHeight;
  int                            haveSeqStart;
  unsigned int                   noSeqStartCnt;

  //
  // output specific seqHdrs, counters
  //
  VID_UDATA_OUT_T                out[IXCODE_VIDEO_OUT_MAX];

  //
  // action flags
  //
  enum STREAM_XCODE_FLAGS        flags;

  VID_ENCODER_FBREQUEST_T       *pStreamerFbReq; 

  //
  // xcode config file path for delayed config loading,
  // such as after receiving input video stream sequence headers
  // to determine conditional properties based on input resolution
  //
  const char                   *xcodecfgfile;
  int                           delayedCfgDone;

  int                           enableOutputSharing; // set if this stream context is to share it's output vid frame
  struct STREAMER_PIP          *pStreamerPip;  // points to it's own &STREAMER_CFG_T::pip
  int                           streamerPipHaveKeyframe;
  PKTQUEUE_T                   *pPipFrQ;

} STREAM_XCODE_VID_UDATA_T;

typedef struct XCODE_AAC_IN {
  AAC_ADTS_FRAME_T               descr;
  //int                            isinit;
} XCODE_AAC_IN_T;

typedef struct XCODE_AC3_IN {
  AC3_DESCR_T                    descr;
  //int                            isinit;
} XCODE_AC3_IN_T;

typedef struct XCODE_MPA_IN {
  int                              pad;
  //int                            isinit;
} XCODE_MPA_IN_T;

typedef struct XCODE_AMR_IN {
  int                              pad;
  //int                            isinit;
} XCODE_AMR_IN_T;

typedef struct XCODE_VORBIS_IN {
  unsigned char                   *phdr;
  unsigned int                     hdrLen;
} XCODE_VORBIS_IN_T;

typedef struct STREAM_XCODE_AUD_UDATA {

  enum XC_CODEC_TYPE            type;
  struct STREAM_XCODE_DATA     *pAudData;

  unsigned char                *pBuf;
  unsigned int                  bufLen;
  unsigned int                  bufIdx;
  PKTQUEUE_PKT_T                tmpFrame;
  int                           lastFrameErr;
  uint64_t                      lastEncodedPts;
  unsigned int                  idxEncodedBuf;
  unsigned int                  szEncodedBuf;
  unsigned char                 encodedBuf[16384];

  uint64_t                      outHz;

  union {
    XCODE_AC3_IN_T               ac3;
    XCODE_AAC_IN_T               aac;
    XCODE_MPA_IN_T               mpa;
    XCODE_AMR_IN_T               amr; 
    XCODE_VORBIS_IN_T            vorbis;
  } u;
  int                            haveSeqStart;

  struct STREAMER_CFG           *pStartMixer;
  struct PIP_LAYOUT_MGR         *pPipLayout;


} STREAM_XCODE_AUD_UDATA_T;


typedef struct COMPOUND_PES_FRAME_OFFSET {
  unsigned int                       byteOffset;
  unsigned int                       len;
  uint64_t                           pts;
} COMPOUND_PES_FRAME_OFFSET_T;

typedef struct FRAME_TIME {

  //
  // Input and output pts/dts according to current transcoding configuration
  // 
  PKTQ_TIME_T                        intm;
  PKTQ_TIME_T                        outtm;

  //
  // the starttm0 corresponds to the start of this transcoding session
  // all output pts across any resets should maintain a consistent clock
  //
  int                                havestarttm0;
  uint64_t                           starttm0;       // 1st frame ever 
  TIME_VAL                           starttv0;       // wall clock time 1st frame out
  uint64_t                           durationTot;

  //
  // a period is the beginning of the previous reset time
  // a reset can be triggered from a stream_net_xxx data source
  // such as a large pts jump (channel change)
  // or a xcoder re-init (input sps / resolution change)
  uint64_t                           starttmperiod; 

  // 
  // seq start is the start of eligible video decoding, such as sps/pps + I frame
  //
  int                                prevhaveseqstart;
  int                                haveseqstart;
  uint64_t                           seqstarttm;
  uint64_t                           seqstartoffset;

  // 
  // If cfgCFRInput is set, cfrInDriftAdj maintains parity for any drift from
  // actual input timestamps
  //
  int64_t                            cfrInDriftAdj;
  
  //
  // If cfgCFROutput is set, cfrOutDriftAdj maintains parity for any drift from
  // actual outpu timestamps
  //
  int64_t                            cfrOutDriftAdj;
 
  //
  // Video transcoding latency in pts 90Khz units
  //
  int64_t                            encDelay;
  int                                encDelayFrames;
  int                                haveEncDelay;
  uint64_t                           encStartPts0;
  uint64_t                           encStartPts1;

  //
  // Output adjustment to pts
  // 
  //int64_t                            outTmAdj;

  //
  // flag to check if coming out of reset action
  //
  int                                waspaused;

  //
  // frame counters - maintained seperate from xcoder internal counters
  //
  unsigned int                       frDecOutPeriod;
  unsigned int                       frPrevDecOutPeriod;
  unsigned int                       frEncOutPeriod;
  unsigned int                       frPrevEncOutPeriod;
  unsigned int                       frEncodeInIdxAtLastUpdate;
  uint64_t                           frEncPtsAtLastUpdate;

} FRAME_TIME_T;

//
// Extension of IXCODE_OUTBUF_T
//
typedef struct XCODE_OUTBUF {
  IXCODE_OUTBUF_T                    outbuf;
  PKTQ_TIME_T                        tms[IXCODE_VIDEO_OUT_MAX];
  int                                keyframes[IXCODE_VIDEO_OUT_MAX];
  unsigned int                       outidx;
} XCODE_OUTBUF_T;

#define COMPOUND_FRAMES_MAX          IXCODE_COMPOUND_FRAMES_MAX

typedef struct STREAM_PES_FRAME {
  uint32_t                           idxReadInFrame;
  unsigned int                       idxReadFrame;  // stream_net_pes uses this to track
                                                    // queue overwriting id
  PKTQUEUE_PKT_T                     pkt;
  int64_t                            dbgprevpts;
  PKTQUEUE_PKT_T                    *pSwappedSlot;

  XCODE_OUTBUF_T                     xout;

  unsigned char                     *pData;
  unsigned int                       lenData;

  int                                useTmpFrameOut;
  int                                upsampleFrIdx;
  int                                numUpsampled;
  FRAME_TIME_T                       tm;

  uint64_t                           ptsLastCompoundFrame;
  unsigned int                       idxCompoundFrame;
  unsigned int                       numCompoundFrames;
  int                                useCompoundFramePts;
  COMPOUND_PES_FRAME_OFFSET_T        compoundFrames[COMPOUND_FRAMES_MAX];

} STREAM_PES_FRAME_T;



typedef struct STREAM_XCODE_DATA {
  STREAM_PES_FRAME_T             curFrame; // latest frame from queue
  IXCODE_CTXT_T                 *piXcode;
  uint16_t                       inStreamType;
  uint16_t                       haveFrameTmStartOffset;
  //uint64_t                       frameTmStartOffset;
  int64_t                       frameTmStartOffset;
  AV_OFFSET_T                   *pavs[IXCODE_VIDEO_OUT_MAX];
  int                           *pignoreencdelay_av;
  struct timeval                 tvXcodeInit0;
  VIDEO_DESCRIPTION_GENERIC_T   *pVidProps[IXCODE_VIDEO_OUT_MAX];
  struct STREAM_XCODE_DATA      *pComplement;
  unsigned int                   numFramesOut;
  int64_t                        inPts90KhzAdj;
  int64_t                        tvLateFromRT; // how late is the output stream from real-time processing
  int64_t                        tvLateFromRT0; // tvLateFromRT at time of first encoder output frame 
  int                            consecErr;
} STREAM_XCODE_DATA_T;

#define TVLATEFROMRT(p) ((p)->tvLateFromRT - (p)->tvLateFromRT0)


typedef struct STREAM_XCODE_CTXT {
  STREAM_XCODE_VID_UDATA_T      vidUData;
  STREAM_XCODE_AUD_UDATA_T      audUData;
  STREAM_XCODE_DATA_T           vidData;
  STREAM_XCODE_DATA_T           audData;
} STREAM_XCODE_CTXT_T;

#define XCODE_AUD_UDATA_PTR(pXcode)  ((STREAM_XCODE_AUD_UDATA_T *) (pXcode)->aud.pUserData) 
#define XCODE_VID_UDATA_PTR(pXcode)  ((STREAM_XCODE_VID_UDATA_T *) (pXcode)->vid.pUserData) 

#endif // __XCODE_TYPES_H__
