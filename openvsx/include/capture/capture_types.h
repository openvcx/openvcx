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


#ifndef __CAPTURE_TYPES_H__
#define __CAPTURE_TYPES_H__

#include "unixcompat.h"
#include "pthread_compat.h"
#if !defined(WIN32)
#include <netinet/in.h>
#endif // WIN32

#include "pkttypes.h"
#include "stream/rtp.h"
#include "timers.h"
#include "formats/filetypes.h"
#include "formats/sdp.h"
#include "stream/stream_rtcp.h"
#include "stream/stream_dtls.h"
#include "stream/stream_turn.h"


#define CAP_FILTER_TRANS_DISP_STR      "dev|dtls-srtp|dtls|http|https|rtp|rtmp|rtmps|rtsp|rtsps|srtp|tcp|udp"
#define CAP_FILTER_TYPES_DISP_STR      \
                     "h263|h263+|h264|mpg4|vp8|\n\
                aac|ac3|amr|g711u|g711a|opus|silk\n\
                flv|m2t|mp4|mpd|rtmp|\n\
                raw|rgba|bgra|rgb565|bgr565|rgb|bgr|yuv420p|yuva420p|nv12|nv21\n\
                alaw|ulaw|pcm"



typedef struct PKT_PAYLOAD_DATA {
  uint16_t                        flag;
  uint16_t                        sz;
  // data goes here
} PKT_PAYLOAD_DATA_T;

//#define ADDR_LEN_IPV4               4
//#define ADDR_LEN_IPV6               16




typedef struct IP_ADDR_PAIR {
  union {
    struct pqos_ip_addr_ipv4 {
      struct in_addr                   srcIp;
      struct in_addr                   dstIp;
    } ipv4;
    struct pqos_ip_addr_ipv6 {
      struct in6_addr                    srcIp;
      struct in6_addr                    dstIp;
    } ipv6;
  } ip_un;

} IP_ADDR_PAIR_T;

typedef struct COLLECT_STREAM_KEY {
  uint32_t                        ssrc;
  uint16_t                        srcPort;
  uint16_t                        dstPort;
  IP_ADDR_PAIR_T                  ipAddr;
  uint16_t                        lenIp;
  uint16_t                        lenKey;
} COLLECT_STREAM_KEY_T;

#define pair_srcipv4 ipAddr.ip_un.ipv4.srcIp
#define pair_dstipv4 ipAddr.ip_un.ipv4.dstIp
#define pair_srcipv6 ipAddr.ip_un.ipv6.srcIp
#define pair_dstipv6 ipAddr.ip_un.ipv6.dstIp

#define COLLECT_STREAM_KEY_SZ_NO_IP         8


typedef struct COLLECT_STREAM_HDR {

  COLLECT_STREAM_KEY_T             key;

  // RTP payload type
  unsigned char                    payloadType;

  // IP TOS
  unsigned char                    ipTos;

  // Inferred codec based on payload type and bandwidth
  uint16_t                        extCodec;

  // Start of stream
  struct timeval                   tvStart;

  // User defined data
  //unsigned char                    userData[STREAM_USER_DATA_SZ];

} COLLECT_STREAM_HDR_T;


typedef struct COLLECT_STREAM_PKTPAYLOAD_LEN {
    uint16_t                         len;
    uint16_t                         caplen;
} COLLECT_STREAM_PKTPAYLOAD_LEN_T;

typedef struct COLLECT_STREAM_PKTPAYLOAD {
  union {
    COLLECT_STREAM_PKTPAYLOAD_LEN_T  l;
    uint32_t                         len32;
  } ul;
  unsigned char *pData;
  unsigned char *pRtp;

//#define tv64   un_pld.tp_tv64
#define PKTCAPLEN(payload)   ((payload).ul.l.caplen) 
#define PKTWIRELEN(payload)      ((payload).ul.l.len) 
#define PKTLEN32(payload)    ((payload).ul.len32) 

} COLLECT_STREAM_PKTPAYLOAD_T;

#define PKTDATA_RTP_MASK_MARKER              0x8000
#define PKTDATA_RTP_MASK_SZ                  0x7fff
typedef struct COLLECT_STREAM_PKTDATA_RTP {
  uint16_t                        seq;        // sequence number
  uint16_t                        marksz;     // 1 bit marker, 15 bits  sz of the 
                                              // entire original packet on the wire
  uint32_t                        ts;         // time stamp in times stamp unit  
} COLLECT_STREAM_PKTDATA_RTP_T;

