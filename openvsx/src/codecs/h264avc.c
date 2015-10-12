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


#define H264_AVC_STREAM_PREBUFSZ            64
#define H264_AVC_STREAM_BUFSZ               4032


int h264_escapeRawByteStream(const BIT_STREAM_T *pIn, BIT_STREAM_T *pOut) {
  BIT_STREAM_T in;
  BIT_STREAM_T *pB = &in;
  int numEsc = 0;

  if(!pIn) {
    return -1;
  }
 
  memcpy(&in, pIn, sizeof(BIT_STREAM_T));
  in.bitIdx = 0;
  if(pOut) {
    pOut->bitIdx = 0;
    pOut->byteIdx = 0;
    pB = pOut;
  }

  for(in.byteIdx = 0; in.byteIdx < pIn->byteIdx; in.byteIdx++) {

    if(pB->byteIdx >= 2 &&
       pB->buf[pB->byteIdx - 2] == 0x00 && pB->buf[pB->byteIdx - 1] == 0x00 &&
       in.buf[in.byteIdx] <= 0x03) {
    
      numEsc++;
      if(pOut) {
        if(pOut->byteIdx >= pOut->sz) {
          return -1;
        }
        pOut->buf[pOut->byteIdx++] = 0x03;
      }
    } 

    if(pOut) {
      if(pOut->byteIdx >= pOut->sz) {
        return -1;
      }
      pOut->buf[pOut->byteIdx++] = in.buf[in.byteIdx]; 
    }
    
  }

  return numEsc;
}

int h264_findStartCode(H264_STREAM_CHUNK_T *pStream, int handle_short_code, int *pstartCodeLen) {
  
  unsigned char *buf = &pStream->bs.buf[pStream->bs.byteIdx];
  unsigned int size = pStream->bs.sz - pStream->bs.byteIdx;
  unsigned int idx;

  for(idx = 0; idx < size; idx++) {

    if(pStream->startCodeMatch < 3 && buf[idx] == 0x00) {
      pStream->startCodeMatch++;

    } else if((pStream->startCodeMatch >= 3 && buf[idx] < 0x03) ||
               (handle_short_code && pStream->startCodeMatch >= 2 && buf[idx] == 0x01)) {

      if(buf[idx] > 0x00) {
        idx++;
        if(pstartCodeLen) {
          *pstartCodeLen = pStream->startCodeMatch + 1;
        }
        pStream->startCodeMatch = 0;
        return idx;
      }

    } else if(pStream->startCodeMatch > 0) {
      pStream->startCodeMatch = 0;
    }
  }

  return -1;
}

const char *h264_getPrintableProfileStr(uint8_t profile_idc) {

  // SPS profile_idc

  switch(profile_idc) {
    case H264_PROFILE_BASELINE:
      return "Baseline";
    case H264_PROFILE_MAIN:
      return "Main";
    case H264_PROFILE_EXT:
      return "Extended";
    case H264_PROFILE_HIGH:
      return "High";
    case H264_PROFILE_HIGH_10:
      return "High 10";
    case H264_PROFILE_HIGH_422:
      return "High 4:2:2";
    case H264_PROFILE_HIGH_444:
      return "High 4:4:4";
    case H264_PROFILE_HIGH_444_PRED:
      return "High 4:4:4 Predictive";
    case H264_PROFILE_CAVLC_444:
      return "CAVLC 4:4:4";
  }
  return "unknown";
}

static int updatePriorDecodeOffsets(H264_AVC_DESCR_T *pH264) {

  H264_NAL_T *pNal = pH264->pNalsLastSliceI;
  H264_NAL_T *pNalsLastSlicePocRef = pH264->pNalsLastSlicePocRef;

  if(!pNal || !pNalsLastSlicePocRef) {
    return -1;
  }
  //fprintf(stderr, "UPDATEPRIORDECODEOFFSETS from I frid:%d\n", pNal->content.frameId);  
  while(pNal) {
    pNal->content.decodeOffset = -1 *((int)(pNal->content.frameId -
                               pNalsLastSlicePocRef->content.frameId) -
                              (pNal->picOrderCnt / pH264->pocDenum));
  //fprintf(stderr, "UPDATEPRIORDECODEOFFSETS from I frid:%d, decodeOffset:%d (frameId:%d, lastpocref.frameId:%d, poc:%d, pocdenum:%d)\n", pNal->content.frameId, pNal->content.decodeOffset, pNal->content.frameId, pNalsLastSlicePocRef->content.frameId, pNal->picOrderCnt, pH264->pocDenum);  
    pNal = (H264_NAL_T *) pNal->content.pnext;
  }

  return 0;
}

