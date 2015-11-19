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

static int capture_should_retry(int connectretrycntminone, const int *prunning, unsigned int *pidxretry, 
                                int consecretryerr, int consecunauth, CONNECT_RETRY_RC_T rccb) {
  unsigned int retrysleepsec = 0;
  struct timeval tv0, tv1;

  if((!prunning || *prunning == 0) && !g_proc_exit) {

    if(rccb == CONNECT_RETRY_RC_CONNECTAGAIN) {    
      return 1; 
    } else if(consecunauth > 0) {
      if(consecunauth > 2) {
        return 0;
      }
      return 1;
    } else if((connectretrycntminone == 1 || 
              (connectretrycntminone > 1 && (*pidxretry)++ < connectretrycntminone - 1))) {

      if(consecretryerr > 6) {
        retrysleepsec = 40;
      } else if(consecretryerr > 2) {
        retrysleepsec = 15;
      } else {
        retrysleepsec = 3;
      }

      if(retrysleepsec > 0) {
        gettimeofday(&tv0, NULL); 
        while((!prunning || *prunning == 0) && !g_proc_exit) {
          sleep(1);
          gettimeofday(&tv1, NULL); 
          if(TIME_TV_DIFF_MS(tv1, tv0) >= retrysleepsec * 1000) {
            break;
          }
        }
      }
  
      return 1;
    }
  }

  return 0;
}

int connect_with_retry(CONNECT_RETRY_CTXT_T *pRetryCtxt) {
  int rcconn = 0;
  CONNECT_RETRY_RC_T rccb = CONNECT_RETRY_RC_OK;
  int do_retry = 0;
  unsigned int idxretry = 0;
  int consecretryerr = 0;
  int consecunauth = 0;
  uint64_t bytesRead = 0;

  if(!pRetryCtxt || !pRetryCtxt->cbConnectedAction || !pRetryCtxt->pConnectedActionArg || 
     !pRetryCtxt->pnetsock || !pRetryCtxt->psa) {
    return -1;
  }

  do {

    rccb = 0;

    if((rcconn = httpcli_connect(pRetryCtxt->pnetsock, (const struct sockaddr *) pRetryCtxt->psa, 
                                 pRetryCtxt->tmtms, pRetryCtxt->connectDescr)) < 0) {

      if(pRetryCtxt->pAuthCliCtxt && consecunauth > 0) {
        pRetryCtxt->pAuthCliCtxt->authorization[0] = '\0';
        consecunauth = 0;
      }
    }

    if(rcconn >= 0) {

      if(pRetryCtxt->pByteCtr) {
        bytesRead = *pRetryCtxt->pByteCtr;
      }

      rccb = pRetryCtxt->cbConnectedAction(pRetryCtxt->pConnectedActionArg);

      if(rccb < CONNECT_RETRY_RC_OK && pRetryCtxt->pAuthCliCtxt && 
         pRetryCtxt->pAuthCliCtxt->lastStatusCode == HTTP_STATUS_UNAUTHORIZED) {

        if(!IS_AUTH_CREDENTIALS_SET(pRetryCtxt->pAuthCliCtxt->pcreds)) {
          //
          // We got back 401 UNAUTHORIZED and we don't have any credentials configured, so no point in retrying
          //
          rccb = CONNECT_RETRY_RC_NORETRY;
        } else if(pRetryCtxt->pAuthCliCtxt->authorization[0] != '\0') {
          //
          // Retry again with authorization credentials
          //
          consecunauth++;
          rccb = CONNECT_RETRY_RC_OK;
        }

      } else {
        consecunauth = 0;
      }

      //
      // If the byte counter is set, use it as indication of success, and not the rc code
      //
      if(rccb < CONNECT_RETRY_RC_OK  || (pRetryCtxt->pByteCtr && *pRetryCtxt->pByteCtr == bytesRead)) {
        consecretryerr++;
      //} else if(pRetryCtxt->pByteCtr || rccb >= 0) {
      } else if(rccb >= CONNECT_RETRY_RC_OK) {
        consecretryerr = 0;
        idxretry = 0;
      }

    } else {
      consecretryerr++;
    }

    if((rcconn < 0 || rccb != CONNECT_RETRY_RC_NORETRY) && pRetryCtxt->pconnectretrycntminone) {
      do_retry = capture_should_retry(*pRetryCtxt->pconnectretrycntminone, pRetryCtxt->prunning, 
                                      &idxretry, consecretryerr, consecunauth, rccb);
    } else {
      do_retry = 0;
    }

    //LOG(X_DEBUG("CONNECT_WITH_RETRY rcconn:%d, rccb:%d idxretry:%d, consecretryerr:%d, consecunauth:%d, do_retry:%d, connectretrycntminone:%d, running:%d, g_proc_exit:%d, author:'%s'"), rcconn, rccb, idxretry, consecretryerr, consecunauth, do_retry, *pRetryCtxt->pconnectretrycntminone, 1, g_proc_exit, pRetryCtxt->pAuthCliCtxt ? pRetryCtxt->pAuthCliCtxt->authorization : "");

    if(do_retry) {
      netio_closesocket(pRetryCtxt->pnetsock);
    }

  } while(do_retry && !g_proc_exit);

  return rcconn < 0 ? rcconn : rccb;
}


