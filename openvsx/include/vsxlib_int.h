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


#ifndef __VSX_LIB_INT_H__
#define __VSX_LIB_INT_H__

#include "vsxlib.h"
#include "capture/capture.h"
#include "stream/streamer_rtp.h"
#include "formats/m3u.h"
#include "stream/stream_piplayout.h"

#if defined(LITE_VERSION) 

#define VSX_CONNECTIONS_MAX           4
#define VSX_CONNECTIONS_DEFAULT       4

#define VSX_LIVEQ_MAX                 4 
#define VSX_LIVEQ_DEFAULT             4

#define VSX_RTP_MAX                   4
#define VSX_RTP_DEFAULT               4 

#else // (LITE_VERSION) 

#define VSX_CONNECTIONS_MAX           100
#define VSX_CONNECTIONS_DEFAULT        20

#define VSX_LIVEQ_MAX                 100
#define VSX_LIVEQ_DEFAULT             4

#define VSX_RTP_MAX                   100
#define VSX_RTP_DEFAULT               8 

#endif // (LITE_VERSION) 


#define RTSP_LISTEN_PORT                   1554
#define RTSP_LISTEN_PORT_STR              "1554"

#define RTMP_LISTEN_PORT                   1935 
#define RTMP_LISTEN_PORT_STR              "1935"

#define HTTP_LISTEN_PORT                   8080  
#define HTTP_LISTEN_PORT_STR              "8080"

#define SSL_CERT_PATH                     "etc/cacert.pem"
#define SSL_PRIVKEY_PATH                  "etc/privkey.pem"
#define SSL_DTLS_CERT_PATH                "etc/dtlscert.pem" 
#define SSL_DTLS_PRIVKEY_PATH             "etc/dtlskey.pem" 

#define STREAM_IDX_VID              0
#define STREAM_IDX_AUD              1

#define DEFAULT_VSX_LOGPATH              "log/vsx.log"

//
// Startup parameter config values
// This allows 0 and -1 to represent false.  0 is be default value.  -1 is explicit false
// value set by commandline, which can be detected by the config value loader and override the config.
//
#define BOOL_ENABLED_OVERRIDE    2
#define BOOL_ENABLED_DFLT        1
#define BOOL_DISABLED_OVERRIDE   -1
#define BOOL_DISABLED_DFLT       0
#define MAKE_BOOL(x) (x) > 0 ? 1 : 0
#define BOOL_ISDFLT(x) ((x) == BOOL_ENABLED_DFLT || (x) == BOOL_DISABLED_DFLT)
#define BOOL_ISENABLED(x) ((x) > 0 ? 1 : 0)
#define BOOL_ISDISABLED(x) ((x) <= 0 ? 1 : 0)

typedef struct SRV_PARAM {
  int                      isinit;
  SRV_START_CFG_T          startcfg;
  POOL_T                   poolHttp;
  POOL_T                   poolRtmp;
  POOL_T                   poolRtsp;
  struct STREAM_STORAGE   *pStorageBuf;
  MEDIADB_DESCR_T          mediaDb;
  char                     cwd[VSX_MAX_PATH_LEN];
  STREAMER_CFG_T          *pStreamerCfg;
  pthread_mutex_t          mtx;
} SRV_PARAM_T;

