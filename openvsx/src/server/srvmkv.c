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


int mkvsrv_sendHttpResp(MKVSRV_CTXT_T *pMkvCtxt, const char *contentType) {
  int rc;
  FILE_OFFSET_T lenLive = 0;

  if(!contentType) {
    //TODO: this should be auto-set based on audio & video codecs
    contentType = CONTENT_TYPE_WEBM;
  }

  http_log(pMkvCtxt->pSd, pMkvCtxt->pReq, HTTP_STATUS_OK, lenLive);

  if((rc = http_resp_sendhdr(pMkvCtxt->pSd, pMkvCtxt->pReq->version, HTTP_STATUS_OK,
                   lenLive, contentType, 
                   http_getConnTypeStr(pMkvCtxt->pReq->connType),
                   pMkvCtxt->pReq->cookie, NULL, NULL, NULL, NULL)) < 0) {
    pMkvCtxt->state = FLVSRV_STATE_ERROR;
    return rc;
  }

  return rc;
}

static int mkv_create_vidtrack(const MKVSRV_CTXT_T *pMkvCtxt, MKV_TRACK_T *pTrack, int trackNum) {
  int rc = 0;

  pTrack->number = trackNum;
  pTrack->uid = pTrack->number;
  pTrack->type = MKV_TRACK_TYPE_VIDEO;
  //
  // Need a frame average duration
  //
  if(pMkvCtxt->av.vid.ctxt.frameDeltaHz > 0 &&
     pMkvCtxt->av.vid.ctxt.clockHz > 0) {
    pTrack->defaultDuration = ((float)pMkvCtxt->av.vid.ctxt.frameDeltaHz * 1000 / 
                              pMkvCtxt->av.vid.ctxt.clockHz) * 1000000;
  } else {
    pTrack->defaultDuration = 33333333; // 30fps
    //pTrack->defaultDuration = 40048057; // 24.97fps
  }

  pTrack->timeCodeScale = 1.0f;
  strncpy(pTrack->language, "eng", sizeof(pTrack->language) - 1);
  strncpy(pTrack->codecId, mkv_getCodecStr(pMkvCtxt->av.vid.codecType), 
          sizeof(pTrack->codecId) - 1);
  pTrack->codecPrivate.nondynalloc = 1;
  pTrack->codecPrivate.p = (void *) pMkvCtxt->av.vid.ctxt.pstartHdrs;
  pTrack->codecPrivate.size = pMkvCtxt->av.vid.ctxt.startHdrsLen;
  pTrack->video.pixelWidth = pMkvCtxt->av.vid.ctxt.width;
  pTrack->video.pixelHeight = pMkvCtxt->av.vid.ctxt.height;

  //TODO: these appear to be present 
//  pTrack->video.displayWidth = pTrack->video.pixelWidth; 
//pTrack->video.displayWidth = 426;
//  pTrack->video.displayHeight = pTrack->video.pixelHeight;
//  pTrack->video.displayUnit = 3;


//pTrack->video.pixelWidth = 400;
//pTrack->video.pixelHeight = 240;
//pTrack->video.displayWidth = 426;
//pTrack->video.displayHeight = 240;
//pTrack->video.displayUnit = 3;
  
  return rc;
}

static int mkv_create_audtrack(const MKVSRV_CTXT_T *pMkvCtxt, MKV_TRACK_T *pTrack, int trackNum) {
  int rc = 0;

  pTrack->number = trackNum;
  pTrack->uid = pTrack->number;
  pTrack->type = MKV_TRACK_TYPE_AUDIO;
  pTrack->defaultDuration = 0;
//pTrack->defaultDuration = 24 * 1000000;
  pTrack->timeCodeScale = 1.0f;
  strncpy(pTrack->language, "eng", sizeof(pTrack->language) - 1);
  strncpy(pTrack->codecId, mkv_getCodecStr(pMkvCtxt->av.aud.codecType), 
          sizeof(pTrack->codecId) - 1);

  pTrack->audio.samplingFreq = pMkvCtxt->av.aud.ctxt.clockHz*1;
  pTrack->audio.samplingOutFreq = 0; // pMkvCtxt->av.aud.ctxt.clockHz*1;
  pTrack->audio.channels = pMkvCtxt->av.aud.ctxt.channels*1;
  pTrack->audio.bitDepth = 16;
  pTrack->codecPrivate.nondynalloc = 1;
  pTrack->codecPrivate.p = (void *) pMkvCtxt->av.aud.ctxt.pstartHdrs;
  pTrack->codecPrivate.size = pMkvCtxt->av.aud.ctxt.startHdrsLen;

  return rc;
}

