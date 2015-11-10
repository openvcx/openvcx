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


#define RTMP_FLASH_VER_CAPTURE_CLIENT       "WIN 10,0,32,18"
#define RTMP_FLASH_VER_PUBLISH              "FMLE/3.0 (compatible; FMSc/1.0)"


static int handle_audpkt_aac(RTMP_CTXT_CLIENT_T *pRtmp, FLV_TAG_AUDIO_T *pTag, 
                             unsigned char *pData, unsigned int len,
                             unsigned int ts) {

  int rc = 0;
  int idx;
  PKTQ_EXTRADATA_T xtra;
  unsigned char buf[32];

  if(pTag->aac.pkttype == FLV_AUD_AAC_PKTTYPE_SEQHDR) {


    if((rc = esds_decodeDecoderSp(pData, len, &pRtmp->ctxt.av.aud.codecCtxt.aac.esds)) < 0) {
      return rc;
    }
    
    //fprintf(stderr, "capture rtmp handle_audpkt_aac len:%d, haveExtFreqIdx:%d, pStreamerCfg:0x%x\n", len, pRtmp->ctxt.av.aud.codecCtxt.aac.esds.haveExtFreqIdx, pRtmp->ctxt.av.aud.pStreamerCfg);

//TODO: set pStreamerCfg xcode user data aac decoder specific here

 
    // Need a better way to do this,
    // but the aac input decoder may only have ADTS headers available, which do not include
    // aac esds extensions
    if(pRtmp->ctxt.av.aud.codecCtxt.aac.esds.haveExtFreqIdx &&
       pRtmp->ctxt.av.aud.pStreamerCfg) {
      memcpy(&XCODE_AUD_UDATA_PTR(&pRtmp->ctxt.av.aud.pStreamerCfg->xcode)->u.aac.descr.sp,
             &pRtmp->ctxt.av.aud.codecCtxt.aac.esds, sizeof(ESDS_DECODER_CFG_T));
      XCODE_AUD_UDATA_PTR(&pRtmp->ctxt.av.aud.pStreamerCfg->xcode)->u.aac.descr.sp.init = 1;
    }

    if(!pRtmp->ctxt.av.aud.haveSeqHdr) {
      pRtmp->ctxt.av.aud.haveSeqHdr = 1;
    }

  } else if(pTag->aac.pkttype == FLV_AUD_AAC_PKTTYPE_RAW) {

    if(!pRtmp->ctxt.av.aud.haveSeqHdr) {
      return 0;
    }

    if((idx = aac_encodeADTSHdr(buf, sizeof(buf),
                           &pRtmp->ctxt.av.aud.codecCtxt.aac.esds, len)) < 0) {
      LOG(X_ERROR("Failed to create ADTS header for frame"));
      return -1;
    }
    //avc_dumpHex(stderr, buf, idx, 1);

    if(idx + len >  pRtmp->ctxt.av.aud.tmpFrame.sz) {
      LOG(X_ERROR("Unable to append AAC frame length %d + %d > %d"),
                    idx, len, pRtmp->ctxt.av.aud.tmpFrame.sz); 
      return -1;
    }

    xtra.tm.pts = ts * 90;
    xtra.tm.dts = 0;
    //xtra.flags = 0;
    xtra.pQUserData = NULL;
 
    memcpy(pRtmp->ctxt.av.aud.tmpFrame.buf, buf, idx);
    memcpy(&pRtmp->ctxt.av.aud.tmpFrame.buf[idx], pData, len);

    if(pRtmp->pQAud->cfg.userDataType != XC_CODEC_TYPE_AAC) {
      pRtmp->pQAud->cfg.userDataType = XC_CODEC_TYPE_AAC;
    }

    VSX_DEBUG_INFRAME( 
     LOG(X_DEBUG("INFRAME - RTMP aud aac pts:%.3f dts:%.3f, ts: %u, len:%d, ts: %u (%llu)"),
        PTSF(xtra.tm.pts), PTSF(xtra.tm.dts), idx + len,  ts, PTSF(ts * 90)););

    rc = pktqueue_addpkt(pRtmp->pQAud, pRtmp->ctxt.av.aud.tmpFrame.buf, idx + len, &xtra, 0);
    VSX_DEBUG_RTMP( 
        LOG(X_DEBUG("RTMP - handle_audpkt_aac queing pkt rc:%d len:%d %.3f %.3f"), 
                        rc, idx + len, PTSF(xtra.tm.pts), PTSF(xtra.tm.dts));
     );

  }

  return rc;
}

static int handle_audpkt_mp3(RTMP_CTXT_CLIENT_T *pRtmp, FLV_TAG_AUDIO_T *pTag, 
                             unsigned char *pData, unsigned int len,
                             unsigned int ts) {

  int rc = 0;
  int idx = 0;
  PKTQ_EXTRADATA_T xtra;
  //unsigned char buf[32];

  if(idx + len >  pRtmp->ctxt.av.aud.tmpFrame.sz) {
    LOG(X_ERROR("Unable to append MP3 frame length %d + %d > %d"),
                  idx, len, pRtmp->ctxt.av.aud.tmpFrame.sz); 
    return -1;
  }

  xtra.tm.pts = ts * 90;
  xtra.tm.dts = 0;
  //xtra.flags = 0;
  xtra.pQUserData = NULL;
 
  memcpy(&pRtmp->ctxt.av.aud.tmpFrame.buf[idx], pData, len);

  if(pRtmp->pQAud->cfg.userDataType != XC_CODEC_TYPE_MPEGA_L3) {
    pRtmp->pQAud->cfg.userDataType = XC_CODEC_TYPE_MPEGA_L3;
  }
  rc = pktqueue_addpkt(pRtmp->pQAud, pRtmp->ctxt.av.aud.tmpFrame.buf, idx + len, &xtra, 0);
  //fprintf(stderr, "QUEING AUD MP3 rc:%d len:%d %.3f %.3f\n", rc, idx + len, PTSF(xtra.tm.pts), PTSF(xtra.tm.dts));

  return rc;
}

typedef struct H264_AVC_FRAME {
  BYTE_STREAM_T      *pTmpFrame;
  unsigned int        idx;
  int                 haveSeqHdrs;
  int                 keyFrame;
  uint8_t             nalType;
} H264_AVC_FRAME_T;

static int append_h264_frame(RTMP_CTXT_CLIENT_T *pRtmp, H264_AVC_FRAME_T *pFrame,
                             const unsigned char *pSliceData, unsigned int len) {
  int rc = 0;
  const uint32_t startCode = htonl(1); 

  if(pFrame->nalType == NAL_TYPE_SEQ_PARAM || 
     pFrame->nalType == NAL_TYPE_PIC_PARAM) {
    return 0;
  }

  if(pFrame->nalType == NAL_TYPE_IDR && !pFrame->haveSeqHdrs) {

    pFrame->keyFrame = 1;

    if(pFrame->idx + 8 + pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.psps[0].len + 
      pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.psps[0].len > pFrame->pTmpFrame->sz) {
      return -1;
    }

    memcpy(&pFrame->pTmpFrame->buf[pFrame->idx], &startCode, 4);
    pFrame->idx += 4;
    memcpy(&pFrame->pTmpFrame->buf[pFrame->idx], 
           pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.psps[0].data,
           pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.psps[0].len);
    pFrame->idx += pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.psps[0].len;

    memcpy(&pFrame->pTmpFrame->buf[pFrame->idx], &startCode, 4);
    pFrame->idx += 4;
    memcpy(&pFrame->pTmpFrame->buf[pFrame->idx], 
           pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.ppps[0].data,
           pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.ppps[0].len);
    pFrame->idx += pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.ppps[0].len;

    pFrame->haveSeqHdrs = 1;
  }

  if(pFrame->idx + len > pFrame->pTmpFrame->sz) {
    LOG(X_ERROR("Unable to append NAL slice length %d to [%d] / %d"),
              len, pFrame->idx, pFrame->pTmpFrame->sz); 
    return -1;
  }
  memcpy(&pFrame->pTmpFrame->buf[pFrame->idx], &startCode, 4);
  pFrame->idx += 4;
  memcpy(&pFrame->pTmpFrame->buf[pFrame->idx], pSliceData, len);
  pFrame->idx += len;

  return rc;
}

typedef struct PKQT_REORDER {
  PKTQUEUE_PKT_T          *pPkts;
  unsigned int             max;
} PKTQ_REORDER_T;


#if 0
static int reorder(PKTQUEUE_T *pQ, const unsigned char *pData, unsigned int len,
                   PKTQ_EXTRADATA_T *pXtra) {
  int rc = 0;


  return rc;
}

static void delmeh264(const unsigned char *pData, unsigned int len) {
  int slice_type;
  int frame_idx;
  int poc;
  BIT_STREAM_T bs264;

  avc_dumpHex(stderr, pData, 16, 1);

  memset(&bs264, 0, sizeof(bs264));
  bs264.buf = pData + 5;
  bs264.sz = len - 5;  
  bits_readExpGolomb(&bs264);
  if((slice_type = bits_readExpGolomb(&bs264)) > H264_SLICE_TYPE_SI) slice_type -= (H264_SLICE_TYPE_SI + 1);
  bits_readExpGolomb(&bs264);
  frame_idx = bits_read(&bs264, 7);
  poc = bits_read(&bs264, 7);
  fprintf(stderr, "H264 SLICE TYPE %d fridx:%d poc:%d\n", slice_type, frame_idx, poc);

}
#endif // 0

