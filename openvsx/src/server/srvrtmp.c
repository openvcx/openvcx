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



//static int vframes;
//static int aframes;

#if 0
static int rtmp_create_vidframe_info(RTMP_CTXT_T *pRtmp, uint8_t type) {
  int rc = 0;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
      RTMP_STREAM_IDX_7, 0, 2, RTMP_CONTENT_TYPE_VIDDATA, htonl(0x01), 12)) < 0) {
  //    RTMP_STREAM_IDX_7, 0, 6, RTMP_CONTENT_TYPE_VIDDATA, 0, 8)) < 0) {
    return rc;
  }

  pRtmp->out.idx += rc;
  pRtmp->out.buf[pRtmp->out.idx++] = (FLV_VID_FRM_VIDINFO << 4) | (FLV_VID_CODEC_SORENSON);
  //pRtmp->out.buf[pRtmp->out.idx++] = FLV_VID_AVC_PKTTYPE_NALU;
  //pRtmp->out.buf[pRtmp->out.idx++] = 0;
  //pRtmp->out.buf[pRtmp->out.idx++] = 0;
  //pRtmp->out.buf[pRtmp->out.idx++] = 0;
  pRtmp->out.buf[pRtmp->out.idx++] = type;

  return 0;
}
#endif // 0

/*
static int testseq(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  unsigned int hdrsz = 12;

  rtmp_create_vidframe_info(pRtmp, FLV_VID_AVC_PKTTYPE_SEQHDR);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
    RTMP_STREAM_IDX_7, 0, 5, RTMP_CONTENT_TYPE_VIDDATA, htonl(0x01), hdrsz)) < 0) {
    return rc;
  }

  bs.idx = hdrsz;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx;
  bs.buf[bs.idx++] = (FLV_VID_FRM_KEYFRAME << 4) | (FLV_VID_CODEC_AVC);
  bs.buf[bs.idx++] = FLV_VID_AVC_PKTTYPE_ENDSEQ;
  flv_write_int24(&bs, 0); // composition time
  pRtmp->out.idx += bs.idx;

  rtmp_create_vidframe_info(pRtmp, FLV_VID_AVC_PKTTYPE_NALU);

  return rc;
}
*/

static int rtmp_send_vidframe(RTMP_CTXT_T *pRtmp, const OUTFMT_FRAME_DATA_T *pFrame, 
                              int keyframe) {
  int rc = 0;
  BYTE_STREAM_T bs;
  unsigned int hdrsz = 8;
  uint32_t mspts = 0;
  unsigned char *pData;
  unsigned int lenData;
  const uint8_t streamIdx = RTMP_STREAM_IDX_7;

/*
  if(pFrame->keyframe) {
    rtmp_create_vidframe_info(pRtmp, FLV_VID_AVC_PKTTYPE_SEQHDR);
    if((rc = netio_send(&pRtmp->pSd->netsocket, &pRtmp->pSd->sain, pRtmp->out.buf,
                        pRtmp->out.idx)) < 0) {
      return -1;
    }
    pRtmp->out.idx = 0;
  }
*/

/*
  int slice_type;
  int frame_idx;
  int poc;
  BIT_STREAM_T bs264;
  memset(&bs264, 0, sizeof(bs264));
  bs264.buf = pFrame->pData+5;
  bs264.sz = pFrame->len-5;
  bits_readExpGolomb(&bs264);
  if((slice_type = bits_readExpGolomb(&bs264)) > H264_SLICE_TYPE_SI) slice_type -= (H264_SLICE_TYPE_SI + 1);
  bits_readExpGolomb(&bs264);
  frame_idx = bits_read(&bs264, 7);
  poc = bits_read(&bs264, 7);
  fprintf(stderr, "H264 SLICE TYPE %d fridx:%d poc:%d\n", slice_type, frame_idx, poc);

  fprintf(stderr, "video frame len:%d\n", pFrame->len);
  avc_dumpHex(stderr, pFrame->pData, 16, 1);
*/


  bs.idx = hdrsz;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  if((rc = flvsrv_create_vidframe_h264(&bs, &pRtmp->av.vid, pFrame, &mspts, NULL, keyframe)) < 0) {
    return rc;
  }

  //TODO: this seems to make the playback smoother, but flowplayer may disconnect
  
  //if(msdts < 0) {
  //  msdts = 0;
  //  bs.idx -= 3;
  //  flv_write_int24(&bs, msdts); // composition time
  //}

  pData = &pRtmp->av.vid.tmpFrame.buf[pRtmp->av.vid.tmpFrame.idx];
  lenData = rc;

  //fprintf(stderr, "VID %s RTMP len[%d]:%d tm: %d COMPOSITION TIME: %d (dts:%.3f) pts:%.3f (%.3f)\n", keyframe ? "KEY" : "", pFrame->xout.outidx,  OUTFMT_LEN(pFrame), mspts, 0, PTSF(pFrame->dts), PTSF(pFrame->pts), PTSF(pFrame->pts - pRtmp->av.aud.tm0)); avc_dumpHex(stderr, pData, MIN(48, lenData), 1);


  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], streamIdx, 
       mspts, 5 + lenData, RTMP_CONTENT_TYPE_VIDDATA, htonl(0x01), hdrsz)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  rc = rtmp_send_chunkdata(pRtmp, streamIdx, pData, lenData, 5);

  return rc;
}

