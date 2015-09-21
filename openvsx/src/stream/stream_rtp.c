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

#define RTCP_SDES_TXT   VSX_APPNAME

#define RTP_SET_IP4_LEN(pRtp) (pRtp)->packet.u_ip.pip4->ip_len =  \
                        htons((unsigned short) (pRtp)->payloadLen + 20 + (((pRtp)->packet.u_ip.pip4->ip_hl) * 4))

#define RTP_SET_UDP_LEN(pRtp) (pRtp)->packet.pudp->len =  \
                        htons((unsigned short) (pRtp)->payloadLen + 20)

static int stream_rtcp_responder_start(STREAM_RTP_MULTI_T *pRtp, int lock);
static int stream_rtcp_responder_stop(STREAM_RTP_MULTI_T *pRtp);
static int stream_rtcp_createbye(STREAM_RTP_MULTI_T *pRtp, unsigned int idxDest,
                                 unsigned char *pBuf, unsigned int lenbuf);


void rtcp_init_sdes(RTCP_PKT_SDES_T *pSdes, uint32_t ssrc) {

  /*
  RFC 3550
  6.5 SDES: Source Description RTCP Packet

          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  header |V=2|P|    SC   |  PT=SDES=202  |             length            |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  chunk  |                          SSRC/CSRC_1                          |
    1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                           SDES items                          |
         |                              ...                              |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  chunk  |                          SSRC/CSRC_2                          |
    2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                           SDES items                          |
         |                              ...                              |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

  6.5.1 CNAME: Canonical End-Point Identifier SDES Item

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |    CNAME=1    |     length    | user and domain name        ...
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  */

  pSdes->hdr.hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pSdes->hdr.hdr |= 0 & 0x40; // padding 0
  pSdes->hdr.hdr |= 1 & 0x3f; // source count 1
  pSdes->hdr.pt = RTCP_PT_SDES;
  pSdes->hdr.ssrc = ssrc;
  pSdes->item.type = RTCP_SDES_CNAME; 
  snprintf((char *) pSdes->item.txt, sizeof(pSdes->item.txt) - 4, RTCP_SDES_TXT);
  pSdes->item.len = (uint8_t) strlen((char *) pSdes->item.txt);
  if(pSdes->item.len % 4 != 2) {
    pSdes->item.len += (4 - ((pSdes->item.len + 2) % 4));
  }
  pSdes->hdr.len = htons((pSdes->item.len + 6) / 4);   // in 4 byte words

}

static int stream_rtcp_reset(STREAM_RTCP_SR_T *pRtcp, uint32_t ssrc) {

  //
  // Build RTCP SR packet static values
  //
  pRtcp->sr.hdr.hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pRtcp->sr.hdr.pt = RTCP_PT_SR;
  pRtcp->sr.hdr.len = htons(6);   // in 4 byte words
  pRtcp->sr.hdr.ssrc = ssrc;

  rtcp_init_sdes(&pRtcp->sdes, pRtcp->sr.hdr.ssrc);

  return 0;
}

static int stream_rtp_reset(STREAM_RTP_MULTI_T *pRtp, int is_dtls) {
  static int g_warnmtu;
  //
  // build the rtp header
  //
  pRtp->pRtp = (struct rtphdr *) ((unsigned char *) pRtp->packet.pudp + UDP_HEADER_LEN);
  pRtp->pRtp->header = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pRtp->pRtp->pt = pRtp->init.pt & RTP_PT_MASK;
  pRtp->pRtp->sequence_num = htons(pRtp->init.seqStart-1); 
  if(pRtp->init.timeStampStart.haveTimeStamp) {
    pRtp->pRtp->timeStamp = htonl(pRtp->init.timeStampStart.timeStamp);
  } else {
    //pRtp->pRtp->timeStamp = htonl(0xffffffff - pRtp->init.frameDeltaHz + 1);
    pRtp->pRtp->timeStamp = 0;
  }
  pRtp->pRtp->ssrc = htonl(pRtp->init.ssrc);
  pRtp->pPayload = ((unsigned char *) pRtp->pRtp) + RTP_HEADER_LEN;
  pRtp->payloadLen = 0u;
  pRtp->tsStartXmit = 0;

  if(pRtp->init.maxPayloadSz == 0) {
    pRtp->init.maxPayloadSz = STREAM_RTP_MTU_DEFAULT;
    if(!g_warnmtu) {
      LOG(X_DEBUG("Using default payload MTU %u"), pRtp->init.maxPayloadSz);
      g_warnmtu = 1;
    }
    if(is_dtls && pRtp->init.maxPayloadSz > DTLS_OVERHEAD_SIZE) {
      pRtp->init.maxPayloadSz -= DTLS_OVERHEAD_SIZE;
    }
  } else {
    if(pRtp->init.maxPayloadSz > sizeof(pRtp->packet.data) -  
                 ( (char *)pRtp->pPayload - (char *)pRtp->packet.peth)) {
      pRtp->init.maxPayloadSz = sizeof(pRtp->packet.data) -  
                 ( (char *)pRtp->pPayload - (char *)pRtp->packet.peth);
      LOG(X_WARNING("Set payload MTU to %u"), pRtp->init.maxPayloadSz);
    } else {
      LOG(X_DEBUG("Setting payload MTU to %u"), pRtp->init.maxPayloadSz);
    }
  }

  pRtp->pktCnt = 0;
  pRtp->tmStartXmit = 0;

  return 0;
}

int stream_rtp_init(STREAM_RTP_MULTI_T *pRtp, const STREAM_RTP_INIT_T *pInit, int is_dtls) {
  int rc = 0;
  unsigned int idx;

  if(!pRtp || !pInit) {
    return -1;
  }

  if(pRtp->init.raw.haveRaw) {

    //
    // Ensure that we were configured with a non-default network interface
    //
    if(pktgen_GetInterface()[0] == '\0') {
      LOG(X_ERROR("No network interface specified for raw net output"));
      return -1;
    }

    if((rc = pktgen_InitUdpPacketIpv4(&pRtp->packet,
                                  pktgen_GetLocalMac(),
                                  (const unsigned char *) "\0\0\0\0\0\0",
                                  pRtp->init.raw.haveVlan,
                                  pRtp->init.raw.vlan,
                                  pRtp->init.raw.tos,
                                  pktgen_GetLocalIp(),
                                  INADDR_NONE,
                                  0,
                                  0,
                                  RTP_HEADER_LEN)) != 0) {
      return -1;
    }

  } else {

#if defined(VSX_HAVE_IPV6) && (VSX_HAVE_IPV > 0) 
  #define IP_HDR_SIZE IPV4_HDR_SIZE
#else
  #define IP_HDR_SIZE IPV6_HDR_SIZE
#endif // (VSX_HAVE_IPV6) 

    memset(&pRtp->packet, 0, sizeof(pRtp->packet));
    pRtp->packet.peth = (struct ether_header *) &pRtp->packet.data;
    pRtp->packet.u_ip.pip4 =  (struct ip *) ((unsigned char *)pRtp->packet.peth  + 14);
    pRtp->packet.pudp = (struct udphdr *) ((unsigned char *)pRtp->packet.u_ip.pip4  + IP_HDR_SIZE);

  }

  memcpy(&pRtp->init, pInit, sizeof(pRtp->init));
  pRtp->numDests = 0;

  for(idx = 0; idx < pRtp->maxDests; idx++) {
    pRtp->pdests[idx].isactive = 0;
    memset(&pRtp->pdests[idx].saDsts, 0, sizeof(pRtp->pdests[idx].saDsts));
    memset(&pRtp->pdests[idx].saDstsRtcp, 0, sizeof(pRtp->pdests[idx].saDstsRtcp));
    NETIOSOCK_FD(pRtp->pdests[idx].xmit.netsock) = INVALID_SOCKET;
    NETIOSOCK_FD(pRtp->pdests[idx].xmit.netsockRtcp) = INVALID_SOCKET;
    pRtp->pdests[idx].sendrtcpsr = pInit->sendrtcpsr;
    pRtp->pdests[idx].tmLastRtcpSr = 0;
  }

  if((rc = stream_rtp_reset(pRtp, is_dtls)) < 0) {
    return rc;
  }

  for(idx = 0; idx < pRtp->maxDests; idx++) {
    stream_rtcp_reset(&pRtp->pdests[idx].rtcp, pRtp->pRtp->ssrc);
  }

  pthread_mutex_init(&pRtp->mtx, NULL);
  pRtp->isinit = 1;

  return rc;
}

static int get_dest_addr(const STREAM_DEST_CFG_T *pDestCfg, struct sockaddr_storage *pDst) {
  int rc = -1;

  if(pDst &&  pDestCfg->dstPort > 0) {

    memset(pDst, 0, sizeof(struct sockaddr_storage));

    if(pDestCfg->haveDstAddr) {

      memcpy(pDst, &pDestCfg->u.dstAddr, sizeof(struct sockaddr_storage));
      PINET_PORT(pDst) = htons(pDestCfg->dstPort);
      rc = 0;

    } else if(pDestCfg->u.pDstHost) {

      if((rc = net_resolvehost(pDestCfg->u.pDstHost, pDst)) == 0) {
        PINET_PORT(pDst) = htons(pDestCfg->dstPort);
      }
    }
  }
  return rc;
}

int stream_rtp_updatedest(STREAM_RTP_DEST_T *pDest, const STREAM_DEST_CFG_T *pDestCfg) {
  int rc = 0;
  int updatedRtp = 0;
  int rtcpOffset = 0;
  struct sockaddr_storage dstAddr;
  int16_t rtcpPort;
  char tmp[128];
  char tmp2[128];
  char descrRtp[128];
  char descrRtcp[128];
  STREAM_RTP_MULTI_T *pRtp;

  memset(&dstAddr, 0, sizeof(dstAddr));
  descrRtp[0] = descrRtcp[0] = '\0';

  if(!pDest || !pDestCfg || !(pRtp = pDest->pRtpMulti) || pDestCfg->noxmit || pRtp->init.raw.haveRaw) {
    return -1;
  } else if(pDestCfg->dstPort > 0 && get_dest_addr(pDestCfg, &dstAddr) != 0) {
    return -1;
  }

  pthread_mutex_lock(&pRtp->mtx);

  if(!pDest->isactive) {
    pthread_mutex_unlock(&pRtp->mtx);
    return 0;
  }

  LOG(X_DEBUG("RTP update destination %s:%d"), INET_NTOP(dstAddr, tmp, sizeof(tmp)), htons(INET_PORT(dstAddr)));

  //
  // Update output RTP destination and port if it has changed
  //
  if(!(INET_IS_SAMEADDR(pDest->saDsts, dstAddr) && INET_PORT(pDest->saDsts) == INET_PORT(dstAddr))) { 

    pthread_mutex_lock(&STREAM_RTP_PSTUNSOCK(*pDest)->mtxXmit);

    snprintf(tmp, sizeof(tmp), "%s:%d", FORMAT_NETADDR(pDest->saDsts, tmp, sizeof(tmp)),
                                        htons(INET_PORT(pDest->saDsts)));
    snprintf(descrRtp, sizeof(descrRtp), "from %s to %s:%d", tmp, FORMAT_NETADDR(dstAddr, tmp2, sizeof(tmp2)), 
                                        pDestCfg->dstPort);

    rtcpOffset = htons(INET_PORT(pDest->saDstsRtcp)) - htons(INET_PORT(pDest->saDsts));

    memcpy(&pDest->saDsts, &dstAddr, sizeof(pDest->saDsts));

    pthread_mutex_unlock(&STREAM_RTP_PSTUNSOCK(*pDest)->mtxXmit);
    updatedRtp = 1;
  }

  //
  // Update output RTCP port
  //
  if(INET_PORT(pDest->saDstsRtcp) != 0 && (updatedRtp || pDestCfg->dstPortRtcp > 0)) {

    if(pDestCfg->dstPortRtcp > 0) {
      rtcpPort = pDestCfg->dstPortRtcp;
    } else {
      rtcpPort = htons(INET_PORT(pDest->saDsts)) + rtcpOffset;
    }

    snprintf(tmp, sizeof(tmp), "%s:%d", FORMAT_NETADDR(pDest->saDstsRtcp, tmp, sizeof(tmp)), 
                                        htons(INET_PORT(pDest->saDstsRtcp)));
    snprintf(descrRtcp, sizeof(descrRtcp), "RTCP from %s to %s:%d", tmp, 
                                     FORMAT_NETADDR(pDest->saDsts, tmp2, sizeof(tmp2)), rtcpPort);

    pthread_mutex_lock(&STREAM_RTCP_PSTUNSOCK(*pDest)->mtxXmit);

    memcpy(&pDest->saDstsRtcp, &pDest->saDsts, sizeof(pDest->saDstsRtcp));
    INET_PORT(pDest->saDstsRtcp) = htons(rtcpPort);

    pthread_mutex_unlock(&STREAM_RTCP_PSTUNSOCK(*pDest)->mtxXmit);
  }

  pthread_mutex_unlock(&pRtp->mtx);

  if(descrRtp[0] != '\0' || descrRtcp[0] != '\0') {
    LOG(X_INFO("Updated RTP pt: %d, ssrc: 0x%x destination %s %s"), 
               pRtp->init.pt, pRtp->init.ssrc, descrRtp, descrRtcp);
  }

  return rc;
}

#if defined(VSX_HAVE_SSL_DTLS)

