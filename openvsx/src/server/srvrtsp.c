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


/*
void testredirect(const RTSP_REQ_CTXT_T *pRtsp) {
  int rc;
  char buf[1024];

  if((rc = snprintf(buf, sizeof(buf),
                    "REDIRECT rtsp://127.0.0.1:1554/test %s\r\n"
                    "Cseq: %d\r\n"
                     "Location: rtsp://127.0.0.1:1556\r\n\r\n",
                     RTSP_VERSION_DEFAULT, 10)) < 0) {
  }

  if((rc = netio_send(&pRtsp->pSd->netsocket,  &pRtsp->pSd->sain, (unsigned char *) buf, rc)) < 0) {

  }

  fprintf(stderr, "RC:%d '%s'\n", rc, buf);

}
*/

static void rtsp_req_gethdrs(RTSP_REQ_T *pReq) {
  const char *parg;

  if((parg = conf_find_keyval(pReq->hr.reqPairs, RTSP_HDR_CSEQ))) {
    pReq->cseq = atoi(parg);
  } else {
    pReq->cseq = -1;
  }

  if((parg = conf_find_keyval(pReq->hr.reqPairs, RTSP_HDR_SESSION))) {
    strncpy(pReq->sessionId, parg, sizeof(pReq->sessionId) - 1);
  } else {
    pReq->sessionId[0] = '\0';
  }

}

static int rtsp_req_parsebuf(const RTSP_REQ_CTXT_T *pRtsp, RTSP_REQ_T *pReq, const char *buf, 
                             unsigned int len) {
  int rc;

  if((rc = http_req_parsebuf(&pReq->hr, buf, len, 0)) < 0) {
    return rc;
  }

  rtsp_req_gethdrs(pReq);

  return rc;
}

static int rtsp_req_get(const RTSP_REQ_CTXT_T *pRtsp, RTSP_REQ_T *pReq) {
  int rc = 0;
  RTSP_HTTP_SESSION_T *pHttpSession;
  struct timespec ts;
  struct timeval tv;
  unsigned int requestSz = 0;
  char requestStr[RTSP_HTTP_POST_BUFFER_SZ];

  //fprintf(stderr, "rtsp_req_get -- \n");

  //
  // Since the GET connection created the RTSP HTTP session, it is ok to hold on to it as
  // it should not be deleted by the POST thread
  if(!(pHttpSession = pReq->pHttpSession)) {
    return -1;
  }


  do {

    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec + 2;
    ts.tv_nsec = tv.tv_usec * 1000;
    if((rc = pthread_cond_timedwait(&pHttpSession->cond.cond, &pHttpSession->cond.mtx, &ts)) < 0) {
      LOG(X_ERROR("RTSP HTTP GET message conditional wait failed" ERRNO_FMT_STR), ERRNO_FMT_ARGS);
      break;
    } 

    //fprintf(stderr, "rtsp_req_get got cond, requestSz:%d, rc:%d\n", pHttpSession->requestSz, rc);

    if(pRtsp->pSession && (pRtsp->pSession->flag & RTSP_SESSION_FLAG_EXPIRED)) {
      rc = -1;
      break;
    } else if(net_issockremotelyclosed(NETIOSOCK_FD(pRtsp->pSd->netsocket), 1)) { 
          LOG(X_DEBUG("RTSP HTTP GET connection to %s:%d remotely closed"),
               inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));
      rc = -1;
      break;
    } else if(pHttpSession->requestSz > 0) {
      break;
    } else {

      gettimeofday(&tv, NULL);

      if(pHttpSession->tvLastMsg.tv_sec == 0) {
        //
        // No message processed by POST thread
        //
        if(tv.tv_sec - pHttpSession->tvCreate.tv_sec > 10) {
          LOG(X_WARNING("RTSP HTTP GET from %s:%d exiting due to POST inactivity"),
               inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));
          rc = -1;
          break;
        }
        
      } else if(!pRtsp->pSession && tv.tv_sec - pHttpSession->tvLastMsg.tv_sec > 10) {
        LOG(X_DEBUG("RTSP HTTP GET connection to %s:%d no longer active"),
               inet_ntoa(pRtsp->pSd->sain.sin_addr), ntohs(pRtsp->pSd->sain.sin_port));
        rc = -1;
        break;
      }

      usleep(20000);
    }

  } while(!g_proc_exit);
  
  if(rc >= 0 && !g_proc_exit) {

    requestSz = pHttpSession->requestSz;
    memcpy(requestStr, pHttpSession->pRequestBufWr, requestSz + 1); 

    //fprintf(stderr, "rtsp_req_get message size:%d ->\n", requestSz); avc_dumpHex(stderr, requestStr, requestSz + 1, 1);

    pHttpSession->requestSz = 0;

    //pthread_mutex_unlock(&pHttpSession->mtx);
  }


  if(requestSz > 0) {
    if((rc = rtsp_req_parsebuf(pRtsp, pReq, requestStr, requestSz)) >= 0) {
      rtsp_req_gethdrs(pReq);
    }
  }

  return rc;
}

