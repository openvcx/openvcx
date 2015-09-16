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

#ifndef __STREAM_STUN_H__
#define __STREAM_STUN_H__

#include "formats/stun.h"
#include "util/netio.h"

#define STUN_SOCKET_FLAG_RCVD_BINDINGREQ           0x0001
#define STUN_SOCKET_FLAG_RCVD_BINDINGREQ_ERR       0x0002
#define STUN_SOCKET_FLAG_RCVD_BINDINGRESP          0x0004 
#define STUN_SOCKET_FLAG_RCVD_BINDINGRESP_ERR      0x0008
#define STUN_SOCKET_FLAG_SENT_BINDINGREQ           0x0010
#define STUN_SOCKET_FLAG_SENT_BINDINGRESP          0x0020
#define STUN_SOCKET_FLAG_UPDATED_DESTADDR          0x0040

#define STUN_MIN_NEW_PING_INTERVAL_MS              200
#define STUN_MIN_ACTIVE_PING_INTERVAL_MS           5000

typedef struct STUN_MESSAGE_INTEGRITY_STORE {
  char                               ufragbuf[STUN_STRING_MAX];
  char                               pwdbuf[STUN_STRING_MAX];
  char                               realmbuf[STUN_STRING_MAX];
} STUN_MESSAGE_INTEGRITY_STORE_T;

typedef struct STUN_REQUESTOR_CFG_STORE {
  STUN_REQUESTOR_CFG_T              cfg;
  STUN_MESSAGE_INTEGRITY_STORE_T    store;
} STUN_REQUESTOR_CFG_STORE_T;

struct TURN_CTXT;

typedef struct STUN_CTXT {
  STUN_POLICY_T                      policy;
  struct STUN_CTXT                  *pPeer;
  STUN_MESSAGE_INTEGRITY_STORE_T     store;
  STUN_MESSAGE_INTEGRITY_PARAM_T     integrity;
  unsigned int                       minNewPingIntervalMs; 
  unsigned int                       minActivePingIntervalMs; 
  TIME_VAL                           tmLastWarn;

  //
  // Last sent binding request
  //
  unsigned char                      idLastReq[STUN_ID_LEN];
  unsigned char                      bufLastReq[STUN_MAX_SZ];
  unsigned int                       szLastReq;
  TIME_VAL                           tmLastReq;

} STUN_CTXT_T;

typedef struct STUN_BINDING_STATE {
  STUN_CTXT_T                       *pCtxt;
} STUN_BINDING_STATE_T;

int stun_onrcv(STUN_CTXT_T *pCtxt, struct TURN_CTXT *pTurn, const unsigned char *pData, unsigned int len, 
               const struct sockaddr *psaSrc, const struct sockaddr *psaLocal, NETIO_SOCK_T *pnetsock,
               int is_turn);

struct STREAM_RTP_DEST;
struct STREAM_RTP_MULTI;

int stream_stun_start(struct STREAM_RTP_DEST *pDest, int lock);
int stream_stun_stop(struct STREAM_RTP_MULTI *pRtp);
int stream_stun_parseCredentialsPair(const char *in,  char *videoStr, char *audioStr);
const char *stun_policy2str(STUN_POLICY_T policy);


#endif // __STREAM_STUN_H__
