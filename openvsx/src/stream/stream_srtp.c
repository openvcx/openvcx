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


static const char *s_srtpStreamTypes[] = { "",
                                         SRTP_CRYPTO_AES_CM_128_HMAC_SHA1_80 , 
                                         SRTP_CRYPTO_AES_CM_128_HMAC_SHA1_32 };

//
//RFC 5764 4.1.2 SRTP Protection Profiles
//
static const char *s_srtpDtlsStreamTypes[] = { "",
                                               SRTP_PROFILE_AES128_CM_SHA1_80, 
                                               SRTP_PROFILE_AES128_CM_SHA1_32,
                                               SRTP_PROFILE_NULL_HMAC_SHA1_80, 
                                               SRTP_PROFILE_NULL_HMAC_SHA1_32 };


const char *srtp_streamTypeStr(SRTP_AUTH_TYPE_T authType, SRTP_CONF_TYPE_T confType, int safe) {


  if(confType == SRTP_CONF_AES_128) {
    if(authType == SRTP_AUTH_SHA1_80) {
      return s_srtpStreamTypes[1];
    } else if(authType == SRTP_AUTH_SHA1_32) {
      return s_srtpStreamTypes[2];
    } else {
      return safe ? s_srtpStreamTypes[0] : NULL;
    }
  } else {
    return safe ? s_srtpStreamTypes[0] : NULL;
  }

  return safe ? s_srtpStreamTypes[0] : NULL;
}

int srtp_streamType(const char *streamTypeStr, SRTP_AUTH_TYPE_T *pAuthType, SRTP_CONF_TYPE_T *pConfType) {
 
  int rc = 0;
  
  if(!streamTypeStr) {
    rc = -1;
  } else if(!strncasecmp(streamTypeStr, s_srtpStreamTypes[1], strlen(s_srtpStreamTypes[1])) ||
            !strncasecmp(streamTypeStr, s_srtpDtlsStreamTypes[1], strlen(s_srtpDtlsStreamTypes[1]))) {
    *pConfType = SRTP_CONF_AES_128;
    *pAuthType = SRTP_AUTH_SHA1_80;
  } else if(!strncasecmp(streamTypeStr, s_srtpStreamTypes[2], strlen(s_srtpStreamTypes[2])) ||
            !strncasecmp(streamTypeStr, s_srtpDtlsStreamTypes[2], strlen(s_srtpDtlsStreamTypes[2]))) {
    *pConfType = SRTP_CONF_AES_128;
    *pAuthType = SRTP_AUTH_SHA1_32;
  } else {
    rc = -1;
  }
  return rc;
}


#if defined(VSX_HAVE_STREAMER)

int srtp_dtls_protect(const NETIO_SOCK_T *pnetsock, 
                      const unsigned char *pDataIn,
                      unsigned int lengthIn,
                      unsigned char *pDataOut,
                      unsigned int lengthOut,
                      const struct sockaddr *pdest_addr,
                      const SRTP_CTXT_T *pSrtp,
                      enum SENDTO_PKT_TYPE pktType) {
  int rc = 0;
  char tmp[128];

  if(!pnetsock || !pDataIn) {
    return -1;
  }

  if(lengthIn <= 0) {
    return 0;
  }

  if(pktType != SENDTO_PKT_TYPE_STUN && pktType != SENDTO_PKT_TYPE_STUN_BYPASSTURN && 
     pktType != SENDTO_PKT_TYPE_TURN && 
     pktType != SENDTO_PKT_TYPE_DTLS && (pnetsock->flags & NETIO_FLAG_SSL_DTLS)) {

    if(pnetsock->ssl.state != SSL_SOCK_STATE_HANDSHAKE_COMPLETED) {
      LOG(X_WARNING("Dropping %s packet to %s:%d length %d bytes because DTLS handshake has not completed"),
                (pktType == SENDTO_PKT_TYPE_RTP ? "rtp " :
                 pktType == SENDTO_PKT_TYPE_RTCP ? "rtcp " :
                 pktType == SENDTO_PKT_TYPE_RTCP ? "stun" :
                 pktType == SENDTO_PKT_TYPE_STUN ? "turn" : ""),
                FORMAT_NETADDR(*pdest_addr, tmp, sizeof(tmp)), ntohs(PINET_PORT(pdest_addr)), lengthIn);
      return -1;

    } else if(!(pnetsock->flags & NETIO_FLAG_SRTP)) {

      //LOG(X_DEBUG("ok will do DTLS sendto %s%s:%d for %d bytes"), (pktType == SENDTO_PKT_TYPE_RTP ? "rtp " : pktType == SENDTO_PKT_TYPE_RTCP ? "rtcp " : pktType == SENDTO_PKT_TYPE_STUN ? "stun " : ""), inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port), length);

      //
      // protect the packet data using DTLS. 
      //
      if((rc = dtls_netsock_write((NETIO_SOCK_T *) pnetsock, pDataIn, lengthIn, pDataOut, lengthOut)) < 0) {
        return -1; 
      }

      pSrtp = NULL;
    }
  }

  if(pSrtp && pSrtp->kt.k.lenKey > 0 && (pktType == SENDTO_PKT_TYPE_RTP ||
                                       (pktType == SENDTO_PKT_TYPE_RTCP && pSrtp->do_rtcp))) {

#if defined(VSX_HAVE_SRTP)
    //LOG(X_DEBUG("SRTP_ENCRYPT pkt len:%d %s, pSrtp: 0x%x, ->is_rtcp:%d"), lengthIn, pktType == SENDTO_PKT_TYPE_RTCP ? "rtcp" : "rtp", pSrtp, pSrtp->is_rtcp);
    if(srtp_encryptPacket(pSrtp, pDataIn, lengthIn, pDataOut, &lengthOut,
                          pktType == SENDTO_PKT_TYPE_RTCP ? 1 : 0) < 0) {
      return -1;
    }
    rc = lengthOut;
#else // (VSX_HAVE_SRTP)
    LOG(X_ERROR(VSX_SRTP_DISABLED_BANNER));
    LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
    return -1;
#endif // (VSX_HAVE_SRTP)

  }

  return rc < 0 ? -1 : rc;
}

