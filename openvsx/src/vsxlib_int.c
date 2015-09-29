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

#define WARN_DEFAULT_CERT_KEY_MSG  "Please create your own server SSL DTLS certificate and key file.  The default certificate is not fit for production use!"
static int vsxlib_dtls_init_certs(DTLS_CFG_T *pDtlsCfg, const SRTP_CFG_T *pSrtpCfg, const char *descr);

const char *vsxlib_get_appnamestr(char *buf, unsigned int len) {
  snprintf(buf, len  > 0 ? len -1 : 0, VSX_APPNAME " " VSX_VERSION, BUILD_INFO_NUM);
  //snprintf(buf, len  > 0 ? len -1 : 0, "");
  return buf;
}
const char *vsxlib_get_appnamewwwstr(char *buf, unsigned int len) {
  snprintf(buf, len > 0 ? len -1 : 0, VSX_APPNAME);
  return buf;
}

int vsxlib_initlog(int verbosity, const char *logfile, const char *homedir,
                    unsigned int logmaxsz, unsigned int logrollmax, const char *tag) {

  int rc = 0;
  int loglevel = S_WARNING;
  const char *plogdir = NULL;
  int flags = 0;
  char buf[VSX_MAX_PATH_LEN];
  struct stat st;

  g_verbosity = verbosity;

  if(g_debug_flags != 0 && g_verbosity <= VSX_VERBOSITY_NORMAL) {
    verbosity++;
  }

  if(logfile && (!strcmp(logfile, "stderr") || !strcmp(logfile, "stdout"))) {
    logfile = NULL;
  }

  if(logfile) {

    //
    // Try to use [home dir]/[plogfile]
    //
    if(homedir && logfile[0] != DIR_DELIMETER && logfile[0] != '.') {
      mediadb_prepend_dir(homedir, logfile, buf, sizeof(buf));
    } else {
      strncpy(buf, logfile, sizeof(buf) - 1);
    }

    if(buf[0] != '\0') {

      LOG(X_DEBUG("Using log file '%s'"), buf);

      if((rc = mediadb_getdirlen(buf)) > 0) {
        buf[rc - 1] = '\0';
        plogdir = buf;
        if(fileops_stat(buf, &st) != 0) {
          mediadb_mkdirfull(NULL, buf);
        }
        logfile = &buf[rc];
      } else {
        logfile = &buf[0];
      }
    }

  }

  //fprintf(stderr, "LOG DIR:'%s' FILE:'%s' '%s' size:%d, roll:%d\n", plogdir, logfile, buf, logmaxsz, logrollmax);

  logutil_tid_init();
  logger_AddStderr(S_INFO, LOG_OUTPUT_DEFAULT_STDERR);

  if(verbosity >= VSX_VERBOSITY_NORMAL) {
    loglevel = MIN( (verbosity + S_WARNING), S_DEBUG_VERBOSE);
  }

  if(tag) {
    flags |= LOG_OUTPUT_PRINT_TAG;
  } else if(g_debug_flags) {
    flags |= LOG_OUTPUT_PRINT_TID;
  }

  flags |= LOG_OUTPUT_PRINT_TID;

  if(logfile) {
    flags |= LOG_OUTPUT_DEFAULT | LOG_FLAG_USELOCKING | LOG_OUTPUT_PRINT_TID | LOG_OUTPUT_PRINT_TAG;
    logger_SetFile(plogdir, logfile, logrollmax, logmaxsz, flags);
    //
    // Be advised when logging to file at debug, stderr is always info
    //
    logger_AddStderr(S_INFO, 0);
  } else {
    flags |= LOG_FLAG_USESTDERR | LOG_FLAG_FLUSHOUTPUT | LOG_OUTPUT_PRINT_SEV_ERROR | LOG_FLAG_USELOCKING;
    logger_Init(NULL, NULL, 0, 0, flags); 
    logger_AddStderr(loglevel, 0);
  }

  logger_SetLevel(loglevel);

  if(tag) {
    //
    // A valid log_tag is needed on the parent thread for any sub-threads tag to show in the log
    //
    logutil_tid_add(pthread_self(), tag);
  }

  if(logfile) {
    LOG(X_INFO("%s"), vsxlib_get_appnamestr(buf, sizeof(buf)));
  }

  return rc;
}


int vsxlib_alloc_destCfg(STREAMER_CFG_T *pStreamerCfg, int force) {

  if(!pStreamerCfg->pmaxRtp) {
    return -1;
  } else if(!force && pStreamerCfg->pdestsCfg && pStreamerCfg->pxmitDestRc) {
    return 0;
  }

  if(*pStreamerCfg->pmaxRtp <= 0) {
    *((unsigned int *) pStreamerCfg->pmaxRtp) = STREAMER_RTP_DEFAULT;
  }

  if(pStreamerCfg->pdestsCfg) {
    avc_free((void *) &pStreamerCfg->pdestsCfg);
  }
  if(!(pStreamerCfg->pdestsCfg = (STREAMER_DEST_CFG_T *)
                         avc_calloc(*pStreamerCfg->pmaxRtp, sizeof(STREAMER_DEST_CFG_T)))) {
    return -1;
  }
  
  if(pStreamerCfg->pxmitDestRc) {
    avc_free((void *) &pStreamerCfg->pxmitDestRc);
  }
  if(!(pStreamerCfg->pxmitDestRc = (int *) avc_calloc(*pStreamerCfg->pmaxRtp, sizeof(int)))) {
    avc_free((void *) &pStreamerCfg->pdestsCfg);
    return -1;
  }

  return 0;
}

void vsxlib_free_destCfg(STREAMER_CFG_T *pStreamerCfg) {

  if(pStreamerCfg->pdestsCfg) {
    avc_free((void *) &pStreamerCfg->pdestsCfg);
  }

  if(pStreamerCfg->pxmitDestRc) {
    avc_free((void *) &pStreamerCfg->pxmitDestRc);
  }
}

void vsxlib_reset_streamercfg(STREAMER_CFG_T *pStreamerCfg) {
  STREAM_XCODE_DATA_T *pDataV = NULL;
  STREAM_XCODE_DATA_T *pDataA = NULL;
  STREAM_XCODE_VID_UDATA_T *pUDataV = NULL;
  STREAM_XCODE_AUD_UDATA_T *pUDataA = NULL;
  PIP_LAYOUT_MGR_T *pPipLayout = NULL;
  STREAMER_CFG_T streamerCfgTmp;

  memcpy(&streamerCfgTmp, pStreamerCfg, sizeof(STREAMER_CFG_T));

  //pStreamerCfg->running = STREAMER_STATE_FINISHED;
  pStreamerCfg->proto = STREAM_PROTO_INVALID;
  pStreamerCfg->numDests = 0;
  pStreamerCfg->fStart = 0;
  pStreamerCfg->pSleepQs[0] = pStreamerCfg->pSleepQs[1] = NULL;
  memset(&pStreamerCfg->action, 0, sizeof(pStreamerCfg->action));
  memcpy(&pStreamerCfg->action.outfmtQCfg, &streamerCfgTmp.action.outfmtQCfg, 
         sizeof(pStreamerCfg->action.outfmtQCfg));
  memset(pStreamerCfg->sdpActions, 0, sizeof(pStreamerCfg->sdpActions));
  memset(pStreamerCfg->cbs, 0, sizeof(pStreamerCfg->cbs));
  pStreamerCfg->cfgrtp.ssrcs[0] = pStreamerCfg->cfgrtp.ssrcs[0] = 0;
  pStreamerCfg->novid = 0;
  pStreamerCfg->noaud = 0;

  memset(&pStreamerCfg->xcode, 0, sizeof(pStreamerCfg->xcode));
  pUDataV = pStreamerCfg->xcode.vid.pUserData = streamerCfgTmp.xcode.vid.pUserData;
  pUDataA = pStreamerCfg->xcode.aud.pUserData = streamerCfgTmp.xcode.aud.pUserData;
  pDataV = pUDataV->pVidData;
  pDataA = pUDataA->pAudData;
  pPipLayout = pUDataA->pPipLayout;
  memset(pUDataV, 0, sizeof(STREAM_XCODE_VID_UDATA_T));
  memset(pUDataA, 0, sizeof(STREAM_XCODE_AUD_UDATA_T));
  memset(pDataV, 0, sizeof(STREAM_XCODE_DATA_T)); 
  memset(pDataA, 0, sizeof(STREAM_XCODE_DATA_T)); 
  pUDataV->pVidData = pDataV;
  pUDataV->type = XC_CODEC_TYPE_VID_GENERIC;
  pUDataV->pStreamerFbReq = &pStreamerCfg->fbReq;
  pUDataV->pStreamerPip = &pStreamerCfg->pip;
  pUDataA->pAudData = pDataA;
  pUDataA->type = XC_CODEC_TYPE_AUD_GENERIC;
  pUDataA->pPipLayout = pPipLayout;
  pStreamerCfg->pXcodeCtxt = streamerCfgTmp.pXcodeCtxt;

  memset(pStreamerCfg->sdpsx, 0, sizeof(pStreamerCfg->sdpsx));
  pStreamerCfg->running = STREAMER_STATE_FINISHED;
  pStreamerCfg->quitRequested = 0;
  pStreamerCfg->pauseRequested = 0;
  pStreamerCfg->pMonitor = NULL;

  pStreamerCfg->sharedCtxt.active = 0;
  pStreamerCfg->sharedCtxt.state = 0;
  pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt = NULL;
  memset(pStreamerCfg->sharedCtxt.av, 0, sizeof(pStreamerCfg->sharedCtxt.av));

}

static void reset_sdp_media(SDP_STREAM_DESCR_T *pMedia) {
  memset(&pMedia->srtp, 0, sizeof(pMedia->srtp));
  memset(&pMedia->rtcpfb, 0, sizeof(pMedia->rtcpfb));
  memset(&pMedia->ice, 0, sizeof(pMedia->ice));
  memset(pMedia->attribCustom, 0, sizeof(pMedia->attribCustom));
  pMedia->avpf = 0;
}

static void reset_sdp(SDP_DESCR_T *pSdp) {
  reset_sdp_media(&pSdp->vid.common);
  reset_sdp_media(&pSdp->aud.common);
}

