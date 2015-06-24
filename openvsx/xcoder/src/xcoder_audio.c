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
#include <fcntl.h>
#include <pthread.h>
#include "logutil.h"
#include "ixcode.h"
#include "xcoder_libavcodec.h"
#include "xcoder.h"
#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
#include "mixer/mixer_api.h"
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

#if defined(XCODE_HAVE_SILK)
#include "xcoder_silk.h"
#endif // XCODE_HAVE_SILK

#if defined(XCODE_HAVE_OPUS)
#include "xcoder_opus.h"
#endif // XCODE_HAVE_OPUS

#if defined(XCODE_HAVE_FAAC)
#include "xcoder_faac.h"
#endif // XCODE_HAVE_FAAC


//#define DEBUG_MIXER_TIMING 1

//extern void av_log_set_level(int);

extern pthread_mutex_t g_xcode_mtx;
extern void ixcode_init_common(IXCODE_COMMON_CTXT_T *pCommon);

//#include "mixer/wav.h"
//static WAV_FILE_T g_wav; 

int ixcode_init_aud(IXCODE_AUDIO_CTXT_T *pXcode) {

  IXCODE_AVCTXT_T *pAvCtx = NULL;
  int rc = 0;
  CODEC_WRAP_AUDIO_ENCODER_T enc;
  CODEC_WRAP_AUDIO_DECODER_T dec;

  if(!pXcode) {
    return -1;
  }

//memset(&g_wav, 0, sizeof(g_wav)); g_wav.path="test.wav"; g_wav.channels=1; g_wav.sampleRate=8000;
//memset(&g_wav, 0, sizeof(g_wav)); g_wav.path="decoded.wav"; g_wav.channels=2; g_wav.sampleRate=48000;
//memset(&g_wav, 0, sizeof(g_wav)); g_wav.path="resampled.wav"; g_wav.channels=1; g_wav.sampleRate=8000;


#if defined(__APPLE__)
  //TODO: logger global (extern or static) is multiply defined
  // in multiple .so
  if(pXcode->common.pLogCtxt) {
    logger_setContext(pXcode->common.pLogCtxt);
  } else if(pXcode->common.cfgVerbosity > 1) {
    logger_SetLevel(S_DEBUG);
    logger_AddStderr(S_DEBUG, 0);
  } else {
    logger_SetLevel(S_INFO);
    logger_AddStderr(S_INFO, 0);
  }
#endif // __APPLE__

  memset(&enc, 0, sizeof(enc));
  enc.fInit = xcoder_libavcodec_audio_init_encoder;
  enc.fClose = xcoder_libavcodec_audio_close_encoder;
  enc.fEncode = xcoder_libavcodec_audio_encode;
  enc.ctxt.pXcode = pXcode;

  memset(&dec, 0, sizeof(dec));
  dec.fInit = xcoder_libavcodec_audio_init_decoder;
  dec.fClose = xcoder_libavcodec_audio_close_decoder;
  dec.fDecode = xcoder_libavcodec_audio_decode;
  dec.ctxt.pXcode = pXcode;

  //
  // Override any default encoder function mappings
  //
  switch(pXcode->common.cfgFileTypeOut) {

#if defined(XCODE_HAVE_AC3)
    case XC_CODEC_TYPE_AC3:
      break;
#endif // XCODE_HAVE_AC3

#if defined(XCODE_HAVE_FAAC)
    case XC_CODEC_TYPE_AAC:
      enc.fInit = xcoder_faac_init_encoder;
      enc.fClose = xcoder_faac_close_encoder;
      enc.fEncode = xcoder_faac_encode;
      break;
#endif // XCODE_HAVE_FAAC

#if defined(XCODE_HAVE_VORBIS)
    case XC_CODEC_TYPE_VORBIS:
      break;
#endif // XCODE_HAVE_VORBIS

#if defined(XCODE_HAVE_SILK)
    case XC_CODEC_TYPE_SILK:
      enc.fInit = xcoder_silk_init_encoder;
      enc.fClose = xcoder_silk_close_encoder;
      enc.fEncode = xcoder_silk_encode;
      pXcode->encoderSamplesInFrame = pXcode->cfgSampleRateOut / 50;
      break;
#endif // XCODE_HAVE_SILK

#if defined(XCODE_HAVE_OPUS)
    case XC_CODEC_TYPE_OPUS:
      enc.fInit = xcoder_opus_init_encoder;
      enc.fClose = xcoder_opus_close_encoder;
      enc.fEncode = xcoder_opus_encode;
      pXcode->encoderSamplesInFrame = pXcode->cfgSampleRateOut / 50;
      break;
#endif // XCODE_HAVE_OPUS

#if defined(XCODE_HAVE_OPENCOREAMR)
    case XC_CODEC_TYPE_AMRNB:
      break;
#endif // XCODE_HAVE_OPENCOREAMR

    case XC_CODEC_TYPE_RAWA_PCM16LE:
      enc.fEncode = NULL;
      break;
    case XC_CODEC_TYPE_RAWA_PCMULAW:
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_RAWA_PCMALAW:
    case XC_CODEC_TYPE_G711_ALAW:
      break;

    default:
      LOG(X_ERROR("Unsupported xcoder output audio codec %d"), pXcode->common.cfgFileTypeOut);
      return -1;
  }

  // fprintf(stderr, "aud type in:%d\n", pXcode->common.cfgFileTypeIn);

  //
  // Override any default decoder function mappings
  //
  switch(pXcode->common.cfgFileTypeIn) {

#if defined(XCODE_HAVE_AC3)
    case XC_CODEC_TYPE_AC3:
      break;
#endif // XCODE_HAVE_AC3

#if defined(XCODE_HAVE_AAC)
    case XC_CODEC_TYPE_AAC:
      break;
#endif // XCODE_HAVE_AAC

#if defined(XCODE_HAVE_AMR)
    case XC_CODEC_TYPE_AMRNB:
      break;
#endif // XCODE_HAVE_AMR

#if defined(XCODE_HAVE_VORBIS)
    case XC_CODEC_TYPE_VORBIS:
      break;
#endif // XCODE_HAVE_VORBIS

#if defined(XCODE_HAVE_SILK)

    case XC_CODEC_TYPE_SILK:
      dec.fInit = xcoder_silk_init_decoder;
      dec.fClose = xcoder_silk_close_decoder;
      dec.fDecode = xcoder_silk_decode;
      break;

#endif // XCODE_HAVE_SILK

#if defined(XCODE_HAVE_OPUS)

    case XC_CODEC_TYPE_OPUS:
      dec.fInit = xcoder_opus_init_decoder;
      dec.fClose = xcoder_opus_close_decoder;
      dec.fDecode = xcoder_opus_decode;
      break;

#endif // XCODE_HAVE_OPUS

    case XC_CODEC_TYPE_AUD_CONFERENCE:
    case XC_CODEC_TYPE_RAWA_PCM16LE:
      dec.fDecode = NULL;
      break;
    case XC_CODEC_TYPE_MPEGA_L2:
    case XC_CODEC_TYPE_MPEGA_L3:
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_RAWA_PCMULAW:
    case XC_CODEC_TYPE_G711_ALAW:
    case XC_CODEC_TYPE_RAWA_PCMALAW:
      break;
    default:
      LOG(X_ERROR("Unsupported xcoder input audio codec %d"), pXcode->common.cfgFileTypeIn);
      return -1;
  }

  pthread_mutex_lock(&g_xcode_mtx);

  //
  // The context was already initialized, perform a dynamic update
  //
  if(pXcode->common.pPrivData) {
    pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

    pAvCtx->volumeadjustment = pXcode->cfgVolumeAdjustment;

    pthread_mutex_unlock(&g_xcode_mtx);

    return rc;

  }

  ixcode_init_common(&pXcode->common);

  pAvCtx = pXcode->common.pPrivData = calloc(1, sizeof(IXCODE_AVCTXT_T));
  pAvCtx->enc_samplefmt = pAvCtx->dec_samplefmt = SAMPLE_FMT_S16;
  enc.ctxt.pAVCtxt = pAvCtx;
  dec.ctxt.pAVCtxt = pAvCtx;
  memcpy(&pAvCtx->out[0].encWrap.u.a, &enc, sizeof(CODEC_WRAP_AUDIO_ENCODER_T));
  memcpy(&pAvCtx->decWrap.u.a, &dec, sizeof(CODEC_WRAP_AUDIO_DECODER_T));

  //
  // Init and open the decoder
  //
  if(rc == 0 && dec.fInit && dec.fInit(&pAvCtx->decWrap.u.a.ctxt) < 0) {
    LOG(X_ERROR("Failed to open audio decoder"));
    rc = -1;
  }

  //
  // Init and open the encoder 
  //
  if(rc == 0 && enc.fInit && enc.fInit(&pAvCtx->out[0].encWrap.u.a.ctxt) < 0) {
    LOG(X_ERROR("Failed to open audio encoder"));
    rc = -1;
  } else if(rc == 0 && !enc.fInit && pXcode->cfgSampleRateOut != 0) {
    pAvCtx->enc_samplerate = pXcode->cfgSampleRateOut;  
  }

  pAvCtx->volumeadjustment = pXcode->cfgVolumeAdjustment;

  pAvCtx->resample[0].needresample = 1;
  pAvCtx->resample[0].decbufresample = NULL;
  pAvCtx->resample[0].pin_codec = (int *) &pXcode->common.cfgFileTypeIn;
  pAvCtx->resample[0].pin_samplerate = &pAvCtx->dec_samplerate;
  pAvCtx->resample[0].pin_samplefmt = &pAvCtx->dec_samplefmt;
  pAvCtx->resample[0].pin_channels = &pAvCtx->dec_channels;
  pAvCtx->resample[0].pout_codec = (int *) &pXcode->common.cfgFileTypeOut;
  pAvCtx->resample[0].pout_samplerate = &pAvCtx->enc_samplerate;
  pAvCtx->resample[0].pout_samplefmt = &pAvCtx->enc_samplefmt;
  pAvCtx->resample[0].pout_channels = &pAvCtx->enc_channels;
  pAvCtx->resample[0].pout_bps = &pAvCtx->encbps;

  pAvCtx->resample[1].needresample = 0;
  pAvCtx->resample[1].decbufresample = NULL;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

    //LOG(X_DEBUG("CHECK ENCODER SAMPLE RATE %d != MIXER SAMPLE RATE %d, pMixerParticipant: 0x%x"), pXcode->cfgSampleRateOut, pXcode->pMixerParticipant ? participant_get_config(pXcode->pMixerParticipant)->sampleRate : -1, pXcode->pMixerParticipant);

  if(pXcode->pMixerParticipant && 
     participant_get_config(pXcode->pMixerParticipant)->sampleRate != pXcode->cfgSampleRateOut) {

    //LOG(X_DEBUG("ENCODER SAMPLE RATE %d != MIXER SAMPLE RATE %d"), pXcode->cfgSampleRateOut, participant_get_config(pXcode->pMixerParticipant)->sampleRate);

    pAvCtx->resample[0].codec = XC_CODEC_TYPE_RAWA_PCM16LE;
    pAvCtx->resample[0].samplerate = participant_get_config(pXcode->pMixerParticipant)->sampleRate;
    pAvCtx->resample[0].samplefmt = SAMPLE_FMT_S16;
    pAvCtx->resample[0].channels = 1; // mixer currently supports only one channel
    pAvCtx->resample[0].pout_codec = (int *) &pAvCtx->resample[0].codec;
    pAvCtx->resample[0].pout_samplerate = &pAvCtx->resample[0].samplerate;
    pAvCtx->resample[0].pout_samplefmt = &pAvCtx->resample[0].samplefmt;
    pAvCtx->resample[0].pout_channels = &pAvCtx->resample[0].channels;

    pAvCtx->resample[1].needresample = 1;
    pAvCtx->resample[1].decbufresample = NULL;
    pAvCtx->resample[1].pin_codec = (int *) pAvCtx->resample[0].pout_codec;
    pAvCtx->resample[1].pin_samplerate = pAvCtx->resample[0].pout_samplerate;
    pAvCtx->resample[1].pin_samplefmt = pAvCtx->resample[0].pout_samplefmt;
    pAvCtx->resample[1].pin_channels = pAvCtx->resample[0].pout_channels;
    pAvCtx->resample[1].pout_codec = (int *) &pXcode->common.cfgFileTypeOut;
    pAvCtx->resample[1].pout_samplerate = &pAvCtx->enc_samplerate;
    pAvCtx->resample[1].pout_samplefmt = &pAvCtx->enc_samplefmt;
    pAvCtx->resample[1].pout_channels = &pAvCtx->enc_channels;
    pAvCtx->resample[1].pout_bps = &pAvCtx->encbps;


  }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)


  if(rc == 0) {
    pAvCtx->decbufinlen = AVCODEC_MAX_AUDIO_FRAME_SIZE * 2;
    pAvCtx->decbufin = av_malloc(pAvCtx->decbufinlen);
    pAvCtx->decbufoutlen = AVCODEC_MAX_AUDIO_FRAME_SIZE * 3;
    pAvCtx->decbufout = av_malloc(pAvCtx->decbufoutlen);

    pAvCtx->decbufidxrd = 0;
    pAvCtx->decbufidxwr = 0;
  }

  pthread_mutex_unlock(&g_xcode_mtx);

  if(rc != 0) {
    LOG(X_ERROR("Failed to init transcode audio context"));
    ixcode_close_aud(pXcode);
  }

  return rc;
}

