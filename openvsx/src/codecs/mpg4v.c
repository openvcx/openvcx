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

//#if defined(ANDROID) 
//#include <math.h>
//#endif // ANDRDOID

int mpg4v_write_sample_to_mp4(FILE_STREAM_T *pStreamOut, MP4_MDAT_CONTENT_NODE_T *pSample,
                              unsigned char *arena, unsigned int sz_arena);

int mpg4v_writeStart(FILE_STREAM_T *pStreamOut, const unsigned char *pData,
                     unsigned int len) {

  if(WriteFileStream(pStreamOut, pData, len) < 0) {
    return -1;
  }

  return 0;
}

int mpg4v_cbWriteSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {

  MPG4V_CBDATA_WRITE_SAMPLE_T *pCbDataMpg4v = NULL;
  unsigned int idxByteInSample = 0;
  unsigned int lenRead;

  if((pCbDataMpg4v = (MPG4V_CBDATA_WRITE_SAMPLE_T *) pCbData) == NULL) {
    return -1;
  }
//static int xxx;
//fprintf(stderr, "extracting mp4v sample len:%d [%d]\n", sampleSize, xxx++);;
  while(idxByteInSample < sampleSize) {

    if((lenRead = sampleSize - idxByteInSample) > sizeof(pCbDataMpg4v->arena)) {
      lenRead = sizeof(pCbDataMpg4v->arena);
    }

    if(ReadFileStream(pStreamIn, pCbDataMpg4v->arena, lenRead) < 0) {
      return -1;
    }

    if(WriteFileStream(pCbDataMpg4v->pStreamOut, pCbDataMpg4v->arena, lenRead) < 0) {
      return -1;
    }

    idxByteInSample += lenRead;

  }

  return idxByteInSample;
}






#define MPG4V_STREAM_PREBUFSZ            64
#define MPG4V_STREAM_BUFSZ               4032


int mpg4v_decodeVOL(BIT_STREAM_T *pStream, MPG4V_DESCR_PARAM_T *pParam) {

  int vo_type;
  int vo_verid;
  int aspect_ratio;
  int vol_shape;
  int timebase_num;
  int timebase_den;
  int timeincr;
  int width;
  int height;

#define AR_EXT 15
#define MPEG_SHAPE_RECT  0
#define MPEG_SHAPE_GRAY  3


  //avc_dumpHex(stderr, pStream->buf[pStream->byteIdx + idx], 16, 0); 

  bits_read(pStream, 8);                            // start code

  bits_read(pStream, 1);                            // random access bit
  vo_type = bits_read(pStream, 8);                  // vol type
  //fprintf(stdout, "vo_type: %d\n", vo_type);
  if(bits_read(pStream, 1) != 0) {                  // vol id
    vo_verid = bits_read(pStream, 4);               // ve version
    bits_read(pStream, 3);                          // vo priority
    //fprintf(stdout, "vo_verid:%d\n", vo_verid);
  } else {
    vo_verid = 1;
    //fprintf(stdout, "default vo_verid:%d\n", vo_verid);
  }

  aspect_ratio = bits_read(pStream, 4);

  if(aspect_ratio == AR_EXT){
    bits_read(pStream, 8);                          // par width (numerator)
    bits_read(pStream, 8);                          // par height (denominator)
  } else{
                                                    // lookup par table [0-15] 
        //s->avctx->sample_aspect_ratio= pixel_aspect[s->aspect_ratio_info];
  }

                                                    // vol control param
  if(bits_read(pStream, 1)) {
    bits_read(pStream, 2);                          // chroma format
    bits_read(pStream, 1);                          // low delay 
    if(bits_read(pStream, 1)) {                     // vbv params
      bits_read(pStream, 15);                       // first half bitrate 
      bits_read(pStream, 1);                        // marker bit
      bits_read(pStream, 15);                       // second half bitrate 
      bits_read(pStream, 1);                        // marker bit
      bits_read(pStream, 15);                       // first half vbv buf size
      bits_read(pStream, 1);                        // marker bit
      bits_read(pStream, 3);                        // second half vbv buf size
      bits_read(pStream, 11);                       // first half vbv occupancy 
      bits_read(pStream, 1);                        // marker bit
      bits_read(pStream, 15);                       // second half vbv buf size
      bits_read(pStream, 1);                        // marker bit
    }

  } else {

  }

  vol_shape = bits_read(pStream, 2);                // vol shape

  //if(vol_shape != MPEG_SHAPE_RECT) {
  //  LOG(X_WARNING("mpeg-4 VOL non-rect shape not supported"));
  //  return -1;
  //}
  if(vol_shape == MPEG_SHAPE_GRAY && vo_verid != 1) {
    bits_read(pStream, 4);                          // shape extension
  }

  bits_read(pStream, 1);                            // marker bit 

  if((timebase_den = bits_read(pStream, 16)) == 0) {
    LOG(X_WARNING("mpeg-4 VOL time base 0"));
    return -1;
  }

  //if((timeincr = (int) log2(timebase_den - 1) + 1) < 1) {
  if((timeincr = (int) math_log2_32(timebase_den - 1) + 1) < 1) {
    timeincr = 1;
  }

  //fprintf(stderr, "LOG2: %d  %d\n", timebase_den-1, (int) log2(timebase_den - 1));

  bits_read(pStream, 1);                            // marker bit

  if(bits_read(pStream, 1)) {                       // vop rate
    timebase_num = bits_read(pStream, timeincr);
  } else {
    timebase_num = 1;
  }

  //fprintf(stdout, "mpg4v vol time: %d / %d\n", timebase_num, timebase_den);

  if(vol_shape == MPEG_SHAPE_RECT) {
    bits_read(pStream, 1);                          // marker bit
    width = bits_read(pStream, 13);   
    bits_read(pStream, 1);                          // marker bit
    height = bits_read(pStream, 13);   
    bits_read(pStream, 1);                          // marker bit

  } else {
    LOG(X_WARNING("mpeg-4 VOL non-rect shape not supported"));
    return -1;
  }

  pParam->frameWidthPx = width;
  pParam->frameHeightPx = height;
  pParam->clockHz = timebase_den;
  pParam->frameDeltaHz = timebase_num;

//pParam->clockHz=15;
  LOG(X_DEBUGV("VOL fps: %d/%d, %dx%d"), pParam->clockHz, pParam->frameDeltaHz, width, height);

  return 0;
}

