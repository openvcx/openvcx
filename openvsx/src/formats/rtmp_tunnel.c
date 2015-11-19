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

#define RTMPT_CLIENT_USER_AGENT   "Shockwave Flash"
#define RTMPT_CMD_OPEN            "/open"
#define RTMPT_CMD_IDLE            "/idle"
#define RTMPT_CMD_SEND            "/send"
#define RTMPT_URL_IDENT           "/fcs/ident2"
#define RTMPT_URL_OPEN            RTMPT_CMD_OPEN"/1"
#define RTMPT_SESSION_ID_LEN      10

#define RTMPT_IDLE_MIN_INTERVAL_MS    500

static int create_post_hdrs(KEYVAL_PAIR_T kvs[], unsigned int szKvs) {
  unsigned int kvidx = 0;

  if(kvidx < szKvs) {
    memset(&kvs[kvidx], 0, sizeof(KEYVAL_PAIR_T));
    snprintf(kvs[kvidx].key, sizeof(kvs[kvidx].key), "%s", HTTP_HDR_CONTENT_TYPE);
    snprintf(kvs[kvidx].val, sizeof(kvs[kvidx].val), "%s", CONTENT_TYPE_RTMPTUNNELED);
  }

  if(kvidx < szKvs) {
    kvidx++;
    memset(&kvs[kvidx], 0, sizeof(KEYVAL_PAIR_T));
    kvs[kvidx - 1].pnext = &kvs[kvidx];
    snprintf(kvs[kvidx].key, sizeof(kvs[kvidx].key), "%s", HTTP_HDR_ACCEPT);
    snprintf(kvs[kvidx].val, sizeof(kvs[kvidx].val), "*/*");
  }

  return kvidx;
}

static int rtmpt_cli_sendheader(RTMP_CTXT_T *pRtmp, const char *cmd, unsigned int sz, const char *descr) {
  int rc = 0;
  KEYVAL_PAIR_T kvs[5];
  char uribuf[64];
  const char *userAgent = RTMPT_CLIENT_USER_AGENT;
  unsigned int contentLen;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;

  contentLen = sz + pRtmpT->idxQueued;
  memset(kvs, 0, sizeof(kvs));
  create_post_hdrs(kvs, sizeof(kvs) / sizeof(kvs[0]));
  snprintf(uribuf, sizeof(uribuf) - 1, "%s/%lld/%d", cmd, pRtmpT->sessionId, pRtmpT->seqnum++);

  VSX_DEBUG_RTMPT(LOG(X_DEBUG("RTMPT - rtmpt_cli_sendheader %s %s contentLen: %d (idxQueued:%d + %d), pending: %d"), 
     descr ? descr : "", uribuf, contentLen, pRtmpT->idxQueued, sz, pRtmpT->pendingRequests + 1); );

  rc = httpcli_req_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa,
                        HTTP_METHOD_POST, uribuf, http_getConnTypeStr(HTTP_CONN_TYPE_KEEPALIVE),
                        pRtmpT->phosthdr, userAgent, NULL, kvs, NULL, contentLen);
               
  pRtmpT->pendingRequests++;

  return rc;
}

static int rtmpt_cli_sendidle(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  unsigned char buf[4];
  unsigned int len = 1;

  pRtmp->rtmpt.tmLastIdleReq = timer_GetTime();

  if((rc = rtmpt_cli_sendheader(pRtmp, RTMPT_CMD_IDLE, 1, "rtmpt-idle")) < 0) {
    return rc;
  }

  buf[0] = '\0';
  if((rc = netio_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa, buf, len)) != len) {
    return -1;
  }

  return rc;
}

