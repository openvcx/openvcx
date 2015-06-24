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

#ifndef __STREAMER_RTP_H__
#define __STREAMER_RTP_H__

#include "util/fileutil.h"
#include "util/auth.h"
#include "formats/filetype.h"
#include "formats/mp2ts.h"
#include "capture/capture.h"
#include "stream/stream_rtp.h"
#include "stream/stream_rtsp.h"
#include "stream/stream_av.h"
#include "stream/stream_net_av.h"
#include "stream/stream_pktz_frbuf.h"
#include "stream/stream_piplayout.h"
#include "server/srvlimits.h"
#include "server/srvauth.h"


#define FRAMESZ_GROW_MULTIPLIER                       2.0
#define FRAMESZ_VIDQUEUE_NUMFRAMES_DFLT               56
#define FRAMESZ_AUDQUEUE_NUMFRAMES_DFLT               48
#define FRAMESZ_VIDQUEUE_SZFRAME_DFLT                 ((IXCODE_SZFRAME_MAX) /  \
                                                        FRAMESZ_GROW_MULTIPLIER) 
#define FRAMESZ_AUDQUEUE_SZFRAME_DFLT                 0x6000
#define FRAMESZ_VIDQUEUE_PREBUF_BYTES                 128

//TODO: setup configurableraw framebuffer sizes
//#define FRAMESZ_VIDRAWQUEUE_SZFRAME_DFLT              

typedef enum STREAM_PROTO {
  STREAM_PROTO_INVALID = 0,
  STREAM_PROTO_AUTO    = 1,
  STREAM_PROTO_RTP     = 2,
  STREAM_PROTO_MP2TS   = 4
} STREAM_PROTO_T;


typedef struct STREAMER_CFG_MP2TS {
  int                           useMp2tsContinuity;
  int                           lastPatContIdx;
  int                           lastPmtContIdx;
  int                           firstProgId;
  int                           replayUseLocalTiming;
  unsigned int                  maxPcrIntervalMs;
  unsigned int                  maxPatIntervalMs;
  int                           xmitflushpkts;
} STREAMER_CFG_MP2TS_T;

typedef struct STREAMER_CFG_RTP {
  uint32_t                      ssrcs[2];
  int                           payloadTypesMin1[2];
  int                           clockHzs[2];
  RTP_TIMESTAMP_INIT_T          timeStamps[2];
  uint16_t                      sequence_nums[2];
  uint16_t                      sequences_at_end[2]; 
  uint16_t                      maxPayloadSz;
  uint16_t                      audioAggregatePktsMs;
  int                           rtpPktzMode;
  int                           rtp_useCaptureSrcPort;   // re-use socket(s) from capture for output to preserve src port
  SDP_XMIT_TYPE_T               xmitType;
  uint32_t                      timestamp; // TODO: implement
  DTLS_TIMEOUT_CFG_T            dtlsTimeouts;
  int                           do_setup_turn;
} STREAMER_CFG_RTP_T;

typedef enum STREAMER_SHARED_STATE {
  STREAMER_SHARED_STATE_ERROR           = -1,
  STREAMER_SHARED_STATE_UNKNOWN         = 0,
  STREAMER_SHARED_STATE_PENDING_CREATE  = 1,
  STREAMER_SHARED_STATE_SOCKETS_CREATED = 2
} STREAMER_SHARED_STATE_T;

struct RTCP_NOTIFY_CTXT;

typedef struct STREAMER_SHARED_CTXT {
  int                           active;
  STREAM_SHARED_CTXT_T          av[2];
  struct RTCP_NOTIFY_CTXT      *pRtcpNotifyCtxt;  // cfgrtp.rtp_useCaptureSrcPort RTCP notify context from 
                                                  //stream_rtp to capture_socket
  STREAMER_SHARED_STATE_T       state;
  pthread_mutex_t               mtxRtcpHdlr;
  TURN_THREAD_CTXT_T            turnThreadCtxt;
} STREAMER_SHARED_CTXT_T;

