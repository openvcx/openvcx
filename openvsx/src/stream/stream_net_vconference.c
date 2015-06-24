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

enum STREAM_NET_ADVFR_RC stream_net_vconference_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg->pArgIn;
  enum STREAM_NET_ADVFR_RC rc;



  //
  // Check if the conference PIP overlay has expired
  //
  if(pip_check_idle(pStreamConf->pCfgOverlay)) {
    return STREAM_NET_ADVFR_RC_ERROR; 
  }

  if((rc = stream_net_check_time(&pStreamConf->tvStart, &pStreamConf->frame.frameId, 
                                 pStreamConf->clockHz, pStreamConf->frameDeltaHz, 
                                 pArg->pPts, &pStreamConf->ptsOffset)) == STREAM_NET_ADVFR_RC_OK) {

    pStreamConf->frame.szTot = 1;
    pStreamConf->frame.idxInSlice = 0;
    pStreamConf->frame.idxSlice = 0;

    if(pArg->plen) {
      *pArg->plen = pStreamConf->frame.szTot;
    }

    if(pArg->pkeyframeIn) {
      *pArg->pkeyframeIn = 1;
    }

    pArg->isvid = 1;

  }

  //if(rc == STREAM_NET_ADVFR_RC_OK) LOG(X_DEBUG("stream_net_vconference_advanceFrame returning: %d pts:%.3f, tvStart:%lld, frameid:%lld, ptsOffset:%.3f"), rc, pArg->pPts ? PTSF(*pArg->pPts) : 0, pStreamConf->tvStart, pStreamConf->frame.frameId, PTSF(pStreamConf->ptsOffset));

  return rc;
}


int64_t stream_net_vconference_frameDecodeHzOffset(void *pArg) {
  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_vconference_getNextSlice(void *pArg) {
  STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg;

  if(pStreamConf->frame.idxSlice++ > 1) {
    return NULL;
  }

  return &pStreamConf->frame.content;
}

int stream_net_vconference_szNextSlice(void *pArg) {
  return -1;
}

int stream_net_vconference_haveMoreData(void *pArg) {
  //STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg;

  return 1;
}

int stream_net_vconference_haveNextSlice(void *pArg) {
  STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg;

  if(pStreamConf->frame.idxSlice < 1) {
    return 1;
  }

  return 0;
}

int stream_net_vconference_getReadSliceBytes(void *pArg) {
  STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg;

  return pStreamConf->frame.idxInSlice;
}

int stream_net_vconference_getUnreadSliceBytes(void *pArg) {
  STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg;

  return pStreamConf->frame.szTot - pStreamConf->frame.idxInSlice;
}

int stream_net_vconference_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_VCONFERENCE_T *pStreamConf = (STREAM_VCONFERENCE_T *) pArg;

  if(pStreamConf->frame.idxInSlice >= pStreamConf->frame.szTot) {
    return 0;
  }

  if(len > pStreamConf->frame.szTot - pStreamConf->frame.idxInSlice) {
    len = pStreamConf->frame.szTot - pStreamConf->frame.idxInSlice;
  }

  pBuf[0] = 0x00; 

  pStreamConf->frame.idxInSlice += len;

  return len;
}

void stream_net_vconference_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_LIVE;
  pNet->cbAdvanceFrame = stream_net_vconference_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_vconference_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_vconference_getNextSlice;
  pNet->cbHaveMoreData = stream_net_vconference_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_vconference_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_vconference_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_vconference_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_vconference_getReadSliceBytes;
  pNet->cbCopyData = stream_net_vconference_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