void vsxlib_set_sdp_from_capture(STREAMER_CFG_T *pStreamerCfg, const SDP_DESCR_T *pSdp) {
  unsigned int idx;

  if(pSdp->vid.common.codecType == MEDIA_FILE_TYPE_MP2TS ||
     pSdp->vid.common.codecType == MEDIA_FILE_TYPE_MP2TS_BDAV) {
    memcpy(&pStreamerCfg->sdpsx[0][0], pSdp, sizeof(pStreamerCfg->sdpsx[0][0]));
  } else {
    memcpy(&pStreamerCfg->sdpsx[1][0], pSdp, sizeof(pStreamerCfg->sdpsx[1][0]));
  }

  memcpy(&pStreamerCfg->sdpInput, pSdp, sizeof(pStreamerCfg->sdpInput));
  //sdp_write(&pStreamerCfg->sdpInput, NULL, S_DEBUG);

  //
  // Reset any SDP parameters which should not be preserved from the input SDP
  //
  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
    reset_sdp(&pStreamerCfg->sdpsx[0][idx]);
    reset_sdp(&pStreamerCfg->sdpsx[1][idx]);
  }

  for(idx = 1; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
    memcpy(&pStreamerCfg->sdpsx[0][idx], &pStreamerCfg->sdpsx[0][0],
           sizeof(pStreamerCfg->sdpsx[1][0]));
    memcpy(&pStreamerCfg->sdpsx[1][idx], &pStreamerCfg->sdpsx[1][0],
           sizeof(pStreamerCfg->sdpsx[1][0]));
  }

  if(!pStreamerCfg->xcode.vid.common.cfgDo_xcode && !pSdp->vid.common.available) {
    LOG(X_DEBUGV("Disabling video output"));
    pStreamerCfg->novid = 1;
  }
  if(!pStreamerCfg->xcode.aud.common.cfgDo_xcode && !pSdp->aud.common.available &&
     !(pSdp->vid.common.codecType == MEDIA_FILE_TYPE_MP2TS ||
      pSdp->vid.common.codecType == MEDIA_FILE_TYPE_MP2TS_BDAV)) {
    LOG(X_DEBUGV("Disabling audio output"));
    pStreamerCfg->noaud = 1;
  }

  //
  // Enable FIR requests from the capture processor if the SDP contains a=rtcp-fb:* ccm fir
  // alternatively this will be already set for '--firxmit=1'
  //
  if((pSdp->vid.common.rtcpfb.flags) & SDP_RTCPFB_TYPE_CCM_FIR) {
    pStreamerCfg->fbReq.firCfg.fir_send_from_capture = 1;
  }

  //
  // WebRTC RTCP APP specific Receiver Estimated Max Bandwidth
  //
  if((pSdp->vid.common.rtcpfb.flags) & SDP_RTCPFB_TYPE_APP_REMB) {
    if(BOOL_ISDISABLED(pStreamerCfg->fbReq.apprembRtcpSend)) {
      LOG(X_DEBUG("RTCP APP REMB notifications for video media are disabled"));
    }
  } else {
    pStreamerCfg->fbReq.apprembRtcpSend = 0;
  }

  //
  // Setup RTCP NACK feedback array for received RTP packets 
  //
  if((pSdp->vid.common.rtcpfb.flags & (SDP_RTCPFB_TYPE_NACK | SDP_RTCPFB_TYPE_NACK_GENERIC))) {
    if(BOOL_ISDISABLED(pStreamerCfg->fbReq.nackRtcpSend)) {
      LOG(X_DEBUG("RTCP NACK requests for video media are disabled"));
    }
  } else {
    //
    // Disable RTCP NACK requests since the SDP video does not contain the attribute 'a=rtcp-fb:* nack' 
    //
    pStreamerCfg->fbReq.nackRtcpSend = 0;
  }

}

int vsxlib_cond_broadcast(pthread_cond_t *cond, pthread_mutex_t *mtx) {
  int rc;

  pthread_mutex_lock(mtx);

  rc = pthread_cond_broadcast(cond);

  pthread_mutex_unlock(mtx);

  return rc;
}

int vsxlib_cond_signal(pthread_cond_t *cond, pthread_mutex_t *mtx) {
  int rc;

  pthread_mutex_lock(mtx);

  rc = pthread_cond_signal(cond);

  pthread_mutex_unlock(mtx);

  return rc;
}

int vsxlib_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx) {
  int rc;

  pthread_mutex_lock(mtx);

  rc = pthread_cond_wait(cond, mtx);

  pthread_mutex_unlock(mtx);

  return rc;
}

int vsxlib_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mtx, struct timespec *pabstime) {
  int rc;

  pthread_mutex_lock(mtx);

  rc = pthread_cond_timedwait(cond, mtx, pabstime);

  pthread_mutex_unlock(mtx);

  return rc;
}

int vsxlib_cond_waitwhile(PKTQUEUE_COND_T *pCond, int *pflag, int value) {
  int rc = 0;
  struct timeval tv;
  struct timespec ts;

  if(!pCond) {
    return -1;
  }

  while(!g_proc_exit && (!pflag || *pflag == value)) {

    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec + 1;
    ts.tv_nsec = tv.tv_usec * 1000;

    if((rc = vsxlib_cond_timedwait(&pCond->cond, &pCond->mtx, &ts)) < 0) {
      LOG(X_ERROR("vsxlib_cond_timedwait failed: %d"), rc);
      break;
    } else if(rc == 0) {
      //LOG(X_DEBUG("COND_WAITWHILE... rc:%d break..."), rc);
      break;
    }
    //LOG(X_DEBUG("COND_WAITWHILE... rc:%d"), rc);
  } 

  return rc;
}


void vsxlib_setsrvconflimits(VSXLIB_STREAM_PARAMS_T *pParams,
                              unsigned int rtmphardlimit, unsigned int rtsphardlimit,
                              unsigned int flvhardlimit, unsigned int mkvhardlimit, 
                              unsigned int httphardlimit) {

  if(!pParams) {
    return;
  }

  //
  // Global HTTP settings
  //
  if(pParams->httpmax > VSX_CONNECTIONS_MAX) {
    LOG(X_WARNING("Max HTTP sessions limited to %d"), VSX_CONNECTIONS_MAX);
    pParams->httpmax = VSX_CONNECTIONS_MAX;
  }

  //
  // /httplive callback is limited by number of HTTP listeners
  //
  if(pParams->httplivemax > VSX_CONNECTIONS_MAX) {
    LOG(X_WARNING("Max HTTPLive sessions limited to %d"), VSX_CONNECTIONS_MAX);
    pParams->httplivemax = VSX_CONNECTIONS_MAX;
  }

  //
  // /dash callback is limited by number of HTTP listeners
  //
  if(pParams->dashlivemax > VSX_CONNECTIONS_MAX) {
    LOG(X_WARNING("Max MOOF sessions limited to %d"), VSX_CONNECTIONS_MAX);
    pParams->dashlivemax = VSX_CONNECTIONS_MAX;
  }

  //
  // /live URL is limited by number of HTTP listeners
  //
  if(pParams->livemax > VSX_CONNECTIONS_MAX) {
    LOG(X_WARNING("Max HTTP sessions limited to %d"), VSX_CONNECTIONS_MAX);
    pParams->livemax = VSX_CONNECTIONS_MAX;
  } else if(pParams->livemax == 0) {
    pParams->livemax = 4;
  }


  //
  // /pip URL is limited by number of HTTP listeners
  //
  if(pParams->piphttpmax > VSX_CONNECTIONS_MAX ||
     (pParams->pipaddr[0] && pParams->piphttpmax == 0)) {
    pParams->piphttpmax = 2;
  }
  if(pParams->piphttpmax > 0 && (!pParams->pipaddr[0] || pParams->pipaddr[0][0] == '\0')) {
    pParams->pipaddr[0] = HTTP_LISTEN_PORT_STR;
  }

  //
  // /status URL is limited by number of HTTP listeners
  //
  if(pParams->statusmax > VSX_CONNECTIONS_MAX ||
     (pParams->statusaddr[0] && pParams->statusmax == 0)) {
    pParams->statusmax = 2;
  }
  if(pParams->statusmax > 0 && (!pParams->statusaddr[0] || pParams->statusaddr[0][0] == '\0')) {
    pParams->statusaddr[0] = HTTP_LISTEN_PORT_STR;
  }

  //
  // /config URL is limited by number of HTTP listeners
  //
  if(pParams->configmax > VSX_CONNECTIONS_MAX ||
     (pParams->configaddr[0] && pParams->configmax == 0)) {
    pParams->configmax = 2;
  }
  if(pParams->configmax > 0 && (!pParams->configaddr[0] || pParams->configaddr[0][0] == '\0')) {
    pParams->configaddr[0] = HTTP_LISTEN_PORT_STR;
  }

  //
  // /tslive URL is limited by number of HTTP listeners
  // as well as number of liveQ available connection threads
  //
  if(pParams->tslivemax > MIN(STREAMER_LIVEQ_MAX, VSX_CONNECTIONS_MAX)) {
    LOG(X_WARNING("Max MPEG-2 TS HTTP sessions limited to %d"), 
                MIN(STREAMER_LIVEQ_MAX, VSX_CONNECTIONS_MAX));
    pParams->tslivemax = MIN(STREAMER_LIVEQ_MAX, VSX_CONNECTIONS_MAX);
  }

  if(pParams->tsliveq_slots <= 0) {
    pParams->tsliveq_slots = STREAMER_TSLIVEQ_SLOTS_DEFAULT;
  }
  if(pParams->tsliveq_slotsmax <= 0) {
    pParams->tsliveq_slotsmax = STREAMER_TSLIVEQ_SLOTS_MAX;
  }
  if(pParams->tsliveq_szslot <= 0) {
    pParams->tsliveq_szslot = STREAMER_TSLIVEQ_SZSLOT_DEFAULT;
  }

  //
  // RTP output accounts for both audio and video channels
  // --transport=rtp, and --transport=m2t do not use the same session resource
  //
  if(pParams->rtplivemax > STREAMER_RTP_MAX) {
    LOG(X_WARNING("Max RTP streams limited to %d"), STREAMER_RTP_MAX);
    pParams->rtplivemax = STREAMER_RTP_MAX;
  } else {
    if(pParams->rtplivemax == 0) {
      pParams->rtplivemax =  STREAMER_RTP_DEFAULT;
    }
    if(pParams->rtplivemax != STREAMER_RTP_DEFAULT) {
      LOG(X_DEBUG("Max RTP streams set to %d"), pParams->rtplivemax);
    }
  }

  //
  // /rtmp URL is limited by the number of RTMP port listeners
  //
  if(pParams->rtmplivemax > rtmphardlimit) {
    LOG(X_WARNING("Max RTMP sessions limited to %d"), rtmphardlimit);
    pParams->rtmplivemax = rtmphardlimit;
  }

  if(pParams->rtmpq_slots <= 0) {
    pParams->rtmpq_slots = STREAMER_RTMPQ_SLOTS_DEFAULT;
  }
  if(pParams->rtmpq_szslot <= 0) {
    pParams->rtmpq_szslot = STREAMER_RTMPQ_SZSLOT_DEFAULT;
  }
  if(pParams->rtmpq_szslotmax == 0) {
    pParams->rtmpq_szslotmax = STREAMER_RTMPQ_SZSLOT_MAX;
  } else if(pParams->rtmpq_szslotmax > STREAMER_RTMPQ_SZSLOT_MAX_LIMIT) {
    pParams->rtmpq_szslotmax = STREAMER_RTMPQ_SZSLOT_MAX_LIMIT;
  }

  //
  // /flv URL is limited by the number of FLV port listeners
  //
  if(pParams->flvlivemax > flvhardlimit) {
    LOG(X_WARNING("Max FLV sessions limited to %d"), flvhardlimit);
    pParams->flvlivemax = flvhardlimit;
  }

  //
  // /mkv URL is limited by the number of MKV port listeners
  //
  if(pParams->mkvlivemax > mkvhardlimit) {
    LOG(X_WARNING("Max MKV sessions limited to %d"), mkvhardlimit);
    pParams->mkvlivemax = mkvhardlimit;
  }

  //
  // /rtsp URL is limited by the number of RTSP port listeners
  // RTSP sessions can use RTP output sessions for UDP/RTP transport
  // or its own channel for tcp interleaved output
  //
  if(pParams->rtsplivemax > rtsphardlimit) {
    LOG(X_WARNING("Max RTSP sessions limited to %d"), rtsphardlimit);
    pParams->rtsplivemax = rtsphardlimit;
  //} else if(pParams->rtsplivemax == 0) {
  //  pParams->rtsplivemax = STREAMER_LIVEQ_DEFAULT;
  }

  if(pParams->rtspliveaddr[0] && pParams->rtplivemax < pParams->rtsplivemax) {
    LOG(X_WARNING("Configuration '%s=%d' (--rtpmax=%d) RTP sessions is below '%s=%d' (--rtspmax=%d)"), 
        SRV_CONF_KEY_RTPMAX, pParams->rtplivemax, pParams->rtplivemax, 
        SRV_CONF_KEY_RTSPLIVEMAX, pParams->rtsplivemax, pParams->rtsplivemax);
  }

  if(pParams->rtspinterq_slots <= 0) {
    pParams->rtspinterq_slots = STREAMER_RTSPINTERQ_SLOTS_DEFAULT;
  }
  if(pParams->rtspinterq_szslot <= 0) {
    pParams->rtspinterq_szslot = STREAMER_RTSPINTERQ_SZSLOT_DEFAULT;
  }
  if(pParams->rtspinterq_szslotmax <= 0) {
    pParams->rtspinterq_szslotmax = STREAMER_RTSPINTERQ_SZSLOT_MAX;
  } else if(pParams->rtspinterq_szslotmax >  STREAMER_RTSPINTERQ_SZSLOT_MAX) {
    pParams->rtspinterq_szslotmax = STREAMER_RTSPINTERQ_SZSLOT_MAX;
  }

  if(!pParams->httplivedir) {
    pParams->httplivedir = VSX_HTTPLIVE_PATH;
  }

  if(!pParams->moofdir) {
   pParams->moofdir = VSX_MOOF_PATH;
  }

  //if(!pParams->mooffileprefix) {
  //  pParams->mooffileprefix = DASH_DEFAULT_NAME_PRFX;
  //}

}

