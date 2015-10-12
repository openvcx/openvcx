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

static int processFrame(CAPTURE_CBDATA_SP_T *pSp, const COLLECT_STREAM_PKTDATA_T *pPkt,
                        CAPTURE_STORE_CBDATA_T *pCbData) { 
  int rc = 0;
  BIT_STREAM_T bs;
  unsigned char buf[2];
  unsigned int idx = 0;
  const unsigned char *pData = (pPkt ? pPkt->payload.pData : NULL);
  unsigned int len = (pPkt ? PKTCAPLEN(pPkt->payload) : 0);

  if(len < 4) {
    LOG(X_ERROR("RTP H.263 insufficient packet length %d"), len);
    return -1;
  }

  if(pSp->u.h263.codecType == XC_CODEC_TYPE_H263_PLUS) {

    //
    //     http://tools.ietf.org/html/rfc4629
    //
    //     0                   1
    //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //    |   RR    |P|V|   PLEN    |PEBIT|
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //

    //LOGHEX_DEBUG(pPkt->payload.pData, MIN(16, len));
    int picSeg = pData[0] & 0x04;
    int vrc = pData[0] & 0x02;
    int plen = ((pData[0] & 0x01) << 5) | ((pData[1] & 0xf8) >> 3);
    idx = 2 + (vrc ? 1 : 0) + plen;

    if(idx >= len) {
      LOG(X_ERROR("RTP H.263+ insufficient packet length %d, VRC:%d, PLEN:%d"), len, vrc, plen);
      return -1;
    }

    if(picSeg) {
      memset(buf, 0, sizeof(buf)); 
      rc = pCbData->cbStoreData(pCbData->pCbDataArg, buf, 2);
    }

  } else if(pSp->u.h263.codecType == XC_CODEC_TYPE_H263) {
 
    //
    // http://tools.ietf.org/html/rfc2190
    // 0                   1                   2                   3
    // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |F|P|SBIT |EBIT | SRC |I|U|S|A|R      |DBQ| TRB |    TR         |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //

    idx = 4;
    if(pData[0] & 0x80) {
      // F=1, Mode > 0 
      idx += 4;
      if(pData[0] & 0x40) {
        // P=1, Mode 2
        idx += 4;
      }
    }

#if 0
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

    LOG(X_DEBUG("H.263 (skip idx:%d) start:%d, end:%d, src:%d, i:%d, u:%d, s:%d, a:%d, r:%d, dbq:%d, trb:%d, tr:%d"), idx, startbit, endbit, src, ibit, ubit, sbit, abit, r, dbq, trb, tr);
#endif // 0

    if(idx >= len) {
      LOG(X_ERROR("RTP H.263 insufficient packet length %d"), len);
      return -1;
    }

  }

  if(pSp->lastPktTs != pPkt->u.rtp.ts) {

    memset(&bs, 0, sizeof(bs));
    bs.buf = (unsigned char *) &pData[idx];
    bs.sz = len - idx;

    pSp->spFlags &= ~CAPTURE_SP_FLAG_KEYFRAME;
    if(h263_decodeHeader(&bs, &pSp->u.h263.descr, 
                         pSp->u.h263.codecType == XC_CODEC_TYPE_H263_PLUS ? 1 : 0) >= 0) {

      if(pSp->u.h263.descr.frame.frameType == H263_FRAME_TYPE_I) {
        pSp->spFlags |= CAPTURE_SP_FLAG_KEYFRAME;
      }

      //LOG(X_DEBUG("H.263%s %dx%d %s picnum:%d%s%s%s"), pSp->u.h263.descr.plus.h263plus ? "+" : "", pSp->u.h263.descr.param.frameWidthPx, pSp->u.h263.descr.param.frameHeightPx,  pSp->u.h263.descr.frame.frameType == H263_FRAME_TYPE_I ? "I" : pSp->u.h263.descr.frame.frameType == H263_FRAME_TYPE_B ? "B" : "P", pSp->u.h263.descr.frame.picNum, pSp->u.h263.descr.plus.advIntraCoding ? ",I=1" : "", pSp->u.h263.descr.plus.deblocking ? ",J=1" : "", pSp->u.h263.descr.plus.sliceStructured ? ",K=1" : "");

    } else {
       LOG(X_ERROR("Failed to decode rtp input H.263 header length %d"), len);
       LOGHEX_DEBUG(pData, MIN(16, len));
    }
   
  }

  //TODO: handle RTP padding

  //LOG(X_DEBUG("CBSTOREDATA idx:%d, len:%d-%d, plen:%d, vrc:%d, picSeg:%d"), idx, len, idx, plen, vrc, picSeg); 
  rc = pCbData->cbStoreData(pCbData->pCbDataArg, &pData[idx], len - idx);

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

int cbOnPkt_h263(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  int rc = 0;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;

  if(pSp == NULL) {
    return -1;
  } else if(!pPkt || !pPkt->payload.pData) {
    // Packet was lost
    LOG(X_WARNING("RTP H.263 lost packet, last seq:%u, last ts:%u"), pSp->lastPktSeq, pSp->lastPktTs);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - rtp-recv-h263 len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
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

  //static uint32_t tslast; if(pPkt->u.rtp.ts!=tslast) fprintf(stderr, "tsd:%d\n", pPkt->u.rtp.ts - tslast); tslast = pPkt->u.rtp.ts;

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
    //fprintf(stderr, "queing mpg4v (prior) %d ts:%u,now:%u\n", pSp->pCapAction->tmpFrameBuf.idx, pSp->lastPktTs, pPkt->u.rtp.ts);
    rc = onCompleteFrame(pSp, pSp->lastPktTs);
  }

  if(pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->spFlags &= ~(CAPTURE_SP_FLAG_KEYFRAME | CAPTURE_SP_FLAG_DAMAGEDFRAME |
                      CAPTURE_SP_FLAG_DROPFRAME);
  }

  if(!(pSp->spFlags & CAPTURE_SP_FLAG_DAMAGEDFRAME) && processFrame(pSp, pPkt, &cbData) < 0) {
    pSp->spFlags |= CAPTURE_SP_FLAG_DAMAGEDFRAME;
  }

  if(queueFr && (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)) { 
    //fprintf(stderr, "queing mpg4v (marker) %d ts:%u\n", pSp->pCapAction->tmpFrameBuf.idx, pPkt->u.rtp.ts);

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

  return rc;
}

#endif // VSX_HAVE_CAPTURE
