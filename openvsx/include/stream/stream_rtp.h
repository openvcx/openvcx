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

#ifndef __STREAM_RTP_H__
#define __STREAM_RTP_H__


#include "timers.h"
#include "pktgen.h"
#include "pktqueue.h"
#include "license/license.h"
#include "stream/rtp.h"
#include "stream/stream_srtp.h"
#include "stream/stream_dtls.h"
#include "stream/stream_rtcp.h"
#include "stream_outfmt.h"


#define STREAM_RTCP_SR_INTERVAL_SEC           5.0
#define STREAM_RTCP_RR_INTERVAL_SEC_DEFAULT   0.0
#define STREAM_RTCP_RR_INTERVAL_SEC           5.0

#define CREATE_RTP_SSRC()    (random() % RAND_MAX)

typedef enum STREAMER_STATE {
  STREAMER_STATE_FINISHED_PIP = -3,
  STREAMER_STATE_FINISHED = -2,
  STREAMER_STATE_ERROR = -1,
  STREAMER_STATE_RUNNING = 0,
  STREAMER_STATE_STARTING = 1,
  STREAMER_STATE_ENDING = 2,
  STREAMER_STATE_INTERRUPT = 3,
  STREAMER_STATE_STARTING_THREAD = 4,
} STREAMER_STATE_T;

typedef struct STREAM_INIT_RAW {
  unsigned char             haveRaw;
  unsigned char             haveVlan;
  unsigned char             tos;
  unsigned char             pad;
  unsigned char             mac[ETH_ALEN];
  unsigned short            vlan;
} STREAM_INIT_RAW_T;

typedef struct STREAM_DEST_CB {
  unsigned int                  *pliveQIdx;
} STREAM_DEST_CB_T;

typedef struct STREAM_RTP_SOCKETS {
  NETIO_SOCK_T                    netsock;           // 
  NETIO_SOCK_T                    netsockRtcp;       //
  NETIO_SOCK_T                   *pnetsock;          // if useSockFromCapture is set, points to capture SOCKET_LIST_T::netsockets[i].sock
  NETIO_SOCK_T                   *pnetsockRtcp;      // if useSockFromCapture is set, points to capture SOCKET_LIST_T::sockRtcp[i]
} STREAM_RTP_SOCKETS_T;

typedef struct STREAM_SHARED_CTXT {
  STREAM_RTP_SOCKETS_T          capSocket;   // cfgrtp.rtp_useCaptureSrcPort enabled rtp output sockets (from socket capture)
  uint32_t                     *psenderSSRC;
  SRTP_CTXT_T                  *psrtps[2];   // 0 -> RTP, 1 -> RTCP
  STUN_CTXT_T                  *pstun;
  struct STREAM_DTLS_CTXT      *pdtls;
  DTLS_KEY_UPDATE_STORAGE_T     dtlsKeyUpdateStorages[2]; // 0 -> RTP, 1 -> RTCP
} STREAM_SHARED_CTXT_T;

typedef struct STREAM_DEST_CFG {
  union {
    char                     *pDstHost;
    struct sockaddr_storage   dstAddr;   // holds only destination host ip address and not port
  } u;
  int                         haveDstAddr;  // indicates a valid u.dstAddr entry, otherwise u.pDstHost

  int                         useSockFromCapture; // uses RTP & RTCP sockets created by socket (SDP) capture
  STREAM_SHARED_CTXT_T       *psharedCtxt;
  SRTP_CTXT_T                 srtp;
  DTLS_CFG_T                  dtlsCfg;
  STUN_REQUESTOR_CFG_STORE_T  stunCfg;
  TURN_CFG_T                  turnCfg;
  uint16_t                    localPort;    // local bind listener port (enable for RTCP RR listener)
  uint16_t                    dstPort;      // destination RTP port
  uint16_t                    dstPortRtcp;  // destination RTCP port
  struct timeval             *ptvlastupdate; // (optional) set time of last RTCP RR 
  STREAM_DEST_CB_T            outCb;
  int                         noxmit;        // null output RTP (used for RTSP interleaved mode)
  unsigned int                xcodeOutidx;
  STREAM_STATS_MONITOR_T     *pMonitor;
  int                         doAbrAuto;
  VID_ENCODER_FBREQUEST_T    *pFbReq; 
  struct STREAM_RTP_DEST     *pDestPeer;  // audio-video peer
} STREAM_DEST_CFG_T;

