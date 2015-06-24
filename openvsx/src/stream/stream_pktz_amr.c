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

static int init_amr_param(PKTZ_AMR_T *pPktzAmr) {

  if(pPktzAmr->pSdp) {
    pPktzAmr->clockRateHz = pPktzAmr->pSdp->common.clockHz;
  }
  if(pPktzAmr->clockRateHz == 0) {
    pPktzAmr->clockRateHz = AMRNB_CLOCKHZ;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzAmr->pRtpMulti->init.clockRateHz == 0) {
    pPktzAmr->pRtpMulti->init.clockRateHz = pPktzAmr->clockRateHz;
  }


  pPktzAmr->bsPkt.buf = pPktzAmr->buf;
  pPktzAmr->bsPkt.sz = sizeof(pPktzAmr->buf);
  pPktzAmr->bsPkt.idx = 0;
  pPktzAmr->queuedFrames = 0;
  
  pPktzAmr->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_amr_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_AMR_T *pPktzAmr = (PKTZ_AMR_T *) pArg;
  PKTZ_INIT_PARAMS_AMR_T *pInitParams = (PKTZ_INIT_PARAMS_AMR_T *) pInitArgs;
  int rc = 0;

  if(!pPktzAmr || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzAmr->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzAmr->pXmitNode = pInitParams->common.pXmitNode;
  pPktzAmr->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzAmr->clockRateHz = pInitParams->common.clockRateHz;
  pPktzAmr->pFrameData = pInitParams->common.pFrameData;

  pPktzAmr->pSdp = pInitParams->pSdp;
  pPktzAmr->isInitFromSdp = 0;
  pPktzAmr->compoundSampleMs = pInitParams->compoundSampleMs;

  if(pPktzAmr->compoundSampleMs / (1000 * AMRNB_SAMPLE_DURATION_HZ / AMRNB_CLOCKHZ) * 32 > 
     pPktzAmr->pRtpMulti->init.maxPayloadSz) {
    LOG(X_ERROR("AMR RTP aggregate packetization duration of %d ms is too large for MTU %d"),
      pPktzAmr->compoundSampleMs, pPktzAmr->pRtpMulti->init.maxPayloadSz);
    return -1;
  }

  if(pPktzAmr->compoundSampleMs != AMR_COMPOUND_SAMPLE_MS) {
    LOG(X_DEBUG("Using AMR RTP aggregate packetization duration of %d ms"), pPktzAmr->compoundSampleMs);
  }

  if(rc < 0) {
     stream_pktz_amr_close(pPktzAmr); 
  }

  return rc;
}

int stream_pktz_amr_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_AMR_T *pPktzAmr = (PKTZ_AMR_T *) pArg;

  //pPktzAmr->pRtpMulti->pktsSent = 0;
  pPktzAmr->pRtpMulti->payloadLen = 0;
  pPktzAmr->bsPkt.idx = 0;
  pPktzAmr->queuedFrames = 0;

  return 0;
}

int stream_pktz_amr_close(void *pArg) {
  PKTZ_AMR_T *pPktzAmr = (PKTZ_AMR_T *) pArg;

  if(pPktzAmr == NULL) {
    return -1;
  }

  return 0;
}

