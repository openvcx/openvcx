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


#define PREPROCESS_FILE_LEN_MAX      8192
#define DEFAULT_EMBED_WIDTH           720
#define DEFAULT_EMBED_HEIGHT          480
#define MIN_EMBED_WIDTH               320
#define MIN_EMBED_HEIGHT              240

#define GET_STREAMER_FROM_CONN(pc)  ((pc)->pStreamerCfg0 ? (pc)->pStreamerCfg0 : (pc)->pStreamerCfg1)

void srv_lock_conn_mutexes(CLIENT_CONN_T *pConn, int lock) {
  if(lock) {
    if(pConn->pMtx0) {
      pthread_mutex_lock(pConn->pMtx0);
    }
    if(pConn->pMtx1) {
      pthread_mutex_lock(pConn->pMtx1);
    }
  } else {
    if(pConn->pMtx0) {
      pthread_mutex_unlock(pConn->pMtx0);
    }
    if(pConn->pMtx1) {
      pthread_mutex_unlock(pConn->pMtx1);
    }
  }
}

static int http_check_pass(const CLIENT_CONN_T *pConn) {
  const char *parg;
  int rc = 0;

  if(pConn->pCfg->livepwd &&
     (!(parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, "pass")) ||
       strcmp(pConn->pCfg->livepwd, parg))) {

    LOG(X_WARNING("Invalid password for :%d%s from %s:%d (%d char given)"),
         ntohs(pConn->pListenCfg->sain.sin_port), pConn->httpReq.url,
         inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port),
         parg ? strlen(parg) : 0);

    rc = -1;
  }

  return rc;
}

static int http_sub_htmlvars(const char *pIn, unsigned int lenIn, char *pOut, 
                             unsigned int lenOut, const char *pSubs[]) {
  unsigned int idxSub = 0;
  unsigned int idxInSub;
  unsigned int idxIn = 1;  
  unsigned int idxOut = 1;  

  if(!pIn || !pOut || !pSubs) {
    return -1;
  }

  //
  // Substition string is marked by '$$...$$'
  //

  pOut[0] = pIn[0];

  while(idxIn < lenIn) {

    if(idxOut >= lenOut) {
      return -1;
    }

    if(pIn[idxIn] == '$' && pIn[idxIn - 1] == '$') {

      if(!pSubs[idxSub]) {
        LOG(X_ERROR("Substition argument missing for index[%d]"), idxSub);
        return -1;
      }

      idxOut--;
      idxInSub = 0;
      while(pSubs[idxSub][idxInSub] != '\0') { 
        if(idxOut >= lenOut) {
          return -1;
        }
        pOut[idxOut++] = pSubs[idxSub][idxInSub++];
      }
      idxSub++;

      idxIn+=2;
      while(idxIn < lenIn && !(pIn[idxIn] == '$' && pIn[idxIn -1] == '$')) {
        idxIn++;
      }
    } else {
      pOut[idxOut++] = pIn[idxIn];
    }

    idxIn++;
  }

  pOut[idxOut] = '\0';

  return idxOut;
}

static int update_indexfile(const char *path, char *pOut, unsigned int lenOut, const char *pSubs[]) {
  int rc = 0;
  FILE_OFFSET_T idx = 0;
  FILE_STREAM_T fileStream;
  unsigned int lenread;
  char buf[PREPROCESS_FILE_LEN_MAX];

  if(OpenMediaReadOnly(&fileStream, path) != 0) {
    return -1;
  }

  if(fileStream.size > sizeof(buf)) {
    LOG(X_ERROR("Unable to pre-process file %s size %d exceeds limitation"), path, sizeof(buf));
    rc = -1;
  } else {

    while(idx < fileStream.size && idx < sizeof(buf)) {

      if((lenread = (unsigned int) (fileStream.size - idx)) > sizeof(buf) - idx) {
        lenread = sizeof(buf);
      }

      if(ReadFileStream(&fileStream, &buf[idx], lenread) != lenread) {
        rc = -1;
        break;
      }
      idx += lenread;
    }
  }

  CloseMediaFile(&fileStream);

  if(rc == 0) {
    if((rc = http_sub_htmlvars(buf, idx, pOut, lenOut, pSubs)) < 0) {
      LOG(X_ERROR("Failed to pre-process file %s"), path);
    }
  }

  return rc;
}
#define VID_WIDTH_HEIGHT_STR_SZ  32

static void get_width_height(const STREAMER_STATUS_T *pStatus, 
                             unsigned int outidx, 
                             char *strwidth, 
                             char *strheight) {

  int width, height;
  float aspectr;

//TODO: get width when retrieving dynamic / sdp catpure.. post sdp update via pStreamerCfg->status;
//TODO: this should be common code that goes in xcode/...
  if(pStatus && (width = pStatus->vidProps[outidx].resH) > 0 && (height = pStatus->vidProps[outidx].resV) > 0) {

    if(width < MIN_EMBED_WIDTH || height < MIN_EMBED_HEIGHT) {

      aspectr = (float) width / height;

      if(width < MIN_EMBED_WIDTH) {
        width = MIN_EMBED_WIDTH;
        height = ((float)width / aspectr);
      }
      if(height < MIN_EMBED_HEIGHT) {
        height = MIN_EMBED_HEIGHT;
        width = (float) height * aspectr; 
      }

      if(width & 0x01) {
        width++;
      }
      if(height & 0x01) {
        height++;
      }
    }

    snprintf(strwidth, VID_WIDTH_HEIGHT_STR_SZ, "%d", width);
    snprintf(strheight, VID_WIDTH_HEIGHT_STR_SZ, "%d", height);

  } else {

    snprintf(strwidth, VID_WIDTH_HEIGHT_STR_SZ, "%d", DEFAULT_EMBED_WIDTH);
    snprintf(strheight, VID_WIDTH_HEIGHT_STR_SZ, "%d", DEFAULT_EMBED_HEIGHT);

  }
}

