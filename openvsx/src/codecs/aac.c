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


void aac_free(AAC_DESCR_T *pAac) {

  if(pAac) {
    if(pAac->content.pSamplesBuf) {
      avc_free((void **) &pAac->content.pSamplesBuf);
    }

    pAac->content.pSampleCbPrev = NULL;
    pAac->content.samplesBufSz = 0;

    if(pAac->pMp4Ctxt) {
      avc_free((void **) &pAac->pMp4Ctxt);
    }
  }
}

void aac_close(AAC_DESCR_T *pAac) {

  if(pAac) {
    aac_free(pAac);
    free(pAac);
  }
}

int aac_encodeADTSHdr(unsigned char *buf, unsigned int len, 
                      const ESDS_DECODER_CFG_T *pSp, 
                      unsigned int sizeAacData) {
  BIT_STREAM_T stream;

  if(len < 7) {
    return -1;
  }

  memset(buf, 0, 7);
  memset(&stream, 0, sizeof(stream));
  stream.buf = buf;
  stream.sz = len - stream.byteIdx;

  bits_write(&stream, 0xffffffff, 12);     // syncword
  bits_write(&stream, 0, 1);  // id       // 0 = MPEG-4, 1 = MPEG-2
  bits_write(&stream, 0, 2);  // layer     // should be 00
  bits_write(&stream, 1, 1);  // crc absent
  bits_write(&stream, pSp->objTypeIdx - 1, 2);   
  bits_write(&stream, pSp->freqIdx, 4);  
  //bits_write(&stream, ESDS_FREQ_IDX(*pSp), 4);  
  bits_write(&stream, 0, 1);  // private
  bits_write(&stream, pSp->channelIdx, 3);  
  bits_write(&stream, 0, 1);  // original copy
  bits_write(&stream, 0, 1);  // home
                              // following variable length headers can change from frame to frame
  bits_write(&stream, 0, 1);  // copyright id bit
  bits_write(&stream, 0, 1);  // copyright id start
  bits_write(&stream, 7 + sizeAacData, 13); // size of frame + header
  bits_write(&stream, 0xffffffff, 11); // buffer fullness   // 0x7ff = VBR
  bits_write(&stream, 0, 2);  // num raw data blocks in frame
  // 16 bits optional CRC
  // num raw data blocks in frame + 1

  return 7;
}


int aac_decodeAdtsHdr(const unsigned char *buf, unsigned int len, AAC_ADTS_FRAME_T *pFrame) {

  int rc;
  BIT_STREAM_T stream;

  if(!pFrame || !buf || len < 7) {
    return -1;
  }

  memset(&stream, 0, sizeof(stream));
  stream.sz = 7;
  stream.buf = (unsigned char *) buf;

  if((rc = bits_read(&stream, 12)) != 0xfff) {
    LOG(X_ERROR("Invalid AAC ADTS prefix 0x%x"), rc);
    return -1;
  }
  bits_read(&stream, 3);
  if(bits_read(&stream, 1) == 0) {
    LOG(X_ERROR("AAC ADTS CRC not supported."));
    return -1;
  }
  pFrame->sp.objTypeIdx = bits_read(&stream, 2) + 1;
  pFrame->sp.freqIdx = bits_read(&stream, 4);
  bits_read(&stream, 1);
  pFrame->sp.channelIdx = bits_read(&stream, 3);
  bits_read(&stream, 4);
  if((pFrame->lentot = bits_read(&stream, 13)) < 7) {
    LOG(X_ERROR("Invalid AAC ADTS frame length: %u"), pFrame->lentot);
    return -1;
  }

  pFrame->lenhdr = 7;
  pFrame->sp.init = 1;

  return 0;
}

int aac_decodeAdtsHdrStream(FILE_STREAM_T *pStreamInAac, AAC_ADTS_FRAME_T *pFrame) {
  unsigned char buf[32];

  if(!pFrame) {
    return -1;
  }
  
  pFrame->lenhdr = 7;

  if(ReadFileStream(pStreamInAac, buf, pFrame->lenhdr) < 0) {
    if(fileops_Feof(pStreamInAac->fp)) {
      return 1;
    }
    return -1;
  }

  return aac_decodeAdtsHdr(buf, pFrame->lenhdr, pFrame);
}

