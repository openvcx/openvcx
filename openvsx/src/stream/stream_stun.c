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
#include "stream/stream_turn.h"

#if defined(VSX_HAVE_CAPTURE)

static int stun_create_binding_response(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, 
                                        const STUN_MSG_T *pBindingReq,
                                        const struct sockaddr *psaSrc,
                                        unsigned char *pout, unsigned int lenout) {
  int rc = 0;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = STUN_MSG_TYPE_BINDING_RESPONSE_SUCCESS;
  memcpy(pktXmit.id, pBindingReq->id, STUN_ID_LEN);

  //if((pAttrib = stun_findattrib(STUN_ATTRIB_USERNAME))) {
  //}

  //
  // Create MAPPED-ADDRESS
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  //memset(pAttr, 0, sizeof(STUN_ATTRIB_T));
  pAttr->type = STUN_ATTRIB_MAPPED_ADDRESS;
  STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).port = htons(PINET_PORT(psaSrc));
  if(psaSrc->sa_family == AF_INET6) {
    pAttr->length = 20;
    STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).family = STUN_MAPPED_ADDRESS_FAMILY_IPV6;
    memcpy(& STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).u_addr.ipv6[0], 
           &((const struct sockaddr_in6 *) psaSrc)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
  } else {
    pAttr->length = 8;
    STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).family = STUN_MAPPED_ADDRESS_FAMILY_IPV4;
    STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).u_addr.ipv4.s_addr = ((const struct sockaddr_in *) psaSrc)->sin_addr.s_addr;
  }
  pktXmit.numAttribs++;

  //
  // Clone MAPPED-ADDRESS to crate XOR_MAPPED_ADDRESS
  //
  memcpy(&pktXmit.attribs[pktXmit.numAttribs], &pktXmit.attribs[pktXmit.numAttribs - 1], sizeof(STUN_ATTRIB_T));
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_XOR_MAPPED_ADDRESS;

  //
  // Add MESSAGE-INTEGRITY
  //
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_MESSAGE_INTEGRITY;

  //
  // Add FINGERPRINT 
  //
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_FINGERPRINT;

  if((rc = stun_serialize(&pktXmit, pout, lenout, pIntegrity)) < 0) {
    return rc;
  }

  return rc;
}

static int stun_onrcv_binding_request(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg, 
                                      const struct sockaddr *psaSrc, const struct sockaddr *psaLocal, 
                                      NETIO_SOCK_T *pnetsock, int is_turn) {
  int rc = 0;
  int len = 0;
  char logbuf[128];
  unsigned char buf[STUN_MAX_SZ];
  unsigned char *pout = buf;
  const struct sockaddr *psaReplyTo = psaSrc;

  PSTUNSOCK(pnetsock)->stunFlags |= STUN_SOCKET_FLAG_RCVD_BINDINGREQ;
  PSTUNSOCK(pnetsock)->tmLastRcv = timer_GetTime();
  
  //
  // Update the last STUN binding request receive address only the socket is not using
  // TURN or the source is not the same as the TURN server control port (to prevent
  // responding to TURN data / channel encapsulated messages
  //
  if(!(pnetsock->turn.use_turn_indication_in || pnetsock->turn.use_turn_indication_out) ||
      !INET_IS_SAMEADDR(pnetsock->turn.saTurnSrv, PSTUNSOCK(pnetsock)->sainLastRcv) ||
       INET_PORT(pnetsock->turn.saTurnSrv) != INET_PORT(PSTUNSOCK(pnetsock)->sainLastRcv)) {

//TOOD: if the packet comes from the TURN peer's relay port then we should still set replies to the new port and turn off turn_indication_out 
    memcpy(&PSTUNSOCK(pnetsock)->sainLastRcv, psaSrc, INET_SIZE(*psaSrc));
  }

  //
  // Create a STUN binding response for the request
  //
  if((rc = stun_create_binding_response(pIntegrity, pMsg, psaSrc, buf, sizeof(buf))) < 0) {
    return rc;
  }

  len = rc;

#if 0
  LOGHEX_DEBUG(buf, rc);
  STUN_MSG_T msg;
  memset(&msg, 0, sizeof(msg));
  if(stun_parse(&msg, buf, rc) < 0) {
    LOG(X_ERROR("Failed to parse serialized binding response"));
    return -1;
  }
  fprintf(stderr, "stun_validate_crc:%d\n", stun_validate_crc(buf, rc, msg.length+STUN_HDR_LEN));
  fprintf(stderr, "stun_validate_hmac:%d\n", stun_validate_hmac(buf, rc, pIntegrity));
  stun_dump(stderr, &msg);

#endif 

  //TODO: create last xmit timer & throttling feedback so we dont get DOSed as an echo server  20ms

  //if((rc = sendto(PNETIOSOCK_FD(pnetsock), pout, rc, 0, (struct sockaddr *) psaReplyTo, 
  //                sizeof(struct sockaddr_in))) < 0) {
  if((rc = SENDTO_STUN(pnetsock, pout, len, 0, psaReplyTo)) < 0) {
      LOG(X_ERROR("Failed to reply with STUN Binding Response id:"STUN_ID_FMT_STR" %s for %d bytes"),
           STUN_ID_FROMBUF_FMT_ARGS(buf), stream_log_format_pkt(logbuf, sizeof(logbuf), psaLocal, psaReplyTo), len);
  } else {

    PSTUNSOCK(pnetsock)->stunFlags |= STUN_SOCKET_FLAG_SENT_BINDINGRESP;

    LOG(X_DEBUG("Sent STUN Binding Response to %s, id:"STUN_ID_FMT_STR", length: %d"), 
         stream_log_format_pkt(logbuf, sizeof(logbuf), psaLocal, psaReplyTo),
         STUN_ID_FROMBUF_FMT_ARGS(buf), len);
    //LOGHEX_DEBUG(buf, len);  
  }

  return rc;
}

