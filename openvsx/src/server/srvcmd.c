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
#include "vsxlib_int.h"


typedef int (*SRV_CMD_HANDLER)(CLIENT_CONN_T *, const KEYVAL_PAIR_T *,
                               unsigned char **, unsigned int *);

static int srv_cmd_handler_playfile(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                    unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_playjumpfile(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                        unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_skipfile(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                    unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_playlive(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                    unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_playsetprop(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                    unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_recordstart(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                       unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_recordpause(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                       unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_recordstop(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                       unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_stop(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_pause(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *,
                                unsigned char **ppout, unsigned int *plenout);
static int srv_cmd_handler_status(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *, 
                                  unsigned char **ppout, unsigned int *plenout);


struct SRV_CMD_HANDLER_LIST {
  enum SRV_CMD               cmd;
  SRV_CMD_HANDLER            cbOnCmd;
  char                      *strCmd;
};

struct SRV_CMD_HANDLER_LIST srv_cmd_handlers[] = {
                          { SRV_CMD_PLAY_FILE, srv_cmd_handler_playfile, "playf" },
                          { SRV_CMD_PLAY_JUMPFILE, srv_cmd_handler_playjumpfile, "playj" },
                          { SRV_CMD_SKIP_FILE, srv_cmd_handler_skipfile, "skip" },
                          { SRV_CMD_PLAY_LIVE, srv_cmd_handler_playlive, "playl" },
                          { SRV_CMD_PLAY_SETPROP, srv_cmd_handler_playsetprop, "playp" },
                          { SRV_CMD_RECORD, srv_cmd_handler_recordstart, "record" },
                          { SRV_CMD_RECORD_STOP, srv_cmd_handler_recordstop, "rstop" },
                          { SRV_CMD_RECORD_PAUSE, srv_cmd_handler_recordpause, "rpause" },
                          { SRV_CMD_PLAY_STOP, srv_cmd_handler_stop, "stop" },
                          { SRV_CMD_PAUSE, srv_cmd_handler_pause, "pause" },
                          { SRV_CMD_STATUS, srv_cmd_handler_status, "status" }
                          };


static void copy_srvctrl(SRV_CTRL_NEXT_T *pDst, const SRV_CTRL_NEXT_T *pSrc) {

  unsigned int frvidqslots = pDst->captureCfg.capActions[0].frvidqslots;
  unsigned int fraudqslots = pDst->captureCfg.capActions[0].fraudqslots;

  //
  // Do not copy the entire contents of nextCfg into curCfg
  // to preserve non-dynamic elements, such as mtx
  //
  // Note:  This is a shallow copy, so any internal pointer
  // references will not need to refer to curCfg.
  //
  memcpy(pDst, pSrc, ((char *)&pDst->streamerCfg.status.useStatus) - ((char *)pDst));

  memcpy(pDst->streamerCfg.pdestsCfg, pSrc->streamerCfg.pdestsCfg,
         *pDst->streamerCfg.pmaxRtp * sizeof(STREAMER_DEST_CFG_T));
  if(pDst->streamerCfg.pxmitDestRc && pSrc->streamerCfg.pxmitDestRc) {
    memcpy(pDst->streamerCfg.pxmitDestRc, pSrc->streamerCfg.pxmitDestRc,
         *pDst->streamerCfg.pmaxRtp * sizeof(int));
  }

  // preserve configured frame queue slot dimensions
  pDst->captureCfg.capActions[0].frvidqslots = frvidqslots;
  pDst->captureCfg.capActions[0].fraudqslots = fraudqslots;

}

static int close_recording(SRV_CTRL_T *pSrvCtrl) {
  struct stat st;
  char filenamenew[VSX_MAX_PATH_LEN];
  const char *filename;
  size_t sz; 

  if(pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.fp != FILEOPS_INVALID_FP) {

    filename = pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.filename;
    LOG(X_DEBUG("Closing recording %s"), filename);

    fileops_Close(pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.fp);
    pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.fp = FILEOPS_INVALID_FP;

    if(fileops_stat(filename, &st) == 0 && st.st_size == 0) {
      LOG(X_WARNING("Deleting empty file '%s'"),filename);
      fileops_DeleteFile(filename); 
    }

    if((sz = strlen(filename)) > VSXTMP_EXT_LEN && 
       !strncmp(&filename[sz - VSXTMP_EXT_LEN], VSXTMP_EXT, VSXTMP_EXT_LEN)) {
      strncpy(filenamenew, filename, sizeof(filenamenew) - 1);
      filenamenew[sz - VSXTMP_EXT_LEN] = '\0';
      LOG(X_DEBUG("Renaming %s -> %s"), filename, filenamenew);
      fileops_Rename(filename, filenamenew);
    }
  }

  return 0;
}

static int recordstop(SRV_CTRL_T *pSrvCtrl) {

  pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  pSrvCtrl->curCfg.streamerCfg.action.do_record_pre = 0;
  pSrvCtrl->curCfg.streamerCfg.action.do_record_post = 0;
  pSrvCtrl->curCfg.streamerCfg.cbs[0].cbRecord = NULL;
  pSrvCtrl->curCfg.streamerCfg.cbs[0].pCbRecordData = NULL;
  close_recording(pSrvCtrl);

  pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  LOG(X_DEBUG("Recording stopped"));

  return 0;
}

static int srv_isactive(SRV_CTRL_T *pSrvCtrl) {

  int isRunning = 0;

  pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  if(pSrvCtrl->curCfg.streamerCfg.running >= 0) {
    isRunning = 1;
  }

  pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  return isRunning;
}

static int srv_cmd_play_abort(SRV_CTRL_T *pSrvCtrl, 
                              int quitRequested, int pauseRequested,
                              STREAMER_JUMP_T *pJump) {

  int isRunning = 0;

  pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  if(pSrvCtrl->curCfg.streamerCfg.running >= 0) {
    isRunning = 1;

    pSrvCtrl->curCfg.streamerCfg.jump.type = JUMP_OFFSET_TYPE_NONE;

    if(pJump) {

      //Heed jump only during file playout 
      if(pSrvCtrl->curCfg.cmd != SRV_CMD_PLAY_FILE) {
        pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
        return isRunning;
      }

      if(pJump->type == JUMP_OFFSET_TYPE_SEC && pJump->u.jumpOffsetSec < 0) {
        pJump->u.jumpOffsetSec = 0;
      }
      if(pJump) {
        memcpy(&pSrvCtrl->curCfg.streamerCfg.jump, pJump, sizeof(STREAMER_JUMP_T));
      }
      pSrvCtrl->curCfg.streamerCfg.pauseRequested = 0;

    } else if(pauseRequested) {

      pSrvCtrl->curCfg.streamerCfg.quitRequested = quitRequested;
      if(pauseRequested == 1 || pauseRequested == 0) {
        pSrvCtrl->curCfg.streamerCfg.pauseRequested = pauseRequested;
      } else if(pauseRequested == -1) {
         // toggle
         pSrvCtrl->curCfg.streamerCfg.pauseRequested =
         !pSrvCtrl->curCfg.streamerCfg.pauseRequested;
      }
    } else if(quitRequested) {
      pSrvCtrl->curCfg.streamerCfg.pauseRequested = 0;
      pSrvCtrl->curCfg.streamerCfg.quitRequested = quitRequested;
    } else {
      // will cause file playout to finish (skip to next playlist item)
    }

    pSrvCtrl->curCfg.streamerCfg.running = STREAMER_STATE_ENDING;
    if(pSrvCtrl->curCfg.cmd == SRV_CMD_PLAY_LIVE) {
      // && pSrvCtrl->curCfg.streamerCfg.pSleepQ) {
      //pSrvCtrl->curCfg.streamerCfg.pSleepQ->quitRequested = quitRequested;
      //pktqueue_wakeup(pSrvCtrl->curCfg.streamerCfg.pSleepQ);
      streamer_stop(&pSrvCtrl->curCfg.streamerCfg, quitRequested,
                    pSrvCtrl->curCfg.streamerCfg.pauseRequested, 0, 0);
    }
  }

  pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  return isRunning;
}

static int srv_cmd_handler_playfile(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals, 
                                    unsigned char **ppout, unsigned int *plenout) {
  int rc = 0;
  unsigned int idxDest;
  char filepath[VSX_MAX_PATH_LEN];
  char tmpstr[32];
  const char *pfilepath = NULL;
  const char *parg = NULL;
  const char *pargdst = NULL;
  PLAYLIST_M3U_T *pPlaylist = NULL;
  SRV_PROPS_T srvProps;
  SRV_CTRL_NEXT_T nextCfg;
  STREAMER_DEST_CFG_T destsCfg[SRV_PROP_DEST_MAX];
  SRV_CTRL_T *pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];
  SRV_CTRL_T *pSrvCtrlLive = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];

  memset(&nextCfg, 0, sizeof(nextCfg));
  memset(&srvProps, 0, sizeof(srvProps));
  memset(destsCfg, 0, sizeof(destsCfg));
  nextCfg.streamerCfg.pdestsCfg = destsCfg;
  nextCfg.streamerCfg.pmaxRtp = pSrvCtrl->nextCfg.streamerCfg.pmaxRtp; 
  // sanity check
  if(*pSrvCtrl->nextCfg.streamerCfg.pmaxRtp > SRV_PROP_DEST_MAX) {
    *((unsigned int *) pSrvCtrl->nextCfg.streamerCfg.pmaxRtp) = SRV_PROP_DEST_MAX;
  }

  if(pConn->pCfg->pMediaDb->mediaDir == NULL) {
    LOG(X_ERROR("Unable to play file - media directory not set in configuration"));
    *plenout = snprintf((char *) *ppout, *plenout, "media dir not set in conf");
    return -1;
  }

  if(!(pfilepath = conf_find_keyval(pKeyvals, "file"))) {
    LOG(X_ERROR("'file' parameter not found in request"));
    *plenout = snprintf((char *) *ppout, *plenout, "file parameter not found");
    return -1;
  }
  
  if(mediadb_prepend_dir(pConn->pCfg->pMediaDb->mediaDir, pfilepath, 
                         filepath, sizeof(filepath)) < 0) {
    LOG(X_ERROR("Unable to create full file path for '%s'"), pfilepath);
    *plenout = snprintf((char *) *ppout, *plenout, 
                  "Unable to create full file path for '%s'", pfilepath);
    return -1;
  } 
  mediadb_fixdirdelims(filepath, DIR_DELIMETER);

  if((parg = conf_find_keyval(pKeyvals, "fps"))) {
    nextCfg.fps = atof(parg); 
  }

  if(pConn->pCfg->propFilePath) {
    srvprop_read(&srvProps, pConn->pCfg->propFilePath);
  }

  for(idxDest = 0; idxDest < *nextCfg.streamerCfg.pmaxRtp; idxDest++) {

    if(idxDest == 0) {
      snprintf(tmpstr, sizeof(tmpstr), "dst");
    } else {
      snprintf(tmpstr, sizeof(tmpstr), "dst%d", idxDest+1);
    }

    if((parg = pargdst = conf_find_keyval(pKeyvals, tmpstr))) {

      //TODO: output_rtphdr is applied to all outbound streams, not just this one
      if(CAPTURE_FILTER_TRANSPORT_UDPRAW != capture_parseTransportStr(&pargdst)) {
        nextCfg.streamerCfg.action.do_output_rtphdr = 1;
      }

      if((nextCfg.streamerCfg.pdestsCfg[idxDest].numPorts = 
         strutil_convertDstStr(pargdst, nextCfg.streamerCfg.pdestsCfg[idxDest].dstHost,
             sizeof(nextCfg.streamerCfg.pdestsCfg[idxDest].dstHost), 
             nextCfg.streamerCfg.pdestsCfg[idxDest].ports, 2, -1, NULL, 0)) <= 0) {

        LOG(X_ERROR("Invalid '%s' value '%s' in request"), tmpstr, pargdst);
        *plenout = snprintf((char *) *ppout, *plenout, "invalid %s argument", tmpstr);
        return -1;
      }
      nextCfg.streamerCfg.numDests++;
      strncpy(srvProps.dstaddr[idxDest], parg, sizeof(srvProps.dstaddr[idxDest]) - 1);
    }  else {
      srvProps.dstaddr[idxDest][0] = '\0';
    }
    
  }

  if((parg = conf_find_keyval(pKeyvals, "live"))) {
    nextCfg.streamerCfg.action.do_tslive = atoi(parg);
  }
  srvProps.tslive = nextCfg.streamerCfg.action.do_tslive;

  if((parg = conf_find_keyval(pKeyvals, "httplive"))) {
    nextCfg.streamerCfg.action.do_httplive = atoi(parg);
  }
  srvProps.httplive = nextCfg.streamerCfg.action.do_httplive;

  if(nextCfg.streamerCfg.numDests == 0 && !nextCfg.streamerCfg.action.do_tslive &&
     !nextCfg.streamerCfg.action.do_httplive) {
    LOG(X_ERROR("No stream destination set for file"));
    *plenout = snprintf((char *) *ppout, *plenout, "no destination");
    return -1;
  } else if(nextCfg.streamerCfg.numDests > 0) {
    nextCfg.streamerCfg.action.do_output = 1;
  }

  if((parg = conf_find_keyval(pKeyvals, "proto"))) {
    if((nextCfg.streamerCfg.proto = 
          strutil_convertProtoStr(parg)) == STREAM_PROTO_INVALID) {
      LOG(X_ERROR("Invalid value '%s' for 'proto' in request"), parg);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid proto argument");
      return -1;
    }
  } else {
    nextCfg.streamerCfg.proto = STREAM_PROTO_AUTO;
  }

  if((parg = conf_find_keyval(pKeyvals, "mtu"))) {
    if((nextCfg.streamerCfg.cfgrtp.maxPayloadSz = atoi(parg)) <= 0) {
      LOG(X_ERROR("Invalid value '%s' for 'mtu' in request"), parg);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid mtu argument");
      return -1;
    }
  }

  if((parg = conf_find_keyval(pKeyvals, "xcode")) && strlen(parg) > 2) {
    if(xcode_parse_configstr(parg, &nextCfg.streamerCfg.xcode, 1, 1) < 0) {
      *plenout = snprintf((char *) *ppout, *plenout, "Invalid xcode options");
      return -1;
    }
    // m2t files can be transcoded only when going throuth the PES interface
    if(nextCfg.streamerCfg.xcode.vid.common.cfgDo_xcode ||
       nextCfg.streamerCfg.xcode.aud.common.cfgDo_xcode) {
      nextCfg.streamerCfg.mp2tfile_extractpes=BOOL_ENABLED_DFLT;
    }
  }

  if((parg = conf_find_keyval(pKeyvals, "xcodeprofile"))) {
    srvProps.xcodeprofile = atoi(parg);

    if(srvProps.xcodeprofile != 0 &&
       nextCfg.streamerCfg.xcode.vid.common.cfgFileTypeOut != MEDIA_FILE_TYPE_H264b) {
      LOG(X_ERROR("Output format is incompatible with selected device."));
      *plenout = snprintf((char *) *ppout, *plenout, "Invalid output format");
      return -1;
    }
  }


  if((parg = conf_find_keyval(pKeyvals, "loop"))) {
    nextCfg.streamerCfg.loop = atoi(parg); 
  }
  if((parg = conf_find_keyval(pKeyvals, "looppl"))) {
    nextCfg.streamerCfg.loop_pl = atoi(parg); 
  }

  if((parg = conf_find_keyval(pKeyvals, "verbose"))) {
    nextCfg.streamerCfg.verbosity = 1; 
  }

  // 'input' is sent along w/ playfile cmd for the sole
  // sake of keeping the srvProps cached list uptodate
  if((parg = conf_find_keyval(pKeyvals, "input"))) {
    strncpy(srvProps.capaddr, parg, sizeof(srvProps.capaddr) - 1);
  }


  if(nextCfg.streamerCfg.action.do_httplive) {
    if(httplive_init(pConn->pCfg->pHttpLiveDatas[0]) != 0) {
     return -1;
    }
    nextCfg.streamerCfg.cbs[0].cbPostProc = httplive_cbOnPkt;
    nextCfg.streamerCfg.cbs[0].pCbPostProcData =  pConn->pCfg->pHttpLiveDatas[0];
  } else {
    httplive_close(pConn->pCfg->pHttpLiveDatas[0]);
  }

  //TODO: temporary
    //nextCfg.streamerCfg.action.outFmt.do_outfmt = 1;
    //nextCfg.streamerCfg.action.outFmt.cbOutFmt = rtmp_addFrame;
    //nextCfg.streamerCfg.action.outFmt.pOutFmtCbData = NULL;

  if(m3u_isvalidFile(filepath, 0)) {
    strncpy(nextCfg.playlistpath, filepath, sizeof(nextCfg.playlistpath) - 1);
    nextCfg.isplaylist = 1;
  } else {

    if(!(pPlaylist = m3u_create_fromdir(pConn->pCfg->pMediaDb, filepath,
                                       srvProps.dirsort))) {
      LOG(X_ERROR("Unable to create directory playlist in '%s'"), filepath);
    }
  }

  // Store the destination in the user config file
  if(pConn->pCfg->propFilePath) {
    srvprop_write(&srvProps, pConn->pCfg->propFilePath);
  }

  nextCfg.cmd = SRV_CMD_PLAY_FILE;
  nextCfg.streamerCfg.action.do_stream = 1;
  nextCfg.streamerCfg.status.vidTnCnt = VID_TN_CNT_NOT_SET;
  nextCfg.streamerCfg.maxPlRecurseLevel = 3;
  strncpy(nextCfg.filepath, filepath, sizeof(nextCfg.filepath) -1);

  LOG(X_INFO("Will play %s"), filepath);

  // preempt live stream output
  if(srv_isactive(pSrvCtrlLive)) {
    pthread_mutex_lock(&pSrvCtrlLive->curCfg.streamerCfg.mtxStrmr);
    if(pSrvCtrlLive->curCfg.streamerCfg.action.do_stream) {
      pSrvCtrlLive->curCfg.streamerCfg.action.do_stream = 0;
    }
    pthread_mutex_unlock(&pSrvCtrlLive->curCfg.streamerCfg.mtxStrmr);
  }

  pthread_mutex_lock(&pSrvCtrl->mtx);


  if(srv_isactive(pSrvCtrl)) {
    if(pSrvCtrl->curCfg.cmd == SRV_CMD_PLAY_FILE) {
      LOG(X_DEBUG("aborting playout of '%s'"), pSrvCtrl->curCfg.filepath);
    }
    srv_cmd_play_abort(pSrvCtrl, 1, 0, NULL);
  }
  
  //
  // Check if pre-empting another thread's play directive within the srvCtrl mutex lock
  // make sure to deallocate any existing playlist that may have been set
  //
  if(pSrvCtrl->nextCfg.pDirPlaylist) {
    m3u_free(pSrvCtrl->nextCfg.pDirPlaylist, 1);
  }

  copy_srvctrl(&pSrvCtrl->nextCfg, &nextCfg);
  pSrvCtrl->nextCfg.pDirPlaylist = pPlaylist;

  pSrvCtrl->startNext = 1;

  pthread_mutex_unlock(&pSrvCtrl->mtx);

  if(rc == 0) {
    *plenout = snprintf((char *) *ppout, *plenout, "Playing %s", nextCfg.filepath);
  }

  return rc;
}

static int srv_cmd_handler_playjumpfile(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals, 
                                        unsigned char **ppout, unsigned int *plenout) {
  int rc = 0;
  const char *parg = NULL;
  float jumpOffsetSec = 0;
  float jumpPercent = -1;
  float jumpFfwdSec = 0;
  double durationMs = 0;
  double locationMs = 0;
  STREAMER_JUMP_T jump;
  SRV_CTRL_T *pSrvCtrlFl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];
  SRV_CTRL_T *pSrvCtrlLv = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];

  memset(&jump, 0, sizeof(jump));

  if((parg = conf_find_keyval(pKeyvals, "offset"))) {
    if((jumpOffsetSec = (float) atof(parg)) < 0) {
      LOG(X_ERROR("invalid jump offset %f"), jumpOffsetSec);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid jump offset");
      return -1;
    }
  } else if((parg = conf_find_keyval(pKeyvals, "percent"))) {
    if((jumpPercent = (float) atof(parg)) < 0) {
      LOG(X_ERROR("invalid jump percent %f"), jumpPercent);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid jump percent");
      return -1;
    }
  } else if((parg = conf_find_keyval(pKeyvals, "ffwd"))) {
    if((jumpFfwdSec = (float) atof(parg)) == 0) {
      LOG(X_ERROR("invalid jump ffwd %f"), jumpFfwdSec);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid ffwd");
      return -1;
    }
  } else {
    LOG(X_ERROR("no 'offset','percent','ffwd' specified in jump command"));
    *plenout = snprintf((char *) *ppout, *plenout, "no offset, percent, or ffwd");
    return -1;
  }

  //fprintf(stderr, "jumpfile perc:%f, jumpOffsetSec:%f\n", jumpPercent, jumpOffsetSec);

  if(jumpPercent >= 0 || jumpFfwdSec != 0) { 

    srv_lock_conn_mutexes(pConn, 1);

    if(pSrvCtrlFl->curCfg.cmd == SRV_CMD_PLAY_FILE &&
      srv_isactive(pSrvCtrlFl)) {

      pthread_mutex_lock(&pSrvCtrlLv->curCfg.streamerCfg.mtxStrmr);
      if(pSrvCtrlLv->curCfg.streamerCfg.action.do_stream) {
        pSrvCtrlLv->curCfg.streamerCfg.action.do_stream = 0;
      }
      pthread_mutex_unlock(&pSrvCtrlLv->curCfg.streamerCfg.mtxStrmr);

      //TODO: reading these vals is not atomic
      durationMs = pSrvCtrlFl->curCfg.streamerCfg.status.durationMs;
      locationMs = pSrvCtrlFl->curCfg.streamerCfg.status.locationMs;
    }

    srv_lock_conn_mutexes(pConn, 0);

    if(durationMs == 0) {
      jump.type = JUMP_OFFSET_TYPE_BYTE;
      jump.u.jumpPercent = jumpPercent; 
    } else {
      jump.type = JUMP_OFFSET_TYPE_SEC;

     if(jumpPercent >= 0) {
        jump.u.jumpOffsetSec = (jumpPercent/100.0f * (float) durationMs)/1000.0f; 
      } else {
        if((jump.u.jumpOffsetSec = (float)(locationMs / 1000.0) + 
                                    jumpFfwdSec) >= durationMs / 1000.0) {
          jump.u.jumpOffsetSec = (float)(durationMs / 1000.0) - .5f;
        }
        if(jump.u.jumpOffsetSec < 0) {
          jump.u.jumpOffsetSec = 0;
        }
      }
    }


  }

  pthread_mutex_lock(&pSrvCtrlFl->mtx);

  if(srv_isactive(pSrvCtrlFl)) {
    srv_cmd_play_abort(pSrvCtrlFl, 0, 0, &jump);
  }

  pthread_mutex_unlock(&pSrvCtrlFl->mtx);

  return rc;
}

static int srv_cmd_handler_skipfile(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals, 
                                    unsigned char **ppout, unsigned int *plenout) {
  int rc = 0;
  SRV_CTRL_T *pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];

  pthread_mutex_lock(&pSrvCtrl->mtx);

  if(pSrvCtrl->curCfg.cmd == SRV_CMD_PLAY_FILE) {

    // If playing a file - and part of a virtual directory playlist
    // turn on loop_pl to allow this skip... loop_pl will remain on
    if(!pSrvCtrl->curCfg.isplaylist && !pSrvCtrl->curCfg.streamerCfg.loop_pl) {
      pSrvCtrl->curCfg.streamerCfg.loop_pl = 1;
    }

    srv_cmd_play_abort(pSrvCtrl, 0, 0, NULL);
  }

  pthread_mutex_unlock(&pSrvCtrl->mtx);

  *plenout = 0;

  return rc;
}

static int channel_change(SRV_CFG_T *pCfg, int channel) {
  int rc = 0;
  struct stat st;
  char cmd[512];

  if(!pCfg->pchannelchgr) {
    LOG(X_ERROR("channel changer program not set in config"));
    return -1;
  } else if(fileops_stat(pCfg->pchannelchgr, &st) != 0) {
    LOG(X_ERROR("Unable to stat channel changer program '%s'"), pCfg->pchannelchgr);
    return -1;
  }

  snprintf(cmd, sizeof(cmd), "%s %d", pCfg->pchannelchgr, channel);

  LOG(X_DEBUG("Changing channel '%s'"), cmd);
  fprintf(stderr, "executing cmd:'%s'\n", cmd);

  rc = system(cmd);

  return rc;
}

static int srv_cmd_handler_playsetprop(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                    unsigned char **ppout, unsigned int *plenout) {
  int rc = 0;
  int haveloop = 0;
  int loopreq = 0;
  int loopplaylistreq = 0;
  int havetslive = 0;
  int haveavsync = 0;
  int havechannel = 0;
  int channel = 0;
  int tslive = 0;
  int avsync = 0;
  int tmp;
  const char *parg = NULL;
  SRV_CTRL_T *pSrvCtrl = NULL;

  if((parg = conf_find_keyval(pKeyvals, "loop"))) {
    haveloop = 1;
    loopreq = atoi(parg);
  }
  if((parg = conf_find_keyval(pKeyvals, "looppl"))) {
    haveloop = 1;
    loopplaylistreq = atoi(parg);
  }
  if((parg = conf_find_keyval(pKeyvals, "live"))) {
    havetslive = 1;
    tslive = atoi(parg);
  }
  if((parg = conf_find_keyval(pKeyvals, "avsync"))) {
    haveavsync = 1;
    avsync = atoi(parg);
  }
  if((parg = conf_find_keyval(pKeyvals, "channel"))) {
    havechannel = 1;
    channel = atoi(parg);
  }

  if(havechannel) {
    channel_change(pConn->pCfg, channel);
  }

  srv_lock_conn_mutexes(pConn, 1);

  if(havetslive) {

    pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];

    if((tmp = pSrvCtrl->curCfg.streamerCfg.action.do_tslive) != tslive) {
      //TODO: only locking 1 mutex here...
      pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
      pConn->ppSrvCtrl[0]->curCfg.streamerCfg.action.do_tslive = tslive;
      pConn->ppSrvCtrl[1]->curCfg.streamerCfg.action.do_tslive = tslive;
      pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
    }
  }

  if(haveavsync) {
    pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];
    fprintf(stderr, "av sync lag at: %"LL64"d\n", pSrvCtrl->curCfg.streamerCfg.status.avs[0].offsetCur);
    pSrvCtrl->curCfg.streamerCfg.status.avs[0].offsetCur = (int64_t) avsync;
    fprintf(stderr, "av sync lag set to: %"LL64"d\n", pSrvCtrl->curCfg.streamerCfg.status.avs[0].offsetCur);
  }


  pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];

  if(haveloop && pSrvCtrl->curCfg.cmd == SRV_CMD_PLAY_FILE &&
     srv_isactive(pSrvCtrl)) {

    pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

    if(pSrvCtrl->curCfg.isplaylist) {

      if(pSrvCtrl->curCfg.streamerCfg.loop != loopreq) {
        pSrvCtrl->curCfg.streamerCfg.loop = loopreq;
      }

      if(pSrvCtrl->curCfg.streamerCfg.loop_pl != loopreq) {
        pSrvCtrl->curCfg.streamerCfg.loop_pl = loopreq;
      }

    } else {
  
      if(pSrvCtrl->curCfg.streamerCfg.loop != loopreq) {
        pSrvCtrl->curCfg.streamerCfg.loop = loopreq;
      }
      // loop_pl describes looping of the virtual directory playlist
      if(pSrvCtrl->curCfg.streamerCfg.loop_pl != loopplaylistreq) {
        pSrvCtrl->curCfg.streamerCfg.loop_pl = loopplaylistreq;
      }

    }
    pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
    
  }

  srv_lock_conn_mutexes(pConn, 0);

  *plenout = 0;

  return rc;
}

static int srv_cmd_handler_pause(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals, 
                                 unsigned char **ppout, unsigned int *plenout) {
  int rc = 0;
  const char *parg;
  int pausefile = 0;
  int pauselive = 0;
  int paused = 0;
  SRV_CTRL_T *pSrvCtrlFl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];
  SRV_CTRL_T *pSrvCtrlLv = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];


  if((parg = conf_find_keyval(pKeyvals, "file"))) {
    pausefile = atoi(parg);
  }

  if((parg = conf_find_keyval(pKeyvals, "live"))) {
    pauselive= atoi(parg);
  }

  srv_lock_conn_mutexes(pConn, 1);

  if(pausefile) {
    if(srv_isactive(pSrvCtrlFl)) {

      pthread_mutex_lock(&pSrvCtrlFl->curCfg.streamerCfg.mtxStrmr);
      paused = pSrvCtrlFl->curCfg.streamerCfg.pauseRequested;
      pthread_mutex_unlock(&pSrvCtrlFl->curCfg.streamerCfg.mtxStrmr);
      if(paused && pSrvCtrlLv->curCfg.streamerCfg.action.do_stream) {

        pthread_mutex_lock(&pSrvCtrlLv->curCfg.streamerCfg.mtxStrmr);
        pSrvCtrlLv->curCfg.streamerCfg.action.do_stream = 0;
        pthread_mutex_unlock(&pSrvCtrlLv->curCfg.streamerCfg.mtxStrmr);
      }

      srv_cmd_play_abort(pSrvCtrlFl, 0, -1, NULL);
    }
  }

  if(pauselive) {
    if(srv_isactive(pSrvCtrlLv)) {
      pthread_mutex_lock(&pSrvCtrlLv->curCfg.streamerCfg.mtxStrmr);

      // Avoid turning on live stream if file playout is ongoing
      if(!pSrvCtrlLv->curCfg.streamerCfg.action.do_stream &&
        srv_isactive(pSrvCtrlFl) && !pSrvCtrlFl->curCfg.streamerCfg.pauseRequested) {

        srv_cmd_play_abort(pSrvCtrlFl, 0, 1, NULL);
      }
      pSrvCtrlLv->curCfg.streamerCfg.action.do_stream =
             !pSrvCtrlLv->curCfg.streamerCfg.action.do_stream;

      pthread_mutex_unlock(&pSrvCtrlLv->curCfg.streamerCfg.mtxStrmr);
    }
  }

  srv_lock_conn_mutexes(pConn, 0);

  *plenout = 0;

  return rc;
}