#define MKV_TRACK_VID(pMkv) (&(pMkv)->mkvTracks[0])
#define MKV_TRACK_AUD(pMkv) (pMkv)->mkvTracks[1].number ? (&(pMkv)->mkvTracks[1]) : \
                             (&(pMkv)->mkvTracks[0])

static int mkvsrv_sendStart(MKVSRV_CTXT_T *pMkvCtxt) {
  int rc = 0;
  EBML_BYTE_STREAM_T mbs;
  unsigned int idx;
  char tmp[128];
  unsigned char buf[8198];
  const char *docType;

  if(pMkvCtxt->av.vid.codecType == XC_CODEC_TYPE_VP8 ||
     pMkvCtxt->av.aud.codecType == XC_CODEC_TYPE_VORBIS) {
    docType = MKV_DOCTYPE_WEBM;
  } else {
    docType = MKV_DOCTYPE_MATROSKA;
  }

  memset(&mbs, 0, sizeof(mbs));

  mbs.bs.buf = buf;
  mbs.bs.sz = sizeof(buf);
  mbs.bufsz = mbs.bs.sz;

  memset(&pMkvCtxt->mkvHdr, 0, sizeof(pMkvCtxt->mkvHdr));
  pMkvCtxt->mkvHdr.version = EBML_VERSION;
  pMkvCtxt->mkvHdr.readVersion = EBML_VERSION;
  pMkvCtxt->mkvHdr.maxIdLength = EBML_MAX_ID_LENGTH;
  pMkvCtxt->mkvHdr.maxSizeLength = EBML_MAX_SIZE_LENGTH;
  //pMkvCtxt->mkvHdr.docVersion = EBML_VERSION;
  //pMkvCtxt->mkvHdr.docReadVersion = EBML_VERSION;
  pMkvCtxt->mkvHdr.docVersion = 2; 
  pMkvCtxt->mkvHdr.docReadVersion = 2;
  strcpy(pMkvCtxt->mkvHdr.docType, docType);

  //
  // Serialize the MKV header
  //
  if((rc = mkv_write_header(&pMkvCtxt->mkvHdr, &mbs)) < 0) {
    LOG(X_ERROR("Failed to create MKV header"));
    return rc;
  }

  memset(&pMkvCtxt->mkvSegment, 0, sizeof(pMkvCtxt->mkvSegment));
  //
  // in ns (1 ms : all timecodes in the segment are expressed in ms)
  //
  pMkvCtxt->mkvSegment.info.timeCodeScale = 1000000; 
  //
  // Need a non-zero duration for webm play (1 week)
  // In chrome, if the playout point exceeds this duration, then the frame rate
  // drops alot.
  //
  //pMkvCtxt->mkvSegment.info.duration = 604800000.0f;
  pMkvCtxt->mkvSegment.info.duration = MKV_SEGINFO_DURATION_DEFAULT;
  //pMkvCtxt->mkvSegment.info.duration = 30047.0f;
  //pMkvCtxt->mkvSegment.info.duration = 0;

  strncpy(pMkvCtxt->mkvSegment.info.title, "", sizeof(pMkvCtxt->mkvSegment.info.title) - 1);
  strncpy(pMkvCtxt->mkvSegment.info.muxingApp, vsxlib_get_appnamestr(tmp, sizeof(tmp)),
          sizeof(pMkvCtxt->mkvSegment.info.muxingApp) - 1);
  strncpy(pMkvCtxt->mkvSegment.info.writingApp, vsxlib_get_appnamestr(tmp, sizeof(tmp)), 
          sizeof(pMkvCtxt->mkvSegment.info.writingApp) - 1);
  for(idx = 0; idx < sizeof(pMkvCtxt->mkvSegment.info.segUid) / sizeof(uint16_t); idx++) {
    *((uint16_t *) &pMkvCtxt->mkvSegment.info.segUid[idx]) = random() % RAND_MAX;
  }

  pMkvCtxt->mkvSegment.tracks.arrTracks.parr = pMkvCtxt->mkvTracks;
  pMkvCtxt->mkvSegment.tracks.arrTracks.nondynalloc = 1;
  pMkvCtxt->mkvSegment.tracks.arrTracks.count = pMkvCtxt->mkvTracks[1].number ? 2 : 1;

  if((rc = mkv_write_segment(&pMkvCtxt->mkvSegment, &mbs)) < 0) {
    LOG(X_ERROR("Failed to create MKV header segment"));
    return rc;
  }

  pMkvCtxt->writeErrDescr = "body start";
  if((rc = pMkvCtxt->cbWrite(pMkvCtxt, mbs.bs.buf, mbs.bs.idx)) < 0) {
    return rc;
  }

  pMkvCtxt->senthdr = 1;

  return rc;
}

