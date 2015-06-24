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

typedef struct HTTPLIVE_CLIENT {
  PLAYLIST_M3U_T          pl;
  CAP_ASYNC_DESCR_T      *pCfg;
  CAPTURE_CBDATA_T       *pStreamsOut;
  CAPTURE_STREAM_T       *pStream;
  int                     insession;
  unsigned int            curidx;
  unsigned int            nextidx;
  pthread_mutex_t         mtx;
  NETIO_SOCK_T            netsock;
  struct sockaddr_in      sa;
  char                    hostbuf[CAPTURE_HTTP_HOSTBUF_MAXLEN];
  char                    uriprefix[256];
  int                     running;
} HTTPLIVE_CLIENT_T;

unsigned char *http_get_contentlen_start(HTTP_RESP_T *pHttpResp, 
                                    HTTP_PARSE_CTXT_T *pHdrCtxt,
                                    unsigned char *pbuf, unsigned int szbuf,
                                    int verifybufsz,
                                    unsigned int *pcontentLen) {
  const char *p;
  int contentLen = 0;
  unsigned int consumed = 0;
  unsigned char *pdata = NULL;

  if((p = conf_find_keyval(pHttpResp->hdrPairs, HTTP_HDR_CONTENT_LEN))) {
    contentLen = atoi(p);
  }

  if(contentLen <= 0) {
    LOG(X_ERROR("%s not found in response"), HTTP_HDR_CONTENT_LEN);
  } else if(verifybufsz && (unsigned int) contentLen >= szbuf) {
    LOG(X_ERROR("Input buffer size too small %d / %d"), szbuf, contentLen);
  } else if(pHdrCtxt->idxbuf > pHdrCtxt->hdrslen) {

    if((consumed = pHdrCtxt->idxbuf - pHdrCtxt->hdrslen) < szbuf) {
      if((char *) pbuf != pHdrCtxt->pbuf) {
        memcpy(pbuf, &pHdrCtxt->pbuf[pHdrCtxt->hdrslen], consumed);
        pdata = pbuf;
      } else {
        pdata = &pbuf[pHdrCtxt->hdrslen];
      }
    } else {
      LOG(X_ERROR("Input buffer size too small %d / %d"), szbuf, contentLen);
    }

  } else if(pHdrCtxt->idxbuf == pHdrCtxt->hdrslen) {
    pdata = pbuf;
  }

  if(pdata && pcontentLen) {
    *pcontentLen = contentLen;
  }

  return pdata;
}

static const char *get_m3u8(CAP_ASYNC_DESCR_T *pCfg,
                         const char *puri,
                         HTTP_RESP_T *pHttpResp,
                         HTTP_PARSE_CTXT_T *pHdrCtxt,
                         unsigned char *pbuf, unsigned int szbuf) {
  int sz = 0;
  struct timeval tv0, tv1;
  unsigned int consumed = 0;
  unsigned int contentLen = 0;
  unsigned int tmtms = 0;
  unsigned char *pdata = NULL;

  gettimeofday(&tv0, NULL);

  //fprintf(stderr, "GET M3U... puri:'%s', hdrslen:%d\n", puri, pHdrCtxt->hdrslen);

  if(pHdrCtxt->hdrslen == 0) {

    if((httpcli_gethdrs(pHdrCtxt, pHttpResp, &pCfg->pSockList->salist[0], puri,
          http_getConnTypeStr(HTTP_CONN_TYPE_CLOSE), 0, 0, pCfg->pcommon->addrsExtHost[0], NULL)) < 0) {
      return NULL;
    }

  }

  if((pdata = http_get_contentlen_start(pHttpResp, pHdrCtxt, pbuf, szbuf, 1, &contentLen))) {
    consumed = pHdrCtxt->idxbuf - pHdrCtxt->hdrslen;
  }

  if(pdata && net_setsocknonblock(NETIOSOCK_FD(pCfg->pSockList->netsockets[0]), 1) < 0) {
 
    pdata = NULL;
  }

//fprintf(stderr, "NET_RECV in m3u... %d < %d idxbuf:%d hdrslen:%d pdata:0x%x\n", consumed, contentLen, pHdrCtxt->idxbuf, pHdrCtxt->hdrslen, pdata);

  while(pdata && consumed < contentLen && !g_proc_exit) {

    if((sz = netio_recvnb(&pCfg->pSockList->netsockets[0],
                          (unsigned char *) &pdata[consumed],
                          contentLen - consumed, 500)) > 0) {
      consumed += sz;
    }
//fprintf(stderr, "NET REceiving... %d < %d, %d tmtms:%d\n", consumed, contentLen, sz, tmtms);
    gettimeofday(&tv1, NULL);
    if(tmtms > 0 && consumed < contentLen && TIME_TV_DIFF_MS(tv1, tv0) > (unsigned int) tmtms) {
      LOG(X_WARNING("HTTP %s:%d%s timeout %d ms exceeded"),
           inet_ntoa(pCfg->pSockList->salist[0].sin_addr),
           ntohs(pCfg->pSockList->salist[0].sin_port), puri, tmtms);
      pdata = NULL;
      break;
    }
  }

  if(pdata && contentLen > 0 && consumed >= contentLen) {
    pdata[consumed] = '\0';
  } else {
    pdata = NULL;
  }

  //fprintf(stderr, "GOT m3u...0x%x %d < %d\n", pdata, consumed, contentLen);avc_dumpHex(stderr, pdata, consumed, 1);
  return (const char *) pdata;
}

