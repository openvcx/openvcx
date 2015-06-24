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

static H264_NAL_T *getNextVclSlice(STREAM_H264_T *pStreamH264) {

  if(pStreamH264->loop && pStreamH264->idxNal >= pStreamH264->pH264->lastFrameId) {

    LOG(X_DEBUG("Looping H.264 stream"));
    pStreamH264->idxNal = 0;
  }
  //fprintf(stderr, "GETNEXTVCLSLICE loop:%d, idxNal [%d/%d], pSlice: 0x%x 0x%x\n", pStreamH264->loop, pStreamH264->idxNal, pStreamH264->pH264->lastFrameId, pStreamH264->frame.pSlice, pStreamH264->frame.pSlice ? pStreamH264->frame.pSlice->hdr : 0);

  if(pStreamH264->frame.pSlice && 
     ((H264_USE_DIRECT_READ(pStreamH264->pH264) && pStreamH264->frame.pSlice->content.pnext) ||
     //(((pStreamH264->pH264->pMkvCtxt || pStreamH264->pH264->pMp4Ctxt) && pStreamH264->frame.pSlice->content.pnext) ||
      (
       //pStreamH264->frame.pSlice == &pStreamH264->audSlice ||
       pStreamH264->frame.pSlice == &pStreamH264->spsSlice ||
       pStreamH264->frame.pSlice == &pStreamH264->ppsSlice)
     )) {

    pStreamH264->frame.pSlice = (H264_NAL_T *) pStreamH264->frame.pSlice->content.pnext;
    if(pStreamH264->idxNal == 0) {
      pStreamH264->idxNal++;
    }
    //fprintf(stderr, "RETURNING NEXT SLICE IN FR 0x%x\n", pStreamH264->frame.pSlice->hdr);
    return pStreamH264->frame.pSlice;
  }

  return  (H264_NAL_T *) h264_cbGetNextAnnexBSample(pStreamH264->pH264, 
                                                    pStreamH264->idxNal++, 0);
}