typedef struct COLLECT_STREAM_PKTDATA_TCP {
  uint32_t                        seq;     
  uint8_t                         flags;
  uint8_t                         pad;
  uint16_t                        sz;         // sz of the entire original packet on the wire
} COLLECT_STREAM_PKTDATA_TCP_T;

typedef struct COLLECT_STREAM_PKTDATA {
  union {
    COLLECT_STREAM_PKTDATA_RTP_T  rtp;
    COLLECT_STREAM_PKTDATA_TCP_T  tcp;
  } u;
  struct timeval                  tv;
  COLLECT_STREAM_PKTPAYLOAD_T     payload;    // packet payload
  int                             flags;
} COLLECT_STREAM_PKTDATA_T;

#define PKT_FLAGS_DTMF2833                    0x01


typedef struct COLLECT_STREAM_PKT {
  const struct NETIO_SOCK         *pnetsock;
  COLLECT_STREAM_HDR_T             hdr;
  COLLECT_STREAM_PKTDATA_T         data;
} COLLECT_STREAM_PKT_T;


#define MAX_FILES_CREATED_PER_CAPTURE            4


#define CAPTURE_JTBUF_FLAG_IN_STREAM             1
#define CAPTURE_JTBUF_FLAG_RX                    2

#define RTP_JTBUF_PKT_BUFSZ_ETH                  1460
#define RTP_JTBUF_PKT_BUFSZ_LOCAL                12408    // Multiple of mpeg2-ts 188
                                                          // for efficient pktqueue
                                                          // aligned insertion 

#define RTP_JTBUF_PKT_BUFSZ_DEFAULT RTP_JTBUF_PKT_BUFSZ_ETH 

#define CAPTURE_RTP_JTBUF_SZ_PKTS	             48 
#define CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS      100    // Maximum jitter buffer
#define CAPTURE_RTP_VIDNACK_JTBUF_GAP_TS_MAXWAIT_MS  500   // Maximum jitter buffer
#define CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS      100    // Maximum jitter buffer
                                                            // wait time for loss / out-of-order
#define CAPTURE_DB_MAX_STREAMS_PCAP                  16 
#define CAPTURE_DB_MAX_STREAMS_LOCAL                 2 

typedef struct CAPTURE_JTBUF_PKTDATA_POOL_ENTRY {
  unsigned char           *pBuf;
  struct CAPTURE_JTBUF_PKTDATA_POOL_ENTRY *pnext;
} CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T;

typedef struct CAPTURE_JTBUF_PKTDATA_POOL {
  unsigned int maxEntries;
  int numAllocEntries;
  unsigned int entryBufSz;
  CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *pEntries;
} CAPTURE_JTBUF_PKTDATA_POOL_T;

typedef struct CAPTURE_JTBUF_PKT_NACK {
  struct timeval           tvLastNack;
} CAPTURE_JTBUF_PKT_NACK_T;

typedef struct CAPTURE_JTBUF_PKT {
  int32_t                             rcvFlag;
  COLLECT_STREAM_PKTDATA_T            pktData;
  CAPTURE_JTBUF_PKT_NACK_T            nack;
  CAPTURE_JTBUF_PKTDATA_POOL_ENTRY_T *pPoolBuf;
} CAPTURE_JTBUF_PKT_T;

typedef struct CAPTURE_JTBUF {

  unsigned int idxPkt;        // idx pending packet to be played, which may wait for rx of out-of-order pkts
  unsigned int idxPktStored;  // idx of latest rx packet
  unsigned int sizePkts;      // max number of elements in pPkts
  unsigned int startingSeq;
  unsigned int flushedAll;
  CAPTURE_JTBUF_PKT_T *pPkts;
  CAPTURE_JTBUF_PKTDATA_POOL_T pool;
} CAPTURE_JTBUF_T;

struct CAPTURE_STREAM;

typedef int (*CAPTURE_CB_ONPKT)(void *, const struct COLLECT_STREAM_PKTDATA *pPkt); 
typedef int (*CAPTURE_CB_ONSTREAMADD) (void *, struct CAPTURE_STREAM *, 
                                      const COLLECT_STREAM_HDR_T *, const char *);
