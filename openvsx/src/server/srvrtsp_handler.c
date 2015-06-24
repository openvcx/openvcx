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

#define RTSP_DESCRIBE_SDP_UPDATE_TM_SEC     8

//#define TESTQT 1

struct STATUS_CODE_MAP {
  enum HTTP_STATUS      code;
  const char           *str;
};

static const char *rtsp_lookup_statuscode(enum RTSP_STATUS statusCode) {

  unsigned int idx = 0;

  //
  // Check RTSP specific error codes
  //
  static const struct STATUS_CODE_MAP statusCodes[] = {
                               {  RTSP_STATUS_UNSUPPORTEDTRANS , "Unsupported Transport" },
                               { 0, NULL }
                               };

  while(statusCodes[idx].str) {
    if(statusCode == statusCodes[idx].code) {
      return statusCodes[idx].str;
    }
    idx++;
  }

  return http_lookup_statuscode(statusCode);
}

static int rtsp_resp_sendhdr(SOCKET_DESCR_T *pSd,
                             enum RTSP_STATUS statusCode,
                             int cseq, 
                             const RTSP_SESSION_T *pSession,
                             int includeDate, 
                             const char *cache_control,
                             const char *xtraHdrs) {
  int rc;
  unsigned int sz = 0;
  char buf[1024];
  char tmp[128];
  const char *statusCodeStr = rtsp_lookup_statuscode(statusCode);

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz,
                    "%s %d %s\r\n"
                    "Server: %s\r\n",
                RTSP_VERSION_DEFAULT, statusCode, statusCodeStr,
                vsxlib_get_appnamewwwstr(tmp, sizeof(tmp)))) < 0) {
    return -1; 
  }
  sz += rc;

  if(cseq >= 0) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %d\r\n", RTSP_HDR_CSEQ, cseq)) < 0) {
      return -1;
    }
     sz += rc;
  }

  if(cache_control) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n", 
                      RTSP_HDR_CACHE_CONTROL, cache_control)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(includeDate) {
    if(http_format_date(tmp, sizeof(tmp), 0) < 0 ||
      (rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n", 
                      RTSP_HDR_DATE, tmp)) < 0) {
      return -1;
    }
    sz += rc;
    if(cache_control) {
      if(http_format_date(tmp, sizeof(tmp), 0) < 0 ||
        (rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n", 
                        RTSP_HDR_EXPIRES, tmp)) < 0) {
        return -1;
      }
      sz += rc;

    }
  }

  if(pSession && pSession->sessionid[0] != '\0') {
    
    if(pSession->expireSec > 0 && pSession->expireSec != RTSP_SESSION_IDLE_SEC_SPEC) {
      snprintf(tmp, sizeof(tmp), ";%s=%u", RTSP_CLIENT_TIMEOUT, pSession->expireSec);
    } else {
      tmp[0] = '\0';
    }

    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s%s\r\n", 
                      RTSP_HDR_SESSION, pSession->sessionid, tmp)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(xtraHdrs && xtraHdrs[0] != '\0') {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s", xtraHdrs)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "\r\n")) < 0) {
    return -1;
  }
  sz += rc;

  VSX_DEBUG_RTSP( 
    LOG(X_DEBUG("RTSP - Sending header %d bytes"), sz);
    LOGHEXT_DEBUG(buf, sz);
  );

  if((rc = netio_send(&pSd->netsocket,  &pSd->sain, (unsigned char *) buf, sz)) < 0) {
    LOG(X_ERROR("Failed to send RTSP response headers %d bytes"), sz);
  }

  return rc;
}

int rtsp_authenticate(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {

  enum RTSP_STATUS statusCode = RTSP_STATUS_UNAUTHORIZED;
  int ok_auth = 0;
  int rc = 0;
  const AUTH_CREDENTIALS_STORE_T *pAuthStore = NULL;
  char buf[1024];

  buf[0] = '\0';

  //VSX_DEBUG_RTSP(LOG(X_DEBUGV("RTSP - authenticate 0x%x 0x%x"), pRtsp->pListenCfg, pRtsp->pListenCfg ? pRtsp->pListenCfg->pAuthStore : NULL));

  //if(pRtsp->pClientSession && pRtsp->pCap) {
    //pAuthStore = &pRtsp->pCap->pcommon->creds[0];
  if(pRtsp->pListenCfg && pRtsp->pListenCfg->pAuthStore) {
    pAuthStore = pRtsp->pListenCfg->pAuthStore;
  } else {
    pAuthStore = &pRtsp->pStreamerCfg->creds[STREAMER_AUTH_IDX_RTSP].stores[0];
  }

  if((ok_auth = srvauth_authenticate(pAuthStore, &pReq->hr, NULL, NULL, NULL, buf, sizeof(buf))) != 0) {

    if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq,
                         pRtsp->pSession, 0, NULL, buf) < 0) {
      rc = -1;
    }
  }

  return ok_auth;
}

int rtsp_handle_options(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  size_t sz = 0;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;
  char buf[1024];

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - OPTIONS handler: '%s'"), pReq->hr.url) );

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "Public: "RTSP_REQ_SETUP", "
                             RTSP_REQ_TEARDOWN", "RTSP_REQ_PLAY", "RTSP_REQ_PAUSE", "
                             RTSP_REQ_OPTIONS)) >= 0) {
    sz += rc;
  }
  if(pRtsp->pClientSession) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, ", "RTSP_REQ_ANNOUNCE)) >= 0) {
      sz += rc;
    }
  } else {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, ", "RTSP_REQ_DESCRIBE)) >= 0) {
      sz += rc;
    }
  }
  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "\r\n")) >= 0) {
    sz += rc;
  }

  //
  // Update the session idle timer
  //
  if(pRtsp->pSession) {
    gettimeofday(&pRtsp->pSession->tvlastupdate, NULL);
    //fprintf(stderr, "rtsp_candle_options updated tvlastupdate %u\n", pRtsp->pSession->tvlastupdate.tv_sec);
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 0, NULL, buf) < 0) {
    rc = -1;
  }

  return rc;
}

int rtsp_handle_describe_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  char bufhdrs[1024];
  RTSP_CLIENT_SESSION_T *pSession = NULL;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - DESCRIBE handler: '%s'"), pReq->hr.url) );

  if(!(pSession = pRtsp->pClientSession)) {
    return -1;
  }

  snprintf(bufhdrs, sizeof(bufhdrs), "%s: Server setup for ANNOUNCE.\r\n", RTSP_HDR_SRVERROR);
  rc = -1;

  if(rc >= 0) {

  } else if(statusCode != RTSP_STATUS_UNSUPPORTEDTRANS) {

    if(statusCode < 400) {
      statusCode = RTSP_STATUS_SERVERERROR;
    }
    LOG(X_ERROR("RTSP error %s %d"), bufhdrs, statusCode);
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq,
                       pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    rc = -1;
  }

  return rc;
}

int rtsp_wait_for_sdp(STREAMER_CFG_T *pStreamerCfg, unsigned int outidx, int sec) {
                     
  int sdpWait = 0;
  struct timeval tv0, tv;

  if(sec <= 0) {
    sec = RTSP_DESCRIBE_SDP_UPDATE_TM_SEC;
  }

  gettimeofday(&tv0, NULL);

  //
  // Ensure that the SDP contents are available to prevent returning
  // empty or incorrect SDP. 
  //

  while((pStreamerCfg->sdpActions[outidx].updateVid && !pStreamerCfg->sdpActions[outidx].vidUpdated) ||
        (pStreamerCfg->sdpActions[outidx].updateAud && !pStreamerCfg->sdpActions[outidx].audUpdated))  {

    if(sdpWait == 0) {

      //
      // Request an IDR from the underling encoder
      //
      streamer_requestFB(pStreamerCfg, outidx, ENCODER_FBREQ_TYPE_FIR, 0, REQUEST_FB_FROM_LOCAL);

      LOG(X_DEBUG("RTSP waiting for SDP update out:[%d] vid:%d,%d aud:%d,%d"), outidx,
          pStreamerCfg->sdpActions[outidx].updateVid, pStreamerCfg->sdpActions[outidx].vidUpdated,
          pStreamerCfg->sdpActions[outidx].updateAud, pStreamerCfg->sdpActions[outidx].audUpdated);
    }

    gettimeofday(&tv, NULL);
    if((tv.tv_sec - tv0.tv_sec) > sec) {
      LOG(X_ERROR("RTSP timeout while waiting for SDP update"));
      return -1;
    }
    usleep(50000);
    sdpWait = 1;

  } // end of while(...

  if(sdpWait) {
    LOG(X_WARNING("RTSP DESCRIBE waited %d sec for SDP update"), (tv.tv_sec - tv0.tv_sec));
  }

  return 0;
}


