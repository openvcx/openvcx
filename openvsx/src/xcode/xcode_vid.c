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

//
// Avoid using dts=0 for video stream even w/ matching pts/dts
//
#define INIT_PTSDTS(pQTm) if((pQTm).dts != 0) { \
                            (pQTm).pts = (pQTm).dts; } (pQTm).dts = (pQTm).pts;

//
// Set the Dts pts/dts to that of the Src
//
#define SET_PTSDTS(pQTmDst, pQTmSrc) \
                       (pQTmDst).pts = (pQTmSrc).pts; \
                       if((pQTmSrc).dts != 0) { (pQTmDst).dts = (pQTmSrc).dts; } \
                       else { (pQTmDst).dts = (pQTmSrc).pts; } 

//#define XCODE_STD_DEV 1
#if defined(XCODE_STD_DEV) 
typedef struct STD_DEV {
#define STD_DEV_CNT     2000
  float           f[STD_DEV_CNT];
  unsigned int    cnt;
} STD_DEV_T;

static STD_DEV_T g_std_dev;
int std_dev_add(STD_DEV_T *pS, float f) {
  if(pS->cnt >= STD_DEV_CNT) {
    return -1;
  }
  pS->f[pS->cnt++] = f;
  return 0;
}
void std_dev_print(STD_DEV_T *pS) {
  float avg = 0;
  float diff = 0;
  float std;
  unsigned int idx;

  for(idx = 0; idx < pS->cnt; idx++) {
    avg += pS->f[idx];
  }

  avg /= pS->cnt;

  for(idx = 0; idx < pS->cnt; idx++) {
    diff += pow(pS->f[idx] - avg, 2);
  }

  std = sqrt(diff / pS->cnt);

  fprintf(stderr, "AVG: %.3f, STD:%.3f, CNT:%d\n", avg, std, pS->cnt);

}
#endif // (XCODE_STD_DEV) 

static int setfps(IXCODE_VIDEO_CTXT_T *pXcode) {

  if(pXcode->cfgOutClockHz > 0) {
    pXcode->resOutClockHz = pXcode->cfgOutClockHz;
    pXcode->resOutFrameDeltaHz = pXcode->cfgOutFrameDeltaHz;
  } else if(pXcode->inClockHz == 0 || pXcode->inFrameDeltaHz == 0) {
    LOG(X_ERROR("No configured output frame rate set and input frame rate unknown or invalid"));
    return -1;
  } else {
    pXcode->resOutClockHz = pXcode->inClockHz;
    pXcode->resOutFrameDeltaHz = pXcode->inFrameDeltaHz;
  }

  if(!pXcode->common.cfgAllowUpsample &&
    ((float)pXcode->inClockHz / pXcode->inFrameDeltaHz) <
    ((float)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz)) {

    LOG(X_WARNING("Not adhering to output fps:%.3f > input fps:%.3f because upsampling not explicitly enabled."),
        ((float)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz),
        ((float)pXcode->inClockHz / pXcode->inFrameDeltaHz));

    pXcode->resOutClockHz = pXcode->inClockHz;
    pXcode->resOutFrameDeltaHz = pXcode->inFrameDeltaHz;

  }

  LOG(X_DEBUG("Set encoder output fps to %u/%u upsampling:%d cfr:%d"),
      pXcode->resOutClockHz, pXcode->resOutFrameDeltaHz, 
      pXcode->common.cfgAllowUpsample, pXcode->common.cfgCFROutput);

  return 0;
}

static int setresolution(IXCODE_VIDEO_OUT_T *pXcodeOut, const STREAM_XCODE_VID_UDATA_T *pUData) {
  float aspectr = 0.0f;

  if(pXcodeOut->cfgOutV > 0 && pXcodeOut->cfgOutH > 0) {

    pXcodeOut->resOutH = pXcodeOut->cfgOutH;
    pXcodeOut->resOutV = pXcodeOut->cfgOutV;

  } else {

    if(pUData->seqHdrWidth > 0 &&  pUData->seqHdrHeight > 0) {
      if(IXCODE_FILTER_ROTATE_90(*pXcodeOut)) {
        aspectr = (float) pUData->seqHdrHeight / pUData->seqHdrWidth;
      } else {
        aspectr = (float) pUData->seqHdrWidth / pUData->seqHdrHeight;
      }
    }
    if(aspectr == 0.0f) {
      //if(pXcodeOut->cfgOutH && pXcodeOut->cfgOutV) {
      //  LOG(X_ERROR("Input video dimensions set %dx%d  output w x h not given."),
      //      pUData->seqHdrWidth, pUData->seqHdrHeight);
      //} else {
        LOG(X_ERROR("Input video dimensions not set %dx%d and both output %dx%d not given."),
            pUData->seqHdrWidth, pUData->seqHdrHeight, pXcodeOut->cfgOutH, pXcodeOut->cfgOutV);
      //}
      return -1;
    }
    if(pXcodeOut->cfgOutV) {
      pXcodeOut->resOutV = pXcodeOut->cfgOutV;
      pXcodeOut->resOutH = (unsigned int) (aspectr * (float)pXcodeOut->cfgOutV);
    } else if(pXcodeOut->cfgOutH) {
      pXcodeOut->resOutH = pXcodeOut->cfgOutH;
      pXcodeOut->resOutV = (unsigned int) ((float)pXcodeOut->cfgOutH / aspectr);
    } else {
      if(IXCODE_FILTER_ROTATE_90(*pXcodeOut)) {
        pXcodeOut->resOutV = pUData->seqHdrWidth;
        pXcodeOut->resOutH = pUData->seqHdrHeight;
      } else {
        pXcodeOut->resOutH = pUData->seqHdrWidth;
        pXcodeOut->resOutV = pUData->seqHdrHeight;
      }
    }
  }

  if(pXcodeOut->resOutH & 0x01) {
    pXcodeOut->resOutH++;
  }
  if(pXcodeOut->resOutV & 0x01) {
    pXcodeOut->resOutV++;
  }

  LOG(X_DEBUG("Video output dimensions set to %dx%d"), pXcodeOut->resOutH, pXcodeOut->resOutV);

  return 0;
}

