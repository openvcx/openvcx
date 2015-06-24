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


#include "vsx_common.h"


typedef STREAM_CHUNK_T MPEG2_STREAM_CHUNK_T;


int mpg2_check_seq_hdr_start(const unsigned char *pbuf, unsigned int len) {

  if(pbuf && len >= 4 &&
     pbuf[0] == 0x00 && pbuf[1] == 0x00 && pbuf[2] == 0x01 && pbuf[3] == 0xb3) {
    return 1;
  }
  return 0;
}

int mpg2_parse_seq_hdr(const unsigned char *pbuf, unsigned int len, MPEG2_SEQ_HDR_T *pHdr) {

  if(len < 8 || !pHdr) {
    LOG(X_ERROR("Unable to parse mpeg2 sequence header (len:%d)"), len);
    return -1;
  }

  pHdr->hpx = htons((*((uint16_t *) pbuf))) >> 4;
  pHdr->vpx = htons(*((uint16_t *) &pbuf[1])) & 0x0fff;
  pHdr->aspratio = pbuf[3] >> 4;
  pHdr->frmrate = pbuf[3] & 0x0f;
  pHdr->bitrate = htonl(*((uint32_t *) &pbuf[4])) >> 14;
  pHdr->vbufsz = (htons(*((uint16_t *) &pbuf[6])) >> 3) & 0x3ff;

  //fprintf(stdout, "resolution: %u x %u, asp:%u, frm:%u, btr:%u, vbufsz:%u\n", 
  //        hpx, vpx, aspratio, frmrate, bitrate, vbufsz);
  
  return 0;
}

float mpg2_get_fps(const MPEG2_SEQ_HDR_T *pHdr, unsigned int *pnum, 
                   unsigned int *pden) {

  switch(pHdr->frmrate) {
    case 1:
      if(pnum) {
        *pnum = 24000;
      }
      if(pden) {
        *pden= 1001;
      }
      return 23.976f; // 24000/1001
    case 2:
      if(pnum) {
        *pnum = 24;
      }
      if(pden) {
        *pden = 1;
      }
      return 24.0f;
    case 3:
      if(pnum) {
        *pnum = 25;
      }
      if(pden) {
        *pden = 1;
      }
      return 25.0f;
    case 4:
      if(pnum) {
        *pnum = 30000;
      }
      if(pden) {
        *pden = 1001;
      }
      return 29.97f; // 30000/1001
    case 5:
      if(pnum) {
        *pnum = 30;
      }
      if(pden) {
        *pden = 1;
      }
      return 30.0f;
    case 6:
      if(pnum) {
        *pnum = 50;
      }
      if(pden) {
        *pden= 1;
      }
      return 50.0f;
    case 7:
      if(pnum) {
        *pnum = 60000;
      }
      if(pden) {
        *pden = 1001;
      }
      return 59.94f; // 60000/1001
    case 8:
      if(pnum) {
        *pnum = 60;
      }
      if(pden) {
        *pden= 1;
      }
      return 60.0f;
    default:
      if(pnum) {
        *pnum = 0;
      }
      if(pden) {
        *pden= 1;
      }
      return 0.0f;
  }
}

int mpg2_findStartCode(STREAM_CHUNK_T *pStream) {

  unsigned char *buf = &pStream->bs.buf[pStream->bs.byteIdx];
  unsigned int size = pStream->bs.sz - pStream->bs.byteIdx;
  unsigned int idx;

//fprintf(stdout, "%u sz %d, byteIdx %d\n", size, pStream->bs.sz, pStream->bs.byteIdx);

  for(idx = 0; idx < size; idx++) {

    if(buf[idx] == 0x00) {
      pStream->startCodeMatch++;
    } else if(pStream->startCodeMatch >= 2 && buf[idx] == 0x01) {

      pStream->startCodeMatch = 0;
      return ++idx;

    } else if(pStream->startCodeMatch > 0) {
      pStream->startCodeMatch = 0;
    }
  }

  return -1;
}

#define MPEG2_STREAM_PREBUFSZ            64
#define MPEG2_STREAM_BUFSZ               4032

