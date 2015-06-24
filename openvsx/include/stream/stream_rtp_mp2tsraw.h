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


#ifndef __STREAM_RTP_MP2TS_RAW_H__
#define __STREAM_RTP_MP2TS_RAW_H__

#include "stream/stream_rtp.h"
#include "formats/mp2pes.h"
#include "formats/mp2ts.h"
#include "codecs/mpeg2.h"
#include "formats/filetype.h"


typedef struct STREAM_MP2TS_PROGDATA {
  union {
    MPEG2_SEQ_HDR_T      mpeg2;
  } u;
} STREAM_MP2TS_PROGDATA_T;

typedef struct STREAM_RTP_MP2TS_REPLAY {
  STREAM_DATASRC_T              cfg;
  STREAM_RTP_MULTI_T            rtpMulti;
  unsigned int                  pktsSent;
  uint32_t                      filePcrOffset;
  int64_t                       localPcrStart;
  int64_t                       pcrStart;
  int64_t                       pcrPrev;
  int64_t                       pcrPrevDuration;
  unsigned int                  pktCntPrevPcr;
  double                        pktGapMs;
  double                        pktGapMultiplier;
  TIME_VAL                      tmLastTx;
  TIME_VAL                      tmLastPcrTx;

  TIME_VAL                      tmMinPktInterval;
  TIME_VAL                      latencyMs;

  int64_t                       cachedPcr;
  unsigned char                 buf[MP2TS_BDAV_LEN];

  uint8_t                       prevPmtVersion;
  MP2_PAT_T                     pat;
  MP2_PMT_T                     pmt;
  unsigned int                  mp2tslen;
  int                           replayUseLocalTiming;
  STREAM_MP2TS_PROGDATA_T       vidData;

  // status updates
  MP2TS_STREAM_CHANGE_T        *pCurStreamChange;
  VIDEO_DESCRIPTION_GENERIC_T  *pVidProp;
  double                       *plocationMs;
  double                        locationMsStart;


} STREAM_RTP_MP2TS_REPLAY_T;


int stream_rtp_mp2tsraw_cbgetfiledata(void *p, unsigned char *pbuf, unsigned int len,
                                 PKTQ_EXTRADATA_T *pXtra, unsigned int *pNumOverwritten);
int stream_rtp_mp2tsraw_cbhavefiledata(void *p);
int stream_rtp_mp2tsraw_cbresetfiledata(void *p);

int stream_rtp_mp2tsraw_init(STREAM_RTP_MP2TS_REPLAY_T *pRtpMp2ts);
int stream_rtp_mp2tsraw_reset(STREAM_RTP_MP2TS_REPLAY_T *pRtpMp2ts);
int stream_rtp_mp2tsraw_close(void *pRtpMp2ts);
int stream_rtp_mp2tsraw_preparepkt(void *pRtpHMp2ts);
int stream_rtp_mp2tsraw_cansend(void *pRtpMp2ts);


#endif //__STREAM_RTP_MP2TS_RAW_H__