int http_recvloop(CAP_ASYNC_DESCR_T *pCfg,
                  NETIO_SOCK_T *pnetsock,
                  const struct sockaddr *psa,
                  CAPTURE_CBDATA_T *pStreamsOut,
                  CAPTURE_STREAM_T *pStream,
                  unsigned int offset,
                  int sz, 
                  FILE_OFFSET_T contentLen,
                  unsigned char *pbuf,
                  unsigned int szbuf) {

  COLLECT_STREAM_PKTDATA_T pkt;
  unsigned int sztot = 0;
  char tmp[128];

  if(!pnetsock) {
    return -1;
  }

  if(net_setsocknonblock(PNETIOSOCK_FD(pnetsock), 1) < 0) {
    return -1;
  }

  memset(&pkt, 0, sizeof(pkt));

  do {
   
    if(offset == 0) {
      if((sz = netio_recvnb(pnetsock, pbuf, szbuf, 500)) < 0 || (sz == 0 && PSTUNSOCK(pnetsock)->rcvclosed)) {

        LOG(X_ERROR("Failed to recv HTTP%s data %d/%d bytes from %s:%d "ERRNO_FMT_STR), 
            (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "",
            sz, szbuf, FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)), ERRNO_FMT_ARGS);
        break;
      }
    }

    //fprintf(stderr, "recvloop sock:%d offset:%d sz:%d %d/%d tot:%d\n", pnetsock->sock, offset, sz, sztot, contentLen, pStream->numBytes);

    if(sz > 0) {

      pkt.payload.pData = &pbuf[offset];
      PKTCAPLEN(pkt.payload) = sz;
      gettimeofday(&pkt.tv, NULL);

//static int g_fd;
//if(g_fd==0)g_fd=open("test.ts", O_RDWR | O_CREAT, 0666);
//write(g_fd, pkt.payload.pData, PKTCAPLEN(pkt.payload));

      if(pStream->cbOnPkt(&pStreamsOut->sp[0], &pkt) < 0) {
        LOG(X_ERROR("cbOnPkt returned error"));
        sz = -1;
        break;
      }

      pStream->numBytes += sz;
      sztot += sz;

    } else if(sz == 0) {
      usleep(5000);
    }

    offset = 0;

  } while(sz >= 0 && pCfg->running == 0  &&  
          (contentLen == 0 || sztot < contentLen) &&
         !g_proc_exit);

  return sztot;
}

