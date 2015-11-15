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


#include "vsx_common.h"

#if defined(VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP)

//
// Global incrementing PIP index
//
static int s_pipId = 1;

typedef struct START_PIP_ARGS {
  VSXLIB_STREAM_PARAMS_T     params;
  char                        input[VSX_MAX_PATH_LEN];
  char                        stopChimeFile[VSX_MAX_PATH_LEN];
  double                      fps;
  STREAMER_CFG_T             *pCfgOverlay;
  STREAMER_CFG_T             *pStreamerCfg;
  CAPTURE_LOCAL_DESCR_T      *pCapCfg;
  unsigned int                idxPip;
  unsigned int                pipXcodeIdx;
  unsigned int                pip_id;
  int                         do_server;
  char                        tid_tag[LOGUTIL_TAG_LENGTH];
} START_PIP_ARGS_T;

#define IS_PIP_FINISHED(running) ((running) == STREAMER_STATE_FINISHED_PIP || \
                                  (running) == STREAMER_STATE_ERROR)

static int pip_stop_int(STREAMER_CFG_T *pCfgOverlay, unsigned int pip_id, int lock);

#if 0
static void pip_dumpall(STREAMER_CFG_T *pCfgOverlay) {
  unsigned int idxPip;

  //pthread_mutex_lock(&pCfgOverlay->pip.mtx);

  for(idxPip = 0; idxPip < MAX_PIPS; idxPip++) {
    fprintf(stderr, "overlay.p_pips[%d]:0x%x active:%d, indexPlus1:%d, id:%d (active:%d, indexPlus1:%d, id:%d)\n", idxPip, pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip], 
    pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip] ?  pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip]->pip.active : -1,
    pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip] ?  pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip]->pip.indexPlus1 : -1,
    pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip] ?  pCfgOverlay->xcode.vid.overlay.p_pipsx[idxPip]->pip.id: -1,
    pCfgOverlay->pStorageBuf->xcodeCtxtPips[idxPip].vidData.piXcode ? pCfgOverlay->pStorageBuf->xcodeCtxtPips[idxPip].vidData.piXcode->vid.pip.active : -1,
    pCfgOverlay->pStorageBuf->xcodeCtxtPips[idxPip].vidData.piXcode ? pCfgOverlay->pStorageBuf->xcodeCtxtPips[idxPip].vidData.piXcode->vid.pip.indexPlus1 : -1,
    pCfgOverlay->pStorageBuf->xcodeCtxtPips[idxPip].vidData.piXcode ? pCfgOverlay->pStorageBuf->xcodeCtxtPips[idxPip].vidData.piXcode->vid.pip.id : -1);

  }

  //pthread_mutex_unlock(&pCfgOverlay->pip.mtx);

}
#endif // 0

static void start_pip_proc(void *pArg) {
  int rc = 0;
  int do_start = 1;
  START_PIP_ARGS_T pip;
  PIP_LAYOUT_MGR_T *pLayout = NULL;

  //
  // pArg may fall out of scope once the 'running' flag is signalled
  // 
  memcpy(&pip, (START_PIP_ARGS_T *) pArg, sizeof(START_PIP_ARGS_T));
  if(pip.pCapCfg) {
    pip.pCapCfg->common.localAddrs[0] = pip.input;
  }

  pip.pStreamerCfg->running = STREAMER_STATE_STARTING;

  logutil_tid_add(pthread_self(), pip.tid_tag);

  LOG(X_DEBUG("Started %s%s (xcode idx:%d, live:%d) for input %s"), pip.pStreamerCfg->delayed_output ? "delayed " : "", 
              pip.tid_tag, pip.pipXcodeIdx, pip.pCapCfg ? 1 : 0, pip.input);

  if(pip.pStreamerCfg->sdpsx[1][0].vid.common.available || pip.pStreamerCfg->sdpsx[1][0].aud.common.available) {
    if(pip.params.verbosity >= VSX_VERBOSITY_HIGH) {
      sdp_write(&pip.pStreamerCfg->sdpsx[1][0], NULL, S_DEBUG);
    }
  }

  //pip_dumpall(pip.pCfgOverlay);

  if(pip.pStreamerCfg->xcode.vid.common.cfgDo_xcode) {
    xcode_dump_conf_video(&pip.pStreamerCfg->xcode.vid, 0, 0, S_DEBUG);
  }
  if(pip.pStreamerCfg->xcode.aud.common.cfgDo_xcode) {
    xcode_dump_conf_audio(&pip.pStreamerCfg->xcode.aud, 0, 0, S_DEBUG);
  }

  if(pip.params.abrAuto) {
    //TODO: need to sort out use of general monitor stats & abr, which makes use of the generic monitor instance
    //if(rc >= 0 && stream_monitor_start(&pip.pStreamerCfg->pStorageBuf->streamMonitors[pip.idxPip], "log/stats.txt", pip.params.statdumpintervalms) < 0) {

    if(rc >= 0 && stream_abr_monitor(&pip.pStreamerCfg->pStorageBuf->streamMonitors[pip.idxPip], pip.pStreamerCfg) < 0) {
      rc = -1;    
      do_start = 0;
      LOG(X_ERROR("Failed to start ABR stream monitor[%d] for %s"), pip.idxPip, pip.tid_tag);
    }
    pip.pStreamerCfg->pMonitor =  &pip.pStreamerCfg->pStorageBuf->streamMonitors[pip.idxPip];
  }

  if(rc >= 0 && pip.do_server && vsxlib_setupServer(&pip.pStreamerCfg->pStorageBuf->srvParamPips[pip.idxPip], &pip.params, 0) < 0) {
    LOG(X_ERROR("Failed to setup server media listener(s) for %s"), pip.tid_tag);
    rc = -1;
    do_start = 0;
  }

  //
  // Update the overlay PIP layout with the addition of the new PIP
  //
  if(rc >= 0 && (pLayout = ((STREAM_XCODE_AUD_UDATA_T *) pip.pStreamerCfg->xcode.aud.pUserData)->pPipLayout)) {
    pip_layout_update(pLayout, PIP_UPDATE_PARTICIPANT_START);
  }

  if(do_start) {

    if(pip.pCapCfg) {

      if(vsxlib_streamLiveCaptureCfg(pip.pCapCfg, pip.pStreamerCfg) < 0) {
        rc = -1;
      }

    } else {

      if(vsxlib_streamFileCfg(pip.input, pip.fps, pip.pStreamerCfg) < 0) {
        rc = -1;
      }
    }
  }

  LOG(X_DEBUG("Finished streaming %s (xcode idx:%d, live:%d) for input %s"), pip.tid_tag, pip.pipXcodeIdx, pip.pCapCfg ? 1 : 0, pip.input);

  if(pip.pStreamerCfg->running != STREAMER_STATE_ERROR) {
    pip.pStreamerCfg->running = STREAMER_STATE_FINISHED_PIP;
  }

  //
  // If this streamer has ended on its own, remove the PIP resources
  // Warning:  after we call pip_stop, the pip index is fair game to be used by a newly started pip
  //

  pthread_mutex_lock(&pip.pCfgOverlay->pip.mtx);

  if(pip_getIndexById(pip.pCfgOverlay,  pip.pip_id, 0) >= 0) {
    pip_stop_int(pip.pCfgOverlay, pip.pip_id, 0);
  }

  if(pip.pStreamerCfg->pMonitor) {
    stream_monitor_stop(pip.pStreamerCfg->pMonitor);
    pip.pStreamerCfg->pMonitor->active = 0;
    pip.pStreamerCfg->pMonitor = NULL;
  }

  pthread_mutex_unlock(&pip.pCfgOverlay->pip.mtx);

  //
  // Play pip stop announcement
  // Warning: since we already called pip_stop, pip.pStreamerCfg is fair game to be used by a newly created pip
  //
  if(do_start && pip.stopChimeFile[0] != '\0') {
    LOG(X_DEBUG("Starting PIP end announcement '%s'"), pip.stopChimeFile); 
    pip_announcement(pip.stopChimeFile, pip.pCfgOverlay);
  }

  if(pip.do_server) {
    //TODO: memset for next pip... so that server listener duplicate check is ok
    vsxlib_closeServer(&pip.pStreamerCfg->pStorageBuf->srvParamPips[pip.idxPip]);
  }

  if(do_start) {
    //
    // Update the overlay PIP layout after the PIP has been removed
    //
    pip_layout_update(pLayout, PIP_UPDATE_PARTICIPANT_END);
  }


  //pip_dumpall(pip.pCfgOverlay);

  logutil_tid_remove(pthread_self());

}

void pip_free_motion(IXCODE_PIP_MOTION_T *pMotion, int do_free) {
  IXCODE_PIP_MOTION_VEC_T *pVec = NULL;

  if(!pMotion) {
    return;
  }
  pVec = pMotion->pm;

  while(pVec) {
    pMotion->pm = pVec->pnext;
    free(pVec);
    pVec = pMotion->pm;
  }

  if(do_free) {
    free(pMotion);
  }

}

void pip_free_overlay(IXCODE_VIDEO_OVERLAY_T *pOverlay) {
  unsigned int idx;

  if(!pOverlay) {
    return;
  }

  for(idx = 0; idx < MAX_PIPS; idx++) {

    if(pOverlay->p_pipsx[idx] && pOverlay->p_pipsx[idx]->pip.pMotion) {

      pip_free_motion(pOverlay->p_pipsx[idx]->pip.pMotion, 1);
      pOverlay->p_pipsx[idx]->pip.pMotion = NULL;

    }
  }

  pthread_mutex_destroy(&pOverlay->mtx);

}

#if 0
static void pip_dump_order(IXCODE_VIDEO_CTXT_T *pPipsX[MAX_PIPS]) {
  unsigned int idx;

  for(idx = 0; idx < MAX_PIPS; idx++) {
    fprintf(stderr, "p_pipsx[%d]:0x%x, active:%d, z-order:%d, indexPlus1:%d\n", idx, pPipsX[idx], pPipsX[idx] ? pPipsX[idx]->pip.active : -99, pPipsX[idx] ? pPipsX[idx]->pip.zorder : -99, pPipsX[idx] ? pPipsX[idx]->pip.indexPlus1 : -99);
  }
}
#endif // 0

