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


#ifdef WIN32

#include <windows.h>
#include "unixcompat.h"

#else // WIN32

#include <unistd.h>

#endif // WIN32

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "logutil.h"
#include "xcodeconfig.h"
#include "xcoder_pip.h"
#include "xcoder_libavcodec.h"
#include "xcoder.h"
#include "ixcode.h"
#include "libavcodec/avcodec.h"

#if defined(XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "libvpx.h"
#endif  // (XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)

#if defined(XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)
#include "libx264.h"
#endif // (XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)

#include "mixer/wav.h"
//static g_wavdec_init=0;
//static WAV_FILE_T g_wavdec;
//static g_wavdec2_init=0;
//static WAV_FILE_T g_wavdec2;


//#define MIN(x,y) ((x) < (y) ? (x) : (y))
//#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define FF_AC3_DECODER                 ff_ac3_decoder
#define FF_AC3_ENCODER                 ff_ac3_encoder
#define FF_AAC_DECODER                 ff_aac_decoder
#define FF_LIBFAAC_ENCODER             ff_libfaac_encoder
#define FF_MP2_DECODER                 ff_mp2_decoder
#define FF_MP3_DECODER                 ff_mp3_decoder
#define FF_AMRNB_DECODER               ff_amrnb_decoder
#define FF_VORBIS_DECODER              ff_vorbis_decoder
#define FF_LIBOPENCORE_AMRNB_ENCODER   ff_libopencore_amrnb_encoder
#define FF_PCM_MULAW_DECODER           ff_pcm_mulaw_decoder
#define FF_PCM_MULAW_ENCODER           ff_pcm_mulaw_encoder
#define FF_PCM_ALAW_DECODER            ff_pcm_alaw_decoder
#define FF_PCM_ALAW_ENCODER            ff_pcm_alaw_encoder
#define FF_MPEG2VIDEO_DECODER          ff_mpeg2video_decoder
#define FF_MPEG4_DECODER               ff_mpeg4_decoder
#define FF_MPEG4_ENCODER               ff_mpeg4_encoder
#define FF_H264_DECODER                ff_h264_decoder
#define FF_VP6_DECODER                 ff_vp6_decoder
#define FF_VP6F_DECODER                ff_vp6f_decoder
#define FF_VP8_DECODER                 ff_vp8_decoder
#define FF_H263_DECODER                ff_h263_decoder
#define FF_H263_ENCODER                ff_h263_encoder
#define FF_H263P_ENCODER               ff_h263p_encoder
#define FF_PNG_DECODER                 ff_png_decoder
#define FF_BMP_DECODER                 ff_bmp_decoder
//#define FF_VP8_ENCODER                 ff_libvpx_encoder
#define FF_LIBVORBIS_ENCODER           ff_libvorbis_encoder

extern AVCodec FF_AC3_DECODER;
extern AVCodec FF_AC3_ENCODER;
extern AVCodec FF_AAC_DECODER;
//extern AVCodec FF_LIBFAAC_ENCODER;
extern AVCodec FF_MP2_DECODER;
extern AVCodec FF_MP3_DECODER;
extern AVCodec FF_AMRNB_DECODER;
extern AVCodec FF_LIBOPENCORE_AMRNB_ENCODER;
extern AVCodec FF_VORBIS_DECODER;
extern AVCodec FF_LIBVORBIS_ENCODER;
extern AVCodec FF_PCM_MULAW_ENCODER;
extern AVCodec FF_PCM_MULAW_DECODER;
extern AVCodec FF_PCM_ALAW_ENCODER;
extern AVCodec FF_PCM_ALAW_DECODER;

extern AVCodec FF_MPEG2VIDEO_DECODER;
extern AVCodec FF_MPEG4_DECODER;
extern AVCodec FF_MPEG4_ENCODER;
extern AVCodec FF_H264_DECODER;
extern AVCodec FF_VP6_DECODER;
extern AVCodec FF_VP6F_DECODER;
extern AVCodec FF_VP8_DECODER;
extern AVCodec FF_H263_DECODER;
extern AVCodec FF_H263_ENCODER;
extern AVCodec FF_H263P_ENCODER;
extern AVCodec FF_PNG_DECODER;
extern AVCodec FF_BMP_DECODER;
//extern AVCodec FF_VP8_ENCODER;
extern AVCodec libvpxext_encoder;    // TODO: move to outside of libavcodec
extern AVCodec libx264ext_encoder;   // TODO: move to outside of libavcodec

//extern int avcodec_is_open(AVCodecContext *s);
#define avcodec_is_open(s) (!!(s)->internal)

typedef struct FFMPEG_AVCODEC_CTXT {
  AVCodec                  *pavcodec;
  AVCodecContext           *pavctx;
  int64_t                   pts;
} FFMPEG_AVCODEC_CTXT_T;


static int have_avcodec_init;

static void xcoder_audio_close(CODEC_WRAP_CTXT_T *pWrap) {
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;

  if(!pWrap || (!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt))) {
    return;
  }

  if(pFFCtx->pavctx) {
    //fprintf(stderr, "XCODER_AUDIO_LIBAVCODEC_FREE isopen:%d\n", avcodec_is_open(pFFCtx->pavctx));

    if(avcodec_is_open(pFFCtx->pavctx)) {
      avcodec_close(pFFCtx->pavctx);
    }
    av_free(pFFCtx->pavctx);
    pFFCtx->pavctx = NULL;
  }

  if(pWrap->pctxt) {
    free(pWrap->pctxt);
    pWrap->pctxt = NULL;
  }


}

void xcoder_libavcodec_audio_close_decoder(CODEC_WRAP_CTXT_T *pWrap) {

  if(!pWrap) {
    return;
  }

  xcoder_audio_close(pWrap);

}

void xcoder_libavcodec_audio_close_encoder(CODEC_WRAP_CTXT_T *pWrap) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;

  if(!pWrap) {
    return;
  }

  pXcode = pWrap->pXcode;

  xcoder_audio_close(pWrap);

  if(pXcode) {
    if(pXcode->phdr) {
      av_free(pXcode->phdr);
      pXcode->phdr = NULL;
    }
    pXcode->hdrLen = 0;
    pWrap->pXcode = NULL;
  }

}

