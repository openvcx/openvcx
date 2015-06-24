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

#define PTS_FROM_HZ(pPktz) (((double)(pPktz)->hz / (pPktz)->clockRateHz) * 90000)

static int init_pcm_param(PKTZ_PCM_T *pPktz) {

  if(pPktz->pSdp) {
    pPktz->clockRateHz = pPktz->pSdp->common.clockHz;
  }
  if(pPktz->clockRateHz == 0) {
    pPktz->clockRateHz = 8000;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktz->pRtpMulti->init.clockRateHz == 0) {
    pPktz->pRtpMulti->init.clockRateHz = pPktz->clockRateHz;
  }

  if(pPktz->compoundSampleMs == 0) {
    pPktz->compoundSampleMs = 20;
  }
  if((pPktz->compoundBytes = pPktz->clockRateHz * pPktz->Bps * pPktz->channels *
       pPktz->compoundSampleMs / 1000) > pPktz->pRtpMulti->init.maxPayloadSz) {
    pPktz->compoundBytes = (pPktz->pRtpMulti->init.maxPayloadSz / 
      (pPktz->Bps * pPktz->channels)) * (pPktz->Bps * pPktz->channels) ;
  }
 
  pPktz->queuedBytes = 0;
  pPktz->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_pcm_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_PCM_T *pPktz = (PKTZ_PCM_T *) pArg;
  PKTZ_INIT_PARAMS_PCM_T *pInitParams = (PKTZ_INIT_PARAMS_PCM_T *) pInitArgs;
  int rc = 0;

  if(!pPktz || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktz->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktz->pXmitNode = pInitParams->common.pXmitNode;
  pPktz->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktz->clockRateHz = pInitParams->common.clockRateHz;
  pPktz->pFrameData = pInitParams->common.pFrameData;

  pPktz->pSdp = pInitParams->pSdp;
  if(pPktz->pSdp) {
    pPktz->channels = pPktz->pSdp->channels;
  }
  if(pPktz->channels) { 
    pPktz->channels = 1;
  }
  pPktz->isInitFromSdp = 0;
  pPktz->havePts0 = 0;
  pPktz->Bps = 1;
  pPktz->compoundSampleMs = pInitParams->compoundSampleMs;

  if(rc < 0) {
     stream_pktz_pcm_close(pPktz); 
  }

  return rc;
}

int stream_pktz_pcm_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_PCM_T *pPktz = (PKTZ_PCM_T *) pArg;

  pPktz->queuedBytes = 0;
  pPktz->havePts0 = 0;

  return 0;
}

int stream_pktz_pcm_close(void *pArg) {
  PKTZ_PCM_T *pPktz = (PKTZ_PCM_T *) pArg;

  if(pPktz == NULL) {
    return -1;
  }

  //if(pPktz->pbuf) {
  //  free(pPktz->pbuf);
  //  pPktz->pbuf = NULL;
  //}

  return 0;
}



static int sendpkt(PKTZ_PCM_T *pPktz, unsigned int len) {
  int rc = 0;

  //pPktz->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
  pPktz->pRtpMulti->pRtp->timeStamp =
    htonl(((double)pPktz->pts0 / 90000.0f * pPktz->clockRateHz) + pPktz->hz);

  pPktz->pRtpMulti->payloadLen = len;

  rc = stream_rtp_preparepkt(pPktz->pRtpMulti);

  //fprintf(stderr, "sendpkt rtp pcm ts:%u(%lluHz since ts0) seq:%d len:%d marker:%d\n", htonl(pPktz->pRtpMulti->pRtp->timeStamp), pPktz->hz, htons(pPktz->pRtpMulti->pRtp->sequence_num), pPktz->pRtpMulti->payloadLen,(pPktz->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  //LOG(X_DEBUG("sendpkt rtp pcm ts:%u(%lluHz since ts0) seq:%d len:%d marker:%d 0x%x 0x%x"), htonl(pPktz->pRtpMulti->pRtp->timeStamp), pPktz->hz, htons(pPktz->pRtpMulti->pRtp->sequence_num), pPktz->pRtpMulti->payloadLen,(pPktz->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0, pPktz->pRtpMulti->pPayload[0],pPktz->pRtpMulti->pPayload[1]); 

  if(rc >= 0) {
    rc = pPktz->cbXmitPkt(pPktz->pXmitNode);
  }

  pPktz->hz += (len / pPktz->Bps / pPktz->channels);

  return rc;
}

