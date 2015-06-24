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


#ifndef __SDP_H__
#define __SDP_H__

#include "codecs/avcc.h"
#include "codecs/mpg4v.h"
#include "stream/stream_dtls.h"
#include "stream/stream_srtp.h"
#include "stream/stream_stun.h"


//SDP rtpmap values
#define SDP_RTPMAP_ENCODINGNAME_H264             "H264"
#define SDP_RTPMAP_ENCODINGNAME_MPEG4V           "MP4V-ES"
#define SDP_RTPMAP_ENCODINGNAME_VP8              "VP8"
#define SDP_RTPMAP_ENCODINGNAME_H263             "H263"
#define SDP_RTPMAP_ENCODINGNAME_H263_PLUS        "H263-1998"
#define SDP_RTPMAP_ENCODINGNAME_AAC              "MPEG4-GENERIC"
#define SDP_RTPMAP_ENCODINGNAME_AMR              "AMR"
#define SDP_RTPMAP_ENCODINGNAME_SILK             "SILK"
#define SDP_RTPMAP_ENCODINGNAME_OPUS             "OPUS"
#define SDP_RTPMAP_ENCODINGNAME_MP2TS            "MP2T"
#define SDP_RTPMAP_ENCODINGNAME_PCMU             "PCMU"
#define SDP_RTPMAP_ENCODINGNAME_PCMA             "PCMA"
#define SDP_RTPMAP_ENCODINGNAME_TELEPHONE_EVENT  "telephone-event"
#define SDP_RTPMAP_ENCODINGNAME_VID_CONFERENCE   "VIDEO-CONFERENCE"
#define SDP_RTPMAP_ENCODINGNAME_AUD_CONFERENCE   "AUDIO-CONFERENCE"

#define SDP_ATTRIB_CONTROL_TRACKID               "trackID"


#define SDP_TRANS_UDP_AVP                        "UDP/AVP" // Non-standard, raw UDP 

#define SDP_TRANS_RTP_AVP                        "RTP/AVP"
#define SDP_TRANS_RTP_AVPF                       "RTP/AVPF"

#define SDP_TRANS_SRTP_AVP                       "RTP/SAVP"
#define SDP_TRANS_SRTP_AVPF                      "RTP/SAVPF"

#define SDP_TRANS_DTLS_AVP                       "UDP/TLS/RTP/SAVP"
#define SDP_TRANS_DTLS_AVPF                      "UDP/TLS/RTP/SAVPF"

#define SDP_TRANS_RTP_AVP_UDP                    "RTP/AVP/UDP"
#define SDP_TRANS_RTP_AVP_TCP                    "RTP/AVP/TCP"
//#define SDP_TRANS_TCP_RTP_AVP                    "TCP/RTP/AVP"

typedef enum SDP_TRANS_TYPE {
  SDP_TRANS_TYPE_UNKNOWN        = 0,
  SDP_TRANS_TYPE_RTP_UDP        = 1,
  SDP_TRANS_TYPE_SRTP_SDES_UDP  = 2,
  SDP_TRANS_TYPE_SRTP_DTLS_UDP  = 3,
  SDP_TRANS_TYPE_DTLS_UDP       = 4,
  SDP_TRANS_TYPE_RTP_TCP        = 5,
  SDP_TRANS_TYPE_RAW_UDP        = 6,
} SDP_TRANS_TYPE_T;

typedef enum SDP_XMIT_TYPE {
  SDP_XMIT_TYPE_UNKNOWN         = 0, 
  SDP_XMIT_TYPE_SENDRECV        = 1, 
  SDP_XMIT_TYPE_RECVONLY        = 2,
  SDP_XMIT_TYPE_SENDONLY        = 3
} SDP_XMIT_TYPE_T;

typedef struct FRAME_RATE {
  unsigned int                  clockHz;
  unsigned int                  frameDeltaHz;
} FRAME_RATE_T;

typedef struct SDP_ICE {
  char                     ufrag[STUN_STRING_MAX]; 
  char                     pwd[STUN_STRING_MAX];
} SDP_ICE_T;

typedef enum SDP_ICE_TRANS_TYPE {
  SDP_ICE_TRANS_TYPE_UNKNOWN     = 0,
  SDP_ICE_TRANS_TYPE_UDP         = 1,
  SDP_ICE_TRANS_TYPE_TCP         = 2
} SDP_ICE_TRANS_TYPE_T;

