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

#include "mgr/srvmgr.h"
#include "mgr/procdb.h"

typedef int (* CB_ONRCVDATA) (void *, int, const unsigned char *, unsigned int);

struct HDR_STATE {
  int havehdr;
  int seqmatchidx;
  int xmithdr;
  unsigned char bufhdr[1024];
  unsigned int bufhdridx;

  unsigned int txbytes;
};


struct CLIENT_INFO {
  //enum PROTO_TYPE protoType;
  CB_ONRCVDATA cbOnRcvData;
  //char userAgent[128];
  //char mediaId[128];
};

typedef int (* CB_INIT_USERDATA) (void *, void *);
//typedef void (* CB_CLOSE_USERDATA) (void *);

typedef struct PROXY_CONNECTION {
  int                    *p_exit; 

  CB_INIT_USERDATA    cbInitUserData;
  //CB_CLOSE_USERDATA    cbCloseUserData;
  void *pCbUserData;

  struct CLIENT_INFO      client;
  SOCKET_DESCR_T         *pAddrRemote;
  SOCKET_DESCR_T         *pAddrClient;
  struct HDR_STATE        cli_header;
  struct HDR_STATE        rem_header;

} PROXY_CONNECTION_T;

static const char DESCR_CLIENT[] = "client";
static const char DESCR_REMOTE[] = "remote";


static int connect_remote(SOCKET_DESCR_T *pAddr) {
  int rc = 0;
  char buf[SAFE_INET_NTOA_LEN_MAX];

  if(!pAddr) {
    return -1;
  } else if(!INET_ADDR_VALID(pAddr->sa) || htons(INET_PORT(pAddr->sa)) <= 0) {
    LOG(X_ERROR("Remote destination not configured"));
    return -1;
  }

  if((NETIOSOCK_FD(pAddr->netsocket) = net_opensocket(SOCK_STREAM, 
      SOCK_RCVBUFSZ_DEFAULT, 0, NULL)) == INVALID_SOCKET) {
    return -1;
  }

  if((rc = net_connect(NETIOSOCK_FD(pAddr->netsocket), (const struct sockaddr *) &pAddr->sa)) < 0) {
    netio_closesocket(&pAddr->netsocket);
    return rc;
  }

  LOG(X_DEBUG("Connected to remote source %s:%d"),
              FORMAT_NETADDR(pAddr->sa, buf, sizeof(buf)), ntohs(INET_PORT(pAddr->sa)));

  return 0;
}

static int sendbytes(SOCKET_DESCR_T *pAddrDst, PROXY_CONNECTION_T *pProxy,
                     const unsigned char *pBuf, unsigned int len) {
  int rc;

  if(net_setsocknonblock(NETIOSOCK_FD(pAddrDst->netsocket), 0) < 0) {
    return -1;
  }

  rc = netio_send(&pAddrDst->netsocket, (const struct sockaddr *) &pAddrDst->sa, pBuf, len);

  if(rc > 0) {
    if(pAddrDst == pProxy->pAddrClient) {
     pProxy->cli_header.txbytes += rc;
    } else {
     pProxy->rem_header.txbytes += rc;
    }
  }

  if(net_setsocknonblock(NETIOSOCK_FD(pAddrDst->netsocket), 1) < 0) {
    return -1;
  }

  return rc;
}

static int findHdrEnd(struct HDR_STATE *pHdr,
                      const unsigned char *pData, unsigned int len) {
  unsigned int idx;
  static unsigned char seqhdrend[] = "\r\n\r\n";

  for(idx = 0; idx < len; idx++) {

    if(pData[idx] == seqhdrend[pHdr->seqmatchidx]) {
      pHdr->seqmatchidx++;
    } else if(pHdr->seqmatchidx > 0) {
      pHdr->seqmatchidx = 0;
    }

    pHdr->bufhdr[pHdr->bufhdridx++] = pData[idx];

    if(pHdr->seqmatchidx >= 4) {
      return idx + 1;
    }

    if(pHdr->bufhdridx >= sizeof(pHdr->bufhdr)) {
      LOG(X_ERROR("No header end found after %d bytes"), pHdr->bufhdridx);
      return -1;
    }

  }

  return 0;
}

static int onRcvHeaderHttp(struct HDR_STATE *pHdr,
                         unsigned char **ppData, unsigned int *plen) {

  int rc = 0;

  if((rc = findHdrEnd(pHdr, *ppData, *plen)) < 0) {
    return rc;
  }

  if(rc == 0) {
    rc = *plen;
  } else {
    pHdr->havehdr = 1;
  }

  (*ppData) += rc;
  (*plen) -= rc;

  return rc;
}

