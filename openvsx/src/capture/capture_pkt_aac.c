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

int cbOnPkt_aac(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  const unsigned char *pData = pPkt ? pPkt->payload.pData : NULL;
  unsigned int len = pPkt ? PKTCAPLEN(pPkt->payload) : 0;
  unsigned char adtsHdr[8];
  const ESDS_DECODER_CFG_T *pDecoderCfg;
  unsigned int numAuFrames;
  unsigned int idx;
  unsigned int payloadLen;
  unsigned int fragmentLen;
  unsigned int payloadStartIdx;
  uint32_t ts;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;
  int rc = 0;


  //
  // TODO: pPkt ts, seq, ssrc, etc... are only set for RTP, if capture config is read
  // from SDP file - and goes through stream_net_av. (streaming capture eonly)
  // capture for recording and stream_net_pes uses deprecated way of queing raw 
  // network data for later depacketization, which calls this cb from the queue reader
  //

  if(pSp == NULL) {
    return -1;
  } else if(pData == NULL) {
    // Packet was lost
    LOG(X_WARNING("Lost AAC-hbr RTP packet"));
    pSp->spFlags &= ~CAPTURE_SP_FLAG_INFRAGMENT;
    return 0;
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - rtp-recv-aac len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
                   "flags:0x%x pQ:0x%x"),
       PKTCAPLEN(pPkt->payload), pPkt->u.rtp.seq, pPkt->u.rtp.ts, PTSF(pPkt->u.rtp.ts),
       pSp->pStream->ts0,
       (pSp->pStream->clockHz > 0 ? ((double)pSp->pStream->ts0/pSp->pStream->clockHz) : 0),
       (pSp->pStream->clockHz > 0 ? ((double)(pPkt->u.rtp.ts - pSp->pStream->ts0)/pSp->pStream->clockHz) : 0),
       (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)?1:0,
       pSp->pStream->hdr.key.ssrc, pSp->spFlags, pSp->pCapAction && pSp->pCapAction->pQueue ? 1 : 0);
    if(pPkt) {
      LOGHEX_DEBUG(pPkt->payload.pData, MIN(16, PKTCAPLEN(pPkt->payload)));
    }
  );

  if(len < 2) {
    LOG(X_ERROR("Invalid AAC RTP payload length: %u"), len);
    return -1;
  }

  memset(&cbData, 0, sizeof(cbData));

  if(pSp->pCapAction && (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
    queueFr = 1;
  }

  if(queueFr) {

    if(!pSp->pCapAction->tmpFrameBuf.buf || pSp->pCapAction->tmpFrameBuf.sz <= 0) {
      return -1;
    }

    cbData.pCbDataArg = &pSp->pCapAction->tmpFrameBuf;
    cbData.cbStoreData = capture_cbWriteBuf;
  } else if(pSp->stream1.fp != FILEOPS_INVALID_FP) {
    cbData.pCbDataArg = &pSp->stream1;
    cbData.cbStoreData = capture_cbWriteFile;
  } else {
    // Nothing to be done
    return 0;
  }

  if(pSp->frameHasError && pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->frameHasError = 0;  
  }

  //
  // RFC3640  RTP Payload Format for Transport of MPEG-4 Elementary Streams
  // for each audio frame read 2 byte header at start of file
  // aac-hbr = 13 byte length, 3 byte idx. 
  //
  numAuFrames = htons(*(uint16_t *)pData) / 8 / 2;
  payloadStartIdx = numAuFrames * 2 + 2;
  ts = pPkt->u.rtp.ts;

  //fprintf(stderr, "numAUFRAMES:%d\n", numAuFrames);
  //avc_dumpHex(stderr, pData, len, 1);

  if(payloadStartIdx > len) {
    LOG(X_ERROR("AU Headers-length: %u exceeds RTP payload length: %u"), numAuFrames, len);
    return -1;
  } else if(numAuFrames == 0) {
    LOG(X_WARNING("Skipping 0 AU header size length.  RTP payload length: %u"), len);
    return 0;
  } else if((pPkt->flags & PKT_FLAGS_DTMF2833)) {
    return 0;
  }

  if(queueFr && pSp->pCapAction->tmpFrameBuf.idx > 0 && 
     pPkt->u.rtp.ts != pSp->lastPktTs) {
     //fprintf(stderr, "queing aac %d ts:%u\n", pSp->pCapAction->tmpFrameBuf.idx, pPkt->u.rtp.ts);
    rc = capture_addCompleteFrameToQ(pSp, pSp->lastPktTs);
  }

  pDecoderCfg = &pSp->pStream->pFilter->u_seqhdrs.aud.u.aac.decoderCfg;

  for(idx = 0; idx < numAuFrames; idx++) {

    //todo: should 3 be indexlength / indexdeltalength
    payloadLen = fragmentLen = ((htons(*(uint16_t *)&pData[idx * 2 + 2]) >> 3) & 0x1fff);
    if(payloadLen == 0) {
      LOG(X_WARNING("Skipping AU frame[%u] empty size"), idx);
      continue;
    }

    if(!(pSp->spFlags & CAPTURE_SP_FLAG_INFRAGMENT)) {

      if(queueFr && pSp->pCapAction->tmpFrameBuf.idx > 0 &&
       (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)) {
        //fprintf(stderr, "queing aac (prior) %d ts:%u\n", pSp->pCapAction->tmpFrameBuf.idx, pSp->lastPktTs);
        rc = capture_addCompleteFrameToQ(pSp, pSp->lastPktTs);
      }

      if((rc = aac_encodeADTSHdr(adtsHdr, sizeof(adtsHdr), pDecoderCfg, payloadLen)) < 0) {
        pSp->frameHasError = 1;
        return -1;
      }
      if((rc = cbData.cbStoreData(cbData.pCbDataArg, adtsHdr, rc)) < 0) {
        pSp->frameHasError = 1;
        return -1;
      }      
   
      if(pSp->frameHasError) {
        pSp->frameHasError = 0;
      }

    }

    if(payloadStartIdx + payloadLen > len) {
      if(idx < numAuFrames - 1) {
        LOG(X_ERROR("non-last AU frame[%u] length %u exceeds RTP payload length: %u"), idx, payloadLen, len);
        pSp->frameHasError = 1;
        return -1;
      }
      pSp->spFlags |= CAPTURE_SP_FLAG_INFRAGMENT;
      fragmentLen = len - payloadStartIdx;

    } else {
      pSp->spFlags &= ~CAPTURE_SP_FLAG_INFRAGMENT;
    }

    if(len > 0 && 
      (rc = cbData.cbStoreData(cbData.pCbDataArg, &pData[payloadStartIdx], fragmentLen)) < 0) {
      pSp->frameHasError = 1;
      return -1;
    }
    payloadStartIdx += fragmentLen;

    if(queueFr && pSp->pCapAction->tmpFrameBuf.idx > 0 &&
       (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)) {
       //fprintf(stderr, "queing aac (marker) %d ts:%u\n", pSp->pCapAction->tmpFrameBuf.idx, ts);
      rc = capture_addCompleteFrameToQ(pSp, ts);
    }

    pSp->lastPktTs = ts;
    ts += pDecoderCfg->frameDeltaHz;
  } // end of for loop


  return rc;
}

