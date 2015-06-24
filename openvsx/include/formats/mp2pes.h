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


#ifndef __MP2PES_H__
#define __MP2PES_H__

//#ifdef WIN32
#include "unixcompat.h"
//#endif // WIN32

#include "util/pktqueue.h"
#include "formats/filetypes.h"

// stream type values from ISO/IEC 13818-7 (table2-29)
#define MP2PES_STREAM_PRIV_START            0x80  // User Private

#define MP2PES_STREAM_VIDEO_H262            0x02  // IS0 13818-2 / ISO-11172-2 ITU-T H.262 
#define MP2PES_STREAM_VIDEO_H264            0x1b
#define MP2PES_STREAM_VIDEO_MP4V            0x10  // ISO/IEC 14496-2 Visual
#define MP2PES_STREAM_VIDEO_H263            0xa0 
#define MP2PES_STREAM_VIDEO_H263_PLUS       MP2PES_STREAM_VIDEO_H263

#define MP2PES_STREAM_AUDIO_AACADTS         0x0f  // ISO-IEC 13818-7 w/ ADTS
#define MP2PES_STREAM_AUDIO_AC3             (MP2PES_STREAM_PRIV_START + 1)
#define MP2PES_STREAM_AUDIO_AMR             (MP2PES_STREAM_PRIV_START + 2) 
#define MP2PES_STREAM_AUDIO_G711            (MP2PES_STREAM_PRIV_START + 3) 
#define MP2PES_STREAM_AUDIO_MP2A_L2         0x03  // ISO/IEC 11172  (MPEG-1/2 Layer 2)(mp2)
#define MP2PES_STREAM_AUDIO_MP2A_L3         0x04  // ISO/IEC 13818-3 (MPEG-1/2 Layer 3) (mp3)
#define MP2PES_STREAM_AUDIO_MP4A            0x0b  // ISO/IEC 14496-3 (MPEG-4 Part 3)


#define MP2PES_STREAMID_VIDEO_START         0xe0
#define MP2PES_STREAMID_AUDIO_START         0xc0
#define MP2PES_STREAMID_PRIV_START          0xbd


#define MP2PES_STREAMTYPE_NOTSET           0x1ff
#define MP2PES_STREAMID_NOTSET             MP2PES_STREAMTYPE_NOTSET

#define MP2PES_STREAM_IDX_VID              STREAM_IDX_VID 
#define MP2PES_STREAM_IDX_AUD              STREAM_IDX_AUD

#define MP2PES_MAX_PROG    10

#define MP2PES_STREAMTYPE_FMT_STR          "stype: %s (0x%x)"
#define MP2PES_STREAMTYPE_FMT_ARGS(st)     codecType_getCodecDescrStr( \
                                           mp2_getCodecType((st))), (st)

typedef struct MP2_PID {
  uint16_t     id;
  uint16_t     streamtype;          // from iso13818-1 table 2-29 stream type assignments
                                    // use 16 bits instead of 8 to allow for not set
} MP2_PID_T;

typedef struct MP2_PES_PROG {
  MP2_PID_T pid;

  FILE_STREAM_T stream;
  uint16_t streamId;                  // from PES start iso13818-1 table 2-18
                                      // use 16 bits instead of 8 to allow for not set

  unsigned int prevCidx;
  uint8_t havePesStart;              // set to 1 if PES start previously rcvd
  FILE_OFFSET_T fileOffsetPrev;
  unsigned int prevPesTotLen;
  unsigned int nextPesTotLen;
  unsigned int idxInPes;
  unsigned int idxPes;               // frame idx
} MP2_PES_PROG_T;

typedef struct MP2_PAT {
  uint8_t      version;
  uint8_t      havever;
  uint16_t     prognum;
  uint16_t     progmappid;
  uint16_t     tsid;
} MP2_PAT_T;


typedef struct MP2_PMT {
  uint32_t     pcrpid;
  uint16_t     prognum;
  uint8_t      version;
  uint8_t      havever;
  uint8_t      numpids;
  uint8_t      pad;
  MP2_PID_T    pids[MP2PES_MAX_PROG]; 
} MP2_PMT_T;




typedef struct MP2_PES_DESCR {
  unsigned int pcrpid;

  MP2_PAT_T pat;
  MP2_PMT_T pmt;

  unsigned int activePrograms;
  MP2_PES_PROG_T progs[MP2PES_MAX_PROG];


  int overWriteFile;
  const char *outputDir;

} MP2_PES_DESCR_T;

typedef int (*CBMP2PMT_ONPROGADD)(void *, const MP2_PID_T *); 


int mp2_check_pes_streamid(int streamId);

XC_CODEC_TYPE_T mp2_getCodecType(uint16_t streamType);
uint16_t mp2_getStreamType(XC_CODEC_TYPE_T codecType);

MP2_PES_PROG_T *mp2_find_prog(MP2_PES_DESCR_T *pMp2, unsigned int progId);
MP2_PES_PROG_T *mp2_new_prog(MP2_PES_DESCR_T *pMp2, unsigned int progPid);


uint64_t mp2_parse_pts(const unsigned char *data);
uint64_t mp2_parse_pcr(const unsigned char *data, uint32_t *pPcr27Mhz);

int mp2_parse_pes(MP2_PES_PROG_T *pProg,
                  unsigned char *buf, unsigned int len, PKTQ_TIME_T *pTm, int *phaveTm);

int mp2_parse_pat(MP2_PAT_T *pPat,
                  unsigned char *buf, unsigned int len);

int mp2_parse_pmt(MP2_PMT_T *pPmt,
                  unsigned int progId,
                  void *pMp2CbData,
                  CBMP2PMT_ONPROGADD cbOnProgAdd,
                  unsigned char *buf, unsigned int len,
                  int newpmtonly);


int mp2_parse_adaptation(unsigned char *buf,
                         unsigned int len,
                         unsigned int pid, 
                         unsigned int pcrpid, 
                         uint64_t *pPcr90Khz, 
                         uint32_t *pPcr27Mhz);

#endif // __MP2PES_H__