static int writeHeaders(const unsigned char *pSrc, unsigned int lenSrc,
                       unsigned char *pDst, unsigned int lenDst) {
  unsigned int idxSrc = 0;
  unsigned int idxDst = 0;
  unsigned int numFrames = 0;
  uint8_t frmtype;
  uint8_t frmsize;

  if(lenDst <= 1) {
    return -1;
  }

  //
  // RFC3267 AMR RTP Packetization using octet aligned mode
  // [ 4 bits CMR ] 0xf - no codec mode request present
  // [ 4 bits reserved 0 ]
  // n frame headers [ 1 bit F, 4 bits FT, 1 bit Q, 2 bits 0 ]
  // F = 1, subsequent frame header will follow
  // FT = frame type used for size lookup table
  // Q = quality indicator, 1 = damaged
  // n frames minus each header

  // CMR=15 (4 bits), no mode request present
  pDst[idxDst++] = 0xf0;

  while(idxSrc < lenSrc) {

    frmtype = AMR_GETFRAMETYPE(pSrc[idxSrc]);
    frmsize = AMRNB_GETFRAMEBODYSIZE_FROMTYPE(frmtype);

    //fprintf(stderr, "FRMTYPE:%d, FRMSIZE:%d\n", frmtype, frmsize);

    if(idxDst + 1 + frmsize + idxSrc - numFrames >= lenDst) {
       LOG(X_ERROR("Unable to write full amr header [%d]/%d"), numFrames, idxDst);
       break;
    }
    if(numFrames++ > 0) {
      pDst[idxDst - 1] |= 0x80;           // F = 1, marks non-last frame
    }
    pDst[idxDst++] = ((frmtype << 3) & 0x78) | 0x04;  // Frame Type (4 bits), Q=1 (1bit)
    idxSrc += frmsize + 1;
  }

  return (int) idxDst;
}

static int storeFrames(const unsigned char *pSrc,  unsigned char *pDst,
                       unsigned int numFrames) {
  unsigned int idxSrc = 0;
  unsigned int idxDst = 0;
  unsigned int idxFrame = 0;
  uint8_t frmsize;

  for(idxFrame = 0; idxFrame < numFrames; idxFrame++) {
    frmsize = AMRNB_GETFRAMEBODYSIZE(pSrc[idxSrc]);
    memcpy(&pDst[idxDst], &pSrc[idxSrc + 1], frmsize);
    idxSrc += frmsize + 1;
    idxDst += frmsize;
  }

  return (int) idxDst;
}

static int createPkt(const unsigned char *pSrc, unsigned int lenSrc,
                       unsigned char *pDst, unsigned int lenDst) {
  int idxhdrs;
  int idxbody;
  if((idxhdrs = writeHeaders(pSrc, lenSrc, pDst, lenDst)) <= 0) {
    return idxhdrs;
  }

  if((idxbody = storeFrames(pSrc,  &pDst[idxhdrs], idxhdrs - 1)) < 0) {
    return idxbody;
  }

  return idxhdrs + idxbody;
}

//static int delmetsoffset = 0;
//static int delme;

static int sendpkt(PKTZ_AMR_T *pPktzAmr) {
  int rc = 0;
  int payloadLen;

  if((payloadLen = createPkt(pPktzAmr->bsPkt.buf, pPktzAmr->bsPkt.idx,
                             pPktzAmr->pRtpMulti->pPayload, 
                             pPktzAmr->pRtpMulti->init.maxPayloadSz)) < 0) {
    return -1;
  }

  pPktzAmr->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
  pPktzAmr->pRtpMulti->pRtp->timeStamp =
    htonl(pPktzAmr->clockRateHz * ((double) pPktzAmr->pts / 90000.0f));

//if(delme++==8) delmetsoffset =-5000;
//pPktzAmr->pRtpMulti->pRtp->timeStamp=htonl(ntohl( pPktzAmr->pRtpMulti->pRtp->timeStamp)+delmetsoffset);
//fprintf(stderr, " amr ts offset %d\n", delmetsoffset);

  pPktzAmr->pRtpMulti->payloadLen = payloadLen;

  rc = stream_rtp_preparepkt(pPktzAmr->pRtpMulti);

#if 0
  fprintf(stderr, "sendpkt rtp amr ts:%u seq:%d len:%d marker:%d\n", htonl(pPktzAmr->pRtpMulti->pRtp->timeStamp), htons(pPktzAmr->pRtpMulti->pRtp->sequence_num), pPktzAmr->pRtpMulti->payloadLen,(pPktzAmr->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  avc_dumpHex(stderr, pPktzAmr->pRtpMulti->pPayload, MIN(340, pPktzAmr->pRtpMulti->payloadLen), 1);
#endif 

  if(rc >= 0) {
    rc = pPktzAmr->cbXmitPkt(pPktzAmr->pXmitNode);
  }

  //pPktzAmr->pRtpMulti->payloadLen = 0;

  return rc;
}

int stream_pktz_amr_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_AMR_T *pPktzAmr = (PKTZ_AMR_T *) pArg;
  int rc = 0;

  if(!pPktzAmr->isInitFromSdp) {
    init_amr_param(pPktzAmr);
    pPktzAmr->pts = OUTFMT_PTS(pPktzAmr->pFrameData);
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - AMR rtp-packetizer got frame tm:%.3f, len:%u (max:%d), rtp-ts:%u %uHz - data:"),
           PTSF(OUTFMT_PTS(pPktzAmr->pFrameData)), OUTFMT_LEN(pPktzAmr->pFrameData),
           pPktzAmr->pRtpMulti->init.maxPayloadSz,
           htonl(pPktzAmr->pRtpMulti->pRtp->timeStamp), pPktzAmr->clockRateHz);
    if(pPktzAmr->pFrameData) {
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzAmr->pFrameData), MIN(OUTFMT_LEN(pPktzAmr->pFrameData), 16));
    }
  );

