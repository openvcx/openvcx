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


#ifndef __MIXER_API_H__
#define __MIXER_API_H__


#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)
#include "ntp_types.h"
#endif // (MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)

//#define DEBUG_MIXER_TIMING 1
//#define MIXER_DUMP_VAD 1


/**
 *
 * The following defines correspond to config file entries in 'etc/mixer.conf'
 *
 */
#define CONFIG_LOG_LEVEL                "logLevel"
#define CONFIG_LOG_DIR                  "logDirectory"
#define CONFIG_LOG_FILENAME             "logFilename"
#define CONFIG_LOG_MAX_FILES            "logMaxFileCount"
#define CONFIG_LOG_MAX_FILESZ           "logMaxFileSize"
#define CONFIG_MAX_PARTICIPANTS         "maxParticipants"
#define CONFIG_SAMPLE_RATE              "sampleRate"
#define CONFIG_MIXER_LATE_THRESHOLD     "mixerLateAudioTresholdMs"
#define CONFIG_MIXER_VAD                "mixerVad"
#define CONFIG_MIXER_AGC                "mixerAgc"
#define CONFIG_MIXER_DENOISE            "mixerDenoise"
#define CONFIG_WAV_DECODED              "decodedWavPath"
#define CONFIG_WAV_MIXED                "mixedWavPath"
#define CONFIG_ENABLE_NTP               "enableNtpOffset"
#define CONFIG_SILK_COMPLEXITY          "silkEncoderComplexity"
#define CONFIG_SILK_BITRATE             "silkEncoderBitrate"
#define CONFIG_MIXER_INCLUDE_SELF       "includeSelfChannel"
#define CONFIG_MIXER_SET_OUTPUT_VAD     "setOutputVad"
#define CONFIG_MIXER_ACCEPT_INPUT_VAD   "acceptInputVad"

typedef int PARTICIPANT_ID;
typedef int CONFERENCE_ID;

//
// The following enum defines the participant input or output type configuration
//
typedef enum PARTICIPANT_TYPE {
  PARTICIPANT_TYPE_NONE           = 0,
  PARTICIPANT_TYPE_PCM_POLL       = 1,                  // PCM samples via polling mixer_read_audio_pcm
  PARTICIPANT_TYPE_CB_PCM         = 2,                  // PCM samples via callback cbSendSamples
  PARTICIPANT_TYPE_CB_SILK        = 3,                  // SILK encoded audio via callback
  PARTICIPANT_TYPE_RTP_SILK       = 4,                  // SILK encoded audio frames via RTP
  PARTICIPANT_TYPE_RTP_PCM        = 5,                  // PCM samples via RTP
} PARTICIPANT_TYPE_T;


typedef struct HOST_ADDR {
  in_addr_t                   address;
  uint16_t                    rtpPort;
} HOST_ADDR_T;

typedef struct RTP_DESTINATION {
  HOST_ADDR_T                 host;
  uint8_t                     payloadType;
} RTP_DESTINATION_T ;

typedef struct PARTICIPANT_OUTPUT_TYPE_RTP_SILK {
  RTP_DESTINATION_T           dest;
} PARTICIPANT_OUTPUT_TYPE_RTP_SILK_T;

typedef struct PARTICIPANT_OUTPUT_TYPE_RTP_PCM {
  RTP_DESTINATION_T           dest;
  unsigned int                payloadMTU;   
} PARTICIPANT_OUTPUT_TYPE_RTP_PCM_T;


//typedef int (*MIXER_CB_ONPCMSAMPLES)(void *, int16_t *, unsigned int);

typedef struct PARTICIPANT_INPUT_TYPE_RTP {
  HOST_ADDR_T                 listenAddress;
} PARTICIPANT_INPUT_TYPE_RTP_T;

typedef struct PARTICIPANT_INPUT_TYPE_CB {
  //MIXER_CB_ONPCMSAMPLES;
  int todo;
} PARTICIPANT_INPUT_TYPE_CB_T;

typedef int (* CB_SEND_SAMPLES) (void *, int16_t *, unsigned int, u_int64_t);


typedef struct PARTICIPANT_OUTPUT_TYPE_CB {
  //MIXER_CB_ONPCMSAMPLES;
  void                                   *pContext;
  CB_SEND_SAMPLES                         cbSendSamples;
} PARTICIPANT_OUTPUT_TYPE_CB_T;


typedef struct PARTICIPANT_INPUT_TYPE {
  PARTICIPANT_TYPE_T                     type;

  union {
    PARTICIPANT_INPUT_TYPE_RTP_T         rtp;
    PARTICIPANT_INPUT_TYPE_CB_T          cb;
  } u;

} PARTICIPANT_INPUT_TYPE_T;
 

typedef struct PARTICIPANT_OUTPUT_TYPE {
  PARTICIPANT_TYPE_T                     type;

  union {
    PARTICIPANT_OUTPUT_TYPE_RTP_PCM_T    rtpPcm;
    PARTICIPANT_OUTPUT_TYPE_RTP_SILK_T   rtpSilk;
    PARTICIPANT_OUTPUT_TYPE_CB_T         cb;
  } u;

} PARTICIPANT_OUTPUT_TYPE_T;

