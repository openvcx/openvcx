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


#ifndef __SERVER_CONF_H__
#define __SERVER_CONF_H__

#include "unixcompat.h"

#ifndef WIN32

#include <arpa/inet.h>

#endif // WIN32


#define SRV_CONF_KEY_AVCTHUMB              "thumb"
#define SRV_CONF_KEY_AVCTHUMBLOG           "thumbLog"
#define SRV_CONF_KEY_CAP_IDLETMT_1STPKT_MS "captureTimeoutFirst"
#define SRV_CONF_KEY_CAP_IDLETMT_MS        "captureTimeout"
#define SRV_CONF_KEY_CHANNELCHANGER        "channelChanger"
#define SRV_CONF_KEY_DBDIR                 "dbDir"
#define SRV_CONF_KEY_DISABLEDB             "dbDisable"
#define SRV_CONF_KEY_DISABLEROOTLIST       "disableRootListing"
#define SRV_CONF_KEY_DEVICE_CONFIG         "deviceConfig"

#define SRV_CONF_KEY_FIR_ENCODER           "FIREncoder"
#define SRV_CONF_KEY_FIR_RECV_VIA_RTCP     "FIRRTCPInputHandler"
#define SRV_CONF_KEY_FIR_RECV_FROM_CONNECT "FIREncoderFromRemoteConnect"
#define SRV_CONF_KEY_FIR_RECV_FROM_REMOTE  "FIREncoderFromRemoteMessage"
#define SRV_CONF_KEY_FIR_SEND_FROM_LOCAL   "FIRSendFromRemoteConnect"
#define SRV_CONF_KEY_FIR_SEND_FROM_REMOTE  "FIRSendFromRemoteMessage"
#define SRV_CONF_KEY_FIR_SEND_FROM_DECODER "FIRSendFromDecoder"
#define SRV_CONF_KEY_FIR_SEND_FROM_CAPTURE "FIRSendFromInputCapture"
#define SRV_CONF_KEY_NACK_RTCP_REQUEST     "NACKRtcpRequestRetransmit"
#define SRV_CONF_KEY_NACK_RTP_RETRANSMIT   "NACKRtpRetransmit"
#define SRV_CONF_KEY_APPREMB_RTCP_NOTIFY   "REMBRtcpNotify"
#define SRV_CONF_KEY_APPREMB_RTCP_MAXRATE  "REMBRtcpMaxRateKbps"
#define SRV_CONF_KEY_APPREMB_RTCP_MINRATE  "REMBRtcpMinRateKbps"
#define SRV_CONF_KEY_APPREMB_RTCP_FORCE    "REMBRtcpForceAdjustment"

#define SRV_CONF_KEY_FRAME_THIN            "FrameThin"
#define SRV_CONF_KEY_FLVLIVE               "FLVLive"
#define SRV_CONF_KEY_FLVLIVEMAX            "FLVLiveMax"
#define SRV_CONF_KEY_FRAUDQSLOTS           "audioFrameQueueSlots"
#define SRV_CONF_KEY_FRVIDQSLOTS           "videoFrameQueueSlots"
#define SRV_CONF_KEY_HOME                  "home"
#define SRV_CONF_KEY_MOOFLIVE              "dash"
#define SRV_CONF_KEY_MOOFDIR               "dashDir"
#define SRV_CONF_KEY_MOOFLIVEMAX           "dashLiveMax"
#define SRV_CONF_KEY_MOOFFILEPREFIX        "dashFilePrefix"
#define SRV_CONF_KEY_MOOFINDEXCOUNT        "dashIndexCount"
#define SRV_CONF_KEY_MOOFURLPREFIX         "dashUrlHost"
#define SRV_CONF_KEY_MOOFNODELETE          "dashNoDelete"
#define SRV_CONF_KEY_MOOFMINDURATION       "dashMinDuration"
#define SRV_CONF_KEY_MOOFMAXDURATION       "dashMaxDuration"
#define SRV_CONF_KEY_MOOFFRAGDURATION      "dashFragmentDuration"
#define SRV_CONF_KEY_MOOFUSEINIT           "dashUseInit"
#define SRV_CONF_KEY_HTTPLIVE              "httplive"
#define SRV_CONF_KEY_HTTPLIVEDIR           "httpliveDir"
#define SRV_CONF_KEY_HTTPLIVEDURATION      "httpliveChunk"
#define SRV_CONF_KEY_HTTPLIVEFILEPREFIX    "httpliveFilePrefix"
#define SRV_CONF_KEY_HTTPLIVEMAX           "httpliveMax"
#define SRV_CONF_KEY_HTTPLIVEURLPREFIX     "httpliveUrlHost"
#define SRV_CONF_KEY_HTTPLIVEINDEXCOUNT    "httpliveIndexCount"
#define SRV_CONF_KEY_HTTPLIVENODELETE      "httpliveNoDelete"
#define SRV_CONF_KEY_IGNOREDIRPRFX         "ignoreDirPrefix"
#define SRV_CONF_KEY_IGNOREFILEPRFX        "ignoreFilePrefix"
#define SRV_CONF_KEY_INTERFACE             "interface"
#define SRV_CONF_KEY_IPADDRHOST            "localHost"
#define SRV_CONF_KEY_LICFILE               "license"
#define SRV_CONF_KEY_LISTEN                "listen"
#define SRV_CONF_KEY_LIVE                  "live"
#define SRV_CONF_KEY_LIVEMAX               "livemax"
#define SRV_CONF_KEY_LIVEPWD               "password"
#define SRV_CONF_KEY_ENABLESYMLINK         "followSymLink"
#define SRV_CONF_KEY_LOGFILE               "logFile"
#define SRV_CONF_KEY_LOGFILE_MAXSZ         "logFileMaxSize"
#define SRV_CONF_KEY_LOGFILE_COUNT         "logFileMaxCount"
#define SRV_CONF_KEY_MAXCONN               "maxConn"
#define SRV_CONF_KEY_MEDIADIR              "media"
#define SRV_CONF_KEY_MKVLIVEMAX            "MKVLiveMax"
#define SRV_CONF_KEY_MKVLIVE               "MKVLive"
#define SRV_CONF_KEY_M2T_PCR_MAX_MS        "MPEG2TSPCRIntervalMs"
#define SRV_CONF_KEY_M2T_PAT_MAX_MS        "MPEG2TSPATIntervalMs"
#define SRV_CONF_KEY_M2T_XMITFLUSHPKTS     "MPEG2TSMinimumPacketSize"
#define SRV_CONF_KEY_M2TPKTQSLOTS          "MPEG2TSCaptureQueueSlots"
#define SRV_CONF_KEY_PROPFILE              "userCfg"
#define SRV_CONF_KEY_RTMPLIVE              "RTMPLive"
#define SRV_CONF_KEY_RTMPLIVEMAX           "RTMPLiveMax"
#define SRV_CONF_KEY_RTMPQSLOTS            "RTMPQueueSlots"
#define SRV_CONF_KEY_RTMPQSZSLOT           "RTMPQueueSlotSize"
#define SRV_CONF_KEY_RTMPQSZSLOTMAX        "RTMPQueueSlotSizeMax"
#define SRV_CONF_KEY_RTPMAX                "RTPMax"
#define SRV_CONF_KEY_RTPMAXPLAYOUTDELAY    "RTPMaxPlayoutDelay"
#define SRV_CONF_KEY_RTPMAXVIDPLAYOUTDELAY "RTPMaxVideoPlayoutDelay"
#define SRV_CONF_KEY_RTPMAXAUDPLAYOUTDELAY "RTPMaxAudioPlayoutDelay"

