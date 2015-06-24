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

static int init_h263_param(PKTZ_H263_T *pPktzH263) {

  if(pPktzH263->pSdp) {
    pPktzH263->clockRateHz = pPktzH263->pSdp->common.clockHz;
  }
  if(pPktzH263->clockRateHz == 0) {
    pPktzH263->clockRateHz = MPEG4_DEFAULT_CLOCKHZ;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzH263->pRtpMulti->init.clockRateHz == 0) {
    pPktzH263->pRtpMulti->init.clockRateHz = pPktzH263->clockRateHz;
  }
  
  pPktzH263->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_h263_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_H263_T *pPktzH263 = (PKTZ_H263_T *) pArg;
  PKTZ_INIT_PARAMS_H263_T *pInitParams = (PKTZ_INIT_PARAMS_H263_T *) pInitArgs;
  int rc = 0;

  if(!pPktzH263 || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzH263->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzH263->pXmitNode = pInitParams->common.pXmitNode;
  pPktzH263->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzH263->clockRateHz = pInitParams->common.clockRateHz;
  pPktzH263->pFrameData = pInitParams->common.pFrameData;

  pPktzH263->pSdp = pInitParams->pSdp;
  pPktzH263->isInitFromSdp = 0;
  pPktzH263->codecType = pInitParams->codecType;

  if(rc < 0) {
     stream_pktz_h263_close(pPktzH263); 
  }

  return rc;
}

int stream_pktz_h263_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_H263_T *pPktzH263 = (PKTZ_H263_T *) pArg;

  //pPktzH263->pRtpMulti->pktsSent = 0;
  pPktzH263->pRtpMulti->payloadLen = 0;

  return 0;
}

int stream_pktz_h263_close(void *pArg) {
  PKTZ_H263_T *pPktzH263 = (PKTZ_H263_T *) pArg;

  if(pPktzH263 == NULL) {
    return -1;
  }

  return 0;
}

static int sendpkt(PKTZ_H263_T *pPktzH263) {
  int rc = 0;

  rc = stream_rtp_preparepkt(pPktzH263->pRtpMulti);

  //fprintf(stderr, "sendpkt rtp h263 ts:%u seq:%d len:%d marker:%d\n", htonl(pPktzH263->pRtpMulti->pRtp->timeStamp), htons(pPktzH263->pRtpMulti->pRtp->sequence_num), pPktzH263->pRtpMulti->payloadLen, (pPktzH263->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  //avc_dumpHex(stderr, pPktzH263->pRtpMulti->pPayload, pPktzH263->pRtpMulti->payloadLen > 80 ? 80 : pPktzH263->pRtpMulti->payloadLen, 1);

  if(rc >= 0 && (rc = pPktzH263->cbXmitPkt(pPktzH263->pXmitNode)) < 0) {
    LOG(X_ERROR("H263 packetizer failed to send output packet length %d"), stream_rtp_datalen(pPktzH263->pXmitNode->pRtpMulti));
  }

  pPktzH263->pRtpMulti->payloadLen = 0;

  return rc;
}

static int find_resync(const unsigned char *pData, unsigned int len) {
  const unsigned char *end = pData + len;
  const unsigned char *p;

  for(p = end -1; p > pData + 1; p-= 2) {
    if(*p == 0x00) {
      if(p[1] == 0x00 && p[2] != 0x00)
        return (p - pData);
      if(p[-1] == 0x00 && p[1] != 0x00)
        return (p - pData - 1);
    }
  }

  return len;
}

