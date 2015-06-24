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


#ifndef __STREAM_PKTZ_H263_H__
#define __STREAM_PKTZ_H263_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_H263        "H263 (RFC 2190)"
#define STREAM_RTP_DESCRIPTION_H263_PLUS   "H263+ (RFC 4629)"

typedef struct PKTZ_INIT_PARAMS_H263_T {
  PKTZ_INIT_PARAMS_T            common;
  SDP_DESCR_VID_T              *pSdp;
  XC_CODEC_TYPE_T               codecType;                                           
} PKTZ_INIT_PARAMS_H263_T;


typedef struct PKTZ_H263 {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;

  const SDP_DESCR_VID_T        *pSdp;
  int                           isInitFromSdp;
  XC_CODEC_TYPE_T               codecType;                                           
} PKTZ_H263_T;


int stream_pktz_h263_init(void *pPktzH263, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_h263_reset(void *pPktzH263, int fullReset, unsigned int progIdx);
int stream_pktz_h263_close(void *pPktzH263);
int stream_pktz_h263_addframe(void *pPktzH263, unsigned int progIdx);

#endif // __STREAM_PKTZ_H263_H__

