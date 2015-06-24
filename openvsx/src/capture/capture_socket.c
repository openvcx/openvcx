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

//
#include "vsx_common.h"

//#include "stream/streamer_rtp.h"

#define CAP_PKT_PRINT_MS           5000

#if defined(VSX_HAVE_CAPTURE)

//#define IS_TURN_ENABLED(filter) (filter).stun.pTurn && (filter).turn.relay_active
#define DEPRECATED_IS_TURN_ENABLED(filter) (filter).turn.relay_active
#define IS_SOCK_TURN(pnetsock) ((pnetsock)->turn.use_turn_indication_in)

static int capture_closeLocalSockets(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_FILTERS_T *pFilt) {
  size_t idx = 0;
  CAPTURE_FILTER_T *pFilter = NULL;
  SOCKET_LIST_T *pSockList = pCfg->pSockList;

#if defined(VSX_HAVE_TURN)
  //
  // Close any associated TURN context(s) and TURN relays
  //
  if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt.flag != 0) {
    LOG(X_DEBUG("Shutting down all TURN services"));
    turn_thread_stop(&pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt);
  }
#endif // (VSX_HAVE_TURN)

  for(idx = 0; idx < sizeof(pSockList->netsockets) / sizeof(pSockList->netsockets[0]); idx++) {

    pFilter = &pFilt->filters[idx];

    //
    // Close the RTP and RTCP socket(s)
    //
    if(NETIOSOCK_FD(pSockList->netsockets[idx]) != INVALID_SOCKET) {
      netio_closesocket(&pSockList->netsockets[idx]);
    }

    if(NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]) != INVALID_SOCKET) {
      netio_closesocket(&pSockList->netsocketsRtcp[idx]);
    }

  }

#if defined(VSX_HAVE_SSL_DTLS)
  //
  // Close any DTLS context
  //
  for(idx = 0; idx < pFilt->numFilters; idx++) {
    if(pFilt->filters[idx].dtls.active == 1) {
      //LOG(X_DEBUG("Closing RTP DTLS input context 0x%x"), &pFilt->filters[idx].dtls);
      dtls_ctx_close(&pFilt->filters[idx].dtls);
    }
    pFilt->filters[idx].dtls.pDtlsShared = NULL;
  } 
#endif // (VSX_HAVE_SSL_DTLS)

  return 0;
}

typedef struct CAPTURE_DTLS_KEY_UPDATE_CBDATA {
  CAPTURE_FILTER_T                     *pFilter;
  CAPTURE_FILTER_T                     *pFilterPeer;
  CAP_ASYNC_DESCR_T                    *pCfg;
  unsigned int                          streamSharedIdx;
} CAPTURE_DTLS_KEY_UPDATE_CBDATA_T;

#if defined(VSX_HAVE_SSL_DTLS)

static int capture_dtls_netsock_key_update_streamer(CAP_ASYNC_DESCR_T *pCfg, 
                                                    DTLS_KEY_UPDATE_STORAGE_T *pdtlsKeyUpdateStorage, 
                                                    NETIO_SOCK_T *pnetsock) {
  int rc = 0;

  if(!pdtlsKeyUpdateStorage->active || !pdtlsKeyUpdateStorage->pCbData) {
    return 0;
  }

  pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

  //
  // Invoke the streamer DTLS-SRTP key update callback 
  //
  rc = stream_dtls_netsock_key_update(pdtlsKeyUpdateStorage->pCbData, 
                                      pnetsock,
                                      pdtlsKeyUpdateStorage->srtpProfileName, 
                                      pdtlsKeyUpdateStorage->dtls_serverkey, 
                                      pdtlsKeyUpdateStorage->is_rtcp, 
                                      pdtlsKeyUpdateStorage->dtlsKeys, 
                                      pdtlsKeyUpdateStorage->dtlsKeysLen);
  pdtlsKeyUpdateStorage->active = 0;

  pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

  return rc;
}

static int capture_init_dtls_srtp(SRTP_CTXT_T *pSrtp,  const char *srtpProfileName, 
                                 int dtls_server, int is_rtcp, int do_init_rtcp_srtp,
                                 const unsigned char *pKeys, unsigned int lenKeys) {
  int rc = 0;

  if(srtp_streamType(srtpProfileName, &pSrtp->kt.type.authType, &pSrtp->kt.type.confType) < 0) {
    LOG(X_ERROR("Unknown DTLS-SRTP %s profile name '%s'"), is_rtcp ? "RTCP" : "RTP", srtpProfileName);
    rc = -1;
  }

  if(rc >= 0) {
    pSrtp->kt.type.authTypeRtcp = pSrtp->kt.type.authType;
    pSrtp->kt.type.confTypeRtcp = pSrtp->kt.type.confType;

    if(srtp_setDtlsKeys(pSrtp, pKeys, lenKeys, !dtls_server, is_rtcp) < 0) {
      LOG(X_ERROR("Failed to update DTLS-SRTP %s key from DTLS handshake"), is_rtcp ? "RTCP" : "RTP");
      rc = -1;
    }
  }

  //
  // TODO: SRTP init can move from capture_rtp to here now, since we don't require to know the SSRC beforehand 
  // for now we only init a unique RTCP srtp context here in the case of srtp-dtls
  //
  if(rc >= 0 && is_rtcp && do_init_rtcp_srtp && pSrtp->kt.k.lenKey > 0) {
    LOG(X_DEBUG("Initializing unique SRTP RTCP capture context key length:%d (0x%x, 0x%x)"), 
        pSrtp->kt.k.lenKey, pSrtp, pSrtp->pSrtpShared);
    if(srtp_initInputStream(pSrtp, 0, 1) < 0) {
      rc = -1;
    }
  }

  //LOG(X_DEBUG("CAPTURE_INIT_DTLS_SRTP is_rtcp:%d, do_init_rtcp_srtp:%d, key: "), is_rtcp, do_init_rtcp_srtp); LOGHEX_DEBUG(pSrtp->kt.k.key, pSrtp->kt.k.lenKey);

  return rc;
}