static int mkvsrv_create_vidframe(MKVSRV_CTXT_T *pMkvCtxt, const OUTFMT_FRAME_DATA_T *pFrame,
                                  int keyframe, unsigned char **ppData) {
  int rc = 0;

  switch(pMkvCtxt->av.vid.codecType) {
    case XC_CODEC_TYPE_H264:
      if((rc = flvsrv_create_vidframe_h264(NULL, &pMkvCtxt->av.vid, pFrame, 
                                       NULL, NULL, keyframe)) < 0) {
        return rc;
      }
      *ppData =  &pMkvCtxt->av.vid.tmpFrame.buf[pMkvCtxt->av.vid.tmpFrame.idx];

      break;
    case XC_CODEC_TYPE_VP8:
      rc = OUTFMT_LEN(pFrame);
      *ppData = OUTFMT_DATA(pFrame);
      break;
    default:
      rc = -1;
      break;
  }

  return rc;
}

static int mkvsrv_create_audframe(MKVSRV_CTXT_T *pMkvCtxt, const OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;

  switch(pMkvCtxt->av.aud.codecType) {
    case XC_CODEC_TYPE_AAC:
      if((rc = flvsrv_create_audframe_aac(NULL, &pMkvCtxt->av.aud, pFrame,
                                        NULL)) < 0) {
        return rc;
      }

      break;
    case XC_CODEC_TYPE_VORBIS:
      rc = OUTFMT_LEN(pFrame);
      break;
    default:
      rc = -1;
      break;
  }

  return rc;
}