static int rtsp_req_readparse(const RTSP_REQ_CTXT_T *pRtsp, RTSP_REQ_T *pReq, 
                              unsigned char *pbuf, unsigned int szbuf) {
  int rc = 0;
  unsigned int readTmtSec = 0;
  unsigned int idxbuf;

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - request parse start... idx:%d/%d '%c'"), pReq->hdrCtxt.idxbuf, szbuf, pbuf[0] ));

  if(pRtsp->pSession && pRtsp->pSession->expireSec > 0) {
    readTmtSec = pRtsp->pSession->expireSec + 3;
  }
  if(readTmtSec == 0) {
    readTmtSec = RTSP_SESSION_IDLE_SEC_SPEC + 3;
  }

  while(!g_proc_exit) {

    if(pRtsp->pSession && (pRtsp->pSession->flag & RTSP_SESSION_FLAG_EXPIRED)) {
      return -1;
    }

    idxbuf = pReq->hdrCtxt.idxbuf;
    memset(&pReq->hdrCtxt, 0, sizeof(pReq->hdrCtxt));
    pReq->hdrCtxt.pnetsock = &pRtsp->pSd->netsocket;
    //pReq->hdrCtxt.pbuf = pReq->buf;
    //pReq->hdrCtxt.szbuf = sizeof(pReq->buf);
    pReq->hdrCtxt.pbuf = (char *) pbuf;
    pReq->hdrCtxt.szbuf = szbuf;
    pReq->hdrCtxt.tmtms = readTmtSec * 1000;
    pReq->hdrCtxt.idxbuf = idxbuf;
    //if(idxbuf == 0) {
      //((unsigned char *)pReq->hdrCtxt.pbuf)[0] = '\0';
    //}

    if((rc = http_req_readpostparse(&pReq->hdrCtxt, &pReq->hr, 0)) < 0) {
      return rc;
    } else if(rc == 0) {

      //
      // Request was not parsed because no data was read (client already disconnected or idle tmt)
      //

      //testredirect(pRtsp);

      if(pRtsp->pSession) {
        if(pReq->hdrCtxt.rcvclosed) {
           break;
        } else if(readTmtSec > 0) {
          //gettimeofday(&tv, NULL);
          //if(tv.tv_sec < pRtsp->pSession->tvlastupdate.tv_sec + pRtsp->pSession->expireSec) {
            //fprintf(stderr, "Trying read again\n");
            usleep(50000);
            continue;
          //}
        }
      }

      return -1;
    } else {
      break;
    }
  }

  rtsp_req_gethdrs(pReq);

  return rc;
}

int rtsp_read_interleaved(NETIO_SOCK_T *pnetsock, HTTP_PARSE_CTXT_T *pHdrCtxt, 
                          unsigned char *buf, unsigned int szbuf, unsigned int tmtms) {
  int rc = 0;
  uint16_t len;

  if(!pnetsock || !buf || szbuf < 4) {
    return -1;
  }

  //int rd=fcntl(PNETIOSOCK_FD(pnetsock), F_GETFL, 0); fprintf(stderr, "SOCK IS %s\n", (rd&O_NONBLOCK) ?"NON_BLOCKING" : "BLOCKING"); 

  pHdrCtxt->idxbuf = 0;

  //if(0&&(pnetsock->flags & NETIO_FLAG_SSL_TLS)) {
  //  if((rc = netio_recv(pnetsock, NULL, buf, 1)) == 1) {
  //    pHdrCtxt->idxbuf = rc;
  //  }
  //  LOG(X_DEBUG("NETIO_RECV rc:%d '%c'.... "), rc, buf[0]);
  //} else {
    rc = netio_peek(pnetsock, buf, 1);
    //rc = recv(PNETIOSOCK_FD(pnetsock), buf, 1, MSG_PEEK);
  //  LOG(X_DEBUG("NETIO_PEEK rc:%d '%c'.... "), rc, buf[0]);
  //}

  if(rc < 0) {
    return rc;
  } else if(rc == 0 || (rc > 0 && buf[0] != RTSP_INTERLEAVED_HEADER)) {
    return 0;
  }

  //
  // 1 byte '$' header
  // 1 byte channel id
  // 2 byte length
  //
  if(tmtms > 0) {
    if((rc = netio_recvnb_exact(pnetsock, &buf[pHdrCtxt->idxbuf], 4 - pHdrCtxt->idxbuf, tmtms)) != 
        4 - pHdrCtxt->idxbuf) {
      LOG(X_ERROR("RTSP Failed to read interleaved header rc:%d, tmt:%d"), rc, tmtms);
      return rc;
    }   
  } else if((rc = netio_recv_exact(pnetsock, NULL, &buf[pHdrCtxt->idxbuf], 4 - pHdrCtxt->idxbuf)) < 0) {
    LOG(X_ERROR("RTSP Failed to read interleaved header rc:%d"), rc);
    return rc;
  }

  if(4 + (len = htons( *((uint16_t *) &buf[2]) )) > szbuf) {
    LOG(X_ERROR("RTSP receive buffer size %d too small for interleaved packet length %d"), szbuf, 4 + len);
    return -1;
  }
  
  if(tmtms > 0) {
    if((rc = netio_recvnb_exact(pnetsock, &buf[4], len, tmtms)) != len) {
      LOG(X_ERROR("RTSP Failed to read interleaved frame length %d, rc:%d, tmt:%d"), len, rc, tmtms);
      return rc;
    }
  } else {
    if((rc = netio_recv_exact(pnetsock, NULL, &buf[4], len)) != len) {
        LOG(X_ERROR("RTSP Failed to read interleaved frame length %d, rc:%d"), len, rc);
      return rc;
    }
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUGV( "RTSP - got interleaved packet len: %d"), rc) );

  return 4 + rc;

}