static int rtmp_send_audframe(RTMP_CTXT_T *pRtmp, const OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;
  BYTE_STREAM_T bs;
  //uint32_t tm;
  //uint64_t pts;
  uint32_t ms = 0;
  //unsigned int frameIdx = 0;
  unsigned int hdrsz = 8;
  //unsigned char *pData;
  unsigned int lenData;
  unsigned int lenHdr;
  const uint8_t streamIdx = RTMP_STREAM_IDX_6;


/*

  // Skip ADTS header
  if(pFrame->pData[0] == 0xff && (pFrame->pData[1] & 0xf0) == 0xf0) {
    frameIdx = 7;
  }

//fprintf(stderr, "aud prev:%llu pts:%llu tm0:%llu \n", pFrame->pts, pRtmp->aud.tmprev, pRtmp->aud.tm0);

  if(pFrame->pts > pRtmp->av.aud.tmprev) {
    ms = (pFrame->pts - pRtmp->av.aud.tmprev) / 90.0f;
  }
  pRtmp->av.aud.tmprev += (ms * 90);
  hdrsz = 8; // hdr size < 12 has relative timestamps

  //if(pFrame->pts > pRtmp->av.aud.tm0) {
  //  ms = (pFrame->pts - pRtmp->av.aud.tm0) / 90.0f;
  //}
  //pRtmp->av.aud.tmprev = pts;

  //TODO: check if the dts is actually a +/- offset from the pts, or an absolute time
  //pRtmp->av.aud.tmprev = pFrame->pts - pRtmp->av.aud.tm0;
  //tm = (uint32_t) (pRtmp->av.aud.tmprev / 90.0f);
  //tm = (pRtmp->av.aud.tmprev / 90.0f) - ms; hdrsz = 8; // hdr size < 12 has relative timestamps
*/

  bs.idx = hdrsz;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  //bs.buf[bs.idx++] = pRtmp->av.aud.u.aac.audHdr;
  //bs.buf[bs.idx++] = FLV_AUD_AAC_PKTTYPE_RAW;

  if((rc = flvsrv_create_audframe_aac(&bs, &pRtmp->av.aud, pFrame, &ms)) < 0) {
    return rc;
  }

  //pData = &pRtmp->out.buf[pRtmp->out.idx];
  lenHdr = bs.idx - hdrsz;
  lenData = rc;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
      //streamIdx, ms, 2 + pFrame->len - frameIdx, 
      streamIdx, ms, lenHdr + lenData,
      RTMP_CONTENT_TYPE_AUDDATA, htonl(0x01), hdrsz)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  //fprintf(stderr, "AUD RTMP len:%d tm:%d, pts:%.3f (%.3f)\n", pFrame->len, ms, PTSF(pFrame->pts), PTSF(pFrame->pts - pRtmp->av.aud.tm0));

  //rc = rtmp_send_chunkdata(pRtmp, streamIdx, &pFrame->pData[pFrame->len - lenData], 
  rc = rtmp_send_chunkdata(pRtmp, streamIdx, &OUTFMT_DATA(pFrame)[OUTFMT_LEN(pFrame) - lenData], 
                           //pFrame->len - frameIdx, 2);
                           lenData, lenHdr);

  //fprintf(stderr, "audframe[%d] len:%d sent:%d ms:%u, pts:%.3f (%.3f)\n", aframes++, pFrame->len, rc, ms, PTSF(pFrame->pts), PTSF(pFrame->pts - pRtmp->av.aud.tm0));

  return rc;
}