static int get_output_idx(const CLIENT_CONN_T *pConn, 
                          const char *url, 
                          char *stroutidx, 
                          unsigned int *poutidx) {
  int rc = 0;
  int ioutidx;
  const STREAMER_CFG_T *pStreamerCfg = NULL;

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  //
  // Obtain the requested xcode output outidx from the HTTP URI such as '/rtmp/2'
  //
  if((ioutidx = outfmt_getoutidx(pConn->httpReq.puri, NULL)) < 0 ||
     ioutidx >= IXCODE_VIDEO_OUT_MAX) {
    ioutidx = 0;
  } else if(ioutidx > 0) {

    if(!pStreamerCfg || !pStreamerCfg->xcode.vid.out[ioutidx].active) {
      LOG(X_ERROR("Output format index[%d] not active"), ioutidx);
      rc = -1;
    } else {
      if(stroutidx) {
        sprintf(stroutidx, "/%d", ioutidx + 1);
      }
      LOG(X_DEBUG("Set %s index file output format index to[%d] url:'%s', for %s:%d"), url, ioutidx,
        pConn->httpReq.puri, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
    }
  }
  *poutidx = ioutidx;

  return rc;
}

static int update_proto_indexfile(CLIENT_CONN_T *pConn, 
                                 STREAMER_CFG_T *pStreamerCfg, 
                                 const char *idxFile, 
                                 const char *rsrcurl, 
                                 unsigned int outidx, 
                                 const char *subs[],
                                 char *strwidth,
                                 char *strheight,
                                 char *pOut, 
                                 unsigned int lenOut) {
  int rc = 0;
  char path[VSX_MAX_PATH_LEN];
  struct stat st;

  if(mediadb_prepend_dir(pConn->pCfg->pMediaDb->homeDir, idxFile, path, sizeof(path)) != 0) {
    return -1;
  } else if(fileops_stat(path, &st) != 0) {
    LOG(X_ERROR("Unable to stat index file '%s'"), path);
    return -1;
  }

  //if(pStreamerCfg && pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].do_outfmt) {
  if(strwidth && strheight) {

    strwidth[0] = '\0';
    strheight[0] = '\0';
    get_width_height(pStreamerCfg ? &pStreamerCfg->status : NULL, outidx, strwidth, strheight);
  }
  rc = update_indexfile(path, pOut, lenOut, subs);

  return rc;
}

typedef int (* CB_UPDATE_PROTO_INDEXFILE) (CLIENT_CONN_T *, STREAMER_CFG_T *, const char *, \
                                            unsigned int, char *pOut, unsigned int);

static int update_rtmp_indexfile(CLIENT_CONN_T *pConn, 
                                 STREAMER_CFG_T *pStreamerCfg, 
                                 const char *rsrcUrl, 
                                 unsigned int outidx, 
                                 char *pOut, 
                                 unsigned int lenOut) {

  char strwidth[VID_WIDTH_HEIGHT_STR_SZ];
  char strheight[VID_WIDTH_HEIGHT_STR_SZ];
  const char *subs[4];

  subs[0] = strwidth;
  subs[1] = strheight;
  subs[2] = rsrcUrl;
  subs[3] = NULL;

  return update_proto_indexfile(pConn, pStreamerCfg, VSX_RTMP_INDEX_HTML, rsrcUrl, outidx,
                  subs, strwidth, strheight, pOut, lenOut);
}

static int update_flv_indexfile(CLIENT_CONN_T *pConn, 
                                 STREAMER_CFG_T *pStreamerCfg, 
                                 const char *rsrcUrl, 
                                 unsigned int outidx, 
                                 char *pOut, 
                                 unsigned int lenOut) {

  char strwidth[VID_WIDTH_HEIGHT_STR_SZ];
  char strheight[VID_WIDTH_HEIGHT_STR_SZ];
  const char *subs[4];

  subs[0] = strwidth;
  subs[1] = strheight;
  subs[2] = rsrcUrl;
  subs[3] = NULL;

  return update_proto_indexfile(pConn, pStreamerCfg, VSX_FLASH_HTTP_INDEX_HTML, rsrcUrl, outidx,
                  subs, strwidth, strheight, pOut, lenOut);
}

static int update_mkv_indexfile(CLIENT_CONN_T *pConn, 
                                STREAMER_CFG_T *pStreamerCfg,
                                const char *rsrcUrl, 
                                unsigned int outidx, 
                                char *pOut, 
                                unsigned int lenOut) {

  char strwidth[VID_WIDTH_HEIGHT_STR_SZ];
  char strheight[VID_WIDTH_HEIGHT_STR_SZ];
  const char *subs[7];

  subs[0] = strwidth;
  subs[1] = strheight;
  subs[2] = strwidth;
  subs[3] = strheight;
  subs[4] = rsrcUrl;
  subs[5] = CONTENT_TYPE_WEBM;
  subs[6] = NULL;

  return update_proto_indexfile(pConn, pStreamerCfg, VSX_MKV_INDEX_HTML, rsrcUrl, outidx,
                  subs, strwidth, strheight, pOut, lenOut);
}

static int update_rtsp_indexfile(CLIENT_CONN_T *pConn, 
                                 STREAMER_CFG_T *pStreamerCfg,
                                 const char *rsrcUrl, 
                                 unsigned int outidx,
                                 char *pOut, 
                                 unsigned int lenOut) {
  const char *subs[4];

  subs[0] = rsrcUrl;
  subs[1] = rsrcUrl;
  subs[2] = rsrcUrl;
  subs[3] = NULL;

  return update_proto_indexfile(pConn, NULL, VSX_RTSP_INDEX_HTML, rsrcUrl, 0,
                  subs, NULL, NULL, pOut, lenOut);
}


static int update_httplive_indexfile(CLIENT_CONN_T *pConn, 
                                     STREAMER_CFG_T *pStreamerCfg,
                                     const char *rsrcUrl, 
                                     unsigned int outidx, 
                                     char *pOut, 
                                     unsigned int lenOut) {

  char strwidth[VID_WIDTH_HEIGHT_STR_SZ];
  char strheight[VID_WIDTH_HEIGHT_STR_SZ];
  const char *subs[4];

  subs[0] = strwidth;
  subs[1] = strheight;
  subs[2] = rsrcUrl;
  subs[3] = NULL;

  return update_proto_indexfile(pConn, pStreamerCfg, VSX_HTTPLIVE_INDEX_HTML, rsrcUrl, outidx,
                  subs, strwidth, strheight, pOut, lenOut);
}


static int update_dash_indexfile(CLIENT_CONN_T *pConn, 
                                 STREAMER_CFG_T *pStreamerCfg,
                                 const char *rsrcUrl, 
                                 unsigned int outidx, 
                                 char *pOut, 
                                 unsigned int lenOut) {

  char strwidth[VID_WIDTH_HEIGHT_STR_SZ];
  char strheight[VID_WIDTH_HEIGHT_STR_SZ];
  const char *subs[4];

  subs[0] = strwidth;
  subs[1] = strheight;
  subs[2] = rsrcUrl;
  subs[3] = NULL;

  return update_proto_indexfile(pConn, pStreamerCfg, VSX_DASH_INDEX_HTML, rsrcUrl, outidx,
                  subs, strwidth, strheight, pOut, lenOut);
}

static int resp_index_file(CLIENT_CONN_T *pConn, 
                           const char *pargfile, 
                           int is_remoteargfile,
                           const SRV_LISTENER_CFG_T *pListenCfg,
                           int urlCap) {

  int rc = 0;
  unsigned int idx = 0;
  char rsrcUrl[VSX_MAX_PATH_LEN];
  char tmp[VSX_MAX_PATH_LEN];
  char stroutidx[32];
  const char *strerror = NULL;
  unsigned int outidx = 0;
  const char *protoUrl = NULL;
  const char *protoLiveUrl = NULL;
  int *p_action = NULL;
  const char *fileExt = NULL;
  CB_UPDATE_PROTO_INDEXFILE cbUpdateProtoIndexFile = NULL;
  const SRV_LISTENER_CFG_T *pListenSearch = pConn->pCfg->pListenHttp;
  unsigned int maxListener = SRV_LISTENER_MAX_HTTP;
  unsigned char buf[PREPROCESS_FILE_LEN_MAX];
  STREAMER_CFG_T *pStreamerCfg = NULL;

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  switch(urlCap) {

    case URL_CAP_FLVLIVE:
      protoUrl = VSX_FLV_URL;
      protoLiveUrl = VSX_FLVLIVE_URL;
      p_action = pStreamerCfg ? &pStreamerCfg->action.do_flvlive : NULL;
      fileExt = ".flv";
      cbUpdateProtoIndexFile = update_flv_indexfile;
      break;

    case URL_CAP_MKVLIVE:
      protoUrl = VSX_MKV_URL;
      protoLiveUrl = VSX_MKVLIVE_URL;
      p_action = pStreamerCfg ? &pStreamerCfg->action.do_mkvlive : NULL;
      fileExt = ".webm";
      cbUpdateProtoIndexFile = update_mkv_indexfile;
      break;

    case URL_CAP_RTMPLIVE:
      protoUrl = VSX_RTMP_URL;
      protoLiveUrl = VSX_LIVE_URL;
      p_action = pStreamerCfg ? &pStreamerCfg->action.do_rtmplive : NULL;
      fileExt = NULL;
      cbUpdateProtoIndexFile = update_rtmp_indexfile;
      maxListener = SRV_LISTENER_MAX_RTMP;
      pListenSearch = pConn->pCfg->pListenRtmp;
      break;

    case URL_CAP_RTSPLIVE:
      protoUrl = VSX_RTSP_URL;
      protoLiveUrl = VSX_LIVE_URL;
      p_action = pStreamerCfg ? &pStreamerCfg->action.do_rtsplive : NULL;
      fileExt = NULL;
      cbUpdateProtoIndexFile = update_rtsp_indexfile;
      maxListener = SRV_LISTENER_MAX_RTSP;
      pListenSearch = pConn->pCfg->pListenRtsp;
      break;

    default:
      return -1;
  }

  //
  // Find the correct listener for the flvlive FLV data source
  //
  if(!pListenCfg &&
    !(pListenCfg = srv_ctrl_findlistener(pListenSearch, maxListener, urlCap,
                                     pConn->pListenCfg->netflags, NETIO_FLAG_SSL_TLS))) {
    pListenCfg = srv_ctrl_findlistener(pListenSearch, maxListener, urlCap, 0, 0);
  }

  //
  // Construct the underlying media stream URL accessible at the '/flvlive' URL
  //
  stroutidx[0] = '\0';
  rc = get_output_idx(pConn, protoUrl, stroutidx, &outidx);

  if(p_action && !(*p_action)) {
    snprintf(tmp, sizeof(tmp), "%s not enabled", protoUrl);
    LOG(X_ERROR("%s"), tmp);
    strerror = tmp;
    rc = -1;
  } else if(!pListenCfg || pListenCfg ->sain.sin_port == 0) {
    LOG(X_ERROR(" address / port substitution not set for %s"), protoUrl);
    rc = -1;
  } else {

    if(pargfile && pargfile[0] != '\0') {
      while(pargfile[idx] == '/') {
        idx++;
      }
      snprintf(tmp, sizeof(tmp), "%s", &pargfile[idx]);
    } else {
      snprintf(tmp, sizeof(tmp), "/live%s?%d", fileExt ? fileExt : "", (int) (random() % RAND_MAX));
    }

    if(is_remoteargfile) {
      //
      // The media resource is located on a remote server
      //
      snprintf(rsrcUrl, sizeof(rsrcUrl), "%s", tmp);
    } else {
      if(urlCap == URL_CAP_RTMPLIVE) {
        snprintf(rsrcUrl, sizeof(rsrcUrl), URL_RTMP_FMT_STR"%s%s%s",
               URL_HTTP_FMT_ARGS(pListenCfg), protoLiveUrl, stroutidx, tmp);
      } else if(urlCap == URL_CAP_RTSPLIVE) {
        snprintf(rsrcUrl, sizeof(rsrcUrl), URL_RTSP_FMT_STR"%s%s%s",
               URL_HTTP_FMT_ARGS(pListenCfg), protoLiveUrl, stroutidx, tmp);
      } else {
        snprintf(rsrcUrl, sizeof(rsrcUrl), URL_HTTP_FMT_STR"%s%s%s",
               URL_HTTP_FMT_ARGS(pListenCfg), protoLiveUrl, stroutidx, tmp);
      }
    }
  }

  VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - Created media link URL: '%s', proto: '%s', pargfile: '%s', "
                             "stroutidx: '%s', tmp: '%s', '%s'"), 
                              rsrcUrl, protoUrl, pargfile, stroutidx, tmp, protoLiveUrl));

  if(rc >= 0 && (rc = cbUpdateProtoIndexFile(pConn, pStreamerCfg, rsrcUrl,  outidx, (char *) buf,
                                 sizeof(buf))) > 0) {
    rc = http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_OK, buf, rc);
  } else {
    http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_SERVERERROR, 0, strerror, NULL);
    rc = -1;
  }

  return rc;
}