static int mkvsrv_send_frame(MKVSRV_CTXT_T *pMkvCtxt, 
                             const OUTFMT_FRAME_DATA_T *pFrame, int keyframe) {
  int rc = 0;
  unsigned char *pData = NULL;
  unsigned int lenData = 0;
  int16_t blockTime;
  int64_t tm;
  EBML_BYTE_STREAM_T mbs;
  const MKV_TRACK_T *pTrack = NULL;
  int startCluster = 0;
  int clusterElapsedPts;
  unsigned char buf[128];
  
  memset(&mbs, 0, sizeof(mbs));
  mbs.bs.buf = buf;
  mbs.bs.sz = sizeof(buf);
  mbs.bufsz = mbs.bs.sz;

  if(pFrame->isvid) {

    if((rc = mkvsrv_create_vidframe(pMkvCtxt, pFrame, keyframe, &pData)) < 0) {
      return rc;
    }
    lenData = rc;
    pTrack = MKV_TRACK_VID(pMkvCtxt);

  //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), MIN(16, OUTFMT_LEN(pFrame)), 1);

  } else if(pFrame->isaud) {

    if((rc = mkvsrv_create_audframe(pMkvCtxt, pFrame)) < 0) {
      return rc;
    }
    //fprintf(stderr, "mvk aud tslast:%lld ts:%lld\n", pMkvCtxt->av.aud.tsLastWrittenFrame, OUTFMT_PTS(pFrame));
    if(OUTFMT_PTS(pFrame) < pMkvCtxt->av.aud.tsLastWrittenFrame) {
      LOG(X_WARNING("MKV output audio ts went backwards %lld -> %lld"), 
          pMkvCtxt->av.aud.tsLastWrittenFrame, OUTFMT_PTS(pFrame));
      return 0;
    }

    pMkvCtxt->av.aud.tsLastWrittenFrame = OUTFMT_PTS(pFrame);

    lenData = rc;
    pData = &OUTFMT_DATA(pFrame)[OUTFMT_LEN(pFrame) - lenData];
    pTrack = MKV_TRACK_AUD(pMkvCtxt);

  }

  if(lenData <= 0) {
    return 0;
  }

  //avc_dumpHex(stderr, pData, MIN(16, lenData), 1);

  if(!pMkvCtxt->havePtsStart) {
    pMkvCtxt->ptsStart = OUTFMT_PTS(pFrame);
    pMkvCtxt->havePtsStart = 1;
  }

  clusterElapsedPts = (OUTFMT_PTS(pFrame) - pMkvCtxt->lastPtsClusterStart);

  if(pMkvCtxt->clusterSz == 0) {
    startCluster = 1;
  } else if(!(*pMkvCtxt->pnovid)) { 

    if(pFrame->isvid && keyframe && clusterElapsedPts > pMkvCtxt->clusterDurationMs * 90) {
    //if(pFrame->isvid && keyframe && clusterElapsedPts > 100 * 90) {
      startCluster = 1;
    }
  } else if(clusterElapsedPts > pMkvCtxt->clusterDurationMs * 90) {
    startCluster = 1;
  }

  if(startCluster) {

    pMkvCtxt->lastPtsClusterStart = OUTFMT_PTS(pFrame);
    pMkvCtxt->clusterSz = 0;
    pMkvCtxt->clusterTimeCode = PTSF_MS(pMkvCtxt->lastPtsClusterStart - pMkvCtxt->ptsStart);

//fprintf(stderr, "CLUSTER START pts:%.3f, cluster timeCode:%llu\n", PTSF(OUTFMT_PTS(pFrame)), pMkvCtxt->clusterTimeCode);
    ebml_write_id(&mbs, MKV_ID_CLUSTER);
    ebml_write_num(&mbs, EBML_MAX_NUMBER, 8);
    ebml_write_uint(&mbs, MKV_ID_CLUSTERTIMECODE, pMkvCtxt->clusterTimeCode);

  }

  tm = OUTFMT_PTS(pFrame) - pMkvCtxt->lastPtsClusterStart;
  tm += OUTFMT_DTS(pFrame);
//if(tm < 0) tm = 0;
  blockTime = PTSF_MS(tm);

  ebml_write_id(&mbs, MKV_ID_SIMPLEBLOCK);
  ebml_write_num(&mbs, lenData + 4, 0);

  ebml_write_num(&mbs, pTrack->number, 1);
  *((int16_t *) &mbs.bs.buf[mbs.bs.idx]) = htons( blockTime );
  mbs.bs.idx += 2;
  mbs.bs.buf[mbs.bs.idx++] = (pFrame->isaud || keyframe) ? 0x80 : 0;

  //fprintf(stderr, "MKV send %s frame size:%d pts:%.3f, dts:%.3f, keyframe:%d, lenData:%d, blockTm:%d, clusterTm:%lld, track:%d\n", pFrame->isvid ? "vid" : pFrame->isaud ? "aud" : "unknown", OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), PTSF(OUTFMT_DTS(pFrame)),  keyframe, lenData, blockTime, pMkvCtxt->clusterTimeCode, pTrack->number);
  //avc_dumpHex(stderr, mbs.bs.buf, mbs.bs.idx, 1);

  if((rc = pMkvCtxt->cbWrite(pMkvCtxt, mbs.bs.buf, mbs.bs.idx)) < 0) {
    return rc;
  }

  if((rc = pMkvCtxt->cbWrite(pMkvCtxt, pData, lenData)) < 0) {
    return rc;
  }

  //fprintf(stderr, "MKV send ok\n");

  pMkvCtxt->lastPts = OUTFMT_PTS(pFrame); 
  pMkvCtxt->clusterSz += lenData + mbs.bs.idx;

  return rc;
}

