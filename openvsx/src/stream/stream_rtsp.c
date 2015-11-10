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

#if defined(VSX_HAVE_STREAMER)

static int rtspsrv_closeSessionStreams(STREAM_RTSP_SESSIONS_T *pSessions,
                                       RTSP_SESSION_T *pSession);

typedef struct RTSP_MONITOR_CTXT {
  char                       tid_tag[LOGUTIL_TAG_LENGTH];
  STREAM_RTSP_SESSIONS_T    *pRtsp;
} RTSP_MONITOR_CTXT_T;

static void rtsp_monitor_proc(void *pArg) {
  RTSP_MONITOR_CTXT_T *pCtxt = (RTSP_MONITOR_CTXT_T *) pArg;
  STREAM_RTSP_SESSIONS_T *pRtsp = (STREAM_RTSP_SESSIONS_T *) pCtxt->pRtsp;
  char buf[1024];
  unsigned int idx;
  int len;
  struct sockaddr_in sa;
  struct timeval tvnow;
  fd_set fdsetRd;
  struct timeval tv;

  logutil_tid_add(pthread_self(), pCtxt->tid_tag);
 
  pRtsp->runMonitor = 1;

  while(pRtsp->runMonitor == 1 && !g_proc_exit) {

    gettimeofday(&tvnow, NULL);

    pthread_mutex_lock(&pRtsp->mtx);

    //
    // Consume and discard any data received on the base (RTP) port
    // which may be pinhole / socket non-ICMP reachable polling data
    //
    if(pRtsp->sockStaticLocalPort != INVALID_SOCKET) {

      memset(&tv, 0, sizeof(tv));
      FD_ZERO(&fdsetRd);
      FD_SET(pRtsp->sockStaticLocalPort, &fdsetRd);

      if((len = select(pRtsp->sockStaticLocalPort + 1, &fdsetRd, NULL, NULL, &tv)) > 0) {

        if(FD_ISSET(pRtsp->sockStaticLocalPort, &fdsetRd)) {
          len = sizeof(sa);
          len = recvfrom(pRtsp->sockStaticLocalPort, (void *) buf, sizeof(buf), 0,
                     (struct sockaddr *) &sa, (socklen_t *) &len);
          //fprintf(stderr, "RECV:%d %d "ERRNO_FMT_STR"\n", len, EAGAIN, ERRNO_FMT_ARGS );
        }
      } else if(len == 0) {

      } else {

      }
    }

    //
    // Check and expire streams
    //
    for(idx = 0; idx < pRtsp->max; idx++) {

      //if(pRtsp->psessions[idx].inuse) fprintf(stderr, "CHECK EXPIRE RTSP STREAM[%d] now:%d, lastupdate:%d + %d\n", idx, tvnow.tv_sec,  pRtsp->psessions[idx].tvlastupdate.tv_sec, pRtsp->psessions[idx].expireSec);

      if(pRtsp->psessions[idx].inuse && 
         !(pRtsp->psessions[idx].flag & RTSP_SESSION_FLAG_EXPIRED) &&
         tvnow.tv_sec > pRtsp->psessions[idx].tvlastupdate.tv_sec + 
         pRtsp->psessions[idx].expireSec) {
         //4) {


        LOG(X_DEBUG("Expiring RTSP session id %s after %"SECT" / %d idle seconds. Active: %d / %d"),
                    pRtsp->psessions[idx].sessionid, 
                    tvnow.tv_sec - pRtsp->psessions[idx].tvlastupdate.tv_sec, 
                    pRtsp->psessions[idx].expireSec,
                    pRtsp->numActive > 0 ? pRtsp->numActive - 1 : 0, pRtsp->max);

        pRtsp->psessions[idx].flag |= RTSP_SESSION_FLAG_EXPIRED;

        //rtspsrv_closeSessionStreams(pRtsp, &pRtsp->psessions[idx]);
        //if(pRtsp->numActive > 0) {
        //  pRtsp->numActive--;
        //}
        //pRtsp->psessions[idx].inuse = 0;

      }
    }

    pthread_mutex_unlock(&pRtsp->mtx);

    sleep(2);
  }

  pRtsp->runMonitor = -1;

  LOG(X_DEBUG("RTSP Monitor thread exiting"));
}