int srv_ctrl_flvlive(CLIENT_CONN_T *pConn) {
  int rc = 0;
  FLVSRV_CTXT_T flvCtxt;
  struct timeval tv0, tv1;
  STREAMER_CFG_T *pStreamerCfg = NULL;
  STREAMER_OUTFMT_T *pLiveFmt = NULL;
  unsigned int numQFull = 0;
  PKTQUEUE_CONFIG_T qCfg;
  int outidx;
  OUTFMT_CFG_T *pOutFmt = NULL;
  STREAM_STATS_T *pstats = NULL;
  unsigned char buf[64];

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);
  //
  // pStreamerCfg may be null if we have been invoked from src/mgr/srvmvgr.c context
  //

  if(pStreamerCfg && pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV].do_outfmt) {
    pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV];
  }

  //
  // Auto increment the flv slot count buffer queue if a buffer delay is given
  //
  memcpy(&qCfg, &pLiveFmt->qCfg, sizeof(qCfg));
  if(pLiveFmt->bufferDelaySec > 0) {
    qCfg.maxPkts += (MIN(10, pLiveFmt->bufferDelaySec) * 50); 
    LOG(X_DEBUG("Increased flv queue(id:%d) from %d to %d for %.3f s buffer delay"), 
        qCfg.id, pLiveFmt->qCfg.maxPkts, qCfg.maxPkts, pLiveFmt->bufferDelaySec);
  }

  //
  // Obtain the requested xcode output outidx specified in the URI such as '/flv/2'
  //
  if((outidx = outfmt_getoutidx(pConn->httpReq.puri, NULL)) < 0 || outidx >= IXCODE_VIDEO_OUT_MAX) {
    outidx = 0;
  } else if(outidx > 0) {
    if(pStreamerCfg && !pStreamerCfg->xcode.vid.out[outidx].active) {
      LOG(X_ERROR("Output format index[%d] not active"), outidx);
      pLiveFmt = NULL;
    } else {
      LOG(X_DEBUG("Set "VSX_FLVLIVE_URL" output format index to[%d] url:'%s', for %s:%d"), outidx,
        pConn->httpReq.puri, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
    }
  }

  if(pLiveFmt) {

    if(pStreamerCfg && pStreamerCfg->pMonitor && pStreamerCfg->pMonitor->active) {
      if(!(pstats = stream_monitor_createattach(pStreamerCfg->pMonitor, &pConn->sd.sain, 
                                               STREAM_METHOD_FLVLIVE, STREAM_MONITOR_ABR_NONE))) {
      }
    }

    //
    // Add a livefmt cb
    //
    pOutFmt = outfmt_setCb(pLiveFmt, flvsrv_addFrame, &flvCtxt, &qCfg, pstats, 1, 
                           pStreamerCfg ? pStreamerCfg->frameThin : 0, &numQFull);
  }

  if(pOutFmt) {

    memset(&flvCtxt, 0, sizeof(flvCtxt));
    flvCtxt.pSd = &pConn->sd;
    flvCtxt.pReq = &pConn->httpReq;
    flvCtxt.avBufferDelaySec = pLiveFmt->bufferDelaySec;
    flvCtxt.pnovid = &pStreamerCfg->novid;
    flvCtxt.pnoaud = &pStreamerCfg->noaud;
    flvCtxt.requestOutIdx = outidx;
    flvCtxt.av.vid.pStreamerCfg = pStreamerCfg;
    flvCtxt.av.aud.pStreamerCfg = pStreamerCfg;

    if(flvsrv_init(&flvCtxt, MAX(pLiveFmt->qCfg.maxPktLen, pLiveFmt->qCfg.growMaxPktLen)) < 0) {
      //
      // Remove the livefmt cb
      //
      outfmt_removeCb(pOutFmt);
      pOutFmt = NULL;
    }

  }

  if(pOutFmt) {

    //
    // Unpause the outfmt callback mechanism now that flvsrv_init was called
    //
    outfmt_pause(pOutFmt, 0);

    LOG(X_INFO("Starting flvlive stream[%d] %d/%d to %s:%d"), pOutFmt->cbCtxt.idx, numQFull + 1,
           pLiveFmt->max,
           inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    //
    // Set outbound QOS
    //
    // TODO: make QOS configurable
    net_setqos(NETIOSOCK_FD(pConn->sd.netsocket), &pConn->sd.sain, DSCP_AF36);

    gettimeofday(&tv0, NULL);

    rc = flvsrv_sendHttpResp(&flvCtxt);

    //
    // Sleep here on the http socket until a socket error or
    // an error from when the frame callback flvsrv_addFrame is invoked.
    //
    while(!g_proc_exit && flvCtxt.state > FLVSRV_STATE_ERROR) {

      //if(net_issockremotelyclosed(pConn->sd.socket, 1)) {
      //  LOG(X_DEBUG("flvlive connection from %s:%d has been remotely closed"),
      //     inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port) );
      //  break;
      //}

      if((rc = netio_recvnb(&pConn->sd.netsocket, buf, sizeof(buf), 500)) < 0) {
        break;
      }
    }

    if(flvCtxt.state <= FLVSRV_STATE_ERROR) {
      //
      // Force srv_cmd_proc loop to exit even for "Connection: Keep-Alive"
      //
      rc = -1;
    }

    gettimeofday(&tv1, NULL);

    LOG(X_DEBUG("Finished sending flvlive %llu bytes to %s:%d (%.1fKb/s)"), flvCtxt.totXmit,
             inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port),
             (double) flvCtxt.totXmit / (((tv1.tv_sec - tv0.tv_sec) * 1000) +
                       ((tv1.tv_usec - tv0.tv_usec) / 1000)) * 7.8125);

    LOG(X_INFO("Ending flvlive stream[%d] to %s:%d"), pOutFmt->cbCtxt.idx,
             inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    //
    // Remove the livefmt cb
    //
    outfmt_removeCb(pOutFmt);

    flvsrv_close(&flvCtxt);

  } else {

    if(pstats) {
      //
      // Destroy automatically detaches the stats from the monitor linked list
      //
      stream_stats_destroy(&pstats, NULL);
    }

    LOG(X_WARNING("No flvlive resource available (max:%d) for %s:%d"),
        (pLiveFmt ? pLiveFmt->max : 0),
       inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    rc = -1;
  }

  // TODO: this should return < 0 to force http connection to end
  return rc;
}

int srv_ctrl_mkvlive(CLIENT_CONN_T *pConn) {
  int rc = 0;
  MKVSRV_CTXT_T mkvCtxt;
  struct timeval tv0, tv1;
  STREAMER_CFG_T *pStreamerCfg = NULL;
  STREAMER_OUTFMT_T *pLiveFmt = NULL;
  unsigned int numQFull = 0;
  PKTQUEUE_CONFIG_T qCfg;
  int outidx;
  OUTFMT_CFG_T *pOutFmt = NULL;
  STREAM_STATS_T *pstats = NULL;
  double duration;
  unsigned char buf[64];

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);
  //
  // pStreamerCfg may be null if we have been invoked from src/mgr/srvmvgr.c context
  //

  if(pStreamerCfg && pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV].do_outfmt) {
    pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MKV];
  }

  //
  // Auto increment the mkv slot count buffer queue if a buffer delay is given
  //
  memcpy(&qCfg, &pLiveFmt->qCfg, sizeof(qCfg));
  if(pLiveFmt->bufferDelaySec > 0) {
    qCfg.maxPkts += (MIN(10, pLiveFmt->bufferDelaySec) * 50); 
    LOG(X_DEBUG("Increased mkv queue(id:%d) from %d to %d for %.3f s buffer delay"), 
        qCfg.id, pLiveFmt->qCfg.maxPkts, qCfg.maxPkts, pLiveFmt->bufferDelaySec);
  }

  //
  // Obtain the requested xcode output outidx specified in the URI such as '/mkv/2'
  //
  if((outidx = outfmt_getoutidx(pConn->httpReq.puri, NULL)) < 0 || outidx >= IXCODE_VIDEO_OUT_MAX) {
    outidx = 0;
  } else if(outidx > 0) {
    if(pStreamerCfg && !pStreamerCfg->xcode.vid.out[outidx].active) {
      LOG(X_ERROR("Output format index[%d] not active"), outidx);
      pLiveFmt = NULL;
    } else {
      LOG(X_DEBUG("Set "VSX_MKVLIVE_URL" output format index to[%d] url:'%s', for %s:%d"), outidx,
        pConn->httpReq.puri, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
    }
  }

  if(pLiveFmt) {

    if(pStreamerCfg && pStreamerCfg->pMonitor && pStreamerCfg->pMonitor->active) {
      if(!(pstats = stream_monitor_createattach(pStreamerCfg->pMonitor, &pConn->sd.sain, 
                                               STREAM_METHOD_MKVLIVE, STREAM_MONITOR_ABR_NONE))) {
      }
    }

    //
    // Add a livefmt cb
    //
    pOutFmt = outfmt_setCb(pLiveFmt, mkvsrv_addFrame, &mkvCtxt, &qCfg, pstats, 1, 
                           pStreamerCfg ? pStreamerCfg->frameThin : 0, &numQFull);
  }

  if(pOutFmt) {

    memset(&mkvCtxt, 0, sizeof(mkvCtxt));
    mkvCtxt.pSd = &pConn->sd;
    mkvCtxt.pReq = &pConn->httpReq;
    mkvCtxt.avBufferDelaySec = pLiveFmt->bufferDelaySec;
    mkvCtxt.pnovid = &pStreamerCfg->novid;
    mkvCtxt.pnoaud = &pStreamerCfg->noaud;
    mkvCtxt.requestOutIdx = outidx;
    mkvCtxt.av.vid.pStreamerCfg = pStreamerCfg;
    mkvCtxt.av.aud.pStreamerCfg = pStreamerCfg;
    mkvCtxt.faoffset_mkvpktz = pStreamerCfg->status.faoffset_mkvpktz;

    if(mkvsrv_init(&mkvCtxt, MAX(pLiveFmt->qCfg.maxPktLen, pLiveFmt->qCfg.growMaxPktLen)) < 0) {
      //
      // Remove the livefmt cb
      //
      outfmt_removeCb(pOutFmt);
      pOutFmt = NULL;
    }

  }

  if(pOutFmt) {

    //
    // Unpause the outfmt callback mechanism now that mkvsrv_init was called
    //
    outfmt_pause(pOutFmt, 0);

    LOG(X_INFO("Starting mkvlive stream[%d] %d/%d to %s:%d"), pOutFmt->cbCtxt.idx, numQFull + 1,
           pLiveFmt->max,
           inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    //
    // Set outbound QOS
    //
    // TODO: make QOS configurable
    net_setqos(NETIOSOCK_FD(pConn->sd.netsocket), &pConn->sd.sain, DSCP_AF36);

    gettimeofday(&tv0, NULL);

    rc = mkvsrv_sendHttpResp(&mkvCtxt, CONTENT_TYPE_MATROSKA);

    //
    // Sleep here on the http socket until a socket error or
    // an error from when the frame callback mkvsrv_addFrame is invoked.
    //
    while(!g_proc_exit && mkvCtxt.state > MKVSRV_STATE_ERROR) {

      //if(net_issockremotelyclosed(pConn->sd.socket, 1)) {
      //  LOG(X_DEBUG("mkvlive connection from %s:%d has been remotely closed"),
      //     inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port) );
      //  break;
      //}

      if((rc = netio_recvnb(&pConn->sd.netsocket, buf, sizeof(buf), 500)) < 0) {
        break;
      }
    }

    if(mkvCtxt.state <= MKVSRV_STATE_ERROR) {
      //
      // Force srv_cmd_proc loop to exit even for "Connection: Keep-Alive"
      //
      rc = -1;
    }

    gettimeofday(&tv1, NULL);

    duration = ((tv1.tv_sec - tv0.tv_sec) * 1000) +  ((tv1.tv_usec - tv0.tv_usec) / 1000);
    LOG(X_DEBUG("Finished sending mkvlive %llu bytes to %s:%d (%.1fKb/s)"), mkvCtxt.totXmit,
             inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port),
             duration > 0 ? (double) mkvCtxt.totXmit / duration  * 7.8125 : 0);

    LOG(X_INFO("Ending mkvlive stream[%d] to %s:%d"), pOutFmt->cbCtxt.idx,
             inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    //
    // Remove the livefmt cb
    //
    outfmt_removeCb(pOutFmt);

    mkvsrv_close(&mkvCtxt);

  } else {

    if(pstats) {
      //
      // Destroy automatically detaches the stats from the monitor linked list
      //
      stream_stats_destroy(&pstats, NULL);
    }

    LOG(X_WARNING("No mkvlive resource available (max:%d) for %s:%d"),
        (pLiveFmt ? pLiveFmt->max : 0),
       inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    rc = http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_SERVERERROR,
      (unsigned char *) HTTP_STATUS_STR_SERVERERROR, strlen(HTTP_STATUS_STR_SERVERERROR));

    rc = -1;
  }

  // TODO: this should return < 0 to force http connection to end
  return rc;

}

int srv_ctrl_tslive(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus) {
  int rc = 0;
  unsigned int idx = 0;
  int liveQIdx = -1;
  unsigned int numQFull = 0;
  STREAMER_LIVEQ_T *pLiveQ;
  STREAM_STATS_T *pstats = NULL;
  PKTQUEUE_T *pQueue = NULL;
  unsigned int queueSz = 0;
  const char *parg;
  char resp[512];
  unsigned char *presp = NULL;
  //HTTP_STATUS_T statusCode = HTTP_STATUS_OK;
  int lenresp = 0;
  int outidx;
  unsigned int maxQ = 0;
  STREAMER_CFG_T *pStreamerCfg = NULL;

  *pHttpStatus = HTTP_STATUS_SERVICEUNAVAIL;

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  //
  // pStreamerCfg may be null if we have been invoked from src/mgr/srvmvgr.c context
  //
  if(!pStreamerCfg) {
    LOG(X_ERROR("tslive may not be enabled"));
    return -1;
  }

  //
  // Validate any URI (un-)secure token  the request
  //
  if((rc = http_check_pass(pConn)) < 0) {
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return rc;
  } 

  //
  // Obtain the requested xcode output outidx specified in the URI such as '/tslive/2'
  //
  if((outidx = outfmt_getoutidx(pConn->httpReq.puri, NULL)) < 0 ||
     outidx >= IXCODE_VIDEO_OUT_MAX) {
    outidx = 0;
  } else if(outidx > 0) {
    if(pStreamerCfg && !pStreamerCfg->xcode.vid.out[outidx].active) {
       LOG(X_ERROR("Output format index[%d] not active"), outidx);
       return -1;
    } else {
      LOG(X_DEBUG("Set "VSX_TSLIVE_URL" output format index to[%d] url:'%s', for %s:%d"), outidx,
        pConn->httpReq.puri, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
    }
  }
  
  //statusCode = HTTP_STATUS_SERVICEUNAVAIL;
  srv_lock_conn_mutexes(pConn, 1);

  if(!(pLiveQ = &pStreamerCfg->liveQs[outidx])) {
    rc = -1;
  }

  if(rc == 0 && !pStreamerCfg->action.do_tslive) {

    LOG(X_WARNING("tslive not currently enabled for request from %s:%d"),
           inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    if((lenresp = snprintf(resp, sizeof(resp), "Live stream not currently enabled")) > 0) {
      presp = (unsigned char *) resp;
    }
    rc = -1;
  }

  if(rc == 0) {
    if(pLiveQ->pQs) {
      maxQ = pLiveQ->max;
    }
    queueSz = pLiveQ->qCfg.maxPkts;
  }

  if(rc == 0 && (parg = conf_find_keyval((const KEYVAL_PAIR_T *) 
                              &pConn->httpReq.uriPairs, "queue"))) {
    if((queueSz = atoi(parg)) > pLiveQ->qCfg.growMaxPkts) {
      queueSz = pLiveQ->qCfg.growMaxPkts;
    }
    LOG(X_INFO("Custom tslive queue size set to %u for %s:%d"), queueSz, 
         inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
  }

  // TODO pktqueue size >= sizeof(PACKETGEN_PKT_UDP_T::data)
  if(rc == 0) {
    if((pQueue = pktqueue_create(queueSz, 
                                 pLiveQ->qCfg.maxPktLen, 0, 0, 0, 0, 1)) == NULL) {
      rc = -1;
      *pHttpStatus = HTTP_STATUS_SERVERERROR;
      LOG(X_ERROR("Failed to create %s queue %d x %d for %s:%d"), VSX_TSLIVE_URL, queueSz, 
        pLiveQ->qCfg.maxPktLen, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
    } else {
      //
      // Set concataddenum to allow filling up as much of each slot as possible,
      // if the reader hasn't yet gobbled up the prior write
      //
      pQueue->cfg.concataddenum = pLiveQ->qCfg.concataddenum;
      LOG(X_DEBUG("Created %s queue %d x %d for %s:%d"), VSX_TSLIVE_URL, queueSz, 
        pLiveQ->qCfg.maxPktLen, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
    }
  }

  if(rc == 0) {

    //
    // Find an available liveQ queue instance 
    //
    pthread_mutex_lock(&pLiveQ->mtx);
    if(pLiveQ->numActive <  maxQ) {
      for(idx = 0; idx < maxQ; idx++) {
        if(pLiveQ->pQs[idx] == NULL) {

          //
          // Create any stream throughput statistics meters
          //
          if(pStreamerCfg->pMonitor && pStreamerCfg->pMonitor->active) {

            if(!(pstats = stream_monitor_createattach(pStreamerCfg->pMonitor, &pConn->sd.sain, 
                                                     STREAM_METHOD_TSLIVE, STREAM_MONITOR_ABR_NONE))) {
            } else {
              pQueue->pstats = &pstats->throughput_rt[0];
            }
          }

          liveQIdx = idx;

          pLiveQ->pQs[liveQIdx] = pQueue;
          pLiveQ->numActive++;

          if(pConn->pStreamerCfg1) {
            pConn->pStreamerCfg1->liveQs[outidx].pQs[liveQIdx] = pQueue;
            pConn->pStreamerCfg1->liveQs[outidx].numActive++;

          }
          pQueue->cfg.id = pLiveQ->qCfg.id + liveQIdx;
          break;
        }
        numQFull++;
      }
    }
    pthread_mutex_unlock(&pLiveQ->mtx);

    if(liveQIdx < 0) {
      LOG(X_ERROR("No tslive queue (max:%d) for %s:%d"), maxQ, 
          inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
      if((lenresp = snprintf(resp, sizeof(resp), 
                     "All available resources currently in use")) > 0) {
        presp = (unsigned char *) resp;
      }
      rc = -1;
    }

  }

  srv_lock_conn_mutexes(pConn, 0);

  //
  // Upon any error send HTTP error status code and quit
  //
  if(rc != 0) {
    if(presp) {
      http_resp_send(&pConn->sd, &pConn->httpReq, *pHttpStatus, presp, lenresp);
      *pHttpStatus = HTTP_STATUS_OK;
    }
    if(pQueue) {
      pktqueue_destroy(pQueue);
    }
    return rc;
  }

  //
  // Set outbound QOS
  //
  // TODO: make QOS configurable
  net_setqos(NETIOSOCK_FD(pConn->sd.netsocket), &pConn->sd.sain, DSCP_AF36);

  LOG(X_INFO("Starting tslive stream[%d] %d/%d to %s:%d"), liveQIdx, numQFull + 1, maxQ,
           inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

  //
  // Request an IDR from the underling encoder
  //
  if(pStreamerCfg) {
    //
    // Set the requested IDR time a bit into the future because there may be a slight
    // delay until the queue reader starts reading mpeg2-ts packets
    //
    streamer_requestFB(pStreamerCfg, outidx, ENCODER_FBREQ_TYPE_FIR, 200, REQUEST_FB_FROM_LOCAL);
  }

  *pHttpStatus = HTTP_STATUS_OK;
  rc = http_resp_sendtslive(&pConn->sd, &pConn->httpReq, pQueue, CONTENT_TYPE_MP2TS);

  LOG(X_INFO("Ending tslive stream[%d] to %s:%d"), liveQIdx,
           inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

  srv_lock_conn_mutexes(pConn, 1);
  pthread_mutex_lock(&pLiveQ->mtx);

  if(pLiveQ->numActive > 0) {
    pLiveQ->numActive--;
  }
  if(pLiveQ->pQs) {
    pLiveQ->pQs[liveQIdx] = NULL;
  }
  if(pConn->pStreamerCfg1) {
    if(pLiveQ->numActive > 0) {
      pLiveQ->numActive--;
    }
    if(pConn->pStreamerCfg1->liveQs[outidx].pQs) {
      pConn->pStreamerCfg1->liveQs[outidx].pQs[liveQIdx] = NULL;
    }
  }
  pthread_mutex_unlock(&pLiveQ->mtx);
  srv_lock_conn_mutexes(pConn, 0);

  if(pQueue) {
    pktqueue_destroy(pQueue);
  }
  if(pstats) {
    //
    // Destroy automatically detaches the stats from the monitor linked list
    //
    stream_stats_destroy(&pstats, NULL);
  }

  // TODO: this should return < 0 to force http connection to end
  return rc;
}

static int get_requested_outidx(const CLIENT_CONN_T *pConn, STREAMER_CFG_T *pStreamerCfg, 
                                const char **ppuri, char *urlbuf, int *phaveoutidx, int *poutidx) {
  int rc = 0;
  size_t sz = 0;
  const char *pend;

  *ppuri = pConn->httpReq.puri;
  pend = *ppuri;

  //
  //
  // Obtain the requested xcode output outidx specified in the URI such as '/httplive/2' or '/httplive/prof_2'
  //
  if((*poutidx = outfmt_getoutidx(*ppuri, &pend)) < 0 || *poutidx >= IXCODE_VIDEO_OUT_MAX) {
    *poutidx = 0;
  } else {

    if(pend && pend != *ppuri && strlen(pend) > 0) {
      *phaveoutidx = 1;
    }

    //
    // Cut out the end of the URL containing the outidx '/httplive[/2]' or '/httplive[/prof_2]'
    //
    strncpy(urlbuf, *ppuri, HTTP_URL_LEN);
    if((sz = (pend - *ppuri)) < HTTP_URL_LEN) {
      urlbuf[sz] = '\0';
    }
    *ppuri = urlbuf;
    if(*poutidx > 0) {
      if(pStreamerCfg && !pStreamerCfg->xcode.vid.out[*poutidx].active) {
        LOG(X_ERROR("Output format index[%d] not active"), *poutidx);
        rc = -1;
      } else {
        LOG(X_DEBUG("Set output format index to[%d] url:'%s', for %s:%d"), *poutidx,
            pConn->httpReq.puri, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));
      }
    }
  }

  return rc;
}

int get_requested_path(const char *uriPrefix, 
                              const char *puri,
                              const char **ppargpath, 
                              const char **ppargrsrc) {
  int rc = 0;
  size_t sz;

  if((sz = strlen(uriPrefix)) > strlen(puri)) {
    return -1;
  }

  *ppargpath = &puri[sz];

  while(**ppargpath == '/') {
    (*ppargpath)++;
  }
  sz = strlen(*ppargpath);
  while(sz > 0 && (*ppargpath)[sz - 1] != '/') {
    sz--;
  }
  *ppargrsrc = &(*ppargpath)[sz];

  return rc;
}

static int send_mediafile(CLIENT_CONN_T *pConn, 
                          const char *path, 
                          HTTP_STATUS_T *pHttpStatus, 
                          const char *contentType) {
                   
  int rc = 0;
  FILE_STREAM_T fileStream;
  HTTP_MEDIA_STREAM_T mediaStream;

  memset(&fileStream, 0, sizeof(fileStream));
  if(rc == 0 && OpenMediaReadOnly(&fileStream, path) < 0) {
    *pHttpStatus = HTTP_STATUS_NOTFOUND;
    return -1;
  }

  memset(&mediaStream, 0, sizeof(mediaStream));
  mediaStream.pFileStream = &fileStream;

  //
  // Get the media file properties, like size, content type,
  //
  if(srv_ctrl_initmediafile(&mediaStream, 0) < 0) {
    CloseMediaFile(&fileStream);
    *pHttpStatus = HTTP_STATUS_NOTFOUND;
    return -1;
  }

  //
  // Check and override the content type
  //
  if(contentType) {
    mediaStream.pContentType = contentType;
  }

  *pHttpStatus = HTTP_STATUS_OK;
  rc = http_resp_sendmediafile(pConn, pHttpStatus, &mediaStream,
  0, 0);
  // HTTP_THROTTLERATE, 1);
  // .5, 4);

  //fprintf(stderr, "RESP_SENDMEDIAFILE '%s' rc:%d\n", fileStream.filename, rc);

  CloseMediaFile(&fileStream);

  return rc;
}

int srv_ctrl_mooflive(CLIENT_CONN_T *pConn, 
                      const char *uriPrefix, 
                      const char *virtFilePath,
                      const char *filePath, 
                      HTTP_STATUS_T *pHttpStatus) {
  int rc = 0;
  const char *pargrsrc = NULL;
  const char *pargpath  = NULL;
  char hostpath[128];
  const char *outdir;
  const char *contentType = NULL;
  const char *ext;
  char stroutidx[32];
  char path[VSX_MAX_PATH_LEN];
  unsigned char buf[PREPROCESS_FILE_LEN_MAX];
  size_t sz = 0;
  size_t sz2;
  const SRV_LISTENER_CFG_T *pListenHttp = NULL;
  char url[HTTP_URL_LEN];
  const char *puri;
  int haveoutidx = 0;
  int outidx = 0;
  STREAMER_CFG_T *pStreamerCfg = NULL;

  if(!pConn || !uriPrefix) {
    return -1;
  }

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);
  //
  // pStreamerCfg may be null if we have been invoked from src/mgr/srvmvgr.c context
  //

  if(pStreamerCfg && !pStreamerCfg->action.do_moofliverecord) {
    LOG(X_ERROR(VSX_DASH_URL" DASH not enabled"));
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return -1;
  }

  stroutidx[0] = '\0';

  //
  // Remove any trailing '/2' stream outidx designation and alter the URI, storing it in 'url'
  //
  if((rc = get_requested_outidx(pConn, pStreamerCfg, &puri, url, &haveoutidx, &outidx)) < 0) {
    *pHttpStatus = HTTP_STATUS_SERVERERROR;
    return -1;
  } else if(haveoutidx) {
    snprintf(stroutidx, sizeof(stroutidx), "%d", outidx + 1);
  }

  //
  // Set pargpath to proceed the '/dash' in the URI
  // Set pargsrc to the resource name without any leading directory path
  //
  if(rc >= 0 && (rc = get_requested_path(uriPrefix, puri, &pargpath, &pargrsrc)) < 0) {
    *pHttpStatus = HTTP_STATUS_SERVERERROR;
    return -1;
  }

  VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_mooflive uri: '%s', virt-file: '%s', on-disk: '%s', "
                  "prefix: '%s', outidx: %d, pargrsrc: '%s', pargpath: '%s'"),
                   pConn->httpReq.puri, virtFilePath, filePath, uriPrefix, outidx, pargrsrc, pargpath));

  //
  // Return rsrc/dash_embed.html file
  //
  if(rc == 0 && (!pargrsrc || pargrsrc [0] == '\0')) {

    //
    // Validate & Authenticate the request
    //
    if((rc = http_check_pass(pConn)) < 0) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      return rc;
    }

    //
    // Find the correct listener for the flvlive FLV data source
    //
    if(!(pListenHttp = srv_ctrl_findlistener(pConn->pCfg->pListenHttp, SRV_LISTENER_MAX_HTTP, URL_CAP_MOOFLIVE,
                                     pConn->pListenCfg->netflags, NETIO_FLAG_SSL_TLS))) {
      pListenHttp = srv_ctrl_findlistener(pConn->pCfg->pListenHttp, SRV_LISTENER_MAX_HTTP, 
                                               URL_CAP_MOOFLIVE, 0, 0);
      //
      // pListenHttp may be null when called from mgr context
      //
    }

    if(!filePath || !virtFilePath) {
      snprintf(path, sizeof(path), VSX_DASH_URL"%s%s", pargpath ? "/" : "", pargpath ? pargpath : "");
      virtFilePath = filePath = path;
    }

    if(pListenHttp) {
      snprintf(hostpath, sizeof(hostpath), URL_HTTP_FMT_STR, URL_HTTP_FMT_ARGS(pListenHttp));
    } else {
      hostpath[0] = '\0';
    }

    if((sz = strlen(virtFilePath)) < 1) {
      sz = 1;
    }

    //
    // Return the default DASH MPD file, which is stored in the html output directory
    //
    if(snprintf(url, sizeof(url), "%s%s%s%s"DASH_MPD_DEFAULT_NAME,
                  hostpath, virtFilePath, virtFilePath[sz - 1] == '/' ? "" : "/", stroutidx) > 0 &&
     (rc = update_dash_indexfile(pConn, pStreamerCfg, url, outidx,
                                         (char *) buf, sizeof(buf))) > 0) {

      VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_mooflive sending substituted index file with media: '%s'"
                                   ", hostpath: '%s', virtFilePath: '%s'"), url, hostpath, virtFilePath));

      rc = http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_OK, buf, rc);
    } else {
      *pHttpStatus = HTTP_STATUS_SERVERERROR;
      rc = -1; 
    }

    return rc;
  }

  if(!filePath) {
    filePath = pargpath;
  }

  if(rc == 0) {
    mediadb_fixdirdelims((char *) filePath, DIR_DELIMETER);
  }

  path[0] = '\0';
  outdir = pConn->pCfg->pMoofCtxts[outidx]->dashInitCtxt.outdir;

  if(rc >= 0 && filePath) {

    if(strlen(filePath) <= 4) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      return -1;
    }

    sz = strlen(filePath);
    sz2 = strlen("."HTTPLIVE_TS_NAME_EXT);

    //
    // Check for default synonym 'dash/default.mpd'
    //
    //if(!strncmp(filePath, DASH_MPD_DEFAULT_ALIAS_NAME, strlen(DASH_MPD_DEFAULT_ALIAS_NAME) + 1)) {
    //  filePath = pargrsrc = DASH_MPD_DEFAULT_NAME;
    //} else 
    if(!strncasecmp(&filePath[sz - sz2], "."HTTPLIVE_TS_NAME_EXT, strlen("."HTTPLIVE_TS_NAME_EXT))) {
      //
      // Load anything with a ".ts" extension from /html/httplive 'outdir_ts'
      //
      outdir = pConn->pCfg->pMoofCtxts[outidx]->dashInitCtxt.outdir_ts;
    }

  }

  if(!outdir) {
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return -1;
  //
  // Convert the URI path to the html/[dash | httplive] output directory file path
  //
  } else if(rc >= 0 && mediadb_prepend_dir(outdir, filePath, path, sizeof(path)) < 0) {
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return -1;
  }

  //fprintf(stderr, "HTTP DASH REQUEST /dash uri:'%s' ('%s'), pargpath:'%s', pargsrc:'%s' rc:%d prfx:'%s' fp:'%s', path:'%s',outdir:'%s', outdir_ts:'%s'\n", pConn->httpReq.puri, puri, pargpath, pargrsrc, rc, uriPrefix, filePath, path, pConn->pCfg->pMoofCtxts[outidx]->dashInitCtxt.outdir, pConn->pCfg->pMoofCtxts[outidx]->dashInitCtxt.outdir_ts);

  if(rc >= 0) {

    sz = strlen(filePath);
    sz2 = strlen(DASH_MPD_EXT);
    if(!strcasecmp(&filePath[sz - sz2], DASH_MPD_EXT)) {

      //
      // Send an MPD file created in the html output directory
      //
      rc = http_resp_sendfile(pConn, pHttpStatus, path, CONTENT_TYPE_DASH_MPD, HTTP_ETAG_AUTO);

    } else {

      //
      // Check the URL_CAP_MOOFLIVE capability because it is not necessary if only
      // returning the main index html
      //
      if(!(pConn->pListenCfg->urlCapabilities & URL_CAP_MOOFLIVE)) {
        *pHttpStatus = HTTP_STATUS_FORBIDDEN;
        return -1;
      }

      //
      // For .m4s moof data files, there is no way to tell if the mp4 file is audio or video
      // related, even by introspecting it, so we try to look at the filename to see if it belongs
      // the the mpd audio adaptation set and set the content type accordingly
      //
      if((ext = strutil_getFileExtension(path)) && !strcmp(ext, DASH_DEFAULT_SUFFIX_M4S) &&
         (ext = mpd_path_get_adaptationtag(path))) {
        if(ext[0] == DASH_MP4ADAPTATION_TAG_AUD[0]) {
          contentType = CONTENT_TYPE_M4A;
        } else if(ext[0] == DASH_MP4ADAPTATION_TAG_VID[0]) {
          contentType = CONTENT_TYPE_M4V;
        }
      }

      // 
      // Send a media file segment from the html output directory
      //
      rc = send_mediafile(pConn, path, pHttpStatus, contentType);
    }

  }

  return rc;
}