typedef struct STREAM_XMIT_QUEUE {
  int                        doRtcpNack;
  int                        doAsyncXmit;
  unsigned int               nackHistoryMs;
  int                        isCurPktKeyframe;
  uint16_t                   haveLastKeyframeSeqNumStart; 
  uint16_t                   lastKeyframeSeqNumStart; 
  int                        haveRequestedRetransmission;
  BURSTMETER_SAMPLE_SET_T    retransmissionMeter;
  float                      retransmissionKbpsMax;
  pthread_mutex_t            mtx;
  PKTQUEUE_T                 *pQ;
} STREAM_XMIT_QUEUE_T;

typedef struct STREAM_RTP_DEST {
  int                       isactive;
  struct sockaddr_storage   saDsts;
  struct sockaddr_storage   saDstsRtcp;
  STREAM_RTP_SOCKETS_T      xmit;
  int                       sendrtcpsr;
  TIME_VAL                  tmLastRtcpSr;
  STREAM_RTCP_SR_T          rtcp;
  SRTP_CTXT_T               srtps[2]; // 0 -> RTP, 1 -> RTCP
  STUN_CTXT_T               stun;
  TURN_CTXT_T               turns[2]; // 0 -> RTP, 1 -> RTCP
  struct timeval           *ptvlastupdate;
  STREAM_DEST_CB_T          outCb;
  STREAM_STATS_T           *pstreamStats;
  pthread_mutex_t           streamStatsMtx;
  STREAM_STATS_MONITOR_T   *pMonitor;
  STREAM_XMIT_QUEUE_T       asyncQ;
  int                       noxmit;      // no udp send, used for rtp/tcp, rtsp tcp interleaved
  struct STREAM_RTP_MULTI  *pRtpMulti;   // points back to STREAM_RTP_MULTI_T owner
  struct STREAM_RTP_DEST   *pDestPeer;  // audio-video peer
} STREAM_RTP_DEST_T;

#define STREAM_RTCP_PNETIOSOCK(dest)  ((INET_PORT((dest).saDstsRtcp)) == INET_PORT((dest).saDsts) ? \
                 ((dest).xmit.pnetsock ? ((dest).xmit.pnetsock) : (&(dest).xmit.netsock)) : \
                 ((dest).xmit.pnetsockRtcp ? ((dest).xmit.pnetsockRtcp) : (&(dest).xmit.netsockRtcp)))


#define STREAM_RTCP_PSTUNSOCK(dest) PSTUNSOCK(STREAM_RTCP_PNETIOSOCK(dest))

#define STREAM_RTCP_FD(dest)  PNETIOSOCK_FD(STREAM_RTCP_PNETIOSOCK(dest))

#define STREAM_RTCP_FD_NONP(dest)  (INET_PORT((dest).saDstsRtcp) == INET_PORT((dest).saDsts) ? \
                           STUNSOCK_FD((dest).xmit.sock) : STUNSOCK_FD((dest).xmit.sockRtcp))

#define STREAM_RTP_PNETIOSOCK(dest)   ((dest).xmit.pnetsock ? ((dest).xmit.pnetsock) : (&(dest).xmit.netsock))
#define STREAM_RTP_PSTUNSOCK(dest)   PSTUNSOCK(STREAM_RTP_PNETIOSOCK(dest))
#define STREAM_RTP_FD(dest)   PNETIOSOCK_FD(STREAM_RTP_PNETIOSOCK(dest)) 
#define RTCP_PORT_FROM_RTP(port)  ((port) + 1)

#define STREAM_RTP_SRTP_CTXT(pD)  SRTP_CTXT_PTR(&(pD)->srtps[0])
#define STREAM_RTCP_SRTP_CTXT(pD)  SRTP_CTXT_PTR(&(pD)->srtps[1])


typedef struct RTP_TIMESTAMP_INIT_T {
  uint32_t                      timeStamp;
  int                           haveTimeStamp;
} RTP_TIMESTAMP_INIT_T;



typedef struct STREAM_RTP_INIT {
  unsigned int              clockRateHz;
  unsigned int              frameDeltaHz;
  uint8_t                   pt;
  uint8_t                   pad;
  uint16_t                  seqStart;
  RTP_TIMESTAMP_INIT_T      timeStampStart;
  uint32_t                  ssrc;
  unsigned int              maxPayloadSz;
  int                       sendrtcpsr;
  int                       rtcplistener;
  int                       fir_recv_via_rtcp;
  uint8_t                   dscpRtp;
  uint8_t                   dscpRtcp;
  STREAM_INIT_RAW_T         raw;
} STREAM_RTP_INIT_T;