static void destroy_rtspgetsessions(STREAM_RTSP_SESSIONS_T *pRtsp) {
  unsigned int idx;

  if(pRtsp->pRtspGetSessionsBuf) {
    for(idx = 0; idx < pRtsp->numRtspGetSessions; idx++) {

      if(pRtsp->pRtspGetSessionsBuf[idx].pRequestBufWr) {

        pRtsp->pRtspGetSessionsBuf[idx].pRequestBufRd[0] = '\0';
        vsxlib_cond_broadcast(&pRtsp->pRtspGetSessionsBuf[idx].cond.cond,
                               &pRtsp->pRtspGetSessionsBuf[idx].cond.mtx);

        pthread_cond_destroy(&pRtsp->pRtspGetSessionsBuf[idx].cond.cond);
        pthread_mutex_destroy(&pRtsp->pRtspGetSessionsBuf[idx].cond.mtx);
        pthread_mutex_destroy(&pRtsp->pRtspGetSessionsBuf[idx].mtx);
        avc_free((void *) &pRtsp->pRtspGetSessionsBuf[idx].pRequestBufWr);
      } 

      pRtsp->pRtspGetSessionsBuf[idx].pRequestBufRd = NULL;

    }
    avc_free((void *) &pRtsp->pRtspGetSessionsBuf);
  }

  pRtsp->pRtspGetSessionsHead = pRtsp->pRtspGetSessionsTail = NULL;
  pRtsp->numRtspGetSessions = 0;
  
}

static int cbparse_entry_rtspua(void *pArg, const char *p) {
  STREAM_RTSP_SESSIONS_T *pRtsp = (STREAM_RTSP_SESSIONS_T *) pArg;
  size_t sz;
  int rc = 0;

  if(pRtsp->rtspForceTcpUAList.count >= RTSP_FORCE_TCP_UA_LIST_MAX) {
    return 0;
  }

  if((p = avc_dequote(p, NULL, 0))) {
    sz = strlen(p);
    if(!(pRtsp->rtspForceTcpUAList.arr[pRtsp->rtspForceTcpUAList.count] = (char *) avc_calloc(1, sz + 1))) {
      return -1;
    }
    memcpy(pRtsp->rtspForceTcpUAList.arr[pRtsp->rtspForceTcpUAList.count++], p, sz);
  }

  return rc;
}

int rtspsrv_init(STREAM_RTSP_SESSIONS_T *pRtsp) {

  pthread_t ptdMonitor;
  struct sockaddr_storage sa;
  pthread_attr_t attrMonitor;
  RTSP_MONITOR_CTXT_T startCtxt;
  const char *s;

  if(!pRtsp || pRtsp->max <= 0) {
    return -1;
  }

  if(pRtsp->psessions) {
    avc_free((void *) &pRtsp->psessions);
  }

  destroy_rtspgetsessions(pRtsp);

  if(!(pRtsp->psessions = (RTSP_SESSION_T *)
                          avc_calloc(pRtsp->max, sizeof(RTSP_SESSION_T)))) {
    return -1;
  }

  pRtsp->numRtspGetSessions = pRtsp->max * 2;
  if(!(pRtsp->pRtspGetSessionsBuf = (RTSP_HTTP_SESSION_T *)
                          avc_calloc(pRtsp->numRtspGetSessions, sizeof(RTSP_HTTP_SESSION_T)))) {
    avc_free((void *) &pRtsp->psessions);
    pRtsp->numRtspGetSessions = 0;
    return -1;
  }

  pthread_mutex_init(&pRtsp->mtx, NULL);
 
  //
  // If all UDP / RTP sockets are bound to the same port then establish
  // the listener of this port prior to any RTSP interaction because some app
  // gateways may send some UDP polling data to the base port - and returning
  // an ICMP port unreachable would prevent such app gateways from allocating
  // UDP proxy ports
  //
  pRtsp->sockStaticLocalPort = INVALID_SOCKET;
  if(pRtsp->staticLocalPort > 0) {
    memset(&sa, 0, sizeof(sa));
    sa.ss_family = AF_INET;
    ((struct sockaddr_in *) &sa)->sin_addr.s_addr = INADDR_ANY;
    INET_PORT(sa) = htons(pRtsp->staticLocalPort);
    if((pRtsp->sockStaticLocalPort = net_opensocket(SOCK_DGRAM, 0, 0, (struct sockaddr *) &sa)) == INVALID_SOCKET) {
      LOG(X_ERROR("Failed to open RTSP static local RTP port %d"), pRtsp->staticLocalPort);
    } else {
      if(net_setsocknonblock(pRtsp->sockStaticLocalPort, 1) < 0) {
      LOG(X_ERROR("Failed to listen on RTSP static local RTP port %d"), pRtsp->staticLocalPort);
        net_closesocket(&pRtsp->sockStaticLocalPort);
      }
    }
    //if(pRtsp->sockStaticLocalPort != INVALID_SOCKET) {
    //  sain.sin_addr.s_addr = inet_addr("127.0.0.1");
    //  rc = sendto(pRtsp->sockStaticLocalPort, &sain, 1, 0, (struct sockaddr *) &sain, sizeof(sain));
    //  fprintf(stderr, "SENDTO:%d\n", rc);
    //}
  }

  //
  // Parse any CSV of quoted User-Agent matches which should try to force TCP interleaved mode
  //
  pRtsp->rtspForceTcpUAList.count = 0;
  if(pRtsp->rtspForceTcpUAList.str) {
    strutil_parse_csv(cbparse_entry_rtspua, pRtsp, pRtsp->rtspForceTcpUAList.str);
  }

  pRtsp->runMonitor = 2;
  memset(&startCtxt, 0, sizeof(startCtxt));
  startCtxt.pRtsp = pRtsp;
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(startCtxt.tid_tag, sizeof(startCtxt.tid_tag), "%s-rtspmon", s);
  }

  PHTREAD_INIT_ATTR(&attrMonitor);

  if(pthread_create(&ptdMonitor,
                    &attrMonitor,
                    (void *) rtsp_monitor_proc,
                    (void *) &startCtxt) != 0) {
    LOG(X_ERROR("Unable to create RTP monitor thread"));
    pRtsp->runMonitor = 0;
    if(pRtsp->psessions) {
      avc_free((void *) &pRtsp->psessions);
    }
    destroy_rtspgetsessions(pRtsp);
    pthread_mutex_destroy(&pRtsp->mtx);
  }

  while(pRtsp->runMonitor != 1 && pRtsp->runMonitor != -1) {
    usleep(5000);
  }

  return 0;
}