int srv_ctrl_rtmp(CLIENT_CONN_T *pConn, 
                  const char *pargfile, 
                  int is_remoteargfile, 
                  const char *rsrcUrl, 
                  const SRV_LISTENER_CFG_T *pListenRtmp,
                  unsigned int outidx) {
  int rc = 0;
  char path[VSX_MAX_PATH_LEN];
  size_t sz = 0;
  const char *contentType = NULL;
  STREAMER_CFG_T *pStreamerCfg = NULL;

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_rtmp file: '%s', is_remoteargfile: %d, outidx: %d"), 
                             pargfile, is_remoteargfile, outidx));

  if(pargfile && pargfile[0] != '\0') {

    //
    // Return a static file from the html directory
    //

    if(mediadb_prepend_dir2(pConn->pCfg->pMediaDb->homeDir, 
           VSX_RSRC_HTML_PATH, pargfile, (char *) path, sizeof(path)) < 0) {
      rc = http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 
         (unsigned char *) HTTP_STATUS_STR_NOTFOUND, strlen(HTTP_STATUS_STR_NOTFOUND));

    } else {

      if((sz = strlen(path)) > 5) {
        if(strncasecmp(&path[sz - 4], ".swf", 4) == 0) {
          contentType = CONTENT_TYPE_SWF;
        } else if(strncasecmp(&path[sz - 5], ".html", 5) == 0) {
          contentType = CONTENT_TYPE_TEXTHTML;
        }
      }
      //fprintf(stderr, "path:'%s' ct:'%s'\n", path, contentType);
      rc = http_resp_sendfile(pConn, NULL, path, contentType, HTTP_ETAG_AUTO);

    }

    return rc;
  } else {

    //
    // Return the rsrc/rtmp_embed.html file with all substitions
    //
    return resp_index_file(pConn, pargfile, is_remoteargfile, pListenRtmp, URL_CAP_RTMPLIVE);
  }

  return rc;
}