/*
static int parseClientHttpHdr(const struct HDR_STATE *pHdr) {
  int rc = 0;
  HTTP_REQ_T req;
  const char *p;

  memset(&req, 0, sizeof(req));

  if(http_parse_req_uri((const char *) pHdr->bufhdr, pHdr->bufhdridx, &req, 0) == NULL) {
    return -1;
  }

  fprintf(stderr, "meth:'%s', ver:'%s', url:'%s', puri:'%s'\n", req.method, req.version, req.url, req.puri);

  return rc;
}
*/

static int cbOnRcvDataRtsp(void *pArg, int idxSrc,
                           const unsigned char *pDataArg, unsigned int len) {

  PROXY_CONNECTION_T *pProxy = (PROXY_CONNECTION_T *) pArg;
  unsigned char *pData = (unsigned char *) pDataArg;
  int rc;
  HTTP_REQ_T req;
  struct HDR_STATE *pHdrRd;

  SOCKET_DESCR_T *pAddrWr;

  if(idxSrc == 0) {
    pAddrWr = pProxy->pAddrRemote;
    pHdrRd = &pProxy->cli_header;
  } else {
    pAddrWr = pProxy->pAddrClient;
    pHdrRd = &pProxy->rem_header;
  }

  //fprintf(stderr, "rcv %d idxSrc:%d %s browser\n", len, idxSrc, idxSrc==0 ? "FROM" : "TO");

  if(idxSrc == 0 && !pHdrRd->havehdr) {

    if((rc = onRcvHeaderHttp(pHdrRd, &pData, &len)) < 0) {
      return -1;
    }

    if(pHdrRd->havehdr) {
      pHdrRd->xmithdr = 1;
      memset(&req, 0, sizeof(req));

      if(http_parse_req_uri((const char *) pHdrRd->bufhdr, pHdrRd->bufhdridx, 
                            &req, 0) == NULL) {
        return -1;
      }

      //
      // Call the user callback to init pProxy->pAddrRemote destination 
      //
      if(pProxy->cbInitUserData) {
        if(pProxy->cbInitUserData(pProxy, &req) < 0) {
          return -1;
        }
      }
    }

    if(pHdrRd->havehdr && idxSrc == 0 && NETIOSOCK_FD(pProxy->pAddrRemote->netsocket) <= 0) {
      if((rc = connect_remote(pProxy->pAddrRemote)) < 0) {
        return -1;
      }
    }
  }


  if(pAddrWr != NULL && NETIOSOCK_FD(pAddrWr->netsocket) > 0 && 
     (idxSrc == 1 || pHdrRd->havehdr)) {

    if(pHdrRd->xmithdr && pHdrRd->bufhdridx > 0) {

//  fprintf(stderr, "SEND %d\n", pHdrRd->bufhdridx);
      if(sendbytes(pAddrWr, pProxy, pHdrRd->bufhdr, pHdrRd->bufhdridx) < 0) {
        return -1;
      }

      pHdrRd->xmithdr = 0;
      pHdrRd->bufhdridx = 0;
    }

//if(len>0) fprintf(stderr, "SEND %d\n", len);
    if(len > 0 && sendbytes(pAddrWr, pProxy, pData, len) < 0) {
      return -1;
    }
  }

  return len;
}


static int cbOnRcvDataRtmp(void *pArg, int idxSrc,
                           const unsigned char *pData, unsigned int len) {

  PROXY_CONNECTION_T *pProxy = (PROXY_CONNECTION_T *) pArg;
  SOCKET_DESCR_T *pAddrWr;
  int rc = 0;

  if(idxSrc == 0) {
    pAddrWr = pProxy->pAddrRemote;
  } else {
    pAddrWr = pProxy->pAddrClient;
  }

  if(idxSrc == 0 && NETIOSOCK_FD(pProxy->pAddrRemote->netsocket) <= 0) {

    if(!INET_ADDR_VALID(pProxy->pAddrRemote->sa) || htons(INET_PORT(pProxy->pAddrRemote->sa)) <= 0) {
      LOG(X_ERROR("Remote RTMP destination not configured"));
      return -1;
    }
  }

  if((rc = sendbytes(pAddrWr, pProxy, pData, len)) < 0) {
    return -1;
  }

  return rc;
}



