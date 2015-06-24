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



static int processPkt(CAPTURE_CBDATA_SP_T *pSp, const COLLECT_STREAM_PKTDATA_T *pPkt,
                        CAPTURE_STORE_CBDATA_T *pCbData) { 
  int rc = 0;
  const unsigned char *pData = pPkt->payload.pData;
  unsigned int len = PKTCAPLEN(pPkt->payload);
  unsigned int extHdrIdx = 0;
  unsigned int picId = 0;
  unsigned int partitionId;

  //fprintf(stderr, "pkt-len:%d ", len); avc_dumpHex(stderr, pPkt->payload.pData, MIN(16, PKTCAPLEN(pPkt->payload)), 0);

  //
  // http://www.potaroo.net/ietf/all-ids/draft-ietf-payload-vp8-08.txt
  //

  if(len < 1) {
    LOG(X_ERROR("RTP VP8 insufficient packet length %d, ssrc: 0x%x, seq:%u, ts:%u"), 
         len, pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts); 
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME);
    return -1;
  }

  if(pSp->lastPktTs != pPkt->u.rtp.ts) {
    memset(&pSp->u.vp8, 0, sizeof(pSp->u.vp8));
  }

  if((partitionId = (pData[0] & VP8_PARTITIONS_MASK)) >= VP8_PARTITIONS_COUNT) {
    LOG(X_ERROR("RTP VP8 invalid partition id %d, ssrc: 0x%x, seq:%u, ts:%u"), 
         partitionId, pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts); 
    return -1;
  }

  if((pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST) && (pData[0] & VP8_PD_START_PARTITION) && partitionId > 0 && 
     pSp->u.vp8.partitions[partitionId - 1].byteIdx == 0) {

    LOG(X_ERROR("RTP VP8 start of partition[%d], but partition[%d] has %d bytes, %s, ssrc: 0x%x, seq:%u, ts:%u"), 
        partitionId, partitionId - 1, pSp->u.vp8.partitions[partitionId - 1].byteIdx, 
        (pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST) ? " prior pkt lost" : "",
        pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts),
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    return -1;

  } else if((pData[0] & VP8_PD_START_PARTITION) && pSp->u.vp8.partitions[partitionId].byteIdx > 0) {

    LOG(X_ERROR("RTP VP8 partition bit set for continuation of partition[%d] byte[%d]%s, ssrc: 0x%x, seq:%u, ts:%u"), 
        partitionId, pSp->u.vp8.partitions[partitionId].byteIdx, 
        (pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST) ? " prior pkt lost" : "",
        pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts),
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    return -1;

  } else if(!(pData[0] & VP8_PD_START_PARTITION) && pSp->pCapAction->tmpFrameBuf.idx == 0) {

    LOG(X_ERROR("RTP VP8 partition bit not set for presumed start of partition[%d] byte[%d]%s, ssrc: 0x%x, seq:%u, ts:%u"), 
        partitionId, pSp->u.vp8.partitions[partitionId].byteIdx,
        (pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST) ? " prior pkt lost" : "",
        pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts),
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    return -1;

  }

  if(pData[0] & VP8_PD_RESERVED_BIT) {
    LOG(X_ERROR("RTP VP8 reserved bit should not be set, ssrc: 0x%x, seq:%u, ts:%u"), 
        pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME);
    return -1;
  }

  if(pData[0] & VP8_PD_EXTENDED_CTRL) {
    if(len < 2) {
      LOG(X_ERROR("RTP VP8 insufficient packet length %d, ssrc: 0x%x, seq:%u, ts:%u"), 
          len, pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts);
      pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME);
      return -1;
    }
    extHdrIdx++;
    if(pData[1] & VP8_PD_X_START_PICID_PRESENT) {
      extHdrIdx++;
      if(pData[2] & VP8_PD_I_START_PICID_16BIT) {
        extHdrIdx++;
        picId = htons(((pData[2] & 0x7f) << 8)| pData[3]);
      } else {
        picId = pData[2] & 0x7f;
      }
    }
    if(pData[1] & VP8_PD_X_START_TLOPICIDX_PRESENT) {
      extHdrIdx++;
    }
    if((pData[1] & VP8_PD_X_START_TID_PRESENT) || (pData[1] & VP8_PD_X_START_TID_KEYIDX_PRESENT)) {
      extHdrIdx++;
    }

  }
  extHdrIdx++;

  if(len < extHdrIdx) {
    LOG(X_ERROR("RTP VP8 insufficient packet length %d, ssrc: 0x%x, seq:%u, ts:%u"), 
        len, pSp->pStream->hdr.key.ssrc, pPkt->u.rtp.seq, pPkt->u.rtp.ts);
    return -1;
  }

  if(pSp->pCapAction->tmpFrameBuf.idx == 0 && (pData[0] & VP8_PD_START_PARTITION)) {
    if(VP8_ISKEYFRAME(&pData[extHdrIdx])) {
    //fprintf(stderr, "setting haveKeyFrame to 1 spFlags:0x%x\n\n", pSp->spFlags);
      pSp->spFlags |= CAPTURE_SP_FLAG_KEYFRAME;
      //LOG(X_DEBUG("cap-vp8 RTP pts:%.3f, seq:%u KEYFRAME"), (float) pPkt->u.rtp.ts / 90000, pPkt->u.rtp.seq); 
    } else {
      //LOG(X_DEBUG("cap-vp8 RTP pts:%.3f, seq:%u"), (float) pPkt->u.rtp.ts / 90000, pPkt->u.rtp.seq);
    }
  } 

  //TODO: handle RTP padding

  pSp->u.vp8.partitions[partitionId].byteIdx += (len - extHdrIdx);

  //LOG(X_DEBUG("vp8 storing data tmpFrame.buf.idx:%d, pktlen:%d,exthdridx:%d"), pSp->pCapAction->tmpFrameBuf.idx, len, extHdrIdx); LOGHEX_DEBUG(pData, MIN(16, PKTCAPLEN(pPkt->payload)));
  rc = pCbData->cbStoreData(pCbData->pCbDataArg, &pData[extHdrIdx], len - extHdrIdx);

  return rc;
}