SRV_CONF_T *vsxlib_loadconf(VSXLIB_STREAM_PARAMS_T *pParams) {
  SRV_CONF_T *pConf = NULL;
  const char *parg;
  unsigned int ui;
  float f;

  if(!pParams || !pParams->confpath) {
    return NULL;
  }

  //
  // Parse the configuration file
  //
  if((pConf = conf_parse(pParams->confpath)) == NULL) {
    return NULL;
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_VERBOSE))) {
    if((ui = abs(atoi(parg)) + 1) > (unsigned int) pParams->verbosity) {
      pParams->verbosity = ui;
    }
  }

  //
  // Logger settings
  //
  if(pParams->logfile == NULL) {
    pParams->logfile = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LOGFILE);
  }
  if((pParams->logmaxsz == 0 || pParams->logmaxsz == LOGGER_MAX_FILE_SZ) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LOGFILE_MAXSZ))) {
    pParams->logmaxsz = atoi(parg);
  }
  if((pParams->logrollmax == 0 || pParams->logrollmax == LOGGER_MAX_FILES) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LOGFILE_COUNT))) {
    pParams->logrollmax = atoi(parg);
  }


  //
  // Get any xcoder args
  //
  if(!pParams->strxcode &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_XCODEARGS))) {
    pParams->strxcode = parg;
  }

  //
  // Global HTTP settings
  //
  if((pParams->httpmax == 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAXCONN))) {
    pParams->httpmax = atoi(parg);
  }

  //
  // Get live auto-detect server broadcast config settings
  //
  conf_load_addr_multi(pConf, pParams->liveaddr, sizeof(pParams->liveaddr) / 
                     sizeof(pParams->liveaddr[0]), SRV_CONF_KEY_LIVE);

  if((pParams->livemax == 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LIVEMAX))) {
    pParams->livemax = atoi(parg);
  }

  if(!pParams->livepwd &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LIVEPWD))) {
    pParams->livepwd = parg;
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_ENABLESYMLINK))) {
    pParams->enable_symlink = atoi(parg);
  }

  //TODO: get multiple output indexes

  //
  // Get tslive broadcast config settings
  //
  conf_load_addr_multi(pConf, pParams->tsliveaddr, sizeof(pParams->tsliveaddr) / 
                     sizeof(pParams->tsliveaddr[0]), SRV_CONF_KEY_TSLIVE);

  if((pParams->tslivemax == 0 || pParams->tslivemax == STREAMER_LIVEQ_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_TSLIVEMAX))) {
    pParams->tslivemax = atoi(parg);
  }

  if(pParams->tsliveq_slots == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_TSLIVEQSLOTS))) {
    pParams->tsliveq_slots = atoi(parg);
  }
  if(pParams->tsliveq_slotsmax == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_TSLIVEQSLOTSMAX))) {
    pParams->tsliveq_slotsmax = atoi(parg);
  }

  if(pParams->tsliveq_szslot == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_TSLIVEQSZSLOT))) {
    pParams->tsliveq_szslot = atoi(parg);
  }

  //
  // Get (fragmented) mp4 moof / dash broadcast config settings
  //
  conf_load_addr_multi(pConf, pParams->dashliveaddr, sizeof(pParams->dashliveaddr) /
                     sizeof(pParams->dashliveaddr[0]), SRV_CONF_KEY_MOOFLIVE);
  
  if(pParams->dashliveaddr[0] && pParams->dash_moof_segments == -1) {
    pParams->dash_moof_segments = 1;
  }
  //for(ui=0; ui< sizeof(pParams->dashliveaddr) / sizeof(pParams->dashliveaddr[0]); ui++) {
  //  if(pParams->dashliveaddr[ui]) {
  //    for(ui=0; ui < IXCODE_VIDEO_OUT_MAX; ui++) {
  //      pParams->dashrecord[ui] = "1";
  //    }
  //    break;
  //  }
  //}

  if(!pParams->moofdir &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFDIR)) &&
    parg[0] != '\0') {
    pParams->moofdir = parg;
  }

  if(!pParams->mooffileprefix &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFFILEPREFIX))) {
    pParams->mooffileprefix = parg;
  }

  if((pParams->moofindexcount <= 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFINDEXCOUNT))) {
    pParams->moofindexcount = atoi(parg);
  }

  if(!pParams->moofuriprefix &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFURLPREFIX))) {
    pParams->moofuriprefix = parg;
  }

  if((pParams->dashlivemax == 0 || pParams->dashlivemax == STREAMER_LIVEQ_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFLIVEMAX))) {
    pParams->dashlivemax = atoi(parg);
  }

  if((pParams->moof_nodelete <= 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFNODELETE))) {
    pParams->moof_nodelete = atoi(parg);
  }

  if((pParams->moofMp4MinDurationSec == MOOF_MP4_MIN_DURATION_SEC_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFMINDURATION))) {
    if((f = (float) atof(parg)) >= 0) {
      pParams->moofMp4MinDurationSec = atoi(parg);
    }
  }

  if((pParams->moofMp4MaxDurationSec == MOOF_MP4_MAX_DURATION_SEC_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFMAXDURATION))) {
    if((f = (float) atof(parg)) >= 0) {
      pParams->moofMp4MaxDurationSec = atoi(parg);
    }
  }
  if((pParams->moofTrafMaxDurationSec == MOOF_TRAF_MAX_DURATION_SEC_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFFRAGDURATION))) {
    if((f = (float) atof(parg)) >= 0) {
      pParams->moofTrafMaxDurationSec = atoi(parg);
    }
  }

  if((pParams->moofUseInitMp4 == MOOF_USE_INITMP4_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MOOFUSEINIT))) {
    pParams->moofUseInitMp4 = atoi(parg);
  }


  //
  // Get httplive broadcast config settings
  //
  conf_load_addr_multi(pConf, pParams->httpliveaddr, sizeof(pParams->httpliveaddr) / 
                     sizeof(pParams->httpliveaddr[0]), SRV_CONF_KEY_HTTPLIVE);

  if(!pParams->httplivedir &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVEDIR)) &&
    parg[0] != '\0') {
    pParams->httplivedir = parg;
  }

  if((pParams->httplive_chunkduration <= 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVEDURATION))) {
    if((f = (float) atof(parg)) > 0) {
      pParams->httplive_chunkduration = f;
    }
  }

  if((pParams->httpliveindexcount <= 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVEINDEXCOUNT))) {
    pParams->httpliveindexcount = atoi(parg);
  }

  if((pParams->httplive_nodelete <= 0) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVENODELETE))) {
    pParams->httplive_nodelete = atoi(parg);
  }

  if(!pParams->httplivefileprefix &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVEFILEPREFIX))) {
    pParams->httplivefileprefix = parg;
  }

  if(!pParams->httpliveuriprefix &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVEURLPREFIX))) {
    pParams->httpliveuriprefix = parg;
  }

  if((pParams->httplivemax == 0 || pParams->httplivemax == STREAMER_LIVEQ_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HTTPLIVEMAX))) {
    pParams->httplivemax = atoi(parg);
  }

  //
  // Get rtp config settings
  //
  if((pParams->rtplivemax <= 1 || pParams->rtplivemax == STREAMER_RTP_DEFAULT) && 
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTPMAX))) {
    pParams->rtplivemax = atoi(parg);
  }

  if((pParams->capture_idletmt_ms == 0 ||
      pParams->capture_idletmt_ms == STREAM_CAPTURE_IDLETMT_MS) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_CAP_IDLETMT_MS))) {
    pParams->capture_idletmt_ms = atoi(parg);
  }

  if((pParams->capture_idletmt_1stpkt_ms == 0 ||
      pParams->capture_idletmt_1stpkt_ms == STREAM_CAPTURE_IDLETMT_1STPKT_MS) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_CAP_IDLETMT_1STPKT_MS))) {
    pParams->capture_idletmt_1stpkt_ms = atoi(parg);
  }

  if((pParams->capture_max_rtpplayoutviddelayms == 0 ||
      pParams->capture_max_rtpplayoutviddelayms == CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTPMAXVIDPLAYOUTDELAY))) {
    pParams->capture_max_rtpplayoutviddelayms = atoi(parg);
  }

  if((pParams->capture_max_rtpplayoutauddelayms == 0 ||
      pParams->capture_max_rtpplayoutauddelayms == CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTPMAXAUDPLAYOUTDELAY))) {
    pParams->capture_max_rtpplayoutauddelayms = atoi(parg);
  }

  if(((pParams->capture_max_rtpplayoutviddelayms == 0 ||
       pParams->capture_max_rtpplayoutviddelayms == CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS) ||
      (pParams->capture_max_rtpplayoutauddelayms == 0 ||
       pParams->capture_max_rtpplayoutauddelayms == CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS)) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTPMAXPLAYOUTDELAY))) {

    if((pParams->capture_max_rtpplayoutviddelayms == 0 ||
      pParams->capture_max_rtpplayoutviddelayms == CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS)) {
      pParams->capture_max_rtpplayoutviddelayms = atoi(parg);
    }

    if((pParams->capture_max_rtpplayoutauddelayms == 0 ||
      pParams->capture_max_rtpplayoutauddelayms == CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS)) {
      pParams->capture_max_rtpplayoutauddelayms = atoi(parg);
    }
  }

  if(pParams->frtcp_rr_intervalsec == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTCP_RR_INTERVAL))) {
      if((f = (float) atof(parg)) >= 0) {
        pParams->frtcp_rr_intervalsec = atoi(parg);
      }
  }

  if((pParams->frtcp_sr_intervalsec == 0 ||
      pParams->frtcp_sr_intervalsec == STREAM_RTCP_SR_INTERVAL_SEC) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTCP_SR_INTERVAL))) {
      if((f = (float) atof(parg)) >= 0) {
        pParams->frtcp_sr_intervalsec = atoi(parg);
      }
  }

  if(BOOL_ISDFLT(pParams->rtcp_reply_from_mcast) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTCP_MCAST_REPLIES))) {
    pParams->rtcp_reply_from_mcast = MAKE_BOOL(atoi(parg));
  }

  //
  // Get FIR settings
  //

  if(BOOL_ISDFLT(pParams->firCfg.fir_encoder) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_ENCODER))) {
    pParams->firCfg.fir_encoder = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_recv_via_rtcp) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_RECV_VIA_RTCP))) {
    pParams->firCfg.fir_recv_via_rtcp = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_recv_from_connect) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_RECV_FROM_CONNECT))) {
    pParams->firCfg.fir_recv_from_connect = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_recv_from_remote) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_RECV_FROM_REMOTE))) {
    pParams->firCfg.fir_recv_from_remote = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_send_from_local) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_SEND_FROM_LOCAL))) {
    pParams->firCfg.fir_send_from_local = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_send_from_remote) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_SEND_FROM_REMOTE))) {
    pParams->firCfg.fir_send_from_remote = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_send_from_decoder) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_SEND_FROM_DECODER))) {
    pParams->firCfg.fir_send_from_decoder = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->firCfg.fir_send_from_capture) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FIR_SEND_FROM_CAPTURE))) {
    pParams->firCfg.fir_send_from_capture = MAKE_BOOL(atoi(parg));
  }

  //
  // Get RTCP NACK settings
  //

  if(BOOL_ISDFLT(pParams->nackRtcpSendVideo) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_NACK_RTCP_REQUEST))) {
    pParams->nackRtcpSendVideo = MAKE_BOOL(atoi(parg));
  }
  if(BOOL_ISDFLT(pParams->nackRtpRetransmitVideo ) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_NACK_RTP_RETRANSMIT))) {
    pParams->nackRtpRetransmitVideo = MAKE_BOOL(atoi(parg));
  }

  //
  // Get RTCP APP-specific REMB settings
  //

  if(BOOL_ISDFLT(pParams->apprembRtcpSendVideo) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_APPREMB_RTCP_NOTIFY))) {
    pParams->apprembRtcpSendVideo = MAKE_BOOL(atoi(parg));
  }
  if(pParams->apprembRtcpSendVideoMaxRateBps == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_APPREMB_RTCP_MAXRATE))) {
    pParams->apprembRtcpSendVideoMaxRateBps =  (unsigned int) strutil_read_numeric(parg, 0, 1000, 0);
  }
  if(pParams->apprembRtcpSendVideoMinRateBps == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_APPREMB_RTCP_MINRATE))) {
    pParams->apprembRtcpSendVideoMinRateBps =  (unsigned int) strutil_read_numeric(parg, 0, 1000, 0);
  }
  if(BOOL_ISDFLT(pParams->apprembRtcpSendVideoForceAdjustment) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_APPREMB_RTCP_FORCE))) {
    pParams->apprembRtcpSendVideoForceAdjustment = MAKE_BOOL(atoi(parg));
  }

  //
  // Get Frame thinning settings
  //
  if(BOOL_ISDFLT(pParams->frameThin) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FRAME_THIN))) {
    pParams->frameThin = MAKE_BOOL(atoi(parg));
  }

  //
  // Get the RTMP config settings
  // 
  if((pParams->rtmplivemax == 0 || pParams->rtmplivemax == STREAMER_OUTFMT_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTMPLIVEMAX))) {
    pParams->rtmplivemax = atoi(parg);
  }

  conf_load_addr_multi(pConf, pParams->rtmpliveaddr, sizeof(pParams->rtmpliveaddr) / 
                     sizeof(pParams->rtmpliveaddr[0]), SRV_CONF_KEY_RTMPLIVE);

  if(pParams->rtmpq_slots == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTMPQSLOTS))) {
    pParams->rtmpq_slots = atoi(parg);
  }

  if(pParams->rtmpq_szslot == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTMPQSZSLOT))) {
    pParams->rtmpq_szslot = atoi(parg);
  }

  if(pParams->rtmpq_szslotmax == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTMPQSZSLOTMAX))) {
    pParams->rtmpq_szslotmax = atoi(parg);
  }

  //
  // Get the FLV config settings
  // 
  if((pParams->flvlivemax == 0 || pParams->flvlivemax == STREAMER_OUTFMT_DEFAULT)  &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FLVLIVEMAX))) {
    pParams->flvlivemax = atoi(parg);
  }

  conf_load_addr_multi(pConf, pParams->flvliveaddr, sizeof(pParams->flvliveaddr) / 
                     sizeof(pParams->flvliveaddr[0]), SRV_CONF_KEY_FLVLIVE);

  //
  // Get the MKV config settings
  // 
  conf_load_addr_multi(pConf, pParams->mkvliveaddr, sizeof(pParams->mkvliveaddr) / 
                     sizeof(pParams->mkvliveaddr[0]), SRV_CONF_KEY_MKVLIVE);

  if((pParams->mkvlivemax == 0 || pParams->mkvlivemax == STREAMER_OUTFMT_DEFAULT)  &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MKVLIVEMAX))) {
    pParams->mkvlivemax = atoi(parg);
  }

  //
  // Get the RTSP config settings
  // 
  if((pParams->rtsplivemax == 0 || pParams->rtsplivemax == STREAMER_LIVEQ_DEFAULT) &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPLIVEMAX))) {
    pParams->rtsplivemax = atoi(parg);
  }

  conf_load_addr_multi(pConf, pParams->rtspliveaddr, sizeof(pParams->rtspliveaddr) / 
                     sizeof(pParams->rtspliveaddr[0]), SRV_CONF_KEY_RTSPLIVE);

  if(pParams->rtspinterq_slots == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPINTERQSLOTS))) {
    pParams->rtspinterq_slots = atoi(parg);
  }

  if(pParams->rtspinterq_szslot == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPINTERQSZSLOT))) {
    pParams->rtspinterq_szslot = atoi(parg);
  }

  if(pParams->rtspinterq_szslotmax == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPINTERQSZSLOTMAX))) {
    pParams->rtspinterq_szslotmax = atoi(parg);
  }

  if(pParams->rtspsessiontimeout == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPSESSIONTMT))) {
    pParams->rtspsessiontimeout = (unsigned int) atoi(parg);
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPRTCPREFRESH))) {
    pParams->rtsprefreshtimeoutviartcp = atoi(parg);
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPLOCALRTPPORT))) {
    pParams->rtsplocalport = atoi(parg);
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_RTSPFORCETCPUALIST))) {
    pParams->rtspualist = parg;
  }

  if((!pParams->devconfpath || pParams->devconfpath[0] == '\0') &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DEVICE_CONFIG))) {
    pParams->devconfpath = parg;
  }

  //
  // Stream output statistics 
  //
  if(!pParams->statfilepath  &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_STREAMSTATSPATH))) {
    pParams->statfilepath = parg;
  }

  if(pParams->statdumpintervalms == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_STREAMSTATSINTERVALMS))) {
    pParams->statdumpintervalms = (unsigned int) atoi(parg);
  }

  //
  //  Check and override local ip
  //

  if(!pParams->strlocalhost && (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_IPADDRHOST))) {
    pParams->strlocalhost = parg;
/*
    if(net_setlocaliphost(parg) < 0) {
      LOG(X_ERROR("Unable to use specified ip/host %s"), parg);
    } else {
      LOG(X_DEBUG("Using local hostname %s"), parg);
    }
*/
  }


  //
  // Packetization config
  //
  if(pParams->m2t_maxpcrintervalms == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_M2T_PCR_MAX_MS))) {
    pParams->m2t_maxpcrintervalms = atoi(parg);
  }

  if(pParams->m2t_maxpatintervalms == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_M2T_PAT_MAX_MS))) {
    pParams->m2t_maxpatintervalms = atoi(parg);
  }

  if(pParams->m2t_xmitflushpkts == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_M2T_XMITFLUSHPKTS))) {
    pParams->m2t_xmitflushpkts = atoi(parg);
  }

  //
  // Queue configs
  //
  if(pParams->pktqslots == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_M2TPKTQSLOTS))) {
    pParams->pktqslots = atoi(parg);
  }
  if(pParams->frvidqslots == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FRVIDQSLOTS))) {
    pParams->frvidqslots = atoi(parg);
  }
  if(pParams->fraudqslots == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_FRAUDQSLOTS))) {
    pParams->fraudqslots = atoi(parg);
  }

  //
  // SSL configs
  //
  if(!pParams->sslcertpath &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_SSLCERTPATH))) {
    pParams->sslcertpath = parg;
  }

  if(!pParams->sslprivkeypath &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_SSLPRIVKEYPATH))) {
    pParams->sslprivkeypath = parg;
  }

  //
  // DTLS configs
  //
  if(!pParams->srtpCfgs[0].dtlscertpath &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DTLSCERTPATH))) {
    pParams->srtpCfgs[0].dtlscertpath = parg;
  }

  if(!pParams->srtpCfgs[0].dtlsprivkeypath &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DTLSPRIVKEYPATH))) {
    pParams->srtpCfgs[0].dtlsprivkeypath = parg;
  }

  //
  // DTLS handshake timeout configs
  //
  if(pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtpTimeoutMs == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DTLSRTPHANDSHAKETMTMS))) {
    pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtpTimeoutMs = atoi(parg);
  }

  if(pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtcpAdditionalMs == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DTLSRTCPADDITIONALMS))) {
    pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtcpAdditionalMs = atoi(parg);
  }

  if(pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtcpAdditionalGiveupMs == 0 &&
     (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DTLSRTCPADDITIONALGIVEUPMS))) {
    pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtcpAdditionalGiveupMs = atoi(parg);
  }


  //
  // Get the license contents
  //
  if(!pParams->licfilepath &&
    (parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LICFILE))) {
    pParams->licfilepath = parg;
  }

  LOG(X_INFO("Read configuration from %s"), pParams->confpath);

  return pConf;
}

