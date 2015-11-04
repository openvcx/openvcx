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


static int flvsrv_init_vid_h264(CODEC_VID_CTXT_T *pCtxtV, const SPSPPS_RAW_T *pSpspps) {
  CODEC_VID_CTXT_H264_T *pCtxtVH264 = &pCtxtV->codecCtxt.h264;
  H264_DECODER_CTXT_T h264Ctxt;
  BIT_STREAM_T h264bs;
  int rc;

  H264_INIT_BITPARSER(h264bs, pSpspps->sps, pSpspps->sps_len);
  memset(pCtxtVH264, 0, sizeof(CODEC_VID_CTXT_H264_T));
  memset(&h264Ctxt, 0, sizeof(h264Ctxt));

  if(h264_decode_NALHdr(&h264Ctxt, &h264bs, 0) <= H264_RESULT_ERR) {
    LOG(X_ERROR("Failed to parse rtmp video start h.264 SPS length: %d"),
        pSpspps->sps_len);
    return -1;
  }

  if((rc = avcc_create(pCtxtVH264->avccRaw, sizeof(pCtxtVH264->avccRaw), pSpspps, 4)) < 0) {
    LOG(X_ERROR("Failed to create video avcc start headers (sps:%d pps:%d)"),
        pSpspps->sps_len, pSpspps->pps_len);
    return -1;
  }

  pCtxtVH264->avccRawLen = rc;

  pCtxtV->ctxt.pstartHdrs = pCtxtVH264->avccRaw;
  pCtxtV->ctxt.startHdrsLen = pCtxtVH264->avccRawLen;

  h264_getCroppedDimensions(&h264Ctxt.sps[h264Ctxt.sps_idx],
                            &pCtxtV->ctxt.width, &pCtxtV->ctxt.height, NULL, NULL);
  h264_getVUITiming(&h264Ctxt.sps[h264Ctxt.sps_idx], &pCtxtV->ctxt.clockHz,
                    &pCtxtV->ctxt.frameDeltaHz);
  pCtxtVH264->profile = h264Ctxt.sps[h264Ctxt.sps_idx].profile_id;
  pCtxtVH264->level = h264Ctxt.sps[h264Ctxt.sps_idx].level_id;

  //
  // The avcc config is needed for dash mpd creation
  //
  if((rc = avcc_initCfg(pCtxtVH264->avccRaw, pCtxtVH264->avccRawLen, &pCtxtVH264->avcc)) < 0 ||
     !pCtxtVH264->avcc.psps || !pCtxtVH264->avcc.ppps) {
    
  }

  return rc;
}

static int flvsrv_init_aud_vorbis(CODEC_AUD_CTXT_T *pCtxtA) {
  int rc = 0;
  //CODEC_AUD_CTXT_VORBIST *pCtxtVorbis = &pCtxtA->codecCtxt.vorbis;
  VORBIS_DESCR_T vorbis;


  memset(&vorbis, 0, sizeof(vorbis));
  if((rc = vorbis_parse_hdr(pCtxtA->pStreamerCfg->xcode.aud.phdr, 
                   pCtxtA->pStreamerCfg->xcode.aud.hdrLen, 
                   &vorbis)) < 0) {
    LOG(X_ERROR("Failed to parse vorbis codec config sequence length %d"),
                pCtxtA->pStreamerCfg->xcode.aud.hdrLen);
    return rc;
  }

  pCtxtA->ctxt.pstartHdrs = pCtxtA->pStreamerCfg->xcode.aud.phdr;
  pCtxtA->ctxt.startHdrsLen = pCtxtA->pStreamerCfg->xcode.aud.hdrLen;
  pCtxtA->ctxt.clockHz = vorbis.clockHz;
  pCtxtA->ctxt.channels = vorbis.channels;

  return rc;
}

static int flvsrv_init_aud_aac(CODEC_AUD_CTXT_T *pCtxtA, const uint8_t *pAdtsHdr) {
  CODEC_AUD_CTXT_AAC_T *pCtxtAac = &pCtxtA->codecCtxt.aac;
  int rc;

  memset(pCtxtAac, 0, sizeof(CODEC_AUD_CTXT_AAC_T));


  if(pCtxtA->pStreamerCfg && !pCtxtA->pStreamerCfg->xcode.aud.common.cfgDo_xcode &&
     XCODE_AUD_UDATA_PTR(&pCtxtA->pStreamerCfg->xcode)->u.aac.descr.sp.init) {

    LOG(X_DEBUG("Loading aac sequence start from source media"));

    memcpy(&pCtxtAac->adtsFrame.sp, &XCODE_AUD_UDATA_PTR(&pCtxtA->pStreamerCfg->xcode)->u.aac.descr.sp,
           sizeof(pCtxtAac->adtsFrame.sp));

    aac_encodeADTSHdr(pCtxtAac->adtsHdr, sizeof(pCtxtAac->adtsHdr), &pCtxtAac->adtsFrame.sp, 0);

  } else {

    LOG(X_DEBUG("Loading aac sequence start from stream ADTS header"));

    if(aac_decodeAdtsHdr(pAdtsHdr, 7, &pCtxtAac->adtsFrame) < 0) {
      return -1;
    }
    memcpy(pCtxtAac->adtsHdr, pAdtsHdr, 7);

  }

  if((rc = esds_encodeDecoderSp((uint8_t *) pCtxtAac->spbuf, sizeof(pCtxtAac->spbuf),
    &pCtxtAac->adtsFrame.sp)) <= 0) {
    return -1;
  }

  pCtxtA->ctxt.clockHz = esds_getFrequencyVal(ESDS_FREQ_IDX(pCtxtAac->adtsFrame.sp));
  pCtxtA->ctxt.channels = pCtxtAac->adtsFrame.sp.channelIdx;
  pCtxtA->ctxt.pstartHdrs = pCtxtAac->spbuf;
  pCtxtA->ctxt.startHdrsLen = rc;

  //fprintf(stderr, "init_aud_aac haveExtFreqIdx:%d, %dHz\n", pCtxtAac->adtsFrame.sp.haveExtFreqIdx, esds_getFrequencyVal(ESDS_FREQ_IDX(pCtxtAac->adtsFrame.sp))); avc_dumpHex(stderr, pCtxtA->ctxt.pstartHdrs, pCtxtA->ctxt.startHdrsLen, 1);

  return 0;
}

int flvsrv_create_vidstart(BYTE_STREAM_T *bs, CODEC_VID_CTXT_T *pFlvV) {

  if(!bs || !pFlvV || !bs->buf || bs->sz < 5 + pFlvV->codecCtxt.h264.avccRawLen) {
    return -1;
  }

  //
  // Create AVCC video tag
  //
  bs->buf[bs->idx++] = (FLV_VID_FRM_KEYFRAME << 4) | (FLV_VID_CODEC_AVC);
  bs->buf[bs->idx++] = FLV_VID_AVC_PKTTYPE_SEQHDR;
  flv_write_int24(bs, 0); // composition time

  //memcpy(&bs->buf[bs->idx], pFlvV->codecCtxt.h264.avccRaw, pFlvV->codecCtxt.h264.avccRawLen);
  //bs->idx += pFlvV->codecCtxt.h264.avccRawLen;
  memcpy(&bs->buf[bs->idx], pFlvV->ctxt.pstartHdrs, pFlvV->ctxt.startHdrsLen);
  bs->idx += pFlvV->ctxt.startHdrsLen;

  return 0;
}