void ixcode_close_aud(IXCODE_AUDIO_CTXT_T *pXcodeA) {
  IXCODE_AVCTXT_T *pAvCtx = NULL;
  unsigned int idx;

  if(!pXcodeA) {
    return;
  }

  //fprintf(stderr, "ixcode_close_aud 0x%x\n", pXcodeA->common.pPrivData);

  pthread_mutex_lock(&g_xcode_mtx);

  if((pAvCtx = pXcodeA->common.pPrivData)) {

    if(pAvCtx->decWrap.u.a.fClose) {
      pAvCtx->decWrap.u.a.fClose(&pAvCtx->decWrap.u.a.ctxt);
    }

    if(pAvCtx->out[0].encWrap.u.a.fClose) {
      pAvCtx->out[0].encWrap.u.a.fClose(&pAvCtx->out[0].encWrap.u.a.ctxt);
    }

    for(idx = 0; idx < 2; idx++) {
      if(pAvCtx->resample[idx].presample) {
        audio_resample_close(pAvCtx->resample[idx].presample);
        pAvCtx->resample[idx].presample = NULL;
      }

      if(pAvCtx->resample[idx].decbufresample) {
        av_free(pAvCtx->resample[idx].decbufresample);
        pAvCtx->resample[idx].decbufresample = NULL;
      }

      pAvCtx->resample[idx].needresample = 0;
    }


    if(pAvCtx->decbufin) {
      av_free(pAvCtx->decbufin);
      pAvCtx->decbufin = NULL;
    }
    pAvCtx->decbufinlen = 0;
    if(pAvCtx->decbufout) {
      av_free(pAvCtx->decbufout);
      pAvCtx->decbufout = NULL;
    }
    pAvCtx->decbufoutlen = 0;

    free(pAvCtx);
    pXcodeA->common.pPrivData = NULL;
  }

  pthread_mutex_unlock(&g_xcode_mtx);

}