int vsxlib_checklicense(VSXLIB_STREAM_PARAMS_T *pParams, const LIC_INFO_T *pLic, SRV_CONF_T *pConf) {

#if defined(VSX_HAVE_LICENSE)

  if(!pConf && pParams->confpath) {
    if(!(pConf = vsxlib_loadconf(pParams))) {
      return -1;
    }
  }

  if(license_init((LIC_INFO_T *) pLic, pParams->licfilepath, NULL) < 0) {
    return -1;
  }

  if(pParams->outputs[0] && (pLic->capabilities & LIC_CAP_NO_RTPOUT)) {
    LOG(X_ERROR("RTP/UDP streaming output not enabled in license"));
    return -1;
  }

  if(pParams->strxcode && (pLic->capabilities & LIC_CAP_NO_XCODE)) {
    LOG(X_ERROR("Transcoding enabled in license"));
    return -1;
  }

  //if(pS->streamerCfg.verbosity > VSX_VERBOSITY_NORMAL) {
  //  license_dump(pLic, stdout);
  //}

#endif //VSX_HAVE_LICENSE

  return 0;
}

int vsxlib_stream_setupcap_dtls(const VSXLIB_STREAM_PARAMS_T *pParams, 
                                 CAPTURE_LOCAL_DESCR_T *pCapCfg) {
#if !defined(VSX_HAVE_SSL_DTLS)

  LOG(X_ERROR(VSX_DTLS_DISABLED_BANNER));
  LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
  return -1;

#else

  int rc = 0;

  //
  // Capture DTLS key / certificate configuration
  //
  memset(&pCapCfg->common.dtlsCfg, 0, sizeof(pCapCfg->common.dtlsCfg));
  if(vsxlib_dtls_init_certs(&pCapCfg->common.dtlsCfg, &pParams->srtpCfgs[0], "capture ") < 0) {
    return -1;
  }

  pCapCfg->common.dtlsCfg.dtls_handshake_server = pParams->srtpCfgs[0].dtls_handshake_server;

  return rc;
#endif // (VSX_HAVE_SSL_DTLS)
}

void vsxlib_stream_setup_rtmpclient(RTMP_CLIENT_CFG_T *pRtmpCfg, const VSXLIB_STREAM_PARAMS_T *pParams) {
  pRtmpCfg->rtmpfp9 = pParams->rtmpfp9;
  pRtmpCfg->prtmppageurl = pParams->rtmppageurl;
  pRtmpCfg->prtmpswfurl = pParams->rtmpswfurl;
}

