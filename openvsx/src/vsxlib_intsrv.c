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



#if defined(VSX_HAVE_STREAMER)

void vsxlib_closeOutFmt(STREAMER_OUTFMT_T *pLiveFmt) {
  if(pLiveFmt) {
    pLiveFmt->do_outfmt = 0;
    avc_free((void **) &pLiveFmt->poutFmts);
    pthread_mutex_destroy(&pLiveFmt->mtx);
  }
}

void vsxlib_closeRtspInterleaved(STREAMER_CFG_T *pStreamerCfg) {
  unsigned int outidx, idx;

  if(!pStreamerCfg) {
    return;
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUG( "Close RTSP Interleaved context")));

  //
  // Signall all rtsp rtp queue listeners to exit
  //
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    pthread_mutex_lock(&pStreamerCfg->liveQ2s[outidx].mtx);
    for(idx = 0; idx < pStreamerCfg->liveQ2s[outidx].max; idx++) {
      if(pStreamerCfg->liveQ2s[outidx].pQs[idx]) {
        pStreamerCfg->liveQ2s[outidx].pQs[idx]->quitRequested = 1;
        pktqueue_wakeup(pStreamerCfg->liveQ2s[outidx].pQs[idx]);
      }
    }
    pthread_mutex_unlock(&pStreamerCfg->liveQ2s[outidx].mtx);
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active) {
      continue;
    }

    pthread_mutex_destroy(&pStreamerCfg->liveQ2s[outidx].mtx);
    if(pStreamerCfg->liveQ2s[outidx].pQs) {
      avc_free((void **) &pStreamerCfg->liveQ2s[outidx].pQs);
    }
  }

}

int vsxlib_initRtspInterleaved(STREAMER_CFG_T *pStreamerCfg, unsigned int maxRtspLive, 
                               const VSXLIB_STREAM_PARAMS_T *pParams) {
  int rc = 0;
  STREAMER_LIVEQ_T *pLiveQ2 = NULL;
  unsigned int outidx;

  if(!pStreamerCfg) {
    return - 1;
  }

  VSX_DEBUG_RTSP( LOG(X_DEBUG( "Init RTSP Interleaved context maxRtsp: %d"), maxRtspLive));

  // 
  // Setup RTSP server parameters
  //
  if(maxRtspLive > 0) {

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      if(!(pLiveQ2 = &pStreamerCfg->liveQ2s[outidx]) || pLiveQ2->pQs) {
        continue; 
      } else if((*((unsigned int *) &pLiveQ2->max) = maxRtspLive) > 0 &&
       !(pLiveQ2->pQs = (PKTQUEUE_T **) avc_calloc(maxRtspLive, sizeof(PKTQUEUE_T *)))) {
        return -1;
      }

      pthread_mutex_init(&pLiveQ2->mtx, NULL);
      pLiveQ2->qCfg.id = STREAMER_QID_RTSP_INTERLEAVED + outidx;
      pLiveQ2->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgRtsp.maxPkts;
      pLiveQ2->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtsp.maxPktLen;
      pLiveQ2->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtsp.growMaxPktLen;

    }

  }

  pStreamerCfg->pRtspSessions->psessionTmtSec = &pParams->rtspsessiontimeout;
  pStreamerCfg->pRtspSessions->prefreshtimeoutviartcp = &pParams->rtsprefreshtimeoutviartcp;

  return rc;
}

void vsxlib_closeServer(SRV_PARAM_T *pSrv) {
  //STREAMER_OUTFMT_T *pLiveFmt;
  unsigned int idx, outidx;
  CLIENT_CONN_T *pConn;

  if(pSrv) {

    if(pSrv->pStreamerCfg->action.do_httplive) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        httplive_close(&pSrv->pStreamerCfg->pStorageBuf->httpLiveDatas[outidx]);
      }
      pSrv->pStreamerCfg->action.do_httplive = 0;
    }
    if(pSrv->pStreamerCfg->action.do_rtsplive) {
      rtspsrv_close(pSrv->startcfg.cfgShared.pRtspSessions);
      pSrv->pStreamerCfg->action.do_rtsplive = 0;
    }
    if(pSrv->pStreamerCfg->action.do_rtmplive) {
      pSrv->pStreamerCfg->action.do_rtmplive = 0;
    }
    if(pSrv->pStreamerCfg->action.do_flvlive) {
      pSrv->pStreamerCfg->action.do_flvlive = 0;
    }
    if(pSrv->pStreamerCfg->action.do_mkvlive) {
      pSrv->pStreamerCfg->action.do_mkvlive = 0;
    }

    //TODO: mutex protect
    //if(pSrv->rtspLive.psockSrv) {
    //  netio_closesocket(pSrv->rtmpLive.pnetsockSrv);
    //}

    //
    // Signall all tslive queue listeners to exit
    //
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pSrv->pStreamerCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      pthread_mutex_lock(&pSrv->pStreamerCfg->liveQs[outidx].mtx);
      for(idx = 0; idx < pSrv->pStreamerCfg->liveQs[outidx].max; idx++) {
        if(pSrv->pStreamerCfg->liveQs[outidx].pQs[idx]) {
          pSrv->pStreamerCfg->liveQs[outidx].pQs[idx]->quitRequested = 1;
          pktqueue_wakeup(pSrv->pStreamerCfg->liveQs[outidx].pQs[idx]);
        }
      }
      pthread_mutex_unlock(&pSrv->pStreamerCfg->liveQs[outidx].mtx);
    }