static int init_dtls_srtp(STREAM_RTP_DEST_T *pDest, const char *srtpProfileName, int dtls_serverkey, int is_rtcp, 
                          const unsigned char *pKeys, unsigned int lenKeys, int do_rtcp) {
  int rc = 0;
  SRTP_CTXT_T *pSrtp = NULL;

  if(!pDest || !pKeys || !srtpProfileName || !(pSrtp = &pDest->srtps[is_rtcp ? 1 : 0])) {
    return -1;
  }

  if(srtp_streamType(srtpProfileName, &pSrtp->kt.type.authType, &pSrtp->kt.type.confType) < 0) {
    LOG(X_ERROR("Unknown DTLS-SRTP %s profile name '%s'"), is_rtcp ? "RTCP" : "RTP", srtpProfileName);
    rc = -1;
  }

  if(rc >= 0) {
    pSrtp->kt.type.authTypeRtcp = pSrtp->kt.type.authType;
    pSrtp->kt.type.confTypeRtcp = pSrtp->kt.type.confType;


    if(srtp_setDtlsKeys(pSrtp, pKeys, lenKeys, !dtls_serverkey, is_rtcp) < 0) {
      LOG(X_ERROR("Failed to update DTLS-SRTP %s key from DTLS handshake"), is_rtcp ? "RTCP" : "RTP");
      rc = -1;
    }
  }

  //
  // Set SRTP configuration
  //
  if(rc >= 0 && srtp_initOutputStream(pSrtp, htonl(pDest->pRtpMulti->init.ssrc), do_rtcp) < 0) {
    rc = -1;
  }

  return rc;
}

int stream_dtls_netsock_key_update(void *pCbData, NETIO_SOCK_T *pnetsock, const char *srtpProfileName, 
                                   int dtls_serverkey, int is_rtcp, const unsigned char *pKeys, unsigned int lenKeys) {
  int rc = 0;
  STREAM_RTP_DEST_T *pDest = (STREAM_RTP_DEST_T *) pCbData;
  const struct sockaddr_storage *psaDst;
  //NETIO_SOCK_T *pnetsockPeer = NULL;
  SRTP_CTXT_T *pSrtp = NULL;
  char tmp[128];
  int do_rtcp = 0;
 
  if(!pDest || !pKeys || !srtpProfileName || !(pSrtp = &pDest->srtps[is_rtcp ? 1 : 0])) {
    return -1;
  }

  psaDst = is_rtcp ? &pDest->saDstsRtcp : &pDest->saDsts;

  if(pDest->sendrtcpsr && STREAM_RTCP_FD(*pDest) != INVALID_SOCKET) {
    do_rtcp = 1;
  }

  //LOG(X_DEBUG("STREAM_DTLS_NETSOCK_KEY_UPDATE handshake profile: '%s', dtls_serverkey:%d, is_rtcp:%d, do_rtcp:%d, pDest: 0x%x, fd:%d, pDest->pDestPeer: 0x%x, fd:%d, pSrtp:0x%x master-keys: "), srtpProfileName, dtls_serverkey, is_rtcp, do_rtcp, pDest, STREAM_RTP_FD(*pDest), pDest->pDestPeer, pDest->pDestPeer ? STREAM_RTP_FD(*pDest->pDestPeer) : -1, &pDest->srtps[is_rtcp ? 1 : 0]); LOGHEX_DEBUG(pKeys, lenKeys);

  rc = init_dtls_srtp(pDest, srtpProfileName, dtls_serverkey, is_rtcp, pKeys, lenKeys, do_rtcp);

  //if(rc >= 0 && pDest->pDestPeer && pDest->pDestPeer != pDest && STREAM_RTP_FD(*pDest->pDestPeer) == INVALID_SOCKET) {
  if(rc >= 0 && pDest->pDestPeer && pDest->pDestPeer != pDest && NETIOSOCK_FD(pDest->pDestPeer->xmit.netsock) == INVALID_SOCKET) {
    //LOG(X_DEBUG("STREAM_DTLS_NETSOCK_KEY_UPDATE on aud/vid mux dest peer"));
    rc = init_dtls_srtp(pDest->pDestPeer, srtpProfileName, dtls_serverkey, is_rtcp, pKeys, lenKeys, do_rtcp);
    //pnetsockPeer = STREAM_RTCP_PNETIOSOCK(*pDest->pDestPeer);
  }

  //LOG(X_DEBUG("STREAM_DTLS_NETSOCK_KEY_UPDATE handshake profile: '%s', master-keys: "), srtpProfileName); LOGHEX_DEBUG(pKeys, lenKeys);

  if(rc >= 0) {
    if(pnetsock) {
      pnetsock->flags |= NETIO_FLAG_SSL_DTLS_SRTP_OUTKEY;
    }
    //if(pnetsockPeer) {
    //  pnetsockPeer->flags |= NETIO_FLAG_SSL_DTLS_SRTP_OUTKEY;
    //}
    pSrtp->do_rtcp_responder = do_rtcp;
    LOG(X_DEBUG("Initialized DTLS-SRTP %s %s %d byte output key from DTLS handshake to %s:%d"),
         is_rtcp ? "RTCP" : "RTP", srtpProfileName, pSrtp->kt.k.lenKey, 
         FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)));
    //LOGHEX_DEBUG(pSrtp->k.key, pSrtp->k.lenKey);

  } else {
    LOG(X_ERROR("Failed to initialize DTLS-SRTP %s %s output key from DTLS handshake to %s:%d"),
         is_rtcp ? "RTCP" : "RTP", srtpProfileName, 
         FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)));
  }

  return rc;
}

static int stream_rtp_init_dtls_socket(STREAM_RTP_DEST_T *pDest, const STREAM_DEST_CFG_T *pDestCfg, 
                                       int is_rtcp, int is_audvidmux) {
  STREAM_DTLS_CTXT_T *pDtlsCtxt = NULL;
  STREAM_RTP_MULTI_T *pRtp = NULL;
  NETIO_SOCK_T *pnetsock = NULL;
  struct sockaddr_storage *psaDst = NULL;
  DTLS_KEY_UPDATE_CTXT_T dtlsKeysUpdateCtxt;
  char tmp[128];
  int do_rtcp = 0;
  int rc = 0; 
  int dtls_handshake_client = 1;

  if(!(pRtp = pDest->pRtpMulti)) {
    return -1;
  } else if(!(pDtlsCtxt = pRtp->dtls.pDtlsShared)) {
    LOG(X_ERROR("DTLS output context not initialized"));
    return -1;
  }

  dtls_handshake_client = pDestCfg->dtlsCfg.dtls_handshake_server == -1 ? 1 : !pDestCfg->dtlsCfg.dtls_handshake_server;

  //
  // if useSockFromCapture is set we still create an output SSL/DTLS context independent from the
  // capture SSL/DTLS context, but the two will share the same socket for socket write/read
  //
  //if(useSockFromCapture) {
  //  if((is_rtcp && NETIOSOCK_FD(pDest->xmit.netsockRtcp) != INVALID_SOCKET) ||
  //     (!is_rtcp && NETIOSOCK_FD(pDest->xmit.netsock) != INVALID_SOCKET)) {
  //    useSockFromCapture = 0;
  //  }
  //}

  if(pDest->sendrtcpsr && STREAM_RTCP_FD(*pDest) != INVALID_SOCKET) {
    do_rtcp = 1;
  }

  if(is_rtcp) {
    pnetsock = &pDest->xmit.netsockRtcp,
    psaDst =  &pDest->saDsts;
  } else {
    pnetsock = &pDest->xmit.netsock,
    psaDst =  &pDest->saDstsRtcp;
  }

  if(INET_IS_MULTICAST(*psaDst)) {
    LOG(X_ERROR("Cannot output DTLS to multicast %s:%d"), 
                FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)));
    return -1;
  }

  memset(&dtlsKeysUpdateCtxt, 0, sizeof(dtlsKeysUpdateCtxt));

  //LOG(X_DEBUG("INIT_DTLS_SOCKET is_rtcp:%d, netsock.fd:%d, fd:%d, is_audvidmux:%d"), is_rtcp, NETIOSOCK_FD(*pnetsock), STREAM_RTP_FD(*pDest), is_audvidmux);

  if(pDtlsCtxt->dtls_srtp != pDestCfg->dtlsCfg.dtls_srtp) {
    LOG(X_ERROR("Cannot mix DTLS and DTLS-SRTP output %s socket to %s:%d for the same DTLS context"),
                  is_rtcp ? "RTCP" : "RTP", FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)));
    return -1;
  }

  if(NETIOSOCK_FD(*pnetsock) != INVALID_SOCKET) {

    if(pDestCfg->dtlsCfg.dtls_srtp) {
      dtlsKeysUpdateCtxt.pCbData = pDest;
      dtlsKeysUpdateCtxt.cbKeyUpdateFunc = stream_dtls_netsock_key_update;
      dtlsKeysUpdateCtxt.is_rtcp = is_rtcp;
      //dtlsKeysUpdateCtxt.dtls_serverkey = pDestCfg->dtlsCfg.dtls_serverkey;
      dtlsKeysUpdateCtxt.dtls_serverkey = !pDestCfg->dtlsCfg.dtls_handshake_server;
    }

    if(dtls_netsock_init(pDtlsCtxt, pnetsock, NULL, &dtlsKeysUpdateCtxt) < 0 ||
       dtls_netsock_setclient(pnetsock, dtls_handshake_client) < 0) {

      LOG(X_ERROR("Failed to initialize DTLS%s output %s socket to %s:%d"),
                   pDtlsCtxt->dtls_srtp ? "-SRTP" : "", is_rtcp ? "RTCP" : "RTP",
                    FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)));
      rc = -1;

    } else {

      //LOG(X_DEBUG("STREAM-DTLS-MTU TEST sock: 0x%x, mtu:%d"), pnetsock, pRtp->init.maxPayloadSz);
      dtls_netsock_setmtu(pnetsock, pRtp->init.maxPayloadSz);

      LOG(X_ERROR("Initialized DTLS%s output %s socket to %s:%d"),
                  pDtlsCtxt->dtls_srtp ? "-SRTP" : "", is_rtcp ? "RTCP" : "RTP",
                  FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)));
    }

  }

  //
  // Set DTLS-SRTP pre-configuration (SRTP key material will be set in the stream_dtls_netsock_key_update callback)
  //
  if(rc >= 0 && pDestCfg->dtlsCfg.dtls_srtp) {

      //if(STREAM_RTP_PNETIOSOCK(*pDest)->flags & jj if(!is_audvidmux && !pDestCfg->useSockFromCapture && rc == 0 && pRtp->init.dscpRtp != 0) {


    memcpy(&pDest->srtps[is_rtcp ? 1 : 0], &pDestCfg->srtp, sizeof(pDest->srtps[0]));
    pDest->srtps[is_rtcp ? 1 : 0].pSrtpShared = &pDest->srtps[is_rtcp ? 1 : 0];
    pDest->srtps[is_rtcp ? 1 : 0].is_rtcp = is_rtcp;

    if(is_rtcp) {
      if(INET_PORT(pDest->saDstsRtcp) != INET_PORT(pDest->saDsts)) {

        if(is_audvidmux) {
          pDest->srtps[1].pSrtpShared = &pDestCfg->pDestPeer->srtps[1]; 
        } else {
          pDest->srtps[1].pSrtpShared = &pDest->srtps[1]; // Unique RTCP context and key
        }

      } else if(is_audvidmux) {
        pDest->srtps[1].pSrtpShared = &pDestCfg->pDestPeer->srtps[0]; 
      } else {
        pDest->srtps[1].pSrtpShared = &pDest->srtps[0]; 
      }

    } else {
      if(is_audvidmux) {
        pDest->srtps[0].pSrtpShared = &pDestCfg->pDestPeer->srtps[0];
      } else {
        //pDest->srtps[0].pSrtpShared = &pDest->srtps[0];
      } 

    }

    if(pDestCfg->useSockFromCapture && pDestCfg->psharedCtxt) {
      pDest->srtps[is_rtcp ? 1 : 0].do_rtcp_responder = do_rtcp;
      pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
      pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psrtps[0] = pDest->srtps[0].pSrtpShared;
      pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psrtps[1] = pDest->srtps[1].pSrtpShared;
      pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    }

  }

  //
  // If we're using shared capture/output socket(s) then the DTLS-SRTP key_update callback will be invoked from
  // the capture thread and we have to set our callback-data-object here
  //
  if(rc >= 0 && pDestCfg->useSockFromCapture && pDestCfg->psharedCtxt) {
     pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
     pDestCfg->psharedCtxt->dtlsKeyUpdateStorages[is_rtcp ? 1 : 0].pCbData = pDest;
     // readyForCb will be set from streamer_rtp after both audio + video streams have been added
     pDestCfg->psharedCtxt->dtlsKeyUpdateStorages[is_rtcp ? 1 : 0].readyForCb = 0;
     pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  return rc;
}
#endif // (VSX_HAVE_SSL_DTLS)

#define DESTCFG_USERTPBINDPORT(pDestCfg)  ((pDestCfg)->useSockFromCapture && (pDestCfg)->psharedCtxt)

