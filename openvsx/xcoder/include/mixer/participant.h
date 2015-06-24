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


#ifndef __PARTICIPANT_H__
#define __PARTICIPANT_H__

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "mixer/mixdefaults.h"
#include "logutil.h"
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
#include "rtplib.h"
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
#include "mixer/mixer.h"
#include "mixer/frameq.h"
#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
#include "decoder_silk.h"
#include "encoder_silk.h"
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
#include "mixer/wav.h"
#include "mixer/util.h"

/*
#define IS_PARTICIPANT_TYPE_RTP(type) ((type) == PARTICIPANT_TYPE_RTP_SILK || \
                                       (type) == PARTICIPANT_TYPE_RTP_PCM)

#define IS_PARTICIPANT_TYPE_SILK(type) ((type) == PARTICIPANT_TYPE_RTP_SILK || \
                                       (type) == PARTICIPANT_TYPE_CB_SILK)

*/
#define IS_PARTICIPANT_TYPE_PCM(type) ((type) == PARTICIPANT_TYPE_RTP_PCM || \
                                       (type) == PARTICIPANT_TYPE_CB_PCM)

#define PARTICIPANT_HAVE_OUTPUT(type) (((type) > PARTICIPANT_TYPE_PCM_POLL) ? 1 : 0)


typedef struct PARTICIPANT {
  LOG_CTXT_T                 *logCtxt;
  PARTICIPANT_CONFIG_T        config;
  AUDIO_CONFIG_T              audioConfig;
  int                         haveNtpSync;
  //NTP_SYNC_T                  ntpSync;
  pthread_mutex_t             mtx;
#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
  RTPLIB_CONTEXT_T           *pRtplib;
  RTPSTREAM_CONTEXT_T        *pRtpstream;
  FRAME_Q_T                  *pFrameQ;
  pthread_t                   ptdUnpack;
  enum THREAD_FLAG            tdFlagUnpack;
  pthread_t                   ptdCap;
  enum THREAD_FLAG            tdFlagCap;
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
#if defined(MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
  SILK_DECODER_CTXT_T        *pSilkDecoder;
  SILK_ENCODER_CTXT_T        *pSilkEncoder;
  SILK_DECODER_CONFIG_T       decoderConfig;
  SILK_ENCODER_CONFIG_T       encoderConfig;
#endif // (MIXER_HAVE_SILK) && (MIXER_HAVE_SILK > 0)
  pthread_t                   ptdOutput;
  enum THREAD_FLAG            tdFlagOutput;
  unsigned int                lastRcvdTs;   //TODO: should handle ts roll
  unsigned int                lastRcvdSeq;
  //TIME_STAMP_T                tmFirstFrame; // captureTm of first frame
  unsigned int                tsFirstFrame; // (RTP) ts of first frame
  TIME_STAMP_T                tmOffsetFromStartHz;
  unsigned int                framesRcvd;
  unsigned int                framesMixed;
  MIXER_SOURCE_T             *pMixerSource;
  int                         masterSource;
  unsigned int                audioFrameDurationHz;
  int16_t                    *pAudioBuf;
  unsigned int                audioBufIndexHz;
  WAV_FILE_T                 *pWavDecoded;
  WAV_FILE_T                 *pWavOutput;
  struct AUDIO_CONFERENCE    *pConference;
  struct PARTICIPANT         *pnext;
} PARTICIPANT_T;

TIME_STAMP_T participant_on_first_pcm(PARTICIPANT_T *pParticipant,
                                      TIME_STAMP_T captureTm, 
                                      unsigned int ts);

#if defined(MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)
int participant_cbOnRtpPacket(void *pUserData, const RTPLIB_PAYLOAD_T *pPayload);
#endif // (MIXER_HAVE_RTP) && (MIXER_HAVE_RTP > 0)

int participant_add_samples_to_mixer(PARTICIPANT_T *pParticipant,
                                     const int16_t *pSamples,
                                     unsigned int numSamples,
                                     unsigned int tsHz,
                                     int vad,
                                     int *pvad);

#define PARTICIPANT_DESCRIPTION_BUFSZ 256

const char *participant_description(const PARTICIPANT_CONFIG_T *pConfig, char *buffer, size_t sz);


#endif // __PARTICIPANT_H__
