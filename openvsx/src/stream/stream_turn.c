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


#define TURN_NONCE_LEN                          8
#define TURN_MSG_RESEND_TIMEOUT_MS              1000
#define TURN_MSG_RECV_TIMEOUT_MS                5000
#define TURN_CHANNEL_ID_MIN                     0x4000
#define TURN_CHANNEL_ID_MAX                     0x7fff

#if defined(VSX_HAVE_TURN)

static int turn_init(TURN_CTXT_T *pTurn, TURN_THREAD_CTXT_T *pThreadCtxt);
static void turn_close(TURN_CTXT_T *pTurn);
static int turn_ctxt_attach(TURN_THREAD_CTXT_T *pThreadCtxt, TURN_CTXT_T *pTurn, 
                            TURN_RELAY_SETUP_RESULT_T *pOnResult);
static void turn_on_connection_closed(TURN_CTXT_T *pTurn, int lock);
static void turn_on_socket_closed(TURN_CTXT_T *pTurn);
static int turn_on_created_allocation(TURN_CTXT_T *pTurn);


static int create_nonce(STUN_ATTRIB_T *pNonce, const STUN_ATTRIB_T *pReuse) {

  //unsigned int idx;
  int rc = 0;
  //unsigned char nonce[TURN_NONCE_LEN * 2];

  pNonce->type = STUN_ATTRIB_NONCE;

  if(pReuse && pReuse->length > 0) {

    //
    // Use a nonce which was previously provided by the TURn server
    //
    if((rc = pReuse->length) > STUN_STRING_MAX) {
      LOG(X_WARNING("Truncating nonce length from %d to %d"), rc, STUN_STRING_MAX);
      rc = STUN_STRING_MAX;
    }
    memcpy(pNonce->u.stringtype.value, pReuse->u.stringtype.value, rc);

  } else {

    //
    // Create a random based nonce value
    //
    if(!(auth_digest_gethexrandom(TURN_NONCE_LEN, pNonce->u.stringtype.value, 
                              sizeof(pNonce->u.stringtype.value)))) {
      return -1;
    }
/*
    for(idx = 0; idx < TURN_NONCE_LEN; idx += 2) {
      *((uint16_t *) &nonce[idx]) = (random() % RAND_MAX);
    }
    if((rc = hex_encode(nonce, TURN_NONCE_LEN, pNonce->u.stringtype.value, 
                        sizeof(pNonce->u.stringtype.value))) < 0) {
      return -1;
    }
*/
  }

  pNonce->length = rc;

  return rc;
}

static int add_attributes_common(STUN_MSG_T *pktXmit, 
                                 const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity,
                                 const STUN_ATTRIB_T *pAttribNonce) {
  int rc = 0;
  STUN_ATTRIB_T *pAttr;
  size_t sz;

  //
  // Create USERNAME 
  //
  if(pIntegrity && pIntegrity->user && pIntegrity->user[0] != '\0') {
    pAttr = &pktXmit->attribs[pktXmit->numAttribs];
    pAttr->type = STUN_ATTRIB_USERNAME;
    if((sz = strlen(pIntegrity->user)) > STUN_STRING_MAX) {
      LOG(X_WARNING("Truncating STUN username field from %d to %d"), sz, STUN_STRING_MAX);
      sz = STUN_STRING_MAX;
    }
    pAttr->length = sz;
    memcpy(pAttr->u.stringtype.value, pIntegrity->user, pAttr->length);
    pktXmit->numAttribs++;
  }

  //
  // Create REALM 
  //
  if(pIntegrity && pIntegrity->realm && pIntegrity->realm[0] != '\0') {
    pAttr = &pktXmit->attribs[pktXmit->numAttribs];
    pAttr->type = STUN_ATTRIB_REALM;
    if((sz = strlen(pIntegrity->realm)) > STUN_STRING_MAX) {
      LOG(X_WARNING("Truncating STUN realm field from %d to %d"), sz, STUN_STRING_MAX);
      sz = STUN_STRING_MAX;
    }
    pAttr->length = sz;
    memcpy(pAttr->u.stringtype.value, pIntegrity->realm, pAttr->length);
    pktXmit->numAttribs++;
  }

  //
  // Create NONCE 
  //
  if(pIntegrity && ((pIntegrity->user && pIntegrity->user[0] != '\0') || 
                    (pIntegrity->realm && pIntegrity->realm[0] != '\0'))) {
    pAttr = &pktXmit->attribs[pktXmit->numAttribs];

    if((rc = create_nonce(pAttr, pAttribNonce)) < 0) {
      LOG(X_ERROR("Failed to create nonce value for TURN request")); 
      return rc;
    }

    pktXmit->numAttribs++;
  }

  //
  // Add MESSAGE-INTEGRITY
  //
  if(pIntegrity && pIntegrity->pass && pIntegrity->pass[0] != '\0') {
    pktXmit->attribs[pktXmit->numAttribs++].type = STUN_ATTRIB_MESSAGE_INTEGRITY;
  }

  //
  // Add FINGERPRINT 
  //
  //pktXmit->attribs[pktXmit->numAttribs++].type = STUN_ATTRIB_FINGERPRINT;

  return rc;
}

static int turn_create_channeldata(const unsigned char *pData, unsigned int szdata,
                            uint16_t channelId,
                            unsigned char *pout, unsigned int lenout) {

  if(channelId < TURN_CHANNEL_ID_MIN || channelId > TURN_CHANNEL_ID_MAX) {
    return -1;
  } else if(lenout < szdata + TURN_CHANNEL_HDR_LEN) {
    return -1;
  }

  //
  // Create a TURN channel data packet with a 4 byte channel binding header
  //
  *((uint16_t *) &pout[0]) = htons(channelId);
  *((uint16_t *) &pout[2]) = htons(szdata);
  memcpy(&pout[TURN_CHANNEL_HDR_LEN], pData, szdata);

  return TURN_CHANNEL_HDR_LEN + szdata;
}

static int turn_create_send_indication(const unsigned char *pData, unsigned int szdata,
                                const struct sockaddr *psadst,
                                unsigned char *pout, unsigned int lenout) {
  int rc = 0;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  if(!pout || !psadst) {
    return -1;
  } else if(!INET_ADDR_VALID(*psadst)) {
    // We probably did not succesfully finish a createpermission to send to the peer address
    LOG(X_ERROR("TURN outgoing send indication does not have a valid relayed address attribute set"));
    return -1;
  }

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = TURN_MSG_TYPE_SEND_REQUEST;
  stun_create_id(&pktXmit);

  //
  // Create XOR-PEER-ADDRESS
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  pAttr->type = TURN_ATTRIB_XOR_PEER_ADDRESS;
  TURN_ATTRIB_VALUE_RELAYED_ADDRESS(*pAttr).port = htons(PINET_PORT(psadst));
  if(psadst->sa_family == AF_INET6) {
    pAttr->length = 20;
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(*pAttr).family = STUN_MAPPED_ADDRESS_FAMILY_IPV6;
    memcpy(& TURN_ATTRIB_VALUE_RELAYED_ADDRESS(*pAttr).u_addr.ipv6[0],
           &((const struct sockaddr_in6 *) psadst)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
  } else {
    pAttr->length = 8;
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(*pAttr).family = STUN_MAPPED_ADDRESS_FAMILY_IPV4;
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(*pAttr).u_addr.ipv4 = ((const struct sockaddr_in *) psadst)->sin_addr;
  }
  pktXmit.numAttribs++;

  //
  // Create DATA
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  pAttr->type = TURN_ATTRIB_DATA;
  pAttr->length = szdata;
  pAttr->u.data.pData = pData;
  pktXmit.numAttribs++;

  if((rc = stun_serialize(&pktXmit, pout, lenout, NULL)) < 0) {
    return rc;
  }

  return rc;
}

int turn_create_datamsg(const unsigned char *pData, unsigned int szdata,
                        const TURN_SOCK_T *pTurnSock,
                        unsigned char *pout, unsigned int lenout) {
  int rc = 0;

  if(!pTurnSock) {
    return -1;
  }

  //
  // Create an outbound data packet to the TURN server.  Use a preferred channel bound transport
  // if a channel binding is present, otherwise try to send as a data indication
  //
  if(pTurnSock->turn_channel > 0) {
    rc = turn_create_channeldata(pData, szdata, pTurnSock->turn_channel, pout, lenout);
  } else if(pTurnSock->have_permission && INET_ADDR_VALID(pTurnSock->saPeerRelay)) {
    rc = turn_create_send_indication(pData, szdata, (const struct sockaddr *) &pTurnSock->saPeerRelay, pout, lenout);
  } else {
    LOG(X_ERROR("Unable to send %d data bytes on TURN socket %s"), szdata,
               !pTurnSock->have_permission ? "without permission" : "");
    return -1;
  }

  //LOG(X_DEBUG("TURN_CREATE_DATAMSG szdata:%d, rc:%d"), szdata, rc);

  return rc;
}


static int create_allocate_request(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, 
                                        unsigned char *pData, unsigned int len,
                                        const STUN_ATTRIB_T *pAttribNonce) {
                                        
  int rc = 0;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = TURN_MSG_TYPE_ALLOCATE_REQUEST;
  stun_create_id(&pktXmit);

  //
  // Create REQUESTED-TRANSPORT 
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  pAttr->type = TURN_ATTRIB_REQUESTED_TRANSPORT;
  pAttr->length = 4;
  pAttr->u.requestedTransport.protocol = IPPROTO_UDP;  
  pktXmit.numAttribs++;

  //
  // Add common attributes such as USERNAME, PASSWORD, REALM, MESSAGE-INTEGRITY
  //
  if((rc = add_attributes_common(&pktXmit, pIntegrity, pAttribNonce)) < 0) {
    return rc;
  }

  if((rc = stun_serialize(&pktXmit, pData, len, pIntegrity)) < 0) {
    return rc;
  }

  return rc;
}

static int create_createpermission_request(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, 
                                                unsigned char *pData, unsigned int len,
                                                const STUN_ATTRIB_T *pAttribNonce,
                                                const STUN_ATTRIB_T *pAttrMappedAddress) {
  int rc = 0;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = TURN_MSG_TYPE_CREATEPERM_REQUEST;
  stun_create_id(&pktXmit);

  //stun_dump_attrib(stderr, &pTurn->lastNonce);

  //
  // Create XOR-PEER-ADDRESS
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  pAttr->type = TURN_ATTRIB_XOR_PEER_ADDRESS;
  pAttr->length = 8;
  memcpy(&TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr), &TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttrMappedAddress), 
        sizeof(TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr)));
  pktXmit.numAttribs++;

  //
  // Add common attributes such as USERNAME, PASSWORD, REALM, MESSAGE-INTEGRITY
  //
  if((rc = add_attributes_common(&pktXmit, pIntegrity, pAttribNonce)) < 0) {
    return rc;
  }

  if((rc = stun_serialize(&pktXmit, pData, len, pIntegrity)) < 0) {
    return rc;
  }

  return rc;
}

