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


#ifndef __XCODER_H__
#define __XCODER_H__

//#define DEBUG_XCODE_PIP 1
#if defined(DEBUG_XCODE_PIP) && (DEBUG_XCODE_PIP > 0)
#define PIPXLOG(fmt, args...) LOG(S_DEBUG, "PIPDBG "fmt"\n",##args)
#else // (DEBUG_XCODE_PIP) && (DEBUG_XCODE_PIP > 0)
#define PIPXLOG(x...) 
#endif // (DEBUG_XCODE_PIP) && (DEBUG_XCODE_PIP > 0)

//#define XCODE_PROFILE 1
#if defined(XCODE_PROFILE)
#define XCODE_PROFILE_VID XCODE_PROFILE 
#define XCODE_PROFILE_AUD XCODE_PROFILE
#include <sys/time.h>

typedef struct XCODER_PROFILE_DATA {
  unsigned int num_vdecodes;
  unsigned int num_vscales;
  unsigned int num_vfilters;
  unsigned int num_vrotate;
  unsigned int num_vencodes;
  unsigned int num_v;
  unsigned int num_blend;
  unsigned int num_adecodes;
  unsigned int num_aencodes;
  unsigned int num_a;
  uint64_t     tottime_vdecodes;
  uint64_t     tottime_vscales;
  uint64_t     tottime_vfilters;
  uint64_t     tottime_vrotate;
  uint64_t     tottime_vencodes;
  uint64_t     tottime_v;
  uint64_t     tottime_blend;
  uint64_t     tottime_adecodes;
  uint64_t     tottime_aencodes;
  uint64_t     tottime_a;
} XCODER_PROFILE_DATA_T;

#endif // XCODE_PROFILE

#include "xcodeconfig.h"
#include "libavcodec/avcodec.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "xcoder_wrap.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define ALIGN(x, a) (((x)+(a)-1)&~((a)-1))
#define ABS(x) ((x) >= 0 ? (x) : (-1 * (x)))

#define H264_PROFILE_BASELINE    66
#define H264_PROFILE_MAIN        77
#define H264_PROFILE_HIGH       100 

typedef struct IXCODE_AVCTXT_SCALE {
  struct SwsContext     *pswsctx;
  int                    forcescaler;
  unsigned int           scalertype;
  AVFrame               *pframeScaled;
  AVFrame               *pframeRotate;
  unsigned char         *pScaleDataBufPad;
  unsigned char         *pScaleDataBufCrop;
  unsigned char         *pScaleDataBufFilter;
  AVFrame                frameenc;

  int                    padTop;
  int                    padBottom;
  int                    padLeft;
  int                    padRight;
  unsigned char          padRGB[4];

  int                    cropTop;
  int                    cropBottom;
  int                    cropLeft;
  int                    cropRight;

  int                    numBlendPrior;

} IXCODE_AVCTXT_SCALE_T;

typedef struct VID_DIMENSIONS {
  int                    width;
  int                    height;
  int                    pix_fmt;
  int                    pic_sz;
} VID_DIMENSIONS_T;

#define SCALER_COUNT_MAX    (2 + (PIP_ADD_MAX - 1))  

typedef struct IXCODE_AVCTXT_OUT {
  VID_DIMENSIONS_T       dim_enc;
  VID_DIMENSIONS_T       dim_pips[PIP_ADD_MAX];
  int                    havepipFrameCntPrior;
  int                    pipFrameCntPrior;
  int                    pipFrameCnt;
  CODEC_WRAP_ENCODER_T   encWrap;
  AVFrame                framespip[PIP_ADD_MAX];
  IXCODE_AVCTXT_SCALE_T  scale[SCALER_COUNT_MAX];
  unsigned char         *pPipBufEncoderIn;
  void                  *pFilterUnsharpCtx;
  void                  *pFilterDenoiseCtx;
  void                  *pFilterColorCtx;
  void                  *pFilterRotateCtx;
  void                  *pFilterTestCtx;
  
  int                    setBorderColor;
  unsigned char          colorBorderRGB[4];
  unsigned char          colorBorderYUV[4];
} IXCODE_AVCTXT_OUT_T;

typedef struct IXCODE_AVCTXT_RESAMPLE {
  ReSampleContext       *presample;
  int                    needresample;
  unsigned char         *decbufresample;
  
  int                   *pin_codec;
  int                   *pin_samplerate;
  int                   *pin_samplefmt;
  int                   *pin_channels;

  int                   *pout_codec;
  int                   *pout_samplerate;
  int                   *pout_samplefmt;
  int                   *pout_channels;
  unsigned int          *pout_bps;

  int                    codec;
  int                    samplerate;
  int                    samplefmt;
  int                    channels;

} IXCODE_AVCTXT_RESAMPLE_T;

typedef struct IXCODE_AVCTXT {

  //
  // Video params
  //

  VID_DIMENSIONS_T       dim_in;

  CODEC_WRAP_DECODER_T   decWrap;
  int                    decIsImage;
  AVFrame                framedec[2];
  int                    framedecidx;
  int                    framedecidxok;

  IXCODE_AVCTXT_OUT_T    out[IXCODE_VIDEO_OUT_MAX];

  int                    pipposxCfg;
  int                    pipposyCfg;
  int                    pipposx0;
  int                    pipposy0;
  int                    pipposx;
  int                    pipposy;
  int                    piphavepos0;
  pthread_mutex_t       *pframepipmtx;
  unsigned char         *piprawDataBufs[PIP_ADD_MAX]; // YUV contents buffer

  //
  // Audio params
  //

  int                    enc_channels;
  int                    enc_samplerate;
  enum SampleFormat      enc_samplefmt;
  unsigned int           encbps;
  int                    volumeadjustment;
  int                    enc_fixed_framesize;

  int                    dec_channels;
  int                    dec_samplerate;
  enum SampleFormat      dec_samplefmt;
  unsigned int           decbps;

  IXCODE_AVCTXT_RESAMPLE_T resample[2];

  unsigned char         *decbufin;
  unsigned char         *decbufout;
  unsigned int           decbufoutlen;
  unsigned int           decbufinlen;
  unsigned int           decbufidxrd;
  unsigned int           decbufidxwr;
  unsigned int           encbpsout;

  unsigned int           audioFrameLen;
  int                    havePrevVidFrame;

#if defined(XCODE_PROFILE)
  XCODER_PROFILE_DATA_T  prof;
#endif // XCODE_PROFILE

} IXCODE_AVCTXT_T;


void xcode_fill_yuv420p(unsigned char *data[4], int linesize[4], unsigned char *p, unsigned int ht);


#endif // __XCODER_H___
