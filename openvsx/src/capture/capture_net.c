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

typedef struct CBDATA_GETDATA_PKTQ {
  PKTQUEUE_T               *pQ;
  STREAM_XMIT_ACTION_T     *pXmitAction;
  STREAM_XMIT_CB_T         *pXmitCb;
} CBDATA_GETDATA_PKTQ_T;

//typedef struct CBDATA_CAP_ASYNC {
//  CAP_ASYNC_DESCR_T capture;
//  STREAMER_CFG_T    *pStreamerCfg; 
//} CBDATA_CAP_ASYNC_T;


//
// Should fit the profile of CBSTREAM_GETDATA
//
int cbstream_getdata_pktqueue(void *p, unsigned char *pBuf, 
                              unsigned int len, PKTQ_EXTRADATA_T *pXtra, 
                              unsigned int *pNumLost) {

  CBDATA_GETDATA_PKTQ_T *pCbData = (CBDATA_GETDATA_PKTQ_T *) p;
  PKTQUEUE_T *pQueue = pCbData->pQ;
  unsigned int idxBuf = 0;
  COLLECT_STREAM_PKTDATA_T pkt;
  int rc;

  //fprintf(stderr, "cbstream_getdata_pktqueue len:%d\n", len);

  do {

    if((rc = pktqueue_readexact(pQueue, &pBuf[idxBuf], len - idxBuf)) < 0) {
      LOG(X_ERROR("Error reading %d from packet queue"), len - idxBuf);
      return rc;
    } else if(rc == 0) {

      pktqueue_waitforunreaddata(pQueue); 

    } else {
      idxBuf += rc;
    }

  } while(idxBuf < len && !pQueue->quitRequested);

  if(pNumLost) {
    //TODO: this is outside of atomic mutex protection
    *pNumLost = pQueue->numOverwritten;
  }

  // 
  // Perform pre-streaming recording
  // 
  if(idxBuf > 0 && pCbData->pXmitAction && pCbData->pXmitCb && pCbData->pXmitAction->do_record_pre &&
     pCbData->pXmitCb->cbRecord && pCbData->pXmitCb->pCbRecordData) {

    memset(&pkt, 0, sizeof(pkt));
    pkt.payload.pData = pBuf;
    PKTCAPLEN(pkt.payload) = idxBuf;
    rc = pCbData->pXmitCb->cbRecord(pCbData->pXmitCb->pCbRecordData, &pkt); 

  }

  //fprintf(stderr, "got %d/%d bytes from input queue\n", idxBuf, len);
  return idxBuf;
}

//
// Should fit the profile of CBSTREAM_GETDATA
//
int cbstream_havedata_pktqueue(void *p) {
  return 1;
}

//
// Should fit the profile of CBSTREAM_RESETDATA
//

int cbstream_resetdata_pktqueue(void *p) {
  CBDATA_GETDATA_PKTQ_T *pCbData = (CBDATA_GETDATA_PKTQ_T *) p;
  PKTQUEUE_T *pQueue = pCbData->pQ;
  int rc = 0;

  LOG(X_DEBUG("Resetting capture packet queue(id:%d) (sz:%u)"), 
                  pQueue->cfg.id,  pQueue->cfg.maxPkts);
  //fprintf(stderr, "Resetting capture packet queue(id:%d)", pQueue->cfg.id);

  rc = pktqueue_reset(pQueue, 1);

  return rc;
}


//
// Should fit the profile of CBSTREAM_GETDATA
//
int cbstream_getpkt_pktqueue(void *p, unsigned char *pBuf, 
                             unsigned int len, PKTQ_EXTRADATA_T *pXtra, 
                             unsigned int *pNumLost) {
  CBDATA_GETDATA_PKTQ_T *pCbData = (CBDATA_GETDATA_PKTQ_T *) p;
  PKTQUEUE_T *pQueue = pCbData->pQ;
  int rc;

  //fprintf(stderr, "cbstream_getpkt_pktqueue\n");

  do {

    //fprintf(stderr, "cbstream_getpkt_pktqueue::pktqueue_readpkt\n");

    if((rc = pktqueue_readpkt(pQueue, pBuf, len, pXtra, pNumLost)) < 0) {
      LOG(X_ERROR("Error cb reading %d from packet queue"), len);
      return rc;
    } else if(rc == 0) {
      pktqueue_waitforunreaddata(pQueue); 
    } else {
      break;
    }

    if(g_proc_exit) {
      //fprintf(stderr, "cbstream_getpkt_pktqueue exiting\n");
      return -1;
    }

  } while(rc == 0 && !pQueue->quitRequested);

  //fprintf(stderr, "cbstream_getpkt_pktqueue got %d / %d bytes!\n", rc, len);
  return rc;
}

/*
//
// Should fit the profile of CBSTREAM_GETDATA
//
int cbstream_getpkt_pktqueue_nowait(void *p, unsigned char *pBuf, 
                         unsigned int len, PKTQ_EXTRADATA_T *pXtra, 
                         unsigned int *pNumLost) {

  CBDATA_GETDATA_PKTQ_T *pCbData = (CBDATA_GETDATA_PKTQ_T *) p;
  PKTQUEUE_T *pQueue = pCbData->pQ;
  int rc;

  //fprintf(stderr, "cbstream_getpkt_pktqueue_nowait\n");

  if((rc = pktqueue_readpkt(pQueue, pBuf, len, pXtra, pNumLost)) < 0) {
    LOG(X_ERROR("Error cb reading %d from packet queue"), len);
    return rc;
  }

  return rc;
}
*/

static void capture_setstate(CAP_ASYNC_DESCR_T *pCapCfg, int runningState, int lock) {
  if(lock) {
    pthread_mutex_lock(&pCapCfg->mtx);
  }

  pCapCfg->running = runningState;

  if(lock) {
    pthread_mutex_unlock(&pCapCfg->mtx);
  }
}

static void capture_pktqueue_rdproc(void *parg) {
  CAP_ASYNC_DESCR_T *pCap = (CAP_ASYNC_DESCR_T *) parg;
  int rc = 0;

  logutil_tid_add(pthread_self(), pCap->tid_tag);

  capture_setstate(pCap, STREAMER_STATE_RUNNING, 1);

/*
  if(pCap->pcommon->filt.filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_VID_CONFERENCE ||
     pCap->pcommon->filt.filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_AUD_CONFERENCE) {

    rc = capture_dummyStart(pCap);

  } else 
*/
  if(IS_CAPTURE_FILTER_TRANSPORT_HTTP(pCap->pcommon->filt.filters[0].transType)) {

    if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pCap->pcommon->filt.filters[0].transType)) {
      pCap->pSockList->netsockets[0].flags |= NETIO_FLAG_SSL_TLS; 
    }

    pCap->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_UNKNOWN;

    rc = capture_httpget(pCap);

  } else if(pCap->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTMP ||
            pCap->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTMPS) {

    if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pCap->pcommon->filt.filters[0].transType)) {
      pCap->pSockList->netsockets[0].flags |= NETIO_FLAG_SSL_TLS; 
    }

    pCap->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_UNKNOWN;

    if(pCap->pcommon->addrsExt[0] == NULL || pCap->pcommon->addrsExt[0] == '\0') {
      LOG(X_DEBUG("RTMP capture server mode"));
      rc = capture_rtmp_server(pCap);
    } else {
      LOG(X_DEBUG("RTMP capture client mode URI: '%s'"), pCap->pcommon->addrsExt[0]);
      rc = capture_rtmp_client(pCap);
    }

  } else if(pCap->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_DEV) {
    pCap->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_UNKNOWN;
    rc = capture_devStart(pCap);
  } else { 

    //
    // pCap->pStreamerCfg->sharedCtxt.state should be set to STREAMER_SHARED_STATE_PENDING_CREATED
    // when shared capture/streamer socket creation has finished 
    //
    rc = capture_socketStart(pCap);

    pCap->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_UNKNOWN;

  }