static int pip_add_zordered(IXCODE_VIDEO_CTXT_T *pPipsX[MAX_PIPS], int zorder) {
  unsigned int idx, idx2;
  int rc = -1;

  //fprintf(stderr, "PIP_ADD BEFORE z-order:%d\n", zorder); pip_dump_order(pPipsX);

  if(!pPipsX[MAX_PIPS -1]) {
 
    for(idx = 0; idx < MAX_PIPS; idx++) {

      if(!pPipsX[idx]) {

	rc = idx;
        break;

      } else if(zorder < pPipsX[idx]->pip.zorder) {

	for(idx2 = MAX_PIPS - 1; idx2 > idx; idx2--) {
	  pPipsX[idx2] = pPipsX[idx2 - 1];
	}

	rc = idx;
        break;
      }

    }

  }

  return rc;
}

int pip_update_zorder(IXCODE_VIDEO_CTXT_T *pPipsArg[MAX_PIPS]) {
  IXCODE_VIDEO_CTXT_T *pPipsTemp[MAX_PIPS];
  unsigned int idx;
  int rc;

  memset(pPipsTemp, 0, sizeof(pPipsTemp));

  for(idx = 0; idx < MAX_PIPS; idx++) {

    if(!pPipsArg[idx]) {
      //break;
      continue;
    }

    if((rc = pip_add_zordered(pPipsTemp, pPipsArg[idx]->pip.zorder)) >= 0) {
      pPipsTemp[rc] = pPipsArg[idx];
    }

  }

  memcpy(pPipsArg, pPipsTemp, sizeof(pPipsTemp));

  return 0;
}

static IXCODE_VIDEO_PIP_T *pip_remove(IXCODE_VIDEO_OVERLAY_T *pXOverlay, unsigned int idxPipPlus1) {
  IXCODE_VIDEO_CTXT_T *pPipX = NULL;
  unsigned int idx;
  //int havePip = 0;
  int shift = 0;

  pthread_mutex_lock(&pXOverlay->mtx);

  //fprintf(stderr, "PIP_REMOVE BEFORE idxPipPlus1:%d:\n",idxPipPlus1); pip_dump_order(pXOverlay->p_pipsx);

  for(idx = 0; idx < MAX_PIPS; idx++) {
    if(pXOverlay->p_pipsx[idx] && pXOverlay->p_pipsx[idx]->pip.indexPlus1 == idxPipPlus1) {
      pPipX = pXOverlay->p_pipsx[idx];
      pXOverlay->p_pipsx[idx] = NULL;
      shift = 1;
    } else if(shift) {
      pXOverlay->p_pipsx[idx - 1] = pXOverlay->p_pipsx[idx];
    }

    //if(pXOverlay->p_pipsx[idx]) {
    //  havePip = 1;
    //}
  }
  if(shift) {
    pXOverlay->p_pipsx[MAX_PIPS-1] = NULL;
  }

  //pXOverlay->havePip = havePip;

  //fprintf(stderr, "PIP_REMOVE AFTER: pPip:0x%x\n", pPip); pip_dump_order(pXOverlay->p_pipsx);

  pthread_mutex_unlock(&pXOverlay->mtx);

  return pPipX ? &pPipX->pip : NULL;
}

int pip_getIndexById(STREAMER_CFG_T *pCfgOverlay,  unsigned int id, int lock) {
  unsigned int idx;
  int cfgPipIdx = -1;
  IXCODE_VIDEO_CTXT_T *pXcode;

  if(!pCfgOverlay) {
    return -1;
  }

  if(lock) {
    pthread_mutex_lock(&pCfgOverlay->pip.mtx);
  }

  for(idx = 0; idx < MAX_PIPS; idx++) {
    //fprintf(stderr, "pip_getIndexById id:%d, p_pipsx[%d].active:%d, id:%d, indexPlus1:%d\n", id, idx, pCfgOverlay->xcode.vid.overlay.p_pipsx[idx] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idx]->pip.active : -1, pCfgOverlay->xcode.vid.overlay.p_pipsx[idx] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idx]->pip.id : -1, pCfgOverlay->xcode.vid.overlay.p_pipsx[idx] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idx]->pip.indexPlus1 : -1);
    //fprintf(stderr, "pip_getIndexById id:%d, pip[%d].active:%d, id:%d, indexPlus1:%d\n", id, idx, pCfgOverlay->pip.pCfgPips[idx]->xcode.vid.pip.active, pCfgOverlay->pip.pCfgPips[idx]->xcode.vid.pip.id, pCfgOverlay->pip.pCfgPips[idx]->xcode.vid.pip.indexPlus1);
    //if((pXcode = pCfgOverlay->xcode.vid.overlay.p_pipsx[idx])) {
    if((pXcode = &pCfgOverlay->pip.pCfgPips[idx]->xcode.vid)) {
      if(id == pXcode->pip.id) {
        if(pXcode->pip.indexPlus1 > 0) {
          cfgPipIdx = pXcode->pip.indexPlus1 - 1;
        }
        break;
      }
    }
  }

  if(lock) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
  }

  //fprintf(stderr, "PIP_GETINDEXBYID pip_id:%d, idx:%d\n", id, cfgPipIdx); pip_dumpall(pCfgOverlay);

  return cfgPipIdx;
}

int pip_getActiveIds(STREAMER_CFG_T *pCfgOverlay,  int ids[MAX_PIPS], int lock) {
  unsigned int idx;
  unsigned int idxActive = 0;
  IXCODE_VIDEO_CTXT_T *pXcode;

  if(!pCfgOverlay) {
    return -1;
  }

  memset(ids, 0, sizeof(ids));

  if(lock) {
    pthread_mutex_lock(&pCfgOverlay->pip.mtx);
  }

  for(idx = 0; idx < MAX_PIPS; idx++) {
    //if((pXcode = pCfgOverlay->xcode.vid.overlay.p_pipsx[idx])) {
    if((pXcode = &pCfgOverlay->pip.pCfgPips[idx]->xcode.vid)) {
      if(pXcode->pip.active && pXcode->pip.indexPlus1 > 0) {
          ids[idxActive++] = pXcode->pip.id; 
      }
    }
  }

  if(lock) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
  }

  //fprintf(stderr, "PIP_GETINDEXBYID pip_id:%d, idx:%d\n", id, cfgPipIdx); pip_dumpall(pCfgOverlay);

  return idxActive;
}

static int pip_stop_int(STREAMER_CFG_T *pCfgOverlay, unsigned int pip_id, int lock) {
  int rc = 0;
  int idxPip;
  int pipRunning = 0;
  STREAMER_CFG_T *pCfgPip = NULL;
  IXCODE_VIDEO_PIP_T *pPip = NULL;

  if(!pCfgOverlay) {
    return -1;
  } 

  //fprintf(stderr, "PIP_STOP pip_id:%d\n", pip_id);

  if(lock) {
    pthread_mutex_lock(&pCfgOverlay->pip.mtx);
  }

  if((idxPip = pip_getIndexById(pCfgOverlay,  pip_id, 0)) < 0 || idxPip >= MAX_PIPS) {
    LOG(X_ERROR("Invalid PIP id to stop %d"), pip_id);
    if(lock) {
      pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    }
    return -1;
  }

  //conference_dump(stderr, pCfgOverlay->xcode.aud.pMixer);

  pCfgPip = pCfgOverlay->pip.pCfgPips[idxPip];

  //fprintf(stderr, "PIP_STOP pip_id:%d, idxPip:%d, running:%d\n", pip_id, idxPip, pCfgPip->running);

  if(pCfgPip->running == STREAMER_STATE_STARTING && pCfgPip->delayed_output > 0) {

    //
    // This stream transmitter thread is waiting for the delayed_output flag to clear while waiting in capture_net.c
    //
    LOG(X_DEBUG("Stopping delayed stream output PIP-%d[%d]"), pip_id, idxPip);
    pipRunning = 1;
    pCfgPip->running =  STREAMER_STATE_INTERRUPT;

  } else if(pCfgPip->running == STREAMER_STATE_RUNNING) {

    LOG(X_DEBUG("Stopping PIP-%d[%d]"), pip_id, idxPip);
    pipRunning = 1;
    //fprintf(stderr, "PIP STOP.. while running:%d, calling streamer_stop\n", pCfgPip->running);

    streamer_stop(pCfgPip, 1, 0, 0, 0);

    while(pCfgPip->running >= STREAMER_STATE_RUNNING) {
      //LOG(X_DEBUG("PIP_STOP... cond_broadcast... running;%d"), pCfgPip->running);
      vsxlib_cond_broadcast(&pCfgPip->pip.cond, &pCfgPip->pip.mtxCond);
      //LOG(X_DEBUG("PIP_STOP... loop sleep..."));
      usleep(100000);
      //usleep(5000);
    }
    //LOG(X_DEBUG("PIP_STOP... running now:%d"), pCfgPip->running);

  } else {
    //pipRunning = 0;
  }

  //LOG(X_DEBUG("PIP-%d[%d] not running (overlay.havePip:%d, state:%d)"), pip_id, idxPip, pCfgOverlay->xcode.vid.overlay.havePip, pCfgPip->running);

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  //if(pCfgOverlay->xcode.aud.pMixer && participant_remove_id(pCfgOverlay->xcode.aud.pMixer, idxPip + 1) == 0) {
  if(pCfgPip->xcode.aud.pMixerParticipant) {
    //if(participant_remove(pCfgOverlay->xcode.aud.pMixer, pCfgPip->xcode.aud.pMixerParticipant) == 0) {
    
    if(participant_remove(pCfgPip->xcode.aud.pMixerParticipant) == 0) {
      pCfgPip->xcode.aud.pMixerParticipant = NULL;
      //fprintf(stderr, "removed mixer participant\n");
    } else {
      LOG(X_DEBUG("Failed to remove mixer participant for PIP-%d[%d]"), pip_id, idxPip);
    }
  }

#endif // XCODE_HAVE_PIP_AUDIO

  //fprintf(stderr, "PIP_STOP calling pip_remove: %d\n", idxPip);

  //
  // Detach the PIP from the xcode overlay context
  //
  if((pPip = pip_remove(&pCfgOverlay->xcode.vid.overlay, idxPip + 1))) {

    //
    // Prevent the PIP from being shown in any overlays
    //
    if(pPip->visible > 0) {
      pPip->visible = 0;
    }

    pPip->showVisual = 0;
    pPip->active = 0;
    pPip->pPrivData = NULL;
    //pPip->pOverlay = NULL;
    pPip->pXOverlay = NULL;

    if(pPip->pMotion) {
      pip_free_motion(pPip->pMotion, 1);
      pPip->pMotion = NULL;
    }

  }

  pCfgOverlay->pip.tvLastPipAction = timer_GetTime();

  if(pCfgOverlay->xcode.vid.overlay.havePip && !pCfgOverlay->xcode.vid.overlay.p_pipsx[0]) {
    pCfgOverlay->xcode.vid.overlay.havePip = 0;
  }
//TODO: check each overlay if doEncode is set

  if(pipRunning) {
    LOG(X_DEBUG("Stopped PIP-%d[%d], have pip:%d"), pip_id, idxPip, pCfgOverlay->xcode.vid.overlay.havePip);
  } else {
    rc = -1;
  }

  if(lock) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
  }

  //conference_dump(stderr, pCfgOverlay->xcode.aud.pMixer);

  return rc;
}

