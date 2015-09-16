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


#define RTMP_TCURL_SZMAX     512


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
    rc = pktqueue_addpkt(pRtmp->pQAud, pRtmp->ctxt.av.aud.tmpFrame.buf, idx + len, &xtra, 0);
    //fprintf(stderr, "QUEING AUD rc:%d len:%d %.3f %.3f\n", rc, idx + len, PTSF(xtra.tm.pts), PTSF(xtra.tm.dts));

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
  PKTQ_EXTRADATA_T xtra;

  memset(&frame, 0, sizeof(frame));
  frame.pTmpFrame =  &pRtmp->ctxt.av.vid.tmpFrame;

  if(pTag->avc.pkttype == FLV_VID_AVC_PKTTYPE_SEQHDR) {

    //fprintf(stderr, "NAL SEQHDRS len:%d\n", len);
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

      //fprintf(stderr, "NAL idx:%d type:0x%x ts[%d]:%d tsAbsolute:%d tm:%d, len:%d 0x%x 0x%x 0x%x 0x%x\n", idx, frame.nalType, pRtmp->ctxt.streamIdx, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.ts, ts, pTag->avc.comptime32, lennal, pData[idx], pData[idx+1], pData[idx+2], pData[idx+3]);
      //avc_dumpHex(stderr, &pData[idx], len > 16 ? 16 : len, 1);

      if(frame.nalType != NAL_TYPE_ACCESS_UNIT_DELIM) {

        if(append_h264_frame(pRtmp, &frame, &pData[idx], lennal) < 0) {
          return -1;
        }
      }

      idx += lennal;
    }

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

      //fprintf(stderr, "RTMP VID AVC PTS:%.3f DTS:%.3f tm:%d, len:%d (from flv pkt:%.3f %.3f)\n", PTSF(xtra.tm.pts), PTSF(xtra.tm.dts), pTag->avc.comptime32, frame.idx, PTSF(ts * 90), PTSF(pTag->avc.comptime32 * 90));

      //if(frame.keyFrame) {
      //  xtra.flags = CAPTURE_SP_FLAG_KEYFRAME;
      //} else {
      //  xtra.flags = 0;
      //}
      rc = pktqueue_addpkt(pRtmp->pQVid, frame.pTmpFrame->buf, frame.idx, &xtra, frame.keyFrame);
      //delmeh264(frame.pTmpFrame->buf, frame.idx);
      //fprintf(stderr, "QUEING VID rc:%d len:%d pts:%.3f dts:%.3f (comptime32:%d) keyFr:%d (qmax:%d,%d)\n", rc, frame.idx, PTSF(xtra.tm.pts), PTSF(xtra.tm.dts), pTag->avc.comptime32, frame.keyFrame, pRtmp->pQVid->cfg.maxPktLen, pRtmp->pQVid->cfg.growMaxPktLen);
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


//static unsigned char delme[64];

static int rtmp_connect(RTMP_CTXT_CLIENT_T *pRtmp, CAP_ASYNC_DESCR_T *pCfg) {
  int rc = 0;
  unsigned int lenData;

  pRtmp->client.app = pCfg->pcommon->addrsExt[0];
  pRtmp->client.flashVer = "WIN 10,0,32,18";
  //pRtmp->client.swfUrl = "http://192.168.1.151:8082/wowza/LiveVideoStreaming/client/live.swf";
  //pRtmp->client.pageUrl = "http://192.168.1.151:8082/wowza/LiveVideoStreaming/client/live.html";
  snprintf((char *) pRtmp->client.tcUrl, RTMP_TCURL_SZMAX , 
           "rtmp://%s", pCfg->pcommon->localAddrs[0]);

  pRtmp->client.playElem = pCfg->pcommon->addrsExtHost[0];


  if((rc = rtmp_create_connect(&pRtmp->ctxt, &pRtmp->client)) < 0) {
    return rc; 
  }

  //avc_dumpHex(stderr, pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx, 1);

  lenData = pRtmp->ctxt.out.idx - 12;
  pRtmp->ctxt.out.idx = 12;
  if((rc = rtmp_send_chunkdata(&pRtmp->ctxt, RTMP_STREAM_IDX_INVOKE, 
                               &pRtmp->ctxt.out.buf[12], lenData, 0)) < 0) {
    return rc;
  }

  pRtmp->ctxt.out.idx = 0;

  return rc;
}