static int mpg4v_decodeVOS(BIT_STREAM_T *pStream, MPG4V_SEQ_HDRS_T *pSeqHdrs) {
  int rc = 0;
  int prof_lvl;
  bits_read(pStream, 8);                            // start code

  prof_lvl = bits_read(pStream, 8);

  pSeqHdrs->profile = (prof_lvl >> 4) & 0x0f;
  pSeqHdrs->level = prof_lvl & 0x0f;
  pSeqHdrs->have_profile_level = 1;

  LOG(X_DEBUGV("VOS profile:%d level:%d (0x%x)"), pSeqHdrs->profile, pSeqHdrs->level, prof_lvl); 

  return rc;
}


int mpg4v_decodeObj(const STREAM_CHUNK_T *pStream, MPG4V_DESCR_T *pMpg4v) {
  int rc = 0;
  BIT_STREAM_T bs;

  pMpg4v->ctxt.startCode = pStream->bs.buf[pStream->bs.byteIdx];

  pMpg4v->ctxt.lastFrameType = MPG4V_FRAME_TYPE_INVALID;

  //fprintf(stderr, "mpg4v slice 0x%x 0x%x (%d) frame[%d]\n", pMpg4v->ctxt.startCode, pStream->bs.buf[pStream->bs.byteIdx+3], pStream->bs.sz -pStream->bs.byteIdx, pMpg4v->ctxt.frameIdx);

  if(pMpg4v->ctxt.startCode == MPEG4_HDR8_VOS ||
     pMpg4v->ctxt.startCode == MPEG4_HDR8_VO ||
     pMpg4v->ctxt.startCode == MPEG4_HDR8_USERDATA ||
     pMpg4v->ctxt.startCode == 0x00 ||
     MPEG4_ISVOL(pMpg4v->ctxt.startCode)) {

    if(MPEG4_ISVOL(pMpg4v->ctxt.startCode)) {
      memcpy(&bs, &pStream->bs, sizeof(bs));
      mpg4v_decodeVOL(&bs, &pMpg4v->param);

      if(((double)pMpg4v->param.clockHz / pMpg4v->param.frameDeltaHz) > MPEG4_MAX_FPS_SANITY) {
        LOG(X_WARNING("VOL may contain incorrect frame rate %d/%d"),
                 pMpg4v->param.clockHz, pMpg4v->param.frameDeltaHz);
      }

    } else if(pMpg4v->ctxt.startCode == MPEG4_HDR8_VOS) {
      memcpy(&bs, &pStream->bs, sizeof(bs));
      mpg4v_decodeVOS(&bs, &pMpg4v->seqHdrs);
    }
    
    pMpg4v->ctxt.flag = MPG4V_DECODER_FLAG_HDR;

    //fprintf(stderr, "FRAME IDX NOT CHANGED -A flag:0x%x\n", pMpg4v->ctxt.flag);

  } else if(pMpg4v->ctxt.startCode == MPEG4_HDR8_GOP) {

    pMpg4v->ctxt.frameIdx++;
    pMpg4v->ctxt.lastFrameType = MPG4V_FRAME_TYPE_I;
    pMpg4v->ctxt.flag = MPG4V_DECODER_FLAG_VIDGOPSTART;

    //fprintf(stderr, "FRAME IDX NOW (GOP)[%d] flag:0x%x\n", pMpg4v->ctxt.frameIdx, pMpg4v->ctxt.flag);

  } else if(pMpg4v->ctxt.startCode == MPEG4_HDR8_VOP) {

    if(pStream->bs.byteIdx + 1 < pStream->bs.sz) {
      // The first 2 bits are the frame type I,P,B
      pMpg4v->ctxt.lastFrameType = (MPG4V_FRAME_TYPE_T)
        ((pStream->bs.buf[pStream->bs.byteIdx + 1] >> 6) & 0x03) + 1;
      //fprintf(stderr, "vop fr type: 0x%x\n", ((pStream->bs.buf[pStream->bs.byteIdx + 1] >> 6) & 0x03)); 
    }

    if(pMpg4v->ctxt.flag != MPG4V_DECODER_FLAG_VIDGOPSTART) {
      pMpg4v->ctxt.frameIdx++;
      //fprintf(stderr, "FRAME IDX NOW-2[%d] flag:0x%x -> 0x%x\n", pMpg4v->ctxt.frameIdx, pMpg4v->ctxt.flag, MPG4V_DECODER_FLAG_VID);
    } else {
      //fprintf(stderr, "FRAME IDX NOT CHANGED -B flag:0x%x -> 0x%x\n", pMpg4v->ctxt.flag, MPG4V_DECODER_FLAG_VID);
    }
    pMpg4v->ctxt.flag = MPG4V_DECODER_FLAG_VID;

  } else {
    //fprintf(stderr, "FRAME IDX NOT CHANGED -C flag:0x%x\n", pMpg4v->ctxt.flag);
    pMpg4v->ctxt.flag = MPG4V_DECODER_FLAG_UNKNOWN;
  }

  //fprintf(stderr, "decode: (0x%x) 0x%x 0x%x 0x%x 0x%x fr:%d, flag:0x%x\n", pMpg4v->ctxt.startCode, pStream->bs.buf[pStream->bs.byteIdx+1], pStream->bs.buf[pStream->bs.byteIdx+2], pStream->bs.buf[pStream->bs.byteIdx+3], pStream->bs.buf[pStream->bs.byteIdx+4], pMpg4v->ctxt.frameIdx, pMpg4v->ctxt.flag);
 
  return rc;
}