static int create_refresh_request(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity,
                                  unsigned char *pData, unsigned int len,
                                  const STUN_ATTRIB_T *pAttribNonce,
                                  int lifetime) {
  int rc = 0;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = TURN_MSG_TYPE_REFRESH_REQUEST;
  stun_create_id(&pktXmit);

  //stun_dump_attrib(stderr, &pTurn->lastNonce);

  //
  // Create LIFETIME
  //
  if(lifetime == 0) {
    pAttr = &pktXmit.attribs[pktXmit.numAttribs];
    pAttr->type = TURN_ATTRIB_LIFETIME;
    pAttr->length = 4;
    pAttr->u.lifetime.value = lifetime;
    pktXmit.numAttribs++;
  }

  //
  // Add common attributes such as USERNAME, PASSWORD, REALM, MESSAGE-INTEGRITY
  //
  if((rc = add_attributes_common(&pktXmit, pIntegrity, pAttribNonce)) < 0) {
    return rc;
  }

  if((rc = stun_serialize(&pktXmit, pData, len, pIntegrity)) < 0) {
    return rc;
  }

  return rc;
}

static int create_channel_request(const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity,
                                  unsigned char *pData, unsigned int len,
                                  const STUN_ATTRIB_T *pAttribNonce,
                                  const STUN_ATTRIB_T *pAttrMappedAddress,
                                  uint16_t channelId) {

  int rc = 0;
  STUN_MSG_T pktXmit;
  STUN_ATTRIB_T *pAttr;

  memset(&pktXmit, 0, sizeof(pktXmit));

  pktXmit.type = TURN_MSG_TYPE_CHANNELBIND_REQUEST;
  stun_create_id(&pktXmit);

  //
  // Create CHANNEL-NUMBER
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  pAttr->type = TURN_ATTRIB_CHANNEL_NUMBER;
  pAttr->length = 4;
  pAttr->u.channel.channel = channelId;
  pktXmit.numAttribs++;

  //
  // Create XOR-PEER-ADDRESS
  //
  pAttr = &pktXmit.attribs[pktXmit.numAttribs];
  pAttr->type = TURN_ATTRIB_XOR_PEER_ADDRESS;
  pAttr->length = 8;
  memcpy(&TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr), &TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttrMappedAddress),
        sizeof(TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(*pAttr)));
  pktXmit.numAttribs++;


  //
  // Add common attributes such as USERNAME, PASSWORD, REALM, MESSAGE-INTEGRITY
  //
  if((rc = add_attributes_common(&pktXmit, pIntegrity, pAttribNonce)) < 0) {
    return rc;
  }

  if((rc = stun_serialize(&pktXmit, pData, len, pIntegrity)) < 0) {
    return rc;
  }

  return rc;
}

static void log_turn_msg(unsigned char *buf, NETIO_SOCK_T *pnetsock, const struct sockaddr *psadst, 
                         const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, int len, 
                         const char *descr, const char *extra) {

  char logbuf[4][128];

  if(g_verbosity > VSX_VERBOSITY_NORMAL) {

    logbuf[1][0] = logbuf[2][0] = logbuf[3][0] = '\0';
    if(pIntegrity && pIntegrity->user && pIntegrity->user[0]) {
      snprintf(logbuf[1], sizeof(logbuf[1]), "user: '%s', ", pIntegrity->user);
    }
    if(pIntegrity && pIntegrity->pass && pIntegrity->pass[0]) {
      snprintf(logbuf[2], sizeof(logbuf[2]), "pass: '%s', ", pIntegrity->pass);
    }
    if(pIntegrity && pIntegrity->realm && pIntegrity->realm[0]) {
      snprintf(logbuf[3], sizeof(logbuf[3]), "realm: '%s', ", pIntegrity->realm);
    }
    LOG(X_DEBUG("%s to %s, (id:"STUN_ID_FMT_STR"), length: %d, %s%s%s%s%s"),
         descr, stream_log_format_pkt_sock(logbuf[0], sizeof(logbuf[0]), pnetsock, psadst),
         STUN_ID_FROMBUF_FMT_ARGS(buf), len, extra ? extra : "", extra ? ", " : "", 
         logbuf[1], logbuf[2], logbuf[3]);
  }

}

static int turn_send_request(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, int sz, int newreq) {
  int rc = 0;
  char tmp[128];

  //
  // Send a TURN protocol message to the TURN server.  The message body is stored for re-sending
  // if a response is not received.
  //

  if((rc = SENDTO_TURN(pnetsock, (void *) pTurn->bufLastReq, sz, 0, &pTurn->saTurnSrv)) < 0) {
    pTurn->szLastReq = 0;
    LOG(X_ERROR("Failed to send TURN %s message to %s:%d"), 
                 stun_type2str( htons(*((uint16_t *) pTurn->bufLastReq))), 
                FORMAT_NETADDR(pTurn->saTurnSrv, tmp, sizeof(tmp)), htons(INET_PORT(pTurn->saTurnSrv))); 
    return rc;
  }

  pTurn->szLastReq = sz;
  pTurn->tmLastReq = timer_GetTime();
  if(newreq) {
    memcpy(pTurn->idLastReq, &pTurn->bufLastReq[8], STUN_ID_LEN);
  }

  return rc;
}

static int turn_send_allocate_request(TURN_CTXT_T *pTurn, 
                                     NETIO_SOCK_T *pnetsock, 
                                     const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity) {
  int rc;

  if((rc = create_allocate_request(pIntegrity, pTurn->bufLastReq, sizeof(pTurn->bufLastReq), 
                                   &pTurn->lastNonce)) < 0) {
    return rc;
  }

#if 0
  LOGHEX_DEBUG(pTurn->bufLastReq, rc);
  STUN_MSG_T msg;
  memset(&msg, 0, sizeof(msg));
  if(stun_parse(&msg, pTurn->bufLastReq, rc) < 0) {
    LOG(X_ERROR("Failed to parse serialized allocate request"));
    return -1;
  }
  fprintf(stderr, "stun_validate_crc:%d\n", stun_validate_crc(pTurn->bufLastReq, msg.length + STUN_HDR_LEN));
  fprintf(stderr, "stun_validate_hmac:%d\n", stun_validate_hmac(pTurn->bufLastReq, rc, pIntegrity));
  stun_dump(stderr, &msg);

#endif


  if((rc = turn_send_request(pTurn, pnetsock, rc, 1)) < 0) {
    return rc;
  }

  log_turn_msg(pTurn->bufLastReq, pnetsock, (const struct sockaddr *) &pTurn->saTurnSrv, pIntegrity, 
               pTurn->szLastReq, "Sent TURN Allocate Request", NULL);

  return rc;
}

static int turn_send_createpermission_request(TURN_CTXT_T *pTurn, 
                                              NETIO_SOCK_T *pnetsock, 
                                              const struct sockaddr *psapermission,
                                              const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity) {
  int rc;
  STUN_ATTRIB_T mappedAddress;
  char tmp[128];
  char logbuf[128];

  memcpy(&mappedAddress, &pTurn->mappedAddress, sizeof(mappedAddress));
  if(psapermission->sa_family == AF_INET6) {
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).family = STUN_MAPPED_ADDRESS_FAMILY_IPV6;
    memcpy(& TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).u_addr.ipv6[0], 
           &((const struct sockaddr_in6 *) psapermission)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
           //&((const struct sockaddr_in6 *) pTurn->saTurnSrv)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
  } else {
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).family = STUN_MAPPED_ADDRESS_FAMILY_IPV4;
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).u_addr.ipv4 = 
                                                   ((const struct sockaddr_in *) psapermission)->sin_addr;
  //TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).u_addr.ipv4 = pTurn->saTurnSrv.sin_addr;
  }

  //
  // Use port 0
  //
  //TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).port = htons(PINET_PORT(psapermission);
  TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).port = 0;

  if((rc = create_createpermission_request(pIntegrity, pTurn->bufLastReq, sizeof(pTurn->bufLastReq), 
                                           &pTurn->lastNonce, &mappedAddress)) < 0) {
    return rc;
  }

  if((rc = turn_send_request(pTurn, pnetsock, rc, 1)) < 0) {
    return rc;
  }

  snprintf(logbuf, sizeof(logbuf), "permission: %s:%d", 
           //inet_ntoa(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).u_addr.ipv4), 
           inet_ntop(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).family == 
                     STUN_MAPPED_ADDRESS_FAMILY_IPV6 ? AF_INET6 : AF_INET,
                     &TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr, tmp, sizeof(tmp)),
           TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).port);

  log_turn_msg(pTurn->bufLastReq, pnetsock, (const struct sockaddr *) &pTurn->saTurnSrv, pIntegrity, 
               pTurn->szLastReq, "Sent TURN CreatePermission Request", logbuf);

  return rc;
}