/*
    //
    // Signall all rtsp rtp queue listeners to exit
    //
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      pthread_mutex_lock(&pSrv->pStreamerCfg->liveQ2s[outidx].mtx);
      for(idx = 0; idx < pSrv->pStreamerCfg->liveQ2s[outidx].max; idx++) {
        if(pSrv->pStreamerCfg->liveQ2s[outidx].pQs[idx]) {
          pSrv->pStreamerCfg->liveQ2s[outidx].pQs[idx]->quitRequested = 1;
          pktqueue_wakeup(pSrv->pStreamerCfg->liveQ2s[outidx].pQs[idx]);
        }
      }
      pthread_mutex_unlock(&pSrv->pStreamerCfg->liveQ2s[outidx].mtx);
    }
*/
#if 1
    //
    // Go through each pool in use list and close the respective socket
    // to have the pool thread exit
    //
    pthread_mutex_lock(&pSrv->poolHttp.mtx);
    pConn = (CLIENT_CONN_T *) pSrv->poolHttp.pInUse;
    while(pConn) {
      //fprintf(stderr, "HTTP POOL SOCK:%d free:%d/%d\n", pConn->sd.netsocket.sock, pSrv->poolHttp.freeElements, pSrv->poolHttp.numElements);
      netio_closesocket(&pConn->sd.netsocket);
      pConn = (CLIENT_CONN_T *) pConn->pool.pNext;
    }
    pthread_mutex_unlock(&pSrv->poolHttp.mtx);

    pthread_mutex_lock(&pSrv->poolRtmp.mtx);
    pConn = (CLIENT_CONN_T *) pSrv->poolRtmp.pInUse;
    while(pConn) {
      //fprintf(stderr, "RTMP POOL SOCK:%d free:%d/%d\n", pConn->sd.socket, pSrv->poolRtmp.freeElements, pSrv->poolRtmp.numElements);
      netio_closesocket(&pConn->sd.netsocket);
      pConn = (CLIENT_CONN_T *) pConn->pool.pNext;
    }
    pthread_mutex_unlock(&pSrv->poolRtmp.mtx);

    pthread_mutex_lock(&pSrv->poolRtsp.mtx);
    pConn = (CLIENT_CONN_T *) pSrv->poolRtsp.pInUse;
    while(pConn) {
      //fprintf(stderr, "RTSP POOL SOCK:%d free:%d/%d\n", pConn->sd.socket, pSrv->poolRtsp.freeElements, pSrv->poolRtsp.numElements);
      netio_closesocket(&pConn->sd.netsocket);
      pConn = (CLIENT_CONN_T *) pConn->pool.pNext;
    }
    pthread_mutex_unlock(&pSrv->poolRtsp.mtx);

    for(idx = 0; idx < SRV_LISTENER_MAX_HTTP; idx++) {
      if(pSrv->startcfg.listenHttp[idx].active) {
        pthread_mutex_lock(&pSrv->startcfg.listenHttp[idx].mtx);
        if(pSrv->startcfg.listenHttp[idx].pnetsockSrv) {
          netio_closesocket(pSrv->startcfg.listenHttp[idx].pnetsockSrv);
        }
        pthread_mutex_unlock(&pSrv->startcfg.listenHttp[idx].mtx);
        pthread_mutex_destroy(&pSrv->startcfg.listenHttp[idx].mtx);
      }
    }
    for(idx = 0; idx < SRV_LISTENER_MAX_RTMP; idx++) {
      if(pSrv->startcfg.listenRtmp[idx].active) {
        pthread_mutex_lock(&pSrv->startcfg.listenRtmp[idx].mtx);
        if(pSrv->startcfg.listenRtmp[idx].pnetsockSrv) {
          netio_closesocket(pSrv->startcfg.listenRtmp[idx].pnetsockSrv);
        }
        pthread_mutex_unlock(&pSrv->startcfg.listenRtmp[idx].mtx);
        pthread_mutex_destroy(&pSrv->startcfg.listenRtmp[idx].mtx);
      }
    }
    for(idx = 0; idx < SRV_LISTENER_MAX_RTSP; idx++) {
      if(pSrv->startcfg.listenRtsp[idx].active) {
        pthread_mutex_lock(&pSrv->startcfg.listenRtsp[idx].mtx);
        if(pSrv->startcfg.listenRtsp[idx].pnetsockSrv) {
          netio_closesocket(pSrv->startcfg.listenRtsp[idx].pnetsockSrv);
        }
        pthread_mutex_unlock(&pSrv->startcfg.listenRtsp[idx].mtx);
        pthread_mutex_destroy(&pSrv->startcfg.listenRtsp[idx].mtx);
      }
    }
