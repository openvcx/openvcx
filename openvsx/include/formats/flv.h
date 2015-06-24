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


#ifndef __FLV_H__
#define __FLV_H__

#include <stdio.h>
#include "fileutil.h"
#include "bits.h"
#include "avcc.h"
#include "esds.h"
#include "filetype.h"

typedef int (*CB_FLV_READ_SAMPLE)(FILE_STREAM_T *, uint32_t, void*, int, uint32_t);

#define FLV_HDR_HAVE_AUDIO(pHdr) ((pHdr)->flags & 0x04)
#define FLV_HDR_HAVE_VIDEO(pHdr) ((pHdr)->flags & 0x01)
#define FLV_HAVE_AUDIO(pFlv)    FLV_HDR_HAVE_AUDIO((&((pFlv)->hdr)))
#define FLV_HAVE_VIDEO(pFlv)    FLV_HDR_HAVE_VIDEO((&((pFlv)->hdr)))


typedef struct FLV_BYTE_STREAM {
  BYTE_STREAM_T        bs;
  unsigned int         prevtagidx;
} FLV_BYTE_STREAM_T;


typedef struct FLV_HDR {
  uint8_t             type[3];
  uint8_t             ver;
  uint8_t             flags;
  uint8_t             pad[3];
  uint32_t            offset;
} FLV_HDR_T;

#define FLV_TAG_AUDIODATA               8	
#define FLV_TAG_VIDEODATA               9
#define FLV_TAG_SCRIPTDATA             18

#define FLV_HDR_VER_1                  0x01
#define FLV_TAG_HDR_SZ                 15
#define FLV_TAG_HDR_SZ_NOPREV          11

typedef struct FLV_TAG_HDR {
  uint32_t            szprev;
  uint8_t             type;
  uint8_t             size[3];
  uint8_t             timestamp[3];
  uint8_t             tsext;
  uint8_t             streamid[3];

  uint8_t             pad;
  uint32_t            size32;
  uint32_t            timestamp32;
  uint32_t            szhdr;
  FILE_OFFSET_T       fileOffset; // offset of header start
  struct FLV_TAG_HDR *pnext;
} FLV_TAG_HDR_T;

#define FLV_AUD_CODEC_LINEARPCM           0
#define FLV_AUD_CODEC_ADPCM               1
#define FLV_AUD_CODEC_MP3                 2
#define FLV_AUD_CODEC_LINEARPCM_LE        3
#define FLV_AUD_CODEC_NELLYMOSER_16KHZ    4
#define FLV_AUD_CODEC_NELLYMOSER_8KHZ     5
#define FLV_AUD_CODEC_NELLYMOSER          6
#define FLV_AUD_CODEC_G711_ALAW           7
#define FLV_AUD_CODEC_G711_ULAW           8
#define FLV_AUD_CODEC_RESERVED            9
#define FLV_AUD_CODEC_AAC                10 
#define FLV_AUD_CODEC_SPEEX              11 
#define FLV_AUD_CODEC_MP3_8KHZ           14 
#define FLV_AUD_CODEC_DEVICE_SP          15 

#define FLV_AUD_RATE_5_5KHZ                0
#define FLV_AUD_RATE_11KHZ                 1 
#define FLV_AUD_RATE_22KHZ                 2 
#define FLV_AUD_RATE_44KHZ                 3 

#define FLV_AUD_SZ_8BIT                    0
#define FLV_AUD_SZ_16BIT                   1

#define FLV_AUD_TYPE_MONO                  0
#define FLV_AUD_TYPE_STEREO                1

#define FLV_AUD_AAC_PKTTYPE_SEQHDR         0
#define FLV_AUD_AAC_PKTTYPE_RAW            1


typedef struct FLV_TAG_AUDIO_AAC {
  uint8_t             pkttype;

} FLV_TAG_AUDIO_AAC_T;

typedef struct FLV_TAG_AUDIO {
  FLV_TAG_HDR_T       hdr;
  uint32_t            szaudhdr;
  uint8_t             frmratesz;      // 4 bits AUD_FORMAT | 2 bits AUD_RATE
                                      // 1 bit AUD_SZ | 1 bit AUD_TYPE
  FLV_TAG_AUDIO_AAC_T aac;

} FLV_TAG_AUDIO_T;


#define FLV_VID_FRM_KEYFRAME		1
#define FLV_VID_FRM_INTERFRAME		2
#define FLV_VID_FRM_DISPOSABLE          3
#define FLV_VID_FRM_GENKEYFRAME         4
#define FLV_VID_FRM_VIDINFO             5

#define FLV_VID_CODEC_JPEG              1
#define FLV_VID_CODEC_SORENSON          2
#define FLV_VID_CODEC_SCREENVID         3
#define FLV_VID_CODEC_ON2VP6            4
#define FLV_VID_CODEC_ON2VP6ALPHA       5
#define FLV_VID_CODEC_SCREENVID2        6
#define FLV_VID_CODEC_AVC               7

#define FLV_VID_AVC_PKTTYPE_SEQHDR      0
#define FLV_VID_AVC_PKTTYPE_NALU        1
#define FLV_VID_AVC_PKTTYPE_ENDSEQ      2

typedef struct FLV_TAG_VIDEO_AVC {
  uint8_t             pkttype;
  uint8_t             comptime[3];
  int32_t             comptime32;

} FLV_TAG_VIDEO_AVC_T;

