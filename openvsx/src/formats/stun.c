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


int stun_parse_attribs(STUN_MSG_T *pMsg, const unsigned char *pData, unsigned int len) {
  int rc = 0;
  unsigned int idx = 0;
  size_t sz;
  STUN_ATTRIB_T attr;

  while(idx < len) {

    if(idx + 4 >= len) {
      LOG(X_ERROR("Invalid STUN attribute header at %d/%d"), idx + STUN_HDR_LEN, len + STUN_HDR_LEN);
      return -1;
    }

    rc = 0;
    memset(&attr, 0, sizeof(attr));
    attr.type = ntohs(*((uint16_t *) &pData[idx]));
    idx += 2;
    attr.length = ntohs(*((uint16_t *) &pData[idx]));
    idx += 2;

    //LOG(X_DEBUGV("STUN attribute type:0x%x, len:%d at%d/%d"), attr.type, attr.length, idx+20, len+20);

    switch(attr.type) {

  /*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |0 0 0 0 0 0 0 0|    Family     |           Port                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                 Address (32 bits or 128 bits)                 |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */

      case STUN_ATTRIB_MAPPED_ADDRESS:
      case STUN_ATTRIB_XOR_MAPPED_ADDRESS:
      case TURN_ATTRIB_XOR_RELAYED_ADDRESS:
      case TURN_ATTRIB_XOR_PEER_ADDRESS:

        if(attr.length < 8) {
          LOG(X_WARNING("Invalid attribute STUN MAPPED-ADDRESS length: %d"), attr.length); 
          rc = -1;
          break;
        } else if(pData[idx] != 0) {
          LOG(X_WARNING("Invalid attribute STUN MAPPED-ADDRESS reserved field 0x%x"), pData[idx]); 
          rc = -1;
          break;
        }

        STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).family = pData[idx + 1];
        STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).port = htons(*((uint16_t *) &pData[idx + 2]));
        if(IS_STUN_ATTRIB_ADDRESS_XOR(attr.type)) {
          STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).port ^= (STUN_COOKIE >> 16);
        }
        memset(&STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv4, 0, 
               sizeof(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv4));
        if(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).family == STUN_MAPPED_ADDRESS_FAMILY_IPV4) {

          STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv4.s_addr = *((uint32_t *) &pData[idx + 4]);
          if(IS_STUN_ATTRIB_ADDRESS_XOR(attr.type)) {
            STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv4.s_addr = 
                  htonl(htonl(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv4.s_addr) ^ STUN_COOKIE);
          }

        } else if(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).family == STUN_MAPPED_ADDRESS_FAMILY_IPV6) {
          if(attr.length >= 20) {
            memcpy(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv6, &pData[idx + 4], 16);

          if(IS_STUN_ATTRIB_ADDRESS_XOR(attr.type)) {
            for(sz = 0; sz < attr.length - 4; sz += 4) {
              STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv6[sz/4] = 
                   htonl(htonl(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(attr).u_addr.ipv6[sz/4]) ^ STUN_COOKIE);
            }
          }

          } else {
            LOG(X_WARNING("Invalid STUN MAPPED-ADDRESS ipv6 length %d"), attr.length); 
            rc = -1;
          }
        }
        break;

      case STUN_ATTRIB_USERNAME:
      case STUN_ATTRIB_REALM:
      case STUN_ATTRIB_NONCE:
      case STUN_ATTRIB_SOFTWARE:

        if((sz = attr.length) > STUN_STRING_MAX - 1) {
          sz = STUN_STRING_MAX - 1; 
          LOG(X_WARNING("Truncating STUN attribute type: %d field from %d to %d"), attr.type, attr.length, sz);
          rc = -1;
        } 

        memcpy(attr.u.stringtype.value, &pData[idx], sz);
        attr.u.stringtype.value[sizeof(attr.u.stringtype.value) - 1] = '\0';
        break;

      case STUN_ATTRIB_ERROR_CODE:

        //
        //   0                   1                   2                   3
        //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //  |           Reserved, should be 0         |Class|     Number    |
        //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //  |      Reason Phrase (variable)                                ..
        //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //

        if(attr.length < 4) {
          LOG(X_WARNING("Invalid attribute STUN ERROR-CODE length: %d"), attr.length); 
          rc = -1;
          break;
        } 
        if((sz = attr.length - 4) > STUN_STRING_MAX - 1) {
          sz = STUN_STRING_MAX - 1; 
          LOG(X_WARNING("Truncating STUN attribute field from %d to %d"), attr.length - 4, sz);
        }
        attr.u.error.code = htonl(*((uint32_t *)&pData[idx]));
        memcpy(attr.u.error.value, &pData[idx + 4], sz);
        attr.u.error.value[sizeof(attr.u.error.value) - 1] = '\0';

        break;

      case STUN_ATTRIB_MESSAGE_INTEGRITY:
        if(attr.length == STUN_HMAC_SZ) {
          memcpy(attr.u.msgIntegrity.hmac, &pData[idx], attr.length);
        } else {
          LOG(X_ERROR("Invalid STUN message-integrity attribute length %d"), attr.length);
          rc = -1;
        }
        break;

      case STUN_ATTRIB_FINGERPRINT:
        if(attr.length >= 4) {
          attr.u.fingerPrint.crc = htonl(*((uint32_t *) &pData[idx]));
        } else {
          LOG(X_ERROR("Invalid STUN fingerprint attribute length %d"), attr.length);
          rc = -1;
        }
        break;

      case STUN_ATTRIB_ICE_PRIORITY:
        if(attr.length >= 4) {
          attr.u.icePriority.value = htonl(*((uint32_t *) &pData[idx]));
        } else {
          LOG(X_ERROR("Invalid STUN ice-priority attribute length %d"), attr.length);
          rc = -1;
        }
        break;

      case STUN_ATTRIB_ICE_USE_CANDIDATE:
        // empty fields
        break;

      case STUN_ATTRIB_ICE_CONTROLLING:
      case STUN_ATTRIB_ICE_CONTROLLED:
        if(attr.length >= 8) {
          attr.u.iceControlling.tieBreaker = HTONL64(*((uint64_t *) &pData[idx]));
        } else {
          LOG(X_WARNING("STUN ice-controll%sattribute length %d does not include tiebreaker value"), 
                   attr.type == STUN_ATTRIB_ICE_CONTROLLING ? "ing" : "ed", attr.length);
          //rc = -1;
        }
        break;

      case TURN_ATTRIB_REQUESTED_TRANSPORT:
        if(attr.length >= 4) {
          attr.u.requestedTransport.protocol = pData[idx];
        } else {
          LOG(X_ERROR("Invalid TURN requestd-transport attribute length %d"), attr.length);
          rc = -1;
        }
        break;

      case TURN_ATTRIB_LIFETIME:
        if(attr.length >= 4) {
          attr.u.lifetime.value = htonl(*((uint32_t *) &pData[idx]));
        } else {
          LOG(X_ERROR("Invalid TURN lifetime attribute length %d"), attr.length);
          rc = -1;
        }
        break;

      case TURN_ATTRIB_DATA:
        attr.u.data.pData = &pData[idx];
        break;

      default:
        LOG(X_WARNING("Unhandled STUN attribute type: 0x%x, length:%d at offset:[%d]/%d"), 
             attr.type, attr.length, idx + STUN_HDR_LEN, len + STUN_HDR_LEN);
        rc = -1;
        break;
    }

    idx += attr.length;

    //
    // Skip any padding
    //
    if((attr.length & 0x03)) {
      idx += 4 - (attr.length & 0x03);
    }

    if(rc >= 0 && pMsg->numAttribs < STUN_MAX_ATTRIBS) {
      memcpy(&pMsg->attribs[pMsg->numAttribs], &attr, sizeof(STUN_ATTRIB_T));
      pMsg->numAttribs++;
    }

  }

  return rc;
}

