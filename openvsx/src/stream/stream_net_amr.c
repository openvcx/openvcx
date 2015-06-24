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


enum STREAM_NET_ADVFR_RC stream_net_amr_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg->pArgIn;
  //int isEof = 0;

  pStreamAmr->pSample = amr_cbGetNextSample(pStreamAmr->pAmr, pStreamAmr->idxSample, 0);

  if(pStreamAmr->pSample == NULL && pStreamAmr->loop) {

   LOG(X_ERROR("looping amr not implemented"));
   return STREAM_NET_ADVFR_RC_ERROR;
   
/*
    LOG(X_DEBUG("Looping audio stream"));

    if(pStreamAmr->pAmr->content.samplesBufSz > 0) {
      // stored samples
      if(pStreamAac->idxSample >= pStreamAac->aac.content.samplesBufSz) {
        isEof = 1;
      }

      // directly read from file
    } else if(pStreamAac->aac.pStream->offset >= pStreamAac->aac.pStream->size) {
      isEof = 1; 
    }

    if(isEof) {
      pStreamAmr->idxSample = 0;
      pStreamAmr->pSample = amr_cbGetNextSample(pStreamAmr->pAmr, pStreamAmr->idxSample, 0);
    }
*/
  }

  pStreamAmr->idxInSample = 0;
  pStreamAmr->idxSample++;
  pStreamAmr->idxSampleInFrame = 0;

  if(pStreamAmr->pSample == NULL) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  if(pArg->plen) {
    *pArg->plen = pStreamAmr->pSample->sizeWr;
  }

  if(pArg->pPts) {
    *pArg->pPts = (uint64_t) ((double)pStreamAmr->pSample->playOffsetHz * 
                          (double) (90000.0 / pStreamAmr->pAmr->clockHz));
  //fprintf(stderr, "amr pts:%.3f %llu\n", (float)*pArg->pPts/90000.0f, *pArg->pPts);
  }
  pArg->isvid = 0;

  //fprintf(stderr, "stream_net_amr_advanceFrame returning\n");

  return STREAM_NET_ADVFR_RC_OK;
}


MP4_MDAT_CONTENT_NODE_T *stream_net_amr_getNextSlice(void *pArg) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg;
  pStreamAmr->idxInSample = 0;
  if(pStreamAmr->idxSampleInFrame++ > 1) {
    return NULL;
  }
  return pStreamAmr->pSample;
}

int stream_net_amr_haveMoreData(void *pArg) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg;

  if(pStreamAmr->loop ||
     pStreamAmr->idxSample < pStreamAmr->pAmr->lastFrameId ||
     (pStreamAmr->pSample && pStreamAmr->idxInSample < pStreamAmr->pSample->sizeWr )) {
    return 1;
  }

  return 0;
}

int stream_net_amr_haveNextSlice(void *pArg) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg;

  if(pStreamAmr->pSample && pStreamAmr->idxSampleInFrame < 1) {
    return 1;
  }
  return 0;
}

int stream_net_amr_szNextSlice(void *pArg) {
  return -1;
}

int stream_net_amr_getReadSliceBytes(void *pArg) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg;

  return pStreamAmr->idxInSample;
}


int stream_net_amr_getUnreadSliceBytes(void *pArg) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg;

  if(pStreamAmr->pSample == NULL) {
    return 0;
  }

  return pStreamAmr->pSample->sizeWr - pStreamAmr->idxInSample;
}

int stream_net_amr_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_AMR_T *pStreamAmr = (STREAM_AMR_T *) pArg;
  //unsigned int lenHdr = 0;
  unsigned int fsIdxInSample;

  if(pStreamAmr->pSample == NULL) {
    return 0;
  }

  fsIdxInSample = pStreamAmr->idxInSample;
/*
  if(pStreamAmr->pAmr->content.samplesBufSz > 0 &&
     pStreamAmr->pSample->sizeWr  > pStreamAmr->pSample->sizeRd) {

    lenHdr = (pStreamAmr->pSample->sizeWr + pStreamAac->lenAdtsHdr) - 
              pStreamAmr->pSample->sizeRd;

    if(fsIdxInSample == 0) {

      if(lenHdr + 1 > len) {
        LOG(X_ERROR("amr slice start request for %d too small given ADTS header length: %d\n"),
                    len, lenHdr);
        return -1;
      }
      memcpy(pBuf, pStreamAac->adtsHdrBuf, lenHdr);
      len -= lenHdr;
    } else {
      fsIdxInSample -= lenHdr;
      lenHdr = 0;
    }
  }
*/
  // Ensure not to read beyond the end of the slice
  if(len > pStreamAmr->pSample->sizeWr - pStreamAmr->idxInSample) {
    len = pStreamAmr->pSample->sizeWr - pStreamAmr->idxInSample;
  }

  if(SeekMediaFile(pStreamAmr->pSample->pStreamIn, 
             pStreamAmr->pSample->fileOffset + fsIdxInSample, SEEK_SET) != 0) {
    return -1;
  }
  if(ReadFileStream(pStreamAmr->pSample->pStreamIn, &pBuf[0], len) < 0) {
    return -1;
  }
  pStreamAmr->idxInSample += len;

  return len;
}

int64_t stream_net_amr_decodeHzOffset(void *pArg) {
  return 0;
}

float stream_net_amr_jump(STREAM_AMR_T *pStreamAmr, float fStartSec) {

  uint64_t desiredOffset = 0; 

  if(!pStreamAmr || fStartSec < 0) {
    return -1;
  }
  
  desiredOffset = (uint64_t) (fStartSec * pStreamAmr->pAmr->clockHz);

  do {
    pStreamAmr->pSample = amr_cbGetNextSample(pStreamAmr->pAmr, pStreamAmr->idxSample++, 0);
  } while(pStreamAmr->pSample && pStreamAmr->pSample->playOffsetHz +
          pStreamAmr->pAmr->frameDurationHz < desiredOffset);

  if(!pStreamAmr->pSample) {
    LOG(X_WARNING("amr stream jump to %.2f out of range %.2f (%u)"),
      fStartSec, (double)pStreamAmr->pAmr->lastPlayOffsetHz / pStreamAmr->pAmr->clockHz,
              pStreamAmr->pAmr->lastFrameId);
    return -1;
  }

  pStreamAmr->idxSampleInFrame = 0;
  pStreamAmr->idxInSample = 0;

  LOG(X_DEBUG("amr stream playout moved to frame %d (%.1fs)"), 
               pStreamAmr->pSample->frameId, fStartSec);

  return fStartSec;
}


void stream_net_amr_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_amr_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_amr_decodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_amr_getNextSlice;
  pNet->cbHaveMoreData = stream_net_amr_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_amr_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_amr_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_amr_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_amr_getReadSliceBytes;
  pNet->cbCopyData = stream_net_amr_getSliceBytes;
}

#endif // VSX_HAVE_STREAMER
