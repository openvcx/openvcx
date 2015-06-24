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
#include "mixer/configfile.h"
#include "mixer/conference.h"

#define LOG_CTXT  (pConference ? pConference->logCtxt : NULL)

//#define DEBUG_MIXER_TIMING 1

static TIME_STAMP_T conference_getAbsoluteTime(AUDIO_CONFERENCE_T *pConference) {
  TIME_STAMP_T tm;

  tm = time_getTime();

  //if(pConference->ntpInfo.valid) {
  //  tm += (TIME_STAMP_T) (pConference->ntpInfo.offsetMs * 1000);
  //}

  if(tm > pConference->tmStart) {
    tm -= pConference->tmStart;
  }

  return tm;
}

static u_int64_t conference_getAbsoluteTimeHz(AUDIO_CONFERENCE_T *pConference) {
  u_int64_t hz;

  hz = pConference->cfg.sampleRate * conference_getAbsoluteTime(pConference) / TIME_VAL_US;

  return hz;
}

static uint64_t setConferenceTsHz(AUDIO_CONFERENCE_T *pConference, u_int64_t tsHzNow) {

  pthread_mutex_lock(&pConference->mtx); 

  pConference->tsHzNow = tsHzNow;

  pthread_mutex_unlock(&pConference->mtx); 

  return pConference->tsHzNow;
}


static void mixer_runproc(void *pArg) {
  int rc;
  AUDIO_CONFERENCE_T *pConference = (AUDIO_CONFERENCE_T *) pArg;
  u_int64_t tsHzProduced = 0;
  const unsigned int sleepMs = 5;
  unsigned int resetThresholdHz;
  int participantCount;
  TIME_STAMP_T tmHzNow;
  TIME_STAMP_T tmHzElapsed = 0;
  TIME_STAMP_T tmOffsetFromStartHz;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  TIME_STAMP_T tmLastDump = time_getTime();
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  //
  // This thread procedure is used to run the mixer
  //

  logutil_tid_add(pthread_self(), pConference->tid_tag);

  pConference->tdFlagMixer = THREAD_FLAG_RUNNING;

  LOG(X_DEBUG("Mixer master thread for conference id %d started"), pConference->id);

  tmOffsetFromStartHz = conference_getAbsoluteTimeHz(pConference);

  resetThresholdHz = pConference->pMixer->thresholdHz * 2;

  setConferenceTsHz(pConference, tmOffsetFromStartHz);

  while(pConference->tdFlagMixer == THREAD_FLAG_RUNNING) {

    participantCount = mixer_count_active(pConference->pMixer);

    tmHzNow = conference_getAbsoluteTimeHz(pConference);
    tmHzElapsed = tmHzNow - tmOffsetFromStartHz;

    mixer_check_idle(pConference->pMixer, tmHzElapsed);

    //if(tmHzNow > tmHzMark + pConference->cfg.sampleRate) {
      //fprintf(stderr, "\n\n%llu\n", time_getTime()/1000);
      //mixer_dump(stderr, pConference->pMixer, 1, NULL);
    //  tmHzMark = tmHzNow;
    //}
    //fprintf(stderr, "tsHzNow:%lluHz, actual tm:%lluHz delta:%lluHz\n", pConference->tsHzNow, tmHzElapsed, pConference->tsHzNow < tmHzElapsed ? tmHzElapsed-pConference->tsHzNow : pConference->tsHzNow-tmHzElapsed);

    if(pConference->tsHzNow + resetThresholdHz < tmHzElapsed) {

      if(participantCount > 0) {
        LOG(X_WARNING("Reset mixer clock by %lluHz, %llu -> %llu, threshold: %uHz, after %lluHz output samples"), 
           (tmHzElapsed - pConference->tsHzNow), pConference->tsHzNow, tmHzElapsed, resetThresholdHz, tsHzProduced);
        mixer_dumpLog(S_DEBUG, pConference->pMixer, 1, "after mixer clock reset");
        //tmHzMark = tmHzNow;
        //mixer_reset_sources(pConference->pMixer);


      }

      //TODO: since the time skips forward, allow the mixer to de-activate any idle streams
      // and then need to get all the queued samples from other active streams, w/o time skip

      setConferenceTsHz(pConference, tmHzElapsed);

      //TODO: should we run mixer_check_idle here since the time skipped forward
      mixer_check_idle(pConference->pMixer, tmHzElapsed);

    }


    //TODO: mixer needs to be called in a loop while rc > 0

    if(participantCount < 1) {

        setConferenceTsHz(pConference, tmHzElapsed);
        tsHzProduced = 0;

        usleep(sleepMs * 1000);

    } else if((rc = mixer_mix(pConference->pMixer, pConference->tsHzNow)) < 0) {

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        TIME_STAMP_T t0 = time_getTime();
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

        usleep(sleepMs * 1000);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        LOG(X_DEBUG("---mixer (rc:%d) slept for %lld ns"), rc, time_getTime() - t0);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    } else if(rc == 0) {

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        TIME_STAMP_T t0 = time_getTime();
        //LOG(X_DEBUG("---mixer %llu.%llu (rc:0) sleeping)", t0/1000000, t0%1000000);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

        usleep(sleepMs * 1000);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        //LOG(X_DEBUG("---mixer %llu.%llu (rc:0) slept for %lld ns"), time_getTime()/1000000, time_getTime()%1000000, time_getTime() - t0);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    } else if(rc > 0) {

      setConferenceTsHz(pConference, pConference->tsHzNow + rc);
      tsHzProduced += rc;

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      //LOG(X_DEBUG("---mixer %llu.%llu produced rc:%d (tsHzProduced:%llu), tsHzNow at: %lluHz"), time_getTime()/1000000, time_getTime()%1000000, rc, tsHzProduced + rc, pConference->tsHzNow);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

      mixer_cond_broadcast(&pConference->condOutput, &pConference->mtxOutput);

    }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    if(time_getTime() - tmLastDump > 200000) {
      tmLastDump = time_getTime();
      //LOG(X_DEBUG("---mixer %llu.%llu dumping mixer, tsHzProduced:%lluHz"), time_getTime()/1000000, time_getTime()%1000000,  tsHzProduced);
      mixer_dumpLog(S_DEBUG, pConference->pMixer, 1, "from conf");
    }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  }

  LOG(X_DEBUG("Mixer master thread ending"));

  pConference->tdFlagMixer = THREAD_FLAG_STOPPED;

  logutil_tid_remove(pthread_self());

}