int srtp_sendto(const NETIO_SOCK_T *pnetsock, 
                void *pData,
                size_t length,
                int flags,
                const struct sockaddr *pdest_addr,
                const SRTP_CTXT_T *pSrtp,
                enum SENDTO_PKT_TYPE pktType,
                int no_protect) {

  int rc = 0;
  unsigned char encodedBuf[PACKETGEN_PKT_UDP_DATA_SZ];
  unsigned char turnBuf[PACKETGEN_PKT_UDP_DATA_SZ];
  unsigned int lengthOut = sizeof(encodedBuf);
  const struct sockaddr *pout_addr = pdest_addr;
  unsigned int lengthBeforeTurn;
  const char *sendto_descr = NULL;
  //char tmp[128];

  if(!pnetsock) {
    return -1;
  }

  //
  // TODO: check STUN state, and if mandatory, don't send rtp/rtcp prior to succesful STUN  binding
  //
/*
  //
  // If we sent out an unresponded STUN binding request and received a binding request from a destination
  // which does not match our outgoing STUN destination
  if((PSTUNSOCK(pnetsock)->stunFlags & STUN_SOCKET_FLAG_SENT_BINDINGREQ) &&
     !(PSTUNSOCK(pnetsock)->stunFlags & STUN_SOCKET_FLAG_RCVD_BINDINGRESP) &&
     (PSTUNSOCK(pnetsock)->stunFlags & STUN_SOCKET_FLAG_RCVD_BINDINGREQ) &&
      (PSTUNSOCK(pnetsock)->sainLastXmit.sin_addr.s_addr != PSTUNSOCK(pnetsock)->sainLastRcv.sin_addr.s_addr ||
       PSTUNSOCK(pnetsock)->sainLastXmit.sin_port != PSTUNSOCK(pnetsock)->sainLastRcv.sin_port)) {

    out_addr = &PSTUNSOCK(pnetsock)->sainLastRcv;
    snprintf((char *) encodedBuf, sizeof(encodedBuf), "%s:%d", inet_ntoa(dest_addr->sin_addr), htons(dest_addr->sin_port));
    LOG(X_WARNING("Changing output destination from %s:%d -> %s"), 
         inet_ntoa(out_addr->sin_addr), htons(out_addr->sin_port), (char *) encodedBuf);
  }
*/

  //TODO: mutex protect the send if the stunsocket requires it


  if(!no_protect) {
    if((rc = srtp_dtls_protect(pnetsock, pData, length, encodedBuf, lengthOut,
                      pout_addr, pSrtp, pktType)) < 0) {
      return rc;
    } else if(rc > 0) {
      pData = (void *) encodedBuf;
      length = rc; 
    }
  }

  //LOG(X_DEBUG("SRTP_SENDTO fd:%d, -> %s:%d length:%d, pktType:%d, no_protect:%d, pSrtp: 0x%x"), PNETIOSOCK_FD(pnetsock), inet_ntoa(out_addr->sin_addr), htons(out_addr->sin_port), length, pktType, no_protect, pSrtp);

  lengthBeforeTurn = length;

  if(pktType != SENDTO_PKT_TYPE_TURN && pktType != SENDTO_PKT_TYPE_STUN_BYPASSTURN &&
     pnetsock->turn.use_turn_indication_out == 1 && length > 0 && pData) {
    pout_addr = (struct sockaddr *) &pnetsock->turn.saTurnSrv;

    if(PINET_PORT(pout_addr) == 0 || !INET_ADDR_VALID(*pout_addr)) {
      LOG(X_ERROR("TURN server address not set for socket output for length %d"), length);
      return -1;
    }

    if((rc = turn_create_datamsg(pData, length, &pnetsock->turn, turnBuf, sizeof(turnBuf))) < 0) {
      return rc;
    } else {
      pData = turnBuf;
      length = rc;
    }

  }

  if(length > 0 && pData) {

  //char logbuf[128]; LOG(X_DEBUG("SRTP_SENDTO fd:%d %s (orig: %s:%d) len:%d, use_turn_indication_out:%d, stunFlags: 0x%x"), PNETIOSOCK_FD(pnetsock), stream_log_format_pkt_sock(logbuf, sizeof(logbuf), pnetsock, out_addr), inet_ntoa(dest_addr->sin_addr), htons(dest_addr->sin_port), length, pnetsock->turn.use_turn_indication_out, PSTUNSOCK(pnetsock)->stunFlags);
    //if((rc = sendto(PNETIOSOCK_FD(pnetsock), pData, length, flags, (const struct sockaddr *) pout_addr,
    //              sizeof(struct sockaddr_in))) < (int) length) {
    if(flags != 0) {
      LOG(X_WARNING("sendto flag: 0x%x discarded!"), flags);
    }
    sendto_descr = (pktType == SENDTO_PKT_TYPE_RTP ? "rtp " :
                 pktType == SENDTO_PKT_TYPE_RTCP ? "rtcp " :
                 (pktType == SENDTO_PKT_TYPE_STUN || pktType == SENDTO_PKT_TYPE_STUN_BYPASSTURN) ? "stun " :
                 pktType == SENDTO_PKT_TYPE_TURN ? "turn " :
                 pktType == SENDTO_PKT_TYPE_DTLS ? "dtls " : "");
                
    if((rc = netio_sendto((NETIO_SOCK_T *) pnetsock, pout_addr, pData, length, sendto_descr)) < (int) length) {

      //LOG(X_ERROR("sendto %s%s:%d for %d bytes failed with rc: %d "ERRNO_FMT_STR),
      //          (pktType == SENDTO_PKT_TYPE_RTP ? "rtp " :
      //           pktType == SENDTO_PKT_TYPE_RTCP ? "rtcp " :
      //           (pktType == SENDTO_PKT_TYPE_STUN || pktType == SENDTO_PKT_TYPE_STUN_BYPASSTURN) ? "stun " :
      //           pktType == SENDTO_PKT_TYPE_TURN ? "turn " :
      //           pktType == SENDTO_PKT_TYPE_DTLS ? "dtls " : ""),
      //          FORMAT_NETADDR(*pout_addr, tmp, sizeof(tmp)), ntohs(PINET_PORT(pout_addr)), length, rc, ERRNO_FMT_ARGS);

    } else if(lengthBeforeTurn != length) {
      //
      // Return original length w/o TURN data framing since the caller stack may be checking
      // the number of bytes sent, akin to sendto functionality
      //
      rc = length;
    }

  }

  return rc;
}