STREAM_RTP_DEST_T *stream_rtp_adddest(STREAM_RTP_MULTI_T *pRtp, const STREAM_DEST_CFG_T *pDestCfg) {

  STREAM_RTP_DEST_T *pDest = NULL;
  struct sockaddr_storage dstAddr;
  struct sockaddr_storage sa;
  struct sockaddr *psa;
  char tmp[128];
  unsigned int idxStream = 0;
  int is_audvidmux = 0;
  int do_rtcp = 0;
  int rc = 0;
  TURN_RELAY_SETUP_RESULT_T onResult;

  if(!pRtp || !pDestCfg) {
    return NULL;
  }

  pthread_mutex_lock(&pRtp->mtx);

  if(pRtp->numDests >= pRtp->maxDests) {
    rc = -1;
  }

  if(rc == 0) {
    for(idxStream = 0; idxStream < pRtp->maxDests; idxStream++) {
      if(!pRtp->pdests[idxStream].isactive) {
        pDest = &pRtp->pdests[idxStream];
        pDest->pRtpMulti = pRtp;
        break;
      }
    }

    if(!pDest) {
      rc = -1;
    }
  }

  VSX_DEBUG_RTP( LOG(X_DEBUG("RTP - stream_add_dest rc:%d, idxStream[%d] numDests:%d/%d, pRtp:0x%x, "
                             "pDest: 0x%x, port:%d, noxmit: %d"), rc, idxStream, pRtp->numDests, 
                             pRtp->maxDests, pRtp, pDest, pDestCfg->dstPort, pDestCfg->noxmit) );

  memset(&dstAddr, 0, sizeof(dstAddr));
  if(rc == 0 && !pDestCfg->noxmit && get_dest_addr(pDestCfg, &dstAddr) != 0) {
    rc = -1;
  } 

  if(rc >= 0) {
    pDest->noxmit = pDestCfg->noxmit;
  }

  if(pRtp->init.raw.haveRaw) {

    if(pDestCfg->noxmit) {
      rc = -1;
    } else if(rc == 0 && pRtp->numDests >= 1) {
      LOG(X_ERROR("Raw transmission not implemented for multiple destinations"));
      rc = -1;
    } else if(dstAddr.ss_family == AF_INET6) {
      LOG(X_ERROR("Raw transmission not implemented for IPv6"));
      rc = -1;
    }

#if defined(WIN32) && !defined(__MINGW32__)

    if(rc == 0) {
      if(maclookup_GetMac(dstAddr.sin_addr.s_addr, pRtp->init.raw.mac, pRtp->init.raw.vlan) != 0) {
        rc = -1;
      } else if(!memcmp(pRtp->init.raw.mac, (unsigned char *) "\0\0\0\0\0\0", 6) ||
                (pRtp->init.raw.mac[0] == 0xff && pRtp->init.raw.mac[1] == 0xff &&
                 pRtp->init.raw.mac[2] == 0xff && pRtp->init.raw.mac[3] == 0xff &&
                 pRtp->init.raw.mac[4] == 0xff && pRtp->init.raw.mac[5] == 0xff)) {
        LOG(X_ERROR("Invalid MAC address %x:%x:%x:%x:%x:%x"),
            pRtp->init.raw.mac[0], pRtp->init.raw.mac[1], pRtp->init.raw.mac[2],
            pRtp->init.raw.mac[3], pRtp->init.raw.mac[4], pRtp->init.raw.mac[5]);
        rc = -1;
      }
    }

#else // WIN32
    LOG(X_ERROR("Raw transmission not implemented"));
    rc = -1;
#endif // WIN32

    if(rc == 0) {
      if(pRtp->init.sendrtcpsr) {
        LOG(X_WARNING("RTCP not supported for raw transmission"));    
        pDest->sendrtcpsr = 0;
      }

      memcpy(pRtp->packet.peth->ether_dhost, pRtp->init.raw.mac, ETH_ALEN);

      if(dstAddr.ss_family == AF_INET6) {
        memcpy(&pRtp->packet.u_ip.pip6->ip6_dst.s6_addr[0], 
           &((struct sockaddr_in6 *) &dstAddr)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
      } else {
        pRtp->packet.u_ip.pip4->ip_dst.s_addr = ((struct sockaddr_in *) &dstAddr)->sin_addr.s_addr;
      }

      pRtp->packet.pudp->dest = htons(pDestCfg->dstPort);
      pRtp->packet.pudp->source = pRtp->packet.pudp->dest;
    }

  } else if(!pDestCfg->noxmit) { // haveRaw

    if(rc == 0 && !INET_ADDR_VALID(dstAddr)) {
      LOG(X_ERROR("Invalid destination address: %s:%d"), INET_NTOP(dstAddr, tmp, sizeof(tmp)),  pDestCfg->dstPort);
      rc = -1;
    }

    if(rc == 0) {

      memset(&pDest->saDsts, 0, sizeof(pDest->saDsts));
      pDest->saDsts.ss_family =  dstAddr.ss_family;
      INET_PORT(pDest->saDsts) = htons(pDestCfg->dstPort);
      if(dstAddr.ss_family == AF_INET6) {
        memcpy(&((struct sockaddr_in6 *) &pDest->saDsts)->sin6_addr.s6_addr[0], 
               &((struct sockaddr_in6 *) &dstAddr)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
      } else {
        ((struct sockaddr_in *) &pDest->saDsts)->sin_addr.s_addr = ((struct sockaddr_in *) &dstAddr)->sin_addr.s_addr;
      }

      //
      // Check if this destination is an audio-video mux peer of the previous destination
      // in which case we should reuse the output socket/srtp/dtls contexts
      //
      if(pDestCfg->pDestPeer && INET_PORT(pDestCfg->pDestPeer->saDsts) == htons(pDestCfg->dstPort) &&
         INET_IS_SAMEADDR(pDestCfg->pDestPeer->saDsts, dstAddr)) {
      //if(pDestCfg->pDestPeer && INET_PORT(pDestCfg->pDestPeer->saDsts) == htons(pDestCfg->dstPort) && 
      //   pDestCfg->pDestPeer->saDsts.sin_addr.s_addr == dstIp) {
        is_audvidmux = 1;
        LOG(X_DEBUG("RTP destination %s:%d is using audio video multiplexing"), 
                    FORMAT_NETADDR(pDest->saDsts, tmp, sizeof(tmp)), htons(INET_PORT(pDest->saDsts)));
      }

      psa = NULL;
      if(!pDestCfg->useSockFromCapture && pDestCfg->localPort > 0) {
        memset(&sa, 0, sizeof(sa));
        psa = (struct sockaddr *) &sa;
        if((psa->sa_family = dstAddr.ss_family) == AF_INET6) {
          memset(&((struct sockaddr_in6 *) psa)->sin6_addr.s6_addr[0], 0, ADDR_LEN_IPV6); // IN6ADDR_ANY
        } else {
          ((struct sockaddr_in *) psa)->sin_addr.s_addr = INADDR_ANY;
        }
        PINET_PORT(psa) = htons(pDestCfg->localPort);
        
      }

      pDest->xmit.pnetsock = NULL;
      pDest->xmit.pnetsockRtcp = NULL;
      if(is_audvidmux) {
        pDest->xmit.pnetsock = STREAM_RTP_PNETIOSOCK(*pDestCfg->pDestPeer);
      } else if(DESTCFG_USERTPBINDPORT(pDestCfg)) {
        pDest->xmit.pnetsock = pDestCfg->psharedCtxt->capSocket.pnetsock; 
      } else if((netio_opensocket(&pDest->xmit.netsock, SOCK_DGRAM, 0, SOCK_SNDBUFSZ_UDP_DEFAULT, psa)) == INVALID_SOCKET) {
        rc = -1;
      }

    //fprintf(stderr, "ADDDEST to 0x%x avidx:%d, isaud:%d, %s:%d\n", pDest, pRtp->avidx, pRtp->isaud, inet_ntoa(pDest->saDsts.sin_addr), htons(pDest->saDsts.sin_port));
      //fprintf(stderr,"ADDDEST avidx:%d RTP SOCK %d, psock:0x%x : %s:%d, useSockFromCapture:%d, psharedCtxt:0x%x\n",  pRtp->avidx, pDest->xmit.sock, pDest->xmit.psock, psain ? inet_ntoa(psain->sin_addr) : "NOT SET", psain ? htons(psain->sin_port) : 0, pDestCfg->useSockFromCapture, pDestCfg->psharedCtxt);

    }

    //TODO: should still set outbound QOS on socket created in capture localsockets
    if(!is_audvidmux && !pDestCfg->useSockFromCapture && rc == 0 && pRtp->init.dscpRtp != 0) {
      net_setqos(STREAM_RTP_FD(*pDest), (const struct sockaddr *) &pDest->saDsts, pRtp->init.dscpRtp);
      //net_setqos(NETIOSOCK_FD(pDest->xmit.netsock), &pDest->saDsts, pRtp->init.dscpRtp);
    }
    //fprintf(stderr, "RTP SOCK %d dest[%d] %s:%d, rc:%d\n", pDest->xmit.sock, idxStream, inet_ntoa(pDest->saDsts.sin_addr), htons(pDest->saDsts.sin_port), rc);

    if(rc == 0) {

      pDest->tmLastRtcpSr = 0;
      pDest->sendrtcpsr = 0;

      pDest->srtps[0].pSrtpShared = &pDest->srtps[0];
      pDest->srtps[1].pSrtpShared = &pDest->srtps[0]; // By default, SRTP RTCP context is the same as SRTP RTP 
      pDest->srtps[1].is_rtcp = 1;

      if(pDestCfg->dstPortRtcp > 0 && (pDest->sendrtcpsr = pRtp->init.sendrtcpsr)) {

        do_rtcp = 1;
        memcpy(&pDest->saDstsRtcp, &pDest->saDsts, sizeof(pDest->saDstsRtcp));
        INET_PORT(pDest->saDstsRtcp) = htons(pDestCfg->dstPortRtcp);

        if(is_audvidmux) {
          //
          // If we are doing audio/video multiplexing, then also use rtcp-mux
          //
          pDest->xmit.pnetsockRtcp = STREAM_RTCP_PNETIOSOCK(*pDestCfg->pDestPeer);
        } else if(DESTCFG_USERTPBINDPORT(pDestCfg)) {
          pDest->xmit.pnetsockRtcp = pDestCfg->psharedCtxt->capSocket.pnetsockRtcp; 
        } else if(INET_PORT(pDest->saDstsRtcp) != INET_PORT(pDest->saDsts)) {

          psa = NULL;
          if(pDestCfg->localPort > 0) {
            memset(&sa, 0, sizeof(sa));
            psa = (struct sockaddr *) &sa;
            if((psa->sa_family = dstAddr.ss_family) == AF_INET6) {
              memset(&((struct sockaddr_in6 *) psa)->sin6_addr.s6_addr[0], 0, ADDR_LEN_IPV6); // IN6ADDR_ANY
            } else {
              ((struct sockaddr_in *) psa)->sin_addr.s_addr = INADDR_ANY;
            }
            PINET_PORT(psa) = htons(RTCP_PORT_FROM_RTP(pDestCfg->localPort));
          }

          if(netio_opensocket(&pDest->xmit.netsockRtcp, SOCK_DGRAM, 0, 0, psa) == INVALID_SOCKET) {
            netio_closesocket(&pDest->xmit.netsock);
            rc = -1;
          }
          if(rc == 0 && (rc = net_setsocknonblock(NETIOSOCK_FD(pDest->xmit.netsockRtcp), 1)) < 0) {
            netio_closesocket(&pDest->xmit.netsock);
            netio_closesocket(&pDest->xmit.netsockRtcp);
          }

          if(rc == 0 && pRtp->init.dscpRtcp != 0) {

          }

        } // if(INET_PORT(pDest->saDstsRtcp) != INET_PORT(pDest->saDsts) ... 

      } // sendrtcpsr

#if defined(VSX_HAVE_SSL_DTLS)
      //
      // If we're using a shared socket, then the DTLS context should have been created
      // by capture_socket creation, otherwise created it here
      //
      if(rc >= 0 && pDestCfg->dtlsCfg.active) {

        if(is_audvidmux) {
          pRtp->dtls.pDtlsShared = pDestCfg->pDestPeer->pRtpMulti->dtls.pDtlsShared;
        } else if(DESTCFG_USERTPBINDPORT(pDestCfg)) {

          //
          // DTLS context will be init in capture_socket thread
          //
          //LOG(X_DEBUG("Using capture-shared DTLS context avidx[%d]"), pDest->pRtpMulti->avidx);
          pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
          if(!(pRtp->dtls.pDtlsShared = pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].pdtls) &&
             pDest->pRtpMulti->avidx > 0) {
            pRtp->dtls.pDtlsShared = pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[0].pdtls;
          }
          pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

        } else {

          //
          // Init one shared output DTLS context
          //
          if(pRtp->pheadall && pRtp->pheadall->dtls.active == 1) {
            //LOG(X_DEBUG("Using shared RTP DTLS output context 0x%x"), &pRtp->pheadall->dtls);
            pRtp->dtls.pDtlsShared = &pRtp->pheadall->dtls;
          } else if(pRtp->dtls.active == 0) {
            //LOG(X_DEBUG("Initializing RTP DTLS output %s context 0x%x"), pDestCfg->dtlsCfg.dtls_serverkey ?"server":"client", &pRtp->dtls);
            if(dtls_ctx_init(&pRtp->dtls, &pDestCfg->dtlsCfg, pDestCfg->dtlsCfg.dtls_srtp) < 0) {
              pRtp->dtls.pDtlsShared = NULL;
            } else {
              pRtp->dtls.pDtlsShared = &pRtp->dtls;
            }
          }
        }

        if(!pRtp->dtls.pDtlsShared) { 
          LOG(X_ERROR("Failed to acquire DTLS context avidx[%d]"), pDest->pRtpMulti->avidx);
          pRtp->dtls.active = -1;
          rc = -1;
        } else {
          //LOG(X_DEBUG("Acquired DTLS context avidx[%d]"), pDest->pRtpMulti->avidx);
        }

        //LOG(X_DEBUG("\n\nSTREAM_RTP usertpbindport dtls context:%d 0x%x 0x%x fd:%d"), DESTCFG_USERTPBINDPORT(pDestCfg), pRtp->dtls.pDtlsShared, &pRtp->dtls, NETIOSOCK_FD(pDest->xmit.netsock));
        //if(rc >= 0 && NETIOSOCK_FD(pDest->xmit.netsock) != INVALID_SOCKET) {
        if(rc >= 0) {
          rc = stream_rtp_init_dtls_socket(pDest, pDestCfg, 0, is_audvidmux);

          if(rc == 0 && NETIOSOCK_FD(pDest->xmit.netsock) != INVALID_SOCKET &&
             (rc = net_setsocknonblock(NETIOSOCK_FD(pDest->xmit.netsock), 1)) < 0) {
            //netio_closesocket(&pDest->xmit.netsock);
            //netio_closesocket(&pDest->xmit.netsockRtcp);
          }

        }
        if(rc >= 0 && do_rtcp) {
          rc = stream_rtp_init_dtls_socket(pDest, pDestCfg, 1, is_audvidmux);
        }

      }
#endif // (VSX_HAVE_SSL_DTLS)

      //
      // Set SRTP configuration
      //
      if(rc >= 0 && pDestCfg->srtp.kt.k.lenKey > 0) {

        memcpy(&pDest->srtps[0], &pDestCfg->srtp, sizeof(pDest->srtps[0]));
        pDest->srtps[0].pSrtpShared = &pDest->srtps[0];

        if(srtp_initOutputStream(&pDest->srtps[0], htonl(pRtp->init.ssrc), do_rtcp) < 0) {
          rc = -1;
        } else if(pDestCfg->useSockFromCapture && pDestCfg->psharedCtxt) {
          pDest->srtps[0].do_rtcp_responder = do_rtcp;
          pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
          pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psrtps[0] = &pDest->srtps[0];
          pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psrtps[1] = &pDest->srtps[1];
          pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
        }
      }

      if(rc >= 0) {
        rc = streamxmit_init(pDest, pDestCfg);
      }

    } // rc == 0

  } else if(rc == 0) { 
    //
    // pDestCfg->noxmit
    //

    pDest->tmLastRtcpSr = 0;
    pDest->sendrtcpsr = pRtp->init.sendrtcpsr;
    NETIOSOCK_FD(pDest->xmit.netsock) = INVALID_SOCKET;
    INET_PORT(pDest->saDsts) = htons(pDestCfg->dstPort);
    NETIOSOCK_FD(pDest->xmit.netsockRtcp) = INVALID_SOCKET;
    INET_PORT(pDest->saDstsRtcp) = RTCP_PORT_FROM_RTP(pDestCfg->dstPort);
    pDest->outCb.pliveQIdx = pDestCfg->outCb.pliveQIdx;
    
  }

  if(rc == 0) {
    pDest->isactive = 1;
    pRtp->numDests++;

    if(pDestCfg->pMonitor && pDestCfg->pMonitor->active) {

      pDest->pMonitor = pDestCfg->pMonitor;
      pthread_mutex_init(&pDest->streamStatsMtx, NULL);

      //TODO: only enable ABR for the 1st ongoing video stream instance  
      if(!(pDest->pstreamStats = stream_monitor_createattach(pDest->pMonitor, 
                                    (const struct sockaddr *) &pDest->saDsts, 
                                    pDest->pRtpMulti->pStreamerCfg->action.do_output_rtphdr ?  
                                    STREAM_METHOD_UDP_RTP : STREAM_METHOD_UDP, 
                                    (pDestCfg->doAbrAuto && pDestCfg->pMonitor->pAbr ? 
                                     (pRtp->isaud ? STREAM_MONITOR_ABR_ACCOUNT : STREAM_MONITOR_ABR_ENABLED) : 
                                     STREAM_MONITOR_ABR_NONE)))) {
        pthread_mutex_destroy(&pDest->streamStatsMtx);
      } else {
        pDest->pstreamStats->pRtpDest = pDest;
      }

      if(pDestCfg->dstPortRtcp > 0 && (pDest->sendrtcpsr = pRtp->init.sendrtcpsr)) {
        pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
        STREAM_RTCP_PSTUNSOCK(*pDest)->pXmitStats = pDest->pstreamStats;
        pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
      }

    }

    if(pDestCfg->useSockFromCapture) {
      pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psenderSSRC = &pRtp->init.ssrc;
    }

    if((pRtp->init.sendrtcpsr || pDest->xmit.netsock.ssl.pCtxt || pDest->xmit.netsockRtcp.ssl.pCtxt) && 
       !pDestCfg->noxmit && !pDestCfg->useSockFromCapture &&
       (!is_audvidmux || pDest->pRtpMulti->avidx == 0)) {
      //
      // If useSockFromCapture is set, capture_socket loop will process recepetion based RTCP (RR/FIR) as 
      // well as sender based RTCP (SR/BYE)
      //
      pDest->ptvlastupdate = pDestCfg->ptvlastupdate;
      stream_rtcp_responder_start(pRtp, 0);
    }

    //
    // Set STUN configuration
    //
    memset(&pDest->stun, 0, sizeof(pDest->stun));

    if(pDestCfg->stunCfg.cfg.bindingRequest != STUN_POLICY_NONE) {
      if(pDestCfg->stunCfg.cfg.reqUsername) {
        strncpy(pDest->stun.store.ufragbuf, pDestCfg->stunCfg.cfg.reqUsername, STUN_STRING_MAX - 1);
        pDest->stun.integrity.user = pDest->stun.store.ufragbuf;
      }
      if(pDestCfg->stunCfg.cfg.reqPass) {
        strncpy(pDest->stun.store.pwdbuf, pDestCfg->stunCfg.cfg.reqPass, STUN_STRING_MAX - 1);
        pDest->stun.integrity.pass = pDest->stun.store.pwdbuf;
      }
      if(pDestCfg->stunCfg.cfg.reqRealm) {
        strncpy(pDest->stun.store.realmbuf, pDestCfg->stunCfg.cfg.reqRealm, STUN_STRING_MAX - 1);
        pDest->stun.integrity.realm = pDest->stun.store.realmbuf;
      }
      pDest->stun.policy = pDestCfg->stunCfg.cfg.bindingRequest;
      pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
      pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].pstun = &pDest->stun;
      pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

      stream_stun_start(pDest, 0);
    }

#if defined(VSX_HAVE_TURN)

    //LOG(X_DEBUG("DEST isaud:%d, avidx:%d, outidx:%d, pnetsock: 0x%x"), pDest->pRtpMulti->isaud, pDest->pRtpMulti->avidx, pDest->pRtpMulti->outidx, STREAM_RTP_PNETIOSOCK(*pDest));

    //
    // Initialize any TURN output context 
    //
    pDest->turns[0].active = pDest->turns[1].active = 0;

    if(rc >= 0 && pDest->pRtpMulti->pStreamerCfg->cfgrtp.do_setup_turn &&
       (pDest->pRtpMulti->avidx == 0 || !is_audvidmux) && pDestCfg->turnCfg.turnPolicy != TURN_POLICY_DISABLED &&
       (rc = turn_init_from_cfg(&pDest->turns[0], &pDestCfg->turnCfg)) == 1) {
      rc = 0;
      memcpy(&pDest->turns[1], &pDest->turns[0], sizeof(pDest->turns[0]));

      if(!INET_ADDR_VALID(pDest->turns[0].saPeerRelay)) {
        memcpy(&pDest->turns[0].saPeerRelay, &pDest->saDsts, INET_SIZE(pDest->saDsts));
      }
      if(!INET_ADDR_VALID(pDest->turns[1].saPeerRelay)) {
        memcpy(&pDest->turns[1].saPeerRelay, &pDest->saDstsRtcp, INET_SIZE(pDest->saDstsRtcp));
      }

//pDest->turns[0].saPeerRelay.sin_addr.s_addr = net_resolvehost("172.16.165.1"); memcpy(&pDest->turns[1].saPeerRelay, &pDest->turns[0].saPeerRelay, sizeof(pDest->turns[1].saPeerRelay));
//TODO: the remote port should be the TURN raddr port
//pDest->turns[0].saPeerRelay.sin_port = htons(54974); pDest->turns[1].saPeerRelay.sin_port = pDest->turns[0].saPeerRelay.sin_port;

      //
      // if we are not setting up a capture based TURN context, then initialize TURN here 
      //
      if(!DESTCFG_USERTPBINDPORT(pDestCfg) && INET_ADDR_VALID(pDest->turns[0].saTurnSrv)) {

        memset(&onResult, 0, sizeof(onResult));
        onResult.active = 1;
        onResult.is_rtcp = 0;
        onResult.isaud = 0;
        onResult.pSdp = pDest->pRtpMulti->pStreamerCfg->sdpsx[1];
        memcpy(&onResult.saListener, &pDest->saDsts, sizeof(onResult.saListener));

        if((rc = turn_thread_start(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.turnThreadCtxt) >= 0) &&
            (rc = turn_relay_setup(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.turnThreadCtxt, 
                                 &pDest->turns[0],  STREAM_RTP_PNETIOSOCK(*pDest), &onResult)) >= 0) {
          pDest->turns[0].active = 1;
          rc = 0;
        }

        onResult.is_rtcp = 1;
        memcpy(&onResult.saListener, &pDest->saDstsRtcp, sizeof(onResult.saListener));

        if(rc >= 0 && pDest->sendrtcpsr && INET_PORT(pDest->saDstsRtcp) != INET_PORT(pDest->saDsts) &&
            (rc = turn_relay_setup(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.turnThreadCtxt, 
                                 &pDest->turns[1],  STREAM_RTCP_PNETIOSOCK(*pDest), &onResult)) >= 0) {
          pDest->turns[1].active = 1;
          rc = 0;
        }
      }
//TODO: wait until TURN relay completes to try to send output.. similar to DTLS loop in streamer...
    }

    //memcpy(&pDest->turnOut.cfg, &pDestCfg->turnCfg, sizeof(pDest->turnOut.cfg));
    //pDest->turnOut.pDest = pDest;
    //pDest->stun.pTurnOut = &pDest->turnOut;

#endif // (VSX_HAVE_TURN)

  } if(!pDest) {
    LOG(X_ERROR("No available RTP session %d/%d"), pRtp->numDests, pRtp->maxDests);
  }

  pthread_mutex_unlock(&pRtp->mtx); 

  return rc == 0 ? pDest : NULL;
}