typedef int (*CAPTURE_CB_ONSTREAMEND)(struct CAPTURE_STREAM *pStream); 


typedef enum CAPTURE_PKT_ACTION {
  CAPTURE_PKT_ACTION_NONE            = 0x00,
  CAPTURE_PKT_ACTION_QUEUE           = 0x01,
  CAPTURE_PKT_ACTION_EXTRACTPES      = 0x02,
  CAPTURE_PKT_ACTION_QUEUEFRAME      = 0x04,
  CAPTURE_PKT_ACTION_STUNONLY        = 0x08   // Respond to STUN bindings only and do not record 
} CAPTURE_PKT_ACTION_T;

typedef struct CAPTURE_PKT_LAG_CTXT {
  int                            doSyncToRT;
  TIME_VAL                       tvLastQWarn;
  int                            waitingOnIDR;
  TIME_VAL                       tvLastFIRRequest; // redundant w/ FIR RTCP request throttling
} CAPTURE_PKT_LAG_CTXT_T;

typedef struct CAPTURE_PKT_ACTION_DESCR {
  CAPTURE_PKT_ACTION_T           cmd;
  unsigned int                   pktqslots;
  unsigned int                   frvidqslots;
  unsigned int                   fraudqslots;
  PKTQUEUE_T                    *pQueue;
  CAPTURE_PKT_LAG_CTXT_T         lagCtxt;
  BYTE_STREAM_T                  tmpFrameBuf;
} CAPTURE_PKT_ACTION_DESCR_T;