int stun_parse(STUN_MSG_T *pMsg, const unsigned char *pData, unsigned int len) {
  int rc = 0;
  uint32_t cookie;

  /*

     RFC 5389 - 6.  STUN Message Structure
         0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |0 0|     STUN Message Type     |         Message Length        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         Magic Cookie                          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                                                               |
    |                     Transaction ID (96 bits)                  |
    |                                                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  */

  if(len < STUN_HDR_LEN) {
    return -1;
  }

  if((pData[0] & 0xc) != 0) {
    LOG(X_ERROR("Invalid STUN header bits: 0x%x"), pData[0]);
    return -1;
  }

  pMsg->type = ntohs(*((uint16_t *) &pData[0])) & 0x3fff;

  pMsg->length = ntohs(*((uint16_t *) &pData[2]));
  if((cookie = ntohl(*((uint32_t *) &pData[4]))) != STUN_COOKIE) {
    LOG(X_ERROR("Invalid STUN cookie 0x%x"), cookie);
    return -1;
  }
  memcpy(pMsg->id, &pData[8], STUN_ID_LEN);

  if(STUN_HDR_LEN + pMsg->length < len) {
    LOG(X_ERROR("Invalid STUN packet length %u, total:%d"), pMsg->length, len);
    return -1;
  }

  pMsg->numAttribs = 0;

  rc = stun_parse_attribs(pMsg, &pData[STUN_HDR_LEN], len - STUN_HDR_LEN);

  return rc;
}