int pip_stop(STREAMER_CFG_T *pCfgOverlay, unsigned int pip_id) {
  int rc = 0;

  if(!pCfgOverlay) {
    return -1;
  } 

  pthread_mutex_lock(&pCfgOverlay->pip.mtx);

  rc = pip_stop_int(pCfgOverlay, pip_id, 0);

  pthread_mutex_unlock(&pCfgOverlay->pip.mtx);

  return rc;
}

static int pip_validate_output_listener(const PIP_CFG_T *pPipCfgArg, const STREAMER_CFG_T *pCfgOverlay) {
  unsigned int idxPip;
  int rc;
  SRV_START_CFG_T *pStartCfg;
  SRV_LISTENER_CFG_T srvListenMedia[SRV_LISTENER_MAX + 1];
  const char *arrAddr[SRV_LISTENER_MAX + 1];


  if(pPipCfgArg->tsliveaddr) {

    for(idxPip = 0; idxPip <= MAX_PIPS; idxPip++) {

      if(IS_PIP_FINISHED(pCfgOverlay->pip.pCfgPips[idxPip]->running)) {
        continue;
      }

      if(idxPip < MAX_PIPS) {  
        pStartCfg = &pCfgOverlay->pStorageBuf->srvParamPips[idxPip].startcfg;
      } else {
        pStartCfg = &pCfgOverlay->pStorageBuf->srvParam.startcfg;
      }

      memset(arrAddr, 0, sizeof(arrAddr));
      arrAddr[0] = pPipCfgArg->tsliveaddr;
      memcpy(srvListenMedia, pStartCfg->listenMedia, SRV_LISTENER_MAX * sizeof(SRV_LISTENER_CFG_T));
      memset(&srvListenMedia[SRV_LISTENER_MAX], 0, sizeof(SRV_LISTENER_CFG_T));

      if((rc = vsxlib_parse_listener(arrAddr, SRV_LISTENER_MAX + 1, srvListenMedia, URL_CAP_TSLIVE, NULL)) <= 0) {
        return -1;
      }
    }

    //pipArgs.params.tsliveaddr[0] = pPipCfgArg->tsliveaddr;
  }

  return 0;
}

/*
static void make_border(IXCODE_VIDEO_CROP_T *pCrop) {
  pCrop->padTopCfg = pCrop->padTop = 2;
  pCrop->padBottomCfg = pCrop->padBottom = 2;
  pCrop->padLeftCfg = pCrop->padLeft = 2;
  pCrop->padRightCfg = pCrop->padRight = 2;
  pCrop->padRGB[0] = pCrop->padRGB[1] = pCrop->padRGB[2] = 0x44;
}
*/

static int pip_load_xcode(const PIP_CFG_T *pPipCfgArg, const STREAMER_CFG_T *pCfgOverlay, STREAMER_CFG_T *pCfgPip, 
                          const VSXLIB_STREAM_PARAMS_T *pParams) {
  unsigned int xidx;
  int rc = 0;
  char xcodebuf[1024];

  // Set an output fps so if we read input through sdp, input stream may not contain inline fps.  Set v/cfrout=-1 to force outpts = inpts, so each input frame will be processed for pip

//#define PIP_XCODE_STR_DEFAULT "vc=yuva420p,vfr=30,vcfrout=-1"
#define PIP_XCODE_STR_DEFAULT "vc=yuva420p"
//#define PIP_XCODE_PAD ",padLeft=2,padRight=2,padTop=2,padBottom=2,padColorRGB=0x444444"
  rc = snprintf(xcodebuf, sizeof(xcodebuf), PIP_XCODE_STR_DEFAULT);

  if(pPipCfgArg->strxcode && pPipCfgArg->strxcode[0] != '\0') {
    snprintf(&xcodebuf[rc], sizeof(xcodebuf) - rc, ",%s", pPipCfgArg->strxcode);
    LOG(X_DEBUG("PIP xcode config string: '%s'"), xcodebuf);
  }

  for(xidx = 0; xidx < IXCODE_VIDEO_OUT_MAX; xidx++) {
    pCfgPip->xcode.vid.out[0].cfgScaler = pCfgOverlay->xcode.vid.out[0].cfgScaler;
    pCfgPip->xcode.vid.out[0].cfgFast = pCfgOverlay->xcode.vid.out[0].cfgFast;
    pCfgPip->xcode.vid.out[0].rotate.common.active = pCfgOverlay->xcode.vid.out[0].rotate.common.active;
    pCfgPip->xcode.vid.out[0].rotate.degrees = pCfgOverlay->xcode.vid.out[0].rotate.degrees;
  }

  if(xcode_parse_configstr(xcodebuf, &pCfgPip->xcode, 1, 0) < 0) {
    //pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    LOG(X_ERROR("Invalid pipxcode configuration string '%s'"), xcodebuf);
    return -1;
  }

  if(pCfgPip->xcode.vid.cfgOutClockHz == 0 && pCfgPip->xcode.vid.cfgOutFrameDeltaHz == 0) {
    //
    // Should be set to >= then overlay fps
    //
    pCfgPip->xcode.vid.cfgOutClockHz = 30;
    pCfgPip->xcode.vid.cfgOutFrameDeltaHz = 1;
  }

  if(!pPipCfgArg->novid && pCfgPip->xcode.vid.common.cfgCFROutput == 0 &&
     (pCfgPip->xcode.vid.cfgOutFrameDeltaHz == 0 || pCfgOverlay->xcode.vid.cfgOutFrameDeltaHz == 0 ||
    (pParams && !pParams->islive &&
    ((float)pCfgPip->xcode.vid.cfgOutClockHz / pCfgPip->xcode.vid.cfgOutFrameDeltaHz) >=
    ((float)pCfgOverlay->xcode.vid.cfgOutClockHz / pCfgOverlay->xcode.vid.cfgOutFrameDeltaHz)))) {
    //
    // vcfrout=-1, use same output frame ts as input unless pip output fps < overlay fps
    //
    pCfgPip->xcode.vid.common.cfgCFROutput = -1;
    LOG(X_DEBUG("vcfrout set to %d  fps:%d/%d, overlay fps:%d/%d"), pCfgPip->xcode.vid.common.cfgCFROutput, 
                pCfgPip->xcode.vid.cfgOutClockHz, pCfgPip->xcode.vid.cfgOutFrameDeltaHz,
                pCfgOverlay->xcode.vid.cfgOutClockHz, pCfgOverlay->xcode.vid.cfgOutFrameDeltaHz);
  } else {
    //LOG(X_DEBUG("CFGCFROUTPUT OUT SET TO %d\n\n\n"), pCfgPip->xcode.vid.common.cfgCFROutput);
  }

  return rc;
}

