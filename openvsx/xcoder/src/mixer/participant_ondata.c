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
//#include "rtplib.h"
#include "mixer/conference.h"
#include "mixer/participant.h"

//#define DEBUG_MIXER_TIMING 1

#define LOG_CTXT  pParticipant->logCtxt

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

//
// Registered callback used to deliver a new incoming frame payload (from rtplib)
//
int participant_cbOnRtpPacket(void *pUserData, 
                              const RTPLIB_PAYLOAD_T *pPayload) {
  int rc;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pUserData;
  TIME_STAMP_T captureTm;

  captureTm = TV_TO_TIME_STAMP(pPayload->tv);

  //fprintf(stderr, "---------- participant_cbRtpOnPacket port %d :%d seq:%u, ts:%u captureTm:%llu.%llu 0x%x 0x%x 0x%x 0x%x\n", pParticipant->config.input.u.rtp.listenAddress.rtpPort, pPayload->lenData, pPayload->rtpSeq, pPayload->rtpTs, captureTm/1000000, captureTm%1000000, ((unsigned char *)pPayload->pData)[0], ((unsigned char *)pPayload->pData)[1], ((unsigned char *)pPayload->pData)[2], ((unsigned char *)pPayload->pData)[3]);


  pthread_mutex_lock(&pParticipant->mtx);

  //
  // En-queue the RTP packet contents preserving RTP timestamp and sequence #
  // RTP seq # roll-over is handled by the output of rtplib 
  //
  rc = frameq_push(pParticipant->pFrameQ, 
                   pPayload->pData, 
                   pPayload->lenData, 
                   pPayload->rtpSeq,
                   pPayload->rtpTs,
                   captureTm);

  pthread_mutex_unlock(&pParticipant->mtx);

  return 0;
}
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

TIME_STAMP_T participant_on_first_pcm(PARTICIPANT_T *pParticipant,
                                      TIME_STAMP_T captureTm, 
                                      unsigned int ts) {
  TIME_STAMP_T tmOffsetFromStartHz;

#if 1
  //pthread_mutex_lock(&pParticipant->pMixerSource->buf.mtx);
  //mixerTsHz = pParticipant->pMixerSource->buf.tsHz;
  //pthread_mutex_unlock(&pParticipant->pMixerSource->buf.mtx);

  pthread_mutex_lock(&pParticipant->pConference->mtx);
  tmOffsetFromStartHz = pParticipant->pConference->tsHzNow;
  pthread_mutex_unlock(&pParticipant->pConference->mtx);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  LOG(X_DEBUG("---mixer source id:%d, participant_on_first_pcm tmOffsetFromStartHz:%lluHz"), pParticipant->pMixerSource->id, tmOffsetFromStartHz);
#endif //(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

#else

  TIME_STAMP_T tmOffsetFromStart;

  if(captureTm > pParticipant->pConference->tmStart) {
    tmOffsetFromStart = captureTm - pParticipant->pConference->tmStart;
  } else {
    tmOffsetFromStart = 0;
  }

  tmOffsetFromStartHz = pParticipant->pConference->sampleRate * tmOffsetFromStart / TIME_VAL_US;

  //pParticipant->tmFirstFrame = captureTm - tmOffsetFromStart;
#endif // 0

  pParticipant->tsFirstFrame = ts;
  pParticipant->lastRcvdTs = 0;

  return tmOffsetFromStartHz;
}

static int participant_cbOnPCM(PARTICIPANT_T *pParticipant, 
                        const int16_t *pcmSamples,
                        unsigned int numSamples,
                        int *pvad) {

  uint64_t tsHz;
  TIME_STAMP_T tm;

  if(!pParticipant || !pcmSamples) {
    return -1;
  }

  if(pParticipant->framesRcvd == 0 || pParticipant->pMixerSource->needInputReset) {

    tm = time_getTime();

    //tm += (pParticipant->config.ntpOffsetMs * 1000);

    pParticipant->tmOffsetFromStartHz = participant_on_first_pcm(pParticipant, tm, 0);
    pParticipant->pMixerSource->needInputReset = 0;

  }

      
  tsHz = pParticipant->lastRcvdTs + pParticipant->tmOffsetFromStartHz;

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 1)
  LOG(X_DEBUG("---mixer participant_cbOnPCM id:%d, numSamples:%d, lastRcvdTs:%uHz + tmOffsetFromStartHz:%lluHz = tsHz::%lluHz, conf at %lluHz"), pParticipant->config.id, numSamples, pParticipant->lastRcvdTs, pParticipant->tmOffsetFromStartHz, tsHz, pParticipant->pConference->tsHzNow);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  participant_add_samples_to_mixer(pParticipant, 
                                   pcmSamples, 
                                   numSamples, 
                                   tsHz,  
                                   -1,
                                   pvad);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 1)
  mixer_dump(stderr, pParticipant->pConference->pMixer, 1, NULL);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  pParticipant->lastRcvdTs += numSamples;
  pParticipant->framesRcvd++; 

  return 0;
}

#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
int participant_cbOnSilkFrame(void *pUserData, 
                              unsigned char *pData,
                              unsigned int lenData,
                              uint64_t ts) {
  int rc;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pUserData;
  TIME_STAMP_T captureTm;

  captureTm = (TIME_STAMP_T) ts;

  pthread_mutex_lock(&pParticipant->mtx);

  //
  // En-queue the frame contents preserving 
  //
  rc = frameq_push(pParticipant->pFrameQ,
                   pData,
                   lenData,
                   0,
                   ts,
                   captureTm);

  pthread_mutex_unlock(&pParticipant->mtx);

  return 0;
}
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)

void mixer_on_receive_audio_pcm(void *pParticipantArg,
                               const int16_t *pcmSamples,
                               unsigned int numSamples,
                               int *pvad) {
  int rc;
  PARTICIPANT_T *pParticipant = (PARTICIPANT_T *) pParticipantArg;

  if(!pParticipant) {
    return;
  }

  rc = participant_cbOnPCM(pParticipant, pcmSamples, numSamples, pvad);

}
      
