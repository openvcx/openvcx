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

static int init_vp8_param(PKTZ_VP8_T *pPktzVp8) {

  if(pPktzVp8->pSdp) {
    pPktzVp8->clockRateHz = pPktzVp8->pSdp->common.clockHz;
  }
  if(pPktzVp8->clockRateHz == 0) {
    pPktzVp8->clockRateHz = VP8_DEFAULT_CLOCKHZ;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzVp8->pRtpMulti->init.clockRateHz == 0) {
    pPktzVp8->pRtpMulti->init.clockRateHz = pPktzVp8->clockRateHz;
  }

  pPktzVp8->rtpTs0 = htonl(pPktzVp8->pRtpMulti->pRtp->timeStamp);

  pPktzVp8->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_vp8_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_VP8_T *pPktzVp8 = (PKTZ_VP8_T *) pArg;
  PKTZ_INIT_PARAMS_VP8_T *pInitParams = (PKTZ_INIT_PARAMS_VP8_T *) pInitArgs;
  int rc = 0;

  if(!pPktzVp8 || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzVp8->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzVp8->pXmitNode = pInitParams->common.pXmitNode;
  pPktzVp8->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzVp8->clockRateHz = pInitParams->common.clockRateHz;
  pPktzVp8->pFrameData = pInitParams->common.pFrameData;
  pPktzVp8->pSdp = pInitParams->pSdp;
  pPktzVp8->isInitFromSdp = 0;

  pPktzVp8->pictureId = 0;
  

  if(rc < 0) {
     stream_pktz_vp8_close(pPktzVp8); 
  }

  return rc;
}

int stream_pktz_vp8_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_VP8_T *pPktzVp8 = (PKTZ_VP8_T *) pArg;

  pPktzVp8->pRtpMulti->payloadLen = 0;

  return 0;
}

int stream_pktz_vp8_close(void *pArg) {
  PKTZ_VP8_T *pPktzVp8 = (PKTZ_VP8_T *) pArg;

  if(pPktzVp8 == NULL) {
    return -1;
  }

  return 0;
}

static int sendpkt(PKTZ_VP8_T *pPktzVp8) {
  int rc = 0;

  rc = stream_rtp_preparepkt(pPktzVp8->pRtpMulti);

  //if(htons(pPktzVp8->pRtpMulti->pRtp->sequence_num) %30 == 29) pPktzVp8->pRtpMulti->payloadLen=0;

  //LOG(X_DEBUG("sendpkt rtp vp8 ts:%u seq:%d len:%d marker:%d"), htonl(pPktzVp8->pRtpMulti->pRtp->timeStamp), htons(pPktzVp8->pRtpMulti->pRtp->sequence_num), pPktzVp8->pRtpMulti->payloadLen, (pPktzVp8->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0); 
  //LOGHEX_DEBUG(pPktzVp8->pRtpMulti->pPayload, MIN(16, pPktzVp8->pRtpMulti->payloadLen));

  if(rc >= 0 && (rc = pPktzVp8->cbXmitPkt(pPktzVp8->pXmitNode)) < 0) {
    LOG(X_ERROR("VP8 packetizer failed to send output packet length %d"), stream_rtp_datalen(pPktzVp8->pXmitNode->pRtpMulti));
  }

  pPktzVp8->pRtpMulti->payloadLen = 0;

  return rc;
}


int stream_pktz_vp8_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_VP8_T *pPktzVp8 = (PKTZ_VP8_T *) pArg;
  int rc = 0;
  unsigned int idxFrame = 0;
  unsigned int idxPkt = 0;
  unsigned int szcopy;

  if(!pPktzVp8 ->isInitFromSdp) {
    init_vp8_param(pPktzVp8);
  }

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - vp8 rtp-packetizer got frame tm:%.3f, len:%u, rtp-ts:%u - data:"),
      PTSF(OUTFMT_PTS(pPktzVp8->pFrameData)), OUTFMT_LEN(pPktzVp8->pFrameData), 
         htonl(pPktzVp8->pRtpMulti->pRtp->timeStamp)); 
    if(pPktzVp8->pFrameData) {       
      LOGHEX_DEBUG(OUTFMT_DATA(pPktzVp8->pFrameData), MIN(OUTFMT_LEN(pPktzVp8->pFrameData), 16));
    }
  );

  //LOG(X_DEBUG("pktz_vp8_addframe[%d] %dHz len:%d pts:%.3f dts:%.3f key:%d fr-len:%d pkt:[%d/%d]"), progIdx, pPktzVp8->clockRateHz,OUTFMT_LEN(pPktzVp8->pFrameData), PTSF(OUTFMT_PTS(pPktzVp8->pFrameData)), PTSF(OUTFMT_DTS(pPktzVp8->pFrameData)), OUTFMT_KEYFRAME(pPktzVp8->pFrameData), OUTFMT_LEN(pPktzVp8->pFrameData), pPktzVp8->pRtpMulti->payloadLen,pPktzVp8->pRtpMulti->init.maxPayloadSz); //LOGHEX_DEBUG(OUTFMT_DATA(pPktzVp8->pFrameData), MIN(16, OUTFMT_LEN(pPktzVp8->pFrameData)));

  if(OUTFMT_LEN(pPktzVp8->pFrameData) == 0) {
    return 0;
  }

  if(OUTFMT_KEYFRAME(pPktzVp8->pFrameData)) {
    pPktzVp8->pXmitNode->cur_iskeyframe = 1;
  } else {
    pPktzVp8->pXmitNode->cur_iskeyframe = 0;
  }

  pPktzVp8->pRtpMulti->pRtp->pt &= ~RTP_PT_MARKER_MASK;
  pPktzVp8->pRtpMulti->payloadLen = 0;
  pPktzVp8->pRtpMulti->pRtp->timeStamp = htonl(pPktzVp8->rtpTs0 + 
                               (pPktzVp8->clockRateHz * PTSF( OUTFMT_PTS(pPktzVp8->pFrameData))));