static int stun_onrcv_binding_response(STUN_CTXT_T *pStun, const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, 
                                       const STUN_MSG_T *pMsg, const struct sockaddr *psaSrc, 
                                       const struct sockaddr *psaLocal, NETIO_SOCK_T *pnetsock, int is_turn) {
  int rc = 0;
  char logbuf[128];


#if 0
  LOGHEX_DEBUG(buf, rc);
  STUN_MSG_T msg;
  memset(&msg, 0, sizeof(msg));
  if(stun_parse(&msg, buf, rc) < 0) {
    LOG(X_ERROR("Failed to parse serialized binding response"));
    return -1;
  }
  fprintf(stderr, "stun_validate_crc:%d\n", stun_validate_crc(buf, rc, msg.legnth+STUN_HDR_LENGTH));
  fprintf(stderr, "stun_validate_hmac:%d\n", stun_validate_hmac(buf, rc, pIntegrity));
  stun_dump(stderr, &msg);

#endif 

  if(pMsg->type == STUN_MSG_TYPE_BINDING_RESPONSE_SUCCESS) {

    PSTUNSOCK(pnetsock)->stunFlags |= STUN_SOCKET_FLAG_RCVD_BINDINGRESP;
    PSTUNSOCK(pnetsock)->tmLastRcv = timer_GetTime();
    //
    // Reset the last binding request time to prevent sending out a new binding request from the last sent queue
    //
    pStun->tmLastReq = 0;

    //
    // If the socket is marked to use a TURN relay but the response received without using a TURN relay 
    // then mark the socket to bypass the TURN relay if the turn policy allows it.
    //
    if(!is_turn && pnetsock->turn.use_turn_indication_out > 0 && 
       pnetsock->turn.turnPolicy == TURN_POLICY_IF_NEEDED) {
      pnetsock->turn.use_turn_indication_out = -1;
    }

  } else if(pMsg->type == STUN_MSG_TYPE_BINDING_RESPONSE_ERROR) {
    LOG(X_WARNING("Received STUN %s %s length:%d, id:"STUN_ID_FMT_STR")"), stun_type2str(pMsg->type), 
         capture_log_format_pkt(logbuf, sizeof(logbuf), psaSrc, psaLocal), pMsg->length + STUN_HDR_LEN, 
         STUN_ID_FMT_ARGS(pMsg->id));
  }

  return rc;
}

static const char *stun_msglabel(int method) {
  switch(method) {
    case TURN_MSG_METHOD_ALLOCATE:
    case TURN_MSG_METHOD_REFRESH:
    case TURN_MSG_METHOD_DATA:
    case TURN_MSG_METHOD_SEND:
    case TURN_MSG_METHOD_CREATEPERM:
    case TURN_MSG_METHOD_CHANNELBIND:
      return "TURN";
    default:
      return "STUN";
  }
}

static char *stun_print_packet(const STUN_MSG_T *pStunPkt, unsigned int rawLen, 
                               const struct sockaddr *psaSrc, const struct sockaddr *psaLocal, char *buf, 
                               unsigned int sz) {

  const STUN_ATTRIB_T *pAttr = NULL;
  //int pktClass = STUN_MSG_CLASS(pStunPkt->type);
  int pktMethod = STUN_MSG_METHOD(pStunPkt->type);
  char tmp[128];
  char buftmp[2][128];

  if(pStunPkt->type == TURN_MSG_TYPE_DATA_REQUEST &&
    (pAttr = stun_findattrib(pStunPkt, TURN_ATTRIB_XOR_PEER_ADDRESS))) {

    snprintf(buftmp[1], sizeof(buftmp[1]), " (original-peer-address: %s:%d)", 
             //inet_ntoa(TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr).u_addr.ipv4), 
             inet_ntop(TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr).family == STUN_MAPPED_ADDRESS_FAMILY_IPV6 ?
                       AF_INET6 : AF_INET,
                       TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr).family == STUN_MAPPED_ADDRESS_FAMILY_IPV6 ?
                       &TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr).u_addr.ipv6[0] :
                       &TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr).u_addr.ipv4.s_addr, tmp, sizeof(tmp)),
             TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr).port);

  } else {
    buftmp[1][0] = '\0';
  }

  snprintf(buf, sz, "%s %s %s%s length:%d, (id:"STUN_ID_FMT_STR")",
         stun_msglabel(pktMethod), stun_type2str(pStunPkt->type),
         capture_log_format_pkt(buftmp[0], sizeof(buftmp[0]), psaSrc, psaLocal), buftmp[1],
         rawLen, STUN_ID_FMT_ARGS(pStunPkt->id));

  return buf;
}