int mkvsrv_addFrame(void *pArg, const OUTFMT_FRAME_DATA_T *pFrame) { 
  int rc = 0;
  MKVSRV_CTXT_T *pMkvCtxt = (MKVSRV_CTXT_T *) pArg;
  OUTFMT_FRAME_DATA_T frame;
  int keyframe = 0;

  //fprintf(stderr, "mkvsrv_addFrame isvid:%d, isaud:%d, pts:%.3f\n", pFrame->isvid, pFrame->isaud, PTSF(OUTFMT_PTS(pFrame)));

  if(!pArg || !pFrame) {
    return -1;
  } else if((pFrame->isvid && (pFrame->mediaType != XC_CODEC_TYPE_H264 &&
                               pFrame->mediaType != XC_CODEC_TYPE_VP8)) ||
            (pFrame->isaud && (pFrame->mediaType != XC_CODEC_TYPE_AAC &&
                               pFrame->mediaType != XC_CODEC_TYPE_VORBIS))) {
    LOG(X_ERROR("MKV %scodec type %s not supported"), pFrame->isvid ? "video " : "audio ", 
         codecType_getCodecDescrStr(pFrame->mediaType));
    pMkvCtxt->state = MKVSRV_STATE_ERROR;
    return -1;
  }

  //if(pMkvCtxt->av.vid.pStreamerCfg->status.faoffset_mkvpktz != 0) {
  //  LOG(X_DEBUG("MKV PKTZ OFFSET: %.3f"), pMkvCtxt->av.vid.pStreamerCfg->status.faoffset_mkvpktz);
  //}

  //LOG(X_DEBUG("mksrv_addFrame %s len:%d pts:%.3f dts:%.3f key:%d (vseq:%d, aseq:%d, sent:%d)"), pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), PTSF(OUTFMT_PTS(pFrame)), OUTFMT_KEYFRAME(pFrame), pMkvCtxt->av.vid.haveSeqHdr, pMkvCtxt->av.aud.haveSeqHdr, pMkvCtxt->av.vid.sentSeqHdr); //LOGHEX_DEBUG(OUTFMT_DATA(pFrame), MIN(OUTFMT_LEN(pFrame), 16));
//if(OUTFMT_KEYFRAME(pFrame)) fprintf(stderr, "\n\n\n\n\n");
  
  //if(pFrame->isvid && pFrame->keyframe) fprintf(stderr, "KEYFRAME sps:%d pps:%d\n", OUTFMT_VSEQHDR(pFrame).h264.sps_len, OUTFMT_VSEQHDR(pFrame).h264.pps_len);

  if((pFrame->isvid && *pMkvCtxt->pnovid) || (pFrame->isaud && *pMkvCtxt->pnoaud)) {
    return rc;
  } 

  if(pFrame->isvid) {

    //
    // since pFrame is read-only, create a copy with the appropriate xcode out index
    //
    if(pMkvCtxt->requestOutIdx != pFrame->xout.outidx || pMkvCtxt->faoffset_mkvpktz < 0) {

      memcpy(&frame, pFrame, sizeof(frame));
      frame.xout.outidx = pMkvCtxt->requestOutIdx;
      OUTFMT_PTS(&frame) += (pMkvCtxt->faoffset_mkvpktz * -90000);
      pFrame = &frame;
    }

    keyframe = OUTFMT_KEYFRAME(pFrame);

    //fprintf(stderr, "mkvsrv_addFrame %s len:%d pts:%llu dts:%lld key:%d (vseq:%d, aseq:%d, sent:%d)\n", pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), OUTFMT_LEN(pFrame), OUTFMT_PTS(pFrame), OUTFMT_DTS(pFrame), keyframe, pMkvCtxt->av.vid.haveSeqHdr, pMkvCtxt->av.aud.haveSeqHdr, pMkvCtxt->av.vid.sentSeqHdr); //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), 48, 1);

  } else {

    if(pMkvCtxt->faoffset_mkvpktz > 0) {
      memcpy(&frame, pFrame, sizeof(frame));
      OUTFMT_PTS(&frame) += (pMkvCtxt->faoffset_mkvpktz * 90000);
      pFrame = &frame;
    }

  }

  if(OUTFMT_DATA(pFrame) == NULL) {
    LOG(X_ERROR("MKV Frame output[%d] data not set"), pFrame->xout.outidx);
    return -1;
  }

  if(pFrame->isvid && !pMkvCtxt->av.vid.haveSeqHdr) {

    if((rc = flvsrv_check_vidseqhdr(&pMkvCtxt->av.vid, pFrame, &keyframe)) <= 0) {

      //
      // Request an IDR from the underlying encoder
      //
      if(pMkvCtxt->av.vid.pStreamerCfg) {
        streamer_requestFB(pMkvCtxt->av.vid.pStreamerCfg, pFrame->xout.outidx, ENCODER_FBREQ_TYPE_FIR, 0, 
                           REQUEST_FB_FROM_LOCAL);
      }
      
      return rc;
    }

/*
pMkvCtxt->av.aud.codecType=XC_CODEC_TYPE_VORBIS;
pMkvCtxt->av.aud.tm0 = 0;
pMkvCtxt->av.aud.haveSeqHdr = 1;
pMkvCtxt->av.aud.ctxt.clockHz = 22050;
pMkvCtxt->av.aud.ctxt.channels = 1;
pMkvCtxt->av.aud.ctxt.pstartHdrs = pMkvCtxt->av.aud.pStreamerCfg->xcode.aud.phdr;
pMkvCtxt->av.aud.ctxt.startHdrsLen = pMkvCtxt->av.aud.pStreamerCfg->xcode.aud.hdrLen;
*/

    //fprintf(stderr, "check vidseqhdr: %d\n", rc);
//fprintf(stderr, "avcc raw len:%d\n", pMkvCtxt->av.vid.codecCtxt.h264.avccRawLen); if(pMkvCtxt->av.vid.codecCtxt.h264.avccRawLen>0) avc_dumpHex(stderr, pMkvCtxt->av.vid.codecCtxt.h264.avccRaw, pMkvCtxt->av.vid.codecCtxt.h264.avccRawLen, 1);

  } else if(pFrame->isaud && !pMkvCtxt->av.aud.haveSeqHdr) {

    if((rc = flvsrv_check_audseqhdr(&pMkvCtxt->av.aud, pFrame)) <= 0) {
      //fprintf(stderr, "check audseqhdr: %d\n", rc);
      return rc;
    }

    //fprintf(stderr, "check audseqhdr: %d\n", rc);

  }

//fprintf(stderr, "isvid:%d, isaud:%d, RC:%d, senthdr:%d, vidseq:%d, audSeq:%d, novid:%d, noaud:%d\n", pFrame->isvid, pFrame->isaud, rc, pMkvCtxt->senthdr, pMkvCtxt->av.vid.haveSeqHdr, pMkvCtxt->av.aud.haveSeqHdr, *pMkvCtxt->pnovid, *pMkvCtxt->pnoaud);

  if(!pMkvCtxt->av.vid.sentSeqHdr && (*pMkvCtxt->pnovid || pMkvCtxt->av.vid.haveSeqHdr) &&
     (*pMkvCtxt->pnoaud || pMkvCtxt->av.aud.haveSeqHdr)) {

    pMkvCtxt->out.bs.idx = 0;

    if(rc >= 0 && !(*pMkvCtxt->pnovid)) {

      if((rc = mkv_create_vidtrack(pMkvCtxt, &pMkvCtxt->mkvTracks[0], 1)) < 0) {
        LOG(X_ERROR("Failed to create mkv video track"));
        pMkvCtxt->state = MKVSRV_STATE_ERROR;
        return rc;
      }

    }
    if(rc >= 0 && !(*pMkvCtxt->pnoaud)) {

      if((rc = mkv_create_audtrack(pMkvCtxt, 
                                   &pMkvCtxt->mkvTracks[*pMkvCtxt->pnovid ? 0 : 1],
                                   *pMkvCtxt->pnovid ? 1 : 2)) < 0) {
        LOG(X_ERROR("Failed to create mkv audio track"));
        pMkvCtxt->state = MKVSRV_STATE_ERROR;
        return rc;
      }
     
    }
    //fprintf(stderr, "RC:%d, senthdr:%d, novid:%d, noaud:%d\n", rc, pMkvCtxt->senthdr, *pMkvCtxt->pnovid, *pMkvCtxt->pnoaud);
    //TODO: get vid/aud present, width, ht, etc
    if(rc >= 0 && !pMkvCtxt->senthdr) {

      //
      // If a buffering delay has been set sleep for its duration
      // to allow a viewing client to pre-buffer the live contents.
      //
      if(!pMkvCtxt->pRecordOutFmt && pMkvCtxt->avBufferDelaySec > 0.0f) {
        usleep( (int) (pMkvCtxt->avBufferDelaySec * 1000000) );
      }

      //
      // Sent the MKV header and segments 
      // 
      if((rc = mkvsrv_sendStart(pMkvCtxt)) < 0) {
        pMkvCtxt->state = FLVSRV_STATE_ERROR;
        return rc;
      }
    }
  
    if(rc >= 0) {
      pMkvCtxt->av.vid.sentSeqHdr = 1;
      LOG(X_DEBUG("mkvplay got audio / video start - beginning transmission"));
    }
  }


  if(rc >= 0) {
 
    if(!pMkvCtxt->av.vid.sentSeqHdr) {

      //
      // TODO: we're going to end up missing the first GOP in the case we get a vid keyframe
      // and then an audio start... because we don't write the FLV header start until we have  both
      //
      //if(pMkvCtxt->pRecordOutFmt) {
        pMkvCtxt->av.vid.haveKeyFrame = 0;
      //}

      //fprintf(stderr, "TRY ABORT this is keyframe:%d, haveKeyFr:%d \n", keyframe, pFlvCtxt->av.vid.haveKeyFrame);
      return rc;
    }

    pMkvCtxt->out.bs.idx = 0;
    if(pFrame->isvid) {

      //
      // Only start sending video content beginning w/ a keyframe
      //
      if(!pMkvCtxt->av.vid.haveKeyFrame) { 
        if(keyframe) {
          pMkvCtxt->av.vid.haveKeyFrame = 1;
          pMkvCtxt->av.vid.nonKeyFrames = 0;
        } else {
          codec_nonkeyframes_warn(&pMkvCtxt->av.vid, "MKV");
        }
      }

      if(pMkvCtxt->av.vid.haveKeyFrame) {
        rc = mkvsrv_send_frame(pMkvCtxt, pFrame, keyframe);
        //fprintf(stderr, "MKVSRV_SEND_VIDEOFRAMElen:%d, pts:%.3f,key:%d, rc:%d\n", OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), OUTFMT_KEYFRAME(pFrame), rc);
      }

    } else if(pFrame->isaud) {
  
      if(pMkvCtxt->av.aud.tm0 == 0) {
        pMkvCtxt->av.aud.tm0 = pMkvCtxt->av.aud.tmprev = OUTFMT_PTS(pFrame);
      }
      rc = mkvsrv_send_frame(pMkvCtxt, pFrame, 0);
      //fprintf(stderr, "MKVSRV_SEND_AUDIOFRAME len:%d, pts:%.3f, rc:%d\n", OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), rc);
    }

  }

  if(rc < 0) {
    pMkvCtxt->state = MKVSRV_STATE_ERROR;
  }

  return rc;
}

