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



#if defined(__linux__)
#define _GNU_SOURCE 1
#endif // __linux__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "logutil.h"
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
#include "rtplib.h"
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
#include "mixer/conference.h"
#include "mixer/participant.h"

#define LOG_CTXT  (pParticipant ? pParticipant->logCtxt : NULL)

#define ENCODED_AUDIO_BUF_SAMPLES_SZ   2048 

//extern char *strcasestr(const char *haystack, const char *needle);


static void lock_set_flag(PARTICIPANT_T *pParticipant, enum THREAD_FLAG *pflag, enum THREAD_FLAG value) {

  pthread_mutex_lock(&pParticipant->mtx);

  if(*pflag != THREAD_FLAG_STOPPED) {
    *pflag = value;
  }
 
  pthread_mutex_unlock(&pParticipant->mtx);

}

static void participant_destroy(PARTICIPANT_T **ppParticipant) {

  if(*ppParticipant) {

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
    //
    // Delete the audio decoder context
    //
    if((*ppParticipant)->pSilkDecoder) {
      decoder_silk_free((*ppParticipant)->pSilkDecoder);
      (*ppParticipant)->pSilkDecoder = NULL;
    }

    //
    // Delete the audio encoder context
    //
    if((*ppParticipant)->pSilkEncoder) {
      encoder_silk_free((*ppParticipant)->pSilkEncoder);
      (*ppParticipant)->pSilkEncoder = NULL;
    }
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

    if((*ppParticipant)->pAudioBuf) {
      free((*ppParticipant)->pAudioBuf);
      (*ppParticipant)->pAudioBuf = NULL;
    }

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
    //
    // Delete the frame queue
    //
    if((*ppParticipant)->pFrameQ) {
      frameq_destroy((*ppParticipant)->pFrameQ);
      (*ppParticipant)->pFrameQ= NULL;
    }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

    //
    // Delete the mixer resources
    //
    if((*ppParticipant)->pMixerSource) {

      mixer_removesource((*ppParticipant)->pConference->pMixer, (*ppParticipant)->pMixerSource);

      mixer_source_free((*ppParticipant)->pMixerSource);
      (*ppParticipant)->pMixerSource = NULL;
    }

    //
    // Delete any .wav output context
    //
    if((*ppParticipant)->pWavDecoded) {

      wav_close((*ppParticipant)->pWavDecoded);

      free((*ppParticipant)->pWavDecoded->path);
      (*ppParticipant)->pWavDecoded->path = NULL;

      free((*ppParticipant)->pWavDecoded);
      (*ppParticipant)->pWavDecoded = NULL;

    }

    if((*ppParticipant)->pWavOutput) {

      wav_close((*ppParticipant)->pWavOutput);

      free((*ppParticipant)->pWavOutput->path);
      (*ppParticipant)->pWavOutput->path = NULL;

      free((*ppParticipant)->pWavOutput);
      (*ppParticipant)->pWavOutput = NULL;

    }

    pthread_mutex_destroy(&(*ppParticipant)->mtx);
    free(*ppParticipant);
    *ppParticipant = NULL;
  }
}

static char *create_wav_path(const char *base, const PARTICIPANT_ID id) {
  size_t sz;
  char idstr[32];
  char *path, *p;

  sz = strlen(base);
  path = (char *) calloc(1, sz + sizeof(idstr));
  strcpy(path, base);
  if((p = strcasestr((const char *) path, ".wav"))) {
    *p = '\0';
  }
  snprintf(idstr, sizeof(idstr) -1, "_%d.wav", id);
  strcat(path, idstr);

  return path;
}


static int participant_stop(PARTICIPANT_T *pParticipant) {

  lock_set_flag(pParticipant, &pParticipant->tdFlagOutput, THREAD_FLAG_DOEXIT);

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  lock_set_flag(pParticipant, &pParticipant->tdFlagCap, THREAD_FLAG_DOEXIT);
  lock_set_flag(pParticipant, &pParticipant->tdFlagUnpack, THREAD_FLAG_DOEXIT);

  if(pParticipant->pRtplib) {
    rtplib_stop(pParticipant->pRtplib);
  }

  if(pParticipant->pRtpstream) {
    rtpstream_stop(pParticipant->pRtpstream);
  }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  mixer_cond_broadcast(&pParticipant->pConference->condOutput, &pParticipant->pConference->mtxOutput);

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
  if(pParticipant->pFrameQ) {
    mixer_cond_broadcast(&pParticipant->pFrameQ->cond, &pParticipant->pFrameQ->mtxCond);
  }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  return 0;
}

