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


#ifndef __CAPTURE_RTSP_H__
#define __CAPTURE_RTSP_H__

#include "capture.h"

struct STREAMER_CFG;

/**
 * Connect to an RTSP server and SETUP listening ports
 */
int capture_rtsp_setup(CAPTURE_DESCR_COMMON_T *pCommon, const struct STREAMER_CFG *pStreamerCfg);
/**
 * PLAY an RTSP sessio which has been previously completed with SETUP
 */
int capture_rtsp_play(RTSP_CLIENT_SESSION_T *pSession);
int capture_rtsp_close(RTSP_CLIENT_SESSION_T *pSession);

struct RTSP_REQ_CTXT;

int rtsp_run_interleavedclient(RTSP_CLIENT_SESSION_T *pSession, CAP_ASYNC_DESCR_T *pCfg, 
                               struct RTSP_REQ_CTXT *pReqCtxt);
int capture_rtsp_onpkt_interleaved(RTSP_CLIENT_SESSION_T *pSession, CAP_ASYNC_DESCR_T *pCfg,
                                   unsigned char *buf, unsigned int sz);

/**
 * Start an RTSP server instance thread in capture mode
 */
int capture_rtsp_server_start(CAP_ASYNC_DESCR_T *pCfg, int wait_for_setup);

int rtsp_alloc_listener_ports(RTSP_SESSION_T *pRtspSession, int staticLocalPort, 
                              int rtpmux, int rtcpmux, int isvid, int isaud);
int rtsp_setup_transport_str(RTSP_SESSION_TYPE_T sessionType, RTSP_CTXT_MODE_T ctxtmode,
                             RTSP_TRANSPORT_SECURITY_TYPE_T rtspsecuritytype, 
                             const RTSP_SESSION_PROG_T *pProg, const struct sockaddr *paddr,
                             char *buf, unsigned int szbuf);
int capture_rtsp_onsetupdone(RTSP_CLIENT_SESSION_T *pSession, CAPTURE_DESCR_COMMON_T *pCommon);

int rtsp_read_sdp(HTTP_PARSE_CTXT_T *pHdrCtxt, const KEYVAL_PAIR_T *hdrPairs, const char *strMethod,
                  SDP_DESCR_T *pSdp);

int stream_rtsp_announce_start(struct STREAMER_CFG *pCfg, int wait_for_setup);
int stream_rtsp_close(struct STREAMER_CFG *pCfg);

#endif // __CAPTURE_RTSP_H__