enum STREAM_NET_ADVFR_RC stream_net_h264_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg->pArgIn;
  H264_NAL_T *pNal = NULL;
  int haveISlice = 0;

  //fprintf(stdout, "h264 advanceFrame\n");

  if(pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 0;
  }

  pStreamH264->frame.szTot = 0;
  pStreamH264->frame.idxSlice = 0;

  if((pNal = getNextVclSlice(pStreamH264)) == NULL) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }
  pStreamH264->frame.idxInSlice = 0;
  pStreamH264->frame.frameId = pNal->content.frameId;
  pStreamH264->frame.pSlice = pNal;
  pStreamH264->frame.numSlices = 1;
  pStreamH264->frame.szTot = pNal->content.sizeRd;
  pStreamH264->frame.idxInAnnexBHdr = 0;

  if(pNal->sliceType == H264_SLICE_TYPE_I || 
     pNal->sliceType == H264_SLICE_TYPE_SI) {
    haveISlice = 1;
  } 

  if(pArg->pPts) {
    *pArg->pPts = (uint64_t) ((double)pNal->content.playOffsetHz *
          (double) (90000.0 / pStreamH264->pH264->clockHzOverride));
  }

  while(pNal->content.pnext) {

    if(h264_isNALVcl(pNal->hdr)) {
      if(pNal->content.frameId != pNal->content.pnext->frameId) {
        break;
      }

      if(!haveISlice &&
        (((H264_NAL_T *)pNal->content.pnext)->sliceType == H264_SLICE_TYPE_I || 
         ((H264_NAL_T *)pNal->content.pnext)->sliceType == H264_SLICE_TYPE_SI)) {
        haveISlice = 1;
      } 

      pStreamH264->frame.numSlices++;
      pStreamH264->frame.szTot += pNal->content.pnext->sizeRd;
    }

    pNal = (H264_NAL_T *) pNal->content.pnext;
  }
  //fprintf(stderr, "SLICE TYPE:%d\n", pNal->sliceType);

  // Avoid streaming the very last frame because it may have been truncated upon writing
  if(pNal) {

    pStreamH264->frame.pFileStream = pNal->content.pStreamIn;

    if(pStreamH264->includeSeqHdrs && haveISlice) {

      // Insert PPS slice
      memcpy(&pStreamH264->ppsSlice, pStreamH264->frame.pSlice, sizeof(H264_NAL_T));
      pStreamH264->ppsSlice.hdr = NAL_NRI_HI | NAL_TYPE_PIC_PARAM;
      pStreamH264->ppsSlice.sliceType = H264_SLICE_TYPE_PPS;
      pStreamH264->ppsSlice.content.pStreamIn = NULL;
      pStreamH264->ppsSlice.content.sizeRd = pStreamH264->pH264->spspps.pps_len;
      pStreamH264->ppsSlice.content.sizeWr = pStreamH264->ppsSlice.content.sizeRd;
      pStreamH264->ppsSlice.content.pnext = &pStreamH264->frame.pSlice->content;
      pStreamH264->ppsSlice.content.fileOffset = -1;
      pStreamH264->ppsSlice.content.cbWriteSample = NULL;
      pStreamH264->frame.szTot += pStreamH264->ppsSlice.content.sizeRd;
      pStreamH264->frame.numSlices++;

      // Insert SPS slice
      memcpy(&pStreamH264->spsSlice, pStreamH264->frame.pSlice, sizeof(H264_NAL_T));
      pStreamH264->spsSlice.hdr = NAL_NRI_HI | NAL_TYPE_SEQ_PARAM;
      pStreamH264->spsSlice.sliceType = H264_SLICE_TYPE_SPS;
      pStreamH264->spsSlice.content.pStreamIn = NULL;
      pStreamH264->spsSlice.content.sizeRd = pStreamH264->pH264->spspps.sps_len;
      pStreamH264->spsSlice.content.sizeWr = pStreamH264->spsSlice.content.sizeRd;
      pStreamH264->spsSlice.content.pnext = &pStreamH264->ppsSlice.content;
      pStreamH264->spsSlice.content.fileOffset = -1;
      pStreamH264->spsSlice.content.cbWriteSample = NULL;
      pStreamH264->frame.szTot += pStreamH264->spsSlice.content.sizeRd;
      pStreamH264->frame.numSlices++;

      pStreamH264->frame.pSlice = &pStreamH264->spsSlice;

      if(pArg->hdr.pspspps) {
        pArg->hdr.pspspps->sps = pStreamH264->pH264->spspps.sps;
        pArg->hdr.pspspps->sps_len = pStreamH264->pH264->spspps.sps_len;
        pArg->hdr.pspspps->pps = pStreamH264->pH264->spspps.pps;
        pArg->hdr.pspspps->pps_len = pStreamH264->pH264->spspps.pps_len;
      }

    }

/*
    if(pStreamH264->includeAUD) {

      // Insert NAL Access Unit Delimeter slice
      memcpy(&pStreamH264->audSlice, pStreamH264->frame.pSlice, sizeof(H264_NAL_T));
      pStreamH264->audSlice.hdr = NAL_NRI_NONE | NAL_TYPE_ACCESS_UNIT_DELIM;
      pStreamH264->audSlice.sliceType = H264_SLICE_TYPE_UNKNOWN;
      pStreamH264->audSlice.content.pStreamIn = NULL;
      pStreamH264->audSlice.content.sizeRd = 2;
      pStreamH264->audSlice.content.sizeWr = pStreamH264->audSlice.content.sizeRd;
      if(haveISlice) {
        pStreamH264->audSlice.content.pnext = &pStreamH264->spsSlice.content;
      } else {
        pStreamH264->audSlice.content.pnext = &pStreamH264->frame.pSlice->content;
      }
      pStreamH264->audSlice.content.fileOffset = -1;
      pStreamH264->audSlice.content.cbWriteSample = NULL;
      pStreamH264->frame.szTot += pStreamH264->audSlice.content.sizeRd;
      pStreamH264->frame.numSlices++;

      pStreamH264->frame.pSlice = &pStreamH264->audSlice;
    }
*/

  } else {
    pStreamH264->frame.pFileStream = NULL;
  }

  if(pArg->plen) {
    *pArg->plen = pStreamH264->frame.szTot;
    if(pStreamH264->includeAnnexBHdr) {
      *pArg->plen += (4 * pStreamH264->frame.numSlices);
    }

  }

  if(haveISlice && pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 1;
  }

  pArg->isvid = 1;

  return STREAM_NET_ADVFR_RC_OK;
}