const char *http_get_doc(CAP_ASYNC_DESCR_T *pCfg,
                         const char *puri,
                         HTTP_RESP_T *pHttpResp,
                         HTTP_PARSE_CTXT_T *pHdrCtxt,
                         unsigned char *pbuf, unsigned int szbuf) {

  if(!pCfg || !pCfg->pSockList || !puri || !pHttpResp || !pHdrCtxt || !pbuf) {
    return NULL;
  }

  return get_m3u8(pCfg, puri, pHttpResp, pHdrCtxt, pbuf, szbuf);
}

static int get_index(const char *path) {
  unsigned int idx = 0;
  char *p;
  char tmp[64];
  size_t sz, sz2;

  if((p = strchr(path, '?'))) {
    sz = p - path;
  } else {
    sz = strlen(path);
  }
  if(sz > 3 && !strncasecmp(&path[sz -3], ".ts", 3)) {
    sz -= 3;
    sz2 = sz;
    while(sz2 > 0 && path[sz2 - 1] >= '0' && path[sz2 - 1] <= '9') {
      sz2--;
    }
    sz = sz - sz2;
    if(sz > sizeof(tmp) - 1) {
      sz = sizeof(tmp) - 1;
    }
    strncpy(tmp, &path[sz2], sz);
    tmp[sz] = '\0';
    idx = atoi(tmp);

  } else {
    idx = -1;
  }

  //fprintf(stderr, "get_index:%d '%s'\n", idx, path);

  return idx;
}

static int find_lowest_idx(PLAYLIST_M3U_T *pl, char **pp) {
  int rc;

  if((rc = get_index(pl->pEntries[0].path)) >= 0) {
    if(pp) {
      *pp = pl->pEntries[0].path;
    }
  }

  return rc;
}

static int find_highest_idx(PLAYLIST_M3U_T *pl, char **pp) {
  int rc;
  unsigned int idx = 0;

  if(pl->numEntries > 0) {
    idx = pl->numEntries - 1;
  }

  if((rc = get_index(pl->pEntries[idx].path)) >= 0) {
    if(pp) {
      *pp = pl->pEntries[idx].path;
    }
  }

  return rc;
}

static const char *find_path(PLAYLIST_M3U_T *pl, unsigned int searchidx) {
  unsigned int idx;

  for(idx = 0; idx < pl->numEntries; idx++) {
    //fprintf(stderr, "find_path searchidx:%d, pl->numEntries:%d, path[%d]:'%s'\n",  searchidx, pl->numEntries, idx, pl->pEntries[idx].path);
    if(get_index(pl->pEntries[idx].path) == searchidx) {
      return pl->pEntries[idx].path;
    }
  }
  return NULL;
}

