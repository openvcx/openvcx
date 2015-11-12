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
#include "mgr/srvproxy.h"

typedef void (*THREAD_FUNC) (void *);
extern void srvmgr_client_proc(void *);

static void srvmgr_main_proc(void *pArg) {
  NETIO_SOCK_T netsocksrv;  
  int rc;
  struct sockaddr_storage sa;
  int haveAuth = 0;
  int backlog = 20;
  char buf[SAFE_INET_NTOA_LEN_MAX];
  SRV_MGR_LISTENER_CFG_T *pSrvListen = (SRV_MGR_LISTENER_CFG_T *) pArg;

  pSrvListen->listenCfg.urlCapabilities = URL_CAP_LIVE | URL_CAP_FILEMEDIA | 
                                          URL_CAP_TSHTTPLIVE | URL_CAP_MOOFLIVE | URL_CAP_IMG; 
  pSrvListen->listenCfg.urlCapabilities |= URL_CAP_STATUS;

  if(!pSrvListen->pStart->pCfg->cfgShared.disable_root_dirlist) {
    pSrvListen->listenCfg.urlCapabilities |= (URL_CAP_TMN | URL_CAP_LIST | URL_CAP_DIRLIST);
  }

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pSrvListen->listenCfg.sa, sizeof(sa));
  netsocksrv.flags = pSrvListen->listenCfg.netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, backlog)) == INVALID_SOCKET) {
    return;
  }

  pSrvListen->listenCfg.pnetsockSrv = &netsocksrv;

  if(pSrvListen->listenCfg.pAuthStore && IS_AUTH_CREDENTIALS_SET(pSrvListen->listenCfg.pAuthStore)) {
    haveAuth = 1;
  }

  LOG(X_INFO("Listening for requests on "URL_HTTP_FMT_STR" %s%s%s max: %d"),
              URL_HTTP_FMT_ARGS2(&pSrvListen->listenCfg, FORMAT_NETADDR(sa, buf, sizeof(buf))),
            (haveAuth ? " (Using auth)" : ""),
             pSrvListen->pStart->pCfg->cfgShared.uipwd ? " (Using password)" : "",
            (pSrvListen->pStart->pCfg->cfgShared.disable_root_dirlist ? " (Directory listing disabled)" : ""),
            pSrvListen->listenCfg.max);

  //
  // Service any client connections on the http listening port
  //
  rc = srvlisten_loop(&pSrvListen->listenCfg, srvmgr_client_proc);

  LOG(X_WARNING("HTTP%s %s:%d listener thread exiting with code: %d"), 
               ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "S" : ""),
               FORMAT_NETADDR(sa, buf, sizeof(buf)), ntohs(INET_PORT(sa)), rc);

  pSrvListen->listenCfg.pnetsockSrv = NULL;
  netio_closesocket(&netsocksrv);


}

static int start_listener(SRV_MGR_LISTENER_CFG_T *pMgrListenCfg, THREAD_FUNC thread_func, int async) {
  pthread_t ptd;
  pthread_attr_t attr;
  int rc = 0;

  if(!pMgrListenCfg || !pMgrListenCfg->pStart ||  !pMgrListenCfg->listenCfg.pConnPool || 
     pMgrListenCfg->listenCfg.max <= 0 || !pMgrListenCfg->listenCfg.pCfg) {
    return -1;
  }


  if((pMgrListenCfg->listenCfg.netflags & NETIO_FLAG_SSL_TLS) && !netio_ssl_enabled(1)) {
    LOG(X_ERROR("SSL not enabled"));
    return -1;
  }

  if(async) {

    PHTREAD_INIT_ATTR(&attr);

    if(pthread_create(&ptd,
                      &attr,
                    (void *) thread_func,
                    (void *) pMgrListenCfg) != 0) {

      LOG(X_ERROR("Unable to create live listener thread"));
      return -1;
    }

  } else {
    thread_func(pMgrListenCfg);
    rc = 0;
  }

  return rc;

}