int flvsrv_create_audstart(BYTE_STREAM_T *bs, CODEC_AUD_CTXT_T *pFlvA) {

  int rc;
  int flvAudRate = 0;

  if(!bs || !pFlvA || !bs->buf || bs->sz < 2 || !pFlvA->ctxt.pstartHdrs) {
    return -1;
  }

  //rc = (int) esds_getFrequencyVal(ESDS_FREQ_IDX(pFlvA->codecCtxt.aac.adtsFrame.sp));
  rc = pFlvA->ctxt.clockHz;

  if(rc <= 5500) {
    flvAudRate = FLV_AUD_RATE_5_5KHZ;
  } else if(rc <= 11000) {
    flvAudRate = FLV_AUD_RATE_11KHZ;
  } else if(rc <= 22050) {
    flvAudRate = FLV_AUD_RATE_22KHZ;
  } else if(rc <= 44100) {
    flvAudRate = FLV_AUD_RATE_44KHZ;
  } else {
    //TODO: what to do here?... it may be ok for RTMP based transfer
    flvAudRate = FLV_AUD_RATE_44KHZ;
  }

  pFlvA->codecCtxt.aac.audHdr = (FLV_AUD_CODEC_AAC << 4) |
                       (flvAudRate << 2) | (FLV_AUD_SZ_16BIT << 1) |
                      //(pFlvA->codecCtxt.aac.adtsFrame.sp.channelIdx < 2 ?
                      (pFlvA->ctxt.channels < 2 ?  FLV_AUD_TYPE_MONO : FLV_AUD_TYPE_STEREO);

  // 22Khz mono does not play correctly w/ prior settings
  pFlvA->codecCtxt.aac.audHdr = (FLV_AUD_CODEC_AAC << 4) | (FLV_AUD_RATE_44KHZ << 2) |
                                  (FLV_AUD_SZ_16BIT << 1) | FLV_AUD_TYPE_STEREO;


  bs->buf[bs->idx++] = pFlvA->codecCtxt.aac.audHdr;
  bs->buf[bs->idx++] = FLV_AUD_AAC_PKTTYPE_SEQHDR;

  memcpy(&bs->buf[bs->idx], pFlvA->ctxt.pstartHdrs, pFlvA->ctxt.startHdrsLen);
  bs->idx += pFlvA->ctxt.startHdrsLen;

  //if((rc = esds_encodeDecoderSp(&bs->buf[bs->idx],
  //             bs->sz - bs->idx, &pFlvA->codecCtxt.aac.adtsFrame.sp)) > 0) {
    //avc_dumpHex(stderr, &bs->buf[bs->idx], rc, 1); avc_dumpHex(stderr, pFlvA->codecCtxt.aac.adtsHdr, 7, 1);
  //  bs->idx += rc;
  //}

  return 0;
}

static unsigned char *vidframe_h264_removenals(unsigned char *pData, unsigned int *plenData) {
  H264_STREAM_CHUNK_T sc;
  unsigned int offset = 0;
  int pos = 0;
//LOG(X_DEBUG("VIDFRAME_H264_REMOVENALS: *plenData: %d 0x%x 0x%x..."), *plenData, pData[0], pData[1]); LOGHEX_DEBUG(pData, MIN(16, *plenData));
  while(*plenData - offset > 4) {

    memset(&sc, 0, sizeof(sc));
    sc.bs.buf = &pData[offset];
    sc.bs.sz = MIN(2048, *plenData - offset);

    if((pos = h264_findStartCode(&sc, 1, NULL)) > 0) {

        //LOG(X_DEBUG("findStartCode returned pos:%d, offset at %d"), pos, offset);

      if(((pData[offset + pos] & NAL_TYPE_MASK) == NAL_TYPE_SEQ_PARAM) ||
         ((pData[offset + pos] & NAL_TYPE_MASK) == NAL_TYPE_PIC_PARAM)) {
        offset += pos;
        //LOG(X_DEBUG("findStartCode SPS/PPS offset:%d, pos:%d"), offset, pos);
      } else {
        //LOG(X_DEBUG("break offset:%d, pos:%d"), offset, pos);
        break;
      }
    } else {
      LOG(X_WARNING("h264_removenals - no start code found in length: %d, offset: %d, pos; %d"), 
          *plenData, offset, pos);
      break;
    }
    //LOG(X_DEBUG("cont offset:%d, pos:%d"), offset, pos);
  }

  if(pos >= 0 && offset + pos >= 4) {
  //if(offset + pos >= 4) {
    pData += (offset + pos - 4);
    *plenData -= (offset + pos - 4);
    if(pData[0] != 0x00) {
      pData[0] = 0x00;
    }
  }

  return pData;
}

