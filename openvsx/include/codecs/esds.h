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


#ifndef __ESDS_H__
#define __ESDS_H__

#include "vsxconfig.h"
#include "fileutil.h"
#include "bits.h"

#define ESDS_STARTCODES_SZMAX            128

enum  {
  ESDS_OBJTYPE_UNKNOWN =               0,
  ESDS_OBJTYPE_SYSV1 =                 1,
  ESDS_OBJTYPE_SYSV2 =                 2,
  ESDS_OBJTYPE_MP4V =                 32,
  ESDS_OBJTYPE_MP4V_AVC_SPS =         33,
  ESDS_OBJTYPE_MP4V_AVC_PPS =         34,
  ESDS_OBJTYPE_MP4A =                 64,  // AAC
  ESDS_OBJTYPE_MP2V_SIMPLE =          96,
  ESDS_OBJTYPE_MP2V_MAIN =            97,
  ESDS_OBJTYPE_MP2V_SNR =             98,
  ESDS_OBJTYPE_MP2V_SPATIAL =         99,
  ESDS_OBJTYPE_MP2V_HIGH =           100,
  ESDS_OBJTYPE_MP2V_422 =            101,
  ESDS_OBJTYPE_MP4A_ADTS_MAIN =      102,
  ESDS_OBJTYPE_MP4A_ADTS_LC =        103,
  ESDS_OBJTYPE_MP4A_ADTS_SSR =       104,  // Scalable Sampling Rate
  ESDS_OBJTYPE_MP2A_ADTS =           105,
  ESDS_OBJTYPE_MP1V =                106,
  ESDS_OBJTYPE_MP1A_ADTS =           107,
  ESDS_OBJTYPE_JPGV =                108,
  ESDS_OBJTYPE_PRIVATE_AUDIO =       192,
  ESDS_OBJTYPE_PRIVATE_VIDEO =       208,
  ESDS_OBJTYPE_AUD_16PCM_LE =        224,
  ESDS_OBJTYPE_AUD_VORBIS =          225,
  ESDS_OBJTYPE_AUD_DOLBYV3 =         226,  // AC3
  ESDS_OBJTYPE_AUD_ALAW =            227,
  ESDS_OBJTYPE_AUD_MULAW =           228,
  ESDS_OBJTYPE_AUD_ADPCM =           229,
  ESDS_OBJTYPE_AUD_16PCM_BE =        230,
  ESDS_OBJTYPE_VID_YCbCr420 =        240,
  ESDS_OBJTYPE_VID_H264 =            241,
  ESDS_OBJTYPE_VID_H263 =            242,
  ESDS_OBJTYPE_VID_H261 =            243
};

enum ESDS_STREAMTYPE {
  ESDS_STREAMTYPE_OBJDESCR =            1,
  ESDS_STREAMTYPE_CLOCKREF =            2,
  ESDS_STREAMTYPE_SCENEDESCR =          3,
  ESDS_STREAMTYPE_VISUAL =              4,
  ESDS_STREAMTYPE_AUDIO =               5,
  ESDS_STREAMTYPE_MPEG7 =               6,
  ESDS_STREAMTYPE_IPMP =                7,
  ESDS_STREAMTYPE_OCI =                 8,
  ESDS_STREAMTYPE_MPEG_JAVA =           9,
  ESDS_STREAMTYPE_USER_PRIV =          32
};

// profile-level-id  iso 14496-1 table8-2 (audioProfiles)
#define ISO_AUDPROFILE_14496_3         0x01 
#define ISO_AUDPROFILE_PRIVATE         0x80
#define ISO_AUDPROFILE_NONE            0xfe

#define MPEG4_AUD_LEVEL_1                1  // up to 24KHz mono
#define MPEG4_AUD_LEVEL_2                2  // up to 24KHz stereo
#define MPEG4_AUD_LEVEL_3                3  // up to 48KHz mono
#define MPEG4_AUD_LEVEL_4                4  // up to 48KHz stereo
#define MPEG4_AUD_LEVEL_5                5  // up to 96KHz

typedef struct ESDS_DECODER_CFG {
  int          init;
  unsigned int objTypeIdx;
  unsigned int freqIdx;
  uint16_t     haveExtTypeIdx;
  uint16_t     haveExtFreqIdx;
  unsigned int extObjTypeIdx;
  unsigned int extFreqIdx;
  unsigned int channelIdx;
  unsigned int frameDeltaHz;  // number of samples / frame (not frame length in bytes)
  unsigned int dependsOnCoreCoderFlag;
  unsigned int extFlag;
} ESDS_DECODER_CFG_T;

#define ESDS_FREQ_IDX(esds) ((esds).haveExtFreqIdx ? (esds).extFreqIdx : (esds).freqIdx)


typedef struct ESDS_CBDATA_WRITE_SAMPLE {
  FILE_STREAM_T           *pStreamOut;
  ESDS_DECODER_CFG_T       decoderCfg; 
  BYTE_STREAM_T            bs;
} ESDS_CBDATA_WRITE_SAMPLE_T;

int esds_encodeDecoderSp(unsigned char *pBuf, unsigned int len,
                         const ESDS_DECODER_CFG_T *pSp);

int esds_decodeDecoderSp(const unsigned char *p, const unsigned int len, 
                         ESDS_DECODER_CFG_T *pSp);

int esds_cbWriteSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz);

const char *esds_getMp4aObjTypeStr(unsigned int objTypeIdx);
const char *esds_getChannelStr(unsigned int channelIdx);
uint32_t esds_getFrequencyVal(unsigned int freqIdx);
int esds_getFrequencyIdx(unsigned int frequency);

#endif //__ESDS_H__