static void adjust_volume(int16_t *samples, uint32_t numsamples, int adjustment) {
  unsigned int idx;
  int volume;
  int16_t *pvolume = samples;

  for(idx=0; idx < numsamples; idx++) {

    volume = ((*pvolume) * adjustment + 128) >> 8;

    if (volume < -32768) {
      volume = -32768;
    } else if (volume >  32767) {
      volume = 32767;
    }

    *pvolume++ = volume;
  }

}

/*
void testaudio(unsigned char *p, unsigned int cnt) {
  unsigned int i;
  int16_t s;

  for(i = 0; i < cnt; i++) {
    s = *((uint16_t *) &p[i * 2]);
    fprintf(stderr, "%d ", s);
  
  }

  fprintf(stderr, "\n\n");

}
*/

static int ixcode_audio_decode(IXCODE_AUDIO_CTXT_T *pXcode,
                               unsigned char *bufIn, 
                               unsigned int lenIn,
                               uint8_t **ppdecdatain) {
  int bytes = 0;
  int bytesTot = 0;
  int szRawSamplesTmp;
  int szRawSamples = 0;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  if(lenIn == 0 || !bufIn) {
    return 0;
  }

  pXcode->common.decodeInIdx++;

  if(pAvCtx->decWrap.u.a.fDecode && !pXcode->common.cfgNoDecode) {

    //
    // Decode one audio frame (codecDec.pavctx->frame_size Hz) into raw samples
    // szRawSamples will be set to the decoded buffer size
    // bytes will be set to the number of input bytes consumed
    //

    do {

      if(szRawSamples > pAvCtx->decbufinlen - pAvCtx->decbufidxwr) {
        LOG(X_WARNING("Audio decode samples buf %d exceeds %d"),
            szRawSamples,  pAvCtx->decbufinlen - pAvCtx->decbufidxwr);
        break;
      }
      szRawSamplesTmp = pAvCtx->decbufinlen - pAvCtx->decbufidxwr - szRawSamples;

      if((bytes = pAvCtx->decWrap.u.a.fDecode(&pAvCtx->decWrap.u.a.ctxt,
                                              &bufIn[bytesTot],
                                      lenIn - bytesTot,
                                     (int16_t *) ((char *) &pAvCtx->decbufin[szRawSamples]),
                                      szRawSamplesTmp)) < 0) {
        LOG(X_ERROR("Failed to decode audio frame length [%d]/%d, szraw:%d,%d (rc:%d)"),
            bytesTot, lenIn, szRawSamples, szRawSamplesTmp, bytes);
        return IXCODE_RC_ERROR_DECODE;
      } else if(bytes == 0) {
        LOG(X_WARNING("Audio decode of %d byte consumed %d byte"), lenIn - bytesTot, bytes);
        break;
      }
      bytesTot += bytes;
      szRawSamples += bytes;

      //LOG(X_DEBUG("xcoder decoded  output bytes:%d/%d -> decbufin[%d]/%d, (lenIn:%d) szR:%d(%d)"),  bytes, bytesTot, szRawSamples, pAvCtx->decbufinlen, lenIn, szRawSamplesTmp, szRawSamples);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      static int ixcode_frame_aud_cnt, ixcode_frame_aud_cnt_pip; fprintf(stderr, "xcoder decoded[%d] tid:0x%x output bytes:%d/%d -> decbufin[%d], (lenIn:%d) szR:%d(%d)\n", (pXcode->pXcodeV && pXcode->pXcodeV->pip.active) ? ixcode_frame_aud_cnt_pip++ : ixcode_frame_aud_cnt++, pthread_self(), bytes, bytesTot, szRawSamples, lenIn, szRawSamplesTmp, szRawSamples);
      //wav_write(&g_wav, &pAvCtx->decbufin[szRawSamples-bytes], bytes/2);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    //TODO: there seems to be confusion whetehre fDecode returns raw bytes/samples output, or encoded bytes consumed
    } while(bytesTot < lenIn);

    //wav_write(&g_wav, (const int16_t *) pAvCtx->decbufin, szRawSamples/2);

    if(pAvCtx->enc_samplerate == 0) {
      pAvCtx->enc_samplerate = pAvCtx->dec_samplerate;
    }

    *ppdecdatain = pAvCtx->decbufin;

  } else {

    if(lenIn > AVCODEC_MAX_AUDIO_FRAME_SIZE) {
      LOG(X_ERROR("Decode raw data input length %d exceeds max %d"), lenIn, AVCODEC_MAX_AUDIO_FRAME_SIZE);
      return IXCODE_RC_ERROR_DECODE;
    }

    *ppdecdatain = bufIn;
    szRawSamples = lenIn;
    bytes = lenIn;

  }

  pXcode->common.decodeOutIdx++;

  return szRawSamples;
}