static int mpg4v_decodeObjects(MPG4V_DESCR_T *pMpg4v,
                               const unsigned char *pData, unsigned int len) {
  STREAM_CHUNK_T stream;
  int idxStartCode;
  int rc = 0;

  memset(&stream, 0, sizeof(stream));
  stream.bs.buf = (unsigned char *) pData;
  stream.bs.sz = len;

  if(mpg2_findStartCode(&stream) != 3) {
    return -1;
  }

  stream.bs.sz -= 3;
  stream.bs.byteIdx += 3;

  do {

    if(mpg4v_decodeObj(&stream, pMpg4v) < 0) {
      break;
    }

    if((idxStartCode = mpg2_findStartCode(&stream)) <= 0) {
      break;
    }

    stream.bs.byteIdx += idxStartCode;
    rc++;

  } while(stream.bs.byteIdx < stream.bs.sz);

  return rc;
}

int mpg4v_appendStartCodes(MPG4V_DESCR_T *pMpg4v, unsigned char *pData, unsigned int len) {

  pMpg4v->seqHdrs.hdrsBufSz = sizeof(pMpg4v->seqHdrs.hdrsBuf);

  if(pMpg4v->seqHdrs.hdrsLen + len > pMpg4v->seqHdrs.hdrsBufSz) {
    LOG(X_WARNING("MPEG-4 start codes truncated from %d  to %d"), 
        len, pMpg4v->seqHdrs.hdrsBufSz - pMpg4v->seqHdrs.hdrsLen);
    len = pMpg4v->seqHdrs.hdrsBufSz - pMpg4v->seqHdrs.hdrsLen;
  }

  if(len > 0) {
    memcpy(&pMpg4v->seqHdrs.hdrsBuf[pMpg4v->seqHdrs.hdrsLen], pData, len);
    pMpg4v->seqHdrs.hdrsLen += len;
  }

  return 0;
}