static int srv_cmd_handler_playlive(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                    unsigned char **ppout, unsigned int *plenout) {
  int rc = 0;

#if defined(VSX_HAVE_CAPTURE)

  const char *parg = NULL;
  const char *parg2 = NULL;
  unsigned int idxDest;
  char tmpstr[32];
  CAPTURE_FILTER_TRANSPORT_T transType = CAPTURE_FILTER_TRANSPORT_UNKNOWN;
  SRV_PROPS_T srvProps;
  SRV_CTRL_NEXT_T nextCfg;
  STREAMER_DEST_CFG_T destsCfg[SRV_PROP_DEST_MAX];
  SRV_CTRL_T *pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];
  SRV_CTRL_T *pSrvCtrlPl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];

  memset(&nextCfg, 0, sizeof(nextCfg));
  memset(destsCfg, 0, sizeof(destsCfg));
  nextCfg.streamerCfg.pdestsCfg = destsCfg;
  nextCfg.streamerCfg.pmaxRtp = pSrvCtrl->nextCfg.streamerCfg.pmaxRtp; 
  // sanity check
  if(*pSrvCtrl->nextCfg.streamerCfg.pmaxRtp > SRV_PROP_DEST_MAX) {
    *((unsigned int *) pSrvCtrl->nextCfg.streamerCfg.pmaxRtp) = SRV_PROP_DEST_MAX;
  }

  // Store the destination in the user config file
  if(pConn->pCfg->propFilePath) {
    srvprop_read(&srvProps, pConn->pCfg->propFilePath);
  }

  for(idxDest = 0; idxDest < *nextCfg.streamerCfg.pmaxRtp; idxDest++) {

    if(idxDest == 0) {
      snprintf(tmpstr, sizeof(tmpstr), "dst");
    } else {
      snprintf(tmpstr, sizeof(tmpstr), "dst%d", idxDest);
    }

    if((parg2 = parg = conf_find_keyval(pKeyvals, tmpstr))) {

      //TODO: output_rtphdr is applied to all outbound streams, not just this one
      if(CAPTURE_FILTER_TRANSPORT_UDPRAW != capture_parseTransportStr(&parg)) {
        nextCfg.streamerCfg.action.do_output_rtphdr = 1;
      }

      if((nextCfg.streamerCfg.pdestsCfg[idxDest].numPorts =
            strutil_convertDstStr(parg, nextCfg.streamerCfg.pdestsCfg[idxDest].dstHost,
             sizeof(nextCfg.streamerCfg.pdestsCfg[idxDest].dstHost),
             nextCfg.streamerCfg.pdestsCfg[idxDest].ports, 2, -1, NULL, 0)) <= 0) {

        LOG(X_ERROR("Invalid '%s' value '%s' in request"), tmpstr, parg);
        *plenout = snprintf((char *) *ppout, *plenout, "invalid %s argument", tmpstr);
        return -1;
      }
      nextCfg.streamerCfg.numDests++;
      strncpy(srvProps.dstaddr[idxDest], parg2, sizeof(srvProps.dstaddr[idxDest]) - 1);
    } else {
      srvProps.dstaddr[idxDest][0] = '\0';
    }
  }

  if((parg = conf_find_keyval(pKeyvals, "live"))) {
    nextCfg.streamerCfg.action.do_tslive = atoi(parg);
  }
  srvProps.tslive = nextCfg.streamerCfg.action.do_tslive;

  if((parg = conf_find_keyval(pKeyvals, "httplive"))) {
    nextCfg.streamerCfg.action.do_httplive = atoi(parg);
  }
  srvProps.httplive = nextCfg.streamerCfg.action.do_httplive;

  if(nextCfg.streamerCfg.numDests == 0 && !nextCfg.streamerCfg.action.do_tslive &&
     !nextCfg.streamerCfg.action.do_httplive) {
    LOG(X_ERROR("No stream destination set for live"));
    *plenout = snprintf((char *) *ppout, *plenout, "invalid destination");
    return -1;
  } else if(nextCfg.streamerCfg.numDests > 0) {
    nextCfg.streamerCfg.action.do_output = 1;
  }

  if((parg = conf_find_keyval(pKeyvals, "proto"))) {
    if((nextCfg.streamerCfg.proto = 
          strutil_convertProtoStr(parg)) == STREAM_PROTO_INVALID) {
      LOG(X_ERROR("Invalid value '%s' for 'proto' in request"), parg);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid proto argument");
      return -1;
    }
  } else {
    nextCfg.streamerCfg.proto = STREAM_PROTO_MP2TS;
  }

  if((parg = conf_find_keyval(pKeyvals, "mtu"))) {
    if((nextCfg.streamerCfg.cfgrtp.maxPayloadSz = atoi(parg)) <= 0) {
      LOG(X_ERROR("Invalid value '%s' for 'mtu' in request"), parg);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid mtu argument");
      return -1;
    }
  }

  if((parg = conf_find_keyval(pKeyvals, "filter"))) {
    
    if(capture_parseFilter(parg, &nextCfg.captureCfg.common.filt.filters[0]) < 0) {
      LOG(X_ERROR("Invalid 'filter' value '%s' in request"), parg);
      *plenout = snprintf((char *) *ppout, *plenout, "invalid filter argument");
      return -1;
    }
    nextCfg.captureCfg.common.filt.numFilters = 1;

    //TODO: ensure no need to call
    // capture_freeFilters(capCfg.filt.filters, capCfg.filt.numFilters);
    // since seq hdrs are not being allocated

  }

  if((parg = parg2 = conf_find_keyval(pKeyvals, "input"))) {

    if((transType = capture_parseTransportStr(&parg2)) == 
                    CAPTURE_FILTER_TRANSPORT_UNKNOWN) {
      transType = CAPTURE_FILTER_TRANSPORT_UDPRAW;
    }

    LOG(X_DEBUG("Using capture transport filter type %d"), transType);

    nextCfg.captureCfg.common.filt.filters[0].transType = transType;
    strncpy(nextCfg.filepath, parg2, sizeof(nextCfg.filepath) - 1);
    strncpy(srvProps.capaddr, parg, sizeof(srvProps.capaddr) - 1);
    srvProps.captranstype = transType;
  }

  if((parg = conf_find_keyval(pKeyvals, "verbose"))) {
    nextCfg.streamerCfg.verbosity = nextCfg.captureCfg.common.verbosity = 1; 
  }

  if((parg = conf_find_keyval(pKeyvals, "extractpes"))) {
    if(nextCfg.captureCfg.common.filt.filters[0].mediaType != CAPTURE_FILTER_PROTOCOL_MP2TS)  {
      LOG(X_ERROR("PES extract can only be done on mpeg2-ts stream"));
      *plenout = snprintf((char *) *ppout, *plenout, "PES extract not permitted");
      return -1;
    }
    nextCfg.captureCfg.capActions[0].cmd |= CAPTURE_PKT_ACTION_EXTRACTPES;

    if((parg = conf_find_keyval(pKeyvals, "xcode")) && strlen(parg) > 2) {
      if(xcode_parse_configstr(parg, &nextCfg.streamerCfg.xcode, 1, 1) < 0) {
        *plenout = snprintf((char *) *ppout, *plenout, "Invalid xcode options");
        return -1;
      }
    }
   
  }

  if((parg = conf_find_keyval(pKeyvals, "xcodeprofile"))) {
    srvProps.xcodeprofile = atoi(parg);

    if(srvProps.xcodeprofile != 0 &&
       nextCfg.streamerCfg.xcode.vid.common.cfgFileTypeOut != MEDIA_FILE_TYPE_H264b) {
      LOG(X_ERROR("Output format is incompatible with selected device."));
      *plenout = snprintf((char *) *ppout, *plenout, "Invalid output format");
      return -1;
    }
  }

  if(nextCfg.streamerCfg.action.do_httplive) {
    if(httplive_init(pConn->pCfg->pHttpLiveDatas[0]) != 0) {
     return -1;
    }
    nextCfg.streamerCfg.cbs[0].cbPostProc = httplive_cbOnPkt;
    nextCfg.streamerCfg.cbs[0].pCbPostProcData =  pConn->pCfg->pHttpLiveDatas[0];
  } else {
    httplive_close(pConn->pCfg->pHttpLiveDatas[0]);
  }

  //TODO: - this only works for 1 port / queue, such as m2t capture
  nextCfg.captureCfg.capActions[0].cmd |= CAPTURE_PKT_ACTION_QUEUE;
  nextCfg.captureCfg.common.filt.filters[0].pCapAction = &nextCfg.captureCfg.capActions[0];
  nextCfg.captureCfg.common.filt.filters[1].pCapAction = &nextCfg.captureCfg.capActions[1];

  if((nextCfg.captureCfg.capActions[0].cmd & CAPTURE_PKT_ACTION_QUEUE)) {

    nextCfg.captureCfg.capActions[0].pktqslots = PKTQUEUE_LEN_DEFAULT;
    if(nextCfg.streamerCfg.xcode.vid.common.cfgDo_xcode) {
      nextCfg.captureCfg.capActions[0].pktqslots *= 2;
    } 

    if((parg = conf_find_keyval(pKeyvals, "queue"))) {
      nextCfg.captureCfg.capActions[0].pktqslots = atoi(parg); 
      LOG(X_INFO("Set client requested queue size to %d"), 
                  nextCfg.captureCfg.capActions[0].pktqslots);
    }
  }

  // Check whether to start streaming or paused
  if((parg = conf_find_keyval(pKeyvals, "stream"))) {
    nextCfg.streamerCfg.action.do_stream = 1;
  }

  // Store the destination in the user config file
  if(pConn->pCfg->propFilePath) {
    srvprop_write(&srvProps, pConn->pCfg->propFilePath);
  }

  nextCfg.cmd = SRV_CMD_PLAY_LIVE;

  srv_lock_conn_mutexes(pConn, 1);

  // disable live stream output if currently streaming file
  if(srv_isactive(pSrvCtrlPl) && nextCfg.streamerCfg.action.do_stream) {
    pthread_mutex_lock(&pSrvCtrlPl->curCfg.streamerCfg.mtxStrmr);
    if(pSrvCtrlPl->curCfg.streamerCfg.action.do_stream) {
      fprintf(stderr, "will not stream live because file is playing\n");
      nextCfg.streamerCfg.action.do_stream = 0;
    }
    pthread_mutex_unlock(&pSrvCtrlPl->curCfg.streamerCfg.mtxStrmr);
  }

  if(srv_isactive(pSrvCtrl)) {
    srv_cmd_play_abort(pSrvCtrl, 1, 0, NULL);
  }
  
  copy_srvctrl(&pSrvCtrl->nextCfg, &nextCfg);

  pSrvCtrl->startNext = 1;

  srv_lock_conn_mutexes(pConn, 0);

  if(rc == 0) {
    *plenout = snprintf((char *) *ppout, *plenout, "Playing live feed");
  }

