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


static int set_output(const char *output, const char *rtcpPorts, STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;
  unsigned int idx;
  char host[256];
  uint16_t ports[2];
  int portsRtcp[2];
  int numChannels = 0, numChannelsRtcp = 0;
  STREAM_DEST_CFG_T destCfg;

  memset(ports, 0, sizeof(ports));
  memset(portsRtcp, 0, sizeof(portsRtcp));
  host[0] = '\0';

  if(output) {
    capture_parseTransportStr(&output);
    if((numChannels = strutil_convertDstStr(output, host, sizeof(host), ports, 2, -1, NULL, 0)) < 0) {
      return -1;
    } else if(numChannels == 0) {
      return -1;
    }
  }

  if(rtcpPorts && (numChannelsRtcp = capture_parsePayloadTypes(rtcpPorts, &portsRtcp[0], &portsRtcp[1])) < 0) {
    return -1;
  }

  if(numChannels <= 0 && rtcpPorts <= 0) {
    return 0;
  }
  
  memset(&destCfg, 0, sizeof(destCfg));
  destCfg.haveDstAddr = 0;
  destCfg.u.pDstHost = host;

  for(idx = 0; idx < 2; idx++) {
    if(!pStreamerCfg->pRtpMultisRtp[0][idx].pdests[0].isactive) {
      continue;
    }
    if(ports[idx] > 0) {
      destCfg.dstPort = ports[idx];
    } else {
      destCfg.dstPort = 0;
    }
    if(portsRtcp[idx] > 0) {
      destCfg.dstPortRtcp = portsRtcp[idx];
    } else {
      destCfg.dstPortRtcp = 0;
    }
    if((rc = stream_rtp_updatedest(&pStreamerCfg->pRtpMultisRtp[0][idx].pdests[0], &destCfg)) < 0) {
      break;
    }
  }

  return rc;
}

int srv_ctrl_config(CLIENT_CONN_T *pConn) {
  int rc = 0;
  enum HTTP_STATUS statusCode = HTTP_STATUS_OK;
  char buf[128];
  const char *parg;
  STREAMER_CFG_T *pStreamerCfg = NULL;
  int reset = 0;
  int checklive = 0;
  int pip_id = 0;
  unsigned int idx;
  const char *rtcpPortsStr = NULL;
  const char *outputStr = NULL;
  enum STREAM_XCODE_FLAGS flags = 0;
  char tmp[64];
  //int outidx = 0;

  LOG(X_DEBUG("Received config command %s%s"), pConn->httpReq.puri,
      http_req_dump_uri(&pConn->httpReq, buf, sizeof(buf)));

  tmp[0] = '\0';

  if(pConn->pStreamerCfg0) {
    pStreamerCfg = pConn->pStreamerCfg0;
  } else if(pConn->pStreamerCfg1) {
    pStreamerCfg = pConn->pStreamerCfg1;
  } else {
    rc = -1;
    statusCode = HTTP_STATUS_SERVERERROR;
  }

  //
  // checklive=1 is set by an ajax client when polling a livestream to check if the
  // live vsx instance is still running and then potentially redirect to the
  // static archive link.
  //
  if(rc >= 0 && (parg = conf_find_keyval(pConn->httpReq.uriPairs, "checklive"))) {
    checklive = 1; 
  }

  //
  // Get any pip id that this configuration update is for
  //
  if(!checklive && rc >= 0 && 
      ((parg = conf_find_keyval(pConn->httpReq.uriPairs, "pip")) ||
       (parg = conf_find_keyval(pConn->httpReq.uriPairs, "pipid")))) {
    if(!pStreamerCfg->xcode.vid.overlay.havePip) {
      LOG(X_ERROR("config pipid given but no PIP(s) active"));
      rc = -1;
    } else {
      pip_id = atoi(parg);
      if((idx = pip_getIndexById(pStreamerCfg, pip_id, 1)) >= 0 && idx < MAX_PIPS && pStreamerCfg->pStorageBuf) {
        //
        // Set pStreamerCfg to the corresponding PIP
        //
        pStreamerCfg = &((STREAM_STORAGE_T *) pStreamerCfg->pStorageBuf)->streamerCfgPips[idx];
        if(!pStreamerCfg->xcode.vid.pip.active) {
          LOG(X_ERROR("config PIP-%d[%d] not active"), pip_id, idx);
          rc = -1;
        }
      }
      snprintf(tmp, sizeof(tmp), "PIP-%d[%d]", pip_id, idx);
    }
  }

  //
  // Ouput sendonly state
  //
  if(!checklive && rc >= 0 && (parg = conf_find_keyval(pConn->httpReq.uriPairs, "sendonly"))) {
    idx = atoi(parg);
    if(!pStreamerCfg->xcode.vid.pip.active) {
      LOG(X_ERROR("SDP sendonly=%d (hold) is only supported for PIP %s"), idx, tmp);
      rc = -1;
    } else if(idx && pStreamerCfg->cfgrtp.xmitType != SDP_XMIT_TYPE_SENDONLY) {
      pStreamerCfg->cfgrtp.xmitType = SDP_XMIT_TYPE_SENDONLY;
      LOG(X_INFO("Set SDP type to sendonly %s"), tmp);
    } else if(!idx && pStreamerCfg->cfgrtp.xmitType == SDP_XMIT_TYPE_SENDONLY) {
      pStreamerCfg->cfgrtp.xmitType = SDP_XMIT_TYPE_UNKNOWN;
      LOG(X_INFO("Cleared SDP from sendonly %s"), tmp);
    }

  }

  //
  // Update RTP/RTCP output destination
  //
  if(!checklive && rc >= 0) {
    rtcpPortsStr = conf_find_keyval(pConn->httpReq.uriPairs, "rtcpports");
  }

  if(!checklive && rc >= 0 && ((outputStr = conf_find_keyval(pConn->httpReq.uriPairs, "out")) ||
                 (outputStr = conf_find_keyval(pConn->httpReq.uriPairs, "output")))) {
  }

  if(outputStr || rtcpPortsStr) {
    set_output(outputStr, rtcpPortsStr, pStreamerCfg);
  }

  //
  // Full encoder reset
  //
  if(!checklive && rc >= 0 && (parg = conf_find_keyval(pConn->httpReq.uriPairs, "reset"))) {
    if((reset = atoi(parg)) > 0) {
      flags |= STREAM_XCODE_FLAGS_RESET;
    }
  }

  //if(rc >= 0 && (parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_XCODE))) {
  //  if((outidx = atoi(parg)) < 0 || outidx >= IXCODE_VIDEO_OUT_MAX) {
  //    rc = -1;
  //  }
  //}

  //TODO: allow config for pip index
  //TODO: allow 'sendonly' for pip index

  if(!checklive) {
    parg = conf_find_keyval(pConn->httpReq.uriPairs, PIP_KEY_XCODE);
    if(rc >= 0 && (flags != 0 || parg)) {

      //LOG(X_DEBUG("xcode update command url: '%s'"), pConn->httpReq.url);
      if((rc = xcode_update(pStreamerCfg, parg, flags)) < 0) {
        statusCode = HTTP_STATUS_SERVERERROR;
      }
    }
  }

  snprintf(buf, sizeof(buf), "config=%s", rc >= 0 ? "OK" : "ERROR");

  rc = http_resp_send(&pConn->sd, &pConn->httpReq, statusCode, (unsigned char *) buf, strlen(buf));

  return rc;
}