int xcoder_libavcodec_audio_init_decoder(CODEC_WRAP_CTXT_T *pCtxt) {
  int rc = 0;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  CODEC_WRAP_CTXT_T *pWrap = NULL;
  CODEC_WRAP_CTXT_T *pWrapEnc = NULL;
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  AVCodec *pavcodec = NULL;
  unsigned int outidx = 0;
  //int codecId = CODEC_ID_NONE;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  //TODO: this is redundant w/ pCtxt
  pWrap = &pCtxt->pAVCtxt->decWrap.u.a.ctxt;
  pWrapEnc = &pCtxt->pAVCtxt->out[outidx].encWrap.u.a.ctxt;
  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(((FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    // Already been initialized
    return -1;
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    pWrap->pctxt = calloc(1, sizeof(FFMPEG_AVCODEC_CTXT_T));
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    return -1;
  }

  switch(pXcode->common.cfgFileTypeIn) {

#if defined(XCODE_HAVE_AC3) && (XCODE_HAVE_AC3 > 0)
    case XC_CODEC_TYPE_AC3:
      pavcodec = &FF_AC3_DECODER; 
      //codecId = CODEC_ID_AC3;
      break;
#endif // (XCODE_HAVE_AC3) && (XCODE_HAVE_AC3 > 0)

#if defined(XCODE_HAVE_AAC) && (XCODE_HAVE_AAC > 0)
    case XC_CODEC_TYPE_AAC:
      pavcodec = &FF_AAC_DECODER;
      //codecId = CODEC_ID_AAC;
      break;
#endif // (XCODE_HAVE_AAC) && (XCODE_HAVE_AAC > 0)

#if defined(XCODE_HAVE_VORBIS) && (XCODE_HAVE_VORBIS > 0)
    case XC_CODEC_TYPE_VORBIS:
      pavcodec = &FF_VORBIS_DECODER; 
      //codecId = CODEC_ID_VORBIS;
      break;
#endif // (XCODE_HAVE_VORBIS) && (XCODE_HAVE_VORBIS > 0)

#if defined(XCODE_HAVE_AMR) && (XCODE_HAVE_AMR > 0)
    case XC_CODEC_TYPE_AMRNB:
      pavcodec = &FF_AMRNB_DECODER; 
      //codecId = CODEC_ID_AMNR_NB;
      break;
#endif // (XCODE_HAVE_AMR) (XCODE_HAVE_AMR > 0)

    case XC_CODEC_TYPE_MPEGA_L2:
      pavcodec = &FF_MP2_DECODER;
      break;
    case XC_CODEC_TYPE_MPEGA_L3:
      pavcodec = &FF_MP3_DECODER;
      break;
    case XC_CODEC_TYPE_RAWA_PCM16LE:
      //memset(&codecDec, 0, sizeof(codecDec));
      break;
    case XC_CODEC_TYPE_AUD_CONFERENCE:
      pCtxt->pAVCtxt->dec_channels = pXcode->cfgChannelsIn;
      pCtxt->pAVCtxt->dec_samplerate = pXcode->cfgSampleRateIn;
      pCtxt->pAVCtxt->decbps = 2;
      break;
      case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_RAWA_PCMULAW:
      pavcodec = &FF_PCM_MULAW_DECODER;
      break;
      case XC_CODEC_TYPE_G711_ALAW:
    case XC_CODEC_TYPE_RAWA_PCMALAW:
      pavcodec = &FF_PCM_ALAW_DECODER;
      break;
    default:
      LOG(X_ERROR("Unsupported xcoder input audio codec %d"), pXcode->common.cfgFileTypeIn);
      return -1;
  }

  if(!have_avcodec_init) {
    avcodec_init();
    have_avcodec_init = 1;
  }

  if(!(pFFCtx->pavcodec = pavcodec)) {
    return 0;
  }
  if(NULL == (pFFCtx->pavctx = avcodec_alloc_context3(pFFCtx->pavcodec))) {
    return -1;
  }

  //pFFCtx->pavctx->codec_id = codecId;
  pFFCtx->pavctx->channels = pCtxt->pAVCtxt->dec_channels = pXcode->cfgChannelsIn;
  pFFCtx->pavctx->sample_rate = pCtxt->pAVCtxt->dec_samplerate = pXcode->cfgSampleRateIn;
  pCtxt->pAVCtxt->dec_samplefmt = SAMPLE_FMT_S16;
  //pFFCtx->pavctx->time_base = (AVRational) { 1, pFFCtx->pavctx->sample_rate };
  pCtxt->pAVCtxt->decbps = 2;

  if(pFFCtx->pavctx->sample_rate == 0 && pWrapEnc->pctxt && 
                                        ((FFMPEG_AVCODEC_CTXT_T *) pWrapEnc->pctxt)->pavctx) {
    pFFCtx->pavctx->sample_rate = ((FFMPEG_AVCODEC_CTXT_T *) pWrapEnc->pctxt)->pavctx->sample_rate;
  }


  switch(pXcode->common.cfgFileTypeIn) {
    case XC_CODEC_TYPE_AMRNB:
      if(pFFCtx->pavctx->frame_size == 0) {
        pFFCtx->pavctx->frame_size = 160;
      }
      pFFCtx->pavctx->bit_rate = 12800;
      pCtxt->pAVCtxt->decbps = 4;    // float sample format
      break;
    case XC_CODEC_TYPE_AAC:
      if(pFFCtx->pavctx->frame_size == 0) {
        pFFCtx->pavctx->frame_size = 1024;
      }
      pFFCtx->pavctx->bit_rate = 64000 * pFFCtx->pavctx->channels;
      break;
    case XC_CODEC_TYPE_VORBIS:
      if(pXcode->extradatasize) {
        pFFCtx->pavctx->extradata_size = pXcode->extradatasize;
        pFFCtx->pavctx->extradata = pXcode->pextradata;
      }
      break;
    default:
      pFFCtx->pavctx->bit_rate = 64000 * pFFCtx->pavctx->channels;
      break;
  }

  if(avcodec_open(pFFCtx->pavctx, pFFCtx->pavcodec) < 0) {
    LOG(X_ERROR("Failed to open audio decoder input codec"));
    return -1;
  } else {
    pCtxt->pAVCtxt->dec_samplefmt = pFFCtx->pavctx->sample_fmt;

    LOG(X_DEBUG("Initialized audio decoder %dHz, %dKb, channels:%d(%d)"),
        pFFCtx->pavctx->sample_rate, pFFCtx->pavctx->bit_rate/1000,
        pFFCtx->pavctx->channels, pFFCtx->pavctx->request_channels);

  }

  return rc;
}

int xcoder_libavcodec_audio_init_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  int rc = 0;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  CODEC_WRAP_CTXT_T *pWrap = NULL;
  CODEC_WRAP_CTXT_T *pWrapDec = NULL;
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  AVCodec *pavcodec = NULL;
  int codecId = CODEC_ID_NONE;
  unsigned int outidx = 0;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pWrap = &pCtxt->pAVCtxt->out[outidx].encWrap.u.a.ctxt;
  pWrapDec = &pCtxt->pAVCtxt->decWrap.u.a.ctxt;
  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(((FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    // Already been initialized
    return -1;
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    pWrap->pctxt = calloc(1, sizeof(FFMPEG_AVCODEC_CTXT_T));
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    return -1;
  }

  switch(pXcode->common.cfgFileTypeOut) {

#if defined(XCODE_HAVE_AC3) && (XCODE_HAVE_AC3 > 0)
    case XC_CODEC_TYPE_AC3:
      pavcodec = &FF_AC3_ENCODER; 
      codecId = CODEC_ID_AC3;
      break;
#endif // (XCODE_HAVE_AC3) && (XCODE_HAVE_AC3 > 0)

//#if defined(XCODE_HAVE_FAAC)
//    case XC_CODEC_TYPE_AAC:
//      pavcodec = &FF_LIBFAAC_ENCODER; 
//      codecId = CODEC_ID_AAC;
//      break;
//#endif // XCODE_HAVE_FAAC

#if defined(XCODE_HAVE_VORBIS) && (XCODE_HAVE_VORBIS > 0)
    case XC_CODEC_TYPE_VORBIS:
      pavcodec = &FF_LIBVORBIS_ENCODER; 
      codecId = CODEC_ID_VORBIS;
      break;
#endif // (XCODE_HAVE_VORBIS) && (XCODE_HAVE_VORBIS > 0)

#if defined(XCODE_HAVE_OPENCOREAMR) && (XCODE_HAVE_OPENCOREAMR > 0)
    case XC_CODEC_TYPE_AMRNB:
      pavcodec = &FF_LIBOPENCORE_AMRNBD_ENCODER; 
      codecId = CODEC_ID_AMNR_NB;
      break;
#endif // (XCODE_HAVE_OPENCOREAMR) && (XCODE_HAVE_OPENCOREAMR > 0)

   case XC_CODEC_TYPE_RAWA_PCM16LE:
      codecId = CODEC_ID_NONE;
      break;

    case XC_CODEC_TYPE_RAWA_PCMULAW:
    case XC_CODEC_TYPE_G711_MULAW:
      pavcodec = &FF_PCM_MULAW_ENCODER; 
      codecId = CODEC_ID_PCM_MULAW;
      break;

    case XC_CODEC_TYPE_RAWA_PCMALAW:
    case XC_CODEC_TYPE_G711_ALAW:
      pavcodec = &FF_PCM_ALAW_ENCODER; 
      codecId = CODEC_ID_PCM_ALAW;
      break;
   
    default:
      LOG(X_ERROR("Unsupported xcoder output audio codec %d"), pXcode->common.cfgFileTypeOut);
      return -1;
  }

  if(!have_avcodec_init) {
    avcodec_init();
    have_avcodec_init = 1;
  }

  if(!(pFFCtx->pavcodec = pavcodec)) {
    // CODEC_ID_NONE
     pCtxt->pAVCtxt->enc_channels = pXcode->cfgChannelsOut;
     pCtxt->pAVCtxt->enc_samplerate = pXcode->cfgSampleRateOut;
    return 0;
  }
  if(NULL == (pFFCtx->pavctx = avcodec_alloc_context3(pFFCtx->pavcodec))) {
    return -1;
  }

  pFFCtx->pavctx->codec_id = codecId;
  pFFCtx->pavctx->bit_rate = pXcode->cfgBitRateOut;
  pFFCtx->pavctx->channels = pCtxt->pAVCtxt->enc_channels = pXcode->cfgChannelsOut;
  pFFCtx->pavctx->sample_rate = pCtxt->pAVCtxt->enc_samplerate = pXcode->cfgSampleRateOut;
  pFFCtx->pavctx->time_base = (AVRational) { 1, pFFCtx->pavctx->sample_rate };
  pFFCtx->pavctx->sample_fmt = pCtxt->pAVCtxt->enc_samplefmt = SAMPLE_FMT_S16;
  pCtxt->pAVCtxt->encbps = 2;
  pFFCtx->pts = 0;

  //pCtxt->pAVCtxt->volumeadjustment = pXcode->cfgVolumeAdjustment;

  if(pFFCtx->pavctx->sample_rate == 0 && pWrapDec->pctxt && 
    ((FFMPEG_AVCODEC_CTXT_T *) pWrapDec->pctxt)->pavctx) {
    pFFCtx->pavctx->sample_rate = ((FFMPEG_AVCODEC_CTXT_T *) pWrapDec->pctxt)->pavctx->sample_rate;
  }

  switch(pXcode->common.cfgFileTypeOut) {
    case XC_CODEC_TYPE_AAC:
      //pFFCtx->pavctx->profile = FF_PROFILE_AAC_MAIN;
      pFFCtx->pavctx->profile = FF_PROFILE_AAC_LOW;
      break;
    case XC_CODEC_TYPE_RAWA_PCMALAW:
    case XC_CODEC_TYPE_RAWA_PCMULAW:
    case XC_CODEC_TYPE_G711_ALAW:
    case XC_CODEC_TYPE_G711_MULAW:
      pCtxt->pAVCtxt->enc_fixed_framesize = 1;
      pCtxt->pAVCtxt->encbpsout = 1;
      break;
    case XC_CODEC_TYPE_RAWA_PCM16LE:
      pCtxt->pAVCtxt->encbpsout = 2;
      break;
    default:
      break;
  }

  if(avcodec_open(pFFCtx->pavctx, pFFCtx->pavcodec) < 0) {
    LOG(X_ERROR("Failed to open audio encoder output codec (%dHz, %dKb channels:%d)"),
      pFFCtx->pavctx->sample_rate, pFFCtx->pavctx->bit_rate/1000, pFFCtx->pavctx->channels);
    return -1;
  } else {
    pCtxt->pAVCtxt->enc_samplefmt = pFFCtx->pavctx->sample_fmt;

    if(pFFCtx->pavctx->extradata_size > 0xffff) {
      LOG(X_ERROR("audio encoder extra data size %d > %d"), pFFCtx->pavctx->extradata_size, 0xffff);
      return -1;
    } else if(pFFCtx->pavctx->extradata_size > 0) {
      if(!(pXcode->phdr = av_malloc(pFFCtx->pavctx->extradata_size))) {
        return -1;
      } else {
        memcpy(pXcode->phdr, pFFCtx->pavctx->extradata, pFFCtx->pavctx->extradata_size);
        pXcode->hdrLen = pFFCtx->pavctx->extradata_size;
      }
    }
  }

  pXcode->encoderSamplesInFrame = pFFCtx->pavctx->frame_size;

  LOG(X_DEBUG("Initialized audio encoder %dHz, %dKb, channels:%d"),
      pFFCtx->pavctx->sample_rate, pFFCtx->pavctx->bit_rate/1000, pFFCtx->pavctx->channels);

  return rc;
}

int xcoder_libavcodec_audio_encode(CODEC_WRAP_CTXT_T *pCtxt,
                                   unsigned char *out_packets,
                                   unsigned int out_sz,
                                   const int16_t *samples,
                                   int64_t *pts) {
  int ret;
  CODEC_WRAP_CTXT_T *pWrap = NULL;
  CODEC_WRAP_CTXT_T *pWrapDec = NULL;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  FFMPEG_AVCODEC_CTXT_T *pFFCtxDec = NULL;
  unsigned int outidx = 0;
  int64_t pts0;

  if(!pCtxt || !(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt) || !pFFCtx->pavctx) {
    return -1;
  }

  pWrap =  &pCtxt->pAVCtxt->out[outidx].encWrap.u.a.ctxt;
  pWrapDec =  &pCtxt->pAVCtxt->decWrap.u.a.ctxt;

  if(pCtxt->pAVCtxt->enc_fixed_framesize == 0 &&
     pFFCtx->pavctx->frame_size == 0 && (pFFCtxDec = ((FFMPEG_AVCODEC_CTXT_T *) pWrapDec->pctxt)) && 
     pFFCtxDec->pavctx && pFFCtxDec->pavctx->frame_size > 0) {
      LOG(X_DEBUG("Setting audio enc frame size to decoder size %d -> %d"),
              pFFCtx->pavctx->frame_size,  pFFCtxDec->pavctx->frame_size);
      pFFCtx->pavctx->frame_size = pFFCtxDec->pavctx->frame_size;
  }

  pts0 = pFFCtx->pavctx->coded_frame->pts;

  ret = avcodec_encode_audio(pFFCtx->pavctx, out_packets, out_sz, samples);

  //the pts may not be set for all codec implementations... it should be set for vorbis
  if(ret > 0 && pts) {
    if(pFFCtx->pavctx->coded_frame->pts - pts0 == 0) {
      pFFCtx->pts += pFFCtx->pavctx->frame_size;
      *pts = pFFCtx->pts;
      //fprintf(stderr, "audio PTS from auto increment %lld by %d\n", pts, pFFCtx->pavctx->frame_size);
    } else {
      pFFCtx->pts = *pts = pFFCtx->pavctx->coded_frame->pts;
      //fprintf(stderr, "audio PTS from coded_frame %lld\n", pFFCtx->pavctx->coded_frame->pts);
    }
  }

  return ret;
}


int xcoder_libavcodec_audio_decode(CODEC_WRAP_CTXT_T *pCtxt, 
                                   const unsigned char *frame,
                                   unsigned int frame_sz,
                                   int16_t *out_samples,
                                   unsigned int out_bytes) {
  int ret;
  AVPacket avpkt;
  CODEC_WRAP_CTXT_T *pWrap = NULL;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  unsigned int inIdx = 0;
  unsigned int outIdx = 0;
  int frame_size = 0;

  if(!pCtxt || !(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt) || !pFFCtx->pavctx) {
    return -1;
  }

  pWrap = (CODEC_WRAP_CTXT_T *) &pCtxt->pAVCtxt->decWrap.u.a.ctxt;

  if(pFFCtx->pavctx->frame_size == 0) {
    pFFCtx->pavctx->frame_size = frame_sz;
  }

  do {

    if(outIdx + frame_size > out_bytes) {
      LOG(X_ERROR("Insufficient audio output buffer size: [%d]/%d"), outIdx, out_bytes);
      break;
    }

    frame_size = out_bytes;

    av_init_packet(&avpkt);
    avpkt.data = (unsigned char *) &frame[inIdx];
    avpkt.size = frame_sz - inIdx;

    if((ret = avcodec_decode_audio3(pFFCtx->pavctx, &((unsigned char *)out_samples)[outIdx], &frame_size, &avpkt)) < 0) {
      return -1;
    } else if(ret == 0) {
      break;
    }

    //LOG(X_DEBUG("avcodec_decode_audio3 ret:%d, frame_size:%d into [%d]/%d -> [%d]/%d"), ret, frame_size, inIdx, frame_sz, outIdx, out_bytes); 

    inIdx += ret; 
    if((ret = frame_size) < 0) {
      return -1;
    } 

    //if(!g_wavdec_init) { g_wavdec_init=1; memset(&g_wavdec, 0, sizeof(g_wavdec)); g_wavdec.path="decoded.wav"; g_wavdec.channels=2; g_wavdec.sampleRate=48000; }
    //wav_write(&g_wavdec, (const int16_t *) &out_samples[outIdx], ret/2);

    outIdx += ret;

  } while(inIdx < frame_sz);

    //if(!g_wavdec2_init) { g_wavdec2_init=1; memset(&g_wavdec2, 0, sizeof(g_wavdec2)); g_wavdec2.path="decoded2.wav"; g_wavdec2.channels=2; g_wavdec2.sampleRate=48000; }
    //wav_write(&g_wavdec2, (const int16_t *) out_samples, outIdx/2);

  if(pFFCtx->pavctx->sample_rate != pCtxt->pAVCtxt->dec_samplerate) {
    pCtxt->pAVCtxt->dec_samplerate = pFFCtx->pavctx->sample_rate;
  }
  if(pFFCtx->pavctx->channels != pCtxt->pAVCtxt->dec_channels) {
    pCtxt->pAVCtxt->dec_channels = pFFCtx->pavctx->channels;
  }
  if(pFFCtx->pavctx->sample_fmt != pCtxt->pAVCtxt->dec_samplefmt) {
    pCtxt->pAVCtxt->dec_samplefmt = pFFCtx->pavctx->sample_fmt;
  }

  // bytes output
  return outIdx;
}




static void xcoder_video_close(CODEC_WRAP_CTXT_T *pWrap) {
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;

  if(!pWrap || (!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt))) {
    return;
  }

  if(pFFCtx->pavctx) {

    if(avcodec_is_open(pFFCtx->pavctx)) {
      avcodec_close(pFFCtx->pavctx);
    }

    if(pFFCtx->pavctx->priv_data) {
      av_free(pFFCtx->pavctx->priv_data);
      pFFCtx->pavctx->priv_data = NULL;
    }

    av_free(pFFCtx->pavctx);
    pFFCtx->pavctx = NULL;
  }

  if(pWrap->pctxt) {
    free(pWrap->pctxt);
    pWrap->pctxt = NULL;
  }

}

void xcoder_libavcodec_video_close_decoder(CODEC_WRAP_CTXT_T *pWrap) {

  if(!pWrap) {
    return;
  }

  xcoder_video_close(pWrap);

}

void xcoder_libavcodec_video_close_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;

  if(pCtxt) {
    pXcode = pCtxt->pXcode;
  }

  xcoder_video_close(pCtxt);

/*
  if(pXcode) {
    if(pXcode->phdr) {
      av_free(pXcode->phdr);
      pXcode->phdr = NULL;
    }
    pXcode->hdrLen = 0;
  }
*/
}

int xcoder_libavcodec_video_init_decoder(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx) {
  int rc = 0;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  CODEC_WRAP_CTXT_T *pWrap = NULL;
  IXCODE_VIDEO_CTXT_T *pXcode = NULL;
  AVCodec *pavcodec = NULL;
  int thread_count = 0;
  enum PixelFormat dec_pix_fmt = PIX_FMT_YUV420P;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pWrap = &pCtxt->pAVCtxt->decWrap.u.v.ctxt;
  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  if(((FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    // Already been initialized
    return -1;
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    pWrap->pctxt = calloc(1, sizeof(FFMPEG_AVCODEC_CTXT_T));
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pWrap->pctxt)) {
    return -1;
  }

  thread_count = pXcode->cfgThreadsDec;
 
  switch(pXcode->common.cfgFileTypeIn) {
    case XC_CODEC_TYPE_H262:
      pavcodec = &FF_MPEG2VIDEO_DECODER;
      break;
    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
      pavcodec = &FF_H263_DECODER;
      thread_count = 0;
      break;
    case XC_CODEC_TYPE_MPEG4V:
      pavcodec = &FF_MPEG4_DECODER;
      thread_count = 0;
      break;
    case XC_CODEC_TYPE_H264:
      pavcodec = &FF_H264_DECODER;
      //if(thread_count == 0) {
      //  thread_count = 4;
      //}
      break;

#if defined(XCODE_HAVE_VP6) && (XCODE_HAVE_VP6 > 0)
    case XC_CODEC_TYPE_VP6:
      pavcodec = &FF_VP6_DECODER;
      break;
#endif // (XCODE_HAVE_VP6) && (XCODE_HAVE_VP6 > 0)

#if defined(XCODE_HAVE_VP6F) && (XCODE_HAVE_VP6F > 0)
    case XC_CODEC_TYPE_VP6F:
      pavcodec = &FF_VP6F_DECODER;
      break;
#endif // (XCODE_HAVE_VP6F) && (XCODE_HAVE_VP6F > 0)

#if defined(XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)
    case XC_CODEC_TYPE_VP8:
      pavcodec = &FF_VP8_DECODER;
      break;
#endif // (XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)

#if defined(XCODE_HAVE_PNG) && (XCODE_HAVE_PNG > 0)
    case XC_CODEC_TYPE_PNG:
      pavcodec = &FF_PNG_DECODER;
      pCtxt->pAVCtxt->decIsImage = 1;
      if(pXcode->pip.active) {
        pXcode->pip.haveAlpha = 1;
      }
      thread_count = 0;
      break;
#endif // (XCODE_HAVE_PNG) && (XCODE_HAVE_PNG > 0)

    case XC_CODEC_TYPE_BMP:
      pavcodec = &FF_BMP_DECODER;
      pCtxt->pAVCtxt->decIsImage = 1;
      thread_count = 0;
      break;

    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_RGBA32:
    case XC_CODEC_TYPE_RAWV_BGR24:
    case XC_CODEC_TYPE_RAWV_RGB24:
    case XC_CODEC_TYPE_RAWV_BGR565:
    case XC_CODEC_TYPE_RAWV_RGB565:
    case XC_CODEC_TYPE_RAWV_YUV420P:
    case XC_CODEC_TYPE_RAWV_YUVA420P:
    case XC_CODEC_TYPE_RAWV_NV21:
    case XC_CODEC_TYPE_RAWV_NV12:
    case XC_CODEC_TYPE_VID_CONFERENCE:

      if(pXcode->inH <= 0 || pXcode->inV <= 0) {
        LOG(X_ERROR("Raw Input dimensions %dx%d not given"), pXcode->inH, pXcode->inV);
        return -1;
      }

      if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_YUVA420P) {
        dec_pix_fmt = PIX_FMT_YUVA420P;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_NV21) {
        dec_pix_fmt = PIX_FMT_NV21;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_NV12) {
        dec_pix_fmt = PIX_FMT_NV12;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_BGRA32) {
        dec_pix_fmt = PIX_FMT_BGRA;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_RGBA32) {
        dec_pix_fmt = PIX_FMT_RGBA;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_BGR24) {
        dec_pix_fmt = PIX_FMT_BGR24;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_RGB24) {
        dec_pix_fmt = PIX_FMT_RGB24;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_BGR565) {
        dec_pix_fmt = PIX_FMT_BGR565LE;
      } else if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_RAWV_RGB565) {
        dec_pix_fmt = PIX_FMT_RGB565LE;
      }

      pCtxt->pAVCtxt->dim_in.pic_sz = avpicture_get_size(dec_pix_fmt, pXcode->inH, pXcode->inV);
      thread_count = 0;

      break;

    default:
      LOG(X_ERROR("Unsupported xcoder input video codec %d"), pXcode->common.cfgFileTypeIn);
      return -1;
  }

  if(!have_avcodec_init) {
    avcodec_init();
    have_avcodec_init = 1;
  }

  pFFCtx->pavcodec = pavcodec;
  pCtxt->pAVCtxt->dim_in.width = pXcode->inH;
  pCtxt->pAVCtxt->dim_in.height = pXcode->inV;
  pCtxt->pAVCtxt->dim_in.pix_fmt = dec_pix_fmt;

  if(pavcodec) {
  
    if(NULL == (pFFCtx->pavctx = avcodec_alloc_context3(pavcodec))) {
      return -1;
    }
    pFFCtx->pavctx->pix_fmt = dec_pix_fmt;
    pFFCtx->pavctx->sample_aspect_ratio = (AVRational) { 1, 1 };
    if(pXcode->extradatasize) {
      pFFCtx->pavctx->extradata_size = pXcode->extradatasize;
      pFFCtx->pavctx->extradata = pXcode->extradata;
    }
    if(thread_count > 0) {
      LOG(X_DEBUG("Set video decoder threads from %d to %d"), pFFCtx->pavctx->thread_count, thread_count);
      pFFCtx->pavctx->thread_count = thread_count;
    }

    if(avcodec_open(pFFCtx->pavctx, pavcodec) < 0) {
      LOG(X_ERROR("Failed to open video decoder input codec"));
      return -1;
    }

  } 

  //fprintf(stderr, "xcoder_libavcodec_video_init_decoder[%d] pavcodec:0x%x opened\n", outidx, pavcodec);

  return rc;
}