int rtsp_prepare_sdp(const STREAMER_CFG_T *pStreamerCfg, unsigned int outidx, 
                     char *bufsdp, unsigned int *pszsdp, 
                     char *bufhdrs, unsigned int *pszhdrs,
                     SRTP_CTXT_KEY_TYPE_T srtpKts[STREAMER_PAIR_SZ], SDP_DESCR_T *pSdp) {
  int rc = 0;
  SRTP_CTXT_T srtpsTmp[STREAMER_PAIR_SZ]; 
  struct in_addr sdp_connect;
  unsigned int srtpidx = 0;
  unsigned int numchannels = 0;
  unsigned int contentLen = 0;
  unsigned int idxhdrs = 0;

  if(!pStreamerCfg || !bufsdp || !pszsdp || !bufhdrs || !pszhdrs || !pSdp) {
    return -1;
  }

  bufhdrs[0] = '\0';
  bufsdp[0] = '\0';
  memcpy(pSdp, &pStreamerCfg->sdpsx[1][outidx], sizeof(SDP_DESCR_T));

  if(pStreamerCfg->novid) {
    pSdp->vid.common.available = 0;
  }
  if(pStreamerCfg->noaud) {
    pSdp->aud.common.available = 0;
  }

  if(pSdp->vid.common.available) {
    //snprintf(sdpAttrib[0], sizeof(sdpAttrib[0]), "control:"RTSP_TRACKID"=%u", RTSP_TRACKID_VID);
    //pSdp->vid.common.attribCustom[0] = sdpAttrib[0];
    numchannels++;
    pSdp->vid.common.controlId = RTSP_TRACKID_VID;
    pSdp->vid.common.port = 0;
    pSdp->vid.common.portRtcp = 0;
    if((pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX)) {
      pSdp->vid.common.rtcpmux = 1;
    }
  }

  if(pSdp->aud.common.available) {
    //snprintf(sdpAttrib[1], sizeof(sdpAttrib[1]), "control:"RTSP_TRACKID"=%u", RTSP_TRACKID_AUD);
    //pSdp->aud.common.attribCustom[0] = sdpAttrib[1];
    numchannels++;
    pSdp->aud.common.controlId = RTSP_TRACKID_AUD;
    pSdp->aud.common.port = 0;
    pSdp->aud.common.portRtcp = 0;
    if((pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX)) {
      pSdp->aud.common.rtcpmux = 1;
    }
  }

  memset(srtpKts, 0, STREAMER_PAIR_SZ * sizeof(srtpKts[0]));

  //
  // Create SRTP keys
  //
  if(pStreamerCfg->pRtspSessions->rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_SRTP) {

    rc = vsxlib_init_srtp(srtpsTmp, numchannels, NULL, NULL, NULL);

    if(rc >= 0 && pSdp->vid.common.available) {
      pSdp->vid.common.transType = SDP_TRANS_TYPE_SRTP_SDES_UDP;
      rc = sdputil_initsrtp(&pSdp->vid.common.srtp, &srtpsTmp[0]);
      if(rc >= 0) {
        memcpy(&srtpKts[srtpidx].k, &srtpsTmp[srtpidx].kt.k, sizeof(srtpKts[srtpidx].k));
        memcpy(&srtpKts[srtpidx].type, &srtpsTmp[srtpidx].kt.type, sizeof(srtpKts[srtpidx].type));
      }
    }
    if(rc >= 0 && pSdp->aud.common.available) {
      srtpidx = pSdp->vid.common.available ? 1 : 0;
      pSdp->aud.common.transType = SDP_TRANS_TYPE_SRTP_SDES_UDP;
      rc = sdputil_initsrtp(&pSdp->aud.common.srtp, &srtpsTmp[srtpidx]);
      if(rc >= 0) {
        memcpy(&srtpKts[srtpidx].k, &srtpsTmp[srtpidx].kt.k, sizeof(srtpKts[srtpidx].k));
        memcpy(&srtpKts[srtpidx].type, &srtpsTmp[srtpidx].kt.type, sizeof(srtpKts[srtpidx].type));
      }
    }
  }

  //
  // Set the SDP 'c=' to 0.0.0.0
  // TODO: this may need to be set to an RTSP multicast address if IN_MULTICAST...
  //
  if(rc >= 0) {
    //sdp_connect.s_addr = inet_addr(pSdp->.c.iphost);
    memset(&sdp_connect, 0, sizeof(sdp_connect));
    sdp_connect.s_addr = INADDR_ANY;
    snprintf(pSdp->c.iphost, sizeof(pSdp->c.iphost), "%s", inet_ntoa(sdp_connect));

#if defined(TESTQT)
    pSdp->vid.common.attribCustom[0] = "3GPP-Adaptation-Support:1";
    pSdp->aud.common.attribCustom[0] = "3GPP-Adaptation-Support:1";
#endif // TESTQ

  }

  //
  // Dump the SDP text and write the Content-Type and Content-Length headers
  //
  if(rc >= 0 && (rc = sdp_dump(pSdp, bufsdp, *pszsdp)) >= 0) {

    contentLen = rc;
    rc = snprintf(&bufhdrs[idxhdrs], *pszhdrs - idxhdrs, "%s: %s\r\n"
                    "%s: %u\r\n",
                    RTSP_HDR_CONTENT_TYPE, CONTENT_TYPE_SDP, RTSP_HDR_CONTENT_LEN, contentLen);
    if(rc >= 0) {
      idxhdrs += rc;
    }

    LOG(X_DEBUG("Prepared SDP: %s"), bufsdp);

  }

  //
  // Set output variables
  //
  *pszsdp = contentLen;
  *pszhdrs = idxhdrs;

  return rc;
}

int rtsp_handle_describe(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  char bufsdp[4096];
  char bufhdrs[1024];
  size_t szt;
  SDP_DESCR_T sdp;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;
  unsigned int sdplen = sizeof(bufsdp);
  unsigned int idxhdrs = sizeof(bufhdrs);
  int outidx = 0;

  bufhdrs[0] = '\0';

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - DESCRIBE handler: '%s'"), pReq->hr.url) );

 if(pRtsp->pClientSession) {
    LOG(X_ERROR("RTSP DESCRIBE request not handled in server capture mode"));
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Expecting ANNOUNCE while in server capture mode.\r\n", RTSP_HDR_SRVERROR);
    rc = -1;
  }

  //
  // Obtain the requested xcode output outidx
  //
  if(rc >= 0 && ((outidx = outfmt_getoutidx(pReq->hr.puri, NULL)) < 0 || outidx >= IXCODE_VIDEO_OUT_MAX)) {
    outidx = 0;
  }

  if(rc >= 0 && (rc = rtsp_wait_for_sdp(pRtsp->pStreamerCfg, outidx, RTSP_DESCRIBE_SDP_UPDATE_TM_SEC)) < 0) {

    snprintf(bufhdrs, sizeof(bufhdrs),
              "%s: Input contents for SDP not yet available.  Try again later.\r\n", RTSP_HDR_SRVERROR);

    if(rtsp_resp_sendhdr(pRtsp->pSd, RTSP_STATUS_SERVICEUNAVAIL, pReq->cseq,
                         pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    }
  }

  if(rc >= 0 && (rc = rtsp_prepare_sdp(pRtsp->pStreamerCfg, outidx, bufsdp, &sdplen,
                                       bufhdrs, &idxhdrs, pRtsp->srtpKts, &sdp)) < 0) {

  }

  if(rc < 0) {

    if(bufhdrs[0] == '\0') {
      snprintf(bufhdrs, sizeof(bufhdrs), "%s: Unable to create SDP\r\n", RTSP_HDR_SRVERROR);
    }
    sdplen = 0;
    statusCode = RTSP_STATUS_SERVERERROR;
    LOG(X_ERROR("RTSP DESCRIBE error %s"), bufhdrs);

  } else {

    //
    // Update the session idle timer
    //
    if(pRtsp->pSession) {
      gettimeofday(&pRtsp->pSession->tvlastupdate, NULL);
    }

    //
    // Set the Content-Base header
    //
    if((szt = strlen(pReq->hr.url)) == 0) {
      szt = 1;
    }
    rc = snprintf(&bufhdrs[idxhdrs], sizeof(bufhdrs) - idxhdrs, "%s: %s%s\r\n", RTSP_HDR_CONTENT_BASE, 
         pReq->hr.url, pReq->hr.url[szt] == '/' ? "" : "/");
    if(rc >= 0) {
      idxhdrs += rc;
    }

    //fprintf(stderr, "SDP:'%s' vid.avail:%d aud:avail:%d updateVid:%d,%d\n", bufsdp, sdp.vid.common.available, sdp.vid.common.available, pRtsp->pStreamerCfg->action.sdpAction.updateVid, pRtsp->pStreamerCfg->action.sdpAction.vidUpdated);

  }

#if defined(TESTQT)
    avc_dumpHex(stderr, bufsdp, strlen(bufsdp) +1, 1);

    idxhdrs += snprintf(&bufhdrs[idxhdrs], sizeof(bufhdrs) - idxhdrs, 
                "x-Transport-Options: late-tolerance=1.400000\r\n"
                "x-Retransmit: our-retransmit\r\n"
                "x-Dynamic-Rate: 1\r\n");
#endif // TESTQT

  //
  // Send the DESCRIBE reponse headers
  //
  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    rc = -1;
  }

  //
  // Send the SDP content body
  //
  if(rc >= 0 && sdplen > 0) {

    VSX_DEBUG_RTSP(
      LOG(X_DEBUG("Sending SDP body %d bytes"), sdplen);
      LOGHEXT_DEBUG(bufsdp, sdplen);
    )

    if((rc = netio_send(&pRtsp->pSd->netsocket,  &pRtsp->pSd->sain, 
                    (unsigned char *) bufsdp, sdplen)) < 0) {
      LOG(X_ERROR("Failed to send RTSP DESCRIBE response SDP %d bytes"), sdplen);
    }
  }

  return rc;
}