static int attachVclNal(H264_AVC_DESCR_T *pH264,  H264_NAL_T *pNalCopy) {
  H264_NAL_T *pNal = NULL;
  H264_NAL_T *pNalTmp;
  int frameDelta = 0;
  int poc;
#if defined(FRAME_POC_HEURISTICS_TYPE1) 
  double pocDeltaTmp;
#endif // (FRAME_POC_HEURISTICS_TYPE1) 

  if(pH264->numNalsVcl >= pH264->nalsBufSz) {
    LOG(X_ERROR("number of VCL NALs: %u exceeds allocated size: %u"), 
          pH264->numNalsVcl, pH264->nalsBufSz);
    return -1;
  }  

  if(pNalCopy->sliceType == H264_SLICE_TYPE_B) {
    pH264->haveBSlices = 1;
  }
  pNal = &pH264->pNalsBuf[pH264->numNalsVcl];


  if(h264_getNALType(pNalCopy->hdr) == NAL_TYPE_SEI) {

    if(pH264->pNalsLastSEI == NULL) {
      pH264->pNalsLastSEI = pNal;
    }

  } else {

    if(pNalCopy->picOrderCnt > pH264->highestPoc) {
      pH264->highestPoc = pNalCopy->picOrderCnt;
    }

    if(pNalCopy->sliceType == H264_SLICE_TYPE_I || 
       pNalCopy->sliceType == H264_SLICE_TYPE_SI) {

      pH264->pNalsLastSliceIPrev = pH264->pNalsLastSliceI;
      pH264->pNalsLastSliceI = pNal;

      if(pNalCopy->picOrderCnt == 0) {
        pH264->pNalsLastSlicePocRef = pH264->pNalsLastSliceI;
        if(pH264->pocDenum > 0 && pH264->pocDeltaSamples > 0 && !pH264->validExpectedPoc) {
          pH264->validExpectedPoc = 1;
        }
      }

      if(pH264->validExpectedPoc) {
        pH264->expectedPoc = pNalCopy->picOrderCnt;
      }

    } else if(pH264->pNalsLastSlice) {

      if(pH264->validExpectedPoc) {
        if((pH264->expectedPoc += pH264->pocDenum) > pH264->highestPoc) {
          pH264->expectedPoc = 0;
        }
      }

      //
      // temporary - certain bitstreams reset POC ref at arbitrary non-I slices 
      //
      if(pNalCopy->picOrderCnt <= 4 && pH264->pNalsLastSlice->picOrderCnt > 10 &&
         pH264->pNalsLastSlicePocRef && 
         pH264->pNalsLastSlicePocRef->content.frameId + 2 < pH264->pNalsLastSlice->content.frameId ) {
        //LOG(X_DEBUG("NAL frame: %u, POC: %d, prior: %d - resetting last slice POC reference"), 
        //    pH264->pNalsLastSlice->content.frameId + 1, pNalCopy->picOrderCnt, pH264->pNalsLastSlice->picOrderCnt);

//fprintf(stderr, "frameId of last poc:%d, last slice:%d\n", pH264->pNalsLastSlicePocRef->content.frameId , pH264->pNalsLastSlice->content.frameId);
      //if(pNalCopy->picOrderCnt == 0) {
        pH264->pNalsLastSlicePocRef = pNal;
        pH264->hasPocWrapOnNonI = 1;
      }

    }

    if(pH264->pNalsLastSlice) {

      if(pH264->ctxt.sps[pH264->ctxt.sps_idx].is_ok &&
        pH264->haveBSlices) {

        // This does not take care of lost frames
        if(pH264->pNalsLastSlice->picOrderCnt != pNalCopy->picOrderCnt ||
          pH264->pNalsLastSlice->frameIdx != pNalCopy->frameIdx) {
          frameDelta = 1;
        }

      } else {

        if(pH264->pNalsLastSlice->frameIdx != pNalCopy->frameIdx ||
          pH264->pNalsLastSlice->picOrderCnt != pNalCopy->picOrderCnt) {

          frameDelta = 1;
          if(pH264->pNalsLastSlice->frameIdx + 1 < pNalCopy->frameIdx ||
            (pH264->pNalsLastSlice->frameIdx > pNalCopy->frameIdx && 
             pNalCopy->frameIdx != 0)) {

              LOG(X_WARNING("Frame skip from idx:%d to %d (frame:%d,%d)"), 
                       pH264->pNalsLastSlice->frameIdx, pNalCopy->frameIdx,
                       pH264->pNalsLastSlice->content.frameId, pNalCopy->content.frameId);      
          } 
        }
      }
    }

    if(frameDelta != 0) {
      pNal->content.frameId = pH264->pNalsLastSlice->content.frameId + frameDelta;   
    } else if(pH264->pNalsLastSlice) {
      pNal->content.frameId = pH264->pNalsLastSlice->content.frameId;
    }
    pNal->content.playDurationHz = pNalCopy->content.playDurationHz;
    pNal->content.playOffsetHz = pH264->lastPlayOffsetHz;

#if defined(FRAME_POC_HEURISTICS_TYPE1) 
#define FRAME_POC_HEURISTICS_CNT 2
#else // (FRAME_POC_HEURISTICS_TYPE1) 
#define FRAME_POC_HEURISTICS_CNT 3
#endif // (FRAME_POC_HEURISTICS_TYPE1) 

    //
    // auto-learn poc delta between frames
    //
    if(pH264->pocDeltaSamples < FRAME_POC_HEURISTICS_CNT) {

      if(pNalCopy->picOrderCnt % 2 == 1) {
        pH264->pocDeltaSamples = FRAME_POC_HEURISTICS_CNT;
        pH264->pocDeltaMin = 1;
      } else
      if(pNalCopy->sliceType == H264_SLICE_TYPE_P &&
         pH264->pNalsLastSliceP &&
         pNalCopy->picOrderCnt > pH264->pNalsLastSliceP->picOrderCnt) {

#if defined(FRAME_POC_HEURISTICS_TYPE1) 

       pocDeltaTmp =  (pNalCopy->picOrderCnt - 
                       pH264->pNalsLastSliceP->picOrderCnt)/
                       (pNal->content.frameId - 
                        pH264->pNalsLastSliceP->content.frameId));

      if(pocDeltaTmp != pH264->pocDeltaTmp) {

        if(pocDeltaTmp > 0) {
          pH264->pocDeltaMin = pocDeltaTmp;
        }

        pH264->pocDeltaTmp = pocDeltaTmp;
        pH264->pocDeltaSamples = 1;
      } else {
        pH264->pocDeltaSamples++;
      }
#else // (FRAME_POC_HEURISTICS_TYPE1) 

       if(pNalCopy->picOrderCnt % 2 == 1) {
         if(++pH264->pocDeltaSamplesOdd >= FRAME_POC_HEURISTICS_CNT) {
           pH264->pocDeltaSamples = FRAME_POC_HEURISTICS_CNT;
           pH264->pocDeltaMin = 1;
         }
       } else {
         if(++pH264->pocDeltaSamplesEven >= FRAME_POC_HEURISTICS_CNT * 2) {
           pH264->pocDeltaSamples = FRAME_POC_HEURISTICS_CNT;
           pH264->pocDeltaMin = 2;
         }
       }

#endif // (FRAME_POC_HEURISTICS_TYPE1) 

      }

      if(pH264->pocDeltaSamples == FRAME_POC_HEURISTICS_CNT) {
        pH264->pocDenum = (int)(pH264->pocDeltaMin);
        updatePriorDecodeOffsets(pH264);
      }

    }

    if(pH264->haveBSlices && pH264->pNalsLastSlicePocRef) {
      if(pH264->pocDenum == 0) {

        LOG(X_WARNING("H.264 computed poc denum is zero.")); 
        pNal->content.decodeOffset = 0;

      } else if(pH264->pocDeltaSamples >= FRAME_POC_HEURISTICS_CNT &&
               pNalCopy->sliceType != H264_SLICE_TYPE_I && 
               pNalCopy->sliceType != H264_SLICE_TYPE_SI) {

        if(1&&pH264->hasPocWrapOnNonI && pH264->validExpectedPoc) {

          if(pH264->validExpectedPoc++ == 1) {
            LOG(X_WARNING("Using non-preferred NAL expected DTS timing"));
          }

          if(pH264->expectedPoc + 4 >= pH264->highestPoc && pNalCopy->picOrderCnt <= 4) {
            poc = pH264->highestPoc + pNalCopy->picOrderCnt + pH264->pocDenum;   
          } else if(pNalCopy->picOrderCnt + 4 >= pH264->highestPoc && pH264->expectedPoc <= 4) {
            poc = pNalCopy->picOrderCnt - pH264->highestPoc - pH264->pocDenum;   
          } else {
            poc = pNalCopy->picOrderCnt;
          }
          pNal->content.decodeOffset = (poc - pH264->expectedPoc) / pH264->pocDenum ;


        } else {
          pNal->content.decodeOffset = -1 *((int)(pNal->content.frameId - 
                               pH264->pNalsLastSlicePocRef->content.frameId) -
                              (pNalCopy->picOrderCnt / pH264->pocDenum));
        }
        //fprintf(stderr, "DECODE OFFSET (method:%s) :%d, poc:%d, frid:%d poc_expected:%d, highest:%d, pocDenum:%d\n", (pH264->hasPocWrapOnNonI && pH264->validExpectedPoc) ? "expected" : "offset from frameId", pNal->content.decodeOffset, pNalCopy->picOrderCnt, pNal->content.frameId, pH264->expectedPoc, pH264->highestPoc, pH264->pocDenum);

        if(abs(pNal->content.decodeOffset) > 10) {

          //if(pH264->hasPocWrapOnNonI && pNalCopy->sliceType == H264_SLICE_TYPE_B &&
          //   pNal->content.decodeOffset > 10 && pH264->pNalsLastSlice->content.decodeOffset <= 4) {
          //  fprintf(stderr, "SHOULD NEVER GET HERE\n");
          //  pNal->content.decodeOffset = -1;
          //} else {

            LOG(X_WARNING("Bogus nal decode offset:%d at frame: %u, ref: %d, poc:%d/%d"), 
               pNal->content.decodeOffset, pNal->content.frameId, pH264->pNalsLastSlicePocRef->content.frameId,
               pNalCopy->picOrderCnt,pH264->pocDenum);
            pNal->content.decodeOffset = 0;

          //}
        }
      }

    } else {
      pNal->content.decodeOffset = 0;
    }

    //fprintf(stderr, "frameId:%u lastPOC:%u, POC:%d/%d (offset:%d)\n", pNal->content.frameId, pH264->pNalsLastSlicePocRef->content.frameId, pNalCopy->picOrderCnt, pH264->pocDenum, pNal->content.decodeOffset);


    // Update frameId of previous SEI(s) to that of current slice
    if(pH264->pNalsLastSEI) {
      pNalTmp = pH264->pNalsLastSEI;
      while(pNalTmp) {
        pNalTmp->content.frameId = pNal->content.frameId;
        pNalTmp->content.playOffsetHz = pNal->content.playOffsetHz;
        pNalTmp->content.playDurationHz = pNal->content.playDurationHz;
        pNalTmp->content.flags = pNalCopy->content.flags;
        pNalTmp->content.decodeOffset = pNal->content.decodeOffset;
        pNalTmp->frameIdx = pNalCopy->frameIdx;
        pNalTmp->picOrderCnt = pNalCopy->picOrderCnt;
        pNalTmp = (H264_NAL_T *) pNalTmp->content.pnext;
      }
      pH264->pNalsLastSEI = NULL;
    }

    pH264->pNalsLastSlice = pNal; // excludes SEI nals

    if(pNalCopy->sliceType == H264_SLICE_TYPE_P) {
      pH264->pNalsLastSliceP = pNal;
    }

  } // end of non SEI

  pNal->content.pStreamIn = pNalCopy->content.pStreamIn;
  pNal->content.fileOffset = pNalCopy->content.fileOffset;
  pNal->content.sizeRd = pNalCopy->content.sizeRd;
  pNal->content.flags = pNalCopy->content.flags;
  pNal->content.sizeWr = pNalCopy->content.sizeRd;
  pNal->content.cbWriteSample = h264_write_nal_to_mp4;
  pNal->content.chunk_idx = pNalCopy->content.chunk_idx;
  pNal->content.pnext = NULL;
  pNal->frameIdx = pNalCopy->frameIdx;
  pNal->picOrderCnt = pNalCopy->picOrderCnt;
  pNal->hdr = pNalCopy->hdr;
  pNal->sliceType = pNalCopy->sliceType;
  pNal->paramSetChanged = pNalCopy->paramSetChanged;

  if(pH264->pNalsVcl == NULL) {
    pH264->pNalsVcl = pNal;
  } else {
    pH264->pNalsLastVcl->content.pnext = (MP4_MDAT_CONTENT_NODE_T *) pNal;
  }

  pH264->numNalsVcl++;
  pH264->pNalsLastVcl = pNal;
  pH264->lastFrameId = pNal->content.frameId;
  
  if(H264_USE_DIRECT_READ(pH264)) {
    if(pH264->pNalsLastSliceI == pNal) {
      memcpy(&pH264->nalLastSliceI, pNal, sizeof(H264_NAL_T));
      pH264->pNalsLastSliceI = &pH264->nalLastSliceI;

      // GDR may result in I frames not on POC 0
      if(pNal->picOrderCnt == 0) {
        memcpy(&pH264->nalLastSlicePocRef, pNal, sizeof(H264_NAL_T));
        pH264->pNalsLastSlicePocRef = &pH264->nalLastSlicePocRef;
      }
    }
    if(pH264->pNalsLastSliceP == pNal) {
       memcpy(&pH264->nalLastSliceP, pNal, sizeof(H264_NAL_T));
       pH264->pNalsLastSliceP = &pH264->nalLastSliceP;
    }
    if(pH264->pNalsLastSlice == pNal) {
      memcpy(&pH264->nalLastSlice, pNal, sizeof(H264_NAL_T));
      pH264->pNalsLastSlice = &pH264->nalLastSlice;
    }
  }

  //fprintf(stderr, "attachVclNal sz:%d frameRdIdx:%d slice:0x%x hdr:0x%x\n", pNal->content.sizeRd, pNal->frameIdx,pNal->sliceType, pNal->hdr);
  return 1;
}