static int attachSample(MPG4V_DESCR_T *pMpg4v, FILE_OFFSET_T fileOffset, 
                       unsigned char *pSample, unsigned int lenSample,
                       uint32_t sampleDurationHz) {

  //fprintf(stderr, "attach startcode:0x%x len:%d ishdr:%d, flag:0x%x\n", pMpg4v->ctxt.startCode, lenSample, pMpg4v->ctxt.flag == MPG4V_DECODER_FLAG_HDR, pMpg4v->ctxt.flag);

  if(pMpg4v->ctxt.flag == MPG4V_DECODER_FLAG_HDR) {

    if(pMpg4v->seqHdrs.hdrsLen == 0 && pMpg4v->ctxt.startCode == MPEG4_HDR8_VOS) {
      pMpg4v->ctxt.storingHdr = 1; 
    }

    if(pMpg4v->ctxt.storingHdr) {

      if(pSample) {
        mpg4v_appendStartCodes(pMpg4v, pSample, lenSample);
      } else {
        // its possible that the header data spans across 2 read buffer calls
        LOG(X_WARNING("Unable to store mpeg-4 header data."));
      }

      if(pMpg4v->seqHdrs.hdrsLen + lenSample > pMpg4v->seqHdrs.hdrsBufSz) {
        pMpg4v->ctxt.storingHdr = 0; 
      }

    }

    //TODO: this may be redundant with hdrsLen
    //TODO: this is an extremely bad hack to use the length
    if(pMpg4v->ctxt.storedHdr && lenSample < 128) {
      //fprintf(stderr, "ret 0...\n");
      return 0;
    }

  } else if(pMpg4v->numSamples >= pMpg4v->content.samplesBufSz) {
    LOG(X_ERROR("mpeg-4 object index[%d] exceeds allocated amount [%d]"),
        pMpg4v->numSamples, pMpg4v->content.samplesBufSz);
    return -1;
  } else if(!pMpg4v->pStreamIn) {
    return -1;
  }

  //
  // mpg4v_decodeObj will not increment theframeIdx on a GOP slice because it
  // does not know have > 5 bytes to see if there are successive video frame data
  // proceeding the GOP slice.  Assume if the GOP slice is greater than x, there is
  // a non-GOP video slice proceeding.
  //
//fprintf(stderr, "check if to increment frameIdx flag:0x%x, lenS:%d, storedHdr:%d, hdrsLen:%d\n", pMpg4v->ctxt.flag, lenSample, pMpg4v->ctxt.storedHdr, pMpg4v->seqHdrs.hdrsLen);
  if((pMpg4v->ctxt.flag == MPG4V_DECODER_FLAG_VIDGOPSTART) && lenSample > 32) {
    pMpg4v->ctxt.flag = MPG4V_DECODER_FLAG_VID;
    pMpg4v->ctxt.frameIdx++;
  } else if((pMpg4v->ctxt.flag == MPG4V_DECODER_FLAG_HDR) && lenSample > 32 && pMpg4v->ctxt.storedHdr) {
    pMpg4v->ctxt.frameIdx++;
  }

  if(pMpg4v->ctxt.storingHdr) {
    //fprintf(stderr, "turning off hdr storage\n");
    pMpg4v->ctxt.storingHdr = 0;
  }
  if(pMpg4v->seqHdrs.hdrsLen > 0) {
    pMpg4v->ctxt.storedHdr = 1;
  }

  if(pMpg4v->content.pSampleCbPrev == NULL) { 
    //fprintf(stderr, "attaching 1st sample, len:%d\n", lenSample);
    pMpg4v->content.pSampleCbPrev = pMpg4v->content.pSamplesBuf;
  } else {
    //fprintf(stderr, "attaching sample->next to [%d], len:%d \n", pMpg4v->numSamples+1, lenSample);
    //static int g_num_samples; if(g_num_samples++ %20==5) { return 0;}
    pMpg4v->content.pSampleCbPrev->pnext = &pMpg4v->content.pSamplesBuf[pMpg4v->numSamples];
    pMpg4v->content.pSampleCbPrev = &pMpg4v->content.pSamplesBuf[pMpg4v->numSamples];
  }
  pMpg4v->numSamples++;
  //fprintf(stderr, "mp4v samples %d/%d\n", pMpg4v->numSamples, pMpg4v->content.samplesBufSz);
  pMpg4v->content.pSampleCbPrev->fileOffset = fileOffset;
  pMpg4v->content.pSampleCbPrev->sizeRd = lenSample;
  pMpg4v->content.pSampleCbPrev->sizeWr = lenSample;
  pMpg4v->content.pSampleCbPrev->frameId = pMpg4v->ctxt.frameIdx;
  pMpg4v->content.pSampleCbPrev->playOffsetHz = pMpg4v->lastPlayOffsetHz;
  pMpg4v->content.pSampleCbPrev->playDurationHz = sampleDurationHz;
  //fprintf(stderr, "playduration:%d %d/%d\n", pMpg4v->clockHzOverride, pMpg4v->param.clockHz, pMpg4v->param.frameDeltaHz);
  pMpg4v->content.pSampleCbPrev->pStreamIn  = pMpg4v->pStreamIn;
  pMpg4v->content.pSampleCbPrev->cbWriteSample = mpg4v_write_sample_to_mp4;
  if(pMpg4v->ctxt.lastFrameType == MPG4V_FRAME_TYPE_I) {
    pMpg4v->content.pSampleCbPrev->flags |= MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE;
  } else {
    pMpg4v->content.pSampleCbPrev->flags = 0;
  }

  //fprintf(stderr, "attached mpg4 node len:%d, frameIdx:%d, flags:0x%x, fileOffset:0x%x\n", lenSample, pMpg4v->ctxt.frameIdx, pMpg4v->content.pSampleCbPrev->flags, fileOffset);

  pMpg4v->lastFrameId = pMpg4v->ctxt.frameIdx;

  return 1;
}