#if defined(XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)
//TODO: this has been deprecated in favor of xcoder_x264.c
static int init_x264params(AVCodecContext *pavctx,
                           const IXCODE_VIDEO_OUT_T *pXcode) {

  int profile = pXcode->cfgProfile;

  switch(profile) {
    case H264_PROFILE_BASELINE:
    case H264_PROFILE_MAIN:
    case H264_PROFILE_HIGH:
      break;
    default:
      profile = H264_PROFILE_HIGH;
  }

  if(pXcode->resLookaheadmin1 > 0) {
    ((X264Context *) pavctx->priv_data)->lookaheadmin1 = pXcode->resLookaheadmin1;
  }
  if(pXcode->cfgMaxSliceSz > 0) {
    ((X264Context *) pavctx->priv_data)->i_slice_max_size = pXcode->cfgMaxSliceSz;
  }

  // preset "flags=+loop"
  pavctx->flags = CODEC_FLAG_LOOP_FILTER;

  if(pXcode->cfgFast < ENCODER_QUALITY_LOW) {
    // prset "cmp=+chroma"  // use chroma plane for m.e.
    pavctx->me_cmp = FF_CMP_CHROMA;
  }

  if(profile == H264_PROFILE_BASELINE || pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    // "coder=0" (CAVLC)
    pavctx->coder_type = FF_CODER_TYPE_VLC;
  } else {
    // preset "coder=1" (CABAC)
    pavctx->coder_type = FF_CODER_TYPE_AC;
  }

  // preset "partitions=+parti8x8+parti4x4+partp8x8+partb8x8
  if(pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pavctx->partitions = X264_PART_I8X8 | X264_PART_I4X4 | X264_PART_P8X8 |
                         X264_PART_B8X8 | X264_PART_P4X4;
  }

  // preset "me_method="
  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pavctx->me_method = ME_EPZS;  // X264_ME_DIA
  } else if(pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    pavctx->me_method = ME_HEX;  // X264_ME_HEX
  } else if(pXcode->cfgFast == ENCODER_QUALITY_HIGH) {
    pavctx->me_method = ME_HEX;   // X264_ME_HEX
  } else if(pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pavctx->me_method = ME_FULL;   // X264_ME_ESA_
  }