static int attachNal(H264_AVC_DESCR_T *pH264,  H264_NAL_T *pNalCopy) {
  int rc = 0;
  unsigned int frameWidthPx;
  unsigned int frameHeightPx;
  unsigned int clockHz;
  unsigned int frameDeltaHz;
  int nalType = h264_getNALType(pNalCopy->hdr);

  pH264->numNalsTot++;

  if(pNalCopy->paramSetChanged) {
    LOG(X_WARNING("Detected change in H.264 parameter set values"));
     //fprintf(stderr, "RESOLUTION: %dx%d\n", pH264->frameWidthPx, pH264->frameHeightPx);
    //return -1;
  }

  switch(nalType) {
    case NAL_TYPE_SEQ_PARAM:

      //frameWidthPx = pH264->frameWidthPx;
      //frameHeightPx = pH264->frameHeightPx;

      if(pH264->spspps.sps_len == 0 || pNalCopy->paramSetChanged) {
        // init to default
        pH264->pocDenum = 1;
        pH264->pocDeltaMin = 0;
        pH264->pocDeltaSamples = 0;
        pH264->pocDeltaSamplesOdd = 0;
        pH264->pocDeltaSamplesEven = 0;
        pH264->pocDeltaTmp = 0;
      } 

      if((pH264->spspps.sps_len = pNalCopy->content.sizeRd) > sizeof(pH264->spspps.sps_buf)) {
        pH264->spspps.sps_len = sizeof(pH264->spspps.sps_buf);
      } 

      if(h264_getCroppedDimensions(&pH264->ctxt.sps[pH264->ctxt.sps_idx],
                                   &pH264->frameWidthPx, &pH264->frameHeightPx,
                                   &frameWidthPx, &frameHeightPx) > 0) {
        LOG(X_DEBUG("SPS dimensions cropped from %dx%d to %dx%d"), 
                    frameWidthPx, frameHeightPx,
                    pH264->frameWidthPx, pH264->frameHeightPx);
      }
      if(pNalCopy->paramSetChanged) {
        LOG(X_WARNING("New H.264 resolution %dx%d"), pH264->frameWidthPx, pH264->frameHeightPx);
      }

      if(h264_getVUITiming(&pH264->ctxt.sps[pH264->ctxt.sps_idx], 
                           &clockHz, &frameDeltaHz) == 0) {
      //if(pH264->ctxt.sps[pH264->ctxt.sps_idx].vui.timing_info_present) {
      //  clockHz = pH264->ctxt.sps[pH264->ctxt.sps_idx].vui.timing_timescale / 2;
      //  frameDeltaHz = pH264->ctxt.sps[pH264->ctxt.sps_idx].vui.timing_num_units_in_tick;

        if(pH264->clockHz != 0 && pH264->frameDeltaHz != 0) {
          if(pH264->clockHzVUI != clockHz || 
             pH264->frameDeltaHzVUI != frameDeltaHz) {
            LOG(X_INFO("Ignoring SPS timing: %u Hz, tick %u Hz (%.3f fps)"),
              clockHz, frameDeltaHz, ((float)clockHz/ frameDeltaHz));
          }
        } else if(clockHz != pH264->clockHz ||
                  frameDeltaHz != pH264->frameDeltaHz) {
          pH264->clockHz = clockHz;
          pH264->frameDeltaHz = frameDeltaHz;
          LOG(X_INFO("Found SPS timing: %u Hz, tick %u Hz (%.3f fps)"),
            pH264->clockHz, pH264->frameDeltaHz, 
            ((float)pH264->clockHz / pH264->frameDeltaHz));
        }
        pH264->clockHzVUI = clockHz;
        pH264->frameDeltaHzVUI = frameDeltaHz;
      }

      break;
    case NAL_TYPE_PIC_PARAM:
      if((pH264->spspps.pps_len = pNalCopy->content.sizeRd) > 
         sizeof(pH264->spspps.pps_buf)) {
        pH264->spspps.pps_len = sizeof(pH264->spspps.pps_buf);
      } 
      break;
    case NAL_TYPE_IDR:
      pNalCopy->content.flags |= MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE;
      break;
  }

  if(h264_isNALVcl(pNalCopy->hdr)) {
    rc = attachVclNal(pH264, pNalCopy);
  }

  return rc;
}

void h264_free(H264_AVC_DESCR_T *pH264) {
  
  pH264->pNalsLastSEI = NULL;
  pH264->pNalsLastVcl = NULL;
  pH264->pNalsLastSlice = NULL;
  pH264->pNalsLastSliceP = NULL;
  pH264->pNalsLastSliceI = NULL;
  pH264->pNalsLastSliceIPrev = NULL;
  pH264->pNalsLastSlicePocRef = NULL;
  pH264->pNalsVcl = NULL;
  
  if(pH264->pNalsBuf) {
    avc_free((void **) &pH264->pNalsBuf);
  }
  pH264->nalsBufSz = 0;

  if(pH264->pMp4Ctxt) {
    avc_free((void **) &pH264->pMp4Ctxt);
  }
  if(pH264->pMkvCtxt) {
    avc_free((void **) &pH264->pMkvCtxt);
  }

}

static H264_RESULT_T decode_NALHdr(H264_AVC_DESCR_T *pH264, H264_STREAM_CHUNK_T *pStream,
                         unsigned int offsetFromStart, H264_NAL_T *pNal) {
  H264_RESULT_T rc;
  H264_NAL_SPS_T sps;
  H264_NAL_PPS_T pps;
  unsigned int sps_idx = 0;
  unsigned int pps_idx = 0;
  unsigned int bufSz = pStream->bs.sz;
  unsigned char *pBufStart = &pStream->bs.buf[pStream->bs.byteIdx];
  int nalType = h264_getNALType(pStream->bs.buf[pStream->bs.byteIdx]);
  unsigned char nalHdr = pStream->bs.buf[pStream->bs.byteIdx];

  if(offsetFromStart == 0) {
    pNal->paramSetChanged = 0;    
    switch(nalType) {
      case NAL_TYPE_SEQ_PARAM:
        sps_idx = pH264->ctxt.sps_idx;
        memcpy(&sps, &pH264->ctxt.sps[pH264->ctxt.sps_idx], sizeof(H264_NAL_SPS_T));
        break;
      case NAL_TYPE_PIC_PARAM:
        pps_idx = pH264->ctxt.pps_idx;
        memcpy(&pps, &pH264->ctxt.pps[pH264->ctxt.pps_idx], sizeof(H264_NAL_PPS_T));
        break;
    }
  }
  //fprintf(stderr, "%d/%d\n", pStream->bs.byteIdx,pStream->bs.sz);avc_dumpHex(stderr, &pStream->bs.buf[pStream->bs.byteIdx], 16, 1);
  rc = h264_decode_NALHdr(&pH264->ctxt, &pStream->bs, offsetFromStart);

  if(offsetFromStart == 0 && rc >= H264_RESULT_DECODED) {

    switch(nalType) {

      case NAL_TYPE_SEQ_PARAM:
        if(pH264->spspps.sps_len > 0 && (sps_idx != pH264->ctxt.sps_idx ||
          memcmp(&sps, &pH264->ctxt.sps[pH264->ctxt.sps_idx], sizeof(H264_NAL_SPS_T)))) {
           pNal->paramSetChanged = 1;

          VSX_DEBUG_CODEC (
            LOGHEXT_DEBUG(pH264->spspps.sps_buf, pH264->spspps.sps_len);
            VSX_DEBUG2( h264_printSps(stderr, &pH264->ctxt.sps[pH264->ctxt.sps_idx]); )

            LOGHEX_DEBUG(pBufStart, pH264->spspps.sps_len); 
            VSX_DEBUG2( h264_printSps(stderr, &sps); )
          );
        }
         
        if(pH264->spspps.sps_len == 0 || pNal->paramSetChanged) {
          if(bufSz > H264_AVC_STREAM_PREBUFSZ) {
            bufSz = H264_AVC_STREAM_PREBUFSZ;
          }
          if(bufSz < 8) {
            LOG(X_WARNING("Possibly invalid sps streambuf len:%d"), bufSz);
          }
          // TODO: H264_AVC_STREAM_PREBUFSZ may be < SPS size for high profile custom 
          //       quantization matrices
          pH264->spspps.sps = pH264->spspps.sps_buf;
          memcpy(pH264->spspps.sps_buf, pBufStart, bufSz);
          pH264->spspps.sps_len = 0; // since length is not known, will be set in next call to attachNal
        }

        break;

      case NAL_TYPE_PIC_PARAM:
        if(pH264->spspps.pps_len > 0 && (pps_idx != pH264->ctxt.pps_idx ||
          memcmp(&pps, &pH264->ctxt.pps[pH264->ctxt.pps_idx], sizeof(H264_NAL_PPS_T)))) {
           pNal->paramSetChanged = 1;
        }
        if(pH264->spspps.pps_len == 0 || pNal->paramSetChanged) {
          if(bufSz > H264_AVC_STREAM_PREBUFSZ) {
            bufSz = H264_AVC_STREAM_PREBUFSZ;
          }
          // TODO: H264_AVC_STREAM_PREBUFSZ may be < SPS size for high profile custom 
          //       quantization matrices
          pH264->spspps.pps = pH264->spspps.pps_buf;
          memcpy(pH264->spspps.pps_buf, pBufStart, bufSz);
          pH264->spspps.pps_len = 0; // since length is not known, will be set in next call to attachNal
        }
        break;
    }

    if(rc == H264_RESULT_ERR) {
      pH264->numNalsErr++;
    } else if(rc == H264_RESULT_DECODED) {

      if(h264_isNALVcl(nalType)) {
        if(pH264->ctxt.sps[pH264->ctxt.sps_idx].is_ok &&
           pH264->ctxt.sps[pH264->ctxt.sps_idx].profile_id >= H264_PROFILE_HIGH) {
           
          pNal->frameIdx = 0;
        } else {
          pNal->frameIdx = pH264->ctxt.frameIdx;        
        }
        pNal->picOrderCnt = pH264->ctxt.picOrderCnt;      
      }

      pH264->sliceCounter[pH264->ctxt.lastSliceType]++;

      pNal->hdr = nalHdr;
      pNal->sliceType = (uint8_t) pH264->ctxt.lastSliceType;

    } 

  }

  return rc;
}