static int turn_send_refresh_request(TURN_CTXT_T *pTurn, 
                                     NETIO_SOCK_T *pnetsock, 
                                     const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity,
                                     int lifetime) {
  int rc;
  char logbuf[128];

  if((rc = create_refresh_request(pIntegrity, pTurn->bufLastReq, sizeof(pTurn->bufLastReq), 
                                  &pTurn->lastNonce, lifetime)) < 0) {
    return rc;
  }

  if((rc = turn_send_request(pTurn, pnetsock, rc, 1)) < 0) {
    return rc;
  }

  pTurn->refreshState = 1;

  snprintf(logbuf, sizeof(logbuf), "lifetime: %d", lifetime);

  log_turn_msg(pTurn->bufLastReq, pnetsock, (const struct sockaddr *) &pTurn->saTurnSrv, 
               pIntegrity, pTurn->szLastReq, "Sent TURN Refresh Request", logbuf);

  return rc;
}

static int turn_send_channel_request(TURN_CTXT_T *pTurn,
                                     NETIO_SOCK_T *pnetsock,
                                     const struct sockaddr *psapermission,
                                     uint16_t channelId,
                                     const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity) {
  int rc;
  STUN_ATTRIB_T mappedAddress;
  char tmp[128];
  char logbuf[128];

  memcpy(&mappedAddress, &pTurn->mappedAddress, sizeof(mappedAddress));
  TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).port = htons(PINET_PORT(psapermission));
  if(psapermission->sa_family == AF_INET6) {
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).family = STUN_MAPPED_ADDRESS_FAMILY_IPV6;
    memcpy(& TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).u_addr.ipv6[0], 
           &((const struct sockaddr_in6 *) psapermission)->sin6_addr.s6_addr[0], ADDR_LEN_IPV6);
  } else {
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).family = STUN_MAPPED_ADDRESS_FAMILY_IPV4;
    TURN_ATTRIB_VALUE_RELAYED_ADDRESS(mappedAddress).u_addr.ipv4 = 
                                            ((const struct sockaddr_in *) psapermission)->sin_addr;
  }

  if((rc = create_channel_request(pIntegrity, pTurn->bufLastReq, sizeof(pTurn->bufLastReq), 
                                  &pTurn->lastNonce, &mappedAddress, channelId)) < 0) {
    return rc;
  }

  if((rc = turn_send_request(pTurn, pnetsock, rc, 1)) < 0) {
    return rc;
  }

  pTurn->refreshState = 2;

  snprintf(logbuf, sizeof(logbuf), "channel: 0x%x, permission: %s:%d", 
           channelId, FORMAT_NETADDR(*psapermission, tmp, sizeof(tmp)), htons(PINET_PORT(psapermission)));

  log_turn_msg(pTurn->bufLastReq, pnetsock, (const struct sockaddr *) &pTurn->saTurnSrv, pIntegrity, 
               pTurn->szLastReq, "Sent TURN Channel Request", logbuf);

  return rc;
}


static int turn_onrcv_allocate_response(TURN_CTXT_T *pTurn, 
                                 const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, 
                                 const STUN_MSG_T *pMsg,
                                 const struct sockaddr *psaSrc, 
                                 const struct sockaddr *psaLocal) {
  int rc = 0;
  const STUN_ATTRIB_T *pAttr = NULL;

  if(pMsg->type == TURN_MSG_TYPE_ALLOCATE_RESPONSE_SUCCESS) {

    if((pAttr = stun_findattrib(pMsg, TURN_ATTRIB_LIFETIME))) {
      memcpy(&pTurn->lastLifetime, pAttr, sizeof(STUN_ATTRIB_T));
    }

    if((pAttr = stun_findattrib(pMsg, TURN_ATTRIB_XOR_RELAYED_ADDRESS))) {
      memcpy(&pTurn->relayedAddress, pAttr, sizeof(STUN_ATTRIB_T));
    }

    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_XOR_MAPPED_ADDRESS)) ||
       (pAttr = stun_findattrib(pMsg, STUN_ATTRIB_MAPPED_ADDRESS))) {
      memcpy(&pTurn->mappedAddress, pAttr, sizeof(STUN_ATTRIB_T));
    }

    pTurn->tmRefreshResp = timer_GetTime();

  } else if(pMsg->type == TURN_MSG_TYPE_ALLOCATE_RESPONSE_ERROR) {

    //
    // Store the last error state for possible re-try w/ different authentication credentials
    //
    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_ERROR_CODE))) {
      memcpy(&pTurn->lastError, pAttr, sizeof(STUN_ATTRIB_T));
    }  

    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_NONCE))) {
      memcpy(&pTurn->lastNonce, pAttr, sizeof(STUN_ATTRIB_T));
    }

    //
    // Store the server realm reply if we were not configured with a realm
    //
    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_REALM))) {
      if(!pTurn->integrity.realm || pTurn->integrity.realm[0] == '\0') {
        //strncpy(pTurn->store.realmbuf, pAttr->u.stringtype.value, sizeof(pTurn->store.realmbuf) - 1);
        //pTurn->integrity.realm = pTurn->store.realmbuf; 
        strncpy((char *)pTurn->integrity.realm, pAttr->u.stringtype.value, STUN_STRING_MAX - 1);
      }
    }

  }

  return rc;

}

static int turn_onrcv_createpermission_response(TURN_CTXT_T *pTurn, 
                                 const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                                 const struct sockaddr *psaSrc, const struct sockaddr *psaLocal) {
  int rc = 0;
  const STUN_ATTRIB_T *pAttr = NULL;

  if(pMsg->type == TURN_MSG_TYPE_CREATEPERM_RESPONSE_SUCCESS) {

    //LOG(X_DEBUG("CREATEPERM RESP OK RELAYED ADDR: %s:%d"), inet_ntoa(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr.ipv4), TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).port);

  } else if(pMsg->type == TURN_MSG_TYPE_CREATEPERM_RESPONSE_ERROR) {

    //
    // Store the last error state for possible re-try w/ different authentication credentials
    //
    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_ERROR_CODE))) {
      memcpy(&pTurn->lastError, pAttr, sizeof(STUN_ATTRIB_T));
    }  

    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_NONCE))) {
      memcpy(&pTurn->lastNonce, pAttr, sizeof(STUN_ATTRIB_T));
    }

  }

  return rc;

}

static int turn_onrcv_refresh_response(TURN_CTXT_T *pTurn, 
                                const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                                const struct sockaddr *psaSrc, const struct sockaddr *psaLocal) {
  int rc = 0;
  const STUN_ATTRIB_T *pAttr = NULL;

  if(pMsg->type == TURN_MSG_TYPE_REFRESH_RESPONSE_SUCCESS) {

    if((pAttr = stun_findattrib(pMsg, TURN_ATTRIB_LIFETIME))) {
      memcpy(&pTurn->lastLifetime, pAttr, sizeof(STUN_ATTRIB_T));
    }

    pTurn->tmRefreshResp = timer_GetTime();

  } else if(pMsg->type == TURN_MSG_TYPE_REFRESH_RESPONSE_ERROR) {

    //
    // Store the last error state for possible re-try w/ different authentication credentials
    //
    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_ERROR_CODE))) {
      memcpy(&pTurn->lastError, pAttr, sizeof(STUN_ATTRIB_T));
    }  

    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_NONCE))) {
      memcpy(&pTurn->lastNonce, pAttr, sizeof(STUN_ATTRIB_T));
    }

  }

  return rc;

}

static int turn_onrcv_channelbind_response(TURN_CTXT_T *pTurn,
                                    const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                                    const struct sockaddr *psaSrc, const struct sockaddr *psaLocal) {
  int rc = 0;
  const STUN_ATTRIB_T *pAttr = NULL;

  if(pMsg->type == TURN_MSG_TYPE_CHANNELBIND_RESPONSE_SUCCESS) {

    //if((pAttr = stun_findattrib(pMsg, TURN_ATTRIB_LIFETIME))) {
    //  memcpy(&pTurn->lastLifetime, pAttr, sizeof(STUN_ATTRIB_T));
    //}

    pTurn->tmBindChannelResp = timer_GetTime();

  } else if(pMsg->type == TURN_MSG_TYPE_CHANNELBIND_RESPONSE_ERROR) {

    //
    // Store the last error state for possible re-try w/ different authentication credentials
    //
    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_ERROR_CODE))) {
      memcpy(&pTurn->lastError, pAttr, sizeof(STUN_ATTRIB_T));
    }

    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_NONCE))) {
      memcpy(&pTurn->lastNonce, pAttr, sizeof(STUN_ATTRIB_T));
    }

  }

  return rc;

}

int turn_onrcv_response(TURN_CTXT_T *pTurn, 
                        const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                        const struct sockaddr *psaSrc, const struct sockaddr *psaLocal) {
  int rc = 0;
  int handled = 0;
  char logbuf[2][128];
  int pktClass;
  int pktMethod;

  if(!pTurn || !pMsg) {
    return -1;
  }

  pktClass = STUN_MSG_CLASS(pMsg->type);
  pktMethod = STUN_MSG_METHOD(pMsg->type);

  pthread_mutex_lock(&pTurn->mtx);

  if(memcmp(pMsg->id, pTurn->idLastReq, STUN_ID_LEN)) {
    LOG(X_DEBUG("TURN response id does not match request id %s %s %s length:%d, id:"STUN_ID_FMT_STR")"),
         stun_type2str(pMsg->type), stun_log_error(logbuf[0], sizeof(logbuf[0]), pMsg),
         capture_log_format_pkt(logbuf[1], sizeof(logbuf[1]), psaSrc, psaLocal),
         pMsg->length + STUN_HDR_LEN, STUN_ID_FMT_ARGS(pMsg->id));
    pthread_mutex_unlock(&pTurn->mtx);
    return 0;
  }

  if(pktClass == STUN_MSG_CLASS_RESPONSE_ERROR) {
    LOG(X_WARNING("Received TURN %s %s %s length:%d, id:"STUN_ID_FMT_STR")"), 
         stun_type2str(pMsg->type), stun_log_error(logbuf[0], sizeof(logbuf[0]), pMsg),
         capture_log_format_pkt(logbuf[1], sizeof(logbuf[1]), psaSrc, psaLocal),
         pMsg->length + STUN_HDR_LEN, STUN_ID_FMT_ARGS(pMsg->id));
  }

  switch(pktMethod) {
    case TURN_MSG_METHOD_ALLOCATE:
      rc = turn_onrcv_allocate_response(pTurn, pIntegrity, pMsg, psaSrc, psaLocal);
      handled = 1;
      break;
    case TURN_MSG_METHOD_CREATEPERM:
      rc = turn_onrcv_createpermission_response(pTurn, pIntegrity, pMsg, psaSrc, psaLocal);
      handled = 1;
      break;
    case TURN_MSG_METHOD_REFRESH:
      rc = turn_onrcv_refresh_response(pTurn, pIntegrity, pMsg, psaSrc, psaLocal);
      handled = 1;
      break;
    case TURN_MSG_METHOD_CHANNELBIND:
      rc = turn_onrcv_channelbind_response(pTurn, pIntegrity, pMsg, psaSrc, psaLocal);
      handled = 1;
      break;
    default:
      rc = 0;
  }

  if(handled) {
    pTurn->state &= ~TURN_OP_RESP_RECV_WAIT;

    if(pTurn->pThreadCtxt) {
      vsxlib_cond_broadcast(&pTurn->pThreadCtxt->notify.cond, &pTurn->pThreadCtxt->notify.mtx);
    }
  }

  pthread_mutex_unlock(&pTurn->mtx);

  return rc;
}

