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
#include "xcoder_opus.h"
#include "xcoder.h"
#include "ixcode.h"
#include "dlfcn.h"
#include "opus.h"
#include "opus_multistream.h"


#if defined(XCODE_HAVE_OPUS)

static const unsigned char s_opusCoupledStreams[8] = { 0, 1, 1, 2, 2, 2, 2, 3 };

static const unsigned char s_opusChannelMap[8][8] = {
    { 0 },
    { 0, 1 },
    { 0, 1, 2 },
    { 0, 1, 2, 3 },
    { 0, 1, 3, 4, 2 },
    { 0, 1, 4, 5, 2, 3 },
    { 0, 1, 5, 6, 2, 4, 3 },
    { 0, 1, 6, 7, 4, 5, 2, 3 },
};

#if defined(XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)
static int g_xcode_opus_refcnt = 0;
static void *g_xcode_opus_dlopus = NULL;

static void dlclose_opus(void **ppOpus) {
  if(--g_xcode_opus_refcnt <= 0) {
    dlclose(g_xcode_opus_dlopus);
    g_xcode_opus_dlopus = NULL;
    if(g_xcode_opus_refcnt < 0) {
      g_xcode_opus_refcnt = 0;
    }
    LOG(X_DEBUG("Dynamically unloaded OPUS"));
  }
  *ppOpus = NULL;
}

static void *dlopen_opus(const char *path) {
  void *pOpus = NULL;

  if(!(pOpus = g_xcode_opus_dlopus)) {
     if((pOpus = dlopen(path, RTLD_LOCAL | RTLD_LAZY))) {
       g_xcode_opus_dlopus = pOpus;
       LOG(X_DEBUG("Dynamically loaded OPUS: %s"), path);
     }
  }

  if(pOpus) {
    g_xcode_opus_refcnt++;
  }

  return pOpus;
}

#endif // (XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)


typedef const char * (* OPUS_STRERROR)(int);

#if defined(XCODE_HAVE_OPUS_ENCODER)

typedef OpusMSEncoder * (* OPUS_MULTISTREAM_ENCODER_CREATE) (opus_int32, int, int, int, 
                                                             const unsigned char * , int , int *);
typedef int (* OPUS_MULTISTREAM_ENCODER_CTL) (OpusMSEncoder *, int , ...) ;
typedef int (* OPUS_MULTISTREAM_ENCODE) (OpusMSEncoder *, const opus_int16 *, int, unsigned char *, 
                                         opus_int32);
typedef void (* OPUS_MULTISTREAM_ENCODER_DESTROY)(OpusMSEncoder *);

typedef struct OPUS_ENCODER_CODEC_CTXT {
  OpusMSEncoder                   *ctxEnc;
  unsigned int                     frameDurationMs;
  unsigned int                     sampleRate;
  int                              channels;
  int                              bitRate;
  int                              complexity;
  int64_t                          pts;

  void                            *dlopus;
  OPUS_MULTISTREAM_ENCODER_CREATE  fCreate;
  OPUS_MULTISTREAM_ENCODER_CTL     fCtl;
  OPUS_MULTISTREAM_ENCODE          fEncode;
  OPUS_MULTISTREAM_ENCODER_DESTROY fClose;
  OPUS_STRERROR                    fError;

} OPUS_ENCODER_CODEC_CTXT_T;