typedef enum STREAMER_AUTH_IDX {
  STREAMER_AUTH_IDX_RTSP          = 0,
  STREAMER_AUTH_IDX_FLV           = 1,
  STREAMER_AUTH_IDX_MKV           = 2,
  STREAMER_AUTH_IDX_TSLIVE        = 3,
  STREAMER_AUTH_IDX_HTTPLIVE      = 4,
  STREAMER_AUTH_IDX_MOOFLIVE      = 5,
  STREAMER_AUTH_IDX_PIP           = 6,
  STREAMER_AUTH_IDX_STATUS        = 7,
  STREAMER_AUTH_IDX_CONFIG        = 8,
  STREAMER_AUTH_IDX_RTSPANNOUNCE  = 9,
  STREAMER_AUTH_IDX_ROOT          = 10,
  STREAMER_AUTH_IDX_MAX           = 11 
} STREAMER_IDX_AUTH_T;

typedef struct STREAMER_AUTH_LIST {
  AUTH_CREDENTIALS_STORE_T    stores[SRV_LISTENER_MAX];
  struct STREAMER_AUTH_LIST  *pnext;
} STREAMER_AUTH_LIST_T;

typedef struct STREAMER_CFG_STREAMDATA {
  int                           includeSeqHdrs;
  //TODO: move user configurable stream options, includeAnnexB, etc... into here
} STREAMER_CFG_STREAMDATA_T;


#define VID_TN_CNT_NOT_SET      0xffff

typedef struct STREAMER_STATUS {
  char                          pathFile[VSX_MAX_PATH_LEN];
  double                        durationMs;
  double                        locationMs;
  FILE_STREAM_T                *pFileStream;
  STREAM_XMIT_BW_DESCR_T        xmitBwDescr;
  VIDEO_DESCRIPTION_GENERIC_T   vidProps[IXCODE_VIDEO_OUT_MAX];
  int                           vidTnCnt;
  int                           ignoreencdelay_av;
  int                           haveavoffsets0[IXCODE_VIDEO_OUT_MAX];
  AV_OFFSET_T                   avs[IXCODE_VIDEO_OUT_MAX];
  unsigned int                  msDelayTx;
  float                         favoffsetrtcp; 
  float                         faoffset_m2tpktz;
  float                         faoffset_mkvpktz;
  int                           useStatus; 
} STREAMER_STATUS_T;

#define STREAMER_PAIR_SZ         2

typedef struct STREAMER_DEST_CFG {
  CAPTURE_FILTER_TRANSPORT_T      outTransType;
  SRTP_CTXT_T                     srtps[STREAMER_PAIR_SZ];
  DTLS_CFG_T                      dtlsCfg;
  STUN_REQUESTOR_CFG_STORE_T      stunCfgs[STREAMER_PAIR_SZ];
  TURN_CFG_T                      turnCfgs[STREAMER_PAIR_SZ];
  char                            dstHost[256];
  char                            dstUri[256];
  uint16_t                        ports[SOCKET_LIST_MAX];
  uint16_t                        portsRtcp[SOCKET_LIST_MAX];
  int                             numPorts;
} STREAMER_DEST_CFG_T;

typedef struct STREAMER_RAWOUT_CFG {
  PKTQUEUE_T                   *pVidFrBuf; 
  PKTQUEUE_T                   *pAudFrBuf; 
} STREAMER_RAWOUT_CFG_T;

enum JUMP_OFFSET_TYPE {
  JUMP_OFFSET_TYPE_NONE = 0,
  JUMP_OFFSET_TYPE_SEC = 1,
  JUMP_OFFSET_TYPE_BYTE = 2
};

typedef struct STREAMER_JUMP {
  enum JUMP_OFFSET_TYPE           type;
  union {
    float                         jumpOffsetSec;
    float                         jumpPercent;
  } u;
} STREAMER_JUMP_T;