static int allocNalListFromH264b(H264_AVC_DESCR_T *pH264, FILE_STREAM_T *pFile) {

  unsigned char arena[H264_AVC_STREAM_PREBUFSZ + H264_AVC_STREAM_BUFSZ];
  H264_STREAM_CHUNK_T stream;
  int lenRead;
  int idxStartCode;
  unsigned int nalIdx = 0;

  if((lenRead = ReadFileStreamNoEof(pFile, arena, sizeof(arena))) < 0) {
    return -1;
  }

  H264_INIT_BITPARSER(stream.bs, arena,  ((lenRead > sizeof(arena) - H264_AVC_STREAM_PREBUFSZ) ?
                                         (sizeof(arena) - H264_AVC_STREAM_PREBUFSZ) : lenRead) );
  stream.startCodeMatch = 0;

  if(h264_findStartCode(&stream, 0, NULL) != H264_STARTCODE_LEN) {
    LOG(X_ERROR("No valid start code at start of stream"));
    return -1;
  }

  stream.bs.sz -= H264_STARTCODE_LEN;
  stream.bs.byteIdx += H264_STARTCODE_LEN;

  nalIdx++;

  // Try to get the clock rate if stored in SPS VUI params
  /*if(pH264->clockHz == 0 || pH264->frameDeltaHz == 0) {
    bs.bitIdx = stream.bs.bitIdx;
    bs.byteIdx = stream.bs.byteIdx;
    bs.buf = stream.bs.buf;
    bs.size = stream.bs.size;
    if(h264_decode_NALHdr(&pH264->ctxt, &bs, 0) >= 0) {
      updateSPSVUIClock(pH264);
    }
  }*/
                
  do {

    if(stream.bs.byteIdx >= stream.bs.sz) {
      memcpy(arena, &arena[sizeof(arena)] - H264_AVC_STREAM_PREBUFSZ, H264_AVC_STREAM_PREBUFSZ);
      if((lenRead = ReadFileStreamNoEof(pFile, &arena[H264_AVC_STREAM_PREBUFSZ], 
                sizeof(arena) - H264_AVC_STREAM_PREBUFSZ)) < 0) {
        return -1;
      }
      stream.bs.byteIdx = 0;
      stream.bs.bitIdx = 0;
      stream.bs.sz = lenRead;

    }
 
    if(pFile->offset == pFile->size) {
      stream.bs.sz += H264_AVC_STREAM_PREBUFSZ;
    }

    while((idxStartCode = h264_findStartCode(&stream, 0, NULL)) >= 0) {
      stream.bs.byteIdx += idxStartCode;
      nalIdx++;
    }

    stream.bs.byteIdx = stream.bs.sz;

  } while(pFile->offset < pFile->size);


  if((pH264->pNalsBuf = (H264_NAL_T *) avc_calloc(nalIdx, sizeof(H264_NAL_T))) == NULL) {
    return -1;
  }
  pH264->nalsBufSz = nalIdx;

  return 0;
}

int h264_getAvcCNalLength(const unsigned char *pData, unsigned int len, uint8_t avcfieldlen) {
  int lennal = -1;

  if(avcfieldlen == 4) {
    lennal = (int32_t) htonl(  *((uint32_t *) pData) );
  } else if(avcfieldlen == 2) {
    lennal = (int32_t) htons(  *((uint16_t *) pData) );
  } else {
    LOG(X_ERROR("H.264 avcc annexB length field: %d not supported"), avcfieldlen);
    return -1;
  }

  if(avcfieldlen + lennal > len) {
    LOG(X_ERROR("H.264 avcc annexB NAL size %d + %d extends beyond end %u"), 
                 lennal, avcfieldlen, len);
    return -1;
  }

  return lennal;
}


static int attachAvcCSample(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize, void *pCbData, 
                            int chunk_idx, uint32_t sampleDurationHz) {
  CBDATA_ATTACH_AVCC_SAMPLE_T *pCbDataAvcC = NULL;
  H264_AVC_DESCR_T *pH264 = NULL;
  unsigned int idxByteInSample = 0;
  int lenNal;
  H264_STREAM_CHUNK_T streamDecoder;
  H264_NAL_T nal;
  int lenNalRead;
  int idxByteInNal;
  int rc = 0;
  H264_RESULT_T decodeRc;

  if((pCbDataAvcC = (CBDATA_ATTACH_AVCC_SAMPLE_T *) pCbData) == NULL ||
    pCbDataAvcC->pH264 == NULL) {
    return -1;
  }
  pH264 = pCbDataAvcC->pH264;

  //fprintf(stderr, "ATTACH AVC len:%d\n", sampleSize);

  while(idxByteInSample < sampleSize) {

    //fprintf(stderr, "READ SAMPLE from '%s' offset:%llu\n", pStreamIn->cbGetName(pStreamIn), pStreamIn->offset);
    //if(ReadFileStream(pStreamIn, pCbDataAvcC->arena, pCbDataAvcC->lenNalSzField) < 0) {
    if(pStreamIn->cbRead(pStreamIn, pCbDataAvcC->arena, pCbDataAvcC->lenNalSzField) < 0) {
      return -1;
    }

//fprintf(stderr, "LENNALSZFIELD:%d, %d/%d, fp:0x%llx/0x%llx\n", pCbDataAvcC->lenNalSzField, idxByteInSample, sampleSize, pStreamIn->offset, pStreamIn->size); avc_dumpHex(stderr, pCbDataAvcC->arena, 16, 1);
//avc_dumpHex(stderr, pCbDataAvcC->arena, pCbDataAvcC->lenNalSzField, 1);

    if((lenNal = h264_getAvcCNalLength(pCbDataAvcC->arena, sampleSize - idxByteInSample,
                                       pCbDataAvcC->lenNalSzField)) < 0) {

      LOG(X_ERROR("byte[%d]/%d, file: 0x%llx / 0x%llx avcc field size:%d"), 
          idxByteInSample, sampleSize, pStreamIn->offset - pCbDataAvcC->lenNalSzField, pStreamIn->size, pCbDataAvcC->lenNalSzField);
      return -1;

    } else if(pCbDataAvcC->lenNalSzField == 4 && lenNal == 1 && idxByteInSample == 0 && sampleSize > 5 && 
      ((u_int8_t *) pCbDataAvcC->arena)[0] == 0x00 && ((u_int8_t *) pCbDataAvcC->arena)[1] == 0x00 &&
      ((u_int8_t *) pCbDataAvcC->arena)[2] == 0x00 && ((u_int8_t *) pCbDataAvcC->arena)[3] == 0x01) { 

      lenNal = sampleSize - 4;
      LOG(X_WARNING("Likely invalid avcC header matching a NAL AnnexB start at file: 0x%llx / 0x%llx for sample size: %d"), 
          pStreamIn->offset - pCbDataAvcC->lenNalSzField, pStreamIn->size, sampleSize);
    }

    idxByteInSample += pCbDataAvcC->lenNalSzField;

    if(pH264->pNalsBuf) {
      idxByteInNal = 0;
      lenNalRead = 0;

      nal.content.fileOffset = pStreamIn->offset;

      if((lenNalRead = lenNal - idxByteInNal) > sizeof(pCbDataAvcC->arena)) {
        lenNalRead = sizeof(pCbDataAvcC->arena);
      }

      // TODO: need to look for 0x00 00 00 context and escape first 2 bytes with 0x 00 00 03

      //fprintf(stderr, "READING NAL of size:%d [%d], len of read:%d\n", lenNal, idxByteInNal, lenNalRead);
      if(pStreamIn->cbRead(pStreamIn, pCbDataAvcC->arena, lenNalRead) < 0) {
      //if(ReadFileStream(pStreamIn, pCbDataAvcC->arena, lenNalRead) < 0) {
        return -1;
      }

      if(idxByteInNal == 0) {
   
        H264_INIT_BITPARSER(streamDecoder.bs, pCbDataAvcC->arena, lenNalRead);
        streamDecoder.startCodeMatch = 0;
        nal.content.sizeRd = lenNal;

        if((decodeRc = decode_NALHdr(pH264, &streamDecoder, 0, &nal)) == H264_RESULT_ERR) {
          //fprintf(stdout, "decode err\n");
          return -1;
        } else if(decodeRc == H264_RESULT_DECODED) {
 
          nal.content.chunk_idx = chunk_idx;
          nal.content.pStreamIn = (FILE_STREAM_T *) pStreamIn->pCbData;
          nal.content.playDurationHz = sampleDurationHz;

          if(attachNal(pH264, &nal) < 0) {
              return -1;
          } 
          if(pH264->pNalsLastVcl) {
            pH264->pNalsLastVcl->content.sizeWr += pCbDataAvcC->lenNalSzField;
          }
        }

      } // end of if(idxByteInNal == 0) {

      if(lenNal > lenNalRead && 
         //SeekMediaFile(pStreamIn, lenNal - lenNalRead, SEEK_CUR) != 0) {
         pStreamIn->cbSeek(pStreamIn, lenNal - lenNalRead, SEEK_CUR) != 0) {
        return -1;
      }

    //} else if(SeekMediaFile(pStreamIn, lenNal, SEEK_CUR) != 0) {
    } else if(pStreamIn->cbSeek(pStreamIn, lenNal, SEEK_CUR) != 0) {
      return -1;
    }
 
    if((idxByteInSample += lenNal) > (unsigned int) sampleSize) {
      return -1;
    }

    if(pH264->pNalsBuf == NULL) {
      pH264->nalsBufSz++;
    }
   
  }

  pH264->lastPlayOffsetHz += sampleDurationHz;

  if(idxByteInSample > (unsigned int) sampleSize) {
    LOG(X_ERROR("Invalid NAL lengths %u > %u"), idxByteInSample, sampleSize);
    rc = -1;
  }

  return rc;
}