static int rtmpt_cli_send(RTMP_CTXT_T *pRtmp, const unsigned char *pData, unsigned int sz, const char *descr) {
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;

  if(!pData) {
    return -1;
  }

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_cli_send %s sz: %d "), descr ? descr: "", sz) );

  //
  // check if we're supposed to just queue the output data, only POSTING it on a subsequent invocation
  //
  if(pRtmpT->doQueueOut && pRtmpT->pbuftmp) {
    if(pRtmpT->idxQueued + sz > pRtmpT->szbuftmp) {
      LOG(X_ERROR("RTMPT - %s idxQueued:%u + %u > %u"), descr ? descr : "", 
                  pRtmpT->idxQueued, sz, pRtmpT->szbuftmp);
      return -1;
    }
    memcpy(&pRtmpT->pbuftmp[pRtmpT->idxQueued], pData, sz);
    pRtmpT->idxQueued += sz;
    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_cli_send %s queueing: %d, now queued: %d "), descr ? descr: "", sz, 
                          pRtmpT->idxQueued); );
    return 0;
  }

  if(pRtmpT->szToPost <= 0) {
    if((rc = rtmpt_cli_sendheader(pRtmp, RTMPT_CMD_SEND, sz, descr)) < 0) {
      return rc;
    }
    pRtmpT->szToPost += (sz + pRtmpT->idxQueued);
  }

  //
  // Flush any queued data
  //
  if(pRtmpT->idxQueued > 0) {

    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_cli_send %s send queued: %d"), descr ? descr : "", 
                                pRtmpT->idxQueued);
                    LOGHEXT_DEBUGV(pRtmpT->pbuftmp, pRtmpT->idxQueued); );

    if((rc = netio_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa, 
                        pRtmpT->pbuftmp, pRtmpT->idxQueued)) != pRtmpT->idxQueued) {
      return -1;
    }
    pRtmpT->szToPost -= rc;
    pRtmpT->idxQueued = 0;
  }

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_cli_send %s send: %d"), descr ? descr : "", sz);
                  LOGHEXT_DEBUGV(pData, sz); );

  if((rc = netio_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa, pData, sz)) != sz) {
    return -1;
  }
  pRtmpT->szToPost -= rc;

  return rc;
}

static int send_response(RTMP_CTXT_T *pRtmp, const unsigned char *buf, 
                         unsigned int sz, unsigned int lenPreBuf, enum HTTP_STATUS statusCode,
                         const KEYVAL_PAIR_T *pHdrs) {
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;
  unsigned char prebuf[4];

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - send_response pending: %d, [%d]/(%d + %d), sz:%d, status: %d, Connection: %s (%d)"), pRtmpT->pendingResponse, pRtmpT->idxInResp, lenPreBuf, pRtmpT->contentLenResp, sz, statusCode, http_getConnTypeStr(pRtmpT->httpReq.connType), pRtmpT->httpReq.connType); );

  if(pRtmpT->pendingResponse > 0) {

    prebuf[0] = 0x01;
    pRtmpT->lastCmd = RTMPT_ICMD_NONE;
    pRtmpT->idxInResp = 0;
    pRtmpT->pendingResponse--;

    if((rc = http_resp_sendhdr(pRtmp->pSd, pRtmpT->httpReq.version, statusCode, lenPreBuf + pRtmpT->contentLenResp, 
                               CONTENT_TYPE_RTMPTUNNELED, http_getConnTypeStr(pRtmpT->httpReq.connType), 
                               NULL, NULL, NULL, NULL, NULL, pHdrs)) < 0) {
      return rc;
    } 

    if(lenPreBuf > 0) {
      VSX_DEBUG_RTMPT (
        LOG(X_DEBUG("RTMP - send_response prebuf length: %d"), lenPreBuf);
        LOGHEXT_DEBUG(prebuf, MIN(lenPreBuf, 512));
      )
      if((rc = netio_send(pRtmpT->httpCtxt.pnetsock, 
                      (const struct sockaddr *) &pRtmp->pSd->sa, prebuf, lenPreBuf)) != lenPreBuf) {
        return -1;
      }
    }

    pRtmpT->tmLastResp = timer_GetTime();
  }

  rc = 0;

  if(buf && pRtmpT->idxInResp < pRtmpT->contentLenResp) {

    sz = MIN(sz, pRtmpT->contentLenResp - pRtmpT->idxInResp);
    if(sz > 0) {

      VSX_DEBUG_RTMP (
        LOG(X_DEBUG("RTMP - send_response body length: [%d] + %d / %d"), 
                    pRtmpT->idxInResp, sz, pRtmpT->contentLenResp);
        LOGHEXT_DEBUG(buf, MIN(sz, 1024));
      )

      if((rc = netio_send(pRtmpT->httpCtxt.pnetsock, (const struct sockaddr *) &pRtmp->pSd->sa, 
                          buf, sz)) != sz) {
        return -1;
      }
  
      pRtmpT->idxInResp += rc;
    }

  }

  return rc;
}