int flvsrv_create_vidframe_h264(BYTE_STREAM_T *bs, CODEC_VID_CTXT_T *pFlvV, 
                                const OUTFMT_FRAME_DATA_T *pFrame, 
                                uint32_t *pmspts, int32_t *pmsdts, int keyframe) {

  int rc = 0;
  unsigned int frameIdx = 0;
  uint32_t mspts = 0;
  int32_t msdts = 0;
  uint8_t frameType;
  unsigned char *pData = NULL;
  unsigned int lenData = 0;

  if(!pFlvV || !pFrame) {
    return -1;
  }

  if(!pmspts) {
    pmspts = &mspts;
  }
  if(!pmsdts) {
    pmsdts = &msdts;
  }

  //
  // Omit NAL AUD
  //
  if((rc = h264_check_NAL_AUD(OUTFMT_DATA(pFrame), OUTFMT_LEN(pFrame))) > 0) {
    frameIdx = rc;
  }

  if(OUTFMT_LEN(pFrame) < frameIdx) {
    LOG(X_ERROR("Invalid video frame length: %d"), OUTFMT_LEN(pFrame));
    return -1;
  }

  if(OUTFMT_PTS(pFrame) > pFlvV->tmprev) {
    *pmspts = (uint32_t) (OUTFMT_PTS(pFrame) - pFlvV->tmprev) / 90.0f;
  }
  pFlvV->tmprev += (*pmspts * 90);
  //hdrsz = 8; // hdr size < 12 has relative timestamps

  //if(*pmspts > 50 || *pmspts <= 0) fprintf(stderr, "funky mspts:%d\n", *pmspts);
  //if(pFrame->pts > pFlvV->tm0) {
  //  *pmspts = (pFrame->pts - pFlvV->tm0) / 90.0f;
  //}


  if(OUTFMT_DTS(pFrame) != 0) {
    *pmsdts = (int32_t) (OUTFMT_DTS(pFrame) / 90.0f);
  }
  //*pmsdts = *pmspts;
  //if(pFrame->pts > pRtmp->av.vid.tmprev) {
  //  *pmsdts = (uint32_t) (pFrame->pts - pFlvV->tmprev) / 90.0f;
  //}
  //pFlvV->tmprev = pFrame->pts;

  //TODO: check if the dts is actually a +/- offset from the pts, or an absolute time
  //pFlvV->tmprev = pFrame->pts + pFrame->dts - pFlvV->tm0;
  //pFlvV->tmprev = pFrame->pts - pFlvV->tm0;
  //tm = (uint32_t) (pFlvV->tmprev / 90.0f);
  //tm = (pFlvV->tmprev / 90.0f) - *pmsdts; hdrsz = 8; // hdr size < 12 has relative timestamps

  if(OUTFMT_LEN(pFrame) - frameIdx > pFlvV->tmpFrame.sz) {
    LOG(X_ERROR("h264 frame size too large %d for buffer size %d"),
                OUTFMT_LEN(pFrame) - frameIdx, pFlvV->tmpFrame.sz);
    return -1;
  }

  memcpy(pFlvV->tmpFrame.buf, &OUTFMT_DATA(pFrame)[frameIdx], OUTFMT_LEN(pFrame) - frameIdx);
  pData = pFlvV->tmpFrame.buf;
  lenData = OUTFMT_LEN(pFrame) - frameIdx;

  //LOG(X_DEBUG("Before convert to h264b len:%d"), lenData); LOGHEX_DEBUG(pData, MIN(48, lenData));

  //
  // Remove any SPS/PPS, which could potentially contain 3 byte start codes
  //
  pData = vidframe_h264_removenals(pData, &lenData);
  //LOG(X_DEBUG("lenData:%d, orig:%d"), lenData, OUTFMT_LEN(pFrame));
  if(lenData <= 0) {
    LOG(X_WARNING("h264 flv/rtmp/mkv frame length is %d, original: %d"), lenData, OUTFMT_LEN(pFrame));
  }

  h264_converth264bToAvc(pData, lenData);

  //LOG(X_DEBUG("After convert to h264b len:%d"), lenData); LOGHEX_DEBUG(pData, MIN(48, lenData));

/*
  //fprintf(stderr, "len:%d 0x%x 0x%x\n", lenData, pData[3], pData[4]);
  while(lenData > 4 && ((pData[4] & NAL_TYPE_MASK) == NAL_TYPE_SEQ_PARAM ||
                        (pData[4] & NAL_TYPE_MASK) == NAL_TYPE_PIC_PARAM)) {
    rc = (int) htonl( *(uint32_t *)pData );
    pData += (4 + rc);
    lenData -= (4 + rc);
    //fprintf(stderr, "decrement vid size to:%d by 4 + %d\n", lenData, rc);
  }
  //fprintf(stderr, "now len:%d 0x%x 0x%x\n", lenData, pData[3], pData[4]);
  LOG(X_DEBUG("After convert to h264b and remove of seq params len:%d"), lenData); LOGHEX_DEBUG(pData, MIN(16, lenData));

*/

  //if((pData[4] & NAL_NRI_MASK) == NAL_NRI_HI) {
  //  pData[4] &= ~NAL_NRI_MASK;
  //  pData[4] |= NAL_NRI_MED;
  //}

  if(bs) {
    //bs.idx = hdrsz;
    //bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
    //bs.sz = pRtmp->out.sz - pRtmp->out.idx;

    frameType = keyframe ? FLV_VID_FRM_KEYFRAME : FLV_VID_FRM_INTERFRAME;
    bs->buf[bs->idx++] = (frameType << 4) | (FLV_VID_CODEC_AVC);
    bs->buf[bs->idx++] = FLV_VID_AVC_PKTTYPE_NALU;
    flv_write_int24(bs, *pmsdts); // composition time
  }

  pFlvV->tmpFrame.idx = (pData - pFlvV->tmpFrame.buf);

  return lenData;
}

int flvsrv_create_audframe_aac(BYTE_STREAM_T *bs, CODEC_AUD_CTXT_T *pFlvA, const OUTFMT_FRAME_DATA_T *pFrame, 
                               uint32_t *pmspts) {
  uint32_t mspts = 0;
  unsigned int frameIdx = 0;

  if(!pmspts) {
    pmspts = &mspts;
  }

  // Skip ADTS header
  if(OUTFMT_DATA(pFrame)[0] == 0xff && (OUTFMT_DATA(pFrame)[1] & 0xf0) == 0xf0) {
    frameIdx = 7;
  }

//fprintf(stderr, "aud prev:%llu pts:%llu tm0:%llu \n", pFrame->pts, pRtmp->aud.tmprev, pRtmp->aud.tm0);

  if(OUTFMT_PTS(pFrame) > pFlvA->tmprev) {
    *pmspts = (OUTFMT_PTS(pFrame) - pFlvA->tmprev) / 90.0f;
  }
  pFlvA->tmprev += (*pmspts * 90);
  //hdrsz = 8; // hdr size < 12 has relative timestamps

  //if(pFrame->pts > pRtmp->av.aud.tm0) {
  //  *pmspts = (pFrame->pts - pRtmp->av.aud.tm0) / 90.0f;
  //}
  //pRtmp->av.aud.tmprev = pts;

  //TODO: check if the dts is actually a +/- offset from the pts, or an absolute time
  //pRtmp->av.aud.tmprev = pFrame->pts - pRtmp->av.aud.tm0;
  //tm = (uint32_t) (pRtmp->av.aud.tmprev / 90.0f);
  //tm = (pRtmp->av.aud.tmprev / 90.0f) - *pmspts; hdrsz = 8; // hdr size < 12 has relative timestamps


  if(bs) {
    bs->buf[bs->idx++] = pFlvA->codecCtxt.aac.audHdr;
    bs->buf[bs->idx++] = FLV_AUD_AAC_PKTTYPE_RAW;
  }

  //fprintf(stderr, "audframe[%d] len:%d sent:%d ms:%u, pts:%.3f (%.3f)\n", aframes++, pFrame->len, rc, *pmspts, PTSF(pFrame->pts), PTSF(pFrame->pts - pRtmp->av.aud.tm0));

  return OUTFMT_LEN(pFrame) - frameIdx;
}



