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

#ifndef __VSX_LIB_H__
#define __VSX_LIB_H__

#include <time.h>

#if !defined(WIN32)
#include <sys/time.h>
#endif // WIN32

// Should match IXCODE_VIDEO_OUT_MAX
#define VIDEO_OUT_MAX    4

typedef enum VSX_RC {
  VSX_RC_NOTREADY         = -2,
  VSX_RC_ERROR            = -1,
  VSX_RC_OK               =  0
} VSX_RC_T;

typedef enum VSX_VERBOSITY {
  VSX_VERBOSITY_NORMAL        =   1,
  VSX_VERBOSITY_HIGH          =   2,
  VSX_VERBOSITY_VERYHIGH      =   3,
  VSX_VERBOSITY_HIGHEST       =   4
} VSX_VERBOSITY_T;

typedef enum VSX_STREAMFLAGS {
  VSX_STREAMFLAGS_DEFAULT           = 0x00,
  VSX_STREAMFLAGS_AV_SAME_START_TM  = 0x02,
  VSX_STREAMFLAGS_RTPMUX            = 0x04,
  VSX_STREAMFLAGS_RTCPMUX           = 0x08,
  VSX_STREAMFLAGS_RTCPDISABLE       = 0x10, // not implemented
  VSX_STREAMFLAGS_RTCPIGNOREBYE     = 0x20
} VSX_STREAMFLAGS_T;

typedef enum HTTP_AUTH_TYPE {
  HTTP_AUTH_TYPE_DIGEST              = 0,
  HTTP_AUTH_TYPE_ALLOW_BASIC         = 1,
  HTTP_AUTH_TYPE_FORCE_BASIC         = 2
} HTTP_AUTH_TYPE_T;


/**
 *
 * Defines how input audio + video frames are
 * processed and ordered
 *
 */
typedef enum VSX_STREAM_READER_TYPE {

  /** 
   * Default type which will sync audio & video
   */ 
  VSX_STREAM_READER_TYPE_DEFAULT        = 0,  

  /** 
   * Deprecated type for capture from local microphone
   */
  VSX_STREAM_READER_TYPE_PREFERAUDIO    = 1, 

  /** 
   * Do not rely on input timestamps
   */ 
  VSX_STREAM_READER_TYPE_NOTIMESTAMPS   = 2,

  /** 
   *  Do not try to sync audio + video 
   * Default used by video conferencing PIPs
   */ 
  VSX_STREAM_READER_TYPE_NOSYNC         = 3

} VSX_STREAM_READER_TYPE_T;

/**
 * Defines the PIP layout manager type
 */
typedef enum PIP_LAYOUT_TYPE {
  PIP_LAYOUT_TYPE_UNKNOWN        = 0,
  PIP_LAYOUT_TYPE_MANUAL         = 1,
  PIP_LAYOUT_TYPE_GRID_P2P       = 2,
  PIP_LAYOUT_TYPE_GRID           = 3,
  PIP_LAYOUT_TYPE_VAD_P2P        = 4,
  PIP_LAYOUT_TYPE_VAD            = 5
} PIP_LAYOUT_TYPE_T;

/**
 * Defines the STUN policy used
 */
typedef enum STUN_POLICY {
  STUN_POLICY_NONE           = 0,
  STUN_POLICY_ENABLED        = 1,
  STUN_POLICY_XMIT_IF_RCVD   = 2,
  STUN_POLICY_MAX            = 3
} STUN_POLICY_T;

/**
 * Defines the TURN policy used
 */
typedef enum TURN_POLICY {
  TURN_POLICY_IF_NEEDED      = 0,
  TURN_POLICY_MANDATORY      = 1,
  TURN_POLICY_DISABLED       = 2,
  TURN_POLICY_MAX            = 3
} TURN_POLICY_T;

typedef enum RTSP_TRANSPORT_TYPE {
  RTSP_TRANSPORT_TYPE_UNKNOWWN           = 0,
  RTSP_TRANSPORT_TYPE_INTERLEAVED        = 1,
  RTSP_TRANSPORT_TYPE_UDP                = 2
} RTSP_TRANSPORT_TYPE_T;

typedef enum RTSP_TRANSPORT_SECURITY_TYPE {
  RTSP_TRANSPORT_SECURITY_TYPE_UNKNOWN   = 0,
  RTSP_TRANSPORT_SECURITY_TYPE_SRTP      = 1,
  RTSP_TRANSPORT_SECURITY_TYPE_RTP       = 2
} RTSP_TRANSPORT_SECURITY_TYPE_T;

typedef enum DASH_MPD_TYPE{
  DASH_MPD_TYPE_INVALID                        = 0,
  DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_NUMBER   = 1,
  DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME     = 2,
  DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_NUMBER     = 3,
  DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_TIME       = 4
} DASH_MPD_TYPE_T;

/**
 * Defines the audio mixer configuration
 */
typedef struct MIXER_CFG {

  /**
    *
    * Set to 1 to enable the conference audio mixer
    *
    */
  int active;
 
  /**
    *
    * Set to 1 to have main streamer act as the conference encoder thread, and 
    * each conference participant is a PIP instance.  This is the same as
    * the special input type: '--in=conference'.
    *
    */
  int conferenceInputDriver;

  /**
    *
    * Set to 1 to enable conference mixer Voice Activity Detection 
    * The mixer will mix a source's input if source speech is detected.
    * If using webrtc VAD module the VAD value (1-4) is the webrtc specific
    * VAD mode type (1 - quality, 2 - low bitrate, 3 aggressive, 4 very aggressive)
    *
    */
  int vad;

  /**
    *
    * Enable Acoustic Gain Control filter for mixer input
    *
    */
  int agc;

  /**
    *
    * Enable Denoise filter for mixer input
    *
    */
  int denoise;

  /**
    *
    * Set to 1 to run the main mixer thread as real-time thread
    *
    */
  int highPriority;

  /**
    *
    * Set to 1 to include each mixer source audio in the output
    * of the output specific mixer audio.
    *
    */
  int includeSelfChannel;

  /**
    *
    * Conference expiration timer if no participants exist , after
    * 1st participant has joined the conference.
    *
    */
  unsigned int pip_idletmt_ms;

  /**
    *
    * Conference expiration timer while waiting for first participant
    *
    */
  unsigned int pip_idletmt_1stpip_ms;

  /**
    *
    * Conference video layout type
    *
    */
  enum PIP_LAYOUT_TYPE  layoutType;

} MIXER_CFG_T;

