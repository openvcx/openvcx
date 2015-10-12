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

#define H264_FLAG_HAVE_SPS      0x01
#define H264_FLAG_HAVE_PPS      0x02

static int writeNALFromNet(CAPTURE_STORE_CBDATA_T *pCbData, 
                           const unsigned char *pNal, unsigned int lenNal, 
                           int writeStartCode) {

  const uint32_t startCode = htonl(1);
  const unsigned char escapeChar = 0x03;
  int rc;
  int lenwrote = 0;
  unsigned idx = 0;
  unsigned int idxPrev = 0;

  if(writeStartCode) {
    if(pCbData->cbStoreData(pCbData->pCbDataArg, &startCode, H264_STARTCODE_LEN) < 0) {
      return -1;
    }
    lenwrote += H264_STARTCODE_LEN; 
  }

  while(idx < (unsigned int) lenNal) {

    //
    // Create escape sequence '0x00 00 03' if encoutering sequenc of 
    // '0x00 00 00 | 0x00 00 01 | 0x 00 00 02' in the bit stream
    // Assume that 0x00 00 03 has already been escaped by remote encoder
    // 
    if(idx > 1 && pNal[idx-2] == 0x00 && pNal[idx-1] == 0x00 && pNal[idx] < 0x03) {
//fprintf(stderr, "writing escape sequence in bitstream\n");
      if((rc = pCbData->cbStoreData(pCbData->pCbDataArg, 
                &pNal[idxPrev], idx - idxPrev)) < 0) {
        return -1;
      }
      lenwrote += rc;
      if((rc = pCbData->cbStoreData(pCbData->pCbDataArg, &escapeChar, 1)) < 0) {
        return -1;
      }
      lenwrote += rc;
      idxPrev = idx;

    }

    idx++;
  }

  if((rc = pCbData->cbStoreData(pCbData->pCbDataArg, &pNal[idxPrev], 
                                idx - idxPrev)) < 0) {
    return -1;
  }
  lenwrote += rc;

/*
  // Avoid ending on 0x00, 0x01... to prevent conflict with naext annexB start code sequence
  if(lenNal > 0 && pNal[lenNal - 1] == 0x00) {
    if((rc = WriteFileStream(pStreamOut, &escapeChar, 1)) < 0) {
      return -1;
    }
    lenwrote += rc;
  }
*/
  return 0;
}

