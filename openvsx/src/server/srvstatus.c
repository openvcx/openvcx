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

static int ctrl_status_show_output(STREAMER_CFG_T *pStreamerCfg, char *buf, unsigned int szbuf) {

  int rc = 0;
  unsigned int idx = 0;
  unsigned int outidx;
  unsigned int numRtmp = 0;
  unsigned int numRtmpTunneled = 0;
  unsigned int numRtsp = 0;
  unsigned int numRtspInterleaved = 0;
  unsigned int numTsLive = 0;
  unsigned int numFlvLive = 0;
  unsigned int numMkvLive = 0;
  unsigned int numActive = 0;
  char bpsdescr[256]; 

  bpsdescr[0] = '\0';

  if(pStreamerCfg) {
    if(pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].do_outfmt) {
      numActive += (numRtmp = pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].numActive);
      numRtmpTunneled += pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].numTunneled;
    }
    if(pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV].do_outfmt) {
      numActive += (numFlvLive = pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV].numActive);
    }
    if(pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV].do_outfmt) {
      numActive += (numMkvLive = pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV].numActive);
    }
  }

  if(pStreamerCfg && pStreamerCfg->running >= 0) {

    numActive += (numRtsp = pStreamerCfg->pRtspSessions->numActive);

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      numActive += (numTsLive += pStreamerCfg->liveQs[outidx].numActive);
      numActive += (numRtspInterleaved += pStreamerCfg->liveQ2s[outidx].numActive);
    }
  }


  if((rc = snprintf(&buf[idx], szbuf - idx, "status=OK&pid=%d", getpid())) > 0) {
    idx += rc;
  }

  if(numActive > 0 && pStreamerCfg->pMonitor->aggregate.bps > 0) {
    burstmeter_printBitrateStr(bpsdescr, sizeof(bpsdescr)-1, pStreamerCfg->pMonitor->aggregate.bps);
    if((rc = snprintf(&buf[idx], szbuf - idx, "&outputRate=%s", bpsdescr)) > 0) {
      idx += rc;
    }
  }

  //
  // numRtmp is superset of RTMP and RTMPT
  // numRtsp is superset of RTSP and RTSP-interleaved
  //
  if((rc = snprintf(&buf[idx], szbuf - idx, "&rtmp=%u&rtmpt=%u&rtspi=%u&rtsp=%u&tslive=%u&flvlive=%u&mkvlive=%u",
              numRtmp, numRtmpTunneled, numRtspInterleaved, numRtsp, numTsLive, numFlvLive, numMkvLive)) > 0) {
    idx += rc;
  }


  return rc;
}

static int ctrl_status_show_streamstats(STREAMER_CFG_T *pStreamerCfg, char *buf, unsigned int szbuf) {

  int rc = 0;

  if(pStreamerCfg->pMonitor) {
    rc = stream_monitor_dump_url(buf, szbuf, pStreamerCfg->pMonitor);
  }

  return rc;
}