#if defined(VSX_HAVE_STREAMER)
  //
  // Stop the stream output
  //
  if(pCap->pStreamerCfg) {
    streamer_stop(pCap->pStreamerCfg, 1, 0, 1, 1);
  }
#endif // VSX_HAVE_STREAMER

  capture_setstate(pCap, STREAMER_STATE_FINISHED, 1);

  LOG(X_DEBUG("Exiting capture thread with code: %d"), rc);

  logutil_tid_remove(pthread_self());
}

static void destroyLocalCfg(CAPTURE_LOCAL_DESCR_T *pLocalCfg) {
  unsigned int idx;

  for(idx = 0; idx < pLocalCfg->common.filt.numFilters; idx++) {

    pktqueue_destroy(pLocalCfg->capActions[idx].pQueue);
    pLocalCfg->capActions[idx].pQueue = NULL;

    if(pLocalCfg->capActions[idx].tmpFrameBuf.buf) {
      free(pLocalCfg->capActions[idx].tmpFrameBuf.buf);
      pLocalCfg->capActions[idx].tmpFrameBuf.buf = NULL;
      pLocalCfg->capActions[idx].tmpFrameBuf.sz = 0;
    }

  }
}

static PKTQUEUE_T *create_capQueue(unsigned int pktQueueLen, int id) {
  PKTQUEUE_T *pQueue = NULL;

  if((pQueue = pktqueue_create(pktQueueLen,
                               RTP_JTBUF_PKT_BUFSZ_LOCAL, 0, 0, 0, 0, 1)) == NULL) {
      LOG(X_ERROR("Failed to create packet queue[%d] length: %d x %d"),
                  id, pktQueueLen, RTP_JTBUF_PKT_BUFSZ_LOCAL);
      return NULL;
  }
  pQueue->cfg.id = id;

  LOG(X_DEBUG("Created MPEG-2TS buffer queue[id:%d] length: %d x %d"),
              pQueue->cfg.id, pktQueueLen, RTP_JTBUF_PKT_BUFSZ_LOCAL);


  return pQueue;
}

#if defined(VSX_HAVE_STREAMER)

#define XMIT_ASYNC 1

enum XMIT_ASYNC_TYPE {
  XMIT_ASYNC_TYPE_M2T_PES = 0,
  XMIT_ASYNC_TYPE_M2T_AV,
  XMIT_ASYNC_TYPE_M2T_RAW,
  XMIT_ASYNC_TYPE_M2T_MP4NET,
  XMIT_ASYNC_TYPE_NONE,
};

typedef struct XMIT_ASYNC_DESCR {
  const STREAM_DATASRC_T   *pDataSrcVid;
  const STREAM_DATASRC_T   *pDataSrcAud;
  STREAMER_CFG_T           *pStreamerCfg;
  CAP_ASYNC_DESCR_T        *pCapCfg;
  enum XMIT_ASYNC_TYPE      xmitType;
  XC_CODEC_TYPE_T           vidType;
  XC_CODEC_TYPE_T           audType;
  unsigned int              frvidqslots;
  unsigned int              fraudqslots;
  char                      tid_tag[LOGUTIL_TAG_LENGTH];
  int running;
} XMIT_ASYNC_DESCR_T;

static int xmitout(XMIT_ASYNC_DESCR_T *pXmit) {
  int rc = 0;
  unsigned int idx, idxCap;
  STREAM_PES_T pes;
  STREAM_NET_AV_T av;
  SOCKET_LIST_T *pSockList;
  unsigned int fraudqslots = pXmit->fraudqslots;

  memset(&pes, 0, sizeof(pes));

  //
  // Check if the RTP output is set to utilize the capture socket(s) to preserve outbound src port
  //
  if(pXmit->pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort > 0 &&
     IS_CAPTURE_FILTER_TRANSPORT_RTP(pXmit->pCapCfg->pcommon->filt.filters[0].transType)) {

    for(idx = 0, idxCap = 0; idx < 2; idx++) {

      pSockList = pXmit->pCapCfg->pSockList;
        //
        // Always set 2 output socket pointers, one for each stream_rtp_adddest call, even 
        // if capture is using one socket, such as video & audio muxed on one port
        //
        if(idx == 1 && MIN(2, pSockList->numSockets) == 1) {
          idxCap = 0;
        } 
        pXmit->pStreamerCfg->sharedCtxt.av[idx].capSocket.pnetsock = &pSockList->netsockets[idxCap];

        if(INET_PORT(pSockList->salistRtcp[idxCap]) != 0 &&
           INET_PORT(pSockList->salistRtcp[idxCap]) == INET_PORT(pSockList->salist[idxCap])) {
          pXmit->pStreamerCfg->sharedCtxt.av[idx].capSocket.pnetsockRtcp = 
                pXmit->pStreamerCfg->sharedCtxt.av[idx].capSocket.pnetsock; 
        } else {
          pXmit->pStreamerCfg->sharedCtxt.av[idx].capSocket.pnetsockRtcp = &pSockList->netsocketsRtcp[idxCap];
        }
        idxCap++;
    }

  }

  pXmit->pStreamerCfg->sharedCtxt.active = 1;

  //
  // Delayed output startup used for starting SIP client to open sockets & respond to STUN bindings
  // possibly before receiving 200 OK w/ remote SDP
  //
  if(pXmit->pStreamerCfg->delayed_output > 0) {

    LOG(X_DEBUG("Waiting for delayed stream output"));

    while(!g_proc_exit && pXmit->pStreamerCfg->running == STREAMER_STATE_STARTING && 
          pXmit->pStreamerCfg->delayed_output > 0) {

      //TODO: wait for timeout or until the stream output has been set from server pip
      usleep(50000);
    }

    if(g_proc_exit || pXmit->pStreamerCfg->running != STREAMER_STATE_STARTING) {
      pXmit->xmitType = XMIT_ASYNC_TYPE_NONE;
    }

    if(pXmit->pStreamerCfg->delayed_output > 0) {
      pXmit->pStreamerCfg->delayed_output = 0;
    } else {
      LOG(X_DEBUG("Starting delayed stream output"));
      //
      // Need a better way to update local SDP codec changes 
      //
      for(idx = 0; idx < pXmit->pCapCfg->pcommon->filt.numFilters; idx++) {
        if(codectype_isAud(pXmit->pCapCfg->pcommon->filt.filters[idx].mediaType)) {
          pXmit->audType = pXmit->pCapCfg->pcommon->filt.filters[idx].mediaType;
        } else if(codectype_isVid(pXmit->pCapCfg->pcommon->filt.filters[idx].mediaType)) {
          pXmit->vidType = pXmit->pCapCfg->pcommon->filt.filters[idx].mediaType;
        }
      }
    }
  }

  //
  // If we are using pXmit->pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort then we may need to
  // wait until the capture socket(s) have been created, which may be required for
  // shared socket DTLS streamer output
  //
  //LOG(X_DEBUG("May be Waiting for shared socket creation"));
  if(pXmit->pStreamerCfg->sharedCtxt.state == STREAMER_SHARED_STATE_PENDING_CREATE) {

    LOG(X_DEBUGV("Waiting for shared socket creation"));

    while(!g_proc_exit && pXmit->pStreamerCfg->running == STREAMER_STATE_STARTING && 
          pXmit->pStreamerCfg->sharedCtxt.state == STREAMER_SHARED_STATE_PENDING_CREATE) {

      //TODO: wait for timeout or until the stream output has been set from server pip
      usleep(10000);
    }

    LOG(X_DEBUGV("Done Waiting for shared socket creation"));

  }


  switch(pXmit->xmitType) {

    case XMIT_ASYNC_TYPE_M2T_MP4NET:

      //
      // Use the file based stream_mp4 based reader as the capture will be caching the downloaded
      // mp4 contents on disk
      //
      if((rc = cap_http_stream_mp4(pXmit->pCapCfg)) < 0) {
        LOG(X_ERROR("Failed to initialize output stream from downloaded mp4 content"));
      }
      break;

    case XMIT_ASYNC_TYPE_M2T_AV:
      memset(&av, 0, sizeof(av));

      if(pXmit->pStreamerCfg->novid) {
        av.vid.inactive = 1;
      }
      if(pXmit->pStreamerCfg->noaud) {
        av.aud.inactive = 1;
      }

      av.vidCodecType = pXmit->vidType;
      av.audCodecType = pXmit->audType;
      //fprintf(stderr, "xmitout input streamtype vid: 0x%x (0x%x) aud: 0x%x (0x%x)\n", av.vidCodecType, pXmit->vidType, av.audCodecType, pXmit->audType);

      if((rc = stream_mp2ts_pes(pXmit->pDataSrcVid, 
                                pXmit->pDataSrcAud,
                                pXmit->pStreamerCfg, 
                                NULL, NULL, &av, MP2TS_LEN)) < 0) {
        LOG(X_ERROR("Failed to initialize output A/V stream"));
      }
      break;

    case XMIT_ASYNC_TYPE_M2T_PES:

      //
      // TODO: the following logic here is duplicated in streamer_rtp::stream_mp2ts_pes_file
      //

      if(pXmit->pStreamerCfg->novid) {
        pes.vid.inactive = 1;
      } else {

        //TODO: the crappy net_pes design of self loading and demuxing pkts doesn't need lock
        // ... this should move to demux m2t on reader side, then insert whole frame into q
        if(!(pes.vid.pQueue = stream_createFrameQ(STREAM_FRAMEQ_TYPE_VID_NETFRAMES,
                                              pXmit->frvidqslots, 0))) {
          rc = -1;
          break;
        }
        pes.vid.pQueue->cfg.uselock = 0;
        pes.vid.pQueue->haveRdr = 1;

        //
        // Increase the audio frames queue to account for video encoder output lag
        //
        if(pXmit->pStreamerCfg->xcode.vid.common.cfgDo_xcode) {
          fraudqslots += 120;
        }

      }

      if(pXmit->pStreamerCfg->noaud) {
        pes.aud.inactive = 1;
      } else {

        if(!(pes.aud.pQueue = stream_createFrameQ(STREAM_FRAMEQ_TYPE_AUD_NETFRAMES,
                                                  fraudqslots, 0))) {
          rc = -1;
          break;
        }
        pes.aud.pQueue->cfg.uselock = 0;
        pes.aud.pQueue->haveRdr = 1;
      }
      LOG(X_DEBUG("Streaming using full elementary stream extract"));

      //fprintf(stderr, "load_frame vid:%d aud:%d\n", pes.vid.inactive, pes.aud.inactive);

      if((rc = stream_mp2ts_pes(pXmit->pDataSrcVid, 
                                pXmit->pDataSrcAud,
                                pXmit->pStreamerCfg, 
                                NULL, &pes, NULL, MP2TS_LEN)) < 0) {
        LOG(X_ERROR("Failed to initialize output PES stream"));
      }

      break;

    case XMIT_ASYNC_TYPE_M2T_RAW:

      LOG(X_DEBUG("Streaming using copy and forward"));

      if((rc = stream_mp2ts_raw(pXmit->pDataSrcVid, pXmit->pStreamerCfg, 
                                0.0f, MP2TS_LEN)) < 0) {
        LOG(X_ERROR("Failed to initialize output stream"));
      }

      break;

    case XMIT_ASYNC_TYPE_NONE:
      break;

    default:
      LOG(X_ERROR("Output type not supported for live streaming."));
     rc = -1;
  }

  if(pes.vid.pQueue) {
    pktqueue_destroy(pes.vid.pQueue);
    pes.vid.pQueue = NULL;
  }
  if(pes.aud.pQueue) {
    pktqueue_destroy(pes.aud.pQueue);
    pes.aud.pQueue = NULL;
  }

  return rc;
}