static PARTICIPANT_T *participant_create(AUDIO_CONFERENCE_T *pConference, 
                                         PARTICIPANT_CONFIG_T *pConfig) {
  PARTICIPANT_T *pParticipant = NULL;
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
  const unsigned int frameQMax = 50;
  char buf[PARTICIPANT_DESCRIPTION_BUFSZ];
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  if(!(pParticipant = (PARTICIPANT_T *) calloc(1, sizeof(PARTICIPANT_T)))) {
    return NULL;
  }

  pParticipant->logCtxt = pConference->logCtxt;
  pthread_mutex_init(&pParticipant->mtx, NULL);

  pParticipant->pConference = pConference;
  memcpy(&pParticipant->config, pConfig, sizeof(pParticipant->config));
  //pParticipant->config.pAudioConfig = NULL;
  if(pConfig->pAudioConfig) {
    memcpy(&pParticipant->audioConfig, pConfig->pAudioConfig,  sizeof(AUDIO_CONFIG_T));
  } else {
    //memcpy(&pParticipant->audioConfig, &pConference->defaultConfig,  sizeof(AUDIO_CONFIG_T));
    memcpy(&pParticipant->audioConfig, &pConference->cfg.ac,  sizeof(AUDIO_CONFIG_T));
  }
  pParticipant->config.pAudioConfig = &pParticipant->audioConfig;

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

  pParticipant->decoderConfig.sampleRate = pConference->cfg.sampleRate;
  pParticipant->encoderConfig.sampleRate = pConference->cfg.sampleRate;

  pParticipant->encoderConfig.complexity = pParticipant->audioConfig.encoderComplexity;
  pParticipant->encoderConfig.bitRate = pParticipant->audioConfig.encoderBitrate;

#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
  
  //
  // Create the audio mixer source instance
  //
  if(!(pParticipant->pMixerSource = mixer_source_init(pParticipant->logCtxt,
                                  pParticipant->config.input.type > PARTICIPANT_TYPE_NONE ? 2.0f : 0.0f,
                                  1.0f,
                                  pConference->cfg.sampleRate,
                                  pParticipant->pConference->pMixer->thresholdHz,
                                  pParticipant->audioConfig.mixerVad ? 1 : 0,
                                  pParticipant->audioConfig.mixerVad ? pParticipant->audioConfig.mixerVad - 1 : 0,
                                  pParticipant->audioConfig.mixerDenoise,
                                  pParticipant->audioConfig.mixerAgc,
                                  pParticipant->config.output.type > PARTICIPANT_TYPE_NONE ? 1 : 0,
                                  pParticipant->audioConfig.includeSelfChannel,
                                  //pParticipant->audioConfig.setOutputVad,
                                  pParticipant->audioConfig.acceptInputVad))) {

    LOG(X_ERROR("Failed to initialize mixer for source participant id %d"), pParticipant->config.id);
    participant_destroy(&pParticipant);
    return NULL;
  }

  pParticipant->pMixerSource->id = pParticipant->config.id;
  pParticipant->pMixerSource->pMixer = pParticipant->pConference->pMixer;

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  //
  // Create the frame queue of RTP packet payloads
  //
  if(IS_PARTICIPANT_TYPE_RTP(pParticipant->config.input.type) &&
    !(pParticipant->pFrameQ = frameq_create(frameQMax))) {
    LOG(X_ERROR("Failed to initialize frame queue of max size %d for source participant %s"), 
                frameQMax, participant_description(&pParticipant->config, buf, sizeof(buf)));
    participant_destroy(&pParticipant);
    return NULL;
  }

  if(IS_PARTICIPANT_TYPE_RTP(pParticipant->config.output.type)) {

    if(!(pParticipant->pAudioBuf = (int16_t *) calloc(sizeof(int16_t), ENCODED_AUDIO_BUF_SAMPLES_SZ))) {
      participant_destroy(&pParticipant);
      return NULL;
    }
    pParticipant->audioBufIndexHz = 0;

  }

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

  //
  // Create the audio decoder context
  //
  if(IS_PARTICIPANT_TYPE_SILK(pParticipant->config.input.type)) {

    if(!(pParticipant->pSilkDecoder = decoder_silk_alloc(LOG_CTXT)) ||
     decoder_silk_init(pParticipant->pSilkDecoder, &pParticipant->decoderConfig) != 0) {
      LOG(X_ERROR("Failed to initialize silk decoder for source participant id %s"), 
                  participant_description(&pParticipant->config, buf, sizeof(buf)));
      participant_destroy(&pParticipant);
      return NULL;
    }
  }

  //
  // Create the audio encoder context
  //
  if(IS_PARTICIPANT_TYPE_SILK(pParticipant->config.output.type)) {

    if((!(pParticipant->pSilkEncoder = encoder_silk_alloc(LOG_CTXT)) ||
        encoder_silk_init(pParticipant->pSilkEncoder, &pParticipant->encoderConfig)) != 0) {
      LOG(X_ERROR("Failed to initialize silk encoder for source participant %s"), 
                  participant_description(&pParticipant->config, buf, sizeof(buf)));
      participant_destroy(&pParticipant);
      return NULL;
    }
    pParticipant->audioFrameDurationHz = encoder_silk_getframesizeHz(pParticipant->pSilkEncoder);
    LOG(X_DEBUG("Initialized SILK encoder %dHz, %dbps, complexity:%d, frame size:%dHz for source participant %s"), 
       pParticipant->encoderConfig.sampleRate, pParticipant->encoderConfig.bitRate, 
       pParticipant->encoderConfig.complexity, pParticipant->audioFrameDurationHz,
       participant_description(&pParticipant->config, buf, sizeof(buf)));
  }

#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

  //
  // Create optional .wav file output
  //
  if(pParticipant->audioConfig.decodedWavPath &&
    (pParticipant->pWavDecoded = (WAV_FILE_T *) calloc(1, sizeof(WAV_FILE_T)))) {
    
    pParticipant->pWavDecoded->path = create_wav_path(pParticipant->audioConfig.decodedWavPath, 
                                                      pParticipant->config.id);
    pParticipant->pWavDecoded->channels = 1;
    pParticipant->pWavDecoded->sampleRate = pConference->cfg.sampleRate;

  }

  if(pParticipant->audioConfig.mixedWavPath &&
    (pParticipant->pWavOutput = (WAV_FILE_T *) calloc(1, sizeof(WAV_FILE_T)))) {

    pParticipant->pWavOutput->path = create_wav_path(pParticipant->audioConfig.mixedWavPath, 
                                                     pParticipant->config.id);
    pParticipant->pWavOutput->channels = 1;
    pParticipant->pWavOutput->sampleRate = pConference->cfg.sampleRate;

  }

  //fprintf(stderr, "participant_cbOnPacket port:%d\n", pParticipant->config.input.u.rtp.rtpPort);

  return pParticipant;
}


int participant_add_samples_to_mixer(PARTICIPANT_T *pParticipant, 
                                     const int16_t *pSamples, 
                                     unsigned int numSamples,
                                     unsigned int tsHz,
                                     int vad,
                                     int *pvad) {

  int rc;

  //fprintf(stderr, "add_samples_to_mixer %uHz numSamples:%d\n", tsHz, numSamples);

  //
  // On the very first call (1st input sample rcvd) add the source channel to the mixer
  //
  if(pParticipant->framesMixed == 0) {
    if((rc = mixer_addsource(pParticipant->pConference->pMixer, pParticipant->pMixerSource)) < 0) {
      LOG(X_ERROR("Failed to add source channel to mixer"));
      participant_stop(pParticipant);
      return -1;
    }
  }
  pParticipant->framesMixed++;

  //
  // resampling sample_rate to mixer freq (24KHz) may not be necessary with silk,
  // as output can always be delivered at decoder configured sample rate
  //
  
  if(pParticipant->pWavDecoded) {
    wav_write(pParticipant->pWavDecoded, pSamples, numSamples);
  }

  //
  // Add the source channel samples to the mixer
  // the tsHz should correspond to the local clock of the first sample
  //
  // TODO: create absolute per-channel clock offset
  if((rc = mixer_addbuffered(pParticipant->pMixerSource,
                             pSamples, 
                             numSamples,
                             1,
                             tsHz,
                             vad,
                             pvad)) < 0) {
    return rc;
  }

  return 0;
}

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