static int ixcode_audio_resample(IXCODE_AVCTXT_RESAMPLE_T *pResample,
                                 int szRawSamples,
                                 int *pnumSamples,
                                 uint8_t **ppdecdatain) {
  if(!(*ppdecdatain)) {
    return 0;
  }

  //LOG(X_DEBUG("needresample:%d, %dHz->%dHz, presample: 0x%x"), pResample->needresample, pResample->pin_samplerate ? *pResample->pin_samplerate : -1, pResample->pin_samplerate ? *pResample->pout_samplerate : -1, pResample->presample);

  if(pResample->needresample && !pResample->presample &&
     (*pResample->pin_samplerate != *pResample->pout_samplerate ||
      *pResample->pin_samplefmt != *pResample->pout_samplefmt ||
      *pResample->pin_channels != *pResample->pout_channels)) {

    LOG(X_INFO("Resampling audio: codec:%d, %dHz/%d (%d)-> codec:%d, %dHz/%d (%d)"),
        *pResample->pin_codec, *pResample->pin_samplerate,
        *pResample->pin_channels, *pResample->pin_samplefmt,
        *pResample->pout_codec, *pResample->pout_samplerate,
        *pResample->pout_channels, *pResample->pout_samplefmt);

    if((pResample->presample = av_audio_resample_init(
                     *pResample->pout_channels, *pResample->pin_channels,
                     *pResample->pout_samplerate, *pResample->pin_samplerate,
                     *pResample->pout_samplefmt, *pResample->pin_samplefmt,
                     16, 10, 0, 0.8)) == NULL) {

      LOG(X_ERROR("Failed to init audio channel resampling context %dHz/%d (%d) -> %dHz/%d (%d)"),
              *pResample->pin_samplerate, *pResample->pin_channels, *pResample->pin_samplefmt, 
              *pResample->pout_samplerate, *pResample->pout_channels, *pResample->pout_samplefmt);

      return IXCODE_RC_ERROR_SCALE;
    }

    pResample->decbufresample = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    pResample->needresample = 0;

  }

  if(pResample->presample && *pnumSamples > 0) {

    if((*pnumSamples = audio_resample(pResample->presample,
                       (int16_t *) pResample->decbufresample,   // output
                       (int16_t *) *ppdecdatain,  // input
                       *pnumSamples)) < 0) {
      LOG(X_ERROR("Failed to resample audio"));
      return IXCODE_RC_ERROR_SCALE;
    }

    szRawSamples = *pnumSamples * *pResample->pout_bps * *pResample->pout_channels;
    *ppdecdatain = pResample->decbufresample;

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    fprintf(stderr, "xcoder resampled(0x%x) tid:0x%x numSamples:%d, szRawSamples:%d, out channels:%d, %dHz\n", pResample, pthread_self(), *pnumSamples, szRawSamples, *pResample->pout_channels, *pResample->pout_samplerate);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  }

  return szRawSamples;
}

