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
#include "xcoder_faac.h"
#include "xcoder.h"
#include "ixcode.h"
#include "dlfcn.h"
#include "faac.h"

#if defined(XCODE_HAVE_FAAC)

typedef faacEncHandle (* FAAC_OPEN) (unsigned long, unsigned int, unsigned long *, unsigned long *);
typedef int (* FAAC_SET_CONFIGURATION) (faacEncHandle, faacEncConfigurationPtr);
typedef faacEncConfigurationPtr (* FAAC_GET_CONFIGURATION) (faacEncHandle);
typedef int (* FAAC_CLOSE) (faacEncHandle);
typedef int (* FAAC_ENCODE)(faacEncHandle, int32_t *, unsigned int, unsigned char *, unsigned int);


typedef struct FAAC_ENCODER_CODEC_CTXT {
  faacEncHandle                    faac_handle;
  int64_t                          pts;
  void                            *dlfaac;
  FAAC_SET_CONFIGURATION           fSetConfig;
  FAAC_GET_CONFIGURATION           fGetConfig;
  FAAC_ENCODE                      fEncode;
  FAAC_CLOSE                       fClose;
  FAAC_OPEN                        fOpen;
} FAAC_ENCODER_CODEC_CTXT_T;

static int encoder_load_dynamic(FAAC_ENCODER_CODEC_CTXT_T *pFaac) {
  int rc = 0;

#if defined(XCODE_HAVE_FAAC_DLOPEN) && (XCODE_HAVE_FAAC_DLOPEN > 0)

  const char *dlpath = NULL;

  if( !((dlpath = LIBFAAC_PATH) && (pFaac->dlfaac = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY))) &&
      !((dlpath = LIBFAAC_NAME) && (pFaac->dlfaac = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY)))) {
    LOG(X_ERROR("Unable to dlopen faac decoder library: %s"), dlerror());
    return -1;
  }

  LOG(X_DEBUG("Dynamically loaded decoder: %s"), dlpath);

  if(!(pFaac->fOpen = dlsym(pFaac->dlfaac, "faacEncOpen"))) {
    LOG(X_ERROR("Unable to dlsym faac function: %s"), dlerror());
    return -1;
  }

  if(!(pFaac->fClose = dlsym(pFaac->dlfaac, "faacEncClose"))) {
    LOG(X_ERROR("Unable to dlsym faac function: %s"), dlerror());
    return -1;
  }

  if(!(pFaac->fEncode = dlsym(pFaac->dlfaac, "faacEncEncode"))) {
    LOG(X_ERROR("Unable to dlsym faac function: %s"), dlerror());
    return -1;
  }

  if(!(pFaac->fSetConfig = dlsym(pFaac->dlfaac, "faacEncSetConfiguration"))) {
    LOG(X_ERROR("Unable to dlsym faac function: %s"), dlerror());
    return -1;
  }

  if(!(pFaac->fGetConfig = dlsym(pFaac->dlfaac, "faacEncGetCurrentConfiguration"))) {
    LOG(X_ERROR("Unable to dlsym faac function: %s"), dlerror());
    return -1;
  }

#else

  pFaac->fEncode = faacEncEncode;
  pFaac->fOpen = faacEncOpen;
  pFaac->fClose = faacEncClose;
  pFaac->fSetConfig = faacEncSetConfiguration;
  pFaac->fGetConfig = faacEncGetCurrentConfiguration;

#endif // (XCODE_HAVE_FAAC_DLOPEN) && (XCODE_HAVE_FAAC_DLOPEN > 0)

  return rc;
}

static void encoder_faac_free(FAAC_ENCODER_CODEC_CTXT_T *pFaac) {
 
  if(!pFaac) {
    return;
  }

  if(pFaac->faac_handle != NULL) {
    pFaac->fClose(pFaac->faac_handle);
  }

#if defined(XCODE_HAVE_FAAC_DLOPEN) && (XCODE_HAVE_FAAC_DLOPEN > 0)
  if(pFaac->dlfaac) {
    dlclose(pFaac->dlfaac);
    pFaac->dlfaac = NULL;
  }
#endif // (XCODE_HAVE_FAAC_DLOPEN) && (XCODE_HAVE_FAAC_DLOPEN > 0)

  free(pFaac);

}

static FAAC_ENCODER_CODEC_CTXT_T *encoder_faac_alloc() {
  FAAC_ENCODER_CODEC_CTXT_T *pFaac;

  if(!(pFaac = (FAAC_ENCODER_CODEC_CTXT_T *) calloc(1, sizeof(FAAC_ENCODER_CODEC_CTXT_T)))) {
    return NULL;
  }

  if(encoder_load_dynamic(pFaac) < 0) {
    return NULL;
  }

  //pFaac->pts = 0;

  return pFaac;
}

void xcoder_faac_close_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  FAAC_ENCODER_CODEC_CTXT_T *pFaac;

  if(!pCtxt || (!(pFaac = (FAAC_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt))) {
    return;
  }

  encoder_faac_free(pFaac);

}