static uint64_t handle_lost_input(PARTICIPANT_T *pParticipant,
                             const FRAME_NODE_T *pNode,
                             int lostFrames) {
  int sz;
  int64_t tsHzGap;
  uint64_t tsHz;
  int16_t pcmSamplesBuf[sizeof(int16_t) * 4096];

  LOG(X_DEBUG("Handling %d lost frames for source participant %s"), lostFrames, 
    participant_description(&pParticipant->config, (char *) pcmSamplesBuf, sizeof(pcmSamplesBuf)));

  tsHz = (pNode->ts - pParticipant->tsFirstFrame) + pParticipant->tmOffsetFromStartHz;

  if(IS_PARTICIPANT_TYPE_PCM(pParticipant->config.input.type)) {

    memset(pcmSamplesBuf, sizeof(pcmSamplesBuf), 0);
    tsHzGap = tsHz - pParticipant->lastRcvdTs;
    tsHz = pParticipant->lastRcvdTs;

    while(tsHzGap > 0) {
      sz = MIN(sizeof(pcmSamplesBuf) / sizeof(int16_t), tsHzGap);
      participant_add_samples_to_mixer(pParticipant, pcmSamplesBuf, sz, tsHz, 0, NULL);
      tsHz += sz;
      tsHzGap -= sz;
    }

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

  } else if(IS_PARTICIPANT_TYPE_SILK(pParticipant->config.input.type)) {

    tsHz = pParticipant->lastRcvdTs;

    while(lostFrames-- > 0) {

      if((sz = decoder_silk_decode(pParticipant->pSilkDecoder,
                               NULL,
                               0,
                               pcmSamplesBuf,
                               sizeof(pcmSamplesBuf))) < 0) {
        LOG(X_ERROR("decoder_silk_decode failed %d for frame length %d, rtp seq:%u, ts:%u"), sz);

      } else if(sz > 0) {
        participant_add_samples_to_mixer(pParticipant, pcmSamplesBuf, sz, tsHz, -1, NULL);
        tsHz += sz;
      }

    } // end of while...

#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

  } // end of if(IS_PARTICIPANT_TYPE_ ...

  return tsHz;
}

static void participant_reset(PARTICIPANT_T *pParticipant) {

  mixer_removesource(pParticipant->pConference->pMixer, pParticipant->pMixerSource);

  //
  // Initialization
  //
  pParticipant->framesRcvd = 0;
  pParticipant->framesMixed = 0;
  pParticipant->lastRcvdSeq = 0;
  pParticipant->lastRcvdTs = 0;


}

static void participant_unpackdata_runproc(void *pArg) {
  int rc;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pArg;
  FRAME_NODE_T *pNode;
  int lostFrames;
  int16_t pcmSamplesBuf[sizeof(int16_t) * 4096];
  int16_t *pcmSamples;
  uint64_t tsHz;
  int vad = -1;

  //
  // This thread procedure is used to read en-queued RTP frames 
  //

  LOG(X_DEBUG("Mixer decoder thread starting for source participant %s"), 
    participant_description(&pParticipant->config, (char *) pcmSamplesBuf, sizeof(pcmSamplesBuf)));

  //
  // Initialization
  //
  pParticipant->tdFlagUnpack = THREAD_FLAG_RUNNING;
  participant_reset(pParticipant);

  while(pParticipant->tdFlagUnpack == THREAD_FLAG_RUNNING) {

    //
    // Get a frame from the frame queue
    //
    if((pNode = frameq_popWait(pParticipant->pFrameQ))) {

      if(pParticipant->framesRcvd == 0 || pParticipant->pMixerSource->needInputReset) {

        pParticipant->tmOffsetFromStartHz = participant_on_first_pcm(pParticipant, pNode->captureTm, pNode->ts);
        pParticipant->pMixerSource->needInputReset = 0;

      }
      
      tsHz = (pNode->ts - pParticipant->tsFirstFrame) + pParticipant->tmOffsetFromStartHz;

      LOG(X_DEBUGV("frameq_pop audio len:%d seq:%u, ts:%u, tsHz:%lluHz, tmOffsetFromStart:%lluHz"), 
          pNode->lenData, pNode->sequence, pNode->ts, tsHz, pParticipant->tmOffsetFromStartHz);

      //
      // Handle any lost packets by looking at the gap in sequence #
      //
      if(pParticipant->framesRcvd > 0 && 
        (lostFrames = pNode->sequence - pParticipant->lastRcvdSeq - 1) > 0) {

        tsHz = handle_lost_input(pParticipant, pNode, lostFrames);

      }

      rc = 0;
      pcmSamples = pcmSamplesBuf;

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

      if(IS_PARTICIPANT_TYPE_SILK(pParticipant->config.input.type)) {

        //
        // Decode the received audio frame
        // 
        if((rc = decoder_silk_decode(pParticipant->pSilkDecoder,
                               pNode->pData,
                               pNode->lenData,
                               pcmSamplesBuf,
                               sizeof(pcmSamplesBuf))) < 0) {
          LOG(X_ERROR("decoder_silk_decode failed %d for frame length %d, rtp seq:%u, ts:%u"), rc, 
                             pNode->sequence, pNode->lenData, pNode->ts);
        }

      } else 
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

      if(IS_PARTICIPANT_TYPE_PCM(pParticipant->config.input.type)) {

        if(pNode->lenData % sizeof(int16_t) == 1) {
          vad = ((unsigned char *) pNode->pData)[pNode->lenData - 1];
        } else {
          vad = -1;
        }

        pcmSamples = pNode->pData;
        rc = pNode->lenData / sizeof(int16_t); 

        //fprintf(stderr, "PCM INPUT FROM RTP samples:%d, length:%d haveVad:%d, vad:%d\n", rc, pNode->lenData, haveVad, vad);
      }

      if(rc > 0) {
        participant_add_samples_to_mixer(pParticipant, pcmSamples, rc, tsHz, vad, NULL);
        tsHz += rc; 
      }

      //fprintf(stderr, "decoder node len:%d seq:%u, ts:%u 0x%x 0x%x 0x%x 0x%x decoded %d samples, tsHz:%lluHz\n", pNode->lenData, pNode->sequence, pNode->ts, ((unsigned char *)pNode->pData)[0], ((unsigned char *)pNode->pData)[1], ((unsigned char *)pNode->pData)[2], ((unsigned char *)pNode->pData)[3] , rc, tsHz);
//for(i=0;i<rc;i++)fprintf(stderr, "0x%x ", pcmSamplesBuf[i]);

      //
      // Update frame based statistics counters
      //
      pParticipant->framesRcvd++; 
      pParticipant->lastRcvdSeq = pNode->sequence;
      pParticipant->lastRcvdTs = tsHz;


//static int fd; if(!fd) fd = open("decoded.pcm", O_RDWR | O_CREAT, 0666);
//write(fd, pcmSamplesBuf, rc);

      //
      // Return the borrowed frame to the frame queue
      //
      frameq_recycle(pParticipant->pFrameQ, pNode);

    } else {

      //
      // Should never really get here but sleep and continue just in case
      //
      usleep(5000);
    }
  }

  //
  // Remove the audio source from the mixer
  //
  mixer_removesource(pParticipant->pConference->pMixer, pParticipant->pMixerSource);

  LOG(X_DEBUG("Mixer decoder thread ending for source participant %s"), 
    participant_description(&pParticipant->config, (char *) pcmSamplesBuf, sizeof(pcmSamplesBuf)));

  //
  // Terminate this participant
  //
  participant_stop(pParticipant);

  pParticipant->tdFlagUnpack = THREAD_FLAG_STOPPED;

}