static int encoder_load_dynamic(OPUS_ENCODER_CODEC_CTXT_T *pOpus) {
  int rc = 0;

#if defined(XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)

  const char *dlpath = NULL;

  if(!((dlpath = LIBOPUS_PATH) && (pOpus->dlopus = dlopen_opus(dlpath))) &&
     !((dlpath = LIBOPUS_NAME) && (pOpus->dlopus = dlopen_opus(dlpath)))) {
      //!((dlpath = LIBOPUS_PATH) && (pOpus->dlopus = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY))) &&
      //!((dlpath = LIBOPUS_NAME) && (pOpus->dlopus = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY)))) {
    LOG(X_ERROR("Unable to dlopen opus encoder library: %s"), dlerror());
    return -1;
  }

  //LOG(X_DEBUG("Dynamically loaded encoder: %s"), dlpath);

  if(!(pOpus->fCreate = dlsym(pOpus->dlopus, "opus_multistream_encoder_create"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fCtl = dlsym(pOpus->dlopus, "opus_multistream_encoder_ctl"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fClose  = dlsym(pOpus->dlopus, "opus_multistream_encoder_destroy"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fEncode = dlsym(pOpus->dlopus, "opus_multistream_encode"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fError = dlsym(pOpus->dlopus, "opus_strerror"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

#else

  pOpus->fCreate = opus_multistream_encoder_create;
  pOpus->fClose = opus_multistream_encoder_destroy;
  pOpus->fCtl = opus_multistream_encoder_ctl;
  pOpus->fEncode = opus_multistream_encode;
  pOpus->fError = opus_strerror;

#endif // (XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)

  return rc;
}

static void encoder_opus_free(OPUS_ENCODER_CODEC_CTXT_T *pOpus) {
 
  if(!pOpus) {
    return;
  }

  if(pOpus->ctxEnc) {
    pOpus->fClose(pOpus->ctxEnc);
    pOpus->ctxEnc = NULL;
  }

#if defined(XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)
  if(pOpus->dlopus) {
    dlclose_opus(&pOpus->dlopus);
  }
#endif // (XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)

  free(pOpus);

}

static OPUS_ENCODER_CODEC_CTXT_T *encoder_opus_alloc() {
  OPUS_ENCODER_CODEC_CTXT_T *pOpus;

  if(!(pOpus = (OPUS_ENCODER_CODEC_CTXT_T *) calloc(1, sizeof(OPUS_ENCODER_CODEC_CTXT_T)))) {
    return NULL;
  }

  if(encoder_load_dynamic(pOpus) < 0) {
    return NULL;
  }

  pOpus->pts = 0;

  return pOpus;
}

void xcoder_opus_close_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  OPUS_ENCODER_CODEC_CTXT_T *pOpus;

  if(!pCtxt || (!(pOpus = (OPUS_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt))) {
    return;
  }

  encoder_opus_free(pOpus);

}

int xcoder_opus_init_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  OPUS_ENCODER_CODEC_CTXT_T *pOpus = NULL;
  int bitRate = 0;
  int coupledStreamCount = 0;
  int streamCount = 0;
  const unsigned char *channelMapping = NULL;
  int applicationType = OPUS_APPLICATION_VOIP;
  int ret = OPUS_OK;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(!(pOpus = (OPUS_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = encoder_opus_alloc();
  }

  if(!(pOpus = (OPUS_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  pCtxt->pAVCtxt->encbps = 2; // S16 format

  pOpus->sampleRate = pCtxt->pAVCtxt->enc_samplerate = pXcode->cfgSampleRateOut;

  if((pOpus->channels = pCtxt->pAVCtxt->enc_channels = pXcode->cfgChannelsOut) <= 0) {
    pOpus->channels = 1;
  } else if(pOpus->channels > 8) {
    pOpus->channels = 8;
  }

  switch(pOpus->sampleRate) {
    case 48000:
      bitRate = 64000;
      break;
    case 24000:
      bitRate = 32000;
      break;
    case 16000:
      bitRate = 24000;
      break;
    case 12000:
      bitRate = 20000;
      break;
    case 8000:
      bitRate = 16000;
      break;
    default:
      LOG(X_ERROR("Unsupported OPUS output rate %dHz"), pOpus->sampleRate);
      return -1;
  }

  //
  // Hardcoded ptime
  //
  pOpus->frameDurationMs = 20;

  //
  // 0 - lowest, 10 - highest
  //
  pOpus->complexity = 10;
  //pOpus->complexity = 0;

  if(pXcode->cfgBitRateOut > 0) {
    bitRate = pXcode->cfgBitRateOut / pOpus->channels;
  }

  coupledStreamCount = s_opusCoupledStreams[pOpus->channels - 1];
  streamCount = pOpus->channels - coupledStreamCount;
  channelMapping = s_opusChannelMap[pOpus->channels - 1];

  pOpus->bitRate = (bitRate * streamCount) +  (bitRate / 2 * coupledStreamCount);
  //fprintf(stderr, "BR:%d = (%d * %d) + (%d * %d)\n", pOpus->bitRate, bitRate,  streamCount, bitRate / 2 ,coupledStreamCount);
  if((pOpus->ctxEnc = pOpus->fCreate(pOpus->sampleRate, 
                                 pOpus->channels,
                                 streamCount,
                                 coupledStreamCount,
                                 channelMapping,
                                 applicationType, 
                                 &ret)) == NULL || ret != OPUS_OK) {
    LOG(X_ERROR("Failed to initialize OPUS encoder: '%s'"), pOpus->fError(ret));
    return -1;
  }

  //pOpus->bitRate=OPUS_BITRATE_MAX;
  if((ret = pOpus->fCtl(pOpus->ctxEnc, OPUS_SET_BITRATE(pOpus->bitRate))) != OPUS_OK) {
    LOG(X_WARNING("Failed to set opus encoder bitrate to %d %s"), pOpus->bitRate, pOpus->fError(ret));
  }

  if((ret = pOpus->fCtl(pOpus->ctxEnc, OPUS_SET_COMPLEXITY(pOpus->complexity))) != OPUS_OK) {
    LOG(X_WARNING("Failed to set opus encoder complexity to %d %s"), pOpus->complexity, pOpus->fError(ret));
  }

  LOG(X_DEBUG("Initialized OPUS encoder %dHz, %dbps, channels:%d, complexity:%d, frame:%dms, %.1fKb/s"),
      pOpus->sampleRate, pOpus->bitRate, pOpus->channels, pOpus->complexity, pOpus->frameDurationMs, 
     (float)pOpus->bitRate/1000.0f);
  //fprintf(stderr, "OPUS CH:%d, STREAMC:%d, COUPLEDC:%d\n", pOpus->channels, streamCount, coupledStreamCount);
  return 0;
}

int xcoder_opus_encode(CODEC_WRAP_CTXT_T *pCtxt,
                       unsigned char *out_packets,
                       unsigned int out_sz,
                       const int16_t *samples,
                       int64_t *pts) {

  int retBytes = 0;
  OPUS_ENCODER_CODEC_CTXT_T *pOpus = NULL;
 int frameSamples;

 if(!pCtxt || !(pOpus = (OPUS_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  frameSamples = pOpus->sampleRate / (1000/ pOpus->frameDurationMs);

  // max payload size 
  //nBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;
  //nBytes = out_sz;
  //counter = pOpus ->encControl.packetSize;
  
  //fprintf(stderr, "0x%x pktsz:%d, int16:0x%x, out_sz:%d, out_pkts:0x%x, counter:%d\n", pOpus->psEnc, pOpus->encControl.packetSize, samples, out_sz, out_packets, counter);
 
  retBytes = pOpus->fEncode(pOpus->ctxEnc, (opus_int16 *) samples, frameSamples, out_packets, out_sz);

  //fprintf(stderr, "OPUS_ENCODE ret:%d, frameSamples:%d, pts:%lld (%.3f)\n", retBytes, frameSamples, pOpus->pts, (float)pOpus->pts/pOpus->sampleRate);
  if(retBytes < 0) {
    LOG(X_ERROR("OPUS encode failed. out_sz:%d, frame size:%d,  %s"), out_sz, frameSamples, 
                pOpus->fError(retBytes));
    retBytes = -1; 
  } else if(retBytes > 0) {
    pOpus->pts += frameSamples;
  }

  if(pts) {
    *pts = pOpus->pts;
  }

  return retBytes;
}

#endif // XCODE_HAVE_OPUS_ENCODER

#if defined(XCODE_HAVE_OPUS_DECODER)

typedef OpusMSDecoder * (* OPUS_MULTISTREAM_DECODER_CREATE) (opus_int32, int, int, int, 
                                                             const unsigned char *, int *);
typedef int (* OPUS_MULTISTREAM_DECODE) (OpusMSDecoder *, const unsigned char *, opus_int32, 
                                         opus_int16 *, int, int);
typedef void (* OPUS_MULTISTREAM_DECODER_DESTROY) (OpusMSDecoder *);

typedef struct OPUS_DECODER_CODEC_CTXT {
  OpusMSDecoder                     *ctxDec;

  void                              *dlopus;
  OPUS_MULTISTREAM_DECODER_CREATE   fCreate;
  OPUS_MULTISTREAM_DECODE           fDecode;
  OPUS_MULTISTREAM_DECODER_DESTROY  fClose;
  OPUS_STRERROR                     fError;

} OPUS_DECODER_CODEC_CTXT_T;

static int decoder_load_dynamic(OPUS_DECODER_CODEC_CTXT_T *pOpus) {
  int rc = 0;

#if defined(XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)

  const char *dlpath = NULL;

  if( !((dlpath = LIBOPUS_PATH) && (pOpus->dlopus = dlopen_opus(dlpath))) &&
      !((dlpath = LIBOPUS_NAME) && (pOpus->dlopus = dlopen_opus(dlpath)))) {
     //!((dlpath = LIBOPUS_PATH) && (pOpus->dlopus = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY))) &&
      //!((dlpath = LIBOPUS_NAME) && (pOpus->dlopus = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY)))) {
    LOG(X_ERROR("Unable to dlopen opus decoder library: %s"), dlerror());
    return -1;
  }

  //LOG(X_DEBUG("Dynamically loaded decoder: %s"), dlpath);

  if(!(pOpus->fCreate = dlsym(pOpus->dlopus, "opus_multistream_decoder_create"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fDecode = dlsym(pOpus->dlopus, "opus_multistream_decode"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fClose = dlsym(pOpus->dlopus, "opus_multistream_decoder_destroy"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

  if(!(pOpus->fError = dlsym(pOpus->dlopus, "opus_strerror"))) {
    LOG(X_ERROR("Unable to dlsym opus function: %s"), dlerror());
    dlclose_opus(&pOpus->dlopus);
    return -1;
  }

#else

  pOpus->fCreate = opus_multistream_decoder_create;
  pOpus->fClose = opus_multistream_decoder_destroy;
  pOpus->fDecode = opus_multistream_decode;
  pOpus->fError = opus_strerror;

#endif // (XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)

  return rc;
}

static void decoder_opus_free(void *pArg) {
  OPUS_DECODER_CODEC_CTXT_T *pOpus = (OPUS_DECODER_CODEC_CTXT_T *) pArg;

  if(!pOpus) {
    return;
  }

  if(pOpus->ctxDec) {
    pOpus->fClose(pOpus->ctxDec);
    pOpus->ctxDec = NULL;
  }

#if defined(XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)
  if(pOpus->dlopus) {
    dlclose_opus(&pOpus->dlopus);
  }
#endif // (XCODE_HAVE_OPUS_DLOPEN) && (XCODE_HAVE_OPUS_DLOPEN > 0)

  free(pOpus);

}

static void *decoder_opus_alloc() {
  OPUS_DECODER_CODEC_CTXT_T *pOpus;

  if(!(pOpus = (OPUS_DECODER_CODEC_CTXT_T *) calloc(1, sizeof(OPUS_DECODER_CODEC_CTXT_T)))) {
    return NULL;
  }

  if(decoder_load_dynamic(pOpus) < 0) {
    return NULL;
  }

  return pOpus;
}

void xcoder_opus_close_decoder(CODEC_WRAP_CTXT_T *pCtxt) {
  OPUS_DECODER_CODEC_CTXT_T *pOpus;

  if(!pCtxt || (!(pOpus = (OPUS_DECODER_CODEC_CTXT_T *) pCtxt->pctxt))) {
    return;
  }

  decoder_opus_free(pOpus);

}

int xcoder_opus_init_decoder(CODEC_WRAP_CTXT_T *pCtxt) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  OPUS_DECODER_CODEC_CTXT_T *pOpus = NULL;
  int streamCount;
  int coupledStreamCount;
  const unsigned char *channelMapping = NULL;
  int ret = 0;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(!(pOpus = (OPUS_DECODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = decoder_opus_alloc();
  }

  if(!(pOpus = (OPUS_DECODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  if((pCtxt->pAVCtxt->dec_channels = pXcode->cfgChannelsIn) <= 0) {
    pCtxt->pAVCtxt->dec_channels = 1;
  }
  pCtxt->pAVCtxt->decbps = 2;
  pCtxt->pAVCtxt->dec_samplerate = pXcode->cfgSampleRateIn;

  coupledStreamCount = s_opusCoupledStreams[pCtxt->pAVCtxt->dec_channels - 1];
  streamCount = pCtxt->pAVCtxt->dec_channels - coupledStreamCount;
  channelMapping = s_opusChannelMap[pCtxt->pAVCtxt->dec_channels - 1];

  switch(pCtxt->pAVCtxt->dec_samplerate) {
    case 48000:
    case 24000:
    case 16000:
    case 12000:
    case 8000:
      break;
    default:
      LOG(X_ERROR("Unsupported OPUS decoder output sample rate %dHz"), pCtxt->pAVCtxt->dec_samplerate);
      return -1;
  }

  if((pOpus->ctxDec = pOpus->fCreate(pCtxt->pAVCtxt->dec_samplerate,
                                     pCtxt->pAVCtxt->dec_channels,
                                     streamCount, 
                                     coupledStreamCount,
                                     channelMapping, 
                                     &ret)) == NULL || ret != OPUS_OK) {
    LOG(X_ERROR("Failed to initialize OPUS decoder: channels:%d, '%s'"), pCtxt->pAVCtxt->dec_channels, 
                pOpus->fError(ret));
    return -1;
  }

  LOG(X_DEBUG("Initialized OPUS decoder %dHz, channels:%d"), 
    pCtxt->pAVCtxt->dec_samplerate, pCtxt->pAVCtxt->dec_channels);
  //fprintf(stderr, "OPUS DEC CH:%d, STREAMC:%d, COUPLEDC:%d\n", pCtxt->pAVCtxt->dec_channels, streamCount, coupledStreamCount);

  return 0;
}

int xcoder_opus_decode(CODEC_WRAP_CTXT_T *pCtxt,
                       const unsigned char *frame,
                       unsigned int frame_sz,
                       int16_t *out_samples,
                       unsigned int sz_bytes) {
  int ret;
  OPUS_DECODER_CODEC_CTXT_T *pOpus = NULL;
  int lostFlag = 0;
  unsigned int outBytes = 0;
  unsigned int outSamples = 0;
  int len;

  if(!pCtxt || !(pOpus = (OPUS_DECODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  if(!frame || frame_sz <= 0) {
    lostFlag = 1;
  }
  //fprintf(stderr, "OPUS lostFlag:%d, frame_sz:%d\n", lostFlag, frame_sz);


  do {

    if(outBytes > sz_bytes) {
      ret = -1;
      break;
    }

    len = sz_bytes - outBytes;

   //fprintf(stderr, "SKP_Opus_SDK_Decode frame_sz:%d loastFlag, 0x%x 0x%x\n", frame_sz, lostFlag, frame[0], frame[1]);

    //ret = SKP_Opus_SDK_Decode(pOpus->psDec,
    ret = pOpus->fDecode(pOpus->ctxDec,
                         lostFlag ? NULL : frame, 
                         frame_sz, 
                         &out_samples[outBytes],
                         len,
                         lostFlag);
    if(ret < 0) {
      LOG(X_ERROR("OPUS decode failed: %s"),  pOpus->fError(ret));
      break;
    }

    if(pCtxt->pAVCtxt->dec_channels) {
      ret *= pCtxt->pAVCtxt->dec_channels;
    }

    outBytes += (ret * sizeof(int16_t));
    outSamples += ret;

    //fprintf(stderr, "OPUS DECODE %d, ret:%d, channels:%d\n", frame_sz, ret, pCtxt->pAVCtxt->dec_channels);

  } while(0);
  //} while(pOpus->decControl.moreInternalDecoderFrames);

  if(ret < 0) {
    ret = -1;
  } else {
    ret = outBytes;
  }

  return ret;
}

#endif // XCODE_HAVE_OPUS_DECODER

#endif // XCODE_HAVE_OPUS