/**
 * SRTP authentication protection type
 */
typedef enum SRTP_AUTH_TYPE {
  SRTP_AUTH_NONE         = 0,
  SRTP_AUTH_SHA1_32      = 1,
  SRTP_AUTH_SHA1_80      = 2
} SRTP_AUTH_TYPE_T;

/**
 * SRTP confidentiality protection type
 */
typedef enum SRTP_CONF_TYPE {
  SRTP_CONF_NONE         = 0,
  SRTP_CONF_AES_128      = 1
} SRTP_CONF_TYPE_T;


/**
 * DTLS handshake timeout settings
 */
typedef struct DTLS_TIMEOUT_CFG {

  /**
   * DTLS / DTLS-SRTP RTP channel handshake timeout.  Stream output will
   * not begin until all RTP handshakes have completed.
   */
  int                         handshakeRtpTimeoutMs;

  /**
   * DTLS / DTLS-SRTP RTCP channel grace period to keep on attempting any
   * RTCP handshake after all RTP handshakes have completed and before
   * stream output begins. 
   */
  int                         handshakeRtcpAdditionalMs;

  /**
   * DTLS / DTLS-SRTP channel maximum time for attempting to complete
   * an RTCP handshake(s) before giving up on RTCP transmission. 
   */
  int                         handshakeRtcpAdditionalGiveupMs;

} DTLS_TIMEOUT_CFG_T;

/**
 * SRTP configuration
 */
typedef struct SRTP_CFG {

  /**
   * SRTP authentication profile type
   */
  SRTP_AUTH_TYPE_T     authType;

  /**
   * SRTP confidentiality profile type
   */
  SRTP_CONF_TYPE_T     confType;

  /**
   * Base64 encoded SRTP output key(s).  If using separate keys for video
   * and audio channels then the video key should be followed by a ',' and
   * then the audio key. 
   */
  const char          *keysBase64;

  /**
   * Flag indicating that SRTP output should be enabled.  This is redundant
   * with specifying '--out=srtp://' as the output transport type.
   */
  int                  srtp;

  /**
   * Flag indicating that DTLS output should be enabled.  This is redundant
   * with specifying '--out=dtls://' as the output transport type.
   * Both srtp and dtls flags may be set to indicate DTLS-SRTP output mode.
   */
  int                  dtls;

  /**
   * Flag indicating that we should used DTLS server keying material extractor
   * mode. 
   */
  int                  dtls_handshake_server;

  /*
   * DTLS certificate path
   */
  const char          *dtlscertpath;

  /*
   * DTLS private key path
   */
  const char          *dtlsprivkeypath;

  /*
   * DTLS handshake timeout configuration
   */
  DTLS_TIMEOUT_CFG_T   dtlsTimeouts;

} SRTP_CFG_T;


/**
 * RTCP FIR (Full Intra Refresh) configuration
 */
typedef struct FIR_CFG {

  /**
   *
   * Set to 1 to enable the local encoder to accept IDR requests which are triggered by remote clients.
   * This flag controls events such as:
   *   - Reception of a remote RTCP Feedback Type FIR for a locally  encoded RTP stream output.
   *   - Remote RTSP connection for locally encoded RTP stream output.
   *   - Any remote TCP based connection for locally encoded stream output such as
   *     /tslive, /flvlive, /mkvlive, RTMP, etc...
   * This flag does not affect how the 'xcode="videoForceIDR=1"'
   * local IDR request is processed when handling an encoder reconfiguration udpate ('/config').
   *
   */
  int fir_encoder;

  /**
   *
   * Set to 1 to accept RTCP FB FIR requests from a remote RTP receiver of the locally output RTP stream.
   *
   */
  int fir_recv_via_rtcp;

  /**
   *
   * Set to 1 to create a local encoder IDR request (if needed) upon a remote connection
   * request for the locally output media stream(s).
   *
   */
  int fir_recv_from_connect;

  /**
   *
   * Set to 1 to create a local encoder IDR request (if needed) upon a remote FB FIR request
   * such as one coming via an RTCP FB FIR message.
   *
   */
  int fir_recv_from_remote;

  /**
   *
   * Set to 1 to enable sending RTCP FB FIR requests to a remote RTP sender of the input stream
   * upon a remote connection request for the locally output.
   *
   */
  int fir_send_from_local;

  /**
   *
   * Set to 1 to enable sending RTCP FB FIR requests to a remote RTP sender of the input stream
   * upon receiving a remote FB FIR request, such as one coming via an RTCP FB FIR message.
   *
   */
  int fir_send_from_remote;

  /**
   *
   * Set to 1 to enable sending RTCP FB FIR requests to a remote RTP sender of the input stream
   * which are initiated by the local decoder if a keyframe is needed.
   *
   */
  int fir_send_from_decoder;

  /**
   *
   * Set to 1 to enable sending RTCP FB FIR requests to a remote RTP sender of the input stream
   * from which are initiated by local RTP input capture processor.  The local input capture
   * processor may request an IDR if RTP packets are missing or late.
   *
   */
  int fir_send_from_capture;

} FIR_CFG_T;

