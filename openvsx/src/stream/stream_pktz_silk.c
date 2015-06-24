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

static int init_silk_param(PKTZ_SILK_T *pPktzSilk) {

  if(pPktzSilk->pSdp) {
    pPktzSilk->clockRateHz = pPktzSilk->pSdp->common.clockHz;
  }


  if(pPktzSilk->clockRateHz == 0) {
    pPktzSilk->clockRateHz = 24000;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzSilk->pRtpMulti->init.clockRateHz == 0) {
    pPktzSilk->pRtpMulti->init.clockRateHz = pPktzSilk->clockRateHz;
  }

  if(pPktzSilk->compoundSampleMs == 0) {
    pPktzSilk->compoundSampleMs = 20;
  }

  //pPktzSilk->bsPkt.buf = pPktzSilk->buf;
  //pPktzSilk->bsPkt.sz = sizeof(pPktzSilk->buf);
  //pPktzSilk->bsPkt.idx = 0;
  //pPktzSilk->queuedFrames = 0;

  pPktzSilk->rtpTs0 = htonl(pPktzSilk->pRtpMulti->pRtp->timeStamp);
  
  pPktzSilk->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_silk_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_SILK_T *pPktzSilk = (PKTZ_SILK_T *) pArg;
  PKTZ_INIT_PARAMS_SILK_T *pInitParams = (PKTZ_INIT_PARAMS_SILK_T *) pInitArgs;
  int rc = 0;

  if(!pPktzSilk || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzSilk->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzSilk->pXmitNode = pInitParams->common.pXmitNode;
  pPktzSilk->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzSilk->clockRateHz = pInitParams->common.clockRateHz;
  pPktzSilk->pFrameData = pInitParams->common.pFrameData;
  //fprintf(stderr, "INIT CLOCK RATE SILK %d\n", pPktzSilk->clockRateHz);
  pPktzSilk->pSdp = pInitParams->pSdp;
  pPktzSilk->isInitFromSdp = 0;
  pPktzSilk->compoundSampleMs = pInitParams->compoundSampleMs;

  if(rc < 0) {
     stream_pktz_silk_close(pPktzSilk); 
  }

  return rc;
}

int stream_pktz_silk_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_SILK_T *pPktzSilk = (PKTZ_SILK_T *) pArg;

  //pPktzSilk->pRtpMulti->pktsSent = 0;
  pPktzSilk->pRtpMulti->payloadLen = 0;
  //pPktzSilk->bsPkt.idx = 0;
  //pPktzSilk->queuedFrames = 0;

  return 0;
}

int stream_pktz_silk_close(void *pArg) {
  PKTZ_SILK_T *pPktzSilk = (PKTZ_SILK_T *) pArg;

  if(pPktzSilk == NULL) {
    return -1;
  }

  //pPktzSilk->queuedFrames = 0;

  return 0;
}

//static int delme; static int32_t delmetsoffset;

static int sendpkt(PKTZ_SILK_T *pPktzSilk) {
  int rc = 0;
  //int payloadLen;

  pPktzSilk->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
  pPktzSilk->pRtpMulti->pRtp->timeStamp =
    htonl(pPktzSilk->rtpTs0 + (pPktzSilk->clockRateHz * ((double) pPktzSilk->pts / 90000.0f)));

  //fprintf(stderr, "TS:%u CLOCKR:%d\n", pPktzSilk->pRtpMulti->pRtp->timeStamp, pPktzSilk->clockRateHz);

  rc = stream_rtp_preparepkt(pPktzSilk->pRtpMulti);

//if(delme++ % 100 == 1) { fprintf(stderr, "jitter event...\n"); delmetsoffset=0; usleep(300000); }
//if(delme++ % 47 == 1) { fprintf(stderr, "dropping packet seq:%d\n", htons(pPktzSilk->pRtpMulti->pRtp->sequence_num));return 0;};
//pPktzSilk->pRtpMulti->pRtp->timeStamp=htonl(ntohl( pPktzSilk->pRtpMulti->pRtp->timeStamp)+delmetsoffset);
//fprintf(stderr, " silk ts offset %d\n", delmetsoffset);

  //fprintf(stderr, "sendpkt rtp silk ts:%u seq:%d len:%d marker:%d\n", htonl(pPktzSilk->pRtpMulti->pRtp->timeStamp), htons(pPktzSilk->pRtpMulti->pRtp->sequence_num), pPktzSilk->pRtpMulti->payloadLen,(pPktzSilk->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  //avc_dumpHex(stderr, pPktzSilk->pRtpMulti->pPayload, pPktzSilk->pRtpMulti->payloadLen > 80 ? 80 : pPktzSilk->pRtpMulti->payloadLen, 1);

  if(rc >= 0) {
    rc = pPktzSilk->cbXmitPkt(pPktzSilk->pXmitNode);
  }

  //pPktzSilk->pRtpMulti->payloadLen = 0;

  return rc;
}

int stream_pktz_silk_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_SILK_T *pPktzSilk = (PKTZ_SILK_T *) pArg;
  int rc = 0;

  if(!pPktzSilk->isInitFromSdp) {
    init_silk_param(pPktzSilk);
    //pPktzSilk->pts = OUTFMT_PTS(pPktzSilk->pFrameData);
  }

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - silk rtp-packetizer got frame tm:%.3f, len:%u, rtp-ts:%u - data:"),
      PTSF(OUTFMT_PTS(pPktzSilk->pFrameData)), OUTFMT_LEN(pPktzSilk->pFrameData),
         htonl(pPktzSilk->pRtpMulti->pRtp->timeStamp));
    if(pPktzSilk->pFrameData) {
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzSilk->pFrameData), MIN(OUTFMT_LEN(pPktzSilk->pFrameData), 16));
    }
  );

  //fprintf(stderr, "pktz_silkaddframe[%d] %dHz len:%d pts:%.3f pkt:[%d/%d]\n", progIdx, pPktzSilk->clockRateHz,OUTFMT_LEN(pPktzSilk->pFrameData), PTSF(OUTFMT_PTS(pPktzSilk->pFrameData)), pPktzSilk->pRtpMulti->payloadLen,pPktzSilk->pRtpMulti->init.maxPayloadSz);

  pPktzSilk->pts = OUTFMT_PTS(pPktzSilk->pFrameData); 
  memcpy(&pPktzSilk->pRtpMulti->pPayload[0], 
         OUTFMT_DATA(pPktzSilk->pFrameData), 
         OUTFMT_LEN(pPktzSilk->pFrameData));
  pPktzSilk->pRtpMulti->payloadLen = OUTFMT_LEN(pPktzSilk->pFrameData);

#if 0
  // Mimic appending VAD byte
  pPktzSilk->pRtpMulti->pPayload[pPktzSilk->pRtpMulti->payloadLen] = 0xff;
  pPktzSilk->pRtpMulti->payloadLen++;
#endif // 0

  rc = sendpkt(pPktzSilk);

  return rc;
}

#endif // VSX_HAVE_STREAMER