unsigned char *http_read_net(CAP_HTTP_COMMON_T *pCommon,
                             NETIO_SOCK_T *pnetsock,
                             const struct sockaddr *psa,
                             unsigned int bytesToRead,
                             unsigned char *bufout, 
                             unsigned int szbuf,
                             unsigned int *pBytesRead) {
  int rc;
  unsigned int idx = 0;
  char tmp[128];
  //NETIO_SOCK_T *pnetsock = &pCommon->pCfg->pSockList->netsockets[0];
  //struct sockaddr *psa = &pCommon->pCfg->pSockList->salist[0];

  if(bytesToRead > szbuf) {
    LOG(X_ERROR("HTTP GET live media read request too large %d > %d"), bytesToRead, szbuf);
    return NULL;
  }

  if(pCommon->dataoffset < pCommon->datasz && pCommon->pdata) {
    if((idx = (pCommon->datasz - pCommon->dataoffset)) > bytesToRead) {
      idx = bytesToRead;
    }
    memcpy(bufout, &pCommon->pdata[pCommon->dataoffset], idx);
    bytesToRead -= idx;
    pCommon->dataoffset += idx;
    if(pBytesRead) {
      (*pBytesRead) += idx;
    }
  }

  if(bytesToRead > 0) {

    //TODO: this should not be an arbitrary timeout
    if((rc = netio_recvnb_exact(pnetsock, &bufout[idx], bytesToRead, 7000)) != bytesToRead) {
      LOG(X_ERROR("Failed to recv HTTP%s data %d bytes from %s:%d "ERRNO_FMT_STR), 
        (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "",
        bytesToRead, FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)), ERRNO_FMT_ARGS);
      return NULL;
    }
    idx += rc;
    if(pBytesRead) {
      (*pBytesRead) += rc;
    }
  }

  return bufout;
}