static int attachAvcCSample_mp4wrap(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, void *pCbData, 
                            int chunk_idx, uint32_t sampleDurationHz) {
  MP4_FILE_STREAM_T mp4Fs;

  memset(&mp4Fs, 0, sizeof(mp4Fs));
  mp4Fs.cbRead = mp4_cbReadDataFile;
  mp4Fs.cbSeek = mp4_cbSeekDataFile;
  mp4Fs.cbClose = mp4_cbCloseDataFile;
  mp4Fs.cbCheckFd = mp4_cbCheckFdDataFile;
  mp4Fs.cbGetName = mp4_cbGetNameDataFile;
  mp4Fs.pCbData = pStreamIn;
  mp4Fs.offset = pStreamIn->offset;
  mp4Fs.size = pStreamIn->size;

  return attachAvcCSample(&mp4Fs, sampleSize, pCbData,
                          chunk_idx, sampleDurationHz);
}

static int attachSpsPpsFromAvcc(H264_AVC_DESCR_T *pH264, AVC_DECODER_CFG_T *pAvcC) {
  H264_STREAM_CHUNK_T streamDecoder;
  H264_NAL_T nal;
  unsigned int idx;

  memset(&nal, 0, sizeof(nal));

  for(idx = 0; idx < (unsigned int) (pAvcC->numsps &  0x1f); idx++) {

    H264_INIT_BITPARSER(streamDecoder.bs, pAvcC->psps[idx].data, pAvcC->psps[idx].len);
    streamDecoder.startCodeMatch = 0;
    nal.content.sizeRd = pAvcC->psps[idx].len;

    if(decode_NALHdr(pH264, &streamDecoder, 0, &nal) != H264_RESULT_DECODED) {
      LOG(X_ERROR("Failed to decode SPS[%d]"), idx);
      return -1;
    }
    if(attachNal(pH264, &nal) < 0) {
      return -1;
    }
  }

  for(idx = 0; idx < (pAvcC->numpps); idx++) {

    H264_INIT_BITPARSER(streamDecoder.bs, pAvcC->ppps[idx].data, pAvcC->ppps[idx].len);
    streamDecoder.startCodeMatch = 0;
    nal.content.sizeRd = pAvcC->ppps[idx].len;

    if(decode_NALHdr(pH264, &streamDecoder, 0, &nal) != H264_RESULT_DECODED) {
      LOG(X_ERROR("Failed to decode SPS[%d]"), idx);
      return -1;
    }
    if(attachNal(pH264, &nal) < 0) {
      return -1;
    }
  }

  return 0;
}

int h264_createNalListFromFlv(H264_AVC_DESCR_T *pH264, FLV_CONTAINER_T *pFlv, double fps) {

  if(!pH264 || !pFlv || !pFlv->vidCfg.pAvcC) {
    return -1;
  }

  LOG(X_DEBUG("Reading NALs from %s"), pFlv->pStream->filename);

  memset(&pH264->ctxt, 0, sizeof(H264_DECODER_CTXT_T));
  pH264->avcCbData.pH264 = pH264;
  pH264->avcCbData.lenNalSzField = pFlv->vidCfg.pAvcC->lenminusone + 1;

  if(attachSpsPpsFromAvcc(pH264, pFlv->vidCfg.pAvcC) < 0) {
    h264_free(pH264);
    return -1;
  }

  // override FLV script tag timing w/ SPS VUI
  if(pH264->clockHz && pH264->frameDeltaHz) {
    pFlv->vidtimescaleHz = pH264->clockHz;
    pFlv->vidsampledeltaHz = pH264->frameDeltaHz;
  }

  if(fps > 0.0) {
    vid_convert_fps(fps, &pH264->clockHz, &pH264->frameDeltaHz);
  }

  pH264->clockHzOverride = pFlv->vidtimescaleHz;

  LOG(X_DEBUG("Reading H.264 frames from %s"), pFlv->pStream->filename);

  if(flv_readSamples(pFlv, attachAvcCSample_mp4wrap, &pH264->avcCbData, 0, 0, 1) < 0) {
    h264_free(pH264);    
    return -1;
  }

  if((pH264->pNalsBuf = (H264_NAL_T *) avc_calloc(pH264->nalsBufSz, 
                                       sizeof(H264_NAL_T))) == NULL) {
    h264_free(pH264);
    return -1;
  }

  pH264->lastPlayOffsetHz = 0;
  if(flv_readSamples(pFlv, attachAvcCSample_mp4wrap, &pH264->avcCbData, 0, 0, 1) < 0) {
    h264_free(pH264);
    return -1;
  }

  return 0;
}

int h264_createNalListFromMkv(H264_AVC_DESCR_T *pH264, MKV_CONTAINER_T *pMkv) {

  if(!pH264 || !pMkv|| !pMkv->vid.u.pCfgH264) {
    return -1;
  }

  LOG(X_DEBUG("Reading NALs from %s"), pMkv->pStream->filename);

  memset(&pH264->ctxt, 0, sizeof(H264_DECODER_CTXT_T));
  pH264->avcCbData.pH264 = pH264;
  pH264->avcCbData.lenNalSzField = pMkv->vid.u.pCfgH264->lenminusone + 1;

  if(attachSpsPpsFromAvcc(pH264, pMkv->vid.u.pCfgH264) < 0) {
    h264_free(pH264);
    return -1;
  }

  // override FLV script tag timing w/ SPS VUI
  //if(pH264->clockHz && pH264->frameDeltaHz) {
  //  pFlv->vidtimescaleHz = pH264->clockHz;
  //  pFlv->vidsampledeltaHz = pH264->frameDeltaHz;
  //}

  //if(fps > 0.0) {
  //  vid_convert_fps(fps, &pH264->clockHz, &pH264->frameDeltaHz);
  //}

  pH264->clockHzOverride = pH264->clockHz;

  LOG(X_DEBUG("Reading H.264 frames from %s"), pMkv->pStream->filename);

  if(!(pH264->pMkvCtxt = (MKV_EXTRACT_STATE_T *) avc_calloc(1, sizeof(MKV_EXTRACT_STATE_T)))) {
    h264_free(pH264);
    return -1;
  }
  pH264->pMkvCtxt->pMkv = pMkv;  

  //if(flv_readSamples(pFlv, attachAvcCSample_mp4wrap, &pH264->avcCbData, 0, 0, 1) < 0) {
  //  h264_free(pH264);    
  //  return -1;
  //}

  pH264->nalsBufSz = H264_AVC_MAX_SLICES_PER_FRAME;
  if((pH264->pNalsBuf = (H264_NAL_T *) avc_calloc(pH264->nalsBufSz, 
                                       sizeof(H264_NAL_T))) == NULL) {
    h264_free(pH264);
    return -1;
  }

  pH264->lastPlayOffsetHz = 0;
  //if(flv_readSamples(pFlv, attachAvcCSample_mp4wrap, &pH264->avcCbData, 0, 0, 1) < 0) {
  //  h264_free(pH264);
  //  return -1;
  //}

  return 0;
}