#endif // VSX_HAVE_CAPTURE

  return rc;
}

static int srv_cmd_handler_stop(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                unsigned char **ppout, unsigned int *plenout) {

  const char *parg;
  int stopfile = 0;
  int stoplive = 0;
  SRV_CTRL_T *pSrvCtrl;

  if((parg = conf_find_keyval(pKeyvals, "file"))) {
    stopfile = atoi(parg);
  }

  if((parg = conf_find_keyval(pKeyvals, "live"))) {
    stoplive= atoi(parg);
  }

  srv_lock_conn_mutexes(pConn, 1);

  if(stopfile) {
    pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];
    if(srv_isactive(pSrvCtrl)) {
      LOG(X_DEBUG("Stopping playout of '%s'"), pSrvCtrl->curCfg.filepath);
      srv_cmd_play_abort(pSrvCtrl, 1, 0, NULL);
    }
  }

  if(stoplive) {
    pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];
    if(srv_isactive(pSrvCtrl)) {

      if(pSrvCtrl->curCfg.streamerCfg.cbs[0].cbRecord) {
        recordstop(pSrvCtrl);
      }

      LOG(X_DEBUG("Stopping live playout"));
      srv_cmd_play_abort(pSrvCtrl, 1, 0, NULL);
    }
  }

  srv_lock_conn_mutexes(pConn, 0);

  *plenout = snprintf((char *) *ppout, *plenout, "Stopped playback");
  
  return 0;
}