static int rtmp_createStream(RTMP_CTXT_T *pRtmp) {
  int rc = 0;

  if(pRtmp->serverbw > 0) {
    if((rc = rtmp_create_serverbw(pRtmp, pRtmp->serverbw)) < 0) {
      return rc; 
    }
  }

  if((rc = rtmp_create_createStream(pRtmp)) < 0) {
    return rc;
  }

  if((rc = rtmp_create_ping_client(pRtmp, 0x03, 0x00, 1, 0x0bb8)) < 0) {
    return rc;
  }

  if((rc = netio_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa, pRtmp->out.buf, 
                      pRtmp->out.idx)) < 0) {
    return -1;
  }
  pRtmp->out.idx = 0; 

  return rc;
}

static int rtmp_play(RTMP_CTXT_CLIENT_T *pRtmp) {
  int rc = 0;
  unsigned int lenData;

  if((rc = rtmp_create_play(&pRtmp->ctxt, &pRtmp->client)) < 0) {
    return rc;
  }


  lenData = pRtmp->ctxt.out.idx - 12;
  pRtmp->ctxt.out.idx = 12;
  if((rc = rtmp_send_chunkdata(&pRtmp->ctxt, RTMP_STREAM_IDX_INVOKE, 
                               &pRtmp->ctxt.out.buf[12], lenData, 0)) < 0) {
    return rc;
  }
  pRtmp->ctxt.out.idx = 0; 


  if((rc = rtmp_create_ping_client(&pRtmp->ctxt, 0x03, 0x01, 1, 0x0bb8)) < 0) {
    return rc;
  }

  if((rc = netio_send(&pRtmp->ctxt.pSd->netsocket, (const struct sockaddr *)  &pRtmp->ctxt.pSd->sa, 
                    pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx)) < 0) {
    return -1;
  }

  pRtmp->ctxt.out.idx = 0; 

  return rc;
}

static FLV_AMF_T *amf_find(FLV_AMF_T *pAmf, const char *str) {

  while(pAmf) {
    if(pAmf->key.p && !strncasecmp((const char *) pAmf->key.p, str, strlen(str))) {
      return pAmf;
    }
    pAmf = pAmf->pnext;
  }
  return NULL;
}

