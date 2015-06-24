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


#ifndef __FMT_STUN_H__
#define __FMT_STUN_H__

#include "util/math_common.h" // htonl64
#include "formats/ice.h"

#define     STUN_HDR_LEN             20
#define     STUN_ID_LEN              12
#define     STUN_COOKIE              0x2112a442
#define     STUN_FINGERPRINT_XOR     0x5354554e

// RFC 5766 (TURN) 11.  Channels
#define     TURN_CHANNEL_HDR_LEN     4 

//
// RFC 5389 STUN attribute types
//  comprehension-required attributes
//
#define STUN_ATTRIB_RESERVED            0x0000
#define STUN_ATTRIB_MAPPED_ADDRESS      0x0001
#define STUN_ATTRIB_RESPONSE_ADDRESS    0x0002
#define STUN_ATTRIB_CHANGE_ADDRESS      0x0003
#define STUN_ATTRIB_SOURCE_ADDRESS      0x0004
#define STUN_ATTRIB_CHANGED_ADDRESS     0x0005
#define STUN_ATTRIB_USERNAME            0x0006
#define STUN_ATTRIB_PASSWORD            0x0007
#define STUN_ATTRIB_MESSAGE_INTEGRITY   0x0008
#define STUN_ATTRIB_ERROR_CODE          0x0009
#define STUN_ATTRIB_UNKNOWN             0x000a
#define STUN_ATTRIB_REFLECTED_FROM      0x000b
#define STUN_ATTRIB_REALM               0x0014
#define STUN_ATTRIB_NONCE               0x0015
#define STUN_ATTRIB_XOR_MAPPED_ADDRESS  0x0020

// RFC 5766 (TURN) 14.  New STUN Attributes

#define TURN_ATTRIB_CHANNEL_NUMBER      0x000c
#define TURN_ATTRIB_LIFETIME            0x000d
#define TURN_ATTRIB_XOR_PEER_ADDRESS    0x0012
#define TURN_ATTRIB_DATA                0x0013
#define TURN_ATTRIB_XOR_RELAYED_ADDRESS 0x0016
#define TURN_ATTRIB_REQUESTED_TRANSPORT 0x0019

//
// RFC 5389 STUN attribute types
//  comprehension-optional attributes
//
#define STUN_ATTRIB_SOFTWARE            0x8022
#define STUN_ATTRIB_ALTERNATE_SERVER    0x8023
#define STUN_ATTRIB_FINGERPRINT         0x8028

//
// RFC 5245 STUN ICE attribute types
//  comprehension-required attributes
//
#define STUN_ATTRIB_ICE_PRIORITY        0x0024
#define STUN_ATTRIB_ICE_USE_CANDIDATE   0x0025
//
//comprehension-optional attributes (0x8000)
//
#define STUN_ATTRIB_ICE_CONTROLLED      0x8029
#define STUN_ATTRIB_ICE_CONTROLLING     0x802a

#define STUN_MAX_SZ        2048
#define STUN_MAX_ATTRIBS   10 
#define STUN_STRING_MAX    64
#define STUN_HMAC_SZ       20

typedef struct _STUN_ATTRIB_ERROR {
  uint32_t            code; 
  char                value[STUN_STRING_MAX]; 
} STUN_ATTRIB_ERROR_T;

typedef struct _STUN_ATTRIB_MAPPED_ADDRESS {

#define STUN_MAPPED_ADDRESS_FAMILY_IPV4   0x01
#define STUN_MAPPED_ADDRESS_FAMILY_IPV6   0x02

  uint8_t             reserved;
  uint8_t             family;
  uint16_t            port;
  union {
    struct in_addr    ipv4;
    uint32_t          ipv6[4];
  } u_addr;
} STUN_ATTRIB_MAPPED_ADDRESS_T;

typedef struct _STUN_ATTRIB_STRINGTYPE {
  char                value[STUN_STRING_MAX]; 
} STUN_ATTRIB_STRINGTYPE_T;

typedef STUN_ATTRIB_STRINGTYPE_T STUN_ATTRIB_USERNAME_T;
typedef STUN_ATTRIB_STRINGTYPE_T STUN_ATTRIB_SOFTWARE_T;
typedef STUN_ATTRIB_STRINGTYPE_T STUN_ATTRIB_REALM_T;
typedef STUN_ATTRIB_STRINGTYPE_T STUN_ATTRIB_NONCE_T;
typedef STUN_ATTRIB_MAPPED_ADDRESS_T  STUN_ATTRIB_XOR_MAPPED_ADDRESS_T;