static int update_curplayfile_tncnt(const char *dbDir, const char *filename) {

  FILE_LIST_T fileList;
  FILE_LIST_ENTRY_T *pFileListEntry = NULL;
  char tnpath[VSX_MAX_PATH_LEN];
  char tmp[VSX_MAX_PATH_LEN];
  const char *ptndir = dbDir;
  int len;
  int rc = -1;

  if((len = mediadb_getdirlen(filename)) > 0) {
    if(len >= sizeof(tmp)) {
      len = sizeof(tmp) - 1;
    }
    strncpy(tmp, filename, sizeof(tmp) - 1);
    tmp[len] = '\0';
    mediadb_prepend_dir(dbDir, tmp, tnpath, sizeof(tnpath));
    ptndir = tnpath;

    while(filename[len] == '/' || filename[len] == '\\') {
      len++;
    }
  }

  memset(&fileList, 0, sizeof(fileList));
  file_list_read(ptndir, &fileList);
  if((pFileListEntry = file_list_find(&fileList, &filename[len]))) {
    rc = pFileListEntry->numTn;
  } else {
    rc = -1;
  }

  file_list_close(&fileList);

  return rc;
}

static int srv_cmd_handler_status(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                  unsigned char **ppout, unsigned int *plenout) {

  struct timeval tvnow;
  struct stat st;
  double locationMs = 0;
  double durationMs= 0;
  double filePercent = 0;
  STREAM_XMIT_BW_DESCR_T xmitBwDescr;
  char rateinfo[256];
  int filecmd = 0;
  int filexmitpaused = 0;
  int livexmitpaused = 0;
  int recording = 0;
  int recordpaused= 0;
  int looping=0;
  int looping_pl=0;
  int isactive = 0;
  size_t sz = 0;
  size_t szpl = 0;
  size_t szfl = 0;
  size_t *psz = &szfl;
  char *path = NULL;
  char *perrbuf = NULL;
  int numTn = -1;
  int outrc = 0;
  int outrc2 = 0;
  SRV_CTRL_T *pSrvCtrl;
  VIDEO_DESCRIPTION_GENERIC_T capVidProp;
  VIDEO_DESCRIPTION_GENERIC_T xmitVidProp;
  //STREAMER_STATUS_VID_PROP_T capVidProp;
  //STREAMER_STATUS_VID_PROP_T xmitVidProp;
  char filename[VSX_MAX_PATH_LEN];
  char capVidPropStr[128];
  char xmitVidPropStr[128];
  char xmitDstStr[128];

  gettimeofday(&tvnow, NULL);
  memset(&xmitBwDescr, 0, sizeof(xmitBwDescr));
  memset(&capVidProp, 0, sizeof(capVidProp));
  memset(&xmitVidProp, 0, sizeof(xmitVidProp));
 
  rateinfo[0] = '\0';
  capVidPropStr[0] = '\0';
  xmitVidPropStr[0] = '\0';
  xmitDstStr[0] = '\0';
  filename[0] = '\0';

  srv_lock_conn_mutexes(pConn, 1);

  pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_FILE];

  if((isactive = srv_isactive(pSrvCtrl)) || pSrvCtrl->curCfg.isplaylist) {

    filecmd = 1;
    if(isactive) {

      locationMs = pSrvCtrl->curCfg.streamerCfg.status.locationMs;
      durationMs = pSrvCtrl->curCfg.streamerCfg.status.durationMs;

      //
      // Check if the file is currently being recorded
      //
      if(durationMs == 0 &&
         pSrvCtrl->curCfg.streamerCfg.status.vidProps[0].mediaType == MEDIA_FILE_TYPE_MP2TS &&
         pSrvCtrl->curCfg.streamerCfg.status.pFileStream) {

         if(fileops_stat(pSrvCtrl->curCfg.streamerCfg.status.pathFile, &st) != 0) {
           st.st_size = pSrvCtrl->curCfg.streamerCfg.status.pFileStream->size;
        }
        
        filePercent = ((double) pSrvCtrl->curCfg.streamerCfg.status.pFileStream->offset /
                       st.st_size) * 100;
      }
    }
    filexmitpaused = pSrvCtrl->curCfg.streamerCfg.pauseRequested;
    looping_pl = pSrvCtrl->curCfg.streamerCfg.loop_pl;
    looping = pSrvCtrl->curCfg.streamerCfg.loop;
      
    memcpy(&xmitBwDescr, &pSrvCtrl->curCfg.streamerCfg.status.xmitBwDescr,
           sizeof(xmitBwDescr));

    if(pSrvCtrl->curCfg.isplaylist) {
      path = pSrvCtrl->curCfg.streamerCfg.status.pathFile;
    } else {
      path = pSrvCtrl->curCfg.playlistpath;
      psz = &szpl;
      if(pConn->pCfg->pMediaDb->mediaDir) {
        szfl = strlen(pConn->pCfg->pMediaDb->mediaDir);
      }
    }
    if(!pConn->pCfg->pMediaDb->mediaDir || 
       (*psz = strlen(pConn->pCfg->pMediaDb->mediaDir)) >= strlen(path)) {
      *psz = 0;
    } else {
      while(path[*psz] == DIR_DELIMETER) {
        (*psz)++;
      }
    }

    if(isactive) {
      memcpy(&xmitVidProp, &pSrvCtrl->curCfg.streamerCfg.status.vidProps[0], sizeof(xmitVidProp));
      if(xmitVidProp.resH != 0 && xmitVidProp.resV != 0) {
        snprintf(xmitVidPropStr, sizeof(capVidPropStr), "&tph=%u&tpv=%u&tpfps=%.3f",
                 xmitVidProp.resH, xmitVidProp.resV, xmitVidProp.fps);
      }

      if(pSrvCtrl->curCfg.streamerCfg.status.pathFile[szfl] == DIR_DELIMETER) {
        szfl++;
      }

      //
      // Update the count of thumbnails for the current play file
      // This value is cached to prevent file index lookups for each status
      // call by the client
      //
      if(pSrvCtrl->curCfg.streamerCfg.status.vidTnCnt == VID_TN_CNT_NOT_SET) {
         numTn = update_curplayfile_tncnt(pConn->pCfg->pMediaDb->dbDir, 
                               &pSrvCtrl->curCfg.streamerCfg.status.pathFile[szfl]);
         pSrvCtrl->curCfg.streamerCfg.status.vidTnCnt = numTn;
      } else {
         numTn = pSrvCtrl->curCfg.streamerCfg.status.vidTnCnt;
      }

      http_urlencode(&pSrvCtrl->curCfg.streamerCfg.status.pathFile[szfl], 
                   filename, sizeof(filename));                               

    }

    outrc = snprintf((char *) *ppout, *plenout, 
            "file=%s%s%s%s%s%s&ms=%.1f&totms=%.1f&per=%.1f%s&tn=%d&",
            filename,
            looping ? "&loop=1" : "",
            looping_pl ? "&looppl=1" : "",
            pSrvCtrl->curCfg.isplaylist ? "&pl=" : "", 
            pSrvCtrl->curCfg.isplaylist ? &pSrvCtrl->curCfg.playlistpath[szpl] : "",
            filexmitpaused ? "&fpause=1" : "",
            locationMs, durationMs, filePercent, xmitVidPropStr, numTn);


    if(pSrvCtrl->lasterrcode != 0) {
      perrbuf = pSrvCtrl->lasterrstr; 
    }

  }

  pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];
  if(srv_isactive(pSrvCtrl)) {
    //TODO: this is not atomic / protected w/ lock from writer
    if(pSrvCtrl->curCfg.streamerCfg.action.do_stream) {
      memcpy(&xmitBwDescr, &pSrvCtrl->curCfg.streamerCfg.status.xmitBwDescr,
             sizeof(xmitBwDescr));
    }
    livexmitpaused = !pSrvCtrl->curCfg.streamerCfg.action.do_stream; 
    if(pSrvCtrl->curCfg.streamerCfg.cbs[0].cbRecord) {
      recording = 1;
      if(!(pSrvCtrl->curCfg.streamerCfg.action.do_record_pre || 
           pSrvCtrl->curCfg.streamerCfg.action.do_record_post)) {
        recordpaused=1;
      }
    }
    if(recording && pConn->pCfg->pMediaDb->mediaDir) {
      if((sz = strlen(pConn->pCfg->pMediaDb->mediaDir)) > 0 && sz < 
          strlen(pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.filename)) {
        if(pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.filename[sz] == DIR_DELIMETER) {
          sz++;
        }
      }
    }
   
    if(!livexmitpaused || !filecmd) {
      memcpy(&capVidProp, &pSrvCtrl->curCfg.streamerCfg.status.vidProps[0], sizeof(capVidProp));

      //
      // During transcoding of captured content, captured vid properties are
      // actually the output video properties
      //

      if(capVidProp.resH != 0 && capVidProp.resV != 0) {
        snprintf(capVidPropStr, sizeof(capVidPropStr), "&cph=%u&cpv=%u&cpfps=%.3f",
                 capVidProp.resH, capVidProp.resV, capVidProp.fps);
      }
    }

    outrc2 = snprintf(&((char *) (*ppout))[outrc], *plenout - outrc, "%scap=1%s%s%s%s%s",
              outrc > 0 ? "&" : "",
              livexmitpaused ? "&lpause=1" : "",
              recording ? "&record=" : "",
              recording ? &pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.filename[sz] : "",
              recordpaused ? "&rpause=1" : "",
              capVidPropStr);
    if(outrc2 > 0) {
      outrc += outrc2;
    }

    if(pSrvCtrl->lasterrcode != 0) {
      perrbuf = pSrvCtrl->lasterrstr; 
    }
    
  }

  srv_lock_conn_mutexes(pConn, 0);

  outrc2 = snprintf(&((char *) (*ppout))[outrc], *plenout - outrc, 
               "&pktrate=%.2f&kbyterate=%.2f&err=%s",
          xmitBwDescr.intervalMs>0 && tvnow.tv_sec < xmitBwDescr.updateTv.tv_sec +2 ?
          (float)xmitBwDescr.pkts*1000/xmitBwDescr.intervalMs : 0.0,
          xmitBwDescr.intervalMs>0 && tvnow.tv_sec < xmitBwDescr.updateTv.tv_sec +2 ?
          (float)xmitBwDescr.bytes/xmitBwDescr.intervalMs : 0.0,
          perrbuf ? perrbuf : "");

  if(outrc2 > 0) {
    outrc += outrc2;
  }
  
  if(outrc < 0) {
    outrc = 0;
  }

  *plenout = outrc; 

  //fprintf(stderr, "status: '%s'\n", *ppout); 

  return 0;
}