//static int mkv_fd;
int mkvsrv_cbWriteDataNet(void *pArg, const unsigned char *pData, unsigned int len) {
  int rc = 0;
  MKVSRV_CTXT_T *pMkvCtxt = (MKVSRV_CTXT_T *) pArg;

  //avc_dumpHex(stderr, pData, len, 1);
  //if(mkv_fd==0) mkv_fd = open("testout.mkv", O_CREAT | O_RDWR, 0660); write(mkv_fd, pData, len);

  if((rc = netio_send(&pMkvCtxt->pSd->netsocket, &pMkvCtxt->pSd->sain, pData, len)) < 0) {
    LOG(X_ERROR("Failed to send mkv %s %u bytes, total: %llu)"),
               (pMkvCtxt->writeErrDescr ? pMkvCtxt->writeErrDescr : ""), len, pMkvCtxt->totXmit);
  } else {
    pMkvCtxt->totXmit += len;
  }

  return rc;
}

int mkvsrv_cbWriteDataFile(void *pArg, const unsigned char *pData, unsigned int len) {
  int rc = 0;

  MKVSRV_CTXT_T *pMkvCtxt = (MKVSRV_CTXT_T *) pArg;

  //fprintf(stderr, "CB MKV RECORD len:%d\n", len);

  if(pMkvCtxt->recordFile.fp == FILEOPS_INVALID_FP) {

    if(capture_openOutputFile(&pMkvCtxt->recordFile, pMkvCtxt->overwriteFile, NULL) < 0) {

      // Do not call outfmt_removeCb since we're invoked from the cb thread
      //outfmt_removeCb(pFlvCtxt->pRecordOutFmt);
      pMkvCtxt->pRecordOutFmt->do_outfmt = 0;
      pMkvCtxt->pRecordOutFmt = NULL;

      return -1;
    }
    LOG(X_INFO("Created mkv recording output file %s"), pMkvCtxt->recordFile.filename);
   
  }

  if((rc = WriteFileStream(&pMkvCtxt->recordFile, pData, len)) < 0) {
    LOG(X_ERROR("Failed to write mkv %s %u bytes, total: %llu)"),
               (pMkvCtxt->writeErrDescr ? pMkvCtxt->writeErrDescr : ""), len, pMkvCtxt->totXmit);
  } else {
    pMkvCtxt->totXmit += len;
  }

  return rc;
}


