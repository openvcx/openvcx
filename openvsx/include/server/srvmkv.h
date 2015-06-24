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


#ifndef __SRV_MKV_H__
#define __SRV_MKV_H__

#include "formats/mkv_types.h"

#define MKVLIVE_DELAY_DEFAULT            1.0f

#define MKV_SEGINFO_DURATION_DEFAULT     86400000.0f

enum MKVSRV_STATE {
  MKVSRV_STATE_ERROR               = -1,
  MKVSRV_STATE_INVALID             = 0,
};

typedef int (* MKVSRV_CB_WRITEDATA) (void *, const unsigned char *, unsigned int);

typedef struct MKVSRV_CTXT_T {
  SOCKET_DESCR_T         *pSd;
  HTTP_REQ_T             *pReq;
  int                     senthdr;

  FILE_STREAM_T           recordFile;
  int                     overwriteFile;
  //unsigned int            recordMaxFileSz;
  OUTFMT_CFG_T           *pRecordOutFmt;

  FILE_OFFSET_T           totXmit;
  MKVSRV_CB_WRITEDATA     cbWrite;
  char                   *writeErrDescr;

  FLV_BYTE_STREAM_T       out;
  enum MKVSRV_STATE       state;
  CODEC_AV_CTXT_T         av;
  float                   avBufferDelaySec;
  int                    *pnovid;
  int                    *pnoaud;

  float                   faoffset_mkvpktz;
  unsigned int            requestOutIdx; // xcode outidx

  MKV_HEADER_T            mkvHdr;
  MKV_SEGMENT_T           mkvSegment;
  MKV_TRACK_T             mkvTracks[2];

  uint64_t                lastPts; 
  int                     havePtsStart;
  uint64_t                ptsStart; 
  uint64_t                lastPtsClusterStart; 
  uint64_t                clusterDurationMs;
  uint64_t                clusterTimeCode;
  unsigned int            clusterSz;

} MKVSRV_CTXT_T;

int mkvsrv_init(MKVSRV_CTXT_T *pMkv, unsigned int vidTmpFrameSz);
int mkvsrv_sendHttpResp(MKVSRV_CTXT_T *pMkv, const char *contentType);
void mkvsrv_close(MKVSRV_CTXT_T *pMkv);
int mkvsrv_record_start(MKVSRV_CTXT_T *pMkvCtxt, STREAMER_OUTFMT_T *pLiveFmt);
int mkvsrv_record_stop(MKVSRV_CTXT_T *pMkvCtxt);

int mkvsrv_addFrame(void *, const OUTFMT_FRAME_DATA_T *pFrame);


#endif // __SRV_MKV_H__