static H264_NAL_T *getFrameSlice(STREAM_H264_T *pStreamH264) {

  //fprintf(stderr, "GET FRAME SLICE %d/%d\n", pStreamH264->frame.idxSlice, pStreamH264->frame.numSlices);

  if(pStreamH264->frame.idxSlice >= pStreamH264->frame.numSlices) {
    return NULL;
  } else if(pStreamH264->frame.idxSlice > 0) {

    if((pStreamH264->frame.pSlice = getNextVclSlice(pStreamH264))) {
      if(pStreamH264->frame.pSlice->content.frameId != pStreamH264->frame.frameId) {
        // never supposed to get here
        LOG(X_WARNING("slice frame id %u does not match current frame %llu, slice %d/%d"), 
            pStreamH264->frame.pSlice->content.frameId, pStreamH264->frame.frameId, 
            pStreamH264->frame.idxSlice,  pStreamH264->frame.numSlices);
        pStreamH264->frame.pSlice = NULL;
      }
    }
  } 
  
  pStreamH264->frame.idxSlice++;
  pStreamH264->frame.idxInSlice = 0;
  pStreamH264->frame.idxInAnnexBHdr = 0;
  //fprintf(stderr, "FRAME SLICE RETURNING sltype: 0x%x, fridx:%d\n", pStreamH264->frame.pSlice ? pStreamH264->frame.pSlice->hdr : 0, pStreamH264->frame.pSlice->frameIdx);
  return pStreamH264->frame.pSlice;
}

static unsigned int stream_net_h264_getNextSliceSz(STREAM_H264_T *pStreamH264) {
  H264_NAL_T *pNal = NULL;
  uint32_t szSlice;

  if(pStreamH264->frame.pSlice) {
    pNal = (H264_NAL_T *) pStreamH264->frame.pSlice->content.pnext;
  }

  if(pNal) {

    if(pNal->content.frameId != pStreamH264->frame.frameId) {
      return 0;
    }

    szSlice = pNal->content.sizeRd;
    if(pStreamH264->includeAnnexBHdr) {
      szSlice += 4;
    }
    return szSlice;

  }

  return 0;
}

int64_t stream_net_h264_frameDecodeHzOffset(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;
//fprintf(stderr, "DTS: (slice:0x%x), %d, %d/%dHz\n", pStreamH264->frame.pSlice, pStreamH264->frame.pSlice->content.decodeOffset,pStreamH264->pH264->frameDeltaHz , pStreamH264->pH264->clockHz);
  if(pStreamH264->frame.pSlice && pStreamH264->frame.pSlice->content.decodeOffset != 0) {
    return (int64_t)  (pStreamH264->frame.pSlice->content.decodeOffset * 90000 *
             ((double)pStreamH264->pH264->frameDeltaHz / (double)pStreamH264->pH264->clockHz));
  }
  return 0;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_h264_getNextSlice(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;
  H264_NAL_T *pNal = NULL;

  //fprintf(stderr, "h264_getNextSlice\n");

  pNal = getFrameSlice(pStreamH264);
  if(pNal) {
    memcpy(&pStreamH264->nal, pNal, sizeof(H264_NAL_T));
    pNal = &pStreamH264->nal;
    pNal->content.sizeWr = pNal->content.sizeRd;
    if(pStreamH264->includeAnnexBHdr) {
      // TODO: this is bad to modify the h264 owned content list directly
      pNal->content.sizeWr = pNal->content.sizeRd + 4;
    } 
  }

  return (MP4_MDAT_CONTENT_NODE_T *) pNal;
}

int stream_net_h264_szNextSlice(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;

  return stream_net_h264_getNextSliceSz(pStreamH264);
}

int stream_net_h264_haveMoreData(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;
  uint32_t szSlice;


  if(pStreamH264->loop ||
     pStreamH264->idxNal < pStreamH264->pH264->numNalsVcl) {

    return 1;

  } else if(pStreamH264->frame.pSlice) {

    szSlice = pStreamH264->frame.pSlice->content.sizeRd;
    if(pStreamH264->includeAnnexBHdr) {
      szSlice += 4;
    }

    if(pStreamH264->frame.idxInSlice < szSlice) {
      return 1;
    }
  }

  return 0;
}


int stream_net_h264_haveNextSlice(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;

  if(pStreamH264->frame.pSlice &&
     pStreamH264->frame.idxSlice < pStreamH264->frame.numSlices) {

    return (int) (pStreamH264->frame.numSlices - pStreamH264->frame.idxSlice);
  }
  return 0;
}


int stream_net_h264_getReadSliceBytes(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;

  return pStreamH264->frame.idxInSlice;
}


int stream_net_h264_getUnreadSliceBytes(void *pArg) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;
  uint32_t szSlice;

  if(pStreamH264->frame.pSlice == NULL) {
    return 0;
  }

  szSlice = pStreamH264->frame.pSlice->content.sizeRd;
  if(pStreamH264->includeAnnexBHdr) {
    szSlice += 4;
  }

  return szSlice - pStreamH264->frame.idxInSlice;
}

