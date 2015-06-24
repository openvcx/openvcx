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


#ifndef __STREAM_PKTZ_MPG4V_H__
#define __STREAM_PKTZ_MPG4V_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_MPG4V       "MPEG-4 (RFC 3016)"

typedef struct PKTZ_INIT_PARAMS_MPG4V_T {
  PKTZ_INIT_PARAMS_T            common;
  SDP_DESCR_VID_T              *pSdp;
} PKTZ_INIT_PARAMS_MPG4V_T;


typedef struct PKTZ_MPG4V {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;

  const SDP_DESCR_VID_T        *pSdp;
  int                           isInitFromSdp;
} PKTZ_MPG4V_T;


int stream_pktz_mpg4v_init(void *pPktzMpg4v, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_mpg4v_reset(void *pPktzMpg4v, int fullReset, unsigned int progIdx);
int stream_pktz_mpg4v_close(void *pPktzMpg4v);
int stream_pktz_mpg4v_addframe(void *pPktzMpg4v, unsigned int progIdx);

#endif // __STREAM_PKTZ_MPG4V_H__