int turn_onrcv_indication_data(TURN_CTXT_T *pTurn,
                               const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                               const struct sockaddr *psaSrc, const struct sockaddr *psaLocal) {
  int rc = 0;
  char logbuf[2][128];
  const STUN_ATTRIB_T *pAttr = NULL;

  if(!pTurn || !pMsg) {
    return -1;
  }

  pthread_mutex_lock(&pTurn->mtx);

  //stun_dump(stderr, pMsg);

  if(pMsg->type == TURN_MSG_TYPE_DATA_REQUEST) {

    //if((pAttr = stun_findattrib(pMsg, TURN_ATTRIB_XOR_PEER_ADDRESS))) {
    //  memcpy(&pTurn->lastLifetime, pAttr, sizeof(STUN_ATTRIB_T));
    //}

    //
    // Store the application data in lastData
    //
    if((pAttr = stun_findattrib(pMsg, TURN_ATTRIB_DATA))) {
      //LOG(X_DEBUG("INDICATION DATA length: %d"), pAttr->length); LOGHEX_DEBUG(pAttr->u.data.pData, pAttr->length);
      memcpy(&pTurn->lastData, pAttr, sizeof(STUN_ATTRIB_T));
    }

  } else {

    //
    // Store the last error state for possible re-try w/ different authentication credentials
    //
    if((pAttr = stun_findattrib(pMsg, STUN_ATTRIB_ERROR_CODE))) {
      memcpy(&pTurn->lastError, pAttr, sizeof(STUN_ATTRIB_T));
    }

    LOG(X_WARNING("(Data Error) Received TURN %s %s %s length:%d, id:"STUN_ID_FMT_STR")"), 
         stun_type2str(pMsg->type), stun_log_error(logbuf[0], sizeof(logbuf[0]), pMsg),
         capture_log_format_pkt(logbuf[1], sizeof(logbuf[1]), psaSrc, psaLocal),
         pMsg->length + STUN_HDR_LEN, STUN_ID_FMT_ARGS(pMsg->id));

  }

  pthread_mutex_unlock(&pTurn->mtx);

  return rc;
}

static void turn_new_command(TURN_CTXT_T *pTurn, int operation) {
  pTurn->state = operation;
  pTurn->retryCnt = 0;
  pTurn->tmFirstReq = timer_GetTime();
}

static int turn_create_allocation_1(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock) { 
  int rc = 0;

  //
  // Do not yet send credentials if a nonce was not received from the server
  //
  const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity = pTurn->lastNonce.length > 0 ? &pTurn->integrity : NULL;

  memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));
  memset(&pTurn->lastLifetime, 0, sizeof(pTurn->lastLifetime));
  memset(&pTurn->relayedAddress, 0, sizeof(pTurn->relayedAddress));
  memset(&pTurn->mappedAddress, 0, sizeof(pTurn->mappedAddress));

  if((rc = turn_send_allocate_request(pTurn, pnetsock, pIntegrity)) < 0) {
    return rc;
  }

  pTurn->retryCnt = 1;
  pTurn->state |= TURN_OP_RESP_RECV_WAIT;

  return rc;
}

static void turn_on_relay_complete(TURN_CTXT_T *pTurn, int have_permission) {

  char tmp[128];
  char logbuf[2][128];

  if(pTurn->relay_active == 0) {
    pTurn->relay_active = 1;
  }

  if(INET_ADDR_VALID(pTurn->saPeerRelay)) {

    pTurn->relay_active = 2;
    pTurn->have_permission = 1;

    if(pTurn->pnetsock) {
      pTurn->pnetsock->turn.have_permission = 1;
      memcpy(&pTurn->pnetsock->turn.saPeerRelay, &pTurn->saPeerRelay, sizeof(pTurn->pnetsock->turn.saPeerRelay));
    }

    snprintf(logbuf[1], sizeof(logbuf[1]), "%s:%d", FORMAT_NETADDR(pTurn->saPeerRelay, tmp, sizeof(tmp)), 
             htons(INET_PORT(pTurn->saPeerRelay)));
    LOG(X_DEBUG("Setup TURN relay allocation for %s, allocated-relay-address: %s:%d, turn-remote-peer-address:%s"),
        stream_log_format_pkt_sock(logbuf[0], sizeof(logbuf[0]), pTurn->pnetsock, 
                                   (const struct sockaddr *) &pTurn->saTurnSrv),
             //inet_ntoa(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr.ipv4),
               inet_ntop(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).family == 
                         STUN_MAPPED_ADDRESS_FAMILY_IPV6 ?  AF_INET6 : AF_INET,
                         &TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr, tmp, sizeof(tmp)),
               TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).port, logbuf[1]);

  } else {

    LOG(X_DEBUG("Setup TURN relay allocation (without permission and without turn-remote-peer-address)"
                " for %s, allocated-relay-address: %s:%d"),
             stream_log_format_pkt_sock(logbuf[0], sizeof(logbuf[0]), pTurn->pnetsock, 
                                        (const struct sockaddr *) &pTurn->saTurnSrv),
               inet_ntop(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).family == 
                         STUN_MAPPED_ADDRESS_FAMILY_IPV6 ? AF_INET6 : AF_INET,
                         &TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr, tmp, sizeof(tmp)),
               TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).port);
  }

}