static int onCompleteFrame(CAPTURE_CBDATA_SP_T *pSp, uint32_t ts) {

  if((pSp->spFlags & CAPTURE_SP_FLAG_DAMAGEDFRAME) && pSp->pAllStreams->pCfg) {

    if(pSp->pAllStreams->pCfg->pcommon->rtp_frame_drop_policy == CAPTURE_FRAME_DROP_POLICY_DROPWANYLOSS) {
      pSp->spFlags |= CAPTURE_SP_FLAG_DROPFRAME;
    } else if(pSp->pAllStreams->pCfg->pcommon->rtp_frame_drop_policy == CAPTURE_FRAME_DROP_POLICY_NODROP) {
      pSp->spFlags &= ~CAPTURE_SP_FLAG_DROPFRAME;
    }
  }

  return capture_addCompleteFrameToQ(pSp, ts);
}

int cbOnPkt_vp8(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  int rc = 0;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;

  if(pSp == NULL) {
    return -1;
  } else if(!pPkt || !pPkt->payload.pData) {
    // Packet was lost
    LOG(X_WARNING("RTP VP8 lost packet, last seq:%u, last ts:%u"), pSp->lastPktSeq, pSp->lastPktTs);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  }

  VSX_DEBUG(
    //VSX_DEBUGLOG_TIME
    LOG(X_DEBUG("rtp-recv-vp8 len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
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
    //fprintf(stderr, "tmpFrame.buf.idx:%d\n", pSp->pCapAction->tmpFrameBuf.idx);
  );

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

  if(queueFr && pSp->pCapAction->tmpFrameBuf.idx > 0 && pSp->lastPktTs != pPkt->u.rtp.ts) { 
    //fprintf(stderr, "queing vp8 (prior) %d ts:%u,now:%u, key:%d, flags:0x%x\n", pSp->pCapAction->tmpFrameBuf.idx, pSp->lastPktTs, pPkt->u.rtp.ts, pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME, pSp->spFlags);

    rc = onCompleteFrame(pSp, pSp->lastPktTs);
  }

  if(pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->spFlags &= ~(CAPTURE_SP_FLAG_KEYFRAME | CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
  }

  if(!(pSp->spFlags & CAPTURE_SP_FLAG_DAMAGEDFRAME) && processPkt(pSp, pPkt, &cbData) < 0) {
    pSp->spFlags |= CAPTURE_SP_FLAG_DAMAGEDFRAME;
  }

  if(queueFr && (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)) { 
    //fprintf(stderr, "queing vp8 (marker) %d ts:%u, key:%d, flags:0x%x\n", pSp->pCapAction->tmpFrameBuf.idx, pPkt->u.rtp.ts,  pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME, pSp->spFlags);

    if(pSp->pCapAction->tmpFrameBuf.idx == 0) {
      pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    }

    rc = onCompleteFrame(pSp, pPkt->u.rtp.ts);
  }

  if((pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST)) {
    pSp->spFlags &= ~CAPTURE_SP_FLAG_PREVLOST;
  }
  if(pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->lastPktTs = pPkt->u.rtp.ts;
  }
  if(pPkt->u.rtp.seq != pSp->lastPktSeq) {
    pSp->lastPktSeq = pPkt->u.rtp.seq;
  }

  return rc;
}

#endif // VSX_HAVE_CAPTURE
