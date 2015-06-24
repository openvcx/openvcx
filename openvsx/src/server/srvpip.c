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

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

int srv_ctrl_pip(CLIENT_CONN_T *pConn) {
  int rc = 0; 
  int rcTmp;
  enum HTTP_STATUS statusCode = HTTP_STATUS_OK;
  const char *parg;
  PIP_CFG_T pipCfg;
  IXCODE_VIDEO_CROP_T crop;
  IXCODE_PIP_MOTION_T *pipMotion = NULL;
  int idx = 0;
  int pip_id = 0;
  int do_stop = 0;
  int do_start = 0;
  int do_update = 0;
  int do_status = 0;
  int do_crop = 0;
  STUN_POLICY_T stunPolicy;
  int activePipIds[MAX_PIPS];
  const char *do_pipconf = NULL;
  unsigned int idxResp;
  char buf[1024];
  char respExtra[512];
  STREAMER_CFG_T *pCfg = NULL;
  char *strxcodebuf = NULL;

  LOG(X_DEBUG("Received PIP command %s%s from %s:%d"), pConn->httpReq.puri,  
      http_req_dump_uri(&pConn->httpReq, buf, sizeof(buf)), inet_ntoa(pConn->sd.sain.sin_addr), 
      htons(pConn->sd.sain.sin_port));

  buf[0] = '\0';
  respExtra[0] = '\0';

  if(pConn->pStreamerCfg0 && pConn->pStreamerCfg0->running >= 0) {
    pCfg = (STREAMER_CFG_T *) pConn->pStreamerCfg0;
  } else if(pConn->pStreamerCfg1 && pConn->pStreamerCfg1->running >= 0) {
    pCfg = (STREAMER_CFG_T *) pConn->pStreamerCfg1;
  }

  if(pCfg) {

    memset(&crop, 0, sizeof(crop));

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_PAD_TOP))) {
      crop.padTop = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_PAD_BOTTOM))) {
      crop.padBottom = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_PAD_LEFT))) {
      crop.padLeft = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_PAD_RGB))) {
      strutil_read_rgb(parg, crop.padRGB);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_PAD_RIGHT))) {
      crop.padRight = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_CROP_TOP))) {
      crop.cropTop = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_CROP_BOTTOM))) {
      crop.cropBottom = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_CROP_LEFT))) {
      crop.cropLeft = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_CROP_RIGHT))) {
      crop.cropRight = atoi(parg);
      do_crop = 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_PAD_ASPECT_RATIO))) {
      crop.padAspectR = atoi(parg);
      do_crop = 1;
    }

    if(do_crop) {
      if((rc = xcode_set_crop_pad(&crop, &pCfg->xcode.vid.out[0], crop.padAspectR, 
                                  pCfg->xcode.vid.out[0].resOutH, pCfg->xcode.vid.out[0].resOutV, 0, 0)) < 0) {
        LOG(X_ERROR("Invalid picture pad / crop dimensions"));
      } else {
        //TODO: updating these params does not work via xcode ipc
        //TODO: this should be atomic w/ respect to xcoder 
        memcpy(&pCfg->xcode.vid.out[0].crop, &crop, sizeof(pCfg->xcode.vid.out[0].crop));
      }
    }

    memset(&pipCfg, 0, sizeof(pipCfg));
    pipCfg.rtpPktzMode = -1;
    pipCfg.dtls_handshake_server = -1;

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipstatus"))) {
      do_status = 1;
      pip_id = atoi(parg);
    } else if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipstop"))) {
      do_stop = 1;
      pip_id = atoi(parg);
    } else if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipstart"))) {
      if(atoi(parg) > 0) {
        do_start = 1;
      }
    } else if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipupdate"))) {
      // start a PIP which was previously started with 'delaystart' parameter
      if((pip_id = atoi(parg)) > 0) {
        do_update = 1;
      }
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipconf"))) {
      do_pipconf = parg;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipxcode")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "xcode"))) {
      pipCfg.strxcode = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pip")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "in")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "input"))) {
      pipCfg.input = parg;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "out")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "output"))) {
      pipCfg.output = parg;
//pipCfg.output="dtlssrtp://127.0.0.1:5000,5008";
//pipCfg.output="srtp://127.0.0.1:5000,5000";
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "tslive"))) {
      pipCfg.tsliveaddr = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rtppayloadtype"))) {
      pipCfg.output_rtpptstr = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rtpssrc"))) {
      pipCfg.output_rtpssrcsstr = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "mtu"))) {
      pipCfg.mtu = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rtppktzmode"))) {
      pipCfg.rtpPktzMode = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rtcpports"))) {
      pipCfg.rtcpPorts = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "srtpkey"))) {
      pipCfg.srtpKeysBase64 = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtls"))) {
      pipCfg.use_dtls = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtlssrtp"))) {
      pipCfg.use_dtls = pipCfg.use_srtp = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtlsserver"))) {
      pipCfg.dtls_handshake_server = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtlsclient"))) {
      pipCfg.dtls_handshake_server = !atoi(parg);
    }
    //if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtlsoutserverkey"))) {
    //  pipCfg.dtls_xmit_serverkey = atoi(parg);
   // }
    //if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtlsinserverkey")) ||
    //   (parg = conf_find_keyval(pConn->httpReq.uriPairs, "dtlsserverkey"))) {
    //  pipCfg.dtls_capture_serverkey = atoi(parg);
    //}
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "stunrespond"))) {
      if((stunPolicy = atoi(parg)) != STUN_POLICY_ENABLED) {
        stunPolicy = STUN_POLICY_NONE;
      }
      if((pipCfg.stunBindingResponse = stunPolicy) != STUN_POLICY_NONE) {
        pipCfg.stunRespUseSDPIceufrag = 1;
      }
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "stunrequestuser"))) {
      pipCfg.stunReqUsername = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "stunrequestpass"))) {
      pipCfg.stunReqPass = parg;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "stunrequestrealm"))) {
      pipCfg.stunReqRealm = parg;
    }
    if(pipCfg.stunBindingRequest == STUN_POLICY_NONE && (pipCfg.stunReqUsername || pipCfg.stunReqPass || pipCfg.stunReqRealm)) {
      pipCfg.stunBindingRequest = STUN_POLICY_XMIT_IF_RCVD;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "stunrequest"))) {
      stunPolicy = atoi(parg);
      pipCfg.stunBindingRequest = stunPolicy;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnuser"))) {
      pipCfg.turnCfg.turnUsername = parg; 
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnpass"))) {
      pipCfg.turnCfg.turnPass = parg; 
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnrealm"))) {
      pipCfg.turnCfg.turnRealm = parg; 
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnserver"))) {
      pipCfg.turnCfg.turnServer = parg; 
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnpeer"))) {
      pipCfg.turnCfg.turnPeerRelay = parg; 
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnsdp"))) {
      pipCfg.turnCfg.turnSdpOutPath = parg; 
    }

    //if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "turnout"))) {
    //  pipCfg.turnCfg.turnOutput = parg; 
    //} else if(pipCfg.turnCfg.turnServer) {
    //  pipCfg.turnCfg.turnOutput = pipCfg.turnCfg.turnServer;
    //}

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "delaystart"))) {
      pipCfg.delayed_output = atoi(parg);
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "abrself"))) {
      pipCfg.abrSelf = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "abrauto")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "abr"))) {
      pipCfg.abrAuto = atoi(parg);
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "piplayout")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "layout"))) {
      pipCfg.layoutType = pip_str2layout(parg);
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipx")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipxleft")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipxright"))) {
      pipCfg.posx = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipy")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipytop")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipybottom"))) {
      pipCfg.posy = atoi(parg);
    }

    // PIP_KEY_ZORDER "zorder"
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipzorder")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "zorder"))) {
      pipCfg.zorder = atoi(parg);
    }

    // PIP_KEY_NOAUDIO
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipnoaudio")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "noaud")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "noaudio"))) {
      pipCfg.noaud = 1;
    }

    // PIP_KEY_NOVIDEO
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipnovideo")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "novid")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "novideo"))) {
      pipCfg.novid = 1;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "duplicate"))) {
      pipCfg.allowDuplicate = 1;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "startchime"))) {
      pipCfg.startChimeFile = parg;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "stopchime"))) {
      pipCfg.stopChimeFile = parg;
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rtpretransmit"))) {
      pipCfg.nackRtpRetransmitVideo = atoi(parg);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rembxmitmaxrate"))) {
      pipCfg.apprembRtcpSendVideoMaxRateBps =  (unsigned int) strutil_read_numeric(optarg, 0, 1000, 0);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rembxmitminrate"))) {
      pipCfg.apprembRtcpSendVideoMinRateBps =  (unsigned int) strutil_read_numeric(optarg, 0, 1000, 0);
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "rembxmitforce"))) {
      pipCfg.apprembRtcpSendVideoForceAdjustment = atoi(parg);
    }

    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipalphamax"))) {
      pipCfg.alphamax_min1 = atoi(parg) + 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipalphamin"))) {
      pipCfg.alphamin_min1 = atoi(parg) + 1;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipbefore"))) {
      if(atoi(parg) > 0) {
        pipCfg.flags |= PIP_FLAGS_INSERT_BEFORE_SCALE;
      }
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipxright"))) {
      pipCfg.flags |= PIP_FLAGS_POSITION_RIGHT;
    }
    if((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipybottom"))) {
      pipCfg.flags |= PIP_FLAGS_POSITION_BOTTOM;
    }

    //
    // Read the PIP configuration from a fiel
    //
    if(do_pipconf) {
      pipCfg.cfgfile = do_pipconf;

      //if((idx = pip_getIndexById(pCfg,  pip_id, 1)) >= 0 && idx < MAX_PIPS && pCfg->pStorageBuf) {
      //  strxcodebuf = ((STREAM_STORAGE_T *) pCfg->pStorageBuf)->pipxcodeStrbuf[idx];

      if(pCfg->pStorageBuf) {
        //TODO: if we're all going to use the same buffer (temporarily) we need some locking here...
        strxcodebuf = ((STREAM_STORAGE_T *) pCfg->pStorageBuf)->pipxcodeStrbuf[0];
      }

      if((pipMotion = pip_read_conf(&pipCfg, pCfg->status.pathFile, strxcodebuf))) {
        do_start = 1;
        do_stop = 0;
      } else {

      }
    }

    if(do_status) {

      if(pip_id == -1) {
        if((rc = pip_getActiveIds(pCfg, activePipIds, 1)) > 0) {
          idxResp = 0;
          if((rcTmp = snprintf(respExtra, sizeof(respExtra), "&ids=")) > 0) {
            idxResp += rcTmp;
          }
          for(idx = 0; idx < rc; idx++) {
            if((rcTmp = snprintf(&respExtra[idxResp], sizeof(respExtra) - idxResp, "%s%d", 
                                 idx > 0 ? "," : "", activePipIds[idx])) > 0) { 
              idxResp += rcTmp;
            }
          }
        }
      } else if((idx = pip_getIndexById(pCfg, pip_id, 1)) >= 0) {
        rc = pip_id;
      } else {
        rc = -1;
      }

    } else if(do_update) {

      if((idx = pip_getIndexById(pCfg, pip_id, 1)) >= 0) {
        rc = pip_update(&pipCfg, pCfg, pip_id);
      } else {
        do_update = 0;
      }

    } else if(do_stop) {

      if((rc = pip_stop(pCfg, pip_id)) < 0) {
        LOG(X_ERROR("Failed to stop PIP[%d]"), pip_id);
      }

    } else if(do_start && rc >= 0) {

      rc = pip_start(&pipCfg, pCfg, pipMotion);

    }

    //
    // The html body response
    //
    snprintf(buf, sizeof(buf), "code=%d%s", rc, respExtra[0] != '\0' ? respExtra : ""); 

  } else {
    rc = -1;
  }
 
  if(rc < 0) {
    statusCode = HTTP_STATUS_SERVERERROR;
  }

  rc = http_resp_send(&pConn->sd, &pConn->httpReq, statusCode, (unsigned char *) buf, strlen(buf));

  LOG(X_DEBUG("Sent PIP response '%s' to %s:%d"), buf, inet_ntoa(pConn->sd.sain.sin_addr), htons(pConn->sd.sain.sin_port));

  return rc;
}
#endif // XCODE_HAVE_PIP