int stream_pktz_pcm_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_PCM_T *pPktz = (PKTZ_PCM_T *) pArg;
  unsigned int szcopy;
  int resetTm = 0;
  unsigned int idxData = 0;
  int rc = 0;

  if(!pPktz->isInitFromSdp) {
    init_pcm_param(pPktz);
  }
  if(!pPktz->havePts0) {
    resetTm = 1; 
  } else if(OUTFMT_PTS(pPktz->pFrameData) + 30000 < 
            (pPktz->pts0 + PTS_FROM_HZ(pPktz))) {
    resetTm = 1;
    LOG(X_WARNING("Adjusting audio rtp output pts from %.3f to %.3f"),
        PTSF(pPktz->pts0 + PTS_FROM_HZ(pPktz)), 
        PTSF(OUTFMT_PTS(pPktz->pFrameData)));
  }

  if(resetTm) {
    pPktz->havePts0 = 1;
    pPktz->pts0 = OUTFMT_PTS(pPktz->pFrameData);
    pPktz->hz = 0;
  }

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - PCM rtp-packetizer got frame tm:%.3f, len:%u (max:%d), rtp-ts:%u (%uHz) - data:"),
           PTSF(OUTFMT_PTS(pPktz->pFrameData)), OUTFMT_LEN(pPktz->pFrameData),
           pPktz->pRtpMulti->init.maxPayloadSz,
           htonl(pPktz->pRtpMulti->pRtp->timeStamp), pPktz->clockRateHz);
    if(pPktz->pFrameData) {
       LOGHEX_DEBUG(OUTFMT_DATA(pPktz->pFrameData), MIN(OUTFMT_LEN(pPktz->pFrameData), 16));
    }
  );

  //LOG(X_DEBUG("pktz_pcm_addframe[%d] %dHz len:%d inpts:%.3f rtppts:%.3f  pkt:[%d/%d] qd:%d comp:%d 0x%x 0x%x 0x%x 0x%x"), progIdx, pPktz->clockRateHz,pPktz->pFrameData->len, PTSF(pPktz->pFrameData->pts), PTSF(pPktz->pts0 + PTS_FROM_HZ(pPktz)), pPktz->pRtpMulti->payloadLen,pPktz->pRtpMulti->init.maxPayloadSz, pPktz->queuedBytes, pPktz->compoundBytes, pPktz->pFrameData->pData[0], pPktz->pFrameData->pData[1], pPktz->pFrameData->pData[2], pPktz->pFrameData->pData[3]);
  
  //fprintf(stderr, "pktz_pcm_addframe[%d] %dHz len:%d inpts:%.3f rtppts:%.3f  pkt:[%d/%d]\n", progIdx, pPktz->clockRateHz,pPktz->pFrameData->len, PTSF(pPktz->pFrameData->pts), PTSF(pPktz->pts0 + PTS_FROM_HZ(pPktz)), pPktz->pRtpMulti->payloadLen,pPktz->pRtpMulti->init.maxPayloadSz);

  if(pPktz->queuedBytes > 0) {
    szcopy = pPktz->compoundBytes - pPktz->queuedBytes;
    if(OUTFMT_LEN(pPktz->pFrameData) < szcopy) {
      szcopy = OUTFMT_LEN(pPktz->pFrameData);
    }
    memcpy(&pPktz->pRtpMulti->pPayload[pPktz->queuedBytes],
           OUTFMT_DATA(pPktz->pFrameData), szcopy);
    pPktz->queuedBytes += szcopy;
    idxData += szcopy;
    //fprintf(stderr, "copied %d\n", szcopy);

    if(pPktz->queuedBytes >= pPktz->compoundBytes) {
      if((rc = sendpkt(pPktz, pPktz->compoundBytes)) < 0) {
        return rc;
      }
      pPktz->queuedBytes = 0;
    }
  }

  while(idxData < OUTFMT_LEN(pPktz->pFrameData)) {
    //fprintf(stderr, "idxData:%d/%d comp:%d\n", idxData, pPktz->pFrameData->len, pPktz->compoundBytes);
    if(idxData + pPktz->compoundBytes <= OUTFMT_LEN(pPktz->pFrameData)) {
      memcpy(pPktz->pRtpMulti->pPayload, &OUTFMT_DATA(pPktz->pFrameData)[idxData], 
             pPktz->compoundBytes);
      if((rc = sendpkt(pPktz, pPktz->compoundBytes)) < 0) {
        return rc;
      }
      idxData += pPktz->compoundBytes;
    } else {
      pPktz->queuedBytes = OUTFMT_LEN(pPktz->pFrameData) - idxData;
      memcpy(pPktz->pRtpMulti->pPayload, &OUTFMT_DATA(pPktz->pFrameData)[idxData], 
             pPktz->queuedBytes);
      idxData += pPktz->queuedBytes;
    }
  }
    
  return rc;
}

#endif // VSX_HAVE_STREAMER