typedef struct _STUN_ATTRIB_MESSAGE_INTEGRITY {
  unsigned char           hmac[STUN_HMAC_SZ]; 
} STUN_ATTRIB_MESSAGE_INTEGRITY_T;

typedef struct _STUN_ATTRIB_FINGERPRINT {
  uint32_t crc;
} STUN_ATTRIB_FINGERPRINT_T;

typedef struct _STUN_ATTRIB_ICE_PRIORITY {
  uint32_t value;
} STUN_ATTRIB_ICE_PRIORITY_T;

typedef struct _STUN_ATTRIB_ICE_USE_CANDIDATE {
  int blank;
} STUN_ATTRIB_ICE_USE_CANDIDATE_T;


typedef struct _STUN_ATTRIB_ICE_CONTROLLING {
  uint64_t tieBreaker;
} STUN_ATTRIB_ICE_CONTROLLING_T;

typedef STUN_ATTRIB_ICE_CONTROLLING_T STUN_ATTRIB_ICE_CONTROLLED_T;

//
// RFC 5766 (TURN) 14.  New STUN Attributes
//
typedef struct _TURN_ATTRIB_REQUESTED_TRANSPORT {
  uint8_t protocol;
  uint8_t rffu[3];
} TURN_ATTRIB_REQUESTED_TRANSPORT_T;

typedef struct _TURN_ATTRIB_LIFETIME {
  uint32_t value; 
} TURN_ATTRIB_LIFETIME_T;

typedef struct _TURN_ATTRIB_CHANNEL{
  uint16_t channel; 
  uint16_t reserved; 
} TURN_ATTRIB_CHANNEL_T;

typedef struct _TURN_ATTRIB_DATA {
  const unsigned char *pData; 
} TURN_ATTRIB_DATA_T;

typedef STUN_ATTRIB_MAPPED_ADDRESS_T  TURN_ATTRIB_XOR_RELAYED_ADDRESS_T;
typedef STUN_ATTRIB_MAPPED_ADDRESS_T  TURN_ATTRIB_XOR_PEER_ADDRESS_T;

typedef struct STUN_ATTRIB {
  uint16_t                  type;
  uint16_t                  length;

  union {

    STUN_ATTRIB_ERROR_T               error;
    STUN_ATTRIB_MAPPED_ADDRESS_T      mappedAddress;   
    STUN_ATTRIB_XOR_MAPPED_ADDRESS_T  xorMappedAddress; 
    STUN_ATTRIB_STRINGTYPE_T          stringtype;
    STUN_ATTRIB_MESSAGE_INTEGRITY_T   msgIntegrity;
    STUN_ATTRIB_FINGERPRINT_T         fingerPrint;
    STUN_ATTRIB_ICE_CONTROLLING_T     iceControlling;
    STUN_ATTRIB_ICE_CONTROLLED_T      iceControlled;
    STUN_ATTRIB_ICE_USE_CANDIDATE_T   iceUseCandidate;
    STUN_ATTRIB_ICE_PRIORITY_T        icePriority;

    TURN_ATTRIB_REQUESTED_TRANSPORT_T requestedTransport; 
    TURN_ATTRIB_LIFETIME_T            lifetime; 
    TURN_ATTRIB_CHANNEL_T             channel; 
    TURN_ATTRIB_XOR_RELAYED_ADDRESS_T xorRelayedAddress;
    TURN_ATTRIB_XOR_PEER_ADDRESS_T    xorPeerAddress;
    TURN_ATTRIB_DATA_T                data;

  } u;

} STUN_ATTRIB_T;

//
// xorMappedAddress, xorRelayedAddress are all referenced through mappedAddress field since they're in a union
//
#define STUN_ATTRIB_VALUE_MAPPED_ADDRESS(x)          ((x).u.mappedAddress)
#define STUN_ATTRIB_VALUE_XOR_MAPPED_ADDRESS(x)      STUN_ATTRIB_VALUE_MAPPED_ADDRESS
#define TURN_ATTRIB_VALUE_XOR_RELAYED_ADDRESS(x)     ((x).u.mappedAddress)
#define TURN_ATTRIB_VALUE_RELAYED_ADDRESS            TURN_ATTRIB_VALUE_XOR_RELAYED_ADDRESS
#define TURN_ATTRIB_VALUE_XOR_PEER_ADDRESS(x)        ((x).u.mappedAddress)
#define TURN_ATTRIB_VALUE_PEER_ADDRESS(x)            ((x).u.mappedAddress)