typedef struct STREAMER_PIP {

  struct STREAMER_CFG          *pCfgPips[MAX_PIPS];   // Can be used from main overlay to refer to any PIP instance
  struct STREAMER_CFG          *pCfgOverlay; // Main overlay streamer instance
  const MIXER_CFG_T            *pMixerCfg;
  pthread_mutex_t               mtx;

  //TODO: make PKTQUEUE_COND_T
  pthread_mutex_t               mtxCond;     // 
  pthread_cond_t                cond;        // conditional used to wait from active pip thread such as when streaming single PIP image
  enum PIP_LAYOUT_TYPE          layoutType;
  int                           pipMax;
  char                          inputPathOrig[VSX_MAX_PATH_LEN]; // original PIP input parameter, such as filepath or SDP (not remapped to SDP capture string)
  struct timeval                tvLastActiveSpkr;

  int                           numPipAddedTot;
  TIME_VAL                      tvLastPipAction;
  int                           maxUniquePipLayouts;
} STREAMER_PIP_T;

typedef struct STREAMER_CFG {
  STREAM_PROTO_T                proto;
  unsigned int                  numDests;
  const unsigned int           *pmaxRtp;                         
  int                           loop;
  int                           loop_pl;
  float                         fStart;
  float                         fDurationMax;
  PKTQUEUE_T                   *pSleepQs[2];
  int                           curPlRecurseLevel; 
  int                           maxPlRecurseLevel;
  int                           mp2tfile_extractpes; // when replaying m2t file
                                                     // extract all elem streams 
  int                           novid;
  int                           noaud;
  VSX_STREAMFLAGS_T             streamflags;        // TODO: deprecate
  int                           delayed_output;
  VSX_STREAM_READER_TYPE_T      avReaderType;
  unsigned int                  audsamplesbufdurationms;
  STREAM_XMIT_ACTION_T          action;
  STREAM_SDP_ACTION_T           sdpActions[IXCODE_VIDEO_OUT_MAX];
  STREAM_XMIT_CB_T              cbs[IXCODE_VIDEO_OUT_MAX];
  IXCODE_CTXT_T                 xcode;
  STREAM_XCODE_CTXT_T          *pXcodeCtxt;
  SDP_DESCR_T                   sdpsx[2][IXCODE_VIDEO_OUT_MAX];  // sdp[0] is used for m2t output
                                                                // sdp[1] is used for native packetization
  SDP_DESCR_T                   sdpInput; // SDP used to initialize any input capture
//TODO: create config to dump sdpInput ... and after TURN completion
  STREAM_RTP_MULTI_T rtpMultisMp2ts[IXCODE_VIDEO_OUT_MAX];
  STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2]; // 0 - video, 1 - audio

                                                  // Everything in STREAMER_CFG_T::useStatus before
                                                  // this will be re-init for each new command
                                                  // Everything afterwards will be preserved
  STREAMER_STATUS_T             status;


  //
  // When streaming via the server/srvcmd interface:
  // Anything before this point will be re-init for each new stream 
  // Anything after this point will be preserved
  //

  STREAMER_DEST_CFG_T          *pdestsCfg;    // owned ptr must be freed
  int                          *pxmitDestRc;  // marked after xmit error, owned ptr 

  STREAMER_CFG_RTP_T            cfgrtp;
  STREAMER_CFG_STREAMDATA_T     cfgstream;
  STREAMER_LIVEQ_T              liveQs[IXCODE_VIDEO_OUT_MAX];  // for queing packetized output (m2t) 
  STREAMER_LIVEQ_T              liveQ2s[IXCODE_VIDEO_OUT_MAX]; // for queing packetized output (rtp + xxx) 
                                                               // such as rtsp tcp interleaved
  union {
    STREAMER_CFG_MP2TS_T        cfgmp2ts;
  } u;

  const LIC_INFO_T              lic;
  STREAMER_REPORT_CFG_T        *pReportCfg;
  STREAM_RTSP_SESSIONS_T       *pRtspSessions;   
  STREAM_RTP_MULTI_T           *pRtpMultisRtp[IXCODE_VIDEO_OUT_MAX];
  STREAMER_RAWOUT_CFG_T        *pRawOutCfg;
  struct STREAM_STORAGE        *pStorageBuf;          // points back to STREAM_STORAGE_T
  STREAMER_SHARED_CTXT_T        sharedCtxt;           // utulized when cfgrtp.rtp_useCaptureSrcPort is enabled
  STREAMER_AUTH_LIST_T          creds[STREAMER_AUTH_IDX_MAX];
  STREAM_RTSP_ANNOUNCE_T        rtspannounce;
  unsigned int                  frvidqslots;
  unsigned int                  fraudqslots;
  float                         frtcp_sr_intervalsec;
  int                           rawxmit;
  STREAM_STATS_MONITOR_T       *pMonitor;
  int                           doAbrAuto;
  STREAM_ABR_SELFTHROTTLE_T     abrSelfThrottle;
  int                           frameThin;
  int                           overwritefile;
  int                           verbosity;
  STREAMER_STATE_T              running;       // set to !=0 to end playout
  int                           quitRequested; // only used for playlists
  int                           pauseRequested; // 
  STREAMER_JUMP_T               jump;
  const char                   *xcodecfgfile;  // xcode config file path used for delayed init
  STREAMER_PIP_T                pip;
  VID_ENCODER_FBREQUEST_T       fbReq; // Feedback requests for input capture to process
  pthread_mutex_t               mtxStrmr;

} STREAMER_CFG_T;

