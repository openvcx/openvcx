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

#if defined(VSX_HAVE_STREAMER)

static MPG4V_SAMPLE_T *getNextVclSlice(STREAM_MPG4V_T *pStreamMpg4v) {

  if(pStreamMpg4v->loop && pStreamMpg4v->idxSlice >= pStreamMpg4v->pMpg4v->lastFrameId) {

    LOG(X_DEBUG("Looping mpg4v video stream"));
    pStreamMpg4v->idxSlice = 0;
  }

  if(pStreamMpg4v->frame.pSlice && 
  //((pStreamMpg4v->pH264->pMp4Ctxt && pStreamMpg4v->frame.pSlice->pnext) ||
      pStreamMpg4v->frame.pSlice == &pStreamMpg4v->seqHdrs) {

    pStreamMpg4v->frame.pSlice = (MPG4V_SAMPLE_T *) pStreamMpg4v->frame.pSlice->pnext;
    if(pStreamMpg4v->idxSlice == 0) {
      pStreamMpg4v->idxSlice++;
    }

    return pStreamMpg4v->frame.pSlice;
  }


  return (MPG4V_SAMPLE_T *) mpg4v_cbGetNextSample(pStreamMpg4v->pMpg4v, 
                                                  pStreamMpg4v->idxSlice++, 0);
}

enum STREAM_NET_ADVFR_RC stream_net_mpg4v_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg->pArgIn;
  MPG4V_SAMPLE_T *pSlice = NULL;
  int isKeyFrame = 0;

  //fprintf(stderr, "mpg4v_advanceFrame called pSlice:0x%x frameId:%d szTot:%d\n", pStreamMpg4v->frame.pSlice, pStreamMpg4v->frame.frameId, pStreamMpg4v->frame.szTot);

  if(pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 0;
  }

  pStreamMpg4v->frame.szTot = 0;
  pStreamMpg4v->frame.idxSlice = 0;

  if((pSlice = getNextVclSlice(pStreamMpg4v)) == NULL) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }
  pStreamMpg4v->frame.idxInSlice = 0;
  pStreamMpg4v->frame.frameId = pSlice->frameId;
  pStreamMpg4v->frame.pSlice = pSlice;
  pStreamMpg4v->frame.numSlices = 1;
  pStreamMpg4v->frame.szTot = pSlice->sizeRd;

  if((pSlice->flags & MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE)) {
    isKeyFrame = 1;
  } 

  if(pArg->pPts) {
    *pArg->pPts = (uint64_t) ((double)pSlice->playOffsetHz *
          (double) (90000.0 / pStreamMpg4v->pMpg4v->clockHzOverride));
  //fprintf(stderr, "playOffsetHz:%.3f, sampleDurationHz:%d\n", (double)pSlice->playOffsetHz/90000.0f, pStreamMpg4v->pMpg4v->clockHzOverride);
  }

  while(pSlice->pnext) {

      if(pSlice->frameId != pSlice->pnext->frameId) {
        break;
      }
      if(!isKeyFrame && (pSlice->flags & MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE)) {
        isKeyFrame = 1;
      } 

      pStreamMpg4v->frame.numSlices++;
      pStreamMpg4v->frame.szTot += pSlice->pnext->sizeRd;

    pSlice = (MPG4V_SAMPLE_T *) pSlice->pnext;
  }

  //
  // Avoid streaming the very last frame because it may have been truncated upon writing
  //
  if(pSlice) {

    pStreamMpg4v->frame.pFileStream = pSlice->pStreamIn;

    if(pStreamMpg4v->includeSeqHdrs && isKeyFrame) {

      //fprintf(stderr, "mpg4v hdrs len:%d\n", pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen);

      // Insert sequence headers as one slice prepending slice list
      memcpy(&pStreamMpg4v->seqHdrs, pStreamMpg4v->frame.pSlice, sizeof(pStreamMpg4v->seqHdrs));
      pStreamMpg4v->seqHdrs.pStreamIn = NULL;
      pStreamMpg4v->seqHdrs.sizeRd = pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen;
      pStreamMpg4v->seqHdrs.sizeWr = pStreamMpg4v->seqHdrs.sizeRd;
      pStreamMpg4v->seqHdrs.pnext = pStreamMpg4v->frame.pSlice;
      pStreamMpg4v->seqHdrs.fileOffset = -1;
      pStreamMpg4v->seqHdrs.cbWriteSample = NULL;
      pStreamMpg4v->frame.szTot += pStreamMpg4v->seqHdrs.sizeRd;
      pStreamMpg4v->frame.numSlices++;

      pStreamMpg4v->frame.pSlice = &pStreamMpg4v->seqHdrs;
      //avc_dumpHex(stderr, pStreamMpg4v->pMpg4v->seqHdrs.hdrsBuf, pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen, 0);

      if(pArg->hdr.pSeqHdrs) {
        if(pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen <= 0) {
          LOG(X_WARNING("MPEG-4 video sequence headers not available"));
        }
        pArg->hdr.pSeqHdrs->buf = pStreamMpg4v->pMpg4v->seqHdrs.hdrsBuf;
        pArg->hdr.pSeqHdrs->sz = pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen;
        pArg->hdr.pSeqHdrs->idx = 0;
      }


    }

  } else {
    pStreamMpg4v->frame.pFileStream = NULL;
  }

  if(pArg->plen) {
    *pArg->plen = pStreamMpg4v->frame.szTot;
  }

  if(isKeyFrame && pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 1;
  }

  pArg->isvid = 1;

  //fprintf(stderr, "mpg4v advanceFrame returning totlen:%d slices:%d keyframe:%d pts:%.3f\n", pStreamMpg4v->frame.szTot, pStreamMpg4v->frame.numSlices, isKeyFrame,  PTSF(*pArg->pPts));

  return STREAM_NET_ADVFR_RC_OK;
}