static int get_transport_ports(const char *strPorts, uint16_t ports[2]) {
  const char *p = strPorts;
  const char *p2 = strPorts;
  char tmp[16];
  unsigned int len;
  unsigned int idxPorts;

  for(idxPorts = 0; idxPorts < 2; idxPorts++) {
    while(*p2 != '-' && *p2 != '\0') {
      p2++;
    }
    if((len = (p2 - p)) > sizeof(tmp) - 1) {
      len = sizeof(tmp) - 1;
    }
    if(len <= 0) {
      break;
    }
    memcpy(tmp, p, len);
    tmp[len] = '\0';
    ports[idxPorts] = atoi(tmp);
    while(*p2 == '-') {
      p2++;
    } 
    p = p2;
  }

  return (int) idxPorts;
}

int rtsp_get_transport_keyvals(const char *strTransport, RTSP_SESSION_PROG_T *pProg, int servermode) {
  const char *p = strTransport;
  const char *p2;
  const char *parg;
  KEYVAL_PAIR_T kv;
  int rc;
  int numkv = 0;

  if(!strTransport || !pProg) {
    return -1;
  }

  do {

    while(*p == ' ') {
      p++;
    }

    p2 = p;
    while(*p2 != ';' && *p2 != '\r' && *p2 != '\n' && *p2 != '\0') {
      p2++;
    }

    if(conf_parse_keyval(&kv, p, p2 - p, '=', 0) > 0) {
      if(conf_find_keyval(&kv, RTSP_TRANS_MULTICAST)) {
        pProg->multicast = 1;
      } else if(!servermode && (parg = conf_find_keyval(&kv, RTSP_SERVER_PORT))) {

        if((rc = get_transport_ports(parg, pProg->ports)) > 0) {
          pProg->numports = rc; 
        } else {
          pProg->numports = -1; 
        }

      } else if(servermode &&  ((parg = conf_find_keyval(&kv, RTSP_CLIENT_PORT)) ||
                                (parg = conf_find_keyval(&kv, RTSP_TRANS_INTERLEAVED)))) {

        if((rc = get_transport_ports(parg, pProg->ports)) > 0) {
          pProg->numports = rc; 
        } else {
          pProg->numports = -1; 
        }

      } else if((parg = conf_find_keyval(&kv, SDP_TRANS_RTP_AVP)) ||
                (parg = conf_find_keyval(&kv, SDP_TRANS_RTP_AVP_UDP)) ||
                (parg = conf_find_keyval(&kv, SDP_TRANS_SRTP_AVP)) ||
                (parg = conf_find_keyval(&kv, SDP_TRANS_RTP_AVP_TCP))) {
         strncpy((char *) pProg->transStr, kv.key, sizeof(pProg->transStr));
      }
      //fprintf(stderr, "key:'%s' '%s'\n", kv.key, kv.val);
      numkv++;
    }

    while(*p2 == ';') {
      p2++;
    }
    p = p2;
  } while(*p != '\r' && *p != '\n' && *p != '\0');

  return numkv;
}

static int rtsp_get_track_id(const HTTP_REQ_T *pReq, char uri[HTTP_URL_LEN], const char **ppuri) {
  const char *p;
  size_t sz;
  int trackId = 0;

  if((p = strcasestr(pReq->puri, RTSP_TRACKID))) {

    //
    // Strip out the '/trackID=X' from the URL
    //
    if(ppuri && uri &&  (sz = (ptrdiff_t) (p - pReq->puri)) > 1) {
      strncpy(uri, pReq->puri, MIN(HTTP_URL_LEN, sz + 1));
      uri[sz - 1] = '\0';
      *ppuri = (const char *) uri;
    }

    p += strlen(RTSP_TRACKID);
    if(*p == '=') {
      p++;
    }
    trackId = atoi(p);
  }

  return trackId;
}

RTSP_SESSION_PROG_T *rtsp_find_prog(int trackId, RTSP_CLIENT_SESSION_T *pSession, SDP_STREAM_DESCR_T **ppSdpProg) {
  RTSP_SESSION_PROG_T *pProg = NULL;
  int controlIdVid = 0;
  int controlIdAud = 0;

  if((controlIdVid = pSession->sdp.vid.common.controlId) == 0) {
    controlIdVid = RTSP_TRACKID_VID;
  }
  if((controlIdAud = pSession->sdp.aud.common.controlId) == 0) {
    controlIdAud = RTSP_TRACKID_AUD;
  }

  if(trackId == controlIdVid && pSession->sdp.vid.common.available) {
    pProg = &pSession->s.vid;
    if(ppSdpProg) {
      *ppSdpProg = &pSession->sdp.vid.common;
    }
  } else if(pSession->sdp.aud.common.available && 
           (trackId == controlIdAud || (trackId == RTSP_TRACKID_VID && !pSession->sdp.vid.common.available))) {
    pProg = &pSession->s.aud;
    if(ppSdpProg) {
      *ppSdpProg = &pSession->sdp.aud.common;
    }
  }

  return pProg;
}

static int rtsp_set_sessiontype(RTSP_REQ_CTXT_T *pRtsp, RTSP_SESSION_PROG_T *pProg, 
                               int forceTcpUA, const char *pUserAgent) {
  int rc = 0;

  pRtsp->pSession->sessionType &= ~(RTSP_SESSION_TYPE_UDP | RTSP_SESSION_TYPE_INTERLEAVED);

  if(pRtsp->pStreamerCfg->pRtspSessions->rtsptranstype != RTSP_TRANSPORT_TYPE_UDP &&
    !strcasecmp(pProg->transStr, SDP_TRANS_RTP_AVP_TCP)) {
     pRtsp->pSession->sessionType |= RTSP_SESSION_TYPE_INTERLEAVED;
    pProg->multicast = 0;
  } else if(!strcasecmp(pProg->transStr, SDP_TRANS_RTP_AVP_TCP) ||
            pRtsp->pStreamerCfg->pRtspSessions->rtsptranstype == RTSP_TRANSPORT_TYPE_INTERLEAVED) {
    LOG(X_WARNING("RTSP unsupported transport set for requested: '%s'"), pProg->transStr);
    rc = -1;
  } else if(forceTcpUA) {
    LOG(X_WARNING("RTSP Force TCP set for User-Agent:'%s'"), pUserAgent ? pUserAgent : "");
    rc = -1;
  } else {
    pRtsp->pSession->sessionType |= RTSP_SESSION_TYPE_UDP;
   }

  return 0;
}