/*
static void generate_id(unsigned char id[STUN_ID_LEN]) {
  unsigned int idx;

  for(idx = 0; idx < STUN_ID_LEN; idx += 4) {
    *((uint32_t *) &id[idx]) = random();
  } 

}
*/

static int serialize_stringtype(const STUN_ATTRIB_STRINGTYPE_T *pAttr, 
                              unsigned int length, unsigned char *buf) {
  memcpy(buf, pAttr->value, length); 
  return 0;
}

static int serialize_error(const STUN_ATTRIB_ERROR_T *pError, 
                           unsigned int length, unsigned char *buf) {
  if(length < 4) {
    return -1;
  }
  *((uint32_t *) buf) = htonl(pError->code);
  memcpy(&buf[4], pError->value, length - 4); 
  return 0;
}


static int serialize_mapped_address(const STUN_ATTRIB_MAPPED_ADDRESS_T *pAddr, 
                                    unsigned int length, unsigned char *buf, int xor) {
  int rc = 0;
  struct in_addr ipv4;
  uint16_t port;

  if(pAddr->family != STUN_MAPPED_ADDRESS_FAMILY_IPV4) {
    return -1;
  } else if(length != 8) {
    return -1;
  }
  buf[0] = 0x00;
  buf[1] = pAddr->family;
  port = pAddr->port;
  memcpy(&ipv4, &pAddr->u_addr.ipv4, sizeof(ipv4));

  if(xor) {
   port ^= (STUN_COOKIE >> 16);
   ipv4.s_addr = htonl( htonl(ipv4.s_addr) ^ STUN_COOKIE );
  }
  *((uint16_t *) &buf[2]) = htons(port);
  *((uint32_t *) &buf[4]) = (ipv4.s_addr);

  return rc;
}

static int serialize_ice_controlling(const STUN_ATTRIB_ICE_CONTROLLING_T *pAttr, 
                                    unsigned int length, unsigned char *buf) {
  uint64_t tieBreaker = HTONL64(pAttr->tieBreaker);
  memcpy(buf, &tieBreaker, 8);
  return 0;
}

static int serialize_ice_priority(const STUN_ATTRIB_ICE_PRIORITY_T *pAttr, 
                                    unsigned int length, unsigned char *buf) {
  uint64_t value = htonl(pAttr->value);
  memcpy(buf, &value, 8);
  return 0;
}