static int rtmpt_srv_send(RTMP_CTXT_T *pRtmp, const unsigned char *pData, unsigned int sz, const char *descr) {
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;
  unsigned int lenPreBuf = 0;
  int doQueueMore = 0;
  TIME_VAL tm;

  //if(pRtmp->state >= RTMP_STATE_CLI_PLAY && sz > 0 && pRtmpT->idxbufdyn < 0x20000) { 
  //  doQueueMore = 1;
  //}
  tm = timer_GetTime();
  if(pRtmp->state >= RTMP_STATE_CLI_PLAY && sz > 0 && pRtmpT->idxbufdyn + sz < pRtmpT->szbufdyn) { 
    if(((tm - pRtmpT->tmLastResp) / TIME_VAL_MS) < 400) {
      doQueueMore = 1;
    }
  }

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_srv_send %s len: %d, doQueueOut: %d, ms-ago: %lld, rtmp.state: %d, doQueueMore: %d, pendingResponse: %d, [%d]/%d"), descr ? descr : "", sz, pRtmpT->doQueueOut, pRtmp->state, doQueueMore, ((tm - pRtmpT->tmLastResp)/TIME_VAL_MS), pRtmpT->pendingResponse, pRtmpT->idxbufdyn, pRtmpT->szbufdyn); );

  if(!doQueueMore && pRtmpT->pendingResponse > 0) {
    pRtmpT->contentLenResp = pRtmpT->idxbufdyn + sz;
    lenPreBuf = 1;

    //pRtmpT->tmLastResp = timer_GetTime();

    if(pRtmpT->idxbufdyn > 0) {
      if((rc = send_response(pRtmp, pRtmpT->pbufdyn, pRtmpT->idxbufdyn, lenPreBuf, HTTP_STATUS_OK, NULL)) <0) {
        return rc;
      }
      pRtmpT->idxbufdyn = 0;
      lenPreBuf--;
    }
 
    if((rc = send_response(pRtmp, pData, sz, lenPreBuf, HTTP_STATUS_OK, NULL)) < 0) {
      return rc;      
    } 

  } else if(sz > 0) {
    if(pRtmpT->idxbufdyn + sz > pRtmpT->szbufdyn) {
      LOG(X_DEBUG("RTMPT - rtmpt_srv_send [%d] + %d > %d"), pRtmpT->idxbufdyn, sz, pRtmpT->szbufdyn);
      return -1;
    }
    memcpy(&pRtmpT->pbufdyn[pRtmpT->idxbufdyn], pData, sz);
    pRtmpT->idxbufdyn += sz;
  }

  //TODO: check if we're pendinga response, then send HTTP respons here, otherwise just queue up to next response

  return rc;
}

int rtmpt_send(RTMP_CTXT_T *pRtmp, const unsigned char *pData, unsigned int sz, const char *descr) {

  if(!pRtmp || !pRtmp->ishttptunnel) {
    return -1;
  } else if(pRtmp->rtmpt.isclient) {
    return rtmpt_cli_send(pRtmp, pData, sz, descr);
  } else {
    return rtmpt_srv_send(pRtmp, pData, sz, descr);
  }

}


static int consume_payload(RTMP_CTXT_T *pRtmp, unsigned int *pidxdata, unsigned char *pData, unsigned int len) {
  unsigned int szcopy = 0;
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;

  //
  // First consume any existing RTMP data already read into the httpCtxt.pbuf 
  //
  if(pRtmpT->idxInHttpBuf < pRtmpT->httpCtxt.idxbuf) {

    //
    // The server sends the first byte as out-of-band data and we discard it as non RTMP
    //
    if(pRtmpT->lenFirstByte > 0) {
      pRtmpT->idxInContentLen += pRtmpT->lenFirstByte;
      pRtmpT->idxInHttpBuf += pRtmpT->lenFirstByte;
      VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - consume_payload consumed lenFirstByte %d from existing http resp., rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d"), pRtmpT->lenFirstByte, *pidxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf););
      pRtmpT->lenFirstByte  = 0;
    }

    szcopy = MIN(pRtmpT->httpCtxt.idxbuf - pRtmpT->idxInHttpBuf,
                 pRtmpT->contentLen - pRtmpT->idxInContentLen);
    szcopy = MIN(szcopy, len - *pidxdata);
  }

  if(szcopy > 0) {
    memcpy(&pData[*pidxdata], &pRtmpT->httpCtxt.pbuf[pRtmpT->idxInHttpBuf], szcopy);
    *pidxdata += szcopy;
    pRtmpT->idxInContentLen += szcopy;
    pRtmpT->idxInHttpBuf += szcopy;
    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - consume_payload copied %d from existing http resp., rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d"), szcopy, *pidxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf););
  }

  //
  // Read from the socket until contentLen
  //
  if(pRtmpT->lenFirstByte > 0) {
    szcopy = pRtmpT->lenFirstByte;
  } else {
    szcopy = MIN(pRtmpT->contentLen - pRtmpT->idxInContentLen, len - *pidxdata);
  }

  if(szcopy > 0) {
    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - consume_payload reading %d from socket, rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d, lenFirstByte: %d"), szcopy, *pidxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf, pRtmpT->lenFirstByte););
    if((rc = netio_recvnb_exact(&pRtmp->pSd->netsocket, &pData[*pidxdata], szcopy, pRtmp->rcvTmtMs)) != szcopy) {
      if(rc == 0) {
        // timeout or remotely closed
        return -1;
      }
      return -1;
    }
    if(pRtmpT->lenFirstByte > 0) {
      pRtmpT->lenFirstByte -= rc;
    } else {
      *pidxdata += rc;
    }
    pRtmpT->idxInContentLen += rc;
  }

  return 1;
}

