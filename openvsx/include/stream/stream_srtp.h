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

#ifndef __STREAM_SRTP_H__
#define __STREAM_SRTP_H__

#include "netutil.h"


#define SRTP_KEYLEN_MIN        30 
#define SRTP_KEYLEN_MAX        64

//
// RFC 4568 6.2 Crypto-Suites
//
#define SRTP_CRYPTO_AES_CM_128_HMAC_SHA1_80 "AES_CM_128_HMAC_SHA1_80"
#define SRTP_CRYPTO_AES_CM_128_HMAC_SHA1_32 "AES_CM_128_HMAC_SHA1_32"

//
// RFC 5764 4.1.2 SRTP Protection Profiles
//
#define SRTP_PROFILE_AES128_CM_SHA1_80 "SRTP_AES128_CM_SHA1_80"
#define SRTP_PROFILE_AES128_CM_SHA1_32 "SRTP_AES128_CM_SHA1_32"
#define SRTP_PROFILE_NULL_HMAC_SHA1_80 "SRTP_NULL_HMAC_SHA1_80"
#define SRTP_PROFILE_NULL_HMAC_SHA1_32 "SRTP_NULL_HMAC_SHA1_32"

typedef struct SRTP_KEY {
  unsigned char               key[SRTP_KEYLEN_MAX];
  unsigned int                lenKey;
} SRTP_KEY_T;

typedef struct SRTP_TYPE {
  SRTP_AUTH_TYPE_T            authType;
  SRTP_CONF_TYPE_T            confType;
  SRTP_AUTH_TYPE_T            authTypeRtcp;
  SRTP_CONF_TYPE_T            confTypeRtcp;
} SRTP_TYPE_T;

typedef struct SRTP_CTXT_KEY_TYPE {
  SRTP_KEY_T                  k;
  SRTP_TYPE_T                 type;
} SRTP_CTXT_KEY_TYPE_T;

typedef struct SRTP_CTXT {
  //SRTP_TYPE_T                 type;
  struct SRTP_CTXT           *pSrtpShared;
  int                         is_rtcp;
  SRTP_CTXT_KEY_TYPE_T        kt;

  // Everything below here is set internally
  int                         do_rtcp;
  int                         do_rtcp_responder;
  //SRTP_KEY_T                  k;
  void                       *privData;
} SRTP_CTXT_T;

#define SRTP_CTXT_PTR(psrtp) ((psrtp) && (psrtp)->pSrtpShared ? (psrtp)->pSrtpShared : (psrtp))

int srtp_loadKeys(const char *in,  SRTP_CTXT_T *pCtxtV, SRTP_CTXT_T *pCtxtA);
int srtp_setDtlsKeys(SRTP_CTXT_T *pCtxt, const unsigned char *pDtlsKeys, unsigned int lenKeys, int client, int is_rtcp);
const char *srtp_streamTypeStr(SRTP_AUTH_TYPE_T authType, SRTP_CONF_TYPE_T confType, int safe);
int srtp_streamType(const char *streamTypeStr, SRTP_AUTH_TYPE_T *pAuthType, SRTP_CONF_TYPE_T *pConfType);
int srtp_createKey(SRTP_CTXT_T *pCtxt);
int srtp_encryptPacket(const SRTP_CTXT_T *pCtxt, const unsigned char *pIn, unsigned int lenIn,
                       unsigned char *pOut, unsigned int *pLenOut, int rtcp);
int srtp_initOutputStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int rtcp);
int srtp_closeOutputStream(SRTP_CTXT_T *pCtxt);


enum SENDTO_PKT_TYPE {
  SENDTO_PKT_TYPE_UNKNOWN                = 0,
  SENDTO_PKT_TYPE_RTP                    = 1,
  SENDTO_PKT_TYPE_RTCP                   = 2,
  SENDTO_PKT_TYPE_STUN                   = 3,
  SENDTO_PKT_TYPE_STUN_BYPASSTURN        = 4,
  SENDTO_PKT_TYPE_TURN                   = 5,
  SENDTO_PKT_TYPE_DTLS                   = 6
};

int srtp_dtls_protect(const NETIO_SOCK_T *pnetsock,
                      const unsigned char *pDataIn,
                      unsigned int lengthIn,
                      unsigned char *pDataOut,
                      unsigned int lengthOut,
                      const struct sockaddr *dest_addr,
                      const SRTP_CTXT_T *pSrtp,
                      enum SENDTO_PKT_TYPE pktType);

int srtp_sendto(const NETIO_SOCK_T *pnetsock, 
                void *buffer, 
                size_t length, 
                int flags, 
                const struct sockaddr *dest_addr,
                const SRTP_CTXT_T *pSrtp, 
                enum SENDTO_PKT_TYPE pktType,
                int no_protect);

#define SENDTO_RTP(s, buf, len, flags, dest_addr, pSrtp) \
   srtp_sendto((s), buf, len, flags, (const struct sockaddr *)dest_addr, (pSrtp), SENDTO_PKT_TYPE_RTP, 0)

#define SENDTO_RTCP(s, buf, len, flags, dest_addr, pSrtp) \
   srtp_sendto((s), buf, len, flags, (const struct sockaddr *)dest_addr, (pSrtp), SENDTO_PKT_TYPE_RTCP, 0)

#define SENDTO_STUN(s, buf, len, flags, dest_addr) \
   srtp_sendto((s), buf, len, flags, (const struct sockaddr *)dest_addr, NULL, SENDTO_PKT_TYPE_STUN, 0)

#define SENDTO_STUN_BYPASSTURN(s, buf, len, flags, dest_addr) \
   srtp_sendto((s), buf, len, flags, (const struct sockaddr *) dest_addr, NULL, SENDTO_PKT_TYPE_STUN_BYPASSTURN, 0)

#define SENDTO_TURN(s, buf, len, flags, dest_addr) \
   srtp_sendto((s), buf, len, flags, (const struct sockaddr *) dest_addr, NULL, SENDTO_PKT_TYPE_TURN, 0)

#define SENDTO_DTLS(s, buf, len, flags, dest_addr) \
   srtp_sendto((s), buf, len, flags, (const struct sockaddr *) dest_addr, NULL, SENDTO_PKT_TYPE_DTLS, 0)

int srtp_decryptPacket(const SRTP_CTXT_T *pCtxt, unsigned char *pData, unsigned int *plength, int rtcp);
int srtp_initInputStream(SRTP_CTXT_T *pCtxt, uint32_t ssrc, int rtcp);
int srtp_closeInputStream(SRTP_CTXT_T *pCtxt);

#endif // __STREAM_SRTP_H__

