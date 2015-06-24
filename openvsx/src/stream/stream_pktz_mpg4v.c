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

static int init_mpg4v_param(PKTZ_MPG4V_T *pPktzMpg4v) {

  if(pPktzMpg4v->pSdp) {
    pPktzMpg4v->clockRateHz = pPktzMpg4v->pSdp->common.clockHz;
  }
  if(pPktzMpg4v->clockRateHz == 0) {
    pPktzMpg4v->clockRateHz = MPEG4_DEFAULT_CLOCKHZ;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzMpg4v->pRtpMulti->init.clockRateHz == 0) {
    pPktzMpg4v->pRtpMulti->init.clockRateHz = pPktzMpg4v->clockRateHz;
  }

  
  pPktzMpg4v->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_mpg4v_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_MPG4V_T *pPktzMpg4v = (PKTZ_MPG4V_T *) pArg;
  PKTZ_INIT_PARAMS_MPG4V_T *pInitParams = (PKTZ_INIT_PARAMS_MPG4V_T *) pInitArgs;
  int rc = 0;

  if(!pPktzMpg4v || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzMpg4v->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzMpg4v->pXmitNode = pInitParams->common.pXmitNode;
  pPktzMpg4v->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzMpg4v->clockRateHz = pInitParams->common.clockRateHz;
  pPktzMpg4v->pFrameData = pInitParams->common.pFrameData;

  pPktzMpg4v->pSdp = pInitParams->pSdp;
  pPktzMpg4v->isInitFromSdp = 0;

  if(rc < 0) {
     stream_pktz_mpg4v_close(pPktzMpg4v); 
  }

  return rc;
}

int stream_pktz_mpg4v_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_MPG4V_T *pPktzMpg4v = (PKTZ_MPG4V_T *) pArg;

  //pPktzMpg4v->pRtpMulti->pktsSent = 0;
  pPktzMpg4v->pRtpMulti->payloadLen = 0;

  return 0;
}

int stream_pktz_mpg4v_close(void *pArg) {
  PKTZ_MPG4V_T *pPktzMpg4v = (PKTZ_MPG4V_T *) pArg;

  if(pPktzMpg4v == NULL) {
    return -1;
  }

  return 0;
}

static int sendpkt(PKTZ_MPG4V_T *pPktzMpg4v) {
  int rc = 0;

  rc = stream_rtp_preparepkt(pPktzMpg4v->pRtpMulti);

  //fprintf(stderr, "sendpkt rtp mpg4v ts:%u seq:%d len:%d marker:%d\n", htonl(pPktzMpg4v->pRtpMulti->pRtp->timeStamp), htons(pPktzMpg4v->pRtpMulti->pRtp->sequence_num), pPktzMpg4v->pRtpMulti->payloadLen, (pPktzMpg4v->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  //avc_dumpHex(stderr, pPktzMpg4v->pRtpMulti->pPayload, pPktzMpg4v->pRtpMulti->payloadLen > 80 ? 80 : pPktzMpg4v->pRtpMulti->payloadLen, 1);

  if(rc >= 0 && (rc = pPktzMpg4v->cbXmitPkt(pPktzMpg4v->pXmitNode)) < 0) {
    LOG(X_ERROR("MPEG-4 packetizer failed to send output packet length %d"), stream_rtp_datalen(pPktzMpg4v->pXmitNode->pRtpMulti));
  }

  pPktzMpg4v->pRtpMulti->payloadLen = 0;

  return rc;
}

/*
static void testmpg4(PKTZ_MPG4V_T *pPktzMpg4v) {
  static unsigned char *s_p[2];
  static unsigned int s_psizes[2];
  static unsigned int s_pidx;
  static unsigned char testdelme[] = { 0x00, 0x00, 0x01, 0xb6 };
  OUTFMT_FRAME_DATA_T *pFrameData = (OUTFMT_FRAME_DATA_T *) pPktzMpg4v->pFrameData;

  if(!s_p[0]) {
    s_p[0] = malloc(0xffff);
    s_p[1] = malloc(0xffff);
  }

  memcpy(s_p[s_pidx], OUTFMT_DATA(pFrameData), OUTFMT_LEN(pFrameData));
  s_psizes[s_pidx] = OUTFMT_LEN(pFrameData);

  if(++s_pidx > 1) {
    s_pidx = 0;
  }

  if(OUTFMT_KEYFRAME(pFrameData)) {
    // Change the frame prior to this I-frame
    memcpy(s_p[s_pidx], testdelme, sizeof(testdelme));
    s_psizes[s_pidx] = sizeof(testdelme);
  }

  OUTFMT_DATA(pFrameData) = s_p[s_pidx];
  OUTFMT_LEN(pFrameData) = s_psizes[s_pidx];

  //if(! OUTFMT_KEYFRAME(pFrameData)) {
  //   const char p[] = { 0x00, 0x00, 0x01, 0x20, 0x00, 0xc4, 0x8d, 0x88, 0x00, 0x65, 0x0c, 0x84, 0x1e, 0x14, 0x43 };
  //   memcpy(s_p, p, sizeof(p));
  //   memcpy(&s_p[sizeof(p)], OUTFMT_DATA(pFrameData), OUTFMT_LEN(pFrameData));
  //   OUTFMT_DATA(pFrameData) = s_p;
  //   OUTFMT_LEN(pFrameData) += sizeof(p);
  //}
}
*/

