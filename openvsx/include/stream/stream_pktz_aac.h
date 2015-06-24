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


#ifndef __STREAM_PKTZ_AAC_H__
#define __STREAM_PKTZ_AAC_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_AAC         "AAC-hbr (RFC 3640)"

#define PKTZ_AAC_MAX_QUEUED_FRAMES       32

typedef struct PKTZ_AAC_QUEUED_FRAMES {
  unsigned int                  idxBuf;
  unsigned int                  szBuf;
  unsigned int                  idxFrame;
  unsigned char                 buf[FRAMESZ_AUDQUEUE_SZFRAME_DFLT];
} PKTZ_AAC_QUEUED_FRAMES_T;

typedef struct PKTZ_INIT_PARAMS_AAC_T {
  PKTZ_INIT_PARAMS_T            common;
  unsigned int                  frameDeltaHz;
  SDP_DESCR_AUD_T              *pSdp;
} PKTZ_INIT_PARAMS_AAC_T;

typedef struct PKTZ_AAC {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;
  //float                         favoffsetrtp;
  //unsigned int                  tsOffsetHz;


  unsigned int                  frameDeltaHz;
  unsigned int                  hbrIndexLen;
  int                           stripAdtsHdr;
  const SDP_DESCR_AUD_T        *pSdp;
  int                           isInitFromSdp;
  uint32_t                      rtpTs0;
  PKTZ_AAC_QUEUED_FRAMES_T      qFrames;
} PKTZ_AAC_T;


int stream_pktz_aac_init(void *pPktzH264, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_aac_reset(void *pPktzH264, int fullReset, unsigned int progIdx);
int stream_pktz_aac_close(void *pPktzH264);
int stream_pktz_aac_addframe(void *pPktzH264, unsigned int progIdx);

#endif // __STREAM_PKTZ_AAC_H__