int vsxlib_stream_setupcap_params(const VSXLIB_STREAM_PARAMS_T *pParams, 
                                   CAPTURE_LOCAL_DESCR_T *pCapCfg) {

  pCapCfg->common.promiscuous = pParams->promisc;
  pCapCfg->common.verbosity = pParams->verbosity;
  pCapCfg->common.idletmt_ms = pParams->capture_idletmt_ms;
  pCapCfg->common.idletmt_1stpkt_ms = pParams->capture_idletmt_1stpkt_ms;

  if((pCapCfg->common.cfgMaxVidRtpGapWaitTmMs = pParams->capture_max_rtpplayoutviddelayms) == 0) {
    pCapCfg->common.cfgMaxVidRtpGapWaitTmMs = CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS;
    pCapCfg->common.dfltCfgMaxVidRtpGapWaitTmMs = 1;
  }
  if((pCapCfg->common.cfgMaxAudRtpGapWaitTmMs = pParams->capture_max_rtpplayoutauddelayms) == 0) {
    pCapCfg->common.cfgMaxAudRtpGapWaitTmMs = CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS;
    pCapCfg->common.dfltCfgMaxAudRtpGapWaitTmMs = 1;
  }

  pCapCfg->common.connectretrycntminone = pParams->connectretrycntminone;
  pCapCfg->common.frtcp_rr_intervalsec = pParams->frtcp_rr_intervalsec;
  if((pCapCfg->common.rtp_frame_drop_policy = pParams->capture_rtp_frame_drop_policy) < 0 ||
     pCapCfg->common.rtp_frame_drop_policy >= CAPTURE_FRAME_DROP_POLICY_MAX_VAL) {
    pCapCfg->common.rtp_frame_drop_policy =  CAPTURE_FRAME_DROP_POLICY_DEFAULT;
  }
  pCapCfg->common.rtcp_reply_from_mcast = pParams->rtcp_reply_from_mcast;
  pCapCfg->common.caprealtime = pParams->caprealtime;
  vsxlib_stream_setup_rtmpclient(&pCapCfg->common.rtmpcfg, pParams);
  pCapCfg->common.novid = pParams->novid;
  pCapCfg->common.noaud = pParams->noaud;

  return 0;
}

int vsxlib_stream_setupcap(const VSXLIB_STREAM_PARAMS_T *pParams, CAPTURE_LOCAL_DESCR_T *pCapCfg,
                            STREAMER_CFG_T *pStreamerCfg, SDP_DESCR_T *pSdp) {
  int rc = 0;
  unsigned int idx;
  int do_video_nack = 0;
  const char *strfilters[2];
  int warn_remb_max = 0;
  int warn_remb_min = 0;
  char tmp[128];
  CAPTURE_FILTER_TRANSPORT_T inTransType = CAPTURE_FILTER_TRANSPORT_UNKNOWN;
  CAPTURE_FILTERS_T *pFilt = NULL; 
  AUTH_CREDENTIALS_STORE_T *pAuthStore = NULL;

  if(!pParams || !pCapCfg) {
    return -1;
  }

  if(pCapCfg->common.localAddrs[0] == NULL && pCapCfg->common.localAddrs[0] == NULL) {
    return -1;
  }

  pFilt =  &pCapCfg->common.filt;
  vsxlib_stream_setupcap_params(pParams, pCapCfg);

  pCapCfg->common.promiscuous = 0;
  pCapCfg->common.caphighprio = pParams->caphighprio;
  pCapCfg->common.xmithighprio = pParams->xmithighprio;

  strfilters[0] = pParams->strfilters[0];
  strfilters[1] = pParams->strfilters[1];
  inTransType = capture_parseTransportStr(&pCapCfg->common.localAddrs[0]);

  //
  // Retrieve any user:pass@<address> authorization credentials from URL
  //
  pAuthStore = &pCapCfg->common.creds[0];
  if(capture_parseAuthUrl(&pCapCfg->common.localAddrs[0], pAuthStore) < 0) {
    return -1;
  } else if(pAuthStore && (pAuthStore->username[0] != '\0' || pAuthStore->realm[0] != '\0')) {
    strncpy(pAuthStore->realm, net_getlocalhostname(), sizeof(pAuthStore->realm) -1);
  }

  pCapCfg->common.creds[0].authtype = pParams->httpauthtype;

  if(IS_CAPTURE_FILTER_TRANSPORT_DTLS(pCapCfg->common.filt.filters[0].transType) && 
                                      vsxlib_stream_setupcap_dtls(pParams, pCapCfg) < 0) {
    return -1;
  }

  if(IS_CAPTURE_FILTER_TRANSPORT_RTSP(inTransType)) {

    // Defaut to 2 (vid, aud) input filters
    pFilt->numFilters = 2;

  } else if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPGET ||
            inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSGET) {

    // default to m2t capture
    if(strfilters[0] == NULL) {
      strfilters[0] = "type=m2t";
    }

  } else if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPFLV ||
            inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSFLV) {

    if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSFLV) {
      inTransType = CAPTURE_FILTER_TRANSPORT_HTTPSGET;
    } else {
      inTransType = CAPTURE_FILTER_TRANSPORT_HTTPGET;
    }
    pFilt->filters[0].mediaType = CAPTURE_FILTER_PROTOCOL_FLV;

  } else if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPMP4 ||
            inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSMP4) {

    if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSMP4) {
      inTransType = CAPTURE_FILTER_TRANSPORT_HTTPSGET;
    } else {
      inTransType = CAPTURE_FILTER_TRANSPORT_HTTPGET;
    }
    pFilt->filters[0].mediaType = CAPTURE_FILTER_PROTOCOL_MP4;

  } else if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPDASH ||
            inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSDASH) {

    if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSDASH) {
      inTransType = CAPTURE_FILTER_TRANSPORT_HTTPSGET;
    } else {
      inTransType = CAPTURE_FILTER_TRANSPORT_HTTPGET;
    }
    pFilt->filters[0].mediaType = CAPTURE_FILTER_PROTOCOL_DASH;

  } else if(inTransType == CAPTURE_FILTER_TRANSPORT_RTMP ||
            inTransType == CAPTURE_FILTER_TRANSPORT_RTMPS) {
    // Defaut to 2 (vid, aud) input filters
    pFilt->numFilters = 2;
    pFilt->filters[0].mediaType = MEDIA_FILE_TYPE_H264b;
    pFilt->filters[1].mediaType = MEDIA_FILE_TYPE_AAC_ADTS;
  }

  for(idx = 0; idx < 2; idx++) {
    if(strfilters[idx]) {
      if(capture_parseFilter(strfilters[idx], &pFilt->filters[idx]) < 0) {
        return -1;
      }
      pFilt->numFilters++;
    }
  }

  if(pFilt->filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_FLV) {

    if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSGET) {
      pFilt->filters[0].transType = CAPTURE_FILTER_TRANSPORT_HTTPSFLV;
    } else {
      pFilt->filters[0].transType = CAPTURE_FILTER_TRANSPORT_HTTPFLV;
    }

    pFilt->numFilters = 2;
    pFilt->filters[1].transType = pFilt->filters[0].transType;
    pFilt->filters[0].mediaType = MEDIA_FILE_TYPE_H264b;
    pFilt->filters[1].mediaType = MEDIA_FILE_TYPE_AAC_ADTS;

  } else if(pFilt->filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_MP4) {

    if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSGET) {
      pFilt->filters[0].transType = CAPTURE_FILTER_TRANSPORT_HTTPSMP4;
    } else {
      pFilt->filters[0].transType = CAPTURE_FILTER_TRANSPORT_HTTPMP4;
    }

    pFilt->numFilters = 2;
    pFilt->filters[1].transType = pFilt->filters[0].transType;
    pFilt->filters[0].mediaType = MEDIA_FILE_TYPE_H264b;
    pFilt->filters[1].mediaType = MEDIA_FILE_TYPE_AAC_ADTS;

  } else if(pFilt->filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_DASH) {

    if(inTransType == CAPTURE_FILTER_TRANSPORT_HTTPSGET) {
      pFilt->filters[0].transType = CAPTURE_FILTER_TRANSPORT_HTTPSDASH;
    } else {
      pFilt->filters[0].transType = CAPTURE_FILTER_TRANSPORT_HTTPDASH;
    }

    pFilt->numFilters = 2;
    pFilt->filters[1].transType = pFilt->filters[0].transType;
    pFilt->filters[0].mediaType = MEDIA_FILE_TYPE_H264b;
    pFilt->filters[1].mediaType = MEDIA_FILE_TYPE_AAC_ADTS;

  }

  for(idx = 0; idx < pFilt->numFilters; idx++) {

    //
    // Set the filter recvonly/sendonly/sendrecv type from the SDP
    // 
    if(IS_CAP_FILTER_VID(pFilt->filters[idx].mediaType)) {
      pFilt->filters[idx].xmitType = pFilt->filters[idx].u_seqhdrs.vid.common.xmitType;
    } else if(IS_CAP_FILTER_AUD(pFilt->filters[idx].mediaType)) {
      pFilt->filters[idx].xmitType = pFilt->filters[idx].u_seqhdrs.aud.common.xmitType;
    }

#if defined(VSX_HAVE_TURN)
    //
    // Set TURN properties
    //
    if(pParams->turnCfg.turnPolicy != TURN_POLICY_DISABLED &&
      turn_init_from_cfg(&pFilt->filters[idx].turn, &pParams->turnCfg) < 0) {
      return -1;
    }
    memcpy(&pFilt->filters[idx].turnRtcp, &pFilt->filters[idx].turn, sizeof(pFilt->filters[idx].turnRtcp));

#endif // (VSX_HAVE_TURN)

    //
    // Set filter STUN responder settings
    //
    if(pParams->stunResponderCfg.bindingResponse != STUN_POLICY_NONE) {
      pFilt->filters[idx].stun.policy = STUN_POLICY_ENABLED;
      pFilt->filters[idx].stun.integrity.pass = pParams->stunResponderCfg.respPass;
      pFilt->filters[idx].stun.integrity.user = pParams->stunResponderCfg.respUsername;
      pFilt->filters[idx].stun.integrity.realm = pParams->stunResponderCfg.respRealm;

      if(pParams->stunResponderCfg.respUseSDPIceufrag) {
        //
        // ufragbuf, pwdbuf may have been set from SDP ice-ufrag, ice-pwd
        //
        pFilt->filters[idx].stun.integrity.user = pFilt->filters[idx].stun.store.ufragbuf;
        pFilt->filters[idx].stun.integrity.pass = pFilt->filters[idx].stun.store.pwdbuf;
      }

      //TODO: make user configurable from cmdline, realm configurable
      if(pFilt->filters[idx].stun.integrity.pass &&
         (idx == 0 || 
          strcmp(pFilt->filters[idx].stun.integrity.pass, pFilt->filters[idx - 1].stun.integrity.pass))) {

        LOG(X_DEBUG("STUN binding[%d] responder policy: %s, username: '%s', password: '%s', realm:'%s'"), idx, 
          stun_policy2str(pFilt->filters[idx].stun.policy),
          pFilt->filters[idx].stun.integrity.user ? pFilt->filters[idx].stun.integrity.user : "",
          pFilt->filters[idx].stun.integrity.pass ? pFilt->filters[idx].stun.integrity.pass : "",
          pFilt->filters[idx].stun.integrity.realm ? pFilt->filters[idx].stun.integrity.realm : "");
      }
    }

    if(pStreamerCfg && IS_CAP_FILTER_VID(pFilt->filters[idx].mediaType)) {

      //
      // Set the filter to enable RTCP NACK requests  
      //
      if(BOOL_ISENABLED(pStreamerCfg->fbReq.nackRtcpSend)) {
        pFilt->filters[idx].enableNack = 1; // auto-determine 
        LOG(X_DEBUG("Enabling RTCP NACK requests for video media"));
        do_video_nack = 1;
      } else {
        pFilt->filters[idx].enableNack = 0;
      }

      //
      // Set the filter to enable RTCP REMB (Receiver Estimated Max Bandwidth) notifications
      //
      if(BOOL_ISENABLED(pStreamerCfg->fbReq.apprembRtcpSend)) {

        pFilt->filters[idx].enableRemb = 1; 

        tmp[0] = '\0';
        if(BOOL_ISENABLED(pStreamerCfg->fbReq.apprembRtcpSendForceAdjustment)) {
          snprintf(tmp, sizeof(tmp), " (rembxmitforce enabled)");
          pStreamerCfg->fbReq.apprembRtcpSendForceAdjustment = 1;
        } else {
          snprintf(tmp, sizeof(tmp), " (rembxmitforce disabled)");
          pStreamerCfg->fbReq.apprembRtcpSendForceAdjustment = 0;
        }

        if(pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps == 0) {
          pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps = APPREMB_DEFAULT_MAXRATE_BPS;
          warn_remb_max = 1;
        }
        if(pStreamerCfg->fbReq.apprembRtcpSendMinRateBps == 0) {
          pStreamerCfg->fbReq.apprembRtcpSendMinRateBps = APPREMB_DEFAULT_MINRATE_BPS;
          warn_remb_min = 1;
        }
        if(pStreamerCfg->fbReq.apprembRtcpSendMinRateBps > pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps) {
          pStreamerCfg->fbReq.apprembRtcpSendMinRateBps = pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps;
          warn_remb_min = 1;
        }
        if(pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps < pStreamerCfg->fbReq.apprembRtcpSendMinRateBps) {
          pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps = pStreamerCfg->fbReq.apprembRtcpSendMinRateBps;
          warn_remb_max = 1;
        }

        if(warn_remb_max) {
          LOG(X_WARNING("RTCP APP REMB notifications for video media max bitrate not set.  Using max rate: %u b/s"),
                    pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps);
        }

        if(warn_remb_min) {
          LOG(X_WARNING("RTCP APP REMB notifications for video media min bitrate not set.  Using min rate: %u b/s"),
                    pStreamerCfg->fbReq.apprembRtcpSendMinRateBps);
        }

        LOG(X_DEBUG("Enabling RTCP APP REMB notifications for video media. Max: %u b/s, Min: %u b/s%s"),
                    pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps, pStreamerCfg->fbReq.apprembRtcpSendMinRateBps, tmp);
      } else {
        pFilt->filters[idx].enableRemb = 0; 
      }

    }

  }

  if(do_video_nack) {

    if(pCapCfg->common.dfltCfgMaxVidRtpGapWaitTmMs) {
      pCapCfg->common.cfgMaxVidRtpGapWaitTmMs = CAPTURE_RTP_VIDNACK_JTBUF_GAP_TS_MAXWAIT_MS;
      //LOG(X_INFO("RTCP NACK video automatically increased video playout buffer duration to %d ms"), 
      //    pCapCfg->common.cfgMaxVidRtpGapWaitTmMs);
    }
    //TODO: we should really be using avReaderType VSX_STREAM_READER_TYPE_NOSYNC

  }

  for(idx = 0; idx < 2; idx++) {

    if(strfilters[idx]) {

      if(IS_CAP_FILTER_VID(pFilt->filters[idx].mediaType)) {
        pFilt->filters[idx].u_seqhdrs.vid.common.codecType =  pFilt->filters[idx].mediaType;
        pFilt->filters[idx].u_seqhdrs.vid.common.available = 1;
        if(pSdp) {
          memcpy(&pSdp->vid, &pFilt->filters[idx].u_seqhdrs.vid, sizeof(SDP_DESCR_VID_T));
        }
      } else if(IS_CAP_FILTER_AUD(pFilt->filters[idx].mediaType)) {
        pFilt->filters[idx].u_seqhdrs.aud.common.codecType = pFilt->filters[idx].mediaType;
        pFilt->filters[idx].u_seqhdrs.aud.common.available = 1;
        if(pSdp) {
          memcpy(&pSdp->aud, &pFilt->filters[idx].u_seqhdrs.aud, sizeof(SDP_DESCR_AUD_T));
        }
      }

    }

    if(pFilt->filters[idx].transType == CAPTURE_FILTER_TRANSPORT_UNKNOWN) {
      pFilt->filters[idx].transType = inTransType;
    }

  } // end of for

  if(pFilt->numFilters > 0) {
    if(BOOL_ISENABLED(pParams->extractpes)) {
      for(idx = 0; idx < pFilt->numFilters; idx++) {
        pCapCfg->capActions[idx].cmd |= CAPTURE_PKT_ACTION_EXTRACTPES;
      }
    }
  }

  // Always use queue to allow for async streaming
  for(idx = 0; idx < pFilt->numFilters; idx++) {
    pCapCfg->capActions[idx].cmd |= CAPTURE_PKT_ACTION_QUEUE;
    pCapCfg->capActions[idx].pktqslots = pParams->pktqslots;
    pCapCfg->capActions[idx].frvidqslots = pParams->frvidqslots;
    pCapCfg->capActions[idx].fraudqslots = pParams->fraudqslots;
    pFilt->filters[idx].pCapAction = &pCapCfg->capActions[idx];

    //TODO: make this currently experimental feature configurable
    pCapCfg->capActions[idx].lagCtxt.doSyncToRT = 0;
  }

  if(pCapCfg->common.cfgMaxVidRtpGapWaitTmMs != CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS ||
     pCapCfg->common.cfgMaxAudRtpGapWaitTmMs != CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS) {
    LOG(X_DEBUG("Using RTPMaxVideoPlayoutDelay: %d ms,  RTPMaxAudioPlayoutDelay: %d ms"),
      pCapCfg->common.cfgMaxVidRtpGapWaitTmMs, pCapCfg->common.cfgMaxAudRtpGapWaitTmMs);
  }

  return rc;
}