static MP4_MDAT_CONTENT_NODE_T *getNextSampleFromMkv(AAC_DESCR_T *pAac, int idx, int advanceFp) {

  if(idx == 0) {
    //TODO: rewind blocks list

    pAac->lastFrameId = 0;
    pAac->lastPlayOffsetHz = 0;
    pAac->prevFrame.pStreamIn = pAac->pStream;
  }

  if(mkv_loadFrame(pAac->pMkv, &pAac->prevFrame, MKV_TRACK_TYPE_AUDIO,
                pAac->clockHz, &pAac->lastFrameId) < 0) {
    return NULL;
  }

  pAac->prevFrame.playDurationHz = pAac->decoderSP.frameDeltaHz;
  pAac->prevFrame.flags = 0;
  pAac->prevFrame.pnext = NULL;
  pAac->lastPlayOffsetHz = pAac->prevFrame.playOffsetHz;

  if(advanceFp && SeekMediaFile(pAac->pStream, pAac->prevFrame.sizeRd - 0, SEEK_CUR) != 0) {
    return NULL;
  }

  return &pAac->prevFrame;
}

static MP4_MDAT_CONTENT_NODE_T *getNextSampleFromList(AAC_DESCR_T *pAac, int idx) {
  
  if(!pAac || !pAac->content.pSamplesBuf) {
    return NULL;
  }

  if(idx == 0) {
    pAac->content.pSampleCbPrev = pAac->content.pSamplesBuf;
  } else if(pAac->content.pSampleCbPrev) {
    pAac->content.pSampleCbPrev = pAac->content.pSampleCbPrev->pnext;
    //fprintf(stderr, "OFFSET 0x%llx sz:%d\n", pAac->content.pSampleCbPrev->fileOffset, pAac->content.pSampleCbPrev->sizeRd);
  }  else {
    pAac->content.pSampleCbPrev = NULL;
  }

  return pAac->content.pSampleCbPrev;
}

static MP4_MDAT_CONTENT_NODE_T *getNextAdtsSampleFromFile(AAC_DESCR_T *pAac, int idx, int advanceFp) {

  int rc;
  AAC_ADTS_FRAME_T frame;
  
  if(!pAac || !pAac->pStream) {
    return NULL;
  }

  if(idx == 0) {
    if(SeekMediaFile(pAac->pStream, 0, SEEK_SET) != 0) {;
      return NULL;
    }
    pAac->lastFrameId = 0;
    pAac->lastPlayOffsetHz = 0;
    pAac->prevFrame.pStreamIn = pAac->pStream;
  }

  if((rc = aac_decodeAdtsHdrStream(pAac->pStream, &frame)) != 0) {
    return NULL;
  }

  if(pAac->pStream->offset + frame.lentot - frame.lenhdr > pAac->pStream->size) {
    LOG(X_WARNING("Frame[%u] body 0x%llx + %u extends beyond end of file 0x%llx"),
      pAac->lastFrameId, pAac->pStream->offset, frame.lentot - frame.lenhdr, 
      pAac->pStream->size);
    return NULL;
  }

  if(pAac->includeAdtsHdr) {
    if(SeekMediaFile(pAac->pStream, ((int)frame.lenhdr) * -1, SEEK_CUR) != 0) {
      return NULL;
    }
    frame.lenhdr = 0;
  }

  pAac->prevFrame.fileOffset = pAac->pStream->offset;
  pAac->prevFrame.sizeRd = frame.lentot;
  pAac->prevFrame.sizeWr = frame.lentot - frame.lenhdr;
  pAac->prevFrame.frameId = pAac->lastFrameId;
  pAac->prevFrame.playOffsetHz = pAac->lastPlayOffsetHz;
  pAac->prevFrame.playDurationHz = pAac->decoderSP.frameDeltaHz;
  pAac->prevFrame.flags = 0;
  pAac->prevFrame.pnext = NULL;

  pAac->lastFrameId++;
  pAac->lastPlayOffsetHz += pAac->decoderSP.frameDeltaHz;

  if(advanceFp && SeekMediaFile(pAac->pStream, pAac->prevFrame.sizeRd - frame.lenhdr, SEEK_CUR) != 0) {
    return NULL;
  }

  return &pAac->prevFrame;
}