#endif // 1

    pool_close(&pSrv->poolHttp, 3000);
    pool_close(&pSrv->poolRtmp, 3000);
    pool_close(&pSrv->poolRtsp, 3000);


    vsxlib_closeOutFmt(&pSrv->pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP]);
/*
    pLiveFmt = &pSrv->pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP];
    pLiveFmt->do_outfmt = 0;
    if(pLiveFmt) {
      avc_free((void **) &pLiveFmt->poutFmts);
    }
    pthread_mutex_destroy(&pLiveFmt->mtx);
*/

    vsxlib_closeOutFmt(&pSrv->pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV]);
/*
    pLiveFmt = &pSrv->pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV];
    pLiveFmt->do_outfmt = 0;
    if(pLiveFmt) {
      avc_free((void **) &pLiveFmt->poutFmts);
    }
    pthread_mutex_destroy(&pLiveFmt->mtx);
*/
    vsxlib_closeOutFmt(&pSrv->pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV]);

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pSrv->pStreamerCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      pthread_mutex_destroy(&pSrv->pStreamerCfg->liveQs[outidx].mtx);
      if(pSrv->pStreamerCfg->liveQs[outidx].pQs) {
        avc_free((void **) &pSrv->pStreamerCfg->liveQs[outidx].pQs);
      }

      //pthread_mutex_destroy(&pSrv->pStreamerCfg->liveQ2s[outidx].mtx);
      //if(pSrv->pStreamerCfg->liveQ2s[outidx].pQs) {
      //  avc_free((void **) &pSrv->pStreamerCfg->liveQ2s[outidx].pQs);
      //}

    }

    //
    // Close pStreamerCfg->liveQ2s used by RTSP interleaved RTP output
    //
    vsxlib_closeRtspInterleaved(pSrv->pStreamerCfg);

    pthread_mutex_destroy(&pSrv->mtx);

    pSrv->isinit = 0;
  }
}


static void init_conn(POOL_T *pPool, SRV_PARAM_T *pSrv) {
  CLIENT_CONN_T *pConn;

  pConn = (CLIENT_CONN_T *) pPool->pElements;
  while(pConn) {

    NETIOSOCK_FD(pConn->sd.netsocket) = INVALID_SOCKET;
    pConn->sd.netsocket.stunSock.stunFlags = 0;
    pConn->sd.netsocket.flags = 0;
    pConn->pCfg = &pSrv->startcfg.cfgShared;
    pConn->pStreamerCfg0 = pSrv->pStreamerCfg;
    pConn->pMtx0 = &pSrv->mtx;

    pConn = (CLIENT_CONN_T *) pConn->pool.pNext;
  }

}

typedef struct CB_PARSE_HTTPLIVE_BITRATES {
  unsigned int    publishedBitrates[IXCODE_VIDEO_OUT_MAX];
  unsigned int    outidx;
} CB_PARSE_HTTPLIVE_BITRATES_T;

static int cbparse_entry_httplivebitrates(void *pArg, const char *p) {
  int rc = 0;
  CB_PARSE_HTTPLIVE_BITRATES_T *pBitrates = (CB_PARSE_HTTPLIVE_BITRATES_T *) pArg;

  if(pBitrates->outidx < IXCODE_VIDEO_OUT_MAX) {
    pBitrates->publishedBitrates[pBitrates->outidx++] = atoi(p);
  }
  
  return rc;
}

