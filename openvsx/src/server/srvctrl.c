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

#if defined(VSX_HAVE_SERVERMODE)

static void init_conn(CLIENT_CONN_T *pClient, SRV_CTRL_T srvCtrl[2],
                      SRV_LISTENER_CFG_T *pListenCfg,
                      SRV_CFG_T *pCfgShared) {

  NETIOSOCK_FD(pClient->sd.netsocket) = INVALID_SOCKET;
  pClient->sd.netsocket.stunSock.stunFlags = 0;
  pClient->sd.netsocket.flags = 0;
  pClient->ppSrvCtrl[0] = &srvCtrl[0];
  pClient->ppSrvCtrl[1] = &srvCtrl[1];
  pClient->pStreamerCfg0 = &(srvCtrl[0].curCfg.streamerCfg);
  pClient->pStreamerCfg1 = &(srvCtrl[1].curCfg.streamerCfg);
  pClient->pMtx0 = &(srvCtrl[0].mtx);
  pClient->pMtx1 = &(srvCtrl[1].mtx);
  pClient->pListenCfg = pListenCfg;
  pClient->pCfg = pCfgShared;

}

static int srv_start(SRV_START_CFG_T *pCfg) {

  int rc;
  SESSION_CACHE_T sessionCache;
  HTTPLIVE_DATA_T httpLiveData;
  SRV_CTRL_PROC_DATA_T srvCtrlData[2];
  SRV_CTRL_T srvCtrl[2];
  pthread_t ptdSrvCtrl[2];
  pthread_attr_t attrSrvCtrl[2];
  pthread_t ptdMediaDb;
  pthread_attr_t attrMediaDb;
  struct stat st;
  unsigned int idx;
  char tmp2[32];
  char cwd[VSX_MAX_PATH_LEN];
  CLIENT_CONN_T *pClient;
  POOL_T poolHttp;
  //POOL_T poolRtmp;
  //POOL_T poolRtsp;
  unsigned int maxConnectionsTsLive = 0;
  //unsigned int maxConnectionsRtmpLive = 0;
  //unsigned int maxConnectionsTot;
  unsigned int maxConnectionsDefault;
  int rawxmit = 0;
  char buf[VSX_MAX_PATH_LEN];
  char buf2[VSX_MAX_PATH_LEN];
  STREAMER_LIVEQ_T *pLiveQ;
  unsigned int outidx;

  //TODO: enable rtsp, rtmp in server mode
  //pCfg->listenRtmp[0].max = pCfg->listenRtsp[0].max = 0;
  //pCfg->listenRtsp[0].max = 0;

  maxConnectionsTsLive = MAX(pCfg->pParams->tslivemax, 1);
  maxConnectionsDefault = MAX(pCfg->pParams->httpmax, 4);

  memset(&poolHttp, 0, sizeof(poolHttp));
  //memset(&poolRtmp, 0, sizeof(poolRtmp));
  //memset(&poolRtsp, 0, sizeof(poolRtsp));
  memset(srvCtrl, 0, sizeof(srvCtrl));
  memset(&sessionCache, 0, sizeof(sessionCache));

  //
  //Ensure any old file remnants are purged
  //
  memset(&httpLiveData, 0, sizeof(httpLiveData));
  httpLiveData.fs.fp = FILEOPS_INVALID_FP;

  //uint16_t rtp_seq_at_end = 0;

  //
  // Set / Init log file properties
  //
  //if(pCfg->plogfile) {
  //  logger_SetFile(pCfg->plogdir, pCfg->plogfile, LOGGER_MAX_FILES, LOGGER_MAX_FILE_SZ,
  //               LOG_OUTPUT_DEFAULT | LOG_FLAG_USELOCKING);
  //  logger_AddStderr(S_INFO, LOG_OUTPUT_DEFAULT_STDERR);
  //}

  if(pCfg->pmediadir == NULL || pCfg->pmediadir[0] == '\0') {

    LOG(X_WARNING("Media directory not set.  Please specify your media directory by modifying '%s' or by passing the '--media=' command line option"),
               VSX_CONF_PATH);
    //return -1;
    pCfg->usedb = 0;
    if(pCfg->pmediadir) {
      pCfg->pmediadir = NULL;
    }
  } else if(fileops_stat(pCfg->pmediadir, &st) != 0) {
    LOG(X_WARNING("Media directory '%s' does not exist"), pCfg->pmediadir);    
    pCfg->pmediadir = NULL;
    pCfg->usedb = 0;
  }

  LOG(X_INFO(" "));
  LOG(X_INFO("Starting in broadcast control mode"));
  LOG(X_WARNING("This mode of operation is no longer supported!"));
  LOG(X_INFO(" "));

  if(pCfg->outiface) {
#if defined(WIN32) && !defined(__MINGW32__)
    pktgen_SetInterface(pCfg->outiface);

    //
    // Perform packet generation engine initialization
    //
    if(pktgen_Init() != 0) {
      return -1;
    }
    //
    // Perform mac address lookup table initialization
    //
    if(maclookup_Init(ACTQOS_MAC_EXPIRE_SEC) != 0) {
      pktgen_Close();
      return -1;
    }
    rawxmit = 1;
#else
    LOG(X_INFO("Configuration key %s is ignored because your OS is good enough."),
               SRV_CONF_KEY_INTERFACE);
#endif // WIN32
  }


  if(pCfg->phomedir == NULL) {

    if(fileops_setcwd(".") != 0) {
      LOG(X_ERROR("Unable to change into current directory"));
      return -1;
    }

    if(!fileops_getcwd(cwd, sizeof(cwd))) {
      LOG(X_ERROR("Unable to get current directory path"));
      return -1;
    }
    pCfg->phomedir = cwd;
  }

  if(pCfg->pdbdir == NULL) {
    mediadb_prepend_dir(pCfg->pmediadir, VSX_DB_PATH, buf2, sizeof(buf2));
    pCfg->pdbdir = buf2;
  }

#if defined(VSX_HAVE_LICENSE)

  //
  // Load the license
  //
  if(pCfg->licfilepath && fileops_stat(pCfg->licfilepath, &st) != 0) {
    mediadb_prepend_dir(pCfg->phomedir, pCfg->licfilepath, buf, sizeof(buf));
    pCfg->licfilepath = buf;
  }

  if(license_init((LIC_INFO_T *) &srvCtrl[0].curCfg.streamerCfg.lic, 
                  pCfg->licfilepath, NULL) < 0) {
    return -1;
  }
  pCfg->licfilepath = NULL;
  memcpy((LIC_INFO_T *) &srvCtrl[1].curCfg.streamerCfg.lic, 
         &srvCtrl[0].curCfg.streamerCfg.lic,
         sizeof(srvCtrl[1].curCfg.streamerCfg.lic));
#endif // VSX_HAVE_LICENSE


  //
  // create a 'unique' key for the mediaDir 
  //
  idx = mp2ts_crc((const unsigned char *) pCfg->pdbdir, strlen(pCfg->pdbdir));
  snprintf(tmp2, sizeof(tmp2), "%2.2x%2.2x%2.2x%2.2x",
            (((unsigned char *)&idx)[0]),  (((unsigned char *)&idx)[1]),
            (((unsigned char *)&idx)[2]),  (((unsigned char *)&idx)[3]));
  mediadb_prepend_dir(pCfg->pdbdir, tmp2, buf, sizeof(buf));
  pCfg->pdbdir = buf;

  pCfg->cfgShared.pMediaDb->homeDir = pCfg->phomedir;
  pCfg->cfgShared.pMediaDb->dbDir = pCfg->pdbdir;
  pCfg->cfgShared.pMediaDb->mediaDir = pCfg->pmediadir;
  pCfg->cfgShared.pMediaDb->avcthumb = pCfg->pavcthumb;
  pCfg->cfgShared.pMediaDb->avcthumblog = pCfg->pavcthumblog;
  mediadb_parseprefixes(pCfg->pignoredirprfx, pCfg->cfgShared.pMediaDb->ignoredirprefixes, 
                        MEDIADB_PREFIXES_MAX);
  mediadb_parseprefixes(pCfg->pignorefileprfx, pCfg->cfgShared.pMediaDb->ignorefileprefixes, 
                        MEDIADB_PREFIXES_MAX);
  //pCfg->cfgShared.pListenRtmp = pCfg->listenRtmp;
  //pCfg->cfgShared.pListenRtsp = pCfg->listenRtsp;
  pCfg->cfgShared.pListenHttp = pCfg->listenHttp;
  pCfg->cfgShared.pSessionCache = &sessionCache;
  for(outidx = 0; outidx < 1; outidx++) {
    pCfg->cfgShared.pHttpLiveDatas[outidx] = &httpLiveData;
  }

  if(pCfg->usedb) {

    //
    // Start the media database management thread
    //
    PHTREAD_INIT_ATTR(&attrMediaDb);

    if(pthread_create(&ptdMediaDb,
                      &attrMediaDb,
                      (void *) mediadb_proc,
                      (void *) pCfg->cfgShared.pMediaDb) != 0) {

      LOG(X_ERROR("Unable to create media database thread"));
      return -1;
    }
  }

  LOG(X_INFO("Home dir: %s Media dir: %s"), pCfg->phomedir, 
             pCfg->pmediadir ? pCfg->pmediadir : "<not set>" );

  //
  // Set HTTP Live properties
  // 
  if(pCfg->phttplivedir) {

    //
    // Try to use absolute path.
    // on win32, each thread may have a cwd different than phomedir 
    //
    mediadb_prepend_dir(pCfg->phomedir, pCfg->phttplivedir, 
                        httpLiveData.dir, sizeof(httpLiveData.dir));

    if(fileops_stat(httpLiveData.dir, &st) != 0) {
      strncpy(httpLiveData.dir, pCfg->phttplivedir, sizeof(httpLiveData.dir) - 1);
    }

  } else {
    mediadb_prepend_dir(pCfg->phomedir, VSX_HTTPLIVE_PATH, 
                        httpLiveData.dir, sizeof(httpLiveData.dir));
  }
  httpLiveData.duration =  pCfg->httpliveduration;
  httpLiveData.fs.fp = FILEOPS_INVALID_FP;
  httplive_close(&httpLiveData);

  //
  // Create and init each client connection instance
  //
  if(pool_open(&poolHttp, maxConnectionsDefault + maxConnectionsTsLive, 
               sizeof(CLIENT_CONN_T), 1) != 0) {
    return -1;
  }
  //if(pool_open(&poolRtmp, maxrtmplive, sizeof(CLIENT_CONN_T), 1) != 0) {
  //  return -1;
  //}
  //if(pool_open(&poolRtmp, maxrtsplive, sizeof(CLIENT_CONN_T), 1) != 0) {
  //  return -1;
  //}

  pClient = (CLIENT_CONN_T *) poolHttp.pElements;
  while(pClient) {
    init_conn(pClient, srvCtrl, &pCfg->listenHttp[0], &pCfg->cfgShared);
    pClient = (CLIENT_CONN_T *) pClient->pool.pNext;
  }

  //pClient = (CLIENT_CONN_T *) poolRtsp.pElements;
  //while(pClient) {
  //  init_conn(pClient);
  //  pClient = pClient->pNext;
  //}

  //pClient = (CLIENT_CONN_T *) poolRtmp.pElements;
  //while(pClient) {
  //  init_conn(pClient);
  //  pClient = pClient->pNext;
  //}


  //memset(&netsocksrv, 0, sizeof(netsocksrv));
  //if((netsocksrv.sock = net_listen(&pCfg->saListenHttp, 5)) == INVALID_SOCKET) {
  //  pool_close(&poolHttp, 0);
    //pool_close(&poolRtmp, 0);
    //pool_close(&poolRtsp, 0);
  //  return -1;
  //}

  //
  // Create the two server control threads responsible for capture / streaming.
  // One is for file playback, the other for live playback.  These are run on
  // seperate threads to allow pausing of one type of playback in favor of the
  // other, and then resuming.
  // 

  for(idx = 0; idx < 2; idx++) {

    srvCtrl[idx].curCfg.streamerCfg.pmaxRtp = &pCfg->maxrtp;
    srvCtrl[idx].nextCfg.streamerCfg.pmaxRtp = &pCfg->maxrtp;
    if(vsxlib_alloc_destCfg(&srvCtrl[idx].curCfg.streamerCfg, 0) < 0 ||
       vsxlib_alloc_destCfg(&srvCtrl[idx].nextCfg.streamerCfg, 0) < 0) {
      return -1;
    }
 
    //TODO: broken & non-configurable
    srvCtrl[idx].curCfg.captureCfg.capActions[0].frvidqslots = VIDFRAMEQ_LEN_DEFAULT;
    srvCtrl[idx].curCfg.captureCfg.capActions[0].fraudqslots = AUDFRAMEQ_LEN_DEFAULT;
    srvCtrl[idx].nextCfg.captureCfg.capActions[0].frvidqslots = VIDFRAMEQ_LEN_DEFAULT;
    srvCtrl[idx].nextCfg.captureCfg.capActions[0].fraudqslots = AUDFRAMEQ_LEN_DEFAULT;

    PHTREAD_INIT_ATTR(&attrSrvCtrl[idx]);
    pthread_mutex_init(&srvCtrl[idx].mtx, NULL);

    srvCtrl[idx].pCfg = &pCfg->cfgShared;
    srvCtrl[idx].isRunning = 1;
    srvCtrl[idx].curCfg.cmd = SRV_CMD_NONE;
    pthread_mutex_init(&srvCtrl[idx].curCfg.streamerCfg.mtxStrmr, NULL);
    //srvCtrl[idx].curCfg.streamerCfg.verbosity = pCfg->verbosity;
    srvCtrl[idx].curCfg.streamerCfg.running = STREAMER_STATE_ERROR;
    if(idx == 0) {
      srvCtrl[idx].curCfg.streamerCfg.cfgrtp.ssrcs[0] = CREATE_RTP_SSRC();
      srvCtrl[idx].curCfg.streamerCfg.cfgrtp.ssrcs[1] = CREATE_RTP_SSRC();
    } else {
      srvCtrl[idx].curCfg.streamerCfg.cfgrtp.ssrcs[0] = srvCtrl[0].curCfg.streamerCfg.cfgrtp.ssrcs[0];
      srvCtrl[idx].curCfg.streamerCfg.cfgrtp.ssrcs[1] = srvCtrl[0].curCfg.streamerCfg.cfgrtp.ssrcs[1];
    }
    srvCtrl[idx].curCfg.streamerCfg.status.useStatus = 1;
    srvCtrl[idx].curCfg.streamerCfg.cfgstream.includeSeqHdrs = 1;
    srvCtrl[idx].curCfg.streamerCfg.u.cfgmp2ts.useMp2tsContinuity = 1;
    srvCtrl[idx].curCfg.streamerCfg.u.cfgmp2ts.lastPatContIdx = MP2TS_HDR_TS_CONTINUITY;
    srvCtrl[idx].curCfg.streamerCfg.u.cfgmp2ts.lastPmtContIdx = MP2TS_HDR_TS_CONTINUITY;
    srvCtrl[idx].curCfg.streamerCfg.u.cfgmp2ts.firstProgId = MP2TS_FIRST_PROGRAM_ID;
    srvCtrl[idx].curCfg.streamerCfg.rawxmit = rawxmit;
    srvCtrl[idx].curCfg.streamerCfg.verbosity = pCfg->verbosity;


    //for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    for(outidx = 0; outidx < 1; outidx++) {

      //if(!pSrv->pStreamerCfg->xcode.vid.out[outidx].active) {
      //  continue;
      //}

      pLiveQ = &srvCtrl[idx].curCfg.streamerCfg.liveQs[outidx];
      pthread_mutex_init(&pLiveQ->mtx, NULL);

      if((*((unsigned int *) &pLiveQ->max) = maxConnectionsTsLive) > 0 && 
         !(pLiveQ->pQs = (PKTQUEUE_T **) avc_calloc(maxConnectionsTsLive, sizeof(PKTQUEUE_T *)))) {
        *((unsigned int *) &pLiveQ->max) = 0;
      }
      pLiveQ->qCfg.id = STREAMER_QID_TSLIVE + outidx;
      pLiveQ->qCfg.maxPkts = pCfg->pParams->tsliveq_slots;
      pLiveQ->qCfg.maxPktLen = pCfg->pParams->tsliveq_szslot;
      pLiveQ->qCfg.growMaxPkts = pCfg->pParams->tsliveq_slotsmax;
      pLiveQ->qCfg.concataddenum = MP2TS_LEN;

      *((unsigned int *) &srvCtrl[idx].curCfg.streamerCfg.liveQ2s[outidx].max) = 0;
      pthread_mutex_init(&srvCtrl[idx].curCfg.streamerCfg.liveQ2s[outidx].mtx, NULL);
    }


    srvCtrl[idx].recordCfg.streamsOut.maxStreams = 1;
    srvCtrl[idx].recordCfg.streamsOut.sp[0].stream1.fp = FILEOPS_INVALID_FP;
    //srvCtrl[idx].prtp_seq_at_end = &rtp_seq_at_end;

    srvCtrlData[idx].ppSrvCtrl[0] = &srvCtrl[0];
    srvCtrlData[idx].ppSrvCtrl[1] = &srvCtrl[1];
    srvCtrlData[idx].idx = idx;


    if(pthread_create(&ptdSrvCtrl[idx],
                    &attrSrvCtrl[idx],
                    (void *) srv_ctrl_proc,
                    (void *) &srvCtrlData[idx]) != 0) {

      LOG(X_ERROR("Unable to create control thread[%d]"), idx);
      //netio_closesocket(&netsocksrv);
      return -1;
    }

  }


  if(pCfg->pParams->tslivemax > 0) {
    if((rc = vsxlib_parse_listener((const char **) pCfg->pParams->tsliveaddr, SRV_LISTENER_MAX,
                            pCfg->listenHttp, URL_CAP_TSLIVE, NULL)) < 0) {
      pool_close(&poolHttp, 0);
      return -1;
    }
  }

  if(pCfg->pParams->httplivemax > 0) {
    if((rc = vsxlib_parse_listener((const char **) pCfg->pParams->httpliveaddr, SRV_LISTENER_MAX,
                            pCfg->listenHttp, URL_CAP_TSHTTPLIVE, NULL)) < 0) {
      pool_close(&poolHttp, 0);
      return -1;
    }
  }

  //
  // Ensure there is at least one /tslive and one /httplive listener
  // if not explicitly specified in the config via 'tslive=' and 'httplive='
  //
  if(!srv_ctrl_findlistener(pCfg->listenHttp, SRV_LISTENER_MAX, URL_CAP_TSLIVE, 0, 0, 0)) {
    pCfg->listenHttp[0].urlCapabilities |= URL_CAP_TSLIVE;
  }
  if(!srv_ctrl_findlistener(pCfg->listenHttp, SRV_LISTENER_MAX, URL_CAP_TSHTTPLIVE, 0, 0, 0)) {
    pCfg->listenHttp[0].urlCapabilities |= URL_CAP_TSHTTPLIVE;
  }


  for(idx = 0; idx < SRV_LISTENER_MAX; idx++) {

    //fprintf(stderr, "LISTENER[%d] %s %s:%d active:%d cap: 0x%x\n", idx, pCfg->listenHttp[idx].netflags & NETIO_FLAG_SSL_TLS ? "SSL" : "", inet_ntoa(pCfg->listenHttp[idx].sain.sin_addr), ntohs(pCfg->listenHttp[idx].sain.sin_port), pCfg->listenHttp[idx].active, pCfg->listenHttp[idx].urlCapabilities);

    if(pCfg->listenHttp[idx].active) {

      //
      // Create and listen on the UI HTTP port
      //
      pCfg->listenHttp[idx].max = maxConnectionsDefault + maxConnectionsTsLive;
      pCfg->listenHttp[idx].pConnPool = &poolHttp;
      pCfg->listenHttp[idx].pCfg = pCfg;
      if(idx > 0) {

        if((rc = vsxlib_ssl_initserver(pCfg->pParams, &pCfg->listenHttp[idx])) < 0 ||
           (rc = srvlisten_startmediasrv(&pCfg->listenHttp[idx], 1)) < 0) {
          pool_close(&poolHttp, 0);
          return -1;
        }
      }
    }
  }

  //
  // Start the first listener synchronously, thus blocking here
  //
  if((rc = vsxlib_ssl_initserver(pCfg->pParams, &pCfg->listenHttp[0])) < 0 ||
     (rc = srvlisten_startmediasrv(&pCfg->listenHttp[0], 0)) < 0) {
    pool_close(&poolHttp, 0);
    return -1;
  }

  //
  // Perform cleanup
  //
  for(idx = 0; idx < 2; idx++) {
    pthread_mutex_destroy(&srvCtrl[idx].curCfg.streamerCfg.mtxStrmr);

    pthread_mutex_destroy(&srvCtrl[idx].curCfg.streamerCfg.liveQs[0].mtx);
    if(srvCtrl[idx].curCfg.streamerCfg.liveQs[0].pQs) {
      avc_free((void **) &srvCtrl[idx].curCfg.streamerCfg.liveQs[0].pQs);
    }

    pthread_mutex_destroy(&srvCtrl[idx].curCfg.streamerCfg.liveQ2s[0].mtx);
    if(srvCtrl[idx].curCfg.streamerCfg.liveQ2s[0].pQs) {
      avc_free((void **) &srvCtrl[idx].curCfg.streamerCfg.liveQ2s[0].pQs);
    }

    pthread_mutex_destroy(&srvCtrl[idx].mtx);

    vsxlib_free_destCfg(&srvCtrl[idx].curCfg.streamerCfg);
    vsxlib_free_destCfg(&srvCtrl[idx].nextCfg.streamerCfg);
  }

  httplive_close(&httpLiveData);
  pool_close(&poolHttp, 3000);
  //pool_close(&poolRtsp, 3000);
  //pool_close(&poolRtmp, 3000);

  return 0;
}