static int onPkt_h264(CAPTURE_CBDATA_SP_T *pSp, CAPTURE_STORE_CBDATA_T *pCbData,
              const unsigned char *pData, const unsigned int len, int recurselevel) {

  int nalType;
  int rc = 0;
  uint16_t nalLen;
  unsigned int idx = 0;

  if(pData == NULL || len == 0) {
    // TODO: write empty NAL
    LOG(X_WARNING("RTP H.264 lost slice len:%d, %s, ssrc: 0x%x"), 
        len, pSp->pStream->strSrcDst, pSp->pStream->hdr.key.ssrc);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  } else if(recurselevel > 10) {
    LOG(X_WARNING("RTP H.264 maximum STAP nesting level reached:%d"), recurselevel);
    return 0;
  }


  nalType = pData[idx] & NAL_TYPE_MASK;
  //fprintf(stderr, "nal: %d\n", nalType); avc_dumpHex(stderr, &pData[idx], 4, 0);

  switch(nalType) {

    case NAL_TYPE_IDR:
    case NAL_TYPE_SEI:

      //
      // Prepend any SPS/PPS (from SDP) if not present in this GOP
      //
      if(pSp->pStream->pFilter->u_seqhdrs.vid.u.h264.spspps.sps_len > 0 &&
        !(pSp->spFlags & CAPTURE_SP_FLAG_H264_HAVESPS)) {

        if((rc = writeNALFromNet(pCbData, 
                 pSp->pStream->pFilter->u_seqhdrs.vid.u.h264.spspps.sps_buf,
                 pSp->pStream->pFilter->u_seqhdrs.vid.u.h264.spspps.sps_len, 1)) < 0) {
          return rc; 
        }
        pSp->spFlags |= CAPTURE_SP_FLAG_H264_HAVESPS;
      }

      if((pSp->spFlags & CAPTURE_SP_FLAG_H264_HAVESPS) &&
        pSp->pStream->pFilter->u_seqhdrs.vid.u.h264.spspps.pps_len > 0 &&
        !(pSp->spFlags & CAPTURE_SP_FLAG_H264_HAVEPPS)) {

        if((rc = writeNALFromNet(pCbData, 
                 pSp->pStream->pFilter->u_seqhdrs.vid.u.h264.spspps.pps_buf,
                 pSp->pStream->pFilter->u_seqhdrs.vid.u.h264.spspps.pps_len, 1)) < 0) {
          return rc; 
        }
        pSp->spFlags |= CAPTURE_SP_FLAG_H264_HAVEPPS;
      }


      pSp->spFlags |= CAPTURE_SP_FLAG_KEYFRAME;

      if((rc = writeNALFromNet(pCbData, pData, len, 1)) < 0) {
        return rc; 
      }
      break;

    case NAL_TYPE_NONSPECIFIC:
    case NAL_TYPE_SLICE:
    case NAL_TYPE_PARTITION_2:
    case NAL_TYPE_PARTITION_3:
    case NAL_TYPE_PARTITION_4:

      if((rc = writeNALFromNet(pCbData, pData, len, 1)) < 0) {
        return rc; 
      }
      break;

    case NAL_TYPE_SEQ_PARAM:

      pSp->spFlags |= CAPTURE_SP_FLAG_H264_HAVESPS;

      if((rc = writeNALFromNet(pCbData, pData, len, 1)) < 0) {
        return rc; 
      }
      break;

    case NAL_TYPE_PIC_PARAM:
      pSp->spFlags |= CAPTURE_SP_FLAG_H264_HAVEPPS;

      if((rc = writeNALFromNet(pCbData, pData, len, 1)) < 0) {
        return rc; 
      }
      break;

    case NAL_TYPE_STAP_A:

      idx++;
      do {
        nalLen = htons(*((uint16_t *) &pData[idx]));
        idx += 2;
        if(nalLen + idx > len) {
          LOG(X_WARNING("RTP H.264 Invalid STAP-A NAL length: %d  (%u/%d, "), 
            nalLen, idx, len - idx);
          return -1;
        } else if(nalLen > 0) {
          if((rc = onPkt_h264(pSp, pCbData, &pData[idx], nalLen, recurselevel + 1)) < 0) {
            return rc;
          }
        }
        idx += nalLen;

      } while(idx + 3 < len);

      break;

    case NAL_TYPE_FRAG_A:

     // Frag Unit header 1 bit start, 1 bit end, 1 bit reserved=0, 5 bits nal type
      nalType = pData[++idx] & NAL_TYPE_MASK;

      if(nalType == NAL_TYPE_IDR) {
        pSp->spFlags |= CAPTURE_SP_FLAG_KEYFRAME;
      }

      if(pData[idx] & 0x80) { // start bit set

        pSp->spFlags |= CAPTURE_SP_FLAG_HAVEFRAGMENTSTART;

        //fprintf(stderr, "frag start nal: %d flag:%d\n", nalType, pSp->spFlags); avc_dumpHex(stderr, &pData[idx], 4, 0);

        ((unsigned char *)pData)[idx] &= 0x1f;
        ((unsigned char *)pData)[idx] |= (NAL_NRI_MASK & pData[idx - 1]);
        if((rc = writeNALFromNet(pCbData, &pData[idx], len - idx, 1)) < 0) {
          return rc;
        }

      } else {

        //
        // If the previous packet(s) were lost, and this is a fragment,
        // ensure it is not a fragment continuation
        //
        if(!(pSp->spFlags & CAPTURE_SP_FLAG_HAVEFRAGMENTSTART) || (pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST)) {
          //fprintf(stderr, "setting H.264 DROPFRAME flag\n");
          pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
        }

        //fprintf(stderr, "frag cont nal: %d flag:%d\n", nalType, pSp->spFlags); avc_dumpHex(stderr, &pData[idx], 4, 0);

        idx++;
        if((rc = writeNALFromNet(pCbData, &pData[idx], len - idx, 
                               (pSp->spFlags & CAPTURE_SP_FLAG_HAVEFRAGMENTSTART) ? 0 : 1)) < 0) {
          return rc;
        }

        pSp->spFlags |= CAPTURE_SP_FLAG_HAVEFRAGMENTSTART;

      }

      break;

    default:
      LOG(X_WARNING("RTP H.264 Unsupported nal type: %d"), nalType);
      break;

  }

  return (int)(len - idx);
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

int cbOnPkt_h264(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  int rc = 0;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;

  if(pSp == NULL) {
    return -1;
   } else if(!pPkt || !pPkt->payload.pData) {
    // Packet was lost
    LOG(X_WARNING("RTP H.264 lost packet, last seq:%u, last ts:%u"), pSp->lastPktSeq, pSp->lastPktTs);
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - rtp-recv-h264 %s len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
                   "flags:0x%x pQ:0x%x"), pSp->pStream->strSrcDst,
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


  if(queueFr && pSp->pCapAction->tmpFrameBuf.idx > 0 && 
     pSp->lastPktTs != pPkt->u.rtp.ts) { 
    //fprintf(stderr, "queing h264 (prior) %d seq:%d ts:%u,now:%u, flags:0x%x\n", pSp->pCapAction->tmpFrameBuf.idx, pPkt->u.rtp.seq, pSp->lastPktTs, pPkt->u.rtp.ts, pSp->spFlags);

    rc = onCompleteFrame(pSp, pSp->lastPktTs);

  }

  if(pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->spFlags &= ~(CAPTURE_SP_FLAG_KEYFRAME | CAPTURE_SP_FLAG_DAMAGEDFRAME |
                      CAPTURE_SP_FLAG_DROPFRAME | CAPTURE_SP_FLAG_H264_HAVESPS | 
                      CAPTURE_SP_FLAG_H264_HAVEPPS);
  }

  if(!(pSp->spFlags & CAPTURE_SP_FLAG_DAMAGEDFRAME)) {
    //fprintf(stderr, "onPkt_h264 flags:0x%x\n", pSp->spFlags);
    if((rc = onPkt_h264(pSp, &cbData, (pPkt ? pPkt->payload.pData : NULL), 
                        (pPkt ? PKTCAPLEN(pPkt->payload) : 0), 0)) < 0) {
      pSp->spFlags |= CAPTURE_SP_FLAG_DAMAGEDFRAME;
    }

    //if((pSp->spFlags & (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME))) {
    //}

  } else {
    //fprintf(stderr, "ignoring processing of this frame since it was damaged...\n");
  }

  if(queueFr && (pPkt->u.rtp.marksz & PKTDATA_RTP_MASK_MARKER)) { 

    if(pSp->pCapAction->tmpFrameBuf.idx == 0) {
      pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    }
    //fprintf(stderr, "queing h264 (marker) %d seq:%d ts:%u, flags:0x%x\n", pSp->pCapAction->tmpFrameBuf.idx, pPkt->u.rtp.seq, pPkt->u.rtp.ts, pSp->spFlags);

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