//static int delmeping;
//static struct timeval tv_connectdone;
//static struct timeval tv_connect;

static int rtmp_create_publishstart(RTMP_CTXT_T *pRtmp) {
  int rc = 0;

  rtmp_create_onstatus(pRtmp, RTMP_ONSTATUS_TYPE_DATASTART);

  LOG(X_DEBUG("rtmp state is published (out sz:%d)"), pRtmp->out.idx);

  return rc;
}

static int rtmp_create_playresponse(RTMP_CTXT_T *pRtmp) {
  int rc = 0;

  if(pRtmp->ishttptunnel) {
    rtmp_create_ping(pRtmp, 0x00, 1);
  }

  //rtmp_create_onstatus(pRtmp, RTMP_ONSTATUS_TYPE_PUBNOTIFY);
  rtmp_create_onstatus(pRtmp, RTMP_ONSTATUS_TYPE_RESET);
  if(!pRtmp->ishttptunnel) {
    rtmp_create_ping(pRtmp, 0x04, 1);
    rtmp_create_ping(pRtmp, 0, 1);      // stream begin
  }
  rtmp_create_onstatus(pRtmp, RTMP_ONSTATUS_TYPE_PLAYSTART);
  rtmp_create_notify(pRtmp);
  rtmp_create_notify_netstart(pRtmp);
  //rtmp_create_ping(pRtmp, 0x20, 1); // buffer ready

  if(pRtmp->ishttptunnel) {
    rtmp_create_ping(pRtmp, 0x20, 1); // buffer ready
  }

  //
  // Change the state to connected - ready to process vid/aud frames
  //
  pRtmp->state = RTMP_STATE_CLI_DONE;

  //gettimeofday(&tv_connectdone, NULL);
  LOG(X_DEBUG("rtmp state is playing"));

  return rc;
}

/*
static int find_h264_spspps(SPSPPS_RAW_T *pSpsPpsRaw, const unsigned char *pData, 
                            unsigned int len) {
  int rc = 0;
  XCODE_H264_OUT_T h264out;

  memset(&h264out, 0, sizeof(h264out));

  rc = xcode_h264_find_spspps(&h264out, pData, len, pSpsPpsRaw);

  return rc;
}
*/