static int srv_cmd_handler_recordstart(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                       unsigned char **ppout, unsigned int *plenout) {
#if defined(VSX_HAVE_CAPTURE)

  const char *parg = NULL;
  char file[VSX_MAX_PATH_LEN];
  char dir[VSX_MAX_PATH_LEN];
  char path[VSX_MAX_PATH_LEN];
  char *fileout = NULL;
  char *dirout = NULL;
  char *pathout = NULL;
  size_t sz;
  struct stat st;
  SRV_CTRL_T *pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];

  memset(file, 0, sizeof(file));
  memset(path, 0, sizeof(path));

  if(pConn->pCfg->pMediaDb->mediaDir == NULL) {
    LOG(X_ERROR("Unable to start recording because media dir not set in conf"));
    return -1;
  }

  if((parg = conf_find_keyval(pKeyvals, "rdir"))) {

    mediadb_prepend_dir(pConn->pCfg->pMediaDb->mediaDir, parg, dir, sizeof(dir));
    mediadb_fixdirdelims(dir, DIR_DELIMETER);
    dirout = dir;  
    strncpy(path, dir, sizeof(path) - 1);
    pathout = path;
  }

  if((parg = conf_find_keyval(pKeyvals, "rfile"))) {

    strncpy(file, parg, sizeof(file) - 1);
    if((sz = strlen(file)) < 4 || 
       (strncasecmp(&file[sz -4], ".m2t", 4) &&
        strncasecmp(&file[sz -3], ".ts", 3))) {
      strncpy(&file[sz], ".m2t", sizeof(file) - sz - 1);
    }
    mediadb_fixdirdelims(file, DIR_DELIMETER);
  } else {
    snprintf(file, sizeof(file),"rec_%ld.m2t", time(NULL));
  }
  fileout = file;  

  if(dirout) {
    mediadb_prepend_dir(dirout, fileout, path, sizeof(path));
    pathout = path;
  } else {
    pathout = fileout;
  }
  if(stat(pathout, &st) == 0) {
    LOG(X_WARNING("Recording '%s' already exists - will not overwrite."), pathout);
    if(ppout && plenout) {
      *plenout = snprintf((char *) *ppout, *plenout, "Filename already exists.");
    }
    return 0;
  }

  strncat(file, VSXTMP_EXT, sizeof(file) - strlen(file));

  LOG(X_DEBUG("Recording started"));

  pthread_mutex_lock(&pSrvCtrl->mtx);

  if(srv_isactive(pSrvCtrl)) {

    pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

    if(pSrvCtrl->curCfg.streamerCfg.cbs[0].pCbRecordData) {
      pSrvCtrl->curCfg.streamerCfg.action.do_record_pre = 0;
      pSrvCtrl->curCfg.streamerCfg.action.do_record_post = 0;
      pSrvCtrl->curCfg.streamerCfg.cbs[0].cbRecord = NULL;
      pSrvCtrl->curCfg.streamerCfg.cbs[0].pCbRecordData = NULL;
    }

    pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

    close_recording(pSrvCtrl);
 

    if(dirout == NULL) {
      pSrvCtrl->recordCfg.streamsOut.outDir = pConn->pCfg->pMediaDb->mediaDir;
    } else {
      pSrvCtrl->recordCfg.streamsOut.outDir = dirout; 
    }

    memset(&pSrvCtrl->noopAction, 0, sizeof(pSrvCtrl->noopAction));
    pSrvCtrl->noopAction.cmd = CAPTURE_PKT_ACTION_NONE;
    pSrvCtrl->recordCfg.streamsOut.sp[0].stream1.fp = FILEOPS_INVALID_FP;
    //pSrvCtrl->recordCfg.streamsOut.sp[0].pCapAction = &pSrvCtrl->noopAction;
    pSrvCtrl->recordCfg.stream.pFilter = pSrvCtrl->curCfg.captureCfg.common.filt.filters;
    ((CAPTURE_FILTER_T *)pSrvCtrl->recordCfg.stream.pFilter)->pCapAction = &pSrvCtrl->noopAction;

    if(capture_cbOnStreamAdd(&pSrvCtrl->recordCfg.streamsOut, 
                           &pSrvCtrl->recordCfg.stream, NULL, 
                           fileout) < 0 ||
      pSrvCtrl->recordCfg.stream.cbOnPkt == NULL) {
      LOG(X_ERROR("Unable to initialize recording"));
      *plenout = snprintf((char *) *ppout, *plenout, "Unable to initialize recording");
      pthread_mutex_unlock(&pSrvCtrl->mtx);
      return -1;
    }

    pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
      
    pSrvCtrl->curCfg.streamerCfg.cbs[0].cbRecord = pSrvCtrl->recordCfg.stream.cbOnPkt;
    pSrvCtrl->curCfg.streamerCfg.cbs[0].pCbRecordData = &pSrvCtrl->recordCfg.streamsOut.sp[0];
    pSrvCtrl->curCfg.streamerCfg.action.do_record_pre = 1;

    pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  }

  pthread_mutex_unlock(&pSrvCtrl->mtx);