int srv_ctrl_rtsp(CLIENT_CONN_T *pConn, 
                  const char *pargfile, 
                  int is_remoteargfile, 
                  const char *rsrcUrl) {
  int rc = 0;
  char path[VSX_MAX_PATH_LEN];
  size_t sz = 0;
  const char *contentType = NULL;
  STREAMER_CFG_T *pStreamerCfg = NULL;
  unsigned char buf[PREPROCESS_FILE_LEN_MAX];

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  if(pargfile && pargfile[0] != '\0') {

    if(mediadb_prepend_dir(pConn->pCfg->pMediaDb->homeDir,
               VSX_RSRC_HTML_PATH, (char *) buf, sizeof(buf)) < 0 ||
       mediadb_prepend_dir((char *) buf, pargfile, path, sizeof(path)) < 0) {

      rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 0, NULL, NULL);
      rc = -1;

    } else {

      if((sz = strlen(path)) > 5) {
        if(strncasecmp(&path[sz - 4], ".png", 4) == 0) {
          contentType = CONTENT_TYPE_IMAGEPNG;
        } else if(strncasecmp(&path[sz - 5], ".html", 5) == 0) {
          contentType = CONTENT_TYPE_TEXTHTML;
        }
      }
      rc = http_resp_sendfile(pConn, NULL, path, contentType, HTTP_ETAG_AUTO);
    }

    return rc;

  } else {

    //
    // Return the rsrc/rtsp_embed.html file with all substitions
    //
    return resp_index_file(pConn, pargfile, is_remoteargfile, NULL, URL_CAP_RTSPLIVE);
  }

  return rc;
}

