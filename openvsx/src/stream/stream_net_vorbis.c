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


enum STREAM_NET_ADVFR_RC stream_net_vorbis_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg->pArgIn;
  VORBIS_SAMPLE_T *pSample;

  //fprintf(stderr, "vorbis_advanceFrame called pSlice:0x%x frameId:%d szTot:%d\n", pStreamVorbis->frame.pSlice, pStreamVorbis->frame.frameId, pStreamVorbis->frame.szTot);

  pSample = vorbis_cbGetNextSample(pStreamVorbis->pVorbis, pStreamVorbis->idxFrame++, 0);

  if(pSample == NULL) {
    if(pStreamVorbis->loop) {
      //TODO: not implemented
      return STREAM_NET_ADVFR_RC_ERROR;
    }
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  pStreamVorbis->frame.idxSlice = 0;
  pStreamVorbis->frame.idxInFrame = 0;
  pStreamVorbis->frame.pSample = pSample;
  pStreamVorbis->frame.szTot = pSample->sizeRd;
  pStreamVorbis->frame.frameId = pSample->frameId;
  pStreamVorbis->frame.pFileStream = pSample->pStreamIn;


  if(pArg->pPts) {
    *pArg->pPts = (uint64_t) ((double) pSample->playOffsetHz *
          (double) (90000.0 / pStreamVorbis->pVorbis->clockHz));
  //fprintf(stderr, "playOffsetHz:%.3f, sampleDurationHz:%d\n", (double)pSample->playOffsetHz/90000.0f, pStreamVorbis->pVorbis->clockHz);
  }

  if(pArg->plen) {
    *pArg->plen = pStreamVorbis->frame.szTot;
  }

  pArg->isvid = 0;

  //fprintf(stderr, "vorbis advanceFrame returning totlen:%d pts:%.3f\n", pStreamVorbis->frame.szTot,   PTSF(*pArg->pPts));

  return STREAM_NET_ADVFR_RC_OK;
}


int64_t stream_net_vorbis_frameDecodeHzOffset(void *pArg) {
  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_vorbis_getNextSlice(void *pArg) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg;

  pStreamVorbis->frame.idxInFrame = 0;
  if(pStreamVorbis->frame.idxSlice++ > 1) {
    return NULL;
  }

  return pStreamVorbis->frame.pSample;
}

int stream_net_vorbis_szNextSlice(void *pArg) {
  return -1;
}

int stream_net_vorbis_haveMoreData(void *pArg) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg;

  if(pStreamVorbis->loop ||
     (pStreamVorbis->frame.pSample && pStreamVorbis->frame.idxInFrame < pStreamVorbis->frame.szTot)) { 
    return 1;
  } 

  return 0;
}

int stream_net_vorbis_haveNextSlice(void *pArg) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg;

  if(pStreamVorbis->frame.pSample && pStreamVorbis->frame.idxSlice < 1) {
    return 1;
  }

  return 0;
}


int stream_net_vorbis_getReadSliceBytes(void *pArg) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg;

  return pStreamVorbis->frame.idxInFrame;
}


int stream_net_vorbis_getUnreadSliceBytes(void *pArg) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg;

  if(pStreamVorbis->frame.pSample == NULL) {
    return 0;
  }

  return pStreamVorbis->frame.pSample->sizeRd - pStreamVorbis->frame.idxInFrame;
}

int stream_net_vorbis_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_VORBIS_T *pStreamVorbis = (STREAM_VORBIS_T *) pArg;
  uint32_t szSample;
  unsigned int lenToRead = len;
  unsigned int bufIdx = 0;
  unsigned int fsIdxInSlice;

  if(pStreamVorbis->frame.pSample == NULL) {
    return 0;
  }

  fsIdxInSlice = pStreamVorbis->frame.idxInFrame;
  szSample = pStreamVorbis->frame.pSample->sizeWr;

  if(len > szSample - pStreamVorbis->frame.idxInFrame) {
    LOG(X_WARNING("Decreasing copy length from %u to (%u - %u)"), 
                  len, szSample, pStreamVorbis->frame.idxInFrame);
    len = szSample - pStreamVorbis->frame.idxInFrame;
  }

  if(SeekMediaFile(pStreamVorbis->frame.pFileStream, 
                    pStreamVorbis->frame.pSample->fileOffset + 
                    fsIdxInSlice, SEEK_SET) != 0) {
    return -1;
  }

  if(ReadFileStream(pStreamVorbis->frame.pFileStream, &pBuf[bufIdx], lenToRead) < 0) {
    return -1;
  }

  pStreamVorbis->frame.idxInFrame += lenToRead;

  return len;
}

float stream_net_vorbis_jump(STREAM_VORBIS_T *pStreamVorbis, float fStartSec) {

  return -1.0f;
}

void stream_net_vorbis_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_vorbis_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_vorbis_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_vorbis_getNextSlice;
  pNet->cbHaveMoreData = stream_net_vorbis_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_vorbis_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_vorbis_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_vorbis_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_vorbis_getReadSliceBytes;
  pNet->cbCopyData = stream_net_vorbis_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