static int attachMp4vSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                            void *pCbData, int chunk_idx, uint32_t sampleDurationHz) {

  CBDATA_ATTACH_MP4V_SAMPLE_T *pCbDataMp4v = (CBDATA_ATTACH_MP4V_SAMPLE_T *) pCbData;
  MPG4V_DESCR_T *pMpg4v = NULL;
  STREAM_CHUNK_T stream;
  FILE_OFFSET_T foffset;
  unsigned int szread = 4;
  int rc = 0;

  if(pCbDataMp4v == NULL || 
     (pMpg4v = (MPG4V_DESCR_T *) pCbDataMp4v->pMpg4v) == NULL) {
    return -1;
  }

  if(pMpg4v->content.pSamplesBuf) {

    if(sampleSize >= 4) {

      foffset = pStreamIn->offset;
      if(sampleSize > 4) {
        // Read the 4 byte header + 1st byte of data
        szread = 5;
      }
      if(ReadFileStream(pStreamIn, pCbDataMp4v->arena, szread) < 0) {
        return -1;
      }

      //fprintf(stderr, "ATTACHMP4VSAMPLE[%d] LEN:%d duration:%d\n", pMpg4v->numSamples, sampleSize, sampleDurationHz); 
      //avc_dumpHex(stderr, pCbDataMp4v->arena, szread, 1);

//fprintf(stderr, "read sz:%d [%d] 0x%x 0x%x 0x%x 0x%x\n", sampleSize, pMpg4v->numSamples, pCbDataMp4v->arena[0], pCbDataMp4v->arena[1], pCbDataMp4v->arena[2], pCbDataMp4v->arena[3]);

      memset(&stream, 0, sizeof(stream));
      stream.bs.buf = pCbDataMp4v->arena;
      stream.bs.byteIdx = 3;
      stream.bs.sz = szread;
      mpg4v_decodeObj(&stream, pCbDataMp4v->pMpg4v);

      if((rc = attachSample(pCbDataMp4v->pMpg4v, foffset, NULL, sampleSize, 
                            sampleDurationHz)) < 0) {
        return rc;
      } else if(rc > 0) {
        pMpg4v->content.pSampleCbPrev->chunk_idx = chunk_idx;
      }

    } else {
      LOG(X_WARNING("Ignoring short mp4v sample length:%d"), sampleSize);
    }

  } else {
    pMpg4v->numSamples++;
  }

  pMpg4v->lastPlayOffsetHz += sampleDurationHz;

  return rc;
}