static int handle_vidpkt_avc(RTMP_CTXT_CLIENT_T *pRtmp, FLV_TAG_VIDEO_T *pTag, 
                             unsigned char *pData, unsigned int len,
                             unsigned int ts) {
  int rc = 0;
  int lennal;
  H264_AVC_FRAME_T frame;
  unsigned int idx = 0;
  unsigned int skip = 0;
  PKTQ_EXTRADATA_T xtra;

  memset(&frame, 0, sizeof(frame));
  frame.pTmpFrame =  &pRtmp->ctxt.av.vid.tmpFrame;

  //LOG(X_DEBUG("handle_vidpkt_avc type: %d, len:%d, ts:%d"), pTag->avc.pkttype, len, ts); LOGHEX_DEBUG(&pData[idx], len > 48 ? 48 : len);

  if(pTag->avc.pkttype == FLV_VID_AVC_PKTTYPE_SEQHDR) {

    //fprintf(stderr, "NAL SEQHDRS len:%d\n\n\n\n\n\n\n\n\n\n\n", len);
    //avc_dumpHex(stderr, pData, len, 1);

    avcc_freeCfg(&pRtmp->ctxt.av.vid.codecCtxt.h264.avcc);
    if((rc = avcc_initCfg(pData, len, &pRtmp->ctxt.av.vid.codecCtxt.h264.avcc)) < 0) {
      return rc; 
    }

    //avc_dumpHex(stderr, pRtmp->ctxt.av.vid.u.h264.avcc.psps[0].data, pRtmp->ctxt.av.vid.u.h264.avcc.psps[0].len, 1);
    //avc_dumpHex(stderr, pRtmp->ctxt.av.vid.u.h264.avcc.ppps[0].data, pRtmp->ctxt.av.vid.u.h264.avcc.ppps[0].len, 1);

    if(!pRtmp->ctxt.av.vid.haveSeqHdr) {
      pRtmp->ctxt.av.vid.haveSeqHdr = 1;
    }

  } else if(pTag->avc.pkttype == FLV_VID_AVC_PKTTYPE_NALU) {

    if(!pRtmp->ctxt.av.vid.haveSeqHdr) {
      return 0;
    }

    //fprintf(stderr, "avc pkt len:%u time:%u\n", len, pTag->avc.comptime32);

    while(idx < len) {
   
      if((lennal = h264_getAvcCNalLength(&pData[idx], len - idx,
          pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.lenminusone + 1)) < 0) {
        return -1;
      }
      idx += pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.lenminusone + 1;
      frame.nalType = (pData[idx] & NAL_TYPE_MASK);

      //
      // Workaround to handle AVC SPS , AVC NAL PPS + NAL IDR 
      //
      if((frame.nalType == NAL_TYPE_PIC_PARAM || frame.nalType == NAL_TYPE_ACCESS_UNIT_DELIM)
         && lennal == (len - idx)) {
        H264_STREAM_CHUNK_T h264Stream;
        memset(&h264Stream, 0, sizeof(h264Stream));
        h264Stream.bs.sz = 32;
        skip  = frame.nalType == NAL_TYPE_ACCESS_UNIT_DELIM ? 2 : 0;
        h264Stream.bs.buf = &pData[idx + skip];
        if((rc = h264_findStartCode(&h264Stream, 1, NULL)) > 0) {
          rc += skip;
          LOG(X_DEBUG("RTMP H.264 skipping %d of improper length %d H.264 0x%x NAL to get to 0x%x NAL"), rc, (len - idx - rc), frame.nalType, (pData[idx + rc] & NAL_TYPE_MASK));
          idx += rc;
          lennal = len - idx;
          frame.nalType = (pData[idx] & NAL_TYPE_MASK);
          //LOG(X_DEBUG("AT 0x%x 0x%x, idx:%d, lennal:%d "), pData[idx], pData[idx+1], idx, lennal);
        }

      }

      //fprintf(stderr, "NAL idx:%d/%d type:0x%x ts[%d]:%d tsAbsolute:%d tm:%d, len:%d 0x%x 0x%x 0x%x 0x%x\n", idx, len, frame.nalType, pRtmp->ctxt.streamIdx, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.ts, ts, pTag->avc.comptime32, lennal, pData[idx], pData[idx+1], pData[idx+2], pData[idx+3]);
      //avc_dumpHex(stderr, &pData[idx], len > 16 ? 16 : len, 1);

      if(frame.nalType != NAL_TYPE_ACCESS_UNIT_DELIM) {

        if(append_h264_frame(pRtmp, &frame, &pData[idx], lennal) < 0) {
          return -1;
        }
      }

      idx += lennal;
    }

    VSX_DEBUG_INFRAME( LOG(X_DEBUG("INFRAME - RTMP vid avc type: %d, len: %d, ts: %u, frame.idx:%d"), pTag->avc.pkttype, len, ts, frame.idx) );

    if(frame.idx > 0) {
      xtra.pQUserData = NULL;
      xtra.tm.pts = ts * 90;
      xtra.tm.dts = 0;

      if(pTag->avc.comptime32 != 0) {
        //xtra.tm.dts = xtra.tm.pts + (pTag->avc.comptime32 * 90);
        xtra.tm.dts = xtra.tm.pts;
      }

      if(pTag->avc.comptime32 != 0) {
        xtra.tm.pts += (pTag->avc.comptime32 * 90);
        //xtra.tm.pts = xtra.tm.dts;
        //if(pTag->avc.comptime32 * 90 < xtra.tm.pts) {
        //  xtra.tm.dts = xtra.tm.pts - (pTag->avc.comptime32 * 90);
        //}
      }

      VSX_DEBUG_INFRAME( 
       LOG(X_DEBUG("INFRAME - RTMP vid avc pts:%.3f dts:%.3f tm:%d, len:%d, key: %d, (from flv pkt:%.3f %.3f)"), 
          PTSF(xtra.tm.pts), PTSF(xtra.tm.dts), pTag->avc.comptime32, frame.idx, frame.keyFrame, PTSF(ts * 90), 
          PTSF(pTag->avc.comptime32 * 90)););

      //if(frame.keyFrame) {
      //  xtra.flags = CAPTURE_SP_FLAG_KEYFRAME;
      //} else {
      //  xtra.flags = 0;
      //}
      rc = pktqueue_addpkt(pRtmp->pQVid, frame.pTmpFrame->buf, frame.idx, &xtra, frame.keyFrame);
      //delmeh264(frame.pTmpFrame->buf, frame.idx);
      VSX_DEBUG_RTMP( 
      LOG(X_DEBUG("RTMP - handle_vidpkt_avc rc:%d len:%d pts:%.3f dts:%.3f (comptime32:%d) keyFr:%d (qmax:%d,%d)"),
          rc, frame.idx, PTSF(xtra.tm.pts), PTSF(xtra.tm.dts), pTag->avc.comptime32, frame.keyFrame, 
          pRtmp->pQVid->cfg.maxPktLen, pRtmp->pQVid->cfg.growMaxPktLen); );
      //avc_dumpHex(stderr, frame.pTmpFrame->buf, 96, 1);
    }

  }

  return rc;
}

static int handle_vidpkt_vp6(RTMP_CTXT_CLIENT_T *pRtmp, FLV_TAG_VIDEO_T *pTag, 
                             unsigned char *pData, unsigned int len,
                             unsigned int ts) {
  int rc = 0;
  PKTQ_EXTRADATA_T xtra;
  int keyFrame = 0;

  if(((pTag->frmtypecodec & 0xf0) >> 4) ==  FLV_VID_FRM_KEYFRAME) {
    keyFrame = 1;
    //xtra.flags = CAPTURE_SP_FLAG_KEYFRAME;
  //} else {
  //  xtra.flags = 0;
  }

  xtra.tm.pts = ts * 90;
  xtra.tm.dts = 0;
  xtra.pQUserData = NULL;

  if(pRtmp->pQVid->cfg.userDataType != XC_CODEC_TYPE_VP6F) {
    pRtmp->pQVid->cfg.userDataType = XC_CODEC_TYPE_VP6F;
  }

  rc = pktqueue_addpkt(pRtmp->pQVid, pData, len, &xtra, keyFrame);
 //fprintf(stderr, "QUEING VID vp6 rc:%d len:%d %.3f %.3f keyFr:%d\n", rc, len, PTSF(xtra.tm.pts), PTSF(xtra.tm.dts), (xtra.flags & CAPTURE_SP_FLAG_KEYFRAME));
  //avc_dumpHex(stderr, pData, 16, 1);

  return rc;
}

int rtmp_handle_vidpkt(RTMP_CTXT_CLIENT_T *pRtmp, unsigned char *pData,
                         unsigned int len, unsigned int ts) {
  int rc = 0;
  FLV_TAG_VIDEO_T vidTag;

  //avc_dumpHex(stderr, pData, len > 16 ? 16 : 5, 0); 
  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_handle_vidpkt len: %d, ts: %u"), len, ts) );

  if(len < 1) {
    return -1;
  }

  vidTag.frmtypecodec = pData[0];

  // FLV video tag
  switch(vidTag.frmtypecodec & 0x0f) {

    case FLV_VID_CODEC_AVC:

      if(len < 5) {
        return -1;
      }

      vidTag.avc.pkttype = pData[1];
      vidTag.avc.comptime32 = htonl(*((uint32_t *)&pData[2]) << 8);

      //
      // Check if comptime32 is actually a negative value
      //
      if(pData[3] == 0xff) {
        vidTag.avc.comptime32 |= 0xff000000; 
      }
      //fprintf(stderr, "comptime32:%d 0x%x 0x%x 0x%x\n", vidTag.avc.comptime32, pData[2], pData[3], pData[4]);

      rc = handle_vidpkt_avc(pRtmp, &vidTag, &pData[5], len - 5, ts);

      break; 

    case FLV_VID_CODEC_ON2VP6:

      //VSX_DEBUG_INFRAME( LOG(X_DEBUG("RTMP - rtmp_handle_vidpkt vp6 len: %d, ts: %u"), len, ts) );
      if(len < 2) {
        return -1;
      }

      //fprintf(stderr, "len:%d comptime32:%d 0x%x 0x%x 0x%x\n", len, vidTag.avc.comptime32, pData[2], pData[3], pData[4]);
     
      //avc_dumpHex(stderr, pData, len > 32? 32: 0, 1); 
      rc = handle_vidpkt_vp6(pRtmp, &vidTag, &pData[2], len - 2, ts);

      break;

    default: 
      LOG(X_WARNING("FLV video codec id %d not supported"), (vidTag.frmtypecodec & 0x0f));
      return -1;
  }


  return rc;
}

int rtmp_handle_audpkt(RTMP_CTXT_CLIENT_T *pRtmp, unsigned char *pData, 
                         unsigned int len, unsigned int ts) {
  int rc = 0;
  FLV_TAG_AUDIO_T audTag;
  //unsigned char *pData = pRtmp->ctxt.in.buf;
  //unsigned int len = pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_handle_audpkt len: %d, ts: %u"), len, ts) );

  if(len < 1) {
    return -1;
  }

  audTag.frmratesz = pData[0];

  //fprintf(stderr, "AUDIO PKT len:%d tm:%d\n", len, ts);
  //avc_dumpHex(stderr, pData, len > 32 ? 32 : len, 1);

  // FLV audio tag
  switch((audTag.frmratesz >> 4)) {
    case  FLV_AUD_CODEC_AAC:
      if(len < 2) {
        return -1;
      }
      audTag.aac.pkttype = pData[1];
      rc = handle_audpkt_aac(pRtmp, &audTag, &pData[2], len - 2, ts);
      break;
    case FLV_AUD_CODEC_MP3:
      rc = handle_audpkt_mp3(pRtmp, &audTag,  &pData[1], len - 1, ts);
      break;
    default:
      LOG(X_WARNING("FLV audio codec id %d not supported"), (audTag.frmratesz >> 4));
      return -1;
  }


  //fprintf(stderr, "audio len:%u 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x\n", len, pData[0], pData[1], pData[2], pData[3], len>3 ? pData[len-4] :0 , len>2 ? pData[len-3]:0,len>1? pData[len-2]:0, len>0?pData[len-1]:0);

  return rc;
}

static int handle_flv(RTMP_CTXT_CLIENT_T *pRtmp) {
  int rc = 0;
  unsigned int idx = 0;
  FLV_TAG_HDR_T hdr;
  unsigned char *pData = pRtmp->ctxt.in.buf;
  unsigned int len = pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt;
  unsigned int ts;
  unsigned int ts0 = 0;
  unsigned int idxPkt = 0;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - handle_flv len: %d"), len) );

  while(idx < len) {

    if(idx + 11 >= len) {
      if(idx == 0 || idx + 4 < len) {
        LOG(X_WARNING("RTMP FLV index %d/%d insufficient header length"), idx, len);
      }
      return rc;
    }

    memset(&hdr, 0, sizeof(hdr));

    if(idx > 0) {
      if(idx + 15 >= len) {
        LOG(X_WARNING("RTMP FLV index %d/%d insufficient previous pkt length"),
            idx, len);
        return rc;
      }
      hdr.szprev = htonl(*((uint32_t *) &pData[idx]));
      idx += 4;
    }
    hdr.type = pData[idx++];

    memcpy(&hdr.size32, &pData[idx], 3);
    idx += 3;
    hdr.size32 = htonl(hdr.size32 << 8);

    memcpy(&hdr.timestamp32, &pData[idx], 3);
    idx += 3;
    hdr.tsext = pData[idx++];
    hdr.timestamp32 = htonl(hdr.timestamp32 << 8);
    hdr.timestamp32 |= (hdr.tsext << 24);

    memcpy(&hdr.streamid, &pData[idx], 3);
    idx += 3;

    if(idxPkt == 0) {
      ts0 = hdr.timestamp32;
    }
    //
    // timestamps of packets within FLV packet appear to have an 
    // absolute time
    //
    ts = (hdr.timestamp32 - ts0) + 
          pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].tsAbsolute;

    //fprintf(stderr, "FLV PKT TYPE:0x%x LEN:%d (%d/%d) ts:%u (0x%x %x %x %x)\n", hdr.type, hdr.size32, idx, len, hdr.timestamp32, pData[idx-7], pData[idx-6], pData[idx-5], pData[idx-4]);

    switch(hdr.type) {
      case RTMP_CONTENT_TYPE_VIDDATA:
        rtmp_handle_vidpkt(pRtmp, &pData[idx], hdr.size32, ts);
        break;
      case RTMP_CONTENT_TYPE_AUDDATA:
        rtmp_handle_audpkt(pRtmp, &pData[idx], hdr.size32, ts);
        break;
      default:
        LOG(X_WARNING("Unhandled flv packet type: 0x%x"), hdr.type);
        break;
    }

    idx += hdr.size32;
    idxPkt++;

  }

  return rc;
}

static int rtmp_create_ping_client(RTMP_CTXT_T *pRtmp, uint16_t type, 
                                   uint32_t arg, int haveArg3, uint32_t arg2) {
  int rc;
  BYTE_STREAM_T bs;

  bs.idx = 8;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx;

  //memset(&bs.buf[bs.idx], 0, 10); 
  *((uint16_t *) &bs.buf[bs.idx]) = htons(type);
  bs.idx += 2;
  *((uint32_t *) &bs.buf[bs.idx]) = htonl(arg);
  bs.idx += 4;
  if(haveArg3) {
    *((uint32_t *) &bs.buf[bs.idx]) = htonl(arg2);
    bs.idx += 4;
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
            RTMP_STREAM_IDX_CTRL, 0, bs.idx - 8, RTMP_CONTENT_TYPE_PING, 0, 8)) < 0) {
    return rc;
  }


  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx;
}