static int pip_set_output(const PIP_CFG_T *pPipCfgArg, const STREAMER_CFG_T *pCfgOverlay, STREAMER_CFG_T *pCfgPip,
                          START_PIP_ARGS_T *pPipArgs, int do_update) {
  int rc = 0;

  memcpy(&pCfgPip->cfgrtp, &pCfgOverlay->cfgrtp, sizeof(pCfgPip->cfgrtp));

  pPipArgs->params.outputs[0] = NULL;
  pPipArgs->params.pipaddr[0] = NULL;
  pPipArgs->params.dashliveaddr[0] = NULL;
  pPipArgs->params.flvliveaddr[0] = NULL;
  pPipArgs->params.flvrecord[0] = NULL;
  pPipArgs->params.statusaddr[0] = NULL;
  pPipArgs->params.configaddr[0] = NULL;
  pPipArgs->params.rtmpliveaddr[0] = NULL;
  pPipArgs->params.rtspliveaddr[0] = NULL;
  pPipArgs->params.tsliveaddr[0] = NULL;
  pPipArgs->params.mkvliveaddr[0] = NULL;
  pPipArgs->params.httpliveaddr[0] = NULL;
  pPipArgs->params.liveaddr[0] = NULL;
  memset(&pPipArgs->params.stunRequestorCfg, 0, sizeof(pPipArgs->params.stunRequestorCfg));

  //
  // PIP stream RTP output
  //
  if(pPipCfgArg->output) {

    memset(pPipArgs->params.srtpCfgs, 0, sizeof(pPipArgs->params.srtpCfgs));
    pPipArgs->params.srtpCfgs[0].keysBase64 = pPipCfgArg->srtpKeysBase64;
    pPipArgs->params.srtpCfgs[0].dtls = pPipCfgArg->use_dtls;
    pPipArgs->params.srtpCfgs[0].srtp = pPipCfgArg->use_srtp;
    pPipArgs->params.srtpCfgs[0].dtls_handshake_server = pPipCfgArg->dtls_handshake_server;
    memset(pPipArgs->params.rtcpPorts, 0, sizeof(pPipArgs->params.rtcpPorts));
    pPipArgs->params.rtcpPorts[0] = pPipCfgArg->rtcpPorts;
    memset(pPipArgs->params.outputs, 0, sizeof(pPipArgs->params.outputs));
    pPipArgs->params.outputs[0] = pPipCfgArg->output;
    pPipArgs->params.output_rtpptstr = pPipCfgArg->output_rtpptstr;
    pPipArgs->params.output_rtpssrcsstr = pPipCfgArg->output_rtpssrcsstr;
    pPipArgs->params.mtu = pPipCfgArg->mtu ? pPipCfgArg->mtu : pCfgOverlay->cfgrtp.maxPayloadSz;

    pPipArgs->params.rtpPktzMode = pPipCfgArg->rtpPktzMode != -1 ? pPipCfgArg->rtpPktzMode :
                                 pCfgOverlay->cfgrtp.rtpPktzMode;
    pCfgPip->doAbrAuto = pPipArgs->params.abrAuto = pPipCfgArg->abrAuto;
    pCfgPip->abrSelfThrottle.active = pPipArgs->params.abrSelf = pPipCfgArg->abrSelf;
    pPipArgs->params.nackRtpRetransmitVideo = pPipCfgArg->nackRtpRetransmitVideo;
    pPipArgs->params.apprembRtcpSendVideoMaxRateBps = pPipCfgArg->apprembRtcpSendVideoMaxRateBps;
    pPipArgs->params.apprembRtcpSendVideoMinRateBps = pPipCfgArg->apprembRtcpSendVideoMinRateBps;
    pPipArgs->params.apprembRtcpSendVideoForceAdjustment = pPipCfgArg->apprembRtcpSendVideoForceAdjustment;


    //
    // STUN binding request properties
    //
    if(pPipCfgArg->stunReqUsername || pPipCfgArg->stunReqPass || pPipCfgArg->stunReqRealm) {
      pPipArgs->params.stunRequestorCfg.bindingRequest = pPipCfgArg->stunBindingRequest;
      pPipArgs->params.stunRequestorCfg.reqUsername = pPipCfgArg->stunReqUsername;
      pPipArgs->params.stunRequestorCfg.reqPass = pPipCfgArg->stunReqPass;
      pPipArgs->params.stunRequestorCfg.reqRealm = pPipCfgArg->stunReqRealm;
    }
/*
    //
    // TURN server properties
    //
    if(pPipCfgArg->turnCfg.turnServer && pPipCfgArg->turnCfg.turnServer[0] != '\0') {
      pPipArgs->params.turnCfg.turnServer = pPipCfgArg->turnCfg.turnServer;
      pPipArgs->params.turnCfg.turnUsername = pPipCfgArg->turnCfg.turnUsername;
      pPipArgs->params.turnCfg.turnPass = pPipCfgArg->turnCfg.turnPass;
      pPipArgs->params.turnCfg.turnRealm = pPipCfgArg->turnCfg.turnRealm;
    }
    //LOG(X_DEBUG("TURN... %s\n\n\n"), pPipArgs->params.turnCfg.turnServer);
*/

    //
    // Setup RTP payload type, SSRC, etc...
    //
    vsxlib_setup_rtpout(pCfgPip, &pPipArgs->params);

    if((rc = vsxlib_set_output(pCfgPip,
                                pPipArgs->params.outputs,
                                pPipArgs->params.rtcpPorts,
                                 "rtp",
                                NULL,
                                pPipArgs->params.srtpCfgs,
                                &pPipArgs->params.stunRequestorCfg,
                                !do_update ? &pPipArgs->params.turnCfg : NULL)) < 0) {

      //pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      LOG(X_ERROR("Invalid pip output configuration '%s' for %s"), pPipCfgArg->output, pPipArgs->tid_tag);
      return -1;
    }

  }

  //
  // PIP server tslive output
  //
  if(pPipCfgArg->tsliveaddr) {

    if(pip_validate_output_listener(pPipCfgArg, pCfgOverlay) < 0) {
      //pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      LOG(X_ERROR("Invalid pip tslive '%s' for %s"), pPipCfgArg->tsliveaddr, pPipArgs->tid_tag);
      return -1;
    }

    pPipArgs->params.tsliveaddr[0] = pPipCfgArg->tsliveaddr;
    pPipArgs->do_server = 1;

  }

  //
  // If a PIP output has been set assume we want to produce mixed audio output
  //
  //if(pCfgPip->xcode.aud.common.cfgDo_xcode && (pCfgOverlay->xcode.aud.pMixer || 
  if(!pPipCfgArg->noaud && (pCfgOverlay->xcode.aud.pMixer || 
    ((STREAM_XCODE_AUD_UDATA_T *) pCfgOverlay->xcode.aud.pUserData)->pStartMixer ||
     pPipCfgArg->output || pPipArgs->do_server)) {

    if(pCfgOverlay->xcode.vid.common.cfgFileTypeOut <= XC_CODEC_TYPE_UNKNOWN || 
       (!pCfgOverlay->noaud && pCfgOverlay->xcode.aud.common.cfgFileTypeOut <= XC_CODEC_TYPE_UNKNOWN)) {
      LOG(X_ERROR("Transcoding must be enabled for both audio and video to support PIP with stream output"));
      //pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      return -1;
    } else if(IS_XC_CODEC_TYPE_VID_RAW(pCfgPip->xcode.vid.common.cfgFileTypeOut)) {
      pCfgPip->xcode.vid.pip.doEncode = 0;
      pCfgPip->xcode.vid.common.cfgFileTypeOut = pCfgOverlay->xcode.vid.common.cfgFileTypeOut;
    } else if(pPipCfgArg->output || pPipArgs->do_server) {
      pCfgPip->xcode.vid.pip.doEncode = 1;
      //xcode_dump_conf_video(&pCfgPip->xcode.vid, 0, 0, S_DEBUG);
      LOG(X_DEBUG("%s specific encoding is enabled"), pPipArgs->tid_tag);
    } else {
      pCfgPip->xcode.vid.pip.doEncode = 0;
      LOG(X_WARNING("%s specific encoding is disabled because no PIP specific output is specified"),  pPipArgs->tid_tag);
    }

//TODO: check if we are doing audio only pip
//pCfgPip->novid=1;
//pCfgPip->xcode.vid.common.cfgDo_xcode = 0;
//fprintf(stderr, "VID X_:%d, novid:%d\n", pCfgPip->xcode.vid.common.cfgDo_xcode, pCfgPip->novid);

    if(!pCfgPip->novid &&
       IS_XC_CODEC_TYPE_VID_RAW(pCfgOverlay->xcode.vid.common.cfgFileTypeOut) && 
       (pPipCfgArg->output || pPipArgs->do_server) && !pCfgPip->xcode.vid.pip.doEncode) {
      LOG(X_ERROR("PIP cannot be added because main overlay encoder output is not enabled "
                  "and PIP specific output encoding is not enabled"));
      //pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      return -1;
    }

    if(pCfgPip->xcode.vid.pip.doEncode) {

      //
      // Legacy PIP scale dimensions are supposed to be specified by 'vx=, vy='
      // PIP output dimensions are supposed to be specified by 'vpipx=, vpipy='
      // For now, we flip the two as it's most likely what the user wants
      //
      if(pCfgPip->xcode.vid.out[0].cfgOutH > 0 && pCfgPip->xcode.vid.out[0].cfgOutPipH == 0) {
        pCfgPip->xcode.vid.out[0].cfgOutPipH = pCfgPip->xcode.vid.out[0].cfgOutH;
        pCfgPip->xcode.vid.out[0].cfgOutH = 0;
      }
      if(pCfgPip->xcode.vid.out[0].cfgOutV > 0 && pCfgPip->xcode.vid.out[0].cfgOutPipV == 0) {
        pCfgPip->xcode.vid.out[0].cfgOutPipV = pCfgPip->xcode.vid.out[0].cfgOutV;
        pCfgPip->xcode.vid.out[0].cfgOutV = 0;
      }

#if 0
      if(pCfgPip->xcode.vid.cfgOutClockHz == 0 || pCfgPip->xcode.vid.cfgOutFrameDeltaHz == 0 ||
        ((float)pCfgPip->xcode.vid.cfgOutClockHz /  pCfgPip->xcode.vid.cfgOutFrameDeltaHz) >
        ((float)pCfgOverlay->xcode.vid.cfgOutClockHz /  pCfgOverlay->xcode.vid.cfgOutFrameDeltaHz)) {
        pCfgPip->xcode.vid.cfgOutClockHz = pCfgOverlay->xcode.vid.cfgOutClockHz;
        pCfgPip->xcode.vid.cfgOutFrameDeltaHz = pCfgOverlay->xcode.vid.cfgOutFrameDeltaHz;
pCfgPip->xcode.vid.cfgOutClockHz = 5;
pCfgPip->xcode.vid.cfgOutFrameDeltaHz = 1;
pCfgPip->xcode.vid.common.cfgCFROutput = 0;
      }
xcode_dump_conf_video(&pCfgPip->xcode.vid, 0, 0, S_DEBUG);
#endif // 0

    }

    if(pCfgPip->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_UNKNOWN) {
      pCfgPip->xcode.aud.common.cfgFileTypeOut = pCfgOverlay->xcode.aud.common.cfgFileTypeOut;
    }

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
    pCfgPip->xcode.aud.common.cfgDo_xcode = 1;
    pCfgPip->xcode.aud.cfgForceOut = 1;
#else
    pCfgPip->noaud = 1;
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  } else {

    pCfgPip->xcode.vid.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWV_YUVA420P;
    pCfgPip->xcode.aud.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWA_PCM16LE;

    //pCfgPip->noaud = 1;
    //pCfgPip->xcode.aud.common.cfgDo_xcode = 0;
    //Keep audio xcode enabled since if receiving from SDP w/ aud+vid, turning off audio processing would have audio frame queue overwrititing
    pCfgPip->xcode.aud.common.cfgDo_xcode = 1;
    pCfgPip->xcode.aud.cfgForceOut = 1;

  }

  if(pCfgOverlay->novid || pCfgPip->novid) {
    pCfgPip->xcode.vid.common.cfgDo_xcode = 0;
  }

  if(pCfgOverlay->noaud || pCfgPip->noaud) {
    pCfgPip->xcode.aud.common.cfgDo_xcode = 0;
  }

  pCfgPip->action.do_stream = 1;
  //memcpy(&pCfgPip->cfgrtp, &pCfgOverlay->cfgrtp, sizeof(pCfgPip->cfgrtp));
  //pCfgPip->cfgrtp.ssrcs[0] = pCfgPip->cfgrtp.ssrcs[1] = 0;
  pCfgPip->cfgrtp.clockHzs[0] = 90000; // Use 90KHz RTP video output clock

  //fprintf(stderr, "PIP RTP PT:%d,%d\n", pCfgPip->cfgrtp.payloadTypesMin1[0], pCfgPip->cfgrtp.payloadTypesMin1[1]);

  //
  // Copy the initial setting from the startup params since the main overlay cfg setting may get overwritten
  // if the overlay is from file, not from capture.
  // The default value of -1, unless explicitly disabled from the command line, so we will enable by default
  // for PIP(s).
  //
  pCfgPip->cfgrtp.rtp_useCaptureSrcPort = pPipArgs->params.rtp_useCaptureSrcPort != 0 ? 1 : 0;

  pCfgPip->cfgstream.includeSeqHdrs = pCfgOverlay->cfgstream.includeSeqHdrs;
  //pCfgPip->loop = pCfgOverlay->loop;
  pCfgPip->verbosity = pCfgOverlay->verbosity;
  pCfgPip->maxPlRecurseLevel = 3;
  //pCfgPip->pRawOutCfg = &privData.rawOutCfg;
  //pCfgPip->proto = pCfgOverlay->proto;
  pCfgPip->pip.pCfgOverlay = (STREAMER_CFG_T *) pCfgOverlay;

  //memcpy(pCfgPip->status.avs, pCfgOverlay->status.avs, sizeof(pCfgPip->status.avs));
  //fprintf(stderr, "AV OFFSET %lld,%lld, overlay:%lld,%lld\n", pCfgPip->status.avs[0].offsetCur, pCfgPip->status.avs[1].offsetCur, pCfgOverlay->status.avs[0].offsetCur, pCfgOverlay->status.avs[1].offsetCur);
  //pCfgPip->streamflags |= VSX_STREAMFLAGS_PREFERAUDIO;

  // TODO: use same clock s overlay for eg. main overlay uses 25Hz for file input stream, pip may default to 90Khz
  //pCfgPip->cfgrtp.  pRtpMulti->init.clockRateHz


  if(pCfgPip->proto == STREAM_PROTO_INVALID) {
    pCfgPip->proto = pCfgOverlay->proto;
  }

  return rc;
}