static int serialize_requested_transport(const TURN_ATTRIB_REQUESTED_TRANSPORT_T *pAttr, 
                                    unsigned int length, unsigned char *buf) {
  buf[0] = pAttr->protocol;
  memset(&buf[1], 0, 3);
  return 0;
}

static int serialize_lifetime(const TURN_ATTRIB_LIFETIME_T *pAttr, unsigned char *buf) {
  uint32_t value = htonl(pAttr->value);
  memcpy(buf, &value, 4);
  return 0;
}

static int serialize_channel(const TURN_ATTRIB_CHANNEL_T *pAttr, unsigned char *buf) {
  uint32_t value = htonl(((uint32_t) pAttr->channel) << 16);
  memcpy(buf, &value, 4);
  return 0;
}

static int serialize_data(const TURN_ATTRIB_DATA_T *pAttr, unsigned int len, unsigned char *buf) {
  if(pAttr->pData) {
    memcpy(buf, pAttr->pData, len);
  } else if(len > 0) {
    return -1;
  }
  return 0;
}

static int serialize_hmac(const unsigned char *data, unsigned int lenData, 
                          const STUN_MESSAGE_INTEGRITY_PARAM_T *pParam, unsigned char *hmac) {
  int rc;
  const unsigned char *key = NULL;
  unsigned int lenKey = 0;
  char str[256];
  unsigned char md5sum[MD5_DIGEST_LENGTH];

  if(pParam->user && pParam->user[0] != '\0' && pParam->pass && pParam->pass[0] != '\0' && 
    pParam->realm && pParam->realm[0] != '\0') {

    //
    // RFC 5389 Long term credentials 
    //
    if(snprintf(str, sizeof(str), "%s:%s:%s", pParam->user, pParam->realm, pParam->pass) < 0) {
      return -1;
    }
    md5_calculate((const unsigned char *) str, strlen(str), md5sum);
    key = md5sum;
    lenKey = MD5_DIGEST_LENGTH;
    //avc_dumpHex(stderr, md5sum, MD5_DIGEST_LENGTH, 1);

  } else if(pParam->pass && pParam->pass[0] != '\0') {

    //
    // RFC 5389 Short term credentials 
    //
    key = (const unsigned char *) pParam->pass;
    lenKey = strlen(pParam->pass);
  } else {
    return -1;
  }

  if((rc = hmac_calculate(data, lenData, key, lenKey, hmac, HMAC_CALC_TYPE_SHA1)) < 0) {
    return rc;
  }

  return STUN_HMAC_SZ;
}