static int ixcode_audio_encode(IXCODE_AUDIO_CTXT_T *pXcode,
                               int szRawSamples,
                               int numSamples,
                               unsigned char *bufOut, 
                               unsigned int lenOut,
                               const uint8_t *pdecdatain) {

  int rc = 0;
  //int output_mixed = 0;
  int lenOutRes = 0;
  unsigned int compoundPktIdx = 0;
  unsigned int lenEncode = 0;
  int64_t pts = 0;

  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  if(pAvCtx->out[0].encWrap.u.a.fEncode) {

    //
    // When utilizing SSE instructions, ensure decoder buffer is always % 16 aligned
    //
    //if(!output_mixed) {
      memcpy(&pAvCtx->decbufout[pAvCtx->decbufidxwr], pdecdatain, szRawSamples);
      pAvCtx->decbufidxwr += szRawSamples;
    //}

    if(pAvCtx->encbpsout > 0) {
      pXcode->encoderSamplesInFrame = numSamples;
    }

    memset(&pXcode->compoundLens, 0, sizeof(pXcode->compoundLens));

    //fprintf(stderr, "numS:%d, decbufidxrd:%d, encbps:%d, enc_channels:%d, encSamplesInFr:%d, decbufidxwr:%d\n", numSamples, pAvCtx->decbufidxrd, pAvCtx->encbps,  pAvCtx->enc_channels,  pXcode->encoderSamplesInFrame, pAvCtx->decbufidxwr);

    while(numSamples > 0 && pAvCtx->decbufidxrd + 
         (pAvCtx->encbps * pAvCtx->enc_channels * pXcode->encoderSamplesInFrame) <= pAvCtx->decbufidxwr) {

      pXcode->common.encodeInIdx++;
      if(pAvCtx->encbpsout > 0) {
        lenEncode = numSamples * pAvCtx->encbpsout * pAvCtx->enc_channels;
      } else {
        //TODO: set this to output frame size
        lenEncode = lenOut - lenOutRes;
      }
//fprintf(stderr, "LEN ENCODE:%d, encbpsout:%d, enc_chan:%d, numSamples:%d\n", lenEncode, pAvCtx->encbpsout, pAvCtx->enc_channels, numSamples);
//static int g_encodecnt;   fprintf(stderr, "xcoder encoding[%d] tid:0x%x, [compoundPktIdx:%d] numsamples:%d, szRawSamples:%d, decbufidxrd:%d decbufidxwr:%d, bytes2enc:%d, lenOut:%d\n", g_encodecnt++, pthread_self(), compoundPktIdx, numSamples, szRawSamples, pAvCtx->decbufidxrd, pAvCtx->decbufidxwr, (pAvCtx->decbufidxwr - pAvCtx->decbufidxrd), lenEncode);

#if 0
     static unsigned char s_buf_last[320];
      // Simulate silence periods
      static int s_loss_cnt;
      if(pXcode->common.encodeInIdx % 200 == 14 || (s_loss_cnt > 0 && s_loss_cnt <= 40)) {
        s_loss_cnt++;
        int iii;
        for(iii=0;iii< (pAvCtx->encbps * pAvCtx->enc_channels * pXcode->encoderSamplesInFrame); iii+=2)
            *((short *) (&pAvCtx->decbufout[pAvCtx->decbufidxrd + iii])) = 0x0000; // htons(0x7fff);
        //memcpy(&pAvCtx->decbufout[pAvCtx->decbufidxrd], s_buf_last, 320);
        fprintf(stderr, "MEMSET..\n");
      } else { s_loss_cnt = 0; memcpy(s_buf_last, &pAvCtx->decbufout[pAvCtx->decbufidxrd], 320); }
#endif // 0

//wav_write(&g_wav, &pAvCtx->decbufout[pAvCtx->decbufidxrd],pAvCtx->enc_channels * pXcode->encoderSamplesInFrame);


#if 1
      if((rc = pAvCtx->out[0].encWrap.u.a.fEncode(&pAvCtx->out[0].encWrap.u.a.ctxt, 
                                                   &bufOut[lenOutRes], 
                                                   lenEncode, 
                                                   (const short *) &pAvCtx->decbufout[pAvCtx->decbufidxrd],
                                                   &pts)) < 0) {
        LOG(X_ERROR("Failed to encode audio frame"));
        return IXCODE_RC_ERROR_ENCODE;
      }
#else
      // Raw PCM out
      //rc = 960; // 24000 / 50 * 2 
      //rc = 1764; // 44100 / 50 * 2 
      rc = pXcode->cfgSampleRateOut / 50 * 2;
      memcpy(&bufOut[lenOutRes], &pAvCtx->decbufout[pAvCtx->decbufidxrd], rc); 
#endif // 0

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      static int g_encodedcnt, g_encodedcnt_pip;   fprintf(stderr, "xcoder encoded[%d] tid:0x%x, rc: %d, [compoundPktIdx:%d] numsamples:%d, szRawSamples:%d, decbufidxrd:%d decbufidxwr:%d, bytes2enc:%d, lenOut:%d, pts:%lld\n", (pXcode->pXcodeV && pXcode->pXcodeV->pip.active) ? g_encodedcnt_pip++ : g_encodedcnt++, pthread_self(), rc, compoundPktIdx, numSamples, szRawSamples, pAvCtx->decbufidxrd, pAvCtx->decbufidxwr, (pAvCtx->decbufidxwr - pAvCtx->decbufidxrd), lenEncode, pts);

#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

      if(rc > 0) {

        lenOutRes += rc;
        pXcode->common.encodeOutIdx++;
        if(compoundPktIdx < sizeof(pXcode->compoundLens) / sizeof(pXcode->compoundLens[0])) {
          pXcode->compoundLens[compoundPktIdx].pts = pts;
          pXcode->compoundLens[compoundPktIdx++].length = rc;
        }
      }

      if(pAvCtx->encbpsout > 0) {
        pAvCtx->decbufidxrd += (rc * pAvCtx->encbps);
        break;
      } else {
        pAvCtx->decbufidxrd += (pAvCtx->encbps * pAvCtx->enc_channels * pXcode->encoderSamplesInFrame);
      }

    }

    if(pAvCtx->decbufidxrd >= pAvCtx->decbufidxwr) {
      pAvCtx->decbufidxrd = 0;
      pAvCtx->decbufidxwr = 0;
    } else if(pAvCtx->decbufidxwr >= pAvCtx->decbufoutlen / 2) {

      if(pAvCtx->decbufidxwr <= pAvCtx->decbufidxwr - pAvCtx->decbufidxrd) {
        // Unable to memcpy because contents within buffer would overlap
        LOG(X_WARNING("xcode_aud setting decoder wr idx to 0. out:%d idxrd:%d idxwr:%d"), 
                     lenOutRes, pAvCtx->decbufidxrd, pAvCtx->decbufidxwr);
        pAvCtx->decbufidxwr = 0;
      } else {

        memcpy(pAvCtx->decbufout,
               &pAvCtx->decbufout[pAvCtx->decbufidxrd], 
               pAvCtx->decbufidxwr - pAvCtx->decbufidxrd);
        pAvCtx->decbufidxwr = pAvCtx->decbufidxwr - pAvCtx->decbufidxrd;
        pAvCtx->decbufidxrd = 0;
      }

    }

  } else {

    if(szRawSamples > lenOut) {
      return IXCODE_RC_ERROR_ENCODE;
    }

    pXcode->encoderSamplesInFrame = numSamples;
    memcpy(bufOut, pdecdatain, szRawSamples); 
    lenOutRes = szRawSamples;

    pXcode->common.encodeInIdx++;
    pXcode->common.encodeOutIdx++;
  }

  return lenOutRes;
}