int rtmp_addFrame(void *pArg, const OUTFMT_FRAME_DATA_T *pFrame) { 
  int rc = 0;
  RTMP_CTXT_T *pRtmp = (RTMP_CTXT_T *) pArg;
  OUTFMT_FRAME_DATA_T frame;
  int keyframe = 0;
  STREAMER_CFG_T *pStreamerCfg = NULL;

  if(!pArg || !pFrame) {
    return -1;
  } else if((pFrame->isvid && pFrame->mediaType != XC_CODEC_TYPE_H264) ||
            (pFrame->isaud && pFrame->mediaType != XC_CODEC_TYPE_AAC)) {
    pRtmp->state = RTMP_STATE_ERROR;
    LOG(X_ERROR("RTMP %scodec type %s not supported"), pFrame->isvid ? "video " : "audio ", 
         codecType_getCodecDescrStr(pFrame->mediaType));
    return -1;
  }

  pStreamerCfg = pRtmp->av.vid.pStreamerCfg ? pRtmp->av.vid.pStreamerCfg :  pRtmp->av.aud.pStreamerCfg;

  //if(pFrame->isvid) avc_dumpHex(stderr, pFrame->pData, 48, 1);
  //if(pFrame->isvid && pFrame->keyframe) fprintf(stderr, "KEYFRAME sps:%d pps:%d\n", OUTFMT_VSEQHDR(pFrame).h264.sps_len, OUTFMT_VSEQHDR(pFrame).h264.pps_len);

  if((pRtmp->isclient && pRtmp->state < RTMP_STATE_CLI_PUBLISH) || 
     (!pRtmp->isclient && pRtmp->state < RTMP_STATE_CLI_PLAY)) {

    //
    // Most likely rtmp_handle_conn is still processing RTMP connection setup
    //
    return rc;
  } else if((pFrame->isvid && pRtmp->novid) || (pFrame->isaud && pRtmp->noaud)) {
    return rc;
  }

  if(pFrame->isvid) {

    //
    // since pFrame is read-only, create a copy with the appropriate xcode out index
    //
    if(pRtmp->requestOutIdx != pFrame->xout.outidx) {
      memcpy(&frame, pFrame, sizeof(frame));
      frame.xout.outidx = pRtmp->requestOutIdx;
      pFrame = &frame;
    }
    keyframe = OUTFMT_KEYFRAME(pFrame);

  }

  if(OUTFMT_DATA(pFrame) == NULL) {
    LOG(X_ERROR("RTMP Frame output[%d] data not set"), pFrame->xout.outidx);
    pRtmp->state = RTMP_STATE_ERROR;
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_addFrame %s outidx[%d] len:%d pts:%.3f dts:%.3f key:%d, state: %d"), 
                pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), pFrame->xout.outidx, OUTFMT_LEN(pFrame), 
                PTSF(OUTFMT_PTS(pFrame)), PTSF(OUTFMT_DTS(pFrame)), OUTFMT_KEYFRAME(pFrame), pRtmp->state) );;

  if(pFrame->isvid && !pRtmp->av.vid.haveSeqHdr) {

    if((rc = flvsrv_check_vidseqhdr(&pRtmp->av.vid, pFrame, &keyframe)) <= 0) {

      //
      // Request an IDR from the underling encoder
      //
      if(pStreamerCfg) {
        streamer_requestFB(pStreamerCfg, pFrame->xout.outidx, ENCODER_FBREQ_TYPE_FIR, 0, REQUEST_FB_FROM_LOCAL);
      }

      return 0;
    }

  } else if(pFrame->isaud && !pRtmp->av.aud.haveSeqHdr) {

    if((rc = flvsrv_check_audseqhdr(&pRtmp->av.aud, pFrame)) <= 0) {
      return 0;
    }

  }

  if(!pRtmp->av.vid.sentSeqHdr && (pRtmp->novid || pRtmp->av.vid.haveSeqHdr) && 
     (pRtmp->noaud || pRtmp->av.aud.haveSeqHdr)) {

    pRtmp->out.idx = 0;

    VSX_DEBUG_RTMP( 
        LOG(X_DEBUG("RTMP - rtmp_addFrame creating %s"), pRtmp->isclient ? "data start" : "play response") );

    if(pRtmp->isclient) {

      if((rc = rtmp_create_publishstart(pRtmp)) < 0) {
        return rc;
      }
      if((rc = rtmp_create_setDataFrame(pRtmp)) < 0) {
        return rc;
      }

    } else {
      if((rc = rtmp_create_playresponse(pRtmp)) < 0) {
        return rc;
      }

      if((rc = rtmp_create_onmeta(pRtmp)) < 0) {
        return rc;
      }

    }

    if(rc >= 0 && !pRtmp->novid) {
      if((rc = rtmp_create_vidstart(pRtmp)) < 0) {
        LOG(X_ERROR("Failed to create rtmp video start sequence"));
      }
    }
    if(rc >= 0 && !pRtmp->noaud) {
      if((rc = rtmp_create_audstart(pRtmp)) < 0) {
        LOG(X_ERROR("Failed to create rtmp audio start sequence"));
      }
    }

    if(rc >= 0 && (rc = rtmp_send(pRtmp, "addFrame")) < 0) {
      return -1;
    }

    if(rc >= 0) {
      pRtmp->av.vid.sentSeqHdr = 1;
      LOG(X_DEBUG("rtmp play got audio / video start - beginning transmission"));
    }
  }

  if(rc < 0) {
    pRtmp->state = RTMP_STATE_ERROR;
    return rc;
  } else if(!pRtmp->av.vid.sentSeqHdr) {
    return rc;
  }

  pRtmp->out.idx = 0;
  if(pFrame->isvid) {

    //
    // Only start sending video content beginning w/ a keyframe
    //
    if(!pRtmp->av.vid.haveKeyFrame) {
      if(keyframe) {
        pRtmp->av.vid.haveKeyFrame = 1;
        pRtmp->av.vid.nonKeyFrames = 0;
      } else {
        codec_nonkeyframes_warn(&pRtmp->av.vid, "RTMP");
        //pRtmp->av.vid.nonKeyFrames++;
        //if(pRtmp->av.vid.nonKeyFrames % 301 == 300) {
        //  LOG(X_WARNING("FLV/RTMP video output no key frame detected after %d video frames"), 
        //    pRtmp->av.vid.nonKeyFrames);
        //}
      }

    }

    if(pRtmp->av.vid.haveKeyFrame) {
      //if(pFrame->keyframe) fprintf(stderr, "KEYFRAME sps:%d pps:%d\n", OUTFMT_VSEQHDR(pFrame).h264.sps_len, OUTFMT_VSEQHDR(pFrame).h264.pps_len);
      //avc_dumpHex(stderr, pFrame->pData, 16, 1);

      rc = rtmp_send_vidframe(pRtmp, pFrame, keyframe);
    }

  } else if(pFrame->isaud) {
  
    if(pRtmp->av.aud.tm0 == 0) {
      //pRtmp->av.aud.tm0 = pFrame->pts;
      pRtmp->av.aud.tm0 = OUTFMT_PTS(pFrame);
      pRtmp->av.aud.tmprev = pRtmp->av.aud.tm0;
    }

    rc = rtmp_send_audframe(pRtmp, pFrame);
  }

  if(rc < 0) {
    pRtmp->state = RTMP_STATE_ERROR;
  }

  return rc;
}