static int turn_create_allocation_2(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock) {
  int rc = 0;
  char logbuf[1][128];

  if(pTurn->lastError.type == STUN_ATTRIB_ERROR_CODE) {

    if(((pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == STUN_ERROR_CODE_UNAUTHORIZED ||
       (pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == TURN_ERROR_CODE_STALE_NONCE)) {

      if(pTurn->retryCnt++ > 1) {
        if(((pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == STUN_ERROR_CODE_UNAUTHORIZED)) {
          LOG(X_ERROR("Failed to Setup TURN relay allocation for %s, user: '%s', pass: '%s', realm: '%s'"),
             stream_log_format_pkt_sock(logbuf[0], sizeof(logbuf[0]), pTurn->pnetsock, 
                                        (const struct sockaddr *) &pTurn->saTurnSrv),
             (pTurn->integrity.user && pTurn->integrity.user[0]) ? pTurn->integrity.user : "<NONE>",
             (pTurn->integrity.pass && pTurn->integrity.pass[0]) ? pTurn->integrity.pass : "<NONE>",
             (pTurn->integrity.realm && pTurn->integrity.realm[0]) ? pTurn->integrity.realm: "<NONE>"); 
        }
        return -1;
      }

      memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

      //
      // The nonce should have been updated in turn_onrcv_allocate_response
      //
      if((rc = turn_send_allocate_request(pTurn, pnetsock, &pTurn->integrity)) < 0) {
        return rc;
      }

      pTurn->state |= TURN_OP_RESP_RECV_WAIT;
      return rc;

    } else if((pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == TURN_ERROR_CODE_ALLOCATION_MISMATCH) {

      //
      // The same allocation may still exist on the TURN server so issue a REFRESh with 0 lifetime to close
      // any stale allocation and retry.
      //
      if(pTurn->do_retry_alloc == 0) {

        LOG(X_DEBUG("Closing and retrying TURN relay allocations and retrying for %s"),
             stream_log_format_pkt_sock(logbuf[0], sizeof(logbuf[0]), pTurn->pnetsock, 
                                        (const struct sockaddr *) &pTurn->saTurnSrv));

        pTurn->do_retry_alloc++;
        turn_new_command(pTurn, TURN_OP_REQ_REFRESH);
        pTurn->reqArgLifetime = 0;
        turn_on_socket_closed(pTurn);
        return 0;

      } else if(pTurn->retryCnt++ > 1) {
        return -1;
      }

    } else {
      pTurn->retryCnt++;
      return -1;
    }
  }

  //
  // Invoke the callback for creating the allocation (receiving the server allocated port)
  //
  if(pTurn->result.active) {
    turn_on_created_allocation(pTurn);
  }

  if(INET_ADDR_VALID(pTurn->saPeerRelay)) {

    //
    // We want to issue a createpermission command after a succesful allocation
    //
    turn_new_command(pTurn, TURN_OP_REQ_CREATEPERM);
    pTurn->relay_active = 1;
    pTurn->do_retry_alloc = 0;

  } else {

    pTurn->state &= ~TURN_OP_REQ_ALLOCATION;
    pTurn->state |= TURN_OP_REQ_OK;

    turn_on_relay_complete(pTurn, 0);

  }

  return rc;
}

static int turn_create_createpermission_1(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock) {
  int rc = 0;

  memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

  if(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).family == STUN_MAPPED_ADDRESS_FAMILY_IPV4 &&
     !IS_ADDR4_VALID(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr.ipv4)) {
    LOG(X_ERROR("TURN allocation response did not contain a valid ipv4 relayed peer address"));
    return -1;
  } else if(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).family == STUN_MAPPED_ADDRESS_FAMILY_IPV6 &&
     IN6_IS_ADDR_UNSPECIFIED(((struct in6_addr *) &(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr.ipv6)))) {
    LOG(X_ERROR("TURN allocation response did not contain a valid ipv6 relayed peer address"));
    return -1;
  } else if(!INET_ADDR_VALID(pTurn->saPeerRelay)) {
    LOG(X_ERROR("TURN createpermission no valid ipv4 peer-relay address"));
    return -1;
  }

  if((rc = turn_send_createpermission_request(pTurn, pnetsock, (const struct sockaddr *) &pTurn->saPeerRelay, 
                                              &pTurn->integrity)) < 0) {
    return rc;
  }

  pTurn->retryCnt = 1;
  pTurn->state |= TURN_OP_RESP_RECV_WAIT;

  return rc;
}

static int turn_create_createpermission_2(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock) {

  int rc = 0;

  if(pTurn->lastError.type == STUN_ATTRIB_ERROR_CODE) {

    if(pTurn->retryCnt++ > 1) {
      return -1;
    }

    if(((pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == STUN_ERROR_CODE_UNAUTHORIZED ||
      (pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == TURN_ERROR_CODE_STALE_NONCE)) {

      memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

      //
      // The nonce should have been updated in turn_onrcv_createpermission_response
      //
      if((rc = turn_send_createpermission_request(pTurn, pnetsock, (const struct sockaddr *) &pTurn->saPeerRelay, 
                                                  &pTurn->integrity)) < 0) {
        return rc;
      }

      pTurn->state |= TURN_OP_RESP_RECV_WAIT;
      return rc;
    } else {
      return -1;
    }
  }

  //
  // The relay setup is considered complete
  //
  pTurn->state &= ~TURN_OP_REQ_CREATEPERM;
  pTurn->state |= TURN_OP_REQ_OK;
  
  turn_on_relay_complete(pTurn, 1);

  return rc;
}

static int turn_create_refresh_1(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, int lifetime) {
  int rc = 0;

  memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

  turn_new_command(pTurn, TURN_OP_REQ_REFRESH);

  if((rc = turn_send_refresh_request(pTurn, pnetsock, &pTurn->integrity, lifetime)) < 0) {
    return rc;
  }

  pTurn->retryCnt = 1;
  pTurn->state |= TURN_OP_RESP_RECV_WAIT;

  return rc;
}

static int turn_create_refresh_2(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, int lifetime) {
  int rc = 0;

  if(pTurn->refreshState) {
    pTurn->refreshState = 0;
  }

  if(pTurn->lastError.type == STUN_ATTRIB_ERROR_CODE) {

    if(pTurn->retryCnt++ > 1) {
      return -1;
    }

    if((pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == TURN_ERROR_CODE_STALE_NONCE) {

      memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

      //
      // The nonce should have been updated in turn_onrcv_refresh_response
      //
      if((rc = turn_send_refresh_request(pTurn, pnetsock, &pTurn->integrity, lifetime)) < 0) {
        return rc;
      }

      pTurn->state |= TURN_OP_RESP_RECV_WAIT;
      return rc;

    } else {
      return -1;
    }
  
  }

  pTurn->state &= ~TURN_OP_REQ_REFRESH;
  pTurn->state |= TURN_OP_REQ_OK;

  if(pTurn->do_retry_alloc == 1) {
    turn_new_command(pTurn, TURN_OP_REQ_ALLOCATION);
  }

  return rc;
}

static int turn_create_channel_1(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, uint16_t channelId) {
  int rc = 0;

  if(!INET_ADDR_VALID(pTurn->saPeerRelay)) {
    LOG(X_ERROR("TURN channel-binding no valid ipv4 peer-relay address set"));
    return -1;
  }

  memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

  turn_new_command(pTurn, TURN_OP_REQ_CHANNEL);

  if((rc = turn_send_channel_request(pTurn, pnetsock, (const struct sockaddr *) &pTurn->saPeerRelay, 
                                     channelId, &pTurn->integrity)) < 0) {
    return rc;
  }

  pTurn->retryCnt = 1;
  pTurn->state |= TURN_OP_RESP_RECV_WAIT;

  return rc;
}

static int turn_create_channel_2(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, uint16_t channelId) {
  int rc = 0;

  if(pTurn->refreshState) {
    pTurn->refreshState = 0;
  }

  if(pTurn->lastError.type == STUN_ATTRIB_ERROR_CODE) {

    if(pTurn->retryCnt++ > 1) {
      return -1;
    }

    if((pTurn->lastError.u.error.code & STUN_ERROR_CODE_MASK) == TURN_ERROR_CODE_STALE_NONCE) {

      memset(&pTurn->lastError, 0, sizeof(pTurn->lastError));

      //
      // The nonce should have been updated in turn_onrcv_refresh_response
      //
      if((rc = turn_send_channel_request(pTurn, pnetsock, (const struct sockaddr *) &pTurn->saPeerRelay, 
                                         channelId, &pTurn->integrity)) < 0) {
        return rc;
      }

      pTurn->state |= TURN_OP_RESP_RECV_WAIT;
      return rc;
    } else {
      return -1;
    }

  }

  if(pTurn->pnetsock) {
    pTurn->pnetsock->turn.turn_channel = channelId;
  }
  pTurn->have_channelbinding = 1;

  pTurn->state &= ~TURN_OP_REQ_CHANNEL;
  pTurn->state |= TURN_OP_REQ_OK;

  return rc;
}

static int turn_state_machine(TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, TIME_VAL tm, int *pdo_sleep) {

  char buf[128];
  int rc = 0;

  //LOG(X_DEBUG("turn_state_machine pTurn: 0x%x, state: 0x%x, retryCnt:%d, lastError.type:%d, tm:%lld, tmLastReq:%lld"), pTurn, pTurn->state, pTurn->retryCnt, pTurn->lastError.type, tm/TIME_VAL_MS, pTurn->tmLastReq/TIME_VAL_MS);

  *pdo_sleep = 0;

  if(pTurn->state & TURN_OP_REQ_TIMEOUT) {
    *pdo_sleep = 1;
    return 0;
  } else if(pTurn->state & TURN_OP_RESP_RECV_ERROR) {
    return -1;
  } else if(pTurn->state & TURN_OP_ERROR) {
    return -1;
  } else if(pTurn->state & TURN_OP_RESP_RECV_WAIT) {

    if((tm - pTurn->tmFirstReq) > TIME_VAL_MS * TURN_MSG_RECV_TIMEOUT_MS) {
    
      LOG(X_WARNING("TURN %s request timeout without receiving response after %lld ms"), 
          stun_type2str(ntohs(*((uint16_t *) &pTurn->bufLastReq[0]))), (tm - pTurn->tmFirstReq) / TIME_VAL_MS);

      pTurn->state &= ~TURN_OP_REQ_MASK;
      pTurn->state &= ~TURN_OP_RESP_RECV_WAIT;
      pTurn->state |= TURN_OP_REQ_TIMEOUT;
      rc = -1;

    } else if(pTurn->szLastReq > 0 && (tm - pTurn->tmLastReq) > TIME_VAL_MS * TURN_MSG_RESEND_TIMEOUT_MS) {

      //
      // Retry mechanism which will re-send the previous TURN message
      //
      rc = turn_send_request(pTurn, pnetsock, pTurn->szLastReq, 0);

      snprintf(buf, sizeof(buf), "Resent TURN %s", stun_type2str(ntohs(*((uint16_t *) &pTurn->bufLastReq[0]))));
      log_turn_msg(pTurn->bufLastReq, pTurn->pnetsock, (const struct sockaddr *) &pTurn->saTurnSrv, 
                   &pTurn->integrity, pTurn->szLastReq, buf, NULL);
    } else {
      *pdo_sleep = 1;
    }

    return rc; 

  } else if(pTurn->state & TURN_OP_REQ_ALLOCATION) {
    if(pTurn->retryCnt == 0) {
      rc = turn_create_allocation_1(pTurn, pnetsock);
    } else {
      rc = turn_create_allocation_2(pTurn, pnetsock);
    }
  } else if(pTurn->state & TURN_OP_REQ_CREATEPERM) {
    if(pTurn->retryCnt == 0) {
      rc = turn_create_createpermission_1(pTurn, pnetsock);
    } else {
      rc = turn_create_createpermission_2(pTurn, pnetsock);
    }
  } else if(pTurn->state & TURN_OP_REQ_REFRESH) {
    if(pTurn->retryCnt == 0) {
      rc = turn_create_refresh_1(pTurn, pnetsock, pTurn->reqArgLifetime);
    } else {
      rc = turn_create_refresh_2(pTurn, pnetsock, pTurn->reqArgLifetime);
    }
  } else if(pTurn->state & TURN_OP_REQ_CHANNEL) {
    if(pTurn->retryCnt == 0) {
      rc = turn_create_channel_1(pTurn, pnetsock, pTurn->reqArgChannelId);
    } else {
      rc = turn_create_channel_2(pTurn, pnetsock, pTurn->reqArgChannelId);
    }
  } else {
    *pdo_sleep = 1;
  }

  if(rc < 0) {
    pTurn->state &= ~TURN_OP_REQ_MASK;
    pTurn->state |= TURN_OP_ERROR;
  } else if(*pdo_sleep == 0 && pTurn->state & TURN_OP_RESP_RECV_WAIT) {
    *pdo_sleep = 1;
  }

  return rc;
}

static int turn_relay_bindchannel(TURN_CTXT_T *pTurn, int channelId) {
  int rc = 0;

  if(!pTurn || ((const struct sockaddr_in *) &pTurn->saTurnSrv)->sin_port == 0 || !pTurn->pThreadCtxt) {
    return -1;
  } else if(channelId > 0 && (channelId < TURN_CHANNEL_ID_MIN || channelId > TURN_CHANNEL_ID_MAX)) {
    return -1;
  }

  if(channelId > 0) {
    pTurn->reqArgChannelId = channelId;
  }
  //pTurn->tmBindChannelResp = 0;
  pTurn->do_retry_alloc = 0;

  //TODO: warn if currently running a command

  turn_new_command(pTurn, TURN_OP_REQ_CHANNEL);

  return rc;
}

static int turn_refresh_relay(TURN_CTXT_T *pTurn) {
  int rc = 0;
  char logbuf[128];

  if(!pTurn || ((const struct sockaddr_in *) &pTurn->saTurnSrv)->sin_port == 0 || !pTurn->pThreadCtxt) {
    return -1;
  }

  LOG(X_DEBUG("Refreshing TURN relay permission allocation %s"),
            stream_log_format_pkt_sock(logbuf, sizeof(logbuf), pTurn->pnetsock, 
                                       (const struct sockaddr *) &pTurn->saTurnSrv));

  //TODO: warn if currently running a command

  //
  // Refresh the permission
  //
  turn_new_command(pTurn, TURN_OP_REQ_REFRESH);
  pTurn->reqArgLifetime = -1;

  return rc;
}

int turn_relay_setup(TURN_THREAD_CTXT_T *pThreadCtxt, TURN_CTXT_T *pTurn,
                     NETIO_SOCK_T *pnetsock, TURN_RELAY_SETUP_RESULT_T *pOnResult) {
  int rc = 0;
  TURN_CTXT_T *pTurnHeadTmp = NULL;
  char tmp[128];
  char logbuf[1][128];

  if(!pThreadCtxt || !pTurn || !pnetsock) {
    return -1;
  } else if(pThreadCtxt->flag <= 0) {
    LOG(X_ERROR("TURN thread is not active"));
    return -1;
  } else if(!INET_ADDR_VALID(pTurn->saTurnSrv) || ((const struct sockaddr_in *) &pTurn->saTurnSrv)->sin_port == 0) {
    LOG(X_ERROR("TURN server address not set"));
    return -1;
  } else if(pTurn->relay_active > 0) {
    LOG(X_ERROR("TURN client instance is already active"));
    return -1;
  } 

  pTurnHeadTmp = pThreadCtxt->pTurnHead;

  if((rc = turn_ctxt_attach(pThreadCtxt, pTurn, pOnResult)) < 0) {
    LOG(X_ERROR("Failed to create new TURN client instance"));
    return rc;
  }

  if(!pTurnHeadTmp) {
    LOG(X_DEBUG("TURN server: %s:%d, username: '%s', password: '%s', policy: %s"),
        FORMAT_NETADDR(pTurn->saTurnSrv, tmp, sizeof(tmp)), htons(INET_PORT(pTurn->saTurnSrv)),
        pTurn->integrity.user, pTurn->integrity.pass, turn_policy2str(pTurn->turnPolicy));
  }

  memset(&pTurn->lastNonce, 0, sizeof(pTurn->lastNonce));

  //TODO: warn if currently running a command

  LOG(X_DEBUG("Setting up TURN relay allocation %s, turn-remote-peer-address:%s:%d"), 
             stream_log_format_pkt_sock(logbuf[0], sizeof(logbuf[0]), pnetsock, 
                                        (const struct sockaddr *) &pTurn->saTurnSrv), 
             FORMAT_NETADDR(pTurn->saPeerRelay, tmp, sizeof(tmp)), htons(INET_PORT(pTurn->saPeerRelay)));

  //
  // Set the argument socket to utilize TURN for data relay
  //
  pnetsock->turn.use_turn_indication_out = 1;
  pnetsock->turn.use_turn_indication_in = 1;

  pTurn->pnetsock = pnetsock;
  memcpy(&pnetsock->turn.saTurnSrv, &pTurn->saTurnSrv, sizeof(pnetsock->turn.saTurnSrv));
  memcpy(&pnetsock->turn.saPeerRelay, &pTurn->saPeerRelay, sizeof(pnetsock->turn.saPeerRelay));
  pTurn->pnetsock->turn.turnPolicy = pTurn->turnPolicy;

  turn_new_command(pTurn, TURN_OP_REQ_ALLOCATION);

  return rc;
}


int turn_close_relay(TURN_CTXT_T *pTurn, int do_wait) {
  int rc = 0;

  if(!pTurn || ((const struct sockaddr_in *) &pTurn->saTurnSrv)->sin_port == 0 || !pTurn->pThreadCtxt) {
    return -1;
  }

  //LOG(X_DEBUG("TURN_CLOSE_RELAY called pTurn: 0x%x, do_wait:%d, relay_active:%d"), pTurn, do_wait, pTurn->relay_active);

  pthread_mutex_lock(&pTurn->mtx);
  //TODO: should mtx protect this relay_active flag.. race condition on close
  if(!pTurn->relay_active) {
    pthread_mutex_unlock(&pTurn->mtx);
    return -1;
  }

  //TODO: warn if currently running a command

  turn_new_command(pTurn, TURN_OP_REQ_REFRESH);
  pTurn->reqArgLifetime = 0;
  turn_on_socket_closed(pTurn);
  vsxlib_cond_broadcast(&pTurn->pThreadCtxt->notify.cond, &pTurn->pThreadCtxt->notify.mtx);

  if(do_wait) {
    while(pTurn->state == TURN_OP_REQ_REFRESH) {
      usleep(10000);
      //LOG(X_DEBUG("CLOSE_RELAY WAITING FOR OUTPUT SENT..."));
    }
  }

  if(do_wait) {
    turn_on_connection_closed(pTurn, 0);
  }

  pthread_mutex_unlock(&pTurn->mtx);

  //TODO: un-attach from pThreadCtxt...

  //LOG(X_DEBUG("CLOSE_RELAY DONE SENT OUTPUT..."));
/*
  if(wait_resp) {
    while((pTurn->state & (TURN_OP_ERROR | TURN_OP_REQ_OK)) == 0) {
      LOG(X_DEBUG("SLEEP IN CLOSE_RELAY state: 0x%x..."), pTurn->state);
      usleep(20000);
    }
    LOG(X_DEBUG("CLOSE_RELAY DONE..."));
  }
*/
  return rc;
}

static int turn_ctxt_attach(TURN_THREAD_CTXT_T *pThreadCtxt, TURN_CTXT_T *pTurn, 
                            TURN_RELAY_SETUP_RESULT_T *pOnResult) {
  TURN_CTXT_T *pTurnTmp;
  int rc = 0;

  if(!pThreadCtxt || !pTurn) {
    return -1;
  }

  pthread_mutex_lock(&pThreadCtxt->mtx);

  if(!(pTurnTmp = pThreadCtxt->pTurnHead)) {
    pThreadCtxt->pTurnHead = pTurn;
  } else {
    while(pTurnTmp) {
      if(pTurnTmp == pTurn) {
        pthread_mutex_unlock(&pThreadCtxt->mtx);
        return -1;
      } else if(!pTurnTmp->pnext) {
        break;
      }
      pTurnTmp = pTurnTmp->pnext;
    } 
    pTurnTmp->pnext = pTurn;
  }

  if(pOnResult && pOnResult->active) {
    memcpy(&pTurn->result, pOnResult, sizeof(pTurn->result));
    pThreadCtxt->cntOnResult++;
  } else {
    memset(&pTurn->result, 0, sizeof(pTurn->result));
  }

  pthread_mutex_unlock(&pThreadCtxt->mtx);

  return rc;
}

typedef struct TURN_WRAP {
  TURN_THREAD_CTXT_T         *pThreadCtxt; 
  char                        tid_tag[LOGUTIL_TAG_LENGTH];
} TURN_WRAP_T;

static void turn_on_socket_closed(TURN_CTXT_T *pTurn) {

  //
  // Invalidate the TURN socket permission 
  //
  if(pTurn->pnetsock) {
    pTurn->pnetsock->turn.turn_channel = 0;
    pTurn->pnetsock->turn.have_permission = 0;
    //memset(&pTurn->pnetsock->turn.saPeerRelay, 0, sizeof(pTurn->pnetsock->turn.saPeerRelay));
  }

}

static void turn_on_connection_closed(TURN_CTXT_T *pTurn, int lock) {

  if(lock) {
    pthread_mutex_lock(&pTurn->mtx);
  }

  turn_on_socket_closed(pTurn);

  pTurn->relay_active = 0;
  pTurn->have_permission = 0;
  pTurn->have_channelbinding = 0;
  pTurn->tmRefreshResp = 0;
  pTurn->tmBindChannelResp = 0;

  if(lock) {
    pthread_mutex_unlock(&pTurn->mtx);
  }
  pTurn->state = 0;

}

static int turn_relay_run(TURN_CTXT_T *pTurn, const TIME_VAL tm, int *pdo_sleep) {

  int rc;
  unsigned int refreshMs;
  int channelId;
  char logbuf[128];

  //LOG(X_DEBUG("turn_relay_run at start: 0x%x, state: 0x%x, active:%d, retry_cnt:%d"), pTurn, pTurn->state, pTurn->relay_active, pTurn->retryCnt);

  *pdo_sleep = 1;

  if((rc = turn_state_machine(pTurn, pTurn->pnetsock, tm, pdo_sleep)) < 0) {
    LOG(X_ERROR("TURN connection %s %s."), stream_log_format_pkt_sock(logbuf, sizeof(logbuf), pTurn->pnetsock, 
              (const struct sockaddr *) &pTurn->saTurnSrv), pTurn->relay_active ? "exiting" : "aborting");

    turn_on_connection_closed(pTurn, 0);
    pTurn->state |= TURN_OP_ERROR;
    return rc;
  }

  //if((pTurn->state & TURN_OP_REQ_TIMEOUT) || (pTurn->state & TURN_OP_REQ_TIMEOUT)

  if(pTurn->relay_active == 2 && (pTurn->state & TURN_OP_REQ_MASK) == 0) { 
//TODO: make this conditional
    //
    // Issue a channel binding request after we have received a succesful createpermission response
    //
    if(pTurn->have_permission && pTurn->have_channelbinding == 0) {

      //channelId = (random() % (TURN_CHANNEL_ID_MAX - TURN_CHANNEL_ID_MIN + 1)) + TURN_CHANNEL_ID_MIN;
      channelId = TURN_CHANNEL_ID_MIN;

      if((rc = turn_relay_bindchannel(pTurn, channelId)) < 0) {
        return rc;
      }
    }

    if(pTurn->lastLifetime.u.lifetime.value >= 50) {
      refreshMs = (pTurn->lastLifetime.u.lifetime.value - 20) * TIME_VAL_MS;
    } else {
      refreshMs = (.80f * (float) pTurn->lastLifetime.u.lifetime.value * TIME_VAL_MS);
    }

    //TODO: refresh channel binding (10min)

    //refreshMs=5000;LOG(X_DEBUG("REFRESH MS:%d"), refreshMs);

    if(pTurn->refreshState == 0) {

      if(pTurn->have_permission && (tm - pTurn->tmRefreshResp) > TIME_VAL_MS * refreshMs) {

        //LOG(X_DEBUG("REFRESHING RELAY %lld > %d"), (tm - pTurn->tmRefreshResp) / TIME_VAL_MS, refreshMs);
        if((rc = turn_refresh_relay(pTurn)) < 0) {
          return rc;
        }

      } else if(pTurn->have_channelbinding && (tm - pTurn->tmBindChannelResp) > TIME_VAL_MS * refreshMs) {
        //LOG(X_DEBUG("REFRESHING BINDCHANNEL %lld > %d"), (tm - pTurn->tmBindChannelResp) / TIME_VAL_MS, refreshMs);
        if((rc = turn_relay_bindchannel(pTurn, 0)) < 0) {
          return rc;
        }
      }

    }

  }

  //LOG(X_DEBUG("turn_relay_run...%lld pTurn: 0x%x, pnetsock: 0x%x, relay:%s:%d,%s, last-lifetime:%d, %u ms ago, state: 0x%x, rc:%d, retryCnt:%d, relay_active:%d, have_permission:%d, have_channel:%d, do_sleep:%d, do_retry_alloc:%d, refreshState:%d"), tm/TIME_VAL_MS, pTurn, pTurn->pnetsock, inet_ntoa(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr.ipv4), TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).port, stream_log_format_pkt_sock(logbuf, sizeof(logbuf), pTurn->pnetsock, &pTurn->saPeerRelay), pTurn->lastLifetime.u.lifetime.value, (tm - pTurn->tmRefreshResp)/TIME_VAL_MS, pTurn->state, rc, pTurn->retryCnt, pTurn->relay_active, pTurn->have_permission, pTurn->have_channelbinding, *pdo_sleep, pTurn->do_retry_alloc, pTurn->refreshState);

  return rc;
}

void turn_relay_ctrl_proc(void *pArg) {
  TURN_WRAP_T wrap;
  TURN_CTXT_T *pTurn;
  int rc = 0;
  int do_sleep, do_sleepTmp;
  int have_active;
  int have_error;
  struct timeval tv;
  struct timespec ts;
  TIME_VAL tm;
  char tmp[128];
  int do_close = 0;

  memcpy(&wrap, pArg, sizeof(wrap));

  logutil_tid_add(pthread_self(), wrap.tid_tag);

  wrap.pThreadCtxt->flag = 1;

  LOG(X_DEBUG("TURN thread started"));

  while(wrap.pThreadCtxt->flag > 0) {

    have_error = 0;
    have_active = 0;
    do_sleep = -1;
    pthread_mutex_lock(&wrap.pThreadCtxt->mtx);
    pTurn = wrap.pThreadCtxt->pTurnHead;
    
    if(g_proc_exit != 0) {
      do_close = 2;
    } else if(wrap.pThreadCtxt->flag == 3) {
      do_close = 1;
    }
  
    while(pTurn) {

      tm = timer_GetTime();
      do_sleepTmp = 1;
      turn_init(pTurn, wrap.pThreadCtxt);

      //
      // Close the selected relay
      //
      if(do_close && pTurn->relay_active) {
        LOG(X_DEBUG("TURN thread closing relay %s:%d"), 
           //inet_ntoa(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr.ipv4), 
           inet_ntop(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).family == 
                     STUN_MAPPED_ADDRESS_FAMILY_IPV6 ? AF_INET6 : AF_INET,
                     &TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).u_addr, tmp, sizeof(tmp)),
                     TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).port);

        turn_close_relay(pTurn, 0);
      }

      pthread_mutex_lock(&pTurn->mtx);

      rc = turn_relay_run(pTurn, tm, &do_sleepTmp);

      if(pTurn->state & TURN_OP_ERROR) {
        have_error = 1;
        if(pTurn->pnetsock) {
          pTurn->pnetsock->turn.have_error = 1;
        }
      } else if(!have_active && ((pTurn->relay_active || (pTurn->state & TURN_OP_RESP_RECV_WAIT)))) {
        have_active = 1;
      }

      pthread_mutex_unlock(&pTurn->mtx);

      if(!do_sleepTmp) {
        do_sleep = 0;
      }

      pTurn = pTurn->pnext;
    }

    if(!have_active && have_error && 
       (wrap.pThreadCtxt->totOnResult > 0 || wrap.pThreadCtxt->cntOnResult == 0)) {
      do_close = 1;
    }

    pthread_mutex_unlock(&wrap.pThreadCtxt->mtx);

    if(do_close == 2 || (do_close && !have_active)) {
      LOG(X_DEBUG("TURN control thread closed all relays"));
      break;
    }

    if(do_sleep != 0) {
      gettimeofday(&tv, NULL);
      TV_INCREMENT_MS(tv, 1000);
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = tv.tv_usec * 1000;
      //LOG(X_DEBUG("TURN sleeping...."));
      if(vsxlib_cond_timedwait(&wrap.pThreadCtxt->notify.cond, &wrap.pThreadCtxt->notify.mtx, &ts) < 0) {
        LOG(X_WARNING("TURN conditional timed wait failed"));
        usleep(500000);
      }
    }

  }

  pthread_mutex_lock(&wrap.pThreadCtxt->mtx);
  pTurn = wrap.pThreadCtxt->pTurnHead;
  while(pTurn) {
    turn_close(pTurn);
    pTurn = pTurn->pnext;
  }
  pthread_mutex_unlock(&wrap.pThreadCtxt->mtx);

  LOG(X_DEBUG("TURN control thread ending with code %d"), rc);

  logutil_tid_remove(pthread_self());

  wrap.pThreadCtxt->flag = 0;
  
}

int turn_thread_start(TURN_THREAD_CTXT_T *pThreadCtxt) {

  int rc = 0;
  const char *s;
  pthread_t ptdCap;
  pthread_attr_t attrCap;
  TURN_WRAP_T wrap;

  if(!pThreadCtxt) {
    return -1;
  } else if(pThreadCtxt->flag > 0) {
    return 0;
  }

  pThreadCtxt->cntOnResult = 0;
  pThreadCtxt->doneOnResult = 0;
  pThreadCtxt->totOnResult = 0;
  pThreadCtxt->pOnResultHead = NULL;

  wrap.pThreadCtxt = pThreadCtxt; 
  wrap.pThreadCtxt->flag = 2;
  wrap.tid_tag[0] = '\0';
  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(wrap.tid_tag, sizeof(wrap.tid_tag), "%s-turn", s);
  }

  pthread_attr_init(&attrCap);
  pthread_attr_setdetachstate(&attrCap, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&ptdCap,
                    &attrCap,
                    (void *) turn_relay_ctrl_proc,
                    (void *) &wrap) != 0) {

    LOG(X_ERROR("Unable to create TURN message sender thread"));
    wrap.pThreadCtxt->flag = -1;
    return -1;
  }

  while(wrap.pThreadCtxt->flag == 2) {
    usleep(5000);
  }

  return rc;
}