int flvsrv_send_vidframe(FLVSRV_CTXT_T *pFlvCtxt, const OUTFMT_FRAME_DATA_T *pFrame, int keyframe) {
  BYTE_STREAM_T bstmp;
  uint32_t timestamp32 = 0;
  unsigned char *pData = NULL;
  unsigned int lenData = 0;
  int rc;

  //
  // Compute the frame timestamp in ms, relative from the start
  //
  if(OUTFMT_PTS(pFrame)  > pFlvCtxt->av.vid.tm0) {
    timestamp32  = (OUTFMT_PTS(pFrame) - pFlvCtxt->av.vid.tm0) / 90.0f;
  }
  //pFlvCtxt->av.vid.tmprev = timestamp32; // this may be wrong... tmprev is a 90KHz pts

  pFlvCtxt->out.prevtagidx = pFlvCtxt->out.bs.idx;
  memcpy(&bstmp, &pFlvCtxt->out.bs, sizeof(bstmp));
  pFlvCtxt->out.bs.idx += FLV_TAG_HDR_SZ_NOPREV;

  if((rc = flvsrv_create_vidframe_h264(&pFlvCtxt->out.bs, &pFlvCtxt->av.vid, pFrame, 
                                        NULL, NULL, keyframe)) < 0) {
    return rc;
  }


  pData = &pFlvCtxt->av.vid.tmpFrame.buf[pFlvCtxt->av.vid.tmpFrame.idx];
  lenData = rc;
  //LOGHEX_DEBUG(pData, MIN(16, lenData));
  //LOG(X_DEBUG("0x%x len:%d"), pData[4] & NAL_TYPE_MASK, lenData);
  if(pFlvCtxt->av.vid.tmpFrame.idx + lenData + 4 > pFlvCtxt->av.vid.tmpFrame.sz) {
    LOG(X_ERROR("flvsrv video frame size %d is too large for %d"), lenData, pFlvCtxt->av.vid.tmpFrame.sz);
    return -1;
  }
  pFlvCtxt->av.vid.tsLastWrittenFrame = timestamp32;

  //
  // Now that the data length is known, fill in the 11 byte flv header
  //
  flv_write_taghdr(&bstmp, FLV_TAG_VIDEODATA, 
                   lenData + pFlvCtxt->out.bs.idx - pFlvCtxt->out.prevtagidx - FLV_TAG_HDR_SZ_NOPREV, 
                   timestamp32); 

  pFlvCtxt->writeErrDescr = "video header";
  if((rc = pFlvCtxt->cbWrite(pFlvCtxt, pFlvCtxt->out.bs.buf, pFlvCtxt->out.bs.idx)) < 0) {
    return rc;
  }

  //
  // 4 byte previous tag size field
  //
  *((uint32_t *) &pData[lenData]) = htonl(lenData + pFlvCtxt->out.bs.idx - pFlvCtxt->out.prevtagidx);
  lenData += 4;

  pFlvCtxt->writeErrDescr = "video payload";
  if((rc = pFlvCtxt->cbWrite(pFlvCtxt, pData, lenData)) < 0) {
    return rc;
  }

  return rc;
}

int flvsrv_send_audframe(FLVSRV_CTXT_T *pFlvCtxt, const OUTFMT_FRAME_DATA_T *pFrame) {
  BYTE_STREAM_T bstmp;
  uint32_t timestamp32 = 0;
  unsigned int lenData = 0;
  int rc;

  //
  // Compute the frame timestamp in ms, relative from the start
  //
  if(OUTFMT_PTS(pFrame) > pFlvCtxt->av.aud.tm0) {
    timestamp32  = (OUTFMT_PTS(pFrame) - pFlvCtxt->av.aud.tm0) / 90.0f;
  }
  //fprintf(stderr, "flvsrv_send_audframe %d (prev:%lld)\n", timestamp32, pFlvCtxt->av.aud.tsLastWrittenFrame);

  //pFlvCtxt->av.aud.tmprev = timestamp32; // this may be wrong... tmprev is a 90KHz pts

  pFlvCtxt->out.prevtagidx = pFlvCtxt->out.bs.idx;
  memcpy(&bstmp, &pFlvCtxt->out.bs, sizeof(bstmp));
  pFlvCtxt->out.bs.idx += FLV_TAG_HDR_SZ_NOPREV;

  if((rc = flvsrv_create_audframe_aac(&pFlvCtxt->out.bs, &pFlvCtxt->av.aud, pFrame, NULL)) < 0) {
    return rc;
  }

  if(timestamp32 < pFlvCtxt->av.aud.tsLastWrittenFrame) {
    LOG(X_WARNING("FLV output audio ts went backwards %lld -> %d"), pFlvCtxt->av.aud.tsLastWrittenFrame, timestamp32);
    return 0;
  }

  lenData = rc;
  pFlvCtxt->av.aud.tsLastWrittenFrame = timestamp32;

  //
  // Now that the data length is known, fill in the 11 byte flv header
  //
  flv_write_taghdr(&bstmp, FLV_TAG_AUDIODATA, 
                   lenData + pFlvCtxt->out.bs.idx - pFlvCtxt->out.prevtagidx - FLV_TAG_HDR_SZ_NOPREV, 
                   timestamp32); 

  pFlvCtxt->writeErrDescr = "audio header";
  if((rc = pFlvCtxt->cbWrite(pFlvCtxt, pFlvCtxt->out.bs.buf, pFlvCtxt->out.bs.idx)) < 0) {
    return rc;
  }

  pFlvCtxt->writeErrDescr = "audio payload";
  if((rc = pFlvCtxt->cbWrite(pFlvCtxt, &OUTFMT_DATA(pFrame)[OUTFMT_LEN(pFrame) - lenData], lenData)) < 0) { 
    return rc;
  }

  //
  // 4 byte previous tag size field
  //
  *((uint32_t *) pFlvCtxt->out.bs.buf) = htonl(lenData + pFlvCtxt->out.bs.idx - pFlvCtxt->out.prevtagidx);
  pFlvCtxt->writeErrDescr = "audio trailer";
  if((rc = pFlvCtxt->cbWrite(pFlvCtxt, pFlvCtxt->out.bs.buf, 4)) < 0) {
    return rc;
  }

  return rc;
}

static int flvsrv_check_seqhdr_vp8(CODEC_VID_CTXT_T *pVidCtxt, const OUTFMT_FRAME_DATA_T *pFrame, 
                                    int *pkeyframe) {
  int rc = 0;
  VP8_DESCR_T vp8;

  if(*pkeyframe) {

    pVidCtxt->haveKeyFrame = *pkeyframe;
    pVidCtxt->tm0 = pVidCtxt->tmprev = OUTFMT_PTS(pFrame);
    pVidCtxt->haveSeqHdr = 1;
    pVidCtxt->ctxt.pstartHdrs = NULL;
    pVidCtxt->ctxt.startHdrsLen = 0;

    //
    // We are not extracting the VP8 dimensions from the bistream (like in H.264)
    //
    if(pVidCtxt->pStreamerCfg) {
      pVidCtxt->ctxt.width = pVidCtxt->pStreamerCfg->status.vidProps[pFrame->xout.outidx].resH;
      pVidCtxt->ctxt.height = pVidCtxt->pStreamerCfg->status.vidProps[pFrame->xout.outidx].resV;
      vid_convert_fps(pVidCtxt->pStreamerCfg->status.vidProps[pFrame->xout.outidx].fps,
                      &pVidCtxt->ctxt.clockHz,
                      &pVidCtxt->ctxt.frameDeltaHz);
    } 


    if(pVidCtxt->ctxt.width == 0 || pVidCtxt->ctxt.height == 0) {

      //
      // We shouldn't really get in here as 
      // this should have already been set from vp8_loadFramesFromMkv - poll for first keyframe
      //

      if((rc = vp8_parse_hdr(OUTFMT_DATA(pFrame), OUTFMT_LEN(pFrame), &vp8)) < 0) {
        return rc;
      }
      pVidCtxt->ctxt.width = vp8.frameWidthPx;
      pVidCtxt->ctxt.height = vp8.frameHeightPx;
      pVidCtxt->pStreamerCfg->status.vidProps[pFrame->xout.outidx].resH = pVidCtxt->ctxt.width;
      pVidCtxt->pStreamerCfg->status.vidProps[pFrame->xout.outidx].resV = pVidCtxt->ctxt.height;
    }


    //fprintf(stderr, "FPS:%.3f %d / %d, %dx%d\n", pVidCtxt->pStreamerCfg->status.vidProps[pFrame->xout.outidx].fps, pVidCtxt->ctxt.clockHz, pVidCtxt->ctxt.frameDeltaHz, pVidCtxt->ctxt.width, pVidCtxt->ctxt.height);

    rc = 1;
  }

  return rc;
}