int srv_start_conf(VSXLIB_STREAM_PARAMS_T *pParams,
                   const char *homeDirArg, const char *dbDirArg, int usedb, 
                   const char *curdir) {

  int rc;
  SRV_CONF_T *pConf = NULL;
  SRV_START_CFG_T cfg;
  MEDIADB_DESCR_T mediaDb;

  if(!pParams || !pParams->pPrivate) {
    //
    // If pPrivate has not been set, it is a good indication that no prior call to vsxlib_open 
    // was made
    //
    return -1;
  }

  memset(&cfg, 0, sizeof(cfg));
  memset(&mediaDb, 0, sizeof(mediaDb));
  cfg.cfgShared.pMediaDb = &mediaDb;
  cfg.pParams = pParams;
  snprintf((char *) mediaDb.tn_suffix, sizeof(mediaDb.tn_suffix), "_tn");
  cfg.conf_file_name = VSX_CONF_FILE;
  cfg.conf_file_path = VSX_CONF_PATH;
  cfg.plogfile = pParams->logfile;

  if(!(pConf = srv_init_conf(pParams->inputs[0], pParams->dir, homeDirArg, pParams->confpath, 
                dbDirArg, usedb, curdir, STREAMER_OUTFMT_MAX, STREAMER_LIVEQ_MAX, VSX_CONNECTIONS_MAX, &cfg, NULL))) {
    return -1;
  }

  vsxlib_initlog(pParams->verbosity, cfg.plogfile, cfg.phomedir, pParams->logmaxsz, pParams->logrollmax, NULL);
  LOG(X_INFO("Read configuration from %s"), cfg.confpath);

  g_proc_exit_flags |= PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED;

  rc = srv_start(&cfg);

  //
  // Force mediadb_proc thread to exti
  //
  g_proc_exit = 1;

  conf_free(pConf);

  return rc;
}


#endif // VSX_HAVE_SERVERMODE