static int attachMp4vSample_mp4wrap(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                                    void *pCbData, int chunk_idx, uint32_t sampleDurationHz) {
  return attachMp4vSample((FILE_STREAM_T *) pStreamIn->pCbData, sampleSize, 
                          pCbData, chunk_idx, sampleDurationHz);
}

int mpg4v_write_sample_to_mp4(FILE_STREAM_T *pStreamOut, MP4_MDAT_CONTENT_NODE_T *pSample,
                             unsigned char *arena, unsigned int sz_arena) {
  int rc = 0;
  unsigned int lenbody;
  unsigned int lenread;

  if(SeekMediaFile(pSample->pStreamIn, pSample->fileOffset, SEEK_SET) != 0) {
    return -1;
  }

  lenbody = pSample->sizeRd;
//static int xxx;
//fprintf(stderr, "write sample [%d] to file:0x%x size:%d from file: 0x%x %s\n", xxx++,pStreamOut->offset, lenbody, pSample->pStreamIn->offset, pSample->pStreamIn->filename);

  while(lenbody > 0) {

    lenread = lenbody > (int) sz_arena ? sz_arena : lenbody;

    if(ReadFileStream(pSample->pStreamIn, arena, lenread) < 0) {
      return -1;
    }
//if(rc == 0)fprintf(stderr, "writing mp4v sample len:%d, fr:%d, 0x%x 0x%x 0x%x 0x%x\n", lenbody, pSample->frameId, arena[0], arena[1], arena[2], arena[3]);
    if(WriteFileStream(pStreamOut, arena, lenread) < 0) {
      return -1;
    }
    rc += lenread;
    lenbody -= lenread;
  }

  return rc;
}


MP4_MDAT_CONTENT_NODE_T *mpg4v_cbGetNextSample(void *pArg, int idx, int advanceFp) {
  MPG4V_DESCR_T *pMpg4v = (MPG4V_DESCR_T *) pArg;
  
  if(!pMpg4v || !pMpg4v->content.pSamplesBuf) {
    return NULL;
  }

  if(idx == 0) {
    pMpg4v->content.pSampleCbPrev = pMpg4v->content.pSamplesBuf; 
  } else if(pMpg4v->content.pSampleCbPrev) {
    pMpg4v->content.pSampleCbPrev = pMpg4v->content.pSampleCbPrev->pnext;
  } else {
    pMpg4v->content.pSampleCbPrev = NULL;
  }

  return pMpg4v->content.pSampleCbPrev;
}