int xcode_setVidSeqStart(IXCODE_VIDEO_CTXT_T  *pXcode, STREAM_XCODE_DATA_T *pData) {

  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *) pXcode->pUserData;
  int rc = 0;

  if(pUData->delayedCfgDone || !(pUData->flags & STREAM_XCODE_FLAGS_LOAD_XCFG)) {
    if((rc = setfps(pXcode)) < 0) {
      return rc;
    }
  }

  pUData->haveSeqStart = 1;
  pData->curFrame.tm.haveseqstart = 1;
  if(pData->pComplement) {
    pData->pComplement->curFrame.tm.haveseqstart = 1;

    if(pData->pComplement->piXcode) {
      ((STREAM_XCODE_AUD_UDATA_T *)pData->pComplement->piXcode->aud.pUserData)->haveSeqStart = 1;
    }
  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_mpeg2(STREAM_XCODE_DATA_T *pData, 
                                                       IXCODE_VIDEO_CTXT_T  *pXcode, 
                                                       int *pInputResChange) {
  MPEG2_SEQ_HDR_T mpeg2seqHdr;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

  if(mpg2_check_seq_hdr_start(pData->curFrame.pData, pData->curFrame.lenData)) {

    memset(&mpeg2seqHdr, 0, sizeof(mpeg2seqHdr));

    if(mpg2_parse_seq_hdr(&pData->curFrame.pData[4],
            pData->curFrame.lenData- 4, &mpeg2seqHdr) < 0) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    if(memcmp(&pUData->uin.xmpeg2, &mpeg2seqHdr, sizeof(mpeg2seqHdr))) {

      if((pUData->uin.xmpeg2.hpx != 0 && (mpeg2seqHdr.hpx != pUData->uin.xmpeg2.hpx)) ||
         (pUData->uin.xmpeg2.vpx != 0 && (mpeg2seqHdr.vpx != pUData->uin.xmpeg2.vpx))) {

        *pInputResChange = 1;

        LOG(X_WARNING("Detected input mpeg2 resolution change %dx%d -> %dx%d"),
              pUData->uin.xmpeg2.hpx, pUData->uin.xmpeg2.vpx,
              mpeg2seqHdr.hpx, mpeg2seqHdr.vpx);
      }
      memcpy(&pUData->uin.xmpeg2, &mpeg2seqHdr, sizeof(mpeg2seqHdr));
      pUData->seqHdrWidth = pUData->uin.xmpeg2.hpx;
      pUData->seqHdrHeight = pUData->uin.xmpeg2.vpx;
    }

    mpg2_get_fps(&pUData->uin.xmpeg2, &pXcode->inClockHz, &pXcode->inFrameDeltaHz);
    if(pXcode->inClockHz == 0 || pXcode->inFrameDeltaHz == 0) {
      LOG(X_ERROR("Unable to get framerate from mpeg2 sequence header (%u/%u)"),
                  pXcode->inClockHz, pXcode->inFrameDeltaHz);
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
      return -1;
    }

  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_mpg4v(STREAM_XCODE_DATA_T *pData, 
                                                       IXCODE_VIDEO_CTXT_T  *pXcode, 
                                                       int *pInputResChange) {
  MPG4V_DESCR_PARAM_T param;
  STREAM_CHUNK_T stream;
  BIT_STREAM_T bs;
  uint8_t type;
  int idx;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;
  MPG4V_DESCR_PARAM_T *pParam = &pUData->uin.xmpg4v.mpg4v.param;

  //fprintf(stderr, "xcode_vid_seqhdr_mpg4v type: 0x%x len:%d\n", pData->curFrame.pData[3], pData->curFrame.lenData);

  if(pData->curFrame.lenData < 4 ||
     *((uint32_t *) pData->curFrame.pData) != htonl(MPEG4_HDR32_VOS)) {
    return rc;
  }

  memset(&stream, 0, sizeof(stream));
  stream.bs.byteIdx = 3;
  stream.bs.buf = pData->curFrame.pData;
  stream.bs.sz = pData->curFrame.lenData;

  while(stream.bs.byteIdx < stream.bs.sz) {

    type = stream.bs.buf[stream.bs.byteIdx];

    //fprintf(stderr, "mpg4v slice idx[%d/%d] type:0x%x)\n", stream.bs.byteIdx, stream.bs.sz,type);

    if(MPEG4_ISVOL(type)) {

      memcpy(&bs, &stream.bs, sizeof(bs));
      //bs.sz = bs.byteIdx + idx - 3; 
      if(mpg4v_decodeVOL(&bs, &param) < 0) {
        return STREAM_NET_ADVFR_RC_ERROR;
      }

      //fprintf(stderr, "decoded VOL prev:%u/%u new:%u/%u haveSeqStart:%d\n", pParam->clockHz, pParam->frameDeltaHz, param.clockHz, param.frameDeltaHz, ((STREAM_XCODE_VID_UDATA_T *) pXcode->pUserData)->haveSeqStart);

      //TODO: we should probably store all the sequence header slices
      // and check for any single change
      if(memcmp(&param, pParam, sizeof(param))) {

        LOG(X_DEBUG("Found MPEG-4 VOL fps: %d/%d, %dx%d"), param.clockHz, param.frameDeltaHz, 
                                                  param.frameWidthPx, param.frameHeightPx);


        if((pParam->frameWidthPx != 0 && (param.frameWidthPx != pParam->frameWidthPx)) ||
          (pParam->frameHeightPx != 0 && (param.frameHeightPx != pParam->frameHeightPx))) {
          *pInputResChange = 1;

          LOG(X_WARNING("Detected input MPEG-4 resolution change %dx%d -> %dx%d"),
              pParam->frameWidthPx, pParam->frameHeightPx, 
              param.frameWidthPx, param.frameHeightPx);
        }
        pUData->seqHdrWidth = param.frameWidthPx;
        pUData->seqHdrHeight = param.frameHeightPx;

        memcpy(pParam, &param, sizeof(param));

        if(pXcode->common.cfgCFRInput) {

          if(param.clockHz == 0 || param.frameDeltaHz == 0) {
            LOG(X_ERROR("Unable to get framerate from MPEG-4 sequence header (%u/%u)"),
                   param.clockHz, param.frameDeltaHz);
            return STREAM_NET_ADVFR_RC_ERROR;
          } else if(pXcode->inClockHz > 0 && pXcode->inFrameDeltaHz > 0 &&
                    ((double)param.clockHz / param.frameDeltaHz) > MPEG4_MAX_FPS_SANITY) {
            LOG(X_WARNING("Refusing to set clock to %d/%d"), param.clockHz, param.frameDeltaHz);
          } else {
            pXcode->inClockHz = param.clockHz;
            pXcode->inFrameDeltaHz = param.frameDeltaHz;
          }

        }

        if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
          return -1;
        }

      } // end of memcmp(&param ...

    //} else if(type == MPEG4_HDR8_VOS) {
    //  haveHdrsInFrame = 1;
    //} else {
    //  break;
    }

    stream.bs.byteIdx++;
    if((idx = mpg2_findStartCode(&stream)) < 3) {
      break;
    }

    stream.bs.byteIdx += idx;
    //fprintf(stderr, "byteidx now at%d (%d +1)\n", stream.bs.byteIdx, idx);
  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_h263(STREAM_XCODE_DATA_T *pData, 
                                                       IXCODE_VIDEO_CTXT_T  *pXcode, 
                                                       int *pInputResChange) {
  //MPG4V_DESCR_PARAM_T param;
  //STREAM_CHUNK_T stream;
  //BIT_STREAM_T bs;
  //uint8_t type;
  //int idx;
  //enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  H263_DESCR_T h263;
  BIT_STREAM_T bs;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

  memset(&h263, 0, sizeof(h263));
  memset(&bs, 0, sizeof(bs));
  bs.buf  = pData->curFrame.pData;
  bs.sz = pData->curFrame.lenData;

  if(h263_decodeHeader(&bs, &h263, 0) < 0) {
    LOG(X_ERROR("Failed to decode xcode input H.263 header length %d"), pData->curFrame.lenData);
    LOGHEX_DEBUG(pData->curFrame.pData, MIN(16, pData->curFrame.lenData));
    return 0;
  }

  if(h263.frame.frameType == H263_FRAME_TYPE_I) {

    if((pUData->seqHdrWidth != 0 && (pUData->seqHdrWidth != h263.param.frameWidthPx)) ||
       (pUData->seqHdrHeight != 0 && (pUData->seqHdrHeight != h263.param.frameHeightPx))) {
      *pInputResChange = 1;

      LOG(X_WARNING("Detected input H.263 resolution change %dx%d -> %dx%d"),
          pUData->seqHdrWidth, pUData->seqHdrHeight, h263.param.frameWidthPx, h263.param.frameHeightPx);
    }
    pUData->seqHdrWidth = h263.param.frameWidthPx;
    pUData->seqHdrHeight = h263.param.frameHeightPx;

    if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
      return -1;
    }

  }

  return 0;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_vp6(STREAM_XCODE_DATA_T *pData, 
                                                     IXCODE_VIDEO_CTXT_T  *pXcode, 
                                                     int *pInputResChange) {
  int wid, ht;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

  //avc_dumpHex(stderr,pData->curFrame.pData, pData->curFrame.lenData > 16 ? 16 : pData->curFrame.lenData, 1);

  if(pData->curFrame.lenData < 1) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  //
  // keyframe check
  //
  if(!(pData->curFrame.pData[0] & 0x80)) {

    if(pData->curFrame.lenData < 6) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    ht = pData->curFrame.pData[2] * 16;
    wid = pData->curFrame.pData[3] * 16;

    if((pUData->seqHdrWidth != 0 && (pUData->seqHdrWidth != wid)) ||
       (pUData->seqHdrHeight != 0 && (pUData->seqHdrHeight != ht))) {
      *pInputResChange = 1;

      LOG(X_WARNING("Detected input vp6 resolution change %dx%d -> %dx%d"),
          pUData->seqHdrWidth, pUData->seqHdrHeight, wid, ht);
    }
    pUData->seqHdrWidth = wid;
    pUData->seqHdrHeight = ht;

    if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
      return -1;
    }
  }

  return 0;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_vp8(STREAM_XCODE_DATA_T *pData, 
                                                     IXCODE_VIDEO_CTXT_T  *pXcode, 
                                                     int *pInputResChange) {
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;
  VP8_DESCR_T vp8;

  //avc_dumpHex(stderr,pData->curFrame.pData, pData->curFrame.lenData > 16 ? 16 : pData->curFrame.lenData, 1);

  if(pData->curFrame.lenData < 1) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  //
  // keyframe check
  //
  if(VP8_ISKEYFRAME(pData->curFrame.pData)) {

    if(vp8_parse_hdr(pData->curFrame.pData, pData->curFrame.lenData, &vp8) < 0) { 
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    if((pUData->seqHdrWidth != 0 && (pUData->seqHdrWidth != vp8.frameWidthPx)) ||
       (pUData->seqHdrHeight != 0 && (pUData->seqHdrHeight != vp8.frameHeightPx))) {

      *pInputResChange = 1;

      LOG(X_WARNING("Detected input vp8 resolution change %dx%d -> %dx%d"),
          pUData->seqHdrWidth, pUData->seqHdrHeight, vp8.frameWidthPx, vp8.frameHeightPx);
    }
    pUData->seqHdrWidth = vp8.frameWidthPx;
    pUData->seqHdrHeight = vp8.frameHeightPx;

    if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
      return -1;
    }

  } else {

  }

  return 0;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_raw(STREAM_XCODE_DATA_T *pData, 
                                                     IXCODE_VIDEO_CTXT_T  *pXcode, 
                                                     int *pInputResChange) {
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

  //if(pXcode->inClockHz == 0 || pXcode->inFrameDeltaHz == 0) {
  //  LOG(X_ERROR("Raw input video framerate not set (%u/%u)"),
  //              pXcode->inClockHz, pXcode->inFrameDeltaHz);
  //  return STREAM_NET_ADVFR_RC_ERROR;
  //} else 

  if((pXcode->inH == 0 || pXcode->inV == 0)) {
    LOG(X_ERROR("Raw input video dimensions not set (%u/%u)"),
                pXcode->inH, pXcode->inV);
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  pUData->seqHdrWidth = pXcode->inH;
  pUData->seqHdrHeight = pXcode->inV;

  if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
    return -1;
  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_conference(STREAM_XCODE_DATA_T *pData,
                                                     IXCODE_VIDEO_CTXT_T  *pXcode,
                                                     int *pInputResChange) {
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

  //if(pXcode->inClockHz == 0 || pXcode->inFrameDeltaHz == 0) {
  //  LOG(X_ERROR("Raw input video framerate not set (%u/%u)"),
  //              pXcode->inClockHz, pXcode->inFrameDeltaHz);
  //  return STREAM_NET_ADVFR_RC_ERROR;
  //} else 

  if(!pXcode->common.is_init && setresolution(&pXcode->out[0], pUData) < 0) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  pUData->seqHdrWidth = pXcode->inH = pXcode->out[0].resOutH;
  pUData->seqHdrHeight = pXcode->inV = pXcode->out[0].resOutV;

  if(!pUData->haveSeqStart && xcode_setVidSeqStart(pXcode, pData) < 0) {
    return -1;
  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr(STREAM_XCODE_DATA_T *pData,
                                                 IXCODE_VIDEO_CTXT_T  *pXcode,
                                                 int *pInputResChange) {
  static int filetype_not_set;
  enum STREAM_NET_ADVFR_RC rc;

  switch(pXcode->common.cfgFileTypeIn) {

    case XC_CODEC_TYPE_H262:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_mpeg2(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_H264:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_h264(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_MPEG4V:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_mpg4v(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_h263(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_VP6:
    case XC_CODEC_TYPE_VP6F:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_vp6(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_VP8:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_vp8(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_BMP:
    case XC_CODEC_TYPE_PNG:
    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_RGBA32:
    case XC_CODEC_TYPE_RAWV_BGR24:
    case XC_CODEC_TYPE_RAWV_RGB24:
    case XC_CODEC_TYPE_RAWV_BGR565:
    case XC_CODEC_TYPE_RAWV_RGB565:
    case XC_CODEC_TYPE_RAWV_YUV420P:
    case XC_CODEC_TYPE_RAWV_YUVA420P:
    case XC_CODEC_TYPE_RAWV_NV21:
    case XC_CODEC_TYPE_RAWV_NV12:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_raw(pData, pXcode, pInputResChange);
      break;

    case XC_CODEC_TYPE_VID_CONFERENCE:
      if(filetype_not_set > 0) {
        filetype_not_set = 0;
      }
      rc = xcode_vid_seqhdr_conference(pData, pXcode, pInputResChange);
      break;

    default:
      if(filetype_not_set++ % 100 == 0) {
        LOG(X_ERROR("Unsupported input video codec to transcode: %s (%d)"),  
             codecType_getCodecDescrStr(pXcode->common.cfgFileTypeIn), 
             pXcode->common.cfgFileTypeIn);
      }
      rc = STREAM_NET_ADVFR_RC_ERROR;
      break;
  }

  return rc;
}

static void xtime_frame_seq(STREAM_XCODE_DATA_T *pData)  {

  xcode_checkresetresume(pData);

  if(pData->curFrame.tm.haveseqstart) {
    if(!pData->curFrame.tm.prevhaveseqstart) {
      pData->curFrame.tm.seqstarttm = pData->curFrame.pkt.xtra.tm.pts;

      if(pData->curFrame.tm.seqstarttm > pData->curFrame.tm.starttmperiod) {
        pData->curFrame.tm.seqstartoffset = 
          pData->curFrame.tm.seqstarttm - pData->curFrame.tm.starttmperiod;
      } else {
        pData->curFrame.tm.seqstarttm = 0;
      }
      if(pData->pComplement) {
        pData->pComplement->curFrame.tm.seqstarttm = pData->curFrame.tm.seqstarttm; 
        pData->pComplement->curFrame.tm.seqstartoffset = pData->curFrame.tm.seqstartoffset; 
      }
      LOG(X_DEBUG("Set sequence headers starttm: %.3f"),PTSF(pData->curFrame.tm.seqstarttm)); 
    }
    pData->curFrame.tm.prevhaveseqstart = pData->curFrame.tm.haveseqstart;

  }

  //
  // If this frame is to be upsampled the time may not be set, 
  // so retain the previous frame input time 
  //
  if(pData->curFrame.numUpsampled == 0) {
    pData->curFrame.tm.intm.pts = pData->curFrame.pkt.xtra.tm.pts;
    pData->curFrame.tm.intm.dts = pData->curFrame.pkt.xtra.tm.dts;
  }

}

static void xtime_frame_in_post(STREAM_XCODE_DATA_T *pData, 
                               IXCODE_VIDEO_CTXT_T *pXcodeV)  {

  if(pXcodeV->common.cfgCFRInput) {
    
    pData->curFrame.tm.intm.pts  = (uint64_t) 
                           pData->curFrame.tm.seqstartoffset +
                          (((double) pData->curFrame.tm.frDecOutPeriod *
                          pXcodeV->inFrameDeltaHz / pXcodeV->inClockHz) * 90000);
    pData->curFrame.tm.intm.pts += pData->curFrame.tm.cfrInDriftAdj;

    if(pData->curFrame.numUpsampled == 0 &&
       (pData->curFrame.tm.intm.pts + pXcodeV->common.cfgCFRTolerance < 
        pData->curFrame.pkt.xtra.tm.pts ||
        pData->curFrame.tm.intm.pts >  pData->curFrame.pkt.xtra.tm.pts + 
        pXcodeV->common.cfgCFRTolerance)) {

      pData->curFrame.tm.cfrInDriftAdj = (int64_t) 
        pData->curFrame.pkt.xtra.tm.pts - (int64_t) pData->curFrame.tm.intm.pts;

      LOG(X_WARNING("CFR input drift %.3f -> %.3f, new drift adjustment: %.3f)"),
        PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.tm.intm.pts),
        PTSF(pData->curFrame.tm.cfrInDriftAdj));

      pData->curFrame.tm.intm.pts = pData->curFrame.pkt.xtra.tm.pts;

    }

    //fprintf(stderr, "cfr in %.3f -> %.3f\n", PTSF(pData->curFrame.pkt.xtra.tm.pts),PTSF(pData->curFrame.tm.intm.pts));
  } else {

    if(pData->curFrame.numUpsampled == 0) {
      pData->curFrame.tm.intm.pts = pData->curFrame.pkt.xtra.tm.pts; 
      //fprintf(stderr, "IN TM SET TO:%llu\n", pData->curFrame.tm.intm.pts);
    }

  }

  //fprintf(stderr, "ptsin %.3f -> %.3f decidx[%d,%d] encidx[%d,%d]\n", PTSF(pData->curFrame.pkt.xtra.tm.pts),PTSF(pData->curFrame.tm.intm.pts), pXcodeV->common.decodeInIdx, pXcodeV->common.decodeOutIdx, pXcodeV->common.encodeInIdx, pXcodeV->common.encodeOutIdx);

  pXcodeV->common.inpts90Khz = pData->curFrame.tm.intm.pts;

  //fprintf(stderr, "xtime_frame_in_post pts:%.3f xcoder inpts:%.3f (%llu) outpts%.3f (%llu) dec %d,%d enc:%d\n", PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pXcodeV->common.inpts90Khz), pXcodeV->common.inpts90Khz, PTSF(pXcodeV->common.outpts90Khz), pXcodeV->common.outpts90Khz, pData->curFrame.tm.frDecOutPeriod, pXcodeV->common.decodeOutIdx, pData->curFrame.tm.frEncOutPeriod);

  if(pXcodeV->common.decodeOutIdx != pData->curFrame.tm.frPrevDecOutPeriod) {
    pData->curFrame.tm.frDecOutPeriod++;
  }
  pData->curFrame.tm.frPrevDecOutPeriod = pXcodeV->common.decodeOutIdx; 

  pData->curFrame.tm.intm.dts = pData->curFrame.pkt.xtra.tm.dts;

}

static void xtime_frame_out_post(STREAM_XCODE_DATA_T *pData, 
                                 IXCODE_VIDEO_CTXT_T *pXcodeV)  {
  int xcoded = 0;
  uint64_t ptsdelta;

  if(pXcodeV->common.encodeOutIdx != pData->curFrame.tm.frPrevEncOutPeriod) {
    pData->curFrame.tm.frEncOutPeriod++;
    xcoded = 1;
  }
  pData->curFrame.tm.frPrevEncOutPeriod = pXcodeV->common.encodeOutIdx; 

  //fprintf(stderr, "xcode tm start inpts90:%.3f  outpts90:%.3f xcoded:%d\n", PTSF(pXcodeV->common.inpts90Khz), PTSF(pXcodeV->common.outpts90Khz), xcoded);

  if(pXcodeV->common.cfgCFROutput == 1) {

    //fprintf(stderr, "xtime_frame_out_post frPrevEncOutPeriod:%d frEncOutPeriod:%d encodeOutIdx:%d\n", pData->curFrame.tm.frPrevEncOutPeriod,pData->curFrame.tm.frEncOutPeriod,pXcodeV->common.encodeOutIdx);

    if(pData->curFrame.tm.frPrevEncOutPeriod < 1) {

      //
      // If the encoder is in the process of queing, ensure to produce output frames
      // the same rate  as subsequent output (even if in fps > out fps)
      //
      //pData->curFrame.tm.outtm.pts = pData->curFrame.pkt.xtra.tm.pts;
      pData->curFrame.tm.outtm.pts  = (uint64_t) pData->curFrame.tm.seqstartoffset +
                     pData->curFrame.tm.frEncPtsAtLastUpdate + 
                      (((double) (pXcodeV->common.encodeInIdx - pData->curFrame.tm.frEncodeInIdxAtLastUpdate) *
                     pXcodeV->resOutFrameDeltaHz / pXcodeV->resOutClockHz) * 90000);

    } else {

      pData->curFrame.tm.outtm.pts  = (uint64_t) pData->curFrame.tm.seqstartoffset +
            pData->curFrame.tm.encDelay + 
            pData->curFrame.tm.cfrOutDriftAdj +
            pData->curFrame.tm.frEncPtsAtLastUpdate +
            (((double) (MAX(1, pData->curFrame.tm.frEncOutPeriod) - 1) *
            pXcodeV->resOutFrameDeltaHz / pXcodeV->resOutClockHz) * 90000);

     //fprintf(stderr, "outtm.pts:%.3f, frEncOutPeriod:%d, frPrevEncOutPeriod:%d, encodeOutIdx:%d, frEncPtsAtLatsUpdate;%.3f\n", PTSF(pData->curFrame.tm.outtm.pts), pData->curFrame.tm.frEncOutPeriod, pData->curFrame.tm.frPrevEncOutPeriod, pXcodeV->common.encodeOutIdx, PTSF(pData->curFrame.tm.frEncPtsAtLastUpdate));

      if(pData->curFrame.numUpsampled == 0 &&
         pXcodeV->common.encodeOutIdx > 1 &&
         (pData->curFrame.tm.outtm.pts + pXcodeV->common.cfgCFRTolerance < 
          pData->curFrame.pkt.xtra.tm.pts ||
          pData->curFrame.tm.outtm.pts >  pData->curFrame.pkt.xtra.tm.pts + 
          pXcodeV->common.cfgCFRTolerance)) {

        LOG(X_DEBUG("CFR out seqst:%.3f, enclag:%.2f, drift:%.3f, tol:%dms, encfr:%d,%d(%d), "
                    "decfr:%d,%d, outfrtm:%.3f, infrtm:%.3f fpsout:%.3f"), 
          PTSF(pData->curFrame.tm.seqstartoffset), PTSF(pData->curFrame.tm.encDelay), 
          PTSF(pData->curFrame.tm.cfrOutDriftAdj), (int)PTSF_MS(pXcodeV->common.cfgCFRTolerance), 
          pXcodeV->common.encodeInIdx, 
          pXcodeV->common.encodeOutIdx, pData->curFrame.tm.frEncOutPeriod, 
          pXcodeV->common.decodeInIdx, pXcodeV->common.decodeOutIdx, 
          PTSF(pData->curFrame.tm.outtm.pts), 
          PTSF(pData->curFrame.pkt.xtra.tm.pts), 
          (double)pXcodeV->resOutClockHz/pXcodeV->resOutFrameDeltaHz);

        pData->curFrame.tm.cfrOutDriftAdj = 
          (int64_t) pData->curFrame.pkt.xtra.tm.pts - 
          (int64_t) pData->curFrame.tm.outtm.pts + pData->curFrame.tm.cfrOutDriftAdj;

        LOG(X_WARNING("CFR output drift %.3f -> %.3f, new drift adjustment: %.3f)"),
         PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.tm.outtm.pts),
         PTSF(pData->curFrame.tm.cfrOutDriftAdj));
  
        pData->curFrame.tm.outtm.pts = pData->curFrame.pkt.xtra.tm.pts;

      }

    }
   
    if(pData->curFrame.tm.outtm.pts > 0) {
      //LOG(X_DEBUG("SETPTS - set outpts90Khz:%.3f -> tm.outtm.pts:%.3f"), PTSF(pXcodeV->common.outpts90Khz), PTSF(pData->curFrame.tm.outtm.pts));
      pXcodeV->common.outpts90Khz = pData->curFrame.tm.outtm.pts;
    }

    //fprintf(stderr, "cfr out inpts:%.3f outpts:%.3f encOutIdx:%d\n", PTSF(pXcodeV->common.inpts90Khz),PTSF(pXcodeV->common.outpts90Khz), pData->curFrame.tm.frEncOutPeriod);

  } else {

    pData->curFrame.tm.outtm.pts = pData->curFrame.tm.intm.pts;

    if(pXcodeV->common.cfgCFROutput == -1 ||
       (pXcodeV->cfgOutClockHz == 0 || pXcodeV->cfgOutFrameDeltaHz == 0)) {
      // force pesxcode_frame_vid to always each input frame transcode
      //LOG(X_DEBUG("SETPTS - set outpts90Khz:%.3f -> zero"), PTSF(pXcodeV->common.outpts90Khz));
      pXcodeV->common.outpts90Khz = 0;

    } else if(xcoded) {

      //
      // cfgCFROutput == 0
      //

      if(pData->curFrame.tm.frEncOutPeriod <= 1) {

        //
        // Set the outpts to the inpts after xcode start or reset
        //
        //LOG(X_DEBUG("SETPTS - set outpts90Khz:%.3f -> inpts90Khz:%.3f"), PTSF(pXcodeV->common.outpts90Khz), PTSF(pXcodeV->common.inpts90Khz));
        pXcodeV->common.outpts90Khz = pXcodeV->common.inpts90Khz;

      } else {

        //
        // if not using constant fps output, and an output fps was specified,
        // do not exceed given fps
        //
        //ptsdelta = (((double) pXcodeV->cfgOutFrameDeltaHz / pXcodeV->cfgOutClockHz) * 90000);
        ptsdelta = (((double) pXcodeV->resOutFrameDeltaHz / pXcodeV->resOutClockHz) * 90000);

        //LOG(X_DEBUG("SETPTS xcode inpts90Khz:%.3f  increment outpts90Khz:%.3f -> %.3f, dlta:%.3f (%llu)"), PTSF(pXcodeV->common.inpts90Khz), PTSF(pXcodeV->common.outpts90Khz), PTSF(pXcodeV->common.outpts90Khz + ptsdelta), PTSF(ptsdelta), ptsdelta);

        pXcodeV->common.outpts90Khz += ptsdelta;


        //if(pXcodeV->common.outpts90Khz < pXcodeV->common.inpts90Khz) {
        if(pXcodeV->common.outpts90Khz + ptsdelta < pXcodeV->common.inpts90Khz) {

          //LOG(X_DEBUG("SETPTS - since outpts is lower than inpts, set it up to outputs90Khz:%.3f -> inpts90Khz:%.3f, clock: in:%d/%d, out:%d/%d"), PTSF(pXcodeV->common.outpts90Khz), PTSF(pXcodeV->common.inpts90Khz), pXcodeV->inClockHz, pXcodeV->inFrameDeltaHz, pXcodeV->resOutClockHz, pXcodeV->resOutFrameDeltaHz);
          pXcodeV->common.outpts90Khz = pXcodeV->common.inpts90Khz;
        }
      }

    }

  }

  pData->curFrame.tm.outtm.dts = pData->curFrame.pkt.xtra.tm.dts;

  //fprintf(stderr, "xcode tm done inpts90:%.3f  outpts90:%.3f\n", PTSF(pXcodeV->common.inpts90Khz), PTSF(pXcodeV->common.outpts90Khz));
}

int xcode_set_crop_pad(IXCODE_VIDEO_CROP_T *pCrop, 
                       const IXCODE_VIDEO_OUT_T *pXcodeOut,
                       int padAspectR,
                       unsigned int width, unsigned int height,
                       unsigned int origWidth, unsigned int origHeight) {
  int rc = 0;
  float aspectr = 0.0f;
  float aspectrOrig = 0.0f;

  pCrop->padLeft = pCrop->padLeftCfg;
  pCrop->padRight = pCrop->padRightCfg;
  pCrop->padTop = pCrop->padTopCfg;
  pCrop->padBottom = pCrop->padBottomCfg;

  if(pCrop->padLeft + pCrop->padRightCfg > width) {
    pCrop->padLeft = pCrop->padRightCfg = 0;
  }
  if(pCrop->padBottom + pCrop->padTop > height) {
    pCrop->padBottom = pCrop->padTop = 0;
  }

  if(padAspectR) {

    //
    // Preserve input aspect ratio and add padding on either vertical or
    // horizontal edges
    //
    aspectr = (float) width / height;
    
    if(origWidth > 0 && origHeight > 0) {
      aspectrOrig = (float) origWidth / origHeight;
    } else {
      aspectrOrig = aspectr;
    }
    if(IXCODE_FILTER_ROTATE_90(*pXcodeOut)) {
      aspectrOrig = 1 / aspectrOrig;
    }

    if(pCrop->padTop > 0 || pCrop->padBottom > 0) {

      aspectr = (float) width  / (height - (pCrop->padTop + pCrop->padBottom));

      if(aspectr > aspectrOrig) {

        if((pCrop->padLeft == 0 && pCrop->padRight == 0) || (pCrop->padLeft > 0 && pCrop->padRight > 0)) {
          pCrop->padLeft = pCrop->padRight = ((float)width - ((height - (pCrop->padTop + pCrop->padBottom)) 
                                             * aspectrOrig)) / 2.0f;
        } else if(pCrop->padRight > 0) {
          pCrop->padRight = ((float)width - ((height - (pCrop->padTop + pCrop->padBottom)) * aspectrOrig)) / 2.0f;
        } else if(pCrop->padLeft > 0) {
          pCrop->padLeft = ((float)width - ((height - (pCrop->padTop + pCrop->padBottom)) * aspectrOrig)) / 2.0f;

        }

      } else if(aspectr < aspectrOrig) {

        if(pCrop->padTop > 0 && pCrop->padBottom > 0) {
          pCrop->padTop = pCrop->padBottom = (height - (float)(width / aspectrOrig)) / 2.0f;
        } else if(pCrop->padTopCfg > 0) { 
          pCrop->padTop = (height - ((float)width / aspectrOrig)); 
        } else if(pCrop->padBottom > 0) { 
          pCrop->padBottom = (height - ((float)width / aspectrOrig)); 
        }

      }

      //fprintf(stderr, "AR:%.3f, ARORIG:%.3f w:%dxh:%d (orig w:%dxh%d) L:%d, R:%d, T:%d, B:%d\n", aspectr, aspectrOrig, width, height, origWidth, origHeight, pCrop->padLeft, pCrop->padRight, pCrop->padTop, pCrop->padBottom);

    } else if(pCrop->padLeft > 0 || pCrop->padRight > 0) {

      aspectr = (float) (width - (pCrop->padLeft + pCrop->padRight)) / height;

      if(aspectr > aspectrOrig) {

        if(pCrop->padLeft > 0 && pCrop->padRight > 0) {
          pCrop->padLeft = pCrop->padRight = (width - (float)(height * aspectrOrig)) / 2.0f;
        } else if(pCrop->padLeft > 0) {
          pCrop->padLeft = (width - (float)(height * aspectrOrig));
        } else if(pCrop->padRight > 0) {
          pCrop->padRight = (width - (float)(height * aspectrOrig));
        }

      } else if(aspectr < aspectrOrig) {
        pCrop->padTop = pCrop->padBottom = ((float)height - ((width - (pCrop->padLeft + pCrop->padRight)) 
                                              / aspectrOrig)) / 2.0f;
      }

      //fprintf(stderr, "_AR:%.3f, ARORIG:%.3f w:%dxh:%d (orig w:%dxh%d) L:%d, R:%d, T:%d, B:%d\n", aspectr, aspectrOrig, width, height, origWidth, origHeight, pCrop->padLeft, pCrop->padRight, pCrop->padTop, pCrop->padBottom);


    } else {

      if(aspectr > aspectrOrig) {
        pCrop->padLeft = pCrop->padRight = (width - (width * aspectrOrig / aspectr)) / 2.0f; 
      } else if(aspectr < aspectrOrig) {
        pCrop->padTop = pCrop->padBottom = (height - (height * aspectr / aspectrOrig)) / 2.0f; 
      }
      
      //fprintf(stderr, "__AR:%.3f, ARORIG:%.3f w:%dxh:%d (orig w:%dxh%d) L:%d, R:%d, T:%d, B:%d\n", aspectr, aspectrOrig, width, height, origWidth, origHeight, pCrop->padLeft, pCrop->padRight, pCrop->padTop, pCrop->padBottom);

    }
  }

  if(pCrop->padTop & 0x01) {
    pCrop->padTop++;
  }
  if(pCrop->padBottom & 0x01) {
    pCrop->padBottom++;
  }
  if(pCrop->padLeft & 0x01) {
    pCrop->padLeft++;
  }
  if(pCrop->padRight & 0x01) {
    pCrop->padRight++;
  }

  if(pCrop->padTop + pCrop->padBottom > height || pCrop->padLeft + pCrop->padRight > width ) {

    LOG(X_ERROR("Invalid output video dimensions (pad:%d,%d,%d)x(pad:%d,%d,%d)"),
        pCrop->padLeft, width, pCrop->padRight,
        pCrop->padTop, height, pCrop->padBottom);
    return -1;
  }

  //TODO: ideally crop dimensions would be interms of output resolution,
  // but true input resolution is not known at this time

  if(pCrop->cropTop & 0x01) {
    pCrop->cropTop++;
  }
  if(pCrop->cropBottom & 0x01) {
    pCrop->cropBottom++;
  }
  if(pCrop->cropLeft & 0x01) {
    pCrop->cropLeft++;
  }
  if(pCrop->cropRight & 0x01) {
    pCrop->cropRight++;
  }

  return rc;
}

static int xcode_vid_same(STREAM_XCODE_DATA_T *pData) {

  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  int lenOut;

  if(pXcode->common.cfgFileTypeIn != XC_CODEC_TYPE_H264) {
    LOG(X_ERROR("Passthru encoding for input codec %s not supported"), 
        codecType_getCodecDescrStr(pXcode->common.cfgFileTypeIn));
    return -1;
  } else if(pXcode->common.cfgFileTypeOut != XC_CODEC_TYPE_H264) {
    LOG(X_ERROR("Passthru encoding for output codec %s not supported"), 
        codecType_getCodecDescrStr(pXcode->common.cfgFileTypeOut));
    return -1;
  }

  if((lenOut = h264_convertAvcToh264b(pData->curFrame.pData, pData->curFrame.lenData, 
                                      pData->curFrame.xout.outbuf.buf, 
                                      pData->curFrame.xout.outbuf.lenbuf, 4, 1)) < 0) {
    LOG(X_ERROR("Unable to convert H.264 AVC length:%d to H.264b size:%d"), 
                pData->curFrame.lenData, pData->curFrame.xout.outbuf.lenbuf);
    return -1;
  }

  //avc_dumpHex(stderr, pData->curFrame.xout.outbuf.buf, MIN(16,pData->curFrame.xout.outbuf.lenbuf), 1);

  pData->curFrame.xout.outbuf.poffsets[0] = pData->curFrame.xout.outbuf.buf;  
  pData->curFrame.xout.outbuf.lens[0] = lenOut;
  //pXcode->common.encodeOutIdx++;

  return lenOut;
}

static void xcode_processfb(IXCODE_VIDEO_CTXT_T *pXcodeV) {

  TIME_VAL tm;
  unsigned int outidx;
  STREAM_XCODE_VID_UDATA_T *pUData = NULL;
  unsigned int minIntervalMs = FIR_INTERVAL_MS_LOCAL_ENCODER;

  pUData = (STREAM_XCODE_VID_UDATA_T *) pXcodeV->pUserData;
  tm = timer_GetTime();

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcodeV->out[outidx].active) {
      continue;
    }

    //fprintf(stderr, "xcode_processfb lag:%.2f %dfr\n", PTSF(pUData->pVidData->curFrame.tm.encDelay), pUData->pVidData->curFrame.tm.encDelayFrames);

    if(pUData->out[outidx].fbReq.tmLastFIRRcvd > 0 &&
       (pUData->out[outidx].fbReq.tmLastFIRProcessed == 0 ||
        pUData->out[outidx].fbReq.tmLastFIRProcessed + (minIntervalMs * TIME_VAL_MS) < tm)) {

      pUData->out[outidx].fbReq.tmLastFIRProcessed = tm;
      if(pUData->out[outidx].fbReq.tmLastFIRRcvd > tm) {
        pUData->out[outidx].fbReq.tmLastFIRRcvd = tm;
      } else {
        pUData->out[outidx].fbReq.tmLastFIRRcvd = 0;
      }
      pXcodeV->out[outidx].cfgForceIDR = 1;

      LOG(X_DEBUG("Processed request for IDR for encoder[%d] at %llu ms"), outidx, tm / TIME_VAL_MS);

    }
  }

}

static enum STREAM_NET_ADVFR_RC init_vidparams(STREAM_XCODE_DATA_T *pData) {
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;
  unsigned int idx;
  IXCODE_PIP_MOTION_T *pMotion = NULL;
  IXCODE_PIP_MOTION_VEC_T *pMv = NULL;
  char padstr[128];
  unsigned int outidx;

  //fprintf(stderr, "xcode_vid init_vidparams\n");

  padstr[0] = '\0';

  //
  // Perform delayed xcode init based on input stream properties
  //
  // For output format considerations, this should probably only be done once
  // and not for any subsequent resolution changes
  //
  if(!pUData->delayedCfgDone && (pUData->flags & STREAM_XCODE_FLAGS_LOAD_XCFG)) {

    //
    // When using delayed xcoder config initialization all output indexes may
    // have been marked as active to allow for packetizer / formatter creation
    // Ensure only the ones configured to be active for the given input resolution
    // are marked active
    // 
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      //fprintf(stderr, "BEFORE DELAYED XCFG outidx[%d] active:%d, passthru:%d\n", outidx, pXcode->out[outidx].active, pXcode->out[outidx].passthru);
      pXcode->out[outidx].active = 0;
    }

    if(xcode_parse_xcfg(pData, 1) < 0) {
      return STREAM_NET_ADVFR_RC_ERROR_CRIT;
    } else if(!pData->piXcode->vid.common.cfgDo_xcode) {
      return STREAM_NET_ADVFR_RC_ERROR_CRIT;
    }

    //
    // For delayed config, setfps is not called until now
    //
    if((rc = setfps(pXcode)) < 0) {
      return STREAM_NET_ADVFR_RC_ERROR_CRIT;
    }

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      if(pXcode->out[outidx].passthru) {
        //
        // duplicate xcodectxt_allocbuf functionality
        //
        pData->curFrame.useTmpFrameOut = 1;
      }
      //fprintf(stderr, "AFTER DELAYED XCFG outidx[%d] active:%d, passthru:%d\n", outidx, pXcode->out[outidx].active, pXcode->out[outidx].passthru);
    }

    pUData->delayedCfgDone = 1;
  }


  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active) {
      continue;
    }
  
    if((pXcode->out[outidx].cfgLookaheadmin1 & ~XCODE_VID_LOOKAHEAD_AUTO_FLAG) > 0) {
      pXcode->out[outidx].resLookaheadmin1 = (pXcode->out[outidx].cfgLookaheadmin1 & ~XCODE_VID_LOOKAHEAD_AUTO_FLAG);
    }

    if(setresolution(&pXcode->out[outidx], pUData) < 0) {
      return -1;
    }

  } // end of for

  for(idx = 0; idx < MAX_PIPS; idx++) {
    if(pXcode->overlay.p_pipsx[idx] && 
       (pMotion = pXcode->overlay.p_pipsx[idx]->pip.pMotion)) {
      pMv = pMotion->pm;    
      while(pMv) {
        if(xcode_set_crop_pad(&pMv->crop, &pXcode->out[0], pMv->crop.padAspectR, 
                              pXcode->out[0].resOutH, pXcode->out[0].resOutV, 0, 0) < 0) {
            return STREAM_NET_ADVFR_RC_ERROR_CRIT;
        }
        pMv = pMv->pnext;
      }
    }
  }

  //
  // Do not adhere to padAspectRatio if the input is a plain canvas solely used for PIP overlay, with no 
  // original input dimensions.
  //
  if(xcode_set_crop_pad(&pXcode->out[0].crop, &pXcode->out[0], 
                        pXcode->common.cfgFileTypeIn != XC_CODEC_TYPE_VID_CONFERENCE ? pXcode->out[0].crop.padAspectR : 0,
                        pXcode->out[0].resOutH, pXcode->out[0].resOutV,
                        pUData->seqHdrWidth, pUData->seqHdrHeight) < 0) {
    return STREAM_NET_ADVFR_RC_ERROR_CRIT;
  }

  //
  // In some cases, where the PIP layout depends on padAspectR to be set, the layout cannot be updated until 
  // the input resolution has been discovered and can be udpated from here.
  //
  if(pXcode->pip.active && pData->piXcode->aud.pUserData) {
    pXcode->pip.showVisual = 1;
    pip_layout_update(((STREAM_XCODE_AUD_UDATA_T *) pData->piXcode->aud.pUserData)->pPipLayout, 
                      PIP_UPDATE_PARTICIPANT_HAVEINPUT);
  }

  if(pXcode->out[0].crop.padTop > 0 || pXcode->out[0].crop.padBottom > 0 || 
     pXcode->out[0].crop.padLeft > 0 || pXcode->out[0].crop.padRight > 0) {
    snprintf(padstr, sizeof(padstr), "pad {left: %d, top: %d, right: %d, bottom: %d}",
             pXcode->out[0].crop.padLeft, pXcode->out[0].crop.padTop, 
             pXcode->out[0].crop.padRight, pXcode->out[0].crop.padBottom);
  }

  if(pXcode->out[0].cfgOutV == 0 || pXcode->out[0].cfgOutH == 0 || padstr[0] != '\0') {
    LOG(X_DEBUG("Set output video resolution to %dx%d %s"), pXcode->out[0].resOutH, pXcode->out[0].resOutV, padstr);
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
      continue;
    }

    if(pData->pVidProps[outidx]) {
      pData->pVidProps[outidx]->resH = pXcode->out[outidx].resOutH;
      pData->pVidProps[outidx]->resV = pXcode->out[outidx].resOutV;
      pData->pVidProps[outidx]->fps = (float) pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz;
      //LOG(X_DEBUG("set pVidProp[outidx:%d] %d x %d, %.3ffps"), outidx, pData->pVidProps[outidx]->resH, pData->pVidProps[outidx]->resV, pData->pVidProps[outidx]->fps);
    }
  }

  if((pXcode->out[0].passthru && pXcode->out[1].active) ||
     (!pXcode->out[0].passthru && pXcode->out[0].active)) {

    if(xcode_init_vid(pXcode) != 0) {
      return STREAM_NET_ADVFR_RC_ERROR_CRIT;
    }

  }

  pXcode->common.is_init = 1;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active) {
      continue;
    }

    pUData->out[outidx].payloadByteCnt = 0;
    pUData->out[outidx].payloadFrameCnt = 0;
    pUData->out[outidx].slowEncodePeriods = 0;
    pUData->out[outidx].payloadByteCntTot = 0;
    pUData->out[outidx].tv0.tv_sec = 0;

  }

  return rc;
}

#define XCODE_UPDATE_SET_PARAM(old, new, u) if((old) != (new)) { (old) = (new); (u) = 1; } 

enum STREAM_NET_ADVFR_RC xcode_vid_update(STREAM_XCODE_DATA_T *pDataCfg, 
                                          const IXCODE_VIDEO_CTXT_T *pUpdateCfg) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  IXCODE_VIDEO_CTXT_T *pXcode;
  STREAM_XCODE_DATA_T *pVData;
  unsigned int outidx;
  int doUpdateIdx;
  int doUpdate = 0;
  int doForceIDR = 0;
  int doConfigChanged = 0;
  int doFpsUpdate = 0;
  uint64_t frameDeltaPtsOld, frameDeltaPtsNew, ptsAddition;

  if(!pDataCfg || !pUpdateCfg) {
    return -1;
  }

  pXcode = &pDataCfg->piXcode->vid;
  //
  // pDataCfg is actually an arbitrary variable used for config, not the actual video stream reference
  //
  pVData = ((STREAM_XCODE_VID_UDATA_T *) pXcode->pUserData)->pVidData;

  //
  // return if the encoder has not yet been init
  //
  if(!pXcode->common.is_init) {
    return rc;
  }

  //
  // Handle mid-stream fps adjustment
  //
  frameDeltaPtsOld = (((double) pXcode->resOutFrameDeltaHz / pXcode->resOutClockHz) * 90000);
  if(pUpdateCfg->cfgOutClockHz > 0 && pUpdateCfg->cfgOutFrameDeltaHz > 0 && pUpdateCfg->resOutClockHz > 0 && pUpdateCfg->resOutFrameDeltaHz > 0) {
    frameDeltaPtsNew = (((double) pUpdateCfg->resOutFrameDeltaHz / pUpdateCfg->resOutClockHz) * 90000);
  } else {
    frameDeltaPtsNew = frameDeltaPtsOld;
  }

  if(frameDeltaPtsOld != frameDeltaPtsNew) {

    doFpsUpdate = 1;

#if 0

    if(pXcode->common.cfgCFROutput == 1) {
      if(pXcode->cfgOutClockHz != pUpdateCfg->cfgOutClockHz ||
         pXcode->cfgOutFrameDeltaHz != pUpdateCfg->cfgOutFrameDeltaHz) {
        LOG(X_WARNING("Dynamic frame rate adjustment not supported if constant output fps is enabled"));
      }
    }

#endif // 0

    if(pXcode->common.cfgCFROutput == 1) {

      ptsAddition = MIN(frameDeltaPtsOld, frameDeltaPtsNew);

      //fprintf(stderr, "set frEncOutOutPeriod:%d, at pts:%.3f, ptsAddition:%.3f\n", pVData->curFrame.tm.frEncOutPeriod, PTSF(pVData->curFrame.tm.outtm.pts), PTSF(ptsAddition));
      pVData->curFrame.tm.frEncOutPeriod = 0;
      pVData->curFrame.tm.frEncPtsAtLastUpdate = pVData->curFrame.tm.outtm.pts - pVData->curFrame.tm.encDelay + 
                                                 ptsAddition;
    }

    pXcode->cfgOutClockHz = pUpdateCfg->cfgOutClockHz;
    pXcode->cfgOutFrameDeltaHz = pUpdateCfg->cfgOutFrameDeltaHz;

    if(setfps(pXcode) < 0) {
      return rc;
    }

  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active) {
      continue;
    }

    doUpdateIdx = 0;

    //
    // CRF, or CQP are the only rc types that are dynamically reconfigurable
    //
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgBitRateOut, pUpdateCfg->out[outidx].cfgBitRateOut, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgBitRateTolOut, pUpdateCfg->out[outidx].cfgBitRateTolOut, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgMaxSliceSz, pUpdateCfg->out[outidx].cfgMaxSliceSz, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgSceneCut, pUpdateCfg->out[outidx].cfgSceneCut, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgQ, pUpdateCfg->out[outidx].cfgQ, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgQMax, pUpdateCfg->out[outidx].cfgQMax, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgVBVBufSize, pUpdateCfg->out[outidx].cfgVBVBufSize, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgVBVMaxRate, pUpdateCfg->out[outidx].cfgVBVMaxRate, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgVBVMinRate, pUpdateCfg->out[outidx].cfgVBVMinRate, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgIQRatio, pUpdateCfg->out[outidx].cfgIQRatio, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgBQRatio, pUpdateCfg->out[outidx].cfgBQRatio, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgGopMaxMs, pUpdateCfg->out[outidx].cfgGopMaxMs, doUpdateIdx);
    XCODE_UPDATE_SET_PARAM(pXcode->out[outidx].cfgGopMinMs, pUpdateCfg->out[outidx].cfgGopMinMs, doUpdateIdx);


    //
    // If we send an update command only changing the fps, then the encoder will not be adjusted
    // and the output bitrate will likely change proportional to the fps change.
    //
    // If we changed the fps, and the min gop / max gop properties are included in the config, do an encoder reconfig
    // to maintain the same gop size w/ respect to time.
    //
    if(!doUpdate && (doFpsUpdate && (pUpdateCfg->out[outidx].cfgGopMinMs > 0 || pUpdateCfg->out[outidx].cfgGopMaxMs > 0))) {
      doUpdateIdx = 1;
    }

    if(doUpdateIdx) {
      doUpdate = 1;
      doConfigChanged = 1;

      xcode_dump_conf_video(pXcode, outidx, 0, S_DEBUG);
    }

    //
    // IDR request does not need any encoder reconfig
    //
    if(pUpdateCfg->out[outidx].cfgForceIDR) {
      pXcode->out[outidx].cfgForceIDR = pUpdateCfg->out[outidx].cfgForceIDR;
      doForceIDR = 1;
    }

  }

  if(doForceIDR || pXcode->common.cfgFlags != pUpdateCfg->common.cfgFlags ||
     pXcode->common.cfgNoDecode != pUpdateCfg->common.cfgNoDecode) {
    doConfigChanged = 1;
  }

  //
  // Pass any arbitrary flags (directives) to xcode layer
  //
  pXcode->common.cfgFlags = pUpdateCfg->common.cfgFlags;
  pXcode->common.cfgNoDecode = pUpdateCfg->common.cfgNoDecode;

  if(doUpdate) {
    //if((rc = (enum STREAM_NET_ADVFR_RC) xcode_init_vid(pXcode)) != 0) {
    //  LOG(X_ERROR("xcode_init_vid (reconfig) returned error:%d"), rc);
    //}

    ((STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData)->flags |= STREAM_XCODE_FLAGS_UPDATE;
  } else if(!doConfigChanged) {
    LOG(X_DEBUG("No video settings to update."));
  }

  return rc;
}

static int share_frame_with_pips(STREAM_XCODE_DATA_T *pData) {
  unsigned int pipidx;
  int rc = 0;
  unsigned int idx;
  PKTQ_EXTRADATA_T xtra;
  STREAM_XCODE_VID_UDATA_T *pUDataPip = NULL;
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

//TODO: make sure this is not run from any PIPS w/ encoder specific output

  if(pUData->enableOutputSharing && pXcode->overlay.havePip && pUData->pStreamerPip) {

    if(!pUData->streamerPipHaveKeyframe) {
      if(pData->curFrame.xout.keyframes[0]) {
        pUData->streamerPipHaveKeyframe = 1;
      } else {
        return rc;
      }
    }

    xtra.tm.pts = pData->curFrame.pkt.xtra.tm.pts;
    xtra.tm.dts = pData->curFrame.pkt.xtra.tm.dts;
    //xtra.flags = 0;
    xtra.pQUserData = NULL;


    pthread_mutex_lock(&pUData->pStreamerPip->mtx);
    for(pipidx = 0; pipidx < MAX_PIPS; pipidx++) {

      if(pXcode->overlay.p_pipsx[pipidx] && 
         pXcode->overlay.p_pipsx[pipidx]->pip.active &&
         pXcode->overlay.p_pipsx[pipidx]->common.cfgDo_xcode &&
         !pXcode->overlay.p_pipsx[pipidx]->pip.doEncode &&
         //pUData->pStreamerPip->pCfgPips[pipidx]->xcode.vid.common.is_init &&
         (pUDataPip =  ((STREAM_XCODE_VID_UDATA_T *)pUData->pStreamerPip->pCfgPips[pipidx]->xcode.vid.pUserData)) &&
         pUDataPip->pPipFrQ) {
        //static int g_share_fr_wr; fprintf(stderr, "SHARING QUEING FRAME[%d] FOR PIP[%d] len:%d, pts:%.3f, qrd:%d, qwr:%d ", g_share_fr_wr++, pipidx, pData->curFrame.lenData, PTSF(xtra.tm.pts), pUDataPip->pPipFrQ->idxRd, pUDataPip->pPipFrQ->idxWr); avc_dumpHex(stderr, pData->curFrame.pData, MIN(16, pData->curFrame.lenData), 0);
        rc = pktqueue_addpkt(pUDataPip->pPipFrQ, pData->curFrame.pData, pData->curFrame.lenData, &xtra, 0);
      
        //
        // If the PIP streamer is reading from capture, it may be stuck waiting on an audio / video frame
        // from the capture queue in stream_av
        //
        for(idx = 0; idx < 2; idx++ ) {
          if(pUData->pStreamerPip->pCfgPips[pipidx]->pSleepQs[idx]) {
            pktqueue_wakeup(pUData->pStreamerPip->pCfgPips[pipidx]->pSleepQs[idx]);
          }
        }

      }
    }

    pthread_mutex_unlock(&pUData->pStreamerPip->mtx);

  }

  return rc;
}

static int pip_get_main_vidframe(STREAM_XCODE_DATA_T *pData) {
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  const PKTQUEUE_PKT_T *pPkt;
  unsigned int outidx;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

  pData->curFrame.lenData = 0;

  if(pUData->pPipFrQ && pktqueue_havepkt(pUData->pPipFrQ)) {
    if((pPkt = pktqueue_readpktdirect(pUData->pPipFrQ))) {

      if(pUData->pStreamerPip && pUData->pStreamerPip->pCfgOverlay) {

        for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

          // 
          // Update any start sequence headers
          //
          if(pUData->pStreamerPip->pCfgOverlay->xcode.vid.out[outidx].hdrLen != pXcode->out[outidx].hdrLen) {
            memcpy(pXcode->out[outidx].hdr, 
                   pUData->pStreamerPip->pCfgOverlay->xcode.vid.out[outidx].hdr,
                   pUData->pStreamerPip->pCfgOverlay->xcode.vid.out[outidx].hdrLen);
            pXcode->out[outidx].hdrLen = pUData->pStreamerPip->pCfgOverlay->xcode.vid.out[outidx].hdrLen;

            //fprintf(stderr, "PIP COPY START HEADERS LEN:%d\n", pXcode->out[outidx].hdrLen);
          } 
        }
      }

      //static int g_pip_main_vidframe; fprintf(stderr, "PIP pip_get_main_vidframe[%d] READ LEN:%d\n", g_pip_main_vidframe++, pPkt->len);
      //fprintf(stderr, "0x%x 0x%x hdrLen:%d, %d\n", pUData->pStreamerPip, pUData->pStreamerPip ? pUData->pStreamerPip->pCfgOverlay : 0, pUData->pStreamerPip ? pUData->pStreamerPip->pCfgOverlay->xcode.vid.out[0].hdrLen : 0, pXcode->out[0].hdrLen);
   
      //TODO: need to properly recreate IXCODE_OUTBUF_T w/ poffsets, lens
      //TODO: need to properly handle passthru, & poffsets for multiple outputs

      memcpy(pData->curFrame.pData, pPkt->pData, pPkt->len);
      pData->curFrame.xout.outbuf.lens[0] = pPkt->len;
      pData->curFrame.xout.outbuf.poffsets[0] = pData->curFrame.pData;
      pData->curFrame.xout.outbuf.lens[1] = 0;
      pData->curFrame.xout.outbuf.poffsets[1] = 0;
      //pData->curFrame.xout.outbuf.lens[2] = NULL;
      //pData->curFrame.xout.outbuf.poffsets[2] = 0;
      pData->curFrame.lenData = pPkt->len;

      memcpy(&pData->curFrame.tm.intm, &pPkt->xtra.tm, sizeof(pData->curFrame.tm.intm));

      if(pData->curFrame.tm.starttm0 == 0) {
        pData->curFrame.tm.starttm0 = pData->curFrame.tm.intm.pts;
        //fprintf(stderr, "pip got overlay starttm0 set to %.3f\n", PTSF(pData->curFrame.tm.starttm0));
      }
      if(pData->curFrame.tm.intm.pts >= pData->curFrame.tm.starttm0) {
        pData->curFrame.tm.intm.pts -= pData->curFrame.tm.starttm0;
        if(pData->curFrame.tm.intm.dts != 0) {
          pData->curFrame.tm.intm.dts -= pData->curFrame.tm.starttm0;
        }
      }

      memcpy(&pData->curFrame.pkt.xtra.tm, &pData->curFrame.tm.intm, sizeof(pData->curFrame.pkt.xtra.tm));

      //static int g_share_fr_rd; fprintf(stderr, "pip GOT SHARED FRAME[%d], poffsets[0]:0x%x, lens[0]:%d, poffsets[1]:0x%x, lens[1]:%d, poffsets[2]:0x%x, lens[2]:%d, pts:%.3f, dts:%.3f, startm0:%.3f, qrd:%d, qwr:%d ", g_share_fr_rd++, pData->curFrame.xout.outbuf.poffsets[0], pData->curFrame.xout.outbuf.lens[0],pData->curFrame.xout.outbuf.poffsets[1], pData->curFrame.xout.outbuf.lens[1],pData->curFrame.xout.outbuf.poffsets[2], pData->curFrame.xout.outbuf.lens[2], PTSF(pData->curFrame.tm.intm.pts), PTSF(pData->curFrame.tm.intm.dts), PTSF(pData->curFrame.tm.starttm0), pUData->pPipFrQ->idxRd, pUData->pPipFrQ->idxWr); avc_dumpHex(stderr, pData->curFrame.xout.outbuf.poffsets[0], MIN(16, pData->curFrame.xout.outbuf.lens[0]), 0);

      pktqueue_readpktdirect_done(pUData->pPipFrQ);

      pData->curFrame.numUpsampled = 0;
      pXcode->common.encodeOutIdx++;

    }
  }

  return pData->curFrame.lenData;
}

static void print_frame_lag(const STREAM_XCODE_DATA_T *pData, unsigned int outidx, unsigned int uiRealtm, const AV_OFFSET_T *pAvs[]) {
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;

  if(pAvs[outidx]) {
    LOG(X_DEBUG("Encoder[%d] lag: %.3fs (%d fr, dec:%d, enc:%d) (%.3fs real-time), "
                "Setting audio delay to: %.3fs (%.3fs base)"),
            outidx, PTSF(pData->curFrame.tm.encDelay),
            (pXcode->common.decodeInIdx - pXcode->common.decodeOutIdx) +
            (pXcode->common.encodeInIdx - pXcode->common.encodeOutIdx),
            (pXcode->common.decodeInIdx - pXcode->common.decodeOutIdx),
            (pXcode->common.encodeInIdx - pXcode->common.encodeOutIdx), (double) uiRealtm / 1000.0,
            PTSF(pAvs[outidx]->offsetCur), PTSF(pAvs[outidx]->offset0));

    if(pData->curFrame.tm.encDelay > 0 &&
       (uiRealtm * 90) > 1.25 * pData->curFrame.tm.encDelay) {
      LOG(X_WARNING("Detected very high transcoding init lag %u ms actual, "
                    "%.1f ms input pts"),
                    uiRealtm, (double) pXcode->common.inpts90Khz/90.0f);
    }
  }
}

enum STREAM_NET_ADVFR_RC xcode_vid(STREAM_XCODE_DATA_T *pData) {
  int len = 0;
  int lenpassthru = 0;
  int64_t tm;
  unsigned int ui;
  int inputResChange = 0;
  struct timeval tv;
  int maskerror = 0;
  int rtPriorWarning;
  unsigned int outidx;
  unsigned xcodeidx = 0;
  int getSharedPipFrame = 0;
  uint64_t ui64;
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  AV_OFFSET_T *pAvs[IXCODE_VIDEO_OUT_MAX];

  VSX_DEBUG_XCODE( 
    LOG(X_DEBUG("XCODE - xcode_vid: decode[%d,%d] encode[%d,%d] xcoder: in:%.3f (%llu) out:%.3f (%llu) "
               "(fr pts:%.3f (%llu), dts:%.3f) len:%d out-seqhdr:%d, in-seqhdr:%d"), 
    pXcode->common.decodeInIdx, pXcode->common.decodeOutIdx, pXcode->common.encodeInIdx, 
    pXcode->common.encodeOutIdx, PTSF(pXcode->common.inpts90Khz), pXcode->common.inpts90Khz,
    PTSF(pXcode->common.outpts90Khz), pXcode->common.outpts90Khz, PTSF(pData->curFrame.pkt.xtra.tm.pts), 
    pData->curFrame.pkt.xtra.tm.pts, PTSF(pData->curFrame.pkt.xtra.tm.dts),
    pData->curFrame.lenData, pData->curFrame.tm.haveseqstart, pUData->haveSeqStart) 

  //fprintf(stderr, "tid:0x%x, numUpsampled:%d, pData:0x%x, lenData:%d\n", pthread_self(), pData->curFrame.numUpsampled, pData->curFrame.pData, pData->curFrame.lenData);

    //gettimeofday(&tv, NULL); fprintf(stderr, "xcode_vid: [%d,%d](%d) pData: 0x%x len:%d seqst:%d tm:%d,%d\n", pXcode->common.encodeInIdx, pXcode->common.encodeOutIdx, pUData->out[0].payloadFrameCnt, pData->curFrame.pData, pData->curFrame.lenData, pUData->haveSeqStart, tv.tv_sec, tv.tv_usec); //avc_dumpHex(stderr, pData->curFrame.pData, 32, 1);

//  if(pXcode->pip.active && pXcode->common.decodeInIdx % 100 == 99) { pXcode->out[0].resOutH += 10; pXcode->out[0].resOutV += 10; }
//  if(pXcode->pip.active && pXcode->common.decodeInIdx % 50 == 49) { pXcode->pip.posx += 10; pXcode->pip.posy += 10; }
  //if(pXcode->pip.active && pXcode->common.decodeInIdx % 50 == 49) { pXcode->pip.visible=!pXcode->pip.visible; }
  );

  // lenData will be 0 when upsampling prior frame
  if(pData->curFrame.lenData > 0) {

    //static g_input_vid_seqhdr; if(pData->piXcode->vid.pip.active && g_input_vid_seqhdr++ < 35) return STREAM_NET_ADVFR_RC_NOTAVAIL; 

    if((rc = xcode_vid_seqhdr(pData, pXcode, &inputResChange)) != STREAM_NET_ADVFR_RC_OK) {
      return rc;
    }

    if(pData->curFrame.lenData == 0) {
      LOG(X_WARNING("Invalid video input frame length 0 after preprocessing."));
      pData->curFrame.lenData = 0;
      return STREAM_NET_ADVFR_RC_NOTAVAIL;
    }

  } else if(pData->curFrame.lenData == 0 &&
            pData->curFrame.numUpsampled > 0 &&
            pData->piXcode->vid.pip.active &&
            !pData->piXcode->vid.pip.doEncode &&
            pUData->pPipFrQ) {

    //
    // This is a PIP thread and we were called w/ no input data, but possibly a (shared) output frame available
    //
    //fprintf(stderr, "xcode_vid getSharedPipFrame set to 1, pXcode->common.is_init:%d\n", pXcode->common.is_init);
    getSharedPipFrame = 1;
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
     //if(pData->pavs[outidx]) {
     //  pAvs[outidx] = pData->pavs[outidx];
     //} else {
       pAvs[outidx] = pData->pComplement->pavs[outidx];
     //}
  }

  if(!getSharedPipFrame) {
    xtime_frame_seq(pData); 
    memcpy(&pData->curFrame.pkt.xtra.tm, &pData->curFrame.tm.intm,
         sizeof(pData->curFrame.pkt.xtra.tm));
  }

//if(pUData->pStreamerFbReq->pCaptureFbArray) {
//fbarray_dump(pUData->pStreamerFbReq->pCaptureFbArray);
//}

  if(!pUData->haveSeqStart && !getSharedPipFrame) {

    //fprintf(stderr, "xcode_vid NOT AVAIL len:%d, haveSeqStart not set\n", pData->curFrame.lenData);
    //avc_dumpHex(stderr, pData->curFrame.pData, 32, 1);

    pUData->noSeqStartCnt++;

    if(pUData->noSeqStartCnt % 15 == 5) {
      //
      // Inform the input capture to request an IDR from the media source
      //
      capture_requestFB(pUData->pStreamerFbReq, ENCODER_FBREQ_TYPE_FIR, REQUEST_FB_FROM_DECODER); 
    }

    if(pUData->noSeqStartCnt++ % 30 == 29) {
      LOG(X_WARNING("Still waiting for input video sequence start after %d frames"), pUData->noSeqStartCnt);
    }

    pData->curFrame.lenData = 0;
    return STREAM_NET_ADVFR_RC_NOTAVAIL;

  } else if(pUData->noSeqStartCnt > 0) {
    pUData->noSeqStartCnt  = 0;
  }

  //
  // Force full re-init upon resolution change
  // 
  if(!getSharedPipFrame && pXcode->common.is_init) {

    if((pUData->flags & STREAM_XCODE_FLAGS_RESET)) {
      inputResChange = 1;
      pUData->flags &= ~STREAM_XCODE_FLAGS_RESET;
    } else if((pUData->flags & STREAM_XCODE_FLAGS_UPDATE)) {

      //rc = init_vidparams(pData)) <= STREAM_NET_ADVFR_RC_ERROR) 
      if((pXcode->out[0].passthru && pXcode->out[1].active) ||
         (!pXcode->out[0].passthru && pXcode->out[0].active)) {

        LOG(X_DEBUG("Requesting video encoder configuration update"));
        if((rc = xcode_init_vid(pXcode) != 0)) {
          LOG(X_ERROR("xcode_init_vid (reconfig) returned error:%d"), rc);
          return STREAM_NET_ADVFR_RC_ERROR_CRIT;
        }
      }

      pUData->flags &= ~STREAM_XCODE_FLAGS_UPDATE;
    }
  } 

//if(pXcode->common.encodeOutIdx %200==199) { LOG(X_WARNING("SIMULATING INPUT RES CHANGE.. RESET..\n")); inputResChange=1; pXcode->common.encodeOutIdx++; }

  if(inputResChange) {
    LOG(X_DEBUG("Resetting transcoders"));

    pXcode->common.is_init = 0;
    //TODO: no need to close encoder, only reset decoder & scaler
    xcode_close_vid(pXcode);
    pData->curFrame.tm.haveEncDelay = 0;
    pData->curFrame.tm.cfrOutDriftAdj = 0;

    if(pData->piXcode && pData->piXcode->aud.common.cfgDo_xcode) {

      xcode_close_aud(&pData->piXcode->aud);
      pData->piXcode->aud.common.is_init = 0;

      //TODO: UDATA objects need their own close obj to clear this stuff
      ((STREAM_XCODE_AUD_UDATA_T *)&pData->piXcode->aud.pUserData)->bufIdx = 0;
      memset(&((STREAM_XCODE_AUD_UDATA_T *)pData->piXcode->aud.pUserData)->u.ac3,
             0, sizeof(XCODE_AC3_IN_T));
      memset(&((STREAM_XCODE_AUD_UDATA_T *)pData->piXcode->aud.pUserData)->u.aac,
             0, sizeof(XCODE_AAC_IN_T));
      pData->curFrame.numCompoundFrames = pData->curFrame.idxCompoundFrame = 0;
    }

    switch(pData->piXcode->vid.common.cfgFileTypeIn) {
      case XC_CODEC_TYPE_MPEG4V:
        memset(&pUData->uin.xmpg4v.mpg4v.param, 0, sizeof(pUData->uin.xmpg4v.mpg4v.param));
        break;
      default:
        break;
    }

    pUData->haveSeqStart = 0;
    //TODO: becareful here - no dyn allocated data is memset
    memset(&pUData->out, 0, sizeof(pUData->out));
    pData->curFrame.tm.haveseqstart = 0;
    if(pData->pComplement) {
      pData->pComplement->curFrame.tm.haveseqstart = 0;
      if(pData->pComplement->piXcode) { 
        ((STREAM_XCODE_AUD_UDATA_T *)pData->pComplement->piXcode->aud.pUserData)->haveSeqStart = 0;
      }
    }

    return STREAM_NET_ADVFR_RC_NEWPROG;
  }

  //
  // Initialize the video transcoder context
  //
  if(!getSharedPipFrame &&
     !pXcode->common.is_init &&
     (rc = init_vidparams(pData)) <= STREAM_NET_ADVFR_RC_ERROR) {
    return rc;
  }

  if(!getSharedPipFrame) {
    if(pXcode->common.encodeInIdx == 0) {
      gettimeofday(&pData->tvXcodeInit0, NULL);
      pData->curFrame.tm.encStartPts0 = pData->curFrame.pkt.xtra.tm.pts;
    } else if(pXcode->common.encodeInIdx == 1) {
      pData->curFrame.tm.encStartPts1 = pData->curFrame.pkt.xtra.tm.pts;
    }
  }

  if(pData->curFrame.useTmpFrameOut) {
    pData->curFrame.xout.outbuf.buf = pUData->tmpFrame.pData;
    pData->curFrame.xout.outbuf.lenbuf = pUData->tmpFrame.allocSz;
  } else {
    pData->curFrame.xout.outbuf.buf = pData->curFrame.pData;
    pData->curFrame.xout.outbuf.lenbuf = pData->curFrame.pkt.allocSz;
  }

  if(pXcode->out[0].passthru && pXcode->out[1].active) {
    xcodeidx = 1;
  }

  //
  // pass-thru non-transcoding
  //
  if(!getSharedPipFrame && pXcode->out[0].passthru) {

    // TODO: handle a single same passthru and subsequent encodes
    len = lenpassthru = xcode_vid_same(pData);

    //fprintf(stderr, "PASSTHRU LEN:%d\n", len);
  }

  //
  // Process any Feedback requests which have been set for the encoder
  //
  if(!getSharedPipFrame) {
    xcode_processfb(pXcode);
  }

//fprintf(stderr, "DECODER len:%d tmpFrameOut:%d\n", pData->curFrame.lenData, pData->curFrame.useTmpFrameOut); avc_dumpHex(stderr, pData->curFrame.pData, MIN(16, pData->curFrame.lenData), 1);

  if(getSharedPipFrame) {
    len = 0;
  } else if(lenpassthru >= 0 && pXcode->out[xcodeidx].active) {

  VSX_DEBUG_XCODE( 
    LOG(X_DEBUGV("XCODE - calling xcode_frame_vid tid:0x%x pip:%d 0x%x -> 0x%x %s / %d xcode pts:%.3f %.3f len:%d"),
     pthread_self(), pXcode->pip.active, pData->curFrame.pData, pData->curFrame.xout.outbuf.buf, pData->curFrame.useTmpFrameOut ? "tmpFrame" : "capslot", pData->curFrame.xout.outbuf.lenbuf, PTSF(pXcode->common.inpts90Khz), PTSF(pXcode->common.outpts90Khz), pData->curFrame.lenData));

    if((len = xcode_frame_vid(pXcode, 
                          pData->curFrame.pData, 
                          pData->curFrame.lenData, 
                          &pData->curFrame.xout.outbuf)) > 0) {
      len = pData->curFrame.xout.outbuf.lens[xcodeidx];
      if(len > 0 && lenpassthru > 0) {
        len += lenpassthru;
      }
    }
 
  }

  VSX_DEBUG_XCODE(
    LOG(X_DEBUGV("XCODE - called xcode_frame_vid tid: 0x%x pip.active:%d, overlay.havePip:%d, len:%d, lenpassthru:%d, lens[0]:%d, lens[1]:%d"), pthread_self(), pData->piXcode->vid.pip.active, pXcode->overlay.havePip, len, lenpassthru, pData->curFrame.xout.outbuf.lens[0], pData->curFrame.xout.outbuf.lens[1]); LOGHEX_DEBUGV(pData->curFrame.xout.outbuf.buf, MIN(16, pData->curFrame.xout.outbuf.lens[0]));
//if((pData->curFrame.xout.outbuf.buf[2]==0x01&&pData->curFrame.xout.outbuf.buf[3]==0x06) || (pData->curFrame.xout.outbuf.buf[3]==0x01&&pData->curFrame.xout.outbuf.buf[4]==0x06)) avc_dumpHex(stderr,pData->curFrame.xout.outbuf.buf, MIN(2048, pData->curFrame.xout.outbuf.lens[0]), 1); else if(pData->curFrame.xout.outbuf.buf[3]==0x01&&pData->curFrame.xout.outbuf.buf[4]==0x67) avc_dumpHex(stderr,pData->curFrame.xout.outbuf.buf, MIN(2048, pData->curFrame.xout.outbuf.lens[0]), 1);
  );

  if(pData->curFrame.pData != pData->curFrame.xout.outbuf.buf) {
    pData->curFrame.pData = pData->curFrame.xout.outbuf.buf;
  }

  if(len < 0) {

    pData->consecErr++;

    //
    // Gracefully handle decoder errors for damaged input
    //
    if(len < IXCODE_RC_OK && pXcode->common.decodeInIdx > 2 &&
       pData->consecErr < 3) {
      maskerror = 1;
      LOG(X_WARNING("xcode_frame_vid frame[%d] masking error rc:%d (consec:%d)"), 
                    pXcode->common.decodeInIdx, len, pData->consecErr);
    } else {
      LOG(X_ERROR("xcode_frame_vid frame[%d] error rc:%d (consec:%d)"), 
                  pXcode->common.decodeInIdx, len, pData->consecErr);
      pData->curFrame.lenData = 0;
      return STREAM_NET_ADVFR_RC_ERROR;
    }

  } else if(pData->consecErr > 0) {
    pData->consecErr = 0;
  }

  //
  // If this is a PIP thread, we may not actually be producing unique encoded video, but if this is a
  // a video conference output thread, then try to get see if there is a shared frame available,
  // which was encoded by the main overlay thread
  //
  if(pData->piXcode->vid.pip.active && !pData->piXcode->vid.pip.doEncode) {

    len = pip_get_main_vidframe(pData);

    //
    // Return here if this frame is an input frame for a PIP 
    //
    if(pData->curFrame.lenData == 0) {
      return STREAM_NET_ADVFR_RC_NOTENCODED;
    }

    //pData->curFrame.lenData = 0;
    //return STREAM_NET_ADVFR_RC_NOTENCODED;
  } else {
    //fprintf(stderr, "poffsets[0]:0x%x, lens[0]:%d, poffsets[1]:0x%x, lens[1]:%d, poffsets[2]:0x%x, lens[2]:%d \n", pData->curFrame.xout.outbuf.poffsets[0], pData->curFrame.xout.outbuf.lens[0],pData->curFrame.xout.outbuf.poffsets[1], pData->curFrame.xout.outbuf.lens[1],pData->curFrame.xout.outbuf.poffsets[2], pData->curFrame.xout.outbuf.lens[2]);
    //avc_dumpHex(stderr, pData->curFrame.xout.outbuf.poffsets[0], MIN(16, pData->curFrame.xout.outbuf.lens[0]), 0);

  }

  //
  // Compute input time 
  // 
  xtime_frame_in_post(pData, pXcode); 

  if(pXcode->common.encodeOutIdx <= 1 || pData->tvLateFromRT0 > pData->tvLateFromRT) {
    pData->tvLateFromRT0 = pData->tvLateFromRT;
    //fprintf(stderr, "encodeOutIdx:%d, RT:%llu, RT0:%llu\n", pXcode->common.encodeOutIdx, pData->tvLateFromRT, pData->tvLateFromRT0);
  } 

  if(pData->piXcode->vid.pip.doEncode && pXcode->common.encodeOutIdx == 1) {
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      if(pXcode->out[outidx].active && pAvs[outidx]) {
        print_frame_lag(pData, outidx, 0, (const AV_OFFSET_T **) pAvs);
      }
    }
  } else if(!pData->piXcode->vid.pip.doEncode && pXcode->common.encodeOutIdx == 1 && !pData->curFrame.tm.haveEncDelay) {

    //
    // Compute encoder frame lag with respect to output pts clock
    //

    //fprintf(stderr, "calc enc delay inpts:%.3f seqstartoffset:%.3f encst:%.3f\n", PTSF(pXcode->common.inpts90Khz), PTSF(pData->curFrame.tm.seqstartoffset), PTSF(pData->curFrame.tm.encStartPts0));
    if(pXcode->common.inpts90Khz > pData->curFrame.tm.seqstartoffset) {

      if(pXcode->common.cfgCFROutput != 0 ||
         (pXcode->resOutFrameDeltaHz > 0 && pXcode->inFrameDeltaHz > 0 &&
         (double)pXcode->resOutClockHz/pXcode->resOutFrameDeltaHz <
         (double)pXcode->inClockHz/pXcode->inFrameDeltaHz)) {

        pData->curFrame.tm.encDelay = (((double) pXcode->common.encodeInIdx *
                    pXcode->resOutFrameDeltaHz / pXcode->resOutClockHz) * 90000);
        //fprintf(stderr, "METHOD 0\n"); pData->curFrame.tm.encDelay += 00000;
      } else if(pXcode->common.cfgCFRInput) {
        pData->curFrame.tm.encDelay = (((double) pXcode->common.encodeInIdx *
                pXcode->inFrameDeltaHz / pXcode->inClockHz) * 90000);
      } else {

        //
        // It's common for many movies to have a large offset of the first two video frames to compensate for
        // audio sync, so try to get the sync point from the 2nd input frame to compute the input audio lag.
        //
        if(pData->curFrame.tm.encStartPts1 > pData->curFrame.tm.encStartPts0) {
          if(pXcode->resOutClockHz > 0) {
            ui64 = ((double) pXcode->resOutFrameDeltaHz / pXcode->resOutClockHz) * 90000;
          } else {
            ui64 = 0;
          }
          pData->curFrame.tm.encDelay = pXcode->common.inpts90Khz - pData->curFrame.tm.encStartPts1 + ui64;

        } else {
          pData->curFrame.tm.encDelay = pXcode->common.inpts90Khz - pData->curFrame.tm.encStartPts0;
        }
        //fprintf(stderr, "ENC START PTS0:%.3f, PTS1:%.3f, inpts90Khz:%.3f, cfrout:%d, resoutfr:%d/%d, infr:%d/%d encDelay:%.3f\n", PTSF(pData->curFrame.tm.encStartPts0), PTSF(pData->curFrame.tm.encStartPts1), PTSF(pXcode->common.inpts90Khz), pXcode->common.cfgCFROutput, pXcode->resOutClockHz, pXcode->resOutFrameDeltaHz, pXcode->inClockHz, pXcode->inFrameDeltaHz, PTSF(pData->curFrame.tm.encDelay));
      }
      pData->curFrame.tm.encDelayFrames = (pXcode->common.encodeInIdx - pXcode->common.encodeOutIdx) +
                                          (pXcode->common.decodeInIdx - pXcode->common.decodeOutIdx);
      LOG(X_DEBUG("Set a/v encoder delay to %.3fsec (pts:%.3f - %.3f) %d fr (lag dec:%d, enc:%d)"), 
          PTSF(pData->curFrame.tm.encDelay),
          PTSF(pXcode->common.inpts90Khz),  PTSF(pData->curFrame.tm.encStartPts0), 
            pData->curFrame.tm.encDelayFrames,
            (pXcode->common.decodeInIdx - pXcode->common.decodeOutIdx),
            (pXcode->common.encodeInIdx - pXcode->common.encodeOutIdx));
          
    } else {
      pData->curFrame.tm.encDelay = 0;
    }
    pData->curFrame.tm.haveEncDelay = 1;

    gettimeofday(&tv, NULL);
    ui = TIME_TV_DIFF_MS(tv, pData->tvXcodeInit0); 

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(!pXcode->out[outidx].active) {
        continue;
      }

      if(pAvs[outidx]) {

        if(!pData->pComplement->pignoreencdelay_av ||
           *pData->pComplement->pignoreencdelay_av == 0) {
          pAvs[outidx]->offsetCur = pAvs[outidx]->offset0 + pData->curFrame.tm.encDelay;
          //LOG(X_DEBUG("SET [%d].offsetCur:%lld=%lld + %lld"), outidx, pAvs[outidx]->offsetCur, pAvs[outidx]->offset0,  pData->curFrame.tm.encDelay);
//TODO: need to account for audio xcode delay, but for some reason its only needed when picking
// up a capture mid-stream.. from the first audio & video packet, the sync is always perfect.../?? why
//pAvs[outidx]->offsetCur -= 40000;
        }

        print_frame_lag(pData, outidx, ui, (const  AV_OFFSET_T **) pAvs);

      } // end of if(pAv...

    } // end of for(outidx...

  }

  //
  // Compute output time 
  // 
  xtime_frame_out_post(pData, pXcode); 
  memcpy(&pData->curFrame.pkt.xtra.tm, &pData->curFrame.tm.outtm,
         sizeof(pData->curFrame.pkt.xtra.tm));

  //
  // Return here if no encoded frame was output
  //
  if(len == 0 || maskerror) {
    pData->curFrame.lenData = 0;
    return STREAM_NET_ADVFR_RC_NOTAVAIL;      
  }

  pData->curFrame.lenData = len;

  // Default to original input video pts
  // Avoid using dts=0 for video stream even w/ matching pts/dts
  INIT_PTSDTS(pData->curFrame.pkt.xtra.tm);

  //
  // When upsampling the frame rate, ensure even distribution of pts/dts
  // among all frames, otherwise playback may not work in quicktime / iphone
  //
  if(pData->curFrame.numUpsampled > 0) {

    //fprintf(stderr, "upsampling frame xcoder inpts:%.3f outpts:%3f idx:%d encidx:%d\n", (double)pXcode->common.inpts90Khz/90000.0f,(double)pXcode->common.outpts90Khz/90000.0f, pData->curFrame.upsampleFrIdx,  pData->curFrame.tm.frEncOutPeriod);

    if(pXcode->common.cfgCFROutput) {

      //pData->curFrame.tm.outtm.pts += (uint64_t) 
      //     (((double) pXcode->resOutFrameDeltaHz / pXcode->resOutClockHz) * 90000);

      pData->curFrame.pkt.xtra.tm.pts = pData->curFrame.tm.outtm.pts;
      pData->curFrame.pkt.xtra.tm.dts = pData->curFrame.pkt.xtra.tm.pts;

    } else {

      LOG(X_WARNING("upampling for non cfr output is no longer supported"));   
/*
      //
      // Evenly adjust time for each frame other than the one proceeding an upsampled one
      // , but only for deviations in out fps greater than 29/30 fps
      ui = (unsigned int) (90000 * pXcode->resOutFrameDeltaHz / pXcode->resOutClockHz);

      if(pData->curFrame.pkt.xtra.tm.pts > 0) {
        if(pData->curFrame.pkt.xtra.tm.pts >= (2 * pData->curFrame.lastOutTm.pts) ||
           pData->curFrame.pkt.xtra.tm.pts < pData->curFrame.lastOutTm.pts) {
          LOG(X_WARNING("Resetting video upsample pts cur: %.3f, last: %.3f"), 
               (double)pData->curFrame.pkt.xtra.tm.pts/90000.0f,
               (double)pData->curFrame.lastOutTm.pts/90000.0f);
         ui = 0;
         //SET_PTSDTS(pData->curFrame.lastOutTm, pData->curFrame.pkt.xtra.tm);

        }
      }

      pData->curFrame.frameData.xtra.tm.pts = pData->curFrame.lastOutTm.pts + ui;
      if(pData->curFrame.lastOutTm.dts != 0) {
        pData->curFrame.frameData.xtra.tm.dts = pData->curFrame.lastOutTm.dts + ui;
      } else {
        pData->curFrame.frameData.xtra.tm.dts = pData->curFrame.frameData.xtra.tm.pts;
      }
*/
    }

    //fprintf(stderr, "xcodee(upsampled) pts:%.3f dts:%.3f\n", (double)pData->curFrame.frameData.xtra.tm.pts/90000.0f, (double)pData->curFrame.frameData.xtra.tm.dts/90000.0f);
      
    //SET_PTSDTS(pData->curFrame.lastOutTm, pData->curFrame.frameData.xtra.tm);

    pData->curFrame.numUpsampled--;

  }

