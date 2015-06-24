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


#ifndef __SDPUTIL_H__
#define __SDPUTIL_H__

#include "sdp.h"

#define SDP_CODEC_PARAM_FLAGS_PKTZMODE    0x01
#define SDP_CODEC_PARAM_FLAGS_CHANNELS    0x02

typedef struct SDP_CODEC_PARAM {

  union {
    PKTZ_H264_MODE_T pktzMode;
    int              channels;
  } u;

  int flags;

} SDP_CODEC_PARAM_T;

int sdputil_initsrtp(SDP_STREAM_SRTP_T *pSdp, const SRTP_CTXT_T *pSrtp);

int sdputil_init(SDP_DESCR_T *pSdp,
                 uint8_t payloadType,
                 unsigned int clockRateHz,
                 XC_CODEC_TYPE_T codecType,
                 const char *pDstHost,
                 uint16_t dstPort, 
                 uint16_t dstPortRtcp, 
                 const SRTP_CTXT_T *pSrtp,
                 const DTLS_CFG_T *pDtlsCfg,
                 const STUN_REQUESTOR_CFG_T *pStunCfg,
                 const SDP_CODEC_PARAM_T *pCodecSpecific,
                 const FRAME_RATE_T *pFps,
                 const VID_ENCODER_FBREQUEST_T *pFbReq);



#endif //__SDPUTIL_H__