static MP4_MDAT_CONTENT_NODE_T *getNextSampleFromMp4(AAC_DESCR_T *pAac, int idx, int advanceFp) {
  int rc = 0;
  uint32_t sampleDuration = 0;
  uint32_t durationHz = 0;

  //fprintf(stderr, "AAC_getNextSampleFromMp4 idx:%d\n", idx);

  if(idx == 0) {

    memcpy(&pAac->pMp4Ctxt->cur, &pAac->pMp4Ctxt->start, sizeof(pAac->pMp4Ctxt->cur));

    pAac->lastFrameId = 0;
    pAac->lastPlayOffsetHz = 0;
    pAac->prevFrame.pStreamIn = pAac->pStream;
  }

  if((rc = mp4_readSampleFromTrack(&pAac->pMp4Ctxt->cur, &pAac->prevFrame, &sampleDuration)) < 0) {
    //fprintf(stderr, "mp4_readSampleFromTrack fail\n");
    return NULL;
  }

  if((durationHz = pAac->decoderSP.frameDeltaHz) != sampleDuration) {
    if(idx == 0) {
      LOG(X_WARNING("Using MP4 sample duration %dHz instead of AAC frame %dHz"), sampleDuration, durationHz);
    }
    durationHz = sampleDuration;
  }

  if(idx > 0) {
    pAac->lastPlayOffsetHz += pAac->prevFrame.playDurationHz;
    pAac->prevFrame.playOffsetHz = pAac->lastPlayOffsetHz;
  }

  pAac->prevFrame.pStreamIn = (FILE_STREAM_T *) pAac->pMp4Ctxt->pStream->pCbData;
  pAac->prevFrame.sizeWr = pAac->prevFrame.sizeRd;
  //pAac->prevFrame.pStream
  pAac->prevFrame.playDurationHz = durationHz;
  pAac->prevFrame.flags = 0;
  pAac->prevFrame.pnext = NULL;

  //fprintf(stderr, "aac getNextSampleFromMp4 idx:%d duration:%d,%dHz, advanceFp:%d, sizeRd:%d, fileOffset:%llu, file:%llu/%llu\n", idx, sampleDuration, pAac->decoderSP.frameDeltaHz, advanceFp, pAac->prevFrame.sizeRd, pAac->prevFrame.fileOffset, pAac->pMp4Ctxt->pStream->offset, pAac->pMp4Ctxt->pStream->size);

  if(pAac->pMp4Ctxt->pStream->cbSeek(pAac->pMp4Ctxt->pStream, 
                                     pAac->prevFrame.fileOffset, SEEK_SET) != 0) {
    return NULL;
  }

  if(advanceFp && pAac->pMp4Ctxt->pStream->cbSeek(pAac->pMp4Ctxt->pStream, 
                                                  pAac->prevFrame.sizeRd - 0, SEEK_CUR) != 0) {
    return NULL;
  }

  return &pAac->prevFrame;
}

/**
 * Get the enxt AAC frame sample given an AAC_DESCR_T context
 */
MP4_MDAT_CONTENT_NODE_T *aac_cbGetNextSample(void *pArg, int idx, int advanceFp) {

  AAC_DESCR_T *pAac = (AAC_DESCR_T *) pArg;
  
  if(!pAac) {
    return NULL; 
  }

  //fprintf(stderr, "AAC_CBNEXTSAMPLE, pMkv:0x%x, pSamplesBuf:0x%x, includeAdts:%d\n", pAac->pMkv, pAac->content.pSamplesBuf, pAac->includeAdtsHdr);

  if(pAac->pMp4Ctxt) {
    //
    // Advance the FP to the end of the frame to ensure all frame contents are there,
    // in case reading a live mp4
    //
    return getNextSampleFromMp4(pAac, idx, 1);
  } else if(pAac->pMkv) {
    return getNextSampleFromMkv(pAac, idx, advanceFp);
  } else if(pAac->content.pSamplesBuf) {
    return getNextSampleFromList(pAac, idx);
  } else {
    return getNextAdtsSampleFromFile(pAac, idx, advanceFp);
  }
}