typedef struct FLV_TAG_VIDEO {
  FLV_TAG_HDR_T       hdr;
  uint32_t            szvidhdr;
  uint8_t             frmtypecodec;
  FLV_TAG_VIDEO_AVC_T avc;
} FLV_TAG_VIDEO_T;


//AMF Data Types
#define FLV_AMF_TYPE_NUMBER       0
#define FLV_AMF_TYPE_BOOL         1
#define FLV_AMF_TYPE_STRING       2
#define FLV_AMF_TYPE_OBJECT       3
#define FLV_AMF_TYPE_MOVCLIP      4
#define FLV_AMF_TYPE_NULL         5
#define FLV_AMF_TYPE_UNDEFINED    6
#define FLV_AMF_TYPE_REFERENCE    7
#define FLV_AMF_TYPE_ECMA         8
#define FLV_AMF_TYPE_OBJ_END      9
#define FLV_AMF_TYPE_STRICTARR    10
#define FLV_AMF_TYPE_DATE         11 
#define FLV_AMF_TYPE_LONGSTR      12 
#define FLV_AMF_TYPE_UNSUPPORTED  13 
#define FLV_AMF_TYPE_RECORD_SET   14 
#define FLV_AMF_TYPE_XML_OBJ      15 

typedef struct FLV_AMF_STR {
  uint16_t                       len;
  uint16_t                       pad;
  unsigned char                 *pBuf; // alloc buffer
  unsigned char                 *p;    // direct pointer to byte stream   
} FLV_AMF_STR_T;

typedef struct FLV_AMF_EMCA {
  uint32_t                count;
  void                   *pEntries; //FLV_AMF_T *
  unsigned int            cntAlloced;
} FLV_AMF_EMCA_ARR_T;

typedef struct FLV_AMF_DATE {
  double                         val;
  uint16_t                       offset; // local time offset from UTC in minutes
} FLV_AMF_DATE_T;

typedef struct FLV_AMF_VAL {
  uint8_t                        type;
  union {
    double                       d;
    uint8_t                      b;
    FLV_AMF_DATE_T      date;
    FLV_AMF_STR_T       str;
    FLV_AMF_EMCA_ARR_T  arr;
  } u;
} FLV_AMF_VAL_T;

typedef struct FLV_AMF {
  struct FLV_AMF       *pnext;
  FLV_AMF_STR_T         key;
  FLV_AMF_VAL_T         val;
} FLV_AMF_T;


typedef struct FLV_TAG_SCRIPT {
  FLV_TAG_HDR_T       hdr;

  FLV_AMF_T  entry;
} FLV_TAG_SCRIPT_T;

typedef struct FLV_DECODER_VID_CFG {
  AVC_DECODER_CFG_T    *pAvcC;
  VIDEO_DESCRIPTION_GENERIC_T vidDescr;
} FLV_DECODER_VID_CFG_T;

typedef struct FLV_DECODER_AUD_CFG {
  ESDS_DECODER_CFG_T   aacCfg;
  AUDIO_DESCRIPTION_GENERIC_T audDescr;
} FLV_DECODER_AUD_CFG_T;

typedef struct FLV_CONTAINER {
  FILE_STREAM_T      *pStream;

  FLV_HDR_T           hdr;
  FLV_TAG_HDR_T      *pHdrPrev;

  FLV_TAG_VIDEO_T    videoTmp;
  FLV_TAG_AUDIO_T    audioTmp;
  FLV_TAG_SCRIPT_T   scriptTmp;

  FLV_DECODER_VID_CFG_T vidCfg;
  FLV_DECODER_AUD_CFG_T audCfg;
  unsigned char      arena[1024];

  //Obtained from script tag
  uint32_t sampledeltaHz;
  uint32_t timescaleHz;
  uint32_t vidsampledeltaHz;  // SPS VUI timing override
  uint32_t vidtimescaleHz;    // SPS VUI timing override
  uint64_t durationTotHz;

  uint32_t lastVidSampleIdx;
  uint32_t lastAudSampleIdx;

} FLV_CONTAINER_T;


FLV_CONTAINER_T *flv_open(const char *path);
void flv_dump(FLV_CONTAINER_T *pFlv, int verbose, FILE *fp);
void flv_close(FLV_CONTAINER_T *pFlv);

int flv_readSamples(FLV_CONTAINER_T *pFlv,
                    CB_FLV_READ_SAMPLE sampleRdFunc, void *pCbData,
                    uint64_t startHz, uint64_t durationHz, int video);


#if defined(VSX_EXTRACT_CONTAINER)
int flv_extractRaw(FLV_CONTAINER_T *pFlv, const char *outPrfx,
                   float fStart, float fDuration, int overwrite,
                   int extractVid, int extractAud);
#endif // VSX_EXTRACT_CONTAINER

int flv_amf_read(int amfKeyType, FLV_AMF_T *pEntry, BYTE_STREAM_T *bs, int alloc);
const unsigned char *flv_amf_read_string(FLV_AMF_STR_T *pStr, BYTE_STREAM_T *bs, 
                                         int alloc);



int flv_parse_onmeta(FLV_AMF_T *pEntry, BYTE_STREAM_T *bs);
void flv_amf_free(FLV_AMF_T *pEntry);

#endif // __FLV_H__