static void close_listener(POOL_T *pPool) {
  SRV_MGR_CONN_T *pConn = NULL;

  // TODO: on signal , create mgr_close which closes all sockets and calls pool_close

  if(pPool->lock) {
    pthread_mutex_lock(&pPool->mtx);
  }
  pConn = (SRV_MGR_CONN_T *) pPool->pInUse;
  while(pConn) {
    netio_closesocket(&pConn->conn.sd.netsocket);
    pConn = (SRV_MGR_CONN_T *) pConn->conn.pool.pNext;
  }
  if(pPool->lock) {
    pthread_mutex_unlock(&pPool->mtx);
  }
  if(pPool->pElements) {
    pool_close(pPool, 1);
  }
}

static int init_listener(POOL_T *pPool, 
                         const SRV_MGR_START_CFG_T *pStart, 
                         SRV_MGR_LISTENER_CFG_T *pMgrListenerCfg,
                         unsigned int max,
                         SRV_LISTENER_CFG_T *pListenerSrc) {
  unsigned int idx;
  SRV_MGR_CONN_T *pConn;

  //
  // Create and init each available client connection instance
  //
  if(pool_open(pPool, max, sizeof(SRV_MGR_CONN_T), 1) != 0) {
    return -1;
  }

  pConn = (SRV_MGR_CONN_T *) pPool->pElements;
  while(pConn) {
    NETIOSOCK_FD(pConn->conn.sd.netsocket) = INVALID_SOCKET;
    pConn->conn.pCfg = &pStart->pCfg->cfgShared;
    //memcpy(&pConn->cfg, pStart, sizeof(pConn->cfg));
    pConn->pMgrCfg = pStart;
    pConn = (SRV_MGR_CONN_T *) pConn->conn.pool.pNext;
  }
 
  for(idx = 0; idx < SRV_LISTENER_MAX; idx++) {
    pMgrListenerCfg[idx].pStart = pStart;
    memcpy(&pMgrListenerCfg[idx].listenCfg, &pListenerSrc[idx], sizeof(pMgrListenerCfg[idx].listenCfg));
    pMgrListenerCfg[idx].listenCfg.max = max;
    pMgrListenerCfg[idx].listenCfg.pConnPool = pPool;
    pMgrListenerCfg[idx].listenCfg.pCfg = pStart->pCfg;
  }

  return 0;
}

static int check_other_listeners(const SRV_LISTENER_CFG_T *pListenerThis,
                                 unsigned int max,
                                 const SRV_LISTENER_CFG_T *pListener1,
                                 const SRV_LISTENER_CFG_T *pListener2,
                                 const SRV_LISTENER_CFG_T *pListener3) {
  unsigned int idx;
  int rc;
  unsigned int max1 = SRV_LISTENER_MAX;

  if(!pListenerThis) {
    return -1;
  }

  for(idx = 0; idx < max; idx++) {

    if(!pListenerThis[idx].active) {
      continue;
    }

    if(pListener1 && (rc = vsxlib_check_prior_listeners(pListener1, max1,  
                             (const struct sockaddr *) &pListenerThis[idx].sa)) > 0) {
      return idx + 1;
    }

    if(pListener2 && (rc = vsxlib_check_prior_listeners(pListener2, max1,  
                             (const struct sockaddr *) &pListenerThis[idx].sa)) > 0) {
      return idx + 1;
    }

    if(pListener3 && (rc = vsxlib_check_prior_listeners(pListener3, max1,  
                             (const struct sockaddr *) &pListenerThis[idx].sa)) > 0) {
      return idx + 1;
    }

  }

  return 0;
}