int turn_thread_stop(TURN_THREAD_CTXT_T *pThreadCtxt) {
  int rc = 0;

  if(!pThreadCtxt) {
    return -1;
  }

  if(pThreadCtxt->flag == 0) {
    return 0;
  } if(pThreadCtxt->flag == 1) {
    pThreadCtxt->flag = 3;
  }

  //
  // Wait until TURN thread exits because it references external TURN context who's lifespan may go away
  //
  while(pThreadCtxt->flag > 0) {
    usleep(10000);
  }
  
  return rc;
}

static int turn_init(TURN_CTXT_T *pTurn, TURN_THREAD_CTXT_T *pThreadCtxt) {
  int rc = 0;

  if(!pTurn) {
    return -1;
  }
  
  pTurn->pThreadCtxt = pThreadCtxt;

  if(!pTurn->is_init) {
    pthread_mutex_init(&pTurn->mtx, NULL);
    pTurn->is_init = 1;
  }

  return rc;
}

static void turn_close(TURN_CTXT_T *pTurn) {

  if(!pTurn) {
    return;
  }

  turn_on_connection_closed(pTurn, 0);

  if(pTurn->is_init) {
    pthread_mutex_destroy(&pTurn->mtx);
    pTurn->is_init = 0;
    //pTurn->pnetsock = NULL;
  }

}

