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

#ifndef __STREAM_DTLS_H__
#define __STREAM_DTLS_H__

#include "stream_srtp.h"


#define DTLS_OVERHEAD_SIZE        64
#define DTLS_FINGERPRINT_MAXSZ    64
#define DTLS_RECORD_HEADER_LEN    13
#define DTLS_ISPACKET(p,len)  ((len) >= DTLS_RECORD_HEADER_LEN && *(p) > 19 && *(p) < 64)

#define DTLS_HANDSHAKE_RTP_TIMEOUT_MS            20000
#define DTLS_HANDSHAKE_RTCP_ADDITIONAL_MS         1000
#define DTLS_HANDSHAKE_RTCP_ADDITIONAL_GIVEUP_MS  5000

#define DTLS_SRTP_KEYING_MATERIAL_SIZE (SRTP_KEYLEN_MIN * 2)

typedef enum DTLS_FINGERPRINT_TYPE {
  DTLS_FINGERPRINT_TYPE_UNKNOWN     = 0,
  DTLS_FINGERPRINT_TYPE_MD2         = 1,
  DTLS_FINGERPRINT_TYPE_MD5         = 2,
  DTLS_FINGERPRINT_TYPE_SHA1        = 3,
  DTLS_FINGERPRINT_TYPE_SHA224      = 4,
  DTLS_FINGERPRINT_TYPE_SHA256      = 5,
  DTLS_FINGERPRINT_TYPE_SHA384      = 6,
  DTLS_FINGERPRINT_TYPE_SHA512      = 7
} DTLS_FINGERPRINT_TYPE_T;


typedef struct DTLS_FINGERPRINT {
  DTLS_FINGERPRINT_TYPE_T     type;
  unsigned int                len;
  unsigned char               buf[DTLS_FINGERPRINT_MAXSZ];
} DTLS_FINGERPRINT_T;

typedef struct DTLS_FINGERPRINT_VERIFY {
  int                         verified;
  DTLS_FINGERPRINT_T          fingerprint;
} DTLS_FINGERPRINT_VERIFY_T;

typedef struct DTLS_KEY_UPDATE_STORAGE_T {
  int                             active;
  int                             readyForCb;
  unsigned char                   dtlsKeys[DTLS_SRTP_KEYING_MATERIAL_SIZE];
  unsigned int                    dtlsKeysLen;
  char                            srtpProfileName[128];
  int                             dtls_serverkey;
  int                             is_rtcp;
  void                           *pCbData;
} DTLS_KEY_UPDATE_STORAGE_T;

typedef struct DTLS_CFG {
  int                         active;
  int                         dtls_srtp;
  int                         dtls_handshake_server;
  const char                 *certpath;
  const char                 *privkeypath;
  DTLS_FINGERPRINT_T          fingerprint; 
} DTLS_CFG_T;

typedef struct STREAM_DTLS_CTXT {
  int                         active;
  int                         dtls_srtp;
  const DTLS_CFG_T           *pCfg;
  struct STREAM_DTLS_CTXT    *pDtlsShared;
  void                       *pctx;
  int                         haveRcvdData;
  pthread_mutex_t             mtx; 
} STREAM_DTLS_CTXT_T;

int dtls_get_cert_fingerprint(const char *certPath, DTLS_FINGERPRINT_T *pFingerprint);
char *dtls_fingerprint2str(const DTLS_FINGERPRINT_T *pFingerprint, char *buf, unsigned int len);
int dtls_str2fingerprint(const char *str, DTLS_FINGERPRINT_T *pFingerprint);
int dtls_ctx_init(STREAM_DTLS_CTXT_T *pCtxt, const DTLS_CFG_T *pDtlsCfg, int dtls_srtp);
void dtls_ctx_close(STREAM_DTLS_CTXT_T *pCtxt);

int dtls_netsock_init(const STREAM_DTLS_CTXT_T *pDtlsCtxt, NETIO_SOCK_T *pnetsock, 
                      const DTLS_FINGERPRINT_VERIFY_T *pFingerprintVerify,
                      const DTLS_KEY_UPDATE_CTXT_T *pKeysUpdateCtxt);
int dtls_netsock_setmtu(NETIO_SOCK_T *pnetsock, int mtu);
int dtls_netsock_setclient(NETIO_SOCK_T *pnetsock, int client);
int dtls_netsock_handshake(NETIO_SOCK_T *pnetsock, const struct sockaddr_in *dest_addr);
int dtls_netsock_ondata(NETIO_SOCK_T *pnetsock, const unsigned char *pData, unsigned int len, 
                        const struct sockaddr_in *psaSrc);
int dtls_netsock_write(NETIO_SOCK_T *pnetsock, const unsigned char *pDataIn, unsigned int lenIn, 
                       unsigned char *pDataOut, unsigned int lenOut);

#endif // __STREAM_DTLS_H__