int stream_aacAdts(FILE_STREAM_T *pFile,
                   STREAMER_CFG_T *pCfg, 
                   unsigned int frameDeltaHz);

int stream_h264b(FILE_STREAM_T *pFile,
                 STREAMER_CFG_T *pCfg, 
                 double fps);

int stream_mp4_path(const char *path, STREAMER_CFG_T *pCfg);
int stream_mp4(MP4_CONTAINER_T *pMp4, STREAMER_CFG_T *pCfg);
int stream_image(const char *path, STREAMER_CFG_T *pCfg, enum MEDIA_FILE_TYPE mediaType);
int stream_flv(const char *path, STREAMER_CFG_T *pCfg, double fps);
int stream_mkv(const char *path, STREAMER_CFG_T *pCfg, double fps);
int stream_conference(STREAMER_CFG_T *pCfg);

int stream_mp2ts_pes_file(const char *path,
                    STREAMER_CFG_T *pCfg,
                    unsigned int mp2tspktlen);

int stream_mp2ts_pes(const STREAM_DATASRC_T *pDataSrcVid,
                     const STREAM_DATASRC_T *pDataSrcAud,
                     STREAMER_CFG_T *pCfg,
                     STREAM_AV_T *pStreamAv,
                     STREAM_PES_T *pPes,
                     STREAM_NET_AV_T *pAv,
                     unsigned int mp2tspktlen);

int stream_mp2ts_raw_file(const char *path,
                      STREAMER_CFG_T *pCfg, 
                      float tmMinPktIntervalMs,
                      unsigned int mp2tspktlen);

int stream_mp2ts_raw(const STREAM_DATASRC_T *pReplayCfg,
                        STREAMER_CFG_T *pStreamerCfg, 
                        float tmMinPktIntervalMs,
                        unsigned int mp2tspktlen);

enum STREAM_FRAMEQ_TYPE {
  STREAM_FRAMEQ_TYPE_VID_NETFRAMES = 0,
  STREAM_FRAMEQ_TYPE_AUD_NETFRAMES = 1,
  STREAM_FRAMEQ_TYPE_VID_FRAMEBUF  = 2,
  STREAM_FRAMEQ_TYPE_AUD_FRAMEBUF  = 3 
};

#define STREAM_FRAMEQ_TYPE_VID(type) ((type) % 2 == 0 ? 1 : 0)
#define STREAM_FRAMEQ_TYPE_AUD(type) ((type) % 2 == 1 ? 1 : 0)
#define STREAM_FRAMEQ_TYPE_FRAMEBUF(type) ((type) == 2 ? 1 : 0) 
#define STREAM_FRAMEQ_TYPE_RINGBUF(type)  ((type) == 3 ? 1 : 0) 

PKTQUEUE_T *stream_createFrameQ(enum STREAM_FRAMEQ_TYPE type, 
                                unsigned int numSlots,  
                                unsigned int nonDefaultSlotSz);

int streamer_stop(STREAMER_CFG_T *pCfg, int quit, int pause, int delay, int lock);


#endif // __STREAMER_RTP_H__