int stream_net_h264_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_H264_T *pStreamH264 = (STREAM_H264_T *) pArg;
  uint32_t szSlice;
  unsigned int lenToRead = len;
  unsigned int bufIdx = 0;
  unsigned int fsIdxInSlice;

  if(pStreamH264->frame.pSlice == NULL) {
    return 0;
  }

  fsIdxInSlice = pStreamH264->frame.idxInSlice;
  szSlice = pStreamH264->frame.pSlice->content.sizeWr;
  if(pStreamH264->includeAnnexBHdr) {
    szSlice += 4;
  }

  if(len > szSlice - pStreamH264->frame.idxInSlice) {
    LOG(X_WARNING("Decreasing copy length from %u to (%u - %u)"), 
                  len, szSlice, pStreamH264->frame.idxInSlice);
    len = szSlice - pStreamH264->frame.idxInSlice;
  }

  if(pStreamH264->includeAnnexBHdr) {

    if(pStreamH264->frame.idxInSlice < 4) {

      while(pStreamH264->frame.idxInAnnexBHdr < 4 && bufIdx < len) {
        pBuf[bufIdx++] = (pStreamH264->frame.idxInAnnexBHdr == 3 ? 0x01 : 0x00);
        pStreamH264->frame.idxInAnnexBHdr++;
        lenToRead--;
        pStreamH264->frame.idxInSlice++;
      }
    
    }
    if(pStreamH264->frame.idxInSlice < 4) {
      return pStreamH264->frame.idxInSlice;
    } else {
      fsIdxInSlice = pStreamH264->frame.idxInSlice - 4;
    }

  }

/*
  if(pStreamH264->frame.pSlice == &pStreamH264->audSlice) {

    if(fsIdxInSlice + lenToRead > 2) {
      LOG(X_ERROR("Attempt to read AUD[%d] + %d > AUD len: 2\n"),  
                  fsIdxInSlice, lenToRead, 2);
      return -1;
    }

    pBuf[bufIdx] =  pStreamH264->audSlice.hdr;

    //
    // Mpeg4-Part10AVC Table 7-2 "Meaning of primary_pic_type"
    // 3 bits 0 - 7 ((7 << 5) = I,Si,P,SP,B possible slice types in primary coded picture)
    //
    pBuf[bufIdx + 1] = H264_AUD_I_SI_P_SP_B; 

  } else 
*/
  if(pStreamH264->frame.pSlice == &pStreamH264->spsSlice) {

    if(fsIdxInSlice + lenToRead > pStreamH264->pH264->spspps.sps_len) {
      LOG(X_ERROR("Attempt to read sps[%d] + %d > sps len: %d\n"),  
        fsIdxInSlice, lenToRead, pStreamH264->pH264->spspps.sps_len);
      return -1;
    }

    memcpy(&pBuf[bufIdx], &pStreamH264->pH264->spspps.sps[fsIdxInSlice], lenToRead);

  } else if(pStreamH264->frame.pSlice == &pStreamH264->ppsSlice) {

    if(fsIdxInSlice + lenToRead > pStreamH264->pH264->spspps.pps_len) {
      LOG(X_ERROR("Attempt to read pps[%d] + %d > pps len: %d\n"),  
        fsIdxInSlice, lenToRead, pStreamH264->pH264->spspps.pps_len);
      return -1;
    }

    memcpy(&pBuf[bufIdx], &pStreamH264->pH264->spspps.pps[fsIdxInSlice], lenToRead);


  } else {

    if(SeekMediaFile(pStreamH264->frame.pFileStream, 
                      pStreamH264->frame.pSlice->content.fileOffset + 
                      fsIdxInSlice, SEEK_SET) != 0) {
      return -1;
    }

    if(ReadFileStream(pStreamH264->frame.pFileStream, &pBuf[bufIdx], lenToRead) < 0) {
      return -1;
    }

  }

