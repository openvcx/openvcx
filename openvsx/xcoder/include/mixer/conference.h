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


#ifndef __CONFERENCE_H__
#define __CONFERENCE_H__

#include <netinet/in.h>
#include <pthread.h>
#include "mixer/mixdefaults.h"
#include "logutil.h"
#include "mixer/participant.h"
#include "mixer/mixer.h"
#include "mixer/configfile.h"
#include "mixer/util.h"
//#include "ntp_query.h"


typedef struct AUDIO_CONFERENCE {
  LOG_CTXT_T                 *logCtxt;
  char                        tid_tag[LOGUTIL_TAG_LENGTH];
  CONFERENCE_ID               id;
  PARTICIPANT_T              *participants;
  MIXER_T                    *pMixer;
  uint64_t                    tsHzNow;
  pthread_mutex_t             mtx;
  //unsigned int                maxParticipants;
  unsigned int                numParticipants;
  //AUDIO_CONFIG_T              defaultConfig;
  MIXER_CONFIG_T              cfg;
  //unsigned int                sampleRate;
  unsigned int                mixerLateThresholdMs;
  //int                         enableNtp;
  //NTP_QUERY_PEER_INFO_T       ntpPeers;
  TIME_STAMP_T                tmStart;           // start of absolute time for mixer
  CONFIG_STORE_T             *pConfig;
  enum THREAD_FLAG            tdFlagMixer;
  pthread_t                   ptdMixer;
  pthread_cond_t              condOutput;
  pthread_mutex_t             mtxOutput;
} AUDIO_CONFERENCE_T;


#endif // __CONFERENCE_H__