int streamadd_aac(CAPTURE_CBDATA_SP_T *pSp) {

  CAPTURE_FILTER_T *pFilter = (CAPTURE_FILTER_T *) pSp->pStream->pFilter;
  ESDS_DECODER_CFG_T *pDecoderCfg = &pFilter->u_seqhdrs.aud.u.aac.decoderCfg;
  int i;

  if(pDecoderCfg->objTypeIdx == 0) {
    pDecoderCfg->objTypeIdx = 2; // AAC-LC default
    LOG(X_WARNING("Defaulting to AAC type %d AAC-LD"), pDecoderCfg->objTypeIdx);
  }

  if(pDecoderCfg->channelIdx == 0) {
    pDecoderCfg->channelIdx = 1; 
    LOG(X_WARNING("Defaulting to %u channel(s) for AAC stream"),
        pDecoderCfg->channelIdx); 
  } else if(pDecoderCfg->channelIdx > 8) {
    LOG(X_ERROR("Invalid number of channels specified for AAC stream: %u"), 
          pDecoderCfg->channelIdx);
    return -1;
  }

  if(pFilter->u_seqhdrs.aud.common.clockHz == 0 ||
    (i = esds_getFrequencyIdx(pFilter->u_seqhdrs.aud.common.clockHz)) < 0) {
    LOG(X_ERROR("Invalid aac clock frequency %uHz"), 
                              pFilter->u_seqhdrs.aud.common.clockHz);
    return -1;
  } 
  pDecoderCfg->freqIdx = i;
  pDecoderCfg->haveExtFreqIdx = 0;
  //pSp->pStream->clockHz = pFilter->u_seqhdrs.aud.common.clockHz;

  // Set up defaults
  if(pFilter->u_seqhdrs.aud.u.aac.sizelength == 0) {
    pFilter->u_seqhdrs.aud.u.aac.sizelength = 13;
  }
  if(pFilter->u_seqhdrs.aud.u.aac.indexlength == 0) {
    pFilter->u_seqhdrs.aud.u.aac.indexlength = 3;
  }
  if(pFilter->u_seqhdrs.aud.u.aac.indexdeltalength == 0) {
    pFilter->u_seqhdrs.aud.u.aac.indexdeltalength = 3;
  }

  LOG(X_DEBUG("Setup AAC RTP input (%s) %uHz %d channels"), 
    esds_getMp4aObjTypeStr(pDecoderCfg->objTypeIdx),
    esds_getFrequencyVal(ESDS_FREQ_IDX(*pDecoderCfg)), pDecoderCfg->channelIdx); 

  return 0;
}

#endif // VSX_HAVE_CAPTURE