static int readSamples(MPG4V_DESCR_T *pMpg4v) {

  unsigned char arena[MPG4V_STREAM_PREBUFSZ + MPG4V_STREAM_BUFSZ];
  STREAM_CHUNK_T stream;
  FILE_OFFSET_T foffset = 0;
  FILE_OFFSET_T foffsetPrev = 0;
  FILE_OFFSET_T foffsetSample = 0;
  int lenRead;
  int idxStartCode;
  unsigned char *pSampleStart = NULL;

  if((lenRead = ReadFileStreamNoEof(pMpg4v->pStreamIn, arena, sizeof(arena))) < 0) {
    return -1;
  }

  memset(&stream, 0, sizeof(stream));
  stream.bs.buf = arena;
  stream.bs.sz = ((lenRead > sizeof(arena) - MPG4V_STREAM_PREBUFSZ) ?
                  (sizeof(arena) - MPG4V_STREAM_PREBUFSZ) : lenRead);
  foffset = stream.bs.sz;
  pSampleStart = stream.bs.buf;

  if(mpg2_findStartCode(&stream) != 3) {
    LOG(X_ERROR("No valid start code at start of stream"));
    return -1;
  }

  stream.bs.sz -= 3;
  stream.bs.byteIdx += 3;
  if(pMpg4v->content.pSamplesBuf) {
    mpg4v_decodeObj(&stream, pMpg4v);
  }

  do {

    if(stream.bs.byteIdx >= stream.bs.sz) {
      memcpy(arena, &arena[sizeof(arena)] - MPG4V_STREAM_PREBUFSZ, 
             MPG4V_STREAM_PREBUFSZ);
      //fprintf(stdout, "reading at file: 0x%x\n", pStreamIn->offset);
      if((lenRead = ReadFileStreamNoEof(pMpg4v->pStreamIn, &arena[MPG4V_STREAM_PREBUFSZ],
                sizeof(arena) - MPG4V_STREAM_PREBUFSZ)) < 0) {
        return -1;
      }
      foffsetPrev = foffset;
      foffset += lenRead;
      stream.bs.byteIdx = 0;
      stream.bs.bitIdx = 0;
      stream.bs.sz = lenRead;
      pSampleStart = NULL;

    }

    if(pMpg4v->pStreamIn->offset == pMpg4v->pStreamIn->size) {
      stream.bs.sz += MPG4V_STREAM_PREBUFSZ;
    }

    while((idxStartCode = mpg2_findStartCode(&stream)) >= 0) {
      stream.bs.byteIdx += idxStartCode;

//if(pMpg4v->content.pSamplesBuf)fprintf(stdout, "rc:%d nal %d file: 0x%x len:%d (0x%x)\n", idxStartCode, pMpg4v->numSamples, foffsetSample, foffsetPrev + stream.bs.byteIdx -3 - foffsetSample, pMpg4v->ctxt.startCode);

      if(pMpg4v->content.pSamplesBuf) {
        if(attachSample(pMpg4v, foffsetSample, pSampleStart,
                  foffsetPrev + stream.bs.byteIdx -3 - foffsetSample, 0) < 0) {
          return -1;
        }
        mpg4v_decodeObj(&stream, pMpg4v);
      } else {
        pMpg4v->numSamples++;
      }

      foffsetSample = foffsetPrev + stream.bs.byteIdx - 3;
      if(stream.bs.byteIdx >= 3) {
        pSampleStart = &stream.bs.buf[stream.bs.byteIdx - 3];
      } else {
        pSampleStart = NULL;
      }
    }

    stream.bs.byteIdx = stream.bs.sz;

  } while(pMpg4v->pStreamIn->offset < pMpg4v->pStreamIn->size);

//if(pMpg4v->content.pSamplesBuf)fprintf(stdout, "nal %d file: 0x%x len:%d \n", pMpg4v->numSamples, foffsetSample, pStreamIn->size - foffsetSample);

  if(pMpg4v->content.pSamplesBuf) {
    if(attachSample(pMpg4v, foffsetSample, pSampleStart, 
                    pMpg4v->pStreamIn->size - foffsetSample, 0) < 0) {
      return -1;
    }
  } else {
   pMpg4v->numSamples++;
  }


  return 0;
}

static int allocSamplesBuf(MPG4V_DESCR_T *pMpg4v) {

  if(pMpg4v->numSamples > 0) {
//fprintf(stderr, "allocating %d samples\n", pMpg4v->numSamples);
    if((pMpg4v->content.pSamplesBuf = (MP4_MDAT_CONTENT_NODE_T *)
      avc_calloc((unsigned int) pMpg4v->numSamples, sizeof(MP4_MDAT_CONTENT_NODE_T))) == NULL) {
      return -1;
    }
    pMpg4v->content.samplesBufSz = pMpg4v->numSamples;
  } else {
    LOG(X_WARNING("No mp4v samples found"));
    return -1;
  }

  return 0;
}

int mpg4v_getSamples(MPG4V_DESCR_T *pMpg4v) {
  int rc;

  if(!pMpg4v || !pMpg4v->pStreamIn) {
    return -1;
  }

  LOG(X_DEBUG("Reading frames from %s"), pMpg4v->pStreamIn->filename);

  if(pMpg4v->content.pSamplesBuf || pMpg4v->seqHdrs.hdrsLen > 0) {
    mpg4v_free(pMpg4v);
  }

  if(SeekMediaFile(pMpg4v->pStreamIn, 0, SEEK_SET) != 0) {;
    return -1;
  }

  memset(&pMpg4v->ctxt, 0, sizeof(pMpg4v->ctxt));
  if((rc = readSamples(pMpg4v)) < 0) {
    return -1;
  }

  if(allocSamplesBuf(pMpg4v) < 0) {
    mpg4v_free(pMpg4v);
    return -1;
  }

  if(SeekMediaFile(pMpg4v->pStreamIn, 0, SEEK_SET) != 0) {;
    return -1;
  }

  memset(&pMpg4v->ctxt, 0, sizeof(pMpg4v->ctxt));
  pMpg4v->numSamples = 0;
  pMpg4v->seqHdrs.hdrsLen = 0;

  if((rc = readSamples(pMpg4v)) < 0) {
    return -1;
  }

  pMpg4v->lastPlayOffsetHz = pMpg4v->param.frameDeltaHz * pMpg4v->ctxt.frameIdx;

  return rc;
}