int h264_createNalListFromMp4Direct(H264_AVC_DESCR_T *pH264, 
                                    MP4_CONTAINER_T *pMp4, float fStartSec) {
  MP4_TRAK_AVCC_T mp4BoxSet;
  unsigned int idx;
  uint64_t startHz = 0;
  uint64_t curHz = 0;
  uint32_t sampleHz;
  int rc;

  if(!pH264 || !pMp4) {
    return -1;
  }

  if(mp4_initAvcCTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  if(fStartSec > 0.0 && mp4BoxSet.tk.pMdhd->timescale == 0) {
    LOG(X_WARNING("Unable to do partial h264 video extract since mdhd timescale is not set"));
  } else {
    startHz = (uint64_t) (fStartSec * mp4BoxSet.tk.pMdhd->timescale);
  }

  memset(&pH264->ctxt, 0, sizeof(H264_DECODER_CTXT_T));
  memset(&pH264->mp4ContentNode, 0, sizeof(pH264->mp4ContentNode));

  if(attachSpsPpsFromAvcc(pH264, &mp4BoxSet.pAvcC->avcc) < 0) {
    h264_free(pH264);
    return -1;
  }

  if(pH264->clockHz != mp4BoxSet.tk.pMdhd->timescale) {
    pH264->clockHzOverride = mp4BoxSet.tk.pMdhd->timescale;
  } else {
    pH264->clockHzOverride = pH264->clockHz;
  }

  pH264->nalsBufSz = H264_AVC_MAX_SLICES_PER_FRAME;
  if((pH264->pNalsBuf = (H264_NAL_T *) 
                        avc_calloc(pH264->nalsBufSz, sizeof(H264_NAL_T))) == NULL) {
    h264_free(pH264);
    return -1;
  }

  if(!(pH264->pMp4Ctxt = mp4_create_extract_state(pMp4, &mp4BoxSet.tk))) {
    h264_free(pH264);
    return -1;
  }

  pH264->lastFrameId = mp4_getSampleCount(&pH264->pMp4Ctxt->trak);
  pH264->avcCbData.pH264 = pH264;
  pH264->avcCbData.lenNalSzField = mp4BoxSet.pAvcC->avcc.lenminusone + 1;

  //
  // Skip fStartSec of content
  //
  if(fStartSec > 0) {

    for(idx=0; idx < pH264->lastFrameId; idx++) {
      if((rc = mp4_readSampleFromTrack(&pH264->pMp4Ctxt->cur, &pH264->mp4ContentNode, 
                                       &sampleHz)) < 0) {
        h264_free(pH264);
        return -1;
      }
      curHz += sampleHz;
      if(sampleHz > startHz || curHz >= startHz - sampleHz) {
        memcpy(&pH264->pMp4Ctxt->start, &pH264->pMp4Ctxt->cur, 
              sizeof(pH264->pMp4Ctxt->start));
        break;
      }
    }
  }

  return 0;
}

int h264_createNalListFromMp4(H264_AVC_DESCR_T *pH264, MP4_CONTAINER_T *pMp4,
                              CHUNK_SZ_T *pStchk, float fStartSec) {

  MP4_TRAK_AVCC_T mp4BoxSet;
  uint64_t startHz = 0;

  if(!pH264 || !pMp4) {
    return -1;
  }

 LOG(X_DEBUG("Reading NALs from %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  if(mp4_initAvcCTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  if(fStartSec > 0.0 && mp4BoxSet.tk.pMdhd->timescale == 0) {
    LOG(X_WARNING("Unable to do partial video extract"));
  } else {
    startHz = (uint64_t) (fStartSec * mp4BoxSet.tk.pMdhd->timescale);
  }

  memset(&pH264->ctxt, 0, sizeof(H264_DECODER_CTXT_T));
  pH264->avcCbData.pH264 = pH264;
  pH264->avcCbData.lenNalSzField = mp4BoxSet.pAvcC->avcc.lenminusone + 1;

  if(attachSpsPpsFromAvcc(pH264, &mp4BoxSet.pAvcC->avcc) < 0) {
    h264_free(pH264);
    return -1;
  }

  if(pH264->clockHz != mp4BoxSet.tk.pMdhd->timescale) {
    pH264->clockHzOverride = mp4BoxSet.tk.pMdhd->timescale;
  } else {
    pH264->clockHzOverride = pH264->clockHz;
  }

  if(mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream, attachAvcCSample, 
                              &pH264->avcCbData, startHz, 0) < 0) {
    h264_free(pH264);
    return -1;
  }

  if(pH264->nalsBufSz == 0) {
    LOG(X_WARNING("No avc samples found"));
    h264_free(pH264);
    return -1;
  }

  if((pH264->pNalsBuf = (H264_NAL_T *) 
                         avc_calloc(pH264->nalsBufSz, sizeof(H264_NAL_T))) == NULL) {
    h264_free(pH264);
    return -1;
  }

  if(pStchk && mp4BoxSet.tk.pStco) {
    mp4BoxSet.tk.stchk.p_stchk = (CHUNK_DESCR_T *) 
                      avc_calloc(mp4BoxSet.tk.pStco->entrycnt, sizeof(CHUNK_DESCR_T));
    mp4BoxSet.tk.stchk.stchk_sz = mp4BoxSet.tk.pStco->entrycnt; 
  }

  pH264->lastPlayOffsetHz = 0;
  if(mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream, attachAvcCSample, 
                              &pH264->avcCbData, startHz, 0) < 0) {
    h264_free(pH264);
    if(mp4BoxSet.tk.stchk.p_stchk) {
      free(mp4BoxSet.tk.stchk.p_stchk);
    }
    return -1;
  }

  if(pStchk) {
    pStchk->p_stchk = mp4BoxSet.tk.stchk.p_stchk;
    pStchk->stchk_sz = mp4BoxSet.tk.stchk.stchk_sz;
  } 

  return 0;
}

