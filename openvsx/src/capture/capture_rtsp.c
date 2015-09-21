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

#if defined(VSX_HAVE_CAPTURE)

static CONNECT_RETRY_RC_T rtsp_req_options(RTSP_CLIENT_SESSION_T *pSession, const char *url, int dorcv);

static void rtsp_client_proc_monitor(void *pArg) {
  RTSP_CLIENT_SESSION_T *pSession = (RTSP_CLIENT_SESSION_T *) pArg;
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  struct timeval tvnow;

  pthread_mutex_lock(&pSession->mtx);
  if(pSession->runMonitor == STREAMER_STATE_STARTING_THREAD) {
    pSession->runMonitor = STREAMER_STATE_RUNNING;
  }
  pthread_mutex_unlock(&pSession->mtx);

  LOG(X_DEBUG("RTSP client monitor thread started with session timeout %d sec"), pSession->s.expireSec);

  while(pSession->runMonitor == STREAMER_STATE_RUNNING && !g_proc_exit) {

    gettimeofday(&tvnow, NULL);

    //fprintf(stderr, "RTSP client monitor now:%d lastupdate:%d expiresec:%d, %d + 10 >= %u\n", tvnow.tv_sec, pSession->s.tvlastupdate.tv_sec, pSession->s.expireSec, (tvnow.tv_sec - pSession->s.tvlastupdate.tv_sec), pSession->s.expireSec);

    if((tvnow.tv_sec - pSession->s.tvlastupdate.tv_sec) + 10 >= pSession->s.expireSec) {
      //LOG(X_DEBUG("CLIENT MONITOR... try to send..."));
      pthread_mutex_lock(&pSession->mtx);

      pSession->cseq++;
      if((rc = rtsp_req_options(pSession, pSession->s.vid.url,
                 ((pSession->s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? 0 : 1))) < CONNECT_RETRY_RC_OK) {
        break;
      }

      LOG(X_DEBUG("Updated RTSP session %s"), pSession->s.sessionid);

      pthread_mutex_unlock(&pSession->mtx);

    }

    sleep(1);
  }

  pthread_mutex_lock(&pSession->mtx);
  pSession->runMonitor = STREAMER_STATE_FINISHED;
  pthread_mutex_unlock(&pSession->mtx);

  LOG(X_DEBUG("RTSP client monitor thread exiting"));
}

static int start_client_monitor(RTSP_CLIENT_SESSION_T *pSession) {
  int rc = 0;
  pthread_t ptdMonitor;
  pthread_attr_t attrMonitor;

  pSession->runMonitor = STREAMER_STATE_STARTING_THREAD;
  pthread_mutex_init(&pSession->mtx, NULL);
  pthread_attr_init(&attrMonitor);
  pthread_attr_setdetachstate(&attrMonitor, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdMonitor,
                    &attrMonitor,
                    (void *) rtsp_client_proc_monitor,
                    (void *) pSession) != 0) {
    LOG(X_ERROR("Unable to create RTSP client monitor thread"));
    pthread_mutex_destroy(&pSession->mtx);
    pSession->runMonitor = STREAMER_STATE_ERROR;
    rc = -1;
  }

  return rc;
}

/**
 * Send an RTSP request to a remote RTSP server
 */
static int rtsp_req_send(RTSP_CLIENT_SESSION_T *pSession, 
                        const char *strMethod,
                        const char *uri,
                        const char *xtraUri,
                        const char *xtraHdrs) {
  int rc = 0;
  char buf[2048];
  char tmp[256];
  int sz = 0;

  //
  // If we're doing Digest auth and the current request method has chaned, recalculate the auth response
  //
  if(pSession->authCliCtxt.authorization[0] != '\0' && !pSession->authCliCtxt.do_basic && 
     pSession->authCliCtxt.lastMethod[0] != '\0' && strcmp(strMethod, pSession->authCliCtxt.lastMethod)) {

    strncpy(pSession->authCliCtxt.lastMethod, strMethod, sizeof(pSession->authCliCtxt.lastMethod) - 1);
    pSession->authCliCtxt.puri = uri;

    if((rc = httpcli_calcdigestauth(&pSession->authCliCtxt)) < 0) {
      return rc;
    }

  }

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz,
         "%s %s%s %s\r\n"
         "%s: %d\r\n"
         "%s: %s\r\n",
         strMethod, uri, (xtraUri ? xtraUri : ""), RTSP_VERSION_DEFAULT,
         RTSP_HDR_CSEQ, pSession->cseq,
         RTSP_HDR_USER_AGENT, vsxlib_get_appnamewwwstr(tmp, sizeof(tmp)))) < 0) {
    return -1;
  }
  sz += rc;
       
  if(pSession->s.sessionid[0] != '\0') {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
                       RTSP_HDR_SESSION, pSession->s.sessionid)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(pSession->authCliCtxt.authorization && pSession->authCliCtxt.authorization[0] != '\0') {
   if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
             HTTP_HDR_AUTHORIZATION, pSession->authCliCtxt.authorization)) < 0) {
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
    LOG(X_DEBUG("RTSP - Sending %d request bytes"), sz); 
    LOGHEXT_DEBUG(buf, sz);
  )

  //
  // Store the last method
  //
  strncpy(pSession->authCliCtxt.lastMethod, strMethod, sizeof(pSession->authCliCtxt.lastMethod) - 1);

  rc = netio_send(&pSession->sd.netsocket, (const struct sockaddr *) &pSession->sd.sa, (unsigned char *) buf, sz);

  return rc;
}

/**
 * Read an RTSP response message
 */
static int rtsp_read_resp(RTSP_CLIENT_SESSION_T *pSession, HTTP_PARSE_CTXT_T *pHdrCtxt,
                          HTTP_RESP_T *pRtspResp) {
  int rc = 0;
  int i;
  char tmp[32];
  const char *p;
  const char *p2;

  if((rc = httpcli_req_queryhdrs(pHdrCtxt, pRtspResp, (const struct sockaddr *) &pSession->sd.sa, 
                                 &pSession->authCliCtxt, "RTSP")) < 0) {

      if(pRtspResp->statusCode == HTTP_STATUS_UNAUTHORIZED) {
        httpcli_authenticate(pRtspResp, &pSession->authCliCtxt);
      }

    return rc;
  }

  //
  // Check and validate the cseq id to match the request
  //
  if(rc >= 0 && (p = conf_find_keyval(pRtspResp->hdrPairs, RTSP_HDR_CSEQ)) &&
    (i = atoi(p)) != pSession->cseq) {
    LOG(X_ERROR("RTSP server cseq %d does not match request %d"), i, pSession->cseq);
    rc = -1;
  }

  //
  // Check any server session header for this session
  //
  if(rc >= 0 && (p = conf_find_keyval(pRtspResp->hdrPairs, RTSP_HDR_SESSION))) {
    p2 = p;
    while(*p2 != '\0' && *p2 != '\r' && *p2 != '\n' && *p2 != ';') {
      p2++;
    }
    strncpy(pSession->s.sessionid, p, MIN(sizeof(pSession->s.sessionid) - 1, (p2 - p)));

    //
    // Check for 'timeout=' field
    // 
    while(*p2 == ';') {
      p2++;
    }
    if(!strncasecmp(p2, RTSP_CLIENT_TIMEOUT, strlen(RTSP_CLIENT_TIMEOUT))) {
      p2 += strlen(RTSP_CLIENT_TIMEOUT);
    }
    while(*p2 == '=') {
      p2++;
    }
    p = p2;
    while(*p != '\0' && *p != '\r' && *p != '\n' && *p2 != ';') {
      p++;
    }
    if(p > p2) {
      strncpy(tmp, p2, MIN(sizeof(tmp) - 1, (p - p2)));
      if((i = atoi(p2)) > 0 && i != pSession->s.expireSec) {
        LOG(X_DEBUG("Updated RTSP session timeout %d -> %d"), pSession->s.expireSec, i);
        pSession->s.expireSec = i;
      } 
    }
    //fprintf(stderr, "TIMEOUT:'%s' %d %d\n", p2, i, pSession->s.expireSec);
  }

  return rc;
}