int mkvsrv_init(MKVSRV_CTXT_T *pMkv, unsigned int vidTmpFrameSz) {
  //int rc;

  pMkv->state = MKVSRV_STATE_INVALID;
  pMkv->cbWrite = mkvsrv_cbWriteDataNet;
  pMkv->clusterDurationMs = 1000;

  pMkv->out.bs.sz = 1024;
  if((pMkv->out.bs.buf = (unsigned char *) avc_calloc(1, pMkv->out.bs.sz)) == NULL) {
    return -1;
  }

  if(vidTmpFrameSz == 0) {
    vidTmpFrameSz = STREAMER_RTMPQ_SZSLOT_MAX;
  }
  if((codecfmt_init(&pMkv->av, vidTmpFrameSz, 0)) < 0) {
  //pMkv->av.vid.tmpFrame.sz = vidTmpFrameSz;
  //if((pMkv->av.vid.tmpFrame.buf = (unsigned char *) avc_calloc(1, pMkv->av.vid.tmpFrame.sz)) == NULL) {
    mkvsrv_close(pMkv);
    return -1;
  }

  if(pMkv->faoffset_mkvpktz != 0) {
     LOG(X_DEBUG("Setting MKV packetizer %s offset forward %.3f sec"), pMkv->faoffset_mkvpktz > 0 ? "audio" : "video", 
                  pMkv->faoffset_mkvpktz);
  }

//pFlv->g_fd = open("test.flv", O_RDWR | O_CREAT, 0666);

  return 0;
}

