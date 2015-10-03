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


#ifndef __CAPTURE_NETTYPES_H__
#define __CAPTURE_NETTYPES_H__

#include "capture_types.h"
#include "stream/stream_abr.h"
#include "stream/stream_monitor.h"
#include "stream/stream_outfmt.h"
#include "stream/stream_rtp.h"
#include "stream/stream_rtsptypes.h"
#include "formats/sdp.h"
#include "formats/http_client.h"
#include "formats/rtmp_pkt.h"
#include "vsxlib.h"


#ifdef WIN32
#define PKTQUEUE_LEN_DEFAULT          600
#else 
#define PKTQUEUE_LEN_DEFAULT          400
#endif // WIN32

#define PKTQUEUE_LEN_MAX              3000
#define FRAMEQUEUE_LEN_MAX            500
#define VIDFRAMEQ_LEN_DEFAULT         64
#define AUDFRAMEQ_LEN_DEFAULT         200
#define AUDFRAMEQRAW_LEN_DEFAULT      150

#define STREAM_CAPTURE_IDLETMT_MS            10000
#define STREAM_CAPTURE_IDLETMT_1STPKT_MS     30000

typedef struct SOCKET_DESCR {
  struct sockaddr_storage   sa;
  NETIO_SOCK_T              netsocket;
} SOCKET_DESCR_T;

typedef enum RTSP_CTXT_MODE {
  RTSP_CTXT_MODE_CLIENT_CAPTURE = 0, 
  RTSP_CTXT_MODE_SERVER_CAPTURE = 1, 
  RTSP_CTXT_MODE_CLIENT_STREAM  = 2, 
  RTSP_CTXT_MODE_SERVER_STREAM  = 3 
} RTSP_CTXT_MODE_T;

typedef struct RTSP_CLIENT_SESSION {
  int16_t                       isplaying;
  int16_t                       issetup;
  RTSP_TRANSPORT_TYPE_T         rtsptranstype;
  STREAMER_STATE_T              runMonitor;
  int                           runInterleaved;
  pthread_mutex_t               mtx;
  char                          localAddrBuf[VSX_MAX_PATH_LEN];
  RTSP_SESSION_T                s;
  SOCKET_DESCR_T                sd;
  int                           cseq;
  SDP_DESCR_T                   sdp;
  HTTPCLI_AUTH_CTXT_T           authCliCtxt;
  RTSP_CTXT_MODE_T              ctxtmode;
  PKTQUEUE_COND_T               cond;    // conditional for server based capture setup completion
} RTSP_CLIENT_SESSION_T;

typedef struct CAPTURE_DEVICE_CFG {
  void                         *pCapVidData;
  void                         *pCapAudData;
} CAPTURE_DEVICE_CFG_T;

enum CAPTURE_FRAME_DROP_POLICY {
  CAPTURE_FRAME_DROP_POLICY_NODROP              = 0,
  CAPTURE_FRAME_DROP_POLICY_DROPWLOSSATSTART    = 1,
  CAPTURE_FRAME_DROP_POLICY_DROPWANYLOSS        = 2,
  CAPTURE_FRAME_DROP_POLICY_MAX_VAL             = 3
};

#define CAPTURE_FRAME_DROP_POLICY_DEFAULT   CAPTURE_FRAME_DROP_POLICY_DROPWLOSSATSTART 

typedef struct CAPTURE_DESCR_COMMON {

  //TODO: should seperate out properties for all filters vs each filter

  // capture input type:  udp://, rtp:// addr:port, rtsp:// , http://, 
  // raw interface name, pcap file, /dev/xxx
  const char                    *localAddrs[CAPTURE_MAX_FILTERS];  
  AUTH_CREDENTIALS_STORE_T       creds[CAPTURE_MAX_FILTERS];
  CAPTURE_DEVICE_CFG_T          *pCapDevCfg;

  //
  // additional param, such as HTTP URI
  //
  const char                    *addrsExt[CAPTURE_MAX_FILTERS]; // usually pointer to localAddrs buffer space
  char                          *addrsExtHost[CAPTURE_MAX_FILTERS]; // should be set to it's own unique buf space
  unsigned int                   idletmt_ms;
  unsigned int                   idletmt_1stpkt_ms;

  unsigned int                   cfgMaxVidRtpGapWaitTmMs;
  unsigned int                   cfgMaxAudRtpGapWaitTmMs;
  unsigned int                   dfltCfgMaxVidRtpGapWaitTmMs;
  unsigned int                   dfltCfgMaxAudRtpGapWaitTmMs;

  int                            verbosity;
  CAPTURE_FILTERS_T              filt;
  int                            promiscuous;
  RTSP_CLIENT_SESSION_T          rtsp; 
  RTSP_TRANSPORT_TYPE_T          rtsptranstype;      // TODO: duplicate w/ streamer
  VSX_STREAMFLAGS_T              streamflags;        // TODO: duplicate w/ streamer
  int                            staticLocalPort;    // TODO: duplicate w/ streamer
  int                            connectretrycntminone;   // http fetch retry control
                                                          // 0 - off, 1 - forever, 
                                                          // 2 - once, 3 twice, etc... 
  float                          frtcp_rr_intervalsec;
  int                            rtcp_reply_from_mcast;
  enum CAPTURE_FRAME_DROP_POLICY rtp_frame_drop_policy;
  int                            caphighprio;
  int                            xmithighprio;
  int                            caprealtime;

  RTMP_CLIENT_CFG_T              rtmpcfg;

  int                            novid;
  int                            noaud;

  DTLS_CFG_T                     dtlsCfg;

} CAPTURE_DESCR_COMMON_T;

typedef struct CAPTURE_LOCAL_DESCR {
  CAPTURE_DESCR_COMMON_T           common;

  //TODO: count of common.numFilters and capActions should always match, 
  // count of capActions must be >= filters
  CAPTURE_PKT_ACTION_DESCR_T     capActions[CAPTURE_DB_MAX_STREAMS_LOCAL];
} CAPTURE_LOCAL_DESCR_T;


typedef struct CAPTURE_RECORD_DESCR {
  const char           *outDir;
  int                   overwriteOut;
} CAPTURE_RECORD_DESCR_T;


#endif // __CAPTURE_NETTYPES_H__
