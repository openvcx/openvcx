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

#define AMR_BUF_SZ                 4096
#define AMR_MAX_FRAMES_PER_SAMPLE  50

const uint8_t AMRNB_SIZES[16] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };

const char *amr_getDescr(AMR_DESCR_T *pAmr) {

  static const char *strType[] = { "4.75", "5.15", "5.9", "6.7", "7.4", "7.95",
                       "10.2", "12.2", "SID", "", "", "", "", "", "", "" };

  if(pAmr->ctxt.frmtype < 16) {
    return strType[pAmr->ctxt.frmtype];
  } else {
    return strType[15];
  }
}

static int readFromFile(AMR_DESCR_T *pAmr, unsigned int maxFrames) {

  unsigned char buf[AMR_BUF_SZ];
  unsigned int lenread = 1;
  uint8_t lenFrame = 0;
  uint8_t lenFrameLeft = 0;
  FILE_OFFSET_T foffset = pAmr->pStreamIn->offset;

  do {

    if(pAmr->ctxt.idxBuf >= AMR_BUF_SZ) {
      if((lenread = ReadFileStreamNoEof(pAmr->pStreamIn, buf, sizeof(buf))) < 0) {
        return -1;
      }
      pAmr->ctxt.idxBuf = lenFrameLeft;
    }

    pAmr->ctxt.frmtype = AMR_GETFRAMETYPE(buf[pAmr->ctxt.idxBuf]);
    lenFrame = AMRNB_GETFRAMEBODYSIZE_FROMTYPE(pAmr->ctxt.frmtype) + 1;
    //fprintf(stderr, "lenFr:%d frtype: 0x%x\n", lenFrame, pAmr->ctxt.frmtype);
    if(lenFrame == 0) {
      LOG(X_ERROR("Invalid amr frame header at file: 0x%llx"), pAmr->pStreamIn->offset);
      return -1;
    }

    pAmr->lastFrameId++;
    pAmr->totFrames++;
    if(maxFrames > 0 && pAmr->totFrames >= maxFrames) {
      break;
    }

    if(pAmr->ctxt.idxBuf + lenFrame > AMR_BUF_SZ) {
      lenFrameLeft = lenFrame - (AMR_BUF_SZ - pAmr->ctxt.idxBuf);
    } else {
      lenFrameLeft = 0;
    }
    pAmr->ctxt.idxBuf += (lenFrame - lenFrameLeft);
    foffset += (lenFrame - lenFrameLeft);

    //fprintf(stderr, "amr file now at:%llu, totFrames:%d\n", pAmr->pStreamIn->offset + foffset, pAmr->totFrames);

  } while(foffset < pAmr->pStreamIn->size);

  return maxFrames ? (int) pAmr->totFrames : 0;
}

static int skiphdr(FILE_STREAM_T *pFile) {
  unsigned char buf[16];
  int rc = 0;

  if(SeekMediaFile(pFile, 0, SEEK_SET) < 0) {
    return -1;
  }
  if(ReadFileStreamNoEof(pFile, buf, 6) != 6) {
    return -1;
  }

  if(!memcmp(buf, AMRNB_TAG, AMRNB_TAG_LEN)) {
    rc = 1;
  } else if(SeekMediaFile(pFile, 0, SEEK_SET) < 0) {
    return -1;
  }

  return rc;
}

static void init(AMR_DESCR_T *pAmr, FILE_STREAM_T *pFile) {

  memset(pAmr, 0, sizeof(AMR_DESCR_T));
  pAmr->pStreamIn = pFile;
  pAmr->ctxt.idxBuf = AMR_BUF_SZ;
  pAmr->clockHz = AMRNB_CLOCKHZ;
  pAmr->frameDurationHz = AMRNB_SAMPLE_DURATION_HZ;
}

int amr_isvalidFile(FILE_STREAM_T *pFile) {
  int rc = 0;
  AMR_DESCR_T amr;
  unsigned char buf[16];

  if(!pFile) {
    return 0;
  }

  init(&amr, pFile);

  if((rc = skiphdr(pFile)) == 1) {
    return 1;
  } else if(rc < 0) {
    return 0;
  }

  if(SeekMediaFile(pFile, 0, SEEK_SET) < 0) {
    return 0;
  }

  if(ReadFileStreamNoEof(pFile, buf, 2) != 2 || (buf[0] == 0x00 && buf[1] == 0x00)) {
    return 0;
  }

  if(SeekMediaFile(pFile, 0, SEEK_SET) < 0) {
    return 0;
  }

  if(readFromFile(&amr, 5) == 5) {
    return 1;
  }

  return 0;
}

