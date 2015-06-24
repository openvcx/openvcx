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


enum STREAM_NET_ADVFR_RC stream_net_vp8_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg->pArgIn;
  VP8_SAMPLE_T *pSample = NULL;
  int isKeyFrame = 0;

  //fprintf(stderr, "vp8_advanceFrame called pSlice:0x%x frameId:%d szTot:%d\n", pStreamVp8->frame.pSlice, pStreamVp8->frame.frameId, pStreamVp8->frame.szTot);

  if(pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 0;
  }

  pStreamVp8->frame.szTot = 0;

  //if(pStreamMpg4v->loop && pStreamMpg4v->idxSlice >= pStreamMpg4v->pMpg4v->lastFrameId) {
  //  LOG(X_DEBUG("Looping vp8 video stream"));
  //  pStreamMpg4v->idxSlice = 0;
  //}

  if((pSample = vp8_cbGetNextSample(pStreamVp8->pVp8, pStreamVp8->idxSlice++, 0)) == NULL) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  pStreamVp8->frame.idxInSlice = 0;
  pStreamVp8->frame.idxSlice = 0;
  pStreamVp8->frame.frameId = pSample->frameId;
  pStreamVp8->frame.pSample = pSample;
  pStreamVp8->frame.szTot = pSample->sizeRd;
  pStreamVp8->frame.pFileStream = pSample->pStreamIn;

  if((pSample->flags & MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE)) {
    isKeyFrame = 1;
  } 

  if(pArg->pPts) {
    *pArg->pPts = (uint64_t) ((double) pSample->playOffsetHz *
          (double) (90000.0 / pStreamVp8->pVp8->clockHz));
  //fprintf(stderr, "playOffsetHz:%.3f, sampleDurationHz:%d\n", (double)pSample->playOffsetHz/90000.0f, pStreamMpg4v->pMpg4v->clockHz);
  }

  if(pArg->plen) {
    *pArg->plen = pStreamVp8->frame.szTot;
  }

  if(isKeyFrame && pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 1;
  }

  pArg->isvid = 1;

  //fprintf(stderr, "vp8 advanceFrame returning totlen:%d keyframe:%d pts:%.3f\n", pStreamVp8->frame.szTot,  isKeyFrame,  PTSF(*pArg->pPts));

  return STREAM_NET_ADVFR_RC_OK;
}


int64_t stream_net_vp8_frameDecodeHzOffset(void *pArg) {
  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_vp8_getNextSlice(void *pArg) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg;

  pStreamVp8->frame.idxInSlice = 0;
  if(pStreamVp8->frame.idxSlice++ > 1) {
    return NULL;
  }

  return pStreamVp8->frame.pSample;
}

int stream_net_vp8_szNextSlice(void *pArg) {
  return -1;
}

int stream_net_vp8_haveMoreData(void *pArg) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg;

  if(pStreamVp8->loop ||
     pStreamVp8->idxSlice < pStreamVp8->pVp8->numSamples ||
     (pStreamVp8->frame.pSample && pStreamVp8->frame.idxInSlice < pStreamVp8->frame.szTot)) { 
    return 1;
  }

  return 0;
}

int stream_net_vp8_haveNextSlice(void *pArg) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg;

  if(pStreamVp8->frame.pSample &&
     pStreamVp8->frame.idxSlice < 1) {
    return 1;
  }
  return 0;
}


int stream_net_vp8_getReadSliceBytes(void *pArg) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg;

  return pStreamVp8->frame.idxInSlice;
}


int stream_net_vp8_getUnreadSliceBytes(void *pArg) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg;

  if(pStreamVp8->frame.pSample == NULL) {
    return 0;
  }

  return pStreamVp8->frame.pSample->sizeRd - pStreamVp8->frame.idxInSlice;
}

int stream_net_vp8_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_VP8_T *pStreamVp8 = (STREAM_VP8_T *) pArg;
  uint32_t szSample;
  unsigned int lenToRead = len;
  unsigned int bufIdx = 0;
  unsigned int fsIdxInSlice;

  if(pStreamVp8->frame.pSample == NULL) {
    return 0;
  }

  fsIdxInSlice = pStreamVp8->frame.idxInSlice;
  szSample = pStreamVp8->frame.pSample->sizeWr;

  if(len > szSample - pStreamVp8->frame.idxInSlice) {
    LOG(X_WARNING("Decreasing copy length from %u to (%u - %u)"), 
                  len, szSample, pStreamVp8->frame.idxInSlice);
    len = szSample - pStreamVp8->frame.idxInSlice;
  }


  if(SeekMediaFile(pStreamVp8->frame.pFileStream, 
                    pStreamVp8->frame.pSample->fileOffset + 
                    fsIdxInSlice, SEEK_SET) != 0) {
    return -1;
  }

  if(ReadFileStream(pStreamVp8->frame.pFileStream, &pBuf[bufIdx], lenToRead) < 0) {
    return -1;
  }

  pStreamVp8->frame.idxInSlice += lenToRead;

  return len;
}

float stream_net_vp8_jump(STREAM_VP8_T *pStreamMpg4v, float fStartSec, int syncframe) {

  return -1.0f;
}

void stream_net_vp8_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_vp8_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_vp8_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_vp8_getNextSlice;
  pNet->cbHaveMoreData = stream_net_vp8_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_vp8_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_vp8_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_vp8_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_vp8_getReadSliceBytes;
  pNet->cbCopyData = stream_net_vp8_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