static int flvsrv_check_seqhdr_h264(CODEC_VID_CTXT_T *pVidCtxt, const OUTFMT_FRAME_DATA_T *pFrame, 
                                    int *pkeyframe) {

  const SPSPPS_RAW_T *pSpspps;
  int keyframe = 0;
  XCODE_H264_OUT_T h264out;
  int rc;

  if(!pVidCtxt || !pFrame) {
    return -1;
  }

  if(!pkeyframe) {
    pkeyframe = &keyframe;
  }


  //pSpspps = &pFrame->vseqhdr.h264;
  pSpspps = &OUTFMT_VSEQHDR(pFrame).h264;
  *pkeyframe = OUTFMT_KEYFRAME(pFrame);

  if(pSpspps->sps_len == 0 || pSpspps->pps_len == 0) {
    pSpspps = &pVidCtxt->seqhdr.h264;
    if(!*pkeyframe && ((OUTFMT_DATA(pFrame)[4] & NAL_TYPE_MASK) == NAL_TYPE_IDR ||
                       (OUTFMT_DATA(pFrame)[4] & NAL_TYPE_MASK) == NAL_TYPE_SEQ_PARAM)) {
      *pkeyframe = 1;
      if(!pVidCtxt->haveKeyFrame) {
        pVidCtxt->haveKeyFrame = 1;
      }
    }
  }

  //LOG(X_DEBUG("Check h264... len:%d"), OUTFMT_LEN(pFrame)); LOGHEX_DEBUG(OUTFMT_DATA(pFrame), MIN(64, OUTFMT_LEN(pFrame)));

  //fprintf(stderr, "FLVSRV_CHECK_SEQHDR outidx:%d, 0x%x sps:%d pps:%d\n", pFrame->xout.outidx, OUTFMT_DATA(pFrame)[4], pSpspps->sps_len, pSpspps->pps_len); avc_dumpHex(stderr, OUTFMT_DATA(pFrame), MIN(32, OUTFMT_LEN(pFrame)), 1);

  if(pSpspps->sps_len == 0 || pSpspps->pps_len == 0 || !pVidCtxt->haveKeyFrame) {
    memset(&h264out, 0, sizeof(h264out));

    //if(pFrame->keyframe)avc_dumpHex(stderr, pFrame->pData, MIN(128, pFrame->len), 1);

    if((rc = xcode_h264_find_spspps(&h264out, OUTFMT_DATA(pFrame), OUTFMT_LEN(pFrame), 
                                    &pVidCtxt->seqhdr.h264)) > 0) {
      pSpspps = &pVidCtxt->seqhdr.h264;
      *pkeyframe = 1;
      if(!pVidCtxt->haveKeyFrame) {
        pVidCtxt->haveKeyFrame = 1;
      }
    }

  }

  //fprintf(stderr, "sps:%d pps:%d key:%d\n", pSpspps->sps_len,pSpspps->pps_len, *pkeyframe);
  //if((pFrame->pData[4] & NAL_TYPE_MASK) != 1) {
  //avc_dumpHex(stderr, pFrame->pData, 48, 1);
  //}

  //if(pSpspps->sps_len == 0 || pSpspps->pps_len == 0 || !*pkeyframe) {
  if(pSpspps->sps_len == 0 || pSpspps->pps_len == 0) {
    return 0;
  }
  //LOG(X_DEBUG("Check h264... calling flvsrv_init_vid_h264... sps_len:%d, pps_len:%d"), pSpspps->sps_len, pSpspps->pps_len); 
  if(flvsrv_init_vid_h264(pVidCtxt, pSpspps) >= 0) {
    pVidCtxt->tm0 = pVidCtxt->tmprev = OUTFMT_PTS(pFrame);
    pVidCtxt->haveSeqHdr = 1;
  }

  //fprintf(stderr, "h264 profile:%u(%u), avccRawLen:%d, avcc:0x%x\n", pVidCtxt->codecCtxt.h264.avcc.avcprofile, pVidCtxt->codecCtxt.h264.profile, pVidCtxt->codecCtxt.h264.avccRawLen, &pVidCtxt->codecCtxt.h264.avcc);

  return 1;
}

int flvsrv_check_vidseqhdr(CODEC_VID_CTXT_T *pVidCtxt, const OUTFMT_FRAME_DATA_T *pFrame, int *pkeyframe) {
  int rc = 0;

  pVidCtxt->codecType = pFrame->mediaType;

  if(pFrame->mediaType == XC_CODEC_TYPE_H264) {

    rc = flvsrv_check_seqhdr_h264(pVidCtxt, pFrame, pkeyframe);

  } else if(pFrame->mediaType == XC_CODEC_TYPE_VP8) {

    rc = flvsrv_check_seqhdr_vp8(pVidCtxt, pFrame, pkeyframe);

  } else {
    LOG(X_ERROR("Unsupported video codec %s"), codecType_getCodecDescrStr(pFrame->mediaType));
    rc = -1;
  }

  return rc;
}

int flvsrv_check_audseqhdr(CODEC_AUD_CTXT_T *pAudCtxt, const OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;

  if(!pAudCtxt || !pFrame) {
    return -1;
  }

  pAudCtxt->codecType = pFrame->mediaType;

  if(pFrame->mediaType == XC_CODEC_TYPE_AAC) {

    if(OUTFMT_LEN(pFrame) >= 7 &&
       (rc = flvsrv_init_aud_aac(pAudCtxt, OUTFMT_DATA(pFrame))) >= 0) {

      pAudCtxt->tm0 = 0;
      pAudCtxt->haveSeqHdr = 1;

      rc = 1;
    }

  } else if(pFrame->mediaType == XC_CODEC_TYPE_VORBIS) {


    if((rc = flvsrv_init_aud_vorbis(pAudCtxt)) >= 0) {

      pAudCtxt->tm0 = 0;
      pAudCtxt->haveSeqHdr = 1;

      rc = 1;
    }

    //pAudCtxt->ctxt.pstartHdrs = pAudCtxt->pStreamerCfg->xcode.aud.phdr;
    //pAudCtxt->ctxt.startHdrsLen = pAudCtxt->pStreamerCfg->xcode.aud.hdrLen;

  } else if(pFrame->mediaType == XC_CODEC_TYPE_AMRNB) {
    pAudCtxt->tm0 = 0;
    pAudCtxt->haveSeqHdr = 1;
    rc = 1;
  } else {
    LOG(X_ERROR("Unsupported audio codec %s"), codecType_getCodecDescrStr(pFrame->mediaType));
    rc = -1;
  }

  return rc;

}