int aac_write_sample_to_mp4(FILE_STREAM_T *pStreamOut, MP4_MDAT_CONTENT_NODE_T *pSample,
                                  unsigned char *arena, unsigned int sz_arena) {
  int lenbody;
  int lenread;
  int rc = 0;
                                    
  if(SeekMediaFile(pSample->pStreamIn, pSample->fileOffset, SEEK_SET) != 0) {
    return -1;
  }

  lenbody = pSample->sizeWr;

  while(lenbody > 0) {

    lenread = lenbody > (int) sz_arena ? sz_arena : lenbody;

    if(ReadFileStream(pSample->pStreamIn, arena, lenread) < 0) {
      return -1;
    }
    
    if(WriteFileStream(pStreamOut, arena, lenread) < 0) {
      return -1;
    }
    rc += lenread;

    lenbody -= lenread;
  }

  return 0;
}

int write_box_mdat_from_file_aacAdts(FILE_STREAM_T *pStreamOut, BOX_T *pBox, 
                                   unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  uint32_t bufaligned[2];
  FILE_STREAM_T *pStreamIn = (FILE_STREAM_T *) pUserData;
  AAC_ADTS_FRAME_T frame;
  long long rc = 0;
  int lenbody;
  int lenread;

  if(!pStreamIn) {
    LOG(X_ERROR("Invalid user data passed to write handler"));
    return -1;
  }

  *((uint32_t *)&((unsigned char *) bufaligned)[0]) = htonl((uint32_t)pBox->szdata + pBox->szhdr);
  *((uint32_t *)&((unsigned char *) bufaligned)[4]) = pBox->type;

  if(WriteFileStream(pStreamOut, bufaligned, 8) < 0) {
    return -1;
  }

  if(pStreamIn->offset != 0 && SeekMediaFile(pStreamIn, 0, SEEK_SET) != 0) {;
    return -1;
  }

  while(aac_decodeAdtsHdrStream(pStreamIn, &frame) == 0) {

    lenbody = frame.lentot - frame.lenhdr;

    while(lenbody > 0) {

      lenread = lenbody > (int) sz_arena ? sz_arena : lenbody;

      if(ReadFileStream(pStreamIn, arena, lenread) < 0) {
        return -1;
      }
      
      if(WriteFileStream(pStreamOut, arena, lenread) < 0) {
        return -1;
      }
      rc += lenread;

      lenbody -= lenread;
    }

  }

  return 0;
}

int aac_getSamplesFromAacAdts(AAC_DESCR_T *pAac, int storeSamples) {
  MP4_MDAT_CONTENT_NODE_T *pSample = NULL;
  MP4_MDAT_CONTENT_NODE_T *pSamplePrev = NULL;
  unsigned int idx = 0;
  BURSTMETER_DECODER_SET_T bw;
  int rc = 0;

  if(!pAac || pAac->clockHz == 0 || pAac->decoderSP.frameDeltaHz == 0) {
    LOG(X_ERROR("aac clock rate has not been set"));
    return -1;
  }   

  memset(&bw, 0, sizeof(bw));
  bw.clockHz = pAac->clockHz;
  bw.frameDeltaHz = pAac->decoderSP.frameDeltaHz;
  if(burstmeter_init(&bw.set, 20, 1000) < 0) {
    return -1;
  }
  pAac->szLargestFrame = 0;
  pAac->szTot = 0;
  pAac->peakBwBytes = 0;

  while((pSample = getNextAdtsSampleFromFile(pAac, idx, 1)) ) {

    pSamplePrev = pSample;
    if(pSample->sizeWr > pAac->szLargestFrame) {
      pAac->szLargestFrame = pSample->sizeWr;
    }
    pAac->szTot += pSample->sizeWr;

    burstmeter_AddDecoderSample(&bw, pSample->sizeWr, pSample->frameId);

    if(storeSamples) {

      if(idx >= pAac->content.samplesBufSz) {
        LOG(X_ERROR("number of AAC samples: %u exceeds allocated size: %u"), 
              idx, pAac->content.samplesBufSz);
        rc = -1;
        break;
      }
      memcpy(&pAac->content.pSamplesBuf[idx], pSample, sizeof(MP4_MDAT_CONTENT_NODE_T)); 

      if(pAac->content.pSampleCbPrev == NULL) {
        pAac->content.pSampleCbPrev = pAac->content.pSamplesBuf;
      } else {
        pAac->content.pSampleCbPrev->pnext = &pAac->content.pSamplesBuf[idx];
        pAac->content.pSampleCbPrev = &pAac->content.pSamplesBuf[idx];
      }
      pAac->content.pSampleCbPrev->cbWriteSample = aac_write_sample_to_mp4;
      pAac->content.pSampleCbPrev->pStreamIn = pAac->pStream;
       
    }

    idx++;
  } while(pSample);
    
  pAac->peakBwBytes = bw.set.meter.max.bytes;

  burstmeter_close(&bw.set);

  return rc;
}