int rtsp_handle_setup_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  char bufhdrs[1024];
  const char *pargTrans;
  int trackId = 0;
  char uri[HTTP_URL_LEN];
  const char *puri = NULL;
  RTSP_SESSION_PROG_T progTmp;
  RTSP_SESSION_PROG_T *pProg = NULL;
  RTSP_SESSION_PROG_T *pProgOrig = NULL;
  RTSP_SESSION_PROG_T *pProgPeer = NULL;
  SDP_STREAM_DESCR_T *pSdpProg = NULL;
  RTSP_CLIENT_SESSION_T *pSession = NULL;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;
  RTSP_TRANSPORT_SECURITY_TYPE_T rtspsecuritytype = RTSP_TRANSPORT_SECURITY_TYPE_UNKNOWN;
  int rtcpmux = 0;
  int rtpmux = 0;
  int first_setup = 0;

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - SETUP handler: '%s'"), pReq->hr.url) );

  if(!(pSession = pRtsp->pClientSession)) {
    return -1;
  }

  bufhdrs[0] = '\0';
  puri = pReq->hr.puri;
  memset(&progTmp, 0, sizeof(progTmp));
  trackId = rtsp_get_track_id(&pReq->hr, uri, &puri);

  if(!(pProg = rtsp_find_prog(trackId, pSession, &pSdpProg))) {
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Invalid "RTSP_TRACKID"\r\n", RTSP_HDR_SRVERROR); 
    rc = -1;
  }
  
  pProgOrig = pProg;

  if(rc >= 0 && !pSdpProg->available) {
    rc = -1;
  }

  if(rc >= 0 && pSession->s.sessionid[0] == '\0') {
    first_setup = 1;
    pProg = &progTmp;
  }

  if(rc >= 0 && 
    (pargTrans = conf_find_keyval(pReq->hr.reqPairs, RTSP_HDR_TRANSPORT))) {
    rtsp_get_transport_keyvals(pargTrans, pProg, 1);
  }

  if(rc >= 0) {

    if((pProg->ports[0] == pProg->ports[1] || 
        (pRtsp->pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX) || pSdpProg->rtcpmux)) {
      rtcpmux = 1;
    }

    if((pRtsp->pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTPMUX)) {
      rtpmux = 1;
    } else if(pSession->s.vid.ports[0] > 0 && pSession->s.aud.ports[0] > 0 &&
              pSession->s.vid.ports[0] == pSession->s.aud.ports[0]) {
      // 
      // If this is the 2nd channel SETUP, then check if the client is using rtp-mux ports
      // and do the same for the 2nd channel
      rtpmux = 1;
      pProgPeer = (pProg == &pSession->s.vid) ? &pSession->s.aud : &pSession->s.vid;
      pProg->localports[0] = pProgPeer->localports[0];
      pProg->localports[1] = pProgPeer->localports[1];
    }

  }

  //
  // First time initialization of an RTSP session instance
  //
  if(rc >= 0 && first_setup) {
    rtspsrv_initSession(pRtsp->pStreamerCfg->pRtspSessions, &pSession->s);

    pProg = pProgOrig;
    pProg->ports[0] = progTmp.ports[0];
    pProg->ports[1] = progTmp.ports[1];
    pProg->numports = progTmp.numports;
    pProg->multicast = progTmp.multicast;
    memcpy(pProg->transStr, progTmp.transStr, sizeof(pProg->transStr));;
  }


  if(rc >= 0) {
    // Store the SETUP URL since there is no DESCRIBE Content-Base for each track 
    strncpy(pProg->url, pReq->hr.url, sizeof(pProg->url) - 1); 

    pProg->dstIp.s_addr = pRtsp->pSd->sain.sin_addr.s_addr;

    //
    // Update the session idle timer
    //
    gettimeofday(&pRtsp->pSession->tvlastupdate, NULL);

  }

  if(rc >= 0 && (rc = rtsp_set_sessiontype(pRtsp, pProg, 0, NULL)) < 0) {
    statusCode = RTSP_STATUS_UNSUPPORTEDTRANS;
  }

  //
  // First time audio video port reservation
  //
  if(rc >= 0 && pSession->s.vid.localports[0] == 0 && pSession->s.aud.localports[0] == 0) {

    if((rc = rtsp_alloc_listener_ports(&pSession->s, pRtsp->pStreamerCfg->pRtspSessions->staticLocalPort, 
                                  rtpmux, rtcpmux,
                                  pSession->sdp.vid.common.available, pSession->sdp.aud.common.available)) < 0) {
      
    }

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - video ports:%d,%d, local-ports:%d,%d, audio ports:%d,%d, local-ports:%d,%d"), 
      pSession->s.vid.ports[0], pSession->s.vid.ports[1], 
      pSession->s.vid.localports[0], pSession->s.vid.localports[1],
      pSession->s.aud.ports[0], pSession->s.aud.ports[1], 
      pSession->s.aud.localports[0], pSession->s.aud.localports[1]));

  }

  if(rc >= 0 && (!strncasecmp(pProg->transStr, SDP_TRANS_SRTP_AVP, strlen(SDP_TRANS_SRTP_AVP)) ||
                 !strncasecmp(pProg->transStr, SDP_TRANS_SRTP_AVPF, strlen(SDP_TRANS_SRTP_AVPF)))) {
    rtspsecuritytype = RTSP_TRANSPORT_SECURITY_TYPE_SRTP;
  }

  if(rc >= 0) {

    if(pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_SRTP &&
       rtspsecuritytype != pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype) {
      LOG(X_ERROR("RTSP client wants disabled transport mode %s"), pProg->transStr);
      rc = -1;
    }
    //
    // If the RTSP control connection is SSL and we're not using interleaved mode then
    // don't want to send plain-text RTP, unless it has been explicitly specified in the config
    //
    else if((pRtsp->pSd->netsocket.flags & NETIO_FLAG_SSL_TLS) &&
       !(pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) &&
        pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_UNKNOWN &&
        rtspsecuritytype != RTSP_TRANSPORT_SECURITY_TYPE_SRTP) {
      LOG(X_ERROR("If you really want to accept plain-text RTP for this secure RTSP connection use '--rtsp-srtp=0'"));
      rc = -1;
    } else {

      rc = rtsp_setup_transport_str(pSession->s.sessionType, pSession->ctxtmode,
                                    rtspsecuritytype, pProg, NULL, bufhdrs, sizeof(bufhdrs));
    }
  }

  if(rc >= 0) {
    //
    // Replace the SDP port from the ANNOUNCE with the locally assigned listener port
    //
    pSdpProg->port = pProg->localports[0];
    pSdpProg->portRtcp = pProg->localports[1];

    pProg->issetup = 1;

  } else if(statusCode != RTSP_STATUS_UNSUPPORTEDTRANS) {

    if(statusCode < 400) {
      statusCode = RTSP_STATUS_SERVERERROR;
    }
    LOG(X_ERROR("RTSP SETUP error %s %d"), bufhdrs, statusCode);
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    rc = -1;
  }

  return rc;
}