/**
 * STUN server responder configuration
 */
typedef struct STUN_RESPONDER_CFG {

  /**
   * STUN responder policy
   */
  STUN_POLICY_T bindingResponse;

  /**
   * Stun binding USERNAME attribute
   */
  const char *respUsername;

  /**
   * STUN binding REALM attribute
   */
  const char *respRealm;

  /**
   * STUN binding password
   */
  const char *respPass;

  /**
   * Flag indicating to use ICE ufrag to construct STUN username and password
   */
  int respUseSDPIceufrag;

} STUN_RESPONDER_CFG_T;

/**
 * STUN client requestor configuration
 */
typedef struct STUN_REQUESTOR_CFG {

  /**
   * STUN requestor policy
   */
  STUN_POLICY_T bindingRequest;

  /**
   * Stun binding USERNAME attribute
   */
  const char *reqUsername;

  /**
   * STUN binding REALM attribute
   */
  const char *reqRealm;

  /**
   * STUN binding password
   */
  const char *reqPass;

} STUN_REQUESTOR_CFG_T;

/**
 * TURN server connection configuration
 */
typedef struct TURN_CFG {

  /**
   * TURN username 
   */
  const char *turnUsername;

  /**
   * TURN user password
   */
  const char *turnPass;

  /**
   * TURN realm for Long-Term-Auth 
   */
  const char *turnRealm;

  /**
   * TURN server address:port used for incoming data
   */
  const char *turnServer;

  /**
   * TURN server address:port used for outgoing data
   */
  //const char *turnOutput;

  /**
   * SDP output file path of the input capture SDP file which will contain
   * any TURN relay allocations
   */
  const char *turnSdpOutPath;

  /**
   * TURN realy address:port of the peer endpoint.  This is the the TURN ip:port 
   * allocation for reaching the peer-receiver endpoint
   */
  const char *turnPeerRelay;

  /**
   * TURN client policy
   */
  TURN_POLICY_T turnPolicy;

} TURN_CFG_T;


typedef enum PIP_FLAGS {
  PIP_FLAGS_INSERT_AFTER_SCALE    =  0x00,
  PIP_FLAGS_INSERT_BEFORE_SCALE   =  0x01,
  PIP_FLAGS_POSITION_BOTTOM       =  0x02,
  PIP_FLAGS_POSITION_RIGHT        =  0x04,
  PIP_FLAGS_CLOSE_ON_END          =  0x08
} PIP_FLAGS_T;

/**
 * PIP (Picture-In-Picture) configuration
 */
typedef struct PIP_CFG {

  /**
   * PIP input source
   */
  const char *input;

  /**
   * PIP input configuration file 
   */
  const char *cfgfile;

  /**
   * PIP transcoder parameter string 
   */
  const char *strxcode; 

  /**
   * PIP X coordinate placement
   */
  int posx;

  /**
   * PIP Y coordinate placement
   */
  int posy;

  /**
   * PIP Z ordering index
   */
  int zorder;

  /**
   * PIP configuration flags
   */
  PIP_FLAGS_T flags;

  /**
   * PIP alpha blend minimum threshold
   */
  int alphamax_min1;

  /**
   * PIP alpha blend maximum threshold
   */
  int alphamin_min1;

  /**
   * PIP video fps
   */
  double fps;

  /**
   * PIP output '--out=' configuration
   */
  const char *output;

  /**
   * PIP output '--tslive=' configuration
   */
  const char *tsliveaddr;

  /**
   * PIP output '--rtppayloadtype=' configuration
   */
  const char *output_rtpptstr;

  /**
   * PIP output '--rtpssrc=' configuration
   */
  const char *output_rtpssrcsstr;

  /**
   * PIP output MTU
   */
  int         mtu;

  /**
   * PIP output '--rtppktzmode' configuration
   */
  int         rtpPktzMode;

  /**
   * PIP output '--srtpkey' configuration
   */
  const char *srtpKeysBase64;

  /**
   * PIP output '--dtls' configuration
   */
  int         use_dtls;

  /**
   * PIP output '--srtp' configuration
   */
  int         use_srtp;

  /**
   * PIP output '--dtlsserver' configuration
   */
  int         dtls_handshake_server;

  /**
   * PIP output '--rtcpports' configuration
   */
  const char *rtcpPorts;

  /**
   * PIP layout manager type
   */
  enum PIP_LAYOUT_TYPE  layoutType;

  /**
   * PIP output '--novideo' configuration
   */
  int novid;

  /**
   * PIP output '--noaudio' configuration
   */
  int noaud;

  /**
   * PIP output '--abrauto' configuration
   */
  int abrAuto;

  /**
   * PIP output '--abrself' configuration
   */
  int abrSelf;

  /**
   * PIP output '--rtpretransmit' configuration
   */
  int nackRtpRetransmitVideo;  

  /**
   * PIP output '--rembxmitmaxrate' configuration
   */
  unsigned int apprembRtcpSendVideoMaxRateBps;

  /**
   * PIP output '--rembxmitminrate' configuration
   */
  unsigned int apprembRtcpSendVideoMinRateBps;

  /**
   *
   * PIP output '--rembxmitforce' configuration
   *
   */
  int apprembRtcpSendVideoForceAdjustment;

  /**
   * PIP output flag enabling duplicate PIP insertion matching the 
   * same input path.  This should be enabled for chime conference 
   * announcements
   */
  int allowDuplicate; 

  /**
   * PIP output flag enabling audio (only) PIP even if there is no overlay
   * mixer audio output.
   */
  int audioChime;

  /**
   * PIP delaystart flag used to start a STUN-responder only PIP
   */
  int delayed_output;

  /**
   * PIP start conference announcement file
   */
  const char *startChimeFile;

  /**
   * PIP end conference announcement file
   */
  const char *stopChimeFile;

  /**
   * PIP STUN server responder policy
   */
  STUN_POLICY_T stunBindingResponse;

  /**
   * Flag indicating to use ICE ufrag to construct STUN username and password
   */
  int stunRespUseSDPIceufrag;
  
  /**
   * PIP STUN client requestor policy
   */
  STUN_POLICY_T stunBindingRequest;

  /**
   * Stun binding USERNAME attribute
   */
  const char *stunReqUsername;

  /**
   * Stun binding realm 
   */
  const char *stunReqRealm;

  /**
   * Stun binding password
   */
  const char *stunReqPass;

  /**
   * TURN server configuration
   */
  TURN_CFG_T turnCfg;

} PIP_CFG_T;