typedef enum CAPTURE_FILTER_PROTOCOL {
  CAPTURE_FILTER_PROTOCOL_UNKNOWN       = MEDIA_FILE_TYPE_UNKNOWN,
  CAPTURE_FILTER_PROTOCOL_RAW           = MEDIA_FILE_TYPE_RAWDUMP,
  CAPTURE_FILTER_PROTOCOL_H264          = MEDIA_FILE_TYPE_H264b,
  CAPTURE_FILTER_PROTOCOL_MPEG4V        = MEDIA_FILE_TYPE_MP4V,
  CAPTURE_FILTER_PROTOCOL_H263          = MEDIA_FILE_TYPE_H263,
  CAPTURE_FILTER_PROTOCOL_H263_PLUS     = MEDIA_FILE_TYPE_H263_PLUS,
  CAPTURE_FILTER_PROTOCOL_VP6           = MEDIA_FILE_TYPE_VP6,
  CAPTURE_FILTER_PROTOCOL_VP6F          = MEDIA_FILE_TYPE_VP6F,
  CAPTURE_FILTER_PROTOCOL_VP8           = MEDIA_FILE_TYPE_VP8,
  CAPTURE_FILTER_PROTOCOL_VID_CONFERENCE = XC_CODEC_TYPE_VID_CONFERENCE,

  CAPTURE_FILTER_PROTOCOL_AAC           = MEDIA_FILE_TYPE_AAC_ADTS,
  CAPTURE_FILTER_PROTOCOL_SILK          = MEDIA_FILE_TYPE_SILK,
  CAPTURE_FILTER_PROTOCOL_OPUS          = MEDIA_FILE_TYPE_OPUS,
  CAPTURE_FILTER_PROTOCOL_G711_MULAW    = XC_CODEC_TYPE_G711_MULAW,
  CAPTURE_FILTER_PROTOCOL_G711_ALAW     = XC_CODEC_TYPE_G711_ALAW,
  CAPTURE_FILTER_PROTOCOL_MPA_L2        = XC_CODEC_TYPE_MPEGA_L2,
  CAPTURE_FILTER_PROTOCOL_MPA_L3        = XC_CODEC_TYPE_MPEGA_L3,
  CAPTURE_FILTER_PROTOCOL_AMR           = MEDIA_FILE_TYPE_AMRNB,
  CAPTURE_FILTER_PROTOCOL_TELEPHONE_EVENT = XC_CODEC_TYPE_TELEPHONE_EVENT,
  CAPTURE_FILTER_PROTOCOL_AUD_CONFERENCE = XC_CODEC_TYPE_AUD_CONFERENCE,

  CAPTURE_FILTER_PROTOCOL_MP2TS         = MEDIA_FILE_TYPE_MP2TS,
  CAPTURE_FILTER_PROTOCOL_MP2TS_BDAV    = MEDIA_FILE_TYPE_MP2TS_BDAV,
  CAPTURE_FILTER_PROTOCOL_RTMP          = MEDIA_FILE_TYPE_RTMPDUMP,
  CAPTURE_FILTER_PROTOCOL_FLV           = MEDIA_FILE_TYPE_FLV,
  CAPTURE_FILTER_PROTOCOL_MKV           = MEDIA_FILE_TYPE_MKV,
  CAPTURE_FILTER_PROTOCOL_MP4           = MEDIA_FILE_TYPE_MP4,
  CAPTURE_FILTER_PROTOCOL_DASH          = MEDIA_FILE_TYPE_DASHMPD,
  CAPTURE_FILTER_PROTOCOL_VID_GENERIC   = MEDIA_FILE_TYPE_VID_GENERIC,
  CAPTURE_FILTER_PROTOCOL_AUD_GENERIC   = MEDIA_FILE_TYPE_AUD_GENERIC,

  CAPTURE_FILTER_PROTOCOL_RAWV_BGRA32   = XC_CODEC_TYPE_RAWV_BGRA32, 
  CAPTURE_FILTER_PROTOCOL_RAWV_RGBA32   = XC_CODEC_TYPE_RAWV_RGBA32, 
  CAPTURE_FILTER_PROTOCOL_RAWV_BGR24    = XC_CODEC_TYPE_RAWV_BGR24, 
  CAPTURE_FILTER_PROTOCOL_RAWV_RGB24    = XC_CODEC_TYPE_RAWV_RGB24, 
  CAPTURE_FILTER_PROTOCOL_RAWV_BGR565   = XC_CODEC_TYPE_RAWV_BGR565, 
  CAPTURE_FILTER_PROTOCOL_RAWV_RGB565   = XC_CODEC_TYPE_RAWV_RGB565, 
  CAPTURE_FILTER_PROTOCOL_RAWV_YUV420P  = XC_CODEC_TYPE_RAWV_YUV420P, 
  CAPTURE_FILTER_PROTOCOL_RAWV_YUVA420P = XC_CODEC_TYPE_RAWV_YUVA420P, 
  CAPTURE_FILTER_PROTOCOL_RAWV_NV12     = XC_CODEC_TYPE_RAWV_NV12, 
  CAPTURE_FILTER_PROTOCOL_RAWV_NV21     = XC_CODEC_TYPE_RAWV_NV21, 

  CAPTURE_FILTER_PROTOCOL_RAWA_PCM16LE  = XC_CODEC_TYPE_RAWA_PCM16LE, 
  CAPTURE_FILTER_PROTOCOL_RAWA_PCMULAW  = XC_CODEC_TYPE_RAWA_PCMULAW, 
  CAPTURE_FILTER_PROTOCOL_RAWA_PCMALAW  = XC_CODEC_TYPE_RAWA_PCMALAW, 

  // Dummy types for file output creation
  CAPTURE_FILTER_PROTOCOL_H262        = MEDIA_FILE_TYPE_H262,
  CAPTURE_FILTER_PROTOCOL_MP4V        = MEDIA_FILE_TYPE_MP4V,
  CAPTURE_FILTER_PROTOCOL_AC3         = MEDIA_FILE_TYPE_AC3 

} CAPTURE_FILTER_PROTOCOL_T;

// non m2t rtp codec specific RFCxxxx packetization method 

#define IS_CAP_FILTER_VID(c) ((c) == MEDIA_FILE_TYPE_MP2TS || codectype_isVid(c))
#define IS_CAP_FILTER_AUD(c) (codectype_isAud(c))

