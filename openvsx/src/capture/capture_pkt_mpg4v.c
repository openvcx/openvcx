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


typedef struct CAPTURE_MPG4V_FRAME {
  int haveIFrame;
  int haveSeqHdr;
} CAPTURE_MPG4V_FRAME_T;


static int decodeFrame(CAPTURE_CBDATA_SP_T *pSp, const unsigned char *pData,
                       unsigned int len, CAPTURE_MPG4V_FRAME_T *pFrameInfo) {
  STREAM_CHUNK_T stream;
  int idxStartCode;
  unsigned int numDecode = 0;

  memset(&stream, 0, sizeof(stream));
  stream.bs.buf = (unsigned char *) pData;
  stream.bs.sz = len;

  if(mpg2_findStartCode(&stream) != 3) {

    if((pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST)) {
      //fprintf(stderr, "setting mpeg4 DROPFRAME flag\n");
      pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    }

    return 0;
  }

  stream.bs.sz -= 3;
  stream.bs.byteIdx += 3;

  do {

    //avc_dumpHex(stderr, &stream.bs.buf[stream.bs.byteIdx], 8, 0);

    if(mpg4v_decodeObj(&stream, &pSp->u.mpg4v) < 0) {
      break;
    }
    numDecode++;

    if(!pFrameInfo->haveIFrame && pSp->u.mpg4v.ctxt.lastFrameType == MPG4V_FRAME_TYPE_I) {
      pFrameInfo->haveIFrame = 1;
    }

    if(!pFrameInfo->haveSeqHdr && pSp->u.mpg4v.ctxt.flag == MPG4V_DECODER_FLAG_HDR) {
      pFrameInfo->haveSeqHdr = 1;
    }

    if(pFrameInfo->haveSeqHdr && pFrameInfo->haveIFrame) {
      break;
    }

    if((idxStartCode = mpg2_findStartCode(&stream)) <= 0) {
      break;
    }

    stream.bs.byteIdx += idxStartCode;

  } while(stream.bs.byteIdx < stream.bs.sz);

  //fprintf(stderr, "mpg4v numDecode:%d\n", numDecode);
  
  return numDecode;
}

static int processFrame(CAPTURE_CBDATA_SP_T *pSp, const COLLECT_STREAM_PKTDATA_T *pPkt,
                        CAPTURE_STORE_CBDATA_T *pCbData) { 
  int rc = 0;
  CAPTURE_MPG4V_FRAME_T frameInfo;
  const MPG4V_SEQ_HDRS_T *pSeqHdrs = NULL;
  const unsigned char *pData = (pPkt ? pPkt->payload.pData : NULL);
  unsigned int len = (pPkt ? PKTCAPLEN(pPkt->payload) : 0);

  if(len < 1) {
    LOG(X_ERROR("RTP MPEG-4V insufficient packet length %d"), len);
    return -1;
  }

  if(pSp->lastPktTs != pPkt->u.rtp.ts) {

    memset(&frameInfo, 0, sizeof(frameInfo));
    memset(&pSp->u.mpg4v, 0, sizeof(pSp->u.mpg4v));

    if(decodeFrame(pSp, pData, len, &frameInfo) > 0 &&
      frameInfo.haveIFrame && !frameInfo.haveSeqHdr) {

      //fprintf(stderr, "MPEG-4 FRAME TS:%d len:%d haveSeqHdr:%d haveIFrame:%d\n", pPkt->u.rtp.ts, len, frameInfo.haveSeqHdr, frameInfo.haveIFrame);

      //
      // Insert mpeg-4 sequence headers into the bitstream, preceeding a keyframe
      //
      pSeqHdrs = &pSp->pStream->pFilter->u_seqhdrs.vid.u.mpg4v.seqHdrs;

      //fprintf(stderr, "got an I frame w/ no seq hdrs, sdp hdrs: 0x%x hdrsLen:%d\n", pSeqHdrs->hdrsBuf, pSeqHdrs->hdrsLen);
      //avc_dumpHex(stderr, pSeqHdrs->hdrsBuf, pSeqHdrs->hdrsLen, 0);

      if(pSeqHdrs && (rc = pCbData->cbStoreData(pCbData->pCbDataArg, pSeqHdrs->hdrsBuf, pSeqHdrs->hdrsLen)) < 0) {
        return rc;
      }
    }

  }

  //TODO: handle RTP padding

  rc = pCbData->cbStoreData(pCbData->pCbDataArg, pData, len);

  if(frameInfo.haveIFrame) {
    pSp->spFlags |= CAPTURE_SP_FLAG_KEYFRAME;
  }

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

int cbOnPkt_mpg4v(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  int rc = 0;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;

  if(pSp == NULL) {
    return -1;
  } else if(!pPkt || !pPkt->payload.pData) {
    // Packet was lost
    LOG(X_WARNING("RTP MPEG-4V lost packet, last seq:%u, last ts:%u"), pSp->lastPktSeq, pSp->lastPktTs);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - rtp-recv-mpg4v len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
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
