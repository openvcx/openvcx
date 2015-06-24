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


#ifndef __STREAM_PKTZ_MP2TS_H__
#define __STREAM_PKTZ_MP2TS_H__

#include "stream_pktz.h"

#define STREAM_RTP_DESCRIPTION_MP2TS     "MPEG-2 TS (ISO/IEC 13818-1)"

#define MP2TS_PMT_ID_DEFAULT             66
#define MP2TS_FIRST_PROGRAM_ID           68
#define MP2TS_PCR_INTERVAL_MAX_MS       100
#define MP2TS_PAT_INTERVAL_MAX_MS       250 

#define PKTZ_MP2TS_NUM_PROGS                2

typedef struct PKTZ_INIT_PARAMS_MP2TS_T {
  PKTZ_INIT_PARAMS_T            common;
  int                           usePatDeltaHz;
  int                          *plastPatContIdx;
  int                          *plastPmtContIdx;
  unsigned int                  maxPcrIntervalMs;
  unsigned int                  maxPatIntervalMs;
  int                           firstProgId;
  int                           includeVidH264AUD;
} PKTZ_INIT_PARAMS_MP2TS_T;

typedef struct PKTZ_MP2TS_PROG {
  uint32_t                progId;
  uint16_t                lastStreamType;
  uint16_t               *pStreamType; // ISO/IEC 13818-1 table 2-29
  uint16_t               *pStreamId;   // ISO/IEC 13818-1 table 2-18
  int                    *pHaveDts;    // include decoder time stamp
  int                    *pPesLenZero; // do not fill in pes len field (which has max of 0xffff)
  uint16_t               *pinactive;
  unsigned int            frameRdIdx;
  int64_t                 lastDtsHz;
  uint64_t                lastHz;     //  90Khz equivalent of frameId relative to start
  uint64_t                lastHzPcrOffset;
  int64_t                *pavoffset_pktz;   // A/V sync offset written to each PES
                                           // pts/dts frame field frame on output
                                           // in 90KHz
  int                     prevContIdx;
  unsigned int            pesLenRemaining; // length left in current PES packet
                                           // w/ max being 0xffff
  unsigned int            pesOverflowSz;   // length left of PES packet which overflows
                                           // into subsequent PES packets (only if
                                           // first PES packet > 0xffff)
  int                     noMoreData;
  int                     includeVidH264AUD;
  unsigned int            frameDataActualLen;
  void                   *pPktzMp2ts; // points back to PKTZ_MP2TS 

  const OUTFMT_FRAME_DATA_T *pFrameData;

  VIDEO_DESCRIPTION_GENERIC_T  *pvidProp;

  unsigned int            pmtXtraDataLen;
  unsigned char           pmtXtraData[16];

  uint64_t                dbglastpts;


} PKTZ_MP2TS_PROG_T;

typedef struct PKTZ_MP2TS {
  unsigned int            numProg;
  PKTZ_MP2TS_PROG_T       progs[PKTZ_MP2TS_NUM_PROGS];
  STREAM_RTP_MULTI_T     *pRtpMulti;

  TIME_VAL                tmLastPcr;
  uint64_t                tmLastPcrHz;
  unsigned int            maxPcrIntervalMs;

  TIME_VAL                tmLastPat;
  uint64_t                tmLastPatHz;
  unsigned int            maxPatIntervalMs;
  int                    *plastPatContIdx;
  uint16_t                patVersion;
  uint16_t                patTsId;
  uint32_t                lastPatCrc;
  unsigned char           lastPatBuf[MP2TS_LEN];

  TIME_VAL                tmLastPmt;
  uint64_t                tmLastPmtHz;
  unsigned int            maxPmtIntervalMs;
  int                    *plastPmtContIdx;
  uint32_t                pmtId;
  uint32_t                lastPmtCrc;
  unsigned char           lastPmtBuf[MP2TS_LEN];

  uint16_t                cacheLastPat;
  uint16_t                cacheLastPmt;

  unsigned int            pcrProgIdx;
  uint64_t                clockStartHz;
  TIME_VAL                tmLastTx;
  int64_t                 lastHz;

  int                     usePatDeltaHz; // insert pat/pmt/pcr according to input m2t Hz
                                         // otherwise default to system clock for real time
                                         // streaming
  TIME_VAL                tmMinPktInterval;
  int                     resetRequested;
  int                     lastLowestHzIdx;
  STREAM_XMIT_NODE_T     *pXmitNode; 
  int                     xmitflushpkts;
  PKTZ_XMITPKT            cbXmitPkt;

} PKTZ_MP2TS_T;


int stream_pktz_mp2ts_init(void *pPktzMp2ts, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_mp2ts_reset(void *pPktzMp2ts, int fullReset, unsigned int progIdx);
int stream_pktz_mp2ts_close(void *pPktzMp2ts);
int stream_pktz_mp2ts_addframe(void *pPktzMp2ts, unsigned int progIdx);

#endif // __STREAM_PKTZ_MP2TS_H__

