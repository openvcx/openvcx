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

static int opus_add_empty_frames(CAPTURE_CBDATA_SP_T *pSp, 
                                 CAPTURE_STORE_CBDATA_T *pCbData,
                                int queueFr,
                                const COLLECT_STREAM_PKTDATA_T *pPkt,
                                uint32_t maxTsDeltaHz) {
  int rc = 0;
  unsigned int tsoffset = 0;
  unsigned int idxFrame;
  unsigned int numFrames;
  unsigned char buf[64];
  uint32_t tsDeltaHz = pPkt->u.rtp.ts - pSp->lastPktTs;

  //fprintf(stderr, "amr_add_empty_frames tsdelta:%d\n", tsdelta);

  if(pSp->pStream->clockHz <= 0) {
    return -1;
  } if((numFrames = (tsDeltaHz * (1000 / OPUS_FRAME_DURATION_MS) / pSp->pStream->clockHz)) <= 0) {
    return rc;
  }

  if(tsDeltaHz > maxTsDeltaHz) {
    LOG(X_WARNING("Audio ts gap %uHz (%uHz -> %uHz) greater than max %uHz to fill silence for %s, ssrc: 0x%x, ts: %u, seq:%u"),
      tsDeltaHz, pSp->lastPktTs, pPkt->u.rtp.ts, maxTsDeltaHz, pSp->pStream->strSrcDst, pSp->pStream->hdr.key.ssrc,
      pPkt->u.rtp.ts, pPkt->u.rtp.seq);
    return 0;
  }

  LOG(X_WARNING("Adding %d silent OPUS frames to account for timestamp gap of %dHz (max %dHz) for %s, ssrc: 0x%x, ts: %u, seq:%u"),
        numFrames, tsDeltaHz,  maxTsDeltaHz, pSp->pStream->strSrcDst, pSp->pStream->hdr.key.ssrc,
        pPkt->u.rtp.ts, pPkt->u.rtp.seq);

  for(idxFrame = 0; idxFrame < numFrames; idxFrame++) {

    //
    // This actually creates an invalid 1 byte packet that should be checked by the OPUS decoder
    //
    buf[0] = OPUS_LOST_FRAME_DATA;
    pCbData->cbStoreData(pCbData->pCbDataArg, buf, OPUS_LOST_FRAME_LEN);

    if(queueFr) {
      if((rc = capture_addCompleteFrameToQ(pSp, pSp->lastPktTs + tsoffset)) < 0) {
        rc = -1;
        break;
      }
    }

    tsoffset += AMRNB_SAMPLE_DURATION_HZ;
  }

  pSp->lastPktTs += tsoffset;

  return rc;
}

static int capture_opus_parse(CAPTURE_CBDATA_SP_T *pSp,
                             CAPTURE_STORE_CBDATA_T *pCbData,
                             int queueFr,
                             const unsigned char *pPkt,
                             unsigned int lenPkt) {
  int rc = 0;
  CAPTURE_FILTER_T *pFilter = (CAPTURE_FILTER_T *) pSp->pStream->pFilter;

  pCbData->cbStoreData(pCbData->pCbDataArg, pPkt, lenPkt);

  if(queueFr) {
    if((rc = capture_addCompleteFrameToQ(pSp, pSp->lastPktTs)) < 0) {
      rc = -1;
    }
  }

  pSp->lastPktTs += (pFilter->u_seqhdrs.aud.common.clockHz / 50);

  if(rc == 0) {
    return lenPkt;
  } else {
    return rc;
  }
}

int cbOnPkt_opus(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  const unsigned char *pData = pPkt ? pPkt->payload.pData : NULL;
  unsigned int len = pPkt ? PKTCAPLEN(pPkt->payload) : 0;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;
  int rc = 0;

  if(pSp == NULL) {
    return -1;
  } else if(!pPkt || !pPkt->payload.pData) {
    // Packet was lost
    LOG(X_WARNING("RTP OPUS lost packet, last seq:%u, last ts:%u"), pSp->lastPktSeq, pSp->lastPktTs);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  }

  VSX_DEBUG(
    //VSX_DEBUGLOG_TIME
    LOG(X_DEBUG("rtp-recv-opus len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
                   "flags:0x%x pQ:0x%x"),
       PKTCAPLEN(pPkt->payload), pPkt->u.rtp.seq, pPkt->u.rtp.ts, PTSF(pPkt->u.rtp.ts),
       pSp->pStream->ts0,
       (pSp->pStream->clockHz > 0 ? ((double)pSp->pStream->ts0/pSp->pStream->clockHz) : 0),
       (pSp->pStream->clockHz > 0 ? ((double)(pPkt->u.rtp.ts - pSp->pStream->ts0)/pSp->pStream->clockHz) : 0),
       (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)?1:0,
       pSp->pStream->hdr.key.ssrc, pSp->spFlags, pSp->pCapAction && pSp->pCapAction->pQueue ? 1 : 0);
    if(pPkt && PKTCAPLEN(pPkt->payload) > 0) {
      LOGHEX_DEBUG(pPkt->payload.pData, MIN(16, PKTCAPLEN(pPkt->payload)));
    }
  );

  if(len < 1) {
    LOG(X_ERROR("RTP Invalid OPUS payload length: %u"), len);
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


  if((pSp->spFlags & CAPTURE_SP_FLAG_STREAM_STARTED) && pSp->lastPktTs < pPkt->u.rtp.ts) {

    //uint32_t maxTsDeltaHz = MAX(40, pSp->pAllStreams->pCfg->pcommon->cfgMaxAudRtpGapWaitTmMs) *
    uint32_t maxTsDeltaHz = MAX(40, pSp->pStream->maxRtpGapWaitTmMs) *
                             pSp->pStream->pFilter->u_seqhdrs.aud.common.clockHz / 1000;

    opus_add_empty_frames(pSp, &cbData, queueFr, pPkt, maxTsDeltaHz);
  }

  if(pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->spFlags &= ~(CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    pSp->lastPktTs = pPkt->u.rtp.ts;
  }
 
  if(!(pPkt->flags & PKT_FLAGS_DTMF2833)) {
    rc = capture_opus_parse(pSp, &cbData, queueFr, pData, len);
  }

  if((pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST)) {
    pSp->spFlags &= ~CAPTURE_SP_FLAG_PREVLOST;
  }
  pSp->spFlags |= CAPTURE_SP_FLAG_STREAM_STARTED;

  if(pPkt->u.rtp.seq != pSp->lastPktSeq) {
    pSp->lastPktSeq = pPkt->u.rtp.seq;
  }

  return rc;
}

int streamadd_opus(CAPTURE_CBDATA_SP_T *pSp) {
  CAPTURE_FILTER_T *pFilter = (CAPTURE_FILTER_T *) pSp->pStream->pFilter;
  int rc = 0;

  if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
    // Nothign to be done, as (raw) output will be recorded to file
    return 0;
  }
  
  if(pFilter->u_seqhdrs.aud.common.clockHz == 0) {
    pFilter->u_seqhdrs.aud.common.clockHz = 8000;
    LOG(X_WARNING("Using default input OPUS clock rate %u"), pFilter->u_seqhdrs.aud.common.clockHz);
  }

  //pSp->pStream->clockHz = pFilter->u_seqhdrs.aud.common.clockHz;

  LOG(X_DEBUG("Setup OPUS RTP input %uHz, %d channels"), pFilter->u_seqhdrs.aud.common.clockHz,
      pFilter->u_seqhdrs.aud.channels);

  return rc;
}


#endif // VSX_HAVE_CAPTURE