int vsxlib_setupServer(SRV_PARAM_T *pSrv, const VSXLIB_STREAM_PARAMS_T *pParams, int do_httplive_segmentor) {
  int rc = 0;
  unsigned int numConn = 0;
  unsigned int outidx;
  unsigned int idx;
  STREAMER_OUTFMT_T *pLiveFmt;
  STREAMER_LIVEQ_T *pLiveQ;
  //STREAMER_LIVEQ_T *pLiveQ2;
  HTTPLIVE_DATA_T *pHttpLiveData = NULL;
  CB_PARSE_HTTPLIVE_BITRATES_T httpLiveBitrates;
  unsigned int numHttp = 0;
  char tmp[128];
  char fileprefix[128];
  STREAMER_CFG_T *pStreamerCfg = pSrv->pStreamerCfg;
  HTTPLIVE_DATA_T *pHttpLiveDatas[IXCODE_VIDEO_OUT_MAX];
  //unsigned int numXcodeOut = 0;
  unsigned int maxHttp = pParams->httpmax;
  unsigned int maxTsLive = pParams->tslivemax;
  unsigned int maxHttpLive = pParams->httplivemax;
  unsigned int maxMoofLive = pParams->dashlivemax;
  unsigned int maxRtmpLive = pParams->rtmplivemax;
  unsigned int maxFlvLive = pParams->flvlivemax;
  unsigned int maxMkvLive = pParams->mkvlivemax;
  unsigned int maxRtspLive = pParams->rtsplivemax;
  unsigned int maxRtspLiveConn = maxRtspLive;
  unsigned int maxWww = pParams->livemax;
  unsigned int maxStatus = pParams->statusmax;
  unsigned int maxPip = pParams->piphttpmax;
  unsigned int maxConfig = pParams->configmax;
  int is_pip = 0;

  if(pStreamerCfg != &pStreamerCfg->pStorageBuf->streamerCfg) {
    //TODO: need to create seperate STREAM_STORAGE pip contexts for these
    is_pip = 1;
    if(pParams->httpliveaddr[0] || do_httplive_segmentor) {
      return -1;
    } else if(pParams->pipaddr[0]) {
      return -1;
    } else if(pParams->dashliveaddr[0]) {
      return -1;
    }
  }

  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
    pHttpLiveDatas[idx] = &pStreamerCfg->pStorageBuf->httpLiveDatas[idx];
  }

  if(!pParams->liveaddr[0]) {
    maxWww = 0;
  }
  if(!pParams->tsliveaddr[0]) {
    maxTsLive = 0;
  }
  if(!pParams->httpliveaddr[0]) {
    maxHttpLive = 0;
  }
  if(!pParams->dashliveaddr[0]) {
    maxMoofLive = 0;
  }
  if(!pParams->flvliveaddr[0]) {
    maxFlvLive = 0;
  }
  if(!pParams->mkvliveaddr[0]) {
    maxMkvLive = 0;
  }
  if(!pParams->rtmpliveaddr[0]) {
    maxRtmpLive = 0;
  }
  if(!pParams->rtspliveaddr[0]) {
    maxRtspLive = 0;
    maxRtspLiveConn = 0;
  }
  if(!pParams->pipaddr[0]) {
    maxPip = 0;
  }
  if(!pParams->statusaddr[0]) {
    maxStatus = 0;
  }
  if(!pParams->configaddr[0]) {
    maxConfig = 0;
  }
#if defined(VSX_HAVE_LICENSE)

  if((pParams->rtmpliveaddr[0] || pParams->flvliveaddr[0]) &&
     (pStreamerCfg->lic.capabilities & LIC_CAP_NO_RTMP)) {
    LOG(X_ERROR("RTMP/FLV server not enabled in license"));
    return -1;
  }
  if(pParams->rtspliveaddr[0] && (pStreamerCfg->lic.capabilities & LIC_CAP_NO_RTSP)) {
    LOG(X_ERROR("RTSP server not enabled in license"));
    return -1;
  }
  if(pParams->tsliveaddr[0] && (pStreamerCfg->lic.capabilities & LIC_CAP_NO_TSLIVE)) {
    LOG(X_ERROR(SRV_CONF_KEY_TSLIVE" output not enabled in license"));
    return -1;
  }
  if((pParams->httpliveaddr[0] > 0 || do_httplive_segmentor) &&
     (pStreamerCfg->lic.capabilities & LIC_CAP_NO_HTTPLIVE)) {
    LOG(X_ERROR("HTTPLive output not enabled in license"));
    return -1;
  }