int rtsp_handle_setup(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  char bufhdrs[1024];
  unsigned int szhdrs = 0;
  unsigned int idx;
  char *p;
  const char *pargTrans;
  const char *pUserAgent = NULL;
  struct in_addr addr;
  int trackId = 0;
  int outidx = 0;
  char uri[HTTP_URL_LEN];
  const char *puri;
  int rtcpmux = 0;
  int rtpmux = 0;
  int client_srtp = 0;
  int forceTcpUA = 0;
  RTSP_SESSION_PROG_T *pProg = NULL;
  RTSP_SESSION_PROG_T *pProgPeer = NULL;
  RTSP_SESSION_PROG_T progTmp;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - SETUP handler: '%s'"), pReq->hr.url) );

  //
  // If the User-Agent array has been set, check against the User-Agent list
  // to perform setup specific logic, such as forceTcp mode
  //
  if(pRtsp->pStreamerCfg->pRtspSessions->rtspForceTcpUAList.count > 0) {

    pUserAgent = conf_find_keyval((const KEYVAL_PAIR_T *) &pReq->hr.reqPairs, RTSP_HDR_USER_AGENT);
    LOG(X_DEBUG("RTSP Setup User-Agent: '%s'"), pUserAgent ? pUserAgent : pUserAgent);

    for(idx = 0; idx < pRtsp->pStreamerCfg->pRtspSessions->rtspForceTcpUAList.count; idx++) {
      if((p = pRtsp->pStreamerCfg->pRtspSessions->rtspForceTcpUAList.arr[idx]) &&
         (!strcmp(p, "*") || !strcasecmp(p, "ANY") || (pUserAgent && strcasestr(pUserAgent, p)))) {
 
        forceTcpUA = 1;
        break;
      }
    }
  }

  bufhdrs[0] = '\0';
  puri = pReq->hr.puri;
  trackId = rtsp_get_track_id(&pReq->hr, uri, &puri);
  memset(&progTmp, 0, sizeof(progTmp));

  //
  // Obtain the requested xcode output outidx
  //
  if((outidx = outfmt_getoutidx(puri, NULL)) < 0 || outidx >= IXCODE_VIDEO_OUT_MAX) {
    outidx = 0;
  }

  if(rc == 0 && 
    (pargTrans = conf_find_keyval(pReq->hr.reqPairs, RTSP_HDR_TRANSPORT))) {
    rtsp_get_transport_keyvals(pargTrans, &progTmp, 1);
  }

  if(progTmp.ports[0] == progTmp.ports[1] || (pRtsp->pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX) ||
     (pRtsp->pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX)) {
    rtcpmux = 1;
  }

  if((pRtsp->pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTPMUX)) {
    rtpmux = 1;
  } else if(pRtsp->pSession) {

    if(trackId == RTSP_TRACKID_VID) {
      pProg = &pRtsp->pSession->vid;
      pProgPeer = &pRtsp->pSession->aud;
    } else if(trackId == RTSP_TRACKID_AUD) {
      pProg = &pRtsp->pSession->aud;
      pProgPeer = &pRtsp->pSession->vid;
    }

    // 
    // If this is the 2nd channel SETUP, then check if the client is using rtp-mux ports
    // and do the same for the 2nd channel
    //
    if(pProg && pProgPeer && progTmp.ports[0] > 0 && pProgPeer->ports[0] == progTmp.ports[0]) {
      rtpmux = 1;
      pProg->localports[0] = pProgPeer->localports[0];
      pProg->localports[1] = pProgPeer->localports[1];
    }
  }

  if(trackId > 0 && !pRtsp->pSession) {
    pRtsp->pSession = rtspsrv_newSession(pRtsp->pStreamerCfg->pRtspSessions, rtpmux, rtcpmux);
  }

  if(rc == 0 && !pRtsp->pSession) {
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Session not available\r\n", RTSP_HDR_SRVERROR); 
    rc = -1;
  } else if(pRtsp->pSession) {
    pRtsp->pSession->requestOutIdx = outidx;
    if(outidx > 0) {
      LOG(X_DEBUG("Set RTSP output format index to[%d] uri:'%s', for %s:%d"), outidx,
               pReq->hr.puri, 
               inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));
    }
  }

  if(trackId == RTSP_TRACKID_VID) {
    pProg = &pRtsp->pSession->vid;
  } else if(trackId == RTSP_TRACKID_AUD) {
    pProg = &pRtsp->pSession->aud;
  } else {
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Invalid "RTSP_TRACKID"\r\n", RTSP_HDR_SRVERROR); 
    rc = -1;
  } 

  if(rc == 0) {
    pProg->ports[0] = progTmp.ports[0];
    pProg->ports[1] = progTmp.ports[1];
    pProg->numports = progTmp.numports;
    pProg->multicast = progTmp.multicast;
    memcpy(pProg->transStr, progTmp.transStr, sizeof(pProg->transStr));;
  }

  if(rc == 0 && (!strncasecmp(pProg->transStr, SDP_TRANS_SRTP_AVP, strlen(SDP_TRANS_SRTP_AVP)) ||
                 !strncasecmp(pProg->transStr, SDP_TRANS_SRTP_AVPF, strlen(SDP_TRANS_SRTP_AVPF)))) {
    client_srtp = 1;
  }

  if(rc == 0) {

    //
    // QuickTime HTTP does not send any interleaed=0-1 transport description
    //
    if(pProg->numports == 0 && !strcasecmp(pProg->transStr, SDP_TRANS_RTP_AVP_TCP)) {
      pProg->ports[0] = (pProg == &pRtsp->pSession->vid) ? 0 : 2;
      pProg->numports = 2;
      LOG(X_DEBUG("No ports specified in "SDP_TRANS_RTP_AVP_TCP" "RTSP_HDR_TRANSPORT" Using default port %d"), 
                  pProg->ports[0]); 
    }

    //
    // Validate client's client_port value
    //
    if(pProg->numports <= 0 || (strncasecmp(pProg->transStr, SDP_TRANS_RTP_AVP_TCP, 
       strlen(SDP_TRANS_RTP_AVP_TCP)) && 
       (pProg->ports[0] == 0 || pProg->ports[0] % 2 != 0)) || 
       (pProg->ports[1] != 0 && 
        !(pProg->ports[1] == pProg->ports[0] || pProg->ports[1] == RTCP_PORT_FROM_RTP(pProg->ports[0])))) {
      if((rc = snprintf(&bufhdrs[szhdrs], sizeof(bufhdrs) - szhdrs, 
                        "%s: Invalid "RTSP_CLIENT_PORT" %d\r\n",
                        RTSP_HDR_SRVERROR, pProg->ports[0])) > 0) {
        szhdrs += rc;
      }
      rc = -1;
    } else if(pProg->ports[1] == 0) {
      // setup RTCP port
      pProg->ports[1] = RTCP_PORT_FROM_RTP(pProg->ports[0]);
    }
  }

  if(rc == 0) {
    pProg->dstIp.s_addr = pRtsp->pSd->sain.sin_addr.s_addr;
    strncpy(pProg->url, pReq->hr.url, sizeof(pProg->url) - 1);
  }

  if(rc == 0) {
    if(pProg->transStr[0] == '\0') { 
      if((rc = snprintf(&bufhdrs[szhdrs], sizeof(bufhdrs) - szhdrs, 
                       "%s: Unsupported transport type in \"%s\"\r\n",
                       RTSP_HDR_SRVERROR, pargTrans)) > 0) {
        szhdrs += rc;
      }
      rc = -1;
    }
  } 

  if(rc >= 0) {

    if(pRtsp->pSession) {

      //
      // Update the session idle timer
      //
      gettimeofday(&pRtsp->pSession->tvlastupdate, NULL);

      if((rc = rtsp_set_sessiontype(pRtsp, pProg, forceTcpUA, pUserAgent)) < 0) {
        statusCode = RTSP_STATUS_UNSUPPORTEDTRANS;
      }
    }

    if(client_srtp && pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype != RTSP_TRANSPORT_SECURITY_TYPE_SRTP) {
      LOG(X_ERROR("RTSP client wants %s You can enable SRTP. by adding --rtsp-srtp'"), pProg->transStr);
      rc = -1;
    }
    //
    // If the RTSP control connection is SSL and we're not using interleaved mode then
    // don't want to send plain-text RTP, unless it has been explicitly specified in the config
    //
    else if((pRtsp->pSd->netsocket.flags & NETIO_FLAG_SSL_TLS) &&
       !(pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) &&
        pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_UNKNOWN) {


      LOG(X_ERROR("If you really want to send plain-text RTP for this secure RTSP connection use '--rtsp-srtp=0'"));
      rc = -1;
    }

    if(rc >= 0) {
      addr.s_addr = net_getlocalip();

      if((rc = rtsp_setup_transport_str(pRtsp->pSession->sessionType, RTSP_CTXT_MODE_SERVER_STREAM,
                               pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype, 
                               pProg, &addr, &bufhdrs[szhdrs], sizeof(bufhdrs) - szhdrs)) > 0) {
        szhdrs += rc;
        pProg->issetup = 1;
      }

    }  // end of if(rc >= 0...

  }

  if(rc >= 0) {
    LOG(X_DEBUG("Setup RTSP %s %s%ssession %s :%d,%d -> %s:%d,%d"), 
        pProg->transStr, 
        ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? "(http) " : ""),
        ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? "(interleaved) " : ""),
        pRtsp->pSession->sessionid, pProg->localports[0], pProg->localports[1], 
        inet_ntoa(pProg->dstIp), pProg->ports[0], pProg->ports[1]);

  } else if(statusCode != RTSP_STATUS_UNSUPPORTEDTRANS) {

    if(statusCode < 400) {
      statusCode = RTSP_STATUS_SERVERERROR;
    }
    LOG(X_ERROR("RTSP SETUP error %s %d"), bufhdrs, statusCode);
  }

#if defined(TESTQT)
    szhdrs += snprintf(&bufhdrs[szhdrs], sizeof(bufhdrs) - szhdrs, 
                "x-Transport-Options: late-tolerance=1.400000\r\n"
                "x-Retransmit: our-retransmit\r\n"
                "x-Dynamic-Rate: 1\r\n");
#endif // TESTQT

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    rc = -1;
  }

  if(statusCode == RTSP_STATUS_UNSUPPORTEDTRANS) {
    rc = 0;
  }

  return rc;
}