static int rtmp_create_bytes_read(RTMP_CTXT_T *pRtmp) {
  int rc;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
            RTMP_STREAM_IDX_CTRL, 0, 4, RTMP_CONTENT_TYPE_BYTESREAD, 0, 8)) < 0) {
    return rc;
  }

  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(pRtmp->bytesRead);

  pRtmp->out.idx += (rc + 4);

  return rc + 4;
}


static int rtmp_connect(RTMP_CTXT_CLIENT_T *pClient, const char *flashVer) {
  int rc = 0;
  size_t sz;
  char authstr[128];
  char portstr[32];
  const char *app = NULL;
  
  portstr[0] = '\0';
  if(ntohs(INET_PORT(pClient->ctxt.pSd->sa)) != RTMP_LISTEN_PORT) {
    snprintf(portstr, sizeof(portstr) - 1, ":%d", ntohs(INET_PORT(pClient->ctxt.pSd->sa)));
  } 

  //LOG(X_DEBUG("URL: '%s' URI: '%s', lastAuthState: %d, salt: '%s', username: '%s'"), pClient->purl, pClient->puri, pClient->auth.ctxt.lastAuthState, pClient->auth.ctxt.p.salt, pClient->auth.pcreds ? pClient->auth.pcreds->username: "<NULL>");

  pClient->client.playElem = pClient->puridocname;
  app = pClient->puri;
  if(*app == '\0') {
    app = pClient->puridocname;
  }
  if(*app == '/') {
    app++;
  }
  pClient->client.flashVer = flashVer;
  //pClient->client.swfUrl = "http://192.168.1.151:8082/wowza/LiveVideoStreaming/client/live.swf";
  //pClient->client.pageUrl = "http://192.168.1.151:8082/wowza/LiveVideoStreaming/client/live.html";
  authstr[0] = '\0';

  //
  // Add client authentication user, challenge, response, opaque to the URI
  //
  if(pClient->auth.ctxt.lastAuthState == RTMP_AUTH_STATE_NEED_CHALLENGE ||
     pClient->auth.ctxt.lastAuthState == RTMP_AUTH_STATE_HAVE_CHALLENGE) {

    if(!pClient->auth.pcreds) {
      return -1;
    }

    rc = snprintf(authstr, sizeof(authstr), "?authmod=%s&user=%s", 
           rtmp_auth_getauthmodstr(pClient->auth.ctxt.authmod), pClient->auth.pcreds->username);
    if(rc >= 0 && pClient->auth.ctxt.lastAuthState == RTMP_AUTH_STATE_HAVE_CHALLENGE) {
      sz = rc; 
      pClient->auth.ctxt.challenge_cli[0] = '\0';
      if((rc = rtmp_auth_compute(&pClient->auth.ctxt, pClient->auth.pcreds)) < 0) {
        LOG(X_ERROR("RTMP failed to create authmod connect request"));
        return rc;
      } else {
        rc = snprintf(&authstr[sz], sizeof(authstr) - sz, "&challenge=%s&response=%s&opaque=%s",
                    pClient->auth.ctxt.challenge_cli, pClient->auth.ctxt.response, pClient->auth.ctxt.p.opaque);
      }
    }
  }
  
  snprintf(pClient->ctxt.connect.app, sizeof(pClient->ctxt.connect.app), "%s%s", app, authstr);
  snprintf(pClient->ctxt.connect.tcUrl, sizeof(pClient->ctxt.connect.tcUrl), 
          "rtmp://%s%s%s%s%s", pClient->purl, portstr, *pClient->puri != '\0' ? "/" : "", pClient->puri, authstr);

   LOG(X_DEBUG("RTMP connect app: '%s', flashVer: '%s', swfUrl: '%s', tcUrl: '%s', pageUrl: '%s', "
               "playElement: '%s'"), 
     pClient->ctxt.connect.app, pClient->client.flashVer, pClient->client.cfg.prtmpswfurl, 
     pClient->ctxt.connect.tcUrl, pClient->client.cfg.prtmppageurl, pClient->client.playElem); 

  pClient->ctxt.out.idx = 0;
  if((rc = rtmp_create_chunksz(&pClient->ctxt)) < 0) {
    return rc;
  }

  if((rc = rtmp_create_connect(&pClient->ctxt, &pClient->client)) < 0) {
    return rc; 
  }

  //
  // If we're doing RTMP tunneling, and have been previously queuing outbound data (from handshake)
  // then make sure we flush everything out here
  //
  pClient->ctxt.rtmpt.doQueueOut = 0;

  if((rc = rtmp_send(&pClient->ctxt, "rtmp_connect")) < 0) {
    return -1;
  }

  pClient->ctxt.out.idx = 0;

  return rc;
}

