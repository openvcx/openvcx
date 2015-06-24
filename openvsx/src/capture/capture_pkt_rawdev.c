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

#if defined(VSX_HAVE_CAPTURE)

typedef struct CAP_QUEUERAW_FRAME_DATA {
  CAPTURE_CBDATA_SP_T               *pSp;
  const COLLECT_STREAM_PKTDATA_T    *pPktData;
} CAP_QUEUERAW_FRAME_DATA_T;

static int capture_cbQueueRawDevFrame(void *pArg, const void *pData, uint32_t szData) {
  int rc = 0;
  CAP_QUEUERAW_FRAME_DATA_T *pFrameData = (CAP_QUEUERAW_FRAME_DATA_T *) pArg;
  PKTQ_EXTRADATA_T xtra;

  xtra.tm.pts = ((uint64_t)pFrameData->pPktData->tv.tv_sec * 90000) +
                ((double)pFrameData->pPktData->tv.tv_usec / TIME_VAL_US * 90000);
  xtra.tm.dts = 0;
  //xtra.flags = CAPTURE_SP_FLAG_KEYFRAME;
  xtra.pQUserData = NULL;
  //fprintf(stderr, "capture_cbWriteRawDevFrame len:%d %.3f(%llu) tv:%u:%u \n", szData, PTSF(xtra.tm.pts), xtra.tm.pts, pFrameData->pPktData->tv.tv_sec, pFrameData->pPktData->tv.tv_usec);

  rc = pktqueue_addpkt(pFrameData->pSp->pCapAction->pQueue, 
                         pFrameData->pPktData->payload.pData,
                         PKTLEN32(pFrameData->pPktData->payload), &xtra, 1);
  
  return rc;
}

int cbOnPkt_rawdev(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPktData) {
  int rc = 0;
  CAP_QUEUERAW_FRAME_DATA_T cbQueueFr;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;

  if(pSp == NULL) {
    return -1;
   } else if(!pPktData || !pPktData->payload.pData) {
    // Frame is missing or lost
    LOG(X_WARNING("Missing rawdev frame"));
    return 0;
  }

  //fprintf(stderr, "cbOnPkt_rawdev %u:%u len:%d\n", pPktData->tv.tv_sec, pPktData->tv.tv_usec, PKTLEN32(pPktData->payload));

  memset(&cbData, 0, sizeof(cbData));

  if(pSp->pCapAction && (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
    queueFr = 1;
  }

  if(queueFr) {

    cbQueueFr.pSp = pSp;
    cbQueueFr.pPktData = pPktData;

    cbData.pCbDataArg = (void *) &cbQueueFr;
    cbData.cbStoreData = capture_cbQueueRawDevFrame;

  } else if(pSp->stream1.fp != FILEOPS_INVALID_FP) {

    cbData.pCbDataArg = &pSp->stream1;
    cbData.cbStoreData = capture_cbWriteFile;

  } else {
    // Nothing to be done
    return 0;
  }

  rc = cbData.cbStoreData(cbData.pCbDataArg, pPktData->payload.pData, 
                          PKTLEN32(pPktData->payload));

  return rc;
}

#endif // VSX_HAVE_CAPTURE