static int rtmpt_cli_recv(RTMP_CTXT_T *pRtmp,  unsigned char *pData, unsigned int len) {
  int rc = 0;
  unsigned int idxdata = 0;
  HTTP_RESP_T httpResp;
  //unsigned int szcopy;
  TIME_VAL tm;
  int noContentAvail = 0;
  int sleepMs = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;

  do {

    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_cli_recv start, pending: %d, rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d, termcharidx: %d"), pRtmpT->pendingRequests, idxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf, pRtmpT->httpCtxt.termcharidx););

    //
    // Check if we need to send a new POST request because any prior content body has been consumed
    //
    if(pRtmpT->idxInContentLen >= pRtmpT->contentLen) {

      //
      // If there are no outstanding POST responses to be read, we need to send an /idle POST request
      //
      if(pRtmpT->pendingRequests == 0) {

        //
        // If we previously received no RTMP data in our tunnel, then sleep a while before asking
        // the server again
        //
        if(noContentAvail) {
          tm = timer_GetTime();
          if((sleepMs = (tm - pRtmpT->tmLastIdleReq) / TIME_VAL_MS) < RTMPT_IDLE_MIN_INTERVAL_MS) {
            sleepMs = RTMPT_IDLE_MIN_INTERVAL_MS - sleepMs;
            usleep(sleepMs * 1000);
          } else {
            sleepMs = 0;
          }
          LOG(X_DEBUG("RTMPT - slept %d ms..."), sleepMs);
        } 

        if((rc = rtmpt_cli_sendidle(pRtmp)) < 0) {
          break;
        }
      }

      //
      // Send an HTTP POST request and read the headers response and Content-Length
      //
      pRtmpT->idxInContentLen = 0;
      pRtmpT->contentLen = 0;
      HTTP_PARSE_CTXT_RESET(pRtmpT->httpCtxt);

      memset(&httpResp, 0, sizeof(httpResp));
      if((rc = httpcli_req_queryhdrs(&pRtmpT->httpCtxt, &httpResp, 
                                     (const struct sockaddr *) &pRtmp->pSd->sa, NULL, "RTMPT")) < 0 ||
          !httpcli_get_contentlen_start(&httpResp, &pRtmpT->httpCtxt, 
                                        (unsigned char *) pRtmpT->httpCtxt.pbuf, 
                                        pRtmpT->httpCtxt.szbuf, 0, &pRtmpT->contentLen)) {
        LOG(X_DEBUG("RTMPT - rtmpt_cli_recv httpcli_req_queryhdrs rc: %d, rcvclosed: %d, tmtms: %d, contentLen: %d"), rc, pRtmpT->httpCtxt.rcvclosed, pRtmpT->httpCtxt.tmtms, pRtmpT->contentLen);
        return rc;
      }

      pRtmpT->pendingRequests--;
      pRtmpT->idxInHttpBuf = pRtmpT->httpCtxt.hdrslen;
      pRtmpT->lenFirstByte = 1;
      if(pRtmpT->contentLen >  1) {
        noContentAvail = 0;
      } else {
        noContentAvail = 1;
      }

      //LOG(X_DEBUG("RTMPT - rtmpt_cli_recv got contentLen: %d"), pRtmpT->contentLen);
    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_cli_recv got resp, pending: %d, rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d, termcharidx: %d"), pRtmpT->pendingRequests, idxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf, pRtmpT->httpCtxt.termcharidx););
    }

    if((rc = consume_payload(pRtmp, &idxdata, pData, len)) < 0) {
      return rc;
    } else if(rc == 0) {
      // timeout reached
      break;
    }

  } while(idxdata < len);

  return idxdata;
}