#define IS_STUN_ATTRIB_ADDRESS_XOR(x) ((x) == STUN_ATTRIB_XOR_MAPPED_ADDRESS || \
                                       (x) == TURN_ATTRIB_XOR_RELAYED_ADDRESS || \
                                       (x) == TURN_ATTRIB_XOR_PEER_ADDRESS)

//
// RFC 5389 15.6.  ERROR-CODE
// RFC 5766 (TURN) 6.4 Receiving an Allocate Error Response
//
#define STUN_ERROR_CLASS(code)  (((code) >> 8) & 0x07)
#define STUN_ERROR_NUMBER(code)  ((code) & 0xff)

#define STUN_ERROR_CODE_MASK                        0x7ff
#define STUN_ERROR_CODE_TRY_ALTERNATE               0x0300
#define STUN_ERROR_CODE_BAD_REQUEST                 0x0400
#define STUN_ERROR_CODE_UNAUTHORIZED                0x0401
#define STUN_ERROR_CODE_FORBIDDEN                   0x0403
#define STUN_ERROR_CODE_UNKNOWN_ATTRIBUTE           0x0414
#define TURN_ERROR_CODE_ALLOCATION_MISMATCH         0x0425
#define TURN_ERROR_CODE_STALE_NONCE                 0x0426
#define TURN_ERROR_CODE_WRONG_CREDENTIALS           0x0429
#define TURN_ERROR_CODE_UNSUPPORTED_TRANSPORT       0x042a
#define TURN_ERROR_CODE_ALLOCATION_QUOTA_EXCEEDED   0x0456
#define STUN_ERROR_CODE_SERVER_ERROR                0x0500
#define TURN_ERROR_CODE_INSUFFICIENT_CAPACITY       0x0508

/*
    RFC 5389
  0                 1
  2  3  4 5 6 7 8 9 0 1 2 3 4 5

  +--+--+-+-+-+-+-+-+-+-+-+-+-+-+
  |M |M |M|M|M|C|M|M|M|C|M|M|M|M|
  |11|10|9|8|7|1|6|5|4|0|3|2|1|0|
  +--+--+-+-+-+-+-+-+-+-+-+-+-+-+
*/

#define STUN_MSG_CLASS(t) ((((t) & 0x0100) >> 7) | (((t) & 0x0010) >> 4))
#define STUN_MSG_METHOD(t) ( (((t) & 0x3e00) >> 2) | ((t) & 0x00e0) >> 1) | (((t) & 0x000f))

#define STUN_MSG_CLASS_REQUEST           0x00
#define STUN_MSG_CLASS_INDICATION        0x01
#define STUN_MSG_CLASS_RESPONSE_SUCCESS  0x02
#define STUN_MSG_CLASS_RESPONSE_ERROR    0x03

#define STUN_MSG_METHOD_BINDING                     0x0001
#define STUN_MSG_TYPE_BINDING_REQUEST               (STUN_MSG_METHOD_BINDING)
#define STUN_MSG_TYPE_BINDING_RESPONSE_SUCCESS      (0x0100 | STUN_MSG_METHOD_BINDING) 
#define STUN_MSG_TYPE_BINDING_RESPONSE_ERROR        (0x0110 | STUN_MSG_METHOD_BINDING)

#define STUN_MSG_TYPE_INDICATION_REQUEST            0x0011

// RFC 5766 (TURN) 13.  New STUN Methods allocate request/response
#define TURN_MSG_METHOD_ALLOCATE                    0x0003
#define TURN_MSG_TYPE_ALLOCATE_REQUEST              (TURN_MSG_METHOD_ALLOCATE)
#define TURN_MSG_TYPE_ALLOCATE_RESPONSE_SUCCESS     (0x0100 | TURN_MSG_METHOD_ALLOCATE)
#define TURN_MSG_TYPE_ALLOCATE_RESPONSE_ERROR       (0x0110 | TURN_MSG_METHOD_ALLOCATE)

// RFC 5766 (TURN) 13.  New STUN Methods refresh request/response
#define TURN_MSG_METHOD_REFRESH                     0x0004
#define TURN_MSG_TYPE_REFRESH_REQUEST               (TURN_MSG_METHOD_REFRESH)
#define TURN_MSG_TYPE_REFRESH_RESPONSE_SUCCESS      (0x0100 | TURN_MSG_METHOD_REFRESH)
#define TURN_MSG_TYPE_REFRESH_RESPONSE_ERROR        (0x0110 | TURN_MSG_METHOD_REFRESH)