/*
  fps test:
  esa=full 81,74  ME_FULL (ESA)
  epzs 89,85  ME_EPZS (DIA)
  hex 88,85 ME_HEX
  umh  87, 86  ME_UMH
  tesa 72-65  ME_TESA
  full 80-74
*/


  //
  // preset "subq="
  //  0. fullpel only
  //  1. QPel SAD 1 iteration
  //  2. QPel SATD 2 iterations
  //  3. HPel on MB then QPel
  //  4. Always QPel
  //  5. Multi QPel + bi-directional motion estimation
  //  6. RD on I/P frames
  //  7. RD on all frames
  //  8. RD refinement on I/P frames
  //  9. RD refinement on all frames
  //  10. QP-RD (requires --trellis=2, --aq-mode > 0)
  //  11. Full RD [1][2] j
  //
  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pavctx->me_subpel_quality = 0;
  } else if(pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    pavctx->me_subpel_quality = 2;
  } else if(pXcode->cfgFast == ENCODER_QUALITY_HIGH) {
    pavctx->me_subpel_quality = 4;
  } else {
    pavctx->me_subpel_quality = 6;
  }

  // preset "me_range="
  pavctx->me_range = 16;

  // preset "g="
  if(pXcode->cfgGopMaxMs != -1) {
    pavctx->gop_size = 150;
  } else {
    pavctx->gop_size=X264_KEYINT_MAX_INFINITE;
  }

  // preset "keyint_min="
  pavctx->keyint_min = 25;

  // preset "sc_threshold="
  if(pXcode->cfgSceneCut != 0) {
    pavctx->scenechange_threshold = pXcode->cfgSceneCut;
  } else {
    pavctx->scenechange_threshold = 40;
  }

  // preset "i_qfactor="
  pavctx->i_quant_factor = .71f;