static int stream_rtp_removedestidx(STREAM_RTP_MULTI_T *pRtp, 
                                    unsigned int idxDest,
                                    int lock) {

  int rc;
  int len = 0;
  STREAM_RTP_DEST_T *pDest = NULL;
  char tmp[128];
  unsigned char buf[256];

  if(pRtp == NULL || idxDest >= pRtp->maxDests) {
    return -1;
  } 

  pDest = &pRtp->pdests[idxDest]; 

  if(lock) {
    pthread_mutex_lock(&pRtp->mtx);
  }

  if(pRtp->init.raw.haveRaw) {
    pDest->isactive = 0;
    pDest->ptvlastupdate = NULL;
    pDest->outCb.pliveQIdx = NULL;
    if(lock) {
      pthread_mutex_unlock(&pRtp->mtx);
    }
    return 0;
  }

  streamxmit_close(pDest, 0);

  if(INET_PORT(pDest->saDstsRtcp) != 0 && 
     PNETIOSOCK_FD(STREAM_RTCP_PNETIOSOCK(pRtp->pdests[idxDest])) != INVALID_SOCKET) {

    VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - CLOSE RTCP SOCK %d dest[%d] %s:%d"),
                   STREAM_RTCP_FD(*pDest), idxDest, 
                   FORMAT_NETADDR(pDest->saDstsRtcp, tmp, sizeof(tmp)), htons(INET_PORT(pDest->saDstsRtcp))));

    //
    // Send RTCP BYE
    //
    if(!pDest->noxmit && (len = stream_rtcp_createbye(pRtp, idxDest, buf, sizeof(buf))) > 0) {

      rc = streamxmit_sendto(&pRtp->pdests[idxDest], buf, len, 1, 0, 0);

      VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - SEND BYE rc:%d len:%d"), rc, len));
    }

    
    //
    // Close any RTCP TURN relay
    //
    if(pDest->turns[1].active) {
      turn_close_relay(&pDest->turns[1], 0);
    }
    netio_closesocket(&pDest->xmit.netsockRtcp);
  }

  if(NETIOSOCK_FD(pDest->xmit.netsock) != INVALID_SOCKET) {
    //
    // Close any RTP TURN relay
    //
    if(pDest->turns[0].active) {
      turn_close_relay(&pDest->turns[0], 0);
    }
    netio_closesocket(&pDest->xmit.netsock);
  }

  if(pDest->pstreamStats) { 

    if(STREAM_RTCP_PSTUNSOCK(*pDest)->pXmitStats) {
      pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
      STREAM_RTCP_PSTUNSOCK(*pDest)->pXmitStats = NULL;
      pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    }

    //
    // destroy and automatically detach the stats from the monitor linked list
    //
    stream_stats_destroy(&(pDest->pstreamStats), &pDest->streamStatsMtx);
  }

  //
  // Remove any capture SRTP RTCP RR responders using the sender's SRTP context
  //
  if(pDest->pRtpMulti) {
    pthread_mutex_lock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psrtps[0] = NULL;
    pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psrtps[1] = NULL;
    pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].pstun = NULL;
    pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].pdtls = NULL;
    pDest->pRtpMulti->pStreamerCfg->sharedCtxt.av[pDest->pRtpMulti->avidx].psenderSSRC = NULL;
    pthread_mutex_unlock(&pDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
  }

  //
  // Close any SRTP context
  //
  srtp_closeOutputStream(&pDest->srtps[0]);
  srtp_closeOutputStream(&pDest->srtps[1]);

  if(pDest->isactive) {
    pDest->isactive = 0;
    if(pRtp->numDests > 0) {
      pRtp->numDests--;
    }
  }

  if(lock) {
    pthread_mutex_unlock(&pRtp->mtx);
  }

  return 0;
}