static int proxyloop(PROXY_CONNECTION_T *pProxy) {

  int idx;
  int fdhighest;
  int ircv;
  int val;
  fd_set rdset;
  fd_set wrset;
  struct timeval tv;
  unsigned char buf[16384];
  SOCKET_DESCR_T *pAddrSet[2];

  pAddrSet[0] = pProxy->pAddrClient;
  pAddrSet[1] = pProxy->pAddrRemote;

  if(net_setsocknonblock(NETIOSOCK_FD(pAddrSet[0]->netsocket), 1) < 0) {
    return -1;
  }

  while(*pProxy->p_exit == 0) {

    idx = 0;
    FD_ZERO(&rdset);
    if(NETIOSOCK_FD(pAddrSet[0]->netsocket) > 0) {
      FD_SET(NETIOSOCK_FD(pAddrSet[0]->netsocket), &rdset);
      idx++;
    }
    if(NETIOSOCK_FD(pAddrSet[1]->netsocket) > 0) {
      FD_SET(NETIOSOCK_FD(pAddrSet[1]->netsocket), &rdset);
      idx++;
    }
    if(idx == 0) {
      break;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    if(NETIOSOCK_FD(pAddrSet[1]->netsocket) > 0 && NETIOSOCK_FD(pAddrSet[1]->netsocket) > NETIOSOCK_FD(pAddrSet[0]->netsocket)) {
      fdhighest = NETIOSOCK_FD(pAddrSet[1]->netsocket) + 1;
    } else {
      fdhighest = NETIOSOCK_FD(pAddrSet[0]->netsocket) + 1;
    }

    if((val = select(fdhighest, &rdset, NULL, NULL, &tv)) > 0) {

      //fprintf(stderr, "[%u] select returned: %d\n", pProxy->id, val);
      for(idx = 0; idx < 2; idx++) {
        if(FD_ISSET(NETIOSOCK_FD(pAddrSet[idx]->netsocket), &rdset)) {

          //fprintf(stderr, "%d is set\n", pAddrSet[idx]->netsocket.sock);
          //if((ircv = recv(pAddrSet[idx]->netsocket.sock, buf, sizeof(buf), 0)) <= 0) {
          if((ircv = netio_recv(&pAddrSet[idx]->netsocket, NULL, buf, sizeof(buf))) <= 0) {
            LOG(X_ERROR("Proxy failed to read from %s %s:%d"),
              (idx == 0 ? DESCR_CLIENT : DESCR_REMOTE),
              FORMAT_NETADDR(pAddrSet[idx]->sa, (char *)buf, sizeof(buf)), 
              htons(INET_PORT(pAddrSet[idx]->sa)));
            return -1;
          }

          //if(pProxy->client.cbOnRcvData == NULL) {
          //  setCbRcvData(pProxy, buf, ircv);
          //}

          if(pProxy->client.cbOnRcvData == NULL) {
            return -1;
          }

          if(pProxy->client.cbOnRcvData(pProxy, idx, buf, ircv) < 0) {
            return -1;
          }

        }
      }

    } else if(val == 0) {
      //fprintf(stderr, "[%u] select timeout returned: %d\n", pProxy->id, val);
    } else {
      LOG(X_ERROR("Proxy select readers returned: %d"), val);
      break;
    }


    //
    // Check writeability (if remote end has been closed)
    // 
    idx = 0;
    FD_ZERO(&wrset);
    if(NETIOSOCK_FD(pAddrSet[0]->netsocket) > 0) {
      FD_SET(NETIOSOCK_FD(pAddrSet[0]->netsocket), &wrset);
      idx++;
    }
    if(NETIOSOCK_FD(pAddrSet[1]->netsocket) > 0) {
      FD_SET(NETIOSOCK_FD(pAddrSet[1]->netsocket) , &wrset);
      idx++;
    }
    if(idx == 0) {
      break;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if((val = select(fdhighest, NULL, &wrset, NULL, &tv)) < 0) {
      LOG(X_ERROR("Proxy select writers returned: %d"), val);
      break;
    }

  }

  return 0;
}

static int get_remote_conn(const char *pvirtRsrc, 
                           SRV_MGR_CONN_T *pConn,
                           struct sockaddr_storage *psaout, 
                           enum STREAM_METHOD streamMethod) {
  int rc = 0;
  META_FILE_T metaFile;
  SRVMEDIA_RSRC_T mediaRsrc;
  MEDIA_DESCRIPTION_T mediaDescr;
  SYS_PROC_T proc;
  STREAM_DEVICE_T devtype;
  char tmp[128];
  //char virtRsrc[VSX_MAX_PATH_LEN];
  int port;
  int startProc = 0;
  char *proxystr = NULL;
  struct stat st;
  size_t sz = 0;
  size_t sz2;

  if(!pvirtRsrc || pvirtRsrc[0] == '\0' || !srv_ctrl_islegal_fpath(pvirtRsrc)) {
    LOG(X_ERROR("Invalid media resource request '%s'"), pvirtRsrc ? pvirtRsrc : "");
    return -1;
  }

  switch(streamMethod) {
    case STREAM_METHOD_RTMP:
      proxystr = metaFile.rtmpproxy;
      break;
    case STREAM_METHOD_RTSP:
    case STREAM_METHOD_RTSP_INTERLEAVED:
    case STREAM_METHOD_RTSP_HTTP:
      proxystr = metaFile.rtspproxy;
      break;
    case STREAM_METHOD_MKVLIVE:
    case STREAM_METHOD_FLVLIVE:
    case STREAM_METHOD_TSLIVE:
      proxystr = metaFile.httpproxy;
      break;
    default:
      return -1;
  }

  memset(&metaFile, 0, sizeof(metaFile));
  memset(&mediaRsrc, 0, sizeof(mediaRsrc));
  memset(&mediaDescr, 0, sizeof(mediaDescr));
  memset(&devtype, 0, sizeof(devtype));

  mediadb_prepend_dir(pConn->conn.pCfg->pMediaDb->mediaDir, pvirtRsrc,
                      mediaRsrc.filepath, sizeof(mediaRsrc.filepath));
  if((sz = strlen(mediaRsrc.filepath)) >= (sz2 = strlen(pvirtRsrc))) {
    sz = sz - sz2; 
  }
  //mediaRsrc.puri = &mediaRsrc.filepath[sz];
  strncpy(mediaRsrc.virtRsrc, &mediaRsrc.filepath[sz], sizeof(mediaRsrc.virtRsrc) - 1);
  //mediaRsrc.pvirtRsrc = virtRsrc;
  mediaDescr.type = MEDIA_FILE_TYPE_SDP;
  memset(devtype.methods, 0, sizeof(devtype.methods));
  devtype.devtype = STREAM_DEVICE_TYPE_UNKNOWN;
  strncpy(devtype.name, STREAM_DEVICE_TYPE_UNKNOWN_STR, sizeof(devtype.name) - 1);

  //
  // Will append any extension (.sdp) if the resource is reference via a 'short' link
  //
  srv_ctrl_file_find_wext(mediaRsrc.filepath);

  //
  // Check the resource metafile for any resource specific configuration 
  //
  rc = srvmgr_check_metafile(&metaFile, &mediaRsrc, NULL, NULL);
  metafile_close(&metaFile);

  //
  // If the proxy port is not set in the metafile, treat this proxy request
  // as if it were an HTTP request for the requested resource and
  // start the stream specific process if necessary.
  // 
  if(proxystr[0] == '\0') {

    if(fileops_stat(mediaRsrc.filepath, &st) != 0) {
      LOG(X_ERROR("Resource for proxy session does not exist '%s'"), mediaRsrc.filepath);
      return -1;
    } else {

      memset(&proc, 0, sizeof(proc));

      //mediaRsrc.puri = (char *) pvirtRsrc;
      //TODO: set the media action to live or not...
      if((rc = srvmgr_check_start_proc(pConn, &mediaRsrc, &mediaDescr,
                          &proc, &devtype, MEDIA_ACTION_UNKNOWN, STREAM_METHOD_UNKNOWN, &startProc)) < 0) {
                          //&proc, &devtype, MEDIA_ACTION_UNKNOWN, streamMethod, &startProc)) < 0) {
        LOG(X_ERROR("Unable to %s process for '%s'"), 
            (startProc ? "start" : "find"), mediaRsrc.filepath);
        return -1;
      }

      switch(streamMethod) {
        case STREAM_METHOD_RTMP:
          port = MGR_GET_PORT_RTMP(proc.startPort);
          break;
        case STREAM_METHOD_RTSP:
        case STREAM_METHOD_RTSP_INTERLEAVED:
        case STREAM_METHOD_RTSP_HTTP:
          port = MGR_GET_PORT_RTSP(proc.startPort);
          break;
        case STREAM_METHOD_MKVLIVE:
        case STREAM_METHOD_FLVLIVE:
        case STREAM_METHOD_TSLIVE:
        default:
          port = MGR_GET_PORT_HTTP(proc.startPort);
          break;
      }
      snprintf(proxystr, META_FILE_PROXY_STR_LEN, "127.0.0.1:%d", port);

    }
  }

  //
  // Fill out the proxy destination and port
  //
  if(proxystr[0] == '\0') {
    LOG(X_ERROR("No remote proxy destination set for '%s'"), mediaRsrc.filepath);
    return -1;
  }

  if((rc = capture_getdestFromStr(proxystr, psaout, NULL, NULL, 0)) < 0) {
    return rc;
  }

  LOG(X_DEBUG("Resource '%s' proxied to '%s' (%s:%d)"), mediaRsrc.filepath, proxystr, 
           INET_NTOP(*psaout, tmp, sizeof(tmp)), htons(PINET_PORT(psaout)));

  return rc;
}


int initUserDataRtsp(void *pProxyArg, void *pArg) {
  int rc = 0;
  PROXY_CONNECTION_T *pProxy = (PROXY_CONNECTION_T *) pProxyArg;
  HTTP_REQ_T *pHttpReq = (HTTP_REQ_T *) pArg;
  SRV_MGR_CONN_T *pConn;
  const char *virtRsrc;

  if(!pProxy || !pProxy->pCbUserData || !pHttpReq || !pHttpReq->puri) {
    return -1;
  }

  pConn = (SRV_MGR_CONN_T *) pProxy->pCbUserData;
  virtRsrc = pHttpReq->puri;
  while(*virtRsrc == '/') {
    virtRsrc++;
  }

  //fprintf(stderr, "CALLING get_remote_conn url:'%s' virtRsrc:'%s'\n", pHttpReq->url, virtRsrc);
  if((rc = get_remote_conn(virtRsrc, pConn,  &pProxy->pAddrRemote->sa, STREAM_METHOD_RTSP)) < 0) {
    return rc;
  }

  return rc;
}

int initUserDataHttp(void *pProxyArg, void *pArg) {
  int rc = 0;
  PROXY_CONNECTION_T *pProxy = (PROXY_CONNECTION_T *) pProxyArg;
  HTTP_REQ_T *pHttpReq = (HTTP_REQ_T *) pArg;
  enum STREAM_METHOD method = STREAM_METHOD_INVALID;
  size_t sz;
  SRV_MGR_CONN_T *pConn;
  const char *virtRsrc;

  if(!pProxy || !pProxy->pCbUserData || !pHttpReq || !pHttpReq->puri) {
    return -1;
  }

  pConn = (SRV_MGR_CONN_T *) pProxy->pCbUserData;

  if(!strncasecmp(pHttpReq->puri, VSX_FLVLIVE_URL, strlen(VSX_FLVLIVE_URL))) {
    sz = strlen(VSX_FLVLIVE_URL);
    method = STREAM_METHOD_FLVLIVE;
  } else if(!strncasecmp(pHttpReq->puri, VSX_MKVLIVE_URL, strlen(VSX_MKVLIVE_URL))) {
    sz = strlen(VSX_MKVLIVE_URL);
    method = STREAM_METHOD_MKVLIVE;
  } else if(!strncasecmp(pHttpReq->puri, VSX_TSLIVE_URL, strlen(VSX_TSLIVE_URL))) {
    sz = strlen(VSX_TSLIVE_URL);
    method = STREAM_METHOD_TSLIVE;
  } else {
    LOG(X_ERROR("Invalid HTTP proxy URI: '%s'"), pHttpReq->puri);
    return -1;
  } 

  virtRsrc = &pHttpReq->puri[sz];
  while(*virtRsrc == '/') {
    virtRsrc++;
  }

  //fprintf(stderr, "CALLING HTTP get_remote_conn url:'%s' virtRsrc:'%s'\n", pHttpReq->url, virtRsrc);
  if((rc = get_remote_conn(virtRsrc, pConn, &pProxy->pAddrRemote->sa, method)) < 0) {
    return rc;
  }

  return rc;

}













static void mgr_rtmp_close(RTMP_CTXT_T *pRtmp) {

  rtmp_parse_close(pRtmp);

}

static int mgr_rtmp_init(RTMP_CTXT_T *pRtmp) {
  int rc;

  if((rc = rtmp_parse_init(pRtmp, 0x2000, 0x2000)) < 0) {
    return rc;
  }

  pRtmp->state = RTMP_STATE_INVALID;
  pRtmp->cbRead = rtmp_cbReadDataNet;
  pRtmp->pCbData = pRtmp;

  // Default values
  //pRtmp->advObjEncoding = 1.0;
  pRtmp->advObjEncoding = 3.0;
  //pRtmp->advObjEncoding = 0;
  pRtmp->advCapabilities = 31.0;

  return 0;
}

static int mgr_rtmp_connect(RTMP_CTXT_CLIENT_T *pRtmp) {
  int rc = 0;
  unsigned int lenData;

  if((rc = rtmp_create_connect(&pRtmp->ctxt, &pRtmp->client)) < 0) {
    return rc;
  }
  //avc_dumpHex(stderr, pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx, 1);

  lenData = pRtmp->ctxt.out.idx - 12;
  pRtmp->ctxt.out.idx = 12;
  if((rc = rtmp_send_chunkdata(&pRtmp->ctxt, RTMP_STREAM_IDX_INVOKE,
                               &pRtmp->ctxt.out.buf[12], lenData, 0)) < 0) {
    return rc;
  }

  pRtmp->ctxt.out.idx = 0;

  return rc;
}


static int mgr_rtmp_handle_conn(RTMP_CTXT_T *pRtmp, SRV_MGR_CONN_T *pConn) {
  SOCKET_DESCR_T sdSrv;
  RTMP_CTXT_CLIENT_T rtmpCli;
  PROXY_CONNECTION_T proxyConn;
  char path[RTMP_PARAM_LEN_MAX];
  char tmp[128];
  int rc = 0;

  pRtmp->state = RTMP_STATE_CLI_START;

  //
  // Accept handshake
  //
  if((rc = rtmp_handshake_srv(pRtmp)) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
        FORMAT_NETADDR(pRtmp->pSd->sa, path, sizeof(path)), ntohs(INET_PORT(pRtmp->pSd->sa)));
    return rc;
  }

 
  //
  // Read connect packet
  //
  do {

    if((rc = rtmp_parse_readpkt_full(pRtmp, 1, NULL)) < 0 ||
      (pRtmp->state != RTMP_STATE_CLI_CONNECT && 
       pRtmp->pkt[pRtmp->streamIdx].hdr.contentType != RTMP_CONTENT_TYPE_CHUNKSZ)) {
      LOG(X_ERROR("Failed to read rtmp connect request.  State: %d"), pRtmp->state);
      return rc;
    }

  } while(pRtmp->state != RTMP_STATE_CLI_CONNECT);

  memset(&sdSrv, 0, sizeof(sdSrv));

  // Expecting app <dir>/<resource name>
  strncpy(path, pRtmp->connect.app, sizeof(path));
  //
  // Strip RTMP filename from 'app' string only leaving the FMS connect param
  //
  //if((sz = mediadb_getdirlen(path)) > 0) {
  //  path[sz - 1] = '\0';
  //}

  //
  // Use the 'app' parameter as the unique resource name
  //
  if((rc = get_remote_conn(path, pConn, &sdSrv.sa, STREAM_METHOD_RTMP)) < 0) {
    LOG(X_ERROR("Invalid RTMP media resource request path: '%s', app: '%s'"), path, pRtmp->connect.app);
    return rc;
  }

  LOG(X_INFO("RTMP media resource request path: '%s', app: '%s' at %s:%d"), path, pRtmp->connect.app, 
              INET_NTOP(sdSrv, tmp, sizeof(tmp)), htons(INET_PORT(sdSrv)));

  if(connect_remote(&sdSrv) < 0) {
    return -1;
  }

  memset(&rtmpCli, 0, sizeof(rtmpCli));
  mgr_rtmp_init(&rtmpCli.ctxt);
  rtmpCli.ctxt.pSd = &sdSrv;
  rtmpCli.ctxt.connect.objEncoding = pRtmp->connect.objEncoding;
  rtmpCli.ctxt.connect.capabilities = pRtmp->connect.capabilities;
  rtmpCli.ctxt.chunkSzIn = pRtmp->chunkSzIn;

  if(rc >= 0 && (rc = rtmp_handshake_cli(&rtmpCli.ctxt, 1)) < 0) {
    LOG(X_ERROR("RTMP handshake with proxy failed for %s:%d"),
      FORMAT_NETADDR(rtmpCli.ctxt.pSd->sa, path, sizeof(path)), ntohs(INET_PORT(rtmpCli.ctxt.pSd->sa)));
  }
  if(rc >= 0 && (rc = mgr_rtmp_connect(&rtmpCli))) {

  }

  if(rc >= 0) {

    memset(&proxyConn, 0, sizeof(proxyConn));
    proxyConn.p_exit = &g_proc_exit;
    proxyConn.pAddrRemote = &sdSrv;
    proxyConn.pAddrClient =  pRtmp->pSd;
    proxyConn.client.cbOnRcvData = cbOnRcvDataRtmp;

    proxyloop(&proxyConn);

    LOG(X_DEBUG("RTMP proxy loop done with %u bytes to client, %u bytes to server"), 
       proxyConn.cli_header.txbytes, proxyConn.rem_header.txbytes);

  }

  mgr_rtmp_close(&rtmpCli.ctxt);
  netio_closesocket(&sdSrv.netsocket);

  return rc;
}


void srvmgr_rtmpclient_proc(void *pfuncarg) {

  SRV_MGR_CONN_T *pConn = (SRV_MGR_CONN_T *) pfuncarg;
  char buf[SAFE_INET_NTOA_LEN_MAX];
  RTMP_CTXT_T rtmpCtxt;

  LOG(X_DEBUG("Handling RTMP connection from %s:%d"), 
       FORMAT_NETADDR(pConn->conn.sd.sa, buf, sizeof(buf)), ntohs(INET_PORT(pConn->conn.sd.sa)));

  memset(&rtmpCtxt, 0, sizeof(rtmpCtxt));

  mgr_rtmp_init(&rtmpCtxt);
  rtmpCtxt.pSd = &pConn->conn.sd;

  mgr_rtmp_handle_conn(&rtmpCtxt, pConn);

  mgr_rtmp_close(&rtmpCtxt);
  netio_closesocket(&pConn->conn.sd.netsocket);

  LOG(X_DEBUG("RTMP Connection ended %s:%d"), 
      FORMAT_NETADDR(pConn->conn.sd.sa, buf, sizeof(buf)), ntohs(INET_PORT(pConn->conn.sd.sa)));

}

static int mgr_rtsp_handle_conn(SRV_MGR_CONN_T *pConn) {
  SOCKET_DESCR_T sdSrv;
  PROXY_CONNECTION_T proxyConn;

  memset(&sdSrv, 0, sizeof(sdSrv));
  memset(&proxyConn, 0, sizeof(proxyConn));
  proxyConn.p_exit = &g_proc_exit;
  proxyConn.pAddrRemote = &sdSrv;
  proxyConn.pAddrClient = &pConn->conn.sd;
  proxyConn.client.cbOnRcvData = cbOnRcvDataRtsp;
  proxyConn.pCbUserData = pConn;
  proxyConn.cbInitUserData = initUserDataRtsp;

  proxyloop(&proxyConn);

  LOG(X_DEBUG("RTSP proxy loop done with %u bytes to client, %u bytes to server"), 
       proxyConn.cli_header.txbytes, proxyConn.rem_header.txbytes);

  netio_closesocket(&sdSrv.netsocket);

  return 0;
}

static int mgr_http_handle_conn(SRV_MGR_CONN_T *pConn) {
  SOCKET_DESCR_T sdSrv;
  PROXY_CONNECTION_T proxyConn;

  memset(&sdSrv, 0, sizeof(sdSrv));
  memset(&proxyConn, 0, sizeof(proxyConn));
  proxyConn.p_exit = &g_proc_exit;
  proxyConn.pAddrRemote = &sdSrv;
  proxyConn.pAddrClient = &pConn->conn.sd;
  proxyConn.client.cbOnRcvData = cbOnRcvDataRtsp;
  proxyConn.pCbUserData = pConn;
  proxyConn.cbInitUserData = initUserDataHttp;

  proxyloop(&proxyConn);

  LOG(X_DEBUG("HTTP proxy loop done with %u bytes to client, %u bytes to server"), 
       proxyConn.cli_header.txbytes, proxyConn.rem_header.txbytes);

  netio_closesocket(&sdSrv.netsocket);

  return 0;
}


void srvmgr_rtspclient_proc(void *pfuncarg) {

  SRV_MGR_CONN_T *pConn = (SRV_MGR_CONN_T *) pfuncarg;
  char tmp[128];

  LOG(X_DEBUG("Handling RTSP connection from %s:%d"), 
       FORMAT_NETADDR(pConn->conn.sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->conn.sd.sa)));

  mgr_rtsp_handle_conn(pConn);

  netio_closesocket(&pConn->conn.sd.netsocket);

  LOG(X_DEBUG("RTSP Connection ended %s:%d"), 
      FORMAT_NETADDR(pConn->conn.sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->conn.sd.sa)));

}

