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


enum STREAM_NET_ADVFR_RC stream_net_aac_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg->pArgIn;
  int isEof = 0;

  pStreamAac->pSample = aac_cbGetNextSample(&pStreamAac->aac, pStreamAac->idxSample, 0);

  //fprintf(stderr, "stream_net_aac_advanceFrame 0x%x, idx:%d, lastFrameId:%d\n", pStreamAac->pSample, pStreamAac->idxSample, pStreamAac->aac.lastFrameId);

  if(pStreamAac->pSample == NULL && pStreamAac->loop) {
    
    LOG(X_DEBUG("Looping audio stream"));

    if(pStreamAac->aac.content.samplesBufSz > 0) {
      // stored samples
      if(pStreamAac->idxSample >= pStreamAac->aac.content.samplesBufSz) {
        isEof = 1;
      }

      // directly read from file
    } else if(pStreamAac->aac.pStream->offset >= pStreamAac->aac.pStream->size) {
      isEof = 1; 
    }

    if(isEof) {
      pStreamAac->idxSample = 0;
      pStreamAac->pSample = aac_cbGetNextSample(&pStreamAac->aac, pStreamAac->idxSample, 0);
    }
  }

  pStreamAac->idxInSample = 0;
  pStreamAac->idxSample++;
  pStreamAac->idxSampleInFrame = 0;

  if(pStreamAac->pSample == NULL) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  // Extract / create an ADTS header

  if(pStreamAac->aac.includeAdtsHdr) {

    if((pStreamAac->lenAdtsHdr = aac_encodeADTSHdr(pStreamAac->adtsHdrBuf, 
                                 sizeof(pStreamAac->adtsHdrBuf),
      &pStreamAac->aac.decoderSP, pStreamAac->pSample->sizeWr)) < 0) {
        LOG(X_ERROR("Unable to encode ADTS header for length: %u"), 
                    pStreamAac->pSample->sizeWr);
      return STREAM_NET_ADVFR_RC_ERROR;
    }
  
  }

  if(pArg->plen) {
    *pArg->plen = (pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr);
  }

  if(pArg->pPts) {
    *pArg->pPts = (uint64_t) ((double)pStreamAac->pSample->playOffsetHz * 
                                      (double) (90000.0 / pStreamAac->aac.clockHz));
  }
  pArg->isvid = 0;

  //fprintf(stderr, "aac advanceFrame returning totlen:%d pts:%.3f, playOffsetHz:%llu, %dHz\n", *pArg->plen,   PTSF(*pArg->pPts), pStreamAac->pSample->playOffsetHz, pStreamAac->aac.clockHz);

  return STREAM_NET_ADVFR_RC_OK;
}


MP4_MDAT_CONTENT_NODE_T *stream_net_aac_getNextSlice(void *pArg) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg;
  pStreamAac->idxInSample = 0;
  if(pStreamAac->idxSampleInFrame++ > 1) {
    return NULL;
  }
  return pStreamAac->pSample;
}

int stream_net_aac_haveMoreData(void *pArg) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg;

  if(pStreamAac->loop ||
     pStreamAac->idxSample < pStreamAac->aac.lastFrameId ||
     (pStreamAac->pSample && pStreamAac->idxInSample < 
     pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr)) {
    return 1;
  }

  return 0;
}

int stream_net_aac_haveNextSlice(void *pArg) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg;

  if(pStreamAac->pSample && pStreamAac->idxSampleInFrame < 1) {
    return 1;
  }
  return 0;
}

int stream_net_aac_szNextSlice(void *pArg) {
  return -1;
}

int stream_net_aac_getReadSliceBytes(void *pArg) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg;

  return pStreamAac->idxInSample;
}


int stream_net_aac_getUnreadSliceBytes(void *pArg) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg;

  if(pStreamAac->pSample == NULL) {
    return 0;
  }

  return (pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr) - pStreamAac->idxInSample;
}