/*
  http://www.potaroo.net/ietf/all-ids/draft-ietf-payload-vp8-08.txt
         0 1 2 3 4 5 6 7
        +-+-+-+-+-+-+-+-+
        |X|R|N|S|PartID | (REQUIRED)
        +-+-+-+-+-+-+-+-+
   X:   |I|L|T|K| RSV   | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
   I:   |M| PictureID   | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
   L:   |   TL0PICIDX   | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
   T/K: |TID|Y| KEYIDX  | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
*/

  while(idxFrame < OUTFMT_LEN(pPktzVp8->pFrameData)) {

    idxPkt = 0;
    pPktzVp8->pRtpMulti->pPayload[idxPkt] =  VP8_PD_EXTENDED_CTRL;
    if(idxFrame == 0) {
      pPktzVp8->pRtpMulti->pPayload[idxPkt] |= VP8_PD_START_PARTITION;
    }
    idxPkt++;
    pPktzVp8->pRtpMulti->pPayload[idxPkt++] =  VP8_PD_X_START_PICID_PRESENT;
#if 1
    pPktzVp8->pRtpMulti->pPayload[idxPkt++] =  (pPktzVp8->pictureId & 0x7f);

    if(idxFrame == 0 && (++pPktzVp8->pictureId) > 0x7f) {
      pPktzVp8->pictureId = 0;
    }
#else
    // 2 byte picture id
    *((uint16_t *) &pPktzVp8->pRtpMulti->pPayload[idxPkt]) =  htons(pPktzVp8->pictureId) & 0x7fff;
    pPktzVp8->pRtpMulti->pPayload[idxPkt] |= VP8_PD_I_START_PICID_16BIT;

    if(idxFrame == 0 && (++pPktzVp8->pictureId) > 0x7fff) {
      pPktzVp8->pictureId = 0;
    }
    idxPkt+=2;

#endif 

    if(idxPkt > pPktzVp8->pRtpMulti->init.maxPayloadSz) {
      return -1;
    }

    if((szcopy = OUTFMT_LEN(pPktzVp8->pFrameData) - idxFrame) > pPktzVp8->pRtpMulti->init.maxPayloadSz - idxPkt) {
      szcopy = pPktzVp8->pRtpMulti->init.maxPayloadSz - idxPkt;
    }
    //LOG(X_DEBUG("rtp-xmit-vp8 %d/%d, idxPkt:%d, szcopy:%d 0x%x 0x%x 0x%x 0x%x"), idxFrame,  OUTFMT_LEN(pPktzVp8->pFrameData), idxPkt, szcopy, pPktzVp8->pRtpMulti->pPayload[0], pPktzVp8->pRtpMulti->pPayload[1], pPktzVp8->pRtpMulti->pPayload[2], pPktzVp8->pRtpMulti->pPayload[3]);

    memcpy(&pPktzVp8->pRtpMulti->pPayload[idxPkt], &OUTFMT_DATA(pPktzVp8->pFrameData)[idxFrame], szcopy);
    pPktzVp8->pRtpMulti->payloadLen = idxPkt + szcopy;
    idxFrame += szcopy; 
 
    //
    // Set RTP marker bit
    //
    if(idxFrame >= OUTFMT_LEN(pPktzVp8->pFrameData)) {
      pPktzVp8->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
    }

    if(sendpkt(pPktzVp8) < 0) {
      return -1;
    }

    //
    // Only enable the keyframe for the first packet of any keyframe.  Outbound filters such as RTSP
    // will check for keyframe presence before starting transmission.  We don't want them to pick up
    // a frame's packet mid-stream, after the initial keyframe packet.
    //
    pPktzVp8->pXmitNode->cur_iskeyframe = 0;

  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