// preset "qcomp="
  pavctx->qcompress = .6f;

  // preset "qmax="
  if(pXcode->cfgQMax >= 10 && pXcode->cfgQMax <= 51) {
    pavctx->qmax = pXcode->cfgQMax;
  } else {
    pavctx->qmax = 51;
  }

  // preset "qmin="
  if(pXcode->cfgQMin >= 10 && pXcode->cfgQMin <= 51) {
    pavctx->qmin = pXcode->cfgQMin;
  } else {
    pavctx->qmin = 10;
  }

  // preset "qmax_diff="
  if(pXcode->cfgQDiff > 0) {
    pavctx->max_qdiff = pXcode->cfgQDiff;
  } else {
    pavctx->max_qdiff = 4;
  }

  if(profile >= H264_PROFILE_MAIN) {
    // preset "b_strategy="
    pavctx->b_frame_strategy = 1;

    // preset "max_b_frames="
    pavctx->max_b_frames = 3;
  } else {
    pavctx->max_b_frames = 0;
  }

  // preset "refs="
  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pavctx->refs = 1;
  } else if(pXcode->cfgFast > ENCODER_QUALITY_BEST) {
    pavctx->refs = 2;
  } else {
    pavctx->refs = 3;
  }

  switch(pXcode->cfgRateCtrl) {
    case RATE_CTRL_TYPE_CQP:
      pavctx->cqp = pXcode->cfgQ;
      break;
    case RATE_CTRL_TYPE_CRF:
      pavctx->crf = pXcode->cfgQ;
      break;
    case RATE_CTRL_TYPE_BITRATE:
    default:
      break;
  }


  if(pXcode->cfgIQRatio > 0) {
    pavctx->i_quant_factor = pXcode->cfgIQRatio;
  }
  if(pXcode->cfgBQRatio > 0) {
    pavctx->b_quant_factor = pXcode->cfgBQRatio;
  }

  LOG(X_DEBUG("H.264 rate ctrl %s %.1f, I:%.3f B:%.3f, Qrange %d - %d, "
              "VBV size: %dKb, max: %dKb/s, min: %dKb/s, factor:%.2f, I scene threshold:%d, slice-max:%d"),
    pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CQP ? "cqp" :
    (pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CRF ? "crf" : "btrt"),
    ((pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CQP ||
      pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CRF) ? (float)pXcode->cfgQ : pXcode->cfgBitRateOut/1000) ,
    pavctx->i_quant_factor, pavctx->b_quant_factor, pavctx->qmin, pavctx->qmax,
    pXcode->cfgVBVBufSize/1000, pXcode->cfgVBVMaxRate/1000, pXcode->cfgVBVMinRate/1000,
    pXcode->cfgVBVAggressivity, pavctx->scenechange_threshold, pXcode->cfgMaxSliceSz);

  //pavctx->rc_buffer_size = pXcode->cfgBitRateOut * 1;
  //pavctx->rc_max_rate = pXcode->cfgBitRateOut * 1.5;
  //fprintf(stderr, "VBV_BUFFER_SIZE:%d  VBV_MAX_BITRATE:%d\n", pavctx->rc_buffer_size / 1000, pavctx->rc_max_rate    / 1000);

  // preset "directpred="
  pavctx->directpred = 3;

  // preset "flags2="
  pavctx->flags2 = CODEC_FLAG2_FASTPSKIP;
  if(pXcode->resLookaheadmin1 != 1) {
    pavctx->flags2 |= CODEC_FLAG2_MBTREE;
  }

  if(profile == H264_PROFILE_HIGH && pXcode->cfgFast <= ENCODER_QUALITY_HIGH) {
    pavctx->flags2 |= CODEC_FLAG2_WPRED |
                      CODEC_FLAG2_8X8DCT;
  }
  if(profile >= H264_PROFILE_MAIN && pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pavctx->flags2 |= CODEC_FLAG2_BPYRAMID |
                      CODEC_FLAG2_MIXED_REFS;
  }

  if(profile >= H264_PROFILE_MAIN) {
    // preset "wpredp="
    pavctx->weighted_p_pred = 2;
  }

  //fprintf(stderr, "pavctx:0x%x lookahead:%d, i_slice_max_size:%d, flags:0x%x, 0x%x, me_cmp:%d, coder_type:0x%x partitions:0x%x, me_method:0x%x, subpel_quality:%d, qmax:%d, refs:%d, level:%d\n", pavctx, pXcode->resLookaheadmin1, pXcode->cfgMaxSliceSz, pavctx->flags, pavctx->flags2, pavctx->me_cmp, pavctx->coder_type, pavctx->partitions, pavctx->me_method, pavctx->me_subpel_quality, pavctx->qmax, pavctx->refs, pavctx->level);

  pavctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
  pavctx->flags2 |= CODEC_FLAG2_BIT_RESERVOIR;
  //pavctx->use_lpc = 0;
  pavctx->min_prediction_order = 0;
  pavctx->max_prediction_order = 0;
  pavctx->prediction_order_method = 0;
  pavctx->min_partition_order = 0;
  pavctx->max_partition_order = 0;
  pavctx->trellis = 0;
  //pavctx->ildct_cmp = FF_CMP_VSAD;

  return 0;
}
#endif // (XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)

static int init_mpeg4vparams(AVCodecContext *pavctx, IXCODE_VIDEO_OUT_T *pXcode) {

  pavctx->profile = pXcode->cfgProfile >> 4;
  pavctx->level = pXcode->cfgProfile & 0x0f;

  if(pavctx->profile == 15) { // adv simple profile
    pavctx->max_b_frames = 2;
    //pavctx->max_b_frames = 0;
  }

//pavctx->flags |= CODEC_FLAG_LOOP_FILTER;
//pavctx->flags |= CODEC_FLAG_QP_RD; pavctx->mb_decision = FF_MB_DECISION_RD;
//pavctx->flags->intra_dc_precision=0;
//pavctx->me_threshold=0;


  if(pXcode->cfgFast >= ENCODER_QUALITY_BEST && pXcode->cfgFast <= ENCODER_QUALITY_MED) {
    pavctx->flags |= CODEC_FLAG_QPEL;
  }

/*
pavctx->me_method = ME_EPZS;

 // preset "me_method="
  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pavctx->me_method = ME_EPZS;  // X264_ME_DIA
  } else if(pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    pavctx->me_method = ME_HEX;  // X264_ME_HEX
  } else if(pXcode->cfgFast == ENCODER_QUALITY_HIGH) {
    pavctx->me_method = ME_HEX;   // X264_ME_HEX
  } else if(pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pavctx->me_method = ME_FULL;   // X264_ME_ESA_
  }
*/

  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pavctx->me_subpel_quality = 0;
  } else if(pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    pavctx->me_subpel_quality = 2;
  } else if(pXcode->cfgFast == ENCODER_QUALITY_HIGH) {
    pavctx->me_subpel_quality = 4;
  } else {
    pavctx->me_subpel_quality = 6;
  }


  //fprintf(stderr, "MAX QDIFF- :%d\n", pavctx->max_qdiff);

  if(pXcode->cfgQDiff > 0) {
    pavctx->max_qdiff = pXcode->cfgQDiff;
  } else {
    //pavctx->max_qdiff = 3;
  }

  // preset "qmin="
  if(pXcode->cfgQMin >= 2 && pXcode->cfgQMin <= 31) {
    pavctx->qmin = pXcode->cfgQMin;
    pavctx->lmin = pavctx->qmin * FF_QP2LAMBDA;
  } else {
    //pavctx->qmin = 2;
  }

  if(pXcode->cfgQMax >= 2 && pXcode->cfgQMax <= 31) {
    pavctx->qmax = pXcode->cfgQMax;
    pavctx->lmax = pavctx->qmax * FF_QP2LAMBDA;
  } else {
    //pavctx->qmax = 31;
  }

  if(pXcode->cfgIQRatio > 0) {
    pavctx->i_quant_factor = pXcode->cfgIQRatio;
  }
  if(pXcode->cfgBQRatio > 0) {
    pavctx->b_quant_factor = pXcode->cfgBQRatio;
  }

  if(pXcode->cfgSceneCut != 0) {
    // higher is more aggressive, mpeg4 comparison is scene_change_score > schechange_threshold
    pavctx->scenechange_threshold = -1 * pXcode->cfgSceneCut;
  }

  LOG(X_DEBUG("MPEG-4 %.1fKb/s, I:%.3f B:%.3f, Qrange %d - %d (L:%d - %d), "
                  "VBV size: %dKb, max: %dKb/s, min: %dKb/s, factor:%.2f, I scene threshold:%d%s"),
    (float)pXcode->cfgBitRateOut/1000,
    pavctx->i_quant_factor, pavctx->b_quant_factor, pavctx->qmin, pavctx->qmax,
    pavctx->lmin, pavctx->lmax,
    pXcode->cfgVBVBufSize/1000, pXcode->cfgVBVMaxRate/1000, pXcode->cfgVBVMinRate/1000, pXcode->cfgVBVAggressivity,
    pavctx->scenechange_threshold,  ((pavctx->flags &CODEC_FLAG_QPEL) ? ", qpel" : "") );

    //fprintf(stderr, "MB_LMIN:%d, MB_LMAX:%d, IQF:%.3f, IQO:%.3f, initial_cplx:%.3f\n", pavctx->mb_lmin, pavctx->mb_lmax, pavctx->i_quant_factor, pavctx->i_quant_offset, pavctx->rc_initial_cplx);

  return 0;
}