int stun_serialize(const STUN_MSG_T *pMsg, unsigned char *buf, unsigned int bufLen,
                   const STUN_MESSAGE_INTEGRITY_PARAM_T *phmacParam) {
  int rc = 0;
  unsigned int idx = 0;
  unsigned int idxAttr;
  unsigned int paddingBytes;
  uint32_t crc;
  int do_fingerprint = 0;
  int do_hmac = 0;
  const STUN_ATTRIB_T *pAttr;

  if(bufLen < STUN_HDR_LEN) {
    return -1;
  }

  *((uint16_t *) &buf[idx]) = htons(pMsg->type);
  idx += 4;
  *((uint32_t *) &buf[idx]) = htonl(STUN_COOKIE);
  idx += 4;
  memcpy(&buf[idx], pMsg->id, STUN_ID_LEN);
  idx += STUN_ID_LEN;
  
  //
  // Serialize STUN attributes
  //
  for(idxAttr = 0; idxAttr < pMsg->numAttribs; idxAttr++) {
    pAttr = &pMsg->attribs[idxAttr];

    if(pAttr->type == STUN_ATTRIB_FINGERPRINT) {
      do_fingerprint = 1;
      continue;
    } else if(pAttr->type == STUN_ATTRIB_MESSAGE_INTEGRITY) {
      if(!phmacParam || (!phmacParam->user && !phmacParam->pass && !phmacParam->realm)) {
        LOG(X_ERROR("Unable to serialize STUN message-integrity:  HMAC parameters not set"));
        return -1;
      }
      do_hmac = 1;
      continue;
    }

    if((paddingBytes = (pAttr->length & 0x03))) {
      paddingBytes = 4 - paddingBytes;
    }

    if(idx + 4 + pAttr->length + paddingBytes > bufLen) {
      return -1;
    }

    *((uint16_t *) &buf[idx]) = htons(pAttr->type);
    *((uint16_t *) &buf[idx + 2]) = htons(pAttr->length);
    idx += 4;

    rc = 0;
    switch(pAttr->type) {
      case STUN_ATTRIB_ERROR_CODE:
        rc = serialize_error(&pAttr->u.error, pAttr->length, &buf[idx]);
        break;
      case STUN_ATTRIB_USERNAME:
      case STUN_ATTRIB_REALM:
      case STUN_ATTRIB_NONCE:
      case STUN_ATTRIB_SOFTWARE:
        rc = serialize_stringtype(&pAttr->u.stringtype, pAttr->length, &buf[idx]);
        break;
      case TURN_ATTRIB_XOR_RELAYED_ADDRESS:
      case TURN_ATTRIB_XOR_PEER_ADDRESS:
      case STUN_ATTRIB_XOR_MAPPED_ADDRESS:
      case STUN_ATTRIB_MAPPED_ADDRESS:
        rc = serialize_mapped_address(&STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr), pAttr->length, &buf[idx], 
                                     IS_STUN_ATTRIB_ADDRESS_XOR(pAttr->type) ? 1 : 0);
        break;
      case STUN_ATTRIB_ICE_CONTROLLING:
      case STUN_ATTRIB_ICE_CONTROLLED:
        rc = serialize_ice_controlling(&pAttr->u.iceControlling, pAttr->length, &buf[idx]);
        break;
    case STUN_ATTRIB_ICE_USE_CANDIDATE:
        break;
    case STUN_ATTRIB_ICE_PRIORITY:
        rc = serialize_ice_priority(&pAttr->u.icePriority, pAttr->length, &buf[idx]);
        break;
    case TURN_ATTRIB_REQUESTED_TRANSPORT:
        rc = serialize_requested_transport(&pAttr->u.requestedTransport, pAttr->length, &buf[idx]);
        break;
    case TURN_ATTRIB_LIFETIME:
        rc = serialize_lifetime(&pAttr->u.lifetime, &buf[idx]);
        break;
    case TURN_ATTRIB_CHANNEL_NUMBER:
        rc = serialize_channel(&pAttr->u.channel, &buf[idx]);
        break;
    case TURN_ATTRIB_DATA:
        rc = serialize_data(&pAttr->u.data, pAttr->length, &buf[idx]);
        break;
      default:
        LOG(X_ERROR("Unhandled STUN attribute type: 0x%x to serialize at offset[%d]"), pAttr->type, idx - 4);
        LOGHEX_DEBUG(buf, idx);
        return -1;
    }

    if(rc < 0) {
      return rc;
    }

    //
    // RFC 5769 uses 0x20 padding bytes, instead of 0x00
    //
    if(0 && paddingBytes > 0) {
      memset(&buf[idx + pAttr->length], 0x20, paddingBytes);
      LOG(X_DEBUG("Adding %d STUN '0x20' padding bytes to attribute: 0x%x"), paddingBytes, pAttr->type);
    }

    idx += pAttr->length + paddingBytes;
  }

  //
  // Set message attributes length field
  //
  *((uint16_t *) &buf[2]) = htons(idx - STUN_HDR_LEN);

  //
  // Create an HMAC of the STUN message 
  //
  if(do_hmac) {

    if(idx + 4 + STUN_HMAC_SZ > bufLen) {
      return -1;
    }

    //
    // Set the STUN message length field prior to computing the HMAC
    //
    *((uint16_t *) &buf[2]) = htons(idx - STUN_HDR_LEN + 4 + STUN_HMAC_SZ);

    if((rc = serialize_hmac(buf, idx, phmacParam, &buf[idx + 4])) < 0) {
      return rc;
    }

//unsigned char hm[] = { 0x54,0xa4,0x5f,0x50,0x75,0x89,0xfb,0x0f, 0xeb,0x78,0xce,0x99,0x89,0x0e,0xc7,0x74,0xdb,0xf8,0x2b,0x90 }; memcpy(&buf[idx + 4], hm, 20);

    *((uint16_t *) &buf[idx]) = htons(STUN_ATTRIB_MESSAGE_INTEGRITY);
    *((uint16_t *) &buf[idx + 2]) = htons(STUN_HMAC_SZ);
    idx += 4 + STUN_HMAC_SZ;

  }

  //
  // Create a CRC fingerprint of the STUN message
  //
  if(do_fingerprint) {

    if(idx + 8 > bufLen) {
      return -1;
    }

    //
    // Set the STUN message length field prior to computing the fingerprint CRC 
    //
    *((uint16_t *) &buf[2]) = htons(idx - STUN_HDR_LEN + 8);

    crc = ituv42_crc32(buf, idx) ^ STUN_FINGERPRINT_XOR;
    *((uint16_t *) &buf[idx]) = htons(STUN_ATTRIB_FINGERPRINT);
    *((uint16_t *) &buf[idx + 2]) = htons(4);
    *((uint32_t *) &buf[idx + 4]) = htonl(crc);
    idx += 8;
  }

  return idx;
}