int h264_createNalListFromAnnexB(H264_AVC_DESCR_T *pH264, FILE_STREAM_T *pStream,
                                 float fStart, double fps) {

  unsigned char arena[H264_AVC_STREAM_PREBUFSZ + H264_AVC_STREAM_BUFSZ];
  H264_STREAM_CHUNK_T stream, streamDecoder, streamDecoder2;
  H264_NAL_T nal;
  H264_NAL_T nalPrev;
  FILE_OFFSET_T fileOffset;
  int decodeRc;
  int lenRead;
  int idxStartCode;
  int nalIdx = 0;
  int rc;

  if(!pH264 || !pStream) {
    return -1;
  } 

  LOG(X_DEBUG("Reading NALs from %s"), pStream->filename);

  if(fStart > 0) {
    LOG(X_WARNING("Partial extract of h.264 annexB not implemented"));
  }

  H264_INIT_BITPARSER(stream.bs, arena, 0);
  stream.startCodeMatch = 0;

  //memset(&stream, 0, sizeof(stream));
  memset(&pH264->ctxt, 0, sizeof(H264_DECODER_CTXT_T));
  memset(&nal, 0, sizeof(nal));
  memset(&nalPrev, 0, sizeof(nalPrev));
  //stream.bs.emulPrevSeqLen = 3;
  //stream.bs.emulPrevSeq[2] = 0x03;
  //stream.bs.buf = arena;

  if(fps > 0.0) {
    vid_convert_fps(fps, &pH264->clockHz, &pH264->frameDeltaHz);
  }

  if(allocNalListFromH264b(pH264, pStream)) {
    return -1;
  }

  if(SeekMediaFile(pStream, 0, SEEK_SET) != 0) {
    h264_free(pH264);
    return -1;
  }

  if((lenRead = ReadFileStreamNoEof(pStream, arena, sizeof(arena))) < 0) {
    h264_free(pH264);
    return -1;
  }

  stream.bs.byteIdx = 0;
  stream.bs.bitIdx = 0;
  stream.bs.sz = (lenRead > sizeof(arena) - H264_AVC_STREAM_PREBUFSZ) ? 
                  (sizeof(arena) - H264_AVC_STREAM_PREBUFSZ) : lenRead;

  if(h264_findStartCode(&stream, 0, NULL) != 4) {
    LOG(X_ERROR("No valid start code at start of stream"));
    h264_free(pH264);
    return -1;
  }

  //stream.bs.sz -= H264_STARTCODE_LEN;
  stream.bs.byteIdx += H264_STARTCODE_LEN;
  fileOffset = stream.bs.byteIdx;

  memcpy(&streamDecoder, &stream, sizeof(streamDecoder));
  if((decodeRc = decode_NALHdr(pH264, &streamDecoder, 0, &nal)) == H264_RESULT_ERR) {
    LOG(X_ERROR("Failed to decode NAL idx: %d"), nalIdx);
    h264_free(pH264);
    return -1;
  }
  nalIdx++;
  nal.content.fileOffset = pStream->offset - lenRead + stream.bs.byteIdx;
                 
  do {

    if(stream.bs.byteIdx >= stream.bs.sz) {
      memcpy(arena, &arena[sizeof(arena)] - H264_AVC_STREAM_PREBUFSZ, H264_AVC_STREAM_PREBUFSZ);
      if((lenRead = ReadFileStreamNoEof(pStream, &arena[H264_AVC_STREAM_PREBUFSZ], 
                sizeof(arena) - H264_AVC_STREAM_PREBUFSZ)) < 0) {
        h264_free(pH264);
        return -1;
      }
      stream.bs.byteIdx = 0;
      stream.bs.bitIdx = 0;
      stream.bs.sz = lenRead;

    }
 
    if(pStream->offset == pStream->size) {
      stream.bs.sz += H264_AVC_STREAM_PREBUFSZ;
    }

    while((idxStartCode = h264_findStartCode(&stream, 0, NULL)) >= 0) {
      stream.bs.byteIdx += idxStartCode;
      fileOffset += idxStartCode;

      if(decodeRc == H264_RESULT_IGNORED) {
        LOG(X_WARNING("Ignoring NAL vcl slice.  SPS:%u PPS:%u"),
          pH264->ctxt.flag & H264_DECODER_CTXT_HAVE_SPS,
          pH264->ctxt.flag & H264_DECODER_CTXT_HAVE_PPS);
      }

      if(nal.hdr != 0 && decodeRc == H264_RESULT_DECODED) {
        nal.content.sizeRd = (uint32_t)(fileOffset - nal.content.fileOffset) - H264_STARTCODE_LEN;
        if((rc = attachNal(pH264, &nal)) < 0) {
          h264_free(pH264);
          return -1;
        } 

        if(pH264->pNalsLastVcl) {

          if(rc > 0) {
             if(nalPrev.frameIdx != nal.frameIdx ||
                nalPrev.picOrderCnt != nal.picOrderCnt) {
              pH264->pNalsLastVcl->content.playDurationHz = pH264->frameDeltaHz; 
              pH264->pNalsLastVcl->content.playOffsetHz += pH264->frameDeltaHz; 
              pH264->lastPlayOffsetHz += pH264->frameDeltaHz;
            } else {
              pH264->pNalsLastVcl->content.playDurationHz = pH264->frameDeltaHz; 
            }
          }
          nalPrev.content.playDurationHz = pH264->pNalsLastVcl->content.playDurationHz;

          pH264->pNalsLastVcl->content.pStreamIn = pStream;
        }
        nal.hdr = 0;
        nal.content.flags = 0;

        nalPrev.frameIdx = nal.frameIdx;
        nalPrev.picOrderCnt = nal.picOrderCnt;
      }

      memcpy(&streamDecoder, &stream, sizeof(streamDecoder));   
      if(lenRead >= sizeof(arena) - H264_AVC_STREAM_PREBUFSZ) {
        streamDecoder.bs.sz += H264_AVC_STREAM_PREBUFSZ;
      }

      //check for next start code start for short (empty?) NALs
      memcpy(&streamDecoder2, &streamDecoder, sizeof(streamDecoder));
      streamDecoder2.bs.sz = streamDecoder2.bs.byteIdx + 16;
      if((idxStartCode = h264_findStartCode(&streamDecoder2, 0, NULL)) >= 4) {
        streamDecoder.bs.sz = streamDecoder.bs.byteIdx + (idxStartCode - 4);
      }

      if((decodeRc = decode_NALHdr(pH264, &streamDecoder, 0, &nal)) == H264_RESULT_ERR) {
        LOG(X_ERROR("Failed to decode NAL idx: %d file: 0x%llx"), nalIdx, fileOffset);
        h264_free(pH264);
        return -1;
      }

      nalIdx++;
      nal.content.fileOffset = fileOffset;
    }


    fileOffset = pStream->offset - H264_AVC_STREAM_PREBUFSZ;
    stream.bs.byteIdx = stream.bs.sz;


  } while(pStream->offset < pStream->size);

  if(nal.hdr != 0 && decodeRc == H264_RESULT_DECODED) {
    nal.content.sizeRd = (uint32_t)(pStream->offset - nal.content.fileOffset);
    nal.content.playDurationHz = pH264->frameDeltaHz; 
    if((rc = attachNal(pH264, &nal)) < 0) {
      h264_free(pH264);
      return -1;
    } else if(rc > 0) {
      pH264->lastPlayOffsetHz += pH264->frameDeltaHz;
    }
    if(pH264->pNalsLastVcl) {
      pH264->pNalsLastVcl->content.pStreamIn = pStream;
    }
    nal.hdr = 0;
  }

  // Ensure the list of content nodes does not end w/ a non P,B,I,
  // since an SEI will not have a known frameId until subsequent
  // reception of P,B,I.
  if(pH264->pNalsLastVcl != pH264->pNalsLastSlice) {
    pH264->pNalsLastVcl = pH264->pNalsLastSlice;
    if(pH264->pNalsLastSlice) {
      pH264->pNalsLastSlice->content.pnext = NULL;
    }
  }

  pH264->clockHzOverride = pH264->clockHz;

  return 0;
}

int h264_moveToPriorSync(H264_AVC_DESCR_T *pH264) {
  H264_NAL_T *pNal;

  if(pH264->pMp4Ctxt) {

    memcpy(&pH264->pMp4Ctxt->cur, &pH264->pMp4Ctxt->lastSync, sizeof(pH264->pMp4Ctxt->cur));
    pH264->lastPlayOffsetHz = pH264->pMp4Ctxt->lastSyncPlayOffsetHz;

  } else if(pH264->pMkvCtxt) {

  } else if((pNal = pH264->pNalsLastSliceIPrev)) {

    while(pNal->content.pnext && pNal->content.pnext->pnext && 
          &pNal->content != pH264->pNalCbPrev) {

      if(pNal->content.pnext->pnext == &pH264->pNalsLastSliceI->content) {
        pH264->pNalCbPrev = &pNal->content;           
        break;
      }
      pNal = (H264_NAL_T *) pNal->content.pnext;
    }
  }

  return 0;
}