static CONNECT_RETRY_RC_T rtmp_releaseStream(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClientParams) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;
  int rc = 0;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_releaseStream called...")) );

  if(rc >= 0 && (rc = rtmp_create_releaseStream(pRtmp, pClientParams)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  if(rc >= 0) {

    if((rc = rtmp_send(pRtmp, "releaseStream")) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
    pRtmp->out.idx = 0; 
  }

  if(rc < 0) {
    pRtmp->state = RTMP_STATE_ERROR;
  }

  return retryRc;
}

static CONNECT_RETRY_RC_T rtmp_fcpublish(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClientParams) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;
  int rc = 0;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_fcpublish called [out.idx: %d]..."), pRtmp->out.idx) );

  if(rc >= 0 && (rc = rtmp_create_fcpublish(pRtmp, pClientParams)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  if(rc >= 0) {

    if((rc = rtmp_send(pRtmp, "fcpublish")) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
    pRtmp->out.idx = 0; 
  }

  if(rc < 0) {
    pRtmp->state = RTMP_STATE_ERROR;
  }

  return retryRc;
}

static CONNECT_RETRY_RC_T rtmp_publish(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClientParams) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;
  int rc = 0;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_publish called [out.idx: %d]..."), pRtmp->out.idx) );

  if(rc >= 0 && (rc = rtmp_create_publish(pRtmp, pClientParams)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  if(rc >= 0) {

    if((rc = rtmp_send(pRtmp, "publish")) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
    pRtmp->out.idx = 0; 
  }

  if(rc < 0) {
    pRtmp->state = RTMP_STATE_ERROR;
  }

  return retryRc;
}

static CONNECT_RETRY_RC_T rtmp_serverBw(RTMP_CTXT_T *pRtmp) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;
  int rc = 0;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_serverBw called [out.idx: %d]..."), pRtmp->out.idx) );

  if(pRtmp->serverbw > 0) {
    if(rc >= 0 && (rc = rtmp_create_serverbw(pRtmp, pRtmp->serverbw)) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }

    if((rc = rtmp_send(pRtmp, "serverBW")) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
    pRtmp->out.idx = 0; 

  }

  return retryRc;
}

static CONNECT_RETRY_RC_T rtmp_createStream(RTMP_CTXT_T *pRtmp, int contentType, int do_ping) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;
  int rc = 0;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_createStream called [out.idx: %d]..."), pRtmp->out.idx) );

  pRtmp->rtmpt.doQueueOut = 1;

/*
  if(pRtmp->serverbw > 0) {
    if(rc >= 0 && (rc = rtmp_create_serverbw(pRtmp, pRtmp->serverbw)) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
  }
*/

  if(rc >= 0 && (rc = rtmp_create_createStream(pRtmp, contentType)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  if(rc >= 0 && do_ping && (rc = rtmp_create_ping_client(pRtmp, 0x03, 0x00, 1, 0x0bb8)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  pRtmp->rtmpt.doQueueOut = 0;

  if(rc >= 0) {

    if((rc = rtmp_send(pRtmp, "createStream")) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
    pRtmp->out.idx = 0; 
  }

  if(rc < 0) {
    pRtmp->state = RTMP_STATE_ERROR;
  }

  return retryRc;
}

static CONNECT_RETRY_RC_T rtmp_play(RTMP_CTXT_CLIENT_T *pClient) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK; 
  int rc = 0;
  unsigned int lenData;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_play called [out.idx: %d]..."), pClient->ctxt.out.idx) );

  pClient->ctxt.rtmpt.doQueueOut = 1;

  if(rc >= 0 && (rc = rtmp_create_play(&pClient->ctxt, &pClient->client)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  if(rc >= 0) {
    lenData = pClient->ctxt.out.idx - 12;
    pClient->ctxt.out.idx = 12;
    if((rc = rtmp_send_chunkdata(&pClient->ctxt, RTMP_STREAM_IDX_INVOKE, 
                               &pClient->ctxt.out.buf[12], lenData, 0)) < 0) {
      retryRc = CONNECT_RETRY_RC_ERROR;
    }
    pClient->ctxt.out.idx = 0; 
  }

  if(rc >= 0 && (rc = rtmp_create_ping_client(&pClient->ctxt, 0x03, 0x01, 1, 0x0bb8)) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  pClient->ctxt.rtmpt.doQueueOut = 0;

  if(rc >= 0 && (rc = rtmp_send(&pClient->ctxt, "play")) < 0) {
    retryRc = CONNECT_RETRY_RC_ERROR;
  }

  pClient->ctxt.out.idx = 0; 

  if(rc < 0) {
    pClient->ctxt.state = RTMP_STATE_ERROR;
  }

  return retryRc;
}

static int rtmp_parse_serverbw(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  BYTE_STREAM_T bs;

  bs.buf = pRtmp->in.buf;
  bs.sz = pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt;
  bs.idx = 0;

  if(bs.sz < 4) {
    return -1;
  } else {
    pRtmp->serverbw = htonl(*((int *)  &bs.buf[0]));
  }

  return rc;
}

static int parse_handle_ping(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  uint16_t type;
  uint32_t arg1;
  char tmp[128];

  type = htons( *((uint16_t *) &pRtmp->in.buf[0]) ); 

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_handle_ping received rtmp ping type: 0x%x"), type); );
  //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);

  switch(type) {
    case 0:
      // stream begin
      break;
    case 1:
      // stream EOF 
      break;
    case 2:
      // stream Dry 
      break;
    case 4:
      // stream Recorded
      break;
    case 6:
      // keepalive request 
      if(pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt >= 6) {

        arg1 = htonl(*((uint32_t *) &pRtmp->in.buf[2])); 
        LOG(X_DEBUG("RTMP sending keep alive response 0x%x to %s:%d"), arg1,
           FORMAT_NETADDR(pRtmp->pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pRtmp->pSd->sa)));

        if((rc = rtmp_create_ping_client(pRtmp, 0x07, arg1, 0, 0)) < 0 ||
           (rc = rtmp_send(pRtmp, "ping reply")) < 0) {
          LOG(X_ERROR("Failed to reply to server ping keep alive"));
          return -1;
        }
        //avc_dumpHex(stderr, pRtmp->out.buf, pRtmp->out.idx, 1);
        pRtmp->out.idx = 0;
      }
      break;
    case 31:
      // buffer empty 
      break;
    case 32:
      // buffer ready 
      break;
    default:
      LOG(X_DEBUG("RTMP unknown ping type 0x%x received"), type);
      break;
  }

  return rc;
}

static int parse_readpkt(RTMP_CTXT_T *pRtmp, FLV_AMF_T *pAmfList) {
  int rc = 0;
  int contentType;

  pRtmp->in.idx = 0;
  pRtmp->methodParsed = RTMP_METHOD_UNKNOWN;

  if((rc = rtmp_parse_readpkt_full(pRtmp, 1, pAmfList)) < 0) {
    return -1;
  }

  contentType = pRtmp->pkt[pRtmp->streamIdx].hdr.contentType;

  switch(contentType) {

    case RTMP_CONTENT_TYPE_SERVERBW:
      rtmp_parse_serverbw(pRtmp);
      break;
    case RTMP_CONTENT_TYPE_PING:
      parse_handle_ping(pRtmp);    
      break;
    default:
      break;
  }

/*
  fprintf(stderr, "RC:%d\n", rc);
  if(rc >= 0) {
    pAmf = pAmfList;
    while(pAmf) {
      if(pAmf->key.p) {
      fprintf(stderr, "'%s'->", pAmf->key.p ? (char *)pAmf->key.p : "");
      if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
        fprintf(stderr, "'%s'\n", pAmf->val.u.str.p);
      } else if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
        fprintf(stderr, "%f\n", pAmf->val.u.d);
      } else {
        fprintf(stderr, "\n");
      }
      }
      pAmf = pAmf->pnext;
    }
  }
*/

  if(rc >= 0) {
    rc = contentType;
  }

  VSX_DEBUG_RTMP( 
    LOG(X_DEBUG("RTMP - parse_readpkt done with methodParsed: %d, contentType: 0x%x, state: %d, rc: %d"), 
                              pRtmp->methodParsed, contentType, pRtmp->state, rc) );

  return rc;
}

static int rtmp_is_resp_error(const FLV_AMF_T *pAmfList, const char *operation, int explicit, int logLevel) {
  int rc = 0;
  const FLV_AMF_T *pAmf;

  if(explicit || (pAmf = flv_amf_find(pAmfList, "_error"))) {

    rc = 1;

    LOG(logLevel, ("RTMP %s error code: '%s' description: '%s'\n"),
      operation, flv_amf_get_key_string(flv_amf_find(pAmfList, "code")),
      flv_amf_get_key_string(flv_amf_find(pAmfList, "description")));

  }

  return rc;
}

static int check_cli_auth(RTMP_CTXT_CLIENT_T *pClient, int *phaveConnect) {
  int rc = 0;
  RTMP_AUTH_PARSE_CTXT_T rtmpAuthParse;
  const char *p = NULL;
  const char *user = NULL;
  const char *reason = NULL;
  const char *response = NULL;
  const char *opaque_cli = NULL;
  size_t sz;
  char description[512];
  char challengestr[256];
  char codestr[256];
  RTMP_AUTH_CTXT_T *pAuth = &pClient->auth.ctxt;

  *phaveConnect = 0;

  if(!pClient->auth.pcreds || !IS_AUTH_CREDENTIALS_SET(pClient->auth.pcreds)) {
    *phaveConnect = 1;
    LOG(X_DEBUG("RTMP sending connect success response for app: '%s'"), pClient->ctxt.connect.app);
    return rtmp_create_result_invoke(&pClient->ctxt);
  }

  memset(&rtmpAuthParse, 0, sizeof(rtmpAuthParse));
  pAuth->authmod = RTMP_AUTHMOD_ADOBE;
  codestr[0] = '\0';
  challengestr[0] = '\0';
  pAuth->challenge_cli[0] = '\0';
  pAuth->response[0] = '\0';
  
  if(rtmp_auth_parse_req(pClient->ctxt.connect.app, &rtmpAuthParse) < 0) {
    LOG(X_ERROR("Failed to parse RTMP connect app: '%s'"), pClient->ctxt.connect.app);
    return -1;
  }

  if((user = conf_find_keyval(&rtmpAuthParse.authList.list[0], "user"))) {

    if(rtmp_auth_getauthmod(&rtmpAuthParse) != pAuth->authmod) {
      LOG(X_ERROR("RTMP connect authmod: '%s' does not match server autmod: '%s'"), 
                 rtmpAuthParse.authmodStr, rtmp_auth_getauthmodstr(pAuth->authmod)); 
      snprintf(challengestr, sizeof(challengestr), "?reason=%s", RTMP_AUTH_REASON_FAILED);
      rc = -1;
    }
  }

  if((p = conf_find_keyval(&rtmpAuthParse.authList.list[0], "challenge"))) {
    strncpy(pAuth->challenge_cli, p, sizeof(pAuth->challenge_cli) - 1);
  }
  response = conf_find_keyval(&rtmpAuthParse.authList.list[0], "response");
  opaque_cli = conf_find_keyval(&rtmpAuthParse.authList.list[0], "opaque");

  if(user != NULL && rc >= 0) {

    if(response && strlen(response) > 0) {

      if(!opaque_cli || opaque_cli[0] == '\0') {
        LOG(X_ERROR("RTMP connect client response is missing opaque"));
        reason = RTMP_AUTH_REASON_FAILED;
        rc = -1;
      } else if(pAuth->challenge_cli[0] == '\0') {
        LOG(X_ERROR("RTMP connect client response is missing challenge"));
        reason = RTMP_AUTH_REASON_FAILED;
        rc = -1;
      } else if((rc = rtmp_auth_cache_remove(pClient->pcachedAuth, opaque_cli, &pAuth->p)) < 0) {
        LOG(X_ERROR("RTMP unable to retrieve cached server challenge for opaque: '%s'"), opaque_cli);
        reason = RTMP_AUTH_REASON_FAILED;
      }

      if(rc >= 0) {
        if((rc = rtmp_auth_compute(pAuth, pClient->auth.pcreds)) >= 0 &&
           !strcmp(response, pAuth->response)) {
  
          *phaveConnect = 1;
          LOG(X_DEBUG("RTMP sending connect success response for app: '%s'"), pClient->ctxt.connect.app);
          return rtmp_create_result_invoke(&pClient->ctxt);

        } else {
          LOG(X_DEBUG("RTMP invalid client credentials authmod: '%s', user: '%s', resp: '%s', computed: '%s', "
                      "salt: '%s', challenge: '%s', opaque: '%s'"), rtmp_auth_getauthmodstr(pAuth->authmod), 
                      user, response, pAuth->response, pAuth->p.salt, pAuth->p.challenge, pAuth->p.opaque); 
          reason = RTMP_AUTH_REASON_FAILED;
          rc = -1;
        }
      }

    } else {

      rtmp_auth_create_challenge(&pAuth->p);
      rtmp_auth_cache_store(pClient->pcachedAuth, &pAuth->p);
      reason = RTMP_AUTH_REASON_NEEDAUTH;
    }

    if(rc < 0) {
      snprintf(challengestr, sizeof(challengestr), "?reason=%s", reason);
    } else {
      snprintf(challengestr, sizeof(challengestr), "?reason=%s&user=%s&salt=%s&challenge=%s&opaque=%s",
             reason, user, pAuth->p.salt, pAuth->p.challenge, pAuth->p.opaque); 
    }

  } else if(challengestr[0] == '\0') {
    snprintf(codestr, sizeof(codestr), " code=%d need auth;", 403);
  }
  
  if((rc = snprintf(description, sizeof(description), 
                "[ AccessManager.Reject ] : [%s authmod=%s ] : %s", 
                codestr, rtmp_auth_getauthmodstr(pAuth->authmod), challengestr)) > 0) {
    sz = rc;
  }

  LOG(X_DEBUG("RTMP sending connect '%s' for app: '%s'"), description, pClient->ctxt.connect.app);
  rtmp_create_error(&pClient->ctxt, 1, RTMP_NETCONNECT_REJECTED, description);

  //LOG(X_DEBUG("HAVE OCNNECT user: '%s', app: '%s', tcUrl: '%s'"), pClient->auth.pcreds ? pClient->auth.pcreds->username : "NOTSET", pClient->ctxt.connect.app, pClient->ctxt.connect.tcUrl);

  return rc;
}

static int check_srv_auth(RTMP_CTXT_CLIENT_T *pClient, const FLV_AMF_T *pAmfList) {
  int rc = 0;
  const FLV_AMF_T *pAmf;
  const char *code = NULL;
  const char *description = NULL;
  const char *p = NULL;
  const char *reason = NULL;
  RTMP_AUTH_PARSE_CTXT_T rtmpAuthParse;
  RTMP_AUTHMOD_T authmod = RTMP_AUTHMOD_UNKNOWN;
  RTMP_AUTH_STATE_T lastAuthState = pClient->auth.ctxt.lastAuthState;

  pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_UNKNOWN;
  pClient->auth.ctxt.consecretry++;

  if(!(pAmf = flv_amf_find(pAmfList, "_error"))) {
    return -1;
  } else if(!(code = flv_amf_get_key_string(flv_amf_find(pAmfList, "code")))) {
    return -1;
  } else if(strcasecmp(code, RTMP_NETCONNECT_REJECTED)) {
    return 0;
  } else if(!(description = flv_amf_get_key_string(flv_amf_find(pAmfList, "description")))) {
    return -1;
  }

  memset(&rtmpAuthParse, 0, sizeof(rtmpAuthParse));

  if(rtmp_auth_parse_resp(description, &rtmpAuthParse) < 0) {
    return -1;
  } else if(strcasecmp(rtmpAuthParse.errorStr, "AccessManager.Reject")) {
    LOG(X_ERROR("Unrecognized RTMP code: '%s' description: '%s' object"), code, description); 
    return -1;
  } else if((authmod = rtmp_auth_getauthmod(&rtmpAuthParse)) != RTMP_AUTHMOD_ADOBE &&
             authmod != RTMP_AUTHMOD_LLNW) {
    LOG(X_ERROR("Unsupported RTMP authmod in: '%s'"), description);
    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_FATAL;
    return -1;
  } else if(!pClient->auth.pcreds || pClient->auth.pcreds->username[0] == '\0' ||
                            pClient->auth.pcreds->pass[0] == '\0') {
    LOG(X_ERROR("RTMP Server requires authentication"));
    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_FATAL;
    return -1;
  } 
        
  if(authmod != RTMP_AUTHMOD_UNKNOWN) {
    pClient->auth.ctxt.authmod = authmod;
  }
  if((reason = conf_find_keyval(&rtmpAuthParse.authList.list[0], "reason"))) {

  }

  if((p = conf_find_keyval(&rtmpAuthParse.authList.list[0], "salt"))) {
    strncpy(pClient->auth.ctxt.p.salt, p, sizeof(pClient->auth.ctxt.p.salt) - 1);
  }
  if((p = conf_find_keyval(&rtmpAuthParse.authList.list[0], "challenge"))) {
    strncpy(pClient->auth.ctxt.p.challenge, p, sizeof(pClient->auth.ctxt.p.challenge) - 1);
  }
  if((p = conf_find_keyval(&rtmpAuthParse.authList.list[0], "opaque"))) {
    strncpy(pClient->auth.ctxt.p.opaque, p, sizeof(pClient->auth.ctxt.p.opaque) - 1);
  }

  if(lastAuthState == RTMP_AUTH_STATE_HAVE_CHALLENGE && reason &&
     !strcasecmp(reason, RTMP_AUTH_REASON_FAILED)) {
    //
    // We tried to auth and we failed with invalid credentials
    //
    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_FATAL;

  } else if(pClient->auth.ctxt.p.salt[0] != '\0' && 
     (pClient->auth.ctxt.p.challenge[0] != '\0' || pClient->auth.ctxt.p.opaque[0] != '\0')) {

    //
    // We want to try to auth given the server's salt and challenge
    //
    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_HAVE_CHALLENGE;

  } else if(lastAuthState == RTMP_AUTH_STATE_UNKNOWN) {
    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_NEED_CHALLENGE;
  } else {
    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_INVALID;
  }

  //LOG(X_DEBUG("Received RTMP error: '%s', authmod: '%s' %d"), rtmpAuthParse.errorStr, rtmpAuthParse.authmod, pClient->auth.ctxt.authmod);

  return rc;
}

static void rtmp_client_close(RTMP_CTXT_CLIENT_T *pRtmp) {

  rtmp_close(&pRtmp->ctxt);
  pRtmp->pQVid = NULL;
  pRtmp->pQAud = NULL;
  pRtmp->pcachedAuth = NULL;

}

static int rtmp_client_init(RTMP_CTXT_CLIENT_T *pRtmp, CAP_ASYNC_DESCR_T *pCfg, 
                            unsigned int maxPktLen, unsigned int growMaxPktLen) {
  int rc = 0;

  if((rc = rtmp_parse_init(&pRtmp->ctxt, maxPktLen, growMaxPktLen)) < 0) {
    return rc;
  }
  pRtmp->ctxt.isclient = 1;

  pRtmp->ctxt.av.vid.tmpFrame.sz = RTMP_TMPFRAME_VID_SZ;
  if((pRtmp->ctxt.av.vid.tmpFrame.buf = (unsigned char *) avc_calloc(1,
                                     pRtmp->ctxt.av.vid.tmpFrame.sz)) == NULL) {
    rtmp_client_close(pRtmp);
    return -1;
  }

  pRtmp->ctxt.av.aud.tmpFrame.sz = RTMP_TMPFRAME_AUD_SZ;
  if((pRtmp->ctxt.av.aud.tmpFrame.buf = (unsigned char *) avc_calloc(1,
                                    pRtmp->ctxt.av.aud.tmpFrame.sz)) == NULL) {
    rtmp_client_close(pRtmp);
    return -1;
  }

  pRtmp->ctxt.pSd = &pRtmp->sd;
  pRtmp->ctxt.cbRead = rtmp_cbReadDataNet;
  pRtmp->ctxt.pCbData = pRtmp;
  pRtmp->ctxt.state = RTMP_STATE_INVALID;

  pRtmp->ctxt.connect.capabilities = 15.0f;
  pRtmp->ctxt.connect.objEncoding = 3.0f;

  if(pCfg && pCfg->pcommon->filt.filters[0].pCapAction) {
    pRtmp->pQVid = pCfg->pcommon->filt.filters[0].pCapAction->pQueue;
  }
  if(pCfg && pCfg->pcommon->filt.filters[1].pCapAction) {
    pRtmp->pQAud = pCfg->pcommon->filt.filters[1].pCapAction->pQueue;
  }

  return rc;
}

static int capture_rtmpUriParts(char *puri, char **ppuri, char **ppuriDocName) {
  int rc = 0;
  char *p;

  if(!puri) {
    return -1;
  }
  if(ppuriDocName) {
    *ppuriDocName = NULL;
  }

  if((p = strrchr(puri, '/'))) {
    *p++ = '\0';
    if(ppuriDocName) {
      *ppuriDocName = p;
    }
    if(ppuri) {
      *ppuri = ++puri;
    }
  }

  return rc;
}

static int onresp_connect(RTMP_CTXT_CLIENT_T *pClient, const FLV_AMF_T *pAmfArr) { 
  int rc = CONNECT_RETRY_RC_OK;
  const FLV_AMF_T *pAmf = NULL;
  RTMP_CTXT_T *pRtmp = &pClient->ctxt;

  if(rtmp_is_resp_error(pAmfArr, "Connect", 0, S_DEBUG)) {

    //
    // Perform any authentication
    // setting value of pClient->auth.ctxt.lastAuthState 
    //
    check_srv_auth(pClient, pAmfArr);

    if(pClient->auth.ctxt.lastAuthState != RTMP_AUTH_STATE_NEED_CHALLENGE &&
       pClient->auth.ctxt.lastAuthState != RTMP_AUTH_STATE_HAVE_CHALLENGE) {
      rtmp_is_resp_error(pAmfArr, "Connect", 0, S_ERROR);
    }

    pRtmp->state = RTMP_STATE_ERROR;
    rc = CONNECT_RETRY_RC_ERROR;

    
  } else if((pAmf = flv_amf_find(pAmfArr, "_result"))) {

    pRtmp->state = RTMP_STATE_CLI_CONNECT;

    LOG(X_DEBUG("RTMP Connect succesful: '%s' description: '%s'"),
      flv_amf_get_key_string(flv_amf_find(pAmfArr, "code")), 
      flv_amf_get_key_string(flv_amf_find(pAmfArr, "description")));

    pClient->auth.ctxt.lastAuthState = RTMP_AUTH_STATE_UNKNOWN;
    pClient->auth.ctxt.consecretry = 0;
  }

  return rc;
}

static int onresp_createStream(RTMP_CTXT_CLIENT_T *pClient, const FLV_AMF_T *pAmfArr) { 
  int rc = CONNECT_RETRY_RC_OK;
  const FLV_AMF_T *pAmf = NULL;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - onresp_createStream...")) );

  if((pAmf = flv_amf_find(pAmfArr, "_result"))) {

    pClient->ctxt.state = RTMP_STATE_CLI_CREATESTREAM;
    LOG(X_DEBUG("RTMP createStream ok"));

  } else {
    pClient->ctxt.state = RTMP_STATE_ERROR;
    rc = CONNECT_RETRY_RC_ERROR;
    LOG(X_ERROR("RTMP Invalid createStream response"));
    rtmp_is_resp_error(pAmfArr, "createStream", 0, S_ERROR);

  }

  return rc;
}

static int onresp_releaseStream(RTMP_CTXT_CLIENT_T *pClient, const FLV_AMF_T *pAmfArr) { 
  int rc = CONNECT_RETRY_RC_OK;
  const FLV_AMF_T *pAmf = NULL;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - onresp_releaseStream...")) );

  if((pAmf = flv_amf_find(pAmfArr, "_result"))) {

    pClient->ctxt.state = RTMP_STATE_CLI_RELEASESTREAM;
    LOG(X_DEBUG("RTMP releaseStream ok"));

  } else {
    pClient->ctxt.state = RTMP_STATE_ERROR;
    rc = CONNECT_RETRY_RC_ERROR;
    LOG(X_ERROR("RTMP Invalid releaseStream response"));
    rtmp_is_resp_error(pAmfArr, "releaseStream", 0, S_ERROR);

  }

  return rc;
}

static int onresp_fcpublish(RTMP_CTXT_CLIENT_T *pClient, const FLV_AMF_T *pAmfArr) { 
  int rc = CONNECT_RETRY_RC_OK;
  const char *p;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - onresp_fcpublish...")) );

  if((p = flv_amf_get_key_string(flv_amf_find(pAmfArr, "code"))) &&
          (!strncasecmp(p, "NetStream.Publish.Start", strlen("NetStream.Publish.Start")))) {
    pClient->ctxt.state = RTMP_STATE_CLI_FCPUBLISH;
    LOG(X_DEBUG("RTMP fcpublish ok"));
  } else {
    pClient->ctxt.state = RTMP_STATE_ERROR;
    rc = CONNECT_RETRY_RC_ERROR;
    LOG(X_ERROR("RTMP Invalid fcpublish response"));
    rtmp_is_resp_error(pAmfArr, "fcpublish", 0, S_ERROR);
  }

  return rc;
}

static int onresp_onstatus(RTMP_CTXT_CLIENT_T *pClient, const FLV_AMF_T *pAmfArr, RTMP_METHOD_T reqMethod) { 
  int rc = CONNECT_RETRY_RC_ERROR;
  const char *code;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - onresp_onstatus...")) );

  code = flv_amf_get_key_string(flv_amf_find(pAmfArr, "code"));

  if(reqMethod == RTMP_METHOD_PLAY) {
    if(code && (!strncasecmp(code, RTMP_NETSTREAM_DATA_START, strlen(RTMP_NETSTREAM_DATA_START)) || 
              !strncasecmp(code, RTMP_NETSTREAM_PLAY_START, strlen(RTMP_NETSTREAM_PLAY_START)) ||
              !strncasecmp(code, RTMP_NETSTREAM_PLAY_RESET, strlen(RTMP_NETSTREAM_PLAY_RESET)))) {

      rc = CONNECT_RETRY_RC_OK;

      if(!strncasecmp(code, RTMP_NETSTREAM_DATA_START, strlen(RTMP_NETSTREAM_DATA_START))) {
        pClient->ctxt.state = RTMP_STATE_CLI_PLAY;
        LOG(X_INFO("RTMP play %s ok.  Starting playback"), code);
      } else {
        LOG(X_DEBUG("RTMP play %s ok"), code);
      }

    } 
/*
else if((pAmf = flv_amf_find(pAmfArr, "onFCSubscribe"))) {

        if((p = flv_amf_get_key_string(flv_amf_find(&amfEntries[0], "code"))) &&
          !strncasecmp(p, RTMP_NETSTREAM_PLAY_START, strlen(RTMP_NETSTREAM_PLAY_START))) {
          rtmp.ctxt.state = RTMP_STATE_CLI_PLAY;
        }
      }
*/

  } else if(reqMethod == RTMP_METHOD_PUBLISH) {

    if(code && (!strncasecmp(code, RTMP_NETSTREAM_PUBLISH_START, strlen(RTMP_NETSTREAM_PUBLISH_START)))) {

      pClient->ctxt.state = RTMP_STATE_CLI_PUBLISH;
      LOG(X_DEBUG("RTMP publish %s ok"), code);
      rc = CONNECT_RETRY_RC_OK;
    }
  }

  if(rc != CONNECT_RETRY_RC_OK) {
    pClient->ctxt.state = RTMP_STATE_ERROR;
    rtmp_is_resp_error(pAmfArr, (reqMethod == RTMP_METHOD_PLAY ? "play" : "publish"), 1, S_ERROR);
  }

  return rc;
}

static CONNECT_RETRY_RC_T run_capture_client(CAP_ASYNC_DESCR_T *pCfg) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  int contentType = 0;
  RTMP_CTXT_CLIENT_T rtmp;
  unsigned char rtmptbufin[2048];
  unsigned char rtmptbufout[4096];
  SOCKET_DESCR_T sockDescr;
  unsigned int idx;
  const char *p = NULL;
  struct timeval tv0, tv;
  char hostname[RTSP_URL_LEN];
  char uribuf[CAPTURE_HTTP_HOSTBUF_MAXLEN];
  FLV_AMF_T amfEntries[20];
  char tmp[128];

  memset(&sockDescr, 0, sizeof(sockDescr));
  NETIO_SET(sockDescr.netsocket, pCfg->pSockList->netsockets[0]);
  memcpy(&sockDescr.sa, &pCfg->pSockList->salist[0], sizeof(sockDescr.sa));

  memset(&rtmp, 0, sizeof(rtmp));

  if(rtmp_client_init(&rtmp, pCfg, RTMP_TMPFRAME_VID_SZ, 0x1000) < 0) {
    return CONNECT_RETRY_RC_NORETRY;
  }

  //
  // Remove any URI parts from the URL and only leave the hostname 
  //
  strncpy(hostname, pCfg->pcommon->localAddrs[0], sizeof(hostname));
  if((p = strchr(hostname, '/'))) {
    *((char *) p) = '\0'; 
  }
  rtmp.purl = hostname;
  strncpy(uribuf, pCfg->pcommon->addrsExt[0], sizeof(uribuf) - 1);
  rtmp.puri = uribuf;
  rtmp.puridocname = "";
  rtmp.ctxt.rtmpt.phosthdr = pCfg->pcommon->addrsExtHost[0];

  capture_rtmpUriParts(uribuf, NULL, (char **) &rtmp.puridocname);

  memcpy(&rtmp.client.cfg, &pCfg->pcommon->rtmpcfg, sizeof(rtmp.client.cfg));
  rtmp.ctxt.av.aud.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.ctxt.av.vid.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.ctxt.pSd = &sockDescr;

  //
  // Setup any explicit RTMP tunnel
  //
  if(pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTMPT ||
     pCfg->pcommon->filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_RTMPTS) {
    if(rtmpt_setupclient(&rtmp, rtmptbufin, sizeof(rtmptbufin), rtmptbufout, sizeof(rtmptbufout)) < 0) {
      LOG(X_ERROR("Failed to setup RTMPT tunnel session"));
      return -1;
    }
  }

  if(rtmp_handshake_cli(&rtmp.ctxt, rtmp.client.cfg.rtmpfp9) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
        FORMAT_NETADDR(rtmp.ctxt.pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(rtmp.ctxt.pSd->sa)));
    rc = CONNECT_RETRY_RC_ERROR;
  } 

  gettimeofday(&tv0, NULL);

  rtmp.ctxt.chunkSzOut = 512;//RTMP_CHUNK_SZ_OUT;

  //
  // Send RTMP connect
  //
  if(rc >= CONNECT_RETRY_RC_OK && rtmp_connect(&rtmp, RTMP_FLASH_VER_CAPTURE_CLIENT) < 0)  {
    rc = CONNECT_RETRY_RC_ERROR;
  } 

  while(rc >= CONNECT_RETRY_RC_OK  && !g_proc_exit) {

    memset(amfEntries, 0, sizeof(amfEntries));
    for(idx = 0; idx < (sizeof(amfEntries) / sizeof(amfEntries[0])) - 1; idx++) {
      amfEntries[idx].pnext = &amfEntries[idx + 1];
    }

    //rtmp.contentTypeLastInvoke = 0;
    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - b4run_capture_client ct: 0x%x, methodParsed: %d, state: %d"),
                                     contentType, rtmp.ctxt.methodParsed, rtmp.ctxt.state) );

    if((contentType = parse_readpkt(&rtmp.ctxt, &amfEntries[0])) < 0) {
      LOG(X_ERROR("Failed to read rtmp packet from server"));
      rc  = CONNECT_RETRY_RC_ERROR;
      break;
    } 

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - run_capture_client ct: 0x%x, methodParsed: %d, state: %d"),
                                     contentType, rtmp.ctxt.methodParsed, rtmp.ctxt.state) );

    if(rtmp.ctxt.state == RTMP_STATE_CLI_HANDSHAKEDONE && 
         (rtmp.ctxt.methodParsed == RTMP_METHOD_RESULT || rtmp.ctxt.methodParsed == RTMP_METHOD_ERROR)) {

      if((rc = onresp_connect(&rtmp, &amfEntries[0])) >= CONNECT_RETRY_RC_OK) {
         rtmp.ctxt.rtmpt.doQueueOut = 1;
         rc = rtmp_serverBw(&rtmp.ctxt);
         if((rc = rtmp_createStream(&rtmp.ctxt, RTMP_CONTENT_TYPE_MSG, 1)) < CONNECT_RETRY_RC_OK) {
         }
      } 
     
    } else if(rtmp.ctxt.state == RTMP_STATE_CLI_CONNECT && 
         (rtmp.ctxt.methodParsed == RTMP_METHOD_RESULT || rtmp.ctxt.methodParsed == RTMP_METHOD_ERROR)) {

      if((rc = onresp_createStream(&rtmp, &amfEntries[0])) >= CONNECT_RETRY_RC_OK) {
        if((rc = rtmp_play(&rtmp)) < CONNECT_RETRY_RC_OK) {
        }
      }

    } else if(rtmp.ctxt.state == RTMP_STATE_CLI_CREATESTREAM && 
         (rtmp.ctxt.methodParsed == RTMP_METHOD_RESULT || rtmp.ctxt.methodParsed == RTMP_METHOD_ERROR ||
         (rtmp.ctxt.methodParsed == RTMP_METHOD_ONSTATUS))) {

        if((rc = onresp_onstatus(&rtmp, &amfEntries[0], RTMP_METHOD_PLAY)) >= CONNECT_RETRY_RC_OK) {
           //gettimeofday(&tv0, NULL);
        }
/*
      else if((pAmf = flv_amf_find(&amfEntries[0], "onFCSubscribe"))) {
        if((p = flv_amf_get_key_string(flv_amf_find(&amfEntries[0], "code"))) &&
          !strncasecmp(p, RTMP_NETSTREAM_PLAY_START, strlen(RTMP_NETSTREAM_PLAY_START))) {
          rtmp.ctxt.state = RTMP_STATE_CLI_PLAY;
        }
      }

      if(rtmp.ctxt.state == RTMP_STATE_CLI_PLAY) {

          gettimeofday(&tv0, NULL);

        //if(!strncasecmp(p, "NetStream.Data.Start", strlen("NetStream.Data.Start")) ||
        //   !strncasecmp(p, "NetStream.Play.Start", strlen("NetStream.Play.Start"))) {
          LOG(X_INFO("RTMP play succesful: '%s' description: '%s'"),
            flv_amf_get_key_string(flv_amf_find(&amfEntries[0], "code")),
            flv_amf_get_key_string(flv_amf_find(&amfEntries[0], "description")));

        //} else {

          //LOG(X_DEBUG("RTMP play) succesful: '%s' description: '%s'"),
          //  flv_amf_get_key_string(flv_amf_find(&amfEntries[0], "code")),
          //  flv_amf_get_key_string(flv_amf_find(&amfEntries[0], "description")));
        //}
      }
*/

    } else if(rtmp.ctxt.state == RTMP_STATE_CLI_PLAY) {

      if(contentType == RTMP_CONTENT_TYPE_MSG ||
         contentType == RTMP_CONTENT_TYPE_NOTIFY ||
         contentType == RTMP_CONTENT_TYPE_INVOKE) {

         if(flv_amf_find(&amfEntries[0], "onMetaData")) {
           LOG(X_DEBUG("RTMP onMetaData"));
         }

      } else if(contentType == RTMP_CONTENT_TYPE_VIDDATA) {
        //fprintf(stderr, "IN BUF SZ:%d in max: %d tmpFrame max:%d (%d)\n", rtmp.ctxt.pkt[rtmp.ctxt.streamIdx].hdr.szPkt, rtmp.ctxt.in.sz, rtmp.ctxt.av.vid.tmpFrame.sz, RTMP_TMPFRAME_VID_SZ);
        rtmp_handle_vidpkt(&rtmp, rtmp.ctxt.in.buf, 
                      rtmp.ctxt.pkt[rtmp.ctxt.streamIdx].hdr.szPkt,
                      rtmp.ctxt.pkt[rtmp.ctxt.streamIdx].tsAbsolute);
      } else if(contentType == RTMP_CONTENT_TYPE_AUDDATA) {
        rtmp_handle_audpkt(&rtmp, rtmp.ctxt.in.buf,  
                      rtmp.ctxt.pkt[rtmp.ctxt.streamIdx].hdr.szPkt,
                      rtmp.ctxt.pkt[rtmp.ctxt.streamIdx].tsAbsolute);
      } else if(contentType == RTMP_CONTENT_TYPE_FLV) {
        handle_flv(&rtmp);
      }

      gettimeofday(&tv, NULL);
      if(tv.tv_sec - tv0.tv_sec > 8) {

        //
        // FMS will silently stop sending data if it does not receive this after > 60 sec
        //
        LOG(X_DEBUG("RTMP sending bytes read report %u to %s:%d"), rtmp.ctxt.bytesRead,
          FORMAT_NETADDR(rtmp.ctxt.pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(rtmp.ctxt.pSd->sa)));

        rtmp_create_bytes_read(&rtmp.ctxt);
        //avc_dumpHex(stderr, rtmp.ctxt.out.buf, rtmp.ctxt.out.idx, 1);

        if(rtmp_send(&rtmp.ctxt, "bytes read report") < 0) {
          rc = CONNECT_RETRY_RC_ERROR;
          break;
        }
        rtmp.ctxt.out.idx = 0; 

        tv0.tv_sec = tv.tv_sec;
      }

    }

    if(rc < CONNECT_RETRY_RC_OK) {

    }

    //fprintf(stderr, "rtmp_parse_readpkt_full rc:%d state:%d\n", rc, rtmp.ctxt.state);

  }

  rtmp_client_close(&rtmp);

  return rc;
}

CONNECT_RETRY_RC_T capture_rtmp_cbonconnect(void *pArg) {
  return run_capture_client((CAP_ASYNC_DESCR_T *) pArg);
}

int capture_rtmp_client(CAP_ASYNC_DESCR_T *pCfg) {
  int rc = 0;
  CONNECT_RETRY_CTXT_T retryCtxt;
  
  if(pCfg->pcommon->filt.numFilters <= 0) {
    return -1;
  }

  NETIOSOCK_FD(pCfg->pSockList->netsockets[0]) = INVALID_SOCKET;

  //
  // Initialize the retry context for connecting to the remote endpoint
  //
  memset(&retryCtxt, 0, sizeof(retryCtxt));
  retryCtxt.cbConnectedAction = capture_rtmp_cbonconnect;
  retryCtxt.pConnectedActionArg = pCfg;
  retryCtxt.pAuthCliCtxt = NULL;
  retryCtxt.pnetsock = &pCfg->pSockList->netsockets[0];
  retryCtxt.psa = (const struct sockaddr *) &pCfg->pSockList->salist[0];
  retryCtxt.connectDescr = "RTMP";
  retryCtxt.pconnectretrycntminone = &pCfg->pcommon->connectretrycntminone;
  retryCtxt.tmtms = 0;

  //
  // Connect to the remote and call capture_rtmp_cbonconnect
  // This will automatically invoke any retry logic according to the retry config
  //
  rc = connect_with_retry(&retryCtxt);

  netio_closesocket(&pCfg->pSockList->netsockets[0]);

  return rc;
}

static int cap_rtmp_handle_conn(RTMP_CTXT_CLIENT_T *pRtmp) {
  int rc = 0;
  unsigned int idx;
  int contentType;
  char tmp[128];
  FLV_AMF_T amfEntries[20];
  int haveConnect = 0;
  int haveCreateStream = 0;
  int havePublish = 0;
  unsigned char buf[4096];

  pRtmp->ctxt.rtmpt.pbuftmp = buf;
  pRtmp->ctxt.rtmpt.szbuftmp = sizeof(buf);
  pRtmp->ctxt.state = RTMP_STATE_CLI_START;

  //
  // Accept handshake
  //
  if((rc = rtmp_handshake_srv(&pRtmp->ctxt)) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
      FORMAT_NETADDR(pRtmp->ctxt.pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pRtmp->ctxt.pSd->sa)));
    return rc;
  }

  pRtmp->ctxt.chunkSzOut = 512;//RTMP_CHUNK_SZ_OUT;0

  while(rc >= 0 && !g_proc_exit) {

    memset(amfEntries, 0, sizeof(amfEntries));
    for(idx = 0; idx < (sizeof(amfEntries) / sizeof(amfEntries[0])) - 1; idx++) {
      amfEntries[idx].pnext = &amfEntries[idx + 1];
    }

    if((rc = contentType = parse_readpkt(&pRtmp->ctxt, &amfEntries[0])) < 0) {
      LOG(X_ERROR("Failed to read rtmp (capture) packet from client"));
      break;
    }

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - cap_rtmp_handle_conn ct: %0x, methodParsed: %d, state: %d"),
           pRtmp->ctxt.contentTypeLastInvoke, pRtmp->ctxt.methodParsed, pRtmp->ctxt.state) );

    if(!haveConnect && pRtmp->ctxt.state == RTMP_STATE_CLI_CONNECT) {

      //
      // Create server connect response
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_serverbw(&pRtmp->ctxt, 2500000);
      rtmp_create_clientbw(&pRtmp->ctxt, 2500000);
      rtmp_create_ping(&pRtmp->ctxt, 0, 0);
      rtmp_create_chunksz(&pRtmp->ctxt);
      
      if((rc = check_cli_auth(pRtmp, &haveConnect)) < 0) {
        return rc;
      }

      if((rc = rtmp_send(&pRtmp->ctxt, "cap_rtmp_handle_conn connect response")) < 0) {
        return rc;
      }

      if(!haveConnect) {
        //
        // Break the connection since the client may not send another connect attempt for auth fail
        //
        break;

        //
        // We did not have succesful authentication, so we expect another connect request with credentials
        //
        pRtmp->ctxt.state = RTMP_STATE_CLI_HANDSHAKEDONE;
      }

    } 

    if(!haveConnect) {

      if(pRtmp->ctxt.methodParsed != RTMP_METHOD_CHUNKSZ && pRtmp->ctxt.methodParsed != RTMP_METHOD_CONNECT) {
        LOG(X_WARNING("RTMP contentType: %0x, method: %d, state: %d.  Expecting connect from client."),
           pRtmp->ctxt.contentTypeLastInvoke, pRtmp->ctxt.methodParsed, pRtmp->ctxt.state);
      }
  
    } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_FCUNPUBLISH ||
       pRtmp->ctxt.methodParsed == RTMP_METHOD_CLOSESTREAM) {
       // pRtmp->ctxt.state = RTMP_STATE_CLI_DELETESTREAM
    } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_DELETESTREAM) {

      LOG(X_INFO("RTMP stream capture closed."));

      //
      // Create deletestream response 
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_close(&pRtmp->ctxt);

      if((rc = rtmp_send(&pRtmp->ctxt, "deletestream close response")) < 0) {
        return rc;
      }

      //
      // Reset capture state
      //
      //pRtmp->ctxt.state = RTMP_STATE_CLI_HANDSHAKEDONE;
      pRtmp->ctxt.state = RTMP_STATE_CLI_CONNECT;
      haveCreateStream = 0;
      havePublish = 0;

    } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_RELEASESTREAM) {

/*
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_result(&pRtmp->ctxt);

      if((rc = rtmp_send(&pRtmp->ctxt, "cap_rtmp_handle_conn releaseStream response")) < 0) {
        return rc;
      }
*/

    } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_FCPUBLISH) {

/*
      //
      // Create FCPublish response
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_onfcpublish(&pRtmp->ctxt);

      if((rc = rtmp_send(&pRtmp->ctxt, "cap_rtmp_handle_conn FCPublish response")) < 0) {
        break;
      }
*/

    } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_CREATESTREAM && !haveCreateStream) {

      //
      // Create createstream response 
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_result(&pRtmp->ctxt);

      if((rc = rtmp_send(&pRtmp->ctxt, "cap_rtmp_handle_conn createStream response")) < 0) {
        return rc;
      }
      haveCreateStream = 1;

      } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_PUBLISH && !havePublish) {

      //
      // Create publish response 
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_ping(&pRtmp->ctxt, 0, 0x01);
      rtmp_create_onstatus(&pRtmp->ctxt, RTMP_ONSTATUS_TYPE_PUBLISH);

      if((rc = rtmp_send(&pRtmp->ctxt, "cap_rtmp_handle_conn publish response")) < 0) {
        return rc;
      }

      havePublish = 1;
      pRtmp->ctxt.state = RTMP_STATE_CLI_PLAY;

    } else if(pRtmp->ctxt.methodParsed == RTMP_METHOD_PLAY) {
      //
      // Check for invalid methods while the server is in capture mode
      //
      LOG(X_WARNING("Invalid client play method received while in server capture mode"));

      pRtmp->ctxt.out.idx = 0;
      rtmp_create_error(&pRtmp->ctxt, 0, NULL, "play not available when in capture mode");
      if((rc = rtmp_send(&pRtmp->ctxt, "cap_rtmp_handle_conn play error response")) < 0) {
        return rc;
      }

      rc = -1;

    } else if(pRtmp->ctxt.state == RTMP_STATE_CLI_PLAY) {

      if(contentType == RTMP_CONTENT_TYPE_VIDDATA) {
        rtmp_handle_vidpkt(pRtmp, pRtmp->ctxt.in.buf,
                      pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt,
                      pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].tsAbsolute);
      } else if(contentType == RTMP_CONTENT_TYPE_AUDDATA) {
        rtmp_handle_audpkt(pRtmp, pRtmp->ctxt.in.buf,
                      pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt,
                      pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].tsAbsolute);
      } else if(contentType == RTMP_CONTENT_TYPE_FLV) {
        handle_flv(pRtmp);
      } else if(contentType != RTMP_CONTENT_TYPE_CHUNKSZ) {
        LOG(X_DEBUG("RTMP possibly unhandled play state contentType: 0x%x"), contentType);
      }

    } else {
      if(pRtmp->ctxt.out.idx == 0) {
        VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - cap_rtmp_handle_conn no response.")););
      }
    }

  }

  LOG(X_DEBUG("rtmp (capture) connection ending with state: %d"), pRtmp->ctxt.state);
  pRtmp->ctxt.state = RTMP_STATE_DISCONNECTED;

  return rc;
}