int stun_validate_hmac(const unsigned char *pData, unsigned int len,
                       const STUN_MESSAGE_INTEGRITY_PARAM_T *phmacParam) {
  int rc = 0;
  unsigned char buf[STUN_MAX_SZ];
  unsigned int idx = len;
  unsigned char hmac[STUN_HMAC_SZ];

  //
  // Returns 0 if HMAC is not present,
  // -1 if HMAC is invalid
  // -2 if HMAC computation encountered an error, such as password not provided
  // 1 if HMAC is correct
  //

  if(pData && len >= STUN_HDR_LEN + 8 && htons(*((uint16_t *) &pData[len - 8])) == STUN_ATTRIB_FINGERPRINT) {
    idx = len - 8;
  }

  if(idx >= STUN_HDR_LEN + 8 + STUN_HMAC_SZ && 
     htons(*((uint16_t *) &pData[idx - (STUN_HMAC_SZ + 4) ])) == STUN_ATTRIB_MESSAGE_INTEGRITY) {
    idx -= (STUN_HMAC_SZ + 4);
    if(idx > STUN_MAX_SZ) {
      return -2;
    }

    //
    // Set the length field to not include anything after the HMAC, such as any fingerprint
    //
    memcpy(buf, pData, idx);
    *((uint16_t *) &buf[2]) = htons(idx + 4 + STUN_HMAC_SZ - STUN_HDR_LEN);
    if((rc = serialize_hmac(buf, idx, phmacParam, hmac)) < 0) {
      return -2;
    }

    if(0 == memcmp(hmac, &pData[idx + 4], STUN_HMAC_SZ)) {
      return 1;
    } else {
      return -1;
    }

  }
    
  return 0;
}

int stun_validate_crc(const unsigned char *pData, unsigned int len) {
  uint32_t crc;

  //
  // Returns 0 if CRC is not present,
  // -1 if CRC is invalid
  // 1 if CRC is correct
  //

  if(pData && len >= STUN_HDR_LEN + 8 && htons(*((uint16_t *) &pData[len - 8])) == STUN_ATTRIB_FINGERPRINT) {

    crc = ituv42_crc32(pData, len - 8) ^ STUN_FINGERPRINT_XOR;

    if(*((uint32_t *) &pData[len - 4]) == htonl(crc)) {
      return 1;
    } else {
      return -1;
    }

  }

  return 0;
}

