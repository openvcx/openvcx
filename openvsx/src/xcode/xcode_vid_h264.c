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


#define H264_SEQHDR_LE   0x01000000
#define H264_SEQHDR_BE   0x00000001
#define H264_SEQHDR      H264_SEQHDR_LE

static int memcmp_spspps(const SPSPPS_RAW_T *pspspps, const SPSPPS_RAW_T *pspspps2) {

  //fprintf(stderr, "memcmp sps: %d -> %d pps: %d -> %d\n", pspspps->sps_len, pspspps->sps_len, pspspps2->pps_len, pspspps2->pps_len);
  
  if(pspspps->sps_len != pspspps2->sps_len || pspspps->pps_len != pspspps2->pps_len ||
     memcmp(pspspps->sps, pspspps2->sps, pspspps->sps_len) ||
     memcmp(pspspps->pps, pspspps2->pps, pspspps->pps_len)) {
    return 1;
  }
  return 0;
}

static void memcpy_spspps(SPSPPS_RAW_T *pspspps_dst, const SPSPPS_RAW_T *pspspps_src) {

  //fprintf(stderr, "memcpy_spspps sps: %d -> %d pps: %d -> %d\n", pspspps_src->sps_len, pspspps_dst->sps_len, pspspps_src->pps_len, pspspps_dst->pps_len);

  //Assuming non-garbage input w/ no bounds checking here
  memcpy(pspspps_dst->sps_buf, pspspps_src->sps_buf, pspspps_src->sps_len);
  pspspps_dst->sps_len = pspspps_src->sps_len;
  memcpy(pspspps_dst->pps_buf, pspspps_src->pps_buf, pspspps_src->pps_len);
  pspspps_dst->pps_len = pspspps_src->pps_len;

  pspspps_dst->sps = pspspps_dst->sps_buf;
  pspspps_dst->pps = pspspps_dst->pps_buf;
  
}

static int convert_slices_h264b_avc(unsigned char **ppData, unsigned int *plen) {

  (*ppData) -= 4;
  *plen += 4;

  h264_converth264bToAvc(*ppData, *plen);

  return 0;
}

enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_h264(STREAM_XCODE_DATA_T *pData, 
                                               IXCODE_VIDEO_CTXT_T  *pXcode, 
                                               int *pInputResChange) {
  H264_STREAM_CHUNK_T sc;
  H264_RESULT_T h264rc;
  int sz;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;
  unsigned char *pDataOrig = pData->curFrame.pData;
  unsigned int lenDataOrig = pData->curFrame.lenData;
  unsigned int seqHdrWidth, seqHdrHeight;
  int pos;
  unsigned int outidx = 0;

  // pData or lenData may not be set when upsampling on a duplicate input frame

  //fprintf(stderr, "SEQHDR H264 len:%d (spsprev_len:%d, sps_len:%d), (ppsprev_len:%d, pps_len:%d)\n", pData->curFrame.lenData, pUData->uin.xh264.spsppsprev.sps_len, pUData->uin.xh264.spspps.sps_len, pUData->uin.xh264.spsppsprev.pps_len, pUData->uin.xh264.spspps.pps_len); avc_dumpHex(stderr, pData->curFrame.pData, 196, 1);

  while(pData->curFrame.lenData > 0) {

    if(pData->curFrame.lenData < 4) {
      return  STREAM_NET_ADVFR_RC_NOTAVAIL;
    }

    //fprintf(stderr, "xcode_vid_seqhdr_h264 0x%x 0x%x 0x%x 0x%x 0x%x\n", pData->curFrame.pData[0],pData->curFrame.pData[1], pData->curFrame.pData[2], pData->curFrame.pData[3], pData->curFrame.pData[4]);

    // Skip past annexB NAL headers
    if(*((uint32_t *) pData->curFrame.pData) == H264_SEQHDR) {
      pData->curFrame.pData += 4;
      pData->curFrame.lenData -= 4;
    }

//fprintf(stderr, "NAL 0x%x\n", *pData->curFrame.pData);

    // Skip past AUD NAL
    if((*pData->curFrame.pData & NAL_TYPE_MASK) == NAL_TYPE_ACCESS_UNIT_DELIM) {

      if(pData->curFrame.lenData < 2) {
        return  STREAM_NET_ADVFR_RC_NOTAVAIL;
      }
      pData->curFrame.pData += 2;
      pData->curFrame.lenData -= 2;

    } else if((*pData->curFrame.pData & NAL_TYPE_MASK) == NAL_TYPE_SEI) {

      H264_INIT_BITPARSER(sc.bs, pData->curFrame.pData, pData->curFrame.lenData);
      sc.startCodeMatch = 0;

      sz = pData->curFrame.lenData;
      if((pos = h264_findStartCode(&sc, 0, NULL)) > 4) {
        sz = pos - 4;
      } else if(pos > (int) pData->curFrame.lenData) {
        sz = pos = pData->curFrame.lenData;
      }
      sc.bs.sz = sz;
      if(sz > (int) pData->curFrame.lenData) {
        LOG(X_ERROR("SEI length %d exceeds past end of NAL length %d"), sz, 
              pData->curFrame.lenData);
        return STREAM_NET_ADVFR_RC_NOTAVAIL;
      }
 
      pData->curFrame.pData += sz;
      pData->curFrame.lenData -= sz;

//fprintf(stderr, "SEI LEN:%d 0x%x 0x%x 0x%x 0x%x 0x%x\n", sz, pData->curFrame.pData[0], pData->curFrame.pData[1], pData->curFrame.pData[2], pData->curFrame.pData[3], pData->curFrame.pData[4]);

    // Parse SPS / PPS
    } else if((*pData->curFrame.pData & NAL_TYPE_MASK) == NAL_TYPE_SEQ_PARAM ||
              (*pData->curFrame.pData & NAL_TYPE_MASK) == NAL_TYPE_PIC_PARAM) {

//fprintf(stderr, "INLINE SPS/PPS 0x%x\n", *pData->curFrame.pData);
 
      H264_INIT_BITPARSER(sc.bs, pData->curFrame.pData, pData->curFrame.lenData);
      sc.startCodeMatch = 0;

      sz = pData->curFrame.lenData;
      if((pos = h264_findStartCode(&sc, 1, NULL)) > 4) {
        sz = pos - 3;
//fprintf(stderr, "SZ:%d POS:%d 0x%x 0x%x 0x%x 0x%x\n", sz, pos, sc.bs.buf[sz], sc.bs.buf[sz+1], sc.bs.buf[sz+2], sc.bs.buf[sz+3]);
        if(sz > 0 && sc.bs.buf[sz - 1] == 0x00) {
          sz--;
        } 
      } else if(pos > (int) pData->curFrame.lenData) {
        sz = pos = pData->curFrame.lenData;
      }

      sc.bs.sz = sz;
//fprintf(stderr, "SZ:%d\n", sz); 
      if((h264rc = h264_decode_NALHdr(&pUData->uin.xh264.h264, &sc.bs, 0)) != H264_RESULT_DECODED) {

        if(h264rc == H264_RESULT_IGNORED) {
          LOG(X_ERROR("Ignoring H.264 NAL: 0x%x len:%d"), pData->curFrame.pData[0],
                      pData->curFrame.lenData);
          return STREAM_NET_ADVFR_RC_NOTAVAIL;
        } else {
          LOG(X_ERROR("Failed to decode H.264 NAL: 0x%x len:%d"), pData->curFrame.pData[0],
                      pData->curFrame.lenData);
          avc_dumpHex(stderr, pData->curFrame.pData, 48, 1);
          return STREAM_NET_ADVFR_RC_ERROR;
        }
      }

      // Store raw SPS / PPS
      if((*pData->curFrame.pData & NAL_TYPE_MASK) == NAL_TYPE_SEQ_PARAM) {

        if((pUData->uin.xh264.spspps.sps_len = sz) > sizeof(pUData->uin.xh264.spspps.sps_buf)) {
          pUData->uin.xh264.spspps.sps_len = sizeof(pUData->uin.xh264.spspps.sps_buf); 
        }
 
        pUData->uin.xh264.spspps.sps = pUData->uin.xh264.spspps.sps_buf;
        memcpy(pUData->uin.xh264.spspps.sps_buf, pData->curFrame.pData, 
               pUData->uin.xh264.spspps.sps_len);

      } else if((*pData->curFrame.pData & NAL_TYPE_MASK) == NAL_TYPE_PIC_PARAM) {

        if((pUData->uin.xh264.spspps.pps_len = sz) > sizeof(pUData->uin.xh264.spspps.pps_buf)) {
          pUData->uin.xh264.spspps.pps_len = sizeof(pUData->uin.xh264.spspps.pps_buf); 
        }

        pUData->uin.xh264.spspps.pps = pUData->uin.xh264.spspps.pps_buf;
        memcpy(pUData->uin.xh264.spspps.pps_buf, pData->curFrame.pData, 
               pUData->uin.xh264.spspps.pps_len);
      }

      if((pUData->uin.xh264.h264.flag & H264_DECODER_CTXT_HAVE_SPS) &&
         (pUData->uin.xh264.h264.flag & H264_DECODER_CTXT_HAVE_PPS)) { 

        seqHdrWidth = pUData->seqHdrWidth;
        seqHdrHeight = pUData->seqHdrHeight;
        h264_getCroppedDimensions(&pUData->uin.xh264.h264.sps[
           pUData->uin.xh264.h264.sps_idx], &pUData->seqHdrWidth, 
           &pUData->seqHdrHeight , NULL, NULL);                                  

        if(pUData->seqHdrWidth != seqHdrWidth || pUData->seqHdrHeight != seqHdrHeight) {
          LOG(X_DEBUG("H.264 input resolution %dx%d"), pUData->seqHdrWidth, pUData->seqHdrHeight);
        }

        if(pUData->uin.xh264.h264.sps[pUData->uin.xh264.h264.sps_idx].vui.timing_info_present) {
          pXcode->inClockHz = 
             pUData->uin.xh264.h264.sps[pUData->uin.xh264.h264.sps_idx].vui.timing_timescale / 2;
          pXcode->inFrameDeltaHz = 
             pUData->uin.xh264.h264.sps[pUData->uin.xh264.h264.sps_idx].vui.timing_num_units_in_tick;
          if(pXcode->common.decodeInIdx == 0) {
            LOG(X_DEBUG("Using SPS VUI fps %u/%u"), pXcode->inClockHz, pXcode->inFrameDeltaHz);
          }

        } else if(pXcode->common.cfgCFRInput && (
                  pXcode->inClockHz == 0 || pXcode->inFrameDeltaHz == 0)) {

          //TODO: need to extrapolate input fps - esp for FLV
          LOG(X_ERROR("Input fps set as CFR but SPS VUI does not include timing"));

          return STREAM_NET_ADVFR_RC_ERROR;
        } else {
          if(pXcode->common.decodeInIdx == 0) {
            LOG(X_DEBUG("Using video input fps %u/%u"), pXcode->inClockHz, pXcode->inFrameDeltaHz);
          }
        }

        if(memcmp_spspps(&pUData->uin.xh264.spspps, &pUData->uin.xh264.spsppsprev)) {
          if(pUData->uin.xh264.spsppsprev.sps_len > 0) {
            *pInputResChange = 1;
            LOG(X_WARNING("Detected input h264 SPS/PPS change"));
//fprintf(stderr, "PREV SPS LEN:%d, CUR LEN:%d, PPS LEN:%d, CUR PPS LEN:%d\n", pUData->uin.xh264.spsppsprev.sps_len, pUData->uin.xh264.spspps.sps_len, pUData->uin.xh264.spsppsprev.pps_len, pUData->uin.xh264.spspps.pps_len);
          }
          memcpy_spspps(&pUData->uin.xh264.spsppsprev, &pUData->uin.xh264.spspps);
        }

        if(!pUData->haveSeqStart) {

          //fprintf(stderr, "xcode_setVidSeqStart\n");
          if(xcode_setVidSeqStart(pXcode, pData) < 0) {
            return -1; 
          }

          // The encoder lookahead may not be set from xcode_parse_configstr
          // if no user specified frame rate is provided, hence it should be
          // set here
   
          //TODO: need to dynamically extrapolate input fps, if inClockHz not set
          if(pXcode->out[outidx].resLookaheadmin1 == 0 && 
            (pXcode->out[outidx].cfgLookaheadmin1 == XCODE_VID_LOOKAHEAD_AUTO_FLAG) &&
             pXcode->inClockHz > 0 && pXcode->inFrameDeltaHz > 0 &&
             ((float) pXcode->inClockHz / pXcode->inFrameDeltaHz) <= XCODE_VID_LOOKAHEAD_MIN_FPS) {
            LOG(X_WARNING("(auto) setting minimum encoder latency for low fps %u/%u"),
                 pXcode->inClockHz,  pXcode->inFrameDeltaHz);
            pXcode->out[outidx].resLookaheadmin1 = 1;
          }

          // 
          // Include AVCC decoder config for decoder 
          //
          if((sz = avcc_create(pXcode->extradata, sizeof(pXcode->extradata),
                      &pUData->uin.xh264.spspps, 4)) < 0) {
            LOG(X_ERROR("Failed to create avcc from SPS PPS"));
            return STREAM_NET_ADVFR_RC_ERROR;
          }
          pXcode->extradatasize = sz;

        }
      }

      if(pos > 0) {
        pData->curFrame.pData += pos;
        pData->curFrame.lenData -= pos;
      } else {
        return STREAM_NET_ADVFR_RC_NOTAVAIL;
      }

    } else {

      if(pData->curFrame.lenData + 4 > lenDataOrig) {
        LOG(X_ERROR("Invalid nal position (%u/%u) for pts:%llu"), 
           pData->curFrame.lenData, lenDataOrig, pData->curFrame.pkt.xtra.tm.pts);
        return STREAM_NET_ADVFR_RC_ERROR;
      }

      //
      // Create an AVC nal length field
      //
      pData->curFrame.pData = pDataOrig;
      pData->curFrame.lenData = lenDataOrig;

//fprintf(stderr, "dumpin\n"); avc_dumpHex(stderr, pData->curFrame.pData, 32, 1);

      convert_slices_h264b_avc(&pData->curFrame.pData, &pData->curFrame.lenData);
      break;
    }

  }

  return rc;
}