typedef struct CAP_RTMP_SRV_PROC_USERDATA {
  CAP_ASYNC_DESCR_T             *pCfg;
  RTMP_AUTH_PERSIST_STORAGE_T   cachedAuth;
} CAP_RTMP_SRV_PROC_USERDATA_T;

static void cap_rtmp_srv_proc(void *pfuncarg) {
  CLIENT_CONN_T *pConn = (CLIENT_CONN_T *) pfuncarg;
  CAP_RTMP_SRV_PROC_USERDATA_T *pUserData = (CAP_RTMP_SRV_PROC_USERDATA_T *) pConn->pUserData;
  CAP_ASYNC_DESCR_T *pCfg = pUserData->pCfg;
  RTMP_CTXT_CLIENT_T rtmp;
  int rc;
  char tmp[128];

  LOG(X_INFO("Starting RTMP capture connection from %s:%d"), 
         FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

  memset(&rtmp, 0, sizeof(rtmp));
  NETIO_SET(rtmp.sd.netsocket, pConn->sd.netsocket);
  memcpy(&rtmp.sd.sa, &pCfg->pSockList->salist[0], sizeof(rtmp.sd.sa));

  if((rc = rtmp_client_init(&rtmp, pCfg, RTMP_TMPFRAME_VID_SZ, 0x1000)) < 0) {
    return;
  }
  rtmp.ctxt.pSd = &rtmp.sd;
  rtmp.ctxt.connect.capabilities = 31.0f;
  rtmp.ctxt.av.aud.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.ctxt.av.vid.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.auth.pcreds = &pCfg->pcommon->creds[0];
  rtmp.pcachedAuth = &pUserData->cachedAuth;

  cap_rtmp_handle_conn(&rtmp);

  rtmp_client_close(&rtmp);
  netio_closesocket(&pConn->sd.netsocket);

  LOG(X_DEBUG("RTMP capture connection ended %s:%d"), FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)),
                                                      ntohs(INET_PORT(pConn->sd.sa)));
}