int capture_dtls_netsock_key_update(void *pCbData, NETIO_SOCK_T *pnetsock, const char *srtpProfileName,
                                    int dtls_serverkey, int is_rtcp, const unsigned char *pKeys, 
                                    unsigned int lenKeys) {
  int rc = 0;
  int do_init_rtcp_srtp = 0;
  CAPTURE_DTLS_KEY_UPDATE_CBDATA_T *pDtlsKeyUpdateCbData = NULL;
  CAPTURE_FILTER_T *pFilter = NULL;
  CAPTURE_FILTER_T *pFilterPeer = NULL;
  DTLS_KEY_UPDATE_STORAGE_T *pdtlsKeyUpdateStorage = NULL;
  SRTP_CTXT_T *pSrtp = NULL;
  SRTP_CTXT_T *pSrtpPeer = NULL;

  if(!(pDtlsKeyUpdateCbData = (CAPTURE_DTLS_KEY_UPDATE_CBDATA_T *) pCbData) ||
     !(pFilter = pDtlsKeyUpdateCbData->pFilter) || !pKeys || !srtpProfileName ||
     !(pSrtp = &pFilter->srtps[is_rtcp ? 1 : 0])) {
    return -1;
  }

  //
  // Since we're using SRTP-DTLS and we have unique ports for RTP and RTCP (non rtcp-mux case) then
  // we will need a unique SRTP RTCP context for unprotecting incoming RTCP packets
  //
  if(is_rtcp && pFilter->srtps[1].pSrtpShared && pFilter->srtps[1].pSrtpShared == &pFilter->srtps[1] &&
     !pDtlsKeyUpdateCbData->pFilterPeer) {
    do_init_rtcp_srtp = 1;
  }

  //LOG(X_DEBUG("CAPTURE_DTLS_NETSOCK_KEY_UPDATE handshake profile: '%s', dtls_serverkey:%d, is_rtcp:%d, master-keys:"), srtpProfileName, dtls_serverkey, is_rtcp); LOGHEX_DEBUG(pKeys, lenKeys);

  rc = capture_init_dtls_srtp(pSrtp,  srtpProfileName, dtls_serverkey, is_rtcp, do_init_rtcp_srtp, pKeys, lenKeys);

  if(rc >= 0 && (pFilterPeer = pDtlsKeyUpdateCbData->pFilterPeer) && (pSrtpPeer = &pFilterPeer->srtps[is_rtcp ? 1 : 0])) {
    //LOG(X_DEBUG("CAPTURE_DTLS_NETSOCK_KEY_UPDATE on aud/vid mux peer"));
    rc = capture_init_dtls_srtp(pSrtpPeer,  srtpProfileName, dtls_serverkey, is_rtcp, 0, pKeys, lenKeys);
  } 

  //
  // If the capture callback data has been set with the appropriate callback parameters then
  // populate the shared context with the raw keying material which will be used for possible
  // later invocation of the stream_dtls_netsock_key_update callback from the capture context 
  // the stream side key_update callback cannot be invoked until the stream side rtp destination
  // initialization has completed
  //
  if(rc >= 0 && pDtlsKeyUpdateCbData->pCfg) {

    pthread_mutex_lock(&pDtlsKeyUpdateCbData->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    pdtlsKeyUpdateStorage = &pDtlsKeyUpdateCbData->pCfg->pStreamerCfg->sharedCtxt.\
                             av[pDtlsKeyUpdateCbData->streamSharedIdx].dtlsKeyUpdateStorages[is_rtcp ? 1 : 0];

    // pdtlsKeyUpdateStorage->pCbData will be set from stream_rtp context when it is ready to receive the update
    memcpy(pdtlsKeyUpdateStorage->dtlsKeys, pKeys, MIN(DTLS_SRTP_KEYING_MATERIAL_SIZE, lenKeys));
    pdtlsKeyUpdateStorage->dtlsKeysLen = lenKeys;
    strncpy(pdtlsKeyUpdateStorage->srtpProfileName, srtpProfileName, sizeof(pdtlsKeyUpdateStorage->srtpProfileName) - 1);
    pdtlsKeyUpdateStorage->dtls_serverkey = !dtls_serverkey;
    pdtlsKeyUpdateStorage->is_rtcp = is_rtcp;
    pdtlsKeyUpdateStorage->active = 1;

    pthread_mutex_unlock(&pDtlsKeyUpdateCbData->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  if(rc >= 0) {
    LOG(X_DEBUG("Initialized DTLS-SRTP %s %s %d byte capture key from DTLS handshake"), 
         srtpProfileName, is_rtcp ? "RTCP" : "RTP", pSrtp->kt.k.lenKey);
    //LOG(X_DEBUG("DTLS-SRTP set filter: 0x%x capture key:"), pFilter); LOGHEX_DEBUG(pSrtp->kt.k.key, pSrtp->kt.k.lenKey);
  } else {
    LOG(X_ERROR("Failed to set DTLS-SRTP %s %s capture key from DTLS handshake"), is_rtcp ? "RTCP" : "RTP", srtpProfileName); 
  }

  return rc;
}
#endif // (VSX_HAVE_SSL_DTLS)

int setup_turn_socket(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_FILTERS_T *pFilt, unsigned int idx, int is_rtcp) {

  CAPTURE_FILTER_T *pFilter = &pFilt->filters[idx];
  NETIO_SOCK_T *pnetsock = is_rtcp ? &pCfg->pSockList->netsocketsRtcp[idx] : &pCfg->pSockList->netsockets[idx];
  TURN_CTXT_T *pTurn =is_rtcp ? &pFilter->turnRtcp : &pFilter->turn;
  TURN_RELAY_SETUP_RESULT_T onResult; 
  int rc = 0;

  //LOG(X_DEBUG("SETUP_TURN_SOCKET idx[%d], is_rtcp:%d"), idx, is_rtcp);

#if defined(VSX_HAVE_TURN)

  //
  // Infer the TURN peer-address from the output destination string.  This is the address:port allocated
  // by the remote TURN server connection(s) to reach the remote party.
  //
  if(pCfg->pStreamerCfg && !IS_ADDR_VALID(pTurn->saPeerRelay.sin_addr)) {

    memset(&pTurn->saPeerRelay, 0, sizeof(pTurn->saPeerRelay));
    if(is_rtcp) {
      if(pCfg->pStreamerCfg->pdestsCfg[0].portsRtcp[idx] == 0) {
        pTurn->saPeerRelay.sin_port = htons(RTCP_PORT_FROM_RTP(pCfg->pStreamerCfg->pdestsCfg[0].ports[idx]));
      } else {
        pTurn->saPeerRelay.sin_port = htons(pCfg->pStreamerCfg->pdestsCfg[0].portsRtcp[idx]);
      }
    } else {
      pTurn->saPeerRelay.sin_port = htons(pCfg->pStreamerCfg->pdestsCfg[0].ports[idx]);
    }

    if(pCfg->pStreamerCfg->pdestsCfg[0].dstHost[0] != '\0') {
      pTurn->saPeerRelay.sin_addr.s_addr = net_resolvehost(pCfg->pStreamerCfg->pdestsCfg[0].dstHost);
    }
  }

  //
  // Setup TURN relay allocation for the socket
  //
  if(IS_ADDR_VALID(pFilter->turn.saTurnSrv.sin_addr)) {
  
    //
    // Set a result notification to modify the SDP argument with the server relay allocated ports once
    // all port allocations are completed
    //
    memset(&onResult, 0, sizeof(onResult));
    onResult.active = 1;
    onResult.is_rtcp = is_rtcp;
    onResult.isaud = IS_CAP_FILTER_AUD(pCfg->pcommon->filt.filters[idx].mediaType);
    onResult.pSdp = &pCfg->pStreamerCfg->sdpInput; 
    memcpy(&onResult.saListener, is_rtcp ? &pCfg->pSockList->salistRtcp[idx] : &pCfg->pSockList->salist[idx],
           sizeof(onResult.saListener));

    if((rc = turn_thread_start(&pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt) >= 0) &&
       (rc = turn_relay_setup(&pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt, pTurn, pnetsock, &onResult)) >= 0) {
    }

  }

#else // (VSX_HAVE_TURN)
  rc = -1;
#endif // (VSX_HAVE_TURN)

  return rc;
}

static int capture_openLocalSockets(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_FILTERS_T *pFilt, int rtcp,  
                                    CAPTURE_DTLS_KEY_UPDATE_CBDATA_T dtlsKeyUpdateCbDatas[SOCKET_LIST_MAX]) {
  int rc;
  size_t idx = 0;
  //size_t tmpidx;
  int all_unicast_rtcp = 1;
  int is_dtls = 0;
  int is_srtp = 0;
  int is_audvidmux = 0;
  char tmpstr[32];
  char tmpstr2[32];
  CAPTURE_FILTER_T *pFilter = NULL;
  CAPTURE_FILTER_T *pFilterPeer = NULL;
  SOCKET_LIST_T *pSockList = pCfg->pSockList;
#if defined(VSX_HAVE_SSL_DTLS)
  DTLS_KEY_UPDATE_CTXT_T dtlsKeysUpdateCtxts[2];
  int dtls_handshake_client = 0;
#endif // (VSX_HAVE_SSL_DTLS)

  //
  // This is called for opening UDP / RTP datagram sockets opened at 
  // startup.  RTP (output only) sockets opened after start (for RTSP) are opened in
  // stream_rtp.c::stream_rtp_adddest
  // 

  //
  // Default to using shared SRTP context for RTP/RTCP
  //
  for(idx = 0; idx < pFilt->numFilters; idx++) {
    pFilt->filters[idx].srtps[0].pSrtpShared = &pFilt->filters[idx].srtps[0];
    pFilt->filters[idx].srtps[1].pSrtpShared = &pFilt->filters[idx].srtps[0];
    pFilt->filters[idx].srtps[1].is_rtcp = 1;
  }

  if(pFilt->numFilters == 2 && pSockList->numSockets == 1) {
    is_audvidmux = 1; 
    pFilterPeer = &pFilt->filters[1];
  }


  for(idx = 0; idx < pSockList->numSockets; idx++) {

    pFilter = NULL;
    is_dtls = 0;
    tmpstr[0] = tmpstr2[0] ='\0';

    if(netio_opensocket(&pSockList->netsockets[idx], SOCK_DGRAM, 
           SOCK_RCVBUFSZ_UDP_DEFAULT, 0, &pSockList->salist[idx]) == INVALID_SOCKET) {
      capture_closeLocalSockets(pCfg, pFilt);
      return -1;
    }
     LOG(X_DEBUG("Created RTP capture socket on port:%d"), htons(pSockList->salist[idx].sin_port));

    if(net_setsocknonblock(NETIOSOCK_FD(pSockList->netsockets[idx]), 1) < 0) {
      capture_closeLocalSockets(pCfg, pFilt);
      return -1;
    }

    if(idx < pFilt->numFilters) {
      pFilter = &pFilt->filters[idx];
      if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_UDPRTP) {
        snprintf(tmpstr, sizeof(tmpstr), " RTP");
      } else if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES) {
        snprintf(tmpstr, sizeof(tmpstr), " SRTP");
        is_srtp = 1;
      } else if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP) {
        is_srtp = 1;
        is_dtls = 1;
        snprintf(tmpstr, sizeof(tmpstr), " DTLS-SRTP");
      } else if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_UDPDTLS) {
        is_dtls = 1;
        snprintf(tmpstr, sizeof(tmpstr), " DTLS");
      }
    } else {
      return -1;
    }

    if(is_srtp) {
      pSockList->netsockets[idx].flags |= NETIO_FLAG_SRTP;
    }

#if defined(VSX_HAVE_SSL_DTLS)

    memset(&dtlsKeysUpdateCtxts, 0, sizeof(dtlsKeysUpdateCtxts));

    if(is_dtls) {

      dtls_handshake_client = pCfg->pcommon->dtlsCfg.dtls_handshake_server == 1 ? 0 : !pCfg->pcommon->dtlsCfg.dtls_handshake_server;

      //LOG(X_DEBUG("DTLS capture config: dtls-handshake-key:%d (dtls_handshake_client:%d)"), pCfg->pcommon->dtlsCfg.dtls_handshake_server, dtls_handshake_client);

      //
      // Init one shared DTLS context
      //
      if(pFilt->filters[0].dtls.active) {
        //LOG(X_DEBUG("Using shared RTP DTLS input context 0x%x"), &pFilt->filters[0].dtls);
        pFilter->dtls.pDtlsShared = &pFilt->filters[0].dtls;
      } else if(pFilter->dtls.active == 0) {
        //LOG(X_DEBUG("Initalizing RTP DTLS input %s context 0x%x, transType:%d"), dtls_serverkey?"server":"client", &pFilter->dtls, pFilter->transType);
        if(dtls_ctx_init(&pFilter->dtls, &pCfg->pcommon->dtlsCfg,
                         pFilter->transType == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP) < 0) {
          pFilter->dtls.active = -1;
          capture_closeLocalSockets(pCfg, pFilt);
          return -1;
        } else {
          pFilter->dtls.pDtlsShared = &pFilter->dtls;
        }
      }

      //
      // Set any streamer shared context dtls context reference
      //
      if(pCfg->pStreamerCfg) {
        pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
        pCfg->pStreamerCfg->sharedCtxt.av[idx].pdtls = pFilter->dtls.pDtlsShared;
        pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
        //LOG(X_DEBUG("Set capture-shared dtls context pointer for avidx[%d]"), idx);
      }

      if(pFilter->dtls.pDtlsShared->dtls_srtp) {

        memset(&dtlsKeyUpdateCbDatas[idx], 0, sizeof(CAPTURE_DTLS_KEY_UPDATE_CBDATA_T));
        dtlsKeyUpdateCbDatas[idx].pFilter = pFilter;
        dtlsKeyUpdateCbDatas[idx].pFilterPeer = pFilterPeer;

        //
        // If we're using shared capture/output socket(s) then set the appropriate callback 
        // parameters so that we will subsequently invoke streamer thread dtls-srtp key callback from here
        //
        if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort) {
          dtlsKeyUpdateCbDatas[idx].pCfg = pCfg;
          dtlsKeyUpdateCbDatas[idx].streamSharedIdx = idx;
        }

        dtlsKeysUpdateCtxts[0].pCbData = &dtlsKeyUpdateCbDatas[idx];
        dtlsKeysUpdateCtxts[0].cbKeyUpdateFunc = capture_dtls_netsock_key_update;
        dtlsKeysUpdateCtxts[0].is_rtcp = 0;
        dtlsKeysUpdateCtxts[0].dtls_serverkey = dtls_handshake_client;

        memcpy(&dtlsKeysUpdateCtxts[1], &dtlsKeysUpdateCtxts[0], sizeof(dtlsKeysUpdateCtxts[0]));
        dtlsKeysUpdateCtxts[1].is_rtcp = 1;

      }

      //TODO: setclient may need to be enabled if using shared input/output sockets!
      if(dtls_netsock_init(pFilter->dtls.pDtlsShared, &pSockList->netsockets[idx], 
                           &pFilter->dtlsFingerprintVerify, &dtlsKeysUpdateCtxts[0]) < 0 ||
         dtls_netsock_setclient(&pSockList->netsockets[idx], dtls_handshake_client) < 0) {

        LOG(X_ERROR("Failed to initialize DTLS%s input RTP socket on port %d"), 
             pFilter->dtls.pDtlsShared && pFilter->dtls.pDtlsShared->dtls_srtp ? "-SRTP" : "", 
             ntohs(pSockList->salist[idx].sin_port));
        capture_closeLocalSockets(pCfg, pFilt);
        return -1;

      } else {
        //LOG(X_DEBUG("CAPTURE-DTLS-MTU TEST RTP netsock: 0x%x will-call:%d, mtu:%d"), &pSockList->netsockets[idx], pCfg->pStreamerCfg && pCfg->pStreamerCfg->cfgrtp.maxPayloadSz ? 1 : 0, pCfg->pStreamerCfg->cfgrtp.maxPayloadSz);
        if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->cfgrtp.maxPayloadSz > 0) {
          dtls_netsock_setmtu(&pSockList->netsockets[idx], pCfg->pStreamerCfg->cfgrtp.maxPayloadSz);
        }
        //dtls_netsock_setmtu(&pSockList->netsockets[idx], 512);

        LOG(X_DEBUG("Initialized DTLS%s input RTP socket on port %d"), 
            pFilter->dtls.pDtlsShared && pFilter->dtls.pDtlsShared->dtls_srtp ? "-SRTP" : "", 
            ntohs(pSockList->salist[idx].sin_port));
      }

    } // end of (is_dtls...
#endif // (VSX_HAVE_SSL_DTLS)

    //
    // Setup any TURN relay allocation
    //
    if(IS_ADDR_VALID(pFilter->turn.saTurnSrv.sin_addr) &&
       (rc = setup_turn_socket(pCfg, pFilt, idx, 0)) < 0) {
      capture_closeLocalSockets(pCfg, pFilt);
      return -1;
    }

    if(rtcp) {

      memcpy(&pSockList->salistRtcp[idx], &pSockList->salist[idx], 
             sizeof(pSockList->salistRtcp[idx]));
      if(idx < pFilt->numFilters && pFilt->filters[idx].dstPortRtcp != 0) {
        pSockList->salistRtcp[idx].sin_port = htons(pFilt->filters[idx].dstPortRtcp);
      } else {
        pSockList->salistRtcp[idx].sin_port = 
                        htons(RTCP_PORT_FROM_RTP(htons(pSockList->salistRtcp[idx].sin_port)));
      }
      
      //fprintf(stderr, "RTCP PORT:%d (RTP:%d)\n", htons(pSockList->salistRtcp[idx].sin_port), htons(pSockList->salist[idx].sin_port));

      if(pSockList->salistRtcp[idx].sin_port != pSockList->salist[idx].sin_port) {

        //if((NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]) = net_opensocket(SOCK_DGRAM, 
        if(netio_opensocket(&pSockList->netsocketsRtcp[idx], SOCK_DGRAM, 
               SOCK_RCVBUFSZ_DEFAULT, 0, &pSockList->salistRtcp[idx]) == INVALID_SOCKET) {
          capture_closeLocalSockets(pCfg, pFilt);
          return -1;
        }
        LOG(X_DEBUG("Created RTCP capture socket on port:%d"), htons(pSockList->salistRtcp[idx].sin_port));
        if(net_setsocknonblock(NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]), 1) < 0) {
          capture_closeLocalSockets(pCfg, pFilt);
          return -1;
        }

        if(is_srtp) {
          pSockList->netsockets[idx].flags |= NETIO_FLAG_SRTP;
        }

#if defined(VSX_HAVE_SSL_DTLS)
        if(is_dtls) {

          //
          // Use a separate SRTP RTCP context if the RTCP port is not muxed with the RTP port
          //
          if(is_srtp) {
            pFilter->srtps[1].pSrtpShared = &pFilter->srtps[1];
          }
           
          if(dtls_netsock_init(pFilter->dtls.pDtlsShared, &pSockList->netsocketsRtcp[idx], 
                    &pFilter->dtlsFingerprintVerify, &dtlsKeysUpdateCtxts[1]) < 0 ||
                    dtls_netsock_setclient(&pSockList->netsocketsRtcp[idx], dtls_handshake_client) < 0) {
            LOG(X_DEBUG("Failed to initialize DTLS%s input RTCP socket on port %d"), 
                  pFilter->dtls.pDtlsShared && pFilter->dtls.pDtlsShared->dtls_srtp ? "-SRTP" : "", 
                  ntohs(pSockList->salistRtcp[idx].sin_port));
            capture_closeLocalSockets(pCfg, pFilt);
            return -1;
          } else {

            //LOG(X_DEBUG("CAPTURE-DTLS-MTU TEST RTCP netsock: 0x%x will-call:%d, mtu:%d"), &pSockList->netsocketsRtcp[idx], pCfg->pStreamerCfg && pCfg->pStreamerCfg->cfgrtp.maxPayloadSz ? 1 : 0, pCfg->pStreamerCfg->cfgrtp.maxPayloadSz);
            if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->cfgrtp.maxPayloadSz > 0) {
              dtls_netsock_setmtu(&pSockList->netsocketsRtcp[idx], pCfg->pStreamerCfg->cfgrtp.maxPayloadSz);
            }
            //dtls_netsock_setmtu(&pSockList->netsocketsRtcp[idx], 512);

            LOG(X_DEBUG("Initialized DTLS%s input RTCP socket on port %d"), 
                  pFilter->dtls.pDtlsShared && pFilter->dtls.pDtlsShared->dtls_srtp ? "-SRTP" : "", 
                  ntohs(pSockList->salistRtcp[idx].sin_port));

          }
        } // end of (is_dtls...