int stun_onrcv(STUN_CTXT_T *pCtxt, TURN_CTXT_T *pTurn, const unsigned char *pData, unsigned int len, 
               const struct sockaddr *psaSrc, const struct sockaddr *psaLocal, 
               NETIO_SOCK_T *pnetsock, int is_turn) {
  int rc = 0;
  int ignored = 0;
  int pktClass;
  int pktMethod;
  char buf[512];
  STUN_MESSAGE_INTEGRITY_PARAM_T *pHmac = NULL;
  STUN_MSG_T pktRcv;
  //const struct sockaddr *psaReplyTo = psaSrc;

  if(!pCtxt || !pData || !psaSrc || !pnetsock) {
    return -1;
  }

  if((rc = stun_parse(&pktRcv, pData, len)) < 0) {
    return rc;
  }

  pktClass = STUN_MSG_CLASS(pktRcv.type);
  pktMethod = STUN_MSG_METHOD(pktRcv.type);

  
  LOG(X_DEBUG("Received %s%s"), stun_print_packet(&pktRcv, len, psaSrc, psaLocal, buf, sizeof(buf)),
      is_turn ? " via TURN" : "");
  //LOGHEX_DEBUG(pData, len);

  if((rc = stun_validate_crc(pData, len)) == -1) {
    //LOG(X_ERROR("%s %s message %s id:"STUN_ID_FMT_STR", length:%d has incorrect fingerprint"), 
    //     stun_msglabel(pktMethod), stun_type2str(pktRcv.type),  capture_log_format_pkt(buf, sizeof(buf), 
    //     psaSrc, psaLocal), STUN_ID_FMT_ARGS(pktRcv.id), len);

    LOG(X_DEBUG("Incorrect fingerprint for %s"), 
         stun_print_packet(&pktRcv, len, psaSrc, psaLocal, buf, sizeof(buf)));
    LOGHEX_DEBUG(pData, len);
    return -1;
  } 

  //
  // Check if we should use the TURN message auth context which should be long-term-auth
  //
  if(pktMethod == TURN_MSG_METHOD_ALLOCATE || pktMethod == TURN_MSG_METHOD_REFRESH || 
     pktMethod == TURN_MSG_METHOD_CREATEPERM || pktMethod == TURN_MSG_METHOD_DATA ||
     pktMethod == TURN_MSG_METHOD_CHANNELBIND) {

    if(pTurn) {
      pHmac = &pTurn->integrity;
    }
    //LOG(X_DEBUG("YES TRY TURN HMAC: 0x%x, pStun: 0x%x, pTurn: 0x%x"), pHmac, pCtxt, pTurn);

  //
  // STUN response will use peer's ICE ufrag/pass auth context
  //
  } else if(pktClass == STUN_MSG_CLASS_RESPONSE_SUCCESS || pktClass == STUN_MSG_CLASS_RESPONSE_ERROR) {
    if(pCtxt->pPeer && pCtxt->pPeer->policy != STUN_POLICY_NONE) {
      pHmac = &pCtxt->pPeer->integrity;
    } else {
    }
  } else {
    pHmac = &pCtxt->integrity;
  }

//LOG(X_DEBUG("pktClass: 0x%x, pktMethod: 0x%x"), pktClass, pktMethod);
//if(pTurn) LOG(X_DEBUG("pTurn user:'%s', pass:'%s', realm:'%s'"), pTurn->integrity.user, pTurn->integrity.pass, pTurn->integrity.realm);
//if(pHmac) LOG(X_DEBUG("pHmac user:'%s', pass:'%s', realm:'%s'"), pHmac->user, pHmac->pass, pHmac->realm); 
//LOG(X_DEBUG("pCtxt->integrity user:'%s', pass:'%s', realm:'%s'"), pCtxt->integrity.user, pCtxt->integrity.pass, pCtxt->integrity.realm); 
//if(pCtxt->pPeer) LOG(X_DEBUG("pCtxt->pPeer->integrity user:'%s', pass:'%s', realm:'%s'"), pCtxt->pPeer->integrity.user, pCtxt->pPeer->integrity.pass, pCtxt->pPeer->integrity.realm); 
//LOGHEX_DEBUG(pData, len);
//LOG(X_DEBUG("TRY HMAC... 0x%x, pass:'%s', message-integrity-attrib:0x%x"), pHmac, pHmac ? pHmac->pass : "no pass", stun_findattrib(&pktRcv, STUN_ATTRIB_MESSAGE_INTEGRITY));
//TODO: handle channel bind messages

  if(pHmac && pHmac->pass && ((rc = stun_validate_hmac(pData, len, pHmac)) == -1 || rc == -2)) {

    //LOG(X_DEBUG("RC:%d"),rc); stun_dump(stderr, &pktRcv);

    LOG(X_DEBUG("Incorrect message-integrity for %s, pass:'%s'"), 
         stun_print_packet(&pktRcv, len, psaSrc, psaLocal, buf, sizeof(buf)), pHmac->pass);
    LOGHEX_DEBUG(pData, len);
    return -1;
  }

  switch(pktClass) {
    case STUN_MSG_CLASS_RESPONSE_SUCCESS:
    case STUN_MSG_CLASS_RESPONSE_ERROR:

      switch(pktMethod) {
        case STUN_MSG_METHOD_BINDING:
          rc = stun_onrcv_binding_response(pCtxt, pHmac, &pktRcv, psaSrc, psaLocal, pnetsock, is_turn);
          break;
#if defined(VSX_HAVE_TURN)
        case TURN_MSG_METHOD_ALLOCATE:
        case TURN_MSG_METHOD_CREATEPERM:
        case TURN_MSG_METHOD_REFRESH:
        case TURN_MSG_METHOD_CHANNELBIND:
          rc = turn_onrcv_response(pTurn, pHmac, &pktRcv, psaSrc, psaLocal);
          break;
#endif // (VSX_HAVE_TURN)
        default:
          ignored = 1;
          break;
      }
      break;

    case STUN_MSG_CLASS_REQUEST:
      rc = stun_onrcv_binding_request(pHmac, &pktRcv, psaSrc, psaLocal, pnetsock, is_turn);
      break;

    case STUN_MSG_CLASS_INDICATION:

      switch(pktMethod) {
#if defined(VSX_HAVE_TURN)
        case TURN_MSG_METHOD_DATA:
          rc = turn_onrcv_indication_data(pTurn, pHmac, &pktRcv, psaSrc, psaLocal);
          break;
#endif // (VSX_HAVE_TURN)
        default:
          ignored = 1;
          break;
      }
   
      //LOG(X_ERROR("%s %s message %s id:"STUN_ID_FMT_STR",length:%d ignored"), 
      //     stun_msglabel(pktMethod), stun_type2str(pktRcv.type), capture_log_format_pkt(buf, sizeof(buf), psaSrc, psaLocal),
      //    STUN_ID_FMT_ARGS(pktRcv.id), len); 
      break;

    default: 
      ignored = 1;
      break;
  }

  if(ignored) {
    //LOG(X_DEBUG("%s %s type: 0x%x, class: 0x%x (0x%02x) id:"STUN_ID_FMT_STR" length:%d %s ignored"), 
    //    stun_msglabel(pktMethod), stun_type2str(pktRcv.type), pktMethod, pktClass, pktRcv.type, 
    //    STUN_ID_FMT_ARGS(pktRcv.id), len, capture_log_format_pkt(buf, sizeof(buf), psaSrc, psaLocal));
    LOG(X_DEBUG("Ignoring %s"), stun_print_packet(&pktRcv, len, psaSrc, psaLocal, buf, sizeof(buf)));
  }

  return rc;
}

