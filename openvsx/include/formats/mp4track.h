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


#ifndef __MP4_TRACK_H__
#define __MP4_TRACK_H__

#include "mp4boxes.h"

typedef struct MP4_MOOF_TRAK {

  BOX_MOOF_T     *pMoof;
  BOX_MDAT_T     *pMdat;
  BOX_MFHD_T     *pMfhd;
  //TODO: the following can be found multiple times
  BOX_TRAF_T     *pTraf;
  BOX_TFHD_T     *pTfhd;
  BOX_TFDT_T     *pTfdt;
  BOX_TRUN_T     *pTrun;
  FILE_OFFSET_T   fileOffset;
  unsigned int    totMoofTruns;
  unsigned int    totSampleCnt;

  BOX_T          *pMp4Root;

} MP4_MOOF_TRAK_T;

typedef struct MP4_TRAK {

  //
  // Used only for mp4 creation
  //
  uint64_t sz_data;
  uint64_t mvhd_duration;
  uint64_t mvhd_timescale;
  uint32_t tkhd_id;

  uint32_t stsdType;

  CHUNK_SZ_T stchk;

  BOX_T     *pTrak;
  BOX_TKHD_T *pTkhd;
  BOX_T      *pMdia;
  BOX_MDHD_T *pMdhd;
  BOX_HDLR_T *pHdlr;
  BOX_T *pMinf;

  BOX_VMHD_T *pVmhd;
  BOX_SMHD_T *pSmhd;
  BOX_T      *pNmhd; // null media handler

  BOX_T      *pDinf;
  BOX_DREF_T *pDref;
  BOX_T      *pStbl;
  BOX_STSD_T *pStsd;
  BOX_T      *pStsdChild;
  BOX_STTS_T *pStts;
  BOX_CTTS_T *pCtts;
  BOX_STSZ_T *pStsz;
  BOX_STSC_T *pStsc;
  BOX_STCO_T *pStco;
  BOX_STSS_T *pStss;

  BOX_MDAT_T *pMdat;

  MP4_MOOF_TRAK_T moofTrak;

} MP4_TRAK_T;

// H.264 video track
typedef struct MP4_TRAK_AVCC {
  MP4_TRAK_T tk;
  BOX_AVC1_T *pAvc1;
  BOX_AVCC_T *pAvcC;
} MP4_TRAK_AVCC_T;

// mp4v video track
typedef struct MP4_TRAK_MP4V {
  MP4_TRAK_T tk;
  BOX_MP4V_T *pMp4v;
  BOX_ESDS_T *pEsds;
} MP4_TRAK_MP4V_T;

// mp4a audio (AAC) track
typedef struct MP4_TRAK_MP4A {
  MP4_TRAK_T tk;
  BOX_MP4A_T *pMp4a;
  BOX_ESDS_T *pEsds;
} MP4_TRAK_MP4A_T;

// samr audio track
typedef struct MP4_TRAK_SAMR {
  MP4_TRAK_T tk;
  BOX_SAMR_T *pSamr;
  BOX_DAMR_T *pDamr;
} MP4_TRAK_SAMR_T;

typedef struct MP4_EXTRACT_STATE_INT_TRAK {
  int                        isinit;
  unsigned int               idxSample;
  unsigned int               idxChunk;
  unsigned int               idxSampleInChunk;
  unsigned int               idxStsc;
  unsigned int               idxStts;
  unsigned int               idxStss;
  unsigned int               idxSampleInStts;
  unsigned int               chunkOffset;
  int                        samplesInChunk;
} MP4_EXTRACT_STATE_INT_TRAK_T;

typedef struct MP4_EXTRACT_STATE_INT_MOOF {
  BOX_MOOF_T                *pMoofCur;
  int                        haveFirstMoof;
  unsigned int               mfhdSequence;
  unsigned int               idxSample;
  unsigned int               countSamples;
  unsigned int               idxMoofTrun;
  FILE_OFFSET_T              dataOffset;
} MP4_EXTRACT_STATE_INT_MOOF_T;

typedef struct MP4_EXTRACT_STATE_INT {

  union {
    MP4_EXTRACT_STATE_INT_TRAK_T   tk;
    MP4_EXTRACT_STATE_INT_MOOF_T   mf;
  } u;

  struct MP4_EXTRACT_STATE  *pExtSt;

} MP4_EXTRACT_STATE_INT_T;

typedef int (* MP4_CB_LOADER_NEXT)(void *);
typedef struct MP4_LOADER_NEXT {
  MP4_CB_LOADER_NEXT        cbLoadNextMp4;
  void                     *pCbData;
} MP4_LOADER_NEXT_T;

typedef struct MP4_EXTRACT_STATE {
  MP4_EXTRACT_STATE_INT_T   cur;
  MP4_EXTRACT_STATE_INT_T   start;
  MP4_EXTRACT_STATE_INT_T   lastSync;
  uint64_t                  lastSyncPlayOffsetHz;
  MP4_TRAK_T                trak;
  MP4_FILE_STREAM_T        *pStream;
  int                       atEndOfTrack;

  MP4_LOADER_NEXT_T         nextLoader;

} MP4_EXTRACT_STATE_T;



#endif // ___MP4_TRACK_H__
