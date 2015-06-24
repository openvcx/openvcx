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


#ifndef __STREAM_PKTZ_PCM_H__
#define __STREAM_PKTZ_PCM_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_G711_MULAW        "G.711 mulaw"
#define STREAM_RTP_DESCRIPTION_G711_ALAW         "G.711 alaw"


typedef struct PKTZ_INIT_PARAMS_PCM_T {
  PKTZ_INIT_PARAMS_T            common;
  SDP_DESCR_AUD_T              *pSdp;
  unsigned int                  compoundSampleMs; 
} PKTZ_INIT_PARAMS_PCM_T;


typedef struct PKTZ_PCM {
  STREAM_RTP_MULTI_T           *pRtpMulti;
  STREAM_XMIT_NODE_T           *pXmitNode; 
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  const OUTFMT_FRAME_DATA_T    *pFrameData;

  const SDP_DESCR_AUD_T        *pSdp;
  int                           isInitFromSdp;
  unsigned int                  compoundSampleMs; 
  unsigned int                  compoundBytes; 
  unsigned int                  Bps;               // Bytes per sample
  unsigned int                  channels;

  int                           havePts0;
  uint64_t                      pts0;
  uint64_t                      hz;
  unsigned int                  queuedBytes;

} PKTZ_PCM_T;


int stream_pktz_pcm_init(void *pPktz, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_pcm_reset(void *pPktz, int fullReset, unsigned int progIdx);
int stream_pktz_pcm_close(void *pPktz);
int stream_pktz_pcm_addframe(void *pPktz, unsigned int progIdx);

#endif // __STREAM_PKTZ_G711_H__