static void xmit_wrproc(void *parg) {
  XMIT_ASYNC_DESCR_T *pXmitDescr = (XMIT_ASYNC_DESCR_T *) parg;
  int rc = 0;

  logutil_tid_add(pthread_self(), pXmitDescr->tid_tag);

  pXmitDescr->running = 0;
  
  xmitout(pXmitDescr);

  LOG(X_DEBUG("Exiting transmitter thread with code: %d"), rc);

  pXmitDescr->running = -1;

  logutil_tid_remove(pthread_self());
}


static int create_tmpFrameBuf(CAPTURE_PKT_ACTION_DESCR_T *pCapAction) {

  //
  // tmpFrameBuf is used by capture_pkt_xxx demuxers to decompose input 
  // packet payload data into a complete media frame
  //

  pCapAction->tmpFrameBuf.sz = 
      MAX(pCapAction->pQueue->cfg.maxPktLen, pCapAction->pQueue->cfg.growMaxPktLen);

  //LOG(X_DEBUG("Creating capture frame size %d"), pCapAction->tmpFrameBuf.sz);

  if(!(pCapAction->tmpFrameBuf.buf = avc_calloc(1, pCapAction->tmpFrameBuf.sz))) {
     return -1;
  }
  return 0;
}

/*
int stream_capture_onEnd(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  int rc = 0;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;


  return rc;
}
*/