static int play_resp_headers(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq, 
                             char *bufhdrs, unsigned int szhdrs, int paused) {
  unsigned int idx = 0;
  int rc = 0;

  //
  // Update the session idle timer
  //
  if(pRtsp->pSession) {
    gettimeofday(&pRtsp->pSession->tvlastupdate, NULL);
  }

  if((rc = snprintf(&bufhdrs[idx], szhdrs - idx, "%s: npt=now-\r\n", RTSP_HDR_RANGE)) > 0) {
    idx += rc;
  }
  if((rc = snprintf(&bufhdrs[idx], szhdrs - idx, "%s: ", RTSP_HDR_RTP_INFO)) > 0) {
    idx += rc;
  }

  if(pRtsp->pSession->vid.issetup) {

    if((rc = snprintf(&bufhdrs[idx], szhdrs - idx, "url=%s", pRtsp->pSession->vid.url)) > 0) {
      idx += rc;
    }
    LOG(X_INFO("RTSP %s video %s%ssession %s %s:%d"), 
            (paused ? "Resuming" : "Playing"),  
            ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? "(http) " : ""),
            ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? "(interleaved) " : ""),
              pRtsp->pSession->sessionid, inet_ntoa(pRtsp->pSession->vid.dstIp), 
              pRtsp->pSession->vid.ports[0]);

  }

  if(pRtsp->pSession->aud.issetup) {
    if((rc = snprintf(&bufhdrs[idx], szhdrs - idx, "%surl=%s",
          (pRtsp->pSession->vid.issetup ? "," : ""), pRtsp->pSession->aud.url)) > 0) {
      idx += rc;
    }
    LOG(X_INFO("RTSP %s audio %s%ssession %s %s:%d"), 
            (paused ? "Resuming" : "Playing"),  
            ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? "(http) " : ""),
            ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? "(interleaved) " : ""),
              pRtsp->pSession->sessionid, inet_ntoa(pRtsp->pSession->aud.dstIp), 
              pRtsp->pSession->aud.ports[0]);
  }

  if((rc = snprintf(&bufhdrs[idx], szhdrs - idx, "\r\n")) > 0) {
    idx += rc;
  }

  return idx;
}

int rtsp_handle_play_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;
  RTSP_CLIENT_SESSION_T *pSession = NULL;
  CAPTURE_DESCR_COMMON_T *pCommon = NULL;
  int idxhdrs = 0;
  char bufhdrs[1024];

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - PLAY handler: '%s'"), pReq->hr.url) );

  if(!(pSession = pRtsp->pClientSession) || !pRtsp->pCap || !(pCommon = pRtsp->pCap->pcommon)) {
    return -1;
  }

  bufhdrs[0] = '\0';

  if(rc >= 0 && !pSession->s.vid.issetup && !pSession->s.aud.issetup) {
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Session not setup\r\n", RTSP_HDR_SRVERROR); 
    rc = -1;
  }

  if(rc >= 0) {
    rc = capture_rtsp_onsetupdone(pSession, pCommon); 
  }

  if(rc >= 0) {
    pSession->isplaying = 1;

    idxhdrs = play_resp_headers(pRtsp, pReq, bufhdrs, sizeof(bufhdrs), 0);

//TODO: only do if not interleaved
    //
    // Signal capture_rtsp_server_start to complete waiting for a capture session to be created
    //
    vsxlib_cond_signal(&pSession->cond.cond, &pSession->cond.mtx);

  } else if(statusCode != RTSP_STATUS_UNSUPPORTEDTRANS) {

    if(statusCode < 400) {
      statusCode = RTSP_STATUS_SERVERERROR;
    }
    LOG(X_ERROR("RTSP PLAY error %s %d"), bufhdrs, statusCode);
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq,
                       pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    rc = -1;
  }

  return rc;

}

int rtsp_do_play(RTSP_REQ_CTXT_T *pRtsp, char *bufhdrs, unsigned int szbufhdrs) {
  int rc = 0;
  STREAM_DEST_CFG_T destCfg;
  unsigned int outidx = 0;
  unsigned int srtpidx = 0;
  int paused = 0;

  if(!pRtsp) {
    return -1;
  }
  
  if(bufhdrs) {
    bufhdrs[0] = '\0';
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - setting up stream play for %s%s"), 
        pRtsp->pSession->vid.issetup ? "video " : "", pRtsp->pSession->aud.issetup ? "audio " : "") );

  if(!pRtsp->pSession || !(pRtsp->pSession->vid.issetup || pRtsp->pSession->aud.issetup)) {
    if(bufhdrs) {
      snprintf(bufhdrs, szbufhdrs, "%s: Session not found or not setup\r\n", RTSP_HDR_SRVERROR);
    }
    return -1;
  }

  outidx = pRtsp->pSession->requestOutIdx;
  if((pRtsp->pSession->flag & RTSP_SESSION_FLAG_PAUSED)) {
    paused = 1;
  }

  if(!paused && pRtsp->pSession && pRtsp->pSession->pLiveQ2) {
    rtsp_interleaved_stop(pRtsp);
  }

  memset(&destCfg, 0, sizeof(destCfg));

 //destCfg.pMonitor = pRtsp->pStreamerCfg->pMonitor;
  if(pRtsp->pStreamerCfg->pMonitor && pRtsp->pStreamerCfg->pMonitor->active) {
    destCfg.pMonitor = pRtsp->pStreamerCfg->pMonitor;
  }
  destCfg.pFbReq = &pRtsp->pStreamerCfg->fbReq;
  destCfg.xcodeOutidx = outidx;

  //
  // Lock the entire RTSP context to prevent from pRtpMultis references
  // from becoming invalidated (during a close)
  //
  pthread_mutex_lock(&pRtsp->mtx);

  if(!paused && rc == 0 && pRtsp->pSession->vid.issetup) {

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - Add video stream")) );

    //fprintf(stderr, "DOING RTSP PLAY video, interleaved:%d, pSession: 0x%x\n", (pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED), pRtsp->pSession);
    if((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
      if(pRtsp->pSession->pLiveQ2 || rtsp_interleaved_start(pRtsp) == 0) {
        destCfg.noxmit = 1;
        destCfg.outCb.pliveQIdx = &pRtsp->pSession->liveQIdx;
      }
      destCfg.dstPort = pRtsp->pSession->vid.ports[0];
      destCfg.dstPortRtcp = RTCP_PORT_FROM_RTP(destCfg.dstPortRtcp);

    } else {
      destCfg.haveDstAddr = 1;
      destCfg.u.dstAddr.s_addr = pRtsp->pSession->vid.dstIp.s_addr;
      destCfg.dstPort = pRtsp->pSession->vid.ports[0];
      //destCfg.dstPortRtcp = RTCP_PORT_FROM_RTP(destCfg.dstPort);
      destCfg.dstPortRtcp  = pRtsp->pSession->vid.ports[1];
      destCfg.localPort = pRtsp->pSession->vid.localports[0];
    }
    if(pRtsp->pSession->refreshtimeoutviartcp > 0) {
      destCfg.ptvlastupdate = &pRtsp->pSession->tvlastupdate;
    }

    //
    // Copy the SRTP keys initialized when processing the DESCRIBE
    //
    if(pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_SRTP) {
      if(pRtsp->srtpKts[srtpidx].k.lenKey > 0) {
        memcpy(&destCfg.srtp.kt.k, &pRtsp->srtpKts[srtpidx].k, sizeof(destCfg.srtp.kt.k));
        memcpy(&destCfg.srtp.kt.type, &pRtsp->srtpKts[srtpidx].type, sizeof(destCfg.srtp.kt.type));
      } else {
        rc = -1;
      }
    }

    if(!(pRtsp->pSession->vid.pDest =
          stream_rtp_adddest(&pRtsp->pStreamerCfg->pRtspSessions->pRtpMultis[outidx][0], &destCfg))) {
      if(bufhdrs) {
        snprintf(bufhdrs, szbufhdrs, "%s: Failed to create video\r\n", RTSP_HDR_SRVERROR);
      }
      rc = -1;

    } else if(destCfg.pMonitor && pRtsp->pSession->vid.pDest->pstreamStats) {

      if((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {

        if((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP)) {
          pRtsp->pSession->vid.pDest->pstreamStats->method = STREAM_METHOD_RTSP_HTTP;
        } else {
          pRtsp->pSession->vid.pDest->pstreamStats->method = STREAM_METHOD_RTSP_INTERLEAVED;
        }
        memcpy(&pRtsp->pSession->vid.pDest->pstreamStats->saRemote,
               &pRtsp->pSd->sain, sizeof(struct sockaddr_in));

        if(pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]  &&
           !pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]->pstats) {
           pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]->pstats =
                                     &pRtsp->pSession->vid.pDest->pstreamStats->throughput_rt[0];
        }

      } else {

        pRtsp->pSession->vid.pDest->pstreamStats->method = STREAM_METHOD_RTSP;

      }
    }

    if(rc == 0) {

      //
      // Request an IDR from the underling encoder
      //
      //
      // Set the requested IDR time a bit into the future because there may be a slight
      // delay until the queue reader starts reading mpeg2-ts packets
      //
      streamer_requestFB(pRtsp->pStreamerCfg, outidx, ENCODER_FBREQ_TYPE_FIR, 200, REQUEST_FB_FROM_LOCAL);

      rtspsrv_playSessionTrack(pRtsp->pStreamerCfg->pRtspSessions,
                             pRtsp->pSession->sessionid, RTSP_TRACKID_VID, 0);
    }

  }

  if(!paused && rc == 0 && pRtsp->pSession->aud.issetup) {
    //fprintf(stderr, "DOING RTSP PLAY audio, interleaved:%d\n", (pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED));

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - Add audio stream")) );

    if((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
      if(pRtsp->pSession->pLiveQ2 || rtsp_interleaved_start(pRtsp) == 0) {
        destCfg.noxmit = 1;
        destCfg.outCb.pliveQIdx = &pRtsp->pSession->liveQIdx;
      }
      destCfg.dstPort = pRtsp->pSession->aud.ports[0];
      destCfg.dstPortRtcp = RTCP_PORT_FROM_RTP(destCfg.dstPort);
    } else {
      destCfg.haveDstAddr = 1;
      destCfg.u.dstAddr.s_addr = pRtsp->pSession->aud.dstIp.s_addr;
      destCfg.dstPort = pRtsp->pSession->aud.ports[0];
      //destCfg.dstPortRtcp = RTCP_PORT_FROM_RTP(destCfg.dstPort);
      destCfg.dstPortRtcp = pRtsp->pSession->aud.ports[0];
      destCfg.localPort = pRtsp->pSession->aud.localports[0];
      //TODO: if audio only is present, streamer_rtp puts it into rtpMulti[outidx][0]
    }
    if(pRtsp->pSession->refreshtimeoutviartcp > 0) {
      destCfg.ptvlastupdate = &pRtsp->pSession->tvlastupdate;
    }

    //
    // Copy the SRTP keys initialized when processing the DESCRIBE
    //
    srtpidx = pRtsp->pSession->aud.issetup ? 1 : 0;
    if(pRtsp->pStreamerCfg->pRtspSessions->rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_SRTP) {
      if(pRtsp->srtpKts[srtpidx].k.lenKey > 0) {
        memcpy(&destCfg.srtp.kt.k, &pRtsp->srtpKts[srtpidx].k, sizeof(destCfg.srtp.kt.k));
        memcpy(&destCfg.srtp.kt.type, &pRtsp->srtpKts[srtpidx].type, sizeof(destCfg.srtp.kt.type));
      } else {
        rc = -1;
      }
    }

    if(!(pRtsp->pSession->aud.pDest =
          stream_rtp_adddest(&pRtsp->pStreamerCfg->pRtspSessions->pRtpMultis[outidx]
                             [pRtsp->pStreamerCfg->novid ? 0 : 1], &destCfg))) {
      if(bufhdrs) {
        snprintf(bufhdrs, szbufhdrs, "%s: Failed to create audio\r\n", RTSP_HDR_SRVERROR);
      }
      rc = -1;

    } else if(destCfg.pMonitor && pRtsp->pSession->aud.pDest->pstreamStats) {

      if((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {

        if((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP)) {
          pRtsp->pSession->aud.pDest->pstreamStats->method = STREAM_METHOD_RTSP_HTTP;
        } else {
          pRtsp->pSession->aud.pDest->pstreamStats->method = STREAM_METHOD_RTSP_INTERLEAVED;
        }
        memcpy(&pRtsp->pSession->aud.pDest->pstreamStats->saRemote,
               &pRtsp->pSd->sain, sizeof(struct sockaddr_in));

        if(pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]) {
          if(pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]->pstats) {
            //
            // In the case of tcp interleaved, if the vid has already set the queue stats pointer
            // disable the audio destination statistics
            //
            //pRtsp->pSession->aud.pDest->pstreamStats->active = 0;
            stream_stats_destroy(&(pRtsp->pSession->aud.pDest->pstreamStats), NULL);
          } else {
            pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]->pstats =
                                       &pRtsp->pSession->aud.pDest->pstreamStats->throughput_rt[0];
          }
        }

      } else {

        pRtsp->pSession->aud.pDest->pstreamStats->method = STREAM_METHOD_RTSP;

      }
    }

   if(rc == 0) {
      rtspsrv_playSessionTrack(pRtsp->pStreamerCfg->pRtspSessions,
                             pRtsp->pSession->sessionid, RTSP_TRACKID_AUD, 0);
    }
  }

  //
  // If resuming from a pause, unset the pause flag
  //
  if(paused && pRtsp->pSession) {
    pRtsp->pSession->flag &= ~RTSP_SESSION_FLAG_PAUSED;
  }

  pthread_mutex_unlock(&pRtsp->mtx);

  return rc;
}

