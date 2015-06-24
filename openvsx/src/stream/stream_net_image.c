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


enum STREAM_NET_ADVFR_RC stream_net_image_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg->pArgIn;
  enum STREAM_NET_ADVFR_RC rc;

  if(pStreamImage->clockHz > 0 && pStreamImage->frameDeltaHz > 0) {
    if((rc = stream_net_check_time(&pStreamImage->tvStart, &pStreamImage->frameId,
                                 pStreamImage->clockHz, pStreamImage->frameDeltaHz,
                                 pArg->pPts, NULL)) != STREAM_NET_ADVFR_RC_OK) {
      return rc;
    }
    pStreamImage->idxFrame = pStreamImage->idxInFrame = 0;
  } else if(pArg->pPts) {
    *pArg->pPts = 0;
  }

  //fprintf(stderr, "IMAGE_ADVFR idxF:%d, pts:%.3f\n", pStreamImage->idxFrame, PTSF(*pArg->pPts));

  if(pStreamImage->idxFrame > 0) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }


  if(pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 1;
  }

  if(pArg->plen) {
    *pArg->plen = pStreamImage->pImage->bs.sz;
  }

  pArg->isvid = 1;

  return STREAM_NET_ADVFR_RC_OK;
}



int64_t stream_net_image_frameDecodeHzOffset(void *pArg) {
  //STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_image_getNextSlice(void *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_GETNEXTSLICE idxF:%d, sz:%d\n", pStreamImage->idxFrame, pStreamImage->node.sizeRd = pStreamImage->pImage->bs.sz);

  if(pStreamImage->idxFrame > 0) {
    return NULL;
  }
  pStreamImage->idxFrame++;

  pStreamImage->node.sizeWr = pStreamImage->node.sizeRd = pStreamImage->pImage->bs.sz;

  return  &pStreamImage->node;
}

int stream_net_image_szNextSlice(void *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_SZNEXTSLICE idxF:%d\n", pStreamImage->idxFrame);

  if(pStreamImage->idxFrame > 0) {
    return 0;
  }

  return pStreamImage->pImage->bs.sz;
}

int stream_net_image_haveMoreData(void *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_HAVEMOREDATA idxF:%d\n", pStreamImage->idxFrame);

  if(pStreamImage->idxInFrame < pStreamImage->pImage->bs.sz) {
    return 1;
  } else if(pStreamImage->idxFrame == 0) {
    return 1;
  }

  return 0;
}


int stream_net_image_haveNextSlice(void *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_HAVENEXTSLICE idxF:%d\n", pStreamImage->idxFrame);

  if(pStreamImage->idxFrame == 0) {
    return 1;
  }

  return 0;
}


int stream_net_image_getReadSliceBytes(void *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_idxF:%d\n", pStreamImage->idxFrame);

  return pStreamImage->idxInFrame;
}


int stream_net_image_getUnreadSliceBytes(void *pArg) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_GETUNREAD idxF:%d unread:%d\n", pStreamImage->idxFrame, pStreamImage->pImage->bs.sz - pStreamImage->idxInFrame);

  return pStreamImage->pImage->bs.sz - pStreamImage->idxInFrame;
}

int stream_net_image_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_IMAGE_T *pStreamImage = (STREAM_IMAGE_T *) pArg;

  //fprintf(stderr, "IMAGE_GETSLICEBYTES idxF:%d request len:%d, has len:%d\n", pStreamImage->idxFrame, len, pStreamImage->pImage->bs.sz - pStreamImage->idxInFrame);

  if(len > pStreamImage->pImage->bs.sz - pStreamImage->idxInFrame) {
    LOG(X_WARNING("Decreasing image copy length from %u to (%u - %u)"),
                  len, pStreamImage->pImage->bs.sz, pStreamImage->idxInFrame);
    len = pStreamImage->pImage->bs.sz - pStreamImage->idxInFrame;
  }

  memcpy(pBuf, &pStreamImage->pImage->bs.buf[pStreamImage->idxInFrame], len);
  pStreamImage->idxInFrame += len;

  return len;
}


void stream_net_image_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_image_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_image_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_image_getNextSlice;
  pNet->cbHaveMoreData = stream_net_image_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_image_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_image_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_image_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_image_getReadSliceBytes;
  pNet->cbCopyData = stream_net_image_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