#if 0
static int g_fd;
static int g_idx;
static void broken_bframes(const STREAM_XCODE_DATA_T *pData, int64_t pts_adjustment) {
  PESXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  PESXCODE_VID_UDATA_T *pUData = (PESXCODE_VID_UDATA_T *)pXcode->pUserData;


  if(!g_fd) g_fd = open("test.h264", O_RDWR | O_CREAT, 0666);
  write(g_fd,  pData->curFrame.pData,  pData->curFrame.lenData);

  fprintf(stderr, "h.264 [%d] len:%5d pts:%.3f dts:%.3f adj:%.3f poc:%d(%d) {0x%x 0x%x 0x%x 0x%x 0x%x 0x%x}\n", g_idx++, pData->curFrame.lenData, PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.pkt.xtra.tm.dts), PTSF(pts_adjustment), pUData->uout.xh264.h264.picOrderCnt, pUData->uout.xh264.expectedPoc, pData->curFrame.pData[0], pData->curFrame.pData[1], pData->curFrame.pData[2], pData->curFrame.pData[3], pData->curFrame.pData[4], pData->curFrame.pData[5]);

}

#endif


int xcode_vid_h264_check(STREAM_XCODE_DATA_T *pData, unsigned int outidx) {
  int rc = 0;
  int irc;
  int64_t pts_adjustment = 0;
  const unsigned char *pSlice0 = NULL;
  int pocDifference;
  ptrdiff_t prebufOffset = 0; 
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;
  unsigned char **ppData = &pData->curFrame.xout.outbuf.poffsets[outidx];
  int *pLenData = &pData->curFrame.xout.outbuf.lens[outidx];
  XCODE_H264_OUT_T *pxh264 = &pUData->out[outidx].uout.xh264;
  uint64_t *p_pts = &pData->curFrame.xout.tms[outidx].pts;
  //int64_t *p_dts = &pData->curFrame.xout.tms[outidx].dts;

  pSlice0 = *ppData;

  //fprintf(stderr, "(start) 264_check outidx:%d, hdrLen:%d, *pLenD:%d\n", outidx, pXcode->out[outidx].hdrLen, *pLenData); avc_dumpHex(stderr, pSlice0, 16, 1);

  //
  // Check for key frame
  //
  if((irc = xcode_h264_find_spspps(pxh264, pSlice0, *pLenData, NULL)) > 0) {
    //fprintf(stderr, "IRC:%d\n", irc);
    pData->curFrame.xout.keyframes[outidx] = 1;

    if(irc >= H264_STARTCODE_LEN) {
      irc -= H264_STARTCODE_LEN;
    }

    //
    // irc may skip over any SEIs
    //
    if(irc > 0) {
      (*ppData) += irc;
      if((*pLenData) >= irc) {
        (*pLenData) -= irc;
      } else {
        LOG(X_ERROR("Invalid output I-Frame offset in frame %d / %d"), irc, *pLenData);
        *pLenData = 0;
      }
    }

    //
    // Stuff the SPS PPS before I-frame contents
    //
    if(pXcode->out[outidx].hdrLen > 0) {

      irc = xcode_h264_find_spspps(pxh264, pXcode->out[outidx].hdr, 
                                   pXcode->out[outidx].hdrLen, pxh264->pSpspps);

      prebufOffset = pData->curFrame.pData - pData->curFrame.pkt.pBuf;

      if(prebufOffset < (ptrdiff_t) pXcode->out[outidx].hdrLen) {

        LOG(X_WARNING("queue slot prebuf size %d < H.264 SPS / PPS size %d"),
            prebufOffset, pXcode->out[outidx].hdrLen);
      } else {

        //
        // Copy the encoder's "hdr" contents to directly precede the I frame
        //
        memcpy(*ppData - pXcode->out[outidx].hdrLen, pXcode->out[outidx].hdr, 
               pXcode->out[outidx].hdrLen);
        (*ppData) -= pXcode->out[outidx].hdrLen;
        (*pLenData) += pXcode->out[outidx].hdrLen;
         //fprintf(stderr, "INSERTED SPS for outidx[%d]\n", outidx);avc_dumpHex(stderr, *ppData, MIN(*pLenData, 48), 1);
      }

    }

  } else {
    pData->curFrame.xout.keyframes[outidx] = 0;
  }

  //
  // Adjust dts based on NAL slice header pic order cnt value
  // We do this becaue x264 pts/dts output is applying pts to
  // I/P frames instead of B frames.  Using direct 90Khz offsets 
  // based on these values produces very jerky video in VLC
  //

  //fprintf(stderr, "CALLING XCODE_H264_DECODENALH outidx[%d] len:%d, keyf:%d pSpspps:0x%x, sps[%d]:%d, pps[%d]:%d, flag:0x%x\n",  outidx, pData->curFrame.xout.outbuf.lens[outidx], pData->curFrame.xout.keyframes[outidx], pxh264->h264.sps_idx, pxh264->pSpspps, pxh264->h264.sps[pxh264->h264.sps_idx].is_ok, pxh264->h264.pps_idx, pxh264->h264.pps[pxh264->h264.pps_idx].is_ok, pxh264->h264.flag); //avc_dumpHex(stderr, pData->curFrame.xout.outbuf.poffsets[outidx], MIN(16, pData->curFrame.xout.outbuf.lens[outidx]), 1);

  if(1) {

    //
    // Adjust dts based on x264 output pts/dts 
    // This may not have worked correctly for older x264 ver ~ 20100605-2245
    //

    if(pXcode->out[outidx].dts != 0) {
      pts_adjustment = (int64_t) ( (90000.0f * (float)pXcode->resOutFrameDeltaHz / 
                       pXcode->resOutClockHz) *  pXcode->out[outidx].dts );
    }

  } else {

    //
    // Adjust dts based on this slice POC / expected POC calculation
    //

    if(!(pxh264->h264.flag & (H264_DECODER_CTXT_HAVE_SPS | H264_DECODER_CTXT_HAVE_PPS))) {
      // SPS, PPS not yet detected
    }
    //TODO: this appears to not work correctly for newer x264 ver > 20100605-2245
    else if(xcode_h264_decodenalhdr(pxh264, pSlice0, *pLenData - (ptrdiff_t) (pSlice0 - *ppData)) == 0) {

      //fprintf(stderr, "264_check outidx:%d\n", outidx); avc_dumpHex(stderr, pSlice0, 16, 1);

      //
      // Adjust for encoder decoder timestamp
      //
      if(pxh264->h264.picOrderCnt !=  pxh264->expectedPoc) {

        pocDifference = (int)pxh264->expectedPoc - (int)pxh264->h264.picOrderCnt;
        //if((pocDifference = (int)pxh264->expectedPoc - (int)pxh264->h264.picOrderCnt) > 16 &&
        //   pxh264->h264.picOrderCnt < pxh264->lastPoc) {
        //}

        pts_adjustment = (int64_t) ( (90000.0f * (float)pXcode->resOutFrameDeltaHz / 
                     pXcode->resOutClockHz)* ( pocDifference / pxh264->pocDelta));
      }

      pxh264->lastPoc = pxh264->h264.picOrderCnt;

    }

  }

  if(pts_adjustment != 0) {

    if(ABS(pts_adjustment) > 16 * (90000.0f * (float)pXcode->resOutFrameDeltaHz /
                                               pXcode->resOutClockHz)) {
      LOG(X_WARNING("Ignoring possibly incorrect out[%d] pts offset %.3f, pts:%.3f "
                    "(poc expected:%d, poc:%d)"),
              outidx, PTSF(pts_adjustment), PTSF(*p_pts),
            pxh264->expectedPoc, pxh264->h264.picOrderCnt);
      pts_adjustment = 0;
      //avc_dumpHex(stderr, pData->curFrame.pData , (pData->curFrame.lenData < 32 ? pData->curFrame.lenData : 32), 1);
    }

    if(pts_adjustment > 0 && *p_pts < (uint64_t) pts_adjustment) {
      LOG(X_WARNING("Unable to adjust h.264 PTS %llu by: %lld"), *p_pts, (-1 * pts_adjustment));
    } else {
      *p_pts -= pts_adjustment;
    }
    //fprintf(stderr, "xcode_vid outidx[%d] pxh264:0x%x pts_adjustment: %lld exp_poc:%d poc:%d (%d) assumed poc_delta:%d pts:%.3f dts:%.3f key:%d, dts from xcode:%lld\n", outidx, pxh264, pts_adjustment, pxh264->expectedPoc, pxh264->h264.picOrderCnt, pocDifference, pxh264->pocDelta, PTSF(*p_pts), PTSF(*p_dts),pData->curFrame.xout.keyframes[outidx], pXcode->out[outidx].dts);

  } else {
    //fprintf(stderr, "xcode outidx[%d] pxh264:0x%x exp_poc:%d poc:%d poc_delta:%d\n", outidx, pxh264, pxh264->expectedPoc, pxh264->h264.picOrderCnt, pxh264->pocDelta);

  }


  //
  // Adjust dts based on x264 output pts/dts - not done anymore since 
  // x264 seems to return incorrect times
  //
  //if(pXcode->pts != pXcode->dts) {
  //  *p_pts += (pUData->ptsdts90Khzoffset * (90000.0f * (float)pXcode->outFrameDeltaHz / pXcode->outClockHz)* (pXcode->pts - pXcode->dts));

//fprintf(stderr, "ptsdts multiplier %f %f %lld\n", pUData->ptsdts90Khzoffset, (90000.0f * (float)pXcode->outFrameDeltaHz / pXcode->outClockHz) , (pXcode->pts - pXcode->dts));
//fprintf(stderr, "pts:%lld dts:%lld (%lld)\n", pXcode->pts, pXcode->dts, pXcode->pts - pXcode->dts);
  //}
      
  //broken_bframes(pData, pts_adjustment);

  return rc;
}