static void participant_recvdata_runproc(void *pArg) {
  int rc;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pArg;
  char buf[PARTICIPANT_DESCRIPTION_BUFSZ];

  //
  // This thread procedure is used to run the RTP receiver library
  //

  pParticipant->tdFlagCap = THREAD_FLAG_RUNNING;

  LOG(X_DEBUG("Mixer data receiver thread starting for source participant %s"), 
    participant_description(&pParticipant->config, buf, sizeof(buf)));
 
  rc = rtplib_run(pParticipant->pRtplib);

  LOG(X_DEBUG("Mixer data receiver thread ending for source participant %s"), 
    participant_description(&pParticipant->config, buf, sizeof(buf)));

  //
  // Terminate this participant
  //
  participant_stop(pParticipant);

  pParticipant->tdFlagCap = THREAD_FLAG_STOPPED;

}


static int xmitAudio(PARTICIPANT_T *pParticipant,
                       const int16_t *pcmSamples,
                       unsigned int numSamples,
                       uint64_t tsHz,
                       int vad) {
  int rc = 0;
  unsigned int indexHz = 0;
  unsigned char buf[sizeof(int16_t) * ENCODED_AUDIO_BUF_SAMPLES_SZ];
  unsigned char *pOutData = NULL;

  memcpy(&pParticipant->pAudioBuf[pParticipant->audioBufIndexHz], pcmSamples, sizeof(int16_t) * numSamples);
  pParticipant->audioBufIndexHz += numSamples;

  while(indexHz < pParticipant->audioBufIndexHz) {

    rc = -1;

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

    if(IS_PARTICIPANT_TYPE_SILK(pParticipant->config.output.type)) {

      pOutData = buf;

      if((rc = encoder_silk_encode(pParticipant->pSilkEncoder,
                                   buf,
                                   sizeof(buf),
                                    &pParticipant->pAudioBuf[indexHz])) < 0) {

        LOG(X_ERROR("encoder_silk_encode failed for source participant %s"),
          participant_description(&pParticipant->config, (char *) buf, sizeof(buf)));

        rc = 0;
      } 

    } else 

#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

    if(pParticipant->config.output.type == PARTICIPANT_TYPE_RTP_PCM) {

      // TODO create payloadMinimumSize;

      if((rc = ((pParticipant->audioBufIndexHz - indexHz) * sizeof(int16_t))) > pParticipant->config.output.u.rtpPcm.payloadMTU) {
        LOG(X_WARNING("RTP PCM output packet clipping data size %d > MTU %d"), rc, pParticipant->config.output.u.rtpPcm.payloadMTU); 
        rc = pParticipant->config.output.u.rtpPcm.payloadMTU;
      }
      pParticipant->audioFrameDurationHz = (pParticipant->audioBufIndexHz - indexHz);
      pOutData = (unsigned char *) &pParticipant->pAudioBuf[indexHz];

      if(pParticipant->pMixerSource->setOutputVad) {
        ((unsigned char *)pOutData)[rc++] = vad ? 0xff : 0x00;
      }
      //fprintf(stderr, "calling rtpstream_addpayload RAW PCM length:%d, vad:%d\n", rc, vad);
    }

    if(rc >= 0) {
      if(rtpstream_addpayload(pParticipant->pRtpstream, 
                         pOutData,
                         rc,
                         tsHz) < 0) {

        LOG(X_ERROR("rtpstream_addpayload failed for source participant %s"),
           participant_description(&pParticipant->config, (char *) buf, sizeof(buf)));
      }
    }

   
    indexHz += pParticipant->audioFrameDurationHz;
    tsHz += pParticipant->audioFrameDurationHz;

    //fprintf(stderr, "encoder_silk_encode returned %d buf[%d]/%d %lluHz\n", rc, indexHz, pParticipant->audioFrameDurationHz, tsHz);

  } // end of while

  if(indexHz < pParticipant->audioBufIndexHz) {

    memcpy(buf, &pParticipant->pAudioBuf[indexHz], (pParticipant->audioBufIndexHz - indexHz) * sizeof(int16_t));
    memcpy(pParticipant->pAudioBuf, buf, (pParticipant->audioBufIndexHz - indexHz) * sizeof(int16_t));
    pParticipant->audioBufIndexHz = indexHz;

  } else {
    pParticipant->audioBufIndexHz = 0;
  }

  return rc;
}
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

