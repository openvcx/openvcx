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

static void srvlisten_http_proc(void *pArg) {

  SRV_LISTENER_CFG_T *pListenCfg = (SRV_LISTENER_CFG_T *) pArg;
  NETIO_SOCK_T netsocksrv;
  struct sockaddr_storage sa;
  CLIENT_CONN_T *pConn;
  char tmp[128];
  const int backlog = NET_BACKLOG_DEFAULT;
  unsigned int tsMax = 0;
  unsigned int flvMax = 0;
  unsigned int mkvMax = 0;
  int haveAuth = 0;
  int haveToken = 0;
  int rc = 0;

  logutil_tid_add(pthread_self(), pListenCfg->tid_tag);

  if((pListenCfg->urlCapabilities & (URL_CAP_TSLIVE | URL_CAP_TSHTTPLIVE | URL_CAP_FLVLIVE |
                                     URL_CAP_MKVLIVE | URL_CAP_LIVE | URL_CAP_STATUS | URL_CAP_MOOFLIVE |
                                     URL_CAP_PIP | URL_CAP_CONFIG | URL_CAP_BROADCAST)) == 0) {
    LOG(X_WARNING("http listener exiting because no capabilities enabled on %s:%d"),
         FORMAT_NETADDR(pListenCfg->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pListenCfg->sa)));
    logutil_tid_remove(pthread_self());
    return;
  }

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pListenCfg->sa, sizeof(pListenCfg->sa));
  netsocksrv.flags = pListenCfg->netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, backlog)) == INVALID_SOCKET) {
    logutil_tid_remove(pthread_self());
    return;
  }

  pthread_mutex_lock(&pListenCfg->mtx);
  pListenCfg->pnetsockSrv = &netsocksrv;
  pthread_mutex_unlock(&pListenCfg->mtx);

  if(pListenCfg->pAuthStore && IS_AUTH_CREDENTIALS_SET(pListenCfg->pAuthStore)) {
    haveAuth = 1;
  }
  if(pListenCfg->pAuthTokenId && pListenCfg->pAuthTokenId[0] != '\0') {
    haveToken = 1;
  }

  pConn = (CLIENT_CONN_T *) pListenCfg->pConnPool->pElements;
  if(pConn) {
    tsMax = pConn->pStreamerCfg0->liveQs[0].max;
    flvMax = pConn->pStreamerCfg0->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV].max;
    mkvMax = pConn->pStreamerCfg0->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV].max;
  }

  if((pListenCfg->urlCapabilities & URL_CAP_ROOTHTML)) {
    LOG(X_INFO("broadcast interface available at "URL_HTTP_FMT_STR"%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_ROOT_URL,
           (haveAuth ? " (Using auth)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), pListenCfg->max);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_LIVE)) {
    LOG(X_INFO("live available at "URL_HTTP_FMT_STR"%s%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_LIVE_URL,
           (haveAuth ? " (Using auth)" : ""),
           (haveToken ? " (Using token)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), pListenCfg->max);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_TSLIVE)) {
    LOG(X_INFO("tslive available at "URL_HTTP_FMT_STR"%s%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_TSLIVE_URL,
           (haveAuth ? " (Using auth)" : ""),
           (haveToken ? " (Using token)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), tsMax);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_TSHTTPLIVE)) {
    LOG(X_INFO("httplive available at "URL_HTTP_FMT_STR"%s%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_HTTPLIVE_URL,
           (haveAuth ? " (Using auth)" : ""),
           (haveToken ? " (Using token)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), pListenCfg->max);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_FLVLIVE)) {
    LOG(X_INFO("flvlive available at "URL_HTTP_FMT_STR"%s%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_FLVLIVE_URL,
           (haveAuth ? " (Using auth)" : ""),
           (haveToken ? " (Using token)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), flvMax);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_MKVLIVE)) {
    LOG(X_INFO("mkvlive available at "URL_HTTP_FMT_STR"%s%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_MKVLIVE_URL,
           (haveAuth ? " (Using auth)" : ""),
           (haveToken ? " (Using token)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), mkvMax);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_MOOFLIVE)) {
    LOG(X_INFO("dash available at "URL_HTTP_FMT_STR"%s%s%s%s max:%d"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_DASH_URL,
           (haveAuth ? " (Using auth)" : ""),
           (haveToken ? " (Using token)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""), pListenCfg->max);
  }

  if((pListenCfg->urlCapabilities & URL_CAP_STATUS)) {
    LOG(X_INFO("status available at "URL_HTTP_FMT_STR"%s%s%s"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_STATUS_URL,
            (haveAuth ? " (Using auth)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""));
  }

  if((pListenCfg->urlCapabilities & URL_CAP_PIP)) {
    LOG(X_INFO("pip available at "URL_HTTP_FMT_STR"%s%s%s"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_PIP_URL,
            (haveAuth ? " (Using auth)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""));
  }

  if((pListenCfg->urlCapabilities & URL_CAP_CONFIG)) {
    LOG(X_INFO("config available at "URL_HTTP_FMT_STR"%s%s%s"), 
           URL_HTTP_FMT_ARGS2(pListenCfg, FORMAT_NETADDR(sa, tmp, sizeof(tmp))), VSX_CONFIG_URL,
            (haveAuth ? " (Using auth)" : ""),
           (pListenCfg->pCfg->cfgShared.livepwd ? " (Using password)" : ""));
  }

  //fprintf(stderr, "LISTENER THREAD %s %s:%d active:%d cap: 0x%x\n", pListenCfg->netflags & NETIO_FLAG_SSL_TLS ? "SSL" : "", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), pListenCfg->active, pListenCfg->urlCapabilities);

  //
  // Service any client connections on the live listening port
  //
  rc = srvlisten_loop(pListenCfg, srv_cmd_proc);

  pthread_mutex_lock(&pListenCfg->mtx);
  pListenCfg->pnetsockSrv = NULL; 
  netio_closesocket(&netsocksrv);
  pthread_mutex_unlock(&pListenCfg->mtx);

  LOG(X_WARNING("HTTP listener thread exiting with code: %d"), rc);

  logutil_tid_remove(pthread_self());

  return;
}

static void srv_rtmp_proc(void *pfuncarg) {
  CLIENT_CONN_T *pConn = (CLIENT_CONN_T *) pfuncarg;
  STREAMER_CFG_T *pStreamerCfg = NULL;
  STREAMER_OUTFMT_T *pLiveFmt = NULL;
  STREAM_STATS_T *pstats = NULL;
  RTMP_CTXT_T rtmpCtxt;
  unsigned int numQFull = 0;
  char tmp[128];
  OUTFMT_CFG_T *pOutFmt = NULL;

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  if(pStreamerCfg && pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].do_outfmt) {

    pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP];

    if(pStreamerCfg->pMonitor && pStreamerCfg->pMonitor->active) {
      if(!(pstats = stream_monitor_createattach(pStreamerCfg->pMonitor,
             (const struct sockaddr *) &pConn->sd.sa, STREAM_METHOD_RTMP, STREAM_MONITOR_ABR_NONE))) {
      }
    }

    //
    // Add a livefmt cb
    //
    pOutFmt = outfmt_setCb(pLiveFmt, rtmp_addFrame, &rtmpCtxt, &pLiveFmt->qCfg, pstats, 1, pStreamerCfg->frameThin, &numQFull);

  } 

  if(pOutFmt) {

    memset(&rtmpCtxt, 0, sizeof(rtmpCtxt));

    rtmp_init(&rtmpCtxt, MAX(pLiveFmt->qCfg.maxPktLen, pLiveFmt->qCfg.growMaxPktLen));
    rtmpCtxt.pSd = &pConn->sd;
    rtmpCtxt.novid = pStreamerCfg->novid;
    rtmpCtxt.noaud = pStreamerCfg->noaud;
    rtmpCtxt.av.vid.pStreamerCfg = pStreamerCfg;
    rtmpCtxt.av.aud.pStreamerCfg = pStreamerCfg;
    rtmpCtxt.pAuthTokenId = pConn->pListenCfg->pAuthTokenId;

    //
    // Unpause the outfmt callback mechanism now that rtmp_init was called
    //
    outfmt_pause(pOutFmt, 0);

    LOG(X_INFO("Starting rtmp stream[%d] %d/%d to %s:%d"), pOutFmt->cbCtxt.idx, numQFull + 1, 
           pLiveFmt->max,
           FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

    rtmp_handle_conn(&rtmpCtxt);

    LOG(X_INFO("Ending rtmp stream[%d] to %s:%d"), pOutFmt->cbCtxt.idx,
           FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

    //
    // Remove the livefmt cb
    //
    outfmt_removeCb(pOutFmt);

    rtmp_close(&rtmpCtxt);

  } else {

    if(pstats) {
      //
      // Destroy automatically detaches the stats from the monitor linked list
      //
      stream_stats_destroy(&pstats, NULL);
    }

    LOG(X_WARNING("No rtmp resource available (max:%d) for %s:%d"), 
        (pLiveFmt ? pLiveFmt->max : 0),
        FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

  }

  netio_closesocket(&pConn->sd.netsocket);

  LOG(X_DEBUG("RTMP connection thread ended %s:%d"), FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), 
                                                     ntohs(INET_PORT(pConn->sd.sa)));

}


static void srvlisten_rtmplive_proc(void *pArg) {

  SRV_LISTENER_CFG_T *pListenCfg = (SRV_LISTENER_CFG_T *) pArg;
  CLIENT_CONN_T *pConn = NULL;
  NETIO_SOCK_T netsocksrv;
  int haveRtmpAuth = 0;
  struct sockaddr_storage  sa;
  int rc = 0;
  const int backlog = NET_BACKLOG_DEFAULT;
  char tmp[128];

  logutil_tid_add(pthread_self(), pListenCfg->tid_tag);

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pListenCfg->sa, sizeof(sa));
  netsocksrv.flags = pListenCfg->netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, backlog)) == INVALID_SOCKET) {
    logutil_tid_remove(pthread_self());
    return;
  }

  if((pConn = (CLIENT_CONN_T *) pListenCfg->pConnPool->pElements)) {
    if(IS_AUTH_CREDENTIALS_SET(&pConn->pStreamerCfg0->creds[STREAMER_AUTH_IDX_RTMP].stores[pListenCfg->idxCfg])) {
      //
      // RTMP server streaming credentials not implemented 
      //
      //haveRtmpAuth = 1;
    }
  }

  pthread_mutex_lock(&pListenCfg->mtx);
  pListenCfg->pnetsockSrv = &netsocksrv; 
  pthread_mutex_unlock(&pListenCfg->mtx);

  LOG(X_INFO("rtmp %sserver available at "URL_RTMP_FMT_STR"%s max:%d"), 
           ((pListenCfg->netflags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : ""), 
           URL_PROTO_FMT2_ARGS(
               (pListenCfg->netflags & NETIO_FLAG_SSL_TLS),
                 FORMAT_NETADDR(sa, tmp, sizeof(tmp))), ntohs(INET_PORT(sa)), 
           (haveRtmpAuth ? " (Using auth)" : ""), pListenCfg->max);

  //
  // Service any client connections on the live listening port
  //
  rc = srvlisten_loop(pListenCfg, srv_rtmp_proc);

  pthread_mutex_lock(&pListenCfg->mtx);
  pListenCfg->pnetsockSrv = NULL; 
  netio_closesocket(&netsocksrv);
  pthread_mutex_unlock(&pListenCfg->mtx);

  LOG(X_WARNING("rtmp listener thread exiting with code: %d"), rc);

  logutil_tid_remove(pthread_self());

  return;
}

int srvlisten_startrtmplive(SRV_LISTENER_CFG_T *pListenCfg) {
  pthread_t ptdRtmpLive;
  pthread_attr_t attrRtmpLive;
  const char *s;
  int rc = 0;

  if(!pListenCfg || !pListenCfg->pConnPool || !pListenCfg->pCfg || pListenCfg->max <= 0) {
    return -1;
  }

  if((pListenCfg->netflags & NETIO_FLAG_SSL_TLS) && !netio_ssl_enabled(1)) {
    LOG(X_ERROR("SSL not enabled"));
    return -1;
  }

  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(pListenCfg->tid_tag, sizeof(pListenCfg->tid_tag), "%s-rtmp", s);
  }
  pthread_attr_init(&attrRtmpLive);
  pthread_attr_setdetachstate(&attrRtmpLive, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdRtmpLive,
                    &attrRtmpLive,
                  (void *) srvlisten_rtmplive_proc,
                  (void *) pListenCfg) != 0) {

    LOG(X_ERROR("Unable to create rtmp listener thread"));
    return -1;
  }

  return rc;
}

static void srv_rtsp_proc(void *pfuncarg) {
  CLIENT_CONN_T *pConn = (CLIENT_CONN_T *) pfuncarg;
  RTSP_REQ_CTXT_T rtspCtxt;
  char tmp[128];

  memset(&rtspCtxt, 0, sizeof(rtspCtxt));
  rtspCtxt.pSd = &pConn->sd;
  rtspCtxt.pListenCfg = pConn->pListenCfg;

  if(pConn->pStreamerCfg0 && pConn->pStreamerCfg0->running >= 0) {
    rtspCtxt.pStreamerCfg = (STREAMER_CFG_T *) pConn->pStreamerCfg0;
  } else if(pConn->pStreamerCfg1 && pConn->pStreamerCfg1->running >= 0) {
    rtspCtxt.pStreamerCfg = (STREAMER_CFG_T *) pConn->pStreamerCfg1;
  }

  if(rtspCtxt.pStreamerCfg && ((STREAMER_CFG_T *)rtspCtxt.pStreamerCfg)->pRtspSessions) {
    rtsp_handle_conn(&rtspCtxt);
  }

  netio_closesocket(&pConn->sd.netsocket);

  LOG(X_DEBUG("RTSP connection thread ended %s:%d"), FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)),
                                                     ntohs(INET_PORT(pConn->sd.sa)));
}

static void srvlisten_rtsplive_proc(void *pArg) {

  SRV_LISTENER_CFG_T *pListenCfg = (SRV_LISTENER_CFG_T *) pArg;
  CLIENT_CONN_T *pConn = NULL;
  NETIO_SOCK_T netsocksrv;
  struct sockaddr_storage  sa;
  int haveRtspAuth = 0;
  char tmp[128];
  char bufses[32];
  const int backlog = NET_BACKLOG_DEFAULT;
  int rc = 0;

  logutil_tid_add(pthread_self(), pListenCfg->tid_tag);

  memset(&sa, 0, sizeof(sa));
  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pListenCfg->sa, sizeof(sa));
  netsocksrv.flags = pListenCfg->netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, backlog)) == INVALID_SOCKET) {
    logutil_tid_remove(pthread_self());
    return;
  }

  if((pConn = (CLIENT_CONN_T *) pListenCfg->pConnPool->pElements)) {
    if(IS_AUTH_CREDENTIALS_SET(&pConn->pStreamerCfg0->creds[STREAMER_AUTH_IDX_RTSP].stores[pListenCfg->idxCfg])) {
      haveRtspAuth = 1;
    }
  }

  pthread_mutex_lock(&pListenCfg->mtx);
  pListenCfg->pnetsockSrv = &netsocksrv;
  pthread_mutex_unlock(&pListenCfg->mtx);

  if(pListenCfg->pCfg->prtspsessiontimeout && *pListenCfg->pCfg->prtspsessiontimeout != 0) {
    snprintf(bufses, sizeof(bufses), ", timeout:%u sec", *pListenCfg->pCfg->prtspsessiontimeout);
  } else {
    bufses[0] = '\0';
  }

  LOG(X_INFO("rtsp %sserver available at "URL_RTSP_FMT_STR"%s max:%d%s"),
           ((pListenCfg->netflags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : ""),
           URL_PROTO_FMT2_ARGS(
               (pListenCfg->netflags & NETIO_FLAG_SSL_TLS),
                 FORMAT_NETADDR(sa, tmp, sizeof(tmp))), ntohs(INET_PORT(sa)), 
           (haveRtspAuth ? " (Using auth)" : ""), pListenCfg->max, bufses);

  //
  // Service any client connections on the rtsp listening port
  //
  rc = srvlisten_loop(pListenCfg, srv_rtsp_proc);

  pthread_mutex_lock(&pListenCfg->mtx);
  pListenCfg->pnetsockSrv = NULL; 
  netio_closesocket(&netsocksrv);
  pthread_mutex_unlock(&pListenCfg->mtx);

  LOG(X_DEBUG("rtsp listener thread exiting with code: %d"), rc);

  logutil_tid_remove(pthread_self());

  return ;
}

int srvlisten_startrtsplive(SRV_LISTENER_CFG_T *pListenCfg) {
  pthread_t ptdRtspLive;
  pthread_attr_t attrRtspLive;
  const char *s;
  int rc = 0;

  if(!pListenCfg || !pListenCfg->pConnPool || pListenCfg->max <= 0 || 
     !pListenCfg->pCfg || !pListenCfg->pCfg->cfgShared.pRtspSessions || 
     pListenCfg->pCfg->cfgShared.pRtspSessions->max <= 0) {
    return -1;
  }

  if((pListenCfg->netflags & NETIO_FLAG_SSL_TLS) && !netio_ssl_enabled(1)) {
    LOG(X_ERROR("SSL not enabled"));
    return -1;
  }

  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(pListenCfg->tid_tag, sizeof(pListenCfg->tid_tag), "%s-rtsp", s);
  }
  pthread_attr_init(&attrRtspLive);
  pthread_attr_setdetachstate(&attrRtspLive, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdRtspLive,
                    &attrRtspLive,
                  (void *) srvlisten_rtsplive_proc,
                  (void *) pListenCfg) != 0) {

    LOG(X_ERROR("Unable to create rtsp listener thread"));
    return -1;
  }

  return rc;
}

int srvlisten_starthttp(SRV_LISTENER_CFG_T *pListenCfg, int async) {
  pthread_t ptdTsLive;
  pthread_attr_t attrTsLive;
  const char *s;
  int rc = 0;

  if(!pListenCfg || !pListenCfg->pConnPool || pListenCfg->max <= 0 || !pListenCfg->pCfg) { 
    return -1;
  }

  if((pListenCfg->netflags & NETIO_FLAG_SSL_TLS) && !netio_ssl_enabled(1)) {
    LOG(X_ERROR("SSL not enabled"));
    return -1;
  }

  if(async) {

    if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
      snprintf(pListenCfg->tid_tag, sizeof(pListenCfg->tid_tag), "%s-http", s);
    }
    pthread_attr_init(&attrTsLive);
    pthread_attr_setdetachstate(&attrTsLive, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&ptdTsLive,
                      &attrTsLive,
                    (void *) srvlisten_http_proc,
                    (void *) pListenCfg) != 0) {

      LOG(X_ERROR("Unable to create live listener thread"));
      return -1;
    }

  } else {
    srvlisten_http_proc(pListenCfg);
    rc = 0;
  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
