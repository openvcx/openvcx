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
#include "xcoder_silk.h"
#include "xcoder.h"
#include "ixcode.h"
#include "dlfcn.h"
#include "SKP_Silk_SDK_API.h"

#if defined(XCODE_HAVE_SILK)

#if defined(XCODE_HAVE_SILK_ENCODER)

typedef SKP_int (* SILK_GET_ENCODER_SIZE) (SKP_int32 *);
typedef SKP_int (* SILK_INIT_ENCODER) (void *, SKP_SILK_SDK_EncControlStruct *);
typedef SKP_int (* SILK_ENCODE) (void *, const SKP_SILK_SDK_EncControlStruct *,
                                 const SKP_int16 *, SKP_int, SKP_uint8 *, SKP_int16 *);

typedef struct SILK_ENCODER_CODEC_CTXT {
  void                            *psEnc;
  SKP_SILK_SDK_EncControlStruct    encControl; 
  unsigned int                     frameDurationMs;
  unsigned int                     sampleRate;
  int64_t                          pts;

  void                            *dlsilk;
  SILK_GET_ENCODER_SIZE            fGetEncoderSize;
  SILK_INIT_ENCODER                fInitEncoder;
  SILK_ENCODE                      fEncode;
} SILK_ENCODER_CODEC_CTXT_T;

static int encoder_load_dynamic(SILK_ENCODER_CODEC_CTXT_T *pSilk) {
  int rc = 0;

#if defined(XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)

  const char *dlpath = NULL;

  if( !((dlpath = LIBSILK_PATH) && (pSilk->dlsilk = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY))) &&
      !((dlpath = LIBSILK_NAME) && (pSilk->dlsilk = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY)))) {
    LOG(X_ERROR("Unable to dlopen silk encoder library: %s"), dlerror());
    return -1;
  }

  LOG(X_DEBUG("Dynamically loaded encoder: %s"), dlpath);

  if(!(pSilk->fGetEncoderSize = dlsym(pSilk->dlsilk, "SKP_Silk_SDK_Get_Encoder_Size"))) {
    LOG(X_ERROR("Unable to dlsym silk function: %s"), dlerror());
    return -1;
  }

  if(!(pSilk->fInitEncoder = dlsym(pSilk->dlsilk, "SKP_Silk_SDK_InitEncoder"))) {
    LOG(X_ERROR("Unable to dlsym silk function: %s"), dlerror());
    return -1;
  }

  if(!(pSilk->fEncode = dlsym(pSilk->dlsilk, "SKP_Silk_SDK_Encode"))) {
    LOG(X_ERROR("Unable to dlsym silk function: %s"), dlerror());
    return -1;
  }

#else

  pSilk->fGetEncoderSize = SKP_Silk_SDK_Get_Encoder_Size;
  pSilk->fInitEncoder = SKP_Silk_SDK_InitEncoder;
  pSilk->fEncode = SKP_Silk_SDK_Encode;

#endif // (XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)

  return rc;
}

static void encoder_silk_free(SILK_ENCODER_CODEC_CTXT_T *pSilk) {
 
  if(!pSilk) {
    return;
  }

  if(pSilk->psEnc) {
    free(pSilk->psEnc); 
    pSilk->psEnc = NULL;
  }

#if defined(XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)
  if(pSilk->dlsilk) {
    dlclose(pSilk->dlsilk);
    pSilk->dlsilk = NULL;
  }
#endif // (XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)

  free(pSilk);

}

static SILK_ENCODER_CODEC_CTXT_T *encoder_silk_alloc() {
  SILK_ENCODER_CODEC_CTXT_T *pSilk;
  SKP_int32 encSizeBytes;

  if(!(pSilk = (SILK_ENCODER_CODEC_CTXT_T *) calloc(1, sizeof(SILK_ENCODER_CODEC_CTXT_T)))) {
    return NULL;
  }

  if(encoder_load_dynamic(pSilk) < 0) {
    return NULL;
  }

  pSilk->pts = 0;

  //if(SKP_Silk_SDK_Get_Encoder_Size( &encSizeBytes ) != 0) {
  if(pSilk->fGetEncoderSize(&encSizeBytes) != 0) {
    LOG(X_ERROR("SKP_Silk_SDK_Get_Encoder_Size failed"));
    encoder_silk_free(pSilk);
    return NULL;
  }

  if(!(pSilk->psEnc = malloc(encSizeBytes))) {
    encoder_silk_free(pSilk);
    return NULL;
  }

  return pSilk;
}

