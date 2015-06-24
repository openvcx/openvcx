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


#ifndef __STREAM_NET_PES_H__
#define __STREAM_NET_PES_H__

#include "stream_net.h"
#include "util/fileutil.h"
#include "util/pktqueue.h"
#include "xcode/xcode_types.h"
#include "stream_rtp.h"


typedef int (*CBSTREAM_GETDATA)(void *, unsigned char *, unsigned int, 
                                PKTQ_EXTRADATA_T *, unsigned int *);
typedef int (*CBSTREAM_HAVEDATA)(void *);
typedef int (*CBSTREAM_RESETDATA)(void *);


typedef struct STREAM_DATASRC {
  CBSTREAM_GETDATA       cbGetData;
  CBSTREAM_HAVEDATA      cbHaveData;
  CBSTREAM_RESETDATA     cbResetData;
  void                  *pCbData;
  MP2TS_CONTAINER_T     *pContainer;
} STREAM_DATASRC_T;

typedef struct STREAM_PES_DATA {
  void                          *pCbData;
  PKTQUEUE_T                    *pQueue;
  const STREAM_DATASRC_T        *pDataSrc;
  uint32_t                       lastQWrIdx;
  uint16_t                      *pOutStreamType; 
  uint32_t                       numFrames; // includes most recent partial frame data
  STREAM_XCODE_DATA_T           *pXcodeData;
  int                            curPesDecodeErr;
  MP2TS_PKT_TIMING_T             curPesTm; // lastest timing obtained by writer into queue
  MP2TS_PKT_TIMING_T             prevPesTm; // prior timing obtained by writer into queue
  MP2_PES_PROG_T                *pProg;     // pointer to STREAM_PES_T::progs
  int                            inactive;
  VSX_STREAMFLAGS_T              streamflags;
  int                           *pHaveSeqStart;
  void                          *pPes;  // points back to STREAM_PES_T or STREAM_NET_AV_T
} STREAM_PES_DATA_T;

typedef struct STREAM_PES {
  STREAM_PES_DATA_T               vid;
  STREAM_PES_DATA_T               aud;
  STREAM_PES_DATA_T              *pPcrData;
  const STREAM_DATASRC_T         *pDataSrc;
  unsigned char                   buf[MP2TS_BDAV_LEN];
  unsigned int                    tsPktLen;
  MP2_PAT_T                       pat;
  MP2_PMT_T                       pmt;
  uint16_t                        pmt_haveVid;
  uint16_t                        pmt_haveAud;
  MP2_PES_PROG_T                  progs[2];
  uint8_t                         prevPmtVersion;
  int64_t                         audAvSyncHz;
  STREAM_XMIT_ACTION_T           *pXmitAction;
  MP2TS_CONTAINER_T              *pContainer;
} STREAM_PES_T;

typedef struct STREAM_PES_FILE_CBDATA {
  FILE_STREAM_T                  *pFile;
  int                             liveFile;
  struct tm                       tmLastSzChange;
  int                             numLiveFileRdErr;
} STREAM_PES_FILE_CBDATA_T;



void stream_net_pes_init(STREAM_NET_T *pNet);
float stream_net_pes_jump(STREAM_PES_DATA_T *pData, float fStartSec, int syncframe);
int stream_mp2ts_pes_cbgetfiledata(void *p, unsigned char *pData, unsigned int len,
                                   PKTQ_EXTRADATA_T *pTm, unsigned int *pNumOverwritten);
int stream_mp2ts_pes_cbhavefiledata(void *p);
int stream_mp2ts_pes_cbresetfiledata(void *p);


#endif // __STREAM_NET_H264_H__