int rtsp_handle_interleaved_msg(RTSP_REQ_CTXT_T *pRtsp, unsigned char *buf, unsigned int len) {
  int rc = 0;
  unsigned char channelId;
  STREAM_RTP_DEST_T *pDest = NULL;

  if(len < 12) {
    return -1;
  } else if(!pRtsp || !buf || !pRtsp->pSession || 
            !(pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
    return -1;
  } else if(pRtsp->pClientSession) {
    return capture_rtsp_onpkt_interleaved(pRtsp->pClientSession, pRtsp->pCap, buf, len);
  }

  channelId = buf[1];

  if(channelId == pRtsp->pSession->vid.ports[1]) {
    pDest = pRtsp->pSession->vid.pDest;
  } else if(channelId == pRtsp->pSession->aud.ports[1]) {
    pDest = pRtsp->pSession->aud.pDest;
  } else {
    LOG(X_ERROR("RTSP (interleaved) message received on invalid RTCP channel: %d, len: %d"), channelId, len);
    return -1;
  }

  if(!pDest) {
    return -1;
  }

  //fprintf(stderr, "READ INTERLEAVED PKT LEN:%d pQ[%d]\n", len, (pRtsp->pSession && pRtsp->pSession->pLiveQ2 ? pRtsp->pSession->pLiveQ2->pQs[pRtsp->pSession->liveQIdx]->idxWr : 0));
  //fprintf(stderr, "vid:%d,%d, aud:%d,%d\n", pRtsp->pSession->vid.ports[0], pRtsp->pSession->vid.ports[1],pRtsp->pSession->aud.ports[0],pRtsp->pSession->aud.ports[1]); avc_dumpHex(stderr, buf, len, 1);

  rc = stream_rtp_handlertcp(pDest, (const RTCP_PKT_HDR_T *) &buf[4], len - 4, NULL, NULL);

  return rc;
}

static int notify_rtsphttp_postdata(RTSP_REQ_CTXT_T *pRtsp, RTSP_HTTP_SESSION_T *pHttpSession) {
  int rc = 0;
  char *p;
  unsigned int sz;
  unsigned int requestSz;
  char buf[RTSP_HTTP_POST_BUFFER_SZ];

  if(pHttpSession->requestIdxWr <= 0) {
    return rc;
  }

  if((p = strstr(pHttpSession->pRequestBufWr, "\r\n\r\n"))) {
    p += 4;
  } else {
    return rc; 
  }

  requestSz = (ptrdiff_t) (p - pHttpSession->pRequestBufWr);
  pHttpSession->requestSz = requestSz;

  memcpy(pHttpSession->pRequestBufRd, pHttpSession->pRequestBufWr, requestSz);
  pHttpSession->pRequestBufRd[rc] = '\0';

  //avc_dumpHex(stderr, pHttpSession->pRequestBufWr, strlen(pHttpSession->pRequestBufWr) + 1, 1);
  //fprintf(stderr, "POST calling pthread_cond_broadcast, requestSz:%d, requestIdxWr:%d\n", pHttpSession->requestSz, pHttpSession->requestIdxWr);

  while(pHttpSession->requestSz > 0) {

    //
    // Notify the GET thread of a pending message
    //
    //pthread_cond_broadcast(&pHttpSession->cond.cond);
    pthread_cond_signal(&pHttpSession->cond.cond);

    //fprintf(stderr, "POST notify checking requestSz\n"); 
    usleep(50000);
    if(pHttpSession->expired) {
      //fprintf(stderr, "POST session expired\n");
      return -1;
    }
  }

  //fprintf(stderr, "POST notify done checking requestSz\n"); 

  //
  // For any starting portion of the next message, copy the contents into the
  // start of the write buffer
  //
  if(pHttpSession->requestIdxWr > requestSz) {

    sz = pHttpSession->requestIdxWr - requestSz;

    memcpy(buf, &pHttpSession->pRequestBufWr[sz], sz + 1);
    memcpy(pHttpSession->pRequestBufWr, buf, sz + 1);

    pHttpSession->requestIdxWr = sz;

  } else {

    pHttpSession->requestIdxWr = 0;

  }

  return rc;
}

static int handle_rtsphttp_postdata(RTSP_REQ_CTXT_T *pRtsp, RTSP_REQ_T *pRtspReq) { 
                                 
  int rc;
  unsigned int szmax;
  size_t sz;
  RTSP_HTTP_SESSION_T *pHttpSession;  
  char buf[RTSP_HTTP_POST_BUFFER_SZ * 4 / 3];

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - POST connection start")) );

  do {

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - POST loop will call recv hdrs:%d/%d"), pRtspReq->hdrCtxt.hdrslen, pRtspReq->hdrCtxt.idxbuf) );
//TODO: we have data here in the buf...netio_recv not getting it 
    if(pRtspReq->hdrCtxt.idxbuf > pRtspReq->hdrCtxt.hdrslen) {

      if((rc = pRtspReq->hdrCtxt.idxbuf - pRtspReq->hdrCtxt.hdrslen) > sizeof(buf) - 1) {
        LOG(X_ERROR("RTSP HTTP POST buffer size %d too small for %d"), sizeof(buf), rc);
        rc = -1;
      } else {
        memcpy(buf, &pRtspReq->hdrCtxt.pbuf[pRtspReq->hdrCtxt.hdrslen], rc);
        //fprintf(stderr, "POST copied from prior recv %d %c%c%c\n", rc, buf[0], buf[1], buf[2]);
      }
      pRtspReq->hdrCtxt.idxbuf = pRtspReq->hdrCtxt.hdrslen = 0;

    } else {

      rc = netio_recv(&pRtsp->pSd->netsocket, NULL, (unsigned char *) buf, sizeof(buf) - 1);
      //fprintf(stderr, "POST rcv got:%d %c%c%c\n", rc, buf[0], buf[1], buf[2]);
    }

    if(rc <= 0) {
      VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - POST loop ending rc: %d"), rc) );
      return rc;
    }

    if(!(pHttpSession = rtspsrv_findHttpSession(pRtsp->pStreamerCfg->pRtspSessions,
                                pRtspReq->httpSessionCookie, &pRtsp->pSd->sain, 1))) {

      LOG(X_ERROR("RTSP HTTP POST from %s:%d unable to find GET prior session "
                  RTSP_HDR_SESSIONCOOKIE":%s"), 
                  net_inet_ntoa(pRtsp->pSd->sain.sin_addr, buf), ntohs(pRtsp->pSd->sain.sin_port),
                  pRtspReq->httpSessionCookie);

      snprintf(buf, sizeof(buf), "Invalid "RTSP_HDR_SESSIONCOOKIE" %s", pRtspReq->httpSessionCookie);
      http_resp_error(pRtsp->pSd, &pRtspReq->hr, HTTP_STATUS_SERVERERROR, 1, buf, NULL);
      return -1;
    }

    // Update the last activity time of the session
    gettimeofday(&pHttpSession->tvLastMsg, NULL);

    buf[MIN(rc, sizeof(buf) - 1)] = '\0';
    szmax = RTSP_HTTP_POST_BUFFER_SZ - pHttpSession->requestIdxWr;
    sz = base64_decode(buf, 
                       (unsigned char *) &pHttpSession->pRequestBufWr[pHttpSession->requestIdxWr], szmax); 
    //fprintf(stderr, "HTTP RTSP POST read base64:%d, sz:%d\n", rc, sz);avc_dumpHex(stderr, &pHttpSession->pRequestBufWr[pHttpSession->requestIdxWr], sz > 0 ? sz + 1: 0, 1);
    if(sz < 0) {
      LOG(X_ERROR("Unable to copy RTSP POST request size %d into [%d]/%d"), szmax, 
          pHttpSession->requestIdxWr, RTSP_HTTP_POST_BUFFER_SZ); 
      pthread_mutex_unlock(&pHttpSession->mtx);
      return -1;
    }

    pHttpSession->requestIdxWr += sz;

    rc = notify_rtsphttp_postdata(pRtsp, pHttpSession);

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - POST after notify rc: %d"), rc) );

    pthread_mutex_unlock(&pHttpSession->mtx);
 
  } while(!g_proc_exit && rc >= 0);

  //fprintf(stderr, "POST THREAD LOOP EXITING rc:%d\n", rc);
  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - POST out of loop rc: %d"), rc) );

  return -1;
}

static int handle_rtsphttp_req(RTSP_REQ_CTXT_T *pRtsp, RTSP_REQ_T *pRtspReq) { 
  int rc = 0;
  RTSP_HTTP_SESSION_T *pHttpSession = NULL;
  const char *sessionCookie, *p;
  char buf[SAFE_INET_NTOA_LEN_MAX];
  HTTP_REQ_T *pHttpReq = &pRtspReq->hr;

  if(!pRtsp->pSd) {
    return -1;
  }

  if(!(sessionCookie = conf_find_keyval(pHttpReq->reqPairs, RTSP_HDR_SESSIONCOOKIE)) || 
     sessionCookie[0] == '\0') {
    LOG(X_ERROR("RTSP "RTSP_HDR_SESSIONCOOKIE" not present in %s request"), pHttpReq->method);
    return -1;
  }

//fprintf(stderr, "SESSION_COOKIE:'%s' method:'%s'\n", sessionCookie, pHttpReq->method);

  if(!strncmp(pHttpReq->method, HTTP_METHOD_GET, 4)) {

    pHttpSession = rtspsrv_findHttpSession(pRtsp->pStreamerCfg->pRtspSessions, 
                              sessionCookie, &pRtsp->pSd->sain, 0);

    if(pHttpSession) {

      LOG(X_ERROR("RTSP HTTP GET "RTSP_HDR_SESSIONCOOKIE":%s still active"), sessionCookie);
      http_resp_error(pRtsp->pSd, pHttpReq, HTTP_STATUS_SERVERERROR, 1, "RTSP Session still active", NULL);
      rc = -1;

    } else if((!(pHttpSession = rtspsrv_newHttpSession(pRtsp->pStreamerCfg->pRtspSessions, 
                              sessionCookie, &pRtsp->pSd->sain)))) {

      LOG(X_ERROR("RTSP HTTP GET unable to allocate new session"));
      http_resp_error(pRtsp->pSd, pHttpReq, HTTP_STATUS_SERVERERROR, 1, "No HTTP session available", NULL);
      rc = -1;

    } else {

      strncpy(pRtspReq->httpSessionCookie, sessionCookie, sizeof(pRtspReq->httpSessionCookie) - 1);
      pRtspReq->pHttpSession = pHttpSession;

      rc = http_resp_sendhdr(pRtsp->pSd, pHttpReq->version, HTTP_STATUS_OK, 0,
                           CONTENT_TYPE_RTSPTUNNELED, 
                           http_getConnTypeStr(HTTP_CONN_TYPE_CLOSE),
                           NULL, NULL, NULL, NULL, NULL);
    }

  } else if(!strncmp(pHttpReq->method, HTTP_METHOD_POST, 5)) {


    if(!(p = conf_find_keyval(pHttpReq->reqPairs, HTTP_HDR_CONTENT_TYPE)) ||
       strncasecmp(p, CONTENT_TYPE_RTSPTUNNELED, strlen(CONTENT_TYPE_RTSPTUNNELED))) {

      LOG(X_ERROR("RTSP HTTP POST has invalid "HTTP_HDR_CONTENT_TYPE": %s"), p ? p : "");
      http_resp_error(pRtsp->pSd, pHttpReq, HTTP_STATUS_SERVERERROR, 1, "Invalid "HTTP_HDR_CONTENT_TYPE, NULL);
      rc = -1;

    } 

    if(!(pHttpSession = rtspsrv_findHttpSession(pRtsp->pStreamerCfg->pRtspSessions, 
                              sessionCookie, &pRtsp->pSd->sain, 1))) {

      LOG(X_ERROR("RTSP HTTP POST from %s:%d unable to find GET session "
                  RTSP_HDR_SESSIONCOOKIE":%s"), 
                  net_inet_ntoa(pRtsp->pSd->sain.sin_addr, buf), ntohs(pRtsp->pSd->sain.sin_port),
                  sessionCookie);
      http_resp_error(pRtsp->pSd, pHttpReq, HTTP_STATUS_SERVERERROR, 1, "Invalid "RTSP_HDR_SESSIONCOOKIE, NULL);
      rc = -1;

    } else {

      strncpy(pRtspReq->httpSessionCookie, sessionCookie, sizeof(pRtspReq->httpSessionCookie) - 1);
      gettimeofday(&pHttpSession->tvLastMsg, NULL);
      NETIO_SET(pHttpSession->netsocketPost, pRtsp->pSd->netsocket);

      pthread_mutex_unlock(&pHttpSession->mtx);
    }


  } else {
    LOG(X_ERROR("RTSP HTTP invalid request method"));
    http_resp_error(pRtsp->pSd, pHttpReq, HTTP_STATUS_BADREQUEST, 1, "Invalid request", NULL);
    rc = -1;
  }

  if(rc >= 0) {

  }

  return rc;
}

int rtsp_handle_conn(RTSP_REQ_CTXT_T *pRtsp) {
  int rc = 0;
  int ishttp = -1;
  int doread = 1;
  char netbuf[128];
  unsigned char buf[4096];
  RTSP_REQ_T req, reqHttp;
  RTSP_REQ_T *pReq = &req;

  if(!pRtsp) {
    return -1;
  } else if(!pRtsp->pStreamerCfg) {
    LOG(X_ERROR("No valid streamer configuration for rtsp request"));
    return -1;
  }

  memset(&req, 0, sizeof(req));
  memset(&reqHttp, 0, sizeof(reqHttp));
  pRtsp->bs.buf = buf;
  pRtsp->bs.sz = sizeof(buf);
  pthread_mutex_init(&pRtsp->mtx, NULL);

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - New control connection from remote %s:%d"), 
                       net_inet_ntoa(pRtsp->pSd->sain.sin_addr, netbuf), ntohs(pRtsp->pSd->sain.sin_port)) );

  while(!g_proc_exit) {

    //
    // Check for non-control, interleaved data packet with '$' header
    //
    if(ishttp != 1 && pRtsp->pSession && (pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {
      while((rc = rtsp_read_interleaved(&pRtsp->pSd->netsocket, &pReq->hdrCtxt, buf, sizeof(buf), 0)) > 0) {
        rtsp_handle_interleaved_msg(pRtsp, buf, rc);
      }

      if(rc < 0) {
        LOG(X_ERROR("Failed to read RTSP interleaved packet"));
        break;
      }
    }

    doread = 1;

    if(g_proc_exit) {
      break;
    } else if(!strncmp(req.hr.method, HTTP_METHOD_POST, 5)) {

      ishttp = 1;
      doread = 0;

      //
      // RTSP over HTTP incoming request messages come as POST data
      // encoded as base64.  Should only return on end of the POST HTTP 
      // connection, as all POST RTSP request commands are sent as async messages to the GET
      // connection thread. 
      //

      //fprintf(stderr, "RTSP HTTP POST thread - calling handle_rtsphttp_postdata\n");
      rc = handle_rtsphttp_postdata(pRtsp, &req);
      break;

    } else if(!strncmp(req.hr.method, HTTP_METHOD_GET, 4)) {

      ishttp = 1;
      doread = 0;

      //
      // RTSP GET thread commands are not read from the HTTP socket but from 
      // the POST thread message queue
      //

      pReq = &reqHttp;
      pReq->pHttpSession = req.pHttpSession;
      if((rc = rtsp_req_get(pRtsp, pReq)) < 0) {
        break;
      }

    } 

    if(doread) {

      //LOG(X_DEBUG("IDXBUF = 0... idxbuf:%d, ishttp:%d"), pReq->hdrCtxt.idxbuf, ishttp);

      if(ishttp != 1) {
        pReq->hdrCtxt.idxbuf = 0;
        buf[0] = '\0';
      }

      if((rc = rtsp_req_readparse(pRtsp, pReq, buf, sizeof(buf))) < 0) {
        break;
      }
    } 

    if(rc == 0 && pReq->hdrCtxt.rcvclosed) {
      // Connection has been closed
      LOG(X_DEBUG("RTSP - connection has been closed"));
      rc = -1;
      break;
    }

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - Request parse rc: %d, rcvclosed: %d, idxbuf:%d, hdrslen:%d"), rc, pReq->hdrCtxt.rcvclosed, pReq->hdrCtxt.idxbuf, pReq->hdrCtxt.hdrslen));

    if(g_proc_exit) {
      break;
    }

    if(pReq->sessionId[0] != '\0') {
      if(pRtsp->pSession || 
         (pRtsp->pSession = rtspsrv_findSession(pRtsp->pStreamerCfg->pRtspSessions, pReq->sessionId))) {
        if(ishttp == 1 && !(pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP)) {
          pRtsp->pSession->sessionType |= RTSP_SESSION_TYPE_HTTP;
        }
      }
    }

    if(pRtsp->pSession && (pRtsp->pSession->flag & RTSP_SESSION_FLAG_EXPIRED)) {
      //fprintf(stderr, "SESSION EXPIRED... out of here port:%d\n", ntohs(pRtsp->pSd->sain.sin_port));
      rc = -1;
      break;
    }

    //fprintf(stderr, "RTSP REQ - method:'%s' version:'%s', url:'%s' uri:'%s' cseq:%d\n", pReq->hr.method, pReq->hr.version, pReq->hr.url, pReq->hr.puri, pReq->cseq);
    //pKv = pReq->hr.reqPairs;
    //while(pKv) {
    //  fprintf(stderr, "hdr '%s':'%s'\n", pKv->key, pKv->val);
    //  pKv = pKv->pnext;
    //}

    LOG(X_DEBUG("RTSP %s %s%s %s%s from %s:%d"), pReq->hr.method, 
        (pRtsp->pSession ? ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_HTTP) ? 
                             "(http) " : "") : ""),
        (pRtsp->pSession ? ((pRtsp->pSession->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? 
                             "(interleaved) " : "") : ""),
        (pRtsp->pSession ? "session " : ""),
        (pRtsp->pSession ? pRtsp->pSession->sessionid : ""),
        net_inet_ntoa(pRtsp->pSd->sain.sin_addr, netbuf), ntohs(pRtsp->pSd->sain.sin_port));

    if(ishttp == -1 && (!strncmp(pReq->hr.method, HTTP_METHOD_GET, 4) ||
       !strncmp(pReq->hr.method, HTTP_METHOD_POST, 5))) {

      if((rc = handle_rtsphttp_req(pRtsp, &req)) < 0) {
        break;
      }

    } else {

      //
      // pReq will be set to reqHttp for RTSP HTTP, otherwise, the session is not HTTP tunnelled
      //
      if(ishttp == -1 && pReq == &req) {
        ishttp = 0; 
      }

      rc = -1;
      if(!strncasecmp(pReq->hr.method, RTSP_REQ_OPTIONS, strlen(RTSP_REQ_OPTIONS))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          rc = rtsp_handle_options(pRtsp, pReq);
        } 
      } else if(!strncasecmp(pReq->hr.method, RTSP_REQ_DESCRIBE, strlen(RTSP_REQ_DESCRIBE))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          rc = rtsp_handle_describe(pRtsp, pReq);
        }
      } else if(!strncasecmp(pReq->hr.method, RTSP_REQ_SETUP, strlen(RTSP_REQ_SETUP))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          if(pRtsp->pClientSession) {
            rc = rtsp_handle_setup_announced(pRtsp, pReq);
          } else {
            rc = rtsp_handle_setup(pRtsp, pReq);
          }
        }
      } else if(!strncasecmp(pReq->hr.method, RTSP_REQ_PLAY, strlen(RTSP_REQ_PLAY))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          if(pRtsp->pClientSession) {
            rc = rtsp_handle_play_announced(pRtsp, pReq);
          } else {
            rc = rtsp_handle_play(pRtsp, pReq);
          }
        }
      } else if(!strncasecmp(pReq->hr.method, RTSP_REQ_PAUSE, strlen(RTSP_REQ_PAUSE))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          rc = rtsp_handle_pause(pRtsp, pReq);
        }
      } else if(!strncasecmp(pReq->hr.method, RTSP_REQ_ANNOUNCE, strlen(RTSP_REQ_ANNOUNCE))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          rc = rtsp_handle_announce(pRtsp, pReq);
        }
      } else if(!strncasecmp(pReq->hr.method, RTSP_REQ_TEARDOWN, strlen(RTSP_REQ_TEARDOWN))) {
        if(rtsp_authenticate(pRtsp, pReq) == 0) {
          if(pRtsp->pClientSession) {
            rc = rtsp_handle_teardown_announced(pRtsp, pReq);
          } else {
            rc = rtsp_handle_teardown(pRtsp, pReq);
          }
          break;
        }
      }

    }

    VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - command rc: %d"), rc) );

    if(rc < 0) {
      break;
    }
    //pReq->hdrCtxt.idxbuf = 0;

   
  } // end of while(...

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - out of loop, rc:%d, closing session"), rc) );

  //fprintf(stderr, "END OF RTSP LOOP RC:%d\n", rc);

  //
  // Close the session on any error
  // 
  if(rc < 0) {
    if(pRtsp->pSession) {
      rtsp_interleaved_stop(pRtsp);
      rtspsrv_closeSession(pRtsp->pStreamerCfg->pRtspSessions, pRtsp->pSession->sessionid);
      pRtsp->pSession = NULL;
    }
  }
   
  rtsp_interleaved_stop(pRtsp);

  //
  // Close and invalidate any RTSP HTTP session created from the GET processor thread
  //
  if(req.pHttpSession) {
    netio_closesocket(&req.pHttpSession->netsocketPost);
    rtspsrv_deleteHttpSession(pRtsp->pStreamerCfg->pRtspSessions, req.httpSessionCookie, &pRtsp->pSd->sain);
  }

  pthread_mutex_destroy(&pRtsp->mtx);

  return rc;
}