/*
  //
  // Share the output frame with any pip (conference endpoint) video output threads
  //
  if(pXcode->overlay.havePip) {
    share_frame_with_pips(pData);
  }
  //fprintf(stderr, "This is the frame I'm sharing len:%d (poffsets len0:%d)", pData->curFrame.lenData, pData->curFrame.xout.outbuf.lens[0]); avc_dumpHex(stderr, pData->curFrame.pData, MIN(16, pData->curFrame.lenData), 0);
*/


  //
  // Check for key frame and any codec specific output framing
  //
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active) {
      continue;
    }

    pData->curFrame.xout.tms[outidx].pts = pData->curFrame.pkt.xtra.tm.pts;
    pData->curFrame.xout.tms[outidx].dts = pData->curFrame.pkt.xtra.tm.dts;
    //fprintf(stderr, "xcode pts:%.3f\n", PTSF(pData->curFrame.pkt.xtra.tm.pts));

    if(pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_H264) {

      // The DTS will actually be set inside here
      if(!pXcode->out[outidx].passthru) {
        xcode_vid_h264_check(pData, outidx);
      }
      //fprintf(stderr, "PTS:%.3f, DTS:%.3f\n", PTSF(pData->curFrame.xout.tms[outidx].pts), PTSF(pData->curFrame.xout.tms[outidx].dts)); 

    } else if(pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_MPEG4V) {

      if(!pXcode->out[outidx].passthru) {
        xcode_vid_mpg4v_check(pData, outidx);
      }

    } else if(pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_H263 ||
              pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_H263_PLUS) {
      //fprintf(stderr, "len:%d\n", pData->curFrame.frameData.len);
      //avc_dumpHex(stderr, pData->curFrame.frameData.pData, pData->curFrame.frameData.len > 48 ? 48 : pData->curFrame.frameData.len, 1);
      if(!pXcode->out[outidx].passthru) {
        //xcode_vid_h263_check(pData);
        pData->curFrame.xout.keyframes[outidx] = pXcode->out[outidx].frameType == FRAME_TYPE_I ? 1 : 0;
      }

    } else if(pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_VP8) {

      if(!pXcode->out[outidx].passthru) {
        pData->curFrame.xout.keyframes[outidx] = pXcode->out[outidx].frameType == FRAME_TYPE_I ? 1 : 0;
      }

    } else if(IS_XC_CODEC_TYPE_VID_RAW(pXcode->common.cfgFileTypeOut)) {
      pData->curFrame.xout.keyframes[outidx] = 1;
    }

    //fprintf(stderr, "outidx[%d] xcode pts:%lld dts:%lld (pts:%.3f, dts:%.3f) hdrLen:%d, keyf:%d\n", outidx, pXcode->out[0].pts, pXcode->out[0].dts, PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.pkt.xtra.tm.dts), pXcode->out[outidx].hdrLen, pData->curFrame.xout.keyframes[0]); //avc_dumpHex(stderr, pXcode->out[outidx].hdr, pXcode->out[outidx].hdrLen, 1);
    
    //pUData->payloadByteCnt += pData->curFrame.lenData;
    pUData->out[outidx].payloadByteCnt += pData->curFrame.xout.outbuf.lens[outidx];
    pUData->out[outidx].payloadFrameCnt++;
    if(pUData->out[outidx].tv0.tv_sec == 0) {
      gettimeofday(&pUData->out[outidx].tv0, NULL);
      pUData->out[outidx].tv1.tv_sec = pUData->out[outidx].tv0.tv_sec;
      pUData->out[outidx].tv1.tv_usec = pUData->out[outidx].tv0.tv_usec;
    }

  }

  //
  // Share the output frame with any pip (conference endpoint) video output threads
  //
  if(pXcode->overlay.havePip && pUData->pStreamerPip) {
    share_frame_with_pips(pData);
  }
  //LOG(X_DEBUG("This is the frame I'm sharing keyf:%d len:%d (poffsets len0:%d)"), pData->curFrame.xout.keyframes[0], pData->curFrame.lenData, pData->curFrame.xout.outbuf.lens[0]); LOGHEX_DEBUG(pData->curFrame.pData, MIN(16, pData->curFrame.lenData));


  gettimeofday(&tv, NULL);
  tm = TIME_TV_DIFF_MS(tv, pUData->out[xcodeidx].tv1);

  //LOG(X_DEBUG("[%d].qSamples:%d, %u/%u, tm:%llu"), xcodeidx, pXcode->out[xcodeidx].qSamples ,pXcode->resOutClockHz , pXcode->resOutFrameDeltaHz, tm);
  //
  // Check if encoder speed is keeping up 
  //
  if(pXcode->out[xcodeidx].qSamples >= ((float)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz) ||
    (pUData->out[xcodeidx].tv1.tv_sec > 0 && tm > 1500)) {

    for(outidx = xcodeidx; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(!pXcode->out[outidx].active) {
        continue;
      }

      rtPriorWarning = 0;
      tm = TIME_TV_DIFF_MS(tv, pUData->out[outidx].tv1);
      pUData->out[outidx].payloadByteCntTot += pUData->out[outidx].payloadByteCnt;

      //fprintf(stderr, "fr:%d, %dms, %f\n",  pXcode->common.encodeOutIdx , (((tv.tv_sec - pUData->out[outidx].tv0.tv_sec)*1000) + ((tv.tv_usec - pUData->out[outidx].tv0.tv_usec)/1000)),(float) pXcode->common.encodeOutIdx/ (((tv.tv_sec - pUData->out[outidx].tv0.tv_sec)*1000) + ((tv.tv_usec - pUData->out[outidx].tv0.tv_usec)/1000)));


      //
      // Print a warning message if the transcoding task has taken an abnormal amount of time
      //
      if(((TVLATEFROMRT(pData) - pUData->out[outidx].tvLateFromRTPrior) / TIME_VAL_MS) > 400) {
        LOG(X_WARNING("Video encoder[%d] output is falling %llu ms behind (%llu ms from start).  %.1ffps"),  
              outidx, 
              ((TVLATEFROMRT(pData) - pUData->out[outidx].tvLateFromRTPrior) / TIME_VAL_MS),
              (TVLATEFROMRT(pData) / TIME_VAL_MS),
              ((float)pUData->out[outidx].payloadFrameCnt / tm*1000.0f));

        rtPriorWarning = 1;
        pUData->out[outidx].tvLateFromRTPrior = TVLATEFROMRT(pData);
      } else if((TVLATEFROMRT(pData) < pUData->out[outidx].tvLateFromRTPrior)) {
        pUData->out[outidx].tvLateFromRTPrior = TVLATEFROMRT(pData);
      }
/*
      if(tm > 1200) {
        pUData->out[outidx].slowEncodePeriods++;
        if(tm > 2000) {
          pUData->out[outidx].slowEncodePeriods += 2;
        }
      } else if(tm < 980) {
        pUData->out[outidx].slowEncodePeriods = 0;
      } else if(((float)pUData->out[outidx].payloadFrameCnt / tm*1000.0f) <
           ((float)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz) * .91) {
        pUData->out[outidx].slowEncodePeriods++;
      } else if(pUData->out[outidx].slowEncodePeriods > 0) {
        pUData->out[outidx].slowEncodePeriods = 0;
      }

      if(pUData->out[outidx].slowEncodePeriods > 2) {

        LOG(X_WARNING("Video encoder[%d] is running too slow %.1ffps / %.1ffps"),  
              outidx, ((float)pUData->out[outidx].payloadFrameCnt / tm*1000.0f), 
                     ((float)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz));

      //} else if(tm < 700 || tm > 1100) {
      } else if(tm > 1100) {

        if(pXcode->common.encodeOutIdx > 1 + ((float)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz)) {
          LOG(X_WARNING("Video encoder[%d] may not be running in real-time. 1 sec task took %.2f sec to process %u frames"), 
          outidx, (float)tm/1000.0f, pUData->out[outidx].payloadFrameCnt);
        }

      }
*/

      if(pXcode->out[outidx].qSamples > 0) {
        pUData->out[outidx].lastQuantizerAvg = pXcode->out[outidx].qTot / pXcode->out[outidx].qSamples;
      } else {
        pUData->out[outidx].lastQuantizerAvg = 0;
      }

      //fprintf(stderr, "in clock:%d/%d\n", pXcode->inClockHz, pXcode->inFrameDeltaHz);
      //fprintf(stderr, "res clock:%d/%d\n", pXcode->resOutClockHz, pXcode->resOutFrameDeltaHz);
      //fprintf(stderr, "cfg clock:%d/%d\n", pXcode->cfgOutClockHz, pXcode->cfgOutFrameDeltaHz);
      //fprintf(stderr, "TV0:%d\n", pUData->out[outidx].tv0.tv_sec);
      //fprintf(stderr, "%.3f fps, %dfr in %lldms, qSamples:%d, slowEncodePeriods:%d, tvLateFromRT:%llums\n",     ((float)pUData->out[outidx].payloadFrameCnt / tm*1000.0f), pUData->out[outidx].payloadFrameCnt ,tm, pXcode->out[xcodeidx].qSamples, pUData->out[outidx].slowEncodePeriods, TVLATEFROMRT(pData)/TIME_VAL_MS);
      //fprintf(stderr, "RT0:%llu, RT:%llu\n", pData->tvLateFromRT0,  pData->tvLateFromRT);
      //static int x_i; if(x_i++ % 3 ==1)usleep(1300000);

      if(g_verbosity > VSX_VERBOSITY_NORMAL && pXcode->common.encodeOutIdx > 1) {
        LOG(X_DEBUG("Encoder[%d]: %.1ffps %.1fB/fr %.1fKb/s (%.1fKb/s) q: %.1f"
                    ", dec[%d,%d]%.3f, enc[%d,%d]%.3f"
                    ""), outidx,
         ((float)pUData->out[outidx].payloadFrameCnt / tm*1000.0f),
         pUData->out[outidx].payloadFrameCnt > 0 ? 
         (float)pUData->out[outidx].payloadByteCnt / pUData->out[outidx].payloadFrameCnt : 0,   
         (float)pUData->out[outidx].payloadByteCnt*8/1024.0f/(tm/1000.0f),   
         (double)pUData->out[outidx].payloadByteCntTot*8/1024.0f / 
          ((((tv.tv_sec - pUData->out[outidx].tv0.tv_sec)*1000) + 
        ((tv.tv_usec - pUData->out[outidx].tv0.tv_usec)/1000))/1000.0f),
          pUData->out[outidx].lastQuantizerAvg,
          pXcode->common.decodeInIdx, pXcode->common.decodeOutIdx, 
          PTSF(pXcode->common.inpts90Khz),
          pXcode->common.encodeInIdx, pXcode->common.encodeOutIdx, 
          PTSF(pXcode->common.outpts90Khz)); 

#if defined(XCODE_STD_DEV) 
        std_dev_add(&g_std_dev, (float)pUData->out[outidx].payloadByteCnt*8/1024.0f/(tm/1000.0f));
        if(g_std_dev.cnt % 6 == 0) std_dev_print(&g_std_dev);
#endif // (XCODE_STD_DEV) 
      }
      //static double s_QTot, s_QNum; s_QTot+=pXcode->out[outidx].qTot; s_QNum+=pXcode->out[outidx].qSamples; fprintf(stderr, "Q:%.2f (%.0f)\n", s_QTot/s_QNum, s_QNum);

      pXcode->out[outidx].qTot = 0;
      pXcode->out[outidx].qSamples = 0;

      pUData->out[outidx].payloadByteCnt = 0;
      pUData->out[outidx].payloadFrameCnt = 0;
      pUData->out[outidx].tv1.tv_sec = tv.tv_sec;
      pUData->out[outidx].tv1.tv_usec = tv.tv_usec;
      //pUData->out[outidx].tvLateFromRTPrior = TVLATEFROMRT(pData);

    } // end of for(outidx...

  } // end of if(pXcode->out[ ].qSamples...


  VSX_DEBUG_XCODE(
    LOG(X_DEBUGV("XCODE - xout.outbuf.buf: 0x%x, lenbuf:%d, { [0].p:0x%x, len:%d, 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x}"), pData->curFrame.xout.outbuf.buf, pData->curFrame.xout.outbuf.lenbuf, pData->curFrame.xout.outbuf.poffsets[0], pData->curFrame.xout.outbuf.lens[0], pData->curFrame.xout.outbuf.poffsets[0][0], pData->curFrame.xout.outbuf.poffsets[0][1], pData->curFrame.xout.outbuf.poffsets[0][2], pData->curFrame.xout.outbuf.poffsets[0][3], pData->curFrame.xout.outbuf.poffsets[0][12], pData->curFrame.xout.outbuf.poffsets[0][13], pData->curFrame.xout.outbuf.poffsets[0][14], pData->curFrame.xout.outbuf.poffsets[0][15]);
    if(pData->curFrame.xout.outbuf.poffsets[1]) LOG(X_DEBUG("xcode - [1]..p:0x%x, len:%d, 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x"), pData->curFrame.xout.outbuf.poffsets[1], pData->curFrame.xout.outbuf.lens[1], pData->curFrame.xout.outbuf.poffsets[1][0],pData->curFrame.xout.outbuf.poffsets[1][1],pData->curFrame.xout.outbuf.poffsets[1][2],pData->curFrame.xout.outbuf.poffsets[1][3], pData->curFrame.xout.outbuf.poffsets[1][12],pData->curFrame.xout.outbuf.poffsets[1][13], pData->curFrame.xout.outbuf.poffsets[1][14], pData->curFrame.xout.outbuf.poffsets[1][15]);
    if(pData->curFrame.xout.outbuf.poffsets[2]) LOG(X_DEBUG("xcode - [2]..p:0x%x, len:%d, 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x"), pData->curFrame.xout.outbuf.poffsets[2], pData->curFrame.xout.outbuf.lens[2], pData->curFrame.xout.outbuf.poffsets[2][0],pData->curFrame.xout.outbuf.poffsets[2][1],pData->curFrame.xout.outbuf.poffsets[2][2],pData->curFrame.xout.outbuf.poffsets[2][3], pData->curFrame.xout.outbuf.poffsets[2][12],pData->curFrame.xout.outbuf.poffsets[2][13], pData->curFrame.xout.outbuf.poffsets[2][14], pData->curFrame.xout.outbuf.poffsets[2][15]);

#if 0
static int s_gopsz; static int s_gopbytes;static uint64_t delmelastptsout; 
LOG(X_DEBUG("xcode_vid done tid:0x%x out[%d] pip.active:%d returning len:%d %s q:%.3f, pkt.xtra.tmpts:%.3f pkt.xtra.tmdts:%.3f (delta:%.3f) inpts90:%.3f, outpts90:%.3f, encDelay:%lld)"), pthread_self(), pXcode->common.encodeOutIdx, pXcode->pip.active, len, 

((pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_H264 && ((pData->curFrame.pData[3]==0x01 && ((pData->curFrame.pData[4] & NAL_TYPE_MASK)==NAL_TYPE_IDR))||(pData->curFrame.pData[2]==0x01&&((pData->curFrame.pData[3]&NAL_TYPE_MASK)==NAL_TYPE_SEI)))) || (pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_VP8 && VP8_ISKEYFRAME(&pData->curFrame.pData[0])) || (pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_MPEG4V && pData->curFrame.pData[3] == 0xb0)) ? "KEYFRAME" : "",
pXcode->out[0].frameQ, PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.pkt.xtra.tm.dts), PTSF(pData->curFrame.pkt.xtra.tm.pts-delmelastptsout), PTSF(pXcode->common.inpts90Khz), PTSF(pXcode->common.outpts90Khz), pData->curFrame.tm.encDelay);
//fprintf(stderr, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", pData->curFrame.pData[0], pData->curFrame.pData[1], pData->curFrame.pData[2], pData->curFrame.pData[3], pData->curFrame.pData[4], pData->curFrame.pData[5], pData->curFrame.pData[6], pData->curFrame.pData[7]);
//s_gopsz++;s_gopbytes+=len;
  delmelastptsout =pData->curFrame.pkt.xtra.tm.pts;