typedef struct STREAM_STORAGE {
  STREAM_STATS_MONITOR_T               streamMonitor;
  STREAM_STATS_MONITOR_T               streamMonitors[MAX_PIPS];
  STREAM_XCODE_CTXT_T                  xcodeCtxt;
  STREAM_XCODE_CTXT_T                  xcodeCtxtPips[MAX_PIPS];
  PIP_LAYOUT_MGR_T                     pipLayout;
  STREAMER_CFG_T                       streamerCfg;
  STREAMER_CFG_T                       streamerCfgPips[MAX_PIPS];
  CAPTURE_LOCAL_DESCR_T                capCfg;
  CAPTURE_LOCAL_DESCR_T                capCfgPips[MAX_PIPS];
  SRV_PARAM_T                          srvParam;
  SRV_PARAM_T                          srvParamPips[MAX_PIPS];
  STREAM_RTSP_SESSIONS_T               rtspSessions;
  STREAM_RTSP_SESSIONS_T               rtspSessionsPips[MAX_PIPS];
  HTTPLIVE_DATA_T                      httpLiveDatas[IXCODE_VIDEO_OUT_MAX];
  MPD_CREATE_CTXT_T                    mpdMp2tsCtxt[IXCODE_VIDEO_OUT_MAX];
  FLVSRV_CTXT_T                        flvRecordCtxts[IXCODE_VIDEO_OUT_MAX];
  MKVSRV_CTXT_T                        mkvRecordCtxts[IXCODE_VIDEO_OUT_MAX];
  MOOFSRV_CTXT_T                       moofRecordCtxts[IXCODE_VIDEO_OUT_MAX];
  const struct VSXLIB_STREAM_PARAMS  *pParams;
  char                                 pipxcodeStrbuf[KEYVAL_MAXLEN][MAX_PIPS];
  char                                 sdppath[IXCODE_VIDEO_OUT_MAX][VSX_MAX_PATH_LEN];
} STREAM_STORAGE_T;

typedef struct VSXLIB_PRIVDATA {
  int                           inUse;
  STREAM_STORAGE_T             *pStorageBuf;
  CAPTURE_DEVICE_CFG_T          capDevCfg;
  STREAMER_RAWOUT_CFG_T         rawOutCfg;
  STREAMER_REPORT_CFG_T         reportDataCfg;
} VSXLIB_PRIVDATA_T;


int vsxlib_cond_broadcast(pthread_cond_t *cond, pthread_mutex_t *mtx);
int vsxlib_cond_signal(pthread_cond_t *cond, pthread_mutex_t *mtx);
int vsxlib_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx);
int vsxlib_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mtx, struct timespec *pabstime);
int vsxlib_cond_waitwhile(PKTQUEUE_COND_T *pCond, int *pflag, int value);


int vsxlib_initlog(int verbosity, const char *logfile, const char *homedir,
                    unsigned int logmaxsz, unsigned int logrollmax, const char *tag);
int vsxlib_alloc_destCfg(STREAMER_CFG_T *pStreamerCfg, int force);
void vsxlib_free_destCfg(STREAMER_CFG_T *pStreamerCfg);
SRV_CONF_T *vsxlib_loadconf(VSXLIB_STREAM_PARAMS_T *pParams);
int vsxlib_checklicense(VSXLIB_STREAM_PARAMS_T *pParams, const LIC_INFO_T *pLic, SRV_CONF_T *pConf);
void vsxlib_setsrvconflimits(VSXLIB_STREAM_PARAMS_T *pParams,
                           unsigned int rtmphardlimit, unsigned int rtsphardlimit,
                           unsigned int flvhardlimit,  unsigned int mkvhardlimit, 
                           unsigned int httphardlimit);
int vsxlib_ssl_initserver(const VSXLIB_STREAM_PARAMS_T *pParams, const SRV_LISTENER_CFG_T *pListenCfg);
int vsxlib_ssl_initclient(NETIO_SOCK_T *pnetsock);
//int vsxlib_init_srtp(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, const SRTP_CFG_T *pSrtpCfg);
int vsxlib_init_srtp(SRTP_CTXT_T srtps[2], unsigned int numChannels, const SRTP_CFG_T *pSrtpCfg,
                     const char *dstHost, uint16_t *ports);
int vsxlib_init_dtls(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, const SRTP_CFG_T *pSrtpCfg, 
                      int srtp);
int vsxlib_init_stunoutput(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, const STUN_REQUESTOR_CFG_T *pStunCfg);
int vsxlib_init_turnoutput(STREAMER_DEST_CFG_T *pdestsCfg, unsigned int numChannels, const TURN_CFG_T *pTurnCfg);

void vsxlib_set_sdp_from_capture(STREAMER_CFG_T *pStreamerCfg, const SDP_DESCR_T *pSdp);

int vsxlib_stream_setupcap(const VSXLIB_STREAM_PARAMS_T *pParams, CAPTURE_LOCAL_DESCR_T *pCapCfg, 
                            STREAMER_CFG_T *pStreamerCfg, SDP_DESCR_T *pSdp);