void xcoder_silk_close_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  SILK_ENCODER_CODEC_CTXT_T *pSilk;

  if(!pCtxt || (!(pSilk = (SILK_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt))) {
    return;
  }

  encoder_silk_free(pSilk);

}


int xcoder_silk_init_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  SILK_ENCODER_CODEC_CTXT_T *pSilk = NULL;
  unsigned int frameDurationMs = 20;
  unsigned int complexity = 2;
  unsigned int bitRate;
  unsigned int bitRateConfig;
  unsigned int channels;
  int ret = 0;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(!(pSilk = (SILK_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = encoder_silk_alloc();
  }

  if(!(pSilk = (SILK_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  pCtxt->pAVCtxt->encbps = 2;

  //frameDurationMs can be one of 20,40,60,80,100
  pSilk->frameDurationMs = 20;

  pSilk->sampleRate = pCtxt->pAVCtxt->enc_samplerate = pXcode->cfgSampleRateOut;

  if((channels = pCtxt->pAVCtxt->enc_channels = pXcode->cfgChannelsOut) <= 0) {
    channels = 1;
  }
  bitRateConfig = pXcode->cfgBitRateOut;

  //
  // 0 - lowest, 2 - highest
  //
  if(complexity > 2) {
    complexity = 2;
  }

  switch(pSilk->sampleRate) {
    //case 48000:
    //  bitRate = 42000;
    //  break;
    //case 44100:
    //  bitRate = 42000;
    //  break;
    case 24000:
      bitRate = 25000;
      break;
    case 16000:
      bitRate = 18000;
      break;
    case 12000:
      bitRate = 14000;
      break;
    case 8000:
      bitRate = 10000;
      break;
    default:
      LOG(X_ERROR("Unsupported SILK output rate %dHz"), pSilk->sampleRate);
      return -1;
  }

  if(channels != 1) {
    LOG(X_ERROR("%d audio output channels not supported"), channels);
    return -1; 
  }

  if(bitRateConfig == 0) {
    bitRateConfig = bitRate;
  }

  //if((ret = SKP_Silk_SDK_InitEncoder(pSilk->psEnc, &pSilk->encControl)) != 0) {
  if((ret = pSilk->fInitEncoder(pSilk->psEnc, &pSilk->encControl)) != 0) {
    LOG(X_ERROR("SKP_Silk_SDK_InitEncoder failed"));
    return -1;
  }

  pSilk->encControl.API_sampleRate        = pSilk->sampleRate;

  pSilk->encControl.maxInternalSampleRate = pSilk->sampleRate;

  pSilk->encControl.packetSize            = ( frameDurationMs * pSilk->encControl.API_sampleRate ) / 1000;

  pSilk->encControl.packetLossPercentage  = 0;

  // Ennable inband FEC usage (0/1)
  pSilk->encControl.useInBandFEC          = 0;
  //pSilk->encControl.useInBandFEC          = 1;

  // Enable Discontinous Tx (0/1)
  pSilk->encControl.useDTX                = 0;

  // complexity, 0: low, 1: medium, 2: high
  pSilk->encControl.complexity            = complexity;

  // Target bitrate
  pSilk->encControl.bitRate               = bitRateConfig;

  LOG(X_DEBUG("Initialized SILK encoder %dHz, %dbps, channels:%d, complexity:%d, frame:%dms,%db"),
      pSilk->sampleRate, bitRateConfig, channels, complexity, frameDurationMs, pSilk->encControl.packetSize);

  return 0;
}

int xcoder_silk_encode(CODEC_WRAP_CTXT_T *pCtxt,
                       unsigned char *out_packets,
                       unsigned int out_sz,
                       const int16_t *samples,
                       int64_t *pts) {

  int ret = 0;
  SILK_ENCODER_CODEC_CTXT_T *pSilk = NULL;
  SKP_int16 nBytes;
  SKP_int16 counter;

 if(!pCtxt || !(pSilk = (SILK_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  // max payload size 
  //nBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;
  nBytes = out_sz;
  counter = pSilk->encControl.packetSize;
  
  //fprintf(stderr, "0x%x pktsz:%d, int16:0x%x, out_sz:%d, out_pkts:0x%x, counter:%d\n", pSilk->psEnc, pSilk->encControl.packetSize, samples, out_sz, out_packets, counter);
 
  //ret = SKP_Silk_SDK_Encode(pSilk->psEnc, 
  ret = pSilk->fEncode(pSilk->psEnc, 
                            &pSilk->encControl, 
                            samples, 
                            (SKP_int16)counter, 
                            out_packets, 
                            &nBytes);

  if(ret != 0) {
    LOG(X_ERROR("SKP_Silk_SDK_Encode failed.  out_sz:%d"), out_sz);
    ret = -1; 
  } else {
    pSilk->pts += pSilk->encControl.packetSize;
    ret = nBytes;
  }

  if(pts) {
    *pts = pSilk->pts;
  }

  return ret;
}

#endif // XCODE_HAVE_SILK_ENCODER

#if defined(XCODE_HAVE_SILK_DECODER)

typedef SKP_int (* SILK_GET_DECODER_SIZE) (SKP_int32 *);
typedef SKP_int (* SILK_INIT_DECODER) (void *);
typedef SKP_int (* SILK_DECODE) (void *, SKP_SILK_SDK_DecControlStruct *, SKP_int, 
                                 const SKP_uint8 *, const SKP_int nBytesIn, SKP_int16 *, SKP_int16 *);

typedef struct SILK_DECODER_CODEC_CTXT {
  void                            *psDec;
  SKP_SILK_SDK_DecControlStruct    decControl;

  void                            *dlsilk;
  SILK_GET_DECODER_SIZE            fGetDecoderSize;
  SILK_INIT_DECODER                fInitDecoder;
  SILK_DECODE                      fDecode;

} SILK_DECODER_CODEC_CTXT_T;

static int decoder_load_dynamic(SILK_DECODER_CODEC_CTXT_T *pSilk) {
  int rc = 0;

#if defined(XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)

  const char *dlpath = NULL;

  if( !((dlpath = LIBSILK_PATH) && (pSilk->dlsilk = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY))) &&
      !((dlpath = LIBSILK_NAME) && (pSilk->dlsilk = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY)))) {
    LOG(X_ERROR("Unable to dlopen silk decoderlibrary: %s"), dlerror());
    return -1;
  }

  LOG(X_DEBUG("Dynamically loaded decoder: %s"), dlpath);

  if(!(pSilk->fGetDecoderSize = dlsym(pSilk->dlsilk, "SKP_Silk_SDK_Get_Decoder_Size"))) {
    LOG(X_ERROR("Unable to dlsym silk function: %s"), dlerror());
    return -1;
  }

  if(!(pSilk->fInitDecoder = dlsym(pSilk->dlsilk, "SKP_Silk_SDK_InitDecoder"))) {
    LOG(X_ERROR("Unable to dlsym silk function: %s"), dlerror());
    return -1;
  }

  if(!(pSilk->fDecode = dlsym(pSilk->dlsilk, "SKP_Silk_SDK_Decode"))) {
    LOG(X_ERROR("Unable to dlsym silk function: %s"), dlerror());
    return -1;
  }

#else


  pSilk->fGetDecoderSize = SKP_Silk_SDK_Get_Decoder_Size;
  pSilk->fInitDecoder = SKP_Silk_SDK_InitDecoder;
  pSilk->fDecode = SKP_Silk_SDK_Decode;

#endif // (XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)

  return rc;
}

static void decoder_silk_free(void *pArg) {
  SILK_DECODER_CODEC_CTXT_T *pSilk = (SILK_DECODER_CODEC_CTXT_T *) pArg;

  if(!pSilk) {
    return;
  }

  if(pSilk->psDec) {
    free(pSilk->psDec); 
    pSilk->psDec = NULL;
  }

#if defined(XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)
  if(pSilk->dlsilk) {
    dlclose(pSilk->dlsilk);
    pSilk->dlsilk = NULL;
  }
#endif // (XCODE_HAVE_SILK_DLOPEN) && (XCODE_HAVE_SILK_DLOPEN > 0)

  free(pSilk);

}

static void *decoder_silk_alloc() {
  SILK_DECODER_CODEC_CTXT_T *pSilk;
  SKP_int32 decSizeBytes;

  if(!(pSilk = (SILK_DECODER_CODEC_CTXT_T *) calloc(1, sizeof(SILK_DECODER_CODEC_CTXT_T)))) {
    return NULL;
  }

  if(decoder_load_dynamic(pSilk) < 0) {
    return NULL;
  }

  //if(SKP_Silk_SDK_Get_Decoder_Size( &decSizeBytes ) != 0) {
  if(pSilk->fGetDecoderSize(&decSizeBytes) != 0) {
    decoder_silk_free(pSilk);
    return NULL;
  }

  if(!(pSilk->psDec = malloc(decSizeBytes))) {
    decoder_silk_free(pSilk);
    return NULL;
  }

  return pSilk;
}

void xcoder_silk_close_decoder(CODEC_WRAP_CTXT_T *pCtxt) {
  SILK_DECODER_CODEC_CTXT_T *pSilk;

  if(!pCtxt || (!(pSilk = (SILK_DECODER_CODEC_CTXT_T *) pCtxt->pctxt))) {
    return;
  }

  decoder_silk_free(pSilk);

}

int xcoder_silk_init_decoder(CODEC_WRAP_CTXT_T *pCtxt) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  SILK_DECODER_CODEC_CTXT_T *pSilk = NULL;
  int ret = 0;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(!(pSilk = (SILK_DECODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = decoder_silk_alloc();
  }

  if(!(pSilk = (SILK_DECODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  pCtxt->pAVCtxt->dec_channels = pXcode->cfgChannelsIn;
  pCtxt->pAVCtxt->decbps = 2;
  pCtxt->pAVCtxt->dec_samplerate = pXcode->cfgSampleRateIn;

  switch(pCtxt->pAVCtxt->dec_samplerate) {
    case 24000:
    case 16000:
    case 12000:
    case 8000:
      break;
    default:
      LOG(X_ERROR("Unsupported SILK decoder output sample rate %dHz"), pCtxt->pAVCtxt->dec_samplerate);
      return -1;
  }

  pSilk->decControl.framesPerPacket = 1;
  pSilk->decControl.API_sampleRate = pCtxt->pAVCtxt->dec_samplerate;

  //if((ret = SKP_Silk_SDK_InitDecoder(pSilk->psDec)) != 0) {
  if((ret = pSilk->fInitDecoder(pSilk->psDec)) != 0) {
    return -1;
  }

  LOG(X_DEBUG("Initialized SILK decoder %dHz, channels:%d"), 
    pCtxt->pAVCtxt->dec_samplerate, pCtxt->pAVCtxt->dec_channels);

  return 0;
}

int xcoder_silk_decode(CODEC_WRAP_CTXT_T *pCtxt,
                       const unsigned char *frame,
                       unsigned int frame_sz,
                       int16_t *out_samples,
                       unsigned int sz_bytes) {
  int ret;
  SILK_DECODER_CODEC_CTXT_T *pSilk = NULL;
  SKP_int lostFlag = 0;
  unsigned int outBytes = 0;
  unsigned int outSamples = 0;
  SKP_int16 len;

  if(!pCtxt || !(pSilk = (SILK_DECODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  if(!frame || frame_sz <= 0) {
    lostFlag = 1;
  }
  //fprintf(stderr, "SILK lostFlag:%d, frame_sz:%d\n", lostFlag, frame_sz);

  do {
    if(outBytes > sz_bytes) {
      ret = -1;
      break;
    }

    len = sz_bytes - outBytes;

   //fprintf(stderr, "SKP_Silk_SDK_Decode frame_sz:%d loastFlag, 0x%x 0x%x\n", frame_sz, lostFlag, frame[0], frame[1]);

    //ret = SKP_Silk_SDK_Decode(pSilk->psDec,
    ret = pSilk->fDecode(pSilk->psDec,
                              &pSilk->decControl,
                              lostFlag,
                              frame,
                              frame_sz,
                              &out_samples[outBytes],
                              &len);
    if(ret != 0) {
      LOG(X_ERROR("SKP_SIlk_SDK_Decode failed"));
      break;
    }
    outBytes += (len * sizeof(int16_t));
    outSamples += len;

  } while(pSilk->decControl.moreInternalDecoderFrames);

  if(ret != 0) {
    ret = -1;
  } else {
    ret = outBytes;
  }

  return ret;
}

#endif // XCODE_HAVE_SILK_DECODER

#endif // XCODE_HAVE_SILK
