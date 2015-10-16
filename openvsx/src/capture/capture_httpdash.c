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


typedef struct HTTPDASH_CLIENT {
  PLAYLIST_MPD_T          pl;
  CAP_ASYNC_DESCR_T      *pCfg;
  //CAP_HTTP_MP4_STREAM_T  *pCapHttpMp4Stream;
  int                     insession;
  NETIO_SOCK_T            netsock;
  struct sockaddr_storage sa;
  char                    hostbuf[CAPTURE_HTTP_HOSTBUF_MAXLEN];
  char                    uriprefix[256];
  int                     running;

  // Shared between playlist retrieval and media retrieval
  unsigned int            curidx;
  unsigned int            nextidx;
  pthread_mutex_t         mtx;
} HTTPDASH_CLIENT_T;

static int get_moof(HTTPDASH_CLIENT_T *pClient, const char *puri, const char *pathOut, unsigned int tmtms) {
  int rc = 0;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T httpResp;
  const char *p = NULL;
  unsigned char *pdata;
  unsigned int contentLen = 0;
  char fulluri[256];
  unsigned char buf[4096];

  //LOG(X_DEBUG("Retrieving DASH chunk '%s'"), puri);

  if(pClient->uriprefix[0] != '\0') {
    snprintf(fulluri, sizeof(fulluri), "%s%s", pClient->uriprefix, puri);
    puri = fulluri;
  }

  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  memset(&httpResp, 0, sizeof(httpResp));
  hdrCtxt.pnetsock = &pClient->netsock;
  hdrCtxt.pbuf = (char *) buf;
  hdrCtxt.szbuf = sizeof(buf);
  hdrCtxt.tmtms = tmtms;

  if((httpcli_gethdrs(&hdrCtxt, &httpResp, (const struct sockaddr *) &pClient->sa, puri,
          http_getConnTypeStr(HTTP_CONN_TYPE_KEEPALIVE), 0, 0, pClient->hostbuf, NULL)) < 0) {
    return -1;
  }

  if(!(p = conf_find_keyval(httpResp.hdrPairs, HTTP_HDR_CONTENT_TYPE))) {
    LOG(X_ERROR("No "HTTP_HDR_CONTENT_TYPE" found in response"));
    return -1;
  } else if(strncasecmp(p, CONTENT_TYPE_MP4, strlen(CONTENT_TYPE_MP4))) {
    LOG(X_ERROR("Unsupported %s: %s received in response for %s"),
               HTTP_HDR_CONTENT_TYPE, (p ? p : "<Not Found>"), puri);
    rc = -1;
  }

  if(!(pdata = httpcli_get_contentlen_start(&httpResp, &hdrCtxt, buf, sizeof(buf),
                                    0, &contentLen))) {
    rc = -1;
  }

  //fprintf(stderr, "DASH media '%s' contentlen:%d pdata:0x%x idxb:%d hdrslen:%d\n", puri, contentLen, pdata, hdrCtxt.idxbuf, hdrCtxt.hdrslen);

  if(rc >= 0 && pClient->pCfg->running == 0) {
    rc = http_mp4_recv(pClient->pCfg, 
                       &pClient->netsock, 
                       (const struct sockaddr *) &pClient->sa, 
                       contentLen, 
                       &hdrCtxt, 
                       pathOut);
                       //pClient->pCapHttpMp4Stream);
  }

  //fprintf(stderr, "MP4 GET DONE w rc:%d\n", rc);

  return rc;
}