typedef struct STREAM_RTP_MULTI {
  STREAM_RTP_DEST_T           *pdests;
  const unsigned int           maxDests;
  unsigned int                 numDests; // number of STREAM_RTP_DEST with .isactive set
  STREAM_RTP_INIT_T            init;

  PACKETGEN_PKT_UDP_T          packet;
  struct rtphdr               *pRtp;
  unsigned char               *pPayload; 
  unsigned short               payloadLen;
  uint32_t                     tsStartXmit;
  TIME_VAL                     tmStartXmit;
  unsigned int                 pktCnt;
  int                          isinit;
  pthread_mutex_t              mtx;  // protects add/remove dests/sockets
  int                          doRrListener;
  int                          doStunXmit;
  unsigned int                 outidx;  // the IXCODE_VIDEO_OUT index this RTP output configuration corresponds to
  unsigned int                 avidx;
  int                          isaud;

  int                          asyncRtpRunning;
  PKTQUEUE_COND_T              asyncRtpCond;

  struct STREAMER_CFG         *pStreamerCfg;

  struct STREAM_RTP_MULTI     *pheadall;
  STREAM_DTLS_CTXT_T           dtls;
  int                          overlappingPorts; // there are overlapping rtp/rtcp ports among multiple 
                                                 // listeners, so inbound rtcp packets may not come back on
                                                 // the same socket it was sent out on.
  //int                          is_output_turn;
  struct STREAM_RTP_MULTI     *pnext;
} STREAM_RTP_MULTI_T;

#define STREAM_RTP_DTLS_CTXT(prtpmulti) ((prtpmulti)->dtls.pDtlsShared ? (prtpmulti)->dtls.pDtlsShared : &(prtpmulti)->dtls)



typedef int (*CB_STREAM_RTP)(void *);
typedef const unsigned char * (*CB_STREAM_RTP_DATA)(void *);
typedef unsigned int (*CB_STREAM_RTP_DATALEN)(void *);

typedef struct STREAMER_REPORT_CFG {
  VSXLIB_CB_STREAM_REPORT       reportCb;
  VSXLIB_STREAM_REPORT_T        reportCbData;
  void                         *pCbData;
} STREAMER_REPORT_CFG_T;

typedef struct STREAM_XMIT_BW_DESCR {
  unsigned int                  pkts;
  unsigned int                  bytes;
  uint64_t                      pktsTot;
  uint64_t                      bytesTot;
  float                         intervalMs;  // the time range that the current set of 
                                           // values corresponds to
  struct timeval                updateTv;

  TIME_VAL                      tmpLastUpdateTm;
  unsigned int                  tmpPkts;
  unsigned int                  tmpBytes;

  STREAMER_REPORT_CFG_T        *pReportCfg;

} STREAM_XMIT_BW_DESCR_T;


typedef int (*POSTPROC_CB_ONPKT)(void *, const unsigned char *pData, unsigned int len);
typedef int (*SDP_CB_UPDATED) (void *);



typedef struct STREAM_SDP_ACTION {
  SDP_CB_UPDATED               cbSdpUpdated;
  uint8_t                      vidUpdated;
  uint8_t                      updateVid;
  uint8_t                      audUpdated;
  uint8_t                      updateAud;
  uint8_t                      calledCb;
  const char                  *pPathOutFile;

  unsigned int                 outidx; // xcode output idx
  struct STREAMER_CFG         *pCfg; // points back to STREAMER_CFG_T
} STREAM_SDP_ACTION_T;


typedef struct STREAM_XMIT_CB {

  CAPTURE_CB_ONPKT             cbRecord;
  void                        *pCbRecordData; 

  POSTPROC_CB_ONPKT            cbPostProc;
  void                        *pCbPostProcData;

} STREAM_XMIT_CB_T;

typedef struct OUTFMT_QCFG {
  PKTQUEUE_CONFIG_T            cfgRtmp;
  PKTQUEUE_CONFIG_T            cfgFlv;
  PKTQUEUE_CONFIG_T            cfgMkv;
  PKTQUEUE_CONFIG_T            cfgMoof;
  PKTQUEUE_CONFIG_T            cfgTslive;
  PKTQUEUE_CONFIG_T            cfgRtsp;
} OUTFMT_QCFG_T;