static int ctrl_status_show_turn(STREAMER_CFG_T *pStreamerCfg, char *buf, unsigned int szbuf) {
  int rc = 0;
  unsigned int idx = 0;
  SDP_STREAM_DESCR_T *pVid;
  SDP_STREAM_DESCR_T *pAud;
  int statusCode = 0;
  char tmp[128];
  char bufRtp[256];
  char bufRtcp[256];

  bufRtp[0] = '\0';
  bufRtcp[0] = '\0';
  pVid = &pStreamerCfg->sdpInput.vid.common;
  pAud = &pStreamerCfg->sdpInput.aud.common;

  if(!pVid->available && !pAud->available) {
    statusCode = -1; 
  } else if((pVid->available && INET_PORT(pVid->iceCandidates[0].address) != 0) ||
            (pAud->available && INET_PORT(pAud->iceCandidates[0].address) != 0)) {

    // 
    // Write the RTP video TURN relay address:port
    //
    idx = 0;
    if(pVid->available && (rc = snprintf(&bufRtp[idx], sizeof(bufRtp) - idx, "%s:%d", 
                                         FORMAT_NETADDR(pVid->iceCandidates[0].address, tmp, sizeof(tmp)),
                                         htons(INET_PORT(pVid->iceCandidates[0].address)))) > 0) {
      idx += rc;
    }

    // 
    // Write the RTP audio TURN relay address:port if not using rtp-mux
    //
    if(pAud->available && (!pVid->available || 
                 INET_PORT(pAud->iceCandidates[0].address) != INET_PORT(pVid->iceCandidates[0].address)) &&
                 (rc = snprintf(&bufRtp[idx], sizeof(bufRtp) - idx, "%s%s:%d", 
                  pVid->available ? "," : "", 
                  FORMAT_NETADDR(pAud->iceCandidates[0].address, tmp, sizeof(tmp)),
                  htons(INET_PORT(pAud->iceCandidates[0].address)))) > 0) {
      idx += rc;
    }

    if((pVid->available && INET_PORT(pVid->iceCandidates[1].address) != 0 &&
        INET_PORT(pVid->iceCandidates[1].address) != INET_PORT(pVid->iceCandidates[0].address)) ||
       (pAud->available && INET_PORT(pAud->iceCandidates[1].address) != 0 &&
        INET_PORT(pAud->iceCandidates[1].address) != INET_PORT(pAud->iceCandidates[0].address))) {

      // 
      // Write the RTCP video TURN relay address:port if not using rtcp-mux
      //
      idx = 0;
      if(pVid->available && (rc = snprintf(&bufRtcp[idx], sizeof(bufRtcp) - idx, "%s:%d", 
                                          FORMAT_NETADDR(pVid->iceCandidates[1].address, tmp, sizeof(tmp)),
                                          htons(INET_PORT(pVid->iceCandidates[1].address)))) > 0) {
        idx += rc;
      }

      // 
      // Write the RTCP video TURN relay address:port if not using rtcp-mux and rtp-mux
      //
      if(pAud->available && (!pVid->available ||
            INET_PORT(pAud->iceCandidates[1].address) != INET_PORT(pVid->iceCandidates[1].address)) &&
            (rc = snprintf(&bufRtcp[idx], sizeof(bufRtcp) - idx, "%s%s:%d", 
            pVid->available ? "," : "", 
            FORMAT_NETADDR(pAud->iceCandidates[1].address, tmp, sizeof(tmp)),
            htons(INET_PORT(pAud->iceCandidates[1].address)))) > 0) {
        idx += rc;
      }
    }
  }

  //
  // Write the HTTP response content-body
  //
  idx = 0;
  if((rc = snprintf(&buf[idx], szbuf - idx, "status=%s", statusCode == 0 ? "OK" : "ERROR")) > 0) {
    idx += rc;
  }
  if(bufRtp[0] != '\0' && (rc = snprintf(&buf[idx], szbuf - idx, "&turnRelayRtp=%s", bufRtp)) > 0) {
    idx += rc;
  }
  if(bufRtcp[0] != '\0' && (rc = snprintf(&buf[idx], szbuf - idx, "&turnRelayRtcp=%s", bufRtcp)) > 0) {
    idx += rc;
  }

  return rc;
}

int srv_ctrl_status(CLIENT_CONN_T *pConn) {
  int rc = 0;
  enum HTTP_STATUS statusCode = HTTP_STATUS_OK;
  STREAMER_CFG_T *pStreamerCfg = NULL;
  const char *parg;
  int pip_id = 0;
  unsigned int idx;
  char tmp[128];
  char buf[2048];

  LOG(X_DEBUG("Received status command %s%s from %s:%d"), pConn->phttpReq->puri,
      http_req_dump_uri(pConn->phttpReq, buf, sizeof(buf)), FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), 
                        htons(INET_PORT(pConn->sd.sa)));

  if(pConn->pStreamerCfg0) {
    pStreamerCfg = pConn->pStreamerCfg0;
  } else if(pConn->pStreamerCfg1) {
    pStreamerCfg = pConn->pStreamerCfg1;
  } else {
    rc = -1;
    statusCode = HTTP_STATUS_SERVERERROR;
  }

  if(pStreamerCfg) {

    if((parg = conf_find_keyval(pConn->phttpReq->uriPairs, "turn"))) {

      if((parg = conf_find_keyval(pConn->phttpReq->uriPairs, "pipid"))) {
        pip_id = atoi(parg);

        if((idx = pip_getIndexById(pStreamerCfg, pip_id, 1)) >= 0 && idx < MAX_PIPS && pStreamerCfg->pStorageBuf) {
          //
          // Set pStreamerCfg to the corresponding PIP
          //
          pStreamerCfg = &((STREAM_STORAGE_T *) pStreamerCfg->pStorageBuf)->streamerCfgPips[idx];
          if(!pStreamerCfg->xcode.vid.pip.active) {
            LOG(X_ERROR("config PIP-%d[%d] not active"), pip_id, idx);
            rc = -1;
            snprintf(buf, sizeof(buf), "status=ERROR");
          }
        }
      }

      if(rc >= 0) {
        rc = ctrl_status_show_turn(pStreamerCfg, buf, sizeof(buf));
      }

    } else if((parg = conf_find_keyval(pConn->phttpReq->uriPairs, "streamstats"))) {

      rc = ctrl_status_show_streamstats(pStreamerCfg, buf, sizeof(buf));

    } else  {

      rc = ctrl_status_show_output(pStreamerCfg, buf, sizeof(buf));

    }

  }

  if(rc < 0) {
    statusCode = HTTP_STATUS_SERVERERROR;
    buf[0] = '\0';
  }

  rc = http_resp_send(&pConn->sd, pConn->phttpReq, statusCode, (unsigned char *) buf, strlen(buf));

  return rc;
}

