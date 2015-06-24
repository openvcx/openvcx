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

enum STREAM_NET_ADVFR_RC stream_net_aconference_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg->pArgIn;
  enum STREAM_NET_ADVFR_RC rc;

  if(pStreamConf->pConferenceV && pStreamConf->ptsOffset == 0 && pStreamConf->pConferenceV->tvStart > 0) {
    TIME_VAL tvNow = timer_GetTime();
    if(tvNow > pStreamConf->pConferenceV->tvStart) {
      pStreamConf->ptsOffset = (int64_t)((double)(tvNow - pStreamConf->pConferenceV->tvStart) * 9 / 100.0); 
      LOG(X_DEBUG("stream_net_aconference_advanceFrame set ptsOffset:%.3f"), PTSF(pStreamConf->ptsOffset));
    }
  }

  if((rc = stream_net_check_time(&pStreamConf->tvStart, &pStreamConf->frame.frameId,
                                 pStreamConf->clockHz, pStreamConf->frameDeltaHz,
                                 pArg->pPts, &pStreamConf->ptsOffset)) == STREAM_NET_ADVFR_RC_OK) {

    pStreamConf->frame.szTot = pStreamConf->frameDeltaHz * 2;
    //fprintf(stderr, "ACONFERENCE FRAME SZ:%d, pts:%.3f\n", pStreamConf->frame.szTot, PTSF(*pArg->pPts));
    pStreamConf->frame.idxInSlice = 0;
    pStreamConf->frame.idxSlice = 0;

    if(pArg->plen) {
      *pArg->plen = pStreamConf->frame.szTot;
    }

    pArg->isvid = 0;

  }

  //if(rc == STREAM_NET_ADVFR_RC_OK) LOG(X_DEBUG("stream_net_aconference_advanceFrame returning: %d pts:%.3f, tvStart:%lld, frameid:%lld, ptsOffset:%.3f"), rc, pArg->pPts ? PTSF(*pArg->pPts) : 0, pStreamConf->tvStart, pStreamConf->frame.frameId, PTSF(pStreamConf->ptsOffset));

  return rc;
}


int64_t stream_net_aconference_frameDecodeHzOffset(void *pArg) {
  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_aconference_getNextSlice(void *pArg) {
  STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg;

  if(pStreamConf->frame.idxSlice++ > 1) {
    return NULL;
  }

  return &pStreamConf->frame.content;
}

int stream_net_aconference_szNextSlice(void *pArg) {
  return -1;
}

int stream_net_aconference_haveMoreData(void *pArg) {
  //STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg;

  return 1;
}

int stream_net_aconference_haveNextSlice(void *pArg) {
  STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg;

  if(pStreamConf->frame.idxSlice < 1) {
    return 1;
  }

  return 0;
}

int stream_net_aconference_getReadSliceBytes(void *pArg) {
  STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg;

  return pStreamConf->frame.idxInSlice;
}

int stream_net_aconference_getUnreadSliceBytes(void *pArg) {
  STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg;

  return pStreamConf->frame.szTot - pStreamConf->frame.idxInSlice;
}

int stream_net_aconference_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_ACONFERENCE_T *pStreamConf = (STREAM_ACONFERENCE_T *) pArg;

  if(pStreamConf->frame.idxInSlice >= pStreamConf->frame.szTot) {
    return 0;
  }

  if(len > pStreamConf->frame.szTot - pStreamConf->frame.idxInSlice) {
    len = pStreamConf->frame.szTot - pStreamConf->frame.idxInSlice;
  }

  memset(pBuf, 0, len);

  pStreamConf->frame.idxInSlice += len;

  return len;
}

void stream_net_aconference_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_LIVE;
  pNet->cbAdvanceFrame = stream_net_aconference_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_aconference_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_aconference_getNextSlice;
  pNet->cbHaveMoreData = stream_net_aconference_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_aconference_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_aconference_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_aconference_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_aconference_getReadSliceBytes;
  pNet->cbCopyData = stream_net_aconference_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