static CONNECT_RETRY_RC_T rtsp_options_rcv(RTSP_CLIENT_SESSION_T *pSession, HTTP_PARSE_CTXT_T *pHdrCtxt) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  char buf[2048];

  if(!pHdrCtxt) {
    memset(&rtspResp, 0, sizeof(rtspResp));
    memset(&hdrCtxt, 0, sizeof(hdrCtxt));
    hdrCtxt.pnetsock = &pSession->sd.netsocket;
    hdrCtxt.pbuf = buf;
    hdrCtxt.szbuf = sizeof(buf);
    pHdrCtxt = &hdrCtxt;
  }
 
  if(rtsp_read_resp(pSession, pHdrCtxt, &rtspResp) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), pSession->authCliCtxt.lastMethod);
    rc = CONNECT_RETRY_RC_ERROR;
  } else if(pSession->s.sessionid[0] != '\0') {
    gettimeofday(&pSession->s.tvlastupdate, NULL);
  }

  return rc;
}

static CONNECT_RETRY_RC_T rtsp_req_options(RTSP_CLIENT_SESSION_T *pSession, const char *url, int dorcv) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  char *strMethod = RTSP_REQ_OPTIONS;

  if(rtsp_req_send(pSession, strMethod, url, NULL, NULL) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  if(dorcv) {
    rc = rtsp_options_rcv(pSession, NULL);
  }

  return rc;
}

int rtsp_read_sdp(HTTP_PARSE_CTXT_T *pHdrCtxt, const KEYVAL_PAIR_T *hdrPairs, const char *strMethod,
                  SDP_DESCR_T *pSdp) {
  int rc = 0;
  int contentLen = 0;
  int szread = 0;
  const char *p;

  if(!pHdrCtxt || !pHdrCtxt->pbuf || !hdrPairs || !pSdp) {
    return -1;
  }

  // 
  // Check the Content-Type header
  //
  if(rc >= 0 && 
     (!(p = conf_find_keyval(hdrPairs, RTSP_HDR_CONTENT_TYPE)) ||
     strncasecmp(CONTENT_TYPE_SDP, p, strlen(CONTENT_TYPE_SDP)))) {
    LOG(X_ERROR("Expected %s "RTSP_HDR_CONTENT_TYPE" %s, got '%s'"), strMethod, CONTENT_TYPE_SDP, (p ? p : ""));
    rc = -1;
  }

  //
  // Get the Content-Length header
  //
  if(rc >= 0 && (!(p = conf_find_keyval(hdrPairs, RTSP_HDR_CONTENT_LEN)) || (contentLen = atoi(p)) <= 0)) {
    LOG(X_ERROR("Invalid %s "RTSP_HDR_CONTENT_LEN" '%s'"), strMethod, (p ? p : ""));
    rc = -1;
  }

  //LOG(X_DEBUG("RTSP READ SDP... contentLen:%d, idxbuf:%d, hdrsLen:%d"), contentLen, pHdrCtxt->idxbuf, pHdrCtxt->hdrslen);

  //
  // Read the SDP contained in the content data
  //
  if(rc >= 0) {
    szread = contentLen;
    if(pHdrCtxt->idxbuf > pHdrCtxt->hdrslen) {
      szread -= (pHdrCtxt->idxbuf - pHdrCtxt->hdrslen);
      if(pHdrCtxt->hdrslen + contentLen > pHdrCtxt->szbuf) {
        LOG(X_ERROR("SDP "RTSP_HDR_CONTENT_LEN": %d response too large!"), contentLen); 
        rc = -1;
      }
    }
  }
  //LOG(X_DEBUG("GONA READ SDP rc: %d, contentLen:%d, szread:%d, idxbuf:%d, hdrslen:%d, FD:%d "), rc, contentLen, szread, pHdrCtxt->idxbuf, pHdrCtxt->hdrslen, NETIOSOCK_FD(pHdrCtxt->pnetsock));

  if(rc >= 0 && szread > 0) {

   if((rc = netio_recv(pHdrCtxt->pnetsock, NULL, (unsigned char *) &pHdrCtxt->pbuf[pHdrCtxt->idxbuf], 
                       szread)) < 0) {
      LOG(X_ERROR("Failed to read %s SDP %d / %d"), strMethod, szread, contentLen);
      return rc;
    }
    pHdrCtxt->idxbuf += rc;
  }

  //LOG(X_DEBUG("IDXBUF NOW %d"), pHdrCtxt->idxbuf);

  if(rc >= 0) {
    VSX_DEBUG_RTSP(
      LOG(X_DEBUG("GOT SDP length: %d / "RTSP_HDR_CONTENT_LEN": %d"), rc, contentLen);
      LOGHEXT_DEBUG(&pHdrCtxt->pbuf[pHdrCtxt->hdrslen], contentLen);
    )
  }

  //
  // Parse the received SDP
  //
  if(rc >= 0 && (rc = sdp_parse(pSdp, &pHdrCtxt->pbuf[pHdrCtxt->hdrslen], contentLen, 0, 1)) < 0) {
    LOG(X_ERROR("Failed to parse %s SDP %d"), strMethod,  contentLen);
  }

  return rc;
}