#endif // VSX_HAVE_LICENSE

  pSrv->isinit = 1;
  //pSrv->pStreamerCfg = pStreamerCfg;

  if(devtype_loadcfg(pParams->devconfpath) < 0 ) {
    LOG(X_WARNING("Invalid device profile configuration file %s"), pParams->devconfpath);
  }

  //
  // Set up the http live segmentor
  //
  if(pParams->httpliveaddr[0] || do_httplive_segmentor) {

    if(pParams->httplivedir && pParams->httplivedir[0] != DIR_DELIMETER) {
      mediadb_prepend_dir(NULL, pParams->httplivedir,
                pHttpLiveDatas[0]->dir, sizeof(pHttpLiveDatas[0]->dir));
    } else if(pParams->httplivedir) {
      strncpy(pHttpLiveDatas[0]->dir, pParams->httplivedir, sizeof(pHttpLiveDatas[0]->dir));
    }

    if(pParams->httplivefileprefix) {
      strncpy(pHttpLiveDatas[0]->fileprefix, pParams->httplivefileprefix,
            sizeof(pHttpLiveDatas[0]->fileprefix));
    }
    if(pParams->httpliveuriprefix) {
      strncpy(pHttpLiveDatas[0]->uriprefix, pParams->httpliveuriprefix,
            sizeof(pHttpLiveDatas[0]->uriprefix));
    }

    //if(pParams->dash_ts_segments == 1 && pParams->httplive_chunkduration <= 0) {
    //fprintf(stderr, "MOOF MAX:%.3f\n", pParams->moofMp4MaxDurationSec);
    //}
    //fprintf(stderr, "MOOF MAX:%.3f\n", pParams->moofMp4MaxDurationSec);
    //fprintf(stderr, "HTTPLIVE DURATION: %.3f\n", pParams->httplive_chunkduration);

    pHttpLiveDatas[0]->fs.fp = FILEOPS_INVALID_FP;
    pHttpLiveDatas[0]->duration = pParams->httplive_chunkduration;
    pHttpLiveDatas[0]->pStreamerCfg = pStreamerCfg;
    pHttpLiveDatas[0]->nodelete = pParams->httplive_nodelete;
    pHttpLiveDatas[0]->indexCount  = pParams->httpliveindexcount;

    httplive_close(pHttpLiveDatas[0]);
    if(pParams->httplivebitrates) {
      memset(&httpLiveBitrates, 0, sizeof(httpLiveBitrates));
      strutil_parse_csv(cbparse_entry_httplivebitrates, &httpLiveBitrates, pParams->httplivebitrates);
    }

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      if(outidx == 0) {
        pHttpLiveDatas[0]->active = 1;
      }

      //
      // Set the multi-bitrate output stream specific output bitrate
      //
      if(pStreamerCfg->xcode.vid.out[outidx].active) {
        pHttpLiveDatas[outidx]->active = 1;
        pHttpLiveDatas[outidx]->publishedBitrate = httpLiveBitrates.publishedBitrates[outidx] * 1000;
        //fprintf(stderr, "pub bitrate[%d]:%d\n", outidx, httpLiveBitrates.bitrates[outidx]);
        pHttpLiveDatas[outidx]->autoBitrate = ((float) pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut *
                                                   HTTPLIVE_BITRATE_MULTIPLIER);
        //numXcodeOut++;
      }
    }

    memset(fileprefix, 0, sizeof(fileprefix));
    strncpy(fileprefix, pHttpLiveDatas[0]->fileprefix, sizeof(fileprefix) - 1);

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      pSrv->startcfg.cfgShared.pHttpLiveDatas[outidx] = pHttpLiveDatas[outidx];
      pHttpLiveDatas[outidx]->outidx = outidx;
      //pHttpLiveDatas[outidx]->count = numXcodeOut > 0 ? numXcodeOut : 1;

      if(outidx > 0) {

        memcpy(pHttpLiveDatas[outidx], pHttpLiveDatas[0],
               (char *) &pHttpLiveDatas[outidx]->count - (char *) pHttpLiveDatas[outidx]);

        //
        // Assign a unique fileprefix index to multiple output indexes
        //
        //pSrv->httpLiveDatas[outidx].useoutidxprefix = 1;
        httplive_format_path_prefix(pHttpLiveDatas[outidx]->fileprefix,
                                    sizeof(pHttpLiveDatas[outidx]->fileprefix),
                                    outidx, fileprefix, NULL);
        //snprintf(pHttpLiveDatas[outidx]->fileprefix, sizeof(pHttpLiveDatas[outidx]->fileprefix),
        //         "%d%s", outidx + 1, fileprefix); 
      }

      if(pHttpLiveDatas[outidx]->active) {

        if(httplive_init(pHttpLiveDatas[outidx]) != 0) {
          vsxlib_closeServer(pSrv);
          return -1;
        } else if(pParams->dash_ts_segments == 1) {


          //
          // Setup DASH MPD creation for .ts segments
          //
          //fprintf(stderr, "DASH FOR TS [%d].active:%d\n\n", outidx, pHttpLiveDatas[outidx]->active);
          pHttpLiveDatas[outidx]->pMpdMp2tsCtxt = &pStreamerCfg->pStorageBuf->mpdMp2tsCtxt[outidx];
          pStreamerCfg->pStorageBuf->moofRecordCtxts[outidx].dashInitCtxt.outdir_ts = pHttpLiveDatas[outidx]->dir;

          if(mpd_init_from_httplive(&pStreamerCfg->pStorageBuf->mpdMp2tsCtxt[outidx],
                   pHttpLiveDatas[outidx],
                   pStreamerCfg->pStorageBuf->moofRecordCtxts[outidx].dashInitCtxt.outdir) != 0) {
           vsxlib_closeServer(pSrv);
           return -1;
          }

        }

      }

      if(pHttpLiveDatas[outidx]->active) {
        if(pHttpLiveData) {
          pHttpLiveData->pnext = pHttpLiveDatas[outidx];
        }
        pHttpLiveData = pHttpLiveDatas[outidx];
      }

    }

    pStreamerCfg->action.do_httplive = 1;
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      pStreamerCfg->cbs[outidx].cbPostProc = httplive_cbOnPkt;
      pStreamerCfg->cbs[outidx].pCbPostProcData = pHttpLiveDatas[outidx];

    }

  } // end of if if(pParams->httplive...