int rtmp_handle_conn(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  long idlems = 0;
  struct timeval tv;
  AUTH_LIST_T authList;
  const char *p = NULL;
  char tmp[128];
  char buf[4096];

  pRtmp->state = RTMP_STATE_CLI_START;
  pRtmp->rtmpt.pbuftmp = (unsigned char *) buf;
  pRtmp->rtmpt.szbuftmp = sizeof(buf);

  //
  // Accept handshake
  //
  if((rc = rtmp_handshake_srv(pRtmp)) < 0) {
    LOG(X_ERROR("RTMP handshake failed for %s:%d"),
        FORMAT_NETADDR(pRtmp->pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pRtmp->pSd->sa)));
    return rc;
  }

  //
  // Read connect packet
  // 
  do {
    if((rc = rtmp_parse_readpkt_full(pRtmp, 1, NULL)) < 0 || 
      (pRtmp->state != RTMP_STATE_CLI_CONNECT && 
       pRtmp->pkt[pRtmp->streamIdx].hdr.contentType != RTMP_CONTENT_TYPE_CHUNKSZ)) {
      LOG(X_ERROR("Failed to read rtmp connect request.  State: %d"), pRtmp->state); 
      return rc;
    }
  } while(pRtmp->state != RTMP_STATE_CLI_CONNECT);


  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - sending connect response ct: %d, methodParsed: %d, state: %d"), 
         pRtmp->contentTypeLastInvoke, pRtmp->methodParsed, pRtmp->state) );

  //
  // Create server response
  //
  pRtmp->out.idx = 0;

  if(pRtmp->pAuthTokenId && pRtmp->pAuthTokenId[0] != '\0') {
    if(rtmp_auth_parse_app(pRtmp->connect.app, &authList) < 0 || 
       !(p = conf_find_keyval(authList.list,  VSX_URI_TOKEN_QUERY_PARAM)) ||
      strcmp(pRtmp->pAuthTokenId, p)) {

      rtmp_create_error(pRtmp, 1, RTMP_NETCONNECT_REJECTED, HTTP_STATUS_STR_FORBIDDEN);
      rtmp_send(pRtmp, "rtmp_handle_conn server initial response");
      LOG(X_WARNING("Missing or invalid RTSP security token for app: '%s'"), pRtmp->connect.app);
      return -1;
    }
  }

  rtmp_create_serverbw(pRtmp, 2500000);
  rtmp_create_clientbw(pRtmp, 2500000);
  rtmp_create_ping(pRtmp, 0, 0);
  pRtmp->chunkSzOut = RTMP_CHUNK_SZ_OUT;
  //pRtmp->chunkSzOut = 128;
  rtmp_create_chunksz(pRtmp);
  rtmp_create_result_invoke(pRtmp);

  if((rc = rtmp_send(pRtmp, "rtmp_handle_conn server initial response")) < 0) {
    return -1;
  }

  pRtmp->rtmpt.doQueueOut = 1; pRtmp->rtmpt.doQueueIdle = 0;

  do {

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - waiting for createStream ct: %d, methodParsed: %d, state: %d"), 
           pRtmp->contentTypeLastInvoke, pRtmp->methodParsed, pRtmp->state) );

    //
    // Createstream
    //
    if((rc = rtmp_parse_readpkt_full(pRtmp, 1, NULL)) < 0) {
      LOG(X_ERROR("Failed to read rtmp packet.  method:%d"), pRtmp->methodParsed); 
      return rc;
    }

    if(pRtmp->methodParsed == RTMP_METHOD_PING ||
       pRtmp->methodParsed == RTMP_METHOD_SERVERBW) {
      VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - method received was: 0x%x"), pRtmp->methodParsed); );

      if(pRtmp->ishttptunnel && pRtmp->rtmpt.idxInContentLen >= pRtmp->rtmpt.contentLen) {
        //
        // Respond with 200 OK right away since the client may not bundle a Connect request
        //
        rtmpt_send(pRtmp, NULL, 0, "rtmp_handle_conn wait for createstream");
      }

      continue;

    } else if(pRtmp->methodParsed == RTMP_METHOD_RELEASESTREAM ||
              pRtmp->methodParsed == RTMP_METHOD_FCPUBLISH) {
      //
      // No server response for releaseStream or FCPublish
      //
      VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - method received was: 0x%x"), pRtmp->methodParsed); );
      continue;
    } else if(pRtmp->methodParsed == RTMP_METHOD_CREATESTREAM) {
      VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - method received was: 0x%x (createStream)")); );
      break;
    } else {
      LOG(X_ERROR("RTMP did not receive expected createStream packet.  method:%d"), pRtmp->methodParsed); 
      return -1;
    }

  } while(1);

  pRtmp->rtmpt.doQueueOut = 0; pRtmp->rtmpt.doQueueIdle = 0;

  if(pRtmp->state != RTMP_STATE_CLI_CREATESTREAM) {
    LOG(X_ERROR("Did not receive rtmp createstream request.  State: 0x%x"), pRtmp->state);
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - sending createStream response ct: %d, methodParsed: %d, state: %d"), 
           pRtmp->contentTypeLastInvoke, pRtmp->methodParsed, pRtmp->state) );

  //
  // Create createstream result
  // 
  pRtmp->out.idx = 0;
  rtmp_create_result(pRtmp);
  if((rc = rtmp_send(pRtmp, "rtmp_handle_conn createstream response")) < 0) {
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - waiting for play/publish ct: %d, methodParsed: %d, state: %d"), 
           pRtmp->contentTypeLastInvoke, pRtmp->methodParsed, pRtmp->state) );

  pRtmp->rtmpt.doQueueOut = 1; pRtmp->rtmpt.doQueueIdle = 0;

  //
  // Get RTMP play
  //
  do {
    if(rtmp_parse_readpkt_full(pRtmp, 1, NULL) < 0) {
      LOG(X_ERROR("Failed to read rtmp packet header")); 
      return -1;
    } else if(pRtmp->state != RTMP_STATE_CLI_PLAY && 
      pRtmp->pkt[pRtmp->streamIdx].hdr.contentType == RTMP_CONTENT_TYPE_INVOKE) {

      LOG(X_ERROR("Did not receive rtmp play request.  State: %d, method: %d"), pRtmp->state, pRtmp->methodParsed);

      if(pRtmp->methodParsed == RTMP_METHOD_PUBLISH) {
        pRtmp->out.idx = 0;
        rtmp_create_error(pRtmp, 0, NULL, "publish not available when in server mode");
        if((rc = rtmp_send(pRtmp, "rtmp_handle_conn play error response")) < 0) {
          return rc;
        }
      }
      return -1;

    } else if(pRtmp->methodParsed == RTMP_METHOD_PING ||
              pRtmp->methodParsed == RTMP_METHOD_SERVERBW) {

      VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - method received was: 0x%x"), pRtmp->methodParsed); );

      if(pRtmp->ishttptunnel && pRtmp->rtmpt.idxInContentLen >= pRtmp->rtmpt.contentLen) {
        //
        // Respond with 200 OK right away since the client may not bundle a play request
        //
        rtmpt_send(pRtmp, NULL, 0, "rtmp_handle_conn wait for play");
      }

    }

  } while(pRtmp->state != RTMP_STATE_CLI_PLAY);

  if(pRtmp->ishttptunnel) {
    //
    // Respond with 200 OK right away
    //
    rtmpt_send(pRtmp, NULL, 0, "rtmp_handle_conn play done");
    //pRtmp->rtmpt.doQueueOut = 1; // do not respond to idle requests from srv_recv handler
    pRtmp->rtmpt.doQueueOut = 1; pRtmp->rtmpt.doQueueIdle = 1;
  }

  //
  // Obtain the requested xcode output outidx 
  //
  if((pRtmp->requestOutIdx = outfmt_getoutidx(pRtmp->connect.app, NULL)) < 0 ||
     pRtmp->requestOutIdx >= IXCODE_VIDEO_OUT_MAX) {
    pRtmp->requestOutIdx = 0;
  } else if(pRtmp->requestOutIdx > 0) {
    LOG(X_DEBUG("Set RTMP output format index to[%d] app:'%s', play:'%s', for %s:%d"), pRtmp->requestOutIdx,  
                pRtmp->connect.app, pRtmp->connect.play,  
                FORMAT_NETADDR(pRtmp->pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pRtmp->pSd->sa)));
  }

  //fprintf(stderr, "PLAY RESP TO: '%s', APP:'%s' outfmt idx;%d\n", pRtmp->connect.play, pRtmp->connect.app, pRtmp->requestOutIdx);

  //pRtmp->out.idx = 0;
  //rtmp_create_onstatus(pRtmp, RTMP_ONSTATUS_TYPE_PUBNOTIFY);
  //if((rc = netio_send(&pRtmp->pSd->netsocket, &pRtmp->pSd->sain, pRtmp->out.buf,
  //                    pRtmp->out.idx)) < 0) {
  //  return -1;
  //}


  //if(net_setsocknonblock(pRtmp->pSd->socket, 1) < 0) {
  //  return -1;
  //}

  pRtmp->rtmpt.httpTmtMs = 0;
  TIME_TV_SET(tv, pRtmp->tvLastRd);

  //
  // Read any RTMP packets from the client
  //
  while(pRtmp->state >= RTMP_STATE_CLI_PLAY) {

    if((rc = rtmp_parse_readpkt_full(pRtmp, 0, NULL)) < 0) {
      LOG(X_ERROR("Failed to read rtmp packet from client"));
      break;
    } 

    if(tv.tv_sec != pRtmp->tvLastRd.tv_sec ||
       tv.tv_usec != pRtmp->tvLastRd.tv_usec) {
      TIME_TV_SET(tv, pRtmp->tvLastRd);
      idlems = 0;
    } else {
      gettimeofday(&tv, NULL);
      idlems = TIME_TV_DIFF_MS(tv, pRtmp->tvLastRd);
      if(idlems > RTMP_RCV_IDLE_TMT_MS) {
        LOG(X_ERROR("Closing RTMP client connection from %s:%d after %d/%d ms idle timeout"), 
             FORMAT_NETADDR(pRtmp->pSd->sa, tmp, sizeof(tmp)), 
             ntohs(INET_PORT(pRtmp->pSd->sa)), idlems, RTMP_RCV_IDLE_TMT_MS);
        break;
      }
    }
    //fprintf(stderr, "PARSE_READPK DONE rc:%d idlems:%d\n",rc, idlems);

  }

  LOG(X_DEBUG("RTMP %sconnection ending with state: %d"), pRtmp->ishttptunnel ? "tunneled " : "", pRtmp->state);
  pRtmp->state = RTMP_STATE_DISCONNECTED;

  return rc;
}