static int rtsp_req_describe(RTSP_CLIENT_SESSION_T *pSession) {
  int rc = 0;
  const char *p;
  char *strMethod = RTSP_REQ_DESCRIBE;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  char buf[4096];
  char bufhdrs[1024];

  //
  // Create Header  Accept: application/sdp
  //
  snprintf(bufhdrs, sizeof(bufhdrs), "%s: %s\r\n", RTSP_HDR_ACCEPT, CONTENT_TYPE_SDP);

  if((rc = rtsp_req_send(pSession, strMethod, pSession->s.vid.url, NULL, bufhdrs)) < 0) {
    return rc;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = buf;
  hdrCtxt.szbuf = sizeof(buf);

  if((rc = rtsp_read_resp(pSession, &hdrCtxt, &rtspResp)) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
  } 

  if((rc = rtsp_read_sdp(&hdrCtxt, rtspResp.hdrPairs, strMethod, &pSession->sdp)) < 0) {
  }

  //
  // Check and read Content-Base header and replace existing URI
  // The RTSP PLAY, PAUSE, TEARDOWN URI will be the Content-Base from the server's DESCRIBE response, and not
  // the original URI used for DESCRIBE, SETUP
  //
  if(rc >= 0 && (p = conf_find_keyval(rtspResp.hdrPairs, RTSP_HDR_CONTENT_BASE))) {
    LOG(X_DEBUG("Setting RTSP "RTSP_HDR_CONTENT_BASE" to '%s'"), p);
    strncpy(pSession->s.vid.url, p, sizeof(pSession->s.vid.url) - 1);
    strncpy(pSession->s.aud.url, p, sizeof(pSession->s.aud.url) - 1);
  }

  if(rc >= 0 && pSession->s.sessionid[0] != '\0') {
    gettimeofday(&pSession->s.tvlastupdate, NULL);
  }

  //fprintf(stderr, "rc:%d DESCRIBE content len:%d read:%d (%d,%d) trackId:%d,%d\n", rc, contentLen, szread, hdrCtxt.hdrslen, hdrCtxt.idxbuf, pSession->sdp.vid.common.controlId, pSession->sdp.aud.common.controlId);
  //if(rc>=0) avc_dumpHex(stderr, &buf[hdrCtxt.hdrslen], contentLen, 1);

  return rc;
}

static int capture_rtsp_req_setupchannel(RTSP_CLIENT_SESSION_T *pSession, int isvid, 
                                         RTSP_TRANSPORT_SECURITY_TYPE_T rtspsecuritytype) {
  int rc = 0;
  char *strMethod = RTSP_REQ_SETUP;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  RTSP_SESSION_PROG_T *pProg = NULL;
  SDP_STREAM_DESCR_T *pSdpProg = NULL;
  size_t sz;
  unsigned int idx = 0;
  char strTrackId[32];
  char buf[2048];

  if(isvid) {
    pProg = &pSession->s.vid;
    pSdpProg = &pSession->sdp.vid.common;
  } else {
    pProg = &pSession->s.aud;
    pSdpProg = &pSession->sdp.aud.common;
  }

  //
  // Add the trackId to the URI
  //
  if((sz = strlen(pProg->url)) > 0 && pProg->url[sz - 1] != '/') {
    strTrackId[idx++] = '/';
  }
  snprintf(&strTrackId[idx], sizeof(strTrackId) - idx, "%s=%d", RTSP_TRACKID, pSdpProg->controlId);

  if((rc = rtsp_setup_transport_str(pSession->s.sessionType, pSession->ctxtmode,
                                    rtspsecuritytype, pProg, NULL, buf, sizeof(buf))) < 0) {
    return rc;
  }

  if((rc = rtsp_req_send(pSession, strMethod, pProg->url, strTrackId, buf)) < 0) {
    return rc;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = buf;
  hdrCtxt.szbuf = sizeof(buf);

  if((rc = rtsp_read_resp(pSession, &hdrCtxt, &rtspResp)) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
  } else {
    if(pSession->s.sessionid[0] != '\0') {
      gettimeofday(&pSession->s.tvlastupdate, NULL);
    }
    pProg->issetup = 1;
    //fprintf(stderr, "public:'%s'\n", conf_find_keyval(rtspResp.hdrPairs, RTSP_HDR_PUBLIC));
  }

  return rc;
}

static int rtsp_req_play(RTSP_CLIENT_SESSION_T *pSession) {
  int rc = 0;
  char *strMethod = RTSP_REQ_PLAY;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  char buf[2048];

  //
  // Create Range (time) Header
  //
  snprintf(buf, sizeof(buf), "%s: %s\r\n", RTSP_HDR_RANGE, "npt=0.000-");

  if((rc = rtsp_req_send(pSession, strMethod, pSession->s.vid.url, NULL, buf)) < 0) {
    return rc;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = buf;
  hdrCtxt.szbuf = sizeof(buf);

  if((rc = rtsp_read_resp(pSession, &hdrCtxt, &rtspResp)) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
    return rc;
  }

  if(pSession->s.sessionid[0] != '\0') {
    gettimeofday(&pSession->s.tvlastupdate, NULL);
  }

  if(pSession->s.vid.issetup) {
    pSession->s.vid.isplaying = 1;
  }
  if(pSession->s.aud.issetup) {
    pSession->s.aud.isplaying = 1;
  }

  return rc;
}

static int rtsp_req_teardown(RTSP_CLIENT_SESSION_T *pSession) {
  int rc = 0;
  char *strMethod = RTSP_REQ_TEARDOWN;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  char buf[2048];

  if((net_issockremotelyclosed(NETIOSOCK_FD(pSession->sd.netsocket), 1))) {
    return 0;
  }

  if((rc = rtsp_req_send(pSession, strMethod, pSession->s.vid.url, NULL, NULL)) < 0) {
    return rc;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = buf;
  hdrCtxt.szbuf = sizeof(buf);

  if((rc = rtsp_read_resp(pSession, &hdrCtxt, &rtspResp)) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
  }

  return rc;
}

int rtsp_alloc_listener_ports(RTSP_SESSION_T *pRtspSession, int staticLocalPort, 
                              int rtpmux, int rtcpmux, int isvid, int isaud) {
  int rc = 0;
  int numportpairs = 0;
  int startmediaport = 0;
  int rtcpportdelta = RTCP_PORT_FROM_RTP(0);

  if(rtcpmux) {
    rtcpportdelta = 0;
  }

  if(isvid) {
    numportpairs++;
  }

  if(isaud) {
    numportpairs++;
  }
  if(rtpmux && numportpairs > 0) {
    numportpairs = 1;
  }

  //
  // Some broadcast servers require video and audio ports to be contiguous
  //
  if(staticLocalPort) {

    //
    // Some RTSP firewalls / SBCs require source port to always be in a range 6970 - 9999
    // or even 6970-6971 only
    //
    startmediaport = staticLocalPort;

  } else if((startmediaport = net_getlocalmediaport(numportpairs)) < 0) {
    //
    // Create contiguous series of ports
    // Some media clients/servers may not work properly with different
    // video / audio port ranges, 
    //
    LOG(X_ERROR("Unable to allocate %d local ports"), numportpairs * 2);
    return -1;
  }


  VSX_DEBUG_RTSP( LOG(X_DEBUG("Allocated starting port: %d (x %d)"), startmediaport, numportpairs) );
  
  if(isvid) {
    pRtspSession->vid.localports[0] = startmediaport;
    pRtspSession->vid.localports[1] = pRtspSession->vid.localports[0] + rtcpportdelta;
    if(!staticLocalPort && !rtpmux) {
      startmediaport += 2;
    }
  }

  if(isaud) {
    pRtspSession->aud.localports[0] = startmediaport;
    pRtspSession->aud.localports[1] = pRtspSession->aud.localports[0] + rtcpportdelta;
    startmediaport += 2;
  }

  return rc;
}

int rtsp_setup_transport_str(RTSP_SESSION_TYPE_T sessionType, RTSP_CTXT_MODE_T ctxtmode, 
                             RTSP_TRANSPORT_SECURITY_TYPE_T rtspsecuritytype, 
                             const RTSP_SESSION_PROG_T *pProg, const struct sockaddr *paddr,
                             char *buf, unsigned int szbuf) {
  int rc = 0;
  unsigned int idx = 0;
  char tmp[128];

  if(!pProg || !buf) {
    return -1;
  }

  if((sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {

    if((rc = snprintf(buf, szbuf, "%s: %s;%s;%s=%d-%d\r\n",
             RTSP_HDR_TRANSPORT, SDP_TRANS_RTP_AVP_TCP, RTSP_TRANS_UNICAST,
             RTSP_TRANS_INTERLEAVED, pProg->ports[0], pProg->ports[1])) > 0) {
      idx += rc;
    }

  } else {

    if((rc = snprintf(&buf[idx], szbuf - idx, "%s: %s;%s",
             RTSP_HDR_TRANSPORT, 
             (rtspsecuritytype == RTSP_TRANSPORT_SECURITY_TYPE_SRTP ? SDP_TRANS_SRTP_AVP :  SDP_TRANS_RTP_AVP), 
             pProg->multicast ? RTSP_TRANS_MULTICAST : RTSP_TRANS_UNICAST)) > 0) {
      idx += rc;
    }

    if(paddr && (rc = snprintf(&buf[idx], szbuf - idx, ";%s=%s", RTSP_SOURCE, 
                FORMAT_NETADDR(*paddr, tmp, sizeof(tmp)))) > 0) {
      idx += rc;
    }

    if((rc = snprintf(&buf[idx], szbuf - idx, ";%s=%d-%d",
             ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE ? RTSP_SERVER_PORT : RTSP_CLIENT_PORT, 
             pProg->localports[0], pProg->localports[1])) > 0) {
      idx += rc;
    }

    if(pProg->ports[0] > 0 && pProg->ports[1] > 0 && (rc = snprintf(&buf[idx], szbuf - idx, ";%s=%d-%d", 
             ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE ? RTSP_CLIENT_PORT : RTSP_SERVER_PORT, 
             pProg->ports[0], pProg->ports[1])) > 0) {
      idx += rc;
    }

    if((rc = snprintf(&buf[idx], szbuf - idx, "\r\n")) > 0) {
      idx += rc;
    }
  }

  return (int) idx;
}

int capture_rtsp_onsetupdone(RTSP_CLIENT_SESSION_T *pSession, CAPTURE_DESCR_COMMON_T *pCommon) {
  int rc = 0;

  if(!pSession || !pCommon) {
    return -1;
  }

  pCommon->filt.numFilters = 0;

  if((rc = capture_filterFromSdp(NULL, pCommon->filt.filters,
                                 pSession->localAddrBuf, sizeof(pSession->localAddrBuf), &pSession->sdp,
                                 pCommon->novid, pCommon->noaud)) <= 0) {
    LOG(X_ERROR("Unable to process RTSP SDP for capture"));
    return -1;
  } 

  LOG(X_DEBUG("Loaded RTSP SDP from body"));
  pCommon->localAddrs[0] = pSession->localAddrBuf;
  pCommon->addrsExt[0] = NULL;
  pCommon->filt.numFilters = rc;
  //TODO: setting transType shouldn't be necessary.. should already be set to rtp/srtp in capture_filterFromSdp
  pCommon->filt.filters[0].transType = CAPTURE_FILTER_TRANSPORT_UDPRTP;
  pCommon->filt.filters[1].transType = CAPTURE_FILTER_TRANSPORT_UDPRTP;

  LOG(X_DEBUG("Setup RTSP%s session %s"),
              (pSession->s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? " (interleaved)" : "",
              pSession->s.sessionid);

  if(pCommon->verbosity >= VSX_VERBOSITY_HIGH) {
    sdp_write(&pSession->sdp, NULL, S_DEBUG);
  }

  pSession->issetup = 1;

  return rc;
}

static CONNECT_RETRY_RC_T capture_client_rtsp_setupstream(CAPTURE_DESCR_COMMON_T *pCommon, 
                                                          const STREAMER_CFG_T *pStreamerCfg, const char *url) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  RTSP_CLIENT_SESSION_T *pSession = &pCommon->rtsp;

  if(!pStreamerCfg) {
    return -1;
  }

  //
  // Set the video and audio RTSP URLs
  //
  snprintf(pSession->s.vid.url, sizeof(pSession->s.vid.url), "%s", url);
  memcpy(pSession->s.aud.url, pSession->s.vid.url, sizeof(pSession->s.aud.url));

  pSession->cseq++;
  if(rtsp_req_options(pSession, pSession->s.vid.url, 1) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  //TODO: should not call rtsp_setupurl which does not maintain state, after auth fails - after succesful options

  pSession->cseq++;
  if(rtsp_req_describe(pSession) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  if((rc = rtsp_alloc_listener_ports(&pSession->s, pStreamerCfg->pRtspSessions->staticLocalPort,
                                  (pCommon->streamflags & VSX_STREAMFLAGS_RTPMUX),
                                  (pCommon->streamflags & VSX_STREAMFLAGS_RTCPMUX),
                                  pSession->sdp.vid.common.available, pSession->sdp.aud.common.available)) < 0) {

    return CONNECT_RETRY_RC_NORETRY;
  }

  if((pSession->s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
    pSession->s.vid.ports[0] = 0;
    pSession->s.vid.ports[1] = pSession->s.vid.ports[0] + 1;
    pSession->s.aud.ports[0] = (pSession->sdp.vid.common.available ? 2 : 0);
    pSession->s.aud.ports[1] = pSession->s.aud.ports[0] + 1;
  } 

  if(pSession->sdp.vid.common.available) {
    pSession->cseq++;

    if(capture_rtsp_req_setupchannel(pSession, 1, pStreamerCfg->pRtspSessions->rtspsecuritytype) < 0) {
      return CONNECT_RETRY_RC_ERROR;
    } else {
      pSession->sdp.vid.common.port = pSession->s.vid.localports[0];
      pSession->sdp.vid.common.portRtcp = pSession->s.vid.localports[1];
    }
  }
  if(pSession->sdp.aud.common.available) {
    pSession->cseq++;

    if(capture_rtsp_req_setupchannel(pSession, 0, pStreamerCfg->pRtspSessions->rtspsecuritytype) < 0) {
      return CONNECT_RETRY_RC_ERROR;
    } else {
      pSession->sdp.aud.common.port = pSession->s.aud.localports[0];
      pSession->sdp.aud.common.portRtcp = pSession->s.aud.localports[1];
    }
  }

  return rc;
} 

static CONNECT_RETRY_RC_T stream_rtsp_req_announce(STREAMER_CFG_T *pStreamerCfg, RTSP_CLIENT_SESSION_T *pSession, 
                                                   SRTP_CTXT_KEY_TYPE_T srtpKts[STREAMER_PAIR_SZ]) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  char *strMethod = RTSP_REQ_ANNOUNCE;
  char bufsdp[4096];
  char bufhdrs[1024];
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  unsigned int sdplen = sizeof(bufsdp);
  unsigned int idxhdrs = sizeof(bufhdrs);

  if(rtsp_prepare_sdp(pStreamerCfg, pSession->s.requestOutIdx, bufsdp, &sdplen, bufhdrs, &idxhdrs,
                      srtpKts, &pSession->sdp) < 0) {
    return CONNECT_RETRY_RC_NORETRY;
  }

  if(rtsp_req_send(pSession, strMethod, pSession->s.vid.url, NULL, bufhdrs) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  if(sdplen > 0) {

    VSX_DEBUG_RTSP(
      LOG(X_DEBUG("Sending SDP body %d bytes"), sdplen);
      LOGHEXT_DEBUG(bufsdp, sdplen);
    )

    if(netio_send(&pSession->sd.netsocket, (const struct sockaddr *) &pSession->sd.sa, (unsigned char *) bufsdp, sdplen) < 0) {
      LOG(X_DEBUG("Failed to send RTSP ANNOUNCE SDP length %d"), sdplen);
      return CONNECT_RETRY_RC_ERROR;    
    }
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = bufsdp;
  hdrCtxt.szbuf = sizeof(bufsdp);

  if(rtsp_read_resp(pSession, &hdrCtxt, &rtspResp) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
    rc = CONNECT_RETRY_RC_ERROR;
  } else {
    if(pSession->s.sessionid[0] != '\0') {
      gettimeofday(&pSession->s.tvlastupdate, NULL);
    }
  }

  return rc;
}

static CONNECT_RETRY_RC_T stream_rtsp_req_setupchannel(STREAMER_CFG_T *pStreamerCfg, 
                                                       RTSP_CLIENT_SESSION_T *pSession, int isvid) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  char *strMethod = RTSP_REQ_SETUP;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  RTSP_SESSION_PROG_T *pProg = NULL;
  SDP_STREAM_DESCR_T *pSdpProg = NULL;
  size_t sz;
  unsigned int idx = 0;
  const char *pargTrans = NULL;
  char strTrackId[32];
  char bufhdrs[1024];

  if(isvid) {
    pProg = &pSession->s.vid;
    pSdpProg = &pSession->sdp.vid.common;
  } else {
    pProg = &pSession->s.aud;
    pSdpProg = &pSession->sdp.aud.common;
  }

  if(!INET_ADDR_VALID(pProg->dstAddr)) {
    memcpy(&pProg->dstAddr, &pSession->sd.sa, sizeof(pProg->dstAddr));
  }

  //
  // Add the trackId to the URI
  //
  if((sz = strlen(pProg->url)) > 0 && pProg->url[sz - 1] != '/') {
    strTrackId[idx++] = '/';
  }
  snprintf(&strTrackId[idx], sizeof(strTrackId) - idx, "%s=%d", RTSP_TRACKID, pSdpProg->controlId);


  if((rc = rtsp_setup_transport_str(pSession->s.sessionType, pSession->ctxtmode,
                 pStreamerCfg->pRtspSessions->rtspsecuritytype, pProg, NULL, bufhdrs, sizeof(bufhdrs))) < 0) {
    return rc;
  }

  if(rtsp_req_send(pSession, strMethod, pProg->url, strTrackId, bufhdrs) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = bufhdrs;
  hdrCtxt.szbuf = sizeof(bufhdrs);

  if(rtsp_read_resp(pSession, &hdrCtxt, &rtspResp) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
    return CONNECT_RETRY_RC_ERROR;
  } 

  if(pSession->s.sessionid[0] != '\0') {
    gettimeofday(&pSession->s.tvlastupdate, NULL);
  }

  if(!(pargTrans = conf_find_keyval(rtspResp.hdrPairs, RTSP_HDR_TRANSPORT)) || 
     rtsp_get_transport_keyvals(pargTrans, pProg, 0) < 0) {
    LOG(X_ERROR("RTSP SETUP response contains invalid "RTSP_HDR_TRANSPORT" header: "), pargTrans);
    return  CONNECT_RETRY_RC_NORETRY;
  }   

  pProg->issetup = 1;

  VSX_DEBUG_RTSP(
    LOG(X_DEBUG("RTSP - remote-ports: %d,%d, local-ports: %d,%d"), 
          pProg->ports[0], pProg->ports[1], pProg->localports[0], pProg->localports[1]));

  return rc;
}

static CONNECT_RETRY_RC_T stream_rtsp_req_play(RTSP_REQ_CTXT_T *pReqCtxt, RTSP_CLIENT_SESSION_T *pSession) {
                                               
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  char *strMethod = RTSP_REQ_PLAY;
  char buf[1024];
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;

  if(rtsp_req_send(pSession, strMethod, pSession->s.vid.url, NULL, NULL) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = buf;
  hdrCtxt.szbuf = sizeof(buf);

  if(rtsp_read_resp(pSession, &hdrCtxt, &rtspResp) < 0) {
    LOG(X_ERROR("Invalid RTSP %s response."), strMethod);
    rc = CONNECT_RETRY_RC_ERROR;
  } else {
    if(pSession->s.sessionid[0] != '\0') {
      gettimeofday(&pSession->s.tvlastupdate, NULL);
    }
  }

  rc = rtsp_do_play(pReqCtxt, NULL, 0);

  return rc;
}

int stream_rtsp_close(STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;

  if(!pStreamerCfg) {
    return -1;
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - stream_rtsp_close Close 0x%x 0x%x"), pStreamerCfg, pStreamerCfg->rtspannounce.pSession) );

  pthread_mutex_lock(&pStreamerCfg->rtspannounce.mtx);
  rc = capture_rtsp_close(pStreamerCfg->rtspannounce.pSession);
  pStreamerCfg->rtspannounce.pSession = NULL;
  pthread_mutex_unlock(&pStreamerCfg->rtspannounce.mtx);

  return rc;
}

static CONNECT_RETRY_RC_T rtsp_announcestream(RTSP_REQ_CTXT_T *pReqCtxt, RTSP_CLIENT_SESSION_T *pSession, 
                                           const char *purl) {

  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  unsigned int outidx = pSession->s.requestOutIdx;
  STREAMER_CFG_T *pStreamerCfg = pReqCtxt->pStreamerCfg;

  //
  // Set the video and audio RTSP URLs
  //
  snprintf(pSession->s.vid.url, sizeof(pSession->s.vid.url), "%s", purl);
  memcpy(pSession->s.aud.url, pSession->s.vid.url, sizeof(pSession->s.aud.url));

  pSession->cseq++;
  if((rc = rtsp_req_options(pSession, purl, 1)) < CONNECT_RETRY_RC_OK) {
    return rc;
  }

  pSession->cseq++;
  if((rc = stream_rtsp_req_announce(pStreamerCfg, pSession, pReqCtxt->srtpKts)) < CONNECT_RETRY_RC_OK) {
    return rc; 
  }

  if((rc = rtsp_alloc_listener_ports(&pSession->s, pStreamerCfg->pRtspSessions->staticLocalPort,
                                  (pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTPMUX),
                                  (pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX),
                                  pSession->sdp.vid.common.available, pSession->sdp.aud.common.available)) < 0) {
    return CONNECT_RETRY_RC_NORETRY;
  }

  if((pSession->s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
    pSession->s.vid.ports[0] = 0;
    pSession->s.vid.ports[1] = pSession->s.vid.ports[0] + 1;
    pSession->s.aud.ports[0] = (pSession->sdp.vid.common.available ? 2 : 0);
    pSession->s.aud.ports[1] = pSession->s.aud.ports[0] + 1;
  }


  if(pSession->sdp.vid.common.available) {
    pSession->cseq++;

    if((rc = stream_rtsp_req_setupchannel(pStreamerCfg, pSession, 1)) < CONNECT_RETRY_RC_OK) {
      return rc;
    } else {
      pStreamerCfg->sdpsx[1][outidx].vid.common.port = pSession->s.vid.ports[0];
      pStreamerCfg->sdpsx[1][outidx].vid.common.portRtcp = pSession->s.vid.ports[1]; 
    }
  }

  if(pSession->sdp.aud.common.available) {
    pSession->cseq++;
    
    if((rc = stream_rtsp_req_setupchannel(pStreamerCfg, pSession, 0)) < CONNECT_RETRY_RC_OK) {
      return rc;
    } else {
      pStreamerCfg->sdpsx[1][outidx].aud.common.port = pSession->s.aud.ports[0];
      pStreamerCfg->sdpsx[1][outidx].aud.common.portRtcp = pSession->s.aud.ports[1]; 
    }
  }

  pSession->issetup = 1;

  pSession->cseq++;
  if((rc = stream_rtsp_req_play(pReqCtxt, pSession)) < CONNECT_RETRY_RC_OK) {
    return rc; 
  }

  if((pSession->s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED) && (rc = start_client_monitor(pSession)) < 0) {
    return rc;
  }

  pSession->isplaying = 1;
  pStreamerCfg->rtspannounce.isannounced = 1;

  //
  // Signal the conditional indicating a that streaming should start
  // This will cause stream_rtsp_announce_start to return to it's caller
  //
  vsxlib_cond_signal(&pStreamerCfg->rtspannounce.cond.cond, &pStreamerCfg->rtspannounce.cond.mtx);

  if((pSession->s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
    rc = rtsp_run_interleavedclient(pSession, NULL, pReqCtxt);
  } else {
    pSession->runMonitor = STREAMER_STATE_STARTING_THREAD;
    rtsp_client_proc_monitor(pSession);
  }

  return rc;
} 

typedef struct CONNECT_RETRY_CTXT_RTSP_ANNOUNCE {
  RTSP_REQ_CTXT_T           *pReqCtxt;
  RTSP_CLIENT_SESSION_T     *pSession;
  const char                *purl;
} CONNECT_RETRY_CTXT_RTSP_ANNOUNCE_T;

CONNECT_RETRY_RC_T stream_rtsp_cbonannouncestream(void *pArg) {
  CONNECT_RETRY_CTXT_RTSP_ANNOUNCE_T *pCtxt = (CONNECT_RETRY_CTXT_RTSP_ANNOUNCE_T *) pArg;
  return rtsp_announcestream(pCtxt->pReqCtxt, pCtxt->pSession, pCtxt->purl);
}


static int stream_rtsp_announce(STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;
  unsigned int outidx = 0;
  STREAMER_DEST_CFG_T *pDestCfg;
  char url[RTSP_URL_LEN];
  unsigned int idx;
  RTSP_REQ_CTXT_T reqCtxt;
  RTSP_CLIENT_SESSION_T session;
  CONNECT_RETRY_CTXT_T retryCtxt;
  CONNECT_RETRY_CTXT_RTSP_ANNOUNCE_T retryCbCtxt;

  if(!pStreamerCfg || !(pDestCfg = &pStreamerCfg->pdestsCfg[0])) {
    return -1;
  }

  //
  // Make sure we have an actual SDP for streaming
  //
  if(rtsp_wait_for_sdp(pStreamerCfg, outidx, 10) < 0) {
    return -1;
  }

  memset(&reqCtxt, 0, sizeof(reqCtxt));
  reqCtxt.pStreamerCfg = pStreamerCfg;
  reqCtxt.pSession = &session.s;
  reqCtxt.pSd = &session.sd;
  //BYTE_STREAM_T            bs;
  pthread_mutex_init(&reqCtxt.mtx, NULL);

  memset(&session, 0, sizeof(session));
  NETIOSOCK_FD(session.sd.netsocket) = INVALID_SOCKET;
  session.authCliCtxt.pcreds = &pStreamerCfg->creds[STREAMER_AUTH_IDX_RTSPANNOUNCE].stores[0];
  session.authCliCtxt.puri = pDestCfg->dstUri;
  session.s.inuse = 1;
  session.s.requestOutIdx = outidx;
  
  if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pDestCfg->outTransType)) {
    session.sd.netsocket.flags |= NETIO_FLAG_SSL_TLS;
  }

  if(pStreamerCfg->pRtspSessions->rtsptranstype == RTSP_TRANSPORT_TYPE_INTERLEAVED) {
    session.s.sessionType |= RTSP_SESSION_TYPE_INTERLEAVED;
  }

  //
  // We're acting as an RTSP client sending an ANNOUNCE to a remote server before sending media
  //
  session.ctxtmode = RTSP_CTXT_MODE_CLIENT_STREAM;
  session.runMonitor =  STREAMER_STATE_FINISHED;

  //
  // Create the request URL
  //
  if(snprintf(url, sizeof(url), URL_RTSP_FMT_STR"%s", 
                            URL_HTTP_FMT_PROTO_HOST(session.sd.netsocket.flags, 
                            pDestCfg->dstHost), pDestCfg->ports[0], pDestCfg->dstUri) < 0) {
    return -1;
  }

  //
  // Copy the same SRTP configuration that would have been used if we're doing --stream=srtp:// output
  //
  for(idx = 0; idx < STREAMER_PAIR_SZ; idx++ ) {
    memcpy(&reqCtxt.srtpKts[idx], &pDestCfg->srtps[idx].kt, sizeof(reqCtxt.srtpKts[idx]));
  }

  if(capture_getdestFromStr(pDestCfg->dstHost, &session.sd.sa, NULL, NULL, pDestCfg->ports[0]) < 0) {
    return -1;
  }

  pStreamerCfg->rtspannounce.pSession = &session;

  //
  // Initialize the retry context for connecting to the remote endpoint
  //
  memset(&retryCtxt, 0, sizeof(retryCtxt));
  retryCbCtxt.pReqCtxt = &reqCtxt;
  retryCbCtxt.pSession = &session;
  retryCbCtxt.purl = url;
  retryCtxt.cbConnectedAction = stream_rtsp_cbonannouncestream;
  retryCtxt.pConnectedActionArg = &retryCbCtxt;;
  retryCtxt.pAuthCliCtxt = &session.authCliCtxt;
  retryCtxt.pnetsock = &session.sd.netsocket;
  retryCtxt.psa = (struct sockaddr *) &session.sd.sa;
  retryCtxt.connectDescr = "RTSP";
  retryCtxt.pconnectretrycntminone = &pStreamerCfg->rtspannounce.connectretrycntminone;

  //
  // Connect to the remote and call capture_rtsp_cbonannounceurl
  // This will automatically invoke any retry logic according to the retry config
  //
  rc = connect_with_retry(&retryCtxt);

  //pthread_mutex_lock(&pStreamerCfg->rtspannounce.mtx);
  //capture_rtsp_close(&session);
  //pStreamerCfg->rtspannounce.pSession = NULL;
  //pthread_mutex_unlock(&pStreamerCfg->rtspannounce.mtx);

  stream_rtsp_close(pStreamerCfg);

  //
  // Stop any on-going RTP streamer
  //
  streamer_stop(pStreamerCfg, 1, 0, 0, 0);

  return rc;
}

static void rtsp_stream_announce_proc(void *pArg) {
  STREAMER_CFG_T *pStreamerCfg = (STREAMER_CFG_T *) pArg;

  LOG(X_DEBUG("RTSP stream announce thread started"));

  stream_rtsp_announce(pStreamerCfg);

  vsxlib_cond_signal(&pStreamerCfg->rtspannounce.cond.cond, &pStreamerCfg->rtspannounce.cond.mtx);

  LOG(X_DEBUG("RTSP stream announce thread exiting"));
}

int stream_rtsp_announce_start(STREAMER_CFG_T *pStreamerCfg, int wait_for_setup) {

  int rc = 0;
  pthread_t ptdMonitor;
  pthread_attr_t attrInterleaved;

  if(!pStreamerCfg) {
    return -1;
  }


  pthread_attr_init(&attrInterleaved);
  pthread_attr_setdetachstate(&attrInterleaved, PTHREAD_CREATE_DETACHED);

  pthread_mutex_init(&pStreamerCfg->rtspannounce.cond.mtx, NULL);
  pthread_cond_init(&pStreamerCfg->rtspannounce.cond.cond, NULL);

  if(pthread_create(&ptdMonitor,
                    &attrInterleaved,
                    (void *) rtsp_stream_announce_proc,
                    (void *) pStreamerCfg) != 0) {
    LOG(X_ERROR("Unable to create RTSP stream announce thread"));

    pthread_cond_destroy(&pStreamerCfg->rtspannounce.cond.cond);
    pthread_mutex_destroy(&pStreamerCfg->rtspannounce.cond.mtx);
    return -1;
  }

  if(wait_for_setup) {
    if((rc = vsxlib_cond_waitwhile(&pStreamerCfg->rtspannounce.cond, NULL, 0)) >= 0) {
      if(!pStreamerCfg->rtspannounce.isannounced) {
        rc = -1;
      }
    }
    LOG(X_DEBUG("RTSP stream announce done waiting for start completion isannounced:%d"), 
        pStreamerCfg->rtspannounce.isannounced);
  }

  if(rc < 0) {
    stream_rtsp_close(pStreamerCfg);
  }

  return rc;

}

typedef struct CONNECT_RETRY_CTXT_RTSP {
  const STREAMER_CFG_T      *pStreamerCfg;
  CAPTURE_DESCR_COMMON_T    *pCommon;
  const char                *purl; 
} CONNECT_RETRY_CTXT_RTSP_T;

CONNECT_RETRY_RC_T  capture_client_rtsp_cbonsetupstream(void *pArg) {
  CONNECT_RETRY_CTXT_RTSP_T *pCtxt = (CONNECT_RETRY_CTXT_RTSP_T *) pArg;
  return capture_client_rtsp_setupstream(pCtxt->pCommon, pCtxt->pStreamerCfg, pCtxt->purl);
}

int capture_rtsp_setup(CAPTURE_DESCR_COMMON_T *pCommon, const STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;
  const char *purl = NULL;
  RTSP_CLIENT_SESSION_T *pSession = NULL;
  CONNECT_RETRY_CTXT_T retryCtxt;
  CONNECT_RETRY_CTXT_RTSP_T retryCbCtxt;

  if(!pCommon || pCommon->filt.numFilters <= 0 || 
     !(purl = pCommon->rtsp.authCliCtxt.puri) || purl[0] == '\0' || !(pSession = &pCommon->rtsp)) {
    return -1;
  }

  //
  // We're acting as an RTSP client sending a DESCRIBE to a remote server before ingesting media 
  //
  pSession->ctxtmode = RTSP_CTXT_MODE_CLIENT_CAPTURE;
  pCommon->rtsp.runMonitor = STREAMER_STATE_FINISHED;

  if(pSession->s.expireSec == 0) {
    pSession->s.expireSec = RTSP_SESSION_IDLE_SEC_SPEC;
  }
  pSession->issetup = 0;
  pSession->isplaying = 0;
  NETIOSOCK_FD(pSession->sd.netsocket) = INVALID_SOCKET;

  if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pCommon->filt.filters[0].transType)) {
    pSession->sd.netsocket.flags |= NETIO_FLAG_SSL_TLS;
    //
    // If using SSL, default to use TCP interleaved unless '--rtsp-interleaved=0' was explicitly specified
    //
    if(pSession->rtsptranstype != RTSP_TRANSPORT_TYPE_UDP) {
      pSession->s.sessionType = RTSP_SESSION_TYPE_INTERLEAVED;
    }

  }

  if(capture_getdestFromStr(pCommon->localAddrs[0], &pSession->sd.sa, NULL, NULL, RTSP_PORT_DEFAULT) < 0) {
    return -1;
  }

  //LOG(X_DEBUG("purl:'%s', localA:'%s', addrsExt:'%s', addrsExtHost:'%s'"), purl, pCommon->localAddrs[0], pCommon->addrsExt[0], pCommon->addrsExtHost[0]);

  //
  // Initialize the retry context for connecting to the remote endpoint
  //
  memset(&retryCtxt, 0, sizeof(retryCtxt));
  retryCbCtxt.pStreamerCfg = pStreamerCfg;
  retryCbCtxt.pCommon = pCommon;
  retryCbCtxt.purl = purl;
  retryCtxt.cbConnectedAction = capture_client_rtsp_cbonsetupstream;
  retryCtxt.pConnectedActionArg = &retryCbCtxt;
  retryCtxt.pAuthCliCtxt = &pSession->authCliCtxt;
  retryCtxt.pnetsock = &pSession->sd.netsocket;
  retryCtxt.psa = (struct sockaddr *) &pSession->sd.sa;
  retryCtxt.connectDescr = "RTSP";
  retryCtxt.pconnectretrycntminone = &pCommon->connectretrycntminone;

  //
  // Connect to the remote and call capture_client_rtsp_cbonsetupstream
  // This will automatically invoke any retry logic according to the retry config
  //
  rc = connect_with_retry(&retryCtxt);

  if(rc >= 0) {
    rc = capture_rtsp_onsetupdone(pSession, pCommon);
  }

  if(rc < 0) {
    capture_rtsp_close(pSession);
  } 

  //TODO: check and close reserved ports pSession->s.vid.localports[0] [1]

  return rc;
}
int capture_rtsp_play(RTSP_CLIENT_SESSION_T *pSession) {
  int rc = 0;                         

  if(!pSession->issetup || pSession->ctxtmode != RTSP_CTXT_MODE_CLIENT_CAPTURE) {
    return -1;
  } else if(pSession->isplaying) {
    return 0;
  }

  pSession->cseq++;
  if((rc = rtsp_req_play(pSession)) < 0) {
    return rc;
  }

  if((rc = start_client_monitor(pSession)) < 0) {
    pSession->isplaying = 0;
  } else {
    LOG(X_DEBUG("Playing RTSP session %s"), pSession->s.sessionid);
    pSession->isplaying = 1;
  }

  return rc;
}

int capture_rtsp_close(RTSP_CLIENT_SESSION_T *pSession) {
  int rc = 0;

  if(!pSession) {
    return -1; 
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - Close")) );

  pthread_mutex_lock(&pSession->mtx);

  if(pSession->runMonitor >= STREAMER_STATE_RUNNING) {
    pSession->runMonitor = STREAMER_STATE_INTERRUPT;
  }

  pSession->runInterleaved = 0;

  if(pSession->s.sessionid[0] != '\0') {
    LOG(X_DEBUG("Closing RTSP session %s"), pSession->s.sessionid);
    if(pSession->ctxtmode == RTSP_CTXT_MODE_CLIENT_CAPTURE || pSession->ctxtmode == RTSP_CTXT_MODE_CLIENT_STREAM) {
      rtsp_req_teardown(pSession);
    }
    pSession->s.sessionid[0] = '\0';
  }

  pthread_mutex_unlock(&pSession->mtx);

  netio_closesocket(&pSession->sd.netsocket);
  pSession->issetup = 0;
  pSession->isplaying = 0;
  pSession->s.vid.issetup = 0;
  pSession->s.vid.isplaying = 0;
  pSession->s.aud.issetup = 0;
  pSession->s.aud.isplaying = 0;
  pSession->s.vid.ports[0] = pSession->s.vid.ports[1] = 0;
  pSession->s.aud.ports[0] = pSession->s.aud.ports[1] = 0;
  pSession->s.vid.localports[0] = pSession->s.vid.localports[1] = 0;
  pSession->s.aud.localports[0] = pSession->s.aud.localports[1] = 0;

  while(pSession->runMonitor >= STREAMER_STATE_RUNNING) {
    usleep(50000);
  }

  pthread_mutex_destroy(&pSession->mtx);

  if(pSession->ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE || pSession->ctxtmode == RTSP_CTXT_MODE_SERVER_STREAM) {
    pthread_cond_destroy(&pSession->cond.cond);
    pthread_mutex_destroy(&pSession->cond.mtx);
  }

  return rc;
}

int capture_rtsp_onpkt_interleaved(RTSP_CLIENT_SESSION_T *pSession, CAP_ASYNC_DESCR_T *pCfg,
                                   unsigned char *buf, unsigned int sz) {

  int rc = 0;
  uint8_t channelId;
  uint16_t dataLen;
  int is_rtcp = 0;
  struct timeval tv;
  struct sockaddr_storage saSrc; 
  struct sockaddr_storage saDst; 
  const CAPTURE_STREAM_T *pStream = NULL;
  RTSP_SESSION_PROG_T *pProg = NULL;
  CAPTURE_STATE_T *pState = NULL;

  if(buf[0] != RTSP_INTERLEAVED_HEADER) {
    LOG(X_ERROR("Invalid RTSP interleaved packet header byte, input-len: %d"), sz);
    return -1;
  }
  channelId = *((uint8_t *) &buf[1]);
  dataLen = htons(*((uint16_t *) &buf[2]));

  if(4 + dataLen != sz) {
    LOG(X_ERROR("Invalid RTSP interleaved packet data length: %d (input-len: %d)"), dataLen, sz);
    return -1;
  }

  //LOG(X_DEBUG("Frame length: %d (%d) channelId: %d '%c'"), dataLen, rc, channelId, buf[0]);
  //LOGHEXT_DEBUG(buf, MIN(dataLen, 32));
  //LOG(X_DEBUG("vid ports: %d, %d, localports:%d,%d"), pSession->s.vid.ports[0], pSession->s.vid.ports[1], pSession->s.vid.localports[0], pSession->s.vid.localports[1]);
  //LOG(X_DEBUG("aud  ports: %d, %d, localports:%d,%d"), pSession->s.aud.ports[0], pSession->s.aud.ports[1], pSession->s.aud.localports[0], pSession->s.aud.localports[1]);

  is_rtcp = (channelId & 0x01) ? 1 : 0;
  //TODO: match channelId to trackId
  //if(channelId == 0 && pSession->sdp.vid.common.available) {
  if(channelId == pSession->s.vid.ports[0] || channelId == pSession->s.vid.ports[1]) {
    pProg = &pSession->s.vid;
  } else {
    pProg = &pSession->s.aud;
  }

  memcpy(&saSrc, &pSession->sd.sa, sizeof(saSrc));
  INET_PORT(saSrc) = htons(pProg->ports[is_rtcp]);
  memcpy(&saDst, &pSession->sd.sa, sizeof(saDst));
  INET_PORT(saDst) = htons(pProg->localports[is_rtcp]);

  if(htons(INET_PORT(saDst)) < RTP_MIN_PORT) {
     INET_PORT(saDst) = htons(RTP_MIN_PORT);
  }

  if(htons(INET_PORT(saSrc)) < RTP_MIN_PORT) {
    INET_PORT(saSrc) = INET_PORT(saDst);
  }

  gettimeofday(&tv, NULL);

  VSX_DEBUG_RTSP( LOG(X_DEBUGV("RTSP - capture_rtsp_onpkt_interleaved ctxtmode:%d, channelId: %d, is_rtcp:%d, :%d -> :%d, len:%d, pProg; 0x%x, vid: 0x%x, aud: 0x%x"), pCfg->pcommon->rtsp.ctxtmode, channelId, is_rtcp, htons(INET_PORT(saSrc)), 
         htons(INET_PORT(saDst)), dataLen, pProg, &pSession->s.vid, &pSession->s.aud) );

  if(pCfg->pcommon->rtsp.ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE) {
    pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }
  
  if(!pCfg->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt || 
     !(pState = pCfg->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt->pState)) {
    rc = -1;
  }

  if(rc >= 0) {
    if(is_rtcp) {
      if(!(pStream  = capture_process_rtcp(pCfg->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt, 
                                           (const struct sockaddr *) &saSrc, (const struct sockaddr *) &saDst, 
                                           (RTCP_PKT_HDR_T *) &buf[4], dataLen))) {
        LOG(X_WARNING("RTSP interleaved RTCP packet on channel %d, len:%d  not processed"), channelId, dataLen);
      }
    } else {
      if(!(pStream = capture_onUdpSockPkt(pState, &buf[4], dataLen, 0, (const struct sockaddr *) &saSrc, 
                                          (const struct sockaddr *) &saDst, &tv, NULL))) {
        LOG(X_WARNING("RTSP interleaved RTP packet on channel %d, len:%d not processed"), channelId, dataLen);
      }
    }

  } else {
    LOG(X_ERROR("RTSP interleaved packet RTP Capture context does not exist!"));
  }

  if(pCfg->pcommon->rtsp.ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE) {
    pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  return rc;
}

int rtsp_run_interleavedclient(RTSP_CLIENT_SESSION_T *pSession, CAP_ASYNC_DESCR_T *pCapCfg, 
                               RTSP_REQ_CTXT_T *pReqCtxt) {
  int rc = 0;
  unsigned char buf[RTP_JTBUF_PKT_BUFSZ_LOCAL];
  unsigned int tmtms = 15000;
  //CAP_HTTP_COMMON_T capHttp;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T rtspResp;
  int *prunning = NULL;

  if(!pSession || (!pCapCfg && !pReqCtxt)) {
    return -1;
  }

  memset(&rtspResp, 0, sizeof(rtspResp));
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSession->sd.netsocket;
  hdrCtxt.pbuf = (char *) buf;
  hdrCtxt.szbuf = sizeof(buf);

  //memset(&capHttp, 0, sizeof(capHttp));
  //capHttp.pCfg = pCfg;

  pSession->runInterleaved = 1;
  if(pCapCfg) {
    prunning = &pCapCfg->running;
  }

  while(pSession->runInterleaved && (!prunning || *prunning == 0) && !g_proc_exit) {

    //LOG(X_DEBUG("IDXBUF ... = 0 ... idxbuf:%d"), hdrCtxt.idxbuf);
    hdrCtxt.idxbuf = 0;
    buf[0] = '\0';

    if((rc = rtsp_read_interleaved(&pSession->sd.netsocket, &hdrCtxt, buf, sizeof(buf), tmtms)) < 0) {
      LOG(X_ERROR("RTSP Failed to read interleaved header"));
      rc = -1;
      break;
    } else if(rc == 0) {

      if(rtsp_options_rcv(pSession, &hdrCtxt) < CONNECT_RETRY_RC_OK) {
        rc = -1;
        break;
      }

    } else if(pCapCfg) {

      //
      // Capture server interleaved mode
      //
      if((rc = capture_rtsp_onpkt_interleaved(pSession, pCapCfg, buf, rc)) < 0) {
      }

    } else if(pReqCtxt) {

      //
      // Stream announce interleaved mode
      //
      if((rc = rtsp_handle_interleaved_msg(pReqCtxt, buf, rc)) < 0) {
      }
    }

  }

  return rc;
}

static void cap_rtsp_srv_proc(void *pfuncarg) {
  CLIENT_CONN_T *pConn = (CLIENT_CONN_T *) pfuncarg;
  CAP_ASYNC_DESCR_T *pCfg = (CAP_ASYNC_DESCR_T *) pConn->pUserData;
  RTSP_REQ_CTXT_T rtspCtxt;
  char tmp[128];

  LOG(X_INFO("Starting RTSP capture connection from %s:%d"),
         FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

  memset(&rtspCtxt, 0, sizeof(rtspCtxt));
  rtspCtxt.pStreamerCfg = pCfg->pStreamerCfg;
  rtspCtxt.pSd = &pConn->sd;
  rtspCtxt.pListenCfg = pConn->pListenCfg;
  rtspCtxt.pCap = pCfg;
  rtspCtxt.pClientSession = &pCfg->pcommon->rtsp;
  rtspCtxt.pSession = &rtspCtxt.pClientSession->s;

  if(rtspCtxt.pStreamerCfg && rtspCtxt.pStreamerCfg->pRtspSessions) {
    rtsp_handle_conn(&rtspCtxt);
  }

  netio_closesocket(&pConn->sd.netsocket);

  LOG(X_DEBUG("RTSP capture connection ended %s:%d"), FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)),
                                                      ntohs(INET_PORT(pConn->sd.sa)));
}

static int capture_rtsp_server(CAP_ASYNC_DESCR_T *pCfg) {
  int rc = 0;
  POOL_T pool;
  SRV_LISTENER_CFG_T listenCfg;
  //char buf[SAFE_INET_NTOA_LEN_MAX];
  CLIENT_CONN_T *pConn;
  NETIO_SOCK_T *pnetsock = NULL;
  const struct sockaddr *psaSrv = NULL;
  char tmp[128];
  char bufses[32];

  if(!pCfg || !pCfg->pcommon) {
    return -1;
  }

  //
  // We're acting as an RTSP SERVER waiting for an ANNOUNCE command to ingest media
  //
  pCfg->pcommon->rtsp.ctxtmode = RTSP_CTXT_MODE_SERVER_CAPTURE;
  pCfg->pcommon->rtsp.runMonitor =  STREAMER_STATE_FINISHED;

  pnetsock = &pCfg->pcommon->rtsp.sd.netsocket;
  psaSrv = (const struct sockaddr *) &pCfg->pcommon->rtsp.sd.sa;

  memset(&pool, 0, sizeof(pool));
  if(pool_open(&pool, 2, sizeof(CLIENT_CONN_T), 1) != 0) {
    return -1;
  }

  pConn = (CLIENT_CONN_T *) pool.pElements;
  while(pConn) {
    pConn->pUserData = pCfg;
    pConn = (CLIENT_CONN_T *) pConn->pool.pNext;
  }

  if((PNETIOSOCK_FD(pnetsock) = net_listen(psaSrv, 2)) == INVALID_SOCKET) {
    pool_close(&pool, 0);
    return -1;
  }

  if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pCfg->pcommon->filt.filters[0].transType)) {
    pnetsock->flags |= NETIO_FLAG_SSL_TLS;
  }

  if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->pRtspSessions->psessionTmtSec &&
     *pCfg->pStreamerCfg->pRtspSessions->psessionTmtSec != 0) {
    snprintf(bufses, sizeof(bufses), ", timeout:%u sec", *pCfg->pStreamerCfg->pRtspSessions->psessionTmtSec);
  } else {
    bufses[0] = '\0';
  }

  memset(&listenCfg, 0, sizeof(listenCfg));
  memcpy(&listenCfg.sa, psaSrv, INET_SIZE(*psaSrv));
  listenCfg.pnetsockSrv = pnetsock;
  listenCfg.pConnPool = &pool;
  listenCfg.urlCapabilities = URL_CAP_RTSPLIVE;
  listenCfg.pAuthStore = &pCfg->pcommon->creds[0];

  LOG(X_INFO("RTSP %scapture listener available at "URL_RTSP_FMT_STR"%s%s"),
           ((pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : ""),
            URL_RTSP_FMT2_ARGS((pnetsock->flags & NETIO_FLAG_SSL_TLS), 
           FORMAT_NETADDR(*psaSrv, tmp, sizeof(tmp))), ntohs(PINET_PORT(psaSrv)), 
           (IS_AUTH_CREDENTIALS_SET(listenCfg.pAuthStore) ? " (Using auth)" : ""), bufses);

  //
  // Service any client connections on the live listening port
  //
  rc = srvlisten_loop(&listenCfg, cap_rtsp_srv_proc);

  netio_closesocket(pnetsock);
  pool_close(&pool, 1);

  return rc;
}

static void rtsp_capture_server_proc(void *pArg) {
  CAP_ASYNC_DESCR_T *pCfg = (CAP_ASYNC_DESCR_T *) pArg;

  LOG(X_DEBUG("RTSP capture server thread started"));

  capture_rtsp_server(pCfg);

  vsxlib_cond_signal(&pCfg->pcommon->rtsp.cond.cond, &pCfg->pcommon->rtsp.cond.mtx);

  LOG(X_DEBUG("RTSP capture server thread exiting"));
}

int capture_rtsp_server_start(CAP_ASYNC_DESCR_T *pCfg, int wait_for_setup) {

  int rc = 0;
  pthread_t ptdMonitor;
  pthread_attr_t attrInterleaved;

  if(!pCfg || !pCfg->pcommon || !pCfg->pStreamerCfg || !pCfg->pSockList) {
    return -1;
  }

  pthread_attr_init(&attrInterleaved);
  pthread_attr_setdetachstate(&attrInterleaved, PTHREAD_CREATE_DETACHED);

  pthread_mutex_init(&pCfg->pcommon->rtsp.cond.mtx, NULL);
  pthread_cond_init(&pCfg->pcommon->rtsp.cond.cond, NULL);

  if(pthread_create(&ptdMonitor,
                    &attrInterleaved,
                    (void *) rtsp_capture_server_proc,
                    (void *) pCfg) != 0) {
    LOG(X_ERROR("Unable to create RTSP capture server thread"));
    //pthread_mutex_destroy(&pSession->mtx);
   
    pthread_cond_destroy(&pCfg->pcommon->rtsp.cond.cond);
    pthread_mutex_destroy(&pCfg->pcommon->rtsp.cond.mtx);
    return -1;
  }

  if(wait_for_setup) {
    if((rc = vsxlib_cond_waitwhile(&pCfg->pcommon->rtsp.cond, NULL, 0)) >= 0) {
      if(!pCfg->pcommon->rtsp.issetup) {
        rc = -1;
      }
    }
    LOG(X_DEBUG("RTSP capture server done waiting for start completion issetup:%d"), pCfg->pcommon->rtsp.issetup);
  }

  return rc;

}

#endif // VSX_HAVE_CAPTURE