int stream_rtp_removedest(STREAM_RTP_MULTI_T *pRtp, const STREAM_RTP_DEST_T *pDest) {

  int rc = -1;
  unsigned int idxDest;

  if(!pRtp || !pDest) {
    return -1;
  }

  if(pRtp->init.raw.haveRaw) {
    return -1;
  }

  pthread_mutex_lock(&pRtp->mtx);

  for(idxDest = 0; idxDest < pRtp->maxDests; idxDest++) {
    if(pRtp->pdests[idxDest].isactive && pDest == &pRtp->pdests[idxDest]) {
      rc = stream_rtp_removedestidx(pRtp, idxDest, 0);
      break;
    }
  }

  pthread_mutex_unlock(&pRtp->mtx);

  return rc;
}

int stream_rtp_close(STREAM_RTP_MULTI_T *pRtp) {
	  
  unsigned int idxDest;

  if(!pRtp->isinit) {
    return -1;
  }

  pthread_mutex_lock(&pRtp->mtx);

  for(idxDest = 0; idxDest < pRtp->maxDests; idxDest++) {
    stream_rtp_removedestidx(pRtp, idxDest, 0);
  }

  pthread_mutex_unlock(&pRtp->mtx);

  stream_stun_stop(pRtp);

  stream_rtcp_responder_stop(pRtp);

  streamxmit_async_stop(pRtp, 0);

#if defined(VSX_HAVE_SSL_DTLS)
  //
  // Close any DTLS context
  //
  if(pRtp->dtls.active == 1) {
    //LOG(X_DEBUG("Closing RTP DTLS output context 0x%x"), &pRtp->dtls);
    dtls_ctx_close(&pRtp->dtls);
  } 
  pRtp->dtls.pDtlsShared = NULL;
#endif // (VSX_HAVE_SSL_DTLS)

  pthread_mutex_destroy(&pRtp->mtx);
  pRtp->isinit = 0;

  return 0;
}

int stream_rtp_preparepkt(STREAM_RTP_MULTI_T *pRtp) {
  int rc  = 0;

  if(pRtp->pktCnt== 0) {
    pRtp->tmStartXmit = timer_GetTime();
    pRtp->pktCnt++;
  }

  pRtp->pRtp->sequence_num = htons(ntohs(pRtp->pRtp->sequence_num) + 1);

  if(pRtp->init.raw.haveRaw) {
    // Assume IPv4
    pRtp->packet.u_ip.pip4->ip_id = htons(ntohs(pRtp->packet.u_ip.pip4->ip_id) + 1);
    RTP_SET_IP4_LEN(pRtp);
    RTP_SET_UDP_LEN(pRtp);
  }

/*
  //
  // Attempt to autodetect transmission falling behind
  //
  if(m_dbgReportIdx == 0) {
    gettimeofday(&m_tvDbgReport, NULL);
    m_dbgLastTs = ntohl(m_pRtp->timeStamp);
    m_dbgLastSeq = ntohs(m_pRtp->sequence_num);
    m_dbgReportIdx = 1;
  }

  if(m_clockRateHz > 0 &&
     (ntohl(m_pRtp->timeStamp) / m_clockRateHz) == m_dbgReportIdx) {
     gettimeofday(&tv, NULL);

    if(m_dbgReportIdx > 1) {

      int ms;
      int deltaTs = ntohl(m_pRtp->timeStamp) - m_dbgLastTs;

      ms = ((tv.tv_sec - m_tvDbgReport.tv_sec) * 1000) + 
           ((tv.tv_usec - m_tvDbgReport.tv_usec) /1000);




      if((deltaTs  < (float) .99f * (ms * (m_clockRateHz / 1000))) ||
        (deltaTs  > (float) 1.01f * (ms * (m_clockRateHz / 1000)))) {


          LOG(X_WARNING("Problem with stream output .  "
                        "ssrc: %u rate: %u ts: %u (%u) seq: %u (%u) ms: %d %u:%u"), m_pRtp->ssrc, m_clockRateHz,
          deltaTs, ntohl(m_pRtp->timeStamp),
         (unsigned int)ntohs(m_pRtp->sequence_num) - m_dbgLastSeq, ntohs(m_pRtp->sequence_num),
          ms, tv.tv_sec, tv.tv_usec);

      }
    }

    m_tvDbgReport.tv_sec = tv.tv_sec;
    m_tvDbgReport.tv_usec = tv.tv_usec;
    m_dbgLastTs = ntohl(m_pRtp->timeStamp);
    m_dbgLastSeq = ntohs(m_pRtp->sequence_num);
    m_dbgReportIdx += 10;
  }
*/

  if(pRtp->init.raw.haveRaw) {
    if((rc = pktgen_ChecksumUdpPacketIpv4(&pRtp->packet)) == 0) {
    } else {
      LOG(X_ERROR("Error computing checksum on packet"));
    }
  }

  return rc;
}


/*
int stream_rtp_cansend(STREAM_RTP_MULTI_T *pRtp) {

  TIME_VAL tvNow, tvElapsed;

  tvNow = timer_GetTime();

  if(pRtp->pktsSent == 0) {

    pRtp->tmStartXmit = tvNow;
    pRtp->pktsSent++;
    if(pRtp->nextFrame > pRtp->framesSent) { 
      pRtp->framesSent++;
    }
    return 1;
  }

  tvElapsed = tvNow - pRtp->tmStartXmit;

  //fprintf(stderr, "cansend:%llu %f %llu fr:%d\n", tvElapsed, (tvElapsed *((double)pRtp->init.clockRateHz / 1000.0f) / 1000.0f)  , ((long long) pRtp->framesSent * pRtp->init.frameDeltaHz), pRtp->framesSent);

  if((tvElapsed * ((double)pRtp->init.clockRateHz / 1000.0f) / 1000.0f) > 
    ((long long) pRtp->framesSent * pRtp->init.frameDeltaHz)) {


    pRtp->pktsSent++;
    if(pRtp->nextFrame > pRtp->framesSent) { 
      pRtp->framesSent++;
    }
 
    return 1;
  }
  
  return 0;
}
*/

const unsigned char *stream_rtp_data(STREAM_RTP_MULTI_T *pRtp) {
  if(pRtp->init.raw.haveRaw) {
    return (const unsigned char *) &pRtp->packet.data;
  } else {
    return (const unsigned char *) pRtp->pRtp;
  }
}

unsigned int stream_rtp_datalen(STREAM_RTP_MULTI_T *pRtp) {
  if(pRtp->init.raw.haveRaw) {
    return pRtp->payloadLen + 
          ((char *)pRtp->pPayload - (char *)pRtp->packet.peth);
  } else {
    return ((char *) pRtp->pPayload - (char *) pRtp->pRtp) + pRtp->payloadLen;
  }
}

static int stream_rtcp_createbye(STREAM_RTP_MULTI_T *pRtp, unsigned int idxDest,
                                 unsigned char *pBuf, unsigned int lenbuf) {
  RTCP_PKT_HDR_T *pHdr;

  /*
  RFC 3550
  6.6 BYE: Goodbye RTCP Packet

         0                   1                   2                   3
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |V=2|P|    SC   |   PT=BYE=203  |             length            |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                           SSRC/CSRC                           |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        :                              ...                              :
        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  (opt) |     length    |               reason for leaving            ...
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */

  if(idxDest >= pRtp->maxDests || !pRtp->pdests[idxDest].isactive) {
    return -1;
  }

  pHdr = (RTCP_PKT_HDR_T *) pBuf;

  //
  // Build RTCP SR packet static values
  //
  pHdr->hdr = (RTP_VERSION << RTP_VERSION_BITSHIFT);
  pHdr->hdr |= 0 & 0x40; // padding 0
  pHdr->hdr |= 1 & 0x3f; // source count 1
  pHdr->pt = RTCP_PT_BYE;
  pHdr->len = htons(1);   // in 4 byte words
  pHdr->ssrc = pRtp->pRtp->ssrc;

  // 1 byte length field and n byte text field

  return 8;
}

int stream_rtcp_createsr(STREAM_RTP_MULTI_T *pRtp, unsigned int idxDest,
                         unsigned char *pBuf, unsigned int lenbuf) {
  unsigned int idx = 0;
  unsigned int len;
  unsigned int lentot = 0;
  struct timeval t;
  double duration;
  uint32_t sec;
  uint32_t usec;

  /*
  RFC 3550
  6.4.1 SR: Sender Report RTCP Packet

          0                   1                   2                   3
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  header |V=2|P|    RC   |   PT=SR=200   |             length            |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                         SSRC of sender                        |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  sender |              NTP timestamp, most significant word             |
  info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |             NTP timestamp, least significant word             |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                         RTP timestamp                         |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                     sender's packet count                     |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                      sender's octet count                     |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  report |                 SSRC_1 (SSRC of first source)                 |
  block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    1    | fraction lost |       cumulative number of packets lost       |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |           extended highest sequence number received           |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                      interarrival jitter                      |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                         last SR (LSR)                         |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                   delay since last SR (DLSR)                  |
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  report |                 SSRC_2 (SSRC of second source)                |
  block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    2    :                               ...                             :
         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
         |                  profile-specific extensions                  |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */

  if(idxDest >= pRtp->maxDests || !pRtp->pdests[idxDest].isactive) {
    return -1;
  }

  if(pRtp->init.clockRateHz > 0) {

    duration = (double) (htonl(pRtp->pdests[idxDest].rtcp.sr.rtpts) - pRtp->tsStartXmit) / 
               pRtp->init.clockRateHz;

    //
    // Apply any pre-configured RTCP timestamp offset
    //
    if(pRtp->pdests[idxDest].rtcp.ftsoffsetdelay > 0) {
      duration += pRtp->pdests[idxDest].rtcp.ftsoffsetdelay;
    } 

    sec = (uint32_t) SEC_BETWEEN_EPOCH_1900_1970 + 
            (pRtp->tmStartXmit / TIME_VAL_US) + ((uint32_t)duration);
    usec = (uint32_t) ((double)(duration - (uint32_t) duration) * 0xffffffff);

    //fprintf(stderr, "RTCP FROM TS DURATION:%f start:%lld %u, sec:%u, usec:%u\n", duration, pRtp->tmStartXmit, (pRtp->tmStartXmit/TIME_VAL_US), sec, usec);


  } else {

    gettimeofday(&t, NULL);

    //
    // Apply any pre-configured RTCP timestamp offset
    //
    if(pRtp->pdests[idxDest].rtcp.ftsoffsetdelay > 0) {
      t.tv_usec +=  (((uint32_t) (pRtp->pdests[idxDest].rtcp.ftsoffsetdelay * 1000000.0f)) % 1000000);
      if(t.tv_usec > 1000000) {
        t.tv_sec += t.tv_usec/1000000;
      }     
      t.tv_sec +=  (uint32_t) pRtp->pdests[idxDest].rtcp.ftsoffsetdelay;
    }

    sec = (uint32_t) SEC_BETWEEN_EPOCH_1900_1970  + t.tv_sec;
    usec = t.tv_usec * (0xffffffff / TIME_VAL_US);

    //fprintf(stderr, "RTCP FROM gettimeofday tv:%u:%u sec:%u, usec:%u\n", t.tv_sec, t.tv_usec, sec, usec);
  }

  //TODO: sec/usec should have ntp offset from 'ntpq -c peers' applied

  pRtp->pdests[idxDest].rtcp.sr.pktcnt = htonl(pRtp->pdests[idxDest].rtcp.pktcnt);
  pRtp->pdests[idxDest].rtcp.sr.octetcnt = htonl(pRtp->pdests[idxDest].rtcp.octetcnt);


  //fprintf(stderr, "SR idxDest[%d] pt:%d, ssrc:0x%x(0x%x) pktcnt:%d, octets:%d, seq:%u, ts:%u/%uHz sec:%u,%u\n", idxDest, pRtp->init.pt, htonl(pRtp->pdests[idxDest].rtcp.sr.hdr.ssrc), pRtp->init.ssrc, pRtp->pdests[idxDest].rtcp.pktcnt, pRtp->pdests[idxDest].rtcp.octetcnt, htons(pRtp->pdests[idxDest].rtcp.seqhighest), htonl(pRtp->pdests[idxDest].rtcp.sr.rtpts), pRtp->init.clockRateHz, sec, usec);

  //
  // Create rtcp sender report
  // 
  pRtp->pdests[idxDest].rtcp.sr.ntp_msw = htonl( *((uint32_t *) &sec) );
  pRtp->pdests[idxDest].rtcp.sr.ntp_lsw = htonl( *((uint32_t *) &usec));
  len = 4 + (ntohs(pRtp->pdests[idxDest].rtcp.sr.hdr.len) * 4);
  if(idx + len > lenbuf) {
    return -1;
  }
  memcpy(pBuf, &pRtp->pdests[idxDest].rtcp.sr, len);
  idx += len;
  lentot += len;

  //
  // Create rtcp source description
  // 
  len = 4 + (ntohs(pRtp->pdests[idxDest].rtcp.sdes.hdr.len) * 4);
  if(idx + len > lenbuf) {
    return -1;
  }
  memcpy(&pBuf[idx], &pRtp->pdests[idxDest].rtcp.sdes, len);
  idx += len;
  lentot += len;

  return lentot;
}