static int attachEsdsSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                            void *pCbData, int chunk_idx, uint32_t sampleDurationHz) {

  AAC_DESCR_T *pAac = (AAC_DESCR_T *) pCbData;
  MP4_MDAT_CONTENT_NODE_T *pSample = NULL;

  if(sampleDurationHz == 0) {
    sampleDurationHz = pAac->decoderSP.frameDeltaHz;
  }

  if(pAac->lastFrameId >= pAac->content.samplesBufSz) {
    LOG(X_ERROR("aac sample[%u] exceeds allocated buffer size of %d"), 
                pAac->lastFrameId, pAac->content.samplesBufSz);
    return -1;
  }

  if(pAac->lastFrameId == 0) {
    pAac->content.pSampleCbPrev = &pAac->content.pSamplesBuf[0];
  } else {
    pAac->content.pSampleCbPrev[pAac->lastFrameId-1].pnext = &pAac->content.pSamplesBuf[pAac->lastFrameId];
  } 

  pSample = &pAac->content.pSamplesBuf[pAac->lastFrameId];
  pSample->pStreamIn = pStreamIn;
  pSample->fileOffset = pStreamIn->offset;
  pSample->sizeRd = sampleSize;
  pSample->sizeWr = sampleSize;
  pSample->playOffsetHz = pAac->lastPlayOffsetHz;
  pSample->playDurationHz = sampleDurationHz;
  pSample->frameId = pAac->lastFrameId++;
  pSample->flags = 0;
  pSample->cbWriteSample = aac_write_sample_to_mp4;
  pSample->tkhd_id = 0;
  pSample->chunk_idx = chunk_idx;

  pAac->lastPlayOffsetHz += sampleDurationHz;

  return 0;
}

static int attachEsdsSample_mp4wrap(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                            void *pCbData, int chunk_idx, uint32_t sampleDurationHz) {

  return attachEsdsSample((FILE_STREAM_T *)pStreamIn->pCbData, sampleSize, 
                           pCbData, chunk_idx, sampleDurationHz);
}