// RFC 5766 (TURN) 13.  New STUN Methods Send indication
#define TURN_MSG_METHOD_SEND                        0x0006
#define TURN_MSG_TYPE_SEND_REQUEST                  (0x0010 | TURN_MSG_METHOD_SEND)
#define TURN_MSG_TYPE_SEND_RESPONSE_SUCCESS         (0x0110 | TURN_MSG_METHOD_SEND)
#define TURN_MSG_TYPE_SEND_RESPONSE_ERROR           (0x0110 | TURN_MSG_METHOD_SEND)

// RFC 5766 (TURN) 13.  New STUN Methods data indication
#define TURN_MSG_METHOD_DATA                        0x0007
#define TURN_MSG_TYPE_DATA_REQUEST                  (0x0010 | TURN_MSG_METHOD_DATA)
#define TURN_MSG_TYPE_DATA_RESPONSE_SUCCESS         (0x0110 | TURN_MSG_METHOD_DATA)
#define TURN_MSG_TYPE_DATA_RESPONSE_ERROR           (0x0110 | TURN_MSG_METHOD_DATA)

// RFC 5766 (TURN) 13.  New STUN Methods CreatePermission request/response
#define TURN_MSG_METHOD_CREATEPERM                  0x0008
#define TURN_MSG_TYPE_CREATEPERM_REQUEST            (TURN_MSG_METHOD_CREATEPERM)
#define TURN_MSG_TYPE_CREATEPERM_RESPONSE_SUCCESS   (0x0100 | TURN_MSG_METHOD_CREATEPERM)
#define TURN_MSG_TYPE_CREATEPERM_RESPONSE_ERROR     (0x0110 | TURN_MSG_METHOD_CREATEPERM)

// RFC 5766 (TURN) 13.  New STUN Methods ChannelBind request/response
#define TURN_MSG_METHOD_CHANNELBIND                 0x0009
#define TURN_MSG_TYPE_CHANNELBIND_REQUEST           (TURN_MSG_METHOD_CHANNELBIND)
#define TURN_MSG_TYPE_CHANNELBIND_RESPONSE_SUCCESS  (0x0100 | TURN_MSG_METHOD_CHANNELBIND)
#define TURN_MSG_TYPE_CHANNELBIND_RESPONSE_ERROR    (0x0110 | TURN_MSG_METHOD_CHANNELBIND)

#define STUN_ID_FMT_STR                     "%08x%08x%08x"
#define STUN_ID_FMT_ARGS_FROMBUF(id,idx)       htonl(*((uint32_t *) &(id)[0+(idx)])), \
                                           htonl(*((uint32_t *) &(id)[4+(idx)])), \
                                           htonl(*((uint32_t *) &(id)[8+(idx)]))
#define STUN_ID_FMT_ARGS(id)           STUN_ID_FMT_ARGS_FROMBUF(id, 0)
#define STUN_ID_FROMBUF_FMT_ARGS(id)        STUN_ID_FMT_ARGS_FROMBUF(id, 8)

typedef struct STUN_MSG {
  uint16_t            type;
  uint16_t            length;
  unsigned char       id[STUN_ID_LEN];

  // hard coded fixed size for non dyn-alloc
  unsigned int        numAttribs;
  STUN_ATTRIB_T       attribs[STUN_MAX_ATTRIBS];
} STUN_MSG_T;

typedef struct STUN_MESSAGE_INTEGRITY_PARAM {
  const char *user;
  const char *pass;
  const char *realm;
} STUN_MESSAGE_INTEGRITY_PARAM_T;

int stun_create_id(STUN_MSG_T *pMsg);
int stun_parse(STUN_MSG_T *pMsg, const unsigned char *pData, unsigned int len);
int stun_serialize(const STUN_MSG_T *pMsg, unsigned char *buf, unsigned int bufLen,
                   const STUN_MESSAGE_INTEGRITY_PARAM_T *phmacParam);
char *stun_log_error(char *buf, unsigned int sz, const STUN_MSG_T *pMsg);
void stun_dump(FILE *fp, const STUN_MSG_T *pMsg);
void stun_dump_attrib(FILE *fp, const STUN_ATTRIB_T *pAttr);
const STUN_ATTRIB_T *stun_findattrib(const STUN_MSG_T *pMsg, int attribType);
int stun_ispacket(const unsigned char *data, unsigned int len);
int stun_validate_hmac(const unsigned char *data, unsigned int len, 
                  const STUN_MESSAGE_INTEGRITY_PARAM_T *phmacParam);
int stun_validate_crc(const unsigned char *data, unsigned int msgLen);
const char *stun_type2str(uint16_t type);


#endif // __FMT_STUN_H__