void rtspsrv_close(STREAM_RTSP_SESSIONS_T *pRtsp) {

  unsigned int idx;

  if(!pRtsp) {
    return;
  }

  if(pRtsp->sockStaticLocalPort != INVALID_SOCKET) {
    net_closesocket(&pRtsp->sockStaticLocalPort);
  }

  if(pRtsp->runMonitor > 0) {
    pRtsp->runMonitor = -2;
    while(pRtsp->runMonitor != -1) {
      usleep(5000);
    }
  }

  pthread_mutex_destroy(&pRtsp->mtx);

  if(pRtsp->psessions) {
    avc_free((void *) &pRtsp->psessions);
  }

  for(idx = 0; idx < RTSP_FORCE_TCP_UA_LIST_MAX; idx++) {
    if(pRtsp->rtspForceTcpUAList.arr[idx]) {
      avc_free((void *) &pRtsp->rtspForceTcpUAList.arr[idx]);
    }
  }
  pRtsp->rtspForceTcpUAList.count = 0;

  destroy_rtspgetsessions(pRtsp);

  *((unsigned int *) &pRtsp->max) = 0;

}

RTSP_HTTP_SESSION_T *rtspsrv_newHttpSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char *sessionCookie,
                                               const struct sockaddr *psa) {
  RTSP_HTTP_SESSION_T *pRtspGetSession = NULL; 
  size_t szCookie;
  unsigned int idx;

  if(!pRtsp || !pRtsp->pRtspGetSessionsBuf || !sessionCookie || sessionCookie[0] == '\0') {
    return NULL;
  }

  if((szCookie = strlen(sessionCookie)) > RTSP_HTTP_SESSION_COOKIE_MAX - 1) {
    LOG(X_ERROR("Client RTSP HTTP session cookie length %d exceeds %d"), szCookie, 
        RTSP_HTTP_SESSION_COOKIE_MAX - 1);
    return NULL;
  }

  pthread_mutex_lock(&pRtsp->mtx);

  for(idx = 0; idx < pRtsp->numRtspGetSessions; idx++) {

    if(!pRtsp->pRtspGetSessionsBuf[idx].inuse) {

      pRtspGetSession = &pRtsp->pRtspGetSessionsBuf[idx];

      if(!pRtspGetSession->pRequestBufWr) {
        if(!(pRtspGetSession->pRequestBufWr = (char *) avc_calloc(2, RTSP_HTTP_POST_BUFFER_SZ))) {
          pthread_mutex_unlock(&pRtsp->mtx);
          return NULL;
        }
        pRtspGetSession->pRequestBufRd = pRtspGetSession->pRequestBufWr + RTSP_HTTP_POST_BUFFER_SZ;

        pthread_mutex_init(&pRtspGetSession->cond.mtx, NULL);
        pthread_cond_init(&pRtspGetSession->cond.cond, NULL);
        pthread_mutex_init(&pRtspGetSession->mtx, NULL);
      }

      pRtspGetSession->inuse = 1;
      memset(&pRtspGetSession->netsocketPost, 0, sizeof(pRtspGetSession->netsocketPost));
      NETIOSOCK_FD(pRtspGetSession->netsocketPost) = INVALID_SOCKET;
      pRtspGetSession->expired = 0;
      strncpy(pRtspGetSession->cookie.sessionCookie, sessionCookie, RTSP_HTTP_SESSION_COOKIE_MAX - 1);
      if(psa) {
        memcpy(&pRtspGetSession->cookie.sa, psa, INET_SIZE(*psa));
      }
      gettimeofday(&pRtspGetSession->tvCreate, NULL);
      pRtspGetSession->tvLastMsg.tv_sec = 0;
      pRtspGetSession->requestSz = 0;
      pRtspGetSession->requestIdxWr = 0;
      pRtspGetSession->pRequestBufWr[0] = '\0';
      pRtspGetSession->pRequestBufRd[0] = '\0';
      pRtspGetSession->pnext = NULL;

      if(pRtsp->pRtspGetSessionsTail) {
        pRtsp->pRtspGetSessionsTail->pnext = pRtspGetSession;
      } else {
        pRtsp->pRtspGetSessionsHead = pRtspGetSession;
      }

      pRtsp->pRtspGetSessionsTail = pRtspGetSession;

      break;
    }

  }

  pthread_mutex_unlock(&pRtsp->mtx);

  return pRtspGetSession;
}