int capture_rtmp_server(CAP_ASYNC_DESCR_T *pCfg) {
  int rc = 0;
  POOL_T pool;
  char tmp[128];
  SRV_LISTENER_CFG_T listenCfg;
  CLIENT_CONN_T *pClient;
  CAP_RTMP_SRV_PROC_USERDATA_T userData;
  
  if(pCfg->pcommon->filt.numFilters <= 0) {
    return -1;
  }

  memset(&userData, 0, sizeof(userData));
  userData.pCfg = pCfg; 

  memset(&pool, 0, sizeof(pool));
  if(pool_open(&pool, 2, sizeof(CLIENT_CONN_T), 1) != 0) {
    return -1;
  }

  //
  // Replace default capability list
  //
  pClient = (CLIENT_CONN_T *) pool.pElements;
  while(pClient) {
    //pClient->purlCap = &urlCapabilities;
    pClient->pUserData = &userData;
    pClient = (CLIENT_CONN_T *) pClient->pool.pNext;
  }

  if((NETIOSOCK_FD(pCfg->pSockList->netsockets[0]) = 
      net_listen((const struct sockaddr *) &pCfg->pSockList->salist[0], 2)) == INVALID_SOCKET) {
    pool_close(&pool, 0);
    return -1;
  }

  if(rtmp_auth_cache_init(&userData.cachedAuth) < 0) {
    pool_close(&pool, 0);
    return -1;
  }

  memset(&listenCfg, 0, sizeof(listenCfg));
  listenCfg.pnetsockSrv = &pCfg->pSockList->netsockets[0];
  listenCfg.netflags = pCfg->pSockList->netsockets[0].flags;
  listenCfg.pConnPool = &pool;
  listenCfg.urlCapabilities = URL_CAP_RTMPLIVE;
  memcpy(&listenCfg.sa, &pCfg->pSockList->salist[0], sizeof(listenCfg.sa));

  //
  // Enable SSL/TLS
  //
  if(pCfg->pStreamerCfg && pCfg->pStreamerCfg->pStorageBuf && 
     ((STREAM_STORAGE_T *) pCfg->pStreamerCfg->pStorageBuf)->pParams) {
    rc = vsxlib_ssl_initserver(((STREAM_STORAGE_T *) pCfg->pStreamerCfg->pStorageBuf)->pParams, &listenCfg);
  }

  LOG(X_INFO("RTMP capture listener available on "URL_RTMP_FMT_STR"%s"),
         URL_HTTP_FMT_ARGS2(&listenCfg, FORMAT_NETADDR(listenCfg.sa, tmp, sizeof(tmp))),
         IS_AUTH_CREDENTIALS_SET(&pCfg->pcommon->creds[0]) ? " (Using auth)" : "");

  //
  // Service any client connections on the live listening port
  //
  rc = srvlisten_loop(&listenCfg, cap_rtmp_srv_proc);

  rtmp_auth_cache_close(&userData.cachedAuth);
  netio_closesocket(&pCfg->pSockList->netsockets[0]);
  pool_close(&pool, 1);

  return rc;
}