/*
  //
  // Set up the moof live recording
  //
  if(pParams->moofliveaddr[0]) {

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      pSrv->startcfg.cfgShared.pMoofCtxts[outidx] = &pS->moofRecordCtxts[outidx];
    }

  } // end of if if(pParams->mooflive...
*/

  if((numHttp = maxWww + maxTsLive + maxHttpLive + maxMoofLive + maxFlvLive +
                maxMkvLive + maxStatus + maxPip + maxConfig)  > maxHttp + 1) {
    //
    // Add one extra connection to allow returning rejections for extra requests
    //
    numHttp = maxHttp + 1;
  }

  //
  // If RTSP over HTTP is enabled, each session requires a GET and POST connection
  //
  maxRtspLiveConn = MIN(VSX_CONNECTIONS_MAX, maxRtspLive * 2);

  if((numConn = numHttp + maxRtmpLive + maxRtspLiveConn) <= 0) {
    return 0;
  }

  LOG(X_DEBUG("Creating %d media listeners, HTTP: %d, RTSP: %d, RTMP: %d"),
      numConn, numHttp, maxRtspLiveConn, maxRtmpLive);

  //
  // Create and init each available client connection instance
  //
  if(numHttp > 0 &&
     pool_open(&pSrv->poolHttp, numHttp, sizeof(CLIENT_CONN_T), 1) != 0) {
    return -1;
  }
  pSrv->poolHttp.descr = STREAM_METHOD_PROGDOWNLOAD_STR;

  if(maxRtmpLive > 0 &&
    pool_open(&pSrv->poolRtmp, maxRtmpLive, sizeof(CLIENT_CONN_T), 1) != 0) {
    return -1;
  }
  pSrv->poolRtmp.descr = STREAM_METHOD_RTMP_STR;

  if(maxRtspLiveConn > 0 &&
     pool_open(&pSrv->poolRtsp, maxRtspLiveConn, sizeof(CLIENT_CONN_T), 1) != 0) {
    return -1;
  }
  pSrv->poolRtsp.descr = STREAM_METHOD_RTSP_STR;

  //
  // Setup the HTTP tslive parameters
  //
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active) {
      continue;
    }

    pLiveQ = &pStreamerCfg->liveQs[outidx];
    if((*((unsigned int *) &pLiveQ->max) = maxTsLive) > 0 &&
      !(pLiveQ->pQs = (PKTQUEUE_T **)
                    avc_calloc(maxTsLive, sizeof(PKTQUEUE_T *)))) {
      return -1;
    }
    pthread_mutex_init(&pLiveQ->mtx, NULL);
    pLiveQ->qCfg.id = STREAMER_QID_TSLIVE + outidx;
    pLiveQ->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgTslive.maxPkts;
    pLiveQ->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgTslive.maxPktLen;
    pLiveQ->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgTslive.growMaxPktLen;
    pLiveQ->qCfg.growMaxPkts = pStreamerCfg->action.outfmtQCfg.cfgTslive.growMaxPkts;
    pLiveQ->qCfg.concataddenum = MP2TS_LEN;

  }

  // 
  // Setup RTMP server parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP];
  if((*((unsigned int *) &pLiveFmt->max) = maxRtmpLive) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *)
                  avc_calloc(maxRtmpLive, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_RTMP;
  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgRtmp.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtmp.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtmp.growMaxPktLen;

  // 
  // Setup FLV server parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV];
  if((*((unsigned int *) &pLiveFmt->max) = maxFlvLive) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *)
                  avc_calloc(maxFlvLive, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_FLV;
  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgFlv.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgFlv.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgFlv.growMaxPktLen;
  pLiveFmt->bufferDelaySec = pParams->flvlivedelay;

  //
  // Setup MKV server parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV];
  if((*((unsigned int *) &pLiveFmt->max) = maxMkvLive) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *)
                  avc_calloc(maxMkvLive, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_MKV;
  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgMkv.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgMkv.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgMkv.growMaxPktLen;
  pLiveFmt->bufferDelaySec = pParams->mkvlivedelay;
/*
  // 
  // Setup RTSP server parameters
  //
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active) {
      continue;
    }

    pLiveQ2 = &pStreamerCfg->liveQ2s[outidx];
    if((*((unsigned int *) &pLiveQ2->max) = maxRtspLive) > 0 &&
     !(pLiveQ2->pQs = (PKTQUEUE_T **) avc_calloc(maxRtspLive, sizeof(PKTQUEUE_T *)))) {
      return -1;
    }
    pthread_mutex_init(&pLiveQ2->mtx, NULL);
    pLiveQ2->qCfg.id = STREAMER_QID_RTSP_INTERLEAVED + outidx;
    pLiveQ2->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgRtsp.maxPkts;
    pLiveQ2->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtsp.maxPktLen;
    pLiveQ2->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgRtsp.growMaxPktLen;

  }
*/
  //pSrv->rtspLive.pCfg = &pSrv->startcfg;
  //pSrv->rtspLive.pConnPool = &pSrv->poolRtsp;

  pthread_mutex_init(&pSrv->mtx, NULL);

  init_conn(&pSrv->poolHttp, pSrv);
  init_conn(&pSrv->poolRtmp, pSrv);
  init_conn(&pSrv->poolRtsp, pSrv);

  //
  // Start the live / httplive server
  // 
  if(numHttp > 0) {

    if(maxTsLive > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->tsliveaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_TSLIVE, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_TSLIVE].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
      pStreamerCfg->action.do_tslive = 1;
    }

    if(maxHttpLive > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->httpliveaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_TSHTTPLIVE, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_HTTPLIVE].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
    }

    if(maxMoofLive > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->dashliveaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_MOOFLIVE, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_MOOFLIVE].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
    }

    if(maxFlvLive > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->flvliveaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_FLVLIVE, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_FLV].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
      pStreamerCfg->action.do_flvlive = 1;
      pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV].do_outfmt = 1;
    }

    if(maxMkvLive > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->mkvliveaddr, SRV_LISTENER_MAX_HTTP,
                                     pSrv->startcfg.listenHttp, URL_CAP_MKVLIVE, 
                                     pStreamerCfg->creds[STREAMER_AUTH_IDX_MKV].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
      pStreamerCfg->action.do_mkvlive = 1;
      pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV].do_outfmt = 1;
    }

    if(maxWww > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->liveaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_LIVE | URL_CAP_IMG, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_ROOT].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
    }

    if(maxStatus > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->statusaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_STATUS, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_STATUS].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
    }

    if(maxConfig > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->configaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_CONFIG, 
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_CONFIG].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
    }

    if(maxPip > 0) {
      if((rc = vsxlib_parse_listener((const char **) pParams->pipaddr, SRV_LISTENER_MAX_HTTP,
                             pSrv->startcfg.listenHttp, URL_CAP_PIP,
                             pStreamerCfg->creds[STREAMER_AUTH_IDX_PIP].stores)) < 0) {
        vsxlib_closeServer(pSrv);
        return rc;
      }
    }


    for(idx = 0; idx < SRV_LISTENER_MAX_HTTP; idx++) {

      if(pSrv->startcfg.listenHttp[idx].active) {

        pSrv->startcfg.listenHttp[idx].max = numHttp;
        pSrv->startcfg.listenHttp[idx].pConnPool = &pSrv->poolHttp;
        pSrv->startcfg.listenHttp[idx].pCfg = &pSrv->startcfg;
        pthread_mutex_init(&pSrv->startcfg.listenHttp[idx].mtx, NULL);

        if((rc = vsxlib_ssl_initserver(pParams, &pSrv->startcfg.listenHttp[idx])) < 0 ||
           (rc = srvlisten_starthttp(&pSrv->startcfg.listenHttp[idx], 1)) < 0) {
          vsxlib_closeServer(pSrv);
          return rc;
        }
      }
    }

  } // end of if(pSrv->startcfg.maxhttp...

  //
  // Start the RTMP server
  // 
  if(maxRtmpLive > 0) {

    if((rc = vsxlib_parse_listener((const char **) pParams->rtmpliveaddr, SRV_LISTENER_MAX_RTMP,
                               pSrv->startcfg.listenRtmp, URL_CAP_RTMPLIVE, NULL)) < 0) {
      vsxlib_closeServer(pSrv);
      return rc;
    }
    if((rc = vsxlib_check_other_listeners(&pSrv->startcfg, pSrv->startcfg.listenRtmp)) > 0) {
      LOG(X_WARNING("RTMP server listener %s:%d cannot be shared with another protocol"),
          INET_NTOP(pSrv->startcfg.listenRtmp[rc - 1].sa, tmp, sizeof(tmp)),
          htons(INET_PORT(pSrv->startcfg.listenRtmp[rc - 1].sa)));
    } else {

      pStreamerCfg->action.do_rtmplive = 1;

      for(idx = 0; idx < SRV_LISTENER_MAX_RTMP; idx++) {

        if(pSrv->startcfg.listenRtmp[idx].active) {
          pSrv->startcfg.listenRtmp[idx].max = maxRtmpLive;
          pSrv->startcfg.listenRtmp[idx].pConnPool = &pSrv->poolRtmp;
          pSrv->startcfg.listenRtmp[idx].pCfg = &pSrv->startcfg;
          pthread_mutex_init(&pSrv->startcfg.listenRtmp[idx].mtx, NULL);

          if((rc = vsxlib_ssl_initserver(pParams, &pSrv->startcfg.listenRtmp[idx])) < 0 ||
             (rc = srvlisten_startrtmplive(&pSrv->startcfg.listenRtmp[idx])) < 0) {
            vsxlib_closeServer(pSrv);
            return rc;
          }
        }
      }

      pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].do_outfmt = 1;

    }

  } // end of if(maxRtmpLive...

  //
  // Start the RTSP server
  // 
  if(maxRtspLive > 0) {

    if((rc = vsxlib_parse_listener((const char **) pParams->rtspliveaddr, SRV_LISTENER_MAX_RTSP,
                               pSrv->startcfg.listenRtsp, URL_CAP_RTSPLIVE, 
                               pStreamerCfg->creds[STREAMER_AUTH_IDX_RTSP].stores)) < 0) {
      vsxlib_closeServer(pSrv);
      return rc;
    }

    if((rc = vsxlib_check_other_listeners(&pSrv->startcfg, pSrv->startcfg.listenRtsp)) > 0) {
      LOG(X_WARNING("RTSP server listener %s:%d cannot be shared with another protocol"),
          INET_NTOP(pSrv->startcfg.listenRtsp[rc - 1].sa, tmp, sizeof(tmp)),
          htons(INET_PORT(pSrv->startcfg.listenRtsp[rc - 1].sa)));

    } else {

      pStreamerCfg->action.do_rtsplive = 1;
      pSrv->startcfg.prtspsessiontimeout = &pParams->rtspsessiontimeout;
      pSrv->startcfg.prtsprefreshtimeoutviartcp = &pParams->rtsprefreshtimeoutviartcp;
      pSrv->startcfg.cfgShared.pRtspSessions->psessionTmtSec = &pParams->rtspsessiontimeout;
      pSrv->startcfg.cfgShared.pRtspSessions->prefreshtimeoutviartcp = &pParams->rtsprefreshtimeoutviartcp;
      pSrv->startcfg.cfgShared.pRtspSessions->psessions = NULL;
      *((unsigned int *) &pSrv->startcfg.cfgShared.pRtspSessions->max) = maxRtspLive;

      if(rtspsrv_init(pSrv->startcfg.cfgShared.pRtspSessions) < 0)  {
        vsxlib_closeServer(pSrv);
        return -1;
      }

      for(idx = 0; idx < SRV_LISTENER_MAX_RTSP; idx++) {

        if(pSrv->startcfg.listenRtsp[idx].active) {
          pSrv->startcfg.listenRtsp[idx].max = maxRtspLiveConn;
          pSrv->startcfg.listenRtsp[idx].pConnPool = &pSrv->poolRtsp;
          pSrv->startcfg.listenRtsp[idx].pCfg = &pSrv->startcfg;
          pthread_mutex_init(&pSrv->startcfg.listenRtsp[idx].mtx, NULL);

          if((rc = vsxlib_ssl_initserver(pParams, &pSrv->startcfg.listenRtsp[idx])) < 0 ||
             (rc = srvlisten_startrtsplive(&pSrv->startcfg.listenRtsp[idx])) < 0) {
            vsxlib_closeServer(pSrv);
            return rc;
          }
        }
      }

    }

  } // end of if(maxRtspLive...

  //
  // Initialize the RTSP interleaved context
  //
  if((rc = vsxlib_initRtspInterleaved(pStreamerCfg, maxRtspLive, pParams) < 0)) {
    vsxlib_closeServer(pSrv);
    return rc;
  }

  pSrv->isinit = 1;

  return rc;
}

#endif // (VSX_HAVE_STREAMER)