static int dump_nal(MPEG2_STREAM_CHUNK_T *pStream, unsigned int nalIdx, 
                    unsigned int frameIdx, const FILE_OFFSET_T foffset, FILE *fp) {
  MPEG2_SEQ_HDR_T seqHdr;

  unsigned int streamId = pStream->bs.buf[pStream->bs.byteIdx];

  switch(streamId) {
    case MPEG2_HDR_SEQ:

      if(mpg2_parse_seq_hdr(&pStream->bs.buf[pStream->bs.byteIdx+1], 
                         MPEG2_STREAM_PREBUFSZ, &seqHdr) == 0) {
        fprintf(fp, "%u x %u %.3ffps\n", seqHdr.hpx, seqHdr.vpx, 
                   mpg2_get_fps(&seqHdr, NULL, NULL));
  //pHdr->aspratio = pbuf[3] >> 4;
  //pHdr->bitrate = htonl(*((uint32_t *) &pbuf[4])) >> 14;
  //pHdr->vbufsz = (htons(*((uint16_t *) &pbuf[6])) >> 3) & 0x3ff;

      }
      break;
    default:

      break; 
  }

  fprintf(fp, "nal[%u], fr[%u], id:0x%x, file:0x%"LL64"x", 
          nalIdx, frameIdx, streamId, foffset + pStream->bs.byteIdx - 3);

  fprintf(fp, ", [0x%.2x 0x%.2x 0x%.2x 0x%.2x ...]", 
          pStream->bs.buf[pStream->bs.byteIdx+1], 
          pStream->bs.buf[pStream->bs.byteIdx+2], 
          pStream->bs.buf[pStream->bs.byteIdx+3], 
          pStream->bs.buf[pStream->bs.byteIdx+4]);
  //fprintf(fp, "\n");

  return streamId;
}