int vsxlib_check_prior_listeners(const SRV_LISTENER_CFG_T *arrCfgs, 
                                 unsigned int max,
                                 const struct sockaddr *psa) {
  unsigned int idx;

  if(!arrCfgs || !psa) {
    return 0;
  }

  for(idx = 0; idx < max; idx++) {

    if(arrCfgs[idx].active &&
       INET_PORT(arrCfgs[idx].sa) == PINET_PORT(psa) &&
       INET_IS_SAMEADDR(arrCfgs[idx].sa, *psa)) {
      return idx + 1;
    }
  }

  return 0;
}

int vsxlib_check_other_listeners(const SRV_START_CFG_T *pStartCfg,
                                 const SRV_LISTENER_CFG_T *arrCfgThis) {
  int rc;
  unsigned int idx;
  unsigned int max, max1, max2;
  const SRV_LISTENER_CFG_T *arrCfgs1, *arrCfgs2;

  if(!pStartCfg || !arrCfgThis) {
    return -1;
  }

  if(arrCfgThis == pStartCfg->listenHttp) {
    arrCfgs1 = pStartCfg->listenRtmp;
    arrCfgs2 = pStartCfg->listenRtsp;
    max = SRV_LISTENER_MAX_HTTP;
    max1 = SRV_LISTENER_MAX_RTMP;
    max2 = SRV_LISTENER_MAX_RTSP;
  } else if(arrCfgThis == pStartCfg->listenRtmp) {
    arrCfgs1 = pStartCfg->listenHttp;
    arrCfgs2 = pStartCfg->listenRtsp;
    max = SRV_LISTENER_MAX_RTMP;
    max1 = SRV_LISTENER_MAX_HTTP;
    max2 = SRV_LISTENER_MAX_RTSP;
  } else if(arrCfgThis == pStartCfg->listenRtsp) {
    arrCfgs1 = pStartCfg->listenHttp;
    arrCfgs2 = pStartCfg->listenRtmp;
    max = SRV_LISTENER_MAX_RTSP;
    max1 = SRV_LISTENER_MAX_HTTP;
    max2 = SRV_LISTENER_MAX_RTMP;
  } else {
    return -1;
  }

  for(idx = 0; idx < max; idx++) {

    if(!arrCfgThis[idx].active) {
      continue;
    }

    if(arrCfgs1 && (rc = vsxlib_check_prior_listeners(arrCfgs1, max1, (const struct sockaddr *) &arrCfgThis[idx].sa)) > 0) {
      return idx + 1;
    }

    if(arrCfgs2 && (rc = vsxlib_check_prior_listeners(arrCfgs2, max2, (const struct sockaddr *) &arrCfgThis[idx].sa)) > 0) {
      return idx + 1;
    }

  }

  return 0;
}

int vsxlib_parse_listener(const char *arrAddr[], unsigned int maxListenerCfgs, SRV_LISTENER_CFG_T *arrCfgs,
                          enum URL_CAPABILITY urlCap, AUTH_CREDENTIALS_STORE_T *ppAuthStores) {

  CAPTURE_FILTER_TRANSPORT_T transType;
  SOCKET_LIST_T sockList;
  unsigned int idxCfgs;
  unsigned int idxArr = 0;
  NETIO_FLAG_T netflags;
  AUTH_CREDENTIALS_STORE_T authStore;
  const char *strListen;
  int rc;
  int countActive = 0;

  if(!arrAddr || !arrCfgs) {
    return -1;
  }

  //
  // maxListenerCfgs is the count of arrAddr[] and ppAuthStores[]
  //
  for(idxArr = 0; idxArr < maxListenerCfgs; idxArr++) {

    if(!(strListen = arrAddr[idxArr])) {
      break;
    }

    netflags = 0;

    //
    // Retrieve and strip any 'https://' prefix
    //
    transType = capture_parseTransportStr(&strListen);
    memset(&authStore, 0, sizeof(authStore));

    //
    // Retrieve any user:pass@<address> authorization credentials from URL
    //
    if(capture_parseAuthUrl(&strListen, ppAuthStores ? &authStore : NULL) < 0) {
      return -1;
    } else if(IS_AUTH_CREDENTIALS_SET(&authStore)) {
      strncpy(authStore.realm, net_getlocalhostname(), sizeof(authStore.realm) -1);
    }

    //
    // Parse the '127.0.0.1:8080' < optional address : mandatory port > pair
    //
    if((rc = capture_parseLocalAddr(strListen, &sockList)) <= 0) {
      LOG(X_ERROR("Not a valid listener configuration: '%s'"), strListen);
      return -1;
    }

    if(transType == CAPTURE_FILTER_TRANSPORT_HTTPSGET ||
       transType == CAPTURE_FILTER_TRANSPORT_RTSPS ||
       transType == CAPTURE_FILTER_TRANSPORT_RTMPS) {
      netflags |= NETIO_FLAG_SSL_TLS;
    }
    //LOG(X_DEBUG("AUTHSTORE: user:'%s', pass:'%s'"), authStore.username, authStore.pass);
    if((rc = vsxlib_check_prior_listeners(arrCfgs, maxListenerCfgs, (const struct sockaddr *) &sockList.salist[0])) > 0) {
      if(arrCfgs[rc - 1].netflags != netflags) {
        LOG(X_ERROR("Cannot mix SSL and non-SSL listener for %s"), arrAddr[idxArr]);
        return -1;
      } else {
        if((arrCfgs[rc - 1].urlCapabilities & urlCap)) {
          LOG(X_WARNING("Duplicate listener specified with %s"), arrAddr[idxArr]);
        }
        //fprintf(stderr, "REUSING idxArr[%d] with new urlCap: 0x%x\n", idxArr, urlCap);
        arrCfgs[rc - 1].urlCapabilities |= urlCap;
        continue;
      }
    }

    //
    // Find an available slot
    //
    for(idxCfgs = 0; idxCfgs < maxListenerCfgs; idxCfgs++) {
      if(!arrCfgs[idxCfgs].active) {
        break;
      }
    }
    if(idxCfgs >= maxListenerCfgs) {
      LOG(X_WARNING("No more listeners available for '%s'"), arrAddr[idxArr]);
      break;
    }

    //fprintf(stderr, "ADDING idxArr[%d] 0x%x idxCfg[%d], port: %d, urlCap: 0x%x, pAuthStore: 0x%x\n", idxArr, arrAddr[idxArr], idxCfgs, htons(arrCfgs[idxCfgs].sain.sin_port), urlCap, ppAuthStores ? ppAuthStores : NULL);

    memcpy(&arrCfgs[idxCfgs].sa, &sockList.salist[0], sizeof(arrCfgs[idxCfgs].sa));
    arrCfgs[idxCfgs].netflags = netflags;
    arrCfgs[idxCfgs].urlCapabilities |= urlCap;
    arrCfgs[idxCfgs].idxCfg = idxCfgs;
    if(ppAuthStores && IS_AUTH_CREDENTIALS_SET(&authStore)) {
      memcpy(&ppAuthStores[idxCfgs], &authStore, sizeof(AUTH_CREDENTIALS_STORE_T));
      arrCfgs[idxCfgs].pAuthStore = &ppAuthStores[idxCfgs];
    } else {
      arrCfgs[idxCfgs].pAuthStore = NULL;
    }

    arrCfgs[idxCfgs].active = 1;
    countActive++;
  }

  return countActive;
}