int pip_update(const PIP_CFG_T *pPipCfgArg, STREAMER_CFG_T *pCfgOverlay, unsigned int pip_id) {
  int rc = 0;
  unsigned int idx;
  int idxPip;
  int sdpChanged = 0;
  char *input = NULL;
  char buf[VSX_MAX_PATH_LEN];
  START_PIP_ARGS_T pipArgs;
  SDP_DESCR_T sdp;
  STREAMER_CFG_T *pCfgPip = NULL;
  CAPTURE_LOCAL_DESCR_T *pCapCfg = NULL;
  CAPTURE_FILTER_T filters[CAPTURE_MAX_FILTERS];

  if(!pPipCfgArg || !pCfgOverlay) {
    return -1;
  }

  //
  // Update and start a PIP which was previously started with the 'delaystart' (delayed_output) parameter
  //

  pthread_mutex_lock(&pCfgOverlay->pip.mtx);

  if((idxPip = pip_getIndexById(pCfgOverlay,  pip_id, 0)) < 0 || idxPip >= MAX_PIPS) {
    LOG(X_ERROR("Invalid PIP id to update %d"), pip_id);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }

  pCfgPip = pCfgOverlay->pip.pCfgPips[idxPip];
  pCapCfg = &pCfgOverlay->pStorageBuf->capCfgPips[idxPip];
  memset(&pipArgs, 0, sizeof(pipArgs));
  snprintf(pipArgs.tid_tag, sizeof(pipArgs.tid_tag), "PIP-%d[%d]", pip_id, idxPip);

  if(pCfgPip->delayed_output == 0) {
    LOG(X_ERROR("%s update id does not have delaystart parameter set"), pipArgs.tid_tag);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }

  LOG(X_DEBUG("Updating delayed output for %s"), pipArgs.tid_tag);

  memset(filters, 0, sizeof(filters));
  memcpy(&pipArgs.params, pCfgOverlay->pStorageBuf->pParams, sizeof(pipArgs.params));


  //TODO: update input SDP crypto keys...

  //
  // Update input SDP crypto keys, ice config 
  //
  if(pPipCfgArg->input) {

    //
    // Check if input is actually an sdp file meant for live capture
    //
    input = avc_dequote(pPipCfgArg->input, buf, sizeof(buf));

    if(sdp_isvalidFilePath(input)) {
      if((rc = capture_filterFromSdp(input, filters,
                                     pipArgs.input, sizeof(pipArgs.input), &sdp,
                                     pCfgOverlay->novid || pPipCfgArg->novid, 
                                     pCfgOverlay->noaud || pPipCfgArg->noaud)) <= 0) {
        return -1;
      }

      vsxlib_set_sdp_from_capture(pCfgPip, &sdp);

      if(rc != pCapCfg->common.filt.numFilters) {
        sdpChanged = 1;
      }
      for(idx = 0; idx < rc; idx++) {

        //char buf[128]; base64_encode(filters[idx].srtp.kt.k.key, filters[idx].srtp.kt.k.lenKey, buf, sizeof(buf)); LOG(X_DEBUG("UPDATED SRTP KEY USED IS: %s"), buf);

        if(filters[idx].dstPort != pCapCfg->common.filt.filters[idx].dstPort ||
           filters[idx].dstPortRtcp != pCapCfg->common.filt.filters[idx].dstPortRtcp) {
          sdpChanged = 1;
          break;
        }
      }

      if(sdpChanged) {
        return -1;
      }
      //LOG(X_DEBUG("STUN POLICY BEFORE: %d, pass:%s, user:%s, realm:%s, transT:%d, mediaT:%d"), pCapCfg->common.filt.filters[0].stun.policy, pCapCfg->common.filt.filters[0].stun.integrity.pass, pCapCfg->common.filt.filters[0].stun.integrity.user, pCapCfg->common.filt.filters[0].stun.integrity.realm, pCapCfg->common.filt.filters[0].transType, pCapCfg->common.filt.filters[0].mediaType);
      for(idx = 0; idx < rc; idx++) {
        // TODO: this presently only updates SRTP crypto keys, but should resolve updating
        // rtpmap
        
        if(pCapCfg->common.filt.filters[idx].srtps[0].privData) { LOG(X_DEBUG("stream_pip cap srtp update [%d].lenKey:%d -> %d, 0x%x -> 0x%x"), idx, pCapCfg->common.filt.filters[idx].srtps[0].kt.k.lenKey, filters[idx].srtps[0].kt.k.lenKey, pCapCfg->common.filt.filters[idx].srtps[0].privData, filters[idx].srtps[0].privData); LOGHEX_DEBUG(pCapCfg->common.filt.filters[idx].srtps[0].kt.k.key, pCapCfg->common.filt.filters[idx].srtps[0].kt.k.lenKey); LOGHEX_DEBUG(filters[idx].srtps[0].kt.k.key, filters[idx].srtps[0].kt.k.lenKey); }

        if(pCapCfg->common.filt.filters[idx].srtps[0].privData || pCapCfg->common.filt.filters[idx].srtps[1].privData) { 
          LOG(X_WARNING("Unable to update SRTP key(s) for %s because SRTP context has already been created"), pipArgs.tid_tag);
        } else {
          memcpy(&pCapCfg->common.filt.filters[idx].srtps, &filters[idx].srtps, 
                 sizeof(pCapCfg->common.filt.filters[idx].srtps));
        }
        //TODO: shouldn't be changin transType
        pCapCfg->common.filt.filters[idx].transType = filters[idx].transType;
        pCapCfg->common.filt.filters[idx].mediaType = filters[idx].mediaType;
        memcpy(&pCapCfg->common.filt.filters[idx].u_seqhdrs, &filters[idx].u_seqhdrs, 
               sizeof(pCapCfg->common.filt.filters[idx].u_seqhdrs));
      }
      //LOG(X_DEBUG("STUN POLICY AFTER: %d, pass:%s, user:%s, realm:%s, transT:%d, mediaT:%d"), pCapCfg->common.filt.filters[0].stun.policy, pCapCfg->common.filt.filters[0].stun.integrity.pass, pCapCfg->common.filt.filters[0].stun.integrity.user, pCapCfg->common.filt.filters[0].stun.integrity.realm, pCapCfg->common.filt.filters[0].transType, pCapCfg->common.filt.filters[0].mediaType);

      input = pipArgs.input;
      pipArgs.params.islive = 1;
      pipArgs.params.strfilters[0] = pipArgs.params.strfilters[1] = NULL;

    }

    if(input != pipArgs.input) {
      strncpy(pipArgs.input, input, sizeof(pipArgs.input) - 1);
      input = pipArgs.input;
    }

  }

  //
  // Set the xcode pip configuration
  //
  if(pip_load_xcode(pPipCfgArg, pCfgOverlay, pCfgPip, NULL) < 0) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    LOG(X_ERROR("Error updating delayed output for %s"), pipArgs.tid_tag);
    return -1;
  }

  //
  // Set PIP output
  //
  if(pip_set_output(pPipCfgArg, pCfgOverlay, pCfgPip, &pipArgs, 1) < 0) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    LOG(X_ERROR("Error updating delayed output for %s"), pipArgs.tid_tag);
    return -1;
  }

  pthread_mutex_unlock(&pCfgOverlay->pip.mtx);

  //
  // Play pip start announcement
  //
  if(rc >= 0 && pPipCfgArg->startChimeFile && pCfgPip->delayed_output > 0) {
    LOG(X_DEBUG("Starting PIP start announcement '%s'"), pPipCfgArg->startChimeFile);
    pip_announcement(pPipCfgArg->startChimeFile, pCfgOverlay);
  }

  //
  // Start the delayed stream output thread by interrupting the delayed_output loop in capture_net.c
  //
  pCfgPip->delayed_output = -1;

  return pip_id;
}