int rtsp_handle_play(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  char bufhdrs[1024];
  int idxhdrs;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;
  int paused = 0;


  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - PLAY handler: '%s'"), pReq->hr.url) );

  if((pRtsp->pSession->flag & RTSP_SESSION_FLAG_PAUSED)) {
    paused = 1;
  }

  rc = rtsp_do_play(pRtsp, bufhdrs, sizeof(bufhdrs));

  if(rc < 0) {
    statusCode = RTSP_STATUS_SERVERERROR;
    LOG(X_ERROR("RTSP PLAY error %s"), bufhdrs);
  } else {

    idxhdrs = play_resp_headers(pRtsp, pReq, bufhdrs, sizeof(bufhdrs), paused);
 
  }

#if defined(TESTQT)
    if(idxhdrs >= 0) {
      idxhdrs += snprintf(&bufhdrs[szhdrs], sizeof(bufhdrs) - szhdrs, 
                "x-Transport-Options: late-tolerance=1.400000\r\n"
                "x-prebuffer: maxtime=2.00000\r\n");
    }
#endif // TESTQT

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs) < 0) {
    rc = -1;
  }

  return rc;
}

int rtsp_handle_pause(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - PAUSE handler: '%s'"), pReq->hr.url) );

  if(pRtsp->pSession) {
    LOG(X_DEBUG("Pausing RTSP %s%ssession %s"), 
        ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? "(http) " : ""),
        ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? "(interleaved) " : ""),
        pRtsp->pSession->sessionid);
  
    pthread_mutex_lock(&pRtsp->mtx);
    pRtsp->pSession->flag |= RTSP_SESSION_FLAG_PAUSED;
    pthread_mutex_unlock(&pRtsp->mtx);

  } else {
    statusCode = RTSP_STATUS_SERVERERROR;
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq,
                       pRtsp->pSession, 0, HTTP_CACHE_CONTROL_NO_CACHE, NULL) < 0) {
    rc = -1;
  }

  return rc;
}

int rtsp_handle_teardown_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  RTSP_CLIENT_SESSION_T *pSession = NULL;
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;
  char bufhdrs[1024];

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - TEARDOWN handler: '%s'"), pReq->hr.url) );

  bufhdrs[0] = '\0';

  if(!(pSession = pRtsp->pClientSession)) {
    return -1;
  }

  pSession->issetup = 0;
  pSession->isplaying = 0;
  if(pRtsp->pCap && pRtsp->pCap->running == STREAMER_STATE_RUNNING) {
    pRtsp->pCap->running = STREAMER_STATE_INTERRUPT;
    vsxlib_cond_signal(&pSession->cond.cond, &pSession->cond.mtx);
  }

  if(rc < 0) {
    statusCode = RTSP_STATUS_SERVERERROR;
    LOG(X_ERROR("RTSP TEARDOWN error %s"), bufhdrs);
  } else {

  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq,
                       pRtsp->pSession, 0, NULL, bufhdrs) < 0) {
    rc = -1;
  }

  // Force the RTSP server to close
  rc = -1;

  return rc;
}

