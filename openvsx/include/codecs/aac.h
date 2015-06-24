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


#ifndef __AAC_H__
#define __AAC_H__

#include "vsxconfig.h"
#include "util/bits.h"
#include "util/fileutil.h"
#include "codecs/codecs.h"
#include "formats/mp4track.h"
#include "formats/flv.h"
#include "formats/mkv.h"

/**
 * AAC ADTS frame representation 
 */
typedef struct AAC_ADTS_FRAME {
  ESDS_DECODER_CFG_T sp;
  unsigned int lenhdr;
  unsigned int lentot;
} AAC_ADTS_FRAME_T;


/*
 * AAC codec context description
 */
typedef struct AAC_DESCR {
  /**
   * Clock rate in Hz
   */
  unsigned int clockHz;
  ESDS_DECODER_CFG_T decoderSP;
  unsigned int szLargestFrame;
  uint32_t     szTot;
  uint32_t     peakBwBytes;
  FILE_STREAM_T *pStream;
  uint32_t lastFrameId;
  uint64_t lastPlayOffsetHz;
  MP4_MDAT_CONTENT_NODE_T prevFrame;

  MP4_MDAT_CONTENT_T content;
  int includeAdtsHdr;
  // Matroska container reference
  MKV_CONTAINER_T *pMkv;
  MP4_EXTRACT_STATE_T *pMp4Ctxt;
} AAC_DESCR_T;


/**
 * Get the next AAC frame sample
 */
MP4_MDAT_CONTENT_NODE_T *aac_cbGetNextSample(void *pAac, int idx, int advanceFp);

int aac_encodeADTSHdr(unsigned char *buf, unsigned int len, 
                      const ESDS_DECODER_CFG_T *pSp, 
                      unsigned int sizeAAcData);
int aac_decodeAdtsHdrStream(FILE_STREAM_T *pStreamInAac, AAC_ADTS_FRAME_T *pFrame);
int aac_decodeAdtsHdr(const unsigned char *buf, unsigned int len, AAC_ADTS_FRAME_T *pFrame);

AAC_DESCR_T *aac_getSamplesFromFlv(FLV_CONTAINER_T *pFlv);
AAC_DESCR_T *aac_getSamplesFromMkv(MKV_CONTAINER_T *pMkv);
AAC_DESCR_T *aac_getSamplesFromMp4(const MP4_CONTAINER_T *pMp4, CHUNK_SZ_T *pStchk, float fStart);
AAC_DESCR_T *aac_getSamplesFromMp4Direct(const MP4_CONTAINER_T *pMp4, float fStart);
AAC_DESCR_T *aac_initFromAdts(FILE_STREAM_T *pFileStream, unsigned int frameDeltaHz);
int aac_getSamplesFromAacAdts(AAC_DESCR_T *pAac, int storeSamples);

void aac_free(AAC_DESCR_T *pAac);
void aac_close(AAC_DESCR_T *pAac);

int write_box_mdat_from_file_aacAdts(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                                   unsigned char *arena, unsigned int sz_arena,
                                   void *pUserData);
int aac_write_sample_to_mp4(FILE_STREAM_T *pStreamOut, MP4_MDAT_CONTENT_NODE_T *pSample,
                                  unsigned char *arena, unsigned int sz_arena);


#endif // __AAC_H__