int stream_pktz_mpg4v_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_MPG4V_T *pPktzMpg4v = (PKTZ_MPG4V_T *) pArg;
  int rc = 0;
  unsigned int idx = 0;
  unsigned int szcopy;
  
  if(!pPktzMpg4v->isInitFromSdp) {
    init_mpg4v_param(pPktzMpg4v);
  }

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - mpg4v rtp-packetizer got frame tm:%.3f, len:%u, rtp-ts:%u - data:"),
      PTSF(OUTFMT_PTS(pPktzMpg4v->pFrameData)), OUTFMT_LEN(pPktzMpg4v->pFrameData), 
         htonl(pPktzMpg4v->pRtpMulti->pRtp->timeStamp)); 
     if(pPktzMpg4v->pFrameData) {       
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzMpg4v->pFrameData), MIN(OUTFMT_LEN(pPktzMpg4v->pFrameData), 16));
    }
  );

  //fprintf(stderr, "pktz_mpg4v_addframe[%d] %dHz len:%d pts:%.3f dts:%.3f key:%d fr:[%d] pkt:[%d/%d]\n", progIdx, pPktzMpg4v->clockRateHz,OUTFMT_LEN(pPktzMpg4v->pFrameData), PTSF(OUTFMT_PTS(pPktzMpg4v->pFrameData)), PTSF(OUTFMT_DTS(pPktzMpg4v->pFrameData)), OUTFMT_KEYFRAME(pPktzMpg4v->pFrameData), OUTFMT_LEN(pPktzMpg4v->pFrameData), pPktzMpg4v->pRtpMulti->payloadLen,pPktzMpg4v->pRtpMulti->init.maxPayloadSz);
//avc_dumpHex(stderr, OUTFMT_DATA(pPktzMpg4v->pFrameData), MIN(16, OUTFMT_LEN(pPktzMpg4v->pFrameData)), 1);


  if(OUTFMT_KEYFRAME(pPktzMpg4v->pFrameData)) {
    pPktzMpg4v->pXmitNode->cur_iskeyframe = 1;
  } else {
    pPktzMpg4v->pXmitNode->cur_iskeyframe = 0;
  }

  //testmpg4(pPktzMpg4v);

  pPktzMpg4v->pRtpMulti->pRtp->pt &= ~RTP_PT_MARKER_MASK;
  pPktzMpg4v->pRtpMulti->payloadLen = 0;
  pPktzMpg4v->pRtpMulti->pRtp->timeStamp =
    htonl(pPktzMpg4v->clockRateHz * PTSF( OUTFMT_PTS(pPktzMpg4v->pFrameData)));

  while(idx < OUTFMT_LEN(pPktzMpg4v->pFrameData)) {

    if((szcopy = OUTFMT_LEN(pPktzMpg4v->pFrameData) - idx) > 
       pPktzMpg4v->pRtpMulti->init.maxPayloadSz) {
      szcopy = pPktzMpg4v->pRtpMulti->init.maxPayloadSz;
    }

    memcpy(pPktzMpg4v->pRtpMulti->pPayload, &OUTFMT_DATA(pPktzMpg4v->pFrameData)[idx], szcopy);
    idx += szcopy;
    pPktzMpg4v->pRtpMulti->payloadLen = szcopy;
 
    //
    // Set RTP marker bit
    //
    if(idx >= OUTFMT_LEN(pPktzMpg4v->pFrameData)) {
      pPktzMpg4v->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
    }

    if(sendpkt(pPktzMpg4v) < 0) {
      return -1;
    }

    //
    // Only enable the keyframe for the first packet of any keyframe.  Outbound filters such as RTSP
    // will check for keyframe presence before starting transmission.  We don't want them to pick up
    // a frame's packet mid-stream, after the initial keyframe packet.
    //
    pPktzMpg4v->pXmitNode->cur_iskeyframe = 0;

  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
