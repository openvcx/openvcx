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


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "logutil.h"
#include "mixer/mixdefaults.h"
#include "mixer/audio_preproc.h"

#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
#include "speex/speex_preprocess.h"
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)

#if defined(XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 
#include "common_audio/vad/webrtc_vad.h"
#endif // (XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 

void audio_preproc_free(AUDIO_PREPROC_T *pPreproc) {

  pPreproc->active = 0;   
  pPreproc->cfg_vad = 0;
  pPreproc->cfg_denoise = 0;
  pPreproc->cfg_agc = 0;

#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
  if(pPreproc->pctxt) {
    speex_preprocess_state_destroy(pPreproc->pctxt);
    pPreproc->pctxt = NULL;
  }
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)

#if defined(XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 
  if(pPreproc->pctxt2) {
    WebRtcVad_Free(pPreproc->pctxt2);
    pPreproc->pctxt2 = NULL;
  }
#endif // (XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 

  if(pPreproc->buffer) {
    free(pPreproc->buffer);
    pPreproc->buffer = NULL;
  }
  //pPreproc->chunkHz = 0;

  if(pPreproc->vad_buffer) {
    free(pPreproc->vad_buffer);
    pPreproc->vad_buffer = NULL;
  }

  pthread_mutex_destroy(&pPreproc->mtx);

}

int audio_preproc_init(AUDIO_PREPROC_T *pPreproc, 
                       unsigned int clockHz, 
                       unsigned int samplesSz,
                       int vad,
                       int denoise,
                       int agc) {


  audio_preproc_free(pPreproc);

  pPreproc->clockHz = clockHz;
  pPreproc->chunkHz = pPreproc->clockHz * MIXER_PREPROC_CHUNKSZ_MS / 1000;   // 20 ms chunks

  pthread_mutex_init(&pPreproc->mtx, NULL);

  //
  // speex_preprocess_state_init returns an instance of type SpeexPreprocessState
  //
  if(denoise || agc) {
#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
    if(!(pPreproc->pctxt = (void *) speex_preprocess_state_init(pPreproc->chunkHz, pPreproc->clockHz))) {
      //audio_preproc_free(pPreproc);
      return -1;
    }
#else // 
    return -1;
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
  }

  if(vad) {
#if defined(XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 

  //int mode = 0;
  // 0 - quality
  // 1 - low bitrate
  // 2 - aggressive
  // 3 - very aggressive

  if(WebRtcVad_Create((VadInst **) &pPreproc->pctxt2) < 0) {
    LOG(X_ERROR("Failed to create VAD preprocessor"));
    audio_preproc_free(pPreproc);
    return -1;
  } else if(!pPreproc->pctxt2 || WebRtcVad_Init(pPreproc->pctxt2) < 0) {
    LOG(X_ERROR("Failed to initialize VAD preprocessor"));
    audio_preproc_free(pPreproc);
    return -1;
  } else if(WebRtcVad_set_mode(pPreproc->pctxt2, pPreproc->cfg_vad_mode)) {
    LOG(X_ERROR("Failed to set preprocessor VAD mode to %d"), pPreproc->cfg_vad_mode);
    audio_preproc_free(pPreproc);
    return -1;
  }
#else
    return -1;
#endif // (XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 

  }

  //
  // Allocate the preprocessing buffer suitable for 1 chunk size of samples
  //
  if(!(pPreproc->buffer = (int16_t *) calloc(sizeof(int16_t), pPreproc->chunkHz))) {
    audio_preproc_free(pPreproc);
    return -1;
  }

  pPreproc->bufferIdx = 0;

  if((pPreproc->cfg_vad = vad)) {

#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0) 
#if !defined(XCODE_HAVE_WEBRTC_VAD) || (XCODE_HAVE_WEBRTC_VAD == 0) 
    speex_preprocess_ctl(pPreproc->pctxt, SPEEX_PREPROCESS_SET_VAD, &pPreproc->cfg_vad);
#endif // (XCODE_HAVE_WEBRTC_VAD) || (XCODE_HAVE_WEBRTC_VAD == 0) 
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)

#if 0

    //pPreproc->cfg_vad_prob_start = 0.05f;
    //pPreproc->cfg_vad_prob_continue = 0.02f;
    //speex_preprocess_ctl(pPreproc->pctxt, SPEEX_PREPROCESS_SET_PROB_START, &pPreproc->cfg_vad_prob_start) ;
    //speex_preprocess_ctl(pPreproc->pctxt, SPEEX_PREPROCESS_SET_PROB_CONTINUE, &pPreproc->cfg_vad_prob_continue);
#endif // 0

    //
    // We shouldn't be using cfg_vad_persist, because it's way too aggressive in setting the vad to active
    // vad smoothing is used in mixer_source.c  
    //
    //pPreproc->cfg_vad_persist = 300 / (1000 / (pPreproc->clockHz / pPreproc->chunkHz )); // 300ms 
    pPreproc->cfg_vad_persist = 0 / (1000 / (pPreproc->clockHz / pPreproc->chunkHz )); // 0ms 
    pPreproc->vad_persist = 0;

    pPreproc->cfg_vad_smooth = 1;

  }


  if((pPreproc->cfg_denoise = denoise)) {
#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
    speex_preprocess_ctl(pPreproc->pctxt, SPEEX_PREPROCESS_SET_DENOISE, &pPreproc->cfg_denoise); 
#else
    return -1;
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
  }

  if((pPreproc->cfg_agc = agc)) {
#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
    speex_preprocess_ctl(pPreproc->pctxt, SPEEX_PREPROCESS_SET_AGC, &pPreproc->cfg_agc);
#else
    return -1;
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
  }

  //
  // Allocate a VAD buffer relative to the MIXER_SOURCE::buffer samples 
  // Always allocate even if cfg_vad is not enabled, because VAD confidence value may be
  // passed as input parameter when adding samples
  //
  //if(pPreproc->cfg_vad) {
    unsigned int vad_bufsz = samplesSz / pPreproc->chunkHz + 1;
    if(!(pPreproc->vad_buffer = (unsigned char *) calloc(1, vad_bufsz))) {
      audio_preproc_free(pPreproc);
      return -1;
    }
  //}

  pPreproc->active = 1;   

  return 0;
}

int audio_preproc_exec(AUDIO_PREPROC_T *pPreproc, int16_t *samples, unsigned int sz) {
  int rc = 0;

#if defined(XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)
  if(pPreproc->pctxt) {
    rc = speex_preprocess_run(pPreproc->pctxt, samples);
  }
#else
  rc = -1;
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)

#if defined(XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 

  if(pPreproc->pctxt2) {
    if((rc = WebRtcVad_Process(pPreproc->pctxt2, pPreproc->clockHz, samples, sz)) < 0) {
    }
    //fprintf(stderr, "%d", rc);
    //fprintf(stderr, "WebRtcVad_Process rc:%d\n", rc);
  }
  
#endif // (XCODE_HAVE_WEBRTC_VAD) && (XCODE_HAVE_WEBRTC_VAD > 0) 

  return rc;
}


#if 0
int audio_preproc_init(AUDIO_PREPROC_T *pPreproc,
                              unsigned int clockHz, 
                              unsigned int samplesSz,
                              int vad,
                              int denoise,
                              int agc) {
  return -1;
}

void audio_preproc_free(AUDIO_PREPROC_T *pPreproc) {
  return;
}

int audio_preproc_exec(AUDIO_PREPROC_T *pPreproc, int16_t *samples, unsigned int sz) {
  return sz;
}
#endif // (XCODE_HAVE_SPEEX) && (XCODE_HAVE_SPEEX > 0)