static int get_ts(HTTPLIVE_CLIENT_T *pClient, const char *puri) {
  int rc = 0;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T httpResp;
  const char *p = NULL;
  unsigned char *pdata;
  unsigned int contentLen = 0;
  char fulluri[256];
  unsigned char buf[RTP_JTBUF_PKT_BUFSZ_LOCAL + SOCKET_LIST_PREBUF_SZ];

  //LOG(X_DEBUG("Retrieving TS chunk '%s'"), puri);

  if(pClient->uriprefix[0] != '\0') {
    snprintf(fulluri, sizeof(fulluri), "%s%s", pClient->uriprefix, puri);
    puri = fulluri;
  }

  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  memset(&httpResp, 0, sizeof(httpResp));
  hdrCtxt.pnetsock = &pClient->netsock;
  hdrCtxt.pbuf = (char *) buf;
  hdrCtxt.szbuf = sizeof(buf);
  hdrCtxt.tmtms = 0;

  if((httpcli_gethdrs(&hdrCtxt, &httpResp, &pClient->sa, puri,
          http_getConnTypeStr(HTTP_CONN_TYPE_KEEPALIVE), 0, 0, pClient->hostbuf, NULL)) < 0) {
    return -1;
  }

  if(!(p = conf_find_keyval(httpResp.hdrPairs, HTTP_HDR_CONTENT_TYPE))) {
    LOG(X_ERROR("No "HTTP_HDR_CONTENT_TYPE" found in response"));
    return -1;
  } else if(!(strncasecmp(p, CONTENT_TYPE_MP2TS, strlen(CONTENT_TYPE_MP2TS)) &&
              strncasecmp(p, CONTENT_TYPE_OCTET_STREAM, strlen(CONTENT_TYPE_OCTET_STREAM)))) {
    LOG(X_ERROR("Unsupported %s: %s received in response for %s"),
               HTTP_HDR_CONTENT_TYPE, (p ? p : "<Not Found>"), puri);
    rc = -1;
  }

  if(!(pdata = http_get_contentlen_start(&httpResp, &hdrCtxt, buf, sizeof(buf), 
                                    0, &contentLen))) {
    rc = -1;
  }

  //fprintf(stderr, "TS contentlen:%d pdata:0x%x idxb:%d hdrslen:%d\n", contentLen, pdata, hdrCtxt.idxbuf, hdrCtxt.hdrslen);  

  if(rc >= 0 && pClient->pCfg->running == 0) {
    rc = http_recvloop(pClient->pCfg, &pClient->netsock, &pClient->sa, 
                       pClient->pStreamsOut, pClient->pStream, 
                       hdrCtxt.hdrslen, hdrCtxt.idxbuf - hdrCtxt.hdrslen, 
                       contentLen,
                       buf, sizeof(buf));
  }

  //fprintf(stderr, "TS DONE w rc:%d\n", rc);

  return rc;
}

static const char *get_uri_from_path(HTTPLIVE_CLIENT_T *pClient, const char *path) {

  const char *puri = NULL;

  if(!strncmp(path, "http://", 7) || !strncmp(path, "https://", 8)) {
    if(capture_getdestFromStr(path, &pClient->sa, &puri, 
                              pClient->hostbuf, HTTP_PORT_DEFAULT) < 0) {
      LOG(X_ERROR("Unable to parse httplive media uri '%s'"), path);
      return NULL;
    }    
  } else {
    memcpy(&pClient->sa, &pClient->pCfg->pSockList->salist[0], sizeof(pClient->sa));
    puri = path;
  }

  return puri;
}