//if((pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_H264 && ((pData->curFrame.pData[3]==0x01 && ((pData->curFrame.pData[4] & NAL_TYPE_MASK)==NAL_TYPE_IDR))||(pData->curFrame.pData[2]==0x01&&((pData->curFrame.pData[3]&NAL_TYPE_MASK)==NAL_TYPE_SEI)))) || (pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_VP8 && VP8_ISKEYFRAME(&pData->curFrame.pData[0])) || (pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_MPEG4V && pData->curFrame.pData[3] == 0xb0)) { fprintf(stderr, "KEYFRAME q:%.3f size:%d, non-key %.1f (%d fr)\n", pXcode->out[0].frameQ, len, (float)s_gopbytes/s_gopsz, s_gopsz); s_gopbytes = 0; s_gopsz = 0;} else { s_gopbytes += len; s_gopsz++;  }
#endif // 0

  //fprintf(stderr, "Q:%.3f, key:%d, size:%d\n", pXcode->out[0].frameQ, pData->curFrame.pData[3] == 0xb0 ? 1 : 0, len);
#if 0
  H263_DESCR_T h263;
  BIT_STREAM_T bs;
  memset(&h263, 0, sizeof(h263));
  memset(&bs, 0, sizeof(bs));
  bs.buf = pData->curFrame.pData;
  bs.sz = pData->curFrame.lenData;
  //fprintf(stderr, "LEN:%d %d\n", len, pData->curFrame.lenData);
  h263_decodeHeader(&bs, &h263);