static int mpg2_parse(MPEG2_CONTAINER_T *pMpg2) {
  unsigned char arena[MPEG2_STREAM_PREBUFSZ + MPEG2_STREAM_BUFSZ];
  MPEG2_STREAM_CHUNK_T stream; 
  int lenRead;
  int idxStartCode;
  FILE_OFFSET_T foffset;
  FILE_OFFSET_T foffsetPrevNal;
  unsigned int frameSize = 0;
  unsigned int nalSize = 0;
  //int havePriorFrame = 0;
  unsigned int streamId = 0;
  unsigned int streamIdPrev = 0;
  unsigned int nalIdx = 0;
  unsigned int frameIdx = 0;

//unsigned int naltype[0xff];
//memset(&naltype, 0, sizeof(naltype));

  if(!pMpg2 || !pMpg2->pStream || pMpg2->pStream->fp == FILEOPS_INVALID_FP) {
    return -1;
  }

  if((lenRead = ReadFileStreamNoEof(pMpg2->pStream, arena, sizeof(arena))) < 0) {
    return -1;
  }

  memset(&stream, 0, sizeof(stream.bs));
  //stream.bs.emulPrevSeqLen = 3;
  //stream.bs.emulPrevSeq[2] = 0x03;
  stream.bs.buf = arena;
  stream.bs.sz = ((lenRead > sizeof(arena) - MPEG2_STREAM_PREBUFSZ) ?
                  (sizeof(arena) - MPEG2_STREAM_PREBUFSZ) : lenRead);

  if(mpg2_findStartCode(&stream) != 3) {
    LOG(X_ERROR("No valid start code at start of stream"));
    return -1;
  }


  foffsetPrevNal = stream.bs.byteIdx;
  streamIdPrev = 0;
  streamId = arena[3];
  stream.bs.sz -= 3;
  stream.bs.byteIdx += 3;
  //foffset = stream.bs.byteIdx;
  foffset = 0;
  dump_nal(&stream, nalIdx, frameIdx, foffset, stdout);
  stream.bs.sz--;
  stream.bs.byteIdx++;
  nalIdx++;

  do {

    if(stream.bs.byteIdx >= stream.bs.sz) {

      memcpy(arena, &arena[sizeof(arena)] - MPEG2_STREAM_PREBUFSZ, MPEG2_STREAM_PREBUFSZ);
      foffset = pMpg2->pStream->offset - MPEG2_STREAM_PREBUFSZ;
      //fprintf(stdout, "reading 0x%x (0x%x)\n", foffset, pMpg2->pStream->offset);
      if((lenRead = ReadFileStreamNoEof(pMpg2->pStream, &arena[MPEG2_STREAM_PREBUFSZ],
                sizeof(arena) - MPEG2_STREAM_PREBUFSZ)) < 0) {
        return -1;
      }
      stream.bs.byteIdx = 0;
      stream.bs.bitIdx = 0;
      stream.bs.sz = lenRead;
    }

    if(pMpg2->pStream->offset == pMpg2->pStream->size) {
      stream.bs.sz += MPEG2_STREAM_PREBUFSZ;
    }

    while((idxStartCode = mpg2_findStartCode(&stream)) >= 0) {

      nalSize = (unsigned int) (foffset + (stream.bs.byteIdx + idxStartCode - 3) 
                                - foffsetPrevNal);
      fprintf(stdout, ", size: %u\n", nalSize);
      foffsetPrevNal = foffset + stream.bs.byteIdx + idxStartCode - 3;
      frameSize += nalSize;

      stream.bs.byteIdx += idxStartCode;

      if((streamId > 0 && streamId <= 0xaf) &&
         (stream.bs.buf[stream.bs.byteIdx] == 0 || 
         stream.bs.buf[stream.bs.byteIdx] > 0xaf)) {
        //if(havePriorFrame) {
          fprintf(stdout, "frame size:%u\n", frameSize);
          frameSize = 0;
          frameIdx++;
        //}
        //havePriorFrame = 1;
      }

      streamId = dump_nal(&stream, nalIdx, frameIdx, foffset, stdout);

//fprintf(stdout, "nal %u (id:0x%x) at 0x%llx", nalIdx, streamId, foffset+stream.bs.byteIdx);
//fprintf(stdout, "  0x%.2x 0x%.2x 0x%.2x 0x%.2x", stream.bs.buf[stream.bs.byteIdx], stream.bs.buf[stream.bs.byteIdx+1], stream.bs.buf[stream.bs.byteIdx+2], stream.bs.buf[stream.bs.byteIdx+3]);
//fprintf(stdout, "\n");
//naltype[streamId]++;
      nalIdx++;
    }

    stream.bs.byteIdx = stream.bs.sz;

  } while(pMpg2->pStream->offset < pMpg2->pStream->size);

  nalSize = (unsigned int) (pMpg2->pStream->size - foffsetPrevNal);
  frameSize += nalSize;
  fprintf(stdout, ", size: %u\n", nalSize);
  //if(havePriorFrame) {
    fprintf(stdout, "frame size: %u\n", frameSize);
  //}

//for(streamId = 0; streamId<0xff; streamId++) {
//if(naltype[streamId]>0) {
//fprintf(stdout, "naltype[0x%x]:%d\n", streamId, naltype[streamId]);
//}
//}

  return 0;
}

MPEG2_CONTAINER_T *mpg2_open(const char *path) {
  FILE_STREAM_T *pStreamIn = NULL;
  MPEG2_CONTAINER_T *pMpg2 = NULL;

  if((pStreamIn = (FILE_STREAM_T *) avc_calloc(1, sizeof(FILE_STREAM_T))) == NULL) {
    return NULL;
  }

  if(OpenMediaReadOnly(pStreamIn, path) < 0) {
    free(pStreamIn);
    return NULL;
  }

  if((pMpg2 = (MPEG2_CONTAINER_T *) avc_calloc(1, sizeof(MPEG2_CONTAINER_T))) == NULL) {
    CloseMediaFile(pStreamIn);
    free(pStreamIn);
    return NULL;
  }

  pMpg2->pStream = pStreamIn;

  if(mpg2_parse(pMpg2) < 0) {
    mpg2_close(pMpg2); 
    return NULL;
  }

  return pMpg2; 
}

void mpg2_close(MPEG2_CONTAINER_T *pMpg2) {
  if(pMpg2) {

   if(pMpg2->pStream) {
      CloseMediaFile(pMpg2->pStream);
      free(pMpg2->pStream);
    }

    free(pMpg2);

  }
}