#endif // (VSX_HAVE_CAPTURE)

#if defined(VSX_HAVE_STREAMER)

int stun_create_id(STUN_MSG_T *pMsg) {
  int rc = 0;
  unsigned int idx;

  for(idx = 0; idx < STUN_ID_LEN; idx += 2) {
    *((uint16_t *) &pMsg->id[idx]) = (random() % RAND_MAX);
  }

  return rc;
}

static int stun_create_binding_request(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, 
                                       unsigned char *pData, unsigned int len) {
  int rc = 0;
  //unsigned int idx;
  size_t sz;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = STUN_MSG_TYPE_BINDING_REQUEST;
  stun_create_id(&pktXmit);

  //
  // Create USERNAME 
  //
  if(pIntegrity->user && pIntegrity->user[0] != '\0') {
    pAttr = &pktXmit.attribs[pktXmit.numAttribs];
    pAttr->type = STUN_ATTRIB_USERNAME;
    if((sz = strlen(pIntegrity->user)) > STUN_STRING_MAX) {
      LOG(X_WARNING("Truncating STUN username field from %d to %d"), sz, STUN_STRING_MAX);
      sz = STUN_STRING_MAX;
    }
    pAttr->length = sz;
    memcpy(pAttr->u.stringtype.value, pIntegrity->user, pAttr->length);
    pktXmit.numAttribs++;
  }

  //
  // Create ICE-CONTROLLED
  //
  pktXmit.attribs[pktXmit.numAttribs].u.iceControlled.tieBreaker = 0;
  pktXmit.attribs[pktXmit.numAttribs].length = 8;
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_ICE_CONTROLLED;

  //
  // Create USE-CANDIDATE
  //
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_ICE_USE_CANDIDATE;
  pktXmit.attribs[pktXmit.numAttribs].length = 0;

  //
  // Create PRIORITY 
  //
  // RFC 5245
  // 4.1.2.1.  Recommended Formula
  //
  // priority = (2^24)*(type preference) +
  //            (2^8)*(IP priority) +
  //            (2^0)*(256 - component ID)
  //
  //
  // If a host is v4-only, it SHOULD set the IP precedence to 65535.  If a
  // host is v6 or dual stack, the IP precedence SHOULD be the precedence
  // value for IP addresses described in RFC 3484 [RFC3484].
  //
  //    Prefix        Precedence Label
  //    ::1/128               50     0
  //    ::/0                  40     1
  //    2002::/16             30     2
  //    ::/96                 20     3
  //    ::ffff:0:0/96         10     4
  //
  //
  // <component-id>:  ...   For media streams based on RTP, candidates for the actual RTP 
  // media MUST have a component ID of 1, and candidates for RTCP MUST have a component ID of 2.  
  //
  //
  pktXmit.attribs[pktXmit.numAttribs].u.icePriority.value = ((ICE_PREFERENCE_TYPE_HOST << 24) | 
                                                             //(30 << 8) | 0xff);
                                                             (0xff << 8) | 0xff);
  pktXmit.attribs[pktXmit.numAttribs].length = 4;
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_ICE_PRIORITY;

  //
  // Add MESSAGE-INTEGRITY
  //
  if(pIntegrity && pIntegrity->pass && pIntegrity->pass[0] != '\0') {
    pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_MESSAGE_INTEGRITY;
  }

  //
  // Add FINGERPRINT 
  //
  pktXmit.attribs[pktXmit.numAttribs++].type = STUN_ATTRIB_FINGERPRINT;

  if((rc = stun_serialize(&pktXmit, pData, len, pIntegrity)) < 0) {
    return rc;
  }

  return rc;
}

