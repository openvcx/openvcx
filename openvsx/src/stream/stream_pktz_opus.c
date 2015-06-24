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

static int init_opus_param(PKTZ_OPUS_T *pPktzOpus) {

  if(pPktzOpus->pSdp) {
    pPktzOpus->clockRateHz = pPktzOpus->pSdp->common.clockHz;
  }

  if(pPktzOpus->clockRateHz == 0) {
    pPktzOpus->clockRateHz = 24000;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzOpus->pRtpMulti->init.clockRateHz == 0) {
    pPktzOpus->pRtpMulti->init.clockRateHz = pPktzOpus->clockRateHz;
  }

  if(pPktzOpus->compoundSampleMs == 0) {
    pPktzOpus->compoundSampleMs = 20;
  }

  //pPktzOpus->bsPkt.buf = pPktzOpus->buf;
  //pPktzOpus->bsPkt.sz = sizeof(pPktzOpus->buf);
  //pPktzOpus->bsPkt.idx = 0;
  //pPktzOpus->queuedFrames = 0;

  pPktzOpus->rtpTs0 = htonl(pPktzOpus->pRtpMulti->pRtp->timeStamp);
  
  pPktzOpus->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_opus_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_OPUS_T *pPktzOpus = (PKTZ_OPUS_T *) pArg;
  PKTZ_INIT_PARAMS_OPUS_T *pInitParams = (PKTZ_INIT_PARAMS_OPUS_T *) pInitArgs;
  int rc = 0;

  if(!pPktzOpus || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzOpus->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzOpus->pXmitNode = pInitParams->common.pXmitNode;
  pPktzOpus->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzOpus->clockRateHz = pInitParams->common.clockRateHz;
  pPktzOpus->pFrameData = pInitParams->common.pFrameData;
  //fprintf(stderr, "INIT CLOCK RATE OPUS %d\n", pPktzOpus->clockRateHz);
  pPktzOpus->pSdp = pInitParams->pSdp;
  pPktzOpus->isInitFromSdp = 0;
  pPktzOpus->compoundSampleMs = pInitParams->compoundSampleMs;

  if(rc < 0) {
     stream_pktz_opus_close(pPktzOpus); 
  }

  return rc;
}

int stream_pktz_opus_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_OPUS_T *pPktzOpus = (PKTZ_OPUS_T *) pArg;

  //pPktzOpus->pRtpMulti->pktsSent = 0;
  pPktzOpus->pRtpMulti->payloadLen = 0;
  //pPktzOpus->bsPkt.idx = 0;
  //pPktzOpus->queuedFrames = 0;

  return 0;
}

int stream_pktz_opus_close(void *pArg) {
  PKTZ_OPUS_T *pPktzOpus = (PKTZ_OPUS_T *) pArg;

  if(pPktzOpus == NULL) {
    return -1;
  }

  //pPktzOpus->queuedFrames = 0;

  return 0;
}

//static int delme; static int32_t delmetsoffset;

static int sendpkt(PKTZ_OPUS_T *pPktzOpus) {
  int rc = 0;

  pPktzOpus->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;

  //
  // Opus RTP timestamp is always in 48KHz units , regardless of actual internal sampling clock
  // http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03
  //
  pPktzOpus->pRtpMulti->pRtp->timeStamp =
    //htonl(pPktzOpus->rtpTs0 + (pPktzOpus->clockRateHz * ((double) pPktzOpus->pts / 90000.0f)));
    htonl(pPktzOpus->rtpTs0 + (STREAM_RTP_OPUS_CLOCKHZ * ((double) pPktzOpus->pts / 90000.0f)));

  //fprintf(stderr, "TS:%u CLOCKR:%d\n", pPktzOpus->pRtpMulti->pRtp->timeStamp, pPktzOpus->clockRateHz);

  rc = stream_rtp_preparepkt(pPktzOpus->pRtpMulti);

//if(delme++ % 100 == 1) { fprintf(stderr, "jitter event...\n"); delmetsoffset=0; usleep(300000); }
//if(delme++ % 47 == 1) { fprintf(stderr, "dropping packet seq:%d\n", htons(pPktzOpus->pRtpMulti->pRtp->sequence_num));return 0;};
//pPktzOpus->pRtpMulti->pRtp->timeStamp=htonl(ntohl( pPktzOpus->pRtpMulti->pRtp->timeStamp)+delmetsoffset);
//fprintf(stderr, " opus ts offset %d\n", delmetsoffset);

  //fprintf(stderr, "sendpkt rtp opus ts:%u seq:%d len:%d marker:%d\n", htonl(pPktzOpus->pRtpMulti->pRtp->timeStamp), htons(pPktzOpus->pRtpMulti->pRtp->sequence_num), pPktzOpus->pRtpMulti->payloadLen,(pPktzOpus->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  //avc_dumpHex(stderr, pPktzOpus->pRtpMulti->pPayload, pPktzOpus->pRtpMulti->payloadLen > 80 ? 80 : pPktzOpus->pRtpMulti->payloadLen, 1);

  if(rc >= 0) {
    rc = pPktzOpus->cbXmitPkt(pPktzOpus->pXmitNode);
  }

  //pPktzOpus->pRtpMulti->payloadLen = 0;

  return rc;
}

int stream_pktz_opus_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_OPUS_T *pPktzOpus = (PKTZ_OPUS_T *) pArg;
  int rc = 0;

  if(!pPktzOpus->isInitFromSdp) {
    init_opus_param(pPktzOpus);
    //pPktzOpus->pts = OUTFMT_PTS(pPktzOpus->pFrameData);
  }

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - opus rtp-packetizer got frame tm:%.3f, len:%u, rtp-ts:%u - data:"),
      PTSF(OUTFMT_PTS(pPktzOpus->pFrameData)), OUTFMT_LEN(pPktzOpus->pFrameData),
         htonl(pPktzOpus->pRtpMulti->pRtp->timeStamp));
    if(pPktzOpus->pFrameData) {
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzOpus->pFrameData), MIN(OUTFMT_LEN(pPktzOpus->pFrameData), 16));
    }
  );

  //LOG(X_DEBUG("pktz_opusaddframe[%d] %dHz len:%d pts:%.3f pkt:[%d/%d]"), progIdx, pPktzOpus->clockRateHz,OUTFMT_LEN(pPktzOpus->pFrameData), PTSF(OUTFMT_PTS(pPktzOpus->pFrameData)), pPktzOpus->pRtpMulti->payloadLen,pPktzOpus->pRtpMulti->init.maxPayloadSz);

  pPktzOpus->pts = OUTFMT_PTS(pPktzOpus->pFrameData); 
  memcpy(&pPktzOpus->pRtpMulti->pPayload[0], 
         OUTFMT_DATA(pPktzOpus->pFrameData), 
         OUTFMT_LEN(pPktzOpus->pFrameData));
  pPktzOpus->pRtpMulti->payloadLen = OUTFMT_LEN(pPktzOpus->pFrameData);

#if 0
  // Mimic appending VAD byte
  pPktzOpus->pRtpMulti->pPayload[pPktzOpus->pRtpMulti->payloadLen] = 0xff;
  pPktzOpus->pRtpMulti->payloadLen++;
#endif // 0

  rc = sendpkt(pPktzOpus);

  return rc;
}

#endif // VSX_HAVE_STREAMER