const STUN_ATTRIB_T *stun_findattrib(const STUN_MSG_T *pMsg, int attribType) {
  unsigned int idx;

  if(!pMsg) {
    return NULL;
  }

  for(idx = 0; idx < pMsg->numAttribs; idx++) {
    if(pMsg->attribs[idx].type == attribType) {
      return &pMsg->attribs[idx];
    }
  }
  return NULL;
}

int stun_ispacket(const unsigned char *data, unsigned int len) {

  if(data && len >= STUN_HDR_LEN && *((uint32_t *) &data[4]) == htonl(STUN_COOKIE) &&
     htons(*((uint16_t *) &data[2])) + STUN_HDR_LEN == len) {
    return 1;
  }

  return 0;
}

#define STUN_CLASS2STR(a,c) (((c) == STUN_MSG_CLASS_RESPONSE_ERROR) ? a" Response Error" : \
                           ((c) == STUN_MSG_CLASS_RESPONSE_SUCCESS) ? a" Response" : \
                           ((c) == STUN_MSG_CLASS_INDICATION) ? a" Indication" : \
                           ((c) == STUN_MSG_CLASS_REQUEST) ? a" Request" : a)

const char *stun_type2str(uint16_t type) {
  int msgClass = STUN_MSG_CLASS(type);
  int msgMethod = STUN_MSG_METHOD(type);

  switch(msgMethod) {
    case STUN_MSG_METHOD_BINDING:
      return STUN_CLASS2STR("Binding", msgClass);
    case TURN_MSG_METHOD_ALLOCATE:
      return STUN_CLASS2STR("Allocate", msgClass);
    case TURN_MSG_METHOD_REFRESH:
      return STUN_CLASS2STR("Refresh", msgClass);
    case TURN_MSG_METHOD_SEND:
      return STUN_CLASS2STR("Send", msgClass);
    case TURN_MSG_METHOD_DATA:
      return STUN_CLASS2STR("Data", msgClass);
    case TURN_MSG_METHOD_CREATEPERM:
      return STUN_CLASS2STR("CreatePermission", msgClass);
    case TURN_MSG_METHOD_CHANNELBIND:
      return STUN_CLASS2STR("ChannelBind", msgClass);
    default:
      return STUN_CLASS2STR("Unknown", msgClass);
  }

}

char *stun_log_error(char *buf, unsigned int sz, const STUN_MSG_T *pMsg) {
  int rc = 0;
  const STUN_ATTRIB_T *pAttr;

  if(pMsg && (pAttr = stun_findattrib(pMsg, STUN_ATTRIB_ERROR_CODE))) {
    rc = snprintf(buf, sz, "error-code: 0x%x '%s'", pAttr->u.error.code, pAttr->u.error.value);
  } 

  if(rc <= 0 && sz > 0) {
    buf[0] = '\0'; 
  }

  return buf;
}