void srvmgr_httpclient_proc(void *pfuncarg) {

  SRV_MGR_CONN_T *pConn = (SRV_MGR_CONN_T *) pfuncarg;
  char tmp[128];

  LOG(X_DEBUG("Handling HTTP (proxy) connection from %s:%d"), 
       FORMAT_NETADDR(pConn->conn.sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->conn.sd.sa)));

  mgr_http_handle_conn(pConn);

  netio_closesocket(&pConn->conn.sd.netsocket);

  LOG(X_DEBUG("HTTP (proxy) Connection ended %s:%d"), 
      FORMAT_NETADDR(pConn->conn.sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->conn.sd.sa)));

}


void srvmgr_rtmpproxy_proc(void *pArg) {
  NETIO_SOCK_T netsocksrv;  
  int rc;
  struct sockaddr_storage sa;
  char tmp[128];
  SRV_MGR_LISTENER_CFG_T *pSrvListen = (SRV_MGR_LISTENER_CFG_T *) pArg;

  pSrvListen->listenCfg.urlCapabilities = URL_CAP_RTMPLIVE;

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pSrvListen->listenCfg.sa, sizeof(sa));
  netsocksrv.flags = pSrvListen->listenCfg.netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, 10)) == INVALID_SOCKET) {
    return;
  }

  pSrvListen->listenCfg.pnetsockSrv = &netsocksrv;

  LOG(X_INFO("Listening for RTMP (proxy) requests on rtmp%s://%s:%d"),
     ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "s" : ""),
     FORMAT_NETADDR(sa, tmp, sizeof(tmp)), ntohs(INET_PORT(sa)));

  //
  // Service any client connections on the http listening port
  //
  rc = srvlisten_loop(&pSrvListen->listenCfg, srvmgr_rtmpclient_proc);

  LOG(X_WARNING("RTMP%s %s:%d listener thread exiting with code: %d"),
               ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "S" : ""),
               FORMAT_NETADDR(sa, tmp, sizeof(tmp)), ntohs(INET_PORT(sa)), rc);

  pSrvListen->listenCfg.pnetsockSrv = NULL;
  netio_closesocket(&netsocksrv);

}