int rtmp_init(RTMP_CTXT_T *pRtmp, unsigned int vidTmpFrameSz) {
  int rc;

  if((rc = rtmp_parse_init(pRtmp, 0x2000, 0x2000)) < 0) {
    return rc;
  }

  pRtmp->state = RTMP_STATE_INVALID;
  pRtmp->cbRead = rtmp_cbReadDataNet;
  pRtmp->pCbData = pRtmp;

  // Default values
  //pRtmp->advObjEncoding = 1.0;
  pRtmp->advObjEncoding = 3.0;
  //pRtmp->advObjEncoding = 0;
  pRtmp->advCapabilities = 31.0;

  if(vidTmpFrameSz == 0) {
    vidTmpFrameSz = STREAMER_RTMPQ_SZSLOT_MAX;
  }
  if((codecfmt_init(&pRtmp->av, vidTmpFrameSz, 0)) < 0) {
  //pRtmp->av.vid.tmpFrame.sz = vidTmpFrameSz;
  //if((pRtmp->av.vid.tmpFrame.buf = (unsigned char *) avc_calloc(1, pRtmp->av.vid.tmpFrame.sz)) == NULL) {
    rtmp_close(pRtmp);
    return -1; 
  }

  return 0;
}

void rtmp_close(RTMP_CTXT_T *pRtmp) {


  if(pRtmp) {

    if(pRtmp->ishttptunnel) {
      if(pRtmp->rtmpt.pbufdyn) {
        avc_free((void *) &pRtmp->rtmpt.pbufdyn);
        pRtmp->rtmpt.szbufdyn = 0;
      } 
    }

    rtmp_parse_close(pRtmp);
    codecfmt_close(&pRtmp->av);
    pRtmp->state = RTMP_STATE_INVALID;
  }

}