typedef enum CAPTURE_FILTER_TRANSPORT {
  CAPTURE_FILTER_TRANSPORT_UNKNOWN     = 0,
  CAPTURE_FILTER_TRANSPORT_UDPRAW      = 1,
  CAPTURE_FILTER_TRANSPORT_UDPRTP      = 2,
  CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES = 3,
  CAPTURE_FILTER_TRANSPORT_UDPDTLS     = 4,
  CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP = 5,
  CAPTURE_FILTER_TRANSPORT_TCP         = 6,
  CAPTURE_FILTER_TRANSPORT_HTTPGET     = 7,
  CAPTURE_FILTER_TRANSPORT_HTTPSGET    = 8,
  CAPTURE_FILTER_TRANSPORT_RTSP        = 9,
  CAPTURE_FILTER_TRANSPORT_RTSPS       = 10,
  CAPTURE_FILTER_TRANSPORT_RTMP        = 11,
  CAPTURE_FILTER_TRANSPORT_RTMPS       = 12,
  CAPTURE_FILTER_TRANSPORT_DEV         = 13,
  CAPTURE_FILTER_TRANSPORT_HTTPFLV     = 14,
  CAPTURE_FILTER_TRANSPORT_HTTPSFLV    = 15,
  CAPTURE_FILTER_TRANSPORT_HTTPMP4     = 16,
  CAPTURE_FILTER_TRANSPORT_HTTPSMP4    = 17,
  CAPTURE_FILTER_TRANSPORT_HTTPDASH    = 18,
  CAPTURE_FILTER_TRANSPORT_HTTPSDASH   = 19,
} CAPTURE_FILTER_TRANSPORT_T;

#define IS_CAPTURE_FILTER_TRANSPORT_HTTP(t)  ((t) == CAPTURE_FILTER_TRANSPORT_HTTPGET \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSGET \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPFLV \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSFLV \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPMP4 \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSMP4 \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPDASH \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSDASH)

#define IS_CAPTURE_FILTER_TRANSPORT_RTSP(t)  ((t) == CAPTURE_FILTER_TRANSPORT_RTSP \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_RTSPS)

#define IS_CAPTURE_FILTER_TRANSPORT_RTMP(t)  ((t) == CAPTURE_FILTER_TRANSPORT_RTMP \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_RTMPS)

#define IS_CAPTURE_FILTER_TRANSPORT_SSL(t)  ((t) == CAPTURE_FILTER_TRANSPORT_HTTPSGET \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSFLV \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSMP4 \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_HTTPSDASH \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_RTMPS \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_RTSPS)

#define IS_CAPTURE_FILTER_TRANSPORT_RTP(t)  ((t) == CAPTURE_FILTER_TRANSPORT_UDPRTP \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_UDPDTLS \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP)

#define IS_CAPTURE_FILTER_TRANSPORT_DTLS(t)  ((t) == CAPTURE_FILTER_TRANSPORT_UDPDTLS \
                                       || (t) == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP)


//
// BPF style stream packet filter
//
typedef struct CAPTURE_FILTER {

  //
  // filter input parameters used to match a stream
  //
  uint32_t                     ssrcFilter;
  uint16_t                     dstPort;
  uint16_t                     srcPort;
  uint16_t                     port;
  uint16_t                     pad1;
  in_addr_t                    dstIp;
  in_addr_t                    srcIp;
  in_addr_t                    ip;
  uint8_t                      payloadType;
  uint8_t                      pad2;
  uint8_t                      haveRtpPtFilter;
  uint8_t                      haveRtpSsrcFilter;
  CAPTURE_FILTER_TRANSPORT_T   transType;

  //
  // filter storage parameters for stream properties
  //
  CAPTURE_FILTER_PROTOCOL_T    mediaType;
  uint16_t                     dstPortRtcp; // optionally obtained from sdp
  SDP_XMIT_TYPE_T              xmitType;

  union {
    SDP_DESCR_AUD_T            aud;
    SDP_DESCR_VID_T            vid;
  } u_seqhdrs;

  // raw video input
  int                          width;
  int                          height;
  double                       fps;

  //
  // Optional configuration directive, such as reference to async queueing 
  // The stream specific CAPTURE_CBDATA_SP::pCapAction should be set to this
  // within onStreamAdd
  //
  CAPTURE_PKT_ACTION_DESCR_T   *pCapAction;

  SRTP_CTXT_T                  srtps[2]; // 0 -> RTP, 1 -> RTCP
  STREAM_DTLS_CTXT_T           dtls;
  DTLS_FINGERPRINT_VERIFY_T    dtlsFingerprintVerify;
  STUN_CTXT_T                  stun;
  TURN_CTXT_T                  turn;
  TURN_CTXT_T                  turnRtcp;

  int                          enableNack;
  int                          enableRemb;

  SDP_DESCR_AUD_TELEPHONE_EVENT_T telephoneEvt; 

  char                         outputFilePrfx[128];

} CAPTURE_FILTER_T;