static int init_h263params(AVCodecContext *pavctx, IXCODE_VIDEO_OUT_T *pXcode) {

  return 0;
}

static int init_h263plusparams(AVCodecContext *pavctx, IXCODE_VIDEO_OUT_T *pXcode) {

  //LOG(X_DEBUG("Setting H.263+ flags"));
  //pavctx->flags |= CODEC_FLAG_H263P_SLICE_STRUCT;  // Annex K Slice Structured Mode (K=1)
  pavctx->flags |= CODEC_FLAG_LOOP_FILTER; // Annex J Deblocking Filter mode (J=1)
  pavctx->flags |= CODEC_FLAG_AC_PRED; // Annex I Advanced INTRA Coding mode (I=1), (T=1)

  return 0;
}

#if defined(XCODE_HAVE_VP6) && (XCODE_HAVE_VP6 > 0)
static int init_vp6params(AVCodecContext *pavctx, IXCODE_VIDEO_OUT_T *pXcode) {

  return 0;
}
#endif // (XCODE_HAVE_VP6) && (XCODE_HAVE_VP6 > 0)

#if defined(XCODE_HAVE_VP6F) && (XCODE_HAVE_VP6F > 0)
static int init_vp6fparams(AVCodecContext *pavctx, IXCODE_VIDEO_OUT_T *pXcode) {

  return 0;
}
#endif // (XCODE_HAVE_VP6F) && (XCODE_HAVE_VP6F > 0)

#if defined(XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)
static int init_vp8params(AVCodecContext *pavctx, IXCODE_VIDEO_OUT_T *pXcode) {

  pavctx->profile = pXcode->cfgProfile;
  //((VP8Context *) pavctx->priv_data)->enccfg.g_profile = pXcode->cfgProfile;

  // preset "qmin="
  pavctx->qmin = 4;

  // preset "qmax="
  if(pXcode->cfgQMax >= 4 && pXcode->cfgQMax <= 63) {
    pavctx->qmax = pXcode->cfgQMax;
  } else {
    pavctx->qmax = 63;
  }

  if(pXcode->cfgQDiff > 0) {
    pavctx->max_qdiff = pXcode->cfgQDiff;
  } else {
    //pavctx->max_qdiff = 3;
  }

  // default key frame interval min,max
  pavctx->keyint_min = 10;
  pavctx->gop_size = 128;

  if(pXcode->resLookaheadmin1 > 0) {
    pavctx->rc_lookahead = pXcode->resLookaheadmin1 - 1;
  }

  switch(pXcode->cfgFast) {
    case ENCODER_QUALITY_BEST:
      ((VP8Context *) pavctx->priv_data)->cpu_used = -4;
      ((VP8Context *) pavctx->priv_data)->deadline = VPX_DL_BEST_QUALITY;
      break;
    case ENCODER_QUALITY_HIGH:
      ((VP8Context *) pavctx->priv_data)->cpu_used = 0;
      ((VP8Context *) pavctx->priv_data)->deadline = VPX_DL_GOOD_QUALITY;
      break;
    case ENCODER_QUALITY_MED:
      ((VP8Context *) pavctx->priv_data)->cpu_used = 4;
      ((VP8Context *) pavctx->priv_data)->deadline = VPX_DL_GOOD_QUALITY;
      break;
    case ENCODER_QUALITY_LOW:
      ((VP8Context *) pavctx->priv_data)->cpu_used = 8;
      ((VP8Context *) pavctx->priv_data)->deadline = VPX_DL_REALTIME;
      break;
    case ENCODER_QUALITY_LOWEST:
      ((VP8Context *) pavctx->priv_data)->cpu_used = 16;
      ((VP8Context *) pavctx->priv_data)->deadline = VPX_DL_REALTIME;
    default:
      break;
   }

  ((VP8Context *) pavctx->priv_data)->error_resilient =  VP8F_ERROR_RESILIENT;
  ((VP8Context *) pavctx->priv_data)->arnr_max_frames = 3;
  ((VP8Context *) pavctx->priv_data)->arnr_strength = 3;
  ((VP8Context *) pavctx->priv_data)->arnr_type = 3;
  ((VP8Context *) pavctx->priv_data)->pXcodeOut = pXcode;

  LOG(X_DEBUG("VP8 Qrange %d - %d, fast: %d, cpu_used:%d, deadline: %d, profile:%d"),
    pavctx->qmin, pavctx->qmax,  pXcode->cfgFast - 1,
    ((VP8Context *) pavctx->priv_data)->cpu_used, ((VP8Context *) pavctx->priv_data)->deadline, 
    pXcode->cfgProfile);


  return 0;
}
#endif // (XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)