int conference_participant_count(AUDIO_CONFERENCE_T *pConference) {
  int count;

  if(!pConference) {
    return 0;
  }

  count = pConference->numParticipants;

  return count;
}

static AUDIO_CONFERENCE_T *conference_create_internal(LOG_CTXT_T *logCtxt,
                                                      CONFERENCE_ID id,
                                                      const MIXER_CONFIG_T *pCfg) {
  int rc = 0;
  AUDIO_CONFERENCE_T *pConference;
  pthread_attr_t attr;
  struct sched_param param;

  if(!(pConference = (AUDIO_CONFERENCE_T *) calloc(1, sizeof(AUDIO_CONFERENCE_T)))) {
    return NULL;
  }


  pConference->logCtxt = logCtxt;
  pthread_mutex_init(&pConference->mtxOutput, NULL);
  pthread_cond_init(&pConference->condOutput, NULL);
  pthread_mutex_init(&pConference->mtx, NULL);

  memcpy(&pConference->cfg, pCfg, sizeof(pConference->cfg));

  pConference->tdFlagMixer = THREAD_FLAG_STOPPED;
  if(pConference->cfg.maxParticipants == 0) {
    pConference->cfg.maxParticipants = MAX_PARTICIPANTS;
  }

  if(pConference->cfg.sampleRate > MIXER_MAX_SAMPLE_RATE) {
    LOG(X_ERROR("Invalid mixer sample rate %d"), pConference->cfg.sampleRate);
    return NULL;
  } else if(pConference->cfg.sampleRate == 0) {
    pConference->cfg.sampleRate = MIXER_SAMPLE_RATE;
  }

  if(pConference->cfg.mixerLateThresholdMs == 0) {
    pConference->cfg.mixerLateThresholdMs = MIXER_LATE_THRESHOLD_MS;
  }

  pConference->id = id;
#if !defined(MIXER_HAVE_NTP) || (MIXER_HAVE_NTP <= 0)
  pConference->cfg.enableNtp = 0;
#endif // (MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)

  LOG(X_INFO("Created conference id %d with %d max participants, sample rate: %dHz, late threshold: %dms"), 
      pConference->id, pConference->cfg.maxParticipants, pConference->cfg.sampleRate, 
      pConference->cfg.mixerLateThresholdMs);

#if defined(MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)

  //
  // Get the NTP offset of the local system clock
  //
  if(pConference->cfg.enableNtp) {

    if(ntp_query_peers(&pConference->ntpPeers, NULL) != 0) {
      LOG(X_ERROR("Failed to query NTP peers!"));
      //conference_destroy(pConference);
      //return NULL;
    } else {
      pConference->ntpPeers.valid = 1;

      LOG(X_DEBUG("Obtained local system NTP offset: %.3fms"), pConference->ntpPeers.offsetMs);
    }

  }
#endif //(MIXER_HAVE_NTP) && (MIXER_HAVE_NTP > 0)

  if(!(pConference->pMixer = mixer_init(pConference->logCtxt,
                                        pConference->cfg.mixerLateThresholdMs * pConference->cfg.sampleRate / 1000, 
                                        0))) {
    conference_destroy(pConference);
    return NULL;
  }


  //
  // Record the start of absolute time for the mixer conference
  //
  pConference->tmStart = time_getTime();

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  if(pConference->cfg.highPriority) {
    LOG(X_DEBUG("Setting mixer thread to high priority"));
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_attr_setschedparam(&attr, &param);
  }

  snprintf(pConference->tid_tag, sizeof(pConference->tid_tag), "mixer");
  pConference->tdFlagMixer = THREAD_FLAG_STARTING;
  if((rc = pthread_create(&pConference->ptdMixer,
                    &attr,
                    (void *) mixer_runproc,
                    (void *) pConference) != 0)) {
    LOG(X_ERROR("Unable to create mixer master thread"));
    pConference->tdFlagMixer = THREAD_FLAG_STOPPED;
    rc = -1;
  }
  pthread_attr_destroy(&attr);

  while(pConference->tdFlagMixer == THREAD_FLAG_STARTING) {
    usleep(5000);
  }

  if(pConference->tdFlagMixer == THREAD_FLAG_STOPPED) {
    rc = -1;
  }
  
  if(rc < 0) {
    conference_destroy(pConference);
    pConference = NULL;
  }

  return pConference;
}