static int flvsrv_sendBodyStart(FLVSRV_CTXT_T *pFlvCtxt) {
  int rc = 0;
  FLV_BYTE_STREAM_T fbs;
  unsigned char buf[1024];

  fbs.bs.sz = sizeof(buf);
  fbs.bs.buf = buf;
  fbs.bs.idx = 0;

  // TODO: set audio or video
  if((rc = flv_write_filebody(&fbs, 1, 1)) < 0) {
    return -1;
  }
  if((rc = flv_write_onmeta(&pFlvCtxt->av, &fbs)) < 0) {
    return -1;
  }

  //fprintf(stderr, "DUMPING START: %d\n", fbs.bs.idx);
  //avc_dumpHex(stderr, fbs.bs.buf, fbs.bs.idx, 1);

  pFlvCtxt->writeErrDescr = "body start";
  if((rc = pFlvCtxt->cbWrite(pFlvCtxt, fbs.bs.buf, fbs.bs.idx)) < 0) {
    return rc;
  }

  pFlvCtxt->senthdr = 1;

  return rc;
}

int flvsrv_sendHttpResp(FLVSRV_CTXT_T *pFlvCtxt) {
  int rc;
  FILE_OFFSET_T lenLive = 0;

  http_log(pFlvCtxt->pSd, pFlvCtxt->pReq, HTTP_STATUS_OK, lenLive);

  if((rc = http_resp_sendhdr(pFlvCtxt->pSd, pFlvCtxt->pReq->version, HTTP_STATUS_OK,
                   lenLive, CONTENT_TYPE_FLV, http_getConnTypeStr(pFlvCtxt->pReq->connType),
                   pFlvCtxt->pReq->cookie, NULL, NULL, NULL, NULL, NULL)) < 0) {
    pFlvCtxt->state = FLVSRV_STATE_ERROR;
    return rc;
  }

  return rc;
}

void codec_nonkeyframes_warn(CODEC_VID_CTXT_T *pVidCtxt, const char *descr) {
  pVidCtxt->nonKeyFrames++;
  if(pVidCtxt->nonKeyFrames % 300 == 299) {
    LOG(X_WARNING("%s Video output no key frame detected after %d video frames"),
            descr ? descr : "", pVidCtxt->nonKeyFrames);
  }
}

int codecfmt_init(CODEC_AV_CTXT_T *pAv, unsigned int vidTmpFrameSz, unsigned int audTmpFrameSz) {

  if(vidTmpFrameSz > 0) {
    pAv->vid.tmpFrame.sz = vidTmpFrameSz;
    if((pAv->vid.tmpFrame.buf = (unsigned char *) avc_calloc(1, pAv->vid.tmpFrame.sz)) == NULL) {
      codecfmt_close(pAv);
      return -1;
    }
  }

  if(audTmpFrameSz > 0) {
    pAv->aud.tmpFrame.sz = audTmpFrameSz;
    if((pAv->aud.tmpFrame.buf = (unsigned char *) avc_calloc(1, pAv->aud.tmpFrame.sz)) == NULL) {
       codecfmt_close(pAv);
      return -1;
    }
  }

  return 0;
}

void codecfmt_close(CODEC_AV_CTXT_T *pAv) {

  if(pAv->vid.tmpFrame.buf) {
    avc_free((void *) &pAv->vid.tmpFrame.buf);
  }
  pAv->vid.tmpFrame.sz = 0;

  if(pAv->aud.tmpFrame.buf) {
    avc_free((void *) &pAv->aud.tmpFrame.buf);
  }
  pAv->aud.tmpFrame.sz = 0;

  if(pAv->vid.codecType == XC_CODEC_TYPE_H264) {
    avcc_freeCfg(&pAv->vid.codecCtxt.h264.avcc);
  }

  pAv->vid.sentSeqHdr = 0;
  pAv->vid.haveSeqHdr = 0;
  pAv->aud.haveSeqHdr = 0;

}

/*
static int flvsrv_sendqueued(FLVSRV_CTXT_T *pFlvCtxt) {
  OUTFMT_FRAME_DATA_WRAP_T *pFrame;
  int rc = 0;
  unsigned int count = 0;

  if(pFlvCtxt->queuedFrames.count <= 0) {
    return 0;
  }

  //
  // If we skipped (queing) any frames, no point in trying to send any queued frames since there 
  // will be a gap.
  //
  if(pFlvCtxt->queuedFrames.skipped > 0) {
    return 0;
  }

  pFrame = pFlvCtxt->queuedFrames.phead;
  while(pFrame) {

    //fprintf(stderr, "FLVSRV_SEND_VIDFRAME QUEUED len:%d\n", OUTFMT_LEN(&pFrame->data));

    if((rc = flvsrv_send_vidframe(pFlvCtxt, &pFrame->data, OUTFMT_KEYFRAME(&pFrame->data))) < 0) {
      return rc; 
    }
    count++;

    pFrame = pFrame->pnext;
  }

  return count;
}
*/

