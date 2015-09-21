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


#ifndef __CAPTURE_SOCKETTYPES_H__
#define __CAPTURE_SOCKETTYPES_H__

#include "capture_nettypes.h"
#include "stream/stream_rtcp.h"
#include "util/pktqueue.h"

#define SOCKET_LIST_MAX         10
#define SOCKET_LIST_PREBUF_SZ   64


typedef struct SOCKET_LIST {
  struct sockaddr_storage   salist[SOCKET_LIST_MAX];
  NETIO_SOCK_T              netsockets[SOCKET_LIST_MAX];
  struct sockaddr_storage   salistRtcp[SOCKET_LIST_MAX];
  struct sockaddr_storage   saRemoteRtcp[SOCKET_LIST_MAX];
  NETIO_SOCK_T              netsocketsRtcp[SOCKET_LIST_MAX];
  size_t                    numSockets;
} SOCKET_LIST_T;


#define CAPTURE_RTCP_NETIOSOCK(psl, i)  (INET_PORT((psl)->salistRtcp[i]) == INET_PORT((psl)->salist[i]) ? \
                                        (psl)->netsockets[i] : (psl)->netsocketsRtcp[i])
#define CAPTURE_RTCP_PNETIOSOCK(psl, i)  (INET_PORT((psl)->salistRtcp[i]) == INET_PORT((psl)->salist[i]) ? \
                                         &(psl)->netsockets[i] : &(psl)->netsocketsRtcp[i])

#define CAPTURE_RTCP_FD(psl, i)  (INET_PORT((psl)->salistRtcp[i]) == INET_PORT((psl)->salist[i]) ? \
                                     NETIOSOCK_FD((psl)->netsockets[i]) : NETIOSOCK_FD((psl)->netsocketsRtcp[i]))
#define CAPTURE_RTCP_PFD(psl, i)  (INET_PORT((psl)->salistRtcp[i]) == INET_PORT((psl)->salist[i]) ? \
                                      &NETIOSOCK_FD((psl)->netsockets[i]) : &NETIOSOCK_FD((psl)->netsocketsRtcp[i]))

typedef struct CAP_ASYNC_DESCR {
  SOCKET_LIST_T                *pSockList;
  CAPTURE_DESCR_COMMON_T       *pcommon;
  CAPTURE_RECORD_DESCR_T        record;
  pthread_mutex_t               mtx;
  int                           running;
  struct STREAMER_CFG          *pStreamerCfg;
  void                         *pUserData;
  PKTQUEUE_T                   *pQExit;
  char                          tid_tag[LOGUTIL_TAG_LENGTH];
} CAP_ASYNC_DESCR_T;


#endif // __CAPTURE_SOCKETTYPES_H__
