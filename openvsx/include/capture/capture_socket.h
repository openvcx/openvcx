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


#ifndef __CAPTURE_SOCKET_H__
#define __CAPTURE_SOCKET_H__

#include "capture_sockettypes.h"

struct STREAMER_CFG;

typedef struct RTCP_NOTIFY_CTXT {
  CAPTURE_STATE_T        *pState;
  SOCKET_LIST_T          *pSockList;
  struct STREAMER_CFG    *pStreamerCfg;
} RTCP_NOTIFY_CTXT_T;

int capture_socketStart(CAP_ASYNC_DESCR_T *pCapCfg);

const CAPTURE_STREAM_T *capture_process_rtcp(RTCP_NOTIFY_CTXT_T *pRtcpNotifyCtxt,
                                             const struct sockaddr_in *pSaSrc,
                                             const struct sockaddr_in *pSaDst,
                                             const RTCP_PKT_HDR_T *pData,
                                             unsigned int len);
int capture_rtcp_srtp_decrypt(RTCP_NOTIFY_CTXT_T *pRtcpNotifyCtxt,
                              const struct sockaddr_in *pSaSrc,      
                              const struct sockaddr_in *pSaDst,      
                              const RTCP_PKT_HDR_T *pHdr,      
                              unsigned int *plen);


#endif // __CAPTURE_SOCKET_H__
