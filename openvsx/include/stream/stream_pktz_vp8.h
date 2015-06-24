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


#ifndef __STREAM_PKTZ_VP8_H__
#define __STREAM_PKTZ_VP8_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_VP8   "VP8 (IETF VP8-08)"


typedef struct PKTZ_INIT_PARAMS_VP8_T{
  PKTZ_INIT_PARAMS_T            common;
  SDP_DESCR_VID_T              *pSdp;
} PKTZ_INIT_PARAMS_VP8_T;


typedef struct PKTZ_VP8V {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;
  const SDP_DESCR_VID_T        *pSdp;
  int                           isInitFromSdp;
  uint32_t                      rtpTs0;
  unsigned int                  pictureId;

} PKTZ_VP8_T;


int stream_pktz_vp8_init(void *pPktzVp8, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_vp8_reset(void *pPktzVp8, int fullReset, unsigned int progIdx);
int stream_pktz_vp8_close(void *pPktzVp8);
int stream_pktz_vp8_addframe(void *pPktzVp8, unsigned int progIdx);

#endif // __STREAM_PKTZ_VP8_H__