void vsxlib_setup_rtpout(STREAMER_CFG_T *pStreamerCfg, const VSXLIB_STREAM_PARAMS_T *pParams) {
  int rc = 0;

  pStreamerCfg->frtcp_sr_intervalsec = pParams->frtcp_sr_intervalsec;
  pStreamerCfg->cfgrtp.maxPayloadSz = pParams->mtu;
  pStreamerCfg->cfgrtp.audioAggregatePktsMs = pParams->audioAggregatePktsMs;
  pStreamerCfg->cfgrtp.rtpPktzMode = pParams->rtpPktzMode;
  pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort = pParams->rtp_useCaptureSrcPort;

  //
  // Get the RTP payload types string
  //
  if(pParams->output_rtpptstr) {
    if((rc = capture_parsePayloadTypes(pParams->output_rtpptstr,
                                       &pStreamerCfg->cfgrtp.payloadTypesMin1[0],
                                       &pStreamerCfg->cfgrtp.payloadTypesMin1[1])) & 0x01) {
      pStreamerCfg->cfgrtp.payloadTypesMin1[0]++;
    }
    if((rc & 0x02)) {
      pStreamerCfg->cfgrtp.payloadTypesMin1[1]++;
    } else if(pStreamerCfg->novid) {
      pStreamerCfg->cfgrtp.payloadTypesMin1[1] = pStreamerCfg->cfgrtp.payloadTypesMin1[0];
    }

  }

  //
  // Get the RTP SSRCs 
  //
  if(pParams->output_rtpssrcsstr) {
    rc = capture_parsePayloadTypes(pParams->output_rtpssrcsstr, 
                                   (int *) &pStreamerCfg->cfgrtp.ssrcs[0],
                                   (int *) &pStreamerCfg->cfgrtp.ssrcs[1]);
    if(!(rc & 0x02) && pStreamerCfg->novid) {
      pStreamerCfg->cfgrtp.ssrcs[1] = pStreamerCfg->cfgrtp.ssrcs[0];
    }

  }

  //
  // Get the RTP Clock rate 
  //
  if(pParams->output_rtpclockstr) {
    rc = capture_parsePayloadTypes(pParams->output_rtpclockstr, 
                              (int *) &pStreamerCfg->cfgrtp.clockHzs[0],
                              (int *) &pStreamerCfg->cfgrtp.clockHzs[1]);
    if(!(rc & 0x02) && pStreamerCfg->novid) {
      pStreamerCfg->cfgrtp.clockHzs[1] = pStreamerCfg->cfgrtp.clockHzs[0];
    }

  }

  pStreamerCfg->cfgrtp.sequence_nums[0] = 0;
  pStreamerCfg->cfgrtp.sequence_nums[1] = 0;
/*
  pStreamerCfg->cfgrtp.timeStamps[0].timeStamp = 2000000;
  pStreamerCfg->cfgrtp.timeStamps[0].haveTimeStamp = 1;
  pStreamerCfg->cfgrtp.timeStamps[1].timeStamp = 4000000;
  pStreamerCfg->cfgrtp.timeStamps[1].haveTimeStamp = 1;
*/
  
  pStreamerCfg->cfgrtp.dtlsTimeouts.handshakeRtpTimeoutMs = pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtpTimeoutMs;
  pStreamerCfg->cfgrtp.dtlsTimeouts.handshakeRtcpAdditionalMs = pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtcpAdditionalMs;
  pStreamerCfg->cfgrtp.dtlsTimeouts.handshakeRtcpAdditionalGiveupMs = pParams->srtpCfgs[0].dtlsTimeouts.handshakeRtcpAdditionalGiveupMs;

}

int vsxlib_init_stunoutput(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, 
                            const STUN_REQUESTOR_CFG_T *pStunCfg) {
  int rc = 0;
  unsigned int idxPort;

  if(!pStunCfg || !pStunCfg->bindingRequest) {
    return -1;
  }

  memset(&pdestsCfg->stunCfgs, 0, sizeof(pdestsCfg->stunCfgs));

  if(stream_stun_parseCredentialsPair(pStunCfg->reqUsername, 
                                      pdestsCfg->stunCfgs[0].store.ufragbuf, 
                                      pdestsCfg->stunCfgs[1].store.ufragbuf) < 0 ||
    stream_stun_parseCredentialsPair(pStunCfg->reqPass, 
                                     pdestsCfg->stunCfgs[0].store.pwdbuf, 
                                     pdestsCfg->stunCfgs[1].store.pwdbuf) < 0 ||
    stream_stun_parseCredentialsPair(pStunCfg->reqRealm, 
                                     pdestsCfg->stunCfgs[0].store.realmbuf, 
                                     pdestsCfg->stunCfgs[1].store.realmbuf) < 0) {
    return -1;
  }

  for(idxPort = 0; idxPort < numChannels; idxPort++) {

    pdestsCfg->stunCfgs[idxPort].cfg.bindingRequest = pStunCfg->bindingRequest;
    pdestsCfg->stunCfgs[idxPort].cfg.reqUsername = pdestsCfg->stunCfgs[idxPort].store.ufragbuf;
    pdestsCfg->stunCfgs[idxPort].cfg.reqPass = pdestsCfg->stunCfgs[idxPort].store.pwdbuf;
    pdestsCfg->stunCfgs[idxPort].cfg.reqRealm = pdestsCfg->stunCfgs[idxPort].store.realmbuf;

    if(pdestsCfg->stunCfgs[idxPort].cfg.bindingRequest) {
      LOG(X_DEBUG("STUN binding[%d] request policy: %s, username: '%s', password: '%s', realm: '%s'"), idxPort,
           stun_policy2str(pdestsCfg->stunCfgs[idxPort].cfg.bindingRequest),
           pdestsCfg->stunCfgs[idxPort].cfg.reqUsername ?  pdestsCfg->stunCfgs[idxPort].cfg.reqUsername : "",
           pdestsCfg->stunCfgs[idxPort].cfg.reqPass ?  pdestsCfg->stunCfgs[idxPort].cfg.reqPass : "",
           pdestsCfg->stunCfgs[idxPort].cfg.reqRealm ?  pdestsCfg->stunCfgs[idxPort].cfg.reqRealm : "");
    }
  }

  return rc;
}

int vsxlib_init_turnoutput(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, 
                            const TURN_CFG_T *pTurnCfg) {
  int rc = 0;
  unsigned int idxPort;

  if(!pTurnCfg) {
    return -1;
  }

  memset(&pdestsCfg->turnCfgs, 0, sizeof(pdestsCfg->turnCfgs));

  for(idxPort = 0; idxPort < numChannels; idxPort++) {
    memcpy(&pdestsCfg->turnCfgs[idxPort], pTurnCfg, sizeof(pdestsCfg->turnCfgs[idxPort]));
  }
  return rc;
}

int vsxlib_init_dtls(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, 
                      const SRTP_CFG_T *pSrtpCfg, int srtp) {

#if !defined(VSX_HAVE_SSL_DTLS)

  LOG(X_ERROR(VSX_DTLS_DISABLED_BANNER));
  LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
  return -1;

#else

  if(!pSrtpCfg) {
    return -1;
  }

  memset(&pdestsCfg->dtlsCfg, 0, sizeof(pdestsCfg->dtlsCfg));

  if(vsxlib_dtls_init_certs(&pdestsCfg->dtlsCfg, pSrtpCfg, "output ") < 0) {
    return -1;
  }

  //
  // Compute a fingerprint of our X.509 certificate public key
  //
  pdestsCfg->dtlsCfg.fingerprint.type = DTLS_FINGERPRINT_TYPE_SHA256;
  if(dtls_get_cert_fingerprint(pdestsCfg->dtlsCfg.certpath, &pdestsCfg->dtlsCfg.fingerprint) < 0) {
    return -1;
  }
  pdestsCfg->dtlsCfg.dtls_srtp = (pSrtpCfg->srtp || srtp) ? 1 : 0;
  pdestsCfg->dtlsCfg.dtls_handshake_server = pSrtpCfg->dtls_handshake_server;
  pdestsCfg->dtlsCfg.active = 1;

  return 0;

#endif // (VSX_HAVE_DTLS)

}

int vsxlib_init_srtp(SRTP_CTXT_T srtps[STREAMER_PAIR_SZ], unsigned int numChannels, 
                     const SRTP_CFG_T *pSrtpCfg, const char *dstHost, uint16_t *ports) {

#if !defined(VSX_HAVE_SRTP)

  LOG(X_ERROR(VSX_SRTP_DISABLED_BANNER));
  LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
  return -1;

#else

  int rc;
  unsigned int idxPort;
  SRTP_AUTH_TYPE_T authType = pSrtpCfg ? pSrtpCfg->authType : SRTP_AUTH_NONE;
  SRTP_CONF_TYPE_T confType = pSrtpCfg ? pSrtpCfg->confType : SRTP_CONF_NONE; 

  memset(srtps, 0, STREAMER_PAIR_SZ * sizeof(srtps[0]));

  //
  // Load pre-set video and audio SRTP key(s)
  //
  if(pSrtpCfg && srtp_loadKeys(pSrtpCfg->keysBase64, &srtps[0], &srtps[1]) < 0) {
    LOG(X_ERROR("Invalid SRTP base64 key '%s'"), pSrtpCfg->keysBase64);
    return -1;
  }

  srtps[0].kt.type.confTypeRtcp = srtps[1].kt.type.confTypeRtcp = confType;
  srtps[0].kt.type.authTypeRtcp = srtps[1].kt.type.authTypeRtcp = authType;
  srtps[0].kt.type.confType = srtps[1].kt.type.confType = confType;
  srtps[0].kt.type.authType = srtps[1].kt.type.authType = authType;

  if(srtps[0].kt.type.authType == SRTP_AUTH_NONE &&
     srtps[0].kt.type.confType == SRTP_CONF_NONE) {
    srtps[0].kt.type.confType = srtps[1].kt.type.confType = SRTP_CONF_AES_128;
    srtps[0].kt.type.authType = srtps[1].kt.type.authType = SRTP_AUTH_SHA1_80;
    srtps[0].kt.type.confTypeRtcp = srtps[1].kt.type.confTypeRtcp = srtps[0].kt.type.confType;
    srtps[0].kt.type.authTypeRtcp = srtps[1].kt.type.authTypeRtcp = srtps[0].kt.type.authType;
//srtps[1].confType = srtps[1].confTypeRtcp = SRTP_CONF_NONE;
  }

  //
  // RTP audio & video could be using the same ports, so make sure we create a per-channel key
  //
  for(idxPort = 0; idxPort < numChannels; idxPort++) {

    if((rc = srtp_createKey(&srtps[idxPort])) < 0) {
      return -1;
    } else if(rc > 0) {
      LOG(X_DEBUG("Created SRTP key length %d bytes for %s:%d"),
          srtps[idxPort].kt.k.lenKey, dstHost ? dstHost : "", ports ? ports[idxPort] : 0);
    } else {
      LOG(X_DEBUG("Using SRTP key length %d bytes for %s:%d"), srtps[idxPort].kt.k.lenKey,
          dstHost ? dstHost : "", ports ? ports[idxPort] : 0);
    }
    //LOGHEX_DEBUG(srtps[idxPort].kt.k.key, srtps[idxPort].kt.k.lenKey);

  }

  //avc_dumpHex(stderr, srtps[0].kt.k.key, srtps[0].kt.k.lenKey, 1); 
  //avc_dumpHex(stderr, srtps[1].kt.k.key, srtps[1].kt.k.lenKey, 1); 

#endif

  return 0;
}

#if defined(VSX_HAVE_SSL_DTLS)

static int g_warn_default_dtls = 0;
static int g_warn_dtls = 0;