static void log_rtcp_err(const char *msg, const STREAM_RTP_DEST_T *pRtpDest, const struct sockaddr *psaSrc,
                         uint32_t ssrc, int ssrc_match_err) {
  int len;
  struct sockaddr_storage saTmp;
  char tmp[128];
  char buf[128];

  if(ssrc_match_err) {
    snprintf(buf, sizeof(buf), " does not match outbound RTP ssrc: 0x%x", pRtpDest->pRtpMulti->init.ssrc);
  }
  len = sizeof(saTmp);
  getsockname(STREAM_RTCP_FD(*pRtpDest), (struct sockaddr *) &saTmp,  (socklen_t *) &len);
  LOG(X_WARNING("%s ssrc: 0x%x %s:%d -> :%d%s"),
    msg, ssrc, psaSrc ? FORMAT_NETADDR(*psaSrc, tmp, sizeof(tmp)) : "", 
    psaSrc ? ntohs(PINET_PORT(psaSrc)) : 0,  ntohs(INET_PORT(saTmp)), ssrc_match_err ? buf : "");

}

static void process_rtcp_RR(STREAM_RTP_DEST_T *pRtpDest, const RTCP_PKT_HDR_T *pHdr) {
  uint16_t seq;
  int lost, lostdelta, seqdelta;

  //
  // Update any last update notification (RTSP session based on RR reports)
  //
  if(pRtpDest->ptvlastupdate) {
    gettimeofday(pRtpDest->ptvlastupdate, NULL);
    //LOG(X_DEBUG("process_rtcp RR updated tvlastupdate %u"), pRtpDest->ptvlastupdate->tv_sec); 
  }

  memcpy(&pRtpDest->rtcp.lastRrRcvd, pHdr, RTCP_RR_LEN);
  pRtpDest->rtcp.tmLastRrRcvd = timer_GetTime();

  seq = htons(pRtpDest->rtcp.lastRrRcvd.seqhighest);
  lost = 0;
  memcpy(&lost, pRtpDest->rtcp.lastRrRcvd.cumulativelost, 3);
  lost = htonl(lost << 8);
  if((lostdelta = (lost - (int) pRtpDest->rtcp.prevcumulativelost)) < 0) {
    lostdelta = 0;
  }
  pRtpDest->rtcp.reportedLost = lostdelta;
  pRtpDest->rtcp.reportedLostTot += lostdelta;
  pRtpDest->rtcp.prevcumulativelost = lost;
  seqdelta = RTP_SEQ_DELTA(seq, pRtpDest->rtcp.prevseqhighest) + 1;
  pRtpDest->rtcp.reportedSeqNum = seqdelta;
  pRtpDest->rtcp.reportedSeqNumTot += seqdelta;
  pRtpDest->rtcp.prevseqhighest = seq;

  if(pRtpDest->pstreamStats) {
    stream_stats_addRTCPRR(pRtpDest->pstreamStats, &pRtpDest->streamStatsMtx, &pRtpDest->rtcp);
  }

}

static int process_rtcp_pkt(STREAM_RTP_DEST_T *pRtpDest, 
                            const RTCP_PKT_HDR_T *pHdr, 
                            unsigned int len, 
                            const struct sockaddr *psaSrc, 
                            const struct sockaddr *psaDst,
                            int fromCapture) {
  int rc = 0;
  RTCP_PKT_RR_T *pRR = NULL;
  RTCP_PKT_PSFB_FIR_T *pFir = NULL;
  RTCP_PKT_PSFB_PLI_T *pPli = NULL;
  RTCP_PKT_RTPFB_NACK_T *pNack = NULL;
  unsigned int pktlen = 0;
  unsigned int count;

  //
  // psaSrc, psaDst could be NULL if we're called from an interleaved RTSP RTCP handler
  //

  if(!RTCP_HDR_VALID(pHdr->hdr) || len < RTCP_HDR_LEN) {
    return -1;
  }

  pktlen = ntohs(pHdr->len) * 4 + 4;

  VSX_DEBUG_RTCP(LOG(X_DEBUG("RTCP stream process_rtcp pt:%d pktlen:%d len:%d, pDest:0x%x, outidx:%d, src_port:%d"),
           pHdr->pt, pktlen, len, pRtpDest, pRtpDest->pRtpMulti->outidx, htons(PINET_PORT(psaSrc)))); 

  switch(pHdr->pt) {

    case RTCP_PT_RR:

      pRR = (RTCP_PKT_RR_T *) pHdr;

      count = RTCP_RR_COUNT(pHdr->hdr);

      if(pktlen  < RTCP_RR_LEN || len < RTCP_RR_LEN || count < 1) {
        return 0;
      } else if(ntohl(pRR->ssrc_mediasrc) != pRtpDest->pRtpMulti->init.ssrc) {
        log_rtcp_err("Received RTCP RR", pRtpDest, psaSrc, ntohl(pRR->ssrc_mediasrc), 1);
        return 0;
      }

      process_rtcp_RR(pRtpDest, pHdr);

      rc = RTCP_PT_RR;
      break;

    case RTCP_PT_PSFB:

      count = RTCP_FB_TYPE(pHdr->hdr);

      switch(count) {
        case RTCP_FCI_PSFB_FIR:

          pFir = (RTCP_PKT_PSFB_FIR_T *) pHdr;

          if(pktlen  < RTCP_FIR_LEN || len < RTCP_FIR_LEN) {
            return 0;
          } else if(ntohl(pFir->ssrc_mediasrc) != pRtpDest->pRtpMulti->init.ssrc) {
            log_rtcp_err("Received RTCP FIR", pRtpDest, psaSrc, ntohl(pFir->ssrc_mediasrc), 1);
            return 0;
          } else if(!pRtpDest->pRtpMulti->init.fir_recv_via_rtcp) {
            log_rtcp_err("Ignoring RTCP FIR", pRtpDest, psaSrc, ntohl(pFir->ssrc_mediasrc), 0);
            return 0;
          } else if(pRtpDest->rtcp.lastRcvdFir > 0 && 
                    pRtpDest->rtcp.lastRcvdFirSeqno == pFir->seqno) {
            log_rtcp_err("Ignoring duplicate RTCP FIR", pRtpDest, psaSrc, ntohl(pFir->ssrc_mediasrc), 0);
            return 0;
          }

          pRtpDest->rtcp.lastRcvdFirSeqno = pFir->seqno;
          pRtpDest->rtcp.lastRcvdFir = 1;

          LOG(X_DEBUG("Received RTCP FIR for RTP ssrc 0x%x"), pRtpDest->pRtpMulti->init.ssrc);
          streamer_requestFB(pRtpDest->pRtpMulti->pStreamerCfg, 
                             pRtpDest->pRtpMulti->outidx, ENCODER_FBREQ_TYPE_FIR, 0, REQUEST_FB_FROM_REMOTE);

          break;

        case RTCP_FCI_PSFB_PLI:

          pPli = (RTCP_PKT_PSFB_PLI_T *) pHdr;

          if(pktlen  < RTCP_PLI_LEN || len < RTCP_PLI_LEN) {
            return 0;
          } else if(ntohl(pPli->ssrc_mediasrc) != pRtpDest->pRtpMulti->init.ssrc) {
            log_rtcp_err("Received RTCP PLI", pRtpDest, psaSrc, ntohl(pPli->ssrc_mediasrc), 1);
            return 0;
          //} else if(!pRtpDest->pRtpMulti->init.fir_recv_via_rtcp) {
          //  log_rtcp_err("Ignoring RTCP PLI", pRtpDest, psaSrc, ntohl(pPli->ssrc_mediasrc), 0);
          //  return 0;
          }
          LOG(X_DEBUG("Received RTCP PLI for RTP ssrc 0x%x"), pRtpDest->pRtpMulti->init.ssrc);
          //streamer_requestFB(pRtpDest->pRtpMulti->pStreamerCfg, 
          //                   pRtpDest->pRtpMulti->outidx, ENCODER_FBREQ_TYPE_PLI, 0, REQUEST_FB_FROM_REMOTE);

          break;
          
        case RTCP_FCI_PSFB_APP:

          if(pktlen  < 24 || len < 24) {
            return 0;
          } else if((*((uint32_t *) &(((unsigned char *) pHdr)[12]))) == *((uint32_t *) "REMB")) {

            unsigned char *uc = (unsigned char *) pHdr;
            uint8_t brExp = uc[17] >> 2;
            uint32_t brMantissa = ((uc[17] & 0x03) << 16) | (uc[18] << 8) | uc[19];
            LOG(X_DEBUG("Received RTCP APP REMB length: %d for RTP ssrc 0x%x, %lldb/s"), 
                 pktlen, pRtpDest->pRtpMulti->init.ssrc, (uint64_t)(brMantissa << brExp));
            VSX_DEBUG_REMB( LOGHEX_DEBUG(pHdr, MIN(48, pktlen)) );

          } else {

            LOG(X_DEBUG("Received RTCP APP length: %d for RTP ssrc 0x%x"), 
                 pktlen, pRtpDest->pRtpMulti->init.ssrc);
            LOGHEX_DEBUG(pHdr, MIN(48, pktlen));
          }
          break;
        default:
          break;
       }

      break;

    case RTCP_PT_SR:
    case RTCP_PT_BYE:
      // Some two way chat may bundle an SR here, that needs to be passed to the capture thread 
      //fprintf(stderr, "----stream_rtcp got type %d 0x%x len:%d\n", pHdr->pt, pRtpDest->pRtpMulti->pStreamerCfg->sharedSock.pRtcpNotifyCtxt, len);

      //
      // Let the capture RTCP handler handle this RTCP type, which corresponds to an RTP receiver application
      //
      if(!fromCapture) {
        pthread_mutex_lock(&pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr); 
        if(pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt && psaSrc && psaDst) {
          capture_process_rtcp(pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt, psaSrc, 
                               psaDst, pHdr, pktlen);
        }
        pthread_mutex_unlock(&pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr); 
      }

      break;

    case RTCP_PT_RTPFB:

      count = RTCP_FB_TYPE(pHdr->hdr);
      switch(count) {
        case RTCP_FCI_RTPFB_NACK:

          pNack = (RTCP_PKT_RTPFB_NACK_T *) pHdr;

          if(pktlen < RTCP_NACK_LEN || len < RTCP_NACK_LEN) {
            return 0;
          } else if(ntohl(pNack->ssrc_mediasrc) != pRtpDest->pRtpMulti->init.ssrc) {
            log_rtcp_err("Received RTCP FIR", pRtpDest, psaSrc, ntohl(pNack->ssrc_mediasrc), 1);
            return 0;
          //} else if(pRtpDest->rtcp.lastRcvdFir > 0 &&
          //          pRtpDest->rtcp.lastRcvdFirSeqno == pFir->seqno) {
          //  log_rtcp_err("Ignoring duplicate RTCP FIR", pRtpDest, psaSrc, ntohl(pFir->ssrc_mediasrc), 0);
          //  return 0;
          }

          LOG(X_DEBUG("Received RTCP NACK for RTP ssrc 0x%x, len:%d, seq:%d, blp-mask:0x%x"), 
                      pRtpDest->pRtpMulti->init.ssrc, len, htons(pNack->pid), htons(pNack->blp));

          if(pRtpDest->asyncQ.doRtcpNack) {
            streamxmit_onRTCPNACK(pRtpDest, (const RTCP_PKT_RTPFB_NACK_T *) pHdr);
          }

          break;
        default:
          break;
       }
        

      break;
    case RTCP_PT_APP:
    case RTCP_PT_SDES:
      //fprintf(stderr, "Unhandled RTCP pt:%d\n", pHdr->pt);
      break;

    default:
      break;
  }

  return rc;
}