typedef struct AUDIO_CONFIG {
  const char                 *decodedWavPath;
  const char                 *mixedWavPath;
  int                         includeSelfChannel;   // participant specific setting only
  int                         mixerVad;
  int                         mixerAgc;
  int                         mixerDenoise;

#if !defined(MIXER_HAVE_RTP) || (MIXER_HAVE_RTP== 0)
  int                         setOutputVad;
  int                         acceptInputVad;
#endif // (MIXER_HAVE_RTP) || (MIXER_HAVE_RTP== 0)

#if !defined(MIXER_HAVE_SILK) || (MIXER_HAVE_SILK == 0)
  unsigned int                encoderComplexity;
  unsigned int                encoderBitrate;
#endif // (MIXER_HAVE_SILK) || (MIXER_HAVE_SILK == 0)

} AUDIO_CONFIG_T;

typedef struct MIXER_CONFIG {
  AUDIO_CONFIG_T              ac;
  int                         verbosity;
  void                       *pLogCtxt;
  int                         maxParticipants;
  int                         sampleRate;
  int                         mixerLateThresholdMs;
  int                         highPriority;
  int                         enableNtp;
} MIXER_CONFIG_T;

typedef struct PARTICIPANT_CONFIG {
  PARTICIPANT_ID              id;
  PARTICIPANT_INPUT_TYPE_T    input;
  PARTICIPANT_OUTPUT_TYPE_T   output;
  AUDIO_CONFIG_T             *pAudioConfig;
} PARTICIPANT_CONFIG_T;



/**
 *
 * conference_create
 * Creates an audio conference instance
 *
 * @return - The Conference instance
 * pConfig - The mixer initialization configuration
 *
 */
struct AUDIO_CONFERENCE *conference_create(const MIXER_CONFIG_T *pConfig, CONFERENCE_ID id);

/**
 *
 * conference_destroy
 * Destroys a conference instance
 *
 * pConference - The conference instance returned via conference_create
 *
 */
void conference_destroy(struct AUDIO_CONFERENCE *pConference);

const MIXER_CONFIG_T *conference_get_config(struct AUDIO_CONFERENCE *pConference);

void conference_dump (FILE *fp, struct AUDIO_CONFERENCE *pConference);

/**
 *
 *
 * Used to initialize initialize the optional element pAudioConfig inside the
 * PARTICIPANT_CONFIG_T element when creating a participant.
 *
 * pConference - The conference instance returned via conference_create
 * pAudioConfig - The AUDIO_CONFIG_T element to be initialized
 *
 */
//int conference_load_config_defaults(struct AUDIO_CONFERENCE *pConference, AUDIO_CONFIG_T *pAudioConfig);

/**
 *
 * conference_participant_count
 * Returns the number of added participants in the conference.
 *
 * pConference - The conference instance returned via conference_create
 *
 */
int conference_participant_count(struct AUDIO_CONFERENCE *pConference);

/**
 *
 * participant_add
 * Adds an audio participant to the conference.
 *
 * @return - A pointer to the internal participant context instance.
 *
 * pConference - The conference instance returned via conference_create
 * pConfig - The participant configuration
 *            Note:  pConfig->id should be set to a unique identifier on input.
 *            If pConfig->pAudioConfig is NULL then the conference
 *            wide audio input settings are used.  These settings are set
 *            in the mixer config file 'etc/mixer.conf'.
 *
 */
struct PARTICIPANT *participant_add(struct AUDIO_CONFERENCE *pConference,
                                    const PARTICIPANT_CONFIG_T *pConfig);

/**
 *
 * participant_remove_id
 *
 * pConference - The conference instance returned via conference_create
 * id - The unique participant identifier used in the configuration of
 *      participant_add.
 *
 */
int participant_remove_id(struct AUDIO_CONFERENCE *pConference,
                          PARTICIPANT_ID id);

/**
 *
 * participant_remove_id
 *
 * pConference - The conference instance returned via conference_create
 * pParticipant - The participant context instance returned by a call to participant_add.
 *
 */
int participant_remove(struct PARTICIPANT *pParticipant);


const MIXER_CONFIG_T *participant_get_config(struct PARTICIPANT *pParticipant);

/**
 *
 * mixer_on_receive_audio_pcm
 * Used to add samples from a participant into the mixer
 *
 * pContext - The participant context instance returned by a call to participant_add.
 * pcmSamples - PCM 16 bit input samples.
 * numSamples - count of input samples.
 * pvad - Optional output parameter to store the computed VAD of teh aggregate
 *        input samples.
 *
 */
void mixer_on_receive_audio_pcm(void *pContext,
                               const int16_t *pcmSamples,
                               unsigned int numSamples,
                               int *pvad);

/**
 *
 * mixer_read_audio_pcm
 * Used to read (poll) samples from the mixer
 *
 * @return - The number of samples read or -1 on error.
 * pContext - The participant context instance returned by a call to participant_add.
 * pcmSamples - PCM 16 bit input samples.
 * numSamplesMin - minimum count of input samples to read.
 * numSamplesMax - maximum count of input samples to read.
 * pvad - Optional output parameter to store the computed VAD of the aggregate
 *        output samples.
 *
 */
int mixer_read_audio_pcm(void *pContext,
                         int16_t *pcmSamples,
                         unsigned int numSamplesMin,
                         unsigned int numSamplesMax,
                         int *pvad);


#if defined(MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)
int participant_set_ntp_time(struct PARTICIPANT *pParticipant, const NTP_SYNC_T *pNtpSync);
#endif // (MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)

#endif // __MIXER_API_H__