int amr_dump(FILE_STREAM_T *pFile, int verbosity) {
  AMR_DESCR_T amr;
  int rc = 0;

  if(!pFile) {
    return 0;
  }

  if(skiphdr(pFile) < 0) {
    return -1;
  }

  init(&amr, pFile);

  if(readFromFile(&amr, 0) < 0) {
    return -1;
  }

  fprintf(stdout, "AMR-NB %s %dHZ, %u frames, duration:%s\n", 
     amr_getDescr(&amr), amr.clockHz, amr.totFrames,
     avc_getPrintableDuration(amr.totFrames * amr.frameDurationHz, amr.clockHz));
 
  return rc;
}

static int attachSample(AMR_DESCR_T *pAmr, FILE_OFFSET_T fileOffset, 
                        unsigned char *pSample, unsigned int len) {

  unsigned int idx = 0;
  unsigned int lenFrame = len;
  unsigned int frameDurationHz = pAmr->lastSampleDurationHz;

  pAmr->content.pSampleCbPrev = NULL;

  if(pAmr->damrFramesPerSample > 1) {
    lenFrame /= pAmr->damrFramesPerSample;
    frameDurationHz /= pAmr->damrFramesPerSample;
  }

  do {

    pAmr->content.pSamplesBuf[idx].fileOffset = fileOffset +
                                                (idx * lenFrame);
    pAmr->content.pSamplesBuf[idx].sizeRd = lenFrame;
    pAmr->content.pSamplesBuf[idx].sizeWr = lenFrame;
    pAmr->content.pSamplesBuf[idx].frameId = pAmr->lastFrameId++;
    pAmr->content.pSamplesBuf[idx].playOffsetHz = pAmr->lastPlayOffsetHz +
                                               (idx * frameDurationHz);
    pAmr->content.pSamplesBuf[idx].playDurationHz = frameDurationHz;
    pAmr->content.pSamplesBuf[idx].pStreamIn = (FILE_STREAM_T *) pAmr->pMp4Ctxt->pStream->pCbData;
    //pAmr->sample.cbWriteSample = mpg4v_write_sample_to_mp4;

  } while(++idx < pAmr->content.samplesBufSz);

  pAmr->lastPlayOffsetHz += pAmr->lastSampleDurationHz;

  return 0;
}

int amr_getSamples(AMR_DESCR_T *pAmr) {
  int rc = 0;

  if(!pAmr || !pAmr->pStreamIn) {
    return -1;
  }
  
  init(pAmr, pAmr->pStreamIn);

  readFromFile(pAmr, 0);

  return rc;
}