static int xcoder_libavcodec_video_update_encoder(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx) {
  int rc = 0;
  int update = 0;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  IXCODE_VIDEO_CTXT_T *pXcode = NULL;
  double gop_ms;
  int gop_size, keyint_min;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  //for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt) || !pFFCtx->pavctx || !pFFCtx->pavcodec) {
      //continue;
      return -1;
    }

    update = 0;

    switch(pXcode->out[outidx].cfgRateCtrl) {
      case RATE_CTRL_TYPE_CQP:
        if(pFFCtx->pavctx->cqp != pXcode->out[outidx].cfgQ) {
          pFFCtx->pavctx->cqp = pXcode->out[outidx].cfgQ;
          update = 1;
        }
        break;
      case RATE_CTRL_TYPE_CRF:
        if(pFFCtx->pavctx->crf != pXcode->out[outidx].cfgQ) {
          pFFCtx->pavctx->crf = pXcode->out[outidx].cfgQ;
          update = 1;
        }
        break;
      case RATE_CTRL_TYPE_BITRATE:
      default:
        break;
    }

    if(pFFCtx->pavctx->bit_rate != pXcode->out[outidx].cfgBitRateOut) {
      pFFCtx->pavctx->bit_rate = pXcode->out[outidx].cfgBitRateOut;
      update = 1;
    }
    if(pFFCtx->pavctx->bit_rate_tolerance != pXcode->out[outidx].cfgBitRateTolOut) {
      pFFCtx->pavctx->bit_rate_tolerance = pXcode->out[outidx].cfgBitRateTolOut;
      update = 1;
    }
    if(pFFCtx->pavctx->rc_buffer_size != pXcode->out[outidx].cfgVBVBufSize) {
      pFFCtx->pavctx->rc_buffer_size = pXcode->out[outidx].cfgVBVBufSize;
      update = 1;
    }
    if(pFFCtx->pavctx->rc_max_rate != pXcode->out[outidx].cfgVBVMaxRate) {
      pFFCtx->pavctx->rc_max_rate = pXcode->out[outidx].cfgVBVMaxRate;
      update = 1;
    }
    if(pFFCtx->pavctx->rc_min_rate != pXcode->out[outidx].cfgVBVMinRate) {
      pFFCtx->pavctx->rc_min_rate = pXcode->out[outidx].cfgVBVMinRate;
      update = 1;
    }
    if(pFFCtx->pavctx->rc_buffer_aggressivity != pXcode->out[outidx].cfgVBVAggressivity) {
      pFFCtx->pavctx->rc_buffer_aggressivity = pXcode->out[outidx].cfgVBVAggressivity;
      update = 1;
    }
    if(pFFCtx->pavctx->qmax != pXcode->out[outidx].cfgQMax) {
      pFFCtx->pavctx->qmax = pXcode->out[outidx].cfgQMax;
      update = 1;
    }

    pFFCtx->pavctx->time_base = (AVRational) { pXcode->resOutFrameDeltaHz, pXcode->resOutClockHz};

    if(pXcode->out[outidx].cfgGopMaxMs > 0) {
      gop_ms = (double) pXcode->out[outidx].cfgGopMaxMs / 1000.0f;
      gop_size = gop_ms * ((double)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz);
      if(pFFCtx->pavctx->gop_size != gop_size) {
        pFFCtx->pavctx->gop_size = gop_size;
        update = 1;
      }
    }
    if(pXcode->out[outidx].cfgGopMinMs > 0) {
      gop_ms = (double) pXcode->out[outidx].cfgGopMinMs / 1000.0f;
      keyint_min = gop_ms * ((double)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz);
      if(pFFCtx->pavctx->keyint_min != keyint_min) {
        pFFCtx->pavctx->keyint_min = keyint_min;
        update = 1;
      }
    }

    if(pFFCtx->pavctx->gop_size < pFFCtx->pavctx->keyint_min) {
       pFFCtx->pavctx->gop_size = pFFCtx->pavctx->keyint_min;
    } else if(pFFCtx->pavctx->keyint_min > pFFCtx->pavctx->gop_size) {
       pFFCtx->pavctx->keyint_min = pFFCtx->pavctx->gop_size;
    }

    if(update) {
      if((rc = pFFCtx->pavcodec->init(pFFCtx->pavctx)) < 0) {
        //break;
      }
      //fprintf(stderr, "UPDATE[%d] rc:%d cqp:%d crf:%f\n", idx, rc, pFFCtx->pavctx->cqp, pFFCtx->pavctx->crf);
    }

  //}

  return rc;
}