int srvmgr_start(SRV_MGR_PARAMS_T *pParams) {
  MEDIADB_DESCR_T mediaDb;
  HTTPLIVE_DATA_T httpLiveDatas[IXCODE_VIDEO_OUT_MAX];
  MOOFSRV_CTXT_T  moofCtxts[IXCODE_VIDEO_OUT_MAX];
  SRV_CONF_T *pConf = NULL;
  SRV_START_CFG_T cfg;
  VSXLIB_STREAM_PARAMS_T params;
  unsigned int idx;
  char tmp[128];
  char tmp2[32];
  char buf[VSX_MAX_PATH_LEN];
  char launchpath[VSX_MAX_PATH_LEN];
  char mediadir[VSX_MAX_PATH_LEN];
  char dbdir[VSX_MAX_PATH_LEN];
  const char *p;
  struct stat st;
  pthread_t ptdMediaDb;
  pthread_attr_t attrMediaDb;
  LIC_INFO_T lic;
  SRV_MGR_START_CFG_T start;
  SRV_LISTENER_CFG_T listenerTmp[SRV_LISTENER_MAX];
  SRV_MGR_LISTENER_CFG_T listenerHttp[SRV_LISTENER_MAX];
  SRV_MGR_LISTENER_CFG_T listenerRtmpProxy[SRV_LISTENER_MAX];
  SRV_MGR_LISTENER_CFG_T listenerRtspProxy[SRV_LISTENER_MAX];
  SRV_MGR_LISTENER_CFG_T listenerHttpProxy[SRV_LISTENER_MAX];
  POOL_T poolHttp;
  POOL_T poolRtmpProxy;
  POOL_T poolRtspProxy;
  POOL_T poolHttpProxy;
  AUTH_CREDENTIALS_STORE_T httpAuthStores[SRV_LISTENER_MAX];
  TIME_VAL tmstart = 0;
  MGR_NODE_LIST_T lbnodes;
  STREAM_STATS_MONITOR_T monitor;
  SRV_LISTENER_CFG_T listenRtmp[SRV_LISTENER_MAX];
  SRV_LISTENER_CFG_T listenRtsp[SRV_LISTENER_MAX];
  CPU_USAGE_PERCENT_T cpuUsage;
  MEM_SNAPSHOT_T memUsage;
  unsigned int outidx;
  const char *log_tag = "main";
  int rc = 0;
  
  if(!pParams) {
    return -1;
  }

  memset(&start, 0, sizeof(start));
  memset(&lic, 0, sizeof(lic));
  memset(&cfg, 0, sizeof(cfg));
  memset(&params, 0, sizeof(params));
  memset(&httpLiveDatas, 0, sizeof(httpLiveDatas));
  memset(&moofCtxts, 0, sizeof(moofCtxts));
  memset(&mediaDb, 0, sizeof(mediaDb));
  memset(listenerHttp, 0, sizeof(listenerHttp));
  memset(listenerRtmpProxy, 0, sizeof(listenerRtmpProxy));
  memset(listenerRtspProxy, 0, sizeof(listenerRtspProxy));
  memset(listenerHttpProxy, 0, sizeof(listenerHttpProxy));
  memset(&httpAuthStores, 0, sizeof(httpAuthStores));
  memset(&lbnodes, 0, sizeof(lbnodes));
  memset(&monitor, 0, sizeof(monitor));
  memset(&poolHttp, 0, sizeof(poolHttp));
  memset(&poolHttpProxy, 0, sizeof(poolHttpProxy));
  memset(&poolRtmpProxy, 0, sizeof(poolRtmpProxy));
  memset(&poolRtspProxy, 0, sizeof(poolRtspProxy));
  memset(&cpuUsage, 0, sizeof(cpuUsage));
  memset(&memUsage, 0, sizeof(memUsage));

#if defined(VSX_HAVE_SSL)
  //
  // TODO: This should be done in vsxlib_open, which is currently not called in mgr
  //
  pthread_mutex_init(&g_ssl_mtx, NULL);
#endif // VSX_HAVE_SSL

  pParams->pCfg = &cfg;
  cfg.cfgShared.pMediaDb = &mediaDb;
  cfg.pParams = &params;
  snprintf((char *) mediaDb.tn_suffix, sizeof(mediaDb.tn_suffix), "_xn");
  cfg.conf_file_name = MGR_CONF_FILE;
  cfg.conf_file_path = MGR_CONF_PATH;
  cfg.plogfile = pParams->logfile;
  cfg.verbosity = params.verbosity = g_verbosity;

  start.pCfg = &cfg;
  start.pProcList = &pParams->procList;
  start.ptmstart = &tmstart;
  start.plic = &lic;
  start.pListenerRtmpProxy = listenerRtmpProxy;
  start.pListenerRtspProxy = listenerRtspProxy;
  start.pListenerHttpProxy = listenerHttpProxy;

  params.logmaxsz = LOGGER_MAX_FILE_SZ;
  params.logrollmax = LOGGER_MAX_FILES;
  params.disable_root_dirlist = pParams->disable_root_dirlist;
  params.httpaccesslogfile = pParams->httpaccesslogfile;

  //
  // Load properties which are common between vsx & vsxmgr
  //
  if(!(pConf = srv_init_conf(pParams->listenaddr[0], pParams->mediadir, 
                      pParams->homedir, pParams->confpath,
                      pParams->dbdir, !pParams->nodb, 
                      pParams->curdir, 
                      MGR_RTMPPROXY_MAX, 
                      MGR_RTSPPROXY_MAX, 
                      MGR_CONNECTIONS_MAX,
                      &cfg, 
                      httpAuthStores))) {
    return -1;
  }

  if(params.httpmax == 0) {
    params.httpmax = MGR_CONNECTIONS_DEFAULT;
  }

  vsxlib_initlog(cfg.verbosity, cfg.plogfile, cfg.phomedir, params.logmaxsz, params.logrollmax, log_tag);
  http_log_setfile(params.httpaccesslogfile);

  LOG(X_INFO("Read configuration from %s"), cfg.pconfpath);

  //
  // Initialize threading limits
  //
  vsxlib_init_threads(&params);

  //
  // Check if we're running in load balancer mode
  //
  if(!pParams->lbnodesfile && (p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LBNODES_CONF))) {
    pParams->lbnodesfile = p;
  }

  if(pParams->lbnodesfile) {
    if((rc = mgrnode_init(pParams->lbnodesfile, &lbnodes)) < 0) {
      conf_free(pConf);
      return -1;
    } else if(lbnodes.count <= 0) {
      LOG(X_ERROR("No active nodes loaded from %s"), pParams->lbnodesfile);
      conf_free(pConf);
      mgrnode_close(&lbnodes);
      return -1;
    }
    start.pLbNodes = &lbnodes;
    cfg.usedb = 0;
    params.rtmplivemax = 0;
    params.rtsplivemax = 0;
    params.flvlivemax = 0;
    LOG(X_INFO("Loaded %d active nodes loaded from %s"), lbnodes.count, pParams->lbnodesfile);
  }

  if(!start.pLbNodes) {
  
    if(cfg.usedb && cfg.pmediadir != NULL && cfg.pmediadir[0] != '\0' &&
      fileops_stat(cfg.pmediadir, &st) != 0 &&
      mediadb_prepend_dir(cfg.phomedir, cfg.pmediadir, mediadir, sizeof(mediadir)) >= 0 &&
      fileops_stat(mediadir, &st) == 0) {
      cfg.pmediadir = mediadir;
    }
  
    //
    // Get config entries specific to the mgr
    //
  
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LAUNCHMEDIA)) && p[0] != '\0') {
      start.plaunchpath = p;
    } else {
      mediadb_prepend_dir(cfg.phomedir, MGR_LAUNCHCHILD, launchpath, sizeof(launchpath));
      start.plaunchpath = launchpath;
    }
  
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_PORTRANGE)) &&
      get_port_range(p, &pParams->procList.minStartPort, &pParams->procList.maxStartPort) < 0) {
  
      LOG(X_ERROR("Invalid %s"), SRV_CONF_KEY_PORTRANGE);
      conf_free(pConf);
      return -1;
    }
  
    if(!params.disable_root_dirlist) {
      if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DISABLE_LISTING)) && p[0] != '\0') {
        params.disable_root_dirlist = IS_CONF_VAL_TRUE(p);
      }
    }
  
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_MEDIA)) && p[0] != '\0') {
      start.pProcList->maxInstances = atoi(p); 
    }
  
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_MEDIA_XCODE)) && p[0] != '\0') {
      start.pProcList->maxXcodeInstances = atoi(p); 
    }
  
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_MBPS)) && p[0] != '\0') {
      start.pProcList->maxMbbpsTot = atoi(p); 
    }

    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_CLIENTS_PER_PROC)) && p[0] != '\0') {
      start.pProcList->maxClientsPerProc = atoi(p); 
    }
    if(start.pProcList->maxClientsPerProc == 0) {
      start.pProcList->maxClientsPerProc = VSX_LIVEQ_DEFAULT;
    }
  
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_EXPIRE_CHILD_SEC)) && p[0] != '\0') {
      start.pProcList->procexpiresec = atoi(p); 
    }
    if(start.pProcList->procexpiresec <= 0) {
      start.pProcList->procexpiresec = PROCDB_EXPIRE_IDLE_SEC;
    }
  
    if(fileops_stat(start.plaunchpath, &st) != 0) {
      LOG(X_ERROR("Launch script %s does not exist"), start.plaunchpath);
      conf_free(pConf);
      return -1;
    }
  
    if(cfg.pdbdir == NULL) {
      mediadb_prepend_dir(cfg.pmediadir, VSX_DB_PATH, buf, sizeof(buf));
      cfg.pdbdir = buf;
    }
  
    if((cfg.cfgShared.disable_root_dirlist = params.disable_root_dirlist)) {
      LOG(X_INFO("Root level directory listing disabled."));
    }
  
    if((cfg.cfgShared.enable_symlink = params.enable_symlink)) {
      LOG(X_DEBUG("Symlink following enabled."));
    }

    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_STATUS_MONITOR)) && IS_CONF_VAL_TRUE(p)) {
      start.pMonitor = &monitor;
    }

    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_CPU_MONITOR)) && IS_CONF_VAL_TRUE(p)) {
      start.pProcList->pCpuUsage = &cpuUsage;
      start.pCpuUsage = start.pProcList->pCpuUsage;
    }
    if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MEM_MONITOR)) && IS_CONF_VAL_TRUE(p)) {
      start.pProcList->pMemUsage = &memUsage;
      start.pMemUsage = start.pProcList->pMemUsage;
    }
  
    //
    // create a 'unique' key for the mediaDir
    //
    idx = mp2ts_crc((const unsigned char *) cfg.pdbdir, strlen(cfg.pdbdir));
    snprintf(tmp2, sizeof(tmp2), "%2.2x%2.2x%2.2x%2.2x",
            (((unsigned char *)&idx)[0]),  (((unsigned char *)&idx)[1]),
            (((unsigned char *)&idx)[2]),  (((unsigned char *)&idx)[3]));
    mediadb_prepend_dir(cfg.pdbdir, tmp2, dbdir, sizeof(dbdir));
    cfg.pdbdir = dbdir;
  
    mediaDb.homeDir= cfg.phomedir;
    mediaDb.dbDir = cfg.pdbdir;
    mediaDb.mediaDir = cfg.pmediadir;
    mediaDb.avcthumb = cfg.pavcthumb;
    mediaDb.avcthumblog = cfg.pavcthumblog;
    mediadb_parseprefixes(cfg.pignoredirprfx, mediaDb.ignoredirprefixes,
                          MEDIADB_PREFIXES_MAX);
    mediadb_parseprefixes(cfg.pignorefileprfx, mediaDb.ignorefileprefixes,
                          MEDIADB_PREFIXES_MAX);
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
  
      //
      // Set HTTP Live properties
      //
      if(cfg.phttplivedir) {
        strncpy(httpLiveDatas[outidx].dir, cfg.phttplivedir, sizeof(httpLiveDatas[outidx].dir) - 1);
      } else {
        mediadb_prepend_dir(cfg.phomedir, VSX_HTTPLIVE_PATH,
                            httpLiveDatas[outidx].dir, sizeof(httpLiveDatas[outidx].dir));
      }
      httplive_format_path_prefix(httpLiveDatas[outidx].fileprefix,
                                  sizeof(httpLiveDatas[outidx].fileprefix),
                                  outidx, HTTPLIVE_TS_NAME_PRFX, NULL);
  
      cfg.cfgShared.pHttpLiveDatas[outidx] = &httpLiveDatas[outidx];
  
      //
      // Set DASH Live properties
      //
      if(cfg.pmoofdir) {
        strncpy((char *) moofCtxts[outidx].dashInitCtxt.outdir, cfg.pmoofdir, 
                sizeof(moofCtxts[outidx].dashInitCtxt.outdir) - 1);
      } else {
        mediadb_prepend_dir((char *) cfg.phomedir, VSX_MOOF_PATH, (char *) moofCtxts[outidx].dashInitCtxt.outdir, 
                          sizeof(moofCtxts[outidx].dashInitCtxt.outdir));
      }
  
      cfg.cfgShared.pMoofCtxts[outidx] = &moofCtxts[outidx];
  
    }

    VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - homedir:'%s', mediaDir:'%s', dbDir:'%s'"),
                    cfg.phomedir, mediaDb.mediaDir, mediaDb.dbDir));

    if(procdb_create(&pParams->procList) < 0) {
      conf_free(pConf);
      mgrnode_close(start.pLbNodes);
      return -1;
    }

    LOG(X_INFO("Home dir: '%s', Media dir: '%s', database %s, %d max connections"), 
             cfg.phomedir, cfg.pmediadir ? cfg.pmediadir : "<not set>",
             cfg.usedb ? cfg.pdbdir : "disabled", params.httpmax);

    if(start.pProcList->maxInstances > 0 || start.pProcList->maxXcodeInstances > 0 ||
       start.pProcList->maxMbbpsTot > 0) {
      LOG(X_INFO(SRV_CONF_KEY_MAX_MEDIA": %d, "SRV_CONF_KEY_MAX_MEDIA_XCODE": %d, "
                 SRV_CONF_KEY_MAX_MBPS": %d"),
         start.pProcList->maxInstances, start.pProcList->maxXcodeInstances,
         start.pProcList->maxMbbpsTot);
    }

  } else { // end of if!(start.pLbNodes

    LOG(X_INFO("Handling up to %d max connections"), params.httpmax);

  }