/**
 * VSX configuration parameters
 */
typedef struct VSXLIB_STREAM_PARAMS {

  /**
   *
   * verbosity logging output level
   *
   */
  VSX_VERBOSITY_T verbosity;

  /**
   *
   * Optional log file output path
   *
   */
  const char *logfile;
 
  /**
   *
   * The maximum size of the output log file before rolling it
   *
   */
  unsigned int logmaxsz;

  /**
   *
   * Maximum number of log files to preserve
   *
   */
  unsigned int logrollmax;

  /**
   *
   * http access log file path
   *
   */
  const char *httpaccesslogfile;

  /**
   *
   * PIP configuration server listening address and port string
   *
   */
  const char *pipaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of PIP server listener connections
   *
   */
  unsigned int piphttpmax;

  /**
   *
   * Maximum number of PIPs (or conference participants) allowed.
   * If not set, will default to system hardcoded MAX_PIPS
   *
   */
  int pipMax;

  /**
   *
   * The startup PIP configuration 
   *
   */
  PIP_CFG_T pipCfg[1];

  /**
   *
   * Conference audio mixer configuration
   *
   */
  MIXER_CFG_T mixerCfg;

  /**
   *
   * Stream output string(s) supporting multiple mirrored outputs to distinct destinations.
   * output address:ports or filepath
   *
   */
  const char *outputs[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * output transport type (rtp|m2t)
   *
   */
  const char *transproto;

  /**
   *
   * output packet MTU
   *
   */
  int mtu; 

  /**
   *
   * If > 0, tries to packetize RTP audio frames into aggregates of this duration
   *
   */
  int audioAggregatePktsMs;

  /**
   *
   * output RTP payload type string
   *
   */
  const char *output_rtpptstr;

  /**
   *
   * output RTP SSRCs string
   *
   */
  const char *output_rtpssrcsstr;

  /**
   *
   * output RTP clock rate string
   *
   */
  const char *output_rtpclockstr;

  /**
   *
   * Set to 1 to bind output RTP socket to input capture socket to
   * preserve outgoing source port
   *
   */
  int rtp_useCaptureSrcPort; 

  /**
   *
   * output RTCP non-default ports 
   *
   */
  const char *rtcpPorts[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * video codec specific output packetization mode
   *
   */
  int rtpPktzMode;

  /**
   *
   * Maximum number of output RTP/UDP streams
   *
   */
  unsigned int rtplivemax;

  /**
   *
   * Set to 1 to not include video specific sequence start headers
   *
   */
  int noIncludeSeqHdrs;

  /**
   *
   * SRTP (Secure RTP) configuration
   *
   */
   SRTP_CFG_T srtpCfgs[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * Transcoder configuration parameters
   *
   */
  const char *strxcode; 

  /**
   *
   * IP address or hostname which will be used in place of the default IP Address 
   * obtained automatically from the local system network interface.  This value 
   * will be used to override the default IP Address for SDP, RTMP, RTSP based 
   * streaming.  It may be necessary to set this value to a global machine IP Address 
   * if the system is used to bridge a private and public network.
   *  
   */
  const char *strlocalhost;

  /**
   *
   * RTMP server listening address and port string
   * RTMP tunneled connections are implicitly allowed on the same
   * RTMP server listener unless rtmpnotunnel is set to 1
   *
   */
  const char *rtmpliveaddr[VIDEO_OUT_MAX];

  /**
   *
   * RTMP tunneling only  server listening address and port string
   *
   */
  const char *rtmptliveaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of output RTMP connections (streams) 
   *
   */
  unsigned int rtmplivemax;

  /**
   *
   * Set to utilize RTMP Handshake signature
   *
   */
  int rtmpfp9;

  /**
   *
   * RTMP Connect pageUrl
   *
   */
  const char *rtmppageurl;

  /**
   *
   * RTMP Connect SWFUrl
   *
   */
  const char *rtmpswfurl;

  /**
   *
   * Number of queue slots used to buffer complete frames.
   * This value affects RTMP, FLV, MKV/WebM, DASH/MOOF
   *
   */
  unsigned int rtmpq_slots;

  /**
   *
   *  Initial size of each queue slots used to buffer complete frames
   * This value affects RTMP, FLV, MKV/WebM, DASH/MOOF
   *
   */
  unsigned int rtmpq_szslot;
  
  /**
   *
   * Max size of each slot used to buffer complete frames
   * This value affects RTMP, FLV, MKV/WebM, DASH/MOOF
   *
   */
  unsigned int rtmpq_szslotmax;

  /**
   *
   * Boolean disabling RTMP server tunneling mode over RTMP server listener
   *
   */
  int rtmpnotunnel;

  /**
   *
   * RTSP configuration server listening address and port string
   *
   */
  const char *rtspliveaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of output RTSP control connections 
   *
   */
  unsigned int rtsplivemax;

  /**
   *
   * RTSP connection timeout
   *
   */
  unsigned int rtspsessiontimeout;

  /**
   *
   * Set to 1 to refresh RTSP control channel timeout upon receiving RTCP packet
   * from client.
   *
   */
  int rtsprefreshtimeoutviartcp;

  /**
   *
   * Number of queue slots used to hold complete RTSP interleaved-mode frames.
   *
   */
  unsigned int rtspinterq_slots;

  /**
   *
   *  Initial size of each queue slots used to hold complete RTSP interleaved-mode frames
   *
   */
  unsigned int rtspinterq_szslot;

  /**
   *
   * Max size of each slot used to hold complete RTSP interleaved-mode frames
   *
   */
  unsigned int rtspinterq_szslotmax;

  /**
   *
   * RTP output media channel source port for all outbound RTSP RTP packets
   * This may need to be set to a value such as '6970' to transgress firewalls and
   * public RTSP proxies.
   *
   */
  int rtsplocalport;

  /**
   *
   * A CSV of RTSP client User-Agent strings which are matched to force a connecting client
   * to use TCP interleaved mode. The RTSP SETUP response will return the code
   * Unsupported Transport for a matching client User-Agent entry.
   *
   */
  const char *rtspualist;

  /**
   * RTSP transport type.  Defaults to UDP/RTP.
   */
  RTSP_TRANSPORT_TYPE_T rtsptranstype;

  /**
   * RTSP transport security type.  Defaults to UDP/RTP
   */
  RTSP_TRANSPORT_SECURITY_TYPE_T rtspsecuritytype;

  /**
   *
   * max number of concurrent HTTP / HTTPS sessions
   *
   */
  unsigned int httpmax;

  /**
   *
   * HTTP tslive server listening address and port string
   *
   */
  const char *tsliveaddr[VIDEO_OUT_MAX]; 

  /**
   *
   * Maximum number of output HTTP tslive client connections 
   *
   */
  unsigned int tslivemax;

  /**
   *
   * Number of slots containing MPEG-2 TS packets for tslive output
   *
   */
  unsigned int tsliveq_slots;

  /**
   *
   * Max number of slots containing MPEG-2 TS packets for tslive output
   *
   */
  unsigned int tsliveq_slotsmax;

  /**
   *
   * Size of each MPEG-2 TS slot for tslive output
   *
   */
  unsigned int tsliveq_szslot;

  /**
   *
   * HTTP MPEG-DASH server listening address and port string
   *
   */
  const char *dashliveaddr[VIDEO_OUT_MAX]; 

  /**
   *
   * Maximum number of output HTTP MPEG-DASH server client connections 
   *
   */
  unsigned int dashlivemax;

  /**
   *
   * If set will produce MPEG-2 TS segments (just like httplive)
   *
   */
  int dash_ts_segments;                     

  /**
   *
   * If set will produce mp4 MOOF segments;
   *
   */
  int dash_moof_segments;                   


  DASH_MPD_TYPE_T dash_mpd_type;

  /**
   *
   * mp4 MOOF segment output directory
   *
   */
  const char *moofdir;

  /**
   *
   * mp4 MOOF file naming prefix
   *
   */
  const char *mooffileprefix;

  /**
   *
   * DASH media entry prefix used in .mpd playlist
   *
   */
  const char *moofuriprefix;

  /**
   *
   * Number of MOOF media entries to publish in .mpd playlist
   *
   */
  unsigned int moofindexcount;

  /**
   *
   * If set will not delete mp4 MOOF segments on exit
   *
   */
  int moof_nodelete;

  /**
   *
   * If set, the mp4 MOOF packetizer will create an mp4 initializer file
   * and avoid including initialization information within successive
   * mp4 file segments
   *
   */
  int moofUseInitMp4;

  /**
   *
   * If set to use Random Access Points, each mp4 file segment will begin
   * on a video key-frame.
   *
   */
  int moofUseRAP;

  /**
   *
   * Sync mp4 cutoff points between multiple xcoder output streams
   *
   */
  int moofSyncSegments;

  /**
   *
   * Maximum MOOF fragment duration within an mp4 file segment
   *
   */
  float moofTrafMaxDurationSec;
  
  /**
   *
   * Maximum mp4 duration used by the MPEG-DASH mp4 segmenter.
   *
   */
  float moofMp4MaxDurationSec;

  /**
   *
   * Minimum mp4 duration used by the MPEG-DASH mp4 segmenter. 
   *
   */
  float moofMp4MinDurationSec;

  /**
   *
   * HTTP FLV media encapsulated output server listening address and port string
   *
   */
  const char *flvliveaddr[VIDEO_OUT_MAX]; 

  /**
   *
   * Maximum number of output FLV server client connections (streams)
   *
   */
  unsigned int flvlivemax;

  /**
   *
   * Pre-buffering delay in seconds for MKV / WebM output to pause before beginning
   * output.  This is used to pre-buffer the client player. 
   *
   */
  float flvlivedelay;

  /**
   *
   * Output file path of FLV format recording file
   *
   */
  const char *flvrecord[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * HTTP MKV / WebM media encapsulated output server listening address and port string
   *
   */
  const char *mkvliveaddr[VIDEO_OUT_MAX]; 

  /**
   *
   * Maximum number of output MKV / WebM server client connections 
   *
   */
  unsigned int mkvlivemax;

  /**
   *
   * Pre-buffering delay in seconds for MKV / WebM output to pause before beginning
   * output.  This is used to pre-buffer the client player. 
   *
   */
  float mkvlivedelay;

  const char *mkvrecord[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * HTTPLive server listening address and port string
   *
   */
  const char *httpliveaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of output HTTPLive server client connections 
   *
   */
  unsigned int httplivemax;

  /**
   *
   * HTTPLive MPEG-2 TS segment output directory
   *
   */
  const char *httplivedir;

  /**
   *
   * HTTPLive MPEG-2 TS segment file naming prefix
   *
   */
  const char *httplivefileprefix;

  /**
   *
   * HTTPLive media entry prefix used in .m3u8 playlist
   *
   */
  const char *httpliveuriprefix;

  /**
   *
   * CSV of bitrates in Kbps advertised in the HTTPLive .m3u8 playlist for each
   * bitrate specific stream.
   *
   */
  const char *httplivebitrates;

  /**
   *
   * Number of HTTPLive MPEG-2 TS media entries to publish in .m3u8 playlist
   *
   */
  unsigned int httpliveindexcount;

  /**
   *
   * HTTPLive segmenter MPEG-2 TS chunk segment duration in seconds
   *
   */
  float httplive_chunkduration;

  /**
   *
   * If set will not delete HTTPLive MPEG-2 TS segments on exit
   *
   */
  int httplive_nodelete;

  /**
   *
   * HTTP automatic format adaptation server address and port string
   *
   */
  const char *liveaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of output HTTPLive server client connections 
   *
   */
  unsigned int livemax;

  /**
   * HTTP access password
   */
  const char *livepwd; 

  /**
   * If set, allow HTTP/RTSP Basic authentication, otherwise only Digest authentication
   */
  HTTP_AUTH_TYPE_T httpauthtype;

  /**
   *
   * If set, the HTTP media server will follow symbolic links when returning
   * static file content from the html directory.
   *
   */
  int enable_symlink;

  /**
   *
   * If set, disables media listing of the root media directory for the
   * '/list' vsx-wp (Web Portal) URI.
   *
   *
   */ 
  int disable_root_dirlist;

  /**
   *
   * HTTP status server listening address and port string
   *
   */
  const char *statusaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of HTTP Status server connections 
   *
   */
  unsigned int statusmax;

  /**
   *
   * HTTP configuration server listening address and port string
   *
   */
  const char *configaddr[VIDEO_OUT_MAX];

  /**
   *
   * Maximum number of HTTP Configuration server connections 
   *
   */
  unsigned int configmax;

  /**
   *
   * CSV of 'IP Address / Mask bits' from which to explicitly allow
   * connections from.
   *
   */
  const char *allowlist;

  /**
   *
   * CSV of 'IP Address / Mask bits' from which to deny connections from.
   *
   */
  const char *denylist;

  /**
   *
   * CSV of 'IP Address / Mask bits' from which to explicitly allow
   * connections to the HTTP '/status' URL from.
   *
   */
  const char *statusallowlist;

  /**
   *
   * SSL certificate path
   *
   */
  const char *sslcertpath;

  /**
   *
   * SSL private key path
   *
   */
  const char *sslprivkeypath;

  /**
   *
   * SSL/TLS client|server method
   *
   */
  const char *sslmethod;

  /**
   *
   * RTCP Sender Report interval
   *
   */
  float frtcp_sr_intervalsec;

  /**
   *
   * RTCP Receiver Report interval
   *
   */
  float frtcp_rr_intervalsec;

  /**
   *
   * RTCP Receiver flag to enable RTCP replies even if subscribed to a multicast socket
   *
   */
  int rtcp_reply_from_mcast;

  /**
   *
   * If set, will use the favoffsetrtcp
   *
   */
  int haveavoffsetrtcp;

  /**
   *
   * RTCP advertised audio/video offset in seconds
   *
   */
  float favoffsetrtcp;

  /**
   *
   * audio/video offset
   *
   */
  int haveavoffsets[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * Output stream index specific audio / video synchronization offset
   *
   */
  float favoffsets[IXCODE_VIDEO_OUT_MAX];

  /**
   *
   * MPEG-2 TS specific audio packetization timestamp offset
   *
   */
  float faoffset_m2tpktz;

  /**
   *
   * MKV specific audio packetization timestamp offset
   *
   */
  float faoffset_mkvpktz;

  /**
   *
   * MKV specific audio packetization timestamp offset for recorded output
   *
   */
  float faoffset_mkvpktz_record;

  /**
   *
   * positive media output buffering delay (from capture / input time) in seconds;
   *
   */
  float favdelay;

  /**
   *
   * Set to 1 to disable video output
   *
   */
  int novid;

  /**
   *
   * Set to 1 to disable audio output
   *
   */
  int noaud;

  /**
   *
   * If set, media timestamps will ignore computed audio / video  adjustment for 
   * video encoder frame lag
   *
   */
  int ignoreencdelay; 

  /**
   *
   * system configuration file path
   *
   */
  const char *confpath;

  /**
   *
   * devices profile configuration file path
   *
   */
  const char *devconfpath;

  /**
   *
   * Stream statistics output file path
   *
   */
  const char *statfilepath;

  /**
   *
   * Stream statistics dumping interval
   *
   */
  unsigned int statdumpintervalms;

  /**
   *
   * Automatic Bitrate Adjustment for RTP output based on 
   * RTCP feedback from RTP receiver
   *
   */
  int abrAuto;

  /**
   *
   * Automatic Bitrate Adjustment for RTP output based
   * on assumption that local encoder is running too slow.
   * 
   */
  int abrSelf;

  /**
   *
   * Video frame thinning (dropping) for FLV, RTMP, MKV output
   *
   */
  int frameThin;

  /**
   *
   * system license file path
   *
   */
  const char *licfilepath;

  /**
   *
   * Input capture specific
   *
   */

  /**
   *
   * input protocol / address:ports, device, local file, pcap source
   *
   */
  const char *inputs[2];

  /**
   *
   * live capture specific
   *
   */
  int islive;

  /**
   *
   * Input capture description filter parameters
   *
   */
  const char *strfilters[2]; 

  /**
   *
   * input packet based queue number of slots (for recording)
   *
   */
  unsigned int pktqslots; 

  /**
   *
   * input video frame queue number of slots 
   *
   */
  unsigned int frvidqslots; 

  /**
   *
   * input audio frame queue number of slots
   *
   */
  unsigned int fraudqslots;

  /**
   *
   * Dynamically Pre-allocate output buffering queue
   *
   */
  int outq_prealloc;

  
  /**
   *
   * Thread stack size
   *
   */
  unsigned int thread_stack_size;


  /**
   *
   * audio output samples buffer storage size in ms
   *
   */
  unsigned int outaudbufdurationms;

  /**
   *
   * set to 1 for m2t direct replay to use local system timing
   *
   */
  int uselocalpcr; 

  /**
   *
   * set to 1 to force MPEG-2 TS PES extraction (non-replay)
   *
   */
  int extractpes;

  /**
   *
   * MPEG-2 TS packetization max PCR interval
   *
   */
  unsigned int m2t_maxpcrintervalms;

  /**
   *
   * MPEG-2 TS packetization max PAT interval
   *
   */
  unsigned int m2t_maxpatintervalms;

  /**
   *
   * Set to 0 > n > MTU to prevent buffering max allowable 
   * MPEG-2 TS frames before sending onto the network
   *
   */
  unsigned int m2t_xmitflushpkts;

  /**
   *
   * Input socket capture idle timeout
   *
   */
  unsigned int capture_idletmt_ms;

  /**
   *
   * Input socket capture idle timeout for 1st packet
   *
   */
  unsigned int capture_idletmt_1stpkt_ms;

  /**
   *
   * Input Max # ms to wait for late RTP video packet
   *
   */
  unsigned int capture_max_rtpplayoutviddelayms;

  /**
   *
   * Input Max # ms to wait for late RTP audio packet
   *
   */
  unsigned int capture_max_rtpplayoutauddelayms;

  /**
   *
   * http/rtsp/rtmp based client connection retry counter
   *
   */
  int connectretrycntminone;

  /**
   *
   * Policy determining how damaged RTP video frames are handled before being
   * passed to the decoder or any stream output.
   *
   * 0 - Do not skip a frame when any of the frame contents were lost in transit
   * 1 - (default) Skip a frame when the beginning of the frame contents were lost
   * 2 - Skip a frame when the any of the frame contents were lost
   *
   */
  int capture_rtp_frame_drop_policy;

  /**
    *
    * FIR (Full Intra Request) RTCP configuration
    *
    */
  FIR_CFG_T firCfg;

  /**
   *
   * Flag to enable RTP re-transmission based on RTCP NACK reception
   *
   */
  int nackRtpRetransmitVideo;  

  /**
   *
   * Flag to enable RTCP NACK messages to be ouput from the RTP receiver
   *
   */
  int nackRtcpSendVideo;

  /**
   *
   * Flag to enable RTCP REMB (Receiver Estimated Max Bandwidth) messages to be 
   * ouput from the RTP receiver
   *
   */
  int apprembRtcpSendVideo;

  /**
   *
   * Ceiling of REMB reported bitrate
   *
   */
  unsigned int apprembRtcpSendVideoMaxRateBps;

  /**
   *
   * Floor of REMB reported bitrate
   *
   */
  unsigned int apprembRtcpSendVideoMinRateBps;

  /**
   *
   * Flag to enable RTCP REMB (Receiver Estimated Max Bandwidth) messages to be 
   * ouput from the RTP receiver even during good network conditions.
   *
   */
  int apprembRtcpSendVideoForceAdjustment;

  /**
    *
    * STUN server responder configuration
    *
    */
  STUN_RESPONDER_CFG_T stunResponderCfg;

  /**
    *
    * STUN client requestor configuration
    *
    */
  STUN_REQUESTOR_CFG_T stunRequestorCfg;

  /**
   * TURN server configuration
   */
  TURN_CFG_T turnCfg;

  /**
   *
   * If set to 1, treats http capture as real-time for non-live static files
   *
   */
  int caprealtime;

  /**
   *
   * Enable capure thread high priority
   *
   */
  int caphighprio;

  /**
   *
   * Enable xmit thread high priority
   *
   */
  int xmithighprio;

  /**
   *
   * enable pseudo real-time streaming
   *
   */
  VSX_STREAMFLAGS_T  streamflags;

  /**
   *
   * stream processor and audio / video synchronization configuration
   *
   */
  VSX_STREAM_READER_TYPE_T  avReaderType;

  /**
   *
   * stream from file specific
   *
   */

  /**
   *
   * override input frame rate
   *
   */
  double fps;

  /**
   *
   * loop input file
   *
   */
  int loop;

  /**
   *
   * float representation of starting file offset in seconds
   *
   */
  const char *strstartoffset; 

  /**
   *
   * float representation of max streamed file duration in seconds
   *
   */
  const char *strdurationmax; 

  /**
   *
   * raw eth dev (non socket) based output
   *
   */
  int rawxmit; 

  /**
   *
   * SDP output file path
   *
   */
  const char *sdpoutpath;

  /**
   *
   * Input recording specific
   *
   */
 
  /**
   *
   * MPEG-2 TS recording output directory
   *
   */
  const char *dir;

  /**
   *
   * Recording flag to overwrite recorded output file
   *
   */
  int overwritefile;

  /**
   *
   * use promiscuous interface setting
   *
   */
  int promisc;

  /**
   *
   *
   * Authentication token id
   *
   */
  const char *tokenid;

  /**
   *
   * application internal private data
   *
   */
  void *pPrivate;

} VSXLIB_STREAM_PARAMS_T;

/**
 *
 *  Setup and initialize vsxlib.  This function will initialize
 *  the VSXLIB_STREAM_PARAMS_T argument and set any default values.  
 *  Caller can set / override any VSXLIB_STREAM_PARAMS_T values
 *  before calling any vsxlib_xxx operations.
 *
 *  returns: 
 *  VSX_RC_ERROR      
 *  VSX_RC_OK          
 *
 */
VSX_RC_T vsxlib_open(VSXLIB_STREAM_PARAMS_T *pParams);

/**
 *
 *  Close vsxlib.  Must be called to close any resources associated
 *  with a corresponding call to vsxlib_open.
 *
 *  returns: 
 *  VSX_RC_ERROR      
 *  VSX_RC_OK          
 *
 */
VSX_RC_T vsxlib_close(VSXLIB_STREAM_PARAMS_T *pParams);

/**
 *
 *  Passes a single raw video frame to be processed by the framework. 
 *
 *  returns: 
 *  VSX_RC_ERROR      
 *  VSX_RC_OK          
 *
 */
VSX_RC_T vsxlib_onVidFrame(VSXLIB_STREAM_PARAMS_T *pParams,
                             const unsigned char *pData, unsigned int len);

/**
 *
 *  Passes a series of raw audio samples to be processed by the framework. 
 *
 *  returns: 
 *  VSX_RC_ERROR      
 *  VSX_RC_OK          
 *
 */
VSX_RC_T vsxlib_onAudSamples(VSXLIB_STREAM_PARAMS_T *pParams,
                               const unsigned char *pData, unsigned int len);

/**
 *
 *  Reads a single raw video frame to be displayed.  If arguments pData is NULL 
 *  or len is 0, then the function returns immediately checking if a frame is 
 *  available.  Otherwise, the function will block until a frame is ready or
 *  until vsx_close is called.   
 *
 *  returns: 
 *  VSX_RC_ERROR      - error 
 *  VSX_RC_NOTREADY   - async processor not yet set up, try again later 
 *  VSX_RC_OK         - no frame available.  This is only returned if pData is NULL. 
 *  n > 0  length in bytes of available frame (n will always be 1 if frame is 
 *         available but pData is NULL).
 *
 */
VSX_RC_T vsxlib_readVidFrame(VSXLIB_STREAM_PARAMS_T *pParams,
                               unsigned char *pData, unsigned int len,
                               struct timeval *ptv);

/**
 *
 *  Reads a series of raw audio samples to be played.   If the argument pData 
 *  is NULL then the function returns immediately with the count of 
 *  bytes available to be read.  Otherwise, the function will block 
 *  until len bytes of audio samples are available or until vsx_close is called.  
 *
 *  returns: 
 *  VSX_RC_ERROR      - error 
 *  VSX_RC_NOTREADY   - async processor not yet set up, try again later 
 *  VSX_RC_OK         - no frame available.  This is only returned if pData is NULL. 
 *  n > 0  length in bytes of sample data
 *
 */
VSX_RC_T vsxlib_readAudSamples(VSXLIB_STREAM_PARAMS_T *pParams,
                                 unsigned char *pData, unsigned int len,
                                 struct timeval *ptv);

/** 
 *
 * Extract raw media from container file
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_extractMedia(int extractVid, 
                               int extractAud,
                               const char *in,
                               const char *out,
                               const char *startSec,
                               const char *durationSec);

/**
 *
 * Create an mp4 container format
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_createMp4(const char *in, 
                            const char *out, 
                            double fps, 
                            const char *avsyncin,
                            int esdsObjTypeIdxPlus1);

/**
 * 
 * Capture and record an input
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_captureNet(VSXLIB_STREAM_PARAMS_T *pParams);

/**
 *
 *  Produce an output stream
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_stream(VSXLIB_STREAM_PARAMS_T *pParams);


/**
 *
 *  Updates a stream already in progress.  The stream must have 
 *  been started via vsxlib_stream.  Only strxcode parameters
 *  or timeout values may be updated.
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_update(VSXLIB_STREAM_PARAMS_T *pParams);

typedef struct VSXLIB_STREAM_REPORT {
  uint32_t                     reportedPktsLost;
  uint32_t                     reportedPktsLostTot;
  uint32_t                     reportedPkts;
  uint64_t                     reportedPktsTot;

  uint32_t                     pktsSent;
  uint32_t                     bytesSent;
  uint64_t                     pktsSentTot;
  uint64_t                     bytesSentTot;

} VSXLIB_STREAM_REPORT_T;

typedef int (* VSXLIB_CB_STREAM_REPORT) (void *, VSXLIB_STREAM_REPORT_T *);

/**
 *
 *  Registers a report callback which is periodically invoked to update
 *  transmission and if available, receiver statistics.
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_registerReportCb(VSXLIB_STREAM_PARAMS_T *pParams,
                                  VSXLIB_CB_STREAM_REPORT cbStreamReport,
                                  void *pCbData);

/**
 *
 *  Returns the SDP describing the configured audio / video session 
 *  context.  The SDP contents are copied to strSdp and *plen is set
 *  to the string length including terminating NULL.
 *
 *  returns: 
 *  VSX_RC_ERROR  
 *  VSX_RC_OK
 *
 */
VSX_RC_T vsxlib_showSdp(VSXLIB_STREAM_PARAMS_T *pParam,
                          char *strSdp, unsigned int *plen);

#endif // __VSX_LIB_H__