void stun_dump_attrib(FILE *fp, const STUN_ATTRIB_T *pAttr) {
  //STUN_ATTRIB_MAPPED_ADDRESS_T mappedAddr;
  unsigned int idx;

  fprintf(fp, "attrib type: 0x%x, length:%d - ", pAttr->type, pAttr->length);

  switch(pAttr->type) {
/*
    case STUN_ATTRIB_XOR_MAPPED_ADDRESS:
      memcpy(&mappedAddr, &pAttr->u.xorMappedAddress,  sizeof(mappedAddr));
      mappedAddr.port ^= (STUN_COOKIE >> 16);
      mappedAddr.u_addr.ipv4.s_addr = htonl(htonl(mappedAddr.u_addr.ipv4.s_addr) ^ STUN_COOKIE);
      fprintf(fp, "xor-mapped-address family:%d, %s:%d\n", mappedAddr.family, inet_ntoa(mappedAddr.u_addr.ipv4), mappedAddr.port);
      break;
*/
    case STUN_ATTRIB_ERROR_CODE:
      fprintf(fp, "error-code: 0x%x '%s'\n", pAttr->u.error.code, pAttr->u.error.value);
      break;
    case STUN_ATTRIB_MAPPED_ADDRESS:
    case STUN_ATTRIB_XOR_MAPPED_ADDRESS:
    case TURN_ATTRIB_XOR_RELAYED_ADDRESS:
    case TURN_ATTRIB_XOR_PEER_ADDRESS:
      fprintf(fp, "%s%s-address family:%d, %s:%d\n", 
         IS_STUN_ATTRIB_ADDRESS_XOR(pAttr->type) ? "xor-" : "", 
         pAttr->type == TURN_ATTRIB_XOR_RELAYED_ADDRESS ? "relayed" : 
         pAttr->type == TURN_ATTRIB_XOR_PEER_ADDRESS ? "peer" : "mapped",
         STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).family, 
         inet_ntoa(STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).u_addr.ipv4), 
         STUN_ATTRIB_VALUE_MAPPED_ADDRESS(*pAttr).port);
      break;
    case STUN_ATTRIB_USERNAME:
      fprintf(fp, "username: '%s'\n", pAttr->u.stringtype.value);
      break;
    case STUN_ATTRIB_REALM:
      fprintf(fp, "realm: '%s'\n", pAttr->u.stringtype.value);
      break;
    case STUN_ATTRIB_NONCE:
      fprintf(fp, "nonce: '%s'\n", pAttr->u.stringtype.value);
      break;
    case STUN_ATTRIB_SOFTWARE:
      fprintf(fp, "software: '%s'\n", pAttr->u.stringtype.value);
      break;
    case STUN_ATTRIB_ICE_CONTROLLING:
      fprintf(fp, "ice-controlling tiebreaker: 0x%llx\n", pAttr->u.iceControlling.tieBreaker);
      break;
    case STUN_ATTRIB_ICE_CONTROLLED:
      fprintf(fp, "ice-controlled tiebreaker: 0x%llx\n", pAttr->u.iceControlled.tieBreaker);
      break;
    case STUN_ATTRIB_ICE_USE_CANDIDATE:
      fprintf(fp, "ice-use-candidate\n");
      break;
    case STUN_ATTRIB_ICE_PRIORITY:
      fprintf(fp, "ice-priority value: 0x%x\n", pAttr->u.icePriority.value);
      break;
    case TURN_ATTRIB_REQUESTED_TRANSPORT:
      fprintf(fp, "requested-transport protocol: 0x%x\n", pAttr->u.requestedTransport.protocol);
      break;
    case TURN_ATTRIB_LIFETIME:
      fprintf(fp, "lifetime: 0x%x (%d)\n", pAttr->u.lifetime.value, pAttr->u.lifetime.value);
      break;
    case TURN_ATTRIB_DATA:
      fprintf(fp, "data");
      break;
    case STUN_ATTRIB_MESSAGE_INTEGRITY:
      fprintf(fp, "message-integrity: ");
      for(idx = 0; idx < STUN_HMAC_SZ; idx++) { 
        fprintf(fp, "%02x", pAttr->u.msgIntegrity.hmac[idx]);
      }
      fprintf(fp, "\n");
      break;
    case STUN_ATTRIB_FINGERPRINT:
      fprintf(fp, "fingerprint : 0x%02x\n", pAttr->u.fingerPrint.crc);
      break;
    default:
      fprintf(fp, "unhandled attribute\n");
      break;
  }


}

void stun_dump(FILE *fp, const STUN_MSG_T *pMsg) {
  unsigned int idx;

  fprintf(fp, "STUN type raw: 0x%x, class:0x%x, type:0x%x, length:%d, id: ", pMsg->type, STUN_MSG_CLASS(pMsg->type), STUN_MSG_METHOD(pMsg->type), pMsg->length);

  for(idx = 0; idx < STUN_ID_LEN; idx++) {
    fprintf(fp, "%02x", pMsg->id[idx]);
  }
  fprintf(fp, "\n");

  for(idx = 0; idx < pMsg->numAttribs; idx++) {
    stun_dump_attrib(fp, &pMsg->attribs[idx]);
  }

}