typedef enum SDP_ICE_TYPE {
  SDP_ICE_TYPE_UNKNOWN         = 0,
  SDP_ICE_TYPE_PRFLX           = 1,
  SDP_ICE_TYPE_SRFLX           = 2,
  SDP_ICE_TYPE_HOST            = 3,
  SDP_ICE_TYPE_RELAY           = 4
} SDP_ICE_TYPE_T;

typedef struct SDP_CANDIDATE {
  long                     foundation;
  int                      component;
  SDP_ICE_TRANS_TYPE_T     transport;
  long                     priority;
  struct sockaddr_in       address;
  SDP_ICE_TYPE_T           type;
  struct sockaddr_in       raddress;
} SDP_CANDIDATE_T;

typedef struct SDP_STREAM_SRTP {
  char                     tag[16];
  SRTP_TYPE_T              type;
  char                     keyBase64[SRTP_KEYLEN_MAX * 2];
} SDP_STREAM_SRTP_T;


#define SDP_RTCPFB_TYPE_ACK           0x01000000       // RFC4585  Positive ack for feedback
#define SDP_RTCPFB_TYPE_NACK          0x02000000       // RFC4585 Negative ack for feedback
#define SDP_RTCPFB_TYPE_TRRINT        0x04000000       // RFC4585 RTCP minimum interval
#define SDP_RTCPFB_TYPE_CCM           0x08000000       // RFC5104 Codec Control Message


#define SDP_RTCPFB_TYPE_ACK_RPSI      0x00000001       // RFC4585 Ref-Pic Selection Indication
#define SDP_RTCPFB_TYPE_ACK_APP       0x00000002       // RFC4585 APP layer feedback
#define SDP_RTCPFB_TYPE_NACK_GENERIC  0x00000010       // RFC4585 6.2.1 Generic NACK
#define SDP_RTCPFB_TYPE_NACK_PLI      0x00000020       // RFC4585 Picture Loss Indication
#define SDP_RTCPFB_TYPE_NACK_SLI      0x00000040       // RFC4585 Slice Loss Indication
#define SDP_RTCPFB_TYPE_NACK_RPSI     0x00000080       // RFC4585 Ref-Pic Selection Indication
#define SDP_RTCPFB_TYPE_NACK_APP      0x00000100       // RFC4585 APP layer feedback
#define SDP_RTCPFB_TYPE_CCM_FIR       0x00001000       // RFC5104 Full Intra Request
#define SDP_RTCPFB_TYPE_CCM_TMMBR     0x00002000       // RFC5104 Temporary max media bit rate
#define SDP_RTCPFB_TYPE_CCM_TSTR      0x00004000       // RFC5104 Temporal spatial trade off
#define SDP_RTCPFB_TYPE_CCM_VBCM      0x00008000       // RFC5104 H.271 Video Back-channel msg
#define SDP_RTCPFB_TYPE_APP_REMB      0x00010000       // APP specific WebRTC Receiver Estimated Max Bandwidth

typedef struct SDP_STREAM_RTCPFB {
  int                      fmtidmin1;
  int                      flags;
  int                      trrIntervalMs;
} SDP_STREAM_RTCPFB_T;

#define SDP_ICE_CANDIDATES_MAX                2

typedef struct SDP_STREAM_DESCR {
  XC_CODEC_TYPE_T          codecType; 
  uint8_t                  available;
  uint8_t                  payloadType;
  uint16_t                 port;
  uint16_t                 portRtcp;
  uint16_t                 rtcpmux;
  unsigned int             clockHz;
  char                     encodingName[31];
  int                      controlId;
  SDP_TRANS_TYPE_T         transType;
  int                      avpf;
  SDP_XMIT_TYPE_T          xmitType;
  SDP_STREAM_SRTP_T        srtp;
  DTLS_FINGERPRINT_T       fingerprint;
  SDP_STREAM_RTCPFB_T      rtcpfb;
  SDP_ICE_T                ice; 
  SDP_CANDIDATE_T          iceCandidates[SDP_ICE_CANDIDATES_MAX]; // 0 -> RTP, 1 -> RTCP
  char                    *attribCustom[4];
} SDP_STREAM_DESCR_T;

typedef struct SDP_DESCR_VID_H264 {
  uint8_t                  packetization_mode;
  uint8_t                  profile_level_id[4];
  SPSPPS_RAW_T             spspps;
} SDP_DESCR_VID_H264_T;