#if 0
  fprintf(stderr, "pktz_amr_addframe[%d] %dHz len:%d pts:%.3f dts:%.3f key:%d fr:[%d] pkt:[%d/%d]\n", progIdx, pPktzAmr->clockRateHz,OUTFMT_LEN(pPktzAmr->pFrameData), PTSF(OUTFMT_PTS(pPktzAmr->pFrameData)), PTSF(OUTFMT_DTS(pPktzAmr->pFrameData)), OUTFMT_KEYFRAME(pPktzAmr->pFrameData), OUTFMT_LEN(pPktzAmr->pFrameData), pPktzAmr->pRtpMulti->payloadLen,pPktzAmr->pRtpMulti->init.maxPayloadSz);
  avc_dumpHex(stderr, OUTFMT_DATA(pPktzAmr->pFrameData), OUTFMT_LEN(pPktzAmr->pFrameData), 1);
#endif // 0

  //
  // Check if we cannot queue (anymore) frames
  //
  if(OUTFMT_LEN(pPktzAmr->pFrameData) > pPktzAmr->bsPkt.sz - pPktzAmr->bsPkt.idx) {
    if(pPktzAmr->bsPkt.idx == 0) {
      return -1;
    }  else if((rc = sendpkt(pPktzAmr)) < 0) {
      return rc;
    }
    pPktzAmr->queuedFrames = 0;
    pPktzAmr->bsPkt.idx = 0;
  }

  if(pPktzAmr->bsPkt.idx == 0 || OUTFMT_PTS(pPktzAmr->pFrameData) < pPktzAmr->pts) {
    pPktzAmr->pts = OUTFMT_PTS(pPktzAmr->pFrameData); 
  }

  //
  // queue one frame
  //
  memcpy(&pPktzAmr->bsPkt.buf[pPktzAmr->bsPkt.idx],
         OUTFMT_DATA(pPktzAmr->pFrameData), OUTFMT_LEN(pPktzAmr->pFrameData));
  pPktzAmr->bsPkt.idx += OUTFMT_LEN(pPktzAmr->pFrameData);
  pPktzAmr->queuedFrames++;
  if((double)(OUTFMT_PTS(pPktzAmr->pFrameData) - pPktzAmr->pts) /90.0f +
    (1000.0f/(pPktzAmr->clockRateHz / AMRNB_SAMPLE_DURATION_HZ)) >= pPktzAmr->compoundSampleMs) {
    if((rc = sendpkt(pPktzAmr)) < 0) {
      return rc;
    }
    pPktzAmr->queuedFrames = 0;
    pPktzAmr->bsPkt.idx = 0;
  }

  return rc;
}

#endif // VSX_HAVE_STREAMER