int vsxlib_stream_setupcap_params(const VSXLIB_STREAM_PARAMS_T *pParams, CAPTURE_LOCAL_DESCR_T *pCapCfg);
int vsxlib_stream_setupcap_dtls(const VSXLIB_STREAM_PARAMS_T *pParams, CAPTURE_LOCAL_DESCR_T *pCapCfg);
void vsxlib_stream_setup_rtmpclient(RTMP_CLIENT_CFG_T *pRtmpCfg, const VSXLIB_STREAM_PARAMS_T *pParams);

void vsxlib_setup_rtpout(STREAMER_CFG_T *pStreamerCfg, const VSXLIB_STREAM_PARAMS_T *pParams);

void vsxlib_closeOutFmt(STREAMER_OUTFMT_T *pLiveFmt);
void vsxlib_closeServer(SRV_PARAM_T *pSrv);
void vsxlib_closeRtspInterleaved(STREAMER_CFG_T *pStreamerCfg);
int vsxlib_initRtspInterleaved(STREAMER_CFG_T *pStreamerCfg, unsigned int maxRtspLive, 
                               const VSXLIB_STREAM_PARAMS_T *pParams);
int vsxlib_setupServer(SRV_PARAM_T *pSrv, const VSXLIB_STREAM_PARAMS_T *pParams, int do_httplive_segmentor);

int vsxlib_check_prior_listeners(const SRV_LISTENER_CFG_T *arrCfgs, unsigned int max,
                                 const struct sockaddr *psa);
int vsxlib_check_other_listeners(const SRV_START_CFG_T *pStartCfg,
                                 const SRV_LISTENER_CFG_T *arrCfgThis);
int vsxlib_parse_listener(const char *arrAddr[], unsigned int max, SRV_LISTENER_CFG_T *arrCfgs,
                          enum URL_CAPABILITY urlCap, AUTH_CREDENTIALS_STORE_T *ppAuthStores);
void vsxlib_reset_streamercfg(STREAMER_CFG_T *pStreamerCfg);
int vsxlib_set_output(STREAMER_CFG_T *pStreamerCfg,
                      const char *outputs[IXCODE_VIDEO_OUT_MAX],
                      const char *rtcpPorts[IXCODE_VIDEO_OUT_MAX],
                      const char *transproto,
                      const char *sdpoutpath,
                      const SRTP_CFG_T srtpCfgs[IXCODE_VIDEO_OUT_MAX],
                      const STUN_REQUESTOR_CFG_T *pstunCfg,
                      const TURN_CFG_T *pTurnCfg);

int vsxlib_setupMkvRecord(MKVSRV_CTXT_T *pMkvCtxts, 
                          STREAMER_CFG_T *pStreamerCfg,
                          const VSXLIB_STREAM_PARAMS_T *pParams);
int vsxlib_setupFlvRecord(FLVSRV_CTXT_T *pFlvCtxts, 
                          STREAMER_CFG_T *pStreamerCfg,
                          const VSXLIB_STREAM_PARAMS_T *pParams);
int vsxlib_setupRtmpPublish(STREAMER_CFG_T *pStreamerCfg,
                            const VSXLIB_STREAM_PARAMS_T *pParams);

#if defined(VSX_DUMP_CONTAINER)

int vsxlib_dumpContainer(const char *in, int verbosity,
                         const char *avsyncout, int readmeta);
#endif // VSX_DUMP_CONTAINER


#if defined(VSX_HAVE_VIDANALYZE)

int vsxlib_analyzeH264(const char *in, double fps, int verbosity);

#endif // VSX_HAVE_VIDANALYZE


#if defined(VSX_HAVE_CAPTURE)

int vsxlib_streamLiveCaptureCfg(CAPTURE_LOCAL_DESCR_T *pCapCfg, 
                                STREAMER_CFG_T *pStreamerCfg);
#endif // VSX_HAVE_CAPTURE

#if defined(VSX_HAVE_STREAMER)

int vsxlib_streamFileCfg(const char *filePath, double fps, STREAMER_CFG_T *pStreamerCfg);

int vsxlib_streamPlaylistCfg(PLAYLIST_M3U_T *pl,
                           STREAMER_CFG_T *pStreamerCfg,
                           int loopCurFile,
                           int loopPl,
                           int virtPl);

#endif // VSX_HAVE_STREAMER


#endif // __VSX_APP_INT__
