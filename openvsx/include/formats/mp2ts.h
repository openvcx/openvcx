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


#ifndef __MP2TS_H__
#define __MP2TS_H__

#ifdef WIN32
#include "unixcompat.h"
#endif // WIN32

#include <stdio.h>
#include "filetype.h"
#include "mp2pes.h"
#include "pktqueue.h"



#define MP2TS_LEN                         188
#define MP2TS_BDAV_LEN                    192

#define MP2TS_SYNCBYTE_VAL                0x47

#define MP2TS_HDR_MAXPID                  0x1fff

#define MP2TS_HDR_SYNCBYTE                0xff000000 
#define MP2TS_HDR_TSERROR                 0x00800000
#define MP2TS_HDR_PAYLOAD_UNIT_START      0x00400000
#define MP2TS_HDR_TS_PRIORITY             0x00200000
#define MP2TS_HDR_TS_PID                  0x001fff00
#define MP2TS_HDR_TS_SCRAMBLING_CTRL      0x000000c0
#define MP2TS_HDR_TS_ADAPTATION_EXISTS    0x00000020
#define MP2TS_HDR_TS_PAYLOAD_EXISTS       0x00000010
#define MP2TS_HDR_TS_CONTINUITY           0x0000000f

#define GET_MP2TS_HDR_SYNCBYTE(p)         (((p)->hdr & MP2TS_HDR_SYNCBYTE) >> 24)
#define GET_MP2TS_HDR_TS_PID(p)           (((p)->hdr & MP2TS_HDR_TS_PID) >> 8)
#define GET_MP2TS_HDR_TS_CONTINUITY(p)    ((p)->hdr & MP2TS_HDR_TS_CONTINUITY)



#define MP2TS_ADAPTATION_DISCONTINUITY    0x80
#define MP2TS_ADAPTATION_RAND_ACCESS      0x40
#define MP2TS_ADAPTATION_ES_PRIORITY      0x20
#define MP2TS_ADAPTATION_PCR              0x10
#define MP2TS_ADAPTATION_OPCR             0x08
#define MP2TS_ADAPTATION_SPLICING_PT      0x04
#define MP2TS_ADAPTATION_TX_PRIVDATA      0x02
#define MP2TS_ADAPTATION_EXT              0x01


#define MP2TS_PARSE_FLAG_HAVEPCR          0x01
#define MP2TS_PARSE_FLAG_HAVEPTS          0x02
#define MP2TS_PARSE_FLAG_HAVEDTS          0x04
#define MP2TS_PARSE_FLAG_PESSTART         0x10

//#define MP2TS_LARGE_PCR_JUMP_MS           500
//#define MP2TS_LARGE_PCR_JUMP_HZ           45000 
#define MP2TS_LARGE_PCR_JUMP_MS           1000
#define MP2TS_LARGE_PCR_JUMP_HZ           90000 


#define MP2TS_FILE_SEEKIDX_NAME       ".seek"

typedef struct MP2TS_PKT_TIMING {
  uint64_t        pcr;
  PKTQ_TIME_T     qtm;
} MP2TS_PKT_TIMING_T;

typedef struct MP2TS_PKT {
  unsigned char *pData;
  unsigned int len;
  unsigned int idx;
  uint32_t hdr;

  uint16_t pid;
  uint16_t cidx;
  uint32_t rtpts;

  MP2TS_PKT_TIMING_T tm;

  int flags;
} MP2TS_PKT_T;

enum MP2TS_PARSE_RC {         
  MP2TS_PARSE_UNKNOWN                  = 0,
  MP2TS_PARSE_ERROR                    = 1,
  MP2TS_PARSE_OK_PAT                   = 2,
  MP2TS_PARSE_OK_PMT                   = 3,
  MP2TS_PARSE_OK_PAYLOADSTART          = 4,
  MP2TS_PARSE_OK_PAYLOADCONT           = 5,
  MP2TS_PARSE_OK_EMPTY                 = 6
};

enum MP2TS_PARSE_RC mp2ts_parse(MP2TS_PKT_T *pPkt,
                                MP2_PAT_T *pPat,
                                MP2_PMT_T *pPmt);

typedef struct OFFSET_NODE {
  FILE_OFFSET_T                     fileOffset;
  uint64_t                          pcr;
  //struct OFFSET_NODE *pnext;
} OFFSET_NODE_T;


//#define DEBUG_PCR 1 
//#define DEBUG_PES 1 
//#define DEBUG_M2TPKTS 1


typedef struct MP2TS_PID_DESCR {
  union {
    VIDEO_DESCRIPTION_GENERIC_T       vidProp;
    AUDIO_DESCRIPTION_GENERIC_T       audProp;
  } u;
  MP2_PID_T                         pid;
  //uint8_t                           streamId;
  uint16_t                          streamId;
  unsigned int                      numPes;
  unsigned int                      numFrames;   // mpeg2-ts frames
#ifdef DEBUG_PES 
  uint64_t prev_pts;
  uint64_t prev_dts;
  uint64_t prev_tm;
#endif // DEBUG_PES
} MP2TS_PID_DESCR_T;


typedef struct MP2TS_STREAM_CHANGE {
  uint64_t                          startHz;
  uint64_t                          pcrStart;
  uint64_t                          pcrEnd;
  uint32_t                          pcrpid; 
  FILE_OFFSET_T                     fileOffsetStart;
  unsigned int                      numPcrOffsets;
  OFFSET_NODE_T                    *pPcrOffsets;
  unsigned int                      numFrames;   // mpeg2-ts frames
  uint8_t                           havepcrStart;
  uint8_t                           havepcrEnd;
  uint8_t                           pmtVersion;
  uint8_t                           pad;
  MP2TS_PID_DESCR_T                 pidDescr[MP2PES_MAX_PROG];
  struct MP2TS_STREAM_CHANGE       *pnext; 
} MP2TS_STREAM_CHANGE_T;

#define MP2TS_CONTAINER_SERIALIZED_VER               6

typedef struct MP2TS_CONTAINER {
  uint32_t                          serializedver;
  uint32_t                          wordsz;
  uint32_t                          szChangeElem;
  FILE_OFFSET_T                     size;
  FILE_STREAM_T                    *pStream;
  unsigned int                      numFrames;
  uint64_t                          durationHz;
  unsigned int                      numPmtChanges;
  MP2TS_STREAM_CHANGE_T            *pmtChanges;
  MP2TS_STREAM_CHANGE_T            *pmtChangesLast;
  int                               liveFile; // file currently being written to
} MP2TS_CONTAINER_T;


MP2TS_CONTAINER_T *mp2ts_open(const char *path, const int *pabort, 
                              unsigned int maxframes, int writemeta);
MP2TS_CONTAINER_T *mp2ts_openfrommeta(const char *path);
void mp2ts_close(MP2TS_CONTAINER_T *pMp2ts);
void mp2ts_dump(MP2TS_CONTAINER_T *pMp2ts, int verbosity, FILE *fp);
MP2TS_STREAM_CHANGE_T *mp2ts_jump_file(MP2TS_CONTAINER_T *pMp2ts, float fStartSec,
                                       double *pLocationMsStart);

int mp2ts_jump_file_offset(FILE_STREAM_T *pFileStream, float percent, 
                           FILE_OFFSET_T offsetFromEnd);



#endif // __MP2TS_H__
