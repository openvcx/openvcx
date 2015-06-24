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


#ifndef __STREAM_PKTZ_OPUS_H__
#define __STREAM_PKTZ_OPUS_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_OPUS        "OPUS"
#define STREAM_RTP_OPUS_CLOCKHZ            48000


typedef struct PKTZ_INIT_PARAMS_OPUS_T {
  PKTZ_INIT_PARAMS_T            common;
  SDP_DESCR_AUD_T              *pSdp;
  unsigned int                  compoundSampleMs; 
} PKTZ_INIT_PARAMS_OPUS_T;


typedef struct PKTZ_OPUS {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;

  const SDP_DESCR_AUD_T        *pSdp;
  int                           isInitFromSdp;
  unsigned int                 compoundSampleMs; 

  uint64_t                     pts;
  uint32_t                     rtpTs0;
  //unsigned int                 queuedFrames;
  //BYTE_STREAM_T                bsPkt;
  //unsigned char                buf[2048];
} PKTZ_OPUS_T;


int stream_pktz_opus_init(void *pPktzOpus, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_opus_reset(void *pPktzOpus, int fullReset, unsigned int progIdx);
int stream_pktz_opus_close(void *pPktzOpus);
int stream_pktz_opus_addframe(void *pPktzOpus, unsigned int progIdx);

#endif // __STREAM_PKTZ_OPUS_H__