const SRV_LISTENER_CFG_T *srv_ctrl_findlistener(const SRV_LISTENER_CFG_T *arrCfgs,
                                                unsigned int maxCfgs, enum URL_CAPABILITY urlCap,
                                                NETIO_FLAG_T netflags, NETIO_FLAG_T netflagsMask) {
  unsigned int idx;

  if(arrCfgs) {

    for(idx = 0; idx < maxCfgs; idx++) {

      if(arrCfgs[idx].active && (arrCfgs[idx].urlCapabilities & urlCap)) {
        if(netflagsMask == 0 || (arrCfgs[idx].netflags & netflagsMask) == netflags) {
          return  &arrCfgs[idx];
        }

      }
    }

  }

  return NULL;
}

int srv_ctrl_httplive(CLIENT_CONN_T *pConn, 
                      const char *uriPrefix, 
                      const char *virtFilePath, 
                      const char *filePath,
                      HTTP_STATUS_T *pHttpStatus) {
  int rc = 0;
  const char *pargrsrc = NULL;
  const char *pargpath  = NULL;
  char hostpath[128];
  char path[VSX_MAX_PATH_LEN];
  struct stat st;
  unsigned char *presp = NULL;
  unsigned int lenresp = 0;
  const SRV_LISTENER_CFG_T *pListenHttp = NULL;
  char url[HTTP_URL_LEN];
  const char *puri, *pend;
  unsigned char buf[PREPROCESS_FILE_LEN_MAX];
  size_t sz;
  int haveoutidx = 0;
  int outidx;
  STREAMER_CFG_T *pStreamerCfg = NULL;

  if(!pConn || !uriPrefix) {
    return -1;
  }
/*
  if(!filePath) {
    filePath = virtFilePath;
  } else if(!virtFilePath) {
    virtFilePath = filePath;
  }
*/

  pStreamerCfg = GET_STREAMER_FROM_CONN(pConn);

  if(pStreamerCfg && !pStreamerCfg->action.do_httplive) {
    LOG(X_ERROR(VSX_HTTPLIVE_URL" httplive not enabled"));
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return -1;
  }

  //
  // Remove any trailing '/2' stream outidx designation and alter the URI, storing it in 'url'
  //
  if((rc = get_requested_outidx(pConn, pStreamerCfg, &puri, url, &haveoutidx, &outidx)) < 0) {
    *pHttpStatus = HTTP_STATUS_SERVERERROR;
  }

  //
  // Set pargpath to proceed the '/httplive' in the URI
  // Set pargsrc to the resource name without any leading directory path
  //
  if(rc >= 0 && (rc = get_requested_path(uriPrefix, puri, &pargpath, &pargrsrc)) < 0) {
    *pHttpStatus = HTTP_STATUS_SERVERERROR;
  }

  VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_httplive uri: '%s', virt-file: '%s', on-disk: '%s', "
        "prefix: '%s', haveoutidx: %d, outidx: %d, pargrsrc: '%s', pargpath: '%s'"), 
         pConn->httpReq.puri, virtFilePath, filePath, uriPrefix, haveoutidx, outidx, pargrsrc, pargpath));

  //
  // Return rsrc/httplive_embed.html file
  //
  if(rc == 0 && (!pargrsrc || pargrsrc [0] == '\0')) {

    //
    // Validate & Authenticate the request
    //
    if((rc = http_check_pass(pConn)) < 0) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      return rc;
    } 

    presp = buf;
    lenresp = sizeof(buf);

    //
    // Check for the existance of a default httplive/index.html
    //
    if(mediadb_prepend_dir(pConn->pCfg->pHttpLiveDatas[outidx]->dir,
                 HTTPLIVE_INDEX_HTML, path, sizeof(path)) == 0 && fileops_stat(path, &st) == 0) {

      VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_httplive sending pre-existing index file path: '%s'"), path));

      rc = http_resp_sendfile(pConn, pHttpStatus, path, CONTENT_TYPE_TEXTHTML, NULL);

    } else {

      if(!filePath || !virtFilePath) {
        snprintf(path, sizeof(path), VSX_HTTPLIVE_URL"%s%s", pargpath ? "/" : "", pargpath ? pargpath : "");
        virtFilePath = filePath = path;
      }
      
      //
      // If multiple output xcode is set, by default return master m3u8 containing bitrate
      // specific m3u8 playlists
      //
      pend = pConn->pCfg->pHttpLiveDatas[outidx]->fileprefix;
      if(!haveoutidx && pConn->pCfg->pHttpLiveDatas[0]->count > 1) {
        pend = HTTPLIVE_MULTIBITRATE_NAME_PRFX;
      }

      //
      // Find the correct listener for the data source
      // and construct the hostname url for the .m3u8 master playlist
      //
      if(!(pListenHttp = srv_ctrl_findlistener(pConn->pCfg->pListenHttp, SRV_LISTENER_MAX_HTTP, 
                                  URL_CAP_TSHTTPLIVE, pConn->pListenCfg->netflags, NETIO_FLAG_SSL_TLS))) {
        pListenHttp = srv_ctrl_findlistener(pConn->pCfg->pListenHttp, SRV_LISTENER_MAX_HTTP, 
                                  URL_CAP_TSHTTPLIVE, 0, 0);
        //
        // pListenHttp may be null when called from mgr context
        //
      }

      if(pListenHttp) {
        snprintf(hostpath, sizeof(hostpath), URL_HTTP_FMT_STR, URL_HTTP_FMT_ARGS(pListenHttp));
      } else {
        hostpath[0] = '\0';
      }

      if((sz = strlen(virtFilePath)) < 1) {
        sz = 1;
      }

      //
      // Use html/rsrc/httplive_embed.html
      // 
      if(rc >= 0 && 
         snprintf(url, sizeof(url), "%s%s%s%s"HTTPLIVE_PL_NAME_EXT,  
                  hostpath, virtFilePath, virtFilePath[sz - 1] == '/' ? "" : "/", pend) > 0 &&
         (rc = update_httplive_indexfile(pConn, pStreamerCfg, url, outidx,
                                         (char *) buf, sizeof(buf))) > 0) {

        VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_httplive sending substituted index file with media: '%s'"
               ", hostpath: '%s', virtFilePath: '%s' ( ext: '%s', count: %d) "), 
               url, hostpath, virtFilePath, pend, pConn->pCfg->pHttpLiveDatas[0]->count));

        rc = http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_OK, buf, rc);

      } else {
        //
        // Generate an index.html document 
        //
        if((rc = httplive_getindexhtml(hostpath, virtFilePath, pend, presp, &lenresp)) >= 0) {

          VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_httplive sending auto-generated index file")));

          rc = http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_OK, presp, lenresp); 

        } else {
          LOG(X_WARNING("%s - HTTPLive not available"), HTTP_STATUS_STR_NOTFOUND);
            *pHttpStatus = HTTP_STATUS_SERVERERROR;
            rc = -1;
        }
      }

    }

    return rc;
  }

  //
  // Check the URL_CAP_TSHTTPLIVE capability because it is not necessary if only 
  // returning the main index html 
  //
  if(!(pConn->pListenCfg->urlCapabilities & URL_CAP_TSHTTPLIVE)) {
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return -1;
  }

  if(!filePath) {
    filePath = pargpath;
  }

  if(rc >= 0) {
    mediadb_fixdirdelims((char *) filePath, DIR_DELIMETER);
  }

  if(rc >= 0 && mediadb_prepend_dir(pConn->pCfg->pHttpLiveDatas[outidx]->dir, filePath, path, sizeof(path)) < 0) {
    *pHttpStatus = HTTP_STATUS_NOTFOUND;
    rc = -1;
  }

  if(rc >= 0) {
    VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_httplive sending media file: '%s' "), path));
    rc = send_mediafile(pConn, path, pHttpStatus, NULL);
  }

  return rc;
}