#endif // 0

/*
fprintf(stderr, "len:%d [3]:0x%x\n", len, pData->curFrame.pData[3]);
static int g_i;
static int g_ff; if(g_ff==0) g_ff=open("testblack.h263", O_RDWR | O_CREAT, 0666);
if(++g_i <= 15) {
fprintf(stderr, "write fr[%d]\n", g_i); write(g_ff, pData->curFrame.pData, len);
} else if(g_i == 16) {
fprintf(stderr, "closing\n"); close(g_ff);
}
*/

); // end of VSX_DEBUG_XCODE...

  //SET_PTSDTS(pData->curFrame.lastOutTm, pData->curFrame.frameData.xtra.tm);
  pData->numFramesOut++;

  return rc;
}

int xcode_getvidheader(STREAM_XCODE_DATA_T *pData, unsigned int outidx, 
                       unsigned char *pHdr, unsigned int *plen) {
  int rc = 0;
  IXCODE_VIDEO_CTXT_T *pXcode;
  //STREAM_XCODE_VID_UDATA_T vUData;
  //SPSPPS_RAW_T spspps;

  if(!pData || !pHdr || !plen) {
    return -1; 
  }

  pXcode = &pData->piXcode->vid;
  //memset(&vUData, 0, sizeof(vUData));
  //memset(&spspps, 0, sizeof(spspps));

  if(init_vidparams(pData) != STREAM_NET_ADVFR_RC_OK) {
    return -1;
  }

  if(pXcode->common.cfgFileTypeOut == XC_CODEC_TYPE_H264)  {
    if(pXcode->out[outidx].hdrLen <= *plen) {
      memcpy(pHdr, &pXcode->out[0].hdr, pXcode->out[outidx].hdrLen);
      *plen = pXcode->out[outidx].hdrLen;
    } else {
      rc = -1;
    }
    //avc_dumpHex(stderr, pXcode->hdr, pXcode->hdrLen, 1);
  } else {
    rc = -1;
  }

  xcode_close_vid(pXcode);
  pXcode->out[0].hdrLen = 0;
  pXcode->common.is_init = 0;

  return rc;
}