int rtspsrv_deleteHttpSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char *sessionCookie,
                              const struct sockaddr *psa) {
  RTSP_HTTP_SESSION_T *pRtspGetSession = NULL; 
  RTSP_HTTP_SESSION_T *pRtspGetSessionPrev = NULL; 

  if(!pRtsp || !pRtsp->pRtspGetSessionsBuf || !sessionCookie || sessionCookie[0] == '\0') {
    return -1;
  }


  pthread_mutex_lock(&pRtsp->mtx);

  pRtspGetSession = pRtsp->pRtspGetSessionsHead;
  while(pRtspGetSession) {

    if(!strcmp(sessionCookie, pRtspGetSession->cookie.sessionCookie) &&
      (!psa || INET_IS_SAMEADDR(*psa, pRtspGetSession->cookie.sa))) {

      if(pRtspGetSessionPrev) {
        pRtspGetSessionPrev->pnext = pRtspGetSession->pnext;
      } else {
        pRtsp->pRtspGetSessionsHead = pRtspGetSession->pnext;
      }
      if(pRtsp->pRtspGetSessionsTail == pRtspGetSession) {
        pRtsp->pRtspGetSessionsTail = pRtspGetSessionPrev;
      }  
      pRtspGetSession->pnext = NULL;
      pthread_mutex_unlock(&pRtsp->mtx);

      //
      // Now that the found node has been unreferenced from the 'find' linked list
      // it can be safely cleared within mutex lock, so that there is no concurrent
      // operation on this node from a thread such as the RTSP HTTP POST msg writer
      //
      pRtspGetSession->expired = 1;
      pthread_mutex_lock(&pRtspGetSession->mtx);
      pRtspGetSession->cookie.sessionCookie[0] = '\0';
      memset(&pRtspGetSession->cookie.sa, 0, sizeof(pRtspGetSession->cookie.sa));
      pRtspGetSession->inuse = 0;
      pthread_mutex_unlock(&pRtspGetSession->mtx);

      return 0;
    }

    pRtspGetSessionPrev = pRtspGetSession;
    pRtspGetSession = pRtspGetSession->pnext;
  }

  pthread_mutex_unlock(&pRtsp->mtx);

  return -1;
}