int srv_ctrl_flv(CLIENT_CONN_T *pConn, 
                 const char *pargfile,  
                 int is_remoteargfile, 
                 const SRV_LISTENER_CFG_T *pListenHttp) {

  int rc = 0;

  VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_flv file: '%s', is_remoteargfile: %d"), pargfile, is_remoteargfile));

  //
  // Return the rsrc/http_embed.html file with all substitions
  //
  rc = resp_index_file(pConn, pargfile, is_remoteargfile, pListenHttp, URL_CAP_FLVLIVE);

  return rc;
}

int srv_ctrl_mkv(CLIENT_CONN_T *pConn, 
                 const char *pargfile, 
                 int is_remoteargfile, 
                 const SRV_LISTENER_CFG_T *pListenHttp) {
  int rc = 0;
  char tmp[VSX_MAX_PATH_LEN];
  char buf[VSX_MAX_PATH_LEN];
  size_t sz = 0;
  const char *contentType = NULL;

  VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_mkv file: '%s', is_remoteargfile: %d"), pargfile, is_remoteargfile));

  //
  // We are likely called from the mgr to request the .mkv media file to be showin within an html5 tag
  //
  if(0&&pargfile && pargfile[0] != '\0') {

    if(mediadb_prepend_dir(pConn->pCfg->pMediaDb->homeDir,
               VSX_RSRC_HTML_PATH, (char *) tmp, sizeof(tmp)) < 0 ||
       mediadb_prepend_dir((char *) tmp, pargfile, buf, sizeof(buf)) < 0) {

      rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 0, NULL, NULL);
      rc = -1;

    } else {

      if((sz = strlen(buf)) > 5) {
        if(strncasecmp(&buf[sz - 4], ".png", 4) == 0) {
          contentType = CONTENT_TYPE_IMAGEPNG;
        } else if(strncasecmp(&buf[sz - 5], ".html", 5) == 0) {
          contentType = CONTENT_TYPE_TEXTHTML;
        }
      }

      VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_mkv returning '%s' contentType: '%s'"), pargfile, contentType));
      rc = http_resp_sendfile(pConn, NULL, buf, contentType, HTTP_ETAG_AUTO);

    }

    return rc;
  } else {

    //
    // Return the rsrc/mkv_embed.html file with all substitions
    //
    return resp_index_file(pConn, pargfile, is_remoteargfile, pListenHttp, URL_CAP_MKVLIVE);
  }

}

