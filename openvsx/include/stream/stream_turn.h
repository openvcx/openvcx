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

#ifndef __STREAM_TURN_H__
#define __STREAM_TURN_H__

#include "stream/stream_stun.h"
#include "util/pktqueue.h"

#define DEFAULT_PORT_TURN            3478

#define TURN_OP_REQ_ALLOCATION          0x000001
#define TURN_OP_REQ_CREATEPERM          0x000002
#define TURN_OP_REQ_REFRESH             0x000004
#define TURN_OP_REQ_CHANNEL             0x000008
#define TURN_OP_REQ_MASK                0x00000f
#define TURN_OP_REQ_OK                  0x000010
#define TURN_OP_REQ_TIMEOUT             0x000020
#define TURN_OP_RESP_RECV_WAIT          0x000100
#define TURN_OP_RESP_RECV_OK            0x000200
#define TURN_OP_RESP_RECV_ERROR         0x000400
#define TURN_OP_ERROR                   0x001000

typedef struct TURN_RELAY_SETUP_RESULT {
  //pthread_mutex_t                    mtx;
  struct sockaddr_storage            saListener;
  int                                active;
  int                                state;
  int                                is_rtcp;
  int                                isaud;
  SDP_DESCR_T                       *pSdp;
} TURN_RELAY_SETUP_RESULT_T;

typedef struct TURN_CTXT {
  int                                active;
  int                                is_init;
  TURN_POLICY_T                      turnPolicy; 
  int                                do_retry_alloc;
  NETIO_SOCK_T                      *pnetsock;
  TURN_RELAY_SETUP_RESULT_T          result;
  char                               sdpWritePath[VSX_MAX_PATH_LEN];
  int                                relay_active;
  int                                have_permission;
  int                                have_channelbinding;
  pthread_mutex_t                    mtx;
  int                                state;
  int                                refreshState;
  int                                retryCnt;
  int                                reqArgLifetime;  // lifetime argument for refresh
  int                                reqArgChannelId; // channel argumetn for channel binding
  struct sockaddr_storage            saPeerRelay;     // peer TURN allocation for local createpermission request
  struct sockaddr_storage            saTurnSrv;
  STUN_MESSAGE_INTEGRITY_STORE_T     store;
  STUN_MESSAGE_INTEGRITY_PARAM_T     integrity;
  STUN_ATTRIB_T                      lastError;       // From last server response
  STUN_ATTRIB_T                      lastNonce;       // From last server nonce
  STUN_ATTRIB_T                      lastLifetime;    // From Allocate response
  STUN_ATTRIB_T                      relayedAddress;  // From Allocate response
  STUN_ATTRIB_T                      mappedAddress;   // From Allocate response
  STUN_ATTRIB_T                      lastData;        // last received data indication
  TIME_VAL                           tmRefreshResp;
  TIME_VAL                           tmBindChannelResp;

  unsigned char                      idLastReq[STUN_ID_LEN];
  unsigned char                      bufLastReq[STUN_MAX_SZ];
  unsigned int                       szLastReq;
  TIME_VAL                           tmLastReq;
  TIME_VAL                           tmFirstReq;

  struct TURN_THREAD_CTXT           *pThreadCtxt;
  struct TURN_CTXT                  *pnext;
} TURN_CTXT_T;

typedef struct TURN_THREAD_CTXT {
  int                                flag;
  PKTQUEUE_COND_T                    notify;
  pthread_mutex_t                    mtx;
  TURN_CTXT_T                       *pTurnHead;

  int                                cntOnResult;
  int                                doneOnResult;
  int                                totOnResult;
  TURN_RELAY_SETUP_RESULT_T         *pOnResultHead;

} TURN_THREAD_CTXT_T;


int turn_init_from_cfg(TURN_CTXT_T *pTurn, const TURN_CFG_T *pTurnCfg);
int turn_ischanneldata(const unsigned char *data, unsigned int len);
int turn_can_send_data(TURN_SOCK_T *turnSock);
int turn_thread_start(TURN_THREAD_CTXT_T *pThreadCtxt);
int turn_thread_stop(TURN_THREAD_CTXT_T *pThreadCtxt);
int turn_relay_setup(TURN_THREAD_CTXT_T *pThreadCtxt, TURN_CTXT_T *pTurn, NETIO_SOCK_T *pnetsock, 
                     TURN_RELAY_SETUP_RESULT_T *pOnResult);
int turn_close_relay(TURN_CTXT_T *pTurn, int do_wait);
int turn_create_datamsg(const unsigned char *pData, unsigned int szdata,
                        const TURN_SOCK_T *pTurnSock,
                        unsigned char *pout, unsigned int lenout);

int turn_onrcv_pkt(TURN_CTXT_T *pTurn, STUN_CTXT_T *pStun,
                   int is_turn_channeldata, int *pis_turn, int *pis_turn_indication,
                   unsigned char **ppData, int *plen,
                   const struct sockaddr *psaSrc, const struct sockaddr *psaDst, NETIO_SOCK_T *pnetsock);

int turn_onrcv_response(TURN_CTXT_T *pTurn,
                        const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                        const struct sockaddr *psaSrc, const struct sockaddr *psaLocal);

int turn_onrcv_indication_data(TURN_CTXT_T *pTurn,
                               const STUN_MESSAGE_INTEGRITY_PARAM_T *pIntegrity, const STUN_MSG_T *pMsg,
                               const struct sockaddr *psaSrc, const struct sockaddr *psaLocal);
const char *turn_policy2str(TURN_POLICY_T policy);



#endif // __STREAM_TURN_H__