int pip_start(const PIP_CFG_T *pPipCfgArg, STREAMER_CFG_T *pCfgOverlay, IXCODE_PIP_MOTION_T *pMotion) {

  int rc = 0;
  unsigned int idxPip, xidx;
  IXCODE_VIDEO_PIP_T *pPip = NULL;
  START_PIP_ARGS_T pipArgs;
  char buf[VSX_MAX_PATH_LEN];
  pthread_t ptd;
  pthread_attr_t attr;
  char *input = NULL;
  const char *p;
  int numActivePips = 0;
  int sendonly = 0;
  SDP_DESCR_T sdp;
  CAPTURE_LOCAL_DESCR_T *pCapCfg = NULL;
  STREAMER_CFG_T *pCfgPip = NULL;

  if(!pPipCfgArg || !pCfgOverlay || !pPipCfgArg->input) {
    return -1;
  }

#if defined(VSX_HAVE_LICENSE)
  if((pCfgOverlay->lic.capabilities & LIC_CAP_NO_PIP)) {
    LOG(X_ERROR("PIP not enabled in license"));
    return -1;
  }
#endif // VSX_HAVE_LICENSE

#if defined(XCODE_IPC)
  LOG(X_ERROR("PIP insertion is not supported in your software version")); 
  return -1;
#endif // XCODE_IPC

  if(xcode_enabled(1) < XCODE_CFG_OK) {
    return -1;
  }

  memset(&pipArgs, 0, sizeof(pipArgs));
  memcpy(&pipArgs.params, pCfgOverlay->pStorageBuf->pParams, sizeof(pipArgs.params));
  pipArgs.params.islive = 0;
  pipArgs.params.strfilters[0] = pipArgs.params.strfilters[1] = NULL;
  input = avc_dequote(pPipCfgArg->input, buf, sizeof(buf));

  pthread_mutex_lock(&pCfgOverlay->pip.mtx);

  //
  // Get an available PIP instance index
  //
  idxPip = MAX_PIPS;
  for(xidx = 0; xidx < MAX_PIPS; xidx++) {
    if(pCfgOverlay->pip.pCfgPips[xidx]) {

      //LOG(X_DEBUG("ALLOW DUP:%d [xidx:%d].%s ... IS_PIP_FINISHED:%d, %s"), pPipCfgArg->allowDuplicate, xidx, pCfgOverlay->pip.pCfgPips[xidx]->pip.inputPathOrig, IS_PIP_FINISHED(pCfgOverlay->pip.pCfgPips[xidx]->running), pPipCfgArg->input);
      if(IS_PIP_FINISHED(pCfgOverlay->pip.pCfgPips[xidx]->running)) {

        if(idxPip == MAX_PIPS) {
          idxPip = xidx;
        }

      //
      // Check for existing PIP input path duplicate
      //
      } else if(!pPipCfgArg->allowDuplicate && !strcmp(pCfgOverlay->pip.pCfgPips[xidx]->pip.inputPathOrig, pPipCfgArg->input)) {
        LOG(X_ERROR("PIP for input path %s already exists"), pPipCfgArg->input);
        idxPip = MAX_PIPS;
        break;
      } else {
        numActivePips++;
      }
    }
  }

  if(idxPip >= MAX_PIPS) {
    LOG(X_ERROR("No available PIP instance (numActive:%d, max:%d)"), numActivePips, MAX_PIPS);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }

  if((pCfgOverlay->pip.pipMax >= 0 && numActivePips >= pCfgOverlay->pip.pipMax)
#if defined(VSX_HAVE_LICENSE)
    || (pCfgOverlay->lic.maxPips > 0 && numActivePips >= pCfgOverlay->lic.maxPips)
#endif // (VSX_HAVE_LICENSE)
    ) {
    LOG(X_ERROR("PIPs are limited at maximum of %d"), numActivePips);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }

  pCfgPip = pCfgOverlay->pip.pCfgPips[idxPip];
  pCapCfg = &pCfgOverlay->pStorageBuf->capCfgPips[idxPip];
  vsxlib_reset_streamercfg(pCfgPip); 
  pCfgPip->running = STREAMER_STATE_FINISHED_PIP;
  strncpy(pCfgPip->pip.inputPathOrig, pPipCfgArg->input, sizeof(pCfgPip->pip.inputPathOrig));
  snprintf(pipArgs.tid_tag, sizeof(pipArgs.tid_tag), "PIP-%d[%d]", s_pipId, idxPip);
  pCfgPip->delayed_output = pPipCfgArg->delayed_output;
  LOG(X_DEBUG("Starting %s"), pipArgs.tid_tag);

  //
  // Retain the license info
  //
  memcpy((void *) &pCfgPip->lic, &pCfgOverlay->lic, sizeof(pCfgPip->lic));

  //
  // Retain the RTCP FB config of the main overlay config
  //
  memcpy(&pCfgPip->fbReq.firCfg, &pCfgOverlay->fbReq.firCfg, sizeof(pCfgPip->fbReq.firCfg));
  pCfgPip->fbReq.nackRtpRetransmit = pPipCfgArg->nackRtpRetransmitVideo;
  pCfgPip->fbReq.nackRtcpSend = pipArgs.params.nackRtcpSendVideo;
  pCfgPip->fbReq.apprembRtcpSend = pipArgs.params.apprembRtcpSendVideo;
  pCfgPip->fbReq.apprembRtcpSendMaxRateBps = pipArgs.params.apprembRtcpSendVideoMaxRateBps;
  pCfgPip->fbReq.apprembRtcpSendMinRateBps = pipArgs.params.apprembRtcpSendVideoMinRateBps;
  pCfgPip->fbReq.apprembRtcpSendForceAdjustment = pipArgs.params.apprembRtcpSendVideoForceAdjustment;

  //
  // Check if input is actually an sdp file meant for live capture
  //
  if(sdp_isvalidFilePath(input)) {
    if((rc = capture_filterFromSdp(input, pCapCfg->common.filt.filters, 
                                   pipArgs.input, sizeof(pipArgs.input), &sdp,
                                   pCfgOverlay->novid || pPipCfgArg->novid,
                                   pCfgOverlay->noaud || pPipCfgArg->noaud)) <= 0) {
      pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      return -1;
    }

    vsxlib_set_sdp_from_capture(pCfgPip, &sdp);

    pCapCfg->common.filt.numFilters = rc;
    input = pipArgs.input;
    pipArgs.params.islive = 1;
    pipArgs.params.strfilters[0] = pipArgs.params.strfilters[1] = NULL;

    //TODO: this needs better logic not to auto-set every time
    // If video RTCP NACK is on then increase video jitter buffer and disable a/v sync
    pCfgPip->avReaderType = VSX_STREAM_READER_TYPE_NOSYNC;

    // set vfr if input comes from SDP

  }

  if(input != pipArgs.input) {
    strncpy(pipArgs.input, input, sizeof(pipArgs.input) - 1);
    input = pipArgs.input;
  }

  if(!pipArgs.params.islive) {
    p = input;
    if(capture_parseTransportStr(&p) != CAPTURE_FILTER_TRANSPORT_UNKNOWN) {
      pipArgs.params.islive = 1;
    }
  }

  memset(&pipArgs.params.stunResponderCfg, 0, sizeof(pipArgs.params.stunResponderCfg));

  if(pipArgs.params.islive) {

    pipArgs.params.stunResponderCfg.bindingResponse = pPipCfgArg->stunBindingResponse;
    pipArgs.params.stunResponderCfg.respUseSDPIceufrag = pPipCfgArg->stunRespUseSDPIceufrag;
    pipArgs.params.extractpes = BOOL_ENABLED_DFLT;
    pipArgs.params.connectretrycntminone = 1;
    pCapCfg->common.localAddrs[0] = input;
    pCapCfg->common.localAddrs[1] = NULL;

    //
    // TURN server properties
    //
    if(pPipCfgArg->turnCfg.turnServer && pPipCfgArg->turnCfg.turnServer[0] != '\0') {
      pipArgs.params.turnCfg.turnServer = pPipCfgArg->turnCfg.turnServer;
      pipArgs.params.turnCfg.turnUsername = pPipCfgArg->turnCfg.turnUsername;
      pipArgs.params.turnCfg.turnPass = pPipCfgArg->turnCfg.turnPass;
      pipArgs.params.turnCfg.turnRealm = pPipCfgArg->turnCfg.turnRealm;
      //pipArgs.params.turnCfg.turnOutput = pPipCfgArg->turnCfg.turnOutput;
      pipArgs.params.turnCfg.turnSdpOutPath = pPipCfgArg->turnCfg.turnSdpOutPath;
      pipArgs.params.turnCfg.turnPeerRelay = pPipCfgArg->turnCfg.turnPeerRelay;
    }

    //pipArgs.params.srtpCfgs[0].dtls_capture_serverkey = pPipCfgArg->dtls_capture_serverkey;
    pipArgs.params.srtpCfgs[0].dtls_handshake_server = pPipCfgArg->dtls_handshake_server;

    if(vsxlib_stream_setupcap(&pipArgs.params, pCapCfg, pCfgPip, NULL) < 0) {
      pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      return -1;    
    }

    //
    // Check if the SDP is sendonly
    //
    if((pCapCfg->common.filt.numFilters > 0 &&
        pCapCfg->common.filt.filters[0].xmitType == SDP_XMIT_TYPE_SENDONLY) ||
       (pCapCfg->common.filt.numFilters > 1 &&
        pCapCfg->common.filt.filters[1].xmitType == SDP_XMIT_TYPE_SENDONLY)) {
      LOG(X_DEBUG("%s SDP is sendonly"), pipArgs.tid_tag);
      sendonly = 1;
    }
  } else if(pCfgPip->delayed_output) {
    LOG(X_ERROR("Delayed stream output %s is only permitted for live capture"), pipArgs.tid_tag);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;    
  }

  //
  // Check if audio or video is disabled in the PIP config
  //
  if(pPipCfgArg->novid) {
    pCfgPip->novid = 1;
  }
  if(pPipCfgArg->noaud) {
    pCfgPip->noaud = 1;
  }

  //
  // Check if audio or video is disabled in the main overlay 
  //
  if(!pCfgPip->novid) {
    pCfgPip->novid = pCfgOverlay->novid;
  }
  if(!pCfgPip->noaud) {
    pCfgPip->noaud = pCfgOverlay->noaud;
  }

  if(pCfgPip->novid) {
    if(pCfgPip->noaud) {
      LOG(X_ERROR("PIP audio and video cannot both be disabled."));
      pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      return -1;
    } else if(!pCfgOverlay->xcode.aud.pMixer && !((STREAM_XCODE_AUD_UDATA_T *) pCfgOverlay->xcode.aud.pUserData)->pStartMixer) {
      LOG(X_ERROR("PIP audio only processing requires a mixer instance."));
      pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
      return -1;
    }
  }

  //
  // Set the xcode pip configuration
  //
  if(pip_load_xcode(pPipCfgArg, pCfgOverlay, pCfgPip, &pipArgs.params) < 0) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }
  