static int stun_send_stun_binding_request(STUN_CTXT_T *pStun, TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, 
                                          const struct sockaddr *psadst, TIME_VAL tm) {
  char logbuf[3][128];
  char tmp[128];
  int len;
  struct sockaddr_storage saLocal;
  TIME_VAL tmnow;
  int do_send_direct = 1;
  int do_send_turn = 0;
  int rc = 0;

  if(pnetsock->turn.use_turn_indication_out) {
    do_send_turn = 1;

    //
    // If we with to communicate via TURN only, then do not send STUN binding requests bypassing any 
    // TURN relay.
    //
    if(pnetsock->turn.turnPolicy == TURN_POLICY_MANDATORY) {
      do_send_direct = 0;
    }
  }

  tmnow = timer_GetTime();

  if(pStun->szLastReq == 0 || (tmnow - pStun->tmLastReq) / TIME_VAL_MS > 1000) {
    //LOG(X_DEBUG("STUN_SEND_BINDING_REQUEST... CREATING..., use_turn_indication_out:%d, turn_channel:%d, turn.saPeerRelay:%s:%d, turn.saTurnSrv.sin_port:%d"), pnetsock->turn.use_turn_indication_out, pnetsock->turn.turn_channel, inet_ntoa(pnetsock->turn.saPeerRelay.sin_addr), htons(pnetsock->turn.saPeerRelay.sin_port), htons(pnetsock->turn.saTurnSrv.sin_port));
    if((rc = stun_create_binding_request(&pStun->integrity, pStun->bufLastReq, sizeof(pStun->bufLastReq))) < 0) {
      return rc;
    }

    //
    // Store the request to be possibly re-sent in the near future to prevent flooding the receiver with many
    // successive STUN binding requests each with a unique message id
    //
    pStun->szLastReq = rc;
    pStun->tmLastReq = tmnow;
    memcpy(pStun->idLastReq, &pStun->bufLastReq[8], STUN_ID_LEN);

  } else {
    
  }

  //TODO: queue the request buffer and re-send a previous one to retain same id on retransmissions to the same dest

#if 0
  LOGHEX_DEBUG(buf, rc);
  STUN_MSG_T msg;
  memset(&msg, 0, sizeof(msg));
  if(stun_parse(&msg, buf, rc) < 0) {
    LOG(X_ERROR("Failed to parse serialized binding request"));
    return -1;
  }
  fprintf(stderr, "stun_validate_crc:%d\n", stun_validate_crc(buf, rc, msg.length + STUN_HDR_LEN));
  fprintf(stderr, "stun_validate_hmac:%d\n", stun_validate_hmac(buf, rc, &pStun->integrity));
  stun_dump(stderr, &msg);

#endif

  if(pnetsock->turn.use_turn_indication_out &&
     (!pnetsock->turn.turn_channel && !pnetsock->turn.have_permission)) {

    LOG(X_DEBUG("STUN request via TURN (%s:%d) to %s not yet sent because TURN relay is not active"),
         FORMAT_NETADDR(pnetsock->turn.saTurnSrv, tmp, sizeof(tmp)), htons(INET_PORT(pnetsock->turn.saTurnSrv)),
         stream_log_format_pkt_sock(logbuf[1], sizeof(logbuf[1]), pnetsock, psadst));

    PSTUNSOCK(pnetsock)->tmLastXmit = tmnow;
    do_send_turn = 0;

  } else if(pTurn && pTurn->active && !turn_can_send_data(&pnetsock->turn)) {

    if((tmnow - pStun->tmLastWarn) / TIME_VAL_MS > 1000) {
      LOG(X_WARNING("Not sending STUN Binding Request because no active TURN relay has been created"));
      pStun->tmLastWarn = tmnow;
    }
    do_send_turn = 0;

  }

  //LOG(X_DEBUG("TRY TO SEND STUN BINDING REQUEST 0...do_send_direct:%d, do_send_turn:%d"), do_send_direct, do_send_turn);
  if(do_send_direct &&
    (rc = SENDTO_STUN_BYPASSTURN(pnetsock, (void *) pStun->bufLastReq, pStun->szLastReq, 0, psadst)) < 0) {
    return rc;
  }

  if(do_send_turn && (rc = SENDTO_STUN(pnetsock, (void *) pStun->bufLastReq, pStun->szLastReq, 0, psadst)) < 0) {
    return rc;
  }

  if(do_send_direct || do_send_turn) {
    PSTUNSOCK(pnetsock)->stunFlags |= STUN_SOCKET_FLAG_SENT_BINDINGREQ;
    PSTUNSOCK(pnetsock)->tmLastXmit = tm;
    memcpy(&PSTUNSOCK(pnetsock)->sainLastXmit, psadst, INET_SIZE(*psadst));

    if(g_verbosity > VSX_VERBOSITY_NORMAL) {
      len = sizeof(saLocal);
      getsockname(PNETIOSOCK_FD(pnetsock), (struct sockaddr *) &saLocal,  (socklen_t *) &len);

      logbuf[1][0] = logbuf[2][0] = '\0';
      if(pStun->integrity.user && pStun->integrity.user[0]) {
        snprintf(logbuf[1], sizeof(logbuf[1]), "user:'%s', ", pStun->integrity.user);
      }
      if(pStun->integrity.pass && pStun->integrity.pass[0]) {
        snprintf(logbuf[2], sizeof(logbuf[2]), "pass:'%s', ", pStun->integrity.pass);
      }
      LOG(X_DEBUG("Sent STUN Binding Request to %s, id:"STUN_ID_FMT_STR", length: %d, %s%s"), 
           stream_log_format_pkt(logbuf[0], sizeof(logbuf[0]), (const struct sockaddr *) &saLocal, psadst),
           STUN_ID_FROMBUF_FMT_ARGS(pStun->bufLastReq), rc, logbuf[1], logbuf[2]);
    }

  }

  return rc;
}

typedef struct STUN_SENDER_WRAP {
  STREAM_RTP_MULTI_T  *pRtp;
  char                tid_tag[LOGUTIL_TAG_LENGTH];
} STUN_SENDER_WRAP_T;