static MPG4V_SAMPLE_T *getFrameSlice(STREAM_MPG4V_T *pStreamMpg4v) {

  if(pStreamMpg4v->frame.idxSlice >= pStreamMpg4v->frame.numSlices) {
    return NULL;
  } else if(pStreamMpg4v->frame.idxSlice > 0) {

    if((pStreamMpg4v->frame.pSlice = getNextVclSlice(pStreamMpg4v))) {
      if(pStreamMpg4v->frame.pSlice->frameId != pStreamMpg4v->frame.frameId) {
        // never supposed to get here
        LOG(X_WARNING("slice frame id %u does not match current frame %llu"), 
            pStreamMpg4v->frame.pSlice->frameId, pStreamMpg4v->frame.frameId);
        pStreamMpg4v->frame.pSlice = NULL;
      }
    }
  } 
  
  pStreamMpg4v->frame.idxSlice++;
  pStreamMpg4v->frame.idxInSlice = 0;

  return pStreamMpg4v->frame.pSlice;
}

static unsigned int stream_net_mpg4v_getNextSliceSz(STREAM_MPG4V_T *pStreamMpg4v) {
  MPG4V_SAMPLE_T *pSlice = NULL;

  if(pStreamMpg4v->frame.pSlice) {
    pSlice = (MPG4V_SAMPLE_T *) pStreamMpg4v->frame.pSlice->pnext;
  }

  if(pSlice) {

    if(pSlice->frameId != pStreamMpg4v->frame.frameId) {
      return 0;
    }

    return pSlice->sizeRd;
  }

  return 0;
}

int64_t stream_net_mpg4v_frameDecodeHzOffset(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;

  if(pStreamMpg4v->frame.pSlice && pStreamMpg4v->frame.pSlice->decodeOffset != 0) {
    return (int64_t)  (pStreamMpg4v->frame.pSlice->decodeOffset * 90000 *
             ((double)pStreamMpg4v->pMpg4v->param.frameDeltaHz / 
              (double)pStreamMpg4v->pMpg4v->param.clockHz));
  }
  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_mpg4v_getNextSlice(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;
  MPG4V_SAMPLE_T *pSlice = NULL;

  pSlice = getFrameSlice(pStreamMpg4v);
  if(pSlice) {
    memcpy(&pStreamMpg4v->slice, pSlice, sizeof(MPG4V_SAMPLE_T));
    pSlice = &pStreamMpg4v->slice;
    pSlice->sizeWr = pSlice->sizeRd;
  }

  return (MP4_MDAT_CONTENT_NODE_T *) pSlice;
}

int stream_net_mpg4v_szNextSlice(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;

  return stream_net_mpg4v_getNextSliceSz(pStreamMpg4v);
}

int stream_net_mpg4v_haveMoreData(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;
  uint32_t szSlice;

  if(pStreamMpg4v->loop ||
     pStreamMpg4v->idxSlice < pStreamMpg4v->pMpg4v->numSamples) {
    return 1;
  } else if(pStreamMpg4v->frame.pSlice) {

    szSlice = pStreamMpg4v->frame.pSlice->sizeRd;

    if(pStreamMpg4v->frame.idxInSlice < szSlice) {
      return 1;
    }
  }

  return 0;
}


int stream_net_mpg4v_haveNextSlice(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;

  if(pStreamMpg4v->frame.pSlice &&
     pStreamMpg4v->frame.idxSlice < pStreamMpg4v->frame.numSlices) {

    return (int) (pStreamMpg4v->frame.numSlices - pStreamMpg4v->frame.idxSlice);
  }
  return 0;
}


int stream_net_mpg4v_getReadSliceBytes(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v= (STREAM_MPG4V_T *) pArg;

  return pStreamMpg4v->frame.idxInSlice;
}


int stream_net_mpg4v_getUnreadSliceBytes(void *pArg) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;

  if(pStreamMpg4v->frame.pSlice == NULL) {
    return 0;
  }

  return pStreamMpg4v->frame.pSlice->sizeRd - pStreamMpg4v->frame.idxInSlice;
}