int xcoder_faac_init_encoder(CODEC_WRAP_CTXT_T *pCtxt) {
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;
  FAAC_ENCODER_CODEC_CTXT_T *pFaac = NULL;

  static const int channel_maps[][6] = {
    { 2, 0, 1 },          //< C L R
    { 2, 0, 1, 3 },       //< C L R Cs
    { 2, 0, 1, 3, 4 },    //< C L R Ls Rs
    { 2, 0, 1, 4, 5, 3 }, //< C L R Ls Rs LFE
  };

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode;

  if(pXcode->cfgSampleRateOut == 0) {
    LOG(X_ERROR("xcode faac encoder sample rate not set"));
    return -1;
  }

  if(!(pFaac = (FAAC_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = encoder_faac_alloc();
  }

  if(!(pFaac = (FAAC_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  faacEncConfigurationPtr pFaacCfg;
  unsigned long samples_input, max_bytes_output;

  pCtxt->pAVCtxt->enc_samplerate = pXcode->cfgSampleRateOut;
  pCtxt->pAVCtxt->enc_samplefmt = SAMPLE_FMT_S16;
  pCtxt->pAVCtxt->encbps = 2;

  if((pCtxt->pAVCtxt->enc_channels = pXcode->cfgChannelsOut) < 1 || pCtxt->pAVCtxt->enc_channels > 6) {
    LOG(X_ERROR("%d audio output channels not supported"), pXcode->cfgChannelsOut);
      return -1;
  }

  if(!(pFaac->faac_handle = pFaac->fOpen(pCtxt->pAVCtxt->enc_samplerate,
                                         pCtxt->pAVCtxt->enc_channels,
                                         &samples_input, 
                                         &max_bytes_output))) {
    LOG(X_ERROR("failed to initialize faac encoder"));
    return -1;
  }

  pFaac->pts = 0;
  pFaacCfg = pFaac->fGetConfig(pFaac->faac_handle);
  if(pFaacCfg->version != FAAC_CFG_VERSION) {
    LOG(X_ERROR("Found incorrect faac version: %d not matching expected: %d"), pFaacCfg->version, FAAC_CFG_VERSION);
    return -1;
  }

  switch(pXcode->cfgProfile) {
    case MAIN:
    case LOW:
    case SSR:
    case LTP:
      pFaacCfg->aacObjectType = pXcode->cfgProfile;
      break;
    case 0:
      pFaacCfg->aacObjectType = LOW;
      break;
    default:
      LOG(X_ERROR("Unsupported AAC faac profile: %d"), pXcode->cfgProfile); 
      return -1;
  }       

  pFaacCfg->mpegVersion = MPEG4;
  pFaacCfg->useTns = 0;
  pFaacCfg->allowMidside = 1;
  pFaacCfg->bitRate = pXcode->cfgBitRateOut / pXcode->cfgChannelsOut;
  pFaacCfg->bandWidth = 0; // avctx->cutoff; 
  pFaacCfg->outputFormat = 1;
  pFaacCfg->inputFormat = FAAC_INPUT_16BIT;
  if(pXcode->cfgChannelsOut > 2) {
    memcpy(pFaacCfg->channel_map, channel_maps[pXcode->cfgChannelsOut], pXcode->cfgChannelsOut * sizeof(int));
  }

  pXcode->encoderSamplesInFrame = samples_input / pCtxt->pAVCtxt->enc_channels;

    //avctx->frame_size = samples_input / pXcode->cfgChannelsOut;
    //avctx->coded_frame= avcodec_alloc_frame();
    //avctx->coded_frame->key_frame = 1;
    /* Set decoder specific info */
    //avctx->extradata_size = 0;

  if(!pFaac->fSetConfig(pFaac->faac_handle, pFaacCfg)) {
    LOG(X_ERROR("faac output format not supported"));
    return -1;
  }

  LOG(X_DEBUG("Initialized FAAC encoder %dHz, %dbps, channels:%d, profile: %s"),
      pCtxt->pAVCtxt->enc_samplerate, pXcode->cfgBitRateOut, pXcode->cfgChannelsOut,
      pFaacCfg->aacObjectType == LOW ? "low" : pFaacCfg->aacObjectType == MAIN ? "main" :
      pFaacCfg->aacObjectType == SSR ? "ssr" : pFaacCfg->aacObjectType == LTP ? "ltp" : "");

  return 0;
}

int xcoder_faac_encode(CODEC_WRAP_CTXT_T *pCtxt,
                       unsigned char *out_packets,
                       unsigned int out_sz,
                       const int16_t *samples,
                       int64_t *pts) {

  int ret = 0;
  FAAC_ENCODER_CODEC_CTXT_T *pFaac = NULL;
  IXCODE_AUDIO_CTXT_T *pXcode = NULL;

 if(!pCtxt || !(pFaac = (FAAC_ENCODER_CODEC_CTXT_T *) pCtxt->pctxt) || 
   !(pXcode = ((IXCODE_AUDIO_CTXT_T *) pCtxt->pXcode))) {
    return -1;
  }

  ret = pFaac->fEncode(pFaac->faac_handle,
                       (int32_t *) samples,
                       pXcode->encoderSamplesInFrame * pCtxt->pAVCtxt->enc_channels,
                       out_packets,
                       out_sz);

  if(ret > 0 && pts) {
    pFaac->pts += pXcode->encoderSamplesInFrame;
    *pts = pFaac->pts;
  }

  if(ret < 0) {
    LOG(X_ERROR("faac encode failed.  output size: %d"), out_sz);
  }

  return ret;
}


#endif // XCODE_HAVE_FAAC
