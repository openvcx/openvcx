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

#if 0
static unsigned char s_amr_empty_32[] =  { 0x3c, 0x54, 0xfd, 0x1f, 0xb6, 0x66, 0x79, 0xe1,
                                           0xe0, 0x01, 0xe7, 0xcf, 0xf0, 0x00, 0x00, 0x00,
                                           0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static unsigned char s_amr_empty_32b[] = { 0x3c, 0x48, 0xf5, 0x1f, 0x96, 0x66, 0x79, 0xe1,
                                           0xe0, 0x01, 0xe7, 0x8a, 0xf0, 0x00, 0x00, 0x00,
                                           0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static int amr_load_silent_frame(CAPTURE_CBDATA_SP_T *pSp, unsigned char *buf) {

  uint8_t frmtype = MIN(7, pSp->u.amr.lastFrameType); 
  uint32_t frmsize = AMRNB_GETFRAMEBODYSIZE_FROMTYPE(frmtype); 
  static int s_amr_silent_fr;

  switch(frmtype) {
    case 7:
    default:
      buf[0] = (frmtype << 3);
      if(s_amr_silent_fr++ == 0)
        memcpy(&buf[1], &s_amr_empty_32[1], frmsize);
      else 
        memcpy(&buf[1], &s_amr_empty_32b[1], frmsize);
      if(s_amr_silent_fr > 1) s_amr_silent_fr=0;
      //memset(&buf[1], 0, frmsize);
      break;
   }

  return frmsize;
}
#endif // 0

static int amr_add_empty_frames(CAPTURE_CBDATA_SP_T *pSp,
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
  //TODO: re-use last frame type instead of hardcoding to 12.2Kb/s AMR-NB
  //uint8_t frmtype = 7;
  //int sz;
  uint8_t frmtype = MIN(7, pSp->u.amr.lastFrameType); 
  uint32_t sz = AMRNB_GETFRAMEBODYSIZE_FROMTYPE(frmtype); 

  if((numFrames = (tsDeltaHz / AMRNB_SAMPLE_DURATION_HZ)) <= 0) {
    return rc;
  }

  if(tsDeltaHz > maxTsDeltaHz) {
    LOG(X_WARNING("Audio ts gap %uHz (%uHz -> %uHz) greater than max %uHz to fill silence for %s, ssrc: 0x%x, ts: %u, seq:%u"),
      tsDeltaHz, pSp->lastPktTs, pPkt->u.rtp.ts, maxTsDeltaHz, pSp->pStream->strSrcDst, pSp->pStream->hdr.key.ssrc,
      pPkt->u.rtp.ts, pPkt->u.rtp.seq);
    return 0;
  }

  LOG(X_WARNING("Adding %d silent AMR-NB frames to account for timestamp gap of %dHz (max %dHz) for %s, ssrc: 0x%x, ts: %u, seq:%u"),
        numFrames, tsDeltaHz, maxTsDeltaHz, pSp->pStream->strSrcDst, pSp->pStream->hdr.key.ssrc,
        pPkt->u.rtp.ts, pPkt->u.rtp.seq);

  //LOG(X_WARNING("Will add %d empty AMR-NB frames to account for timestamp gap of %dHz"), numFrames, tsdelta);

  for(idxFrame = 0; idxFrame < numFrames; idxFrame++) {

    buf[0] = (frmtype << 3);
    memset(&buf[1], 0, sz);
    //if((sz = amr_load_silent_frame(pSp, buf)) < 0) {
    //  rc = -1;
    //  break;
    //}
    //avc_dumpHex(stderr, buf, sz + 1, 1);
    pCbData->cbStoreData(pCbData->pCbDataArg, buf, sz + 1);

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

static int capture_amr_parse(CAPTURE_CBDATA_SP_T *pSp, 
                             CAPTURE_STORE_CBDATA_T *pCbData,
                             int queueFr,
                             const unsigned char *pPkt,
                             unsigned int lenPkt) {

#define AMR_FRAMES_IN_RTP_PKT_MAX           100

  unsigned int idxHdr = 0;
  unsigned int idxData;
  int rc = 0;
  uint32_t tsoffset = 0;
  unsigned int numpackets = 0;
  int haserror = 0;
  uint8_t havenext = 1;
  uint8_t frmtype;
  uint8_t frmsize;
  uint16_t offsets[AMR_FRAMES_IN_RTP_PKT_MAX];
  uint16_t framesizes[AMR_FRAMES_IN_RTP_PKT_MAX];
  uint8_t headers[AMR_FRAMES_IN_RTP_PKT_MAX];
  
  //
  // RFC3267 AMR RTP Packetization using octet aligned mode
  //
  
  // check 4 reserved bits
  if((pPkt[idxHdr++] & 0x0f) != 0) {
    LOG(X_ERROR("AMR RTP packet has incorrect CMR reserved field for octet-aligned mode"));
    return -1;
  }

  idxData = idxHdr;

  //
  // Count and gather all the frame offsets
  //
  while(havenext && idxHdr < lenPkt && idxData  < lenPkt) {

    havenext = pPkt[idxHdr] & 0x80;
    frmtype = AMR_GETFRAMETYPE( pPkt[idxHdr] );
    frmsize = AMRNB_GETFRAMEBODYSIZE_FROMTYPE( frmtype );

    pSp->u.amr.lastFrameType = frmtype;

    if(!haserror) {

      if(idxData + frmsize > lenPkt) {
        LOG(X_WARNING("AMR packet [%d] at offset %u + %u > %u"), 
           numpackets + 1, idxData, frmsize, lenPkt); 
        haserror = 1;
      } else {

        // F=0 (1 bit), Frame Type (4 bits), Q=1 (1bit)
        headers[numpackets] = (frmtype << 3) | (pPkt[idxHdr] & 0x04);  
        offsets[numpackets] = idxData - 1; 
        framesizes[numpackets] = frmsize; 

        if(++numpackets >= AMR_FRAMES_IN_RTP_PKT_MAX) {
          LOG(X_WARNING("AMR max frames per packet reached: %d"), numpackets); 
          haserror = 1;
        }
      }

    }

    idxHdr++;
    idxData += frmsize;
  }

  //fprintf(stderr, "amr numpackets:%d idxHdr:%d\n", numpackets, idxHdr);

  for(idxData = 0; idxData < numpackets; idxData++) {

    if(idxHdr + offsets[idxData] > lenPkt) {
      break;
    } 

    //fprintf(stderr, "amr header: 0x%x\n", headers[idxData]);
    pCbData->cbStoreData(pCbData->pCbDataArg, &headers[idxData], 1);
    pCbData->cbStoreData(pCbData->pCbDataArg, &pPkt[offsets[idxData] + idxHdr], 
                        framesizes[idxData]);

    if(queueFr) {
      if((rc = capture_addCompleteFrameToQ(pSp, pSp->lastPktTs + tsoffset)) < 0) {
        rc = -1;
        break;
      }
    }

    tsoffset += AMRNB_SAMPLE_DURATION_HZ; 
  }

  pSp->lastPktTs += (numpackets * AMRNB_SAMPLE_DURATION_HZ);

  if(rc == 0) {
    return idxData;
  } else {
    return rc;
  }
}

int cbOnPkt_amr(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  const unsigned char *pData = pPkt ? pPkt->payload.pData : NULL;
  unsigned int len = pPkt ? PKTCAPLEN(pPkt->payload) : 0;
  CAPTURE_STORE_CBDATA_T cbData;
  int queueFr = 0;
  int rc = 0;

  if(pSp == NULL) {
    return -1;
  } else if(pData == NULL) {
    // Packet was lost
    LOG(X_WARNING("Lost AMR RTP packet"));
    pSp->spFlags |= (CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_PREVLOST);
    return 0;
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - rtp-recv-amr %s len:%d seq:%u ts:%u(%.3f) ts0:%u(%.3f, dlta:%.3f), mrk:%d ssrc:0x%x "
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

  if(len < 1) {
    LOG(X_ERROR("Invalid AMR RTP payload length: %u"), len);
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

    //
    // Write the file start header
    // 
    if(pSp->pStream->numPkts == 1) {
      capture_cbWriteFile(cbData.pCbDataArg, AMRNB_TAG, AMRNB_TAG_LEN);
    }

  } else {
    // Nothing to be done
    return 0;
  }

#if 0
  //fprintf(stderr, "amr... spFlags:0x%x %u %u\n", pSp->spFlags, pSp->lastPktTs, pPkt->u.rtp.ts);
  static int test_drop, test_drop_start; 
  if(test_drop++ % 20 == 14 || (test_drop_start > 0 && test_drop_start < 3)) { 
    test_drop_start++; fprintf(stderr, "dropping...\n"); return 0; 
  } else { 
    test_drop_start=0; 
  }
#endif // 0

  if((pSp->spFlags & CAPTURE_SP_FLAG_STREAM_STARTED) && pSp->lastPktTs < pPkt->u.rtp.ts) {
    //uint32_t maxTsDeltaHz = MAX(40, pSp->pAllStreams->pCfg->pcommon->cfgMaxAudRtpGapWaitTmMs) *
    uint32_t maxTsDeltaHz = MAX(40, pSp->pStream->maxRtpGapWaitTmMs) *
                             pSp->pStream->pFilter->u_seqhdrs.aud.common.clockHz / 1000;

    amr_add_empty_frames(pSp, &cbData, queueFr, pPkt, maxTsDeltaHz);
  }

  if(pPkt->u.rtp.ts != pSp->lastPktTs) {
    pSp->spFlags &= ~(CAPTURE_SP_FLAG_DAMAGEDFRAME | CAPTURE_SP_FLAG_DROPFRAME);
    pSp->lastPktTs = pPkt->u.rtp.ts;
  }

  if(!(pPkt->flags & PKT_FLAGS_DTMF2833)) {
    rc = capture_amr_parse(pSp, &cbData, queueFr, pData, len);
  }

  if((pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST)) {
    pSp->spFlags &= ~CAPTURE_SP_FLAG_PREVLOST;
  }
  pSp->spFlags |= CAPTURE_SP_FLAG_STREAM_STARTED;

  return rc;
}

int streamadd_amr(CAPTURE_CBDATA_SP_T *pSp) {
  CAPTURE_FILTER_T *pFilter = (CAPTURE_FILTER_T *) pSp->pStream->pFilter;
  int rc = 0;

  if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
    // Nothign to be done, as (raw) output will be recorded to file
    return 0;
  }
  
  if(pFilter->u_seqhdrs.aud.common.clockHz == 0) {
    pFilter->u_seqhdrs.aud.common.clockHz = AMRNB_CLOCKHZ;
      LOG(X_WARNING("Using default input AMR clock rate %u"), 
          pFilter->u_seqhdrs.aud.common.clockHz);
  } else if(pFilter->u_seqhdrs.aud.common.clockHz != AMRNB_CLOCKHZ) {
    LOG(X_ERROR("AMR clock %uHz not supported"), pFilter->u_seqhdrs.aud.common.clockHz);
    return -1;
  }

  //pSp->pStream->clockHz = pFilter->u_seqhdrs.aud.common.clockHz;

  if(pFilter->u_seqhdrs.aud.u.amr.octet_align != 1) {
    LOG(X_ERROR("AMR octet-align=%u not supported"), 
        pFilter->u_seqhdrs.aud.u.amr.octet_align); 
    return -1;
  }

  LOG(X_DEBUG("Setup AMR RTP input %uHz 1 channel octet-align=%d"), 
        pFilter->u_seqhdrs.aud.common.clockHz, pFilter->u_seqhdrs.aud.u.amr.octet_align);

  return rc;
}


#endif // VSX_HAVE_CAPTURE