#endif // VSX_HAVE_CAPTURE

  *plenout = 0;

  return 0;
}

static int srv_cmd_handler_recordpause(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                      unsigned char **ppout, unsigned int *plenout) {

  int isrecording = 0;
  SRV_CTRL_T *pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];

  pthread_mutex_lock(&pSrvCtrl->mtx);

  if(srv_isactive(pSrvCtrl)) {

    pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
    pSrvCtrl->curCfg.streamerCfg.action.do_record_pre = 
      !pSrvCtrl->curCfg.streamerCfg.action.do_record_pre;
    isrecording = pSrvCtrl->curCfg.streamerCfg.action.do_record_pre;
    pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);

  }

  pthread_mutex_unlock(&pSrvCtrl->mtx);

  if(isrecording) {
    LOG(X_INFO("Recording resumed"));
  } else {
    LOG(X_INFO("Recording paused"));
  }

  *plenout = 0;

  return 0;
}

static int srv_cmd_handler_recordstop(CLIENT_CONN_T *pConn, const KEYVAL_PAIR_T *pKeyvals,
                                      unsigned char **ppout, unsigned int *plenout) {

  SRV_CTRL_T *pSrvCtrl = pConn->ppSrvCtrl[IDX_SRV_CTRL_PLAY_LIVE];

  pthread_mutex_lock(&pSrvCtrl->mtx);

  recordstop(pSrvCtrl);

  pthread_mutex_unlock(&pSrvCtrl->mtx);

  *plenout = 0;

  return 0;
}