static int check_stun_addr(STREAM_RTP_DEST_T *pDest, int is_rtcp) {
  int rc = 0;
  char tmpbuf[2][64];
  struct sockaddr_storage *pdest_addr = NULL;
  struct sockaddr_storage *pdest_addr_peer = NULL;
  struct sockaddr_storage *pdest_addr_peer_rtcp = NULL;
  struct sockaddr_storage *pdest_addr_rtcppeer = NULL;
  struct sockaddr_storage *pnew_addr = NULL;
  NETIO_SOCK_T *pnetsock = NULL;
  STUN_SOCKET_T *pstunsock = NULL;
  //TIME_VAL tmnow = timer_GetTime();

  if(is_rtcp) {
    pnetsock = STREAM_RTCP_PNETIOSOCK(*pDest);
    pdest_addr = &pDest->saDstsRtcp;
    pdest_addr_rtcppeer  = &pDest->saDsts;
    if(pDest->pDestPeer) {
      pdest_addr_peer = &pDest->pDestPeer->saDstsRtcp;
      pdest_addr_peer_rtcp = &pDest->pDestPeer->saDsts;
    }
  } else {
    pnetsock = STREAM_RTP_PNETIOSOCK(*pDest);
    pdest_addr = &pDest->saDsts;
    pdest_addr_rtcppeer = &pDest->saDstsRtcp;
    if(pDest->pDestPeer) {
      pdest_addr_peer = &pDest->pDestPeer->saDsts;
      pdest_addr_peer_rtcp = &pDest->pDestPeer->saDstsRtcp;
    }
  }
  pstunsock = PSTUNSOCK(pnetsock);

  if(pnetsock->ssl.pDtlsCtxt && pnetsock->ssl.pDtlsCtxt->haveRcvdData) {
    return 0;
  }

  if(pdest_addr_rtcppeer && PINET_PORT(pdest_addr_rtcppeer) == 0) {
    pdest_addr_rtcppeer = NULL;
  }

  if(pdest_addr_peer_rtcp && PINET_PORT(pdest_addr_peer_rtcp) == 0) {
    pdest_addr_peer_rtcp = NULL;
  }

  //
  // Check if we sent out an unresponded STUN binding request and received a binding response from a destination
  // which does not match our outgoing STUN destination
  //
  if(!(pstunsock->stunFlags & STUN_SOCKET_FLAG_UPDATED_DESTADDR) &&
     !(pstunsock->stunFlags & STUN_SOCKET_FLAG_RCVD_BINDINGRESP) &&
     (pstunsock->stunFlags & STUN_SOCKET_FLAG_SENT_BINDINGRESP) &&
      INET_ADDR_VALID(pstunsock->sainLastRcv) &&
      (!INET_IS_SAMEADDR(*pdest_addr, pstunsock->sainLastRcv) ||
        PINET_PORT(pdest_addr) != INET_PORT(pstunsock->sainLastRcv))) {

    pnew_addr = &pstunsock->sainLastRcv;
    snprintf(tmpbuf[0], sizeof(tmpbuf[0]), "%s:%d", FORMAT_NETADDR(*pdest_addr, tmpbuf[1], sizeof(tmpbuf[1])),
             htons(PINET_PORT(pdest_addr)));
    LOG(X_WARNING("STUN changing %s output destination from %s to %s:%d"),
         is_rtcp ? "RTCP" : "RTP", tmpbuf[0], FORMAT_NETADDR(pnew_addr, tmpbuf[1], sizeof(tmpbuf[1])), 
          htons(PINET_PORT(pnew_addr)));

    //
    // If we're using rtp-mux then we should update the peer audio/video address here
    //
    if(pdest_addr_peer && PINET_PORT(pdest_addr) == PINET_PORT(pdest_addr_peer)) {
      LOG(X_DEBUG("STUN changing peer %s output destination"), is_rtcp ? "RTCP" : "RTP");

      //
      // If we're using rtp-mux and rtcp-mux then we should update the peer complementary rtp/rtcp address also
      //
      if(pdest_addr_peer_rtcp && PINET_PORT(pdest_addr_peer) == PINET_PORT(pdest_addr_peer_rtcp)) {
        LOG(X_DEBUG("STUN changing peer %s output destination"), !is_rtcp ? "RTCP" : "RTP");
        memcpy(pdest_addr_peer_rtcp, pnew_addr, INET_SIZE(*pnew_addr));
      }

      memcpy(pdest_addr_peer, pnew_addr, INET_SIZE(*pnew_addr));

    }

    //
    // If we're using rtcp-mux then we should update the complementary rtp/rtcp address also
    //
    if(pdest_addr_rtcppeer && PINET_PORT(pdest_addr) == PINET_PORT(pdest_addr_rtcppeer)) {
      LOG(X_DEBUG("STUN changing %s output destination"), !is_rtcp ? "RTCP" : "RTP");
      memcpy(pdest_addr_rtcppeer, pnew_addr, INET_SIZE(*pnew_addr));
    }

    memcpy(pdest_addr, pnew_addr, INET_SIZE(*pnew_addr));

    pstunsock->stunFlags |= STUN_SOCKET_FLAG_UPDATED_DESTADDR;

    rc = 1;
  }

  return rc;
}


static int is_dest_duplicate(const struct sockaddr_storage sadests[], unsigned int sz, 
                             const struct sockaddr_storage *psadest) {
  unsigned int idx;
  for(idx = 0; idx < sz; idx++) {
    if(!memcmp(&sadests[idx], psadest, INET_SIZE(sadests[idx]))) {
      return 1; 
    }
  }
  return 0;
}

#define STUNREQ_CAN_SEND(stun, psock) ((stun).policy == STUN_POLICY_ENABLED || \
                                    ((stun).policy == STUN_POLICY_XMIT_IF_RCVD && \
                                     (psock)->stunFlags & STUN_SOCKET_FLAG_RCVD_BINDINGREQ))


/*
#define STUNREQ_ISTIME_TO_SEND(minNewPingIntervalMs, minActivePingIntervalMs, psock, tm) \
                                                (((psock)->tmLastRcv == 0 && \
                                                (tm) - (psock)->tmLastXmit > 1000 * (minNewPingIntervalMs)) || \
                                                ((psock)->tmLastRcv > 0 && \
                                                ((tm) - (psock)->tmLastXmit > 1000 * (minActivePingIntervalMs))))
*/
#define STUNREQ_ISTIME_TO_SEND(minNewPingIntervalMs, minActivePingIntervalMs, psock, tm) \
                                                ((!((psock)->stunFlags & STUN_SOCKET_FLAG_RCVD_BINDINGRESP)  && \
                                                (tm) - (psock)->tmLastXmit > 1000 * (minNewPingIntervalMs)) || \
                                                (((psock)->stunFlags & STUN_SOCKET_FLAG_RCVD_BINDINGRESP) && \
                                                ((tm) - (psock)->tmLastXmit > 1000 * (minActivePingIntervalMs))))