static STREAM_METHOD_T getBestMethod(const STREAM_DEVICE_T *pdevtype, const CLIENT_CONN_T *pConn) {
  unsigned int idx;
  const STREAMER_CFG_T *pStreamerCfg = NULL;

  if(pConn->pStreamerCfg0) {
    pStreamerCfg = pConn->pStreamerCfg0;
  } else if(pConn->pStreamerCfg1) {
    pStreamerCfg = pConn->pStreamerCfg1;
  } else {
    return pdevtype->methods[0];
  }

  //
  // Check if the preferred streaming output method is available
  //
  for(idx = 0; idx < STREAM_DEVICE_METHODS_MAX; idx++) {

    if(!((pdevtype->methods[idx] == STREAM_METHOD_MKVLIVE && !pStreamerCfg->action.do_mkvlive) ||
       (pdevtype->methods[idx] == STREAM_METHOD_FLVLIVE && !pStreamerCfg->action.do_flvlive) ||
       (pdevtype->methods[idx] == STREAM_METHOD_RTMP && !pStreamerCfg->action.do_rtmplive) ||
       (pdevtype->methods[idx] == STREAM_METHOD_HTTPLIVE && !pStreamerCfg->action.do_httplive) ||
       (pdevtype->methods[idx] == STREAM_METHOD_TSLIVE && !pStreamerCfg->action.do_tslive) ||
       ((pdevtype->methods[idx] == STREAM_METHOD_RTSP ||
         pdevtype->methods[idx] == STREAM_METHOD_RTSP_INTERLEAVED ||
         pdevtype->methods[idx] == STREAM_METHOD_RTSP_HTTP) && !pStreamerCfg->action.do_rtsplive) ||
       (pdevtype->methods[idx] == STREAM_METHOD_DASH && !pStreamerCfg->action.do_moofliverecord))) {
      break;
    }
  }

  if(idx >= STREAM_DEVICE_METHODS_MAX || (idx > 0 && (pdevtype->methods[idx] == STREAM_METHOD_UNKNOWN ||
     pdevtype->methods[idx] == STREAM_METHOD_INVALID))) {
    idx--;
  }

  return pdevtype->methods[idx];
}

int srv_ctrl_live(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus, const char *accessUri) {
  int rc = 0;
  const char *pargfile = NULL;
  const char *parg = NULL;
  const char *p;
  int have_outidx = 0;
  const STREAM_DEVICE_T *pdevtype = NULL;
  size_t sz = 0;
  STREAM_METHOD_T streamMethod = STREAM_METHOD_UNKNOWN;

  //rsrcurl[0] = '\0';

  //
  // Validate & Authenticate the request
  //
  if((rc = http_check_pass(pConn)) < 0) {
    *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    return rc;
  }

  if(!strncasecmp(pConn->httpReq.puri, VSX_RTSP_URL, strlen(VSX_RTSP_URL))) {

    streamMethod = STREAM_METHOD_RTSP;
    sz = strlen(VSX_RTSP_URL);
    if(!accessUri) {
      accessUri = VSX_RTSP_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_RTMP_URL, strlen(VSX_RTMP_URL))) {

    streamMethod = STREAM_METHOD_RTMP;
    sz = strlen(VSX_RTMP_URL);
    if(!accessUri) {
      accessUri = VSX_RTMP_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_FLV_URL, strlen(VSX_FLV_URL))) {

    streamMethod = STREAM_METHOD_FLVLIVE;
    sz = strlen(VSX_FLV_URL);
    if(!accessUri) {
      accessUri = VSX_FLV_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_MKV_URL, strlen(VSX_MKV_URL))) {

    streamMethod = STREAM_METHOD_MKVLIVE;
    sz = strlen(VSX_MKV_URL);
    if(!accessUri) {
      accessUri = VSX_MKV_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_WEBM_URL, strlen(VSX_WEBM_URL))) {

    streamMethod = STREAM_METHOD_MKVLIVE;
    sz = strlen(VSX_WEBM_URL);
    if(!accessUri) {
      accessUri = VSX_WEBM_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_HTTPLIVE_URL, strlen(VSX_HTTPLIVE_URL))) {

    streamMethod = STREAM_METHOD_HTTPLIVE;
    sz = strlen(VSX_HTTPLIVE_URL);
    if(!accessUri) {
      accessUri = VSX_HTTPLIVE_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_DASH_URL, strlen(VSX_DASH_URL))) {

    streamMethod = STREAM_METHOD_DASH;
    sz = strlen(VSX_DASH_URL);
    if(!accessUri) {
      accessUri = VSX_DASH_URL;
    }

  } else if(!strncasecmp(pConn->httpReq.puri, VSX_RSRC_URL, strlen(VSX_RSRC_URL))) {

    streamMethod = STREAM_METHOD_NONE;
    sz = strlen(VSX_RSRC_URL);
    if(!accessUri) {
      accessUri = VSX_RSRC_URL;
    }

  } else {

    sz = strlen(VSX_LIVE_URL);
    if(!accessUri) {
      accessUri = VSX_LIVE_URL;
    }

  }

  if(sz <= strlen(pConn->httpReq.puri)) { 
    pargfile = &pConn->httpReq.puri[sz];

    while(*pargfile == '/') {
      pargfile++;
    }

    //
    // Check if the URL contains an xcode outidx, such as '/live/2' similar to outfmt_getoutidx
    //
    p = pargfile;
    while(*p != '/' && *p != '\0') {
      if(*p >= '0' && *p <= '9') {
        have_outidx = 1;
      } else {
        have_outidx = 0;
        break;
      }
      p++;
    }

    if(have_outidx) {
      while(*p== '/') {
        p++;
      }
      pargfile = p;

    }

  } else {
    rc = -1;
  }

  //fprintf(stderr, "URL...'%s' '%s' have_outidx:%d\n", pConn->httpReq.puri, pargfile ? pargfile : "<NULL>", have_outidx);

  if(pConn->pCfg->livepwd &&
   (!(parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, "pass")) ||
     strcmp(pConn->pCfg->livepwd, parg))) {

    LOG(X_WARNING("live invalid password from %s:%d (%d char given)"),
       inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port),
       parg ? strlen(parg) : 0);

    rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_FORBIDDEN, 0, NULL, NULL);
    return -1;
  }

  //
  // If the URL does not contain the desired access method (such as /live), get the 
  // User-Agent and lookup the device type to get the streaming method
  //
  if(streamMethod == STREAM_METHOD_UNKNOWN &&
     (parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.reqPairs,
                                    HTTP_HDR_USER_AGENT))) {

    //parg = "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0; SAMSUNG; SGH-i937)";

    if(!(pdevtype = devtype_finddevice(parg, 1))) {
      LOG(X_ERROR("No device type found for "HTTP_HDR_USER_AGENT": '%s'"), parg);
      rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_SERVERERROR,
                             0, NULL, NULL);
      return -1;
    }

    streamMethod = getBestMethod(pdevtype, pConn);

    LOG(X_DEBUG("Found streaming method: '%s' devname: '%s' for "HTTP_HDR_USER_AGENT": '%s'"), 
               devtype_methodstr(streamMethod), pdevtype->name, parg);
  }



  //
  // Deliver the content via the desired access method
  //
  switch(streamMethod) {

    case STREAM_METHOD_HTTPLIVE:

      if(pConn->pListenCfg->urlCapabilities & (URL_CAP_LIVE | URL_CAP_TSHTTPLIVE)) {
        return srv_ctrl_httplive(pConn, accessUri, NULL, NULL, pHttpStatus);
      } else {
        *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      }
      break;

    case STREAM_METHOD_DASH:

      if(pConn->pListenCfg->urlCapabilities & (URL_CAP_LIVE | URL_CAP_MOOFLIVE)) {
        return srv_ctrl_mooflive(pConn, accessUri, NULL, NULL, pHttpStatus);
      } else {
        *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      }
      break;

    case STREAM_METHOD_TSLIVE:

      if(pConn->pListenCfg->urlCapabilities & URL_CAP_TSLIVE) {
        return srv_ctrl_tslive(pConn, pHttpStatus);
      } else {
        *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      }
      break;

    case STREAM_METHOD_RTSP:
    case STREAM_METHOD_RTSP_INTERLEAVED:
    case STREAM_METHOD_RTSP_HTTP:

      rc = srv_ctrl_rtsp(pConn, pargfile, 0, NULL);
      break;


    case STREAM_METHOD_FLVLIVE:

      rc = srv_ctrl_flv(pConn, pargfile, 0, NULL);
      break;

    case STREAM_METHOD_MKVLIVE:

      rc = srv_ctrl_mkv(pConn, pargfile, 0, NULL);
      break;

    case STREAM_METHOD_PROGDOWNLOAD:
    case STREAM_METHOD_FLASHHTTP:
      LOG(X_ERROR("Streaming method '%s' not suitable for live delivery."), devtype_methodstr(streamMethod));
      rc = -1;
      break;

    case STREAM_METHOD_RTMP:
    case STREAM_METHOD_NONE:
    default:

      VSX_DEBUG_LIVE(LOG(X_DEBUG("LIVE - srv_ctrl_live calling srv_ctrl_rtmp.. file: '%s' ."), pargfile));

      rc = srv_ctrl_rtmp(pConn, pargfile, 0, NULL, NULL, 0);
      break;
  }

  return rc;
}