static int getSamplesFromMp4(const MP4_CONTAINER_T *pMp4, 
                             AAC_DESCR_T *pAac, 
                             int storeSamples, 
                             CHUNK_SZ_T *pStchk,
                             float fStart) {

  MP4_TRAK_MP4A_T mp4BoxSet;
  unsigned int numSamples = 0;
  uint64_t startHz = 0;

  if(pAac->includeAdtsHdr) {
    // ADTS headers are not present in the filestream of the aac data in .mp4
    return -1;
  }

  if(mp4_initMp4aTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  if(esds_decodeDecoderSp(mp4BoxSet.pEsds->specific.startcodes, 
        (mp4BoxSet.pEsds->specific.type.length > sizeof(mp4BoxSet.pEsds->specific.startcodes) ?
        sizeof(mp4BoxSet.pEsds->specific.startcodes) : mp4BoxSet.pEsds->specific.type.length), 
        &pAac->decoderSP) != 0) {

    LOG(X_ERROR("Failed to decode mp4a esds decoder specific data"));  
    return -1;
  }

  if((pAac->clockHz = esds_getFrequencyVal(ESDS_FREQ_IDX(pAac->decoderSP))) != 
                                    mp4BoxSet.tk.pMdhd->timescale) {
    LOG(X_WARNING("mp4a esds clock rate %uHz does not match track mdhd %uHz"),
                 pAac->clockHz, mp4BoxSet.tk.pMdhd->timescale);
    pAac->clockHz = mp4BoxSet.tk.pMdhd->timescale;
  }

  pAac->szLargestFrame = mp4BoxSet.pEsds->config.bufsize[0];
  pAac->szTot = (uint32_t)( htonl(mp4BoxSet.pEsds->config.avgbitrate) * 
              ((double)mp4BoxSet.tk.pMdhd->duration / mp4BoxSet.tk.pMdhd->timescale)/8.0f);
  pAac->peakBwBytes = (unsigned int) ((double)htonl(mp4BoxSet.pEsds->config.maxbitrate)/8.0f);

  startHz = (uint64_t) (fStart * mp4BoxSet.tk.pMdhd->timescale);

  if((numSamples = mp4_getSampleCount(&mp4BoxSet.tk)) <= 0) {
    LOG(X_ERROR("Unable to obtain number of aac samples in audio track"));
    return -1;
  }
   
  if(storeSamples) {
    if((pAac->content.pSamplesBuf = (MP4_MDAT_CONTENT_NODE_T *) 
      avc_calloc((size_t)  numSamples, sizeof(MP4_MDAT_CONTENT_NODE_T))) == NULL) {
      return -1;
    }
    pAac->content.samplesBufSz =  numSamples;
  } else {
    LOG(X_ERROR("Reading aac samples directly from mp4 track w/o memory storage not implemented."));
    return -1;
  }

  if(pStchk && mp4BoxSet.tk.pStco) {
    mp4BoxSet.tk.stchk.p_stchk = (CHUNK_DESCR_T *) avc_calloc(mp4BoxSet.tk.pStco->entrycnt, 
                                                          sizeof(CHUNK_DESCR_T));
    mp4BoxSet.tk.stchk.stchk_sz = mp4BoxSet.tk.pStco->entrycnt; 
  }


  if(mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream, attachEsdsSample_mp4wrap, 
                              pAac, startHz, 0) < 0) {
    aac_free(pAac);
    if(mp4BoxSet.tk.stchk.p_stchk) {
      free(mp4BoxSet.tk.stchk.p_stchk);
    }
    return -1;
  }

  if(pAac->lastFrameId == 0) {
    LOG(X_WARNING("Extracted 0 aac samples"));
    aac_free(pAac);
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

AAC_DESCR_T *aac_getSamplesFromMp4Direct(const MP4_CONTAINER_T *pMp4, float fStartSec) {
  int rc = 0;
  AAC_DESCR_T *pAac = NULL;
  MP4_TRAK_MP4A_T mp4BoxSet;
  unsigned int idx;
  uint32_t sampleHz;
  uint64_t curHz = 0;
  uint64_t startHz = 0;

  //fprintf(stderr, "AAC_GETSAMPLESFROMMP4DIRECT\n");

  if(!pMp4) {
    return NULL;
  } else if((pAac = (AAC_DESCR_T *) avc_calloc(1, sizeof(AAC_DESCR_T))) == NULL) {
    return NULL;
  } 

  if(mp4_initMp4aTrack(pMp4, &mp4BoxSet) < 0) {
    aac_free(pAac);
    return NULL;
  }

  if(fStartSec > 0.0 && mp4BoxSet.tk.pMdhd->timescale == 0) {
    LOG(X_WARNING("Unable to do partial aac audio extract"));
  } else {
    startHz = (uint64_t) (fStartSec * mp4BoxSet.tk.pMdhd->timescale);
  }

  if(esds_decodeDecoderSp(mp4BoxSet.pEsds->specific.startcodes, 
        (mp4BoxSet.pEsds->specific.type.length > sizeof(mp4BoxSet.pEsds->specific.startcodes) ?
        sizeof(mp4BoxSet.pEsds->specific.startcodes) : mp4BoxSet.pEsds->specific.type.length), 
        &pAac->decoderSP) != 0) {

    LOG(X_ERROR("Failed to decode mp4a esds decoder specific data"));  
    aac_free(pAac);
    return NULL;
  }

  if((pAac->clockHz = esds_getFrequencyVal(ESDS_FREQ_IDX(pAac->decoderSP))) != 
                                    mp4BoxSet.tk.pMdhd->timescale) {
    LOG(X_WARNING("mp4a esds clock rate %uHz does not match track mdhd %uHz"),
                 pAac->clockHz, mp4BoxSet.tk.pMdhd->timescale);
    pAac->clockHz = mp4BoxSet.tk.pMdhd->timescale;
  }

  pAac->szLargestFrame = mp4BoxSet.pEsds->config.bufsize[0];
  pAac->szTot = (uint32_t)( htonl(mp4BoxSet.pEsds->config.avgbitrate) * 
              ((double)mp4BoxSet.tk.pMdhd->duration / mp4BoxSet.tk.pMdhd->timescale)/8.0f);
  pAac->peakBwBytes = (unsigned int) ((double)htonl(mp4BoxSet.pEsds->config.maxbitrate)/8.0f);

  if(!(pAac->pMp4Ctxt = mp4_create_extract_state(pMp4, &mp4BoxSet.tk))) {
  //if(!(pAac->pMp4Ctxt = (MP4_EXTRACT_STATE_T *) avc_calloc(1, sizeof(MP4_EXTRACT_STATE_T)))) {
    aac_free(pAac);
    return NULL;
  }
  //memcpy(&pAac->pMp4Ctxt->trak, &mp4BoxSet.tk, sizeof(pAac->pMp4Ctxt->trak));
  //pAac->pMp4Ctxt->cur.pExtSt = pAac->pMp4Ctxt;
  //pAac->pMp4Ctxt->start.pExtSt = pAac->pMp4Ctxt;
  //pAac->pMp4Ctxt->pStream = pMp4->pStream;
  pAac->lastFrameId = mp4_getSampleCount(&pAac->pMp4Ctxt->trak);

  //
  // Skip fStartSec of content
  //
  if(fStartSec > 0) {

    for(idx=0; idx < pAac->lastFrameId; idx++) {

      if((rc = mp4_readSampleFromTrack(&pAac->pMp4Ctxt->cur, &pAac->prevFrame, &sampleHz)) < 0) {
        aac_free(pAac);
        return NULL;
      }
      curHz += sampleHz;
      if(sampleHz > startHz || curHz >= startHz - sampleHz) {
        memcpy(&pAac->pMp4Ctxt->start, &pAac->pMp4Ctxt->cur, sizeof(pAac->pMp4Ctxt->start));
        break;
      }
    }
  }

  return pAac;
}

static int probeEsdsSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                           void *pCbData, int chunk_idx, uint32_t sampleDurationHz) {
  AAC_DESCR_T *pAac = (AAC_DESCR_T *) pCbData;

  if(sampleSize > pAac->szLargestFrame) {
    pAac->szLargestFrame = sampleSize;
  }
  pAac->szTot += sampleSize;

  pAac->lastFrameId++;
  pAac->lastPlayOffsetHz += sampleDurationHz;

  return 0;
}

static int getSamplesFromFlv(FLV_CONTAINER_T *pFlv,
                             AAC_DESCR_T *pAac, 
                             int storeSamples) {

  AAC_DESCR_T aacTmp;

  if(pAac->includeAdtsHdr) {
    // ADTS headers are not present in the filestream of the aac data in .flv
    return -1;
  }

  if(!storeSamples) {
    LOG(X_ERROR("Reading aac samples directly from flv w/o memory storage not implemented."));
    return -1; 
  }

  memcpy(&pAac->decoderSP, &pFlv->audCfg.aacCfg, sizeof(ESDS_DECODER_CFG_T));
  if(pAac->decoderSP.objTypeIdx == 0) {
    LOG(X_ERROR("esds decoder configuration not initialized"));
    return -1;
  }

  if((pAac->clockHz = esds_getFrequencyVal(pAac->decoderSP.freqIdx)) == 0) {
    LOG(X_WARNING("Invalid flv audio esds clock rate: %uHz"), pAac->clockHz);
  }

  memset(&aacTmp, 0, sizeof(aacTmp));
  if(flv_readSamples(pFlv, probeEsdsSample, &aacTmp, 0, 0, 0) != 0) {
    LOG(X_ERROR("Failed to read all aac samples"));
    return -1;
  }

  if((pAac->content.pSamplesBuf = (MP4_MDAT_CONTENT_NODE_T *) 
    avc_calloc((size_t)  aacTmp.lastFrameId, sizeof(MP4_MDAT_CONTENT_NODE_T))) == NULL) {
    return -1;
  }

  pAac->content.samplesBufSz =  aacTmp.lastFrameId;
  pAac->szLargestFrame = aacTmp.szLargestFrame;
  pAac->szTot = aacTmp.szTot;
  pAac->peakBwBytes = 0;
  pAac->lastPlayOffsetHz = 0;
  
  if(flv_readSamples(pFlv, attachEsdsSample, pAac, 0, 0, 0) != 0) {
    LOG(X_ERROR("Failed to read all aac samples"));
    return -1;
  }

  return 0;
}

AAC_DESCR_T *aac_getSamplesFromMp4(const MP4_CONTAINER_T *pMp4, CHUNK_SZ_T *pStchk, float fStart) {
  AAC_DESCR_T *pAac = NULL;

  if(!pMp4) {
    return NULL;
  }

  if((pAac = (AAC_DESCR_T *) avc_calloc(1, sizeof(AAC_DESCR_T))) == NULL) {
    return NULL;
  }

  if(getSamplesFromMp4(pMp4, pAac, 1, pStchk, fStart) < 0) {
    aac_close(pAac);
    return NULL;
  }

  return pAac;
}


AAC_DESCR_T *aac_getSamplesFromFlv(FLV_CONTAINER_T *pFlv) {
  AAC_DESCR_T *pAac = NULL;

  if(!pFlv || !pFlv->pStream || pFlv->pStream->fp == FILEOPS_INVALID_FP) {
    return NULL;
  }

  if((pAac = (AAC_DESCR_T *) avc_calloc(1, sizeof(AAC_DESCR_T))) == NULL) {
    return NULL;
  }

  if(getSamplesFromFlv(pFlv, pAac, 1) < 0) {
    aac_close(pAac);
    return NULL;
  }

  return pAac;
}

AAC_DESCR_T *aac_getSamplesFromMkv(MKV_CONTAINER_T *pMkv) {
  AAC_DESCR_T *pAac = NULL;

  if(!pMkv || !pMkv->pStream || pMkv->pStream->fp == FILEOPS_INVALID_FP) {
    return NULL;
  }

  if((pAac = (AAC_DESCR_T *) avc_calloc(1, sizeof(AAC_DESCR_T))) == NULL) {
    return NULL;
  }

  if(!pMkv->aud.pTrack->codecPrivate.p || esds_decodeDecoderSp(pMkv->aud.pTrack->codecPrivate.p, 
                         pMkv->aud.pTrack->codecPrivate.size, &pAac->decoderSP) != 0) {
    LOG(X_ERROR("Failed to decode aac decoder config"));
    return NULL;
  }

  if(pAac->decoderSP.objTypeIdx == 0) {
    LOG(X_ERROR("esds decoder configuration not initialized"));
    return NULL;
  }

  if((pAac->clockHz = esds_getFrequencyVal(pAac->decoderSP.freqIdx)) == 0) {
    LOG(X_WARNING("Invalid mkv audio esds clock rate: %uHz"), pAac->clockHz);
  }

  pAac->pMkv = pMkv;

  return pAac;
}

AAC_DESCR_T *aac_initFromAdts(FILE_STREAM_T *pFileStream, unsigned int frameDeltaHz) {
  AAC_DESCR_T *pAac = NULL;
  AAC_ADTS_FRAME_T frame;

  if(!pFileStream || pFileStream->fp == FILEOPS_INVALID_FP) {
    return NULL;
  }

  memset(&frame, 0, sizeof(frame));

  if(frameDeltaHz != 960 && frameDeltaHz != 1024) {
    LOG(X_ERROR("Invalid number of samples / frame: %u"), frameDeltaHz);
    return NULL;
  } else if(aac_decodeAdtsHdrStream(pFileStream, &frame) != 0) {
    return NULL;
  }

  if((pAac = (AAC_DESCR_T *) avc_calloc(1, sizeof(AAC_DESCR_T))) == NULL) {
    return NULL;
  }

  pAac->pStream = pFileStream;
  frame.sp.frameDeltaHz = frameDeltaHz;
  memcpy(&pAac->decoderSP, &frame.sp, sizeof(ESDS_DECODER_CFG_T));

  if((pAac->clockHz = esds_getFrequencyVal(frame.sp.freqIdx)) == 0) {
    LOG(X_ERROR("Unable to obtain valid clock rate from ADTS header"));
    aac_close(pAac);
    return NULL;
  }

  return pAac;
}