static int participant_read_samples(PARTICIPANT_T *pParticipant, 
                                    int *pvad, 
                                    int16_t *pcmSamples, 
                                    unsigned int numSamplesMin,
                                    unsigned int numSamplesMax,
                                    u_int64_t *ptsHz) {
  int pcmCount = 0;
  unsigned int vadIdx, vadIdxStart = 0, vadIdxEnd;
  unsigned int vadChunkCnt = 0;
  int vad;
  float vadTotal = 0, vadAvg = 0;

  pthread_mutex_lock(&pParticipant->pMixerSource->pOutput->buf.mtx);

  if(numSamplesMin < numSamplesMax) {
    pcmCount = MIN(pParticipant->pMixerSource->pOutput->buf.numSamples, numSamplesMax); 
  } else {
    pcmCount = numSamplesMax;
  }

  if(pcmCount <= 0 || pParticipant->pMixerSource->pOutput->buf.numSamples < numSamplesMin) {
    pthread_mutex_unlock(&pParticipant->pMixerSource->pOutput->buf.mtx);
    return 0;
  }

  //TODO: read multiple VAD(s) since we may be reading more than one chunk size of audio (20ms)
  if(pvad && pParticipant->pMixerSource->pOutput->vad_buffer) {
    vadIdxStart = pParticipant->pMixerSource->pOutput->buf.samplesRdIdx /
                               pParticipant->pMixerSource->preproc.chunkHz;
  }

  pcmCount = ringbuf_get(&pParticipant->pMixerSource->pOutput->buf,
                         pcmSamples,
                         pcmCount,
                         ptsHz,
                         0);

  if(pvad && pParticipant->pMixerSource->pOutput->vad_buffer) {
    vadIdxEnd = pParticipant->pMixerSource->pOutput->buf.samplesRdIdx /
                               pParticipant->pMixerSource->preproc.chunkHz;
    vadIdx = vadIdxStart; 
    while(vadIdx != vadIdxEnd) {

      vadTotal += pParticipant->pMixerSource->pOutput->vad_buffer[vadIdx];
      vadChunkCnt++;

      //fprintf(stderr, "vad[%d]:%d\n", vadIdx, pParticipant->pMixerSource->pOutput->vad_buffer[vadIdx]);
      if(++vadIdx >= pParticipant->pMixerSource->pOutput->buf.samplesSz / 
                     pParticipant->pMixerSource->preproc.chunkHz) {
        vadIdx = 0;
      }
    }
   
    vad = 0;
    if(vadChunkCnt > 0 && (vadAvg = vadTotal / vadChunkCnt) >= .5f) {
      vad = 1;
    }
    *pvad = vad;

#if defined(MIXER_DUMP_VAD)
    fprintf(stderr, "---mixer source id:%d, read_samples %d, vad:%d, vadIdxStart:%d, vadIdxEnd:%d (bufsz:%d, rd:%d, wr:%d, chunk:%dHz)\n", pParticipant->pMixerSource->id, pcmCount, *pvad, vadIdxStart, vadIdxEnd, pParticipant->pMixerSource->pOutput->buf.samplesSz, pParticipant->pMixerSource->pOutput->buf.samplesRdIdx, pParticipant->pMixerSource->pOutput->buf.samplesWrIdx, pParticipant->pMixerSource->preproc.chunkHz);
#endif // (MIXER_DUMP_VAD)
  }

  pthread_mutex_unlock(&pParticipant->pMixerSource->pOutput->buf.mtx);

  return pcmCount;
}


static void participant_output_runproc(void *pArg) {
  int rc;
  int pcmCount;
  u_int64_t tsHz;
  unsigned int outSamplesTot = 0;
  //int16_t pcmSamples[sizeof(int16_t) * 4096];
  int16_t pcmSamples[sizeof(int16_t) * MIXER_MAX_SAMPLE_RATE * MIXER_OUTPUT_CHUNKSZ_MS / 1000];
  int vad = 0;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pArg;
  //int logLevel = logger_GetLogLevel(LOG_CTXT);
  TIME_STAMP_T tm, tm0 = 0;
  int64_t jitterUs;

  //
  // This thread procedure is used to process mixed output PCM output
  //

  pParticipant->tdFlagOutput = THREAD_FLAG_RUNNING;

  LOG(X_DEBUG("Mixer output thread for participant source participant %s"), 
     participant_description(&pParticipant->config, (char *) pcmSamples, sizeof(pcmSamples)));
 
  while(pParticipant->tdFlagOutput == THREAD_FLAG_RUNNING) {

    //fprintf(stderr, "output pthread_cond_wait id:%d cond:0x%x\n", pParticipant->config.id, &pParticipant->pConference->condOutput);

    //
    // Wait on the mixer output conditional
    //
    rc = mixer_cond_wait(&pParticipant->pConference->condOutput, &pParticipant->pConference->mtxOutput);

    //fprintf(stderr, "output pthread_cond_wait done id:%d, rc:%d\n", pParticipant->config.id, rc);

    if(rc == 0) {

      do { 

        //
        // Read a MIXER_OUTPUT_CHUNK_MS (20ms) chunk of output samples
        //
        pcmCount = pParticipant->pMixerSource->clockHz * MIXER_OUTPUT_CHUNKSZ_MS / 1000;

        if((pcmCount = participant_read_samples(pParticipant, 
                                                &vad, 
                                                pcmSamples, 
                                                pcmCount,
                                                pcmCount,
                                                &tsHz)) <= 0) {
          break;
        }

        tm = time_getTime();
        if(outSamplesTot == 0) {
          tm0 = tm;
        } 

        jitterUs = (tm - tm0) - ((int64_t) outSamplesTot *1000000 / pParticipant->pMixerSource->clockHz);

        outSamplesTot += pcmCount;

        //fprintf(stderr, "----- elapsed:%llu us, %uHz (%llu us), jitter:%lld us\n", (tm - tm0), outSamplesTot, ((int64_t)outSamplesTot *1000000 / pParticipant->pMixerSource->clockHz), jitterUs);
        //fprintf(stderr, "output_runproc %llu.%llu jitter:%lldms, sourceid:%d ringbuf_get %d samples, rd[%d], wr[%d]/%d, numS:%d %lluHz\n", 
        LOG(X_DEBUGV("output_runproc %llu.%llu jitter:%lldms, sourceid:%d ringbuf_get %d samples, rd[%d], wr[%d]/%d, numS:%d %lluHz"), 
            tm/1000000, tm%1000000, jitterUs/1000, pParticipant->pMixerSource->id, pcmCount, 
            pParticipant->pMixerSource->pOutput->buf.samplesRdIdx, pParticipant->pMixerSource->pOutput->buf.samplesWrIdx,
            pParticipant->pMixerSource->pOutput->buf.samplesSz, pParticipant->pMixerSource->pOutput->buf.numSamples, 
            pParticipant->pMixerSource->pOutput->buf.tsHz);


        //
        // Call any user-supplied callback to process the output samples 
        //
        if(pParticipant->config.output.type == PARTICIPANT_TYPE_CB_PCM &&
           pParticipant->config.output.u.cb.cbSendSamples) {


          pParticipant->config.output.u.cb.cbSendSamples(pParticipant->config.output.u.cb.pContext,
                                                          pcmSamples,
                                                          pcmCount,
                                                          tsHz);
                                                          
        } 

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
        else if(IS_PARTICIPANT_TYPE_RTP(pParticipant->config.output.type)) {

          rc = xmitAudio(pParticipant,
                           pcmSamples,
                           pcmCount,
                           tsHz,
                           vad);
        }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

        if(pParticipant->pWavOutput) {
          wav_write(pParticipant->pWavOutput, pcmSamples, pcmCount);
        }

      } while(pcmCount > 0);

      //pthread_mutex_unlock(&pParticipant->pMixerSource->output.mtx);

    } else {

      //
      // should never get here
      //
      usleep(5000);
    }

  } 

  LOG(X_DEBUG("Mixer output thread for source participant %s ending."),
    participant_description(&pParticipant->config, (char *) pcmSamples, sizeof(pcmSamples)));

  participant_stop(pParticipant);

  pParticipant->tdFlagOutput = THREAD_FLAG_STOPPED;

}