static int prepare_response(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;
  enum HTTP_STATUS statusCode = HTTP_STATUS_OK;
  FILE_OFFSET_T contentLen = 0;
  unsigned char buf[64];
  KEYVAL_PAIR_T kvs[1];
  KEYVAL_PAIR_T *pHdrs = NULL;
  int doQueue = 0;

  buf[0] = 0x01;
  if(pRtmpT->lastCmd == RTMPT_ICMD_IDENT) {

    statusCode = HTTP_STATUS_NOTFOUND;
    contentLen = 0;

    //
    // Force the 404 response to include Content-Length: 0
    //
    memset(kvs, 0, sizeof(kvs));
    snprintf(kvs[0].key, sizeof(kvs[0].key), "%s", HTTP_HDR_CONTENT_LEN);
    snprintf(kvs[0].val, sizeof(kvs[0].val), "0");
    pHdrs = &kvs[0];

  } else if(pRtmpT->lastCmd == RTMPT_ICMD_OPEN) {
    pRtmpT->sessionId = ((random() % RAND_MAX) << 16) | (random() % RAND_MAX);
    snprintf((char *) buf, sizeof(buf), "%lld", pRtmpT->sessionId);
    if((contentLen = strlen((char *) buf)) > RTMPT_SESSION_ID_LEN) {
      contentLen = RTMPT_SESSION_ID_LEN;
    }
    buf[contentLen++] = '\n';
    buf[contentLen] = '\0';

  } else if(pRtmpT->lastCmd == RTMPT_ICMD_IDLE) {
   
    if(pRtmpT->doQueueIdle <= 0) {
      contentLen = 1;
    }
    doQueue = pRtmpT->doQueueIdle;

  } else if(pRtmpT->lastCmd == RTMPT_ICMD_SEND) {

    if(pRtmpT->doQueueOut == -1) {
      pRtmpT->doQueueOut = 1;
    }
    doQueue = pRtmpT->doQueueOut;

  } else {
    rc = -1;
  }


//TODO: separate doQueueOut... respond 200 OK to /idle if nothing to send, but keep waiting for /send when expecting data

  //pRtmpT->pendingResponse++;

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - prepare_response contentLen: %d, pending: %d, doQueue: %d, pendingResponse: %d"), contentLen, pRtmpT->pendingResponse, doQueue, pRtmpT->pendingResponse); );

  if(statusCode != HTTP_STATUS_OK || (contentLen > 0 && doQueue <= 0)) {
    pRtmpT->contentLenResp = contentLen;
    rc = send_response(pRtmp, buf, contentLen, 0, statusCode, pHdrs);
  } else if(rc < 0) {
    //
    // Send error response to an invalid request URI
    //
    send_response(pRtmp, NULL, 0, 0, HTTP_STATUS_BADREQUEST, NULL);
  }

  return rc;
}