int mpg4v_createSampleListFromMp4(MPG4V_DESCR_T *pMpg4v, MP4_CONTAINER_T *pMp4,
                                  CHUNK_SZ_T *pStchk, float fStartSec) {

  MP4_TRAK_MP4V_T mp4BoxSet;
  uint64_t startHz = 0;

  if(!pMpg4v || !pMp4 || !pMp4->pStream) {
    return -1;
  }

  LOG(X_DEBUG("Reading mpg4v samples from %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  if(mp4_initMp4vTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  if(fStartSec > 0.0 && mp4BoxSet.tk.pMdhd->timescale == 0) {
    LOG(X_WARNING("Unable to do partial video extract"));
  } else {
    startHz = (uint64_t) (fStartSec * mp4BoxSet.tk.pMdhd->timescale);
  }

  memset(&pMpg4v->ctxt, 0, sizeof(pMpg4v->ctxt));
  pMpg4v->mp4vCbData.pMpg4v = pMpg4v;

  // Attach the start code sequence from the esds box
  pMpg4v->seqHdrs.hdrsLen = 0;
  mpg4v_appendStartCodes(pMpg4v, mp4BoxSet.pEsds->specific.startcodes, 
                   mp4BoxSet.pEsds->specific.type.length);  
  mpg4v_decodeObjects(pMpg4v, pMpg4v->seqHdrs.hdrsBuf, pMpg4v->seqHdrs.hdrsLen);

  pMpg4v->param.clockHz = mp4BoxSet.tk.pMdhd->timescale;
  pMpg4v->param.frameDeltaHz = mp4_getSttsSampleDelta(&mp4BoxSet.tk.pStts->list);
  LOG(X_DEBUG("Using mp4 fps %d/%d"), pMpg4v->param.clockHz, pMpg4v->param.frameDeltaHz);

  if(pMpg4v->param.clockHz != mp4BoxSet.tk.pMdhd->timescale) {
    pMpg4v->clockHzOverride = mp4BoxSet.tk.pMdhd->timescale;
  } else {
    pMpg4v->clockHzOverride = pMpg4v->param.clockHz;
  }

  if(mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream, attachMp4vSample_mp4wrap,
                              &pMpg4v->mp4vCbData, startHz, 0) < 0) {
    mpg4v_free(pMpg4v);
    return -1;
  }

  if(allocSamplesBuf(pMpg4v) < 0) {
    mpg4v_free(pMpg4v);
    return -1;
  }

  memset(&pMpg4v->ctxt, 0, sizeof(pMpg4v->ctxt));
  pMpg4v->numSamples = 0;
  pMpg4v->lastPlayOffsetHz = 0;
  pMpg4v->pStreamIn = (FILE_STREAM_T *) pMp4->pStream->pCbData;

  if(pStchk && mp4BoxSet.tk.pStco) {
    mp4BoxSet.tk.stchk.p_stchk = (CHUNK_DESCR_T *)
                      avc_calloc(mp4BoxSet.tk.pStco->entrycnt, sizeof(CHUNK_DESCR_T));
    mp4BoxSet.tk.stchk.stchk_sz = mp4BoxSet.tk.pStco->entrycnt;
  }

  //
  // TODO: should deprecate this in favor of on-the fly reading of samples
  // Then, mp4_readSampleFromTrack can be called to mark a sample as a 
  // keyframe - because GOP start headers may not be included in bitstream 
  //

  if(mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream, attachMp4vSample_mp4wrap,
                              &pMpg4v->mp4vCbData, startHz, 0) < 0) {
    mpg4v_free(pMpg4v);
    if(mp4BoxSet.tk.stchk.p_stchk) {
      free(mp4BoxSet.tk.stchk.p_stchk);
    }
    return -1;
  }

  if(pStchk) {
    pStchk->p_stchk = mp4BoxSet.tk.stchk.p_stchk;
    pStchk->stchk_sz = mp4BoxSet.tk.stchk.stchk_sz;
  }

  return 0;
}

void mpg4v_free(MPG4V_DESCR_T *pMpg4v) {

  if(pMpg4v) {

    if(pMpg4v->content.pSamplesBuf) {
      free(pMpg4v->content.pSamplesBuf);
      pMpg4v->content.pSamplesBuf = NULL;
    }
    pMpg4v->content.samplesBufSz = 0;

    //pMpg4v->seqHdrs.pHdrs = NULL;
    pMpg4v->seqHdrs.hdrsBufSz = 0;
    pMpg4v->seqHdrs.hdrsLen = 0;
  }

}