int rtsp_addFrame(void *pArg, const OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;
  RTSP_REQ_CTXT_T *pRtsp = (RTSP_REQ_CTXT_T *) pArg;
  unsigned char buf[32];
  unsigned int outidx = 0;
  unsigned int dataLen;
  int isvid;
  int isaud;
  int paused = 0;

  if(!pRtsp || !pFrame) {
    return -1;
  }

  //TODO: isvid/isaud may not be set since the originating frame
  // comes from streamer2.c - which is already RTP packetized data
  isvid = pFrame->isvid;
  isaud = pFrame->isaud;

  if(pRtsp->pSession) {

    if((pRtsp->pSession->flag & RTSP_SESSION_FLAG_EXPIRED)) {
      rc = -1;
    }

    gettimeofday(&pRtsp->pSession->tvlastupdate, NULL);
    //fprintf(stderr, "rtsp_addFrame updated tvlastupdate %u\n", pRtsp->pSession->tvlastupdate.tv_sec);

    //
    // outidx should always be 0, because the frame data comes through its own unique liveQ2
    // the outidx for the SDP should be set appropriately
    //
    outidx = 0;

    //
    // pFrame->isvid / isaud may not be set if coming via liveq_addpkt, such as for interleaved output
    //
    if(!isvid && !isaud) {

      if( !((STREAMER_CFG_T *) pRtsp->pStreamerCfg)->novid &&
         (pFrame->channelId == pRtsp->pSession->vid.ports[0] || 
          pFrame->channelId == pRtsp->pSession->vid.ports[1])) {
        isvid = 1;
      } else if( !((STREAMER_CFG_T *) pRtsp->pStreamerCfg)->noaud &&
                (pFrame->channelId == pRtsp->pSession->aud.ports[0] || 
                 pFrame->channelId == pRtsp->pSession->aud.ports[1])) {
        isaud = 1;
      }
    }

    if((pRtsp->pSession->flag & RTSP_SESSION_FLAG_PAUSED)) {
      paused = 1;
    }
  }

  dataLen = OUTFMT_LEN_IDX(pFrame, outidx);

  VSX_DEBUG_RTSP( 
    static int s_rtsp_idx; 
    LOG(X_DEBUGV("RTSP - addFrame[outidx:%d] %s [%d] len:%d pts:%.3f  dts:%.3f  key:%d, channelId:%d, "
                "v:{%d,%d}, a:{%d,%d}"), 
                pFrame->xout.outidx, isvid ? "vid" : (isaud ? "aud" : "?"), 
                isvid ? s_rtsp_idx++ : 0,OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), 
                PTSF(OUTFMT_DTS(pFrame)), OUTFMT_KEYFRAME(pFrame), pFrame->channelId,
                pRtsp->pSession->vid.ports[0], pRtsp->pSession->vid.ports[1],
                pRtsp->pSession->aud.ports[0], pRtsp->pSession->aud.ports[1]);
    LOGHEX_DEBUGV(OUTFMT_DATA(pFrame), MIN(16, dataLen));
  )

  if((isvid && ((STREAMER_CFG_T *) pRtsp->pStreamerCfg)->novid) || 
     (isaud && ((STREAMER_CFG_T *) pRtsp->pStreamerCfg)->noaud)) {
    return rc;
  }

  if(rc >= 0 && OUTFMT_DATA(pFrame) == NULL) {
    LOG(X_ERROR("RTSP Frame output[%d] data not set"), pFrame->xout.outidx);
    //TODO: set (create) a flag causing rtsp_handle_conn loop to exit
    rc = -1;
  }

  if(rc >= 0 && pRtsp->pSession) {
    if(isvid && !pRtsp->pSession->vid.haveKeyFrame && OUTFMT_KEYFRAME(pFrame)) {
      pRtsp->pSession->vid.haveKeyFrame = 1;
    }

    //
    // Drop any audio & video RTP packets prior to getting a video key frame
    //
    if(!(pRtsp->pStreamerCfg->novid) && !pRtsp->pSession->vid.haveKeyFrame) {
//fprintf(stderr, "RTSP ADDFRAME %s len:%d DROP (no keyframe yet)\n", isvid ? "vid" : "aud", OUTFMT_LEN(pFrame));
      //codec_nonkeyframes_warn(&pRtsp->pSession->vid, "RTSP");
      pRtsp->pSession->vid.nonKeyFrames++;
      if(pRtsp->pSession->vid.nonKeyFrames % 300 == 299) {
        LOG(X_WARNING("RTSP Video output no key frame detected after %d video packets"),
            pRtsp->pSession->vid.nonKeyFrames);
        streamer_requestFB(pRtsp->pStreamerCfg, pFrame->xout.outidx, ENCODER_FBREQ_TYPE_FIR, 0, 
                           REQUEST_FB_FROM_LOCAL);
      }
      return 0;
    }
  }


  if(rc >= 0 && !paused) {

    //
    // 1 byte '$' header
    // 1 byte channel id
    // 2 byte length
    //
    buf[0] = RTSP_INTERLEAVED_HEADER;
    buf[1] = pFrame->channelId;
    *((uint16_t *) &buf[2]) = htons(dataLen);

   VSX_DEBUG_RTSP( 
      LOG(X_DEBUGV("RTSP sending channelId:%d isvid:%d outidx:%d len:%d 0x%x 0x%x 0x%x 0x%x"), 
          pFrame->channelId, isvid, outidx,  OUTFMT_LEN_IDX(pFrame, outidx), buf[0], buf[1], buf[2], buf[3]);
      //LOGHEX_DEBUG(OUTFMT_DATA_IDX(pFrame, outidx),  MIN(dataLen, 32));
    )

    if((rc = netio_send(&pRtsp->pSd->netsocket,  &pRtsp->pSd->sain, buf, 4)) < 0) {
      LOG(X_ERROR("Failed to send RTSP interleaved header for packet length %d"), OUTFMT_LEN_IDX(pFrame, outidx));
    }

    if(rc >= 0 && (rc = netio_send(&pRtsp->pSd->netsocket,  &pRtsp->pSd->sain, 
                    OUTFMT_DATA_IDX(pFrame, outidx), OUTFMT_LEN_IDX(pFrame, outidx))) < 0) {
      LOG(X_ERROR("Failed to send RTSP interleaved data length %d"), OUTFMT_LEN_IDX(pFrame, outidx));
    }
  }

  if(rc < 0) {
    netio_closesocket(&pRtsp->pSd->netsocket);
  }

  return rc;
}