int stream_rtp_handlertcp(STREAM_RTP_DEST_T *pRtpDest, const RTCP_PKT_HDR_T *pData, 
                          unsigned int len, 
                          const struct sockaddr *psaSrc,  // The originating sender's address
                          const struct sockaddr *psaDst) { // The local listener address
  //uint16_t seq;
  unsigned int idx = 0;
  //int lost, lostdelta, seqdelta;
  STREAM_RTP_MULTI_T *pRtpMulti;
  const RTCP_PKT_HDR_T *pHdr = NULL;
  char buf[256];
  int rc = 0;

  if(!pRtpDest || !pData || len < 8) {
    LOG(X_ERROR("Bad RTCP packet pRtpDest: 0x%x, pData: 0x%x, len:%d"), pRtpDest, pData, len);
    return -1;
  }

  VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - STREAM_RTP_HANDLERTCP len:%d"), len));

  //
  // Check if useSockFromCapture is enabled which means we are sharing the capture_socket input socket
  // to send and receive RTCP packets.  In such case, the SRTP reception context for decryption of RTCP 
  // and RTP input is stored in the capture stream filter.
  //
  if(pRtpDest->xmit.pnetsockRtcp && pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt && 
     psaSrc && psaDst) {

    pthread_mutex_lock(&pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);
    rc = capture_rtcp_srtp_decrypt(pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.pRtcpNotifyCtxt, 
                              psaSrc, psaDst, pData, &len);
    pthread_mutex_unlock(&pRtpDest->pRtpMulti->pStreamerCfg->sharedCtxt.mtxRtcpHdlr);

    if(rc < 0) {
      LOG(X_ERROR("Failed to decrypt incoming RTCP SRTP packet length %d"), len);
      return -1;
    } else if(rc == 0) {
      return 0;
    }
  }

  pRtpMulti = (STREAM_RTP_MULTI_T *) pRtpDest->pRtpMulti;

  while(idx < len) {

    pHdr = (const RTCP_PKT_HDR_T *) &((unsigned char *) pData)[idx];

    if(!RTCP_HDR_VALID(pHdr->hdr) || len - idx < 4) {
      snprintf(buf, sizeof(buf), "Invalid RTCP packet header from RTP receiver at offset %d/%d", idx, len);
      log_rtcp_err(buf, pRtpDest, psaSrc, ntohl(pHdr->ssrc), 0);
      VSX_DEBUG_RTCP( LOGHEX_DEBUG(pHdr, 8 ));
      return -1;
    } else if(ntohs(pHdr->len) < 1) {
      snprintf(buf, sizeof(buf), "Invalid RTCP type %d, length %d, packet length %d from RTP receiver",
             pHdr->pt, ntohs(pHdr->len), len - idx);
      log_rtcp_err(buf, pRtpDest, psaSrc, ntohl(pHdr->ssrc), 0);
      return -1;
    }

    VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - pt:%d, hdrlen:%d, pktlen:%d [%d/%d]"), 
                                pHdr->pt, ntohs(pHdr->len), len - idx, idx, len));

    if((rc = process_rtcp_pkt(pRtpDest, pHdr, len - idx, psaSrc, psaDst, 0)) < 0) {
      break;
    }

    idx += (4 + 4 * ntohs(pHdr->len));

    VSX_DEBUG_RTCP( 
      LOG(X_DEBUG("RTCP - RR pt:%d ssrc:0x%x rcvd fraclost:%d, seqhighest:%d dlsr:%u(%u us) "
                  "lost:0x%x rrdelay:%d us ssrc:0x%x, seq_at_xmitter:%d"),  
                  pRtpMulti->init.pt, pRtpMulti->init.ssrc, pRtpDest->rtcp.lastRrRcvd.fraclost, 
                  htons(pRtpDest->rtcp.lastRrRcvd.seqhighest), htonl(pRtpDest->rtcp.lastRrRcvd.dlsr), 
                  (int32_t) (htonl(pRtpDest->rtcp.lastRrRcvd.dlsr) * (TIME_VAL_US / 65536.0f)), rc, 
                  (int32_t) (pRtpDest->rtcp.tmLastRrRcvd - pRtpDest->rtcp.tmLastSrSent) - 
                  (int32_t) (htonl(pRtpDest->rtcp.lastRrRcvd.dlsr) * (TIME_VAL_US / 65536.0f)), 
                  htonl(pRtpDest->rtcp.sr.hdr.ssrc), htons(pRtpDest->rtcp.seqhighest)));

  }

  return rc;
}

static STREAM_RTP_DEST_T *find_rtcp_dest(STREAM_RTP_MULTI_T *pRtp, const RTCP_PKT_HDR_T *pHdr, 
                                         unsigned int len, uint16_t port) {
  unsigned int idx;
  int match_ssrc = 1;
  STREAM_RTP_MULTI_T *pRtpCur = pRtp->pheadall;
  uint32_t ssrc = 0;

  //
  // If overlappingPorts is set, assume that all RTP (RTCP) listener ports are the same, so each
  // incoming RTCP message may not come in on the right socket
  //

  //TODO: ssrc here is only applicable for Reception based (RR/FIR) packets and not sender based SR/BYE
  ssrc =  htonl(*((uint32_t *) &(((uint8_t *) pHdr)[8])));

  if(ssrc == 0 && pHdr->pt == RTCP_PT_PSFB && RTCP_RR_COUNT(pHdr->hdr) == RTCP_FCI_PSFB_APP && len >= 24 &&
     (*((uint32_t *) &(((unsigned char *) pHdr)[12]))) == *((uint32_t *) "REMB")) {
    //match_ssrc = 0;
    ssrc =  htonl(*((uint32_t *) &(((uint8_t *) pHdr)[20])));
  }

  while(pRtpCur) {
    
    if(pRtpCur->numDests > 0) {
      for(idx = 0; idx < pRtp->maxDests; idx++) {

        //fprintf(stderr, "RTSP RTCP RR try %s:%d pRtpCur:0x%x, ssrc: 0x%x numDests:%d active:%d %d/%d\n", inet_ntoa(pRtpCur->pdests[idx].saDstsRtcp.sin_addr), ntohs(pRtpCur->pdests[idx].saDstsRtcp.sin_port), pRtpCur, pRtpCur->init.ssrc, pRtpCur->numDests, pRtpCur->pdests[idx].isactive, idx, pRtp->maxDests);

        //
        // TODO: this match is only done on port to allow localhost / virtual IP / proxy mappings
        //
        if(pRtpCur->pdests[idx].isactive) {
          if(match_ssrc) {
            if(ssrc == pRtpCur->init.ssrc &&
              INET_PORT(pRtpCur->pdests[idx].saDstsRtcp) == port) {
              return &pRtpCur->pdests[idx];
            }
          } else if(INET_PORT(pRtpCur->pdests[idx].saDstsRtcp) == port) {
            return &pRtpCur->pdests[idx];
          }
        }
      }
    }
    pRtpCur = pRtpCur->pnext;
  }
 
  return NULL;
}

int stream_process_rtcp(STREAM_RTP_MULTI_T *pRtpHeadall, 
                        const struct sockaddr *psaSrc, 
                        const struct sockaddr *psaDst, 
                        const RTCP_PKT_HDR_T *pHdr, 
                        unsigned int len) {
  int rc = 0;
  char tmp[128];
  STREAM_RTP_DEST_T *pRtpDest = NULL;

  //
  // This is meant to be called from the capture RTCP processor
  //

  if(!pRtpHeadall || len < RTCP_HDR_LEN + 4) {
    return -1;
  } else if(!pRtpHeadall->pStreamerCfg || !pRtpHeadall->pStreamerCfg->sharedCtxt.active) {
    // Make sure the streamer shared instance is still active
    return 0;
  }

  VSX_DEBUG_RTCP( LOG(X_DEBUG("RTCP - stream_process_rtcp RTCP  %s:%d -> :%d :%d, ssrc: 0x%x, pt:%d"), 
                    FORMAT_NETADDR(*psaSrc, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaSrc)), ntohs(PINET_PORT(psaDst)), rc, 
                    htonl(*(uint32_t *) (&((uint8_t *)pHdr)[8])), ((uint8_t *)pHdr)[1]));

  pthread_mutex_lock(&pRtpHeadall->mtx);

  if(!(pRtpDest = find_rtcp_dest(pRtpHeadall, pHdr, len, PINET_PORT(psaSrc)))) {
    LOG(X_ERROR("recv[rtcp] RTCP ssrc: 0x%x unable to find RTP stream %s:%d -> :%d"), 
      htonl(*((uint32_t *) &(((uint8_t *) pHdr)[8]))), FORMAT_NETADDR(*psaSrc, tmp, sizeof(tmp)), 
      ntohs(PINET_PORT(psaSrc)), ntohs(PINET_PORT(psaDst)));
    pthread_mutex_unlock(&pRtpHeadall->mtx);
    return -1;
  }

  if(pRtpDest->pRtpMulti != pRtpHeadall) {
    pthread_mutex_lock(&((STREAM_RTP_MULTI_T *) pRtpDest->pRtpMulti)->mtx);
  }

  if((rc = process_rtcp_pkt(pRtpDest, pHdr, len, psaSrc, psaDst, 1)) < 0) {

  }

  if(pRtpDest->pRtpMulti != pRtpHeadall) {
    pthread_mutex_unlock(&((STREAM_RTP_MULTI_T *) pRtpDest->pRtpMulti)->mtx);
  }

  pthread_mutex_unlock(&pRtpHeadall->mtx);

  return rc;
}


typedef struct RR_LISTENER_WRAP {
  STREAM_RTP_MULTI_T *pRtp;
  char                tid_tag[LOGUTIL_TAG_LENGTH];
} RR_LISTENER_WRAP_T;

void stream_rtcp_responder_listener_proc(void *pArg) {
  RR_LISTENER_WRAP_T rrListenerWrap;
  STREAM_RTP_MULTI_T *pRtp;
  STREAM_RTP_DEST_T *pdest;
  int numDests;
  fd_set fdsetRd;
  int fdHighest = 0;
  unsigned int idx, idxTmp;
  struct timeval tv;
  struct sockaddr_storage saSrc, saDst, saDstRtp;
  int len;
  int rc = 0;
  unsigned char *pData;
  int pktlen;
  int is_turn_channeldata;
  int is_turn;
  int is_turn_indication;
  char tmp[128];
  unsigned char buf[4096];
#if defined(WIN32) 
  DWORD tmp;
#endif // WIN32

  memcpy(&rrListenerWrap, pArg, sizeof(rrListenerWrap));
  pRtp = rrListenerWrap.pRtp;

  logutil_tid_add(pthread_self(), rrListenerWrap.tid_tag);

  //pthread_mutex_lock(&pRtp->mtx);
  pRtp->doRrListener = 1;
  //pthread_mutex_unlock(&pRtp->mtx);

  idxTmp = 0;
  buf[idxTmp] = '\0';
  for(idx = 0; idx < pRtp->maxDests; idx++) {
#if 1
    if(STREAM_RTCP_FD(pRtp->pdests[idx]) != INVALID_SOCKET) {
      len = sizeof(saDst);
      getsockname(STREAM_RTCP_FD(pRtp->pdests[idx]), (struct sockaddr *) &saDst,  (socklen_t *) &len);
#else
    if(STREAM_RTCP_FD_NONP(pRtp->pdests[idx]) != INVALID_SOCKET) {
      len = sizeof(saDst);
      getsockname(STREAM_RTCP_FD_NONP(pRtp->pdests[idx]), (struct sockaddr *) &saDst,  (socklen_t *) &len);
#endif // 0
      if((rc = snprintf((char *) &buf[idxTmp], sizeof(buf) - idxTmp, "%s:%d%s", 
            FORMAT_NETADDR(saDst, tmp, sizeof(tmp)), ntohs(INET_PORT(saDst)), idxTmp > 0 ? " , " : "")) > 0) {
        idxTmp += rc;
      }
    }
    if(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock) != INVALID_SOCKET) {
      len = sizeof(saDstRtp);
      getsockname(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock), (struct sockaddr *) &saDstRtp, 
                  (socklen_t *) &len);
    }
  }

  LOG(X_DEBUG("RTCP listener thread started destinations:%d pt:%d, listening on %s"), 
      pRtp->numDests, pRtp->init.pt, buf);
  rc = 0;

  while(pRtp->doRrListener == 1 && !g_proc_exit) { 

    numDests = 0;
    pthread_mutex_lock(&pRtp->mtx);

    if(pRtp->numDests > 0) {

      tv.tv_sec = 0;
      tv.tv_usec = 300000;

      FD_ZERO(&fdsetRd);
      fdHighest = 0;

      for(idx = 0; idx < pRtp->maxDests; idx++) {

//if(pRtp->pdests[idx].isactive) LOG(X_DEBUG("TRY OUT DEST[%d], turn.active:%d, saud:%d, avidx:%d, outidx:%d, pnetsock: 0x%x"), idx,  pRtp->pdests[0].turn.active, pRtp->pdests[idx].pRtpMulti->isaud, pRtp->pdests[idx].pRtpMulti->avidx, pRtp->pdests[idx].pRtpMulti->outidx, STREAM_RTP_PNETIOSOCK(pRtp->pdests[idx]));

        //
        // FD_SET the RTCP socket
        //
#if 1
        //TODO: shouldn't this jsut reference the netsock instead of STREAM_RTCP_FD since we don't want to interfere w/ any capture_socket based select/recvfrom
        if(pRtp->pdests[idx].sendrtcpsr && pRtp->pdests[idx].isactive &&
           STREAM_RTCP_FD(pRtp->pdests[idx]) != INVALID_SOCKET) {

          FD_SET(STREAM_RTCP_FD(pRtp->pdests[idx]), &fdsetRd);

          if(STREAM_RTCP_FD(pRtp->pdests[idx]) > fdHighest) {
            fdHighest = STREAM_RTCP_FD(pRtp->pdests[idx]); 
          }
#else
        if(pRtp->pdests[idx].sendrtcpsr && pRtp->pdests[idx].isactive &&
           STREAM_RTCP_FD_NONP(pRtp->pdests[idx]) != INVALID_SOCKET) {

          FD_SET(STREAM_RTCP_FD_NONP(pRtp->pdests[idx]), &fdsetRd);

          if(STREAM_RTCP_FD_NONP(pRtp->pdests[idx]) > fdHighest) {
            fdHighest = STREAM_RTCP_FD_NONP(pRtp->pdests[idx]); 
          }
#endif /// 0
          numDests++;
        }
       
        //
        // FD_SET the RTP socket
        //
        if(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock) != STREAM_RTCP_FD(pRtp->pdests[idx]) &&
          (pRtp->pdests[idx].xmit.netsock.ssl.pCtxt || pRtp->pdests[idx].xmit.netsock.turn.use_turn_indication_in)) {

          FD_SET(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock), &fdsetRd);

          if(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock) > fdHighest) {
            fdHighest = PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock);
          }

          numDests++;
        }

      }

    } // end of for(idx...

    //LOG(X_DEBUG("SELECT NUMD:%d"), numDests);

    pthread_mutex_unlock(&pRtp->mtx);

    if(g_proc_exit != 0 || numDests <= 0) {
      break;
    }

    if((rc = select(fdHighest + 1, &fdsetRd, NULL, NULL, &tv)) > 0) {

      //LOG(X_DEBUG("tid:0x%x SELECT rc:%d, [0] {rtp_fd:%d, rtcp_fd:%d , is_set:%d,%d}, "), pthread_self(), rc, PNETIOSOCK_FD(&pRtp->pdests[0].xmit.netsock), STREAM_RTCP_FD(pRtp->pdests[0]), FD_ISSET(PNETIOSOCK_FD(&pRtp->pdests[0].xmit.netsock), &fdsetRd), FD_ISSET(STREAM_RTCP_FD(pRtp->pdests[0]), &fdsetRd));
      //len = sizeof(saDst); getsockname(STREAM_RTCP_FD(pRtp->pdests[idx]), (struct sockaddr *) &saDst,  (socklen_t *) &len);    //fprintf(stderr, "RTCP recv[rtcp rr:%u] %s:%d -> :%d with rc:%d\n", idx, inet_ntoa(saSrc.sin_addr), ntohs(saSrc.sin_port), ntohs(saDst.sin_port), rc);

      pthread_mutex_lock(&pRtp->mtx);

      for(idx = 0; idx < pRtp->maxDests; idx++) {

        if(!pRtp->pdests[idx].isactive) {
          continue;
        }

        if((pRtp->pdests[idx].xmit.netsock.ssl.pCtxt || pRtp->pdests[idx].xmit.netsock.turn.use_turn_indication_in) &&
          PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock) != INVALID_SOCKET &&
          PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock) != STREAM_RTCP_FD(pRtp->pdests[idx])
#if !defined(WIN32)
        //TODO: not sure - but FD_ISSET may have been always false on win xp
           && FD_ISSET(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock), &fdsetRd)