#if defined(VSX_HAVE_LICENSE)

  if(license_init(&lic, cfg.licfilepath, cfg.phomedir) < 0) {
    conf_free(pConf);
    mgrnode_close(start.pLbNodes);
    return -1;
  } else if((lic.capabilities & LIC_CAP_NO_MGR)) {
    LOG(X_ERROR("VSXMgr not enabled in license"));
    conf_free(pConf);
    mgrnode_close(start.pLbNodes);
    return -1;
  } 

  tmstart = timer_GetTime();

#endif //VSX_HAVE_LICENSE

  if(start.pMonitor) {
    if(stream_monitor_start(start.pMonitor, NULL, params.statdumpintervalms) < 0) {
      conf_free(pConf);
      mgrnode_close(start.pLbNodes);
      return -1;
    }
  }

  //
  // Create and init each available http connection instance
  //
  if(init_listener(&poolHttp, &start, listenerHttp, params.httpmax, cfg.listenHttp) < 0) {
    conf_free(pConf);
    mgrnode_close(start.pLbNodes);
    stream_monitor_stop(start.pMonitor);
    return -1;
  }

  memset(cfg.listenHttp, 0, sizeof(cfg.listenHttp));
  for(idx = 0; idx < SRV_LISTENER_MAX; idx++) {
    memcpy(&listenerTmp[idx], &listenerHttp[idx].listenCfg, sizeof(listenerTmp[idx]));
  }

  if(cfg.usedb) {

    if(!mediaDb.mediaDir) {
      LOG(X_CRITICAL("Media directory not set.  Please specify your media directory by modifying '%s' or by passing the '--media=' command line option"),
               MGR_CONF_PATH);
      conf_free(pConf);
      mgrnode_close(start.pLbNodes);
      stream_monitor_stop(start.pMonitor);
      return -1;
    }

    //
    // Start the media database management thread
    //
    PHTREAD_INIT_ATTR(&attrMediaDb);

    if(pthread_create(&ptdMediaDb,
                      &attrMediaDb,
                      (void *) mediadb_proc,
                      (void *) &mediaDb) != 0) {

      LOG(X_ERROR("Unable to create media database thread"));
      conf_free(pConf);
      mgrnode_close(start.pLbNodes);
      stream_monitor_stop(start.pMonitor);
      return -1;
    }
  }

  memset(&listenRtmp, 0, sizeof(listenRtmp));
  if(rc >= 0 && params.rtmplivemax > 0 && params.rtmpliveaddr[0] &&
     (rc = vsxlib_parse_listener((const char **) params.rtmpliveaddr, SRV_LISTENER_MAX,
                                  listenRtmp, URL_CAP_RTMPLIVE, NULL)) > 0) {

    //
    // Start the RTMP proxy
    //
    if((rc = check_other_listeners(listenRtmp, SRV_LISTENER_MAX,
                                   listenerTmp, NULL, NULL)) > 0) {
      LOG(X_WARNING("RTMP server listener %s:%d cannot be shared with another protocol"),
          FORMAT_NETADDR(listenRtmp[rc - 1].sa, tmp, sizeof(tmp)), htons(INET_PORT(listenRtmp[rc - 1].sa)));

    } else {

      //
      // Create and init each available client connection instance
      //
      if((rc = init_listener(&poolRtmpProxy, &start, listenerRtmpProxy, params.rtmplivemax, listenRtmp)) >= 0) {
        //
        // Start the RTMP listener(s)
        //
        for(idx = 0; idx < sizeof(listenerRtmpProxy) / sizeof(listenerRtmpProxy[0]); idx++) {
          if(listenerRtmpProxy[idx].listenCfg.active &&
             ((rc = vsxlib_ssl_initserver(&params, &listenerRtmpProxy[idx].listenCfg)) < 0 ||
              (rc = start_listener(&listenerRtmpProxy[idx], srvmgr_rtmpproxy_proc, 1) < 0))) {
            break;
          }
        }
      } // end of init_listener...

    }

  } // end of if(params.rtmplivemax ...

  memset(listenRtsp, 0, sizeof(listenRtsp));
  if(rc >= 0 && params.rtsplivemax > 0 && params.rtspliveaddr[0] &&
     (rc = vsxlib_parse_listener((const char **) params.rtspliveaddr, SRV_LISTENER_MAX,
                                  listenRtsp, URL_CAP_RTSPLIVE, NULL)) > 0) {

    //
    // Start the RTSP proxy
    //
    if((rc = check_other_listeners(listenRtsp, SRV_LISTENER_MAX,
                                   listenerTmp, listenRtmp,  NULL)) > 0) {
      LOG(X_WARNING("RTSP server listener %s:%d cannot be shared with another protocol"),
          FORMAT_NETADDR(listenRtsp[rc - 1].sa, tmp, sizeof(tmp)), htons(INET_PORT(listenRtsp[rc - 1].sa)));
    } else {

      //
      // Create and init each available client connection instance
      //
      if((rc = init_listener(&poolRtspProxy, &start, listenerRtspProxy, params.rtsplivemax, listenRtsp)) >= 0) {

        //
        // Start the RTSP listener(s)
        //
        for(idx = 0; idx < sizeof(listenerRtspProxy) / sizeof(listenerRtspProxy[0]); idx++) {
          if(listenerRtspProxy[idx].listenCfg.active &&
             ((rc = vsxlib_ssl_initserver(&params, &listenerRtspProxy[idx].listenCfg)) < 0 ||
             (rc = start_listener(&listenerRtspProxy[idx], srvmgr_rtspproxy_proc, 1) < 0))) {
            break;
          }
        }
      } // end of init_listener...

    }

  } // end of if(params.rtsplivemax > 0...

  if(rc >= 0 && params.flvlivemax > 0 && params.flvliveaddr[0] &&
     (rc = vsxlib_parse_listener((const char **) params.flvliveaddr, SRV_LISTENER_MAX,
                                  cfg.listenHttp, URL_CAP_FLVLIVE, NULL)) > 0) {

    //
    // Start the HTTP / FLV proxy
    //
    if((rc = check_other_listeners(cfg.listenHttp, SRV_LISTENER_MAX,
                                   listenerTmp, listenRtmp, listenRtsp)) > 0) {
      LOG(X_WARNING("HTTP (proxy) server listener %s:%d cannot be shared with another protocol"),
          FORMAT_NETADDR(cfg.listenHttp[rc - 1].sa, tmp, sizeof(tmp)), htons(INET_PORT(cfg.listenHttp[rc - 1].sa)));
    } else {

      //
      // Create and init each available client connection instance
      //
      if((rc = init_listener(&poolHttpProxy, &start, listenerHttpProxy, params.flvlivemax, cfg.listenHttp)) >= 0) {

        //
        // Start the HTTP / FLV listener(s)
        //
        for(idx = 0; idx < sizeof(listenerHttpProxy) / sizeof(listenerHttpProxy[0]); idx++) {
          if(listenerHttpProxy[idx].listenCfg.active &&
             ((rc = vsxlib_ssl_initserver(&params, &listenerHttpProxy[idx].listenCfg)) < 0 ||
              (rc = start_listener(&listenerHttpProxy[idx], srvmgr_httpproxy_proc, 1) < 0))) {
            break;
          }
        }
      } // end of if(init_listener...

    }

  } // end of if(params.flvlivemax > 0 ... 

  if(rc >= 0) {

    //
    // Check if we have any SSL/TLS http listeners
    //
    for(idx = 0; idx < sizeof(listenerHttp) / sizeof(listenerHttp[0]); idx++) {
      if((listenerHttp[idx].listenCfg.netflags & NETIO_FLAG_SSL_TLS)) {
         start.enable_ssl_childlisteners = 1;
         break;
      }
    }

    //
    // Start the HTTP listener(s)
    //
    for(idx = 0; idx < sizeof(listenerHttp) / sizeof(listenerHttp[0]); idx++) {
      if(listenerHttp[idx].listenCfg.active) {

        if(((rc = vsxlib_ssl_initserver(&params, &listenerHttp[idx].listenCfg)) < 0 ||
           (rc = start_listener(&listenerHttp[idx], srvmgr_main_proc, 1) < 0))) {
          break;
        }
      } // end of if(idx...
    } // end of for(idx...

  }

  if(rc >= 0) {
    while(g_proc_exit == 0) {
      usleep(500000);
    }
  }

  LOG(X_DEBUG("Cleaning up"));

  close_listener(&poolHttp);
  close_listener(&poolRtmpProxy);
  close_listener(&poolRtspProxy);
  close_listener(&poolHttpProxy);

  if(start.pLbNodes) {
    procdb_destroy(&pParams->procList);
  }
  conf_free(pConf);
  mgrnode_close(start.pLbNodes);
  stream_monitor_stop(start.pMonitor);

  return rc;
}