int stream_rtmp_close(STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;

  if(!pStreamerCfg) {
    return -1;
  }

  VSX_DEBUG_RTMP( 
    LOG(X_DEBUG("RTMP - stream_rtmp_close close 0x%x 0x%x"), pStreamerCfg, pStreamerCfg->rtmppublish.pSd) );

  pthread_mutex_lock(&pStreamerCfg->rtmppublish.mtx);
  if(pStreamerCfg->rtmppublish.pSd) {
    netio_closesocket(&pStreamerCfg->rtmppublish.pSd->netsocket);
    pStreamerCfg->rtmppublish.pSd = NULL;
  }
  pthread_mutex_unlock(&pStreamerCfg->rtmppublish.mtx);

  return rc;
}

static CONNECT_RETRY_RC_T rtmp_run_publishclient(RTMP_REQ_CTXT_T *pReqCtxt) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;
  int rc = 0;
  int contentType;
  unsigned int idx;
  RTMP_CTXT_CLIENT_T *pClient = NULL;
  RTMP_CTXT_T *pRtmp = NULL;
  FLV_AMF_T amfEntries[20];
  char tmp[128];
  unsigned int rcvTmtMs = 0;

  if(!pReqCtxt || !(pClient = pReqCtxt->pClient) || !(pRtmp = &pClient->ctxt) || !pReqCtxt->pCfg || !pRtmp->pSd) {
    return CONNECT_RETRY_RC_NORETRY;
  }

  rtmp_parse_reset(pRtmp);

  if(rtmp_handshake_cli(pRtmp, pReqCtxt->pCfg->rtmpfp9) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
        FORMAT_NETADDR(pRtmp->pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pRtmp->pSd->sa)));
    rc = CONNECT_RETRY_RC_ERROR;
  }

  //TODO: broken w/ chunk sz 128 cuz of improper chunked sending before getting to media data
  pClient->ctxt.chunkSzOut = RTMP_CHUNK_SZ_OUT;
  rcvTmtMs = pRtmp->rcvTmtMs;

  //
  // Send RTMP connect
  //
  if(rc >= CONNECT_RETRY_RC_OK && rtmp_connect(pClient, RTMP_FLASH_VER_PUBLISH) < 0)  {
    rc = CONNECT_RETRY_RC_ERROR;
  }

  while(rc >= CONNECT_RETRY_RC_OK &&  !g_proc_exit) {

    //LOG(X_DEBUG("RTMP - start of loop... PKT CT:%d LASTCT: 0x%x state:%d"), contentType, pRtmp->contentTypeLastInvoke, pRtmp->state);

    memset(amfEntries, 0, sizeof(amfEntries));
    for(idx = 0; idx < (sizeof(amfEntries) / sizeof(amfEntries[0])) - 1; idx++) {
      amfEntries[idx].pnext = &amfEntries[idx + 1];
    }

    //
    // Disable the socket reception timeout when beginning to send actual media data to the remote
    //
    if(pRtmp->state >= RTMP_STATE_CLI_PUBLISH && pRtmp->rcvTmtMs != 0) {
      LOG(X_DEBUG("Setting RTMP socket timeout to 0"));
      pRtmp->rcvTmtMs = 0;
    }

    if((contentType = parse_readpkt(pRtmp, &amfEntries[0])) < 0) {
      LOG(X_ERROR("Failed to read rtmp packet from server"));
      rc  = CONNECT_RETRY_RC_ERROR;
      break;
    }

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_run_publishclient ct: 0x%x, methodParsed: %d, state: %d"),
                                     contentType, pRtmp->methodParsed, pRtmp->state) );

    if(pRtmp->state == RTMP_STATE_CLI_HANDSHAKEDONE && 
       (pRtmp->methodParsed == RTMP_METHOD_RESULT || pRtmp->methodParsed == RTMP_METHOD_ERROR)) {

      if((rc = onresp_connect(pClient, &amfEntries[0])) >= CONNECT_RETRY_RC_OK) {
        rc = rtmp_serverBw(pRtmp);
        rc = rtmp_releaseStream(pRtmp, &pClient->client);
        rc = rtmp_fcpublish(pRtmp, &pClient->client);
        rc = rtmp_createStream(pRtmp, RTMP_CONTENT_TYPE_INVOKE, 0);
        pRtmp->state = RTMP_STATE_CLI_FCPUBLISH;
      }

    } else if(pRtmp->state == RTMP_STATE_CLI_CONNECT && pRtmp->methodParsed == RTMP_METHOD_RESULT) {

      if((rc = onresp_releaseStream(pClient, &amfEntries[0])) >= CONNECT_RETRY_RC_OK) {
        if((rc = rtmp_fcpublish(pRtmp, &pClient->client)) < CONNECT_RETRY_RC_OK) {
        }
      }

    } else if(pRtmp->state == RTMP_STATE_CLI_RELEASESTREAM && 
             (pRtmp->methodParsed == RTMP_METHOD_RESULT || pRtmp->methodParsed == RTMP_METHOD_ONFCPUBLISH)) {

      if((rc = onresp_fcpublish(pClient, &amfEntries[0])) >= CONNECT_RETRY_RC_OK) {
        if((rc = rtmp_createStream(pRtmp, RTMP_CONTENT_TYPE_INVOKE, 0)) < CONNECT_RETRY_RC_OK) {
        }
      }
    
    } else if(pRtmp->state == RTMP_STATE_CLI_FCPUBLISH && 
               (pRtmp->methodParsed == RTMP_METHOD_RESULT)) {

      if((rc = onresp_createStream(pClient, &amfEntries[0])) >= CONNECT_RETRY_RC_OK) {
        if((rc = rtmp_publish(pRtmp, &pClient->client)) < CONNECT_RETRY_RC_OK) {
        }
      }

    } else if(pRtmp->state == RTMP_STATE_CLI_CREATESTREAM && 
               (pRtmp->methodParsed == RTMP_METHOD_RESULT || pRtmp->methodParsed == RTMP_METHOD_ONSTATUS)) {

      if((rc = onresp_onstatus(pClient, &amfEntries[0], RTMP_METHOD_PUBLISH)) >= CONNECT_RETRY_RC_OK) {

        pReqCtxt->pStreamerCfg->rtmppublish.ispublished = 1;

        //
        // Signal the conditional indicating a that streaming should start
        // This will cause stream_rtmp_publish_start to return to it's caller
        //
        vsxlib_cond_signal(&pReqCtxt->pStreamerCfg->rtmppublish.cond.cond, 
                           &pReqCtxt->pStreamerCfg->rtmppublish.cond.mtx);

      }

    } else if(pRtmp->state == RTMP_STATE_CLI_PUBLISH && 
               (pRtmp->methodParsed == RTMP_METHOD_RESULT || pRtmp->methodParsed == RTMP_METHOD_ONSTATUS)) {
      LOG(X_DEBUG("RESP TO ONST.."));
    }

  }

  //
  // If the server needs authentication, check if we should reconnect and try again with credentials
  //
  if(pClient->auth.ctxt.lastAuthState == RTMP_AUTH_STATE_FATAL) {
    retryRc = CONNECT_RETRY_RC_NORETRY;
  } else if(pClient->auth.ctxt.consecretry < 3 && (
     pClient->auth.ctxt.lastAuthState == RTMP_AUTH_STATE_NEED_CHALLENGE ||
      pClient->auth.ctxt.lastAuthState == RTMP_AUTH_STATE_HAVE_CHALLENGE)) {
     retryRc = CONNECT_RETRY_RC_CONNECTAGAIN; 
  }

  //LOG(X_DEBUG("DONE... with connect...rc: %d, lastAuthState: %d, auth.consecretry:%d, retryRc: %d"), rc, pClient->auth.ctxt.lastAuthState, pClient->auth.ctxt.consecretry, retryRc);

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_run_publishclient loop done rc: %d, retryRc: %d, running: %d"), 
                     rc,  retryRc, pReqCtxt->pStreamerCfg->running); );

  return retryRc;
}

