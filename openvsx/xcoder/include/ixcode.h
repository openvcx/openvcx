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

#ifndef __IXCODE_H__
#define __IXCODE_H__

#include "unixcompat.h"
#include "formats/filetypes.h"
#include "ixcode_pip.h"
#include "ixcode_filter.h"
#include "xcodeconfig.h"

//
// Maximum single frame data size
//
// Should be large enough to accomodate large MPEG-2 I frames
#define IXCODE_SZFRAME_MAX                0x88000

#define IXCODE_COMPOUND_FRAMES_MAX        24

typedef enum FRAME_TYPE {
  FRAME_TYPE_UNKNOWN    = 0,
  FRAME_TYPE_I          = 1,
  FRAME_TYPE_P          = 2,
  FRAME_TYPE_B          = 3,
  FRAME_TYPE_SI         = 5,
  FRAME_TYPE_SP         = 6
} FRAME_TYPE_T;

typedef enum RATE_CTRL_TYPE {
  RATE_CTRL_TYPE_BITRATE     = 0, 
  RATE_CTRL_TYPE_CQP         = 1, 
  RATE_CTRL_TYPE_CRF         = 2 
} RATE_CTRL_TYPE_T;

typedef enum ENCODER_QUALITY {
  ENCODER_QUALITY_NOTSET     = 0,
  ENCODER_QUALITY_BEST       = 1,
  ENCODER_QUALITY_HIGH       = 2,
  ENCODER_QUALITY_MED        = 3,
  ENCODER_QUALITY_LOW        = 4,
  ENCODER_QUALITY_LOWEST     = 5
} ENCODER_QUALITY_T;


//
// When XCODE_IPC is defined, the IXCODE_CTXT_T is stored in
// shared memory.
// If adding parameters within IXCODE_CTXT or its contents
// that are referenced from ixcode.c then src/xcode_ipc.c:xcode_init_vid,
// xcode_init_aud, xcode_frame_vid, xcode_frame_aud must also be updated
// to ensure these parameters are properly passed to the transcoder process
// via IPC when XCODE_IPC is defined
//
//

typedef struct IXCODE_COMMON_CTXT {
  int                          cfgDo_xcode;
  int                          cfgNoDecode;      // used when swithching to sendonly (input on hold)
  int                          cfgAllowUpsample; // allow upsample if out fps > in fps
  int                          is_init;
  int                          cfgCFRInput;       // treat input as constant frame rate
  int                          cfgCFROutput;      // treat output as constant frame rate
  int                          cfgCFRTolerance;

  XC_CODEC_TYPE_T              cfgFileTypeIn;
  XC_CODEC_TYPE_T              cfgFileTypeOut;
  int                          cfgVerbosity;
  int                          cfgFlags;

  // Set by ixcode internally
  uint64_t                     inpts90Khz;
  uint64_t                     outpts90Khz;
  unsigned int                 decodeInIdx;      // count of frames into decoder
  unsigned int                 decodeOutIdx;     // count of frames from decoder
  unsigned int                 encodeInIdx;      // count of frames into encoder
  unsigned int                 encodeOutIdx;     // count of frames from encoder

  void                        *pPrivData;         // private date managed internally by
                                                  // ixcode
  void                        *pIpc;
  void                        *pLogCtxt;
  
} IXCODE_COMMON_CTXT_T;

typedef struct COMPOUND_PKT {
  uint64_t                     pts;
  uint32_t                     length;
} COMPOUND_PKT_T;

typedef struct IXCODE_OUTBUF {
  unsigned char               *buf;
  unsigned int                 lenbuf;
  unsigned char               *poffsets[IXCODE_VIDEO_OUT_MAX]; 
  int                          lens[IXCODE_VIDEO_OUT_MAX];
} IXCODE_OUTBUF_T;


typedef struct IXCODE_VIDEO_ABR {
  unsigned int                 cfgBitRateOutMax;
  unsigned int                 cfgBitRateOutMin;
  int                          cfgGopMsMax;
  int                          cfgGopMsMin;
  unsigned int                 cfgOutMaxClockHz;
  unsigned int                 cfgOutMaxFrameDeltaHz;
  unsigned int                 cfgOutMinClockHz;
  unsigned int                 cfgOutMinFrameDeltaHz;
  unsigned int                 resOutThrottledMaxClockHz;
  unsigned int                 resOutThrottledMaxFrameDeltaHz;

} IXCODE_VIDEO_ABR_T;