#define CAPTURE_RTP_SRTP_CTXT(pF)  SRTP_CTXT_PTR((SRTP_CTXT_T *) &(pF)->srtps[0])
#define CAPTURE_RTCP_SRTP_CTXT(pF)  SRTP_CTXT_PTR((SRTP_CTXT_T *) &(pF)->srtps[1])

//
// Stream specific context 
// A stream is made unique by the content of its COLLECT_STREAM_KEY_T
// contained within COLLECT_STREAM_HDR_T hdr
//
typedef struct CAPTURE_STREAM {
  uint64_t                 numPkts;
  uint64_t                 numBytes;
  uint64_t                 numBytesPayload;
  int                      numSeqIncrOk;
  int                      numSeqIncrInvalid;
  unsigned int             numSeqnoNacked;
  unsigned int             frameDeltaHz;
  struct timeval           lastTsChangeTv;
  unsigned int             ts0;
  int                      tsRolls;
  //uint32_t                 highestRcvdTs;
  uint32_t                 id;
  unsigned int             clockHz;
  unsigned int             maxRtpGapWaitTmMs; 
  int                      fir_send_intervalms;
  int                      appremb_send_intervalms;
  struct CAPTURE_ABR      *pRembAbr;
  // stream start time is stored in hdr.tvStart.tv_sec
  COLLECT_STREAM_HDR_T     hdr;
  CAPTURE_JTBUF_T         *pjtBuf;

  //
  // Will be automatically set to a valid 'filters' entry from CAPTURE_STATE_T
  // prior to onStreamAdd being called
  //
  const CAPTURE_FILTER_T        *pFilter;    
  STREAM_RTCP_RR_T              rtcpRR;
  int                           haveRtcpBye;
  
  //
  // Callback invoked for each packet of this stream
  //
  CAPTURE_CB_ONPKT              cbOnPkt;
  struct CAPTURE_CBDATA_SP     *pCbUserData;  // This should always be CAPTURE_CBDATA_SP_T
  CAPTURE_CB_ONSTREAMEND        cbOnStreamEnd;

  TIME_VAL                      tmLastPkt;
  uint64_t                      ptsLastFrame;
  char                          strSrcDst[MAX_IP_STR_LEN * 2 + 32]; 
} CAPTURE_STREAM_T;

#define CAPTURE_MAX_FILTERS          CAPTURE_DB_MAX_STREAMS_LOCAL 
#define CAPTURE_MAX_FILTERS_PCAP     CAPTURE_MAX_FILTERS 

#define CAPTURE_STREAM_TYPESTR(c,s) ((s)->pjtBuf == &(c)->jtBufVid ? "video " : (s)->pjtBuf == &(c)->jtBufAud ? "audio " : "")

typedef struct CAPTURE_FILTERS {
  unsigned int numFilters;
  CAPTURE_FILTER_T filters[CAPTURE_MAX_FILTERS_PCAP];
} CAPTURE_FILTERS_T;


//
// Main reference point for all packet capture
//
typedef struct CAPTURE_STATE {
  // counters incremented on rcv
  long long numPkts;                  
  long long numRtpPkts;               
  long long numUdprawPkts;
  //unsigned int numFilters;
  CAPTURE_FILTERS_T filt;
  //CAPTURE_FILTER_T filters[CAPTURE_MAX_FILTERS_PCAP];
  unsigned int maxStreams;
  CAPTURE_STREAM_T *pStreams;
  unsigned int cfgMaxVidRtpGapWaitTmMs;
  unsigned int cfgMaxAudRtpGapWaitTmMs;
  CAPTURE_JTBUF_T jtBufVid;
  CAPTURE_JTBUF_T jtBufAud;
  CAPTURE_JTBUF_T *pjtBufVid;
  CAPTURE_JTBUF_T *pjtBufAud;
  CAPTURE_ABR_T   rembAbr;
  pthread_mutex_t mutexStreams;

  //
  // Callback invoked for first packet of any stream
  //
  CAPTURE_CB_ONSTREAMADD     cbOnStreamAdd;
  void *pCbUserData;  // This should always be CAPTURE_CBDATA_T

} CAPTURE_STATE_T;




#endif //__CAPTURE_TYPES_H__