int amr_createSampleListFromMp4Direct(AMR_DESCR_T *pAmr,
                                      MP4_CONTAINER_T *pMp4,
                                      float fStartSec) {
  MP4_TRAK_SAMR_T mp4BoxSet;
  unsigned int idx;
  uint64_t startHz = 0;
  uint64_t curHz = 0;
  uint32_t sampleHz;
  int rc = 0;

  if(!pAmr || !pMp4) {
    return -1;
  }

  if(mp4_initSamrTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  if(htons(mp4BoxSet.pSamr->timescale) != AMRNB_CLOCKHZ) {
    LOG(X_ERROR("Unsupported amr timescale %uHz"), htons(mp4BoxSet.pSamr->timescale));
    return -1;
  }

  memset(&pAmr->ctxt, 0, sizeof(pAmr->ctxt));
  init(pAmr, NULL);

  if(!(pAmr->pMp4Ctxt = mp4_create_extract_state(pMp4, &mp4BoxSet.tk))) {
    return -1;
  }

  pAmr->clockHz = htons(mp4BoxSet.pSamr->timescale);
  //memcpy(&pAmr->pMp4Ctxt->trak, &mp4BoxSet.tk, sizeof(pAmr->pMp4Ctxt->trak));
  //pAmr->pMp4Ctxt->cur.pExtSt = pAmr->pMp4Ctxt;
  //pAmr->pMp4Ctxt->start.pExtSt = pAmr->pMp4Ctxt;
  //pAmr->pMp4Ctxt->pStream = pMp4->pStream;
  if((pAmr->damrFramesPerSample = mp4BoxSet.pDamr->framespersample) < 1) {
    pAmr->damrFramesPerSample = 1;
  }
  pAmr->totFrames = pAmr->pMp4Ctxt->trak.pStsz->samplecount *
                    pAmr->damrFramesPerSample;

  if((pAmr->content.samplesBufSz = pAmr->damrFramesPerSample) > 
     AMR_MAX_FRAMES_PER_SAMPLE) {
    LOG(X_ERROR("AMR frames per sample %d exceeds threshold %d"),
         pAmr->damrFramesPerSample, AMR_MAX_FRAMES_PER_SAMPLE);
    amr_free(pAmr);
    return -1;
  } else if(pAmr->damrFramesPerSample < 1) {
    pAmr->content.samplesBufSz = 1;
  }
  if(!(pAmr->content.pSamplesBuf = (MP4_MDAT_CONTENT_NODE_T *) 
    avc_calloc(pAmr->content.samplesBufSz, sizeof(MP4_MDAT_CONTENT_NODE_T)))) {
    amr_free(pAmr);
    return -1;
  }

  if(fStartSec > 0) {

    startHz = (uint64_t) (fStartSec * mp4BoxSet.tk.pMdhd->timescale);

    for(idx=0; idx < pAmr->totFrames; idx++) {

      if((rc = mp4_readSampleFromTrack(&pAmr->pMp4Ctxt->cur, 
                      &pAmr->content.pSamplesBuf[0], &sampleHz)) < 0) {
        return -1;
      }
      curHz += sampleHz;
      if(sampleHz > startHz || curHz >= startHz - sampleHz) {
        memcpy(&pAmr->pMp4Ctxt->start, &pAmr->pMp4Ctxt->cur,
              sizeof(pAmr->pMp4Ctxt->start));
        break;
      }
    }
  }

  return rc;
}

MP4_MDAT_CONTENT_NODE_T *amr_cbGetNextSample(void *pArg, int idx, int advanceFp) {
  AMR_DESCR_T *pAmr = (AMR_DESCR_T *) pArg;
  MP4_MDAT_CONTENT_NODE_T sample;
  int rc = 0;

  //fprintf(stderr, "amr_cbGetNextSample\n");

  if(!pAmr) {
    return NULL;
  } 

  if(pAmr->pMp4Ctxt) {

    memset(&sample, 0, sizeof(sample));

    if(idx == 0) {
      memcpy(&pAmr->pMp4Ctxt->cur, &pAmr->pMp4Ctxt->start, 
             sizeof(pAmr->pMp4Ctxt->cur));
      pAmr->lastPlayOffsetHz = 0;
      pAmr->lastFrameId = 0;
      pAmr->idxFrameInSample = pAmr->content.samplesBufSz;
    }

    if(pAmr->idxFrameInSample >= pAmr->content.samplesBufSz) { 

      pAmr->idxFrameInSample = 0;

      if((rc = mp4_readSampleFromTrack(&pAmr->pMp4Ctxt->cur, 
                         &sample, &pAmr->lastSampleDurationHz)) < 0) {
        return NULL;
      }
      
      if(attachSample(pAmr, sample.fileOffset, NULL, sample.sizeRd) < 0) {
        return NULL;
      }
    }

    if(advanceFp && pAmr->pMp4Ctxt->pStream->cbSeek(pAmr->pMp4Ctxt->pStream, 
              pAmr->content.pSamplesBuf[pAmr->idxFrameInSample].fileOffset, SEEK_SET) != 0) {
      return NULL;
    }

  } else {

    //TODO: implement readFromFile to read into amr->buffer
    
    LOG(X_ERROR("amr non-mp4 sample call back not implemented"));
    return NULL;
  }

  //fprintf(stderr, "amr_cbGetNextSample returning idxFrameInSample:%d fr:%d at %lluHz len:%d\n", pAmr->idxFrameInSample, pAmr->content.pSamplesBuf[pAmr->idxFrameInSample].frameId, pAmr->content.pSamplesBuf[pAmr->idxFrameInSample].playOffsetHz, pAmr->content.pSamplesBuf[pAmr->idxFrameInSample].sizeWr);

  return &pAmr->content.pSamplesBuf[pAmr->idxFrameInSample++];
}

void amr_free(AMR_DESCR_T *pAmr) {
  if(pAmr) {
    if(pAmr->pMp4Ctxt) {
      free(pAmr->pMp4Ctxt);
      pAmr->pMp4Ctxt = NULL;
    }
    if(pAmr->content.pSamplesBuf) {
      free(pAmr->content.pSamplesBuf);
      pAmr->content.pSamplesBuf = NULL;
    }
    pAmr->content.samplesBufSz = 0;
  }
}