//pCfgPip->xcode.

  //pCfgPip->xcode.vid.out[0].cfgOutH=120; pCfgPip->xcode.vid.out[0].cfgOutV=180;
  //fprintf(stderr, "PIP RES %dx%d\n", pCfgPip->xcode.vid.out[0].cfgOutH, pCfgPip->xcode.vid.out[0].cfgOutV);

  //fprintf(stderr, "PIP AFTER XCODE_PARSE_CONFIGSTR PIP AUDIO XCODE:%d, VIDEO XCODE:%d, strxcode:'%s'\n", pCfgPip->xcode.aud.common.cfgDo_xcode, pCfgPip->xcode.vid.common.cfgDo_xcode, strxcode);


  //
  // Set PIP output
  //
  if(pip_set_output(pPipCfgArg, pCfgOverlay, pCfgPip, &pipArgs, 0) < 0) {
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }
  //LOG(X_DEBUG("RC:%d, NOAUD:%d, cfgFileTypeOut:%d, AUD_RAW:%d"), rc, pCfgPip->noaud, pCfgPip->xcode.aud.common.cfgFileTypeOut, IS_XC_CODEC_TYPE_AUD_RAW(pCfgPip->xcode.aud.common.cfgFileTypeOut));

  pCfgPip->running = STREAMER_STATE_FINISHED_PIP;

  pthread_mutex_lock(&pCfgOverlay->xcode.vid.overlay.mtx);

  if((rc = pip_add_zordered(pCfgOverlay->xcode.vid.overlay.p_pipsx, pPipCfgArg->zorder)) >= 0) {

    xidx = rc;
    pCfgOverlay->xcode.vid.overlay.p_pipsx[xidx] = &pCfgPip->xcode.vid;
    pPip = &pCfgOverlay->xcode.vid.overlay.p_pipsx[xidx]->pip;
    pPip->active = 1; 

    //
    // Set PIP visible setting.  -1 indicates audio-only PIP.  0 cn be set to temporarily disable video such as call hold.
    //
    if(pCfgPip->novid) {
      pPip->visible = -1;
    } else if(sendonly) {
      //if(pPip->visible > 0) {
        pPip->visible = 0;
      //}
    //} else if(pPip->visible == 0) {
    } else {
      pPip->visible = 1;
    }

    pPip->indexPlus1 = idxPip + 1;
    pPip->id = s_pipId++;
    //pPip->pOverlay = &pCfgOverlay->xcode.vid.overlay;
    pPip->pXOverlay = &pCfgOverlay->xcode.vid;

    if(pPipCfgArg->layoutType == PIP_LAYOUT_TYPE_MANUAL ||
       pCfgOverlay->pip.layoutType == PIP_LAYOUT_TYPE_MANUAL) {

      pPip->zorder = pPipCfgArg->zorder;
      pPip->pos[0].posx = pPipCfgArg->posx;
      pPip->pos[0].posy = pPipCfgArg->posy;
      if(pPipCfgArg->layoutType != PIP_LAYOUT_TYPE_MANUAL) {
        pPip->autoLayout = 1;
      } else {
        pPip->autoLayout = 0;
      }
      //
      // Start showing the PIP (grid section) immediately at pip startup and prior
      // to receiving an I-frame
      //
      pPip->showVisual = 1;  

    } else {

      //
      // zorder will be auto-assigned by the layout mgr
      //
      pPip->zorder = 0;
      pPip->pos[0].posx = 0;
      pPip->pos[0].posy = 0;
      pPip->autoLayout = 1;
      //
      // showVisual will be set when pip_layout_update PIP_UPDATE_PARTICIPANT_HAVEINPUT 
      // is sent from xcode input processor
      //
      pPip->showVisual = 0; 

      //make_border(&pCfgPip->xcode.vid.out[0].crop);
    }

    //
    // If the output resolution is not given, resOutH will not be available until after the resolution is
    // detected from the input video stream
    //
    pPip->pos[0].pwidth = &pCfgPip->xcode.vid.out[0].resOutH;
    pPip->pos[0].pheight = &pCfgPip->xcode.vid.out[0].resOutV;
    pPip->pwidthCfg = &pCfgPip->xcode.vid.out[0].cfgOutH;
    pPip->pheightCfg = &pCfgPip->xcode.vid.out[0].cfgOutV;
    pPip->pwidthInput =  &((STREAM_XCODE_VID_UDATA_T *) pCfgPip->xcode.vid.pUserData)->seqHdrWidth;
    pPip->pheightInput =  &((STREAM_XCODE_VID_UDATA_T *) pCfgPip->xcode.vid.pUserData)->seqHdrHeight;
    pPip->flags = pPipCfgArg->flags | PIP_FLAGS_CLOSE_ON_END; 

    if(pPipCfgArg->alphamin_min1 > 256) {
      pPip->alphamin_min1 = 256;
    } else if(pPipCfgArg->alphamin_min1 < 0) {
      pPip->alphamin_min1 = 1;
    } else {
      pPip->alphamin_min1 = pPipCfgArg->alphamin_min1;
    }
    if(pPipCfgArg->alphamax_min1 > 256) {
      pPip->alphamax_min1 = 256;
    } else if(pPipCfgArg->alphamax_min1 < 0) {
      pPip->alphamax_min1 = 1;
    } else {
      pPip->alphamax_min1 = pPipCfgArg->alphamax_min1;
    }

    pPip->pMotion = pMotion;

    if(pCfgPip->xcode.aud.common.cfgDo_xcode && 
       (pPipCfgArg->audioChime || !IS_XC_CODEC_TYPE_AUD_RAW(pCfgPip->xcode.aud.common.cfgFileTypeOut))) {

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

#if defined(VSX_HAVE_LICENSE)

//  fprintf(fp, "Max Conference Participants: %d\n",
//      (htons(*((uint16_t *) pLic->u.hdr.custom)) & LIC_DATA_MAX_CONF_PARTICIPANTS_MASK));

      if((pCfgOverlay->lic.capabilities & LIC_CAP_NO_PIPAUDIO)) {
        LOG(X_ERROR("PIP not enabled in license"));
        rc = -1;
      } else if(pCfgOverlay->lic.maxPips > 0 && numActivePips >= pCfgOverlay->lic.maxPips) {
        LOG(X_ERROR("PIP license limit of %d reached"), numActivePips);
        rc = -1;
      }


#endif // VSX_HAVE_LICENSE

      if(rc >= 0) {
        LOG(X_DEBUGV("Will process %s audio"), pipArgs.tid_tag);
      }

      //
      // We will get here if we depend on having the mixer pre-created and '--mixer' was not used to start us 
      //
      if(rc >= 0 && !pCfgOverlay->xcode.aud.pMixer) {
	if(pPipCfgArg->output || pipArgs.do_server) {
	  LOG(X_ERROR("%s output '%s' cannot be created because no audio mixer has been created"), pipArgs.tid_tag, pPipCfgArg->output);
	  rc = -1;
	} else {
	  LOG(X_WARNING("%s output is disabled because no audio mixer has been created"), pipArgs.tid_tag, pPipCfgArg->output);
	  pCfgPip->noaud = 1;
	  pCfgPip->xcode.aud.common.cfgDo_xcode = 0;
	  pCfgPip->xcode.aud.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWA_PCM16LE;
	  pCfgPip->xcode.vid.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWV_YUVA420P;
	  pCfgPip->action.do_output = 0;
	  pCfgPip->numDests = 0;
          pipArgs.do_server = 0;
	}
      }
/*
      //TODO: we're not creating an on-the-fly mixer anymore until we fix seamless xcoder audio transition from non-mixed audio to mixed 
      if(pip_create_mixer(pCfgOverlay) < 0) {
	pPip = pCfgOverlay->xcode.vid.overlay.p_pips[xidx] = NULL;
	break;
      }
*/

      if(rc >= 0 && !pCfgPip->noaud) {
	PARTICIPANT_CONFIG_T participantCfg;
	AUDIO_CONFIG_T audioCfg;

	memset(&participantCfg, 0, sizeof(participantCfg));
	memset(&audioCfg, 0, sizeof(audioCfg));
	participantCfg.id = idxPip + 1;
	participantCfg.input.type = PARTICIPANT_TYPE_CB_PCM;
	participantCfg.output.type = PARTICIPANT_TYPE_PCM_POLL;
	//audioCfg.decodedWavPath = "decoded.wav";
	//audioCfg.mixedWavPath = "mixed.wav";
	//participantCfg.pAudioConfig = &audioCfg;

	if(!(pCfgPip->xcode.aud.pMixerParticipant = participant_add(pCfgOverlay->xcode.aud.pMixer, &participantCfg))) {
	    LOG(X_ERROR("%s Failed to create audio mixer participant[%d]"), pipArgs.tid_tag, xidx);
	  rc = -1;
	} else {

  	  pCfgPip->xcode.aud.pXcodeV = &pCfgPip->xcode.vid;
  	  pCfgPip->xcode.vid.pip.pXcodeA = &pCfgPip->xcode.aud;
          //TODO: need to support PIP output rate, other than mixer output rate
          if(pCfgPip->xcode.aud.cfgSampleRateOut == 0) {
            pCfgPip->xcode.aud.cfgSampleRateOut = pCfgOverlay->xcode.aud.cfgSampleRateOut;
          }
          pCfgPip->xcode.aud.cfgChannelsOut = 1;
          pCfgPip->xcode.aud.cfgBitRateOut = pCfgOverlay->xcode.aud.cfgBitRateOut;
          pCfgPip->cfgrtp.clockHzs[1] = pCfgPip->xcode.aud.cfgSampleRateOut;

          //xcode_dump_conf_audio(&pCfgPip->xcode.aud, 0, 0, S_DEBUG);
        }

      }

#else
      
      LOG(X_ERROR("%s stream output requires audio mixer to be enabled"), pipArgs.tid_ta);
      rc = -1;

#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

    } else {
      LOG(X_DEBUGV("%s Not processing audio"), pipArgs.tid_tag);
    }
  }

  //fprintf(stderr, "PIP_ADD AFTER rc:%d, xidx:%d\n", rc, xidx); pip_dump_order(pCfgOverlay->xcode.vid.overlay.p_pips);

  pthread_mutex_unlock(&pCfgOverlay->xcode.vid.overlay.mtx);

  if(rc < 0 || !pPip) {
    if(pPip) {
      pip_remove(&pCfgOverlay->xcode.vid.overlay, pPip->indexPlus1);
    }
    LOG(X_ERROR("Unable to create %s"), pipArgs.tid_tag);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }

  //fprintf(stderr, "DELETEME pCfgPip:0x%x, pCFgOverlay:0x%x, resOutH[0]:%d, resOutH[1]:%d\n", &pCfgPip->xcode.vid, &pCfgOverlay->xcode.vid, pCfgPip->xcode.vid.out[0].resOutH, pCfgPip->xcode.vid.out[1].resOutH);


  //pthread_mutex_init(&pCfgPip->pip.mtxCond, NULL);
  //pthread_cond_init(&pCfgPip->pip.cond, NULL);

  PHTREAD_INIT_ATTR(&attr);
  pipArgs.idxPip = idxPip;
  pipArgs.pip_id = pPip->id;
  pipArgs.pipXcodeIdx = xidx;
  pipArgs.fps = pPipCfgArg->fps;
  pipArgs.pCapCfg = (pipArgs.params.islive ? pCapCfg : NULL);
  pipArgs.pCfgOverlay = pCfgOverlay;
  pipArgs.pStreamerCfg = pCfgPip;
  pipArgs.pStreamerCfg->running = STREAMER_STATE_STARTING_THREAD;
  if(pPipCfgArg->stopChimeFile) {
    strncpy(pipArgs.stopChimeFile, pPipCfgArg->stopChimeFile, sizeof(pipArgs.stopChimeFile) - 1);
  }
  //s_pipIdx++;

  //
  // Create asynchronous decoder of PIP video / image
  //
  if(pthread_create(&ptd, &attr,
                  (void *) start_pip_proc,
                  (void *) &pipArgs) != 0) {

    LOG(X_DEBUG("Failed to create %s (xcode idx:%d) thread"), pipArgs.tid_tag, idxPip, xidx);
    pipArgs.pStreamerCfg->running = STREAMER_STATE_ERROR;
    pip_remove(&pCfgOverlay->xcode.vid.overlay, pPip->indexPlus1);
    pthread_mutex_unlock(&pCfgOverlay->pip.mtx);
    return -1;
  }

  while(pipArgs.pStreamerCfg->running == STREAMER_STATE_STARTING_THREAD) {
    usleep(5000);
  }

  usleep(100000);
  if(pipArgs.pStreamerCfg->running == STREAMER_STATE_ERROR) {
    LOG(X_DEBUG("Failed to start %s (xcode idx:%d)"), pipArgs.tid_tag, xidx);
    pip_remove(&pCfgOverlay->xcode.vid.overlay, pPip->indexPlus1);
    rc = -1;
  }

  if(rc >= 0) {
    //rc = idxPip;
    rc = pPip->id;
    pCfgOverlay->pip.numPipAddedTot++;
    pCfgOverlay->xcode.vid.overlay.havePip = 1;

  }

  pthread_mutex_unlock(&pCfgOverlay->pip.mtx);

  //
  // Play pip start announcement
  //
  if(rc >= 0 && pPipCfgArg->startChimeFile && pCfgPip->delayed_output == 0) {
    LOG(X_DEBUG("Starting PIP start announcement '%s'"), pPipCfgArg->startChimeFile); 
    pip_announcement(pPipCfgArg->startChimeFile, pCfgOverlay);
  }

  return rc;
}