void mkvsrv_close(MKVSRV_CTXT_T *pMkv) {

  if(pMkv) {
    codecfmt_close(&pMkv->av);

    if(pMkv->out.bs.buf) {
      avc_free((void **) &pMkv->out.bs.buf);
    }
  }

}

int mkvsrv_record_start(MKVSRV_CTXT_T *pMkvCtxt, STREAMER_OUTFMT_T *pLiveFmt) {
  int rc = 0;
  unsigned int numQFull = 0;

  if(!pMkvCtxt || !pLiveFmt) {
    return -1;
  }

  //
  // Add a livefmt cb
  //
  pMkvCtxt->pRecordOutFmt = outfmt_setCb(pLiveFmt, mkvsrv_addFrame, pMkvCtxt, &pLiveFmt->qCfg, NULL, 0, 0, &numQFull);

  if(pMkvCtxt->pRecordOutFmt) {

   if(mkvsrv_init(pMkvCtxt, pLiveFmt->qCfg.growMaxPktLen) < 0) {
      return rc;
    }
    pMkvCtxt->cbWrite = mkvsrv_cbWriteDataFile;

    LOG(X_INFO("Starting mkvlive recording[%d] %d/%d to %s"), pMkvCtxt->pRecordOutFmt->cbCtxt.idx,
        numQFull + 1, pLiveFmt->max, pMkvCtxt->recordFile.filename);

  } else {
    LOG(X_ERROR("No mkvlive recording resource available (max:%d)"), (pLiveFmt ? pLiveFmt->max : 0));
  }

  return rc;
}

int mkvsrv_record_stop(MKVSRV_CTXT_T *pMkvCtxt) {
  int rc = 0;

  if(!pMkvCtxt || !pMkvCtxt->pRecordOutFmt) {
    return -1;
  }

  //
  // Remove the livefmt cb
  //
  outfmt_removeCb(pMkvCtxt->pRecordOutFmt);
  pMkvCtxt->pRecordOutFmt = NULL;
  CloseMediaFile(&pMkvCtxt->recordFile);

  mkvsrv_close(pMkvCtxt);

  return rc;
}