int mixer_read_audio_pcm(void *pContext,
                         int16_t *pcmSamples,
                         unsigned int numSamplesMin,
                         unsigned int numSamplesMax,
                         int *pvad) {
  int pcmCount;
  u_int64_t tsHz;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pContext;

  if(!pParticipant || !pParticipant->pMixerSource || !pParticipant->pMixerSource->pOutput) {
    return -1;
  } else if(numSamplesMin > numSamplesMax) {
    numSamplesMin = numSamplesMax;
  }

  pcmCount = participant_read_samples(pParticipant, 
                                      pvad, 
                                      pcmSamples, 
                                      numSamplesMin,
                                      numSamplesMax,
                                      &tsHz);

  if(pcmCount > 0 && pParticipant->pWavOutput) {
    wav_write(pParticipant->pWavOutput, pcmSamples, pcmCount);
  }

  return pcmCount;
}

static int participant_startListener(PARTICIPANT_T *pParticipant) {
  pthread_attr_t attr;
  int rc = 0;
  char buf[PARTICIPANT_DESCRIPTION_BUFSZ];

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  RTPLIB_CONFIG_T config;
  RTP_DESTINATION_T *pDestination = NULL;
  struct in_addr inaddr;
  char bufIn[64];
  char bufOut[64];

  if(IS_PARTICIPANT_TYPE_RTP(pParticipant->config.input.type)) {

    memset(&inaddr, 0, sizeof(inaddr));
    inaddr.s_addr = pParticipant->config.input.u.rtp.listenAddress.address; 
    memset(&config, 0, sizeof(config));
    snprintf(bufIn, sizeof(bufIn), "rtp://%s:%d", inet_ntoa(inaddr), 
      pParticipant->config.input.u.rtp.listenAddress.rtpPort);
    config.listenerAddress = bufIn;
    config.rtcpRRIntervalSec = 5.0f;
    config.idleTmtSec = 10.0f;
    //config.rtpGapWaitTimeSec = 0.100f; // TODO: this seems broken, need to add real time gettimeofday
    config.rtpGapWaitTimeSec = ((float) pParticipant->pConference->cfg.mixerLateThresholdMs) / 1000.0f;
    config.pUserData = pParticipant;
    config.cbOnPacket = participant_cbOnRtpPacket;

    if(!(pParticipant->pRtplib = rtplib_create(pParticipant->logCtxt, &config))) {
      LOG(X_ERROR("Failed to create rtplib context for source participant %s"),
         participant_description(&pParticipant->config, buf, sizeof(buf)));
      rc = -1;
    }

  }

  if(rc == 0 && IS_PARTICIPANT_TYPE_RTP(pParticipant->config.output.type)) {

    if(pParticipant->config.output.type == PARTICIPANT_TYPE_RTP_SILK) {
      pDestination = &pParticipant->config.output.u.rtpSilk.dest;
    } if(pParticipant->config.output.type == PARTICIPANT_TYPE_RTP_PCM) {
      pDestination = &pParticipant->config.output.u.rtpPcm.dest;
    }

    if(pDestination) {
      inaddr.s_addr = pDestination->host.address; 
      snprintf(bufOut, sizeof(bufOut), "%s:%d", inet_ntoa(inaddr), pDestination->host.rtpPort);
    }

    if(!pDestination || !(pParticipant->pRtpstream = rtpstream_create(LOG_CTXT, 
                                                     bufOut, 
                                                     pDestination->payloadType,
                                                     pParticipant->pConference->cfg.sampleRate))) {
      LOG(X_ERROR("Failed to create rtpstream context for source participant %s"),
         participant_description(&pParticipant->config, buf, sizeof(buf)));
      rc = -1;
    }

  }

  if(rc == 0 && IS_PARTICIPANT_TYPE_RTP(pParticipant->config.input.type)) {
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pParticipant->tdFlagCap = THREAD_FLAG_STARTING;
    if((rc = pthread_create(&pParticipant->ptdCap,
                    &attr,
                    (void *) participant_recvdata_runproc,
                    (void *) pParticipant) != 0)) {
      LOG(X_ERROR("Unable to create mixer capture thread for source participant %s"), 
        participant_description(&pParticipant->config, buf, sizeof(buf)));
      pParticipant->tdFlagCap = THREAD_FLAG_STOPPED;
      rc = -1;
    }
    pthread_attr_destroy(&attr);
  }

  if(rc == 0 && (IS_PARTICIPANT_TYPE_RTP(pParticipant->config.input.type) ||
                 pParticipant->config.input.type == PARTICIPANT_TYPE_CB_SILK)) {
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pParticipant->tdFlagUnpack = THREAD_FLAG_STARTING;
    if((rc = pthread_create(&pParticipant->ptdCap,
                    &attr,
                    (void *) participant_unpackdata_runproc,
                    (void *) pParticipant) != 0)) {
      LOG(X_ERROR("Unable to create mixer decoder thread for source participant %s"), 
        participant_description(&pParticipant->config, buf, sizeof(buf)));
      pParticipant->tdFlagUnpack = THREAD_FLAG_STOPPED;
      rc = -1;
    }
    pthread_attr_destroy(&attr);
  }

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  if(rc == 0 && PARTICIPANT_HAVE_OUTPUT(pParticipant->config.output.type)) {
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pParticipant->tdFlagOutput = THREAD_FLAG_STARTING;
    if((rc = pthread_create(&pParticipant->ptdOutput,
                    &attr,
                    (void *) participant_output_runproc,
                    (void *) pParticipant) != 0)) {
      LOG(X_ERROR("Unable to create mixer output thread for source participant %s"), 
        participant_description(&pParticipant->config, buf, sizeof(buf)));
      pParticipant->tdFlagOutput = THREAD_FLAG_STOPPED;
      rc = -1;
    }
    pthread_attr_destroy(&attr);
  }

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  if(rc == 0 && (IS_PARTICIPANT_TYPE_RTP(pParticipant->config.output.type))) {
    rc = rtpstream_start(pParticipant->pRtpstream);
  }

  while(pParticipant->tdFlagUnpack == THREAD_FLAG_STARTING) {
    usleep(5000);
  }

  while(pParticipant->tdFlagCap == THREAD_FLAG_STARTING) {
    usleep(5000);
  }

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)


  while(pParticipant->tdFlagOutput == THREAD_FLAG_STARTING) {
    usleep(5000);
  }

  if(
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
     (IS_PARTICIPANT_TYPE_RTP(pParticipant->config.input.type) && pParticipant->tdFlagCap == THREAD_FLAG_STOPPED) ||
     (IS_PARTICIPANT_TYPE_RTP(pParticipant->config.input.type) && pParticipant->tdFlagUnpack == THREAD_FLAG_STOPPED) ||
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
     (PARTICIPANT_HAVE_OUTPUT(pParticipant->config.output.type) &&pParticipant->tdFlagOutput == THREAD_FLAG_STOPPED)) {

    participant_stop(pParticipant);
    rc = -1; 
  }

  return rc;
}

