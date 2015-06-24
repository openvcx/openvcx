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


#ifndef __CAPTURE_PKT_RTMP_H__
#define __CAPTURE_PKT_RTMP_H__


#include "capture.h"
#include "codecs/avcc.h"
#include "formats/rtmp_parse.h"


enum RTMP_HANDSHAKE_STATE {
  RTMP_HANDSHAKE_STATE_UNKNOWN    = 0,
  RTMP_HANDSHAKE_STATE_INPROGRESS = 1,
  RTMP_HANDSHAKE_STATE_DONE       = 2
};


typedef struct RTMP_TRANS_STATE {
  uint8_t                   client;
  uint8_t                   server;
  enum RTMP_HANDSHAKE_STATE handshake;
  unsigned int              handshakeIdx;
} RTMP_TRANS_STATE_T;

typedef struct RTMP_RECORD {
  RTMP_CTXT_T              ctxt;
  RTMP_TRANS_STATE_T       state;
  BYTE_STREAM_T            bsIn;
  uint64_t                 bytesTot;
  uint64_t                 prevPktOffset;

  FILE_STREAM_T            fsVid;
  FILE_STREAM_T            fsAud;
  int                      overWriteFile;
  const char              *outputDir;
  const char              *pDescr; 
} RTMP_RECORD_T;


int rtmp_record_init(RTMP_RECORD_T *pRtmp);
void rtmp_record_close(RTMP_RECORD_T *pRtmp);

int cbOnPkt_rtmp(void *pArg, const COLLECT_STREAM_PKTDATA_T *pPkt);


#endif // __CAPTURE_PKT_RTMP_H__