void srvmgr_rtspproxy_proc(void *pArg) {
  NETIO_SOCK_T netsocksrv;
  int rc;
  struct sockaddr_storage sa;
  char tmp[128];
  SRV_MGR_LISTENER_CFG_T *pSrvListen = (SRV_MGR_LISTENER_CFG_T *) pArg;

  pSrvListen->listenCfg.urlCapabilities = URL_CAP_RTSPLIVE;

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pSrvListen->listenCfg.sa, sizeof(sa));
  netsocksrv.flags = pSrvListen->listenCfg.netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, 10)) == INVALID_SOCKET) {
    return;
  }

  pSrvListen->listenCfg.pnetsockSrv = &netsocksrv;

  LOG(X_INFO("Listening for RTSP (proxy) requests on rtmp%s://%s:%d"),
     ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "s" : ""),
     FORMAT_NETADDR(sa, tmp, sizeof(tmp)), ntohs(INET_PORT(sa)));

  //
  // Service any client connections on the http listening port
  //
  rc = srvlisten_loop(&pSrvListen->listenCfg, srvmgr_rtspclient_proc);

  LOG(X_WARNING("RTSP%s %s:%d listener thread exiting with code: %d"),
               ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "S" : ""),
               FORMAT_NETADDR(sa, tmp, sizeof(tmp)), ntohs(INET_PORT(sa)), rc);

  pSrvListen->listenCfg.pnetsockSrv = NULL;
  netio_closesocket(&netsocksrv);

}