int rtsp_handle_teardown(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  unsigned int szhdrs = 0;
  int clearSession = 0;
  char bufhdrs[1024];
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;

  bufhdrs[0] = '\0';

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - TEARDOWN handler: '%s'"), pReq->hr.url) );

  if(!pRtsp->pSession) {
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Session not found\r\n", RTSP_HDR_SRVERROR); 
    rc = -1;
  }

  if(rc == 0) {
    if((rc = rtspsrv_closeSession(pRtsp->pStreamerCfg->pRtspSessions, pRtsp->pSession->sessionid)) < 0) {
      snprintf(bufhdrs, sizeof(bufhdrs), "%s: Failed to close session\r\n", RTSP_HDR_SRVERROR); 
    } else {
      clearSession = 1;
    }
  }

  rtsp_interleaved_stop(pRtsp);

  if(rc < 0) {
    statusCode = RTSP_STATUS_SERVERERROR;
    LOG(X_ERROR("RTSP TEARDOWN error %s"), bufhdrs);
  } else {

    if((rc = snprintf(bufhdrs, sizeof(bufhdrs), "%s: Close\r\n", RTSP_HDR_CONNECTION)) > 0) {
      szhdrs += rc;
    }
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 0, NULL, bufhdrs) < 0) {
    rc = -1;
  }

  if(clearSession) {
    pRtsp->pSession = NULL;
  }

  return rc;
}

int rtsp_handle_announce(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq) {
  int rc = 0;
  char bufhdrs[1024];
  enum RTSP_STATUS statusCode = RTSP_STATUS_OK;

  bufhdrs[0] = '\0';

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - ANNOUNCE handler: '%s'"), pReq->hr.url) );

  //rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, pRtsp->pSession, 1, HTTP_CACHE_CONTROL_NO_CACHE, bufhdrs); return 0;

  if(!pRtsp->pClientSession) {
    rc = -1;
  } else {
    pRtsp->pClientSession->sdp.vid.common.available = 0;
    pRtsp->pClientSession->sdp.aud.common.available = 0;
  }

  //
  // Read and Parse the received SDP
  //
  if(rc >= 0 && (rc = rtsp_read_sdp((HTTP_PARSE_CTXT_T *) &pReq->hdrCtxt, pReq->hr.reqPairs, 
                         RTSP_REQ_ANNOUNCE, &pRtsp->pClientSession->sdp)) < 0) {
    snprintf(bufhdrs, sizeof(bufhdrs), "%s: Unable to process SDP\r\n", RTSP_HDR_SRVERROR);
  }

  if(rc < 0) {
    statusCode = RTSP_STATUS_SERVERERROR;
    LOG(X_ERROR("RTSP ANNOUNCE error %s"), bufhdrs);
  } else {
    LOG(X_DEBUG("RTSP - ANOUNCE have SDP %s %s"), 
         pRtsp->pClientSession->sdp.vid.common.available ? " with video" : "",
         pRtsp->pClientSession->sdp.aud.common.available ? "with audio" : "");
  }

  if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq, 
                       pRtsp->pSession, 0, NULL, bufhdrs) < 0) {
    rc = -1;
  }

  return rc;
}

int rtsp_interleaved_start(RTSP_REQ_CTXT_T *pRtsp) {
  STREAMER_LIVEQ_T *pLiveQ2 = NULL;
  PKTQUEUE_T *pQ = NULL;
  unsigned int idx;
  //unsigned int queueSz = (unsigned int) ((float)TSLIVE_PKTQUEUE_SZ_MAX * .45);
  int liveQIdx = -1;
  unsigned int numQFull = 0;
  unsigned int maxQ = 0;
  unsigned int outidx = 0;
  int rc = 0;

  if(!pRtsp || !pRtsp->pSession || !pRtsp->pStreamerCfg) {
    return -1;
  }

  outidx = pRtsp->pSession->requestOutIdx;

  pLiveQ2 = &pRtsp->pStreamerCfg->liveQ2s[outidx];
  if(pLiveQ2->pQs) {
    maxQ = pLiveQ2->max;
  }

  //
  // Find an available liveQ2 queue instance
  //
  pthread_mutex_lock(&pLiveQ2->mtx);
  if(pLiveQ2->numActive <  maxQ) {
    for(idx = 0; idx < maxQ; idx++) {

      if(pLiveQ2->pQs[idx] == NULL) {

        if(!(pQ = pktqueue_create(pLiveQ2->qCfg.maxPkts, pLiveQ2->qCfg.maxPktLen,
                                  pLiveQ2->qCfg.maxPkts, pLiveQ2->qCfg.growMaxPktLen,
                                  0,  sizeof(OUTFMT_FRAME_DATA_T), 1))) {
          LOG(X_ERROR("Failed to create queue for RTSP interleaved listener[%d] length: %d x %d"), 
                      idx, pLiveQ2->qCfg.maxPkts, pLiveQ2->qCfg.maxPktLen);
          rc = -1;
          break;
        }

        //
        // If the queue does not have a reader, each insert will overwrite the prior
        //
        //pktqueue_setrdr(pQ);

        //pktqueue_createstats(pQ, 8000, 2000);
        //pQ->pstats = NULL;

        pRtsp->pSession->pLiveQ2 = pLiveQ2;
        pRtsp->pSession->liveQIdx = liveQIdx = idx;
        pLiveQ2->pQs[liveQIdx] = pQ;
        pLiveQ2->numActive++;

        pQ->cfg.id = pLiveQ2->qCfg.id + liveQIdx;
        break;
      }
      numQFull++;
    }
  }

  pthread_mutex_unlock(&pLiveQ2->mtx);

  if(liveQIdx < 0) {
    LOG(X_ERROR("No RTSP interleaved live queue (max:%d) for %s:%d"), pLiveQ2->max,
        inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));
    rc = -1;
  } else {
    memset(&pRtsp->pSession->liveQCtxt, 0, sizeof(pRtsp->pSession->liveQCtxt));
    pRtsp->pSession->liveQCtxt.ppQ = &pLiveQ2->pQs[liveQIdx];
    pRtsp->pSession->liveQCtxt.cbOnFrame = rtsp_addFrame;
    pRtsp->pSession->liveQCtxt.pCbData = pRtsp;
    pRtsp->pSession->liveQCtxt.id = pRtsp->pSession->sessionid;
    pRtsp->pSession->liveQCtxt.pMtx = &pRtsp->mtx;
    if((rc = liveq_start_cb(&pRtsp->pSession->liveQCtxt)) < 0) {
      LOG(X_ERROR("Unable to start RTSP liveq cb thread for session %s"),
          pRtsp->pSession->sessionid);
    }
  }

  if(rc == 0) {

    LOG(X_DEBUG("Starting RTSP %s(interleaved) stream[%d] %d/%d to %s:%d"),
     ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? "(http) " : ""),
      liveQIdx, numQFull + 1, pLiveQ2->max,
           inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));
  } else {

    if(pQ) {
      //fprintf(stderr, "PQ DESTROY: 0x%x\n", pQ);
      pktqueue_destroy(pQ);
    }

  }

  return rc;
}

void rtsp_interleaved_stop(RTSP_REQ_CTXT_T *pRtsp) {
  STREAMER_LIVEQ_T *pLiveQ2 = NULL;
  PKTQUEUE_T *pQ = NULL;
  unsigned int outidx;

  if(!pRtsp || !pRtsp->pSession || !pRtsp->pSession->pLiveQ2) {
    return;
  }

  outidx = pRtsp->pSession->requestOutIdx;
  //TODO: pLiveQ2 may not always coincide w/ session outidx
  pLiveQ2 = pRtsp->pSession->pLiveQ2;

  LOG(X_DEBUG("Ending RTSP %s(interleaved) session %s stream[%d] to %s:%d"),
         ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? "(http) " : ""),
           pRtsp->pSession->sessionid, pRtsp->pSession->liveQIdx,
           inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));

  pthread_mutex_lock(&pLiveQ2->mtx);

  if((pQ = pRtsp->pStreamerCfg->liveQ2s[outidx].pQs[pRtsp->pSession->liveQIdx])) {
    pRtsp->pSession->liveQCtxt.ppQ = NULL;
    pQ->quitRequested = 1;
    pktqueue_wakeup(pQ);
    pthread_mutex_unlock(&pLiveQ2->mtx);
    liveq_stop_cb(&pRtsp->pSession->liveQCtxt);
  }

  pthread_mutex_lock(&pLiveQ2->mtx);
  pthread_mutex_lock(&pRtsp->mtx);
  if((pQ = pRtsp->pStreamerCfg->liveQ2s[outidx].pQs[pRtsp->pSession->liveQIdx])) {
    pktqueue_destroy(pQ);
  }

  pRtsp->pSession->pLiveQ2 = NULL;
  pRtsp->pStreamerCfg->liveQ2s[outidx].pQs[pRtsp->pSession->liveQIdx] = NULL;
  if(pLiveQ2->numActive > 0) {
    pLiveQ2->numActive--;
  }

  pthread_mutex_unlock(&pRtsp->mtx);
  pthread_mutex_unlock(&pLiveQ2->mtx);

}