#endif // (VSX_HAVE_STREAMER)

#if defined(VSX_HAVE_SRTP)

#include "srtp.h" // libsrtp/include/srtp.h

static int g_is_srtp_init = 0;

typedef struct SRTP_CTXT_INT {
  srtp_ctx_t               *srtp_ctx;
  srtp_policy_t             policy;
  uint32_t                  ssrc;
  pthread_mutex_t           mtx;
} SRTP_CTXT_INT_T;


static int srtp_initialize() {
  int rc = 0;
  if(g_is_srtp_init == 0) {
    if((rc = srtp_init()) != 0) {
      g_is_srtp_init = -1;
      LOG(X_ERROR("Failed to initialize SRTP library")); 
      return -1;
    }
    g_is_srtp_init = 1;
  } else if(g_is_srtp_init < 0) {
    return -1;
  }
  return rc;
}

static int getSaltLengthBits(SRTP_AUTH_TYPE_T authType) {
  switch(authType) {
    case SRTP_AUTH_SHA1_80:
    case SRTP_AUTH_SHA1_32:
      return 112;
    case SRTP_AUTH_NONE:
      return 0;
    default:
      return -1;    
  }
}

static int getKeyLengthBits(SRTP_CONF_TYPE_T confType) {
  switch(confType) {
    case SRTP_CONF_AES_128:
      return 128;
    case SRTP_CONF_NONE:
      return 0; 
    default:
      return -1;
  }
}

typedef struct PARSE_SRTPKEYS_CTXT {
  int rc;
  int isvid;
  SRTP_CTXT_T *pCtxtV;
  SRTP_CTXT_T *pCtxtA;
  SRTP_CTXT_T *pCtxt;
} PARSE_SRTPKEYS_CTXT_T;