#define SRV_CONF_KEY_RTCP_RR_INTERVAL       "RTCPReceiverReportInterval"
#define SRV_CONF_KEY_RTCP_SR_INTERVAL       "RTCPSenderReportInterval"
#define SRV_CONF_KEY_RTCP_MCAST_REPLIES     "RTCPReplyToMulticastStream"

#define SRV_CONF_KEY_RTSPLIVE              "RTSPLive"
#define SRV_CONF_KEY_RTSPLIVEMAX           "RTSPLiveMax"
#define SRV_CONF_KEY_RTSPINTERQSLOTS       "RTSPInterleavedQueueSlots"
#define SRV_CONF_KEY_RTSPINTERQSZSLOT      "RTSPInterleavedQueueSlotSize"
#define SRV_CONF_KEY_RTSPINTERQSZSLOTMAX   "RTSPInterleavedQueueSlotSizeMax"
#define SRV_CONF_KEY_RTSPRTCPREFRESH       "RTSPRefreshTimeoutViaRTCP"
#define SRV_CONF_KEY_RTSPSESSIONTMT        "RTSPSessionTimeout"
#define SRV_CONF_KEY_RTSPLOCALRTPPORT      "RTSPLocalRTPPort"
#define SRV_CONF_KEY_RTSPFORCETCPUALIST    "RTSPForceTCPUserAgents"
#define SRV_CONF_KEY_SSLCERTPATH           "SSLCertificate"
#define SRV_CONF_KEY_SSLPRIVKEYPATH        "SSLPrivateKey"

#define SRV_CONF_KEY_DTLSCERTPATH          "DTLSCertificate"
#define SRV_CONF_KEY_DTLSPRIVKEYPATH       "DTLSPrivateKey"
#define SRV_CONF_KEY_DTLSRTPHANDSHAKETMTMS "DTLSRTPHandshakeTimeoutMs"
#define SRV_CONF_KEY_DTLSRTCPADDITIONALMS  "DTLSRTCPHandshakeAdditionalMs"
#define SRV_CONF_KEY_DTLSRTCPADDITIONALGIVEUPMS "DTLSRTCPHandshakeAdditionalGiveupMs"

#define SRV_CONF_KEY_STREAMSTATSPATH       "outputStatisticsFile"
#define SRV_CONF_KEY_STREAMSTATSINTERVALMS "outputStatisticsIntervalMs"
#define SRV_CONF_KEY_THUMBLARGE            "thumbLarge"
#define SRV_CONF_KEY_THUMBSMALL            "thumbSmall"
#define SRV_CONF_KEY_THROTTLEPREBUF        "throttlePreBuf"
#define SRV_CONF_KEY_THROTTLERATE          "throttleRate"
#define SRV_CONF_KEY_TSLIVE                "tslive"
#define SRV_CONF_KEY_TSLIVEMAX             "tsliveMax"
#define SRV_CONF_KEY_TSLIVEQSLOTS          "tsliveQueueSlots"
#define SRV_CONF_KEY_TSLIVEQSLOTSMAX       "tsliveQueueSlotsMax"
#define SRV_CONF_KEY_TSLIVEQSZSLOT         "tsliveQueueSlotSize"
#define SRV_CONF_KEY_UIPWD                 "password"
#define SRV_CONF_KEY_VERBOSE               "verbose"
#define SRV_CONF_KEY_XCODEARGS             "xcodeArgs"




#endif // __SERVER_CONF_H__