enum IXCODE_RC ixcode_frame_aud(IXCODE_AUDIO_CTXT_T *pXcode, 
                                unsigned char *bufIn, unsigned int lenIn,
                                unsigned char *bufOut, unsigned int lenOut) {
  IXCODE_AVCTXT_T *pAvCtx;
  int numSamples = 0;
  int szRawSamples = 0;
  int lenOutRes = 0;
  //int output_mixed = 0;
  uint8_t *pdecdatain = NULL;
#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  //unsigned int numSamplesConsumed;
  //uint64_t pts;
  IXCODE_VIDEO_CTXT_T *pXcodeV = NULL;
#endif // XCODE_HAVE_PIP_AUDIO


#if defined(XCODE_PROFILE_AUD)
  struct timeval gtv[10];
  memset(gtv, 0, sizeof(gtv));
  gettimeofday(&gtv[0], NULL);
#endif // XCODE_PROFILE_AUD

  if(!pXcode || !pXcode->common.pPrivData || !bufIn || !bufOut) {
    return IXCODE_RC_ERROR;
  }

  pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[2], NULL);
#endif // XCODE_PROFILE_AUD

  //fprintf(stderr, "xcoder xcode_aud tid:0x%x, bufIn:0x%x, lenIn:%d, bufOut:0x%x, lenOut:%d, pMixerParticipant:0x%x\n", pthread_self(), bufIn, lenIn, bufOut, lenOut, pXcode->pMixerParticipant);

  //
  // Decode the input samples
  //
  if((szRawSamples = ixcode_audio_decode(pXcode, bufIn, lenIn, &pdecdatain)) < 0) {
    return (enum IXCODE_RC) szRawSamples;
  }

  //
  // If no output buffer has been set we're all done
  //
  if(!bufOut || lenOut == 0) {
    return IXCODE_RC_OK; 
  }

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[3], NULL);
  pAvCtx->prof.num_adecodes++;