static int rtmpt_srv_recv(RTMP_CTXT_T *pRtmp,  unsigned char *pData, unsigned int len) {
  const char *p;
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = &pRtmp->rtmpt;
  unsigned int idxdata = 0;

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_srv_recv len: %d, idxbuf0: %d, doQueueOut: %d"), len, pRtmpT->idxbuf0, pRtmpT->doQueueOut) );

  do {

    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_srv_recv start, pending: %d, rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d, termcharidx: %d, doQueueOut: %d, tmtMs: %d"), pRtmpT->pendingResponse, idxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf, pRtmpT->httpCtxt.termcharidx, pRtmpT->doQueueOut, pRtmpT->httpTmtMs););

    //
    // Check if we need to send a new POST request because any prior content body has been consumed
    //
    if(pRtmpT->idxInContentLen >= pRtmpT->contentLen) {

      //
      // Send an HTTP POST request and read the headers response and Content-Length
      //
      pRtmpT->lastCmd = RTMPT_ICMD_NONE;
      pRtmpT->idxInContentLen = 0;
      pRtmpT->contentLen = 0;
      HTTP_PARSE_CTXT_RESET(pRtmpT->httpCtxt);
      pRtmpT->httpCtxt.tmtms = pRtmpT->httpTmtMs;
      pRtmpT->httpCtxt.idxbuf = pRtmpT->idxbuf0;

      if((rc = http_req_readpostparse(&pRtmpT->httpCtxt, &pRtmpT->httpReq, 1)) <= 0) {
        LOG(X_ERROR("RTMPT - Failed to read HTTP request"));
        LOG(X_DEBUG("RTMPT - pending: %d"), pRtmpT->pendingResponse);
        return -1;
      } else if(strcasecmp(pRtmpT->httpReq.method, "POST")) {
        LOG(X_ERROR("RTMPT - Invalid HTTP method: %s"), pRtmpT->httpReq.method);
        return -1;
      } if(!(p = conf_find_keyval(pRtmpT->httpReq.reqPairs, HTTP_HDR_CONTENT_LEN))) {
        LOG(X_ERROR("RTMPT - No %s found in POST request"), HTTP_HDR_CONTENT_LEN);
        return -1;
      }

      pRtmpT->idxbuf0 = 0;
      pRtmpT->idxInHttpBuf = pRtmpT->httpCtxt.hdrslen;
      pRtmpT->contentLen = atoi(p);
      pRtmpT->pendingResponse++;

      VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_srv_recv got req rc: %d, url: '%s' (%s) pending: %d, rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d, termcharidx: %d, connType: %s (%d)"), rc, pRtmpT->httpReq.url, pRtmpT->httpReq.puri, pRtmpT->pendingResponse, idxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf, pRtmpT->httpCtxt.termcharidx, http_getConnTypeStr(pRtmpT->httpReq.connType), pRtmpT->httpReq.connType););

      if(!strncasecmp(pRtmpT->httpReq.puri, RTMPT_URL_IDENT, strlen(RTMPT_URL_IDENT) + 1)) {
        pRtmpT->lastCmd = RTMPT_ICMD_IDENT;
        pRtmpT->lenFirstByte = pRtmpT->contentLen;
      } else if(!strncasecmp(pRtmpT->httpReq.puri, RTMPT_CMD_OPEN"/", strlen(RTMPT_CMD_OPEN) + 1)) {
        pRtmpT->lastCmd = RTMPT_ICMD_OPEN;
        pRtmpT->lenFirstByte = pRtmpT->contentLen;
      } else if(!strncasecmp(pRtmpT->httpReq.puri, RTMPT_CMD_IDLE"/", strlen(RTMPT_CMD_IDLE) + 1)) {
        //TODO: for idle & send, verify session Id in uri and disconnect upon error
        pRtmpT->lastCmd = RTMPT_ICMD_IDLE;
        pRtmpT->lenFirstByte = pRtmpT->contentLen;
      } else if(!strncasecmp(pRtmpT->httpReq.puri, RTMPT_CMD_SEND"/", strlen(RTMPT_CMD_SEND) + 1)) {
        pRtmpT->lastCmd = RTMPT_ICMD_SEND;
        pRtmpT->lenFirstByte = 0;
      } else {
        LOG(X_ERROR("RTMPT - Invalid command received %s"), pRtmpT->httpReq.url);
        return -1;
      }

    }

    if((rc = consume_payload(pRtmp, &idxdata, &pData[idxdata], len - idxdata)) < 0) {
      return rc;
    } else if(rc == 0) {
      // timeout reached
      return 0;
    }

    VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmpt_srv_recv done consuming, cmd: %d, rcvdata: [%d]/%d, contentLen: [%d]/%d, buf: [%d]/%d, hdrslen:%d, idxbuf: %d, lenFirstByte: %d, doQueueOut: %d"), pRtmpT->lastCmd, idxdata, len, pRtmpT->idxInContentLen, pRtmpT->contentLen, pRtmpT->idxInHttpBuf, pRtmpT->httpCtxt.szbuf, pRtmpT->httpCtxt.hdrslen, pRtmpT->httpCtxt.idxbuf, pRtmpT->lenFirstByte, pRtmpT->doQueueOut););
    //
    // We consumed the entire POST and can now send a response
    //
    if(pRtmpT->idxInContentLen >= pRtmpT->contentLen) {

      if(pRtmpT->pendingResponse > 0) {
        if((rc = prepare_response(pRtmp)) < 0) { 
          return rc;
        }
      } else {
        LOG(X_DEBUG("RTMPT - rtmp_srv_recv prepare_response not called since pendingResponse: %d"), pRtmpT->pendingResponse);
      }
    }

  } while(idxdata < len);

  VSX_DEBUG_RTMPT( LOG(X_DEBUG("RTMPT - rtmp_srv_recv returning %d/%d"), idxdata, len); );

  return idxdata;
}

