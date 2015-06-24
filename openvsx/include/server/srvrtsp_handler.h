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


#ifndef __SRV_RTSP_HANDLER_H__
#define __SRV_RTSP_HANDLER_H__

#include "formats/rtsp_parse.h"
#include "server/srvhttp.h"
#include "stream/stream_rtsptypes.h"

struct CAP_ASYNC_DESCR;

typedef struct RTSP_REQ_CTXT {
  SOCKET_DESCR_T          *pSd;
  struct SRV_LISTENER_CFG *pListenCfg;
  BYTE_STREAM_T            bs;
  struct STREAMER_CFG     *pStreamerCfg;
  RTSP_SESSION_T          *pSession;
  pthread_mutex_t          mtx;

  struct CAP_ASYNC_DESCR  *pCap; // If set, denotest server capture mode
  RTSP_CLIENT_SESSION_T   *pClientSession; // If set, denotes server capture mode

  SRTP_CTXT_KEY_TYPE_T     srtpKts[STREAMER_PAIR_SZ];

} RTSP_REQ_CTXT_T;

typedef struct RTSP_REQ {
  HTTP_REQ_T                hr;
  HTTP_PARSE_CTXT_T         hdrCtxt;
  //char                      buf[2048];

  int                       cseq;
  char                      sessionId[RTSP_SESSION_ID_MAX];

  RTSP_HTTP_SESSION_T      *pHttpSession;
  char                      httpSessionCookie[RTSP_HTTP_SESSION_COOKIE_MAX];
} RTSP_REQ_T;

int rtsp_authenticate(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_options(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);

//
// Server output mode
//
int rtsp_handle_describe(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_setup(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_play(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_pause(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_teardown(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);

//
// Server capture mode
//
int rtsp_handle_announce(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_setup_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_play_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);
int rtsp_handle_teardown_announced(RTSP_REQ_CTXT_T *pRtsp, const RTSP_REQ_T *pReq);

int rtsp_interleaved_start(RTSP_REQ_CTXT_T *pRtsp);
void rtsp_interleaved_stop(RTSP_REQ_CTXT_T *pRtsp);

int rtsp_wait_for_sdp(STREAMER_CFG_T *pStreamerCfg, unsigned int outidx, int sec);
int rtsp_prepare_sdp(const STREAMER_CFG_T *pStreamerCfg, unsigned int outidx,
                     char *bufsdp, unsigned int *pszsdp,
                     char *bufhdrs, unsigned int *pszhdrs,
                     SRTP_CTXT_KEY_TYPE_T srtpKts[STREAMER_PAIR_SZ], SDP_DESCR_T *pSdp);
int rtsp_get_transport_keyvals(const char *strTransport, RTSP_SESSION_PROG_T *pProg, int servermode);
int rtsp_do_play(RTSP_REQ_CTXT_T *pRtsp, char *bufhdrs, unsigned int szbufhdrs);


#endif // __SRV_RTSP_HANDLER_H__
