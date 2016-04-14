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


#ifndef __MKV_H__
#define __MKV_H__

#include "mkv_types.h"
#include "formats/mp4boxes.h"

#define MKV_BLOCK_SIZE(pb) ((pb)->pBg->simpleBlock.size - (pb)->bsIdx)
#define MKV_BLOCK_BUF(pb)  (&((pb)->pBg->simpleBlock.buf[(pb)->bsIdx]))
#define MKV_BLOCK_BUFSZ(pb) MIN(MKV_BLOCK_SIZE(pb), EBML_BLOB_PARTIAL_DATA_SZ - (pb)->bsIdx)
#define MKV_BLOCK_FOFFSET(pb)  ((pb)->pBg->simpleBlock.fileOffset + (pb)->bsIdx)

typedef int (*CB_MKV_READ_FRAME)(void *, const MP4_MDAT_CONTENT_NODE_T *);


typedef struct MKV_CODEC_TYPE {
  const char               *str;
  XC_CODEC_TYPE_T           type;
} MKV_CODEC_TYPE_T;

#define MKV_DOCTYPE_MATROSKA           "matroska"
#define MKV_DOCTYPE_WEBM               "webm"

#define MKV_CODEC_ID_H264              "V_MPEG4/ISO/AVC"
#define MKV_CODEC_ID_VP8               "V_VP8"
#define MKV_CODEC_ID_AAC               "A_AAC"
#define MKV_CODEC_ID_VORBIS            "A_VORBIS"

typedef struct MKV_BLOCK {
  MKV_BLOCKGROUP_T         *pBg;
  MKV_TRACK_T              *pTrack;
  uint64_t                  timeCode;
  int                       keyframe;
  unsigned int              bsIdx;   // index within pBg->simpleBlock.buf to start of payload data
} MKV_BLOCK_T;

typedef struct MKV_BLOCK_QUEUED {
  MKV_BLOCKGROUP_T          bg;
  MKV_BLOCK_T               block;
} MKV_BLOCK_QUEUED_T;

#define MKV_BLOCKLIST_SIZE       256

typedef struct MKV_BLOCKLIST {
  unsigned int              sz;
  unsigned int              idxOldest;
  unsigned int              idxNewest;
  MKV_BLOCK_QUEUED_T       *parrBlocks;
  int                       haveRead;
  MKV_BLOCK_QUEUED_T        lastBlock;
} MKV_BLOCKLIST_T;

typedef struct MKV_PARSE_CTXT {
  EBML_BYTE_STREAM_T       mbs;
  uint32_t                 id;
  unsigned int             level;
  EBML_LEVEL_T             levels[EBML_MAX_LEVELS];
  unsigned char            buf[EBML_BUF_SZ];

  FILE_OFFSET_T            firstClusterOffset;
  FILE_OFFSET_T            lastFileOffset;
  MKV_CLUSTER_T            cluster; // current cluster
  unsigned int             blockIdx;  // index within cluster.arrBlocks.count
  int                      haveCluster;

} MKV_PARSE_CTXT_T;

typedef struct MKV_CODEC_CFG_VID_T {
  MKV_TRACK_T              *pTrack;

  union {
    AVC_DECODER_CFG_T        *pCfgH264;
  } u;
  VIDEO_DESCRIPTION_GENERIC_T descr; // TODO: currently not utilized

} MKV_CODEC_CFG_VID_T;

typedef struct MKV_CODEC_CFG_AUD_T {
  MKV_TRACK_T              *pTrack;

  union {
    ESDS_DECODER_CFG_T        aacCfg;
    EBML_BLOB_T              *pVorbisCfg;
  } u;
  AUDIO_DESCRIPTION_GENERIC_T descr; // TODO currently not utilized

} MKV_CODEC_CFG_AUD_T;


typedef struct MKV_CONTAINER {
  FILE_STREAM_T            *pStream;
  MKV_PARSE_CTXT_T          parser;

  MKV_HEADER_T              hdr;
  MKV_SEGMENT_T             seg;

  MKV_CODEC_CFG_VID_T       vid; 
  MKV_CODEC_CFG_AUD_T       aud; 

  MKV_BLOCKLIST_T           blocksVid;
  MKV_BLOCKLIST_T           blocksAud;

} MKV_CONTAINER_T;

typedef struct MKV_EXTRACT_STATE {
  MKV_CONTAINER_T *pMkv;
} MKV_EXTRACT_STATE_T;



MKV_CONTAINER_T *mkv_open(const char *path);
void mkv_close(MKV_CONTAINER_T *pMkv);
void mkv_dump(FILE *fp, const MKV_CONTAINER_T *pCtxt);
int mkv_next_block(MKV_CONTAINER_T *pMkv, MKV_BLOCK_T *pBlock);
int mkv_next_block_type(MKV_CONTAINER_T *pMkv, MKV_BLOCK_T *pBlock, MKV_TRACK_TYPE_T type);
int mkv_reset(MKV_CONTAINER_T *pMkv);
int mkv_loadFrame(MKV_CONTAINER_T *pMkv, MP4_MDAT_CONTENT_NODE_T *pFrame,
                  MKV_TRACK_TYPE_T mkvTrackType, unsigned int clockHz, unsigned int *pLastFrameId);

XC_CODEC_TYPE_T mkv_getCodecType(const char *strCodec);
const char *mkv_getCodecStr(XC_CODEC_TYPE_T codecType);

MEDIA_FILE_TYPE_T mkv_isvalidFile(FILE_STREAM_T *pFile);
double mkv_getfps(const MKV_CONTAINER_T *pMkv);

int mkv_loadFrames(MKV_CONTAINER_T *pMkv, void *pArgCb, CB_MKV_READ_FRAME cbMkvOnReadFrame);


#endif // __MKV_H__