static int parse_pair_cb_srtpkeys(void *pArg, const char *pElement) {
  PARSE_SRTPKEYS_CTXT_T *pParseCtxt = (PARSE_SRTPKEYS_CTXT_T *) pArg;
  int sz;

  if(*pElement != '\0' && pParseCtxt->pCtxt) {
    if((sz = base64_decode(pElement, pParseCtxt->pCtxt->kt.k.key, SRTP_KEYLEN_MAX)) < 0) {
      return -1;
    }
    pParseCtxt->pCtxt->kt.k.lenKey = sz;
    pParseCtxt->rc |= 1 << (pParseCtxt->isvid ? 0 : 1);
  }

  pParseCtxt->isvid = !pParseCtxt->isvid;
  pParseCtxt->pCtxt = pParseCtxt->isvid ? pParseCtxt->pCtxtV : pParseCtxt->pCtxtA;

  return 0;
}

int srtp_loadKeys(const char *in,  SRTP_CTXT_T *pCtxtV, SRTP_CTXT_T *pCtxtA) {
  int rc = 0;
  PARSE_SRTPKEYS_CTXT_T parseCtxt; 

  parseCtxt.rc = 0;
  parseCtxt.isvid = 1;
  parseCtxt.pCtxtV = pCtxtV;
  parseCtxt.pCtxtA = pCtxtA;
  parseCtxt.pCtxt = pCtxtV;

  if(pCtxtV) {
    pCtxtV->kt.k.lenKey = 0;
  }

  if(pCtxtA) {
    pCtxtA->kt.k.lenKey = 0;
  }

  if(SRTP_KEYLEN_MAX > PARSE_PAIR_BUF_SZ) {
    return -1;
  }

  if((rc = strutil_parse_pair(in, (void *) &parseCtxt,  parse_pair_cb_srtpkeys)) < 0) {
    return rc;
  }

  return parseCtxt.rc;
}

static int setCryptoPolicy(crypto_policy_t *policy, SRTP_AUTH_TYPE_T authType, SRTP_CONF_TYPE_T confType, int rtcp) {
  int rc = 0;

  switch(confType) {
    case SRTP_CONF_AES_128:

      switch(authType) {
        case SRTP_AUTH_SHA1_80:
          crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
          break;
        case SRTP_AUTH_SHA1_32:
          if(!rtcp) {
            crypto_policy_set_aes_cm_128_hmac_sha1_32(policy);
          } else {
            // 
            // RFC 5764 4.1.2 SRTP Protection Profiles  
            // RTCP should always be 80 bit auth tag length
            //
            crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
          }
          break;
        case SRTP_AUTH_NONE:
          crypto_policy_set_aes_cm_128_null_auth(policy);
          break;
        default:
          rc = -1;
          break;
      }

      break;

    case SRTP_CONF_NONE:

      switch(authType) {
        case SRTP_AUTH_SHA1_80:
          crypto_policy_set_null_cipher_hmac_sha1_80(policy);
          break;
        case SRTP_AUTH_SHA1_32:
          if(!rtcp) {
            crypto_policy_set_null_cipher_hmac_sha1_80(policy);
            policy->auth_tag_len = 4;
          } else {
            // 
            // RFC 5764 4.1.2 SRTP Protection Profiles  
            // RTCP should always be 80 bit auth tag length
            //
            crypto_policy_set_null_cipher_hmac_sha1_80(policy);
          }
          break;
        case SRTP_AUTH_NONE:
          policy->sec_serv = sec_serv_none; 
          break;
        default:
          rc = -1;
          break;
      }

      break;

    default:
      rc = -1;
      break;
  }

  return rc;
}