int rtmpt_recv(RTMP_CTXT_T *pRtmp,  unsigned char *pData, unsigned int len) {

  if(!pRtmp || !pData || !pRtmp->ishttptunnel) {
    return -1;
  } else if(pRtmp->rtmpt.isclient) {
    return rtmpt_cli_recv(pRtmp, pData, len);
  } else {
    return rtmpt_srv_recv(pRtmp, pData, len);
  }

}

int rtmpt_setupclient(RTMP_CTXT_CLIENT_T *pClient, unsigned char *rtmptbufin, unsigned int szrtmptbufin,
                      unsigned char *rtmptbufout, unsigned int szrtmptbufout) {
  HTTP_RESP_T httpResp;
  unsigned char *pdata = NULL;
  const char *uri = NULL;
  char uribuf[64];
  const char *userAgent = RTMPT_CLIENT_USER_AGENT;
  unsigned char buf[4096];
  unsigned int szbuf;
  unsigned char post[8];
  KEYVAL_PAIR_T kvs[5];
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = NULL;

  if(!pClient || !(pRtmpT = &pClient->ctxt.rtmpt) || !rtmptbufin || szrtmptbufin <= 0 || 
     !rtmptbufout || szrtmptbufout <= 0) {
    return -1;
  }

  //TODO: set pRtmpT->httpTmtMs;
  //LOG(X_DEBUG("TMT: %d (%d)"), pRtmpT->httpTmtMs, pRtmpT->httpCtxt.tmtms);

  memset(kvs, 0, sizeof(kvs));
  post[0] = '\0';
  pRtmpT->sessionId = 0;
  pRtmpT->seqnum = 0;
  memset(&pRtmpT->httpCtxt, 0, sizeof(pRtmpT->httpCtxt));
  pRtmpT->httpCtxt.pnetsock = &pClient->ctxt.pSd->netsocket;;
  pRtmpT->httpCtxt.pbuf = (const char *) rtmptbufin;
  pRtmpT->httpCtxt.szbuf = szrtmptbufin;
  pRtmpT->httpCtxt.tmtms = pRtmpT->httpTmtMs;
  pRtmpT->pbuftmp = rtmptbufout;
  pRtmpT->szbuftmp = szrtmptbufout;

  create_post_hdrs(kvs, sizeof(kvs) / sizeof(kvs[0]));

  //
  // POST /fcs/ident2
  //
  uri = RTMPT_URL_IDENT;
  memset(&httpResp, 0, sizeof(httpResp));
  szbuf = sizeof(buf);
  pdata = httpcli_post(uri, buf, &szbuf, &pClient->ctxt.pSd->netsocket,
                       (const struct sockaddr *) &pClient->ctxt.pSd->sa,
                       pRtmpT->httpCtxt.tmtms, pRtmpT->phosthdr, 
                       userAgent, &httpResp, kvs, post, 1);

  //
  // We expect a 404
  //
  if(httpResp.statusCode != HTTP_STATUS_NOTFOUND) {
    LOG(X_ERROR("RTMPT expecting %d to %s but got %d"), HTTP_STATUS_NOTFOUND, uri, httpResp.statusCode);
    return -1;
  }

  //
  // POST /open/1
  //
  uri = RTMPT_URL_OPEN;
  memset(&httpResp, 0, sizeof(httpResp));
  szbuf = sizeof(buf);
  pdata = httpcli_post(uri, buf, &szbuf, &pClient->ctxt.pSd->netsocket,
                       (const struct sockaddr *) &pClient->ctxt.pSd->sa,
                       pRtmpT->httpCtxt.tmtms, pRtmpT->phosthdr, 
                       userAgent, &httpResp, kvs, post, 1);
  //
  // We expect a sessionId as the response
  //
  if(httpResp.statusCode != HTTP_STATUS_OK) {
    LOG(X_ERROR("RTMPT got %d response to %s"), httpResp.statusCode, uri);
    return -1;
  } else if((pRtmpT->sessionId = atoll((const char *) pdata)) == 0) {
    LOG(X_ERROR("RTMPT got invalid session id '%s', length: %d, to %s"), pdata, szbuf, uri);
    return -1;
  }

  //
  // POST /idle/<session id>/<command sequence>
  //
  snprintf(uribuf, sizeof(uribuf) - 1, "%s/%lld/%d", RTMPT_CMD_IDLE, 
           pRtmpT->sessionId, pRtmpT->seqnum++);
  memset(&httpResp, 0, sizeof(httpResp));
  szbuf = sizeof(buf);
  pRtmpT->tmLastIdleReq = timer_GetTime();
  pdata = httpcli_post(uribuf, buf, &szbuf, &pClient->ctxt.pSd->netsocket,
                       (const struct sockaddr *) &pClient->ctxt.pSd->sa,
                       pRtmpT->httpCtxt.tmtms, pRtmpT->phosthdr, 
                       userAgent, &httpResp, kvs, post, 1);
  if(httpResp.statusCode != HTTP_STATUS_OK) {
    LOG(X_ERROR("RTMPT got %d response to %s"), httpResp.statusCode, uribuf);
    return -1;
  } 

  pClient->ctxt.ishttptunnel = 1;
  pRtmpT->isclient = 1;

  VSX_DEBUG_RTMPT(LOG(X_DEBUG("RTMPT - rtmpt_setupclient done %s"), uribuf); );

  return rc;
}