int pip_create_mixer(STREAMER_CFG_T *pStreamerCfg, int includeMainSrc) {
  int rc = 0;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  PARTICIPANT_CONFIG_T participantCfg;
  MIXER_CONFIG_T mixerCfg;
  AUDIO_CONFIG_T audioCfg;

  if(pStreamerCfg->noaud) {
    LOG(X_ERROR("Audio mixer requires audio processing to be enabled"));
    return -1;
  } else if(!pStreamerCfg->xcode.aud.common.cfgDo_xcode) {
    LOG(X_ERROR("Audio mixer requires audio transcoding to be enabled"));
    LOG(X_ERROR(VSX_XCODE_DISABLED_BANNER));
    return -1;
  } else if(pStreamerCfg->xcode.aud.cfgChannelsOut > 1) {
    LOG(X_ERROR("Audio mixer does not support more than one output channel"));
    return -1;
  } else if(pStreamerCfg->xcode.aud.cfgSampleRateOut <= 0) {
    LOG(X_ERROR("Audio mixer encoding sample rate not set"));
    return -1; 
  } else if(pStreamerCfg->xcode.aud.pMixer || pStreamerCfg->xcode.aud.pMixerParticipant) {
    return -1;
  }

  memset(&mixerCfg, 0, sizeof(mixerCfg));
  mixerCfg.verbosity = pStreamerCfg->xcode.vid.common.cfgVerbosity;
  mixerCfg.pLogCtxt = logger_getContext();
  mixerCfg.sampleRate = pStreamerCfg->xcode.aud.cfgSampleRateOut;
  if(mixerCfg.sampleRate <= 8000) {
    mixerCfg.mixerLateThresholdMs = 100;
  } else if(mixerCfg.sampleRate <= 16000) {
    mixerCfg.mixerLateThresholdMs = 75;
  } else {
    mixerCfg.mixerLateThresholdMs = 40;
  }
  mixerCfg.ac.includeSelfChannel = pStreamerCfg->pip.pMixerCfg->includeSelfChannel;
//mixerCfg.ac.includeSelfChannel = 1;
  mixerCfg.highPriority = pStreamerCfg->pip.pMixerCfg->highPriority;
  // The mixer vad value is the mixer vad (webrtc) mode -1 (1: vad mode 0, 4: vad mode 3)
  mixerCfg.ac.mixerVad = pStreamerCfg->pip.pMixerCfg->vad; 
  mixerCfg.ac.mixerAgc = pStreamerCfg->pip.pMixerCfg->agc;
  mixerCfg.ac.mixerDenoise = pStreamerCfg->pip.pMixerCfg->denoise;

  if(!(pStreamerCfg->xcode.aud.pMixer = conference_create(&mixerCfg, 1))) {
    LOG(X_ERROR("Failed to create audio mixer instance"));
    return -1;
  }

  if(includeMainSrc && !IS_XC_CODEC_TYPE_AUD_RAW(pStreamerCfg->xcode.aud.common.cfgFileTypeOut)) {
  //if(includeMainSrc) {

    memset(&participantCfg, 0, sizeof(participantCfg));
    memset(&audioCfg, 0, sizeof(audioCfg));
    participantCfg.id = 0;
    participantCfg.input.type = PARTICIPANT_TYPE_CB_PCM;
    participantCfg.output.type = PARTICIPANT_TYPE_PCM_POLL;
    //audioCfg.decodedWavPath = "decoded.wav";
    //audioCfg.mixedWavPath = "mixed.wav";
    //participantCfg.pAudioConfig = &audioCfg;

    if(!(pStreamerCfg->xcode.aud.pMixerParticipant = participant_add(pStreamerCfg->xcode.aud.pMixer, &participantCfg))) {
      LOG(X_ERROR("Failed to create audio mixer participant"));
      conference_destroy(pStreamerCfg->xcode.aud.pMixer);
      pStreamerCfg->xcode.aud.pMixer = NULL;
    }
    LOG(X_DEBUG("Created mixer main participant"));
  }

#else // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  rc = -1;
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  return rc;
}

int pip_check_idle(STREAMER_CFG_T *pCfgOverlay) {
  int rc = 0;
  TIME_VAL tv;

  if(!pCfgOverlay || !pCfgOverlay->pip.pMixerCfg || !pCfgOverlay->pip.pMixerCfg->conferenceInputDriver) {
    return 0;
  }

  tv = timer_GetTime();

  pthread_mutex_lock(&pCfgOverlay->pip.mtx);

  if((pCfgOverlay->pip.numPipAddedTot == 0 && pCfgOverlay->pip.pMixerCfg->pip_idletmt_1stpip_ms > 0 &&
     ((tv - pCfgOverlay->pip.tvLastPipAction) / TIME_VAL_MS > pCfgOverlay->pip.pMixerCfg->pip_idletmt_1stpip_ms)) ||
     (pCfgOverlay->pip.numPipAddedTot > 0 && !pCfgOverlay->xcode.vid.overlay.havePip &&
      pCfgOverlay->pip.pMixerCfg->pip_idletmt_ms > 0 &&
      ((tv - pCfgOverlay->pip.tvLastPipAction) / TIME_VAL_MS > pCfgOverlay->pip.pMixerCfg->pip_idletmt_ms))) {
    LOG(X_WARNING("Reached conference participant idle timeout  %u ms"), pCfgOverlay->pip.numPipAddedTot == 0 ? 
       pCfgOverlay->pip.pMixerCfg->pip_idletmt_1stpip_ms : pCfgOverlay->pip.pMixerCfg->pip_idletmt_ms);
    rc = 1;
  }

  pthread_mutex_unlock(&pCfgOverlay->pip.mtx);

  if(rc > 0) {
    pCfgOverlay->running = STREAMER_STATE_ERROR;
    g_proc_exit = 1;
  }

  return rc;
}

int pip_announcement(const char *file, STREAMER_CFG_T *pCfgOverlay) {
  int rc = 0;
  PIP_CFG_T pipCfg;

  if(!file || !pCfgOverlay) {
    return -1;
  }

  //
  // Audio announcement
  //
  if(!(pCfgOverlay->xcode.aud.pMixer ||
     ((STREAM_XCODE_AUD_UDATA_T *) pCfgOverlay->xcode.aud.pUserData)->pStartMixer)) {
    return -1;
  }

  memset(&pipCfg, 0, sizeof(pipCfg));
  pipCfg.input = file;
  pipCfg.novid = 1;
  pipCfg.layoutType = PIP_LAYOUT_TYPE_MANUAL;
  pipCfg.allowDuplicate = 1;
  pipCfg.audioChime = 1;

  rc = pip_start(&pipCfg, pCfgOverlay, NULL);

  return rc;
}

#if !defined(ENABLE_XCODE) || !defined(XCODE_HAVE_PIP_AUDIO) || (XCODE_HAVE_PIP_AUDIO == 0)

struct AUDIO_CONFERENCE *conference_create(const MIXER_CONFIG_T *pCfg, CONFERENCE_ID id) {
  return NULL;
}

void conference_destroy(struct AUDIO_CONFERENCE *pConference) {

}

struct PARTICIPANT *participant_add(struct AUDIO_CONFERENCE *pConference, const struct PARTICIPANT_CONFIG *pConfig) {
  return NULL;
}

int participant_remove(struct PARTICIPANT *pParticipant) {
  return -1;
}

#endif // defined(ENABLE_XCODE) || !defined(XCODE_HAVE_PIP_AUDIO) || (XCODE_HAVE_PIP_AUDIO == 0)


#endif //  (VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP)