typedef struct SDP_DESCR_VID_MPEG4V {
  uint8_t                  profile_level_id;
  MPG4V_SEQ_HDRS_T         seqHdrs;
} SDP_DESCR_VID_MPEG4V_T;

typedef struct SDP_DESCR_AUD_AAC {
  uint8_t                  profile_level_id;
  char                     mode[16];
  uint8_t                  sizelength;
  uint8_t                  indexlength;
  uint8_t                  indexdeltalength;
  uint8_t                  config[ESDS_STARTCODES_SZMAX];
  ESDS_DECODER_CFG_T       decoderCfg;
} SDP_DESCR_AUD_AAC_T;

typedef struct SDP_DESCR_AUD_AMR {
  uint8_t                  octet_align;
} SDP_DESCR_AUD_AMR_T;

typedef struct SDP_DESCR_AUD_SILK {
  int                      dtx;
  int                      fec;
  int                      maxAvgBitrate;
} SDP_DESCR_AUD_SILK_T;

typedef struct SDP_DESCR_AUD_OPUS{
  int                      todo;
} SDP_DESCR_AUD_OPUS_T;

typedef struct SDP_DESCR_AUD_PCMU {
  int                      pad;
} SDP_DESCR_AUD_PCMU_T;

typedef struct SDP_DESCR_AUD_PCMA {
  int                      pad;
} SDP_DESCR_AUD_PCMA_T;

typedef struct SDP_DESCR_AUD_TELEPHONE_EVENT {
  XC_CODEC_TYPE_T          codecType; 
  uint8_t                  available;
  uint8_t                  payloadType;
  char                     encodingName[32];
} SDP_DESCR_AUD_TELEPHONE_EVENT_T;


typedef struct SDP_DESCR_VID {
 SDP_STREAM_DESCR_T        common;
 FRAME_RATE_T              fps; 
 union {
   SDP_DESCR_VID_H264_T    h264;
   SDP_DESCR_VID_MPEG4V_T  mpg4v;
 } u;
} SDP_DESCR_VID_T;

typedef struct SDP_DESCR_AUD {
 SDP_STREAM_DESCR_T        common;
 unsigned int              channels;
  union {
   SDP_DESCR_AUD_AAC_T     aac;
   SDP_DESCR_AUD_AMR_T     amr;
   SDP_DESCR_AUD_SILK_T    silk;
   SDP_DESCR_AUD_OPUS_T    opus;
   SDP_DESCR_AUD_PCMU_T    pcmu;
   SDP_DESCR_AUD_PCMA_T    pcma;
  } u;
  SDP_DESCR_AUD_TELEPHONE_EVENT_T  aux;
} SDP_DESCR_AUD_T;

typedef struct SDP_DESCR_CONNECT {
  char                     iphost[128];
  uint8_t                  ttl;
  uint16_t                 numaddr;
} SDP_DESCR_CONNECT_T;

typedef struct SDP_DESCR_ORIGIN {
  char                     username[32];  // set to '-' if no username used
  unsigned int             sessionid; 
  unsigned int             sessionver; 
  char                     iphost[128];  // session creator ip / fqdn 
} SDP_DESCR_ORIGIN_T;

typedef struct SDP_DESCR_TIME {
  uint64_t                 tm_start;
  uint64_t                 tm_stop;
} SDP_DESCR_TIME_T;

typedef struct SDP_DESCR {
  SDP_DESCR_ORIGIN_T    o; 
  char                  sessionName[64];
  SDP_DESCR_TIME_T      t; 
  SDP_DESCR_CONNECT_T   c; 
  SDP_DESCR_VID_T       vid; 
  SDP_DESCR_AUD_T       aud; 
} SDP_DESCR_T;

int sdp_load(SDP_DESCR_T *pSdp, const char *filepath);
int sdp_parse(SDP_DESCR_T *pSdp, const char *pData, unsigned int len, int verify, int allow_noport_rtsp);
int sdp_write(const SDP_DESCR_T *pSdp, void *pFp, int logLevel);
int sdp_write_path(const SDP_DESCR_T *pSdp, const char *path);
int sdp_dump(const SDP_DESCR_T *pSdp, char *buf, unsigned int len);
int sdp_isvalidFile(FILE_STREAM_T *pFile);
int sdp_isvalidFilePath(const char *path);

int sdp_parse_seqhdrs(XC_CODEC_TYPE_T codecType, const char *strhdrs, void *pArg);
char *sdp_dtls_get_fingerprint_typestr(DTLS_FINGERPRINT_TYPE_T type);



#endif //__SDP_H__
