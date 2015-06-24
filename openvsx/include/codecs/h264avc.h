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


#ifndef __H264_AVC_H__
#define __H264_AVC_H__


#include "vsxconfig.h"
#include "codecs/h264.h"
#include "codecs/avcc.h"
#include "formats/mp4.h"
#include "formats/mp4track.h"
#include "formats/flv.h"
#include "formats/mkv.h"


#define H264_AVC_MAX_SLICES_PER_FRAME       140
#define H264_STARTCODE_LEN                  4

#define H264_DEFAULT_CLOCKHZ                90000


typedef struct H264_NAL {
  MP4_MDAT_CONTENT_NODE_T content;

  uint32_t frameIdx;   // from NAL slice header (0th based relative to IDR)
  uint32_t picOrderCnt;
  uint8_t hdr;
  uint8_t sliceType;  // enum H264_SLICE_TYPE
  uint8_t paramSetChanged;
  uint8_t pad;
} H264_NAL_T;

typedef struct CBDATA_ATTACH_AVCC_SAMPLE {
  void *pH264;
  unsigned int lenNalSzField;
  unsigned char arena[4096];
} CBDATA_ATTACH_AVCC_SAMPLE_T;


#define H264_USE_DIRECT_READ(pH264) ((pH264)->pMp4Ctxt || (pH264)->pMkvCtxt)

typedef struct H264_AVC_DESCR {

  unsigned int clockHz;
  unsigned int frameDeltaHz;
  unsigned int clockHzVUI;
  unsigned int frameDeltaHzVUI;

  unsigned int frameWidthPx;
  unsigned int frameHeightPx;

  SPSPPS_RAW_T spspps; 

  double pocDeltaTmp; 
  double pocDeltaMin;
  unsigned int pocDeltaSamples;
  unsigned int pocDeltaSamplesOdd;
  unsigned int pocDeltaSamplesEven;
  int pocDenum;
  int hasPocWrapOnNonI;
  unsigned int highestPoc;
  int validExpectedPoc;
  int expectedPoc;
  H264_DECODER_CTXT_T ctxt;

  // TODO haveBSlices flag should be for current SPS context only
  int haveBSlices;

  H264_NAL_T *pNalsLastSlice;
  H264_NAL_T *pNalsLastSliceP;
  H264_NAL_T *pNalsLastSliceI;
  H264_NAL_T *pNalsLastSliceIPrev;
  H264_NAL_T *pNalsLastSlicePocRef;
  H264_NAL_T *pNalsLastSEI;

  H264_NAL_T *pNalsVcl;
  H264_NAL_T *pNalsLastVcl;
  unsigned int numNalsVcl;
  unsigned int numNalsTot;
  unsigned int numNalsErr;
  unsigned int lastFrameId;
  uint64_t lastPlayOffsetHz;
  uint32_t clockHzOverride;                    // clockHz obtained from any container 
  MP4_MDAT_CONTENT_NODE_T *pNalCbPrev;

  MP4_MDAT_CONTENT_NODE_T mp4ContentNode;
  MP4_EXTRACT_STATE_T *pMp4Ctxt;
  MKV_EXTRACT_STATE_T *pMkvCtxt;
  CBDATA_ATTACH_AVCC_SAMPLE_T avcCbData;
  H264_NAL_T nalLastSlice;
  H264_NAL_T nalLastSliceP;
  H264_NAL_T nalLastSliceI;
  H264_NAL_T nalLastSlicePocRef;

  unsigned int nalsBufSz;
  H264_NAL_T  *pNalsBuf;

  // counters
  long long sliceCounter[H264_SLICE_TYPE_UNKNOWN + 1];

} H264_AVC_DESCR_T;




int h264_escapeRawByteStream(const BIT_STREAM_T *pIn, BIT_STREAM_T *pOut);
int h264_moveToPriorSync(H264_AVC_DESCR_T *pH264);

MP4_MDAT_CONTENT_NODE_T *h264_cbGetNextAnnexBSample(void *pArg, int idx, int advanceFp);
int h264_write_nal_to_mp4(FILE_STREAM_T *pStreamOut, MP4_MDAT_CONTENT_NODE_T *pSample,
                      unsigned char *arena, unsigned int sz_arena);

int h264_findStartCode(H264_STREAM_CHUNK_T *pStream, int handle_short_code, int *pstartCodeLen);
int h264_getAvcCNalLength(const unsigned char *pData, unsigned int len, uint8_t avcfield);
const char *h264_getPrintableProfileStr(uint8_t profile_idc);

void h264_free(H264_AVC_DESCR_T *pH264);

int h264_createNalListFromFlv(H264_AVC_DESCR_T *pH264, FLV_CONTAINER_T *pFlv, double fps);
int h264_createNalListFromMkv(H264_AVC_DESCR_T *pH264, MKV_CONTAINER_T *pMkv);
int h264_createNalListFromMp4(H264_AVC_DESCR_T *pH264, MP4_CONTAINER_T *pMp4, 
                             CHUNK_SZ_T *pStchk, float fStartSec);
int h264_createNalListFromMp4Direct(H264_AVC_DESCR_T *pH264, MP4_CONTAINER_T *pMp4, 
                       float fStartSec);
int h264_createNalListFromAnnexB(H264_AVC_DESCR_T *pH264, FILE_STREAM_T *pStream,
                                 float fStartSec, double fps);
int h264_updateFps(H264_AVC_DESCR_T *pH264, double fps);
int h264_converth264bToAvc(unsigned char *pData, unsigned int len);
int h264_convertAvcToh264b(const unsigned char *pIn, unsigned int lenIn, 
                           unsigned char *pOut, unsigned int lenOut, int avcFieldLen,
                           int failOnSize);
int h264_check_NAL_AUD(const unsigned char *p, unsigned int len);
void h264_printSps(FILE *fpOut, const H264_NAL_SPS_T *pSps);
void h264_printPps(FILE *fpOut, const H264_NAL_PPS_T *pPps);


#endif // __H264_AVC_H__