static CONNECT_RETRY_RC_T gethttpdata(CAP_ASYNC_DESCR_T *pCfg,
                                      CAPTURE_CBDATA_T *pStreamsOut,
                                      CAPTURE_STREAM_T *pStream,
                                      const char *puri,
                                      int *pstreamAdded,
                                       HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt) {

  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T httpResp;
  enum CAP_HTTP_ACTION action = CAP_HTTP_ACTION_UNKNOWN;
  enum CAP_HTTP_ACTION actionCfg = CAP_HTTP_ACTION_UNKNOWN;
  CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream = NULL;
  const char *p = NULL;
  FILE_OFFSET_T contentLen = 0;
  unsigned char buf[RTP_JTBUF_PKT_BUFSZ_LOCAL + SOCKET_LIST_PREBUF_SZ];

  if(pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_HTTPFLV) {
    action = actionCfg = CAP_HTTP_ACTION_FLV;
  } else if(pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_HTTPMP4) {
    action = actionCfg = CAP_HTTP_ACTION_MP4;
  } else if(pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_HTTPDASH) {
    action = actionCfg = CAP_HTTP_ACTION_DASH;
  //} else if(pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_HTTPTS) {
  //  action = actionCfg = CAP_HTTP_ACTION_TS;
  }

  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  memset(&httpResp, 0, sizeof(httpResp));
  hdrCtxt.pnetsock = &pCfg->pSockList->netsockets[0];
  hdrCtxt.pbuf = (char *) buf;
  hdrCtxt.szbuf = sizeof(buf);
  hdrCtxt.tmtms = 0;

  if((httpcli_gethdrs(&hdrCtxt, &httpResp, (const struct sockaddr *) &pCfg->pSockList->salist[0], puri,
          http_getConnTypeStr(HTTP_CONN_TYPE_CLOSE), 0, 0, pCfg->pcommon->addrsExtHost[0],
          pAuthCliCtxt)) < 0) {
    return CONNECT_RETRY_RC_ERROR;
  }

  if((p = conf_find_keyval(httpResp.hdrPairs, HTTP_HDR_CONTENT_TYPE))) {

    LOG(X_DEBUG("Received "HTTP_HDR_CONTENT_TYPE": %s"), p);

    if(!strncasecmp(p, CONTENT_TYPE_M3U8, strlen(CONTENT_TYPE_M3U8)) ||
       // TODO: handle multi-br or master playlist
       !strncasecmp(p, CONTENT_TYPE_VND_APPLE, strlen(CONTENT_TYPE_VND_APPLE)) ||
       !strncasecmp(p, CONTENT_TYPE_M3U, strlen(CONTENT_TYPE_M3U))) {

      action = CAP_HTTP_ACTION_HTTPLIVE;

    } else if(!strncasecmp(p, CONTENT_TYPE_FLV, strlen(CONTENT_TYPE_FLV))) {

      //if(pCfg->pcommon->filt.filters[0].mediaType != CAPTURE_FILTER_PROTOCOL_FLV) {
      if(pCfg->pcommon->filt.filters[0].transType != CAPTURE_FILTER_TRANSPORT_HTTPFLV &&
         pCfg->pcommon->filt.filters[0].transType != CAPTURE_FILTER_TRANSPORT_HTTPSFLV) {
        LOG(X_ERROR("Capture filter should be set to flv '--filter=\"type=flv\"' for HTTP %s %s"), 
             HTTP_HDR_CONTENT_TYPE, p);
        rc = CONNECT_RETRY_RC_NORETRY;
      }

      action = CAP_HTTP_ACTION_FLV;

    } else if(!strncasecmp(p, CONTENT_TYPE_MP4, strlen(CONTENT_TYPE_MP4)) ||
              !strncasecmp(p, CONTENT_TYPE_XM4V, strlen(CONTENT_TYPE_XM4V)) ||
              !strncasecmp(p, CONTENT_TYPE_XM4A, strlen(CONTENT_TYPE_XM4A))) {

      //if(pCfg->pcommon->filt.filters[0].mediaType != CAPTURE_FILTER_PROTOCOL_MP4) {
      if(pCfg->pcommon->filt.filters[0].transType != CAPTURE_FILTER_TRANSPORT_HTTPMP4 &&
         pCfg->pcommon->filt.filters[0].transType != CAPTURE_FILTER_TRANSPORT_HTTPSMP4) {
        LOG(X_ERROR("Capture filter should be set to mp4 '--filter=\"type=mp4\"' for HTTP %s %s"), 
             HTTP_HDR_CONTENT_TYPE, p);
        rc = CONNECT_RETRY_RC_NORETRY;
      }

      action = CAP_HTTP_ACTION_MP4;

    } else if(!strncasecmp(p, CONTENT_TYPE_MP2TS, strlen(CONTENT_TYPE_MP2TS)) ||
              !strncasecmp(p, CONTENT_TYPE_OCTET_STREAM, strlen(CONTENT_TYPE_OCTET_STREAM))) {

      action = CAP_HTTP_ACTION_TS;

    } else if(actionCfg == CAP_HTTP_ACTION_UNKNOWN) {

      LOG(X_ERROR("Unsupported %s: %s received in response for %s"), 
               HTTP_HDR_CONTENT_TYPE, p, puri);
      rc = CONNECT_RETRY_RC_NORETRY;

    } else if(actionCfg != CAP_HTTP_ACTION_UNKNOWN && action != actionCfg) {

      LOG(X_ERROR("%s: %s received in response for %s does not match configured %s"), 
               HTTP_HDR_CONTENT_TYPE, p, puri, HTTP_HDR_CONTENT_TYPE);
      rc = CONNECT_RETRY_RC_NORETRY;

    }

  } else if(action == CAP_HTTP_ACTION_UNKNOWN) {

    LOG(X_ERROR("No "HTTP_HDR_CONTENT_TYPE" found in response"));
    rc = CONNECT_RETRY_RC_NORETRY;

  }

  if((p = conf_find_keyval(httpResp.hdrPairs, HTTP_HDR_CONTENT_LEN))) {
    contentLen = atoi(p);
  }

  if(action != CAP_HTTP_ACTION_FLV && action != CAP_HTTP_ACTION_MP4 && 
     action != CAP_HTTP_ACTION_DASH &&
     !(*pstreamAdded) && rc >= CONNECT_RETRY_RC_OK && 
    (capture_cbOnStreamAdd(pStreamsOut, pStream, NULL, NULL) < 0 ||
     pStream->cbOnPkt == NULL)) {
    LOG(X_ERROR("Unable to initialize HTTP packet processing"));
    rc = CONNECT_RETRY_RC_NORETRY;
  }

  if(rc >= CONNECT_RETRY_RC_OK && pCfg->running == 0) {

    switch(action) {
      case CAP_HTTP_ACTION_FLV:

        //
        // FLV over HTTP 
        //
        if(http_flv_recvloop(pCfg, hdrCtxt.hdrslen, hdrCtxt.idxbuf - hdrCtxt.hdrslen, 
                               contentLen, buf, sizeof(buf), pStream) < 0) {
          rc = CONNECT_RETRY_RC_ERROR;
        }
        break;

      case CAP_HTTP_ACTION_MP4:

        //
        // MP4 over HTTP
        //

        pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCfg->pUserData;
        pCapHttpMp4Stream->pl.endOfList = 1;

        if(http_mp4_recv(pCfg, &pCfg->pSockList->netsockets[0], (const struct sockaddr *) 
                         &pCfg->pSockList->salist[0], contentLen, &hdrCtxt, NULL) < 0) {
          rc = CONNECT_RETRY_RC_ERROR;
        }
        break;

      case CAP_HTTP_ACTION_HTTPLIVE:

        //
        // HTTPLive content (chunked mpeg-2 ts using playlist)
        //
        if(http_gethttplive(pCfg, pStreamsOut, pStream, puri, &httpResp, &hdrCtxt) < 0) {
          rc = CONNECT_RETRY_RC_ERROR;
        }
        break;

      case CAP_HTTP_ACTION_DASH:

        //
        // DASH content 
        //
        //rc = http_gethttpdash(pCfg, puri, &httpResp, &hdrCtxt);
        //TODO: finish implementing MPD parser
        rc = CONNECT_RETRY_RC_NORETRY;

        break;

      default:

        //
        // continuous non-segmented stream using mpeg-2 ts encapsulation
        //
        if(http_recvloop(pCfg, &pCfg->pSockList->netsockets[0],
               (const struct sockaddr *) &pCfg->pSockList->salist[0], pStreamsOut, pStream, 
               hdrCtxt.hdrslen, hdrCtxt.idxbuf - hdrCtxt.hdrslen, 
               contentLen, buf, sizeof(buf)) < 0) {
          rc = CONNECT_RETRY_RC_ERROR;
        }
        break;
    }

  }

  LOG(X_DEBUG("Ending HTTP%s reception rc:%d"), 
               (pCfg->pSockList->netsockets[0].flags & NETIO_FLAG_SSL_TLS) ? "S" : "", rc);

  return rc;
}