void stream_stun_sender_proc(void *pArg) {
  STUN_SENDER_WRAP_T wrap;
  STREAM_RTP_MULTI_T *pRtp = NULL;
  STREAM_RTP_MULTI_T *pRtpCur = NULL;
  STREAM_RTP_DEST_T *pDest = NULL;
  NETIO_SOCK_T *pnetsock = NULL;
  unsigned int idx;
  int numDests, numSent;
  int *pdoStunXmit;
  pthread_mutex_t *pmtx = NULL;
  TIME_VAL tm;
  unsigned int minNewPingIntervalMs;
  unsigned int minActivePingIntervalMs;
  struct sockaddr_storage sadests[IXCODE_VIDEO_OUT_MAX * 2 * 2];

  memcpy(&wrap, pArg, sizeof(wrap));
  pRtp = wrap.pRtp;

  pdoStunXmit = &pRtp->pheadall->doStunXmit;
  pmtx = &pRtp->pheadall->mtx;

  logutil_tid_add(pthread_self(), wrap.tid_tag);

  //pthread_mutex_lock(&pRtp->mtx);
  *pdoStunXmit = 1;
  //pthread_mutex_unlock(&pRtp->mtx);

  LOG(X_DEBUG("STUN message sender thread started"));

  while(*pdoStunXmit == 1 && !g_proc_exit) {

    numDests = 0;
    numSent = 0;
    memset(sadests, 0, sizeof(sadests));
    pthread_mutex_lock(pmtx);
    pRtpCur = pRtp->pheadall;

    while(pRtpCur) {

      tm = timer_GetTime();

      if(pRtpCur->numDests > 0) {
        for(idx = 0; idx < pRtpCur->maxDests; idx++) {

          pDest = &pRtpCur->pdests[idx];
          if(!pDest->isactive || pDest->stun.policy == STUN_POLICY_NONE) {
            continue;
          }

          if((minNewPingIntervalMs = pDest->stun.minNewPingIntervalMs) == 0) {
            minNewPingIntervalMs = STUN_MIN_NEW_PING_INTERVAL_MS;
          }
          if((minActivePingIntervalMs = pDest->stun.minActivePingIntervalMs) == 0) {
            minActivePingIntervalMs = STUN_MIN_ACTIVE_PING_INTERVAL_MS;
          }

          if(!is_dest_duplicate(sadests, numSent, &pDest->saDsts)) {

            pnetsock = STREAM_RTP_PNETIOSOCK(*pDest);
//LOG(X_DEBUG("TRY SEND STUN RTP.. cansend:%d, is_time_to_send:%d, minNewPingIntervalMs:%d, minActivePingIntervalMs:%d, %lld ms ago"), STUNREQ_CAN_SEND(pDest->stun, PSTUNSOCK(pnetsock)),  STUNREQ_ISTIME_TO_SEND(minNewPingIntervalMs, minActivePingIntervalMs, PSTUNSOCK(pnetsock), tm), minNewPingIntervalMs, minActivePingIntervalMs, ((tm) - PSTUNSOCK(pnetsock)->tmLastXmit)/1000 );
            if(STUNREQ_CAN_SEND(pDest->stun, PSTUNSOCK(pnetsock)) &&
               STUNREQ_ISTIME_TO_SEND(minNewPingIntervalMs, minActivePingIntervalMs, PSTUNSOCK(pnetsock), tm)) {

//LOG(X_DEBUG("SENDING STUN RTP tmLastRcv:%llu (%llu) vs %llu, tmLastXmit:%llu (%llu) vs %llu,"), STREAM_RTP_PSTUNSOCK(*pDest)->tmLastRcv, tm - STREAM_RTP_PSTUNSOCK(*pDest)->tmLastRcv, 1000 * pDest->stun.minNewPingIntervalMs, STREAM_RTP_PSTUNSOCK(*pDest)->tmLastXmit,tm -  STREAM_RTP_PSTUNSOCK(*pDest)->tmLastXmit,1000 * pDest->stun.minActivePingIntervalMs);
              //
              // Only alter output address (according ot STUN response address) if not using TURN
              //
              //if(!pnetsock->turn.use_turn_indication_out) {
                check_stun_addr(pDest, 0);
              //}

              if(stun_send_stun_binding_request(&pDest->stun, &pDest->turns[0], pnetsock, 
                                                (const struct sockaddr *) &pDest->saDsts, tm) < 0) {
                pthread_mutex_unlock(pmtx);
                break;
              }
              memcpy(&sadests[numSent++], &pDest->saDsts, sizeof(sadests[0]));
              //STREAM_RTP_PSTUNSOCK(*pDest)->tmLastXmit = tm;
              //memcpy(STREAM_RTP_PSTUNSOCK(*pDest)->sainLastXmit, &pDest->saDsts, sizeof(struct sockaddr_in));
             
            }
            numDests++;
          }

          if(pDest->sendrtcpsr && INET_PORT(pDest->saDstsRtcp) != INET_PORT(pDest->saDsts) &&
            !is_dest_duplicate(sadests, numSent, &pDest->saDstsRtcp)) {

            pnetsock = STREAM_RTCP_PNETIOSOCK(*pDest);

            if(STUNREQ_CAN_SEND(pDest->stun, PSTUNSOCK(pnetsock)) &&
               STUNREQ_ISTIME_TO_SEND(minNewPingIntervalMs, minActivePingIntervalMs, PSTUNSOCK(pnetsock), tm)) {

//LOG(X_DEBUG("SENDING STUN RTCP tmLastRcv:%llu (%llu) vs %llu, tmLastXmit:%llu (%llu) vs %llu,"), STREAM_RTCP_PSTUNSOCK(*pDest)->tmLastRcv, tm - STREAM_RTCP_PSTUNSOCK(*pDest)->tmLastRcv, 1000 * pDest->stun.minNewPingIntervalMs, STREAM_RTCP_PSTUNSOCK(*pDest)->tmLastXmit,tm -  STREAM_RTCP_PSTUNSOCK(*pDest)->tmLastXmit,1000 * pDest->stun.minActivePingIntervalMs);

              //
              // Only alter output address (according ot STUN response address) if not using TURN
              //
              //if(!pnetsock->turn.use_turn_indication_out) {
                check_stun_addr(pDest, 1);
              //}

              if(stun_send_stun_binding_request(&pDest->stun, &pDest->turns[1], pnetsock, 
                                                (const struct sockaddr *) &pDest->saDstsRtcp, tm) < 0) {
                pthread_mutex_unlock(pmtx);
                break;
              }
              memcpy(&sadests[numSent++], &pDest->saDstsRtcp, sizeof(sadests[0]));
              //STREAM_RTCP_PSTUNSOCK(*pDest)->tmLastXmit = tm;
              //memcpy(STREAM_RTCP_PSTUNSOCK(*pDest)->sainLastXmit, &pDest->saDstsRtcp, sizeof(struct sockaddr_in));
            }

            numDests++;
          }


        } // end of for(idx = 0;...

      } // end of if(rtpCur->numDests > 0...
  
      pRtpCur = pRtpCur->pnext;

    } // end of while(pRtpCur...

    pthread_mutex_unlock(pmtx);

    if(g_proc_exit != 0 || numDests <= 0) {
     break;
    }

    usleep(50000);

  }

  LOG(X_DEBUG("STUN message sender thread ending"));

  pthread_mutex_lock(pmtx);
  *pdoStunXmit = -1;
  pthread_mutex_unlock(pmtx);

  logutil_tid_remove(pthread_self());

}