#if 0
int capture_create_queue(MEDIA_FILE_TYPE_T mediaType) {
  PKTQUEUE_T *pQueue = NULL;

    switch(mediaType) {
      case CAPTURE_FILTER_PROTOCOL_MP2TS:
      case CAPTURE_FILTER_PROTOCOL_MP2TS_BDAV:

*create_capQueue(unsigned int pktQueueLen, int id) {
        if(!(pLocalCfg->capActions[0].pQueue =
            create_capQueue(pLocalCfg->capActions[0].pktqslots, 0))) {
          return -1;
        }

  return pQueue;
}
#endif

static void init_cap_async(CAP_ASYNC_DESCR_T *pCapAsync, const CAPTURE_DESCR_COMMON_T *pCommon, 
                           const SOCKET_LIST_T *pSockList, const STREAMER_CFG_T *pStreamerCfg) {

  memset(pCapAsync, 0, sizeof(CAP_ASYNC_DESCR_T));
  pCapAsync->pcommon = (CAPTURE_DESCR_COMMON_T *) pCommon;
  pCapAsync->pSockList = (SOCKET_LIST_T *) pSockList;
  pCapAsync->pStreamerCfg = (STREAMER_CFG_T *) pStreamerCfg;
  //pCapAsync->pUserData = NULL;
  //pCapAsync->record
  pthread_mutex_init(&pCapAsync->mtx, NULL);
  capture_setstate(pCapAsync, STREAMER_STATE_STARTING_THREAD, 0);

  //if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
  //  snprintf(capAsyncArg.tid_tag, sizeof(capAsyncArg.tid_tag), "%s-capture", s);
  //}

}

//static int stream_capture(SOCKET_LIST_T *pSockList, CAPTURE_LOCAL_DESCR_T *pLocalCfg, STREAMER_CFG_T *pStreamerCfg) {
static int stream_capture(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_PKT_ACTION_DESCR_T *capActions) {
  int rc = 0;
  pthread_t ptdCap;
  pthread_attr_t attrCap;
  STREAM_DATASRC_T dataSrcVid;
  STREAM_DATASRC_T dataSrcAud;
  CBDATA_GETDATA_PKTQ_T cbDataGetPktVid;
  CBDATA_GETDATA_PKTQ_T cbDataGetPktAud;
  CAPTURE_FILTER_T *pFilter;
  XMIT_ASYNC_DESCR_T xmitDescr;
  unsigned int idx;
  unsigned int uisz;
  CAP_HTTP_MP4_STREAM_T capHttpMp4Stream;
  const char *s;
#ifdef XMIT_ASYNC
  pthread_t ptdXmit;
  pthread_attr_t attrXmit;
  struct sched_param paramCap;
  struct sched_param paramXmit;
#endif // XMIT_ASYNC

  if(!pCfg || !pCfg->pcommon || !pCfg->pStreamerCfg || pCfg->pcommon->filt.numFilters == 0) {
    return -1;
  }

  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(pCfg->tid_tag, sizeof(pCfg->tid_tag), "%s-capture", s);
  }

  //
  // init data source settings 
  //
  memset(&dataSrcVid, 0, sizeof(dataSrcVid));
  cbDataGetPktVid.pXmitAction = &pCfg->pStreamerCfg->action;
  cbDataGetPktVid.pXmitCb = &pCfg->pStreamerCfg->cbs[0];

  memset(&dataSrcAud, 0, sizeof(dataSrcAud));
  cbDataGetPktAud.pXmitAction = &pCfg->pStreamerCfg->action;
  cbDataGetPktAud.pXmitCb = &pCfg->pStreamerCfg->cbs[0];

  memset(&xmitDescr, 0, sizeof(xmitDescr));
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(xmitDescr.tid_tag, sizeof(xmitDescr.tid_tag), "%s-stream", s);
  }

  for(idx = 0; idx < pCfg->pcommon->filt.numFilters; idx++ ) {

    pFilter = &pCfg->pcommon->filt.filters[idx];

    switch(pFilter->mediaType) {
      case CAPTURE_FILTER_PROTOCOL_MP2TS:
      case CAPTURE_FILTER_PROTOCOL_MP2TS_BDAV:

        if(idx > 0) {
          LOG(X_ERROR("Multiple m2t input filt.filters not permitted"));
          return -1;
        }

        //
        // TODO: mpeg2-ts demuxing should happen before queue insertion on the rcvr side
        // just like stream_net_av Create a single queue to store all packets
        //
        if(!(capActions[0].pQueue =
            create_capQueue(capActions[0].pktqslots, 0))) {
          return -1;
        }

        //fprintf(stderr, "CREATING M2T CAP Q[%d]\n", 0);

        //
        // optimize queue packet stuffing up to each
        // queue slot's size, regardless of input packet payload size
        //
        if(pFilter->mediaType == MEDIA_FILE_TYPE_MP2TS) {
          capActions[0].pQueue->cfg.concataddenum = MP2TS_LEN;
        } else if(pFilter->mediaType == MEDIA_FILE_TYPE_MP2TS_BDAV) {
          capActions[0].pQueue->cfg.concataddenum = MP2TS_BDAV_LEN;
        }

        cbDataGetPktVid.pQ = capActions[0].pQueue;
        dataSrcVid.pCbData = &cbDataGetPktVid;
        dataSrcVid.cbGetData = cbstream_getdata_pktqueue;
        dataSrcVid.cbHaveData = cbstream_havedata_pktqueue;
        dataSrcVid.cbResetData = cbstream_resetdata_pktqueue;
        memcpy(&dataSrcAud, &dataSrcVid, sizeof(dataSrcAud));

        if((capActions[idx].cmd & CAPTURE_PKT_ACTION_EXTRACTPES)) {
          xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_PES; 
          xmitDescr.frvidqslots = capActions[0].frvidqslots;
          xmitDescr.fraudqslots = capActions[0].fraudqslots;
        }  else {
          xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_RAW; 
        }

        idx = pCfg->pcommon->filt.numFilters;
        break;

      case CAPTURE_FILTER_PROTOCOL_AAC:
      case CAPTURE_FILTER_PROTOCOL_AMR:
      case CAPTURE_FILTER_PROTOCOL_SILK:
      case CAPTURE_FILTER_PROTOCOL_OPUS:
      case CAPTURE_FILTER_PROTOCOL_G711_MULAW:
      case CAPTURE_FILTER_PROTOCOL_G711_ALAW:
      case CAPTURE_FILTER_PROTOCOL_RAWA_PCM16LE:
      case CAPTURE_FILTER_PROTOCOL_RAWA_PCMULAW:
      case CAPTURE_FILTER_PROTOCOL_RAWA_PCMALAW:
      case CAPTURE_FILTER_PROTOCOL_AUD_GENERIC:
      case CAPTURE_FILTER_PROTOCOL_AUD_CONFERENCE:

        if(IS_XC_CODEC_TYPE_AUD_RAW(pFilter->mediaType)) {

          if(pFilter->u_seqhdrs.aud.common.clockHz <= 0) {
            LOG(X_ERROR("Raw input filter audio clock rate \"clock=[int]\" (in Hz) not set"));
            return -1;
          } else if(pFilter->u_seqhdrs.aud.channels <= 0) {
            LOG(X_ERROR("Raw input filter audio channel count \"channels=[int]\" not set "));
            return -1;
          }

          pCfg->pStreamerCfg->xcode.aud.cfgSampleRateIn = pFilter->u_seqhdrs.aud.common.clockHz;
          pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn = pFilter->u_seqhdrs.aud.channels;

          //if(capActions[idx].fraudqslots < 2) {
          //  capActions[idx].fraudqslots = AUDFRAMEQRAW_LEN_DEFAULT;
          //}
          // 500ms of PCM
          uisz = codecType_getBytesPerSample(pFilter->mediaType) *
            pFilter->u_seqhdrs.aud.common.clockHz * pFilter->u_seqhdrs.aud.channels / 2;
  
        } else { 

          //
          // capture_pkt_xxx depacketizers require codec specific rtp transport
          //
          if(pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPRTP &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPDTLS &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTMP &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTMPS &&
             //pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTSP &&
             //pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTSPS &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPFLV &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPSFLV &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPMP4 &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPSMP4 &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPDASH &&
             pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPSDASH) {
            LOG(X_ERROR("Input transport protocol (%d) for %s capture must be RTP"),
                 pFilter->transType, codecType_getCodecDescrStr(pFilter->mediaType));
            return -1;
          }

          //if(capActions[idx].fraudqslots < 2) {
          //  capActions[idx].fraudqslots = AUDFRAMEQ_LEN_DEFAULT;
          //}
          if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_AMR) {
            uisz = 64;
          } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_AAC) {
            uisz = 0x2000;
          } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_SILK ||
                    pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_OPUS) {
            uisz = 0x1000;
          } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_G711_ALAW ||
                    pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_G711_MULAW) {
            uisz = 160;
          } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_AUD_GENERIC) {
            uisz = 0x2000;
          } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_AUD_CONFERENCE) {
            uisz = 32;
          } else {
            return -1;
          }

        }

        if(!(capActions[idx].pQueue = stream_createFrameQ(STREAM_FRAMEQ_TYPE_AUD_NETFRAMES, 
                                                          capActions[idx].fraudqslots, uisz))) {
          return -1;
        }

        //fprintf(stderr, "CREATED AUD CAP Q[%d]:0x%x\n", idx, capActions[idx].pQueue);

        //TODO: this can go away, and readpktdirect can go back to calling (set_rdr_pos) once m2t demux is moved prior to fr queue insertion
        //capActions[idx].pQueue->haveRdr = 1;
        pktqueue_setrdr(capActions[idx].pQueue, 0);

        if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPMP4 ||
           pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPSMP4 ||
           pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPDASH ||
           pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPSDASH) {
          xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_MP4NET; 
        } else {
          xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_AV; 
        }

        xmitDescr.audType = pFilter->mediaType; 
        dataSrcAud.pCbData = capActions[idx].pQueue;

        capActions[idx].cmd &= ~CAPTURE_PKT_ACTION_QUEUE;
        capActions[idx].cmd |= CAPTURE_PKT_ACTION_QUEUEFRAME;

        // Create temporary frame storage buffer
        if(create_tmpFrameBuf(&capActions[idx]) < 0) {
          return -1;
        }

        break;

      case CAPTURE_FILTER_PROTOCOL_H264:
      case CAPTURE_FILTER_PROTOCOL_MPEG4V:
      case CAPTURE_FILTER_PROTOCOL_H263:
      case CAPTURE_FILTER_PROTOCOL_H263_PLUS:
      case CAPTURE_FILTER_PROTOCOL_VP8:
      case CAPTURE_FILTER_PROTOCOL_VID_GENERIC:
      case CAPTURE_FILTER_PROTOCOL_VID_CONFERENCE:
         
        //
        // capture_pkt_xxx depacketizers require codec specific rtp transport
        //
        if(pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPRTP &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPDTLS &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTMP &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTMPS &&
           //pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTSP &&
           //pFilter->transType != CAPTURE_FILTER_TRANSPORT_RTSPS &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPFLV &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPSFLV &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPMP4 &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPSMP4 &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPDASH &&
           pFilter->transType != CAPTURE_FILTER_TRANSPORT_HTTPSDASH) {
          LOG(X_ERROR("Input transport protocol (%d) for %s capture must be RTP"),
                pFilter->transType, codecType_getCodecDescrStr(pFilter->mediaType));
          return -1;   
        }

        //if(capActions[idx].frvidqslots < 2) {
        //  capActions[idx].frvidqslots = VIDFRAMEQ_LEN_DEFAULT;
        //}


        if(!(capActions[idx].pQueue = stream_createFrameQ(STREAM_FRAMEQ_TYPE_VID_NETFRAMES, 
                                                          capActions[idx].frvidqslots,0))) {
          return -1;
        }
        //fprintf(stderr, "CREATED VID CAP Q[%d]:0x%x\n", idx, capActions[idx].pQueue);
        //TODO: this can go away, and readpktdirect can go back to calling (set_rdr_pos) once m2t demux is moved prior to fr queue insertion
        //capActions[idx].pQueue->haveRdr = 1;
        pktqueue_setrdr(capActions[idx].pQueue, 0);
 
        // Create temporary frame storage buffer
        if(create_tmpFrameBuf(&capActions[idx]) < 0) {
          return -1;
        }
        //fprintf(stderr, "alloced %d at 0x%x\n", capActions[idx].tmpFrameBuf.sz,capActions[idx].tmpFrameBuf.buf);

        if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPMP4 ||
           pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPSMP4 ||
           pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPDASH ||
           pFilter->transType == CAPTURE_FILTER_TRANSPORT_HTTPSDASH) {
          xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_MP4NET; 
        } else {
          xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_AV; 
        }
        xmitDescr.vidType = pFilter->mediaType; 
        // stream_net_av should read directly from the frame queue 
        dataSrcVid.pCbData = capActions[idx].pQueue;

        capActions[idx].cmd &= ~CAPTURE_PKT_ACTION_QUEUE;
        capActions[idx].cmd |= CAPTURE_PKT_ACTION_QUEUEFRAME;

        break;

      case CAPTURE_FILTER_PROTOCOL_RAWV_BGRA32:
      case CAPTURE_FILTER_PROTOCOL_RAWV_RGBA32:
      case CAPTURE_FILTER_PROTOCOL_RAWV_BGR24:
      case CAPTURE_FILTER_PROTOCOL_RAWV_RGB24:
      case CAPTURE_FILTER_PROTOCOL_RAWV_BGR565:
      case CAPTURE_FILTER_PROTOCOL_RAWV_RGB565:
      case CAPTURE_FILTER_PROTOCOL_RAWV_YUV420P:
      case CAPTURE_FILTER_PROTOCOL_RAWV_NV12:
      case CAPTURE_FILTER_PROTOCOL_RAWV_NV21:

        uisz = pFilter->width * pFilter->height *
               codecType_getBitsPerPx(pFilter->mediaType) / 8;

        if(uisz <= 0) {
          LOG(X_ERROR("Raw input filter dimensions \"size=[width]x[height]\" not set"));
          return -1;
        } else if(strncasecmp(pCfg->pcommon->localAddrs[idx], CAP_DEV_DUMMY,
                  strlen(CAP_DEV_DUMMY)) && pFilter->fps <= 0) {
          LOG(X_ERROR("Raw input filter video frame rate \"fps=[float]\" not set "));
          return -1;
        }

        vid_convert_fps(pFilter->fps, &pCfg->pStreamerCfg->xcode.vid.inClockHz, 
                        &pCfg->pStreamerCfg->xcode.vid.inFrameDeltaHz);
        pCfg->pStreamerCfg->xcode.vid.inH = pFilter->width; 
        pCfg->pStreamerCfg->xcode.vid.inV = pFilter->height; 

        //
        // Create a frame buffer, 
        //
        // TODO: this may be referenced directly by libavcodec raw scaler / encoder - 
        // it may require // special %16 alignment for SSE
        if(!(capActions[idx].pQueue = stream_createFrameQ( STREAM_FRAMEQ_TYPE_VID_FRAMEBUF , 2, uisz))) {
          return -1;
        }
        //capActions[idx].pQueue->haveRdr = 1;

        xmitDescr.xmitType = XMIT_ASYNC_TYPE_M2T_AV; 
        xmitDescr.vidType = pFilter->mediaType; 
        // stream_net_av should read directly from the frame queue 
        dataSrcVid.pCbData = capActions[idx].pQueue;

        capActions[idx].cmd &= ~CAPTURE_PKT_ACTION_QUEUE;
        capActions[idx].cmd |= CAPTURE_PKT_ACTION_QUEUEFRAME;

        break;

      default:
        LOG(X_ERROR("Capture filter[%d]/%d protocol %s (%d) not supported for live streaming."),
          idx, pCfg->pcommon->filt.numFilters, 
          codecType_getCodecDescrStr(pFilter->mediaType), pFilter->mediaType);
        return -1;
    }
  }

  //
  // init transmitter settings xmitDescr
  //
  if(dataSrcVid.pCbData) {
    xmitDescr.pDataSrcVid = &dataSrcVid;
  } else if(!pCfg->pStreamerCfg->xcode.vid.pip.active && pCfg->pStreamerCfg->xcode.vid.common.cfgDo_xcode) {
    LOG(X_DEBUG("Turning off video transcoding since no video input stream configured"));
    pCfg->pStreamerCfg->xcode.vid.common.cfgDo_xcode = 0;
  }
  if(dataSrcAud.pCbData) {
    xmitDescr.pDataSrcAud = &dataSrcAud;
  } else if(!pCfg->pStreamerCfg->xcode.vid.pip.active && pCfg->pStreamerCfg->xcode.aud.common.cfgDo_xcode) {
    LOG(X_DEBUG("Turning off audio transcoding since no audio input stream configured"));
    pCfg->pStreamerCfg->xcode.aud.common.cfgDo_xcode = 0;
  }
  xmitDescr.pStreamerCfg = pCfg->pStreamerCfg;
  xmitDescr.pCapCfg = pCfg;
  if(xmitDescr.xmitType == XMIT_ASYNC_TYPE_M2T_MP4NET) {
    memset(&capHttpMp4Stream, 0, sizeof(capHttpMp4Stream));
    //capHttpMp4Stream.mp4NetStream.rcvTmtMs = 10000;
    pthread_mutex_init(&capHttpMp4Stream.pl.mtx, NULL);
    //pthread_mutex_init(&capHttpMp4Stream.plUsed.mtx, NULL);
    pCfg->pUserData = &capHttpMp4Stream;
  }

  //
  // If we are set to share the capture socket for input and output then set the indicator flag
  // which will be re-set when the capture socket initialization has completed
  // This may be needed for streamer DTLS output as it needs to wait upon RTP socket DTLS handshake completion
  //
  if(pCfg->pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort) {
    pCfg->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_PENDING_CREATE;
    //LOG(X_DEBUG("Set state.STREAMER_SHARED_STATE_PENDING_CREATE"));
  }

  if(((capActions[0].cmd & CAPTURE_PKT_ACTION_QUEUE) ||
      (capActions[0].cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) && capActions[0].pQueue) {

    //
    // SleepQ is waited on in streamer2 loop
    //
    pCfg->pStreamerCfg->pSleepQs[0] = capActions[0].pQueue;
    if((capActions[1].cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {

      pCfg->pStreamerCfg->pSleepQs[1] = capActions[1].pQueue;

      //
      // Enable pktqueue_wakeup to notify both vid & aud queues 
      // If stream_net_xxx is waiting on vid, and aud frame arrives,
      // ADVFR_RC_NOTAVAIL should be returned for vid and stream_av 
      // then has the choice to call advanceFr on the complementing 
      // ES depending on its frame output algorithm 
      //
      if(capActions[0].pQueue && capActions[1].pQueue) {
        capActions[0].pQueue->pnotifyComplement = &capActions[1].pQueue->notify;
        capActions[1].pQueue->pnotifyComplement = &capActions[0].pQueue->notify;
      }

    } 

    LOG(X_DEBUG("Setting sleepQ to %d %d"), pCfg->pStreamerCfg->pSleepQs[0]->cfg.id,
        pCfg->pStreamerCfg->pSleepQs[1] ? pCfg->pStreamerCfg->pSleepQs[1]->cfg.id : 0);
  }

  if(capActions[0].pQueue && capActions[1].pQueue) {
    capActions[0].pQueue->pQComplement = capActions[1].pQueue;
    capActions[1].pQueue->pQComplement = capActions[0].pQueue;
  } 

  pthread_attr_init(&attrCap);
  pthread_attr_setdetachstate(&attrCap, PTHREAD_CREATE_DETACHED);
#ifdef XMIT_ASYNC
  if(pCfg->pcommon->caphighprio) {
    LOG(X_DEBUG("Setting capture thread to high priority"));
    pthread_attr_setschedpolicy(&attrCap, SCHED_RR);
    paramCap.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_attr_setschedparam(&attrCap, &paramCap);
  }
#endif //XMIT_ASYNC 

  if(pthread_create(&ptdCap,
                    &attrCap,
                    (void *) capture_pktqueue_rdproc,
                    (void *) pCfg) != 0) {
    LOG(X_ERROR("Unable to create capture thread"));
    pthread_mutex_destroy(&pCfg->mtx);
    capture_setstate(pCfg, STREAMER_STATE_ERROR, 0);
    return -1;
  }

  while(pCfg->running == STREAMER_STATE_STARTING_THREAD) {
    usleep(5000);
  }

  // why sleep here still?
  //usleep(100000);

  if(pCfg->running < 0) {
    pthread_mutex_destroy(&pCfg->mtx);
    return -1;
  }

#ifdef XMIT_ASYNC

  pthread_attr_init(&attrXmit);
  pthread_attr_setdetachstate(&attrXmit, PTHREAD_CREATE_DETACHED);

  if(pCfg->pcommon->xmithighprio) {
    LOG(X_DEBUG("Setting transmitter thread to high priority"));
    pthread_attr_setschedpolicy(&attrXmit, SCHED_RR);
    paramXmit.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_attr_setschedparam(&attrXmit, &paramXmit);
  }

  xmitDescr.running = 1;

  if(pthread_create(&ptdXmit,
                    &attrXmit,
                    (void *) xmit_wrproc,
                    (void *) &xmitDescr) != 0) {
    LOG(X_ERROR("Unable to create transmitter thread"));
    pthread_mutex_destroy(&pCfg->mtx);
    return -1;
  }

  while(xmitDescr.running == 1) {
    usleep(20000);
  }

  while(xmitDescr.running == 0) {
    usleep(300000);
  }

  if(pCfg->pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort) {
    pCfg->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_UNKNOWN;
  }

#else

  xmitout(&xmitDescr);

#endif // XMIT_ASYNC

  pthread_mutex_lock(&pCfg->mtx);
  if(pCfg->running >= STREAMER_STATE_RUNNING) {
    if(pCfg->running == STREAMER_STATE_RUNNING) {
      //fprintf(stderr, "terminating cap thread\n");
      pCfg->running = STREAMER_STATE_INTERRUPT;
      pthread_mutex_unlock(&pCfg->mtx);
      while(pCfg->running >= STREAMER_STATE_RUNNING) {
        usleep(50000);
      }
      //fprintf(stderr, "terminated cap thread\n");
    } else {
      pthread_mutex_unlock(&pCfg->mtx);
    }
  } else {
    pthread_mutex_unlock(&pCfg->mtx);
  }

  pthread_mutex_destroy(&pCfg->mtx);

  //
  // Perform cleanup on the shared resources (typically this was just aud & vid pktqueue
  // cleaned up later via destroyLocalCfg)
  // between stream & capture
  //
  if(xmitDescr.xmitType == XMIT_ASYNC_TYPE_M2T_MP4NET) {
    mpdpl_free(&capHttpMp4Stream.pl);
  }

  LOG(X_DEBUG("Stream capture exiting"));

  return rc;
}

#endif // VSX_HAVE_STREAMER

static int record_async(SOCKET_LIST_T *pSockList,
                        CAPTURE_LOCAL_DESCR_T *pLocalCfg,
                        const CAPTURE_RECORD_DESCR_T *pRecordCfg) {
  pthread_t ptd;
  pthread_attr_t attr;
  CAP_ASYNC_DESCR_T capAsyncArg;
  unsigned char buf[RTP_JTBUF_PKT_BUFSZ_LOCAL];
  CAPTURE_STREAM_T stream;
  CAPTURE_CBDATA_T streamCbData;
  CAPTURE_PKT_ACTION_DESCR_T *pCapAction = NULL;
  COLLECT_STREAM_PKTDATA_T pkt;
  PKTQ_EXTRADATA_T pktxtra;
  CBDATA_GETDATA_PKTQ_T cbDataGetPkt;
  int len;
  unsigned int numOverwritten;

  if(pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_UNKNOWN) {
    LOG(X_ERROR("No capture transport type specified in input filter."));
    return -1;
  } 

  //
  // TODO: mpeg2-ts demuxing should happen before queue insertion on the rcvr side
  // Create a single queue to store all packets
  //
  if(!(pLocalCfg->capActions[0].pQueue =
          create_capQueue(pLocalCfg->capActions[0].pktqslots, 0))) {
    return -1;
  }
  //
  // If input type is m2t optimize queue packet stuffing up to each
  // queue slot's size, regardless of input packet payload size
  //
  if(pLocalCfg->common.filt.filters[0].mediaType == MEDIA_FILE_TYPE_MP2TS) {
    pLocalCfg->capActions[0].pQueue->cfg.concataddenum = MP2TS_LEN;
  } else if(pLocalCfg->common.filt.filters[0].mediaType == MEDIA_FILE_TYPE_MP2TS_BDAV) {
    pLocalCfg->capActions[0].pQueue->cfg.concataddenum = MP2TS_BDAV_LEN;
  }


  init_cap_async(&capAsyncArg, &pLocalCfg->common, pSockList, NULL);

  memset(&pkt, 0, sizeof(pkt));
  memset(&stream, 0, sizeof(stream));
  stream.pFilter = pLocalCfg->common.filt.filters;

  capture_initCbData(&streamCbData, pRecordCfg->outDir, pRecordCfg->overwriteOut);

  //
  // Call cbOnStreamAdd to initialize the stream specific packet callback function
  //
  pCapAction = stream.pFilter->pCapAction;

#if 0
  pCapAction->cmd = CAPTURE_PKT_ACTION_STUNONLY;
#else
  ((CAPTURE_FILTER_T *) stream.pFilter)->pCapAction = NULL;
#endif
  

  if(capture_cbOnStreamAdd(&streamCbData, &stream, NULL, NULL) < 0 ||
     stream.cbOnPkt == NULL) {
    LOG(X_ERROR("Unable to initialize recording"));
    pthread_mutex_destroy(&capAsyncArg.mtx);
    capture_deleteCbData(&streamCbData);
    return -1;
  }
  ((CAPTURE_FILTER_T *) stream.pFilter)->pCapAction = pCapAction;

  // initialize capture thread
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  //pthread_attr_setschedpolicy(&attr, SCHED_RR);
  //param.sched_priority = sched_get_priority_max(SCHED_RR);
  //pthread_attr_setschedparam(&attr, &param);

  if(pthread_create(&ptd,
                    &attr,
                    (void *) capture_pktqueue_rdproc,
                    (void *) &capAsyncArg) != 0) {
    LOG(X_ERROR("Unable to create capture thread"));
    pthread_mutex_destroy(&capAsyncArg.mtx);
    capture_setstate(&capAsyncArg, STREAMER_STATE_ERROR, 0);
    capture_deleteCbData(&streamCbData);
    return -1;
  }


  while(capAsyncArg.running == STREAMER_STATE_STARTING_THREAD) {
    usleep(5000);
  }

  usleep(100000);
  if(capAsyncArg.running < STREAMER_STATE_RUNNING) {
    pthread_mutex_destroy(&capAsyncArg.mtx);
    capture_deleteCbData(&streamCbData);
    return -1;
  }

  cbDataGetPkt.pQ = pLocalCfg->capActions[0].pQueue;
  cbDataGetPkt.pXmitAction = NULL;
  cbDataGetPkt.pXmitCb = NULL;
  memset(&pktxtra, 0, sizeof(pktxtra));

  while(!g_proc_exit) {

    if((len = cbstream_getpkt_pktqueue(&cbDataGetPkt, buf, sizeof(buf), 
                                       &pktxtra, &numOverwritten)) < 0) {
      break;
    } else if(len > 0) {

      pkt.payload.pData = buf;
      PKTCAPLEN(pkt.payload) = len;
      stream.numPkts++;

      //fprintf(stderr, "got pkt from queue len:%d\n", len);

      if(stream.cbOnPkt(&streamCbData.sp[0], (const COLLECT_STREAM_PKTDATA_T *) &pkt) < 0) {
        break;
      }

    }

  }

  capture_deleteCbData(&streamCbData);
  //fileops_Close(streamCbData.sp[0].stream1.fp);

  while(capAsyncArg.running >= STREAMER_STATE_RUNNING) {
    usleep(20000);
  }

  pthread_mutex_destroy(&capAsyncArg.mtx);

  LOG(X_DEBUG("Exiting record async"));

  return 0;
}

static void initUri(CAPTURE_DESCR_COMMON_T *pCommon) {
  int rc; 
  if((rc = mediadb_getdirlen(pCommon->localAddrs[0])) > 1) {
    pCommon->addrsExtHost[0] = (char *) &((pCommon->localAddrs[0])[rc]);
    if(*(pCommon->addrsExt[0]) == '/') {
       pCommon->addrsExt[0]++;
     }
    ((char *) (pCommon->localAddrs[0]))[rc  - 1] = '\0';
  }
}

int capture_net_async(CAPTURE_LOCAL_DESCR_T *pLocalCfg,
                      STREAMER_CFG_T *pStreamerCfg,
                      const CAPTURE_RECORD_DESCR_T *pRecordCfg) {

  int rc = 0;
  SOCKET_LIST_T sockList;
  unsigned int idx;
  SRV_LISTENER_CFG_T tmpListenCfg;
  CAP_ASYNC_DESCR_T capAsyncCfg;
  char hostbuf[CAPTURE_HTTP_HOSTBUF_MAXLEN];

  if(!pLocalCfg || !pLocalCfg->common.localAddrs[0]) {
    LOG(X_ERROR("No input interface or file path specified"));
    return -1;
  } 

  if(pLocalCfg->common.filt.numFilters <= 0) {
    LOG(X_ERROR("Stream input capture filter not configured."));
    return -1;
  }

  memset(&sockList, 0, sizeof(sockList));
  hostbuf[0] = '\0';
  pLocalCfg->common.addrsExtHost[0] = hostbuf;
  pLocalCfg->common.addrsExt[0] = '\0';

  if(IS_CAPTURE_FILTER_TRANSPORT_HTTP(pLocalCfg->common.filt.filters[0].transType)) {

    if(capture_getdestFromStr(pLocalCfg->common.localAddrs[0], 
                              &sockList.salist[0], 
                              &pLocalCfg->common.addrsExt[0],
                              pLocalCfg->common.addrsExtHost[0], HTTP_PORT_DEFAULT) == 0) {
      sockList.numSockets = 1;
    } else {
      return -1;
    }

  } else if(pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTMP ||
            pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTMPS) {

    if(capture_getdestFromStr(pLocalCfg->common.localAddrs[0],
                              &sockList.salist[0],
                              &pLocalCfg->common.addrsExt[0],
                              NULL, RTMP_PORT_DEFAULT) == 0) {

      sockList.numSockets = 1;
      initUri(&pLocalCfg->common);

    } else {
      return -1;
    }

  } else if(pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTSP ||
            pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTSPS) {

    if(capture_getdestFromStr(pLocalCfg->common.localAddrs[0],
                              &sockList.salist[0],
                              &pLocalCfg->common.addrsExt[0],
                              NULL, RTSP_PORT_DEFAULT) == 0) {

      //sockList.numSockets = 1;
      initUri(&pLocalCfg->common);

    } else {
      return -1;
    }

  } 

  init_cap_async(&capAsyncCfg, &pLocalCfg->common, &sockList, pStreamerCfg);

  //
  // For rtsp:// input , call capture_rtsp_setup, to connect to the rtsp server
  // and setup the listening port numbers
  // 
  if((pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTSP ||
      pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTSPS)) {

    if(pLocalCfg->common.addrsExt[0] == NULL || pLocalCfg->common.addrsExt[0] == '\0') {
      //LOG(X_DEBUG("RTSP capture server mode"));
      memcpy(&pLocalCfg->common.rtsp.sd.sa, &sockList.salist[0], sizeof(pLocalCfg->common.rtsp.sd.sa));

      //
      // The global SSL server context is usually initialized when calling vsxlib_setupServer but that
      // may not have been the case if we're just running an SSL RTSP capture 
      //
      if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pLocalCfg->common.filt.filters[0].transType) &&
        !netio_ssl_enabled(1)) {
        memset(&tmpListenCfg, 0, sizeof(tmpListenCfg));
        tmpListenCfg.netflags |= NETIO_FLAG_SSL_TLS;
        vsxlib_ssl_initserver(pStreamerCfg->pStorageBuf->pParams, &tmpListenCfg);
      }

      if((rc = capture_rtsp_server_start(&capAsyncCfg, 1)) < 0) {
        pthread_mutex_destroy(&capAsyncCfg.mtx);
        return rc;
      }

    } else {

      memset(&pLocalCfg->common.rtsp, 0, sizeof(pLocalCfg->common.rtsp)); 
      pLocalCfg->common.rtsp.rtsptranstype = pLocalCfg->common.rtsptranstype;

      //
      // Initialize RTSP Basic/Digest auth client context
      //
      pLocalCfg->common.rtsp.authCliCtxt.pcreds = &pLocalCfg->common.creds[0];
      snprintf((char *) pLocalCfg->common.rtsp.authCliCtxt.uribuf, 
              sizeof(pLocalCfg->common.rtsp.authCliCtxt.uribuf), 
              URL_RTSP_FMT2_STR"%s%s", 
              URL_RTSP_FMT2_ARGS(IS_CAPTURE_FILTER_TRANSPORT_SSL(pLocalCfg->common.filt.filters[0].transType), 
                           pLocalCfg->common.localAddrs[0]), 
                           pLocalCfg->common.addrsExtHost[0] != '\0' ? "/" : "", pLocalCfg->common.addrsExtHost[0]);

      pLocalCfg->common.rtsp.authCliCtxt.puri = pLocalCfg->common.rtsp.authCliCtxt.uribuf; 
      LOG(X_DEBUG("Trying to SETUP RTSP to %s"), pLocalCfg->common.rtsp.authCliCtxt.puri);

     if((rc = capture_rtsp_setup(&pLocalCfg->common, pStreamerCfg)) < 0) {
        pthread_mutex_destroy(&capAsyncCfg.mtx);
        return -1;
      } 
    }

    if(pLocalCfg->common.filt.numFilters > 0) {

      //TODO: is this right????, what if it overwrites output ip:ports
      for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
        memcpy(&pStreamerCfg->sdpsx[1][idx], &pLocalCfg->common.rtsp.sdp, 
              sizeof(pStreamerCfg->sdpsx[1][0]));
      }
    }

  }

  if(sockList.numSockets == 0) {
    if(pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_DEV) {

    // Since net_async only uses local socket capture (not pcap), do not proceed
    } else if((rc = capture_parseLocalAddr(pLocalCfg->common.localAddrs[0], &sockList)) <= 0) {
      LOG(X_ERROR("Invalid local address or port '%s'"), pLocalCfg->common.localAddrs[0]); 
      pthread_mutex_destroy(&capAsyncCfg.mtx);
      return -1; 
    }
  }

  for(idx = 0; idx < pLocalCfg->common.filt.numFilters; idx++) {

    if(!(pLocalCfg->capActions[idx].cmd & CAPTURE_PKT_ACTION_QUEUE) ||
         pLocalCfg->capActions[idx].pktqslots <= 0) {
      LOG(X_ERROR("Capture queue size cannot be %d"), pLocalCfg->capActions[idx].pktqslots);
      pthread_mutex_destroy(&capAsyncCfg.mtx);
      return -1;
    }
  }

  if(pStreamerCfg) {

#if defined(VSX_HAVE_STREAMER)

    if(pLocalCfg->common.filt.filters[0].mediaType == CAPTURE_FILTER_PROTOCOL_UNKNOWN) {
      LOG(X_ERROR("Please specify a capture filter protocol type")); 
      destroyLocalCfg(pLocalCfg);
      pthread_mutex_destroy(&capAsyncCfg.mtx);
      return -1;
    }

    rc = stream_capture(&capAsyncCfg, pLocalCfg->capActions);

#else
    rc = -1;
#endif // VSX_HAVE_STREAMER

  } else if(pRecordCfg) {

    if(pLocalCfg->common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_DEV) {
      LOG(X_ERROR("Device capture recording not implemented"));
    } else {
      rc = record_async(&sockList, pLocalCfg, pRecordCfg);
    }
  }
 
  if(pLocalCfg->common.rtsp.issetup) {
    capture_rtsp_close(&pLocalCfg->common.rtsp);
  }

  destroyLocalCfg(pLocalCfg);
  pthread_mutex_destroy(&capAsyncCfg.mtx);

  return rc; 
}