#if defined(MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)
int participant_set_ntp_time(PARTICIPANT_T *pParticipant,
                             const NTP_SYNC_T *pNtpSync) {

  if(!pParticipant || !pNtpSync) {
    return -1;
  }

  memcpy(&pParticipant->ntpSync, pNtpSync, sizeof(NTP_SYNC_T));
  pParticipant->haveNtpSync = 1;

  return 0;
}
#endif // (MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)


#undef LOG_CTXT
#define LOG_CTXT  (pConference ? pConference->logCtxt : NULL)

PARTICIPANT_T *participant_add(AUDIO_CONFERENCE_T *pConference, const PARTICIPANT_CONFIG_T *pConfig) {
  int rc;
  PARTICIPANT_T *pList;
  PARTICIPANT_T *pParticipant = NULL;
  PARTICIPANT_T **ppParticipant = NULL;
  PARTICIPANT_CONFIG_T config;
  char buf[PARTICIPANT_DESCRIPTION_BUFSZ];

  if(!pConference || !pConfig || pConfig->id < 0) {
    return NULL;
  }

  //
  // preserve const input parameter
  //
  memcpy(&config, pConfig, sizeof(config));

  //
  // Validate the input type
  // 
  switch(config.input.type) {

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP> 0)

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK> 0)
    case PARTICIPANT_TYPE_RTP_SILK:
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK> 0)

    case PARTICIPANT_TYPE_RTP_PCM:
      if(config.input.u.rtp.listenAddress.rtpPort <= 1024 || 
        (config.input.u.rtp.listenAddress.rtpPort % 2 == 1)) {
        LOG(X_ERROR("Invalid mixer listener RTP port %d"), config.input.u.rtp.listenAddress.rtpPort);
        return NULL;
      }
      break;
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP> 0)

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK> 0)
    case PARTICIPANT_TYPE_CB_SILK:
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK> 0)

    case PARTICIPANT_TYPE_CB_PCM:
    case PARTICIPANT_TYPE_NONE:
      break;
    default:
      LOG(X_ERROR("Unsupported mixer participant input type %d"), config.input.type);
      return NULL;
  }

  //
  // Validate the output type
  // 
  switch(config.output.type) {

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP> 0)

    case PARTICIPANT_TYPE_RTP_PCM:

      if(config.output.u.rtpPcm.dest.host.rtpPort <= 1024 || (config.output.u.rtpPcm.dest.host.rtpPort % 2 == 1)) {
        LOG(X_ERROR("Invalid mixer participant output RTP port %d"), config.output.u.rtpPcm.dest.host.rtpPort);
        return NULL;
      }

      if(config.output.u.rtpPcm.payloadMTU == 0) {
        config.output.u.rtpPcm.payloadMTU = RTP_PAYLOAD_MTU_SILK;
      }

      //
      // Ensure that the MTU is a multiple of the PCM sample size
      //
      if(config.output.u.rtpPcm.payloadMTU % 2 == 1) {
        config.output.u.rtpPcm.payloadMTU++;
      }

      break;

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
    case PARTICIPANT_TYPE_RTP_SILK:
      if(config.output.u.rtpSilk.dest.host.rtpPort <= 1024 || (config.output.u.rtpSilk.dest.host.rtpPort % 2 == 1)) {
        LOG(X_ERROR("Invalid mixer participant output RTP port %d"), config.output.u.rtpSilk.dest.host.rtpPort);
        return NULL;
      }
      break;
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP> 0)

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
    case PARTICIPANT_TYPE_CB_SILK:
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

    case PARTICIPANT_TYPE_CB_PCM:
      if(!config.output.u.cb.cbSendSamples) {
        LOG(X_ERROR("Mixer participant output callback not set"));
        return NULL;
      }
      break;

    case PARTICIPANT_TYPE_PCM_POLL:
    case PARTICIPANT_TYPE_NONE:
      //
      // NULL output participant w/o output stream
      //
      break;

    default:
      LOG(X_ERROR("Unsupported mixer participant output type %d"), config.output.type);
      return NULL;
  }

  if(config.output.type == PARTICIPANT_TYPE_NONE &&
     config.input.type == PARTICIPANT_TYPE_NONE) {
    LOG(X_ERROR("Mixer participant input and output types are both set to none"));
    return NULL;
  }

  pthread_mutex_lock(&pConference->mtx);

  if(pConference->numParticipants >= pConference->cfg.maxParticipants) {
    LOG(X_ERROR("Mixer participants are at capacity %d"), pConference->cfg.maxParticipants);
    pthread_mutex_unlock(&pConference->mtx);
    return NULL;
  }

  pList = pConference->participants;

  //
  // Iterate through the existing participant list and ensure that the new participant
  // can be added
  //
  while(pList) {

    if(pList->config.id == config.id) {

      LOG(X_ERROR("Mixer already has active participant with id %d"), config.id);
      pthread_mutex_unlock(&pConference->mtx);
      return NULL;

    } 

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
    else if(IS_PARTICIPANT_TYPE_RTP(config.input.type) &&
              IS_PARTICIPANT_TYPE_RTP(pList->config.input.type) &&
       pList->config.input.u.rtp.listenAddress.rtpPort == config.input.u.rtp.listenAddress.rtpPort) {

      LOG(X_ERROR("Mixer already has active RTP participant on listener port %d"), 
         config.input.u.rtp.listenAddress.rtpPort);
      pthread_mutex_unlock(&pConference->mtx);
      return NULL;
    }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

    else if(!pList->pnext) {
  
      ppParticipant = &pList->pnext;
      break;
    }

    pList = pList->pnext;

  } // end of while(

  if(!(pParticipant = participant_create(pConference, &config))) {
    pthread_mutex_unlock(&pConference->mtx);
    return NULL;
  }

  if(!(pConference->participants)) {
    pConference->participants = pParticipant;
  } else if(ppParticipant) {
    (*ppParticipant) = pParticipant;
  } else {
    pthread_mutex_unlock(&pConference->mtx);
    return NULL;
  }

  pConference->numParticipants++;

  pthread_mutex_unlock(&pConference->mtx);

  pthread_mutex_lock(&pParticipant->mtx);

  if((rc = participant_startListener(pParticipant)) < 0) {
    pthread_mutex_unlock(&pParticipant->mtx);
    participant_remove(pParticipant);
    return NULL;
  } else {
    LOG(X_INFO("Added mixer participant id %s"), participant_description(&pParticipant->config, buf, sizeof(buf)));
  }

  pthread_mutex_unlock(&pParticipant->mtx);

  return pParticipant;
}