AUDIO_CONFERENCE_T *conference_create(const MIXER_CONFIG_T *pCfg, CONFERENCE_ID id) {

  AUDIO_CONFERENCE_T *pConference = NULL;
  LOG_CTXT_T *logCtxt = NULL;

  if(!pCfg) {
    return NULL;
  }

#if defined(__APPLE__)
  //TODO: logger global (extern or static) is multiply defined
  // in multiple .so
  if(pCfg->pLogCtxt) {
    logger_setContext(pCfg->pLogCtxt);
  } else if(pCfg->verbosity > 1) {
    logger_SetLevel(S_DEBUG);
    logger_AddStderr(S_DEBUG, 0);
  } else {
    logger_SetLevel(S_INFO);
    logger_AddStderr(S_INFO, 0);
  }
#endif // __APPLE__


  if(!(pConference = conference_create_internal(logCtxt,
                                                id,
                                                pCfg))) {
    return NULL;
  }

  pConference->pConfig = NULL;

#if !defined(XCODE_HAVE_SPEEX) || (XCODE_HAVE_SPEEX == 0)
  if(pConference->cfg.ac..mixerVad) {
    pConference->cfg.ac.mixerVad = 0;
  }
  if(pConference->cfg.ac.mixerAgc) {
    pConference->cfg.ac.mixerAgc = 0;
  }
  if(pConference->cfg.ac..mixerDenoise) {
    pConference->cfg.ac.mixerDenoise = 0;
  }
#endif // (XCODE_HAVE_SPEEX) || (XCODE_HAVE_SPEEX == 0)

#if !defined(MIXER_HAVE_RTP) || (MIXER_HAVE_RTP== 0)
  if(pConference->cfg.ac.setOutputVad) {
    pConference->cfg.ac.setOutputVad = 0;
  }
  if(pConference->cfg.ac.acceptInputVad) {
    pConference->cfg.ac.acceptInputVad = 0;
  }
#endif // (MIXER_HAVE_RTP) || (MIXER_HAVE_RTP == 0)


#if !defined(MIXER_HAVE_SILK) || (MIXER_HAVE_SILK == 0)
  if(pConference->cfg.ac.encoderComplexity) {
    pConference->cfg.ac.encoderComplexity = 0;
  }
  if(pConference->cfg.ac.encoderBitrate) {
    pConference->cfg.ac.encoderBitrate = 0;
  }
#endif // (MIXER_HAVE_SILK) || (MIXER_HAVE_SILK == 0)

  return pConference;
}

void conference_destroy(AUDIO_CONFERENCE_T *pConference) {
  PARTICIPANT_ID partId;
  CONFERENCE_ID confId;

  if(!pConference) {
    return;
  }

  confId = pConference->id;

  //
  // Remove all participants
  //
  while(pConference->participants) {
    pthread_mutex_lock(&pConference->mtx); 
    if(pConference->participants) {
      partId = pConference->participants->config.id;
    } else {
      partId = -1;
    }
    pthread_mutex_unlock(&pConference->mtx); 
    if(partId >= 0) {
      participant_remove_id(pConference, partId);
    }
  }

  while(pConference->tdFlagMixer > THREAD_FLAG_STOPPED) {
    pConference->tdFlagMixer = THREAD_FLAG_DOEXIT;
    usleep(5000);
  }

  if(pConference->pMixer) {
    mixer_free(pConference->pMixer);
    pConference->pMixer = NULL;
  }

  if(pConference->pConfig) {
    config_free(pConference->pConfig);
    pConference->pConfig = NULL;
  }

  pthread_mutex_destroy(&pConference->mtxOutput);
  pthread_cond_destroy(&pConference->condOutput);
  pthread_mutex_destroy(&pConference->mtx);

#if defined(MIXER_HAVE_LOGGER) && (MIXER_HAVE_LOGGER > 0)
  Logger_DestroyContext(pConference->logCtxt);
#endif // (MIXER_HAVE_LOGGER) && (MIXER_HAVE_LOGGER > 0)

  free(pConference);

}

const MIXER_CONFIG_T *conference_get_config(struct AUDIO_CONFERENCE *pConference) {
  if(!pConference) {
    return NULL;
  }

  return &pConference->cfg;
}

void conference_dump (FILE *fp, struct AUDIO_CONFERENCE *pConference) {

  if(!fp || !pConference) {
    return;
  }

  if(pConference->pMixer) {
    mixer_dump(fp, pConference->pMixer, 1, NULL);
  }

}