RTSP_HTTP_SESSION_T *rtspsrv_findHttpSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char *sessionCookie,
                                              const struct sockaddr *psa, int lock) {
  RTSP_HTTP_SESSION_T *pRtspGetSession = NULL;

  if(!pRtsp || !pRtsp->pRtspGetSessionsBuf || !sessionCookie || sessionCookie[0] == '\0') {
    return NULL;
  }

  pthread_mutex_lock(&pRtsp->mtx);

  pRtspGetSession = pRtsp->pRtspGetSessionsHead;
  while(pRtspGetSession) {

    if(!strcmp(sessionCookie, pRtspGetSession->cookie.sessionCookie) &&
      (!psa || INET_IS_SAMEADDR(*psa, pRtspGetSession->cookie.sa))) {
      break;
    }

    pRtspGetSession = pRtspGetSession->pnext;
  }

  if(lock && pRtspGetSession) {
    pthread_mutex_lock(&pRtspGetSession->mtx);
  }

  pthread_mutex_unlock(&pRtsp->mtx);

  return pRtspGetSession;
}

RTSP_SESSION_T *rtspsrv_findSession(STREAM_RTSP_SESSIONS_T *pRtsp, 
                                           const char *sessionId) {
  RTSP_SESSION_T *pSes = NULL;
  unsigned int idx;

  if(!pRtsp || !sessionId) {
    return NULL;
  }

  pthread_mutex_lock(&pRtsp->mtx);

  for(idx = 0; idx < pRtsp->max; idx++) {
    if(pRtsp->psessions[idx].inuse && 
       !strncmp(pRtsp->psessions[idx].sessionid, sessionId, RTSP_SESSION_ID_MAX)) {
      pSes = &pRtsp->psessions[idx];
      break;
    }
  }

  pthread_mutex_unlock(&pRtsp->mtx);

  return pSes;
}

static int rtspsrv_create_sessionid(char *sessionId) {
  unsigned int idx;

  for(idx = 0; idx < RTSP_SESSION_ID_LEN - 2; idx++) {
    sessionId[idx] = '0' + (random() % 10);
  }

  sessionId[idx] = '\0';

  return 0;
}

void rtspsrv_initSession(const STREAM_RTSP_SESSIONS_T *pRtsp, RTSP_SESSION_T *pSes) {

  if(!pRtsp || !pSes) {
    return;
  }

  memset(pSes, 0, sizeof(RTSP_SESSION_T));

  //pSes->vid.dstAddr.s_addr = INADDR_NONE;
  //pSes->aud.dstAddr.s_addr = INADDR_NONE;
  rtspsrv_create_sessionid(pSes->sessionid);
  gettimeofday(&pSes->tvlastupdate, NULL);

  if(!pRtsp->psessionTmtSec || *pRtsp->psessionTmtSec == 0) {
    pSes->expireSec = RTSP_SESSION_IDLE_SEC;
    //pSes->expireSec = RTSP_SESSION_IDLE_SEC_SPEC;
  } else {
    pSes->expireSec = *pRtsp->psessionTmtSec;
  }
  if(pRtsp->prefreshtimeoutviartcp) {
    pSes->refreshtimeoutviartcp = *pRtsp->prefreshtimeoutviartcp;
  }
}

RTSP_SESSION_T *rtspsrv_newSession(STREAM_RTSP_SESSIONS_T *pRtsp, int rtpmux, int rtcpmux) {
  RTSP_SESSION_T *pSes = NULL;
  unsigned int idx;

  if(!pRtsp) {
    return NULL;
  }

  pthread_mutex_lock(&pRtsp->mtx);

  for(idx = 0; idx < pRtsp->max; idx++) {

    if(!pRtsp->psessions[idx].inuse) {

      pSes = &pRtsp->psessions[idx];

      rtspsrv_initSession(pRtsp, pSes);

      if(rtsp_alloc_listener_ports(pSes, pRtsp->staticLocalPort, rtpmux, rtcpmux, 1, 1) < 0) {
        memset(pSes, 0, sizeof(RTSP_SESSION_T));
        return NULL;
      }
/*
      if(pRtsp->staticLocalPort) {
        //
        // Some RTSP firewalls / SBCs require source port to always be in a range 6970 - 9999
        // or even 6970-6971 only
        //
        pSes->vid.localports[0] = pRtsp->staticLocalPort;
        pSes->vid.localports[1] = pSes->vid.localports[0] + 1;
        pSes->aud.localports[0] = pSes->vid.localports[0] + 0;
        pSes->aud.localports[1] = pSes->aud.localports[0] + 1;
      } else {
        //
        // Create contiguous series of ports
        // Some media clients/servers may not work properly with different
        // video / audio port ranges, 
        //
        pSes->vid.localports[0] = net_getlocalmediaport(2);
        pSes->vid.localports[1] = pSes->vid.localports[0] + 1;
        pSes->aud.localports[0] = pSes->vid.localports[0] + 2;
        pSes->aud.localports[1] = pSes->aud.localports[0] + 1;
      }
*/
      pRtsp->numActive++;
      pSes->inuse = 1;

      LOG(X_DEBUG("Created RTSP %ssession %s Active: %d / %d"), 
            ((pSes->sessionType & RTSP_SESSION_TYPE_INTERLEAVED) ? "(interleaved) " : ""),
             pSes->sessionid, pRtsp->numActive, pRtsp->max);
      break;
    }
  }

  pthread_mutex_unlock(&pRtsp->mtx);


  return pSes;
}