static const char *amf_get_key_string(FLV_AMF_T *pAmf) {
  if(pAmf && pAmf->val.type == FLV_AMF_TYPE_STRING && pAmf->val.u.str.p) {
    return (const char *) pAmf->val.u.str.p;
  } else {
    return "";
  }
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

static int rtmp_handle_ping(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  uint16_t type;
  uint32_t arg1;
  char tmp[128];
  //char buf[SAFE_INET_NTOA_LEN_MAX];

  type = htons( *((uint16_t *) &pRtmp->in.buf[0]) ); 

  //fprintf(stderr, "received rtmp ping type: 0x%x\n", type);
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
           (rc = netio_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa, pRtmp->out.buf, 
                    pRtmp->out.idx)) < 0) {
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

static int rtmp_parse_readpkt_client(RTMP_CTXT_T *pRtmp, FLV_AMF_T *pAmfList) {
  int rc = 0;
  int contentType;
  //FLV_AMF_T *pAmf = NULL;

  pRtmp->in.idx = 0;

  if((rc = rtmp_parse_readpkt(pRtmp)) < 0) {
    return -1;
  } else if(rc == 0) {
    LOG(X_ERROR("Failed to read entire rtmp packet contents"));
    return -1;
  }
  //fprintf(stderr, "rtmp full pkt sz:%d ct:%d, id:%d, ts:%d(%d), dest:0x%x, hdr:%d\n", pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, pRtmp->pkt[pRtmp->streamIdx].hdr.contentType, pRtmp->pkt[pRtmp->streamIdx].hdr.id, pRtmp->pkt[pRtmp->streamIdx].hdr.ts, pRtmp->pkt[pRtmp->streamIdx].tsAbsolute, pRtmp->pkt[pRtmp->streamIdx].hdr.dest, pRtmp->pkt[pRtmp->streamIdx].szHdr0);
  //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt > 16 ? 16 : pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);

  //*pRtmpContentType = pRtmp->pkt[pRtmp->streamIdx].hdr.contentType;
  contentType = pRtmp->pkt[pRtmp->streamIdx].hdr.contentType;

  switch(contentType) {

    case RTMP_CONTENT_TYPE_INVOKE:
    case RTMP_CONTENT_TYPE_MSG:
    case RTMP_CONTENT_TYPE_NOTIFY:
      //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);
      rc = rtmp_parse_invoke(pRtmp, pAmfList);
      break;
    case RTMP_CONTENT_TYPE_SERVERBW:
      rtmp_parse_serverbw(pRtmp);
      break;
    case RTMP_CONTENT_TYPE_CLIENTBW:
      break;
    case RTMP_CONTENT_TYPE_PING:
      rtmp_handle_ping(pRtmp);    
      break;
    case RTMP_CONTENT_TYPE_CHUNKSZ:
      break;

    default:
      //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);
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

  if(rc >=0) {
    rc = contentType;
  }

  return rc;
}


static int rtmp_is_resp_error(FLV_AMF_T *pAmfList, const char *operation) {
  int rc = 0;
  FLV_AMF_T *pAmf;

  if((pAmf = amf_find(pAmfList, "_error"))) {

    rc = 1;

    LOG(X_ERROR("RTMP %s error code: '%s' description: '%s'"),
      operation, amf_get_key_string(amf_find(pAmfList, "code")),
      amf_get_key_string(amf_find(pAmfList, "description")));

  }

  return rc;
}

static void rtmp_client_close(RTMP_CTXT_CLIENT_T *pRtmp) {

  rtmp_close(&pRtmp->ctxt);

}

static int rtmp_client_init(CAP_ASYNC_DESCR_T *pCfg, RTMP_CTXT_CLIENT_T *pRtmp) {
  int rc = 0;

  memset(pRtmp, 0, sizeof(RTMP_CTXT_CLIENT_T));

  if((rc = rtmp_parse_init(&pRtmp->ctxt, RTMP_TMPFRAME_VID_SZ, 0x1000)) < 0) {
    return rc;
  }

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

  pRtmp->ctxt.cbRead = rtmp_cbReadDataNet;
  pRtmp->ctxt.pCbData = pRtmp;
  pRtmp->ctxt.state = RTMP_STATE_INVALID;

  pRtmp->ctxt.connect.capabilities = 15.0f;
  pRtmp->ctxt.connect.objEncoding = 3.0f;
  pRtmp->ctxt.chunkSz = 128;

  if(pCfg->pcommon->filt.filters[0].pCapAction) {
    pRtmp->pQVid = pCfg->pcommon->filt.filters[0].pCapAction->pQueue;
  }
  if(pCfg->pcommon->filt.filters[1].pCapAction) {
    pRtmp->pQAud = pCfg->pcommon->filt.filters[1].pCapAction->pQueue;
  }

  return rc;
}

static CONNECT_RETRY_RC_T getrtmpdata(CAP_ASYNC_DESCR_T *pCfg) {
  CONNECT_RETRY_RC_T rc = CONNECT_RETRY_RC_OK;
  int contentType;
  RTMP_CTXT_CLIENT_T rtmp;
  SOCKET_DESCR_T sockDescr;
  unsigned int idx;
  const char *p = NULL;
  struct timeval tv0, tv;
  char tcUrl[RTMP_TCURL_SZMAX];
  FLV_AMF_T *pAmf = NULL;
  FLV_AMF_T amfEntries[20];
  //char buf[SAFE_INET_NTOA_LEN_MAX];
  char tmp[128];

  memset(&sockDescr, 0, sizeof(sockDescr));
  NETIO_SET(sockDescr.netsocket, pCfg->pSockList->netsockets[0]);
  memcpy(&sockDescr.sa, &pCfg->pSockList->salist[0], sizeof(sockDescr.sa));

  if(rtmp_client_init(pCfg, &rtmp) < 0) {
    return CONNECT_RETRY_RC_NORETRY;
  }
  rtmp.client.tcUrl = tcUrl;
  rtmp.client.pageUrl = pCfg->pcommon->rtmppageurl;
  rtmp.client.swfUrl = pCfg->pcommon->rtmpswfurl;
  rtmp.fp9 = pCfg->pcommon->rtmpfp9;
  rtmp.ctxt.av.aud.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.ctxt.av.vid.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.ctxt.pSd = &sockDescr;

  if(rtmp_handshake_cli(&rtmp.ctxt, rtmp.fp9) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
        FORMAT_NETADDR(rtmp.ctxt.pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(rtmp.ctxt.pSd->sa)));
    rc = CONNECT_RETRY_RC_ERROR;
  } 

  gettimeofday(&tv0, NULL);

  if(rtmp_connect(&rtmp, pCfg) < 0)  {
    rc = CONNECT_RETRY_RC_ERROR;
  } 

  while(rc >= CONNECT_RETRY_RC_OK  && !g_proc_exit) {

    memset(amfEntries, 0, sizeof(amfEntries));
    for(idx = 0; idx < (sizeof(amfEntries) / sizeof(amfEntries[0])) - 1; idx++) {
      amfEntries[idx].pnext = &amfEntries[idx + 1];
    }

    //rtmp.contentTypeLastInvoke = 0;

    if((contentType = rtmp_parse_readpkt_client(&rtmp.ctxt, &amfEntries[0])) < 0) {
      LOG(X_ERROR("Failed to read rtmp packet from server"));
      rc  = CONNECT_RETRY_RC_ERROR;
      break;
    } 

    //fprintf(stderr, "READ PKT CT:%d LASTCT: 0x%x state:%d\n", contentType, rtmp.ctxt.contentTypeLastInvoke, rtmp.ctxt.state);

    if(rtmp.ctxt.state < RTMP_STATE_CLI_CONNECT && 
       contentType == RTMP_CONTENT_TYPE_INVOKE) {

      if(rtmp_is_resp_error(&amfEntries[0], "Connect")) {
        rtmp.ctxt.state = RTMP_STATE_ERROR;
        rc = CONNECT_RETRY_RC_ERROR;
      } else if((pAmf = amf_find(&amfEntries[0], "_result"))) {

        //TODO: check code ??
        //pAmf = amf_find(&amfEntries[0], "code");

        LOG(X_DEBUG("RTMP Connect succesful: '%s' description: '%s'"),
          amf_get_key_string(amf_find(&amfEntries[0], "code")),
          amf_get_key_string(amf_find(&amfEntries[0], "description")));

        if(rtmp_createStream(&rtmp.ctxt) < 0) {
          rtmp.ctxt.state = RTMP_STATE_ERROR;
          rc = CONNECT_RETRY_RC_ERROR;
        } else {   
          rtmp.ctxt.state = RTMP_STATE_CLI_CONNECT;
        }

      }
     
    } else if(rtmp.ctxt.state == RTMP_STATE_CLI_CONNECT && 
              (contentType == RTMP_CONTENT_TYPE_MSG ||
               contentType == RTMP_CONTENT_TYPE_NOTIFY ||
               contentType == RTMP_CONTENT_TYPE_INVOKE)) {

      if(rtmp_is_resp_error(&amfEntries[0], "createStream")) {
        rtmp.ctxt.state = RTMP_STATE_ERROR;
        rc = CONNECT_RETRY_RC_ERROR;
      } else if((pAmf = amf_find(&amfEntries[0], "_result"))) {

        LOG(X_DEBUG("RTMP createStream ok"));

        if(rtmp_play(&rtmp) < 0) {
          rtmp.ctxt.state = RTMP_STATE_ERROR;
          rc = CONNECT_RETRY_RC_ERROR;
        } else {   
          rtmp.ctxt.state = RTMP_STATE_CLI_CREATESTREAM;
        }


      }

    } else if(rtmp.ctxt.state == RTMP_STATE_CLI_CREATESTREAM && 
              (contentType == RTMP_CONTENT_TYPE_MSG ||
               contentType == RTMP_CONTENT_TYPE_NOTIFY ||
               contentType == RTMP_CONTENT_TYPE_INVOKE)) {

      if(rtmp_is_resp_error(&amfEntries[0], "play")) {
        rtmp.ctxt.state = RTMP_STATE_ERROR;
        rc = CONNECT_RETRY_RC_ERROR;
        break;
      } 

      if((pAmf = amf_find(&amfEntries[0], "onStatus"))) {

        if((p = amf_get_key_string(amf_find(&amfEntries[0], "code"))) &&
          (!strncasecmp(p, "NetStream.Data.Start", strlen("NetStream.Data.Start")) || 
           !strncasecmp(p, "NetStream.Play.Start", strlen("NetStream.Play.Start")))) {
          rtmp.ctxt.state = RTMP_STATE_CLI_PLAY;
        }


      } else if((pAmf = amf_find(&amfEntries[0], "onFCSubscribe"))) {
        if((p = amf_get_key_string(amf_find(&amfEntries[0], "code"))) &&
          !strncasecmp(p, "NetStream.Play.Start", strlen("NetStream.Play.Start"))) {
          rtmp.ctxt.state = RTMP_STATE_CLI_PLAY;
        }
      }

      if(rtmp.ctxt.state == RTMP_STATE_CLI_PLAY) {

        gettimeofday(&tv0, NULL);

        //if(!strncasecmp(p, "NetStream.Data.Start", strlen("NetStream.Data.Start")) ||
        //   !strncasecmp(p, "NetStream.Play.Start", strlen("NetStream.Play.Start"))) {
          LOG(X_INFO("RTMP play succesful: '%s' description: '%s'"),
            amf_get_key_string(amf_find(&amfEntries[0], "code")),
            amf_get_key_string(amf_find(&amfEntries[0], "description")));

        //} else {

          LOG(X_DEBUG("RTMP play) succesful: '%s' description: '%s'"),
            amf_get_key_string(amf_find(&amfEntries[0], "code")),
            amf_get_key_string(amf_find(&amfEntries[0], "description")));
        //}

      }

    } else if(rtmp.ctxt.state == RTMP_STATE_CLI_PLAY) {

      if(contentType == RTMP_CONTENT_TYPE_MSG ||
         contentType == RTMP_CONTENT_TYPE_NOTIFY ||
         contentType == RTMP_CONTENT_TYPE_INVOKE) {

         if(amf_find(&amfEntries[0], "onMetaData")) {
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

        if(netio_send(&rtmp.ctxt.pSd->netsocket, (const struct sockaddr *) &rtmp.ctxt.pSd->sa, 
                      rtmp.ctxt.out.buf, rtmp.ctxt.out.idx) < 0) {
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
  return getrtmpdata((CAP_ASYNC_DESCR_T *) pArg);
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

  //
  // Connect to the remote and call capture_rtmp_cbonconnect
  // This will automatically invoke any retry logic according to the retry config
  //
  rc = connect_with_retry(&retryCtxt);

  netio_closesocket(&pCfg->pSockList->netsockets[0]);

  return rc;
}

static int rtmp_parse_readpkt_srv(RTMP_CTXT_T *pRtmp, FLV_AMF_T *pAmfList) {
  int rc = 0;
  int contentType;
  //FLV_AMF_T *pAmf = NULL;

  pRtmp->in.idx = 0;

  if((rc = rtmp_parse_readpkt(pRtmp)) < 0) {
    return -1;
  } else if(rc == 0) {
    LOG(X_ERROR("Failed to read entire rtmp packet contents"));
    return -1;
  }
  //fprintf(stderr, "rtmp full pkt_srv sz:%d ct:%d, id:%d, ts:%d(%d), dest:0x%x, hdr:%d\n", pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, pRtmp->pkt[pRtmp->streamIdx].hdr.contentType, pRtmp->pkt[pRtmp->streamIdx].hdr.id, pRtmp->pkt[pRtmp->streamIdx].hdr.ts, pRtmp->pkt[pRtmp->streamIdx].tsAbsolute, pRtmp->pkt[pRtmp->streamIdx].hdr.dest, pRtmp->pkt[pRtmp->streamIdx].szHdr0);
  //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt > 16 ? 16 : pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);

  //*pRtmpContentType = pRtmp->pkt[pRtmp->streamIdx].hdr.contentType;
  contentType = pRtmp->pkt[pRtmp->streamIdx].hdr.contentType;

  switch(contentType) {

    case RTMP_CONTENT_TYPE_INVOKE:
    case RTMP_CONTENT_TYPE_MSG:
    case RTMP_CONTENT_TYPE_NOTIFY:
      //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);
      rc = rtmp_parse_invoke(pRtmp, pAmfList);
      break;
    case RTMP_CONTENT_TYPE_SERVERBW:
      rtmp_parse_serverbw(pRtmp);
      break;
    case RTMP_CONTENT_TYPE_CLIENTBW:
      break;
    case RTMP_CONTENT_TYPE_PING:
      rtmp_handle_ping(pRtmp);    
      break;
    case RTMP_CONTENT_TYPE_CHUNKSZ:
      break;

    default:
      //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);
      break;
  }

  if(rc >=0) {
    rc = contentType;
  }

  return rc;
}

static int cap_rtmp_handle_conn(RTMP_CTXT_CLIENT_T *pRtmp) {
  int rc = 0;
  unsigned int idx;
  int contentType;
  char tmp[128];
  FLV_AMF_T amfEntries[20];
  int haveFcPublish = 0;
  int haveCreateStream = 0;
  int havePublish = 0;

  pRtmp->ctxt.state = RTMP_STATE_CLI_START;

  //
  // Accept handshake
  //
  if((rc = rtmp_handshake_srv(&pRtmp->ctxt)) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
      FORMAT_NETADDR(pRtmp->ctxt.pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pRtmp->ctxt.pSd->sa)));
    return rc;
  }

  pRtmp->ctxt.state = RTMP_STATE_CLI_HANDSHAKEDONE;
  LOG(X_DEBUG("rtmp handshake completed"));

  //
  // Read connect packet
  //
  if((rc = rtmp_parse_readpkt_full(&pRtmp->ctxt, 1)) < 0 ||
    pRtmp->ctxt.state != RTMP_STATE_CLI_CONNECT) {
    LOG(X_ERROR("Failed to read rtmp connect request"));
    return rc;
  }

  //
  // Create server response
  //
  pRtmp->ctxt.out.idx = 0;
  rtmp_create_serverbw(&pRtmp->ctxt, 2500000);
  rtmp_create_clientbw(&pRtmp->ctxt, 2500000);
  rtmp_create_ping(&pRtmp->ctxt, 0, 0);
  rtmp_create_chunksz(&pRtmp->ctxt, RTMP_CHUNK_SZ_OUT);
  rtmp_create_result_invoke(&pRtmp->ctxt);
  if((rc = netio_send(&pRtmp->ctxt.pSd->netsocket, (const struct sockaddr *) &pRtmp->ctxt.pSd->sa, 
                      pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx)) < 0) {
    return -1;
  }


  while(rc >= 0 && !g_proc_exit) {

    memset(amfEntries, 0, sizeof(amfEntries));
    for(idx = 0; idx < (sizeof(amfEntries) / sizeof(amfEntries[0])) - 1; idx++) {
      amfEntries[idx].pnext = &amfEntries[idx + 1];
    }

    //rtmp.contentTypeLastInvoke = 0;

    if((rc = contentType = rtmp_parse_readpkt_srv(&pRtmp->ctxt, &amfEntries[0])) < 0) {
      LOG(X_ERROR("Failed to read rtmp (capture) packet from client"));
      break;
    }
//fprintf(stderr, "STATE:%d CT:%d chunksz:%d\n", pRtmp->ctxt.state, contentType, pRtmp->ctxt.chunkSz);

    if(contentType == RTMP_CONTENT_TYPE_INVOKE &&
       !haveFcPublish && pRtmp->ctxt.state == RTMP_STATE_CLI_FCPUBLISH) {

      //
      // Create FCPublish response
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_onfcpublish(&pRtmp->ctxt);
      if((rc = netio_send(&pRtmp->ctxt.pSd->netsocket, (const struct sockaddr *) &pRtmp->ctxt.pSd->sa, 
                          pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx)) < 0) {
        break;
      }

      haveFcPublish = 1;

    } else if(contentType == RTMP_CONTENT_TYPE_INVOKE &&
             //!havePublish && haveFcPublish && !haveCreateStream && 
             !haveCreateStream && 
             pRtmp->ctxt.state == RTMP_STATE_CLI_CREATESTREAM) {

      //
      // Create createstream response 
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_result(&pRtmp->ctxt);
      if((rc = netio_send(&pRtmp->ctxt.pSd->netsocket, (const struct sockaddr *) &pRtmp->ctxt.pSd->sa, 
                          pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx)) < 0) {
        return -1;
      }

      haveCreateStream = 1;

    } else if(contentType == RTMP_CONTENT_TYPE_INVOKE &&
              !havePublish && pRtmp->ctxt.state == RTMP_STATE_CLI_PUBLISH) {

      //
      // Create publish response 
      //
      pRtmp->ctxt.out.idx = 0;
      rtmp_create_ping(&pRtmp->ctxt, 0, 0x01);
      rtmp_create_onstatus(&pRtmp->ctxt, RTMP_ONSTATUS_TYPE_PUBLISH);
      if((rc = netio_send(&pRtmp->ctxt.pSd->netsocket, (const struct sockaddr *) &pRtmp->ctxt.pSd->sa, 
                        pRtmp->ctxt.out.buf, pRtmp->ctxt.out.idx)) < 0) {
        return -1;
      }

      havePublish = 1;
      pRtmp->ctxt.state = RTMP_STATE_CLI_PLAY;

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
        LOG(X_DEBUG("RTMP possibly unhandled play state contentType:%d"), contentType);
      }

    }


  }

  LOG(X_DEBUG("rtmp (capture) connection ending with state: %d"), pRtmp->ctxt.state);
  pRtmp->ctxt.state = RTMP_STATE_DISCONNECTED;

  return rc;
}

static void cap_rtmp_srv_proc(void *pfuncarg) {
  CLIENT_CONN_T *pConn = (CLIENT_CONN_T *) pfuncarg;
  CAP_ASYNC_DESCR_T *pCfg = (CAP_ASYNC_DESCR_T *) pConn->pUserData;
  RTMP_CTXT_CLIENT_T rtmp;
  int rc;
  SOCKET_DESCR_T sockDescr;
  char tmp[128];

  LOG(X_INFO("Starting RTMP capture connection from %s:%d"), 
         FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

  memset(&sockDescr, 0, sizeof(sockDescr));
  NETIO_SET(sockDescr.netsocket, pConn->sd.netsocket);
  memcpy(&sockDescr.sa, &pCfg->pSockList->salist[0], sizeof(sockDescr.sa));

  if((rc = rtmp_client_init(pCfg, &rtmp)) < 0) {
    return;
  }
  rtmp.ctxt.pSd = &sockDescr;
  rtmp.ctxt.connect.capabilities = 31.0f;
  rtmp.ctxt.av.aud.pStreamerCfg = pCfg->pStreamerCfg;
  rtmp.ctxt.av.vid.pStreamerCfg = pCfg->pStreamerCfg;

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
  
  if(pCfg->pcommon->filt.numFilters <= 0) {
    return -1;
  }

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
    pClient->pUserData = pCfg;
    pClient = (CLIENT_CONN_T *) pClient->pool.pNext;
  }


  if((NETIOSOCK_FD(pCfg->pSockList->netsockets[0]) = 
      net_listen((const struct sockaddr *) &pCfg->pSockList->salist[0], 2)) == INVALID_SOCKET) {
    pool_close(&pool, 0);
    return -1;
  }

  LOG(X_INFO("RTMP capture listener available on %s:%d"),
      FORMAT_NETADDR(pCfg->pSockList->salist[0], tmp, sizeof(tmp)), ntohs(INET_PORT(pCfg->pSockList->salist[0])));

  memset(&listenCfg, 0, sizeof(listenCfg));
  listenCfg.pnetsockSrv = &pCfg->pSockList->netsockets[0];
  listenCfg.pConnPool = &pool;
  listenCfg.urlCapabilities = URL_CAP_RTMPLIVE;

  //
  // Service any client connections on the live listening port
  //
  rc = srvlisten_loop(&listenCfg, cap_rtmp_srv_proc);

  netio_closesocket(&pCfg->pSockList->netsockets[0]);
  pool_close(&pool, 1);

  return rc;
}

#endif // VSX_HAVE_CAPTURE