#if defined(VSX_HAVE_SERVERMODE)

int srv_ctrl_oncommand(CLIENT_CONN_T *pConn, enum HTTP_STATUS *pHttpStatus,
                       unsigned char *pout, unsigned int *plenout) {
  int rc = -1;
  unsigned int idx;
  char tmp[128];
  int havehandler = 0;
  const char *pcmd = NULL;
  const KEYVAL_PAIR_T *pKeyvals = (const KEYVAL_PAIR_T *) &pConn->phttpReq->uriPairs;

  if(!pout || !plenout) {
    return -1;
  }

  if((pcmd = conf_find_keyval(pKeyvals, "cmd"))) {

    for(idx = 0; idx < sizeof(srv_cmd_handlers) / sizeof(srv_cmd_handlers[0]); idx++) {
      if(!strncasecmp((char *) srv_cmd_handlers[idx].strCmd, pcmd, 
        strlen(srv_cmd_handlers[idx].strCmd))) {
         havehandler = 1;
         rc = srv_cmd_handlers[idx].cbOnCmd(pConn, pKeyvals, &pout, plenout);
         break;
      }
    }

    if(!havehandler) {
      LOG(X_WARNING("Received unsupported command '%s' from  %s:%d"), 
          pcmd, FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));
      havehandler = 1;
      *plenout = snprintf((char *) pout, *plenout, "Unsupported command");
    }

  }

  if(!havehandler) {
    LOG(X_WARNING("Received unknown request from %s:%u '%s'"), 
          FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)), pConn->phttpReq->puri);
    *plenout = snprintf((char *) pout, *plenout, "No command argument found");
  }

  if(rc < 0) {
    *pHttpStatus = HTTP_STATUS_SERVERERROR;
  } else {
    *pHttpStatus = HTTP_STATUS_OK;
  }

  //Avoid Content-Length:0 or no Content-Length header in response
  if(*plenout == 0) {
    pout[0] = '\0';
    *plenout = 1;
  }

  return rc;
}