int rtmpt_istunneled(const unsigned char *pData, unsigned int len) {
  int rc = 0;

  if(!pData || len < 12) {
    return 0;
  }

  if(!memcmp("POST "RTMPT_URL_IDENT, pData, 5 + strlen(RTMPT_URL_IDENT)) ||
     !memcmp("POST "RTMPT_URL_OPEN, pData, 5 + strlen(RTMPT_URL_OPEN))) {
    rc = 1;
  }

  return rc;
}

int rtmpt_setupserver(RTMP_CTXT_T *pRtmp, const unsigned char *prebufdata, unsigned int prebufdatasz) {
  int rc = 0;
  RTMPT_CTXT_T *pRtmpT = NULL;

  if(!pRtmp || !(pRtmpT = &pRtmp->rtmpt) || !pRtmpT->pbuftmp || pRtmpT->szbuftmp <= 0) {
    return -1;
  }

  pRtmpT->httpTmtMs = 10000;

  memset(&pRtmpT->httpCtxt, 0, sizeof(pRtmpT->httpCtxt));
  pRtmpT->httpCtxt.pnetsock = &pRtmp->pSd->netsocket;;
  pRtmpT->httpCtxt.pbuf = (const char *) pRtmpT->pbuftmp;
  pRtmpT->httpCtxt.szbuf = pRtmpT->szbuftmp;
  pRtmpT->httpCtxt.tmtms = pRtmpT->httpTmtMs;

  pRtmp->ishttptunnel = 1;
  pRtmpT->isclient = 0;
  pRtmpT->doQueueOut = -1;
  pRtmpT->doQueueIdle = -1;

//TODO: need a good way to alloc a buffer...
  pRtmpT->szbufdyn = 0x40000; // 0xffff;
  if(!(pRtmpT->pbufdyn = (unsigned char *) avc_calloc(1, pRtmpT->szbufdyn))) {
    return -1;
  }
  pRtmpT->idxbufdyn = 0;

  if(prebufdata && prebufdatasz > 0) {
    if(prebufdatasz > pRtmpT->httpCtxt.szbuf) {
      return -1;
    }
    memcpy((unsigned char *) pRtmpT->httpCtxt.pbuf, prebufdata, prebufdatasz);
    pRtmpT->idxbuf0 = prebufdatasz;
  }

  //
  // Let the stream monitor context know that this connection is tunneled
  //
  if(pRtmp->pStreamMethod) {
    *pRtmp->pStreamMethod = STREAM_METHOD_RTMPT;
  }
  if(pRtmp->pOutFmt) {
    outfmt_setTunneled(pRtmp->pOutFmt, pRtmp->ishttptunnel);  
  }

  return rc;
}