static int dash_get_index(const char *path) {
  unsigned int idx = 0;
  char *p;
  char tmp[64];
  size_t sz, sz2;

  if((p = strchr(path, '?'))) {
    sz = p - path;
  } else {
    sz = strlen(path);
  }

  for(; sz > 0; sz--) {
    if(path[sz - 1] == '.') {
      sz--;
      break;
    }
  }

  if(sz > 3) {
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

  //fprintf(stderr, "dash_get_index:%d '%s'\n", idx, path);

  return idx;
}

static const char *dash_find_path(PLAYLIST_MPD_T *pl, unsigned int searchidx) {
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;

  pEntry = pl->pEntriesHead;
  while(pEntry) {
    //fprintf(stderr, "find_path searchidx:%d, pl->numEntries:%d, path[%d]:'%s'\n",  searchidx, pl->numEntries, idx, pEntry->path);

    if(dash_get_index(pEntry->path) == searchidx) {
      return pEntry->path;
    }

    pEntry = pEntry->pnext;
  }
  return NULL;
}

static const char *get_uri_from_path(HTTPDASH_CLIENT_T *pClient, const char *path) {

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



static int dash_find_lowest_idx(PLAYLIST_MPD_T *pl, char **pp) {
  int rc = 0;

  if(pl->pEntriesHead && (rc = dash_get_index(pl->pEntriesHead->path)) >= 0) {
    if(pp) {
      *pp = pl->pEntriesHead->path;
    }
  }

  return rc;
}

static int dash_find_highest_idx(PLAYLIST_MPD_T *pl, char **pp) {
  int rc = 0;

  if(pl->pEntriesTail && (rc = dash_get_index(pl->pEntriesTail->path)) >= 0) {
    if(pp) {
      *pp = pl->pEntriesTail->path;
    }
  }

  return rc;
}

static void httpdash_mediaproc(void *pArg) {
  HTTPDASH_CLIENT_T *pClient = (HTTPDASH_CLIENT_T *) pArg;
  int rc = 0;
  const char *p;
  const char *puri = NULL;
  int haveInitializer = 0;
  int getInitializer = 0;
  char stridx[32];
  char urlpath[VSX_MAX_PATH_LEN];
  char fileoutpath[VSX_MAX_PATH_LEN];
  unsigned int tmtms = 0;
  CAP_HTTP_MP4_STREAM_T  *pCapHttpMp4Stream = NULL;

  pClient->running = 0;

  LOG(X_DEBUG("Starting DASH client media thread"));

  urlpath[sizeof(urlpath) - 1] = '\0';
  pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pClient->pCfg->pUserData;

  while(rc >= 0 && pClient->pCfg->running == 0 && pClient->insession) {

    urlpath[0] = '\0';
    p = NULL;

    pthread_mutex_lock(&pClient->mtx);

    if(!haveInitializer && pClient->pl.initializer.path) {

      p = pClient->pl.initializer.path;
      snprintf(stridx, sizeof(stridx), "init");
      getInitializer = 1;

    } else {

      if(pClient->nextidx > pClient->curidx) {
        LOG(X_WARNING("DASH skipping playlist media chunk index %d - %d"),
                       pClient->curidx, pClient->nextidx - 1);
        pClient->curidx = pClient->nextidx;
      }

      p = dash_find_path(&pClient->pl, pClient->curidx);

      snprintf(stridx, sizeof(stridx), "%d", pClient->curidx);

    }

    if(p) {
      strncpy(urlpath, p, sizeof(urlpath) - 1);
    }

    pthread_mutex_unlock(&pClient->mtx);

    // 
    // Create a unique output filename for the downloaded mp4 file
    //
    snprintf(fileoutpath, sizeof(fileoutpath), "%s%c%s_%d_%s.mp4",
           "tmp", DIR_DELIMETER, "dl", getpid(), stridx);

    if(!(puri = get_uri_from_path(pClient, urlpath))) {
      rc = -1;
      break;
    }

    if(puri && puri[0] != '\0') {

      // TODO: create httplive retrieval flag forcing new socket... lame
      if(NETIOSOCK_FD(pClient->netsock) != INVALID_SOCKET && net_issockremotelyclosed(NETIOSOCK_FD(pClient->netsock), 1)) {
        LOG(X_DEBUG("DASH media socket has been closed"));
        netio_closesocket(&pClient->netsock);
      }

      if(NETIOSOCK_FD(pClient->netsock) == INVALID_SOCKET) {

        fprintf(stderr, "----DASH MEDIA GET for '%s' '%s'\n", urlpath, puri);

        if((rc = httpcli_connect(&pClient->netsock, (const struct sockaddr *) &pClient->sa, 
                                 tmtms, "DASH media thread")) < 0) {
          break;
        }
      }

      //fprintf(stderr, "will call get_moof puri:'%s', idx:%d, next:%d\n", puri, pClient->curidx, pClient->nextidx);

      if((rc = get_moof(pClient, puri, fileoutpath, tmtms)) >= 0) {

        if(getInitializer) {
          haveInitializer = 1;
          getInitializer = 0;
        } else {

          pthread_mutex_lock(&pClient->mtx);
          pClient->nextidx++;
          pClient->curidx = pClient->nextidx;
          pthread_mutex_unlock(&pClient->mtx);

        }

        fprintf(stderr, "DASH media retrieval '%s' returned %d, curidx now:%d\n", puri, rc, pClient->curidx);
        mpdpl_dump(&pCapHttpMp4Stream->pl);

      }

    } // end of if(puri && puri[0] != '\0' ...

    sleep(1);

  }

  netio_closesocket(&pClient->netsock);

  pClient->insession = 0;
  pClient->running = -1;

  LOG(X_DEBUG("DASH media download thread exiting with code %d"), rc);

}



int http_gethttpdash(CAP_ASYNC_DESCR_T *pCfg,
                     const char *puri,
                     HTTP_RESP_T *pHttpResp,
                     HTTP_PARSE_CTXT_T *pHdrCtxt) {
  int rc = 0;
  char *path;
  const char *pbuf = pHdrCtxt->pbuf;
  unsigned int szbuf = pHdrCtxt->szbuf;
  unsigned int szcontent;
  const char *pmpdbuf;
  pthread_t ptd;
  pthread_attr_t attr;
  HTTPDASH_CLIENT_T client;
  SOCKET_LIST_T sockList;
  int highestIdx, lowestIdx, firstIdx;
  unsigned int tmtms = 0;

  if(!pCfg || !pCfg->pUserData || !puri || !pHttpResp || !pHdrCtxt) {
    return -1;
  }

  memset(&sockList, 0, sizeof(sockList));
  memset(&client, 0, sizeof(client));
  client.pCfg = pCfg;
  //client.pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCfg->pUserData;
  NETIOSOCK_FD(client.netsock) = INVALID_SOCKET;
  client.netsock.flags = pCfg->pSockList->netsockets[0].flags;
  client.running = -1;

  pthread_mutex_init(&client.mtx, NULL);

  do {

    if(NETIOSOCK_FD(pCfg->pSockList->netsockets[0]) == INVALID_SOCKET) {

//fprintf(stderr, "GOING TO CONNECT FOR DASH PLAYLIST...\n");

      if((rc = httpcli_connect(&pCfg->pSockList->netsockets[0], (const struct sockaddr *) 
                               &pCfg->pSockList->salist[0], tmtms, "DASH playlist thread")) < 0) {
        break;
      }

      memset(pHdrCtxt, 0, sizeof(HTTP_PARSE_CTXT_T));
      memset(pHttpResp, 0, sizeof(HTTP_RESP_T));
      pHdrCtxt->pnetsock = &pCfg->pSockList->netsockets[0];
      pHdrCtxt->pbuf = pbuf;
      pHdrCtxt->szbuf = szbuf;
      pHdrCtxt->tmtms = tmtms;
    }

    if((rc = mediadb_getdirlen(puri)) > 0) {
      if(rc >= sizeof(client.uriprefix)) {
        rc = sizeof(client.uriprefix) - 1;
      }
      memcpy(client.uriprefix, puri, rc);
    }

    //fprintf(stderr, "calling http_get_doc '%s'\n", puri);

    // TODO: this may need to be a bigger buffer for large XML mpd files
    //if((pmpdbuf = http_get_doc(pCfg, puri, pHttpResp, pHdrCtxt, (unsigned char *) pbuf, szbuf))) {
    szcontent = szbuf;
    if((pmpdbuf = (const char *) httpcli_loadpagecontent(puri, (unsigned char *) pbuf, &szcontent, 
                                   &pCfg->pSockList->netsockets[0], 
                                   (const struct sockaddr *) &pCfg->pSockList->salist[0], 0, 
                                   pHttpResp, pHdrCtxt, pCfg->pcommon->addrsExtHost[0]))) {

      mpdpl_free(&client.pl);
      memset(&client.pl, 0, sizeof(client.pl));

      VSX_DEBUG_DASH( LOG(X_DEBUG("Got DASH mpd contents '%s'"), pmpdbuf); );
      //fprintf(stderr, "Got DASH mpd contents '%s'\n", pmpdbuf);

      pthread_mutex_lock(&client.mtx);
      rc = mpdpl_create(&client.pl, pmpdbuf);
      pthread_mutex_unlock(&client.mtx);

      if(rc > 0) {

        path = NULL;
        lowestIdx = dash_find_lowest_idx(&client.pl, NULL);
        highestIdx = dash_find_highest_idx(&client.pl, NULL);

        fprintf(stderr, "dash mpd lowestIdx:%d, highestIdx:%d, insession:%d\n", lowestIdx, highestIdx, client.insession);

        if(!client.insession) {

          if(highestIdx < 0) {
            LOG(X_WARNING("Unable to find DASH starting index from %s"), puri);
          } else {

            //firstIdx = highestIdx;
            firstIdx = lowestIdx;

            client.curidx  = firstIdx;
            client.nextidx = client.curidx;
            client.insession = 1;

            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            client.running = 1;

            if(pthread_create(&ptd, &attr, (void *) httpdash_mediaproc, &client) != 0) {
              LOG(X_ERROR("Unable to create DASH media download thread"));
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

    } else { // dash_get_doc
      fprintf(stderr, "dash_get_doc failed\n");
      rc = -1;
    }

    netio_closesocket(&pCfg->pSockList->netsockets[0]);

    if(rc >= 0) {
      //fprintf(stderr, "MPD SLEEPING FOR %d\n", client.pl.targetDuration);
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

  mpdpl_free(&client.pl);
  pthread_mutex_destroy(&client.mtx);

  LOG(X_DEBUG("http_gethttpdash exiting"));

  return rc;
}



#endif // VSX_HAVE_CAPTURE