int xcoder_libavcodec_video_init_encoder(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx) {
  int rc = 0;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  IXCODE_VIDEO_CTXT_T *pXcode = NULL;
  AVCodec *pavcodec = NULL;
  enum PixelFormat enc_pix_fmt = PIX_FMT_YUV420P;
  enum CodecID codecId = CODEC_ID_NONE;
  double gop_ms = 4.3;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  //fprintf(stderr, "xcoder_libavcodec_video_init_encoder[%d] tid:0x%x, cfgFileTypeOut:%d \n", outidx, pthread_self(), pXcode->common.cfgFileTypeOut);

  switch(pXcode->common.cfgFileTypeOut) {

  //TODO: move x264 to outside of libavcodec
#if defined(XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)
    case XC_CODEC_TYPE_H264:
      // Use our own x264 encoder wrapper - instead of libavcodec's
      pavcodec = &libx264ext_encoder;
      codecId = CODEC_ID_H264;
      break;
#endif // (XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)

    case XC_CODEC_TYPE_MPEG4V:
      pavcodec = &FF_MPEG4_ENCODER;
      codecId = CODEC_ID_MPEG4;
      break;
    case XC_CODEC_TYPE_H263:
      pavcodec = &FF_H263_ENCODER;
      codecId = CODEC_ID_H263;
      break;
    case XC_CODEC_TYPE_H263_PLUS:
      pavcodec = &FF_H263P_ENCODER;
      codecId = CODEC_ID_H263P;
      break;

/*
#if defined(XCODE_HAVE_VP6)
    case XC_CODEC_TYPE_VP6:
      pavcodec = NULL;
      codecId = CODEC_ID_VP6;
      break;
#endif // XCODE_HAVE_VP6

#if defined(XCODE_HAVE_VP6F)
    case XC_CODEC_TYPE_VP6F:
      pavcodec = NULL;
      codecId = CODEC_ID_VP6F;
      break;
#endif // XCODE_HAVE_VP6
*/

#if defined(XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)
    case XC_CODEC_TYPE_VP8:
      pavcodec = &libvpxext_encoder;
      codecId = CODEC_ID_VP8;
      break;
#endif // (XCODE_HAVE_VP8) && (XCODE_HAVE_VP8 > 0)

    case XC_CODEC_TYPE_RAWV_YUVA420P:
      enc_pix_fmt = PIX_FMT_YUVA420P;
      break;
    case XC_CODEC_TYPE_RAWV_YUV420P:
      enc_pix_fmt = PIX_FMT_YUV420P;
      break;
    case XC_CODEC_TYPE_RAWV_BGRA32:
      enc_pix_fmt = PIX_FMT_BGRA;
      break;
    case XC_CODEC_TYPE_RAWV_RGBA32:
      enc_pix_fmt = PIX_FMT_RGBA;
      break;
    case XC_CODEC_TYPE_RAWV_BGR24:
      enc_pix_fmt = PIX_FMT_BGR24;
      break;
    case XC_CODEC_TYPE_RAWV_RGB24:
      enc_pix_fmt = PIX_FMT_RGB24;
      break;
    case XC_CODEC_TYPE_RAWV_BGR565:
      enc_pix_fmt = PIX_FMT_BGR565LE;
      break;
    case XC_CODEC_TYPE_RAWV_RGB565:
      enc_pix_fmt = PIX_FMT_RGB565LE;
      break;
    default:
      LOG(X_ERROR("Unsupported xcoder output video codec %d"), pXcode->common.cfgFileTypeOut);
      return -1;
  }


  if(((FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt)) {
    //
    // Already been initialized
    //
    return xcoder_libavcodec_video_update_encoder(pCtxt, outidx);
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = calloc(1, sizeof(FFMPEG_AVCODEC_CTXT_T));
  }

  if(!(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  if(pCtxt->pAVCtxt->out[outidx].dim_enc.width <= 0 || pCtxt->pAVCtxt->out[outidx].dim_enc.height <= 0) {
    LOG(X_ERROR("Encoder output dimensions not set"));
    return -1;
  }

  if(!have_avcodec_init) {
    avcodec_init();
    have_avcodec_init = 1;
  }

  pFFCtx->pavcodec = pavcodec;

  pCtxt->pAVCtxt->out[outidx].dim_enc.pix_fmt = enc_pix_fmt;
  //pCtxt->pAVCtxt->out[outidx].dim_enc.width = pXcode->out[outidx].resOutH;
  //pCtxt->pAVCtxt->out[outidx].dim_enc.height = pXcode->out[outidx].resOutV;
  pCtxt->pAVCtxt->out[outidx].dim_enc.pic_sz = avpicture_get_size(enc_pix_fmt,
                                                              pCtxt->pAVCtxt->out[outidx].dim_enc.width, 
                                                              pCtxt->pAVCtxt->out[outidx].dim_enc.height);

  //fprintf(stderr, "xcoder_libavcodec_video_init_encoder[%d] cfgFileTypeOut:%d, pavcodec:0x%x enc_pic_sz:%d, resOutH:%d\n", outidx, pXcode->common.cfgFileTypeOut, pavcodec, pCtxt->pAVCtxt->out[outidx].enc_pic_sz, pXcode->out[outidx].resOutH);

  if(pavcodec) {
    // Be careful, using context3 set max_b to -1 (used to be 0) and this caused b frames in profile=66
    //if(NULL == (pFFCtx->pavctx = avcodec_alloc_context3(pFFCtx->pavcodec))) {
    if(NULL == (pFFCtx->pavctx = avcodec_alloc_context2(AVMEDIA_TYPE_VIDEO))) {
      return -1;
    }
    if(pavcodec->priv_data_size > 0 && !(pFFCtx->pavctx->priv_data = av_mallocz(pavcodec->priv_data_size))) {
      return -1;
    }

    //TODO: call codec specific init
    switch(pXcode->common.cfgFileTypeOut) {

#if defined(XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)
      case XC_CODEC_TYPE_H264:
        if(init_x264params(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;
#endif // (XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)

      case XC_CODEC_TYPE_MPEG4V:
        if(init_mpeg4vparams(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;
      case XC_CODEC_TYPE_H263:
        // ensure init is done only for single threading
        pXcode->out[outidx].cfgThreads = pFFCtx->pavctx->thread_count;
        if(init_h263params(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;
      case XC_CODEC_TYPE_H263_PLUS:
        pXcode->out[outidx].cfgThreads = pFFCtx->pavctx->thread_count;
        if(init_h263plusparams(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;

#if defined(XCODE_HAVE_VP6)
      case XC_CODEC_TYPE_VP6:
        if(init_vp6params(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;
#endif // XCODE_HAVE_VP6

#if defined(XCODE_HAVE_VP6F)
      case XC_CODEC_TYPE_VP6F:
        if(init_vp6fparams(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;
#endif // XCODE_HAVE_VP6

#if defined(XCODE_HAVE_VP8)
      case XC_CODEC_TYPE_VP8:
        if(init_vp8params(pFFCtx->pavctx, &pXcode->out[outidx]) != 0) {
          return -1;
        }
        break;
#endif // XCODE_HAVE_VP8
      default:
        break;
    }

    pFFCtx->pavctx->codec_id = codecId;
    pFFCtx->pavctx->width = pCtxt->pAVCtxt->out[outidx].dim_enc.width;
    pFFCtx->pavctx->height = pCtxt->pAVCtxt->out[outidx].dim_enc.height;
    pFFCtx->pavctx->pix_fmt = pCtxt->pAVCtxt->out[outidx].dim_enc.pix_fmt;
    pFFCtx->pavctx->sample_aspect_ratio = (AVRational) { 1, 1};
    pFFCtx->pavctx->bit_rate = pXcode->out[outidx].cfgBitRateOut;
    pFFCtx->pavctx->bit_rate_tolerance = pXcode->out[outidx].cfgBitRateTolOut;
    if(pXcode->out[outidx].cfgVBVBufSize > 0) {
      pFFCtx->pavctx->rc_buffer_size = pXcode->out[outidx].cfgVBVBufSize;
    }
    if(pXcode->out[outidx].cfgVBVMaxRate > 0) {
      pFFCtx->pavctx->rc_max_rate = pXcode->out[outidx].cfgVBVMaxRate;
    }
    if(pXcode->out[outidx].cfgVBVMinRate > 0) {
      pFFCtx->pavctx->rc_min_rate = pXcode->out[outidx].cfgVBVMinRate;
    }
    if(pXcode->out[outidx].cfgVBVAggressivity > 0) {
      pFFCtx->pavctx->rc_buffer_aggressivity = pXcode->out[outidx].cfgVBVAggressivity;
    }
    //if(pXcode->out[outidx].cfgSceneCut != 0) {
      //pFFCtx->pavctx->scenechange_threshold = pXcode->out[outidx].cfgSceneCut;
    //}
    if(pXcode->out[outidx].cfgThreads > 0) {
      pFFCtx->pavctx->thread_count = pXcode->out[outidx].cfgThreads;
    } else {
      // default value.  Rough optimal val be around (# cores * ~2)
      pFFCtx->pavctx->thread_count = 8;
    }
    if(pXcode->out[outidx].cfgGopMaxMs > 0) {
      gop_ms = (double) pXcode->out[outidx].cfgGopMaxMs / 1000.0f;
      pFFCtx->pavctx->gop_size = gop_ms * ((double)pXcode->resOutClockHz /
                                                   pXcode->resOutFrameDeltaHz);
    }
    if(pXcode->out[outidx].cfgGopMinMs > 0) {
      gop_ms = (double) pXcode->out[outidx].cfgGopMinMs / 1000.0f;
      pFFCtx->pavctx->keyint_min = gop_ms * ((double)pXcode->resOutClockHz /
                                                pXcode->resOutFrameDeltaHz);
    }
    if(pFFCtx->pavctx->gop_size < pFFCtx->pavctx->keyint_min) {
       pFFCtx->pavctx->gop_size = pFFCtx->pavctx->keyint_min;
    } else if(pFFCtx->pavctx->keyint_min > pFFCtx->pavctx->gop_size) {
       pFFCtx->pavctx->keyint_min = pFFCtx->pavctx->gop_size;
    }

    pFFCtx->pavctx->time_base = (AVRational) { pXcode->resOutFrameDeltaHz,
                                               pXcode->resOutClockHz};

    LOG(X_DEBUG("GOP: %dfr - %dfr (%dms - %dms)"),
          pFFCtx->pavctx->keyint_min, pFFCtx->pavctx->gop_size,
          pXcode->out[outidx].cfgGopMinMs, pXcode->out[outidx].cfgGopMaxMs);

    if(avcodec_open(pFFCtx->pavctx, pFFCtx->pavcodec) < 0) {
      LOG(X_ERROR("Failed to open video encoder output codec %d, %dx%d"), 
          pXcode->common.cfgFileTypeOut, pFFCtx->pavctx->width, pFFCtx->pavctx->height);
      return -1;
    }

    if(pFFCtx->pavctx->extradata_size + 4 > sizeof(pXcode->out[outidx].hdr)) {
      LOG(X_ERROR("video encoder[%d] extra data size %d > %d"), outidx,
          pFFCtx->pavctx->extradata_size + 4, sizeof(pXcode->out[outidx].hdr));
      return -1; 
    } else if(pFFCtx->pavctx->extradata_size > 0) {
      memcpy(pXcode->out[outidx].hdr, pFFCtx->pavctx->extradata, pFFCtx->pavctx->extradata_size);
      pXcode->out[outidx].hdrLen = pFFCtx->pavctx->extradata_size;
    }

  }

  pXcode->out[outidx].qTot = 0;
  pXcode->out[outidx].qSamples = 0;

  return rc;
}

int xcoder_libavcodec_video_decode(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx,
                                   struct AVFrame *rawout, const unsigned char *inbuf, unsigned int inlen) {
  AVPacket avpkt;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  int have_prev = 0;
  int lenRaw;

  if(!pCtxt || !(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt) || !pFFCtx->pavctx) {
    return -1;
  }

  //fprintf(stderr, "xcoder_libavcodec_video_decode[%d]\n", outidx);

  av_init_packet(&avpkt);
  avpkt.data = (unsigned char *) inbuf;
  avpkt.size = inlen;

  have_prev = pCtxt->pAVCtxt->havePrevVidFrame;

  lenRaw = avcodec_decode_video2(pFFCtx->pavctx, rawout, &have_prev, &avpkt);

  if(lenRaw < 0) {
    LOG(X_ERROR("avcodec_decode_video2[%d] failed"), outidx);
    return -1;
  }

  pCtxt->pAVCtxt->havePrevVidFrame = have_prev;
  pCtxt->pAVCtxt->framedecidxok = pCtxt->pAVCtxt->framedecidx;

  if(pFFCtx->pavctx->width != pCtxt->pAVCtxt->dim_in.width) {
    pCtxt->pAVCtxt->dim_in.width = pFFCtx->pavctx->width ;
  }
  if(pFFCtx->pavctx->height != pCtxt->pAVCtxt->dim_in.height) {
    pCtxt->pAVCtxt->dim_in.height = pFFCtx->pavctx->height;
  }

  if(pFFCtx->pavctx->pix_fmt != pCtxt->pAVCtxt->dim_in.pix_fmt) {
    pCtxt->pAVCtxt->dim_in.pix_fmt = pFFCtx->pavctx->pix_fmt;
  }

  return lenRaw;
}

int xcoder_libavcodec_video_encode(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx,
                                   const struct AVFrame *rawin, unsigned char *out, unsigned int outlen) {
  IXCODE_VIDEO_CTXT_T *pXcode = NULL;
  FFMPEG_AVCODEC_CTXT_T *pFFCtx = NULL;
  int lenOut;

  if(!pCtxt || !(pFFCtx = (FFMPEG_AVCODEC_CTXT_T *) pCtxt->pctxt) || !pFFCtx->pavctx) {
    return -1;
  }

  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  if(pXcode->out[outidx].cfgForceIDR) {
    ((AVFrame *) rawin)->pict_type = AV_PICTURE_TYPE_I;
    pXcode->out[outidx].cfgForceIDR = 0;
  }

  //fprintf(stderr, "xcoder_libavcodec_video_encode[%d]\n", outidx);

  lenOut = avcodec_encode_video(pFFCtx->pavctx, out, outlen, rawin);

  if(lenOut < 0) {
    LOG(X_ERROR("avcodec_encode_video[%d] failed"), outidx);
  } else if(lenOut > 0) {

    pXcode->out[outidx].frameQ = (pFFCtx->pavctx->coded_frame->quality / (float) FF_QP2LAMBDA);
    pXcode->out[outidx].qTot += pXcode->out[outidx].frameQ;
    pXcode->out[outidx].qSamples++;
    pXcode->out[outidx].frameType = pFFCtx->pavctx->coded_frame->pict_type;
    pXcode->out[outidx].pts = pFFCtx->pavctx->coded_frame->pts;
    if(pFFCtx->pavctx->coded_frame->display_picture_number != pFFCtx->pavctx->coded_frame->coded_picture_number) {
      pXcode->out[outidx].dts = (pFFCtx->pavctx->coded_frame->coded_picture_number -
                                 pFFCtx->pavctx->coded_frame->display_picture_number);
                             //* (int) pXcode->resOutFrameDeltaHz;
    } else {
      pXcode->out[outidx].dts = 0;
    }
  }

  return lenOut;
}