CONNECT_RETRY_RC_T stream_rtmp_cbonpublishstream(void *pArg) {
  CONNECT_RETRY_RC_T retryRc = CONNECT_RETRY_RC_OK;

  retryRc = rtmp_run_publishclient((RTMP_REQ_CTXT_T *) pArg);


  return retryRc;
}

static int stream_rtmp_outfmt_start(STREAMER_CFG_T *pStreamerCfg, RTMP_CTXT_CLIENT_T *pClient) {
  int rc = 0;
  unsigned int numQFull = 0;
  STREAMER_OUTFMT_T *pLiveFmt = NULL;
  OUTFMT_CFG_T *pOutFmt = NULL;
  STREAM_STATS_T *pstats = NULL;
  RTMP_CTXT_T *pRtmp = &pClient->ctxt;

  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMPPUBLISH];
  pLiveFmt->do_outfmt = 1;

  if(pStreamerCfg->pMonitor && pStreamerCfg->pMonitor->active) {
      if(!(pstats = stream_monitor_createattach(pStreamerCfg->pMonitor,
             (const struct sockaddr *) &pClient->sd.sa, STREAM_METHOD_RTMP, STREAM_MONITOR_ABR_NONE))) {
      }
  }

  //
  // Add a livefmt cb
  //
  if(!(pOutFmt = outfmt_setCb(pLiveFmt, rtmp_addFrame, pRtmp, &pLiveFmt->qCfg, pstats, 1, 
                           pStreamerCfg->frameThin, &numQFull))) {
    stream_stats_destroy(&pstats, NULL);
    return -1;
  }

  if(rtmp_client_init(pClient, NULL, pLiveFmt->qCfg.maxPktLen, pLiveFmt->qCfg.growMaxPktLen) < 0) {
    outfmt_removeCb(pOutFmt);
    stream_stats_destroy(&pstats, NULL);
    return -1;
  }

  pRtmp->pSd = &pClient->sd;
  pRtmp->novid = pStreamerCfg->novid;
  pRtmp->noaud = pStreamerCfg->noaud;
  pRtmp->av.vid.pStreamerCfg = pStreamerCfg;
  pRtmp->av.aud.pStreamerCfg = pStreamerCfg;
  pClient->pStreamStats = pstats;
  pClient->pOutFmt = pOutFmt;
  NETIOSOCK_FD(pClient->sd.netsocket) = INVALID_SOCKET;
  pClient->auth.pcreds = &pStreamerCfg->creds[STREAMER_AUTH_IDX_RTMPPUBLISH].stores[0];

  //
  // Unpause the outfmt callback mechanism now that rtmp_init was called
  //
  outfmt_pause(pOutFmt, 0);

  return rc;
}

static int stream_rtmp_outfmt_stop(RTMP_CTXT_CLIENT_T *pClient) {
  int rc = 0;

  if(pClient->pOutFmt) {
    outfmt_removeCb(pClient->pOutFmt);
    pClient->pOutFmt = NULL; 
  }
  if(pClient->pStreamStats) {
    stream_stats_destroy(&pClient->pStreamStats, NULL);
  }

  return rc;
}

static int stream_rtmp_publish(STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;
  //unsigned int outidx = 0;
  STREAMER_DEST_CFG_T *pDestCfg;
  RTMP_CTXT_CLIENT_T rtmpClient;
  RTMP_REQ_CTXT_T reqCtxt;
  CONNECT_RETRY_CTXT_T retryCtxt;
  char uribuf[CAPTURE_HTTP_HOSTBUF_MAXLEN];

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - stream_rtmp_publish start...")); );

  if(!pStreamerCfg || !(pDestCfg = &pStreamerCfg->pdestsCfg[0])) {
    return -1;
  }

  memset(&rtmpClient, 0, sizeof(rtmpClient));

  if(pDestCfg->dstUri[0] == '\0') {
    LOG(X_ERROR("No RTMP URL specified for %s"), pDestCfg->dstHost);
    return -1;
  } else if(capture_getdestFromStr(pDestCfg->dstHost, &rtmpClient.sd.sa, NULL, NULL, pDestCfg->ports[0]) < 0) {
    return -1;
  }

  if((rc = stream_rtmp_outfmt_start(pStreamerCfg, &rtmpClient)) < 0) {
    stream_rtmp_close(pStreamerCfg);
    return -1;
  }

  memset(&reqCtxt, 0, sizeof(reqCtxt));
  reqCtxt.pStreamerCfg = pStreamerCfg;
  reqCtxt.pClient = &rtmpClient;
  reqCtxt.pCfg = &pStreamerCfg->rtmppublish.cfg;
  //pthread_mutex_init(&reqCtxt.mtx, NULL);

  if(IS_CAPTURE_FILTER_TRANSPORT_SSL(pDestCfg->outTransType)) {
    rtmpClient.sd.netsocket.flags |= NETIO_FLAG_SSL_TLS;
  }

  uribuf[0] = '\0';
  rtmpClient.purl = pDestCfg->dstHost;
  rtmpClient.puri = pDestCfg->dstUri;
  memcpy(&rtmpClient.client.cfg, &pStreamerCfg->rtmppublish.cfg, sizeof(rtmpClient.client.cfg));
  strncpy(uribuf, rtmpClient.puri, sizeof(uribuf) - 1);
  capture_rtmpUriParts(uribuf, (char **) &rtmpClient.puri, (char **) &rtmpClient.puridocname);
  rtmpClient.ctxt.av.aud.pStreamerCfg = pStreamerCfg;
  rtmpClient.ctxt.av.vid.pStreamerCfg = pStreamerCfg;

  pStreamerCfg->rtmppublish.pSd = &rtmpClient.sd;

  //
  // Initialize the retry context for connecting to the remote endpoint
  //
  memset(&retryCtxt, 0, sizeof(retryCtxt));
  retryCtxt.cbConnectedAction = stream_rtmp_cbonpublishstream;
  retryCtxt.pConnectedActionArg = &reqCtxt;
  //retryCtxt.pAuthCliCtxt = &session.authCliCtxt;
  retryCtxt.pnetsock = &rtmpClient.sd.netsocket;
  retryCtxt.psa = (struct sockaddr *) &rtmpClient.sd.sa;
  retryCtxt.connectDescr = "RTMP";
  retryCtxt.pconnectretrycntminone = &pStreamerCfg->rtmppublish.connectretrycntminone;
  retryCtxt.tmtms = 0;

  //
  // Connect to the remote and call capture_rtsp_cbonannounceurl
  // This will automatically invoke any retry logic according to the retry config
  //
  rc = connect_with_retry(&retryCtxt);

  if(!pStreamerCfg->rtmppublish.ispublished) {
    pStreamerCfg->running = STREAMER_STATE_ERROR;
  }
  pStreamerCfg->rtmppublish.ispublished = 0;

  stream_rtmp_outfmt_stop(&rtmpClient);
  rtmp_client_close(&rtmpClient);
  stream_rtmp_close(pStreamerCfg);

  return rc;
}

static void rtmp_stream_publish_proc(void *pArg) {
  STREAMER_CFG_T *pStreamerCfg = (STREAMER_CFG_T *) pArg;

  LOG(X_DEBUG("RTMP stream publish thread started"));

  stream_rtmp_publish(pStreamerCfg);

  //
  // Signal the conditional which cause stream_rtmp_publish_start to return to it's caller
  // if it hasn't already
  //
  vsxlib_cond_signal(&pStreamerCfg->rtmppublish.cond.cond, &pStreamerCfg->rtmppublish.cond.mtx);

  LOG(X_DEBUG("RTMP stream publish thread exiting"));
}

int stream_rtmp_publish_start(STREAMER_CFG_T *pStreamerCfg, int wait_for_setup) {

  int rc = 0;
  pthread_t ptdMonitor;
  pthread_attr_t attrInterleaved;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - stream_rtmp_publish_start")) );

  if(!pStreamerCfg) {
    return -1;
  }

  PHTREAD_INIT_ATTR(&attrInterleaved);
  pthread_mutex_init(&pStreamerCfg->rtmppublish.cond.mtx, NULL);
  pthread_cond_init(&pStreamerCfg->rtmppublish.cond.cond, NULL);

  if(pthread_create(&ptdMonitor,
                    &attrInterleaved,
                    (void *) rtmp_stream_publish_proc,
                    (void *) pStreamerCfg) != 0) {
    LOG(X_ERROR("Unable to create RTMP stream publish thread"));

    pthread_cond_destroy(&pStreamerCfg->rtmppublish.cond.cond);
    pthread_mutex_destroy(&pStreamerCfg->rtmppublish.cond.mtx);
    return -1;
  }

  if(wait_for_setup) {
    if((rc = vsxlib_cond_waitwhile(&pStreamerCfg->rtmppublish.cond, NULL, 0)) >= 0) {
      if(!pStreamerCfg->rtmppublish.ispublished) {
        rc = -1;
      }
    }
    LOG(X_DEBUG("RTMP stream publish done waiting for start completion ispublished:%d"),
        pStreamerCfg->rtmppublish.ispublished);
  }

  if(rc < 0) {
    stream_rtmp_close(pStreamerCfg);
  }

  return rc;

}

#endif // VSX_HAVE_CAPTURE
