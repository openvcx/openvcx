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

#ifndef __STREAM_FB_H__
#define __STREAM_FB_H__

#define FIR_INTERVAL_MS_XMIT_RTCP         1000
#define FIR_INTERVAL_MS_LOCAL_ENCODER     1000

#define APPREMB_INTERVAL_MS_XMIT_RTCP     1000
#define APPREMB_DEFAULT_MAXRATE_BPS       800000
#define APPREMB_DEFAULT_MINRATE_BPS        90000

enum ENCODER_FBREQ_TYPE {
  ENCODER_FBREQ_TYPE_UNKNOWN   = 0,
  ENCODER_FBREQ_TYPE_FIR       = 1,
  ENCODER_FBREQ_TYPE_PLI       = 2
};

enum REQUEST_FB_SRC {
  REQUEST_FB_FROM_UNKNOWN = 0,
  REQUEST_FB_FROM_DECODER = 1,  // From local decoder which needs a keyframe to decode stream
  REQUEST_FB_FROM_CAPTURE = 2,  // From RTP input processor handling packet loss based FIR request
  REQUEST_FB_FROM_REMOTE  = 3,  // From remote receiver, such as RTCP FB FIR
  REQUEST_FB_FROM_LOCAL   = 4   // From local connection request (flv,tslive,etc...) to start processign output
};

typedef struct VID_ENCODER_FBREQUEST {

  FIR_CFG_T                    firCfg;
  //struct CAPTURE_FBARRAY      *pCaptureFbArray;

  TIME_VAL                     tmLastFIRRcvd;
  TIME_VAL                     tmLastFIRProcessed;
  TIME_VAL                     tmLastPLIRcvd;
  TIME_VAL                     tmLastPLIProcessed;

  int                          nackRtpRetransmit;
  int                          nackRtcpSend;
  int                          apprembRtcpSend;
  int                          apprembRtcpSendMaxRateBps;
  int                          apprembRtcpSendMinRateBps;
  int                          apprembRtcpSendForceAdjustment;

} VID_ENCODER_FBREQUEST_T;

struct STREAMER_CFG;

int streamer_requestFB(struct STREAMER_CFG *pStreamerCfg, unsigned int outidx,
                     enum ENCODER_FBREQ_TYPE fbReqType, unsigned int msDelay, enum REQUEST_FB_SRC requestSrc);

int capture_requestFB(VID_ENCODER_FBREQUEST_T *pFbReq, enum ENCODER_FBREQ_TYPE fbReqType,
                      enum REQUEST_FB_SRC requestSrc);


#endif // __STREAM_FB_H__