void srvmgr_httpproxy_proc(void *pArg) {
  NETIO_SOCK_T netsocksrv;
  int rc;
  struct sockaddr_storage sa;
  char tmp[128];
  SRV_MGR_LISTENER_CFG_T *pSrvListen = (SRV_MGR_LISTENER_CFG_T *) pArg;

  pSrvListen->listenCfg.urlCapabilities = URL_CAP_FLVLIVE;

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pSrvListen->listenCfg.sa, sizeof(sa));
  netsocksrv.flags = pSrvListen->listenCfg.netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, 10)) == INVALID_SOCKET) {
    return;
  }

  pSrvListen->listenCfg.pnetsockSrv = &netsocksrv;

  LOG(X_INFO("Listening for HTTP (proxy) requests on "URL_HTTP_FMT_STR),
      URL_HTTP_FMT_ARGS2(&pSrvListen->listenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))),
            ntohs(INET_PORT(sa)));

  //
  // Service any client connections on the http listening port
  //
  rc = srvlisten_loop(&pSrvListen->listenCfg, srvmgr_httpclient_proc);

  LOG(X_WARNING("HTTP%s (proxy) %s:%d listener thread exiting with code: %d"),
               ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "S" : ""),
               FORMAT_NETADDR(sa, tmp, sizeof(tmp)), ntohs(INET_PORT(sa)), rc);

  pSrvListen->listenCfg.pnetsockSrv = NULL;
  netio_closesocket(&netsocksrv);

}