int turn_onrcv_pkt(TURN_CTXT_T *pTurn, STUN_CTXT_T *pStun, 
                   int is_turn_channeldata, int *pis_turn, int *pis_turn_indication,
                   unsigned char **ppData, int *plen, 
                   const struct sockaddr *psaSrc, const struct sockaddr *psaDst, NETIO_SOCK_T *pnetsock) {
  int rc = 0;

  if(!pTurn || !pStun) {
    return -1;
  }

  if(pis_turn) {
    *pis_turn = 0;
  }
  if(pis_turn_indication) {
    *pis_turn_indication = 0;
  }


  if(!is_turn_channeldata) {

    pTurn->lastData.length = 0;
    pTurn->lastData.u.data.pData = NULL;

    rc = stun_onrcv(pStun, pTurn, *ppData, *plen, psaSrc, psaDst, pnetsock, 1);

  }

  if(rc >= 0) {

    if(pis_turn) {
      *pis_turn = 1;
    }

    if(is_turn_channeldata) {
      // TODO: verify channel number
      (*ppData) = &(*ppData)[TURN_CHANNEL_HDR_LEN];
      *plen = *plen - TURN_CHANNEL_HDR_LEN;
      //LOG(X_DEBUG("RCV_HANDLE_TURN data channel data len to data.length:%d"), *plen); //LOGHEX_DEBUG(*ppData, *plen);
    } else if(pTurn->lastData.length > 0 && pTurn->lastData.u.data.pData) {
      (*ppData) = (unsigned char *) pTurn->lastData.u.data.pData;
      *plen = pTurn->lastData.length;
      //LOG(X_DEBUG("RCV_HANDLE_TURN data indication pkt len to data.length:%d"), *plen); //LOGHEX_DEBUG(*ppData, *plen);
      if(pis_turn_indication) {
        *pis_turn_indication = 1;
      }
    } else {
      //LOG(X_DEBUG("RCV_HANDLE_TURN set is_turn=1 TURN message (not indication)"));
    }
  }

  return rc;
}

