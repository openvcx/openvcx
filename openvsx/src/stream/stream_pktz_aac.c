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

static int init_aac_param(PKTZ_AAC_T *pPktzAac) {

  //
  // Init parameters which may not be set (from SDP) during streaming setup
  // and only directly after the first packet is to be packetized
  //

  if(pPktzAac->pSdp && pPktzAac->pSdp->u.aac.decoderCfg.frameDeltaHz != 0) {
    pPktzAac->frameDeltaHz = pPktzAac->pSdp->u.aac.decoderCfg.frameDeltaHz;
  } else {
    // fallback to default value (either 1024, or 960)
    pPktzAac->frameDeltaHz = 1024;
    LOG(X_WARNING("Defaulting to aac frame delta %uHz"), pPktzAac->frameDeltaHz);
  }
  if(pPktzAac->clockRateHz == 0 &&(pPktzAac->clockRateHz =
             esds_getFrequencyVal(ESDS_FREQ_IDX(pPktzAac->pSdp->u.aac.decoderCfg))) == 0) {
    pPktzAac->clockRateHz = 48000;
    LOG(X_WARNING("Defaulting to aac clock %uHz"), pPktzAac->clockRateHz);
  }
  // Set the clock rate - which may not have been set if using live capture
  if(pPktzAac->pRtpMulti->init.clockRateHz == 0) {
    pPktzAac->pRtpMulti->init.clockRateHz = pPktzAac->clockRateHz;
  }

  //if(pPktzAac->favoffsetrtp > 0) {
  //  pPktzAac->tsOffsetHz = pPktzAac->favoffsetrtp * pPktzAac->clockRateHz;
  //}


  if(pPktzAac->pSdp && pPktzAac->pSdp->u.aac.indexlength != 0) {
    pPktzAac->hbrIndexLen = pPktzAac->pSdp->u.aac.indexlength;
  } else {
    // AAC-hbr indexlength=3 (bits) 
    pPktzAac->hbrIndexLen = 3;
  }

  pPktzAac->rtpTs0 = htonl(pPktzAac->pRtpMulti->pRtp->timeStamp);

  pPktzAac->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_aac_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_AAC_T *pPktzAac = (PKTZ_AAC_T *) pArg;
  PKTZ_INIT_PARAMS_AAC_T *pInitParams = (PKTZ_INIT_PARAMS_AAC_T *) pInitArgs;
  int rc = 0;

  if(!pPktzAac || !pInitParams || !pInitParams->common.pRtpMulti || 
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzAac->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzAac->pXmitNode = pInitParams->common.pXmitNode;
  pPktzAac->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzAac->clockRateHz = pInitParams->common.clockRateHz;
  pPktzAac->pFrameData = pInitParams->common.pFrameData;
  //pPktzAac->favoffsetrtp = pInitParams->common.favoffsetrtp;

  pPktzAac->pSdp = pInitParams->pSdp;
  pPktzAac->isInitFromSdp = 0;
  pPktzAac->stripAdtsHdr = -1;

  if(rc < 0) {
     stream_pktz_aac_close(pPktzAac); 
  }

  return rc;
}

int stream_pktz_aac_reset(void *pArg, int fullReset, unsigned int progIdx) {
  //PKTZ_AAC_T *pPktzAac = (PKTZ_AAC_T *) pArg;

  //pPktzAac->pRtpMulti->pktsSent = 0;

  return 0;
}

int stream_pktz_aac_close(void *pArg) {
  PKTZ_AAC_T *pPktzAac = (PKTZ_AAC_T *) pArg;

  if(pPktzAac == NULL) {
    return -1;
  }

  return 0;
}

static int sendpkt(PKTZ_AAC_T *pPktzAac) {
  int rc = 0;


  rc = stream_rtp_preparepkt(pPktzAac->pRtpMulti);

  //LOG(X_DEBUG("AAC rtp-packetizer sendpkt ts:%u seq:%d len:%d ssrc:0x%x"), htonl(pPktzAac->pRtpMulti->pRtp->timeStamp), htons(pPktzAac->pRtpMulti->pRtp->sequence_num), pPktzAac->pRtpMulti->payloadLen, pPktzAac->pRtpMulti->pRtp->ssrc);
  if(rc >= 0) {
    rc = pPktzAac->cbXmitPkt(pPktzAac->pXmitNode);
  }

  pPktzAac->pRtpMulti->payloadLen = 0;

  return rc;
}

int stream_pktz_aac_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_AAC_T *pPktzAac = (PKTZ_AAC_T *) pArg;
  int rc = 0;
  unsigned int idx = 0;
  unsigned int payloadLen;
  OUTFMT_FRAME_DATA_T frameData;
  const OUTFMT_FRAME_DATA_T *pFrameData = pPktzAac->pFrameData;

  if(!pPktzAac->isInitFromSdp) {
    init_aac_param(pPktzAac);
  }

  //
  // Check if the input begins with an ADTS header which needs to be stripped
  // 
  if(pPktzAac->stripAdtsHdr == -1 && OUTFMT_LEN(pPktzAac->pFrameData) >= 7) {
    if(OUTFMT_DATA(pPktzAac->pFrameData)[0] == 0xff && 
       (OUTFMT_DATA(pPktzAac->pFrameData)[1] & 0xf6) == 0xf0) { 
      pPktzAac->stripAdtsHdr = 1; 
    } else {
      pPktzAac->stripAdtsHdr = 0; 
    }
  }

  if(pPktzAac->stripAdtsHdr > 0 &&
     OUTFMT_LEN(pPktzAac->pFrameData) >= 7 && OUTFMT_DATA(pPktzAac->pFrameData)) {
    memcpy(&frameData, pPktzAac->pFrameData, sizeof(frameData));
    OUTFMT_DATA(&frameData) += 7;
    OUTFMT_LEN(&frameData) -= 7;
    pFrameData = &frameData;
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - AAC rtp-packetizer got frame tm:%.3f, len:%u (max:%d), rtp-ts:%u (%uHz / %uHz) - data:"),
           PTSF(OUTFMT_PTS(pPktzAac->pFrameData)), OUTFMT_LEN(pPktzAac->pFrameData),
           pPktzAac->pRtpMulti->init.maxPayloadSz,
           htonl(pPktzAac->pRtpMulti->pRtp->timeStamp), pPktzAac->clockRateHz, pPktzAac->frameDeltaHz);
    if(pPktzAac->pFrameData) {
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzAac->pFrameData), MIN(OUTFMT_LEN(pPktzAac->pFrameData), 16));
    }
  );

  //fprintf(stderr, "pktz_aac_addframe[%d] %d/%d len:%d pts:%.3f dts:%.3f key:%d pkt:[%d/%d]\n", progIdx, pPktzAac->clockRateHz, pPktzAac->frameDeltaHz, OUTFMT_LEN(pFrameData), PTSF(OUTFMT_PTS(pFrameData)), PTSF(OUTFMT_PTS(pFrameData)), OUTFMT_KEYFRAME(pFrameData), pPktzAac->pRtpMulti->payloadLen,pPktzAac->pRtpMulti->init.maxPayloadSz);
  //avc_dumpHex(stderr, pFrameData->pData, 16, 1);


  //
  // RFC3640  RTP Payload Format for Transport of MPEG-4 Elementary Streams
  // aac-hbr default format 13 byte length, 3 byte idx.
  //
  //TODO: using constant frame rate output here, not adhering to pts
  //fprintf(stderr, "AAC TS OFFSET %d\n", pPktzAac->tsOffsetHz);
  //pPktzAac->pRtpMulti->m_pRtp->timeStamp = 
  //    htonl(ntohl(pPktzAac->pRtpMulti->m_pRtp->timeStamp) + pPktzAac->frameDeltaHz);
  pPktzAac->pRtpMulti->pRtp->timeStamp =
    htonl(pPktzAac->rtpTs0 + (pPktzAac->clockRateHz * ((double) OUTFMT_PTS(pFrameData) / 90000.0f)));

  //fprintf(stderr, "send aac timing: pts:%llu (%.3f) %dHz %u (rtp ts:%u)\n", pFrameData->pts, (double)pFrameData->pts/90000.0f, pPktzAac->clockRateHz, (unsigned int) (pPktzAac->clockRateHz * ((double) pFrameData->pts / 90000.0f)), htonl(pPktzAac->pRtpMulti->pRtp->timeStamp));

  pPktzAac->pRtpMulti->pRtp->pt &= ~RTP_PT_MARKER_MASK;

  while(idx < OUTFMT_LEN(pFrameData)) {

    if(idx == 0) {

      // (8 * 2), 1 AU frame
      *((uint16_t *) &pPktzAac->pRtpMulti->pPayload[0]) = htons(16);  


      if(OUTFMT_LEN(pFrameData) > (uint32_t) 
         pPktzAac->pRtpMulti->init.maxPayloadSz - 4) {
        // fragment start
        payloadLen = pPktzAac->pRtpMulti->init.maxPayloadSz - 4;
        //pPktzAac->pRtpMulti->nextFrame = pPktzAac->pRtpMulti->framesSent;
      } else {
        // entire frame into pkt
        payloadLen = OUTFMT_LEN(pFrameData);
        //pPktzAac->pRtpMulti->nextFrame = pPktzAac->pRtpMulti->framesSent + 1;
      }

      *((uint16_t *) &pPktzAac->pRtpMulti->pPayload[2]) = 
               htons(OUTFMT_LEN(pFrameData) << pPktzAac->hbrIndexLen);


    } else {

      //
      // fragment continuation
      //

      if(OUTFMT_LEN(pFrameData) - idx > (unsigned int) 
                         pPktzAac->pRtpMulti->init.maxPayloadSz - 4) {

        *((uint16_t *) &pPktzAac->pRtpMulti->pPayload[2]) = 
             htons((OUTFMT_LEN(pFrameData) - idx) << pPktzAac->hbrIndexLen);
        payloadLen = pPktzAac->pRtpMulti->init.maxPayloadSz - 4;

      } else {

        // fragment continuation ending - set marker bit
        pPktzAac->pRtpMulti->pRtp->pt |= 0x80;

        payloadLen = OUTFMT_LEN(pFrameData) - idx;
        *((uint16_t *) &pPktzAac->pRtpMulti->pPayload[2]) = 
                              htons(payloadLen << pPktzAac->hbrIndexLen);
        //pPktzAac->pRtpMulti->nextFrame = pPktzAac->pRtpMulti->framesSent + 1;
      }
 
    }

    pPktzAac->pRtpMulti->payloadLen = 4 + payloadLen;

    memcpy(&pPktzAac->pRtpMulti->pPayload[4], &OUTFMT_DATA(pFrameData)[idx], payloadLen);

    idx += payloadLen;

    if(idx >= OUTFMT_LEN(pFrameData)) {
      pPktzAac->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
    } else {
      pPktzAac->pRtpMulti->pRtp->pt &= RTP_PT_MARKER_MASK;
    }
    
    if(sendpkt(pPktzAac) < 0) {
      return -1;
    }

  } // end of while

  //fprintf(stderr, "stream_pktz_aac rc:%d\n", rc);

  return rc;
}

#endif // VSX_HAVE_STREAMER