static void httplive_mediaproc(void *pArg) {
  HTTPLIVE_CLIENT_T *pClient = (HTTPLIVE_CLIENT_T *) pArg;
  int rc = 0;
  const char *p;
  const char *puri = NULL;
  char path[VSX_MAX_PATH_LEN];

  pClient->running = 0;

  LOG(X_DEBUG("Starting HTTPLive client media thread"));

  path[sizeof(path) - 1] = '\0';

  while(rc >= 0 && pClient->pCfg->running == 0 && pClient->insession) {

    path[0] = '\0';

    pthread_mutex_lock(&pClient->mtx);

    if(pClient->nextidx > pClient->curidx) {
      LOG(X_WARNING("HTTPLive skipping playlist media chunk index %d - %d"),
                     pClient->curidx, pClient->nextidx - 1);
      pClient->curidx = pClient->nextidx;
      //pClient->nextidx++;
    }

    if((p = find_path(&pClient->pl, pClient->curidx))) {
      strncpy(path, p, sizeof(path) - 1);
    } else {

    }
    pthread_mutex_unlock(&pClient->mtx);

    if(!(puri = get_uri_from_path(pClient, path))) {
      rc = -1;
      break;
    }

    if(puri && puri[0] != '\0') {

      // TODO: create httplive retrieval flag forcing new socket... lame
      if(NETIOSOCK_FD(pClient->netsock) != INVALID_SOCKET && net_issockremotelyclosed(NETIOSOCK_FD(pClient->netsock), 1)) {
        LOG(X_DEBUG("HTTPLive media socket has been closed")); 
        netio_closesocket(&pClient->netsock);
      }

      if(NETIOSOCK_FD(pClient->netsock) == INVALID_SOCKET) {

      //fprintf(stderr, "----MEDIA GET for '%s' '%s'\n", path, puri);

        if((rc = httpcli_connect(&pClient->netsock, &pClient->sa, "HTTPLive media thread")) < 0) {
          break;
        }

      }

      //fprintf(stderr, "may call get_ts puri:'%s', idx:%d, next:%d\n", puri, pClient->curidx, pClient->nextidx);

      if((rc = get_ts(pClient, puri)) >= 0) {

        //fprintf(stderr, "HTTPLive ts retrieval '%s' returned %d\n", puri, rc);

        pthread_mutex_lock(&pClient->mtx);
        pClient->nextidx++;
        pClient->curidx = pClient->nextidx;
        pthread_mutex_unlock(&pClient->mtx);
      }

    } // end of if(puri && puri[0] != '\0' ...

    sleep(1);

  }

  netio_closesocket(&pClient->netsock);

  pClient->insession = 0;
  pClient->running = -1;

  LOG(X_DEBUG("HTTPLive media download thread exiting with code %d"), rc);
}