typedef struct STREAM_XMIT_ACTION {
  int                          do_stream;           // set if not paused
  int                          do_record_pre;       // record raw data at input 
                                                    // (read from async pktqueue)
  int                          do_record_post;      // record raw data at output in streamer.c

  // The following should be set during startup only
  int                          do_output;           // set if > 0 active xmit destinations
  int                          do_output_rtphdr;    // include udp + rtp header
  int                          do_tslive;           // set if data available via 
                                                    // HTTP request
  int                          do_httplive;         // set to enable HTTP live streaming

  int                          do_rtmplive;
  int                          do_rtmppublish;
  int                          do_rtsplive;
  int                          do_rtspannounce;
  int                          do_flvlive;
  int                          do_flvliverecord;
  int                          do_moofliverecord;
  int                          do_mkvlive;
  int                          do_mkvliverecord;

  OUTFMT_QCFG_T                outfmtQCfg;

  STREAMER_OUTFMT_LIST_T       liveFmts;

} STREAM_XMIT_ACTION_T;

#define STREAMER_DO_OUTPUT(x) ((x).do_output || (x).do_rtmplive || (x).do_rtsplive || \
                               (x).do_mkvlive || (x).do_mkvliverecord || \
                               (x).do_flvlive || (x).do_flvliverecord || (x).do_moofliverecord || \
                               (x).do_tslive || (x).do_httplive)


typedef struct STREAM_XMIT_NODE {
  int                         rawSend;
  int                        *pXmitDestRc;
  void                       *pCbData;
  STREAM_RTP_MULTI_T         *pRtpMulti;
  CB_STREAM_RTP               cbPreparePkt;
  CB_STREAM_RTP               cbCanSend;
  char                        descr[128];
  int                         verbosity;
  float                      *pfrtcp_sr_intervalsec;
  uint16_t                   *prtp_sequences_at_end[2];
  int                         disableActions;
  STREAM_XMIT_ACTION_T       *pXmitAction;
  STREAM_XMIT_CB_T           *pXmitCbs;
  STREAM_XMIT_BW_DESCR_T     *pBwDescr;
  const LIC_INFO_T           *pLic;
  STREAMER_STATE_T           *prunning;           
  PKTQUEUE_T                 *pSleepQ; // optional pktqueue to sleep on instead of calling sleep
  STREAMER_LIVEQ_T           *pLiveQ;  // subscriber list to to 'pull' live broadcast (m2t)
  STREAMER_LIVEQ_T           *pLiveQ2; // rtp + packetized format  
  int                        *poverwritefile;
  const DTLS_TIMEOUT_CFG_T   *pdtlsTimeouts;
  int                         cur_iskeyframe;
  unsigned int                idxProg;
  
  struct STREAM_XMIT_NODE    *pNext;
} STREAM_XMIT_NODE_T;

int stream_rtp_init(STREAM_RTP_MULTI_T *pRtp, const STREAM_RTP_INIT_T *pInit, int is_dtls);
int stream_rtp_close(STREAM_RTP_MULTI_T *pRtp);
STREAM_RTP_DEST_T *stream_rtp_adddest(STREAM_RTP_MULTI_T *pRtp, const STREAM_DEST_CFG_T *pDestCfg);
int stream_rtp_updatedest(STREAM_RTP_DEST_T *pDest, const STREAM_DEST_CFG_T *pDestCfg);
int stream_rtp_removedest(STREAM_RTP_MULTI_T *pRtp, const STREAM_RTP_DEST_T *pDest);
int stream_rtp_handlertcp(STREAM_RTP_DEST_T *pRtpDest, const RTCP_PKT_HDR_T *pHdr, unsigned int len,
                          const struct sockaddr *psaSrc, const struct sockaddr *psaDst);
int stream_rtp_preparepkt(STREAM_RTP_MULTI_T *pRtp);
const unsigned char *stream_rtp_data(STREAM_RTP_MULTI_T *pRtp);
unsigned int stream_rtp_datalen(STREAM_RTP_MULTI_T *pRtp);

int stream_rtcp_createsr(STREAM_RTP_MULTI_T *pRtp, unsigned int idxDest,
                         unsigned char *pBuf, unsigned int lenbuf);
int stream_process_rtcp(STREAM_RTP_MULTI_T *pRtpHeadall, const struct sockaddr *psaSrc, 
                        const struct sockaddr *psaDst, const RTCP_PKT_HDR_T *pHdr, unsigned int len);
int stream_dtls_netsock_key_update(void *pCbData, NETIO_SOCK_T *pnetsock, const char *srtpProfileName,
                                   int dtls_serverkey, int is_rtcp, const unsigned char *pKeys, unsigned int lenKeys);







#endif // __STREAM_RTP_H__