//if(pStreamH264->frame.idxInSlice <= 4) {
//fprintf(stderr, "h264 frameId:%d\n", pStreamH264->frame.frameId);
//if(pStreamH264->frame.pSlice->content.sizeWr > 65530) fprintf(stderr, "\n\n\n\nh264 size:%d\n", pStreamH264->frame.pSlice->content.sizeWr); 
//fprintf(stderr, "h264 size:%d\n", pStreamH264->frame.pSlice->content.sizeWr); 
//avc_dumpHex(stderr, pBuf, 16, 1);
//}

  pStreamH264->frame.idxInSlice += lenToRead;

  return len;
}

float stream_net_h264_jump(STREAM_H264_T *pStreamH264, float fStartSec, int syncframe) {

  uint64_t desiredOffset = 0;
  H264_NAL_T *pNal = NULL;
  float fOffsetActual;

  if(!pStreamH264 || fStartSec < 0) {
    return -1;
  }

  desiredOffset = (uint64_t) (fStartSec * pStreamH264->pH264->clockHzOverride);

  pStreamH264->idxNal = 0;
  pNal = (H264_NAL_T *) h264_cbGetNextAnnexBSample(
                               pStreamH264->pH264, pStreamH264->idxNal++, -1);

  while(pNal && pNal->content.playOffsetHz + pNal->content.playDurationHz <
        desiredOffset) {

    pNal = (H264_NAL_T *) h264_cbGetNextAnnexBSample(
                               pStreamH264->pH264, pStreamH264->idxNal++, -1);
  }

  if(pNal) {

    if(syncframe) {
      h264_moveToPriorSync(pStreamH264->pH264);
    }
    pNal = (H264_NAL_T *) h264_cbGetNextAnnexBSample(
                             pStreamH264->pH264, pStreamH264->idxNal++, 1);
  }

  if(!pNal) {
    LOG(X_WARNING("h.264 stream jump to %.2f out of range %.2f (%u)"), 
                  fStartSec, (double)pStreamH264->pH264->lastPlayOffsetHz /
                  pStreamH264->pH264->clockHzOverride, 
                  pStreamH264->pH264->pNalsLastVcl->content.frameId);
    return -1;
  }

  fOffsetActual = (float) ((double)pNal->content.playOffsetHz /
                                 pStreamH264->pH264->clockHzOverride);

  pStreamH264->frame.numSlices = 1;
  pStreamH264->frame.szTot = pNal->content.sizeRd;
  pStreamH264->frame.frameId = pNal->content.frameId;
  pStreamH264->frame.pFileStream = pNal->content.pStreamIn;
  pStreamH264->frame.idxInSlice = pStreamH264->frame.szTot;
  pStreamH264->frame.idxSlice = 1;
  pStreamH264->frame.pSlice = pNal;

  if(pStreamH264->includeAnnexBHdr) {
    pStreamH264->frame.idxInSlice += 4;
  }

  LOG(X_DEBUG("h.264 stream playout moved to frame %d actual:%.1fs (%.1fs) file:0x%llx"), 
       pNal->content.frameId, fOffsetActual, fStartSec, pNal->content.fileOffset);

  return fOffsetActual;
}



void stream_net_h264_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_FILE;
  pNet->cbAdvanceFrame = stream_net_h264_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_h264_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_h264_getNextSlice;
  pNet->cbHaveMoreData = stream_net_h264_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_h264_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_h264_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_h264_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_h264_getReadSliceBytes;
  pNet->cbCopyData = stream_net_h264_getSliceBytes;
}


#endif // VSX_HAVE_STREAMER