int participant_remove_id(AUDIO_CONFERENCE_T *pConference, PARTICIPANT_ID id) {
  PARTICIPANT_T *pParticipant = NULL, *pPrev = NULL;

  if(!pConference) {
    return -1;
  }

  LOG(X_DEBUG("Removing mixer participant id %d"), id);

  pthread_mutex_lock(&pConference->mtx);

  pParticipant = pConference->participants;
  while(pParticipant) {

    if(pParticipant->config.id == id) {

      if(pPrev) {
        pPrev->pnext = pParticipant->pnext;
      } else {
        pConference->participants = pParticipant->pnext;
      }
      if(pConference->numParticipants > 0) {
        pConference->numParticipants--;
      }
      break;
    }

    pPrev = pParticipant;
    pParticipant = pParticipant->pnext;
  }

  pthread_mutex_unlock(&pConference->mtx);

  if(pParticipant) {

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

    //pthread_mutex_lock(&pParticipant->mtx);
    if(pParticipant->pRtplib) {

      rtplib_stop(pParticipant->pRtplib);

      rtplib_destroy(pParticipant->pRtplib);
      pParticipant->pRtplib = NULL;

    }

    if(pParticipant->pRtpstream) {

      rtpstream_stop(pParticipant->pRtpstream);

      rtpstream_destroy(pParticipant->pRtpstream);
      pParticipant->pRtpstream = NULL;

    }
    //pthread_mutex_unlock(&pParticipant->mtx);

    while(pParticipant->tdFlagCap > THREAD_FLAG_STOPPED) {
      pParticipant->tdFlagCap = THREAD_FLAG_DOEXIT;
      usleep(5000);
    }

    while(pParticipant->tdFlagUnpack > THREAD_FLAG_STOPPED) {
      pParticipant->tdFlagUnpack = THREAD_FLAG_DOEXIT;
      mixer_cond_broadcast(&pParticipant->pFrameQ->cond, &pParticipant->pFrameQ->mtxCond);
      usleep(5000);
    }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

    while(pParticipant->tdFlagOutput > THREAD_FLAG_STOPPED) {
      pParticipant->tdFlagOutput = THREAD_FLAG_DOEXIT;
      mixer_cond_broadcast(&pConference->condOutput, &pConference->mtxOutput);
      usleep(5000);
    }

    participant_destroy(&pParticipant);

    LOG(X_INFO("Removed mixer participant id %d"), id);

    return 0;
  }

  return -1;
}

int participant_remove(PARTICIPANT_T *pParticipant) {

  if(!pParticipant || !pParticipant->pConference) {
    return -1;
  }

  return participant_remove_id(pParticipant->pConference, pParticipant->config.id);
}

static const char *participant_type_description(PARTICIPANT_TYPE_T type) {
  
  switch(type) {
    case PARTICIPANT_TYPE_NONE:
      return "none";
    case PARTICIPANT_TYPE_PCM_POLL:
      return "PCM output polling";
    case PARTICIPANT_TYPE_CB_PCM:
      return "PCM callback";

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

    case PARTICIPANT_TYPE_CB_SILK:
      return "SILK callback";

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
    case PARTICIPANT_TYPE_RTP_SILK:
      return "SILK RTP";
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
    case PARTICIPANT_TYPE_RTP_PCM:
      return "PCM RTP";
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

    default: 
      return "";
  }   
}

const char *participant_description(const PARTICIPANT_CONFIG_T *pConfig, char *buffer, size_t sz) {
  int rc;
  struct in_addr inaddr;
  char vadstr[64];
  char inputstr[64];
  char outputstr[64];

  memset(&inaddr, 0, sizeof(inaddr));

  inputstr[0] = '\0';
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
  if(IS_PARTICIPANT_TYPE_RTP(pConfig->input.type)) {
    snprintf(inputstr, sizeof(inputstr) - 1, "port %d", pConfig->input.u.rtp.listenAddress.rtpPort);
  }
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  outputstr[0] = '\0';
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
  if(pConfig->output.type == PARTICIPANT_TYPE_RTP_PCM) {
    inaddr.s_addr = pConfig->output.u.rtpPcm.dest.host.address;
    snprintf(outputstr, sizeof(outputstr) - 1, "%s:%d", inet_ntoa(inaddr), 
                                                  pConfig->output.u.rtpPcm.dest.host.rtpPort);
  } 
#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
  else if(pConfig->output.type == PARTICIPANT_TYPE_RTP_SILK) {
    inaddr.s_addr = pConfig->output.u.rtpSilk.dest.host.address;
    snprintf(outputstr, sizeof(outputstr) - 1, "%s:%d", inet_ntoa(inaddr), 
                                                  pConfig->output.u.rtpSilk.dest.host.rtpPort);
  }
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

  if(pConfig->pAudioConfig->mixerVad > 1) {
    snprintf(vadstr, sizeof(vadstr), " (mode %d)", pConfig->pAudioConfig->mixerVad -1);
  } else {
    vadstr[0] = '\0';
  }

  rc = snprintf(buffer, sz, "id:%d input %s %s, output %s %s. %s%s%s%s", pConfig->id, 
               participant_type_description(pConfig->input.type),
               inputstr,
               participant_type_description(pConfig->output.type),
               outputstr,
               pConfig->pAudioConfig->mixerVad ? " vad" : "",
               vadstr,
               pConfig->pAudioConfig->mixerAgc ? " agc" : "",
               pConfig->pAudioConfig->mixerDenoise ? " denoise" : "");

  return buffer;
}

const MIXER_CONFIG_T *participant_get_config(struct PARTICIPANT *pParticipant) {

  if(!pParticipant || !pParticipant->pConference) {
    return NULL;
  }

  return &pParticipant->pConference->cfg;

}