int flvsrv_addFrame(void *pArg, const OUTFMT_FRAME_DATA_T *pFrame) { 
  int rc = 0;
  FLVSRV_CTXT_T *pFlvCtxt = (FLVSRV_CTXT_T *) pArg;
  OUTFMT_FRAME_DATA_T frame;
  int keyframe = 0;
  //const SPSPPS_RAW_T *pSpspps = &pFrame->vseqhdr.h264;
  //int keyframe = OUTFMT_KEYFRAME(pFrame);

//if(pFrame->isvid) fprintf(stderr, "KEYFRAME: %d\n", keyframe);return 0;

  if(!pArg || !pFrame) {
    return -1;
  } else if((pFrame->isvid && pFrame->mediaType != XC_CODEC_TYPE_H264) ||
            (pFrame->isaud && pFrame->mediaType != XC_CODEC_TYPE_AAC)) {
    LOG(X_ERROR("FLV %scodec type %s not supported"), pFrame->isvid ? "video " : "audio ",
         codecType_getCodecDescrStr(pFrame->mediaType));
    pFlvCtxt->state = FLVSRV_STATE_ERROR;
    return -1;
  }

//static int s_flv_idx; LOG(X_DEBUG("flvsrv_addFrame[%d,%d] %s [%d] len:%d pts:%.3f  dts:%.3f %s (vseq:%d, aseq:%d, sent:%d)"), pFlvCtxt->requestOutIdx, pFrame->xout.outidx, pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), pFrame->isvid ? s_flv_idx++ : 0,OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), PTSF(OUTFMT_DTS(pFrame)), OUTFMT_KEYFRAME(pFrame) ? " KEYFRAME" : "", pFlvCtxt->av.vid.haveSeqHdr, pFlvCtxt->av.aud.haveSeqHdr, pFlvCtxt->av.vid.sentSeqHdr);
  //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), 16, 1);
  //if(pFrame->isvid && OUTFMT_KEYFRAME(pFrame)) LOG(X_DEBUG("KEYFRAME sps:%d pps:%d"), OUTFMT_VSEQHDR(pFrame).h264.sps_len, OUTFMT_VSEQHDR(pFrame).h264.pps_len);

  if((pFrame->isvid && *pFlvCtxt->pnovid) || (pFrame->isaud && *pFlvCtxt->pnoaud)) {
    return rc;
  } 

  if(pFrame->isvid) {

    //
    // since pFrame is read-only, create a copy with the appropriate xcode out index
    //
    if(pFlvCtxt->requestOutIdx != pFrame->xout.outidx) {
      memcpy(&frame, pFrame, sizeof(frame));
      frame.xout.outidx = pFlvCtxt->requestOutIdx;
      pFrame = &frame;
    }

    keyframe = OUTFMT_KEYFRAME(pFrame);

    //fprintf(stderr, "flvsrv_addFrame %s len:%d pts:%llu dts:%lld key:%d (vseq:%d, aseq:%d, sent:%d)\n", pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), OUTFMT_LEN(pFrame), OUTFMT_PTS(pFrame), OUTFMT_DTS(pFrame), keyframe, pFlvCtxt->av.vid.haveSeqHdr, pFlvCtxt->av.aud.haveSeqHdr, pFlvCtxt->av.vid.sentSeqHdr);
    //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), 48, 1);

  }

  if(OUTFMT_DATA(pFrame) == NULL) {
    LOG(X_ERROR("FLV Frame output[%d] data not set"), pFrame->xout.outidx);
    return -1;
  }

  if(pFrame->isvid && !pFlvCtxt->av.vid.haveSeqHdr) {

    if((rc = flvsrv_check_vidseqhdr(&pFlvCtxt->av.vid, pFrame, &keyframe)) <= 0) {

      //
      // Request an IDR from the underlying encoder
      //
      if(pFlvCtxt->av.vid.pStreamerCfg) {
        streamer_requestFB(pFlvCtxt->av.vid.pStreamerCfg, pFrame->xout.outidx, ENCODER_FBREQ_TYPE_FIR, 
                            0, REQUEST_FB_FROM_LOCAL);
      }

      return rc;
    }

  } else if(pFrame->isaud && !pFlvCtxt->av.aud.haveSeqHdr) {

    if((rc = flvsrv_check_audseqhdr(&pFlvCtxt->av.aud, pFrame)) <= 0) {
      return rc;
    }

  }

  if(!pFlvCtxt->av.vid.sentSeqHdr && (*pFlvCtxt->pnovid || pFlvCtxt->av.vid.haveSeqHdr) &&
     (*pFlvCtxt->pnoaud || pFlvCtxt->av.aud.haveSeqHdr)) {

    pFlvCtxt->out.bs.idx = 0;

    if(rc >= 0 && !(*pFlvCtxt->pnovid)) {

      if((rc = flv_write_vidstart(&pFlvCtxt->out, &pFlvCtxt->av.vid)) < 0) {
        LOG(X_ERROR("Failed to create flv video start sequence"));
        pFlvCtxt->state = FLVSRV_STATE_ERROR;
        return rc;
      }
    }
    if(rc >= 0 && !(*pFlvCtxt->pnoaud)) {
      if((rc = flv_write_audstart(&pFlvCtxt->out, &pFlvCtxt->av.aud)) < 0) {
        LOG(X_ERROR("Failed to create flv audio start sequence"));
        pFlvCtxt->state = FLVSRV_STATE_ERROR;
        return rc;
      }
    }

    //TODO: get vid/aud presenet, width, ht, etc
    if(rc >= 0 && !pFlvCtxt->senthdr) {

      //
      // If a buffering delay has been set sleep for its duration
      // to allow a viewing client to pre-buffer the live contents.
      //
      if(!pFlvCtxt->pRecordOutFmt && pFlvCtxt->avBufferDelaySec > 0.0f) {
        usleep( (int) (pFlvCtxt->avBufferDelaySec * 1000000) );
      }

      //
      // Sent the FLV body start and onmeta tag
      // 
      if((rc = flvsrv_sendBodyStart(pFlvCtxt)) < 0) {
        pFlvCtxt->state = FLVSRV_STATE_ERROR;
        return rc;
      }
    }
  
    //
    // Sent the audio / video sequence start
    // 
    if(rc >= 0) {
      pFlvCtxt->writeErrDescr = "audio / video sequence start";
      if((rc = pFlvCtxt->cbWrite(pFlvCtxt, pFlvCtxt->out.bs.buf, pFlvCtxt->out.bs.idx)) < 0) {
        pFlvCtxt->state = FLVSRV_STATE_ERROR;
        return -1;
      }
    }

    if(rc >= 0) {
      pFlvCtxt->av.vid.sentSeqHdr = 1;
      LOG(X_DEBUG("flv play got audio / video start - beginning transmission"));
    }
  }



  if(rc >= 0) {
 
    if(!pFlvCtxt->av.vid.sentSeqHdr) {

      //
      // TODO: we're going to end up missing the first GOP in the case we get a vid keyframe
      // and then an audio start... because we don't write the FLV header start until we have  both
      //
      if(pFlvCtxt->pRecordOutFmt) {
        pFlvCtxt->av.vid.haveKeyFrame = 0;
      }

      //
      // Check for a case where we got a video keyframe, but not audio yet
      // and queue the video frame, which will be sent whenever the audio sequence start
      // is observed
      //
      if(pFrame->isvid && keyframe && pFlvCtxt->av.vid.haveSeqHdr && 
         !(*pFlvCtxt->pnoaud) && !pFlvCtxt->av.aud.haveSeqHdr) {
        if(pFlvCtxt->queuedFrames.count < pFlvCtxt->queuedFrames.max) {
          outfmt_queueframe(&pFlvCtxt->queuedFrames, pFrame);
        } else {
          pFlvCtxt->queuedFrames.skipped++;
        }
      }

      //fprintf(stderr, "TRY ABORT this is keyframe:%d, haveKeyFr:%d \n", keyframe, pFlvCtxt->av.vid.haveKeyFrame);
      return rc;
    }

    pFlvCtxt->out.bs.idx = 0;
    if(pFrame->isvid) {

      //
      // Only start sending video content beginning w/ a keyframe
      //
      if(!pFlvCtxt->av.vid.haveKeyFrame) { 
        if(keyframe) {
          pFlvCtxt->av.vid.haveKeyFrame = 1;
          pFlvCtxt->av.vid.nonKeyFrames = 0;
        } else {
          codec_nonkeyframes_warn(&pFlvCtxt->av.vid, "FLV");
        }
      }

      if(pFlvCtxt->av.vid.haveKeyFrame) {
        //LOG(X_DEBUG("FLVSRV_SEND_VIDFRAME len:%d"), OUTFMT_LEN(pFrame)); LOGHEX_DEBUG(OUTFMT_DATA(pFrame), MIN(16, OUTFMT_LEN(pFrame)));
        rc = flvsrv_send_vidframe(pFlvCtxt, pFrame, keyframe);
      }

    } else if(pFrame->isaud) {

      rc = 0;
      //
      // Check if we queued any video frames and send these first
      //
      if(pFlvCtxt->queuedFrames.count > 0) {
        //rc = flvsrv_sendqueued(pFlvCtxt);
        //outfmt_freequeuedframes(&pFlvCtxt->queuedFrames);
      }
  
      if(pFlvCtxt->av.aud.tm0 == 0) {
        if(pFlvCtxt->av.vid.haveSeqHdr && pFlvCtxt->av.vid.tm0 < OUTFMT_PTS(pFrame)) {
          pFlvCtxt->av.aud.tm0 = pFlvCtxt->av.aud.tmprev = pFlvCtxt->av.vid.tm0;
          //LOG(X_DEBUG("SET FLV audio start tm0 to: %.3f to match vid tm0"), PTSF(pFlvCtxt->av.aud.tm0));
        } else {
          pFlvCtxt->av.aud.tm0 = pFlvCtxt->av.aud.tmprev = OUTFMT_PTS(pFrame);
        }
      }
      //fprintf(stderr, "FLVSRV_SEND_AUDFRAME len:%d\n", OUTFMT_LEN(pFrame));
      if(rc >= 0) {
        rc = flvsrv_send_audframe(pFlvCtxt, pFrame);
      }
    }

  }

  if(rc < 0) {
    pFlvCtxt->state = FLVSRV_STATE_ERROR;
  }

  return rc;
}