#endif // XCODE_PROFILE_AUD

  //szRawSamples = 1960 ; 
  //fprintf(stderr, "aud[%d] processed %d/%d \n", pXcode->common.decodeInIdx, szRawSamples, lenIn);

  //
  // numSamples is the # of decoded samples (Hz) per channel
  //
  numSamples = szRawSamples / pAvCtx->decbps / pAvCtx->dec_channels;

  //
  // Do any volume adjustment
  //
  if(pAvCtx->volumeadjustment != 0 && pdecdatain && pAvCtx->volumeadjustment != 256 &&
     pAvCtx->dec_samplefmt == SAMPLE_FMT_S16) {
    adjust_volume((int16_t *) pdecdatain, szRawSamples / 2, pAvCtx->volumeadjustment);
  }

  //
  // Resample the input frequency to the output
  //
  if((szRawSamples = ixcode_audio_resample(&pAvCtx->resample[0], szRawSamples, &numSamples, &pdecdatain)) < 0) {
    return (enum IXCODE_RC) szRawSamples;
  }

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  pXcodeV = pXcode->pXcodeV;

  if(pXcodeV && pXcode->pMixerParticipant && pdecdatain) {

//static struct timeval gt0; if(gt0.tv_sec==0) gettimeofday(&gt0, NULL); struct timeval gt; gettimeofday(&gt, NULL); if(((gt.tv_sec-gt0.tv_sec)*1000)+((gt.tv_usec-gt0.tv_usec)/1000) > 4000) { memset(pdecdatain, 0, numSamples * 2); }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    if(pXcodeV->pip.active) {
      static int g_addmixer_pip, g_addmixer_pip_samples; g_addmixer_pip_samples+=numSamples; fprintf(stderr, "xcoder tid:0x%x adding[%d] %d (tot:%d) samples to mixer (vad:%d) from pip[%d]\n", pthread_self(), g_addmixer_pip++, numSamples, g_addmixer_pip_samples, pXcode->vadLatest, pXcodeV->pip.indexPlus1);
    } else if(pXcode->pMixer) {
      static int g_addmixer_ov, g_addmixer_ov_samples; g_addmixer_ov_samples+=numSamples, fprintf(stderr, "xcoder tid:0x%x adding[%d] %d (tot:%d) samples to mixer (vad:%d) from overlay source\n", pthread_self(), g_addmixer_ov++, numSamples, g_addmixer_ov_samples, pXcode->vadLatest);
    }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[6], NULL);