#endif // (VSX_HAVE_SSL_DTLS)

        //
        // Setup any TURN relay allocation
        //
        if(IS_ADDR_VALID(pFilter->turn.saTurnSrv.sin_addr) &&
           (rc = setup_turn_socket(pCfg, pFilt, idx, 1)) < 0) {
          capture_closeLocalSockets(pCfg, pFilt);
          return -1;
        }

        if(IN_MULTICAST(htonl(pSockList->salistRtcp[idx].sin_addr.s_addr))) {
          all_unicast_rtcp = 0;
        }

      } // end of if(pSockList->salistRtcp[idx].sin_port != ...

      snprintf(tmpstr2, sizeof(tmpstr2), " RTCP:%d", ntohs(pSockList->salistRtcp[idx].sin_port));

    } // end of if(rtcp...

    //if(pFilter && pFilterPeer) {
    //  pFilterPeer->srtps[0].pSrtpShared = pFilter->srtps[0].pSrtpShared;
    //  pFilterPeer->srtps[1].pSrtpShared = pFilter->srtps[1].pSrtpShared;
    //}

    LOG(X_DEBUG("Listening on %s:%d%s%s"), inet_ntoa(pSockList->salist[idx].sin_addr), 
                ntohs(pSockList->salist[idx].sin_port), tmpstr, tmpstr2);
  } // end of for(idx = 0...

  if(rtcp) {
    if(pCfg->pcommon->frtcp_rr_intervalsec > 0) {
      if(all_unicast_rtcp || pCfg->pcommon->rtcp_reply_from_mcast) {
        LOG(X_DEBUG("RTCP Receiver Report interval set to %.2f sec"), pCfg->pcommon->frtcp_rr_intervalsec);
      } else {
        LOG(X_WARNING("RTCP responses to multicast are not enabled."));
      }
    } else {
      LOG(X_DEBUG("RTCP Receiver Reports are disabled."));
    }
  }

  //
  // Indicate that we are finished adding any TURN relay requests, so that the SDP callback notification
  // knows how many relays are to be completed before being invoked
  //
  if(pCfg->pStreamerCfg) {
   pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt.mtx);
   pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt.totOnResult = pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt.cntOnResult;
   pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.turnThreadCtxt.mtx);
  }
  

  return 0;
}
/*
char *capture_log_format_pkt(char *buf, unsigned int sz, const struct sockaddr_in *pSaSrc, 
                    const struct sockaddr_in *pSaDst) {
  int rc;
  if((rc = snprintf(buf, sz, "%s:%d -> :%d", inet_ntoa(pSaSrc->sin_addr), htons(pSaSrc->sin_port), 
           htons(pSaDst->sin_port))) <= 0 && sz > 0) {
    buf[0] = '\0';
  }

  return buf;
}
*/

typedef struct CAPTURE_SOCKET_CTXT {
  CAP_ASYNC_DESCR_T             *pCfg;
  TIME_VAL                       tmLastRtcpRr[SOCKET_LIST_MAX];
  TIME_VAL                       tmLastRtcpFir[SOCKET_LIST_MAX];
  TIME_VAL                       tmLastRtcpAppRemb[SOCKET_LIST_MAX];
  TIME_VAL                       tmlastpkt;
} CAPTURE_SOCKET_CTXT_T;