int stream_stun_stop(STREAM_RTP_MULTI_T *pRtp) {

  int rc = 0;
  int *pdoStunXmit;
  pthread_mutex_t *pmtx = NULL;

  if(!pRtp || !pRtp->pheadall) {
    //TODO: pheadall may not be set if this is an MP2TS output (not native rtp transport destination)
    return -1;
  }

  pdoStunXmit = &pRtp->pheadall->doStunXmit;
  pmtx = &pRtp->pheadall->mtx;

  pthread_mutex_lock(pmtx);

  if(*pdoStunXmit <= 0) {
    pthread_mutex_unlock(pmtx);
    return 0;
  } else {
    *pdoStunXmit = 0;
  }

  pthread_mutex_unlock(pmtx);

  while(*pdoStunXmit != -1) {
    usleep(40000);
  }

  return rc;
}

int stream_stun_start(STREAM_RTP_DEST_T *pDest, int lock) {
  int rc = 0;
  const char *s;
  pthread_t ptdCap;
  pthread_attr_t attrCap;
  STUN_SENDER_WRAP_T wrap;
  int *pdoStunXmit;
  pthread_mutex_t *pmtx = NULL;

  if(!pDest || !pDest->pRtpMulti || !pDest->pRtpMulti->pheadall) {
    //TODO: pheadall may not be set if this is an MP2TS output (not native rtp transport destination)
    return -1;
  } 

  pdoStunXmit = &pDest->pRtpMulti->pheadall->doStunXmit;
  if(lock || pDest->pRtpMulti != pDest->pRtpMulti->pheadall) {
    pmtx = &pDest->pRtpMulti->pheadall->mtx;
  }

  if(pmtx) {
    pthread_mutex_lock(pmtx);
  }

  if(*pdoStunXmit > 0) {
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    }
    
    return 0;
  }

  wrap.pRtp = pDest->pRtpMulti;
  wrap.tid_tag[0] = '\0';
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(wrap.tid_tag, sizeof(wrap.tid_tag), "%s-stun", s);
  }

  *pdoStunXmit = 2;
  pthread_attr_init(&attrCap);
  pthread_attr_setdetachstate(&attrCap, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdCap,
                    &attrCap,
                    (void *) stream_stun_sender_proc,
                    (void *) &wrap) != 0) {

    LOG(X_ERROR("Unable to create STUN message sender thread"));
    *pdoStunXmit = 0;
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    }
    return -1;
  }

  if(pmtx) {
    pthread_mutex_unlock(pmtx);
  }

  while(*pdoStunXmit == 2) {
    usleep(5000);
  }

  return rc;
}

typedef struct PARSE_STUNCREDENTIALS_CTXT {
  int rc;
  int isvid;
  char *videoStrBuf;
  char *audioStrBuf;
} PARSE_STUNCREDENTIALS_CTXT_T;

static int parse_pair_cb_credentials(void *pArg, const char *pElement) {

  PARSE_STUNCREDENTIALS_CTXT_T *pParseCtxt = (PARSE_STUNCREDENTIALS_CTXT_T*) pArg;

  if(*pElement != '\0') {
    if(pParseCtxt->isvid) {
      strncpy(pParseCtxt->videoStrBuf, pElement, STUN_STRING_MAX - 1);
    } else {
      strncpy(pParseCtxt->audioStrBuf, pElement, STUN_STRING_MAX - 1);
    }
    pParseCtxt->rc |= 1 << (pParseCtxt->isvid ? 0 : 1);
  }

  pParseCtxt->isvid = !pParseCtxt->isvid;

  return 0;
}

int stream_stun_parseCredentialsPair(const char *in,  char *videoStrBuf, char *audioStrBuf) {
  int rc = 0;
  PARSE_STUNCREDENTIALS_CTXT_T parseCtxt;

  parseCtxt.rc = 0;
  parseCtxt.isvid = 1;
  parseCtxt.videoStrBuf = videoStrBuf;
  parseCtxt.audioStrBuf = audioStrBuf;

  if((rc = strutil_parse_pair(in, (void *) &parseCtxt,  parse_pair_cb_credentials)) < 0) {
    return rc;
  }

  if(videoStrBuf[0] && !audioStrBuf[0]) {
    strncpy(audioStrBuf, videoStrBuf, STUN_STRING_MAX - 1);
  } else if(audioStrBuf[0] && !videoStrBuf[0]) {
    strncpy(videoStrBuf, audioStrBuf, STUN_STRING_MAX - 1);
  }

  return parseCtxt.rc;
}

const char *stun_policy2str(STUN_POLICY_T policy) {
  switch(policy) {
    case STUN_POLICY_NONE:
      return "none";
    case STUN_POLICY_ENABLED:
      return "enabled";
    case STUN_POLICY_XMIT_IF_RCVD:
      return "enabled-if-peer";
    default:
      return "unknown";
  }
}


#endif // (VSX_HAVE_STREAMER)