static int initStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int output, int rtcp) {
  int rc = 0;
  SRTP_CTXT_INT_T *pSrtp;
  err_status_t srtp_rc;

  if(!pCtxt || pCtxt->kt.k.lenKey <= 0) {
    return -1;
  }

  if(srtp_initialize() < 0) {
    return -1;
  }

  pCtxt->do_rtcp = pCtxt->do_rtcp_responder = 0;

  if(!(pCtxt->privData = avc_calloc(1, sizeof(SRTP_CTXT_INT_T)))) {
    return -1;
  }
  pSrtp = (SRTP_CTXT_INT_T *) pCtxt->privData;

  if((rc = setCryptoPolicy(&pSrtp->policy.rtp, pCtxt->kt.type.authType, pCtxt->kt.type.confType, 0)) < 0) {
    LOG(X_ERROR("Failed to set SRTP crypto policy for RTP"));
    avc_free((void **) &pCtxt->privData);
    return -1;
  }

  if(rtcp && (pCtxt->kt.type.authTypeRtcp != SRTP_AUTH_NONE || pCtxt->kt.type.confTypeRtcp != SRTP_CONF_NONE)) {

    if((rc = setCryptoPolicy(&pSrtp->policy.rtcp, pCtxt->kt.type.authTypeRtcp, 
                             pCtxt->kt.type.confTypeRtcp, 1)) < 0) {
      LOG(X_ERROR("Failed to set SRTP crypto policy for RTCP"));
      avc_free((void **) &pCtxt->privData);
      return -1;
    }

    pCtxt->do_rtcp = 1;

  } else {
    pSrtp->policy.rtcp.sec_serv = sec_serv_none; 
  }

  if(output) {
    pSrtp->policy.ssrc.type  = ssrc_any_outbound;
  } else {
    pSrtp->policy.ssrc.type  = ssrc_any_inbound;
  }
  pSrtp->policy.ssrc.value = 0;
  //pSrtp->policy.ssrc.type  = ssrc_specific;
  //pSrtp->policy.ssrc.value = htonl(ssrc);

  pSrtp->policy.key = pCtxt->kt.k.key;
  pSrtp->ssrc = htonl(ssrc);

  VSX_DEBUG_SRTP(
    LOG(X_DEBUG("SRTP - srtp_create key %s length:%d, ssrc: 0x%x"), 
                srtp_streamTypeStr(pCtxt->kt.type.authType, pCtxt->kt.type.confType, 1), 
                pCtxt->kt.k.lenKey, htonl(pSrtp->ssrc)); 
    LOGHEX_DEBUG(pCtxt->kt.k.key, pCtxt->kt.k.lenKey);
  )

  if(rc >= 0 && (srtp_rc = srtp_create(&pSrtp->srtp_ctx, &pSrtp->policy)) != err_status_ok) {
    LOG(X_ERROR("SRTP srtp_create failed with code %d"), srtp_rc);
    pSrtp->ssrc = 0;
    pSrtp->srtp_ctx = NULL;
    rc = -1;
  }

  if(rc >= 0 && pSrtp->policy.ssrc.type == ssrc_specific &&
     (srtp_rc = srtp_add_stream(pSrtp->srtp_ctx, &pSrtp->policy)) != err_status_ok) {
    LOG(X_ERROR("SRTP srtp_add_stream failed with code %d"), srtp_rc);
    rc = -1;
  }

  if(rc >= 0) {
    pthread_mutex_init(&pSrtp->mtx, NULL);
  } else {
    avc_free((void **) &pCtxt->privData);
  }

  return rc;
}

static int closeStream(SRTP_CTXT_T *pCtxt) {
  int rc = 0;
  SRTP_CTXT_INT_T *pSrtp;
  err_status_t srtp_rc;

  if(!pCtxt) {
    return -1;
  }

  //pCtxt->pSrtpShared = NULL;
  pCtxt->kt.k.lenKey = 0;

  if(!(pSrtp = (SRTP_CTXT_INT_T *) pCtxt->privData)) {
    return 0;
  }

  if(rc >= 0 && pSrtp->srtp_ctx && pSrtp->policy.ssrc.type == ssrc_specific && pSrtp->ssrc != 0 &&
     (srtp_rc = srtp_remove_stream(pSrtp->srtp_ctx, pSrtp->ssrc)) != err_status_ok) {
    //LOG(X_ERROR("SRTP srtp_remove_stream failed with code %d for ssrc:0x%x"), srtp_rc, htonl(pSrtp->ssrc));
    //rc = -1;
  }

  if(pSrtp->srtp_ctx && (srtp_rc = srtp_dealloc(pSrtp->srtp_ctx)) != err_status_ok) {
    LOG(X_ERROR("SRTP srtp_dealloc failed with code %d"), srtp_rc);
  }

  pthread_mutex_destroy(&pSrtp->mtx);

  pSrtp->srtp_ctx = NULL;
  free(pCtxt->privData);
  pCtxt->privData = NULL;

  return rc;
}


#if defined(VSX_HAVE_STREAMER)