int stream_pktz_h263_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_H263_T *pPktzH263 = (PKTZ_H263_T *) pArg;
  int rc = 0;
  const unsigned char *pData;
  unsigned int idxFrame = 0;
  unsigned int idxPkt;
  unsigned int szcopy;
  
  if(!pPktzH263->isInitFromSdp) {
    init_h263_param(pPktzH263);
  }

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - h263 rtp-packetizer got frame tm:%.3f, len:%u, rtp-ts:%u - data:"),
      PTSF(OUTFMT_PTS(pPktzH263->pFrameData)), OUTFMT_LEN(pPktzH263->pFrameData), 
         htonl(pPktzH263->pRtpMulti->pRtp->timeStamp)); 
     if(pPktzH263->pFrameData) {       
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzH263->pFrameData), MIN(OUTFMT_LEN(pPktzH263->pFrameData), 16));
    }
  );

#if 0
  BIT_STREAM_T bs;
  H263_DESCR_T h263;
  memset(&h263, 0, sizeof(h263));
  memset(&bs, 0, sizeof(bs));
  bs.buf = (unsigned char *) OUTFMT_DATA(pPktzH263->pFrameData);
  bs.sz = OUTFMT_LEN(pPktzH263->pFrameData);
  if(h263_decodeHeader(&bs, &h263, 0) >= 0) {
      LOG(X_DEBUG("output - H.263%s %dx%d %s picnum:%d%s%s%s"), h263.plus.h263plus ? "+" : "", h263.param.frameWidthPx, h263.param.frameHeightPx,  h263.frame.frameType == H263_FRAME_TYPE_I ? "I" : h263.frame.frameType == H263_FRAME_TYPE_B ? "B" : "P", h263.frame.picNum, h263.plus.advIntraCoding ? ",I=1" : "", h263.plus.deblocking ? ",J=1" : "", h263.plus.sliceStructured ? ",K=1" : "");
  } else { LOG(X_ERROR("Failed to decode header")); }

#endif 

  //fprintf(stderr, "pktz_h263_addframe[%d] %dHz len:%d pts:%.3f dts:%.3f key:%d fr:[%d] pkt:[%d/%d]\n", progIdx, pPktzH263->clockRateHz,OUTFMT_LEN(pPktzH263->pFrameData), PTSF(OUTFMT_PTS(pPktzH263->pFrameData)), PTSF(OUTFMT_DTS(pPktzH263->pFrameData)), OUTFMT_KEYFRAME(pPktzH263->pFrameData), OUTFMT_LEN(pPktzH263->pFrameData), pPktzH263->pRtpMulti->payloadLen,pPktzH263->pRtpMulti->init.maxPayloadSz);
