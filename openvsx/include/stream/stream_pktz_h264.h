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


#ifndef __STREAM_PKTZ_H264_H__
#define __STREAM_PKTZ_H264_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_H264     "H.264 (RFC 3984)"

typedef enum PKTZ_H264_MODE {
  PKTZ_H264_MODE_NOTSET    = 0,
  PKTZ_H264_MODE_0         = 1,
  PKTZ_H264_MODE_1         = 2,
  PKTZ_H264_MODE_2         = 3
} PKTZ_H264_MODE_T;

typedef struct PKTZ_INIT_PARAMS_H264_T {
  PKTZ_INIT_PARAMS_T            common;
  SDP_DESCR_VID_T              *pSdp;
} PKTZ_INIT_PARAMS_H264_T;

typedef struct PKTZ_H264 {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;
  const SDP_DESCR_VID_T        *pSdp;
  int                           isInitFromSdp;
  uint32_t                      rtpTs0;
  PKTZ_H264_MODE_T              mode;
  int                           consecErrors;
} PKTZ_H264_T;


int stream_pktz_h264_init(void *pPktzH264, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_h264_reset(void *pPktzH264, int fullReset, unsigned int progIdx);
int stream_pktz_h264_close(void *pPktzH264);
int stream_pktz_h264_addframe(void *pPktzH264, unsigned int progIdx);

#endif // __STREAM_PKTZ_H264_H__