int srtp_setDtlsKeys(SRTP_CTXT_T *pCtxt, const unsigned char *pDtlsKeys, unsigned int lenKeys, int client, int is_rtcp) {
  int rc = 0;
  int lengthEncBits, lengthSaltBits;
  int keyLengthBytes;
  const unsigned char *clientEnc = NULL;
  const unsigned char *clientSalt = NULL;
  const unsigned char *serverEnc = NULL;
  const unsigned char *serverSalt = NULL;

  if((lengthSaltBits = getSaltLengthBits(pCtxt->kt.type.authType)) < 0 ||
     (lengthEncBits = getKeyLengthBits(pCtxt->kt.type.confType)) < 0) {
    return -1;
  }

  keyLengthBytes = (lengthEncBits + lengthSaltBits + ((lengthEncBits + lengthSaltBits) %8)) / 8;

  if(keyLengthBytes >  SRTP_KEYLEN_MAX) {
    return -1;
  } else if(keyLengthBytes * 2 > lenKeys) {
    return -1;
  }

  clientEnc = pDtlsKeys; 
  serverEnc = &clientEnc[lengthEncBits / 8];
  clientSalt = &serverEnc[lengthEncBits / 8];
  serverSalt = &clientSalt[lengthSaltBits / 8];

  //
  // RFC 5764 4.2 Key Derivation
  //
  if(client) {
    if(0&&is_rtcp) {
      memcpy(pCtxt->kt.k.key, serverEnc, lengthEncBits / 8);
      memcpy(&pCtxt->kt.k.key[lengthEncBits / 8], clientSalt, lengthSaltBits / 8);
    } else {
      memcpy(pCtxt->kt.k.key, clientEnc, lengthEncBits / 8);
      memcpy(&pCtxt->kt.k.key[lengthEncBits / 8], clientSalt, lengthSaltBits / 8);
    }
  } else {
    if(0&&is_rtcp) {
      memcpy(pCtxt->kt.k.key, clientEnc, lengthEncBits / 8);
      memcpy(&pCtxt->kt.k.key[lengthEncBits / 8], serverSalt, lengthSaltBits / 8);
    } else {
      memcpy(pCtxt->kt.k.key, serverEnc, lengthEncBits / 8);
      memcpy(&pCtxt->kt.k.key[lengthEncBits / 8], serverSalt, lengthSaltBits / 8);
    }
  }

  pCtxt->kt.k.lenKey = keyLengthBytes;

  //LOG(X_DEBUG("SRTP_SETDTLSKEYS client:%d, lengthEncBits:%d, lengthSaltBits:%d, keyLengthBytes:%d, lenKeys:%d, srtp-key:"), client, lengthEncBits, lengthSaltBits, keyLengthBytes, lenKeys); LOGHEX_DEBUG(pCtxt->k.key, pCtxt->k.lenKey);

  return rc;
}

int srtp_createKey(SRTP_CTXT_T *pCtxt) {
  int lengthEncBits, lengthSaltBits;
  int keyLengthBytes;

  if((lengthSaltBits = getSaltLengthBits(pCtxt->kt.type.authType)) < 0 || 
     (lengthEncBits = getKeyLengthBits(pCtxt->kt.type.confType)) < 0) {
    return -1;
  }

  keyLengthBytes = (lengthEncBits + lengthSaltBits + ((lengthEncBits + lengthSaltBits) %8)) / 8;

  if(keyLengthBytes >  SRTP_KEYLEN_MAX) {
    return -1;
  }

  if(srtp_initialize() < 0) {
    return -1;
  }

  if(pCtxt->kt.k.lenKey > 0 && pCtxt->kt.k.lenKey < keyLengthBytes) {
    LOG(X_ERROR("Specified key size %d is below required %d bytes"), pCtxt->kt.k.lenKey, keyLengthBytes);
    return -1;
  } else if(pCtxt->kt.k.lenKey > 0) {
    //
    // A pre-defined key has already been assigned by prior configuration
    //
    return 0;
  } else if(crypto_get_random(pCtxt->kt.k.key, keyLengthBytes) != err_status_ok) {
    LOG(X_ERROR("Failed to generate SRTP key size%d"), keyLengthBytes);
    return -1;
  }

  pCtxt->kt.k.lenKey = keyLengthBytes;

  return pCtxt->kt.k.lenKey;
}