typedef struct CONNECT_RETRY_CTXT_HTTP {
  CAP_ASYNC_DESCR_T         *pCfg;
  CAPTURE_CBDATA_T          *pstreamCbData;
  CAPTURE_STREAM_T          *pstream;
  const char                *puri;
  int                       *pstreamAdded;  
  HTTPCLI_AUTH_CTXT_T       *pAuthCliCtxt;
} CONNECT_RETRY_CTXT_HTTP_T;

CONNECT_RETRY_RC_T capture_http_cbonconnect(void *pArg) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  CONNECT_RETRY_CTXT_HTTP_T *pCtxt = (CONNECT_RETRY_CTXT_HTTP_T *) pArg;
  rc = gethttpdata(pCtxt->pCfg, pCtxt->pstreamCbData, pCtxt->pstream, pCtxt->puri, 
                   pCtxt->pstreamAdded, pCtxt->pAuthCliCtxt);
  return rc;
}


int capture_httpget(CAP_ASYNC_DESCR_T *pCfg) {

  int streamAdded = 0;
  int rc = 0;
  CAPTURE_CBDATA_T streamCbData;
  CAPTURE_STREAM_T stream;
  HTTPCLI_AUTH_CTXT_T authCliCtxt;
  const char *puri = VSX_TSLIVE_URL;
  CONNECT_RETRY_CTXT_T retryCtxt;
  CONNECT_RETRY_CTXT_HTTP_T retryCbCtxt;

  if(pCfg->pcommon->filt.numFilters <= 0) {
    return -1;
  }

  //fprintf(stderr, "HTTP GET... transType:%d, mediaType:%d\n", pCfg->pcommon->filt.filters[0].transType, pCfg->pcommon->filt.filters[0].mediaType);

  if(!(IS_CAPTURE_FILTER_TRANSPORT_HTTP(pCfg->pcommon->filt.filters[0].transType) ||
       pCfg->pcommon->filt.filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_MP2TS)) {

    LOG(X_ERROR("The capture type is not supported for HTTP%s"),
        IS_CAPTURE_FILTER_TRANSPORT_SSL(pCfg->pcommon->filt.filters[0].transType) ? "S" : "");

    return -1;
  }

  if(pCfg->pcommon->addrsExt[0]) {
    puri = pCfg->pcommon->addrsExt[0];
  }

  NETIOSOCK_FD(pCfg->pSockList->netsockets[0]) = INVALID_SOCKET;

  memset(&stream, 0, sizeof(stream));
  stream.pFilter = (CAPTURE_FILTER_T *) pCfg->pcommon->filt.filters;

  memset(&authCliCtxt, 0, sizeof(authCliCtxt));
  authCliCtxt.pcreds = &pCfg->pcommon->creds[0];
  authCliCtxt.puri = puri;

  capture_initCbData(&streamCbData, pCfg->record.outDir, pCfg->record.overwriteOut);

  //
  // Enable any transmitter thread to initialize thus marking the queue haveRdr = 1
  //
  usleep(50000);

  //
  // Initialize the retry context for connecting to the remote endpoint
  //
  memset(&retryCtxt, 0, sizeof(retryCtxt));
  retryCbCtxt.pCfg = pCfg;
  retryCbCtxt.pstreamCbData = &streamCbData;
  retryCbCtxt.pstream = &stream;
  retryCbCtxt.puri = puri;
  retryCbCtxt.pstreamAdded = &streamAdded;
  retryCbCtxt.pAuthCliCtxt = &authCliCtxt;
  retryCtxt.cbConnectedAction = capture_http_cbonconnect;
  retryCtxt.pConnectedActionArg = &retryCbCtxt;
  retryCtxt.pAuthCliCtxt = &authCliCtxt;
  retryCtxt.pnetsock = &pCfg->pSockList->netsockets[0];
  retryCtxt.psa = (const struct sockaddr *) &pCfg->pSockList->salist[0];
  retryCtxt.pByteCtr = &stream.numBytes;
  retryCtxt.connectDescr = "HTTP";
  retryCtxt.pconnectretrycntminone = &pCfg->pcommon->connectretrycntminone;
  retryCtxt.prunning = &pCfg->running;

  //
  // Connect to the remote and call capture_rtsp_cbonconnect
  // This will automatically invoke any retry logic according to the retry config
  //
  rc = connect_with_retry(&retryCtxt);

  capture_deleteCbData(&streamCbData);
  netio_closesocket(&pCfg->pSockList->netsockets[0]);

  if(rc > 0) {
    rc = 0;
  }

  return rc;
}

#endif // VSX_HAVE_CAPTURE
