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


#ifndef __STREAM_AV_H__
#define __STREAM_AV_H__

#include "stream_rtp.h"
#include "stream_net.h"
#include "stream_pktz.h"

#define AV_MAX_PROGS                     2

typedef struct AV_PROG {
  STREAM_NET_T            stream;
  void                   *pCbData;
  uint16_t                streamId; // ISO/IEC 13818-1 table 2-18
  uint16_t                streamType; // ISO/IEC 13818-1 table 2-29
  int                     pesLenZero; // TODO: - this is only for a packeterizer
  int                     haveDts;    // include decoder time stamp
  int64_t                 avoffset_pktz;
  OUTFMT_FRAME_DATA_T     frameData;

  uint32_t                progId;
  uint16_t                lastStreamType;
  uint16_t                inactive;
  uint64_t                frameId;    // total frame counter for timestamp computation
  unsigned int            frameWrIdx;
  uint64_t                lastHzIn;     //  90Khz input ts equivalent of frameId relative to start
  uint64_t                lastHzOut;    //  90Khz output ts equivalent of frameId relative to start
  //uint64_t               *plastHz;
  uint64_t                lastHzOffsetJmp;
  int                     haveStartHzIn;
  uint64_t                startHzIn;
  int                     haveStartHzOut;
  uint64_t                startHzOut;
  AV_OFFSET_T             *pavs[IXCODE_VIDEO_OUT_MAX];  // time affecting PES packetization ordering
                                                        // in 90KHz

  TIME_VAL                 tmLastFrCheck;
  enum STREAM_NET_ADVFR_RC lastRc;
  int                      dtx;

  int                     noMoreData;
  int                     pipUpsampleFr;
  //void                   *pAv; // points back to STREAM_RTP_MP2TS
  struct STREAM_AV        *pAv; 
  STREAM_XCODE_DATA_T    *pXcodeData;

  PKTZ_CBSET_T            pktz[MAX_PACKETIZERS * IXCODE_VIDEO_OUT_MAX];
  unsigned int            numpacketizers;

  STREAMER_OUTFMT_LIST_T *pLiveFmts;

} AV_PROG_T;


typedef struct STREAM_AV {
  AV_PROG_T                     progs[AV_MAX_PROGS];
  unsigned int                  numProg;
  VIDEO_DESCRIPTION_GENERIC_T  *pvidProp;
  TIME_VAL                      tmStartTx;
  int                           haveTmStartTx;
  unsigned int                  msDelayTx;
  uint64_t                      lastHzX;
  int                           haveStartHzX;
  uint64_t                      startHzX;
  double                       *plocationMs;
  double                        locationMsStart;
  int                           prepareCalled;
  int                           useInputTiming;
  STREAM_SDP_ACTION_T           *pSdpActions[IXCODE_VIDEO_OUT_MAX];
  VSX_STREAMFLAGS_T             streamflags;
  VSX_STREAM_READER_TYPE_T      avReaderType;

  uint64_t                      dbglastptsvid;
  uint64_t                      dbglastptsaud;

} STREAM_AV_T;


int stream_av_init(STREAM_AV_T *pAv, const PKTZ_CBSET_T *pPktz0,
                   const PKTZ_CBSET_T *pPktz1, unsigned int numpacketizers);
int stream_av_reset(STREAM_AV_T *pAv, int fullReset);
int stream_av_close(STREAM_AV_T *pAv);

int stream_av_preparepkt(void *pAv);
int stream_av_cansend(void *pAv);


#endif // __STREAM_AV_H__