int http_gethttplive(CAP_ASYNC_DESCR_T *pCfg,
                         CAPTURE_CBDATA_T *pStreamsOut,
                         CAPTURE_STREAM_T *pStream,
                         const char *puri,
                         HTTP_RESP_T *pHttpResp,
                         HTTP_PARSE_CTXT_T *pHdrCtxt) {
  int rc = 0;
  char *path;
  const char *pbuf = pHdrCtxt->pbuf;
  unsigned int szbuf = pHdrCtxt->szbuf;
  const char *pm3ubuf;
  pthread_t ptd;
  pthread_attr_t attr;
  HTTPLIVE_CLIENT_T client;
  int highestIdx, lowestIdx, firstIdx;

  memset(&client, 0, sizeof(client));
  client.pCfg = pCfg;
  NETIOSOCK_FD(client.netsock) = INVALID_SOCKET;
  client.netsock.flags = pCfg->pSockList->netsockets[0].flags;
  client.pStreamsOut = pStreamsOut;
  client.pStream = pStream;
  client.running = -1;

  pthread_mutex_init(&client.mtx, NULL);

  do {

    //fprintf(stderr, "HTTP_GETHTTPLIVE sock: %d\n", NETIOSOCK_FD(pCfg->pSockList->netsockets[0]));

    if(NETIOSOCK_FD(pCfg->pSockList->netsockets[0]) == INVALID_SOCKET) {

//fprintf(stderr, "GOING TO CONNECT FOR M3U...\n");
      if((rc = httpcli_connect(&pCfg->pSockList->netsockets[0], &pCfg->pSockList->salist[0], 
                               "HTTPLive playlist thread")) < 0) {
        break;
      }

      memset(pHdrCtxt, 0, sizeof(HTTP_PARSE_CTXT_T));
      memset(pHttpResp, 0, sizeof(HTTP_RESP_T));
      pHdrCtxt->pnetsock = &pCfg->pSockList->netsockets[0];
      pHdrCtxt->pbuf = pbuf;
      pHdrCtxt->szbuf = szbuf;
      pHdrCtxt->tmtms = 0;
    }

    if((rc = mediadb_getdirlen(puri)) > 0) {
      if(rc >= sizeof(client.uriprefix)) {
        rc = sizeof(client.uriprefix) - 1;
      }
      memcpy(client.uriprefix, puri, rc);
    }

    if((pm3ubuf = get_m3u8(pCfg, puri, pHttpResp, pHdrCtxt, (unsigned char *) pbuf, 
                           szbuf))) {
      m3u_free(&client.pl, 0);
      memset(&client.pl, 0, sizeof(client.pl));

/*
pm3ubuf="#EXTM3U\r\n"
        "#EXT-X-VERSION:3\r\n"
        "#EXT-X-ALLOW-CACHE:NO\r\n"
        "#EXT-X-TARGETDURATION:14\r\n"
        "#EXT-X-MEDIA-SEQUENCE:2699\r\n"
        "#EXTINF:10,\r\n"
        "media_2699.ts?wowzasessionid=1144297750\r\n"
        "#EXTINF:10,\r\n"
        "media_2700.ts?wowzasessionid=1144297750\r\n"
        "#EXTINF:7,\r\n"
        "media_2701.ts?wowzasessionid=1144297750\r\n";
*/

      VSX_DEBUGLOG("Got m3u contents '%s'", pm3ubuf);
      //fprintf(stderr, "Got m3u contents '%s'\n", pm3ubuf);

      pthread_mutex_lock(&client.mtx);
      rc = m3u_create_buf(&client.pl, pm3ubuf);
      pthread_mutex_unlock(&client.mtx);

      if(rc > 0) {

        path = NULL;
        lowestIdx = find_lowest_idx(&client.pl, NULL);
        highestIdx = find_highest_idx(&client.pl, NULL);

        if(!client.insession) {

          if(highestIdx < 0) {
            LOG(X_WARNING("Unable to find httplive starting index from %s"), puri);
          } else {

            //
            // Some segmentors may write the last chunk to the playlist file even though
            // the chunk is still being appended on disk
            //
            //if(highestIdx > lowestIdx) {
            //  firstIdx = highestIdx - 1;
            //} else {
              firstIdx = highestIdx;
            //}

            client.curidx  = firstIdx;
            client.nextidx = client.curidx;
            client.insession = 1;

            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            client.running = 1;

            if(pthread_create(&ptd, &attr, (void *) httplive_mediaproc, 
                              &client) != 0) {
              LOG(X_ERROR("Unable to create httplive media download thread"));
              rc = -1; 
              client.running = -1;
            }
          }

        } else { // client.insession

          //fprintf(stderr, "check playlist falling behind curidx:%d nextidx:%d, lowestIdx:%d, highestIdx:%d\n", client.curidx, client.nextidx, lowestIdx, highestIdx);
          //
          // Prevent falling behind playlist, a warning will be printed in the ts get thread
          //
          pthread_mutex_lock(&client.mtx);
          if(client.nextidx < lowestIdx) {
            client.nextidx = lowestIdx;
          }
          pthread_mutex_unlock(&client.mtx);
        }

        //if(rc >= 0) {
        //  m3u_dump(&client.pl);
        //}

      }

    } else { // get_m3u8
      rc = -1;
    }

    netio_closesocket(&pCfg->pSockList->netsockets[0]);

    if(rc >= 0) {
//fprintf(stderr, "M3U SLEEPING FOR %d\n", client.pl.targetDuration);
      if(client.pl.targetDuration > 0) {
        sleep(client.pl.targetDuration);
      } else {
        sleep(9);
      }
    }

  } while(rc >= 0);

  client.insession = 0;

  while(client.running != -1) {
    usleep(20000);
  } 

  m3u_free(&client.pl, 0);
  pthread_mutex_destroy(&client.mtx);

  return rc;
}






























#endif // VSX_HAVE_CAPTURE