void srv_ctrl_proc(void *parg) {
  SRV_CTRL_PROC_DATA_T *pSrvCtrlData = (SRV_CTRL_PROC_DATA_T *) parg;
  SRV_CTRL_T *pSrvCtrl = pSrvCtrlData->ppSrvCtrl[pSrvCtrlData->idx];
  SRV_CTRL_T *pSrvCtrlPeer = NULL;
  int rc;
  enum SRV_CMD cmd = SRV_CMD_NONE;
  int startNext = 0;
  int progIdMin = pSrvCtrl->curCfg.streamerCfg.u.cfgmp2ts.firstProgId;
  int progIdRangeMax = 4;
  unsigned int idx;

  if(pSrvCtrlData->idx == 0) {
    pSrvCtrlPeer = pSrvCtrlData->ppSrvCtrl[1];
  } else {
    pSrvCtrlPeer = pSrvCtrlData->ppSrvCtrl[0];
  }

  while(!g_proc_exit && pSrvCtrl->isRunning) {

    pthread_mutex_lock(&pSrvCtrl->mtx);

    if(pSrvCtrl->curCfg.streamerCfg.pdestsCfg && pSrvCtrl->curCfg.streamerCfg.pxmitDestRc && 
       pSrvCtrl->nextCfg.streamerCfg.pdestsCfg && pSrvCtrl->nextCfg.streamerCfg.pxmitDestRc && 
       (startNext = pSrvCtrl->startNext)) {

      startNext = 1; 

      //
      // Do not copy the entire contents of nextCfg into curCfg 
      // to preserve non-dynamic elements, such as mtx 
      //
      // Note:  This is a shallow copy, so any internal pointer 
      // references will not need to refer to curCfg.
      //
      copy_srvctrl(&pSrvCtrl->curCfg, &pSrvCtrl->nextCfg);

/*
      memcpy(&pSrvCtrl->curCfg, 
             &pSrvCtrl->nextCfg, 
         ((char *)&pSrvCtrl->nextCfg.streamerCfg.status.useStatus) - 
         ((char *)&pSrvCtrl->nextCfg));

      memcpy(pSrvCtrl->curCfg.streamerCfg.pdestsCfg, pSrvCtrl->nextCfg.streamerCfg.pdestsCfg, 
             *pSrvCtrl->curCfg.streamerCfg.pmaxRtp * sizeof(STREAMER_DEST_CFG_T));
      memcpy(pSrvCtrl->curCfg.streamerCfg.pxmitDestRc, pSrvCtrl->nextCfg.streamerCfg.pxmitDestRc, 
             *pSrvCtrl->curCfg.streamerCfg.pmaxRtp * sizeof(int));
*/

      // the curCfg becomse the owner of any directory virtual playlist
      pSrvCtrl->nextCfg.pDirPlaylist = NULL;

      //auto preserve state of cfgmp2ts, cfgrtp.ssrc
      pSrvCtrl->curCfg.streamerCfg.cfgrtp.maxPayloadSz = 
                pSrvCtrl->nextCfg.streamerCfg.cfgrtp.maxPayloadSz;
      if(pSrvCtrl->nextCfg.reset_rtp_seq) {
        pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequence_nums[0] = pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequence_nums[1] = 0;
      } else {
        pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequence_nums[0] = pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequences_at_end[0];
        pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequence_nums[1] = pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequences_at_end[1];
      }

      pSrvCtrl->curCfg.streamerCfg.status.durationMs = 0;
      pSrvCtrl->curCfg.streamerCfg.status.locationMs = 0;
      memset(&pSrvCtrl->curCfg.streamerCfg.status.xmitBwDescr, 0,
             sizeof(pSrvCtrl->curCfg.streamerCfg.status.xmitBwDescr));

      //TODO: preserve RTP seq #

      pSrvCtrl->startNext = 0;
    }

    cmd = pSrvCtrl->curCfg.cmd;
    pthread_mutex_unlock(&pSrvCtrl->mtx);

    if(startNext) {

      pthread_mutex_lock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
      pSrvCtrl->curCfg.streamerCfg.running = STREAMER_STATE_STARTING;
      pSrvCtrl->curCfg.streamerCfg.quitRequested = 0;
      pthread_mutex_unlock(&pSrvCtrl->curCfg.streamerCfg.mtxStrmr);
      pSrvCtrl->lasterrcode = 0;

      if(cmd == SRV_CMD_PLAY_FILE) {  

        LOG(X_DEBUG("Starting playlist playout '%s' with playlist"), pSrvCtrl->curCfg.filepath);

        if(pSrvCtrl->curCfg.pDirPlaylist && pSrvCtrl->curCfg.pDirPlaylist->pCur) {
          rc = vsxlib_streamPlaylistCfg(pSrvCtrl->curCfg.pDirPlaylist,
                                        &pSrvCtrl->curCfg.streamerCfg,
                                        pSrvCtrl->curCfg.streamerCfg.loop, 
                                        pSrvCtrl->curCfg.streamerCfg.loop_pl,
                                        1);
        } else {

        LOG(X_DEBUG("Starting file playout '%s' with no playlist"), pSrvCtrl->curCfg.filepath);

          rc = vsxlib_streamFileCfg(pSrvCtrl->curCfg.filepath, 
                         pSrvCtrl->curCfg.fps, &pSrvCtrl->curCfg.streamerCfg);
        }

        if(rc == 0) {
          LOG(X_DEBUG("Playout ended '%s'"), pSrvCtrl->curCfg.filepath);
          pSrvCtrl->curCfg.streamerCfg.running = STREAMER_STATE_FINISHED;
        } else {
          snprintf(pSrvCtrl->lasterrstr, sizeof(pSrvCtrl->lasterrstr), 
                   "Unable to play content");  
          pSrvCtrl->lasterrcode = 1;
          pSrvCtrl->curCfg.streamerCfg.running = STREAMER_STATE_ERROR;
          LOG(X_ERROR("Unable to play '%s'"), pSrvCtrl->curCfg.filepath);
        }

        if(pSrvCtrl->curCfg.pDirPlaylist) {
          m3u_free(pSrvCtrl->curCfg.pDirPlaylist, 1);
          pSrvCtrl->curCfg.pDirPlaylist = NULL;
        }
        pSrvCtrl->curCfg.isplaylist = 0;

        //fprintf(stdout, "last file rtp seq:%u (live peer:%u)\n", pSrvCtrl->curCfg.streamerCfg.cfgrtp.sequence_at_end, pSrvCtrlPeer->curCfg.streamerCfg.cfgrtp.sequence_at_end);


      } else if(cmd == SRV_CMD_PLAY_LIVE) {

        LOG(X_DEBUG("Starting live playout"));

        //
        // Need to make sure a deep copy is performed
        //
        pSrvCtrl->curCfg.captureCfg.common.localAddrs[0] = pSrvCtrl->curCfg.filepath;
        for(idx = 0; idx < pSrvCtrl->curCfg.captureCfg.common.filt.numFilters; idx++) {
          pSrvCtrl->curCfg.captureCfg.common.filt.filters[idx].pCapAction = 
               &pSrvCtrl->curCfg.captureCfg.capActions[idx];
        }

        if((rc = vsxlib_streamLiveCaptureCfg(&pSrvCtrl->curCfg.captureCfg,
                                             &pSrvCtrl->curCfg.streamerCfg)) == 0) {

          LOG(X_DEBUG("Live playout ended"));
        } else {
          snprintf(pSrvCtrl->lasterrstr, sizeof(pSrvCtrl->lasterrstr), 
                   "Unable to play live content");  
          pSrvCtrl->lasterrcode = 1;
          pSrvCtrl->curCfg.streamerCfg.running = STREAMER_STATE_ERROR;
          LOG(X_ERROR("Unable to start live playout"));
        }

        if(pSrvCtrl->curCfg.streamerCfg.action.do_httplive) {
          httplive_close(pSrvCtrl->pCfg->pHttpLiveDatas[0]);
        }

      }

      if(pSrvCtrl->curCfg.streamerCfg.proto == STREAM_PROTO_MP2TS) {
        pSrvCtrl->curCfg.streamerCfg.u.cfgmp2ts.firstProgId += 2;
        if(progIdRangeMax > 0 && 
          pSrvCtrl->curCfg.streamerCfg.u.cfgmp2ts.firstProgId - progIdMin >
          progIdRangeMax) {
          pSrvCtrl->curCfg.streamerCfg.u.cfgmp2ts.firstProgId = progIdMin;
        }
      }

      pthread_mutex_lock(&pSrvCtrl->mtx);
      close_recording(pSrvCtrl);
      pSrvCtrl->curCfg.cmd = SRV_CMD_NONE;
      pthread_mutex_unlock(&pSrvCtrl->mtx);

    } else {
      usleep(50000);
    }

    startNext = 0;
  }

  LOG(X_WARNING("Server control thread exiting"));

}

#endif // VSX_HAVE_SERVERMODE