static int rtspsrv_closeSessionStreams(STREAM_RTSP_SESSIONS_T *pSessions,
                                       RTSP_SESSION_T *pSession) {
  int rc = 0;
  //STREAM_DEST_CFG_T destCfg;
  unsigned int outidx;
  char tmp[128];

  if(!pSessions || !pSession || !pSessions->pRtpMultis) {
    return -1;
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUG("RTSP - Close Session Streams")) );

  outidx = pSession->requestOutIdx;

  if(pSession->vid.issetup) {

    rc = stream_rtp_removedest(&pSessions->pRtpMultis[outidx][0], pSession->vid.pDest);

    if(rc < 0 && pSession->vid.isplaying) {
      LOG(X_WARNING("Failed to remove RTSP video destination[%d] %s:%d"), 
              outidx, FORMAT_NETADDR(pSession->vid.dstAddr, tmp, sizeof(tmp)), pSession->vid.ports[0]);
    }
    pSession->vid.issetup = 0;
  }

  if(pSession->aud.issetup) {

    if((rc = stream_rtp_removedest(&pSessions->pRtpMultis[outidx][1], pSession->aud.pDest)) < 0) {
      rc = stream_rtp_removedest(&pSessions->pRtpMultis[outidx][0], pSession->aud.pDest);
    }

    if(rc < 0 && pSession->aud.isplaying) {
      LOG(X_WARNING("Failed to remove RTSP audio destination[%d] %s:%d"), 
            outidx, FORMAT_NETADDR(pSession->aud.dstAddr, tmp, sizeof(tmp)), pSession->aud.ports[0]);
    }
    pSession->aud.issetup = 0;
  }

  return rc;
}

int rtspsrv_closeSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char sessionId[]) {
  unsigned int idx;
  int rc = -1;

  if(!pRtsp) {
    return -1;
  }

  LOG(X_DEBUG("Closing RTSP session %s Active: %d / %d"), 
      sessionId, pRtsp->numActive > 0 ? pRtsp->numActive - 1 : 0, pRtsp->max);

  pthread_mutex_lock(&pRtsp->mtx);

  if(pRtsp->psessions) {

    for(idx = 0; idx < pRtsp->max; idx++) {
      if(pRtsp->psessions[idx].inuse && 
         !strncmp(pRtsp->psessions[idx].sessionid, sessionId, RTSP_SESSION_ID_MAX)) {

        rtspsrv_closeSessionStreams(pRtsp, &pRtsp->psessions[idx]);

        pRtsp->psessions[idx].inuse = 0;
        if(pRtsp->numActive > 0) {
          pRtsp->numActive--;
        }
        rc = 0;
        break;
      }
    }

  }

  pthread_mutex_unlock(&pRtsp->mtx);

  if(rc != 0) {
    LOG(X_WARNING("Failed to close RTSP session %s"), sessionId);
  }

  return rc;
}

int rtspsrv_playSessionTrack(STREAM_RTSP_SESSIONS_T *pRtsp, 
                             const char sessionId[], int trackId, int lock) {
  unsigned int idx;
  int rc = -1;

  if(!pRtsp) {
    return -1;
  }

  if(lock) {
    pthread_mutex_lock(&pRtsp->mtx);
  }

  for(idx = 0; idx < pRtsp->max; idx++) {
    if(pRtsp->psessions[idx].inuse && 
       !strncmp(pRtsp->psessions[idx].sessionid, sessionId, RTSP_SESSION_ID_MAX)) {

      rc = 0;

      if(trackId == RTSP_TRACKID_VID) {

        pRtsp->psessions[idx].vid.isplaying = 1;

      } else if(trackId == RTSP_TRACKID_AUD) {

        pRtsp->psessions[idx].aud.isplaying = 1;

      } else {
        rc = -1;
      }

      break;
    }
  }

  if(lock) {
    pthread_mutex_unlock(&pRtsp->mtx);
  }

  return 0;
}

#endif // VSX_HAVE_STREAMER