//avc_dumpHex(stderr, OUTFMT_DATA(pPktzH263->pFrameData), MIN(16, OUTFMT_LEN(pPktzH263->pFrameData)), 1);


  if(OUTFMT_KEYFRAME(pPktzH263->pFrameData)) {
    pPktzH263->pXmitNode->cur_iskeyframe = 1;
  } else {
    pPktzH263->pXmitNode->cur_iskeyframe = 0;
  }

  //testmpg4(pPktzH263);

  pPktzH263->pRtpMulti->pRtp->pt &= ~RTP_PT_MARKER_MASK;
  pPktzH263->pRtpMulti->payloadLen = 0;
  pPktzH263->pRtpMulti->pRtp->timeStamp =
    htonl(pPktzH263->clockRateHz * PTSF( OUTFMT_PTS(pPktzH263->pFrameData)));

  while(idxFrame < OUTFMT_LEN(pPktzH263->pFrameData)) {
  
    idxPkt = 0;
    pData = &OUTFMT_DATA(pPktzH263->pFrameData)[idxFrame];

    if(pPktzH263->codecType == XC_CODEC_TYPE_H263_PLUS) {

      //
      //     http://tools.ietf.org/html/rfc4629
      //
      //     0                   1
      //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      //    |   RR    |P|V|   PLEN    |PEBIT|
      //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      //

      if(OUTFMT_LEN(pPktzH263->pFrameData) >= 2 && pData[0] == 0x00 && pData[1] == 0x00) {
        pPktzH263->pRtpMulti->pPayload[idxPkt++] = 0x04;
        idxFrame += 2;
      } else {
        pPktzH263->pRtpMulti->pPayload[idxPkt++] = 0x00;
      }
      pPktzH263->pRtpMulti->pPayload[idxPkt++] = 0x00;

    } else if(pPktzH263->codecType == XC_CODEC_TYPE_H263) {

      //
      // http://tools.ietf.org/html/rfc2190
      // 0                   1                   2                   3
      // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // |F|P|SBIT |EBIT | SRC |I|U|S|A|R      |DBQ| TRB |    TR         |
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      //

      //
      // 3 byte picture resolution array index
      //
      int fmt = (OUTFMT_DATA(pPktzH263->pFrameData)[4] & 0x1c) >> 2;

      pPktzH263->pRtpMulti->pPayload[idxPkt++] = 0x00; // F=0,P=0 (Mode=0) SBIT=0,EBIT=0
      pPktzH263->pRtpMulti->pPayload[idxPkt++] = 
        (fmt << 5) | (OUTFMT_KEYFRAME(pPktzH263->pFrameData) ? 0x00 : 0x10);
      pPktzH263->pRtpMulti->pPayload[idxPkt++] = 0x00;
      pPktzH263->pRtpMulti->pPayload[idxPkt++] = 0x00;

#if 0
    pData = &pPktzH263->pRtpMulti->pPayload[idxPkt-4];
    int startbit = (pData[0] & 0x38) >> 3;
    int endbit = pData[0] & 0x07;
    int src = (pData[1] & 0xe0) >> 5;
    int ibit= pData[1] & 0x10;
    int ubit = pData[1] & 0x08;
    int sbit = pData[1] & 0x04;
    int abit = pData[1] & 0x02;
    int r = ((pData[1] & 0x01) << 3) | ((pData[2] & 0xe0) >> 5);
    int dbq = ((pData[2] & 0x14) >>  3);
    int trb = (pData[2] & 0x07);
    int tr = pData[3];

    LOG(X_DEBUG("H.263 output start:%d, end:%d, src:%d, i:%d, u:%d, s:%d, a:%d, r:%d, dbq:%d, trb:%d, tr:%d"), startbit, endbit, src, ibit, ubit, sbit, abit, r, dbq, trb, tr);
#endif // 0

    }
    
    if((szcopy = OUTFMT_LEN(pPktzH263->pFrameData) - idxFrame) > pPktzH263->pRtpMulti->init.maxPayloadSz - idxPkt) {
      szcopy = pPktzH263->pRtpMulti->init.maxPayloadSz - idxPkt;

      //if(pPktzH263->codecType == XC_CODEC_TYPE_H263_PLUS) {
      if(1) {
        int szcopy_resync = find_resync(&OUTFMT_DATA(pPktzH263->pFrameData)[idxFrame], szcopy);
        //LOG(X_DEBUG("h263 pktzr frame:%d/%d, pkt:%d/%d, szcopy:%d,%d"), idxFrame, OUTFMT_LEN(pPktzH263->pFrameData), idxPkt, pPktzH263->pRtpMulti->payloadLen, szcopy, szcopy_resync); 
        szcopy = szcopy_resync;
      }

    }

    memcpy(&pPktzH263->pRtpMulti->pPayload[idxPkt], &OUTFMT_DATA(pPktzH263->pFrameData)[idxFrame], szcopy);
    pPktzH263->pRtpMulti->payloadLen = idxPkt + szcopy;
    idxFrame += szcopy;
 
    //
    // Set RTP marker bit
    //
    if(idxFrame >= OUTFMT_LEN(pPktzH263->pFrameData)) {
      pPktzH263->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
    }

    if(sendpkt(pPktzH263) < 0) {
      return -1;
    }

    //
    // Only enable the keyframe for the first packet of any keyframe.  Outbound filters such as RTSP
    // will check for keyframe presence before starting transmission.  We don't want them to pick up
    // a frame's packet mid-stream, after the initial keyframe packet.
    //
    pPktzH263->pXmitNode->cur_iskeyframe = 0;

  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