int srtp_encryptPacket(const SRTP_CTXT_T *pCtxt, const unsigned char *pIn, unsigned int lenIn,
                       unsigned char *pOut, unsigned int *pLenOut, int rtcp) {
  int rc = 0;
  SRTP_CTXT_INT_T *pSrtp = NULL;
  unsigned int szOut;
  err_status_t srtp_rc;

  VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - encrypt packet len:%d"), lenIn));

  if(!pCtxt || !pIn || !lenIn || !pOut || !pLenOut || !(pSrtp = (SRTP_CTXT_INT_T *) pCtxt->privData)) {
    return -1;
  } else if(pCtxt->kt.k.lenKey <= 0) {
    return rc;
  } else if(*pLenOut < lenIn) {
    return -1;
  }

  szOut = *pLenOut;
  memcpy(pOut, pIn, lenIn);
  *pLenOut = lenIn;

  if(rtcp) {
    if(lenIn < RTCP_HDR_LEN) {
      LOG(X_ERROR("Invalid SRTP RTCP packet length %d"), lenIn);
      return -1; 
    }

    VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - calling protect_rtcp ssrc:0x%x, len:%d"), 
                                htonl(((RTCP_PKT_HDR_T *) pIn)->ssrc), lenIn)
                    //LOGHEX_DEBUG(pData, MIN(32, *plength));
                  ); 

    pthread_mutex_lock(&pSrtp->mtx);

    if((srtp_rc = srtp_protect_rtcp(pSrtp->srtp_ctx, pOut, (int *) pLenOut)) != err_status_ok) {
      LOG(X_ERROR("SRTP srtp_protect_rtcp failed with code %d for length %d, ssrc: 0x%x, %s key size:%d"), 
            srtp_rc, *pLenOut, htonl(pSrtp->ssrc), 
            srtp_streamTypeStr(pCtxt->kt.type.authType, pCtxt->kt.type.confType, 1), pCtxt->kt.k.lenKey);
      pthread_mutex_unlock(&pSrtp->mtx);
      return -1;
    }
    //fprintf(stderr, "SRTP RTCP_PROTECT done ssrc:0x%x, len:%d\n", htonl(((RTCP_PKT_HDR_T *) pIn)->ssrc), *pLenOut);

    pthread_mutex_unlock(&pSrtp->mtx);

  } else {
    if(lenIn < RTP_HEADER_LEN) {
      LOG(X_ERROR("Invalid SRTP RTCP packet length %d"), lenIn);
      return -1; 
    }

    pthread_mutex_lock(&pSrtp->mtx);

    VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - calling protect_rtp ssrc:0x%x, len:%d"), 
                                htonl(((struct rtphdr *) pIn)->ssrc), lenIn)
                    //LOGHEX_DEBUG(pIn, MIN(32, *plength));
                  ); 

    if((srtp_rc = srtp_protect(pSrtp->srtp_ctx, pOut, (int *) pLenOut)) != err_status_ok) {
      LOG(X_ERROR("SRTP srtp_protect_rtp failed with code %d for length %d, ssrc: 0x%x, %s key size:%d"), 
            srtp_rc, *pLenOut, htonl(pSrtp->ssrc), 
            srtp_streamTypeStr(pCtxt->kt.type.authType, pCtxt->kt.type.confType, 1), pCtxt->kt.k.lenKey);
      pthread_mutex_unlock(&pSrtp->mtx);
      return -1;
    }

    pthread_mutex_unlock(&pSrtp->mtx);

  }

  //fprintf(stderr, "SRTP protect done lenIn:%d, lenOut:%d, key:%d, ssrc:0x%x ", lenIn, *pLenOut, pCtxt->kt.k.lenKey, htonl(pSrtp->ssrc)); avc_dumpHex(stderr, pOut, MIN(16, *pLenOut), 1);

  if(rc >=0) {
    rc = *pLenOut;
  }

  return rc;
}

int srtp_initOutputStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int rtcp) {
  int rc = 0;

  VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - init output 0x%x ssrc:0x%x, rtcp: %d, key-len:%d"), 
                              pCtxt, ssrc, rtcp, pCtxt->kt.k.lenKey));

  if(!pCtxt || pCtxt->kt.k.lenKey <= 0) {
    return -1;
  }

  if(srtp_initialize() < 0) {
    return -1;
  }

  if(pCtxt->privData) {
    srtp_closeOutputStream(pCtxt);
  }

  rc = initStream(pCtxt, ssrc, 1, rtcp);

  return rc;
}

int srtp_closeOutputStream(SRTP_CTXT_T *pCtxt) {
  int rc = 0;

  VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - close output 0x%x"), pCtxt));

  rc = closeStream(pCtxt);

  return rc;
}

#endif // (VSX_HAVE_STREAMER)


int srtp_initInputStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int rtcp) {
  int rc;

  VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - init input 0x%x ssrc:0x%x, rtcp: %d, key-len:%d"), 
                              pCtxt, ssrc, rtcp, pCtxt->kt.k.lenKey));

  if(!pCtxt || pCtxt->kt.k.lenKey <= 0) {
    return -1;
  }

  if(srtp_initialize() < 0) {
    return -1;
  }

  if(pCtxt->privData) {
    srtp_closeInputStream(pCtxt);
  }

  rc = initStream(pCtxt, ssrc, 0, rtcp);

  return rc;
}

int srtp_closeInputStream(SRTP_CTXT_T *pCtxt) {
  int rc = 0;

  VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - close input 0x%x"), pCtxt));

  rc = closeStream(pCtxt);

  return rc;
}