static int send_rtcp_toxmitter(CAPTURE_SOCKET_CTXT_T *pCtxt,
                               const CAPTURE_STREAM_T *pStream,
                               SOCKET_LIST_T *pSockList, 
                               unsigned int idx,
                               const struct sockaddr_in *psaSrc) {

  int rc = 0;
  int do_rr = 0;
  int do_fir = 0;
  int do_pli = 0;
  int do_nack = 0;
  int do_remb = 0;
  double bwPayloadBpsCurRemb = 0;
  STREAM_RTCP_RR_T *pRtcpRr;
  SRTP_CTXT_T *pSrtp = NULL;
  unsigned char buf[512];
  unsigned char buf2[512];
  unsigned char *pData = buf;
  NETIO_SOCK_T *pNetsock = NULL;

  if(!pStream) {
    return 0;
  }

  pRtcpRr  = (STREAM_RTCP_RR_T *) &pStream->rtcpRR;

  //
  // If the RTCP listener is for a multicast, ensure its ok to send back any RTCP
  //
  if(IN_MULTICAST(htonl(pSockList->salistRtcp[idx].sin_addr.s_addr)) && 
     !pCtxt->pCfg->pcommon->rtcp_reply_from_mcast) {
    //fprintf(stderr, "NOT TO MCAST..\n");
    return 0;
  } else if(!pCtxt->pCfg->pStreamerCfg) {
    //
    // We may have been called in capture only mode w/o any stream output
    //
    return 0;
  }

  //
  // Check if to send Receiver Report
  //
  if(pCtxt->pCfg->pcommon->frtcp_rr_intervalsec > 0) {

    if(pCtxt->tmLastRtcpRr[idx] == 0) {
      pCtxt->tmLastRtcpRr[idx] = pCtxt->tmlastpkt;
    } else if((pCtxt->tmlastpkt - pCtxt->tmLastRtcpRr[idx]) / TIME_VAL_MS >
      pCtxt->pCfg->pcommon->frtcp_rr_intervalsec * 1000) {

      do_rr = 1;
      pCtxt->tmLastRtcpRr[idx] = pCtxt->tmlastpkt;
    }
  }

  //fprintf(stderr, "CAP FILTER mt:%d, fir_send_intervalms:%d\n", pStream->pFilter->mediaType, pStream->fir_send_intervalms);//pCfg->pcommon->filters[0].transType

  //
  // Check if to send Full Intra Refresh request
  //
  if(pStream->fir_send_intervalms > 0 && 
     pCtxt->pCfg->pStreamerCfg->fbReq.tmLastFIRRcvd > 0 &&
    (pCtxt->tmLastRtcpFir[idx] == 0 || 
    (pCtxt->tmlastpkt - pCtxt->tmLastRtcpFir[idx]) / TIME_VAL_MS > pStream->fir_send_intervalms)) {

    do_fir = 1;
    //do_pli = 1;
    //do_rr = 1; // webrtc may need this as first pkt
    pCtxt->tmLastRtcpFir[idx] = pCtxt->tmlastpkt;
    pCtxt->pCfg->pStreamerCfg->fbReq.tmLastFIRRcvd  = 0;

    LOG(X_DEBUG("Will request IDR to RTP transmitter using RTCP FIR at %llu ms."), 
        pCtxt->tmlastpkt / TIME_VAL_MS);
  }

  //LOG(X_DEBUG("CHECK NACK COUNT:%d, nackFbSize:%d"), pRtcpRR->countNackPkts, pStream->pFilter->nackFbSize);
  //
  // Check if to send RTCP NACK 
  //
  if(pRtcpRr->countNackPkts > 0) {

    do_nack = 1;
    //do_rr = 1; // webrtc may need this as first pkt
    //LOG(X_DEBUG("Will request NACK to RTP transmitter at %llu ms."), pCtxt->tmlastpkt / TIME_VAL_MS);
  }

  //
  // Check if to send APP Receiver Estimated Max Bitrate
  //
  if(BOOL_ISENABLED(pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSend) && 
    pStream->pRembAbr && pStream->appremb_send_intervalms > 0 &&
    (pCtxt->tmLastRtcpAppRemb[idx] == 0 || 
    (pCtxt->tmlastpkt - pCtxt->tmLastRtcpAppRemb[idx]) / TIME_VAL_MS > pStream->appremb_send_intervalms)) {

    if(pStream->pRembAbr->bwPayloadBpsTargetMax != (double) pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps) {
      pStream->pRembAbr->bwPayloadBpsTargetMax = (double) pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSendMaxRateBps;
    }
    if(pStream->pRembAbr->bwPayloadBpsTargetMin != (double) pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSendMinRateBps) {
      pStream->pRembAbr->bwPayloadBpsTargetMin = (double) pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSendMinRateBps;
    }

    if(pStream->pRembAbr->forceAdjustment != pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSendForceAdjustment) {
      pStream->pRembAbr->forceAdjustment = pCtxt->pCfg->pStreamerCfg->fbReq.apprembRtcpSendForceAdjustment;
    }

    //
    // Calculate the REMB to send to the RTP sender
    //
    if(capture_abr_notifyBitrate(pStream, &bwPayloadBpsCurRemb) >= 1 && pRtcpRr->rembBitrateBps != 0) {
      do_remb = 1;
      //do_rr = 1; // webrtc may need this as first pkt
      LOG(X_DEBUG("Will send RTCP APP REMB notification to RTP video transmitter at %llu ms for %lld b/s.  Calculated %u b/s"), 
          pCtxt->tmlastpkt / TIME_VAL_MS, pRtcpRr->rembBitrateBps, (unsigned int) bwPayloadBpsCurRemb);
    }
    pCtxt->tmLastRtcpAppRemb[idx] = pCtxt->tmlastpkt;
  }

  if(!do_rr && !do_fir && !do_pli && !do_nack && !do_remb) {
    return 0;
  }

  //
  // Make sure we have the sender's remote address to where to send the RTCP packet to
  //
  if(pSockList->saRemoteRtcp[idx].sin_port == 0) {

    //
    // No prior RTCP (SR) received from transmitter, so we try to infer the RTCP ip:port
    // from the previous RTP packet
    //
    if(psaSrc && psaSrc->sin_port != 0 && IS_ADDR_VALID(psaSrc->sin_addr)) {

      memcpy(&pSockList->saRemoteRtcp[idx], psaSrc, sizeof(pSockList->saRemoteRtcp[idx]));

      if(pSockList->salistRtcp[idx].sin_port != pSockList->salist[idx].sin_port) {
        pSockList->saRemoteRtcp[idx].sin_port = htons(RTCP_PORT_FROM_RTP(
                                                      ntohs(pSockList->saRemoteRtcp[idx].sin_port)));
      }

      LOG(X_WARNING("Using remote RTCP destination obtained from RTP port %s:%d"), 
          inet_ntoa(psaSrc->sin_addr), ntohs(pSockList->saRemoteRtcp[idx].sin_port)); 
    } else {
      LOG(X_ERROR("Remote RTCP destination not set ant not available from inbound RTP (idx:%d)"), idx);
      return 0;
    }
  }

  pthread_mutex_lock(&pCtxt->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

  //
  // Since we're sending SRTP back to the stream sender using the outbound RTCP (SR) context
  // we should ensure that the outbound SSRC is the same for RR as it is for SR
  //
  if(idx < 2 && pCtxt->pCfg->pStreamerCfg->sharedCtxt.av[idx].psenderSSRC &&
     pRtcpRr->rr.hdr.ssrc != htonl(*pCtxt->pCfg->pStreamerCfg->sharedCtxt.av[idx].psenderSSRC)) {
    LOG(X_DEBUG("Setting capture RTCP RR SSRC to 0x%x"), *pCtxt->pCfg->pStreamerCfg->sharedCtxt.av[idx].psenderSSRC);
    capture_rtcp_set_local_ssrc(pRtcpRr, htonl(*pCtxt->pCfg->pStreamerCfg->sharedCtxt.av[idx].psenderSSRC));
  }

  //
  // Retrieve the SRTP context from the outbound RTP/RTCP sender storage context
  //
  if(idx < 2 && (pSrtp = SRTP_CTXT_PTR(pCtxt->pCfg->pStreamerCfg->sharedCtxt.av[idx].psrtps[1])) &&
     pSrtp->do_rtcp_responder && pSrtp->kt.k.lenKey > 0) {


  } else {
    pSrtp = NULL;
    pthread_mutex_unlock(&pCtxt->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  if((rc = capture_rtcp_create(pRtcpRr, do_rr, do_fir, do_pli, do_nack, do_remb, buf, sizeof(buf))) > 0) {

    //LOG(X_DEBUG("RTCP capture_rtcp_create len:%d, do_rr:%d, do_fir:%d, do_pli:%d, do_nack:%d, do_remb:%d"), rc, do_rr, do_fir, do_pli, do_nack, do_remb); LOGHEX_DEBUG(&buf, rc);

    VSX_DEBUG_RTCP (
      snprintf((char *) buf2, sizeof(buf2), ", via %s:%d", inet_ntoa(pSockList->salistRtcp[idx].sin_addr), 
               htons(pSockList->salistRtcp[idx].sin_port));
      LOG(X_DEBUG("RTCP - rtcp-rcvr-xmt [idx:%d], rtcp-type:%d sender-ssrc: 0x%x, len:%d to %s:%d%s"), idx, buf[1], 
             htonl( *((uint32_t *) &buf[8])  ), rc,
             inet_ntoa(pSockList->saRemoteRtcp[idx].sin_addr), htons(pSockList->saRemoteRtcp[idx].sin_port), buf2); 
      LOGHEX_DEBUG(buf, rc);
    );

    //
    // SRTP protect the RTCP packet going back to the stream sender
    //
    if(pSrtp) {
      unsigned int lenOut = sizeof(buf2);
      if(srtp_encryptPacket(pSrtp, buf, rc, buf2, &lenOut, 1) < 0) {
        rc = -1;
      } else {
        pData = buf2;
        rc = lenOut;
      }
      //LOGHEX_DEBUG(pSrtp->kt.k.key, pSrtp->kt.k.lenKey);

      pthread_mutex_unlock(&pCtxt->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    }


    //TODO: the sendto address here may be different then the streamer2.c rtp/rtcp sendto since
    // here we're using the input rtp source address.  (This may also happen when running webrtc locally).
    // Need to reconcile the two and only enable sending as per stun-policy.

    if(rc >= 0) {

      pNetsock = CAPTURE_RTCP_PNETIOSOCK(pSockList, idx);
      if(PSTUNSOCK(pNetsock)->pXmitStats) {
        pthread_mutex_lock(&pCtxt->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
        stream_stats_addPktSample(PSTUNSOCK(pNetsock)->pXmitStats, NULL, rc, 0);
        pthread_mutex_unlock(&pCtxt->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
      }

      //
      // Call the unified sendto function to ensure any mutex/stun checks are done.
      // Use a NULL SRTP context because srtp_encryptPacket was already called
      //
      rc = SENDTO_RTCP(pNetsock, (void *) pData, rc, 0, &pSockList->saRemoteRtcp[idx], NULL);

    }

    VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - sendto (rtcp %s%s%s)[%d] %s:%d %d bytes"), 
           do_rr ? "RR " : "", do_fir ? "FIR " : "", do_pli ? "PLI " : "", idx, 
           inet_ntoa(pSockList->saRemoteRtcp[idx].sin_addr), ntohs(pSockList->saRemoteRtcp[idx].sin_port), rc) );

  } else {
    if(pSrtp) {
      pthread_mutex_unlock(&pCtxt->pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    }
    if(rc < 0) {
      LOG(X_ERROR("Failed to create RTCP response packet"));
    }
  }

  return rc;
}

static CAPTURE_STREAM_T *capture_lookup_stream_fromrtcp_nossrc(CAPTURE_STATE_T *pState,
                                           const SOCKET_LIST_T *pSockList,
                                           const COLLECT_STREAM_KEY_T *pKey) {
  CAPTURE_STREAM_T *pStream = NULL;
  unsigned int idx;
  uint16_t rtcpListenerPort = 0;

  //
  // If the number of filters > number of sockets we are likely using aud/vid mux over a single port
  // so we can't rely matching incoming RTCP packet w/o knowing the corresponding SSRC
  //
  if(!pSockList || pState->filt.numFilters != pSockList->numSockets) {
    return NULL;
  }

  for(idx = 0; idx < pState->maxStreams; idx++) {

    if(pState->pStreams[idx].pFilter && 
       !memcmp(&pState->pStreams[idx].hdr.key.ipAddr, &pKey->ipAddr, pState->pStreams[idx].hdr.key.lenIp)) {

      rtcpListenerPort = pState->pStreams[idx].pFilter->dstPortRtcp > 0 ? pState->pStreams[idx].pFilter->dstPortRtcp :
                             RTCP_PORT_FROM_RTP(pState->pStreams[idx].hdr.key.dstPort);

      if(rtcpListenerPort == pKey->dstPort) {
        if(pStream) {
          pStream= NULL;
          break;
        } else {
          pStream= &pState->pStreams[idx];
        }
      }
    }
  }

  return pStream;
}

static CAPTURE_STREAM_T *capture_lookup_stream_fromrtcp(CAPTURE_STATE_T *pState,
                                           const SOCKET_LIST_T *pSockList,
                                           const struct sockaddr_in *pSaSrc, 
                                           const struct sockaddr_in *pSaDst,
                                           uint32_t ssrc)  {
  CAPTURE_STREAM_T *pStream = NULL;
  COLLECT_STREAM_KEY_T key;

  key.ssrc = htonl(ssrc);
  key.srcPort = htons(pSaSrc->sin_port);
  key.dstPort = htons(pSaDst->sin_port);
  
  // ipv4 only
  key.lenIp = ADDR_LEN_IPV4;
  key.lenKey = COLLECT_STREAM_KEY_SZ_NO_IP + (key.lenIp * 2);
  key.pair_srcipv4.s_addr = pSaSrc->sin_addr.s_addr;
  key.pair_dstipv4.s_addr = pSaSrc->sin_addr.s_addr;

  if(!(pStream = capture_rtcp_lookup(pState, &key))) {
    //
    // Attempt to lookup the corresponding RTP/RTCP stream based on source / destination ports only
    // assuming the crappy remote end is using an RTCP SSRC which does not match the RTP flow.
    // This can be seen in firefox webrtc 
    //
    if((pStream = capture_lookup_stream_fromrtcp_nossrc(pState, pSockList, &key))) {
      LOG(X_DEBUG("RTCP/RTP loose match for possibly invalid RTCP ssrc: 0x%x, rtcp-pkt :%d -> :%d, (rtp :%d-> :%d, ssrc: 0x%x)"),
      htonl(ssrc), key.srcPort, key.dstPort, pStream->hdr.key.srcPort, pStream->hdr.key.dstPort, key.ssrc);
    }
  }

  return pStream;
}

static int process_rtcp_pkt(STREAMER_CFG_T *pStreamerCfg, 
                            CAPTURE_STREAM_T *pStream,
                            const RTCP_PKT_HDR_T *pHdr,
                            unsigned int len,
                            const struct sockaddr_in *pSaSrc,
                            const struct sockaddr_in *pSaDst) {
  int rc = 0;
  char buf[128];
  RTCP_PKT_RR_T *pRR;

#define RTCP_SR_LEN  28 
  VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - process_rtcp_pkt pt:%d, len:%d"), pHdr->pt, len));

  switch(pHdr->pt) {

    case RTCP_PT_SR:

      if(ntohs(pHdr->len) < 6 || len < RTCP_SR_LEN) {
        LOG(X_ERROR("Invalid RTCP SR length %d, packet length %d %s"), ntohs(pHdr->len), len,
             capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
        return -1;
      }
 
      //TODO: check SSRC
      //TODO: check RR piggybacked on SR by checking RTCP_RR_COUNT(pHdr->hdr)

      //
      // Store the received SR into lastSrRcvd
      //
      memcpy(&((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd, pHdr, RTCP_SR_LEN);

      
      VSX_DEBUG_RTCP( 
          LOG(X_DEBUG("RTCP - Rcvd SR pt:%u, len:%u, ssrc:0x%x, ts:%u, pktcnt:%u, octetcnt:%u, ntp:%u,%u"),
              ((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.hdr.pt,
              htons(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.hdr.len),
              htonl(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.hdr.ssrc),
              htonl(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.rtpts),
              htonl(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.pktcnt),
              htonl(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.octetcnt),
              htonl(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.ntp_msw),
              htonl(((CAPTURE_STREAM_T *) pStream)->rtcpRR.lastSrRcvd.ntp_lsw)));

      rc = RTCP_PT_SR;
      break;

    case RTCP_PT_BYE:

      if(ntohs(pHdr->len) < 1 || len < 8) {
        LOG(X_ERROR("Invalid RTCP BYE length %d, packet length %d %s"), ntohs(pHdr->len), len,
             capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
        return -1;
      }

      pStream->haveRtcpBye = 1;
      LOG(X_DEBUG("Got RTCP BYE for ssrc: 0x%x %s"), pStream->hdr.key.ssrc,
             capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));

      rc = RTCP_PT_BYE;
      break;

    case RTCP_PT_RR:
      pRR = (RTCP_PKT_RR_T *) pHdr;
      VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - Receiver Report ssrc_mediassc:0x%x (%u), fraclost:0x%x, "
                      "cumlost:0x%x,0x%x,0x%x, seqcycles:%d, seqhighest:%d, jitter:0x%x, lsr:0x%x, dlsr:0x%x"), 
                      htonl(pRR->ssrc_mediasrc), htonl(pRR->ssrc_mediasrc), pRR->fraclost, pRR->cumulativelost[0], 
                      pRR->cumulativelost[1], pRR->cumulativelost[2], htons(pRR->seqcycles), htons(pRR->seqhighest),
                       htonl(pRR->jitter), htonl(pRR->lsr), htonl(pRR->dlsr)));

      //LOG(X_WARNING("RTCP reception report type (pt:%d) not handled in capture processor when rtpusebindport is set"), pHdr->pt);
      //break;

    case RTCP_PT_RTPFB:
    case RTCP_PT_PSFB:

      if(pStreamerCfg && pStreamerCfg->rtpMultisRtp[0][0].pheadall) {
        pthread_mutex_lock(&pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
        stream_process_rtcp(pStreamerCfg->rtpMultisRtp[0][0].pheadall, pSaSrc, pSaDst, pHdr, len);
        pthread_mutex_unlock(&pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

      } else {
        LOG(X_WARNING("RTCP reception report type (pt:%d) not handled in capture processor when rtpusebindport is set"), pHdr->pt);
      }

      break;

    case RTCP_PT_SDES:
      break;
    default:
      LOG(X_DEBUG("RTCP type %d not handled (hdr:0x%x)"), pHdr->pt, (pHdr->hdr & 0x3f));
      VSX_DEBUG_RTCP( LOGHEX_DEBUG(pHdr, MIN(len, 128)) );
      break;
  }

  return rc;
} 

static int validate_rtcp(const RTCP_PKT_HDR_T *pHdr, 
                         const struct sockaddr_in *pSaSrc, 
                         const struct sockaddr_in *pSaDst, 
                         unsigned int len) {
  char buf[128];

  if(!RTCP_HDR_VALID(pHdr->hdr) || len < RTCP_HDR_LEN) {
    LOG(X_WARNING("Received invalid RTCP packet header 0x%x, length %d %s"), pHdr->hdr, len,
        capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
    LOGHEX_DEBUG(pHdr, MIN(16, len));
    return -1;
  } else if(ntohs(pHdr->len) < 1) {
    LOG(X_ERROR("Invalid RTCP type %d, length %d, packet length %d %s"),
           pHdr->pt, ntohs(pHdr->len), len,
           capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
    return -1;
  }
  return 0;
}

static const CAPTURE_STREAM_T *process_rtcp(CAPTURE_STATE_T *pState,
                                            SOCKET_LIST_T     *pSockList,
                                            STREAMER_CFG_T *pStreamerCfg,
                                            const struct sockaddr_in *pSaSrc, 
                                            const struct sockaddr_in *pSaDst, 
                                            const RTCP_PKT_HDR_T *pData, 
                                            unsigned int len,
                                            int do_srtp,
                                            const NETIO_SOCK_T *pnetsock) {

  int rc = 0;
  unsigned int idx = 0;
  CAPTURE_STREAM_T *pStream = NULL;
  const RTCP_PKT_HDR_T *pHdr; 
  SRTP_CTXT_T *pSrtp = NULL;
  char buf[128];

  VSX_DEBUG_RTCP( 
    LOG(X_DEBUG("RTCP - capture process_rtcp RTCP type %d len:%d %s"), 
               ((unsigned char *) pData)[1], len, capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
    LOGHEX_DEBUG(pData, MIN(64, len));
  );

  pHdr = (const RTCP_PKT_HDR_T *) pData;
  //avc_dumpHex(stderr, pHdr, 16, 1);

  if(validate_rtcp(pHdr, pSaSrc, pSaDst, len) < 0) {
    return NULL;
  } else if(!(pStream = capture_lookup_stream_fromrtcp(pState, pSockList, pSaSrc, pSaDst, pHdr->ssrc))) {
    LOG(X_DEBUG("Unable to find corresponding RTP stream for RTCP type %d, ssrc: 0x%x %s"), 
                pHdr->pt, htonl(pHdr->ssrc), capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
    return NULL;
  }

  //LOG(X_DEBUG("----capture process_rtcp RTCP type %d len:%d %s, pFilter: 0x%x"), ((unsigned char *) pData)[1], len, capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst), pStream->pFilter); logger_LogHex(S_DEBUG, pData, len, 1);

  //
  // SRTP decrypt the rtcp packet
  //
  if(do_srtp && (pSrtp = CAPTURE_RTCP_SRTP_CTXT(pStream->pFilter)) && pSrtp->do_rtcp) {
    if(pnetsock && pSrtp->kt.k.lenKey == 0) {
      LOG(X_ERROR("Capture SRTP input key not set for RTCP packet length %d port:%d"), len, htons(pSaDst->sin_port));
    } else if(pSrtp->kt.k.lenKey > 0 && (rc = srtp_decryptPacket(pSrtp, (unsigned char *) pHdr, &len, 1)) < 0) {
      return NULL;
    } 
  }

  //LOG(X_DEBUG("----rcvd RTCP capture process_rtcp RTCP type %d len:%d %s, do_srtp:%d, pSrtp: 0x%x, lenK:%d, pSrtp->do-rtcp:%d, pnetsock->flags:0x%x, rc:%d"), ((unsigned char *) pData)[1], len, capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst), do_srtp, pSrtp, pSrtp ? pSrtp->kt.k.lenKey : -1, pSrtp ? pSrtp->do_rtcp : -1, pnetsock->flags, rc); logger_LogHex(S_DEBUG, pData, len, 1);

  while(idx < len) {

    pHdr = (const RTCP_PKT_HDR_T *) &((unsigned char *) pData)[idx];

    VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP packet at %d/%d"), idx, len) );

    if(validate_rtcp(pHdr, pSaSrc, pSaDst, len - idx) < 0) {
      break;
    }

    VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - capture calling process_rtcp_pkt RTCP pt:%d, len:%d"), 
                                pHdr->pt, len - idx) );

    if((rc = process_rtcp_pkt(pStreamerCfg, pStream, pHdr, len - idx, pSaSrc, pSaDst)) < 0) {
      break; 
    }

    //fprintf(stderr, "RTCP pt:%d, hdrlen:%d, pktlen:%d\n", pHdr->pt, ntohs(pHdr->len), len - idx);

    idx += (4 + 4 * ntohs(pHdr->len));

  } 

  return pStream;
}

static int rtcp_srtp_decrypt(CAPTURE_STATE_T *pState,
                             const struct sockaddr_in *pSaSrc, 
                             const struct sockaddr_in *pSaDst, 
                             const RTCP_PKT_HDR_T *pHdr, 
                             unsigned int *plen) {
  int rc = 0;
  CAPTURE_STREAM_T *pStream = NULL;
  SRTP_CTXT_T *pSrtp = NULL;
  char buf[128];

  if(validate_rtcp(pHdr, pSaSrc, pSaDst, *plen) < 0) {
    return -1;
  } else if(!(pStream = capture_lookup_stream_fromrtcp(pState, NULL, pSaSrc, pSaDst, pHdr->ssrc))) {
    LOG(X_DEBUG("Unable to find corresponding SRTP stream for RTCP type %d, ssrc: 0x%x %s"),
                pHdr->pt, htonl(pHdr->ssrc), capture_log_format_pkt(buf, sizeof(buf), pSaSrc, pSaDst));
    return 0;
  } else if(!(pSrtp = CAPTURE_RTCP_SRTP_CTXT(pStream->pFilter))) {
    return -1;
  }

  //
  // SRTP decrypt the rtcp packet
  //
  if(pSrtp->kt.k.lenKey > 0 && pSrtp->do_rtcp) {
    if((rc = srtp_decryptPacket(pSrtp, (unsigned char *) pHdr, plen, 1)) < 0) {
      return -1;
    }
    rc = *plen;
  }

  return rc;
}

int capture_rtcp_srtp_decrypt(RTCP_NOTIFY_CTXT_T *pRtcpNotifyCtxt,
                              const struct sockaddr_in *pSaSrc, 
                              const struct sockaddr_in *pSaDst, 
                              const RTCP_PKT_HDR_T *pHdr, 
                              unsigned int *plen) {

  if(!pRtcpNotifyCtxt || !pRtcpNotifyCtxt->pState || !pSaSrc || !pSaDst || !pHdr) {
    return -1;
  }

  return rtcp_srtp_decrypt(pRtcpNotifyCtxt->pState, pSaSrc, pSaDst, pHdr, plen);
}

const CAPTURE_STREAM_T *capture_process_rtcp(RTCP_NOTIFY_CTXT_T *pRtcpNotifyCtxt,
                                             const struct sockaddr_in *pSaSrc, 
                                             const struct sockaddr_in *pSaDst, 
                                             const RTCP_PKT_HDR_T *pData, 
                                             unsigned int len) {

  unsigned int idx;
  const CAPTURE_STREAM_T *pStream;

//LOG(X_DEBUG("CAPTURE_PROCESS_RTCP.... TURN state not yet handled....\n\n\n"));

  if(!pRtcpNotifyCtxt || !pRtcpNotifyCtxt->pState || !pRtcpNotifyCtxt->pSockList || 
     !pRtcpNotifyCtxt->pStreamerCfg || !pSaSrc || !pSaDst || !pData) {
    return NULL;
  }

  if((pStream  = process_rtcp(pRtcpNotifyCtxt->pState, pRtcpNotifyCtxt->pSockList, 
                              pRtcpNotifyCtxt->pStreamerCfg, pSaSrc, pSaDst, pData, len, 0, NULL))) {

    for(idx = 0; idx < pRtcpNotifyCtxt->pSockList->numSockets; idx++) {
      if(pSaDst->sin_port == pRtcpNotifyCtxt->pSockList->salistRtcp[idx].sin_port) {

//TODO: test for TURN here!
        if(!DEPRECATED_IS_TURN_ENABLED(pRtcpNotifyCtxt->pState->filt.filters[idx])) {
          //
          // Store the sender's remote endpoint address as the destination of RTCP RR
          //
          //fprintf(stderr, "COPY src %s:%d (idx:%d), my rcv port is:%d\n", inet_ntoa(pSaSrc->sin_addr), ntohs(pSaSrc->sin_port), idx, htons(pRtcpNotifyCtxt->pSockList->salistRtcp[idx].sin_port));
          memcpy(&pRtcpNotifyCtxt->pSockList->saRemoteRtcp[idx], pSaSrc, 
                 sizeof(pRtcpNotifyCtxt->pSockList->saRemoteRtcp[idx]));
        }
        ((STREAM_RTCP_RR_T *) &pStream->rtcpRR)->tmLastSrRcvd = timer_GetTime(); 

        break;
      }
    }


  }

  return pStream;
}

static void printCaptureStats(const CAP_ASYNC_DESCR_T *pCfg, const CAPTURE_STATE_T *pState, TIME_VAL *ptmprev) {
  TIME_VAL tm = timer_GetTime();

  if(pCfg->pcommon->verbosity > VSX_VERBOSITY_NORMAL &&
     (tm - *ptmprev) / TIME_VAL_MS > CAP_PKT_PRINT_MS) {
    rtp_capturePrintLog(pState, S_INFO);
    *ptmprev = tm;
  }
}

static int have_sendonly(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_STATE_T *pState) {
  unsigned int idx;
  SDP_XMIT_TYPE_T xmitType = SDP_TRANS_TYPE_UNKNOWN;

  //
  // cfgrtp.xmitType can be set by server/srvconfig::srv_ctrl_config 'sendonly' such as when
  // placing a PIP participant on hold
  //
  // filter xmitType is set when initializing from an SDP
  //

  if(!pCfg->pStreamerCfg) {
    return 0;
  } else if(pCfg->pStreamerCfg->cfgrtp.xmitType == SDP_XMIT_TYPE_SENDONLY) {
    return 1;
  }

  for(idx = 0; idx < pState->filt.numFilters; idx++) {

#if 1
    if(IS_CAP_FILTER_VID(pState->filt.filters[idx].mediaType)) {
      xmitType = pState->filt.filters[idx].u_seqhdrs.vid.common.xmitType;
    } else if(IS_CAP_FILTER_VID(pState->filt.filters[idx].mediaType)) {
      xmitType = pState->filt.filters[idx].u_seqhdrs.aud.common.xmitType; 
    } else {
      xmitType = SDP_XMIT_TYPE_SENDRECV;
    }
#else
    xmitType = pState->filt.filters[idx].xmitType;
#endif

    if(xmitType == SDP_XMIT_TYPE_SENDONLY) {
      return 1;
    }
  }

  return 0;
}

#define DTLS_RECORD_HEADER_LEN   13

static int rcv_handle_dtls(CAP_ASYNC_DESCR_T *pCfg, unsigned int idx, unsigned char *pData, int *pktlen,
                           const struct sockaddr_in *psaSrc, const struct sockaddr_in *psaDst, 
                           NETIO_SOCK_T *pnetsock) {
#if defined(VSX_HAVE_SSL_DTLS)

  int rc = 0;

  //LOG(X_DEBUG("DTLS rcv_handle_dtls input packet len:%d %d -> %d"), *pktlen, ntohs(psaSrc->sin_port), ntohs(psaDst->sin_port)); //logger_LogHex(S_DEBUG, pData, MIN(*pktlen, 128), 1); 

  if((rc = dtls_netsock_ondata(pnetsock, pData, *pktlen, psaSrc)) < 0) {
    return rc;
  }

  //
  // The result should be 0 if the DTLS handshake is still ongoing
  //
  *pktlen = rc;

  //LOG(X_DEBUG("DTLS rcv_handle_dtls output packet len:%d %d -> %d"), *pktlen, ntohs(psaSrc->sin_port), ntohs(psaDst->sin_port)); //logger_LogHex(S_DEBUG, pData, MIN(*pktlen, 128), 1); 

  return 0;

#else // (VSX_HAVE_SSL_DTLS)
  return -1;
#endif // (VSX_HAVE_SSL_DTLS)
}

static int rcv_handle_turn(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_FILTERS_T *pFilt, 
                           unsigned int idx, unsigned char **ppData, int *pktlen,
                           const struct sockaddr_in *psaSrc, SOCKET_LIST_T *pSockList,
                           int is_rtcp, int is_turn_channeldata, int *pis_turn, int *pis_turn_indication) {

  int rc = 0;
  NETIO_SOCK_T *pnetsock = is_rtcp ? &pSockList->netsocketsRtcp[idx] : &pSockList->netsockets[idx];
  const struct sockaddr_in *psaDst = is_rtcp ? &pSockList->salistRtcp[idx] : &pSockList->salist[idx];
  TURN_CTXT_T *pTurn = is_rtcp ? &pFilt->filters[idx].turnRtcp : &pFilt->filters[idx].turn;

  rc = turn_onrcv_pkt(pTurn, &pFilt->filters[idx].stun, is_turn_channeldata,  pis_turn, pis_turn_indication,
                      ppData, pktlen, psaSrc, psaDst, pnetsock);
  return rc;
}

static int rcv_check_stun(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_FILTERS_T *pFilt,
                          unsigned int idx, const unsigned char *pData, 
                          unsigned int pktlen, const struct sockaddr_in *psaSrc, 
                          const struct sockaddr_in *psaDst, NETIO_SOCK_T *pnetsock, 
                          int *pis_stun, int is_turn, int is_rtcp) {
  int rc = 0;
  TURN_CTXT_T *pTurn = is_rtcp ? &pFilt->filters[idx].turnRtcp : &pFilt->filters[idx].turn;
  *pis_stun = 0;

  //LOG(X_DEBUG("RCV_CHECKSTUN: TRANSPORT_RTP:%d, pData:%d, stun.policy:%d, stun_ispacket:%d, pktlen:%d"), IS_CAPTURE_FILTER_TRANSPORT_RTP(pFilt->filters[idx].transType), (pData[0] & RTPHDR8_VERSION_MASK) != RTP8_VERSION, pFilt->filters[idx].stun.policy, stun_ispacket(pData, pktlen), pktlen); //LOGHEX_DEBUG(pData, pktlen);

  //
  // Check for a STUN packet
  //
  if(IS_CAPTURE_FILTER_TRANSPORT_RTP(pFilt->filters[idx].transType) &&
     (pData[0] & RTPHDR8_VERSION_MASK) != RTP8_VERSION &&
     pFilt->filters[idx].stun.policy && stun_ispacket(pData, pktlen)) {

    *pis_stun = 1;

    if(pCfg->pStreamerCfg) {
      pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
      pFilt->filters[idx].stun.pPeer = pCfg->pStreamerCfg->sharedCtxt.av[idx].pstun;
    }

    // If TURN is enabled, check if this is a packet originating from the TURN server without use of a TURN channel / send indication
    if(!is_turn && pnetsock->turn.use_turn_indication_in &&
       psaSrc->sin_addr.s_addr == pnetsock->turn.saTurnSrv.sin_addr.s_addr) {
      is_turn = 1;
    }

    //LOG(X_DEBUG("RCV_CHECK_STUN calling stun_onrcv turnRcvCtxt.is_turn_indication:%d, %s:%d"),  pnetsock->turn.use_turn_indication_in, inet_ntoa(pnetsock->turn.saPeerRelay.sin_addr), htons(pnetsock->turn.saPeerRelay.sin_port));
    rc = stun_onrcv(&pFilt->filters[idx].stun, pTurn, pData, pktlen, psaSrc, psaDst, 
                    pnetsock, is_turn);

    if(pCfg->pStreamerCfg) {
      pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    }
  }

  return rc;
}


static int readLocalSockets(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_STATE_T *pState, int rtcp, 
                            int update_delayed_output) {
  unsigned int idx = 0;
  //unsigned int tmpidx;
  int rc;
  int pktlen;
  struct timeval tv;
  struct sockaddr_in saSrc;
  SOCKET fdHighest;
  fd_set fdsetRd;
  int len;
  TIME_VAL tmprev, tm;
  int moreData;
  const CAPTURE_STREAM_T *pStream;
  unsigned char  buf[RTP_JTBUF_PKT_BUFSZ_LOCAL + SOCKET_LIST_PREBUF_SZ];
  SOCKET_LIST_T *pSockList = pCfg->pSockList;
  unsigned char *pData = &buf[SOCKET_LIST_PREBUF_SZ];
  unsigned int szDataMax = sizeof(buf) - (pData - buf);
  int sendonly = 0;
  int no_output = 0;
  int have_sendonly_generator = 0;
  int is_stun;
  int is_turn;
  int is_turn_indication;
  int is_turn_channeldata;
  int is_dtls;
  NETIO_SOCK_T *pnetsock;
  STREAMER_STATE_T state_sendonly = STREAMER_STATE_FINISHED;
  pthread_mutex_t mtx_sendonly;
  CAPTURE_SOCKET_CTXT_T ctxt;
#if defined(VSX_HAVE_SSL_DTLS)
  unsigned int iter;
#endif // (VSX_HAVE_SSL_DTLS)

  memset(&ctxt, 0, sizeof(ctxt));
  memset(&saSrc, 0, sizeof(saSrc));
  ctxt.pCfg = pCfg;
  ctxt.tmlastpkt = tmprev = timer_GetTime();
  pthread_mutex_init(&mtx_sendonly, NULL);

//TIME_VAL tmdelme0 = timer_GetTime();;
  while(pCfg->running == STREAMER_STATE_RUNNING && g_proc_exit == 0) {

//if(((timer_GetTime() - tmdelme0)/TIME_VAL_MS) > 2000 && pCfg->pStreamerCfg->xcode.vid.pip.active) { g_proc_exit=1; }

    tv.tv_sec = 0;
    tv.tv_usec = 300000;

    fdHighest = 0;
    FD_ZERO(&fdsetRd);

    //
    // If the input is sendonly then start the dummy input frame generator
    //
    sendonly = have_sendonly(pCfg, pState);
    if(sendonly && !have_sendonly_generator) {
      have_sendonly_generator = 1;
      capture_send_dummy_frames_start(pCfg, pState, &state_sendonly, &mtx_sendonly);
    }

    //
    // If we are a PIP and we were started with delayed_output set, then we only want to respond to STUN 
    // binding requests and not yet receive any RTP because RTP payload type(s) / SRTP keys may not
    // have yet been setup from remote source (remote 200 OK w/ SDP not yet received).
    //
    if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->delayed_output > 0) {
      no_output = 1;
    } else {

      //
      // One time delayed output update, for when we were first started only to respond to STUN bindings
      // but before any SDP SRTP crypto keys are known or rtpmap stream types
      //
      if(update_delayed_output) {
        update_delayed_output = 0;
        for(idx = 0; idx < pState->filt.numFilters; idx++) {

          if(pState->filt.filters[idx].srtps[0].privData) { LOG(X_DEBUG("capture delayed_output: srtp context already exists, key-len:%d -> %d 0x%x -> 0x%x!!!"), pState->filt.filters[idx].srtps[0].kt.k.lenKey, pCfg->pcommon->filt.filters[idx].srtps[0].kt.k.lenKey, pState->filt.filters[idx].srtps[0].privData, pCfg->pcommon->filt.filters[idx].srtps[0].privData); LOGHEX_DEBUG(pState->filt.filters[idx].srtps[0].kt.k.key, pState->filt.filters[idx].srtps[0].kt.k.lenKey); LOGHEX_DEBUG(pCfg->pcommon->filt.filters[idx].srtps[0].kt.k.key, pCfg->pcommon->filt.filters[idx].srtps[0].kt.k.lenKey); }

          if(pState->filt.filters[idx].srtps[0].privData || pState->filt.filters[idx].srtps[1].privData) {
            LOG(X_WARNING("delaystart failed to update SRTP key(s) because SRTP context is already created"));            
          } else {
            memcpy(pState->filt.filters[idx].srtps, pCfg->pcommon->filt.filters[idx].srtps, 
                   sizeof(pState->filt.filters[idx].srtps));
          }
          //LOG(X_DEBUG("UPDATING DELAY START [idx:%d], pStun: 0x%x (from 0x%x), pTurn: 0x%x (from 0x%x), stun.pTurn: 0x%x, relay_active:%d"), idx, &pState->filt.filters[idx].stun, &pCfg->pcommon->filt.filters[idx].stun, &pState->filt.filters[idx].turn, &pCfg->pcommon->filt.filters[idx].turn, pState->filt.filters[idx].stun.pTurn, pState->filt.filters[idx].turn.relay_active);


          //
          // We are assuming that the stun context was read-only and we overwrite it with possibly updated ufrag/password info
          //
          //pTurn = pState->filt.filters[idx].stun.pTurn;
          //pTurnOut = pState->filt.filters[idx].stun.pTurnOut;
          memcpy(&pState->filt.filters[idx].stun, &pCfg->pcommon->filt.filters[idx].stun, 
                 sizeof(pState->filt.filters[idx].stun));

          //
          // We don't want to touch the TURN context since it may already have allocate a TURN relay and has an active thread
          //
          //pState->filt.filters[idx].stun.pTurn = pTurn;
          //pState->filt.filters[idx].stun.pTurnOut = pTurnOut;

          //if(pCfg->pStreamerCfg && IS_TURN_ENABLED(pState->filt.filters[idx])) {
            //  LOG(X_DEBUG("SETTING IS_OUTPUT_TURN=1"));
            //for(tmpidx = 0; tmpidx < IXCODE_VIDEO_OUT_MAX; tmpidx++) {
            //  pCfg->pStreamerCfg->rtpMultisRtp[tmpidx][0].is_output_turn = pCfg->pStreamerCfg->rtpMultisRtp[tmpidx][1].is_output_turn = 1;
            //}
          //}
//TODO: set the ouptut->pdest stun server context from pdest
          pState->filt.filters[idx].transType = pCfg->pcommon->filt.filters[idx].transType;
          pState->filt.filters[idx].mediaType = pCfg->pcommon->filt.filters[idx].mediaType;
          memcpy(&pState->filt.filters[idx].u_seqhdrs, &pCfg->pcommon->filt.filters[idx].u_seqhdrs, 
              sizeof(pState->filt.filters[idx].u_seqhdrs));

          //LOG(X_DEBUG("POST UPDATE DELAY START [idx:%d], pStun: 0x%x (from 0x%x), pTurn: 0x%x (from 0x%x), stun.pTurn: 0x%x, relay_active:%d"), idx, &pState->filt.filters[idx].stun, &pCfg->pcommon->filt.filters[idx].stun, &pState->filt.filters[idx].turn, &pCfg->pcommon->filt.filters[idx].turn, pState->filt.filters[idx].stun.pTurn, pState->filt.filters[idx].turn.relay_active);
        }
      }

      no_output = 0;
    }

    for(idx = 0; idx < pSockList->numSockets; idx++) {

      //
      // Set any RTP / UDP sockets to listen
      //
      if(NETIOSOCK_FD(pSockList->netsockets[idx]) != INVALID_SOCKET) {
        FD_SET(NETIOSOCK_FD(pSockList->netsockets[idx]), &fdsetRd);
        if(NETIOSOCK_FD(pSockList->netsockets[idx]) > fdHighest) {
          fdHighest = NETIOSOCK_FD(pSockList->netsockets[idx]);
        }
        //fprintf(stderr, "SET SOCKET RTP port:%d, fd:%d\n", ntohs(pSockList->salist[idx].sin_port), pSockList->netsockets[idx].sock);
      }

      //
      // Set any RTCP sockets to listen
      //
      if(rtcp && pSockList->salistRtcp[idx].sin_port != pSockList->salist[idx].sin_port &&
         NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]) != INVALID_SOCKET) {
        FD_SET(NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]), &fdsetRd);
        if(NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]) > fdHighest) {
          fdHighest = NETIOSOCK_FD(pSockList->netsocketsRtcp[idx]);
        }
        //fprintf(stderr, "SET SOCKET RTCP port:%d fd:%d\n", ntohs(pSockList->salistRtcp[idx].sin_port), pSockList->sockRtcp[idx]);
      }

    }

    if((pktlen = select(fdHighest + 1, &fdsetRd, NULL, NULL, &tv)) > 0) {

      moreData = 0;

      do {

        ctxt.tmlastpkt = timer_GetTime();
        TV_FROM_TIMEVAL(tv, ctxt.tmlastpkt);

        //
        // Read socket UDP / RTP loop
        //
        for(idx = 0; idx < pSockList->numSockets; idx++) {

        //TODO: not sure - but FD_ISSET may have been always false on win xp
#if !defined(WIN32)
          if(NETIOSOCK_FD(pSockList->netsockets[idx]) == INVALID_SOCKET || 
             !FD_ISSET(NETIOSOCK_FD(pSockList->netsockets[idx]), &fdsetRd)) {
            continue;
          }
#endif // WIN32
          pData = &buf[SOCKET_LIST_PREBUF_SZ];
          rc = 0;
          is_stun = 0;
          is_turn = 0;
          is_turn_indication = 0;
          is_turn_channeldata = 0;
          is_dtls = 0;
          len = sizeof(struct sockaddr_in);
          pStream = NULL;
          if((pktlen = recvfrom(NETIOSOCK_FD(pSockList->netsockets[idx]), 
                            (void *) pData, 
                            szDataMax, 0,
                            (struct sockaddr *) &saSrc, 
                            (socklen_t *) &len)) > 0) {

            //LOG(X_DEBUG("local socket read[sock:%lu/%d]: %d time:%llu 0x%x pt:0x%x, ssrc:0x%x, sendonly:%d, rtcp:%d %s:%d->:%d"), idx,pSockList->numSockets, pktlen, timer_GetTime(), pData[0], pData[1]&0x7f, htonl(*((uint32_t *) (&pData[8]))), sendonly, rtcp, inet_ntoa(saSrc.sin_addr), htons(saSrc.sin_port), htons(pSockList->salist[idx].sin_port)); //logger_LogHex(S_DEBUG, pData, MIN(pktlen, 16), 1); 

            //
            // If TURN is enabled on the listener socket then check for a TURN (STUN) packet and possibly
            // unpack any encapsulated TURN data if the TURN packet is an indication
            //
            //if(IS_TURN_ENABLED(pState->filt.filters[idx]) && IS_CAPTURE_FILTER_TRANSPORT_RTP(pState->filt.filters[idx].transType) &&
            if(IS_SOCK_TURN(&pSockList->netsockets[idx]) && 
               IS_CAPTURE_FILTER_TRANSPORT_RTP(pState->filt.filters[idx].transType) &&
               (pData[0] & RTPHDR8_VERSION_MASK) != RTP8_VERSION && 
               (stun_ispacket(pData, pktlen) || (is_turn_channeldata = turn_ischanneldata(pData, pktlen)))) {

              if((rc = rcv_handle_turn(pCfg, &pState->filt, idx, &pData, &pktlen, &saSrc, pSockList,
                                       0, is_turn_channeldata, &is_turn, &is_turn_indication)) < 0) {

              }

            }

            if(rc >= 0 && (!is_turn || is_turn_indication || is_turn_channeldata) && 
               (pSockList->netsockets[idx].flags & NETIO_FLAG_SSL_DTLS) && DTLS_ISPACKET(pData, pktlen)) {
              if((rc = rcv_handle_dtls(pCfg, idx, pData, &pktlen, &saSrc, &pSockList->salist[idx], 
                                       &pSockList->netsockets[idx])) < 0 ||
                 pktlen <=  0) {
                is_dtls = 1;
              }
            }

            //
            // Check for a STUN packet
            //
            if(rc >= 0 && !is_dtls && (!is_turn || is_turn_indication || is_turn_channeldata)) { 
              rc = rcv_check_stun(pCfg, &pState->filt, idx, pData, pktlen, &saSrc, 
                                  &pSockList->salist[idx], &pSockList->netsockets[idx], &is_stun, is_turn, 0);
            }

#if 0 && defined(VSX_HAVE_DEBUG)
            if(is_stun) {
               LOG(X_DEBUG("RECV-STUN sock:%d, len:%d %s:%d->%d"), NETIOSOCK_FD(pSockList->netsockets[idx]), pktlen, inet_ntoa(saSrc.sin_addr), htons(saSrc.sin_port), htons(pSockList->salist[idx].sin_port));
            } else if((pData[0] & RTPHDR8_VERSION_MASK) == RTP8_VERSION && ((pData[1] & 0x7f) < 72 || (((pData[1] & 0x7f) > 76) && (pData[1] & 0x7f) <= 128))) {
               LOG(X_DEBUG("RECV-RTP sock:%d, pt:%d, seq:%d, ts:%u, ssrc:0x%x, len:%d %s:%d->:%d"), NETIOSOCK_FD(pSockList->netsockets[idx]), (pData[1] & 0x7f), htons(*((uint16_t*) &pData[2])), htonl(*((uint32_t*)&pData[4])), htonl(*((uint32_t*)&pData[8])), pktlen, inet_ntoa(saSrc.sin_addr), htons(saSrc.sin_port), htons(pSockList->salist[idx].sin_port));


            } else if((pData[0] & RTPHDR8_VERSION_MASK) == RTP8_VERSION && (pData[1]) > 126) {
               LOG(X_DEBUG("RECV-RTCP sock:%d, pt:%d, ssrc:0x%x, len:%d %s:%d->%d"), NETIOSOCK_FD(pSockList->netsockets[idx]), pData[1],  htonl(*((uint32_t*)&pData[4])), pktlen, inet_ntoa(saSrc.sin_addr), htons(saSrc.sin_port), htons(pSockList->salist[idx].sin_port));

            } else if(((pData[idx] & RTPHDR8_VERSION_MASK) != RTP8_VERSION && !is_stun &&
               IS_CAPTURE_FILTER_TRANSPORT_RTP(pCfg->pcommon->filters[idx].transType))) {
               LOG(X_DEBUG("RECV-Not RTP/RTCP read[sock:%lu/%d], sz:%d stun_policy:%d, %s:%d->%d"), idx, pSockList->numSockets, pktlen, pCfg->pcommon->filters[idx].stun.policy, inet_ntoa(saSrc.sin_addr), htons(saSrc.sin_port), htons(pSockList->salist[idx].sin_port));
               LOGHEX_DEBUG(pData, MIN(pktlen, 128));

            } else {
              //if((pData[1] & 0x7f) == 101) {
              //  LOG(X_DEBUG("Telephone-event len:%d"), pktlen);  logger_LogHex(S_DEBUG, pData, MIN(pktlen, 128), 1); 
              //}
            }
            //LOG(X_DEBUG("SOCKET:%d, pt:%d, seq:%d, ts:%u, ssrc:0x%x"), pSockList->netsockets[idx].sock, (pData[1] & 0x7f), htons(*((uint16_t*) &pData[2])), htonl(*((uint32_t*)&pData[4])), htonl(*((uint32_t*)&pData[8])));
#endif // (VSX_HAVE_DEBUG)
 
            //
            // If the RTCP port is the same as the RTP port, check if this is actually an RTCP packet.
            // This accounts for RTCP types 200-206 and herby excludes RTP payload types 72-78 
            // with marker bit set
            //
            if(rc >= 0) {
            if(!no_output && rtcp &&
               pSockList->salistRtcp[idx].sin_port == pSockList->salist[idx].sin_port && 
               (!is_turn || is_turn_indication || is_turn_channeldata) && !is_stun && !is_dtls && len > 2 &&
               pData[1] >= RTCP_PT_SR && pData[1] <= RTCP_PT_PSFB) {
             
              if((pStream  = process_rtcp(pState, pSockList, pCfg->pStreamerCfg, &saSrc, 
                                          &pSockList->salistRtcp[idx], (RTCP_PKT_HDR_T *) pData, pktlen, 
                                          1, &pSockList->netsockets[idx]))) {

                //if(!IS_TURN_ENABLED(pState->filt.filters[idx])) {
                if(!IS_SOCK_TURN(&pSockList->netsockets[idx])) {
                  //
                  // Store the sender's remote endpoint address as the destination of RTCP RR
                  //
                  memcpy(&pSockList->saRemoteRtcp[idx], &saSrc, sizeof(pSockList->saRemoteRtcp[idx]));
                }
                ((STREAM_RTCP_RR_T *) &pStream->rtcpRR)->tmLastSrRcvd = ctxt.tmlastpkt;
              }

            } else if(!no_output && !sendonly && !is_stun && (!is_turn || is_turn_indication || is_turn_channeldata) && !is_dtls) {

              if(have_sendonly_generator) {
                pthread_mutex_lock(&mtx_sendonly);
              }

              pStream = capture_onUdpSockPkt(pState, pData, pktlen, (pData - buf),
                                 &saSrc, &pSockList->salist[idx], &tv, &pSockList->netsockets[idx]);

              if(have_sendonly_generator) {
                pthread_mutex_unlock(&mtx_sendonly);
              }
            }
            }

            //if(recvfrom(pSockList->sockets[idx], buf, 1, MSG_PEEK, NULL, NULL) > 0) {
            //  fprintf(stderr, "have more data\n");
            //  moreData = 1;
            //}

            //
            // Send any RTCP packet(s) back to the RTP sender 
            //
            if(!sendonly && !no_output) {
              rc = send_rtcp_toxmitter(&ctxt, pStream, pSockList, idx, &saSrc);
            }

#if defined(WIN32) && !defined(__MINGW32__)
          } else if(WSAGetLastError() == WSAEWOULDBLOCK) {
#else // WIN32
          } else if(errno == EAGAIN) {
#endif // WIN32
            pktlen = 0;
          } else {
            LOG(X_ERROR("recv[sock:%u] failed with pktlen:%d "ERRNO_FMT_STR), idx, pktlen, ERRNO_FMT_ARGS);
            pktlen = -1;
            break;
          }

#if defined(VSX_HAVE_SSL_DTLS)
          //
          // For DTLS-SRTP output which uses shared capture/output sockets we need to call the stream
          // site dtls_netsock_key_update callback to set output SRTP keys for RTP/RTCP
          // This can only be done after the DTLS handshake completed (capture_dtls_netsock_key_udpate was invoked)
          // and the streamer thread output has completed initialization
          //
          for(iter = 0; iter < 2; iter++) {

            if(pCfg->pStreamerCfg &&
               pCfg->pStreamerCfg->sharedCtxt.av[idx].dtlsKeyUpdateStorages[iter].active &&
               pCfg->pStreamerCfg->sharedCtxt.av[idx].dtlsKeyUpdateStorages[iter].pCbData &&
               pCfg->pStreamerCfg->sharedCtxt.av[idx].dtlsKeyUpdateStorages[iter].readyForCb) {

              capture_dtls_netsock_key_update_streamer(pCfg,  
                   &pCfg->pStreamerCfg->sharedCtxt.av[idx].dtlsKeyUpdateStorages[iter], 
                   iter == 0 ? &pSockList->netsockets[idx] : &pSockList->netsocketsRtcp[idx]);

              pCfg->pStreamerCfg->sharedCtxt.av[idx].dtlsKeyUpdateStorages[iter].active = 0;
            }
          }
#endif // (VSX_HAVE_SSL_DTLS)

        } // end of for

        if(pktlen < 0) {
          break;
        }

        //
        // Read RTCP loop
        //
        if(rtcp) {


          for(idx = 0; idx < pSockList->numSockets; idx++) {

            //
            // Skip this if the RTCP port is the same as RTP
            //
            if(pSockList->salistRtcp[idx].sin_port == pSockList->salist[idx].sin_port) {
              continue;
            }

            //TODO: not sure - but FD_ISSET may have been always false on win xp
#if !defined(WIN32)
            if(CAPTURE_RTCP_FD(pSockList, idx) == INVALID_SOCKET || 
               !FD_ISSET(CAPTURE_RTCP_FD(pSockList, idx), &fdsetRd)) {
              continue;
            }
#endif // WIN32
            pData = &buf[SOCKET_LIST_PREBUF_SZ];
            rc = 0;
            is_stun = 0;
            is_turn = 0;
            is_turn_indication = 0;
            is_turn_channeldata = 0;
            is_dtls = 0;
            pnetsock = CAPTURE_RTCP_PNETIOSOCK(pSockList, idx);
            len = sizeof(struct sockaddr_in);
            pStream = NULL;

            //if((pktlen = recvfrom(CAPTURE_RTCP_FD(pSockList, idx), 
            if((pktlen = recvfrom(PNETIOSOCK_FD(pnetsock),
                            (void *) pData, 
                            szDataMax, 0,
                            (struct sockaddr *) &saSrc, 
                            (socklen_t *) &len)) > 0) {

              //LOG(X_DEBUG("local socket RTCP%s read[sock:%lu/%d]: %d time:%llu 0x%x pt:0x%x, ssrc:0x%x, sendonly:%d, rtcp:%d %s:%d->:%d"), (CAPTURE_RTCP_PNETIOSOCK(pSockList, idx)->flags & NETIO_FLAG_SSL_DTLS) ? "DTLS" : "", idx,pSockList->numSockets, pktlen, timer_GetTime(), pData[0], pData[1]&0x7f, htonl(*((uint32_t *) (&pData[8]))), sendonly, rtcp, inet_ntoa(saSrc.sin_addr), htons(saSrc.sin_port), htons(pSockList->salistRtcp[idx].sin_port)); //logger_LogHex(S_DEBUG, pData, MIN(pktlen, 64), 1); 

              //
              // If TURN is enabled on the listener socket then check for a TURN (STUN) packet and possibly
              // unpack any encapsulated TURN data if the TURN packet is an indication
              //
              //if(IS_TURN_ENABLED(pState->filt.filters[idx]) && IS_CAPTURE_FILTER_TRANSPORT_RTP(pState->filt.filters[idx].transType) &&
              if(IS_SOCK_TURN(pnetsock) && IS_CAPTURE_FILTER_TRANSPORT_RTP(pState->filt.filters[idx].transType) &&
                 (pData[0] & RTPHDR8_VERSION_MASK) != RTP8_VERSION && 
                 (stun_ispacket(pData, pktlen) || (is_turn_channeldata = turn_ischanneldata(pData, pktlen)))) {

                if((rc = rcv_handle_turn(pCfg, &pState->filt, idx, &pData, &pktlen, &saSrc, pSockList,
                                         1, is_turn_channeldata, &is_turn, &is_turn_indication)) < 0) {
  
                }
              }

              //if(rc >= 0 && (CAPTURE_RTCP_PNETIOSOCK(pSockList, idx)->flags & NETIO_FLAG_SSL_DTLS) && DTLS_ISPACKET(pData, pktlen)) {
              if(rc >= 0 && (pnetsock->flags & NETIO_FLAG_SSL_DTLS) && DTLS_ISPACKET(pData, pktlen)) {

                if((rc = rcv_handle_dtls(pCfg, idx, pData, &pktlen, &saSrc, &pSockList->salistRtcp[idx], 
                                         //CAPTURE_RTCP_PNETIOSOCK(pSockList, idx))) < 0 || pktlen <=  0) {
                                         pnetsock)) < 0 || pktlen <=  0) {
                  is_dtls = 1;
                }
              }

              //
              // Check for a STUN packet
              //
              if(rc >= 0 && !is_dtls && (!is_turn || is_turn_indication || is_turn_channeldata)) { 
                rc = rcv_check_stun(pCfg, &pState->filt, idx, pData, pktlen, &saSrc, &pSockList->salistRtcp[idx], 
                                    pnetsock, &is_stun, is_turn, 1);
              }

              if(rc >= 0 && !is_stun && (!is_turn || is_turn_indication || is_turn_channeldata) && !is_dtls && !no_output && 
                 (pStream  = process_rtcp(pState, pSockList, pCfg->pStreamerCfg, &saSrc, 
                                          &pSockList->salistRtcp[idx], (RTCP_PKT_HDR_T *) pData, pktlen, 
                                          1, pnetsock))) {
                
                if(!IS_SOCK_TURN(pnetsock)) {
                  //
                  // Store the sender's remote endpoint address as the destination of RTCP RR
                  //
                  memcpy(&pSockList->saRemoteRtcp[idx], &saSrc, sizeof(pSockList->saRemoteRtcp[idx]));
                }
                ((STREAM_RTCP_RR_T *) &pStream->rtcpRR)->tmLastSrRcvd = ctxt.tmlastpkt;
                //avc_dumpHex(stderr, pData, pktlen, 1);
              }

#if defined(WIN32) && !defined(__MINGW32__)
            } else if(WSAGetLastError() == WSAEWOULDBLOCK) {
#else // WIN32
            } else if(errno == EAGAIN) {
#endif // WIN32
              //fprintf(stderr, "no rtcp[%d]\n", idx);
              pktlen = 0;
            } else {
              LOG(X_ERROR("recv[rtcpsock:%u] failed with pktlen:%d "ERRNO_FMT_STR), idx, pktlen, ERRNO_FMT_ARGS);
              pktlen = -1;
              break; 
            }

          } // end of for

          if(pktlen < 0) {
            break;
          }

        } // end of if(rtcp

      } while(moreData);

      //
      // Print any capture statistics
      //
      printCaptureStats(pCfg, pState, &tmprev);

    } else if(pktlen == 0) {

       //fprintf(stderr, "select rc:%d\n", pktlen);

//if(recvfrom(pSockList->sockets[idx], pData, 1, MSG_PEEK, NULL, NULL) > 0) {
//  fprintf(stderr, "have more data\n");
//  moreData = 1;
//} else fprintf(stderr, "no data\n");

      //
      // Print any capture statistics
      //
      printCaptureStats(pCfg, pState, &tmprev);

      //
      // Check capture idle timeouts
      //
      tm = timer_GetTime();
      if(!sendonly &&
         ((pState->numPkts == 0 && pCfg->pcommon->idletmt_1stpkt_ms > 0 && 
           (tm -  ctxt.tmlastpkt) / TIME_VAL_MS > pCfg->pcommon->idletmt_1stpkt_ms) ||
         (pState->numPkts > 0 && pCfg->pcommon->idletmt_ms > 0 && 
          (tm - ctxt.tmlastpkt) / TIME_VAL_MS > pCfg->pcommon->idletmt_ms))) {
        LOG(X_WARNING("Capture idle timeout reached %u ms"), 
         (pState->numPkts == 0 ? pCfg->pcommon->idletmt_1stpkt_ms : pCfg->pcommon->idletmt_ms));
        break;
      }

//for(idx = 0; idx < pState->maxStreams; idx++) {
//  fprintf(stderr, "STREAM[%d/%d] mt:%d\n", idx, pState->maxStreams,
//pState->pStreams[idx].pFilter ? pState->pStreams[idx].pFilter->transType: -1);
//}

      //
      // Do not check for RTCP BYE reception during periods of being on hold
      // since some client implementations can send an RTCP BYE when going on hold.
      //
      if(!sendonly && capture_check_rtp_bye(pState)) {
        LOG(X_WARNING("All input streams ended with RTCP BYE"));
        break;
      }

    } else {
      LOG(X_ERROR("capture socket select returned: %d"), pktlen);
      pktlen = -1;
      break;
    } 

  }

  while(state_sendonly >= STREAMER_STATE_RUNNING) {
    state_sendonly = STREAMER_STATE_INTERRUPT;
    usleep(5000);
  }
  pthread_mutex_destroy(&mtx_sendonly);

  return 0;
}

int capture_socketStart(CAP_ASYNC_DESCR_T *pCfg) {

  RTCP_NOTIFY_CTXT_T rtcpNotifyCtxt;
  CAPTURE_STATE_T *pState = NULL;
  CAPTURE_CBDATA_T streamCbData;
  unsigned int idx;
  int rc = 0;
  int rtcp = 0;
  int update_delayed_output = 0;
  unsigned int jtBufSzInPktsVid = 0;
  unsigned int jtBufSzInPktsAud = 0;
  unsigned int jtBufPktBufSz = 0;
  CAPTURE_DTLS_KEY_UPDATE_CBDATA_T dtlsKeyUpdateCbDatas[SOCKET_LIST_MAX];

  if(!pCfg || !pCfg->pSockList) {
    return -1;
  }

  if(pCfg->pcommon->filt.filters[0].transType != CAPTURE_FILTER_TRANSPORT_UDPRAW &&
     !IS_CAPTURE_FILTER_TRANSPORT_RTP(pCfg->pcommon->filt.filters[0].transType)) {
    LOG(X_ERROR("Socket capture for non udp/rtp method not implemented"));
    return -1;
  }

  capture_initCbData(&streamCbData, pCfg->record.outDir, pCfg->record.overwriteOut);
  streamCbData.pCfg = pCfg;

  //if(pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_UDPRAW) {
  //  jtBufSzInPktsVid = 0;
  //  jtBufSzInPktsAud = 0;
  //}

  jtBufPktBufSz = RTP_JTBUF_PKT_BUFSZ_LOCAL;

  if(IS_CAPTURE_FILTER_TRANSPORT_RTP(pCfg->pcommon->filt.filters[0].transType)) {

    for(idx = 0; idx < pCfg->pcommon->filt.numFilters; idx++) {

      //TODO: need better way to dynamically set jitter buffer dimensions
      if(IS_CAP_FILTER_AUD(pCfg->pcommon->filt.filters[idx].mediaType)) {

        if(pCfg->pcommon->cfgMaxAudRtpGapWaitTmMs > CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS) {
          jtBufSzInPktsAud = MIN(100, (pCfg->pcommon->cfgMaxAudRtpGapWaitTmMs - 
                                       CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS) / 20 + CAPTURE_RTP_JTBUF_SZ_PKTS);
          jtBufPktBufSz = 2048;
        } else {
          jtBufSzInPktsAud = CAPTURE_RTP_JTBUF_SZ_PKTS;
        }
      } else {
        if(pCfg->pcommon->cfgMaxVidRtpGapWaitTmMs > CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS) {
          jtBufSzInPktsVid = MIN(100, (pCfg->pcommon->cfgMaxVidRtpGapWaitTmMs - 
                                       CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS) / 20 + CAPTURE_RTP_JTBUF_SZ_PKTS);
          jtBufPktBufSz = 2048;
        } else {
          jtBufSzInPktsVid = CAPTURE_RTP_JTBUF_SZ_PKTS;
        }
      }
     
    }

  }

  if(IS_CAPTURE_FILTER_TRANSPORT_RTP(pCfg->pcommon->filt.filters[0].transType)) {
    rtcp = 1;
  } else if(pCfg->pcommon->frtcp_rr_intervalsec > 0) {
    pCfg->pcommon->frtcp_rr_intervalsec = 0;
  }

  if(IS_CAPTURE_FILTER_TRANSPORT_RTP(pCfg->pcommon->filt.filters[0].transType)) {

    if(pCfg->pcommon->rtp_frame_drop_policy != CAPTURE_FRAME_DROP_POLICY_DEFAULT) {
      LOG(X_DEBUG("RTP input frame drop policy set to %s"), 
      pCfg->pcommon->rtp_frame_drop_policy == CAPTURE_FRAME_DROP_POLICY_DROPWANYLOSS ?
      "aggressive" : pCfg->pcommon->rtp_frame_drop_policy == CAPTURE_FRAME_DROP_POLICY_NODROP ?
      "none" : "default");
    }
  }

  // TODO, unify rtp jtbuf params between sock capture & pcap
  if((pState = rtp_captureCreate(CAPTURE_DB_MAX_STREAMS_LOCAL, 
                                 jtBufSzInPktsVid, 
                                 jtBufSzInPktsAud, 
                                 jtBufPktBufSz,
                                 pCfg->pcommon->cfgMaxVidRtpGapWaitTmMs,
                                 pCfg->pcommon->cfgMaxAudRtpGapWaitTmMs)) == NULL) {
    capture_deleteCbData(&streamCbData);
    return -1;
  }

  if(pCfg->pcommon->filt.numFilters > 0) {
    pState->pCbUserData = &streamCbData;
    pState->cbOnStreamAdd = capture_cbOnStreamAdd;

    if((pState->filt.numFilters = pCfg->pcommon->filt.numFilters) > CAPTURE_MAX_FILTERS) {
      pState->filt.numFilters = CAPTURE_MAX_FILTERS;
    }

    if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->delayed_output > 0) {
      //TODO: Not sure why we REALLY need a copy of the filters... but since w/ delayed_output
      // the SRTP, STUN config can change, we need to updated it in pState->filters later
      update_delayed_output = 1;
    }

    memcpy(pState->filt.filters, pCfg->pcommon->filt.filters, pState->filt.numFilters * sizeof(CAPTURE_FILTER_T));
  }

/*
  //TODO: check if SDP is recvonly/sendonly
  int idx;
  for(idx = 0; idx < pState->numFilters; idx++) {
    if(IS_CAP_FILTER_VID(pState->filters[idx].mediaType)) {
      fprintf(stderr, "VID XMITTYPE:%d\n", pState->filters[idx].u_seqhdrs.vid.common.xmitType);
    } else if(IS_CAP_FILTER_AUD(pState->filters[idx].mediaType)) {
      fprintf(stderr, "AUD XMITTYPE:%d\n", pState->filters[idx].u_seqhdrs.aud.common.xmitType);
    }
fprintf(stderr, " telephone-evt.codec:%d, avail:%d\n", pState->filters[idx].telephoneEvt.codecType,  pState->filters[idx].telephoneEvt.available);
  }
*/


//fprintf(stderr, "sockets cmd:%d 0x%x queue:%d numf:%d queue:0x%x \n", pState->filters[0].pCapAction->cmd,&pState->filters[0], pState->filters[0].pCapAction->pktQueueLen, pState->numFilters, pState->filters[0].pCapAction->pQueue);
//fprintf(stderr, "sockets cmd:%d 0x%x queue:%d numf:%d\n", pState->filters[1].pCapAction->cmd,&pState->filters[1], pState->filters[1].pCapAction->pktQueueLen, pState->numFilters);

  if(!(pCfg->pcommon->rtsp.issetup && (pCfg->pcommon->rtsp.s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED))) {
    if(capture_openLocalSockets(pCfg, &pState->filt, rtcp, dtlsKeyUpdateCbDatas) < 0) {
      LOG(X_ERROR("Failed to open listener socket(s)"));
      rtp_captureFree(pState);
      capture_deleteCbData(&streamCbData);
      return -1;
    }
  }

  //
  // Signal to the streamer thread that socket initialization has been completed
  // This may be required if the streamer will be waiting for socket DTLS handshakes, 
  // which it may now begin to wait for on sockets with a valid file descriptor
  //
  if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->sharedCtxt.state == STREAMER_SHARED_STATE_PENDING_CREATE) {
    pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    //LOG(X_DEBUG("Set state.STREAMER_SHARED_STATE_SOCKETS_CREATED"));
    pCfg->pStreamerCfg->sharedCtxt.state = STREAMER_SHARED_STATE_SOCKETS_CREATED;
    pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  rc = 0;

  // 
  // If this is an RTSP client capture session, which was already setup,
  // issue an RTSP play since the sockets are now bound and listening
  //
  if(pCfg->pcommon->rtsp.ctxtmode == RTSP_CTXT_MODE_CLIENT_CAPTURE && 
     pCfg->pcommon->rtsp.issetup && !pCfg->pcommon->rtsp.isplaying && 
    (rc = capture_rtsp_play(&pCfg->pcommon->rtsp)) < 0) {
    LOG(X_ERROR("Failed to play RTSP session %s"), pCfg->pcommon->rtsp.s.sessionid);
  }

  //
  // The sharedCtxt.pRtcpNotifyCtxt is used by stream_rtp RTCP handler to notify the capture
  // thread of incoming RTCP messages (RR, BYE), when the local RTP output srcport is the
  // same as the capture RTCP port(s)
  //
  if(pCfg->pStreamerCfg && (pCfg->pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort ||
                            (pCfg->pcommon->rtsp.s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED))) {
    rtcpNotifyCtxt.pState = pState;
    rtcpNotifyCtxt.pSockList = pCfg->pSockList;
    rtcpNotifyCtxt.pStreamerCfg = pCfg->pStreamerCfg;
    pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    pCfg->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt = &rtcpNotifyCtxt;
    pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  if(rc >= 0) {
    if(pCfg->pcommon->rtsp.issetup && (pCfg->pcommon->rtsp.s.sessionType & RTSP_SESSION_TYPE_INTERLEAVED)) {

      if(pCfg->pcommon->rtsp.ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE) {
        rc = vsxlib_cond_waitwhile(&pCfg->pcommon->rtsp.cond, &pCfg->running, STREAMER_STATE_RUNNING);
      } else {
        rc = rtsp_run_interleavedclient(&pCfg->pcommon->rtsp, pCfg, NULL);
      }

    } else {
      rc = readLocalSockets(pCfg, pState, rtcp, update_delayed_output);
    }
  }

  //
  // Sleep a bit when closing an RTSP server capture session
  //
  if(pCfg->pcommon->rtsp.ctxtmode == RTSP_CTXT_MODE_SERVER_CAPTURE) {
    usleep(100000);
  }

  LOG(X_DEBUG("Capture local socket(s) ended rc:%d (%d)"), rc, pCfg->running);

  if(pCfg->pStreamerCfg) {
    pthread_mutex_lock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    pCfg->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt = NULL;
    pCfg->pStreamerCfg->sharedCtxt.av[0].pdtls = NULL;
    pCfg->pStreamerCfg->sharedCtxt.av[1].pdtls = NULL;
    pthread_mutex_unlock(&pCfg->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  rtp_captureFree(pState);
  capture_closeLocalSockets(pCfg, &pState->filt);
  capture_deleteCbData(&streamCbData);

  return rc;
}


#endif // VSX_HAVE_CAPTURE