#endif // WIN32
          ) {

          //LOG(X_DEBUG("tid:0x%x, CALLING RECVFROM RTP SOCK [idx:%d]"), pthread_self(), idx);
          pData = buf;
          is_turn = 0;
          is_turn_indication = 0;
          is_turn_channeldata = 0;
          len = sizeof(saSrc);
          if((pktlen = recvfrom(PNETIOSOCK_FD(&pRtp->pdests[idx].xmit.netsock),
                          (void *) pData, sizeof(buf), 0,
                          (struct sockaddr *) &saSrc,
                          (socklen_t *) &len)) > 0) {

            //LOG(X_DEBUG("tid:0x%x, RECVFROM RTP [idx:%d] pnetsock: 0x%x, GOT %d 0x%x, is_dtls:%d, is_turn:%d"), pthread_self(), idx, &pRtp->pdests[idx].xmit.netsock, pktlen, buf[0], DTLS_ISPACKET(buf, pktlen), stun_ispacket(buf, pktlen));
            //
            // Receive any TURN data
            //
            if(pRtp->pdests[idx].xmit.netsock.turn.use_turn_indication_in &&
                (stun_ispacket(buf, pktlen) || (is_turn_channeldata = turn_ischanneldata(buf, pktlen)))) {

              if((rc = turn_onrcv_pkt(&pRtp->pdests[idx].turns[0], &pRtp->pdests[idx].stun, is_turn_channeldata, 
                                      &is_turn, &is_turn_indication, &pData, &pktlen, 
                                      (const struct sockaddr *) &saSrc, (const struct sockaddr *) &saDstRtp, 
                                      &pRtp->pdests[idx].xmit.netsock)) < 0) {
              }
            }

            if((!is_turn || is_turn_indication || is_turn_channeldata) && DTLS_ISPACKET(buf, pktlen) && 
              (pktlen = dtls_netsock_ondata(&pRtp->pdests[idx].xmit.netsock, buf, pktlen, 
                                            (const struct sockaddr *) &saSrc)) < 0) {

            }

          } else {

#if defined(WIN32) 
         if((tmp = WSAGetLastError()) != 0 && tmp != WSAECONNRESET) {
#endif // WIN32

          len = sizeof(saDst);
          getsockname(STREAM_RTCP_FD(pRtp->pdests[idx]), (struct sockaddr *) &saDst,  (socklen_t *) &len);
          LOG(X_ERROR("recv[rtp rr:%u] %s:%d -> :%d failed with rc:%d "ERRNO_FMT_STR),
                      idx, FORMAT_NETADDR(saSrc, tmp, sizeof(tmp)), ntohs(INET_PORT(saSrc)),
                      ntohs(INET_PORT(saDstRtp)), pktlen, ERRNO_FMT_ARGS);

#if defined(WIN32) 
          }
#endif // WIN32

          }
        }

        if(!pRtp->pdests[idx].sendrtcpsr || STREAM_RTCP_FD(pRtp->pdests[idx]) == INVALID_SOCKET
#if !defined(WIN32)
        //TODO: not sure - but FD_ISSET may have been always false on win xp
           || !FD_ISSET(STREAM_RTCP_FD(pRtp->pdests[idx]), &fdsetRd)
#endif // WIN32
        ) {
          //LOG(X_DEBUG("tid:0x%x, CONTINUE FROM RR THREAD [idx:%d], sendrtcpsr:%d, fd:%d"), pthread_self(), idx, pRtp->pdests[idx].sendrtcpsr, STREAM_RTCP_FD(pRtp->pdests[idx]));
          continue;
        }

        //LOG(X_DEBUG("tid:0x%x, CALLING RECVFROM RTCP SOCK [idx:%d]"), pthread_self(), idx);
        pData = buf;
        is_turn = 0;
        is_turn_indication = 0;
        is_turn_channeldata = 0;
        len = sizeof(saSrc);
        if((pktlen = recvfrom(STREAM_RTCP_FD(pRtp->pdests[idx]),
                          (void *) pData, sizeof(buf), 0,
                          (struct sockaddr *) &saSrc,
                          (socklen_t *) &len)) > 0) {

          len = sizeof(saDst); 
          getsockname(STREAM_RTCP_FD(pRtp->pdests[idx]), (struct sockaddr *) &saDst,  (socklen_t *) &len);
          //LOG(X_DEBUGV("recv[rtcp rr:%u] %s:%d -> :%d pktlen:%d, ssrc: 0x%x, pt:%d"), idx, inet_ntoa(saSrc.sin_addr), ntohs(saSrc.sin_port), ntohs(saDst.sin_port), pktlen, htonl(*(uint32_t *) (&buf[8])), buf[1]);
          //LOG(X_DEBUG("tid:0x%x, RECVFROM RTCP [idx:%d] pnetsock: 0x%x, GOT %d 0x%x, is_dtls:%d, is_turn:%d"), pthread_self(), idx, STREAM_RTCP_PNETIOSOCK(pRtp->pdests[idx]), pktlen, buf[0], DTLS_ISPACKET(buf, pktlen), stun_ispacket(buf, pktlen));

          //
          // Receive any TURN data on the RTCP socket
          //
          if(pRtp->pdests[idx].xmit.netsock.turn.use_turn_indication_in &&
              (stun_ispacket(buf, pktlen) || (is_turn_channeldata = turn_ischanneldata(buf, pktlen)))) {

            if((rc = turn_onrcv_pkt(INET_PORT(pRtp->pdests[idx].saDstsRtcp) == INET_PORT(pRtp->pdests[idx].saDsts) ?
                                    &pRtp->pdests[idx].turns[0] : &pRtp->pdests[idx].turns[1], 
                                    &pRtp->pdests[idx].stun, is_turn_channeldata,
                                    &is_turn, &is_turn_indication, &pData, &pktlen, 
                                    (const struct sockaddr *) &saSrc, (const struct sockaddr *) &saDst, 
                                    STREAM_RTCP_PNETIOSOCK(pRtp->pdests[idx]))) < 0) {
            }
          }

          if((!is_turn || is_turn_indication || is_turn_channeldata) &&
            STREAM_RTCP_PNETIOSOCK(pRtp->pdests[idx])->ssl.pCtxt && DTLS_ISPACKET(buf, pktlen)) {
            pktlen = dtls_netsock_ondata(STREAM_RTCP_PNETIOSOCK(pRtp->pdests[idx]), buf, pktlen, 
                                         (const struct sockaddr *) &saSrc);
            LOG(X_DEBUG("DTLS packet pktlen:%d"), pktlen); 
            if(pktlen <= 0) {
              continue;
            }
          }

          pdest = NULL;
          //
          // If overlappingPorts is set, assume that all RTP (RTCP) listener ports are the same, so each
          // incoming RTCP message may not come in on the right socket
          //
          if(!pRtp->overlappingPorts) {
            pdest = &pRtp->pdests[idx];
          } else if(len >= RTCP_HDR_LEN + 4 && 
            !(pdest = find_rtcp_dest(pRtp, (RTCP_PKT_HDR_T *) buf, len, INET_PORT(saSrc)))) {
            LOG(X_ERROR("recv[rtcp rr:%u] RTCP ssrc: 0x%x unable to find RTP stream %s:%d -> :%d"), idx,
                htonl(*((uint32_t *) &(((uint8_t *) buf)[8]))), FORMAT_NETADDR(saSrc, tmp, sizeof(tmp)), 
                ntohs(INET_PORT(saSrc)), ntohs(INET_PORT(saDst)));
          }

          if(pdest) {
            if(pdest->pRtpMulti != pRtp) {
              pthread_mutex_lock(&((STREAM_RTP_MULTI_T *) pdest->pRtpMulti)->mtx);
            }

            stream_rtp_handlertcp(pdest, (RTCP_PKT_HDR_T *) buf, pktlen, (const struct sockaddr *) &saSrc, 
                                  (const struct sockaddr *) &saDst);

            if(pdest->pRtpMulti != pRtp) {
              pthread_mutex_unlock(&((STREAM_RTP_MULTI_T *) pdest->pRtpMulti)->mtx);
            }
          }

        } else {

#if defined(WIN32) 
         if((tmp = WSAGetLastError()) != 0 && tmp != WSAECONNRESET) {
#endif // WIN32

          len = sizeof(saDst); 
          getsockname(STREAM_RTCP_FD(pRtp->pdests[idx]), (struct sockaddr *) &saDst,  (socklen_t *) &len);
          LOG(X_ERROR("recv[rtcp rr:%u] %s:%d -> :%d failed with rc:%d "ERRNO_FMT_STR),
                      idx, FORMAT_NETADDR(saSrc, tmp, sizeof(tmp)), ntohs(INET_PORT(saSrc)), 
                      ntohs(INET_PORT(saDst)), pktlen, ERRNO_FMT_ARGS);

#if defined(WIN32) 
          } 
#endif // WIN32

        }

      } // end of for(idx...
      pthread_mutex_unlock(&pRtp->mtx);

    } else if(rc == 0) { // end of if((rc = select(...

    } else { // end of if((rc = select(...
      usleep(300000);
      pthread_mutex_lock(&pRtp->mtx);
      rc = pRtp->numDests;
      pthread_mutex_unlock(&pRtp->mtx);
      if(rc > 0) {
        //TODO: numDests can be true if rtp output is in addition to RTSP output
        LOG(X_WARNING("rtcp rr receiver exiting - RTP output has %d destinations"), rc);
      }
    }

  }

  LOG(X_DEBUG("RTCP listener thread ending pt:%d"), pRtp->init.pt);

  pthread_mutex_lock(&pRtp->mtx);
  pRtp->doRrListener = -1;
  pthread_mutex_unlock(&pRtp->mtx);

  logutil_tid_remove(pthread_self());

}

static int stream_rtcp_responder_stop(STREAM_RTP_MULTI_T *pRtp) {

  int rc = 0;

  pthread_mutex_lock(&pRtp->mtx);

  if(pRtp->doRrListener <= 0) {
    pthread_mutex_unlock(&pRtp->mtx);
    return 0;
  } else {
    pRtp->doRrListener = 0;
  }

  pthread_mutex_unlock(&pRtp->mtx);

  while(pRtp->doRrListener != -1) {
    usleep(40000);
  }

  return rc;
}

static int stream_rtcp_responder_start(STREAM_RTP_MULTI_T *pRtp, int lock) {
  int rc = 0;
  const char *s;
  pthread_t ptdCap;
  pthread_attr_t attrCap;
  RR_LISTENER_WRAP_T wrap;

  //fprintf(stderr, "RR_START called lock:%d do_rr:%d sendrtcpsr:%d\n", lock, pRtp->doRrListener, pRtp->init.sendrtcpsr);

  if(lock) {
    pthread_mutex_lock(&pRtp->mtx);
  }

  //
  // Check if the RTCP
  //
  if(pRtp->doRrListener > 0 || !pRtp->init.sendrtcpsr) {
    if(lock) {
      pthread_mutex_unlock(&pRtp->mtx);
    }
    return 0;
  }

  wrap.tid_tag[0] = '\0'; 
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(wrap.tid_tag, sizeof(wrap.tid_tag), "%s-rtcp", s);
  }
  wrap.pRtp = pRtp;
  pRtp->doRrListener = 2;
  pthread_attr_init(&attrCap);
  pthread_attr_setdetachstate(&attrCap, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdCap,
                    &attrCap,
                    (void *) stream_rtcp_responder_listener_proc,
                    (void *) &wrap) != 0) {

    LOG(X_ERROR("Unable to create RTCP listener thread"));
    pRtp->doRrListener = 0;
    if(lock) {
      pthread_mutex_unlock(&pRtp->mtx);
    }
    return -1;
  }

  if(lock) {
    pthread_mutex_unlock(&pRtp->mtx);
  }

  while(pRtp->doRrListener == 2) {
    usleep(5000);
  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