static int turn_on_created_allocation(TURN_CTXT_T *pTurn) {
  int rc = 0;
  TURN_RELAY_SETUP_RESULT_T *pResult = NULL;
  unsigned int candidateIdx;
  SDP_CANDIDATE_T *pCandidate = NULL; 
  SDP_DESCR_T *pSdp = NULL;

  if(!pTurn) {
    return -1;
  } 

  pResult = &pTurn->result;
  pSdp = pResult->pSdp;

  //
  // Set the TURN relay port in the SDP ice candidate description line
  //
  if(pSdp) {

    candidateIdx = pResult->is_rtcp ? 1 : 0;
  
    pCandidate = pResult->isaud ? &pSdp->aud.common.iceCandidates[candidateIdx] : 
                                  &pSdp->vid.common.iceCandidates[candidateIdx];

    pCandidate->foundation = random() % RAND_MAX;
    pCandidate->component = pResult->is_rtcp ? 2 : 1;
    pCandidate->transport = SDP_ICE_TRANS_TYPE_UDP ;
    //
    // RFC 5245 4.1.2.2.  Guidelines for Choosing Type and Local Preferences
    // priority = (type preference << 24) | (ip preference << 8) | (256 - component id)
    // Suggested priorities: Host type 126, peer reflexive 110, server reflexive 100, relay 0
    //
    pCandidate->priority = ((0x10 << 24) | (0x1e << 8) | (0xff));
    memcpy(&pCandidate->address, &pTurn->saTurnSrv, sizeof(pCandidate->address));
    INET_PORT(pCandidate->address) = htons(TURN_ATTRIB_VALUE_RELAYED_ADDRESS(pTurn->relayedAddress).port);
    pCandidate->type = SDP_ICE_TYPE_RELAY;
    memcpy(&pCandidate->raddress, &pResult->saListener, sizeof(pCandidate->raddress));
    if(pCandidate->raddress.ss_family == AF_INET) {
      ((struct sockaddr_in *) &pCandidate->raddress)->sin_addr.s_addr = net_getlocalip4();
    } else {
      LOG(X_ERROR("TURN IPv6 raddress not supported"));
      return -1;
    }
  }

  //
  // Mark the state complete
  //
  if(pResult->state == 0) {

    pResult->state = 1;

    if(pTurn->pThreadCtxt && ++pTurn->pThreadCtxt->doneOnResult >= pTurn->pThreadCtxt->totOnResult) {

      //
      // If we are using audio/video muxing over one RTP channel then copy set the TURN properties
      // for both audio and video media
      //
      if(pSdp->vid.common.available && pSdp->vid.common.iceCandidates[0].foundation == 0 &&
         pSdp->aud.common.available && pSdp->aud.common.iceCandidates[0].foundation != 0) {
        memcpy(&pSdp->vid.common.iceCandidates[0], &pSdp->aud.common.iceCandidates[0], 
               sizeof(pSdp->vid.common.iceCandidates[0]));
      } else if(pSdp->aud.common.available && pSdp->aud.common.iceCandidates[0].foundation == 0 &&
                pSdp->vid.common.available && pSdp->vid.common.iceCandidates[0].foundation != 0) {
        memcpy(&pSdp->aud.common.iceCandidates[0], &pSdp->vid.common.iceCandidates[0], 
               sizeof(pSdp->aud.common.iceCandidates[0]));
      }

      //
      // Make sure both audio and video use the same ICE candidate foundation
      //
      if(pSdp->vid.common.iceCandidates[0].foundation != 0 && pSdp->aud.common.iceCandidates[0].foundation != 0) {
        pSdp->vid.common.iceCandidates[0].foundation = pSdp->aud.common.iceCandidates[0].foundation;
      }

      if(pSdp->vid.common.iceCandidates[1].foundation != 0) {
        pSdp->vid.common.iceCandidates[1].foundation = pSdp->vid.common.iceCandidates[0].foundation;
      } else {
        //
        // If we are using rtcp-mux then use the same TURN properties for RTP and RTCP, except the unique
        // component-id 
        //
        memcpy(&pSdp->vid.common.iceCandidates[1], (pSdp->vid.common.port == pSdp->vid.common.portRtcp ? 
               &pSdp->vid.common.iceCandidates[0] : &pSdp->aud.common.iceCandidates[1]), 
                 sizeof(pSdp->vid.common.iceCandidates[1]));
        pSdp->vid.common.iceCandidates[1].component = 2;
      }

      if(pSdp->aud.common.iceCandidates[1].foundation != 0) {
        pSdp->aud.common.iceCandidates[1].foundation = pSdp->aud.common.iceCandidates[0].foundation;
      } else {
        //
        // If we are using rtcp-mux then use the same TURN properties for RTP and RTCP, except the unique
        // component-id 
        //
        memcpy(&pSdp->aud.common.iceCandidates[1], (pSdp->aud.common.port == pSdp->aud.common.portRtcp ? 
               &pSdp->aud.common.iceCandidates[0] : &pSdp->vid.common.iceCandidates[1]), 
                 sizeof(pSdp->aud.common.iceCandidates[1]));
        pSdp->aud.common.iceCandidates[1].component = 2;
      }


      if(pTurn->sdpWritePath[0] != '\0') {
        LOG(X_INFO("All TURN allocations completed.  Creating %s with TURN relay candidates."), 
                   pTurn->sdpWritePath);
        sdp_write_path(pSdp, pTurn->sdpWritePath);
      }
    }
  }

  return rc;
}

#else // (VSX_HAVE_TURN)

int turn_create_datamsg(const unsigned char *pData, unsigned int szdata,
                        const TURN_SOCK_T *pTurnSock,
                        unsigned char *pout, unsigned int lenout) {
  return -1;
}

int turn_onrcv_pkt(TURN_CTXT_T *pTurn, STUN_CTXT_T *pStun,
                   int is_turn_channeldata, int *pis_turn, int *pis_turn_indication,
                   unsigned char **ppData, int *plen,
                   const struct sockaddr *psaSrc, const struct sockaddr *psaDst, NETIO_SOCK_T *pnetsock) {
  return -1;
}

#endif // (VSX_HAVE_TURN)

int turn_ischanneldata(const unsigned char *data, unsigned int len) {
  uint16_t channelId = 0;

  if(data && len >= TURN_CHANNEL_HDR_LEN && 
     (channelId = htons(*((uint16_t *) &data[0]))) >= TURN_CHANNEL_ID_MIN && channelId <= TURN_CHANNEL_ID_MAX &&
     htons(*((uint16_t *) &data[2])) + TURN_CHANNEL_HDR_LEN == len) {
    return 1;
  }

  return 0;
} 

int turn_can_send_data(TURN_SOCK_T *pTurnSock) {

  if(pTurnSock && pTurnSock->use_turn_indication_out &&
      (pTurnSock->turn_channel == 0 && (!pTurnSock->have_permission || 
                                        !INET_ADDR_VALID(pTurnSock->saPeerRelay)))) {
    return 0;
  }
  return 1;
}

int turn_init_from_cfg(TURN_CTXT_T *pTurn, const TURN_CFG_T *pTurnCfg) {
  int rc = 0;

  if(pTurnCfg && pTurnCfg->turnServer && pTurnCfg->turnServer[0] != '\0') {

    if((rc = capture_getdestFromStr(pTurnCfg->turnServer, &pTurn->saTurnSrv, NULL, NULL, DEFAULT_PORT_TURN)) < 0) {
      LOG(X_ERROR("Invalid TURN server address: %s"), pTurnCfg->turnServer);
      return -1;
    }

    if(pTurnCfg->turnPeerRelay &&
      (rc = capture_getdestFromStr(pTurnCfg->turnPeerRelay, &pTurn->saPeerRelay, NULL, NULL, 0)) < 0) {
      LOG(X_ERROR("Invalid TURN peer relay address: %s"), pTurnCfg->turnPeerRelay);
      return -1;
    }

    pTurn->turnPolicy = pTurnCfg->turnPolicy;

    safe_strncpy(pTurn->store.pwdbuf, pTurnCfg->turnPass, STUN_STRING_MAX - 1);
    safe_strncpy(pTurn->store.ufragbuf, pTurnCfg->turnUsername, STUN_STRING_MAX - 1);
    safe_strncpy(pTurn->store.realmbuf, pTurnCfg->turnRealm, STUN_STRING_MAX - 1);

    pTurn->integrity.pass = pTurn->store.pwdbuf;
    pTurn->integrity.user = pTurn->store.ufragbuf;
    pTurn->integrity.realm = pTurn->store.realmbuf;

    if(pTurnCfg->turnSdpOutPath) {
      safe_strncpy(pTurn->sdpWritePath, pTurnCfg->turnSdpOutPath, sizeof(pTurn->sdpWritePath) -1);
    }

    rc = 1;
  }

  return rc;
}

const char *turn_policy2str(TURN_POLICY_T policy) {
  switch(policy) {
    case TURN_POLICY_MANDATORY:
      return "mandatory";
    case TURN_POLICY_IF_NEEDED:
      return "as-needed";
    case TURN_POLICY_DISABLED:
      return "disabled";
    default:
      return "unknown";
  }
}

#endif // (VSX_HAVE_STREAMER)