int flvsrv_cbWriteDataNet(void *pArg, const unsigned char *pData, unsigned int len) {
  int rc = 0;
  FLVSRV_CTXT_T *pFlvCtxt = (FLVSRV_CTXT_T *) pArg;

  if((rc = netio_send(&pFlvCtxt->pSd->netsocket, (const struct sockaddr *) &pFlvCtxt->pSd->sa, pData, len)) < 0) {
    LOG(X_ERROR("Failed to send flv %s %u bytes, total: %llu)"),
               (pFlvCtxt->writeErrDescr ? pFlvCtxt->writeErrDescr : ""), len, pFlvCtxt->totXmit);
  } else {
    pFlvCtxt->totXmit += len;
//write(pFlvCtxt->g_fd, pData, len);
  }

  return rc;
}

int flvsrv_cbWriteDataFile(void *pArg, const unsigned char *pData, unsigned int len) {
  int rc = 0;
  FLVSRV_CTXT_T *pFlvCtxt = (FLVSRV_CTXT_T *) pArg;

  //fprintf(stderr, "CB FLV RECORD len:%d\n", len);

  if(pFlvCtxt->recordFile.fp == FILEOPS_INVALID_FP) {

    if(capture_openOutputFile(&pFlvCtxt->recordFile, pFlvCtxt->overwriteFile, NULL) < 0) {

      // Do not call outfmt_removeCb since we're invoked from the cb thread
      //outfmt_removeCb(pFlvCtxt->pRecordOutFmt);
      pFlvCtxt->pRecordOutFmt->do_outfmt = 0;
      pFlvCtxt->pRecordOutFmt = NULL;

      return -1;
    }
    LOG(X_INFO("Created flv recording output file %s"), pFlvCtxt->recordFile.filename);
   
  }

  if((rc = WriteFileStream(&pFlvCtxt->recordFile, pData, len)) < 0) {
    LOG(X_ERROR("Failed to write flv %s %u bytes, total: %llu)"),
               (pFlvCtxt->writeErrDescr ? pFlvCtxt->writeErrDescr : ""), len, pFlvCtxt->totXmit);
  } else {
    pFlvCtxt->totXmit += len;
  }

  return rc;
}

int flvsrv_record_start(FLVSRV_CTXT_T *pFlvCtxt, STREAMER_OUTFMT_T *pLiveFmt) {
  int rc = 0;
  unsigned int numQFull = 0;

  if(!pFlvCtxt || !pLiveFmt) {
    return -1;
  }

  //
  // Add a livefmt cb
  //
  pFlvCtxt->pRecordOutFmt = outfmt_setCb(pLiveFmt, flvsrv_addFrame, pFlvCtxt, 
                                         &pLiveFmt->qCfg, NULL, 0, 0, &numQFull);

  if(pFlvCtxt->pRecordOutFmt) {

   if(flvsrv_init(pFlvCtxt, pLiveFmt->qCfg.growMaxPktLen) < 0) {
      return rc;
    }
    pFlvCtxt->cbWrite = flvsrv_cbWriteDataFile;

    LOG(X_INFO("Starting flvlive recording[%d] %d/%d to %s"), pFlvCtxt->pRecordOutFmt->cbCtxt.idx, 
        numQFull + 1, pLiveFmt->max, pFlvCtxt->recordFile.filename);

  } else {
    LOG(X_ERROR("No flvlive recording resource available (max:%d)"), (pLiveFmt ? pLiveFmt->max : 0));
  }

  return rc;
}

int flvsrv_record_stop(FLVSRV_CTXT_T *pFlvCtxt) {
  int rc = 0;

  if(!pFlvCtxt || !pFlvCtxt->pRecordOutFmt) {
    return -1;
  }

  //
  // Remove the livefmt cb
  //
  outfmt_removeCb(pFlvCtxt->pRecordOutFmt);
  pFlvCtxt->pRecordOutFmt = NULL;
  CloseMediaFile(&pFlvCtxt->recordFile);

  flvsrv_close(pFlvCtxt);

  return rc;
}

int flvsrv_init(FLVSRV_CTXT_T *pFlv, unsigned int vidTmpFrameSz) {
  //int rc;

  pFlv->state = FLVSRV_STATE_INVALID;
  pFlv->cbWrite = flvsrv_cbWriteDataNet;

  pFlv->out.bs.sz = 1024;
  if((pFlv->out.bs.buf = (unsigned char *) avc_calloc(1, pFlv->out.bs.sz)) == NULL) {
    return -1;
  }

  if(vidTmpFrameSz == 0) {
    vidTmpFrameSz = STREAMER_RTMPQ_SZSLOT_MAX;
  }
  if((codecfmt_init(&pFlv->av, vidTmpFrameSz, 0)) < 0) {
  //pFlv->av.vid.tmpFrame.sz = vidTmpFrameSz;
  //if(!(pFlv->av.vid.tmpFrame.buf = (unsigned char *) avc_calloc(1, pFlv->av.vid.tmpFrame.sz))) {
    flvsrv_close(pFlv);
    return -1;
  }

  //
  // Enable queing up to 1 video frame - in the case where a keyframe is received, but no audio
  // start headers have yet been observed.  Otherwise, the video would not start until the next
  // video keyframe after the audio start headers have been observed
  //
  pFlv->queuedFrames.max = 1;

//pFlv->g_fd = open("test.flv", O_RDWR | O_CREAT, 0666);

  return 0;
}

void flvsrv_close(FLVSRV_CTXT_T *pFlv) {


  if(pFlv) {

    codecfmt_close(&pFlv->av);

    if(pFlv->out.bs.buf) {
      avc_free((void *) &pFlv->out.bs.buf);
    }
    pFlv->out.bs.sz = 0;

    //if(pFlv->av.vid.tmpFrame.buf) {
    //  avc_free((void *) &pFlv->av.vid.tmpFrame.buf);
    //}
    //pFlv->av.vid.tmpFrame.sz = 0;

    //if(pFlv->av.vid.codecType == XC_CODEC_TYPE_H264) {
    //  avcc_freeCfg(&pFlv->av.vid.codecCtxt.h264.avcc);
    //}

    outfmt_freequeuedframes(&pFlv->queuedFrames);

  }

}