#endif // XCODE_PROFILE_AUD

    mixer_on_receive_audio_pcm(pXcode->pMixerParticipant, (const int16_t *) pdecdatain, numSamples, 
                               &pXcode->vadLatest); 
    //fprintf(stderr, "%d", pXcode->vadLatest);

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[7], NULL);
#endif // XCODE_PROFILE_AUD

  }

  if(pXcodeV && pXcode->pMixerParticipant) {

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[8], NULL);
#endif // XCODE_PROFILE_AUD

    numSamples = 0;
    pdecdatain = pAvCtx->decbufin;

    if((numSamples = mixer_read_audio_pcm(pXcode->pMixerParticipant, 
                                      (int16_t *) pdecdatain,
                                      0,
                                      AVCODEC_MAX_AUDIO_FRAME_SIZE,
                                      NULL)) < 0) {
      return IXCODE_RC_ERROR_MIXER;
    }

    szRawSamples = numSamples * pAvCtx->encbps * pAvCtx->enc_channels;

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[9], NULL);
#endif // XCODE_PROFILE_AUD

    //TODO: resample from mixer clock to encoder output clock

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    if(pXcodeV->pip.active) { static int g_readmixer_pip, g_readmixer_pip_samples; g_readmixer_pip_samples += numSamples; fprintf(stderr, "xcoder tid:0x%x read[%d] %d (tot:%d) samples (szRawSamples:%d) from mixer\n", pthread_self(), g_readmixer_pip++, numSamples, g_readmixer_pip_samples, szRawSamples); }
    if(pXcode->pMixer) { static int g_readmixer_ov, g_readmixer_ov_samples; g_readmixer_ov_samples += numSamples; fprintf(stderr, "xcoder tid:0x%x read[%d] %d (tot:%d) samples (szRawSamples:%d) from mixer\n", pthread_self(), g_readmixer_ov++, numSamples, g_readmixer_ov_samples, szRawSamples); }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    //
    // Resample the mixer output frequency to the encoder output
    //
    if((szRawSamples = ixcode_audio_resample(&pAvCtx->resample[1], szRawSamples, &numSamples, &pdecdatain)) < 0) {
      return (enum IXCODE_RC) szRawSamples;
    }

  }

#endif //XCODE_HAVE_PIP_AUDIO


#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[4], NULL);
#endif // XCODE_PROFILE_AUD


  //
  // Encode the PCM samples
  //
  if((lenOutRes = ixcode_audio_encode(pXcode, szRawSamples, numSamples, bufOut, lenOut, pdecdatain)) < 0) {
    return (enum IXCODE_RC) lenOutRes;
  }

  //fprintf(stderr, "xcode_aud tid:0x%x called ixcode_audio_encode numS:%d, pXcodeV:0x%x, pip.active:%d, lenOutRes:%d, fEncode:0x%x\n", pthread_self(), numSamples, pXcodeV, pXcodeV ? pXcodeV->pip.active : 0, lenOutRes, pAvCtx->out[0].encWrap.u.a.fEncode);

#if defined(XCODE_PROFILE_AUD)
  gettimeofday(&gtv[5], NULL);
  gettimeofday(&gtv[1], NULL);
  pAvCtx->prof.num_aencodes++;
  pAvCtx->prof.num_a++;
  pAvCtx->prof.tottime_adecodes += (uint64_t) ((gtv[3].tv_sec-gtv[2].tv_sec)*1000000)+
                                 ((gtv[3].tv_usec-gtv[2].tv_usec));
  pAvCtx->prof.tottime_aencodes += (uint64_t) ((gtv[5].tv_sec-gtv[4].tv_sec)*1000000)+
                                 ((gtv[5].tv_usec-gtv[4].tv_usec));
  pAvCtx->prof.tottime_a        += (uint64_t) ((gtv[1].tv_sec-gtv[0].tv_sec)*1000000)+
                                 ((gtv[1].tv_usec-gtv[0].tv_usec));

  LOG(X_DEBUG("xcode_aud done in %ld(%.1f) (dec:%ld(%.1f), enc:%ld(%.1f), mix-wr:%ld, mix-rd:%ld) (len:%d)"), 

   (uint32_t)((gtv[1].tv_sec-gtv[0].tv_sec)*1000000)+((gtv[1].tv_usec-gtv[0].tv_usec)),
     (float)pAvCtx->prof.tottime_a / pAvCtx->prof.num_a/1000.0f,
   ((gtv[3].tv_sec-gtv[2].tv_sec)*1000000)+((gtv[3].tv_usec-gtv[2].tv_usec)),
    pAvCtx->prof.num_adecodes > 0 ? (float)pAvCtx->prof.tottime_adecodes / pAvCtx->prof.num_adecodes/1000.0f : 0,

   ((gtv[5].tv_sec-gtv[4].tv_sec)*1000000)+((gtv[5].tv_usec-gtv[4].tv_usec)),
    pAvCtx->prof.num_aencodes > 0 ? (float)pAvCtx->prof.tottime_aencodes / pAvCtx->prof.num_aencodes /1000.0f : 0,
   ((gtv[7].tv_sec-gtv[6].tv_sec)*1000000)+((gtv[7].tv_usec-gtv[6].tv_usec)),
   ((gtv[9].tv_sec-gtv[8].tv_sec)*1000000)+((gtv[9].tv_usec-gtv[8].tv_usec)),
     lenOutRes);
#endif // XCODE_PROFILE_AUD

  return (enum IXCODE_RC) lenOutRes;
}