static int vsxlib_dtls_init_certs(DTLS_CFG_T *pDtlsCfg, const SRTP_CFG_T *pSrtpCfg, const char *descr) {


  int rc = 0;

  if(!(pDtlsCfg->certpath = pSrtpCfg->dtlscertpath)) {
    pDtlsCfg->certpath = SSL_DTLS_CERT_PATH;
    if(g_warn_default_dtls == 0) {
      g_warn_default_dtls = 1;
    }
  }

  if(!(pDtlsCfg->privkeypath = pSrtpCfg->dtlsprivkeypath)) {
    pDtlsCfg->privkeypath = SSL_DTLS_PRIVKEY_PATH;
    if(g_warn_default_dtls == 0) {
      g_warn_default_dtls = 1;
    }
  }


  if(g_warn_dtls++ == 0) {
    LOG(X_DEBUG("Using SSL DTLS %sserver certificate: %s, key-file: %s"), 
       descr ? descr: "", pDtlsCfg->certpath, pDtlsCfg->privkeypath);
  } 

  if(g_warn_default_dtls == 1) {
    LOG(X_WARNING(WARN_DEFAULT_CERT_KEY_MSG));
    g_warn_default_dtls++;
  }

  return rc;

}
#endif // VSX_HAVE_SSL_DTLS

static int g_warn_default_tls = 0;

int vsxlib_ssl_initserver(const VSXLIB_STREAM_PARAMS_T *pParams, const SRV_LISTENER_CFG_T *pListenCfg) {
#if defined(VSX_HAVE_SSL)
  int rc = 0;
  const char *sslcertpath;
  const char *sslprivkeypath;
#else // (VSX_HAVE_SSL)
  char tmp[128];
#endif // (VSX_HAVE_SSL)

  if(!pParams || !pListenCfg) {
    return -1;
  }

  //
  // Return succesfully here if this connection does not require SSL / TLS  
  //
  if(!(pListenCfg->netflags & NETIO_FLAG_SSL_TLS)) {
    return 0;
  }

#if defined(VSX_HAVE_SSL)

  if(netio_ssl_enabled(1)) {
    return 0;
  }

  sslcertpath = pParams->sslcertpath;
  sslprivkeypath = pParams->sslprivkeypath;

  if(!sslcertpath) {
    sslcertpath = SSL_CERT_PATH;
    if(g_warn_default_tls == 0) {
      g_warn_default_tls = 1;
    }
  }

  if(!sslprivkeypath) {
    sslprivkeypath = SSL_PRIVKEY_PATH;
    if(g_warn_default_tls == 0) {
      g_warn_default_tls = 1;
    }
  }

  LOG(X_DEBUG("Using SSL TLS server certificate: %s, key-file: %s"), sslcertpath, sslprivkeypath);

  if((rc = netio_ssl_init_srv(sslcertpath, sslprivkeypath)) < 0) {
    LOG(X_ERROR("SSL TLS server initialization failed. Certifcate: %s, key-file: %s"), sslcertpath, sslprivkeypath);
  } else if(g_warn_default_tls == 1) {
    LOG(X_WARNING(WARN_DEFAULT_CERT_KEY_MSG));
     g_warn_default_tls++;
  }

  return rc;

#else // VSX_HAVE_SSL

  LOG(X_ERROR("SSL not enabled for server listener on %s:%d"), 
      INET_NTOP(pListenCfg->sa, tmp, sizeof(tmp)), ntohs(INET_ADDR(pListenCfg->sa)));
  LOG(X_ERROR(VSX_SSL_DISABLED_BANNER));
  LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
  return -1;

#endif // VSX_HAVE_SSL

}

int vsxlib_ssl_initclient(NETIO_SOCK_T *pnetsock) {
#if defined(VSX_HAVE_SSL)
  int rc = 0;
#endif // (VSX_HAVE_SSL)

  if(!pnetsock || PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET) {
    return -1;
  }

  //
  // Return succesfully here if this connection does not require SSL / TLS  
  //
  if(!(pnetsock->flags & NETIO_FLAG_SSL_TLS)) {
    return 0;
  }

#if defined(VSX_HAVE_SSL)

  if(netio_ssl_enabled(0)) {
    return 0;
  }

  if((rc = netio_ssl_init_cli(NULL, NULL)) < 0) {
    LOG(X_ERROR("SSL client initialization failed."));
  }

  return rc;

#else // VSX_HAVE_SSL

  LOG(X_ERROR("SSL not enabled for client connection"));
  LOG(X_ERROR(VSX_SSL_DISABLED_BANNER));
  LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
  return -1;

#endif // VSX_HAVE_SSL

}

int vsxlib_setupFlvRecord(FLVSRV_CTXT_T *pFlvCtxts,
                          STREAMER_CFG_T *pStreamerCfg,
                          const VSXLIB_STREAM_PARAMS_T *pParams) {


#if defined(ENABLE_RECORDING)
  int rc = 0;
  size_t sz;
  unsigned int outidx;
  STREAMER_OUTFMT_T *pLiveFmt;
  unsigned int maxFlvRecord = 0;
  int alter_filename = 0;
  char buf[VSX_MAX_PATH_LEN];
  buf[0] = '\0';

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    if(pParams->flvrecord[outidx] && (outidx == 0 || pStreamerCfg->xcode.vid.out[outidx].active)) {
      maxFlvRecord++;
    }
  }

  //
  // Setup FLV server parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLVRECORD];
  if((*((unsigned int *) &pLiveFmt->max) = maxFlvRecord) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *) avc_calloc(maxFlvRecord, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_FLV_RECORD;

  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgFlv.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgFlv.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgFlv.growMaxPktLen;
  pLiveFmt->do_outfmt = 1;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pParams->flvrecord[outidx] || (outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    //
    // Create output index specific named .flv files
    //
    alter_filename = 0;
    if(outidx > 0 && buf[0] != '\0') {
      if(!strncasecmp(buf, pParams->flvrecord[outidx], strlen(pParams->flvrecord[outidx]))) {
        alter_filename = 1;
      }
    }
    strncpy(buf, pParams->flvrecord[outidx], sizeof(buf));

    if(alter_filename) {
      if((sz = strlen(buf)) >= 4 && !strncasecmp(&buf[sz - 4], ".flv", 4)) {
        sz -= 4;
      }
      snprintf(&buf[sz], sizeof(buf) - sz, "%d", outidx + 1);
    }
    //fprintf(stderr, "FLV[%d] '%s'\n", outidx, buf);
    if(capture_createOutPath(pFlvCtxts[outidx].recordFile.filename, 
                             sizeof(pFlvCtxts[outidx].recordFile.filename),
                             NULL, buf, NULL, MEDIA_FILE_TYPE_FLV) < 0) {
      LOG(X_ERROR("Invalid flv record output file name or directory '%s'."), pParams->flvrecord[outidx]);
      rc = -1;
      break;
    }

    pFlvCtxts[outidx].recordFile.fp = FILEOPS_INVALID_FP;
    //pFlvCtxts[outidx].recordMaxFileSz = pParams->flvrecordmaxfilesz;
    pFlvCtxts[outidx].overwriteFile = pParams->overwritefile;
    pFlvCtxts[outidx].requestOutIdx = outidx;
    pFlvCtxts[outidx].pnovid = &pStreamerCfg->novid;
    pFlvCtxts[outidx].pnoaud = &pStreamerCfg->noaud;
    pFlvCtxts[outidx].av.vid.pStreamerCfg = pStreamerCfg;
    pFlvCtxts[outidx].av.aud.pStreamerCfg = pStreamerCfg;

    if((rc = flvsrv_record_start(&pFlvCtxts[outidx], pLiveFmt)) < 0) {
      break;
    }
  }

  if(rc < 0) {
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      flvsrv_record_stop(&pFlvCtxts[outidx]);
    }
  }

  return rc;
#else // (ENABLE_RECORDING)
  return -1;
#endif // (ENABLE_RECORDING)
}

int vsxlib_setupMkvRecord(MKVSRV_CTXT_T *pMkvCtxts, 
                          STREAMER_CFG_T *pStreamerCfg,
                          const VSXLIB_STREAM_PARAMS_T *pParams) {

#if defined(ENABLE_RECORDING)
  int rc = 0;
  size_t sz;
  unsigned int outidx;
  STREAMER_OUTFMT_T *pLiveFmt;
  unsigned int maxMkvRecord = 0;
  int alter_filename = 0;
  char buf[VSX_MAX_PATH_LEN];
  buf[0] = '\0';

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    if(pParams->mkvrecord[outidx] && (outidx == 0 || pStreamerCfg->xcode.vid.out[outidx].active)) {
      maxMkvRecord++;
    }
  }

  //
  // Setup MKV server parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKVRECORD];
  if((*((unsigned int *) &pLiveFmt->max) = maxMkvRecord) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *) avc_calloc(maxMkvRecord, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_MKV_RECORD;

  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgMkv.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgMkv.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgMkv.growMaxPktLen;
  pLiveFmt->do_outfmt = 1;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pParams->mkvrecord[outidx] || (outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    //
    // Create output index specific named .mkv files
    //
    alter_filename = 0;
    if(outidx > 0 && buf[0] != '\0') {
      if(!strncasecmp(buf, pParams->mkvrecord[outidx], strlen(pParams->mkvrecord[outidx]))) {
        alter_filename = 1;
      }
    }
    strncpy(buf, pParams->mkvrecord[outidx], sizeof(buf));

    if(alter_filename) {
      if((sz = strlen(buf)) >= 4 && !strncasecmp(&buf[sz - 4], ".mkv", 4)) {
        sz -= 4;
      }
      snprintf(&buf[sz], sizeof(buf) - sz, "%d", outidx + 1);
    }
    //fprintf(stderr, "FLV[%d] '%s'\n", outidx, buf);
    if(capture_createOutPath(pMkvCtxts[outidx].recordFile.filename, 
                             sizeof(pMkvCtxts[outidx].recordFile.filename),
                             NULL, buf, NULL, MEDIA_FILE_TYPE_MKV) < 0) {
      LOG(X_ERROR("Invalid mkv record output file name or directory '%s'."), pParams->mkvrecord[outidx]);
      rc = -1;
      break;
    }

    pMkvCtxts[outidx].recordFile.fp = FILEOPS_INVALID_FP;
    //pMkvCtxts[outidx].recordMaxFileSz = pParams->mkvrecordmaxfilesz;
    pMkvCtxts[outidx].overwriteFile = pParams->overwritefile;
    pMkvCtxts[outidx].requestOutIdx = outidx;
    pMkvCtxts[outidx].pnovid = &pStreamerCfg->novid;
    pMkvCtxts[outidx].pnoaud = &pStreamerCfg->noaud;
    pMkvCtxts[outidx].av.vid.pStreamerCfg = pStreamerCfg;
    pMkvCtxts[outidx].av.aud.pStreamerCfg = pStreamerCfg;
    pMkvCtxts[outidx].faoffset_mkvpktz = pParams->faoffset_mkvpktz_record;

    if((rc = mkvsrv_record_start(&pMkvCtxts[outidx], pLiveFmt)) < 0) {
      break;
    }
  }

  if(rc < 0) {
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      mkvsrv_record_stop(&pMkvCtxts[outidx]);
    }
  }

  return rc;
#else // (ENABLE_RECORDING)
  return -1;
#endif // (ENABLE_RECORDING)
}

int vsxlib_setupRtmpPublish(STREAMER_CFG_T *pStreamerCfg,
                            const VSXLIB_STREAM_PARAMS_T *pParams) {
  int rc = 0;
  STREAMER_OUTFMT_T *pLiveFmt;
  unsigned int maxRtmpPublish = 1;

  //
  // Setup RTMP PUBLISH parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMPPUBLISH];

  if((*((unsigned int *) &pLiveFmt->max) = maxRtmpPublish) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *) avc_calloc(maxRtmpPublish, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_RTMP_PUBLISH;

  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgRtmp.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtmp.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtmp.growMaxPktLen;
  pLiveFmt->do_outfmt = 1;

  return rc;
}