MP4_MDAT_CONTENT_NODE_T *h264_cbGetNextAnnexBSample(void *pArg, int idx, int advanceFp) {
  H264_AVC_DESCR_T *pH264 = (H264_AVC_DESCR_T *) pArg;
  H264_NAL_T *pNal = NULL;
  int rc;
  uint32_t sampleDuration = 0;

  if(!pH264) {
    return NULL;
  }

  if(idx == 0) {
    pH264->pNalsLastSlice = NULL;
    pH264->pNalsLastSliceP = NULL;
    pH264->pNalsLastSliceI = NULL;
    pH264->pNalsLastSliceIPrev = NULL;
    pH264->pNalsLastSlicePocRef = NULL;
    pH264->pNalsLastSEI = NULL;
    pH264->pNalCbPrev = NULL;
  }

  if(pH264->pMp4Ctxt) {

    if(idx == 0) {
      memcpy(&pH264->pMp4Ctxt->cur, &pH264->pMp4Ctxt->start, sizeof(pH264->pMp4Ctxt->cur));
      memset(&pH264->pNalsBuf[0], 0, sizeof(H264_NAL_T));
      pH264->lastPlayOffsetHz = 0;
    }

    if(mp4_getNextSyncSampleIdx(&pH264->pMp4Ctxt->cur) == pH264->pMp4Ctxt->cur.u.tk.idxSample + 1) {
      // Position to frame 1 before read of next sync frame
      memcpy(&pH264->pMp4Ctxt->lastSync, &pH264->pMp4Ctxt->cur, 
             sizeof(pH264->pMp4Ctxt->lastSync));
      pH264->pMp4Ctxt->lastSyncPlayOffsetHz = pH264->lastPlayOffsetHz;
    }

    if((rc = mp4_readSampleFromTrack(&pH264->pMp4Ctxt->cur, &pH264->mp4ContentNode, 
                                     &sampleDuration)) < 0) {
      return NULL;
    }

    if(advanceFp == -1) {
      pH264->mp4ContentNode.playDurationHz = sampleDuration; 
      pH264->mp4ContentNode.playOffsetHz = pH264->lastPlayOffsetHz;
      pH264->lastPlayOffsetHz += sampleDuration;
      return &pH264->mp4ContentNode;
    }

    if(pH264->pMp4Ctxt->pStream->cbSeek(pH264->pMp4Ctxt->pStream, 
                                        pH264->mp4ContentNode.fileOffset, SEEK_SET) != 0) {
      return NULL;
    }

    pH264->pNalsLastSEI = NULL;
    pH264->pNalsLastVcl = NULL;
    pH264->pNalsVcl = NULL;
    pH264->numNalsVcl = 0;
    pH264->numNalsTot = 0;

    do {
      //fprintf(stderr, "calling attachAvcCSample sz:%d\n", pH264->mp4ContentNode.sizeRd);
      if(attachAvcCSample(pH264->pMp4Ctxt->pStream, pH264->mp4ContentNode.sizeRd,
                       &pH264->avcCbData, pH264->mp4ContentNode.chunk_idx, 
                       sampleDuration) < 0) {
        //fprintf(stdout, "attachAvc err\n");
        return NULL;
      }

    } while(!pH264->pNalsLastVcl || 
            !h264_isNALVcl(pH264->pNalsLastVcl->hdr));

    pH264->pNalCbPrev = &pH264->pNalsVcl->content;

  } else if(pH264->pMkvCtxt) {

    if(idx == 0) {
      //memcpy(&pH264->pMp4Ctxt->cur, &pH264->pMp4Ctxt->start, sizeof(pH264->pMp4Ctxt->cur));
      memset(&pH264->pNalsBuf[0], 0, sizeof(H264_NAL_T));
      pH264->lastPlayOffsetHz = 0;
    }


    if(mkv_loadFrame(pH264->pMkvCtxt->pMkv, &pH264->mp4ContentNode, MKV_TRACK_TYPE_VIDEO,
                  pH264->clockHz, &pH264->lastFrameId) < 0) {
      return NULL;
    }

    pH264->lastPlayOffsetHz = pH264->mp4ContentNode.playOffsetHz;

    pH264->pNalsLastSEI = NULL;
    pH264->pNalsLastVcl = NULL;
    pH264->pNalsVcl = NULL;
    pH264->numNalsVcl = 0;
    pH264->numNalsTot = 0;

    //do {
//fprintf(stderr, "calling attachAvcCSample sz:%d\n", pH264->mp4ContentNode.sizeRd);
      if(attachAvcCSample_mp4wrap(pH264->pMkvCtxt->pMkv->pStream, pH264->mp4ContentNode.sizeRd,
                       &pH264->avcCbData, pH264->mp4ContentNode.chunk_idx,
                       sampleDuration) < 0) {
        //fprintf(stdout, "attachAvc err\n");
        return NULL;
      }

    //} while(!pH264->pNalsLastVcl || !h264_isNALVcl(pH264->pNalsLastVcl->hdr));

    pH264->pNalCbPrev = &pH264->pNalsVcl->content;

//H264_NAL_T *pD = pH264->pNalsVcl;
//while(pD) { fprintf(stderr, "NAL 0x%x, sz:%d\n", pD->hdr, pD->content.sizeRd); pD = pD->content.pnext; }
//fprintf(stderr, "RETURNING NAL sz:%d, frameId:%d\n", pH264->pNalCbPrev->sizeRd, pH264->pNalCbPrev->frameId);

  } else {

    if(idx == 0 && pH264->pNalsVcl) {
      pNal = pH264->pNalsVcl;
    } else if(pH264->pNalCbPrev) {
      pNal = (H264_NAL_T *) pH264->pNalCbPrev->pnext;
    }

    if(pNal && pNal->sliceType == H264_SLICE_TYPE_I &&
       (pH264->pNalsLastSliceI == NULL || 
        pH264->pNalsLastSliceI->content.frameId != pNal->content.frameId)) {
        pH264->pNalsLastSliceIPrev = pH264->pNalsLastSliceI;
        pH264->pNalsLastSliceI = pNal;
    }

    if(pNal) {
      pH264->pNalCbPrev = &pNal->content;
    } else {
      pH264->pNalCbPrev = NULL;
    }
  }

  return pH264->pNalCbPrev;
}

int h264_write_nal_to_mp4(FILE_STREAM_T *pStreamOut, MP4_MDAT_CONTENT_NODE_T *pSample,
                                  unsigned char *arena, unsigned int sz_arena) {

  int lenbody;
  int lenread;
  unsigned int lenhdr;
  int rc = 0;

  lenhdr = pSample->sizeWr - pSample->sizeRd;

  if(lenhdr == 4) {
    *((uint32_t *) arena) = htonl(pSample->sizeRd);
  } else if(lenhdr == 2) {
    *((uint16_t *) arena) = htons(pSample->sizeRd);
  } else if(lenhdr != 0) {
    LOG(X_ERROR("Invalid chunk sample entry header prefix length: %d"), lenhdr);
    return -1;
  }
  if(lenhdr > 0) {
    if(WriteFileStream(pStreamOut, arena, lenhdr) < 0) {
      return -1;
    }
    rc += lenhdr;
  }

  if(SeekMediaFile(pSample->pStreamIn, pSample->fileOffset, SEEK_SET) != 0) {
    return -1;
  }

  lenbody = pSample->sizeRd;

  while(lenbody > 0) {

    lenread = lenbody > (int) sz_arena ? sz_arena : lenbody;

    if(ReadFileStream(pSample->pStreamIn, arena, lenread) < 0) {
      return -1;
    }
    
    if(WriteFileStream(pStreamOut, arena, lenread) < 0) {
      return -1;
    }
    rc += lenread;
    lenbody -= lenread;
  }

  return rc;
}

int h264_updateFps(H264_AVC_DESCR_T *pH264, double fps) {

  unsigned int clockHz = 0;
  unsigned int frameDeltaHz = 0;

  vid_convert_fps(fps, &clockHz, &frameDeltaHz);

  if(fps != 0 && pH264->clockHzVUI != 0 && pH264->frameDeltaHzVUI != 0) {
    LOG(X_WARNING("Overriding SPS clock %u Hz, tick to %u Hz (%f fps) "
                    "with %u Hz, tick to %u Hz (%.4f fps)"),
                    pH264->clockHzVUI, pH264->frameDeltaHzVUI, 
                    (float) pH264->clockHzVUI/pH264->frameDeltaHzVUI,
                    clockHz, frameDeltaHz, (float)clockHz/frameDeltaHz);
     
    pH264->clockHz = clockHz;
    pH264->frameDeltaHz = frameDeltaHz;
  } else if(pH264->clockHz == 0 || pH264->frameDeltaHz == 0) {

    if(clockHz == 0 || frameDeltaHz == 0) {
      LOG(X_ERROR("Frame rate not found in SPS VUI.  Specify frame rate with '--fps='"));
      return -1;
    }
    LOG(X_WARNING("Using clock %uHz, tick to %uHz (%.4f fps)"),
                    clockHz, frameDeltaHz, (float)clockHz/frameDeltaHz);
    
    pH264->clockHz = clockHz;
    pH264->frameDeltaHz = frameDeltaHz;
  }

  return 0;
}

int h264_converth264bToAvc(unsigned char *pData, unsigned int len) {
  H264_STREAM_CHUNK_T sc;
  unsigned char *p;
  int pos;

  if(len < 4) {
    return -1;
  }

  //
  // Convert each h264 annexB slice header to AVC representation
  //

  p = pData;
  len -= 4;

  memset(&sc, 0, sizeof(sc));
  sc.bs.buf = pData + 4;
  sc.bs.sz = len;

  //
  //Handle multiple slices per frame
  //
  while((pos = h264_findStartCode(&sc, 0, NULL)) > 4) {

    *((uint32_t *) p) = htonl(pos - 4);

    sc.bs.buf += pos;
    sc.bs.sz -= pos;

    p = sc.bs.buf - 4;
    len = sc.bs.sz;
  }

  *((uint32_t *) p) = htonl(len);

  return 0;
}

int h264_convertAvcToh264b(const unsigned char *pIn, unsigned int lenIn, 
                           unsigned char *pOut, unsigned int lenOut, 
                           int avcFieldLen, int failOnSize) {
  unsigned int idxIn = 0;
  unsigned int idxOut = 0;
  int lennal;
  unsigned int lenCopy;

  //fprintf(stderr, "CONVERT len:%d->%d, avcFieldLen:%d\n", lenIn, lenOut, avcFieldLen);

  //
  // Convert H.264 AVC format to AnnexB w/ 4 byte startcodes
  //
  while(idxIn < lenIn) {

    if((lennal = h264_getAvcCNalLength(&pIn[idxIn], lenIn - idxIn, avcFieldLen)) < 0) {
      return -1;
    }

    if((lenCopy  = (unsigned int) lennal) > lenOut - idxOut + 4) {
      if(failOnSize) {
        LOG(X_ERROR("Output buffer [%u]/%u not sufficient for NAL length %d"), idxOut, lenOut, lennal);
        return -1;
      }
      if(lenCopy > lenOut - idxOut) {
        lenCopy = lenOut - idxOut;
      }
    }

    idxIn += avcFieldLen;

    //
    // Create 4 byte AnnexB start code
    //
    if( htonl(*((uint32_t *)  &pIn[idxIn])) != 0x01) {
      *((uint32_t *) &pOut[idxOut]) = htonl(0x01);
      idxOut += 4;
    }

    memcpy(&pOut[idxOut], &pIn[idxIn], lenCopy);
    idxIn += lenCopy;
    idxOut =+ lenCopy;

    if(lennal > lenCopy) {
      break;
    }

  }

  return idxOut;
}

int h264_check_NAL_AUD(const unsigned char *p, unsigned int len) {

  if(!p) {
    return 0;
  }

  if(len >= 6 && *((uint32_t *) p) == htonl(0x01) &&
     (p[4] & NAL_TYPE_MASK) == NAL_TYPE_ACCESS_UNIT_DELIM) {
    return 6;
  }

  return 0;
}