int stream_net_aac_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_AAC_T *pStreamAac = (STREAM_AAC_T *) pArg;
  unsigned int lenHdr = 0;
  unsigned int fsIdxInSample;

  if(pStreamAac->pSample == NULL) {
    return 0;
  }

  fsIdxInSample = pStreamAac->idxInSample;

  if(pStreamAac->aac.includeAdtsHdr &&
     pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr > pStreamAac->pSample->sizeRd) {

    lenHdr = (pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr) - pStreamAac->pSample->sizeRd;

    if(fsIdxInSample == 0) {

      if(lenHdr + 1 > len) {
        LOG(X_ERROR("aac slice start request for %d too small given ADTS header length: %d\n"),
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

  // Ensure not to read beyond the end of the slice
  if(len > (pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr) - pStreamAac->idxInSample) {
    len = (pStreamAac->pSample->sizeWr + pStreamAac->lenAdtsHdr) - pStreamAac->idxInSample;
  }

  //fprintf(stderr, "stream_net_aac_getSliceBytes, len:%d, fsIdxInSample:%d, lenHdr:%d, pStreamIn: 0x%x\n", len, fsIdxInSample, lenHdr, pStreamAac->pSample->pStreamIn);

  if(SeekMediaFile(pStreamAac->pSample->pStreamIn, 
                    pStreamAac->pSample->fileOffset + 
                    fsIdxInSample, SEEK_SET) != 0) {
    return -1;
  }

  if(ReadFileStream(pStreamAac->pSample->pStreamIn, &pBuf[lenHdr], len) < 0) {
    return -1;
  }

  //fprintf(stderr, "aac read len:%d, lenHdr:%d, fsIdxInSample:%d, fOffset:0x%llx\n", len, lenHdr, fsIdxInSample, pStreamAac->pSample->fileOffset); avc_dumpHex(stderr, pBuf, MIN(32, len + lenHdr), 1);

  //if(pStreamAac->idxInSample==0){fprintf(stderr, "lenhdr:%d\n", lenHdr);avc_dumpHex(stderr, &pBuf[lenHdr], MIN(16, len), 1);}

  pStreamAac->idxInSample += len + lenHdr;

  return len + lenHdr;
}

int64_t stream_net_aac_decodeHzOffset(void *pArg) {
  return 0;
}

float stream_net_aac_jump(STREAM_AAC_T *pStreamAac, float fStartSec) {

  uint64_t desiredOffset = 0; 

  if(!pStreamAac || fStartSec < 0) {
    return -1;
  }
  
  desiredOffset = (uint64_t) (fStartSec * pStreamAac->aac.clockHz);

  do {
    pStreamAac->pSample = aac_cbGetNextSample(&pStreamAac->aac, pStreamAac->idxSample++, 1);
  } while(pStreamAac->pSample && pStreamAac->pSample->playOffsetHz +
          pStreamAac->aac.decoderSP.frameDeltaHz < desiredOffset);

  if(!pStreamAac->pSample) {
    LOG(X_WARNING("aac stream jump to %.2f out of range %.2f (%u)"),
             fStartSec, (double)pStreamAac->aac.lastPlayOffsetHz / pStreamAac->aac.clockHz,
              pStreamAac->aac.lastFrameId);
    return -1;
  }

  pStreamAac->idxSampleInFrame = 0;
  pStreamAac->idxInSample = 0;

  LOG(X_DEBUG("aac stream playout moved to frame %d (%.1fs)"), pStreamAac->pSample->frameId, 
              fStartSec);

  return fStartSec;
}


void stream_net_aac_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_aac_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_aac_decodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_aac_getNextSlice;
  pNet->cbHaveMoreData = stream_net_aac_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_aac_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_aac_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_aac_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_aac_getReadSliceBytes;
  pNet->cbCopyData = stream_net_aac_getSliceBytes;
}

#endif // VSX_HAVE_STREAMER