int srtp_decryptPacket(const SRTP_CTXT_T *pCtxt, unsigned char *pData, unsigned int *plength, int rtcp) {
  int rc = 0;
  SRTP_CTXT_INT_T *pSrtp = NULL;
  unsigned int length;
  err_status_t srtp_rc;

  VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - decrypt packet len:%d"), *plength));

  if(!pCtxt || !pData || !plength || !(pSrtp = (SRTP_CTXT_INT_T *) pCtxt->privData)) {
    return -1;
  } else if(pCtxt->kt.k.lenKey <= 0) {
    return rc;
  }

  length = *plength;

  if(rtcp) {
    if(length < RTCP_HDR_LEN) {
      LOG(X_ERROR("Invalid SRTP RTCP packet length %d"), length);
      return -1; 
    }

    VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - calling unprotect_rtcp ssrc:0x%x, len:%d"), 
                                htonl(((RTCP_PKT_HDR_T *) pData)->ssrc), *plength)
                    //LOGHEX_DEBUG(pData, MIN(32, *plength));
                  ); 

    pthread_mutex_lock(&pSrtp->mtx);

    if((srtp_rc = srtp_unprotect_rtcp(pSrtp->srtp_ctx, pData, (int *) plength)) != err_status_ok) {
      LOG(X_ERROR("SRTP srtp_unprotect_rtcp failed with code %d for length %d, ssrc: 0x%x, %s key size:%d"), 
            srtp_rc, length, htonl(pSrtp->ssrc), 
            srtp_streamTypeStr(pCtxt->kt.type.authType, pCtxt->kt.type.confType, 1), pCtxt->kt.k.lenKey);
      //char buf[128]; base64_encode(pCtxt->kt.k.key, pCtxt->kt.k.lenKey, buf, sizeof(buf)); LOG(X_DEBUG("SRTP key: %s"), buf);
      LOG(X_DEBUG("SRTP key used to unprotect was: ")); LOGHEX_DEBUG(pCtxt->kt.k.key, pCtxt->kt.k.lenKey); 
      pthread_mutex_unlock(&pSrtp->mtx);
      return -1;
    }
    //LOG(X_DEBUG("SRTP RTCP_UNPROTECT done ssrc:0x%x, len:%d"), htonl(((RTCP_PKT_HDR_T *) pData)->ssrc), *plength);

    pthread_mutex_unlock(&pSrtp->mtx);

  } else {
    if(length < RTP_HEADER_LEN) {
      LOG(X_ERROR("Invalid SRTP RTCP packet length %d"), length);
      return -1; 
    }

    pthread_mutex_lock(&pSrtp->mtx);

    VSX_DEBUG_SRTP( LOG(X_DEBUG("SRTP - calling unprotect_rtp ssrc:0x%x"), 
                    htonl(((struct rtphdr *) pData)->ssrc)); 
                    //LOGHEX_DEBUG(pData, MIN(32, *plength));
                  );
    if((srtp_rc = srtp_unprotect(pSrtp->srtp_ctx, pData, (int *) plength)) != err_status_ok) {
      LOG(X_ERROR("SRTP srtp_unprotect_rtp failed with code %d for length %d, ssrc: 0x%x, %s key size:%d"), 
            srtp_rc, length, htonl(pSrtp->ssrc), 
            srtp_streamTypeStr(pCtxt->kt.type.authType, pCtxt->kt.type.confType, 1), pCtxt->kt.k.lenKey);

      //char buf[128]; base64_encode(pCtxt->kt.k.key, pCtxt->kt.k.lenKey, buf, sizeof(buf)); LOG(X_DEBUG("KEY USED IS: %s"), buf); LOGHEX_DEBUG(pCtxt->kt.k.key, pCtxt->kt.k.lenKey);
      pthread_mutex_unlock(&pSrtp->mtx);
      return -1;
    }

    pthread_mutex_unlock(&pSrtp->mtx);

  }

  //fprintf(stderr, "SRTP protect done lenIn:%d, lenOut:%d, key:%d, ssrc:0x%x ", lenIn, *pLenOut, pCtxt->kt.k.lenKey, htonl(pSrtp->ssrc)); avc_dumpHex(stderr, pOut, MIN(16, *pLenOut), 1);

  if(rc >=0) {
    rc = *plength;
  }

  return rc;
}

#else // (VSX_HAVE_SRTP)

int srtp_encryptPacket(const SRTP_CTXT_T *pCtxt, const unsigned char *pIn, unsigned int lenIn,
                       unsigned char *pOut, unsigned int *pLenOut, int rtcp) {
  return -1;
}
int srtp_initOutputStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int rtcp) {
  return -1;
}
int srtp_closeOutputStream(SRTP_CTXT_T *pCtxt) {
  return -1;
}

int srtp_initInputStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int rtcp) {
  return -1;
}
int srtp_closeInputStream(SRTP_CTXT_T *pCtxt) {
  return -1;
}
int srtp_decryptPacket(const SRTP_CTXT_T *pCtxt, unsigned char *pData, unsigned int *plength, int rtcp) {
  return -1;
}

#endif // (VSX_HAVE_SRTP)