typedef struct IXCODE_VIDEO_OUT {
  int                          active;
  int                          passthru;

  unsigned int                 cfgOutH;       // desired output resolution
  unsigned int                 cfgOutV;       // desired output resolution

  unsigned int                 cfgOutPipH;    // desired output PIP resolution
  unsigned int                 cfgOutPipV;    // desired output PIP resolution

  IXCODE_VIDEO_ABR_T           abr;

  unsigned int                 cfgBitRateOut;
  unsigned int                 cfgBitRateOutAbrMin;
  unsigned int                 cfgBitRateOutAbrMax;
  unsigned int                 cfgBitRateTolOut;
  int                          cfgMaxSliceSz;   
  RATE_CTRL_TYPE_T             cfgRateCtrl;
  int                          cfgSceneCut;
  int                          cfgQ;  
  int                          cfgQMax;  
  int                          cfgQMin;  
  int                          cfgQDiff;  
  unsigned int                 cfgVBVBufSize;
  unsigned int                 cfgVBVMaxRate;
  unsigned int                 cfgVBVMinRate;
  float                        cfgVBVAggressivity;
  float                        cfgVBVInitial;
  float                        cfgIQRatio;
  float                        cfgBQRatio;
  int                          cfgThreads;
  ENCODER_QUALITY_T            cfgFast;       
  int                          cfgProfile;
  unsigned int                 cfgScaler;
  int                          cfgGopMaxMs;
  int                          cfgGopMinMs;
  int                          cfgForceIDR;
  int                          cfgLookaheadmin1;   // 0 - not set (chosen by encoder)
                                                   // 1 - min value, sets it to 0
                                                   // x > 1, # of fo lookahead frames 
  int                          cfgMBTree;
  float                        cfgQCompress;

  IXCODE_VIDEO_CROP_T          crop;
  IXCODE_FILTER_UNSHARP_T      unsharp;
  IXCODE_FILTER_DENOISE_T      denoise;
  IXCODE_FILTER_COLOR_T        color;
  IXCODE_FILTER_ROTATE_T       rotate;
  IXCODE_FILTER_TEST_T         testFilter;


  unsigned int                 resOutH;      // actual output resolution
  unsigned int                 resOutV;      // actual output resolution
                                             // if either cfgOutH or cfgOutV
                                             // is set, but not both, then
                                             // actual output resolution will
                                             // be set according to input
                                             // picture aspect ratio
  int                          resLookaheadmin1;

  // Set by ixcode internally

  float                        frameQ;
  float                        qTot;
  int                          qSamples;
  FRAME_TYPE_T                 frameType;
  int64_t                      pts;
  int64_t                      dts;
  unsigned char                hdr[256];
  unsigned int                 hdrLen;

} IXCODE_VIDEO_OUT_T;

typedef struct IXCODE_VIDEO_CTXT {
  IXCODE_COMMON_CTXT_T         common;

  IXCODE_VIDEO_OUT_T           out[IXCODE_VIDEO_OUT_MAX];

  unsigned int                 cfgOutClockHz;
  unsigned int                 cfgOutFrameDeltaHz;
  int                          cfgThreadsDec;
  unsigned int                 inClockHz;
  unsigned int                 inFrameDeltaHz;
  int                          usewatermark;
  unsigned int                 extradatasize;
  unsigned char                extradata[512];
  void                         *pUserData;

  unsigned int                 inH;          // input horizontal resolution (raw rgb/yuv)
  unsigned int                 inV;          // input vertical resolution (raw rgb/yuv)

  unsigned int                 resOutClockHz;
  unsigned int                 resOutFrameDeltaHz;
                                             
  IXCODE_VIDEO_PIP_T           pip;
  IXCODE_VIDEO_OVERLAY_T       overlay; 

} IXCODE_VIDEO_CTXT_T;

typedef struct IXCODE_AUDIO_CTXT {
  IXCODE_COMMON_CTXT_T       common;

  unsigned int                 cfgBitRateOut;
  unsigned int                 cfgSampleRateOut;
  unsigned int                 cfgSampleRateIn;
  unsigned int                 cfgChannelsOut;
  unsigned int                 cfgChannelsIn;
  unsigned int                 cfgDecoderSamplesInFrame;
  int                          cfgVolumeAdjustment;
  int                          cfgForceOut;
  int                          cfgProfile;
  unsigned int                 extradatasize;
  unsigned char               *pextradata;
  void                        *pUserData;


  // Set by ixcode internally

  unsigned int                 encoderSamplesInFrame;
  int64_t                      pts;
  unsigned char               *phdr;
  unsigned int                 hdrLen;
  COMPOUND_PKT_T               compoundLens[IXCODE_COMPOUND_FRAMES_MAX];

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  struct AUDIO_CONFERENCE     *pMixer;
  struct PARTICIPANT          *pMixerParticipant;
  IXCODE_VIDEO_CTXT_T         *pXcodeV;
  int                          vadLatest;
#endif // XCODE_HAVE_PIP_AUDIO

} IXCODE_AUDIO_CTXT_T;


typedef struct IXCODE_CTXT {
  IXCODE_VIDEO_CTXT_T        vid;
  IXCODE_AUDIO_CTXT_T        aud;
} IXCODE_CTXT_T;

enum IXCODE_RC {
  IXCODE_RC_ERROR_SCALE   = -4,
  IXCODE_RC_ERROR_DECODE  = -2,
  IXCODE_RC_ERROR_ENCODE  = -3,
  IXCODE_RC_ERROR_MIXER   = -4,
  IXCODE_RC_ERROR         = -1,
  IXCODE_RC_OK            = 0
};


int ixcode_init_vid(IXCODE_VIDEO_CTXT_T *pXcode);
int ixcode_init_aud(IXCODE_AUDIO_CTXT_T *pXcode);
void ixcode_close_vid(IXCODE_VIDEO_CTXT_T *pXcode);
void ixcode_close_aud(IXCODE_AUDIO_CTXT_T *pXcode);
enum IXCODE_RC ixcode_frame_vid(IXCODE_VIDEO_CTXT_T *pXcode, 
                                  unsigned char *bufIn, unsigned int lenIn,
                                  IXCODE_OUTBUF_T *pout);
enum IXCODE_RC ixcode_frame_aud(IXCODE_AUDIO_CTXT_T *pXcode, 
                                  unsigned char *bufIn, unsigned int lenIn,
                                  unsigned char *bufOut, unsigned int lenOut);


#endif // __IXCODE_H__