int stream_net_mpg4v_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_MPG4V_T *pStreamMpg4v = (STREAM_MPG4V_T *) pArg;
  uint32_t szSlice;
  unsigned int lenToRead = len;
  unsigned int bufIdx = 0;
  unsigned int fsIdxInSlice;

  if(pStreamMpg4v->frame.pSlice == NULL) {
    return 0;
  }

  fsIdxInSlice = pStreamMpg4v->frame.idxInSlice;
  szSlice = pStreamMpg4v->frame.pSlice->sizeWr;

  if(len > szSlice - pStreamMpg4v->frame.idxInSlice) {
    LOG(X_WARNING("Decreasing copy length from %u to (%u - %u)"), 
                  len, szSlice, pStreamMpg4v->frame.idxInSlice);
    len = szSlice - pStreamMpg4v->frame.idxInSlice;
  }

  if(pStreamMpg4v->frame.pSlice == &pStreamMpg4v->seqHdrs) {

    if(fsIdxInSlice + lenToRead > pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen) {
      LOG(X_ERROR("Attempt to read mpg4v sequence hdrs[%d] + %d > len: %d\n"),
        fsIdxInSlice, lenToRead, pStreamMpg4v->pMpg4v->seqHdrs.hdrsLen);
      return -1;
    }

    memcpy(&pBuf[bufIdx], &pStreamMpg4v->pMpg4v->seqHdrs.hdrsBuf[fsIdxInSlice], 
           lenToRead);

  } else {

    if(SeekMediaFile(pStreamMpg4v->frame.pFileStream, 
                      pStreamMpg4v->frame.pSlice->fileOffset + 
                      fsIdxInSlice, SEEK_SET) != 0) {
      return -1;
    }

    if(ReadFileStream(pStreamMpg4v->frame.pFileStream, &pBuf[bufIdx], lenToRead) < 0) {
      return -1;
    }

  }

  pStreamMpg4v->frame.idxInSlice += lenToRead;

  return len;
}

float stream_net_mpg4v_jump(STREAM_MPG4V_T *pStreamMpg4v, float fStartSec, int syncframe) {

  uint64_t desiredOffset = 0;
  MPG4V_SAMPLE_T *pSlice = NULL;
  float fOffsetActual;

  if(!pStreamMpg4v || fStartSec < 0) {
    return -1;
  }

  desiredOffset = (uint64_t) (fStartSec * pStreamMpg4v->pMpg4v->clockHzOverride);

  pStreamMpg4v->idxSlice = 0;
  pSlice = (MPG4V_SAMPLE_T *) mpg4v_cbGetNextSample(
                      pStreamMpg4v->pMpg4v, pStreamMpg4v->idxSlice++, -1);

  while(pSlice && pSlice->playOffsetHz + pSlice->playDurationHz <
        desiredOffset) {

    pSlice= (MPG4V_SAMPLE_T *) mpg4v_cbGetNextSample(
                  pStreamMpg4v->pMpg4v, pStreamMpg4v->idxSlice++, -1);
  }

/*
  if(pNal) {

    if(syncframe) {
      h264_moveToPriorSync(pStreamH264->pH264);
    }
    pNal = (H264_NAL_T *) h264_cbGetNextAnnexBSample(
                             pStreamH264->pH264, pStreamH264->idxNal++, 1);
  }
*/

  if(!pSlice) {
    LOG(X_WARNING("mpg4v stream jump to %.2f out of range %.2f (%u)"), 
                  fStartSec, (double)pStreamMpg4v->pMpg4v->lastPlayOffsetHz /
                  pStreamMpg4v->pMpg4v->clockHzOverride, 
                  pStreamMpg4v->pMpg4v->lastFrameId);
    return -1;
  }

  fOffsetActual = (float) ((double)pSlice->playOffsetHz /
                          pStreamMpg4v->pMpg4v->clockHzOverride);

  pStreamMpg4v->frame.numSlices = 1;
  pStreamMpg4v->frame.szTot = pSlice->sizeRd;
  pStreamMpg4v->frame.frameId = pSlice->frameId;
  pStreamMpg4v->frame.pFileStream = pSlice->pStreamIn;
  pStreamMpg4v->frame.idxInSlice = pStreamMpg4v->frame.szTot;
  pStreamMpg4v->frame.idxSlice = 1;
  pStreamMpg4v->frame.pSlice = pSlice;

  LOG(X_DEBUG("mpg4v stream playout moved to frame %d actual:%.1fs (%.1fs) file:0x%llx"), 
       pSlice->frameId, fOffsetActual, fStartSec, pSlice->fileOffset);

  return fOffsetActual;
}



void stream_net_mpg4v_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_mpg4v_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_mpg4v_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_mpg4v_getNextSlice;
  pNet->cbHaveMoreData = stream_net_mpg4v_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_mpg4v_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_mpg4v_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_mpg4v_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_mpg4v_getReadSliceBytes;
  pNet->cbCopyData = stream_net_mpg4v_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