int capture_net_sync(CAPTURE_LOCAL_DESCR_T *pLocalCfg,
                     const CAPTURE_RECORD_DESCR_T *pRecordCfg) {
  int rc = 0;
  SOCKET_LIST_T sockList;
  CAP_ASYNC_DESCR_T capAsyncCfg;

#ifndef DISABLE_PCAP
  FILE_STREAM_T file;
  struct stat st;
#endif // DISABLE_PCAP

  if(!pLocalCfg || !pLocalCfg->common.localAddrs[0]) {
    LOG(X_ERROR("No input interface or file path specified"));
    return -1;
  }

  memset(&sockList, 0, sizeof(sockList));

#ifndef DISABLE_PCAP
  //
  // If input is an existing filepath, threat it as a pcap file
  //
  if(fileops_stat(pLocalCfg->common.localAddrs[0], &st) == 0) {

    if((file.fp = fileops_Open(pLocalCfg->common.localAddrs[0], O_RDONLY)) == FILEOPS_INVALID_FP) {
      LOG(X_ERROR("Unable to open file for reading: %s"), pLocalCfg->common.localAddrs[0]);
      return -1; 
    }
    fileops_Close(file.fp);

    if(pLocalCfg->common.filt.numFilters == 0) {
      LOG(X_DEBUG("No filter specified.  Will list all streams in capture file."));
    }

    rc = cap_pcapStart(pLocalCfg->common.localAddrs[0], pRecordCfg->outDir, 1, 
                       pLocalCfg->common.filt.filters, pLocalCfg->common.filt.numFilters, 
                       pRecordCfg->overwriteOut, 0);

  } else 
#endif // DISABLE_PCAP


  //TODO: the sync part may be broken for flv/httplive capture

  //
  // Check if input is an <addr>:<port> combination
  //
  if(IS_CAPTURE_FILTER_TRANSPORT_HTTP(pLocalCfg->common.filt.filters[0].transType) &&
     (rc = capture_parseLocalAddr(pLocalCfg->common.localAddrs[0], &sockList)) > 0) {

    if(pLocalCfg->common.filt.numFilters == 0) {
      LOG(X_DEBUG("No filter specified.  Will list all streams."));
    }

    if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pLocalCfg->common.filt.filters[0].transType)) {
      sockList.netsockets[0].flags |= NETIO_FLAG_SSL_TLS;
    }

    init_cap_async(&capAsyncCfg, &pLocalCfg->common, &sockList, NULL);
    //memset(&capAsyncCfg, 0, sizeof(capAsyncCfg));
    //capAsyncCfg.pcommon = &pLocalCfg->common;
    //capAsyncCfg.pSockList = &sockList;
    capAsyncCfg.record.outDir = pRecordCfg->outDir;
    capAsyncCfg.record.overwriteOut = pRecordCfg->overwriteOut;
    //capture_setstate(&capAsyncCfg, STREAMER_STATE_STARTING, 0);
    //pthread_mutex_init(&capAsyncCfg.mtx, NULL);

   if(IS_CAPTURE_FILTER_TRANSPORT_HTTP(pLocalCfg->common.filt.filters[0].transType)) {

      if(capture_getdestFromStr(pLocalCfg->common.localAddrs[0], 
                                &sockList.salist[0], 
                                &pLocalCfg->common.addrsExt[0],
                                pLocalCfg->common.addrsExtHost[0], HTTP_PORT_DEFAULT) == 0) {
        sockList.numSockets = 1;
        rc = capture_httpget(&capAsyncCfg);
      } else {
        rc = -1;
      }

    } else {
      rc = capture_socketStart(&capAsyncCfg);
    }

    pthread_mutex_destroy(&capAsyncCfg.mtx);
  } 
#ifndef DISABLE_PCAP

  //
  // Assume input is the name of local pcap interface
  //
  else if(rc == 0) {

    if(pLocalCfg->common.filt.numFilters == 0) {
      LOG(X_DEBUG("No filter specified.  Will list all streams."));
    }

    rc = cap_pcapStart(pLocalCfg->common.localAddrs[0], pRecordCfg->outDir, 0, 
                       pLocalCfg->common.filt.filters, pLocalCfg->common.filt.numFilters, 
                       pRecordCfg->overwriteOut, pLocalCfg->common.promiscuous);
  } else {
    return -1;
  }
#else // DISABLE_PCAP
  else {
    LOG(X_ERROR("Invalid local interface or pcap file %s"), 
        pLocalCfg->common.localAddrs[0]);
    return -1;
  }
#endif // DISABLE_PCAP

  return 0;
}

#endif // VSX_HAVE_CAPTURE
