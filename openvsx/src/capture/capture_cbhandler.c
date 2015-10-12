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

int cbOnPkt_raw(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {
  int rc;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  const unsigned char *pData = pPkt ? pPkt->payload.pData : 0;
  unsigned int len = pPkt ? PKTCAPLEN(pPkt->payload) : 0;

  if(pSp == NULL) {
    return -1;
  }
//fprintf(stderr, "cbOnPkt_raw writing %d\n", len);
  if(len == 0) {
    LOG(X_WARNING("Detected lost packet"));
    return 0;
  } else if((rc = WriteFileStream(&pSp->stream1, pData, len)) < 0) {
    return -1;
  }

  return 0;
}

//static int g_idx;

int cbOnPkt_queuePkt(void *pUserData, const COLLECT_STREAM_PKTDATA_T *pPkt) {

  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  const unsigned char *pData = pPkt ? pPkt->payload.pData : 0;
  unsigned int len = pPkt ? PKTCAPLEN(pPkt->payload) : 0;
  int rc;
  unsigned int lenadd;
  unsigned int idx = 0;
  uint16_t rtpseq;
  PKTQ_EXTRADATA_T pktxtra;
  PKTQ_EXTRADATA_T *pXtra = NULL;

  if(pSp == NULL || pSp->pCapAction->pQueue == NULL) {
    return -1;
  }

  if(IS_CAPTURE_FILTER_TRANSPORT_RTP(pSp->pStream->pFilter->transType)) {

    if(len == 0) {
      pSp->islastPktLost = 1;
      return 0;
    }

    rtpseq = pPkt->u.rtp.seq;
    pktxtra.tm.pts = pPkt->u.rtp.ts;
    pktxtra.tm.dts = 0;
    //pktxtra.flags = 0;
    pktxtra.pQUserData = NULL;
    pXtra = &pktxtra;

    if(pSp->islastPktLost) {
      //LOG(X_WARNING("Detected RTP %d packets lost"), RTP_SEQ_DELTA(rtpseq, pSp->lastPktSeq) -1);
      pSp->islastPktLost = 0;
    }
    if(rtpseq != 0) {
      pSp->lastPktSeq = (uint16_t) rtpseq;
    }

  } else if(len == 0) {
    LOG(X_WARNING("Detected lost packet(s)"));
    return 0;
  }

  //fprintf(stderr, "queuepkt len:%d concat:%d, max:%d(%d)\n", len, pSp->pCapAction->pQueue->cfg.concataddenum, pSp->pCapAction->pQueue->cfg.maxPktLen, pSp->pCapAction->pQueue->cfg.growMaxPktLen);

  do {

    if((lenadd = (len - idx)) > pSp->pCapAction->pQueue->cfg.maxPktLen) {
      lenadd = pSp->pCapAction->pQueue->cfg.maxPktLen;
    }
    if((rc = pktqueue_addpkt(pSp->pCapAction->pQueue, &pData[idx], lenadd, pXtra, 
                             ((pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME) ? 1 : 0))) < 0) {
      break;
    }
    
    idx += lenadd;

  } while(idx < len);

//fprintf(stderr, "cbqueue_pkt %d rc:%d (%f) %s\n", len, rc, len/188.0f, len%188 ? "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" : "");
  return rc;
}

int capture_cbOnStreamEnd(CAPTURE_STREAM_T *pStream) {
  int rc = 0;
  //int queueFr = 0;
  CAPTURE_CBDATA_SP_T *pSp = NULL;

  if(!pStream || !(pSp = (CAPTURE_CBDATA_SP_T *) pStream->pCbUserData)) {
    return -1;
  }

  //if(pSp->pCapAction && (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
  //  queueFr = 1;
  //}

  LOG(X_DEBUGV("cbOnStreamEnd %s"), pStream->strSrcDst);

  if(pSp->stream1.fp != FILEOPS_INVALID_FP) {
    LOG(X_DEBUG("Closing output file %s"), pSp->stream1.filename);
    CloseMediaFile(&pSp->stream1);    
  }

  //
  // Delete the FB array used for RTCP FIR / PLI requests and dropping of partially damaged
  // frame content
  //
  if(pSp->pCaptureFbArray) {
    fbarray_destroy(pSp->pCaptureFbArray);
    pSp->pCaptureFbArray = NULL;
  }

  pSp->inuse = 0;

  return rc;
}


static CAPTURE_FBARRAY_T *capture_create_fb_array(CAPTURE_CBDATA_T *pAllStreams, 
                                                  CAPTURE_CBDATA_SP_T *pSp, 
                                                  unsigned int clockHz) {
  CAPTURE_FBARRAY_T *pCaptureFbArray = NULL;

  if(pAllStreams->pCfg && pAllStreams->pCfg->pStreamerCfg &&
     pAllStreams->pCfg->pStreamerCfg->fbReq.firCfg.fir_send_from_capture) {

    if(pSp->pCaptureFbArray) {
       fbarray_destroy(pSp->pCaptureFbArray);
       pSp->pCaptureFbArray = NULL;
     }

     if((pCaptureFbArray = fbarray_create(50, clockHz))) {
       pCaptureFbArray->pFbReq = &pAllStreams->pCfg->pStreamerCfg->fbReq;
     }
   }

  return pCaptureFbArray;
}

int capture_cbOnStreamAdd(void *pCbUserData, 
                         CAPTURE_STREAM_T *pStream, 
                         const COLLECT_STREAM_HDR_T *pktHdr,
                         const char *filepath) {
  unsigned int idx;
  CAPTURE_CBDATA_T *pAllStreams = (CAPTURE_CBDATA_T *) pCbUserData;
  CAPTURE_CBDATA_SP_T *pSp = NULL;
  FILE_STREAM_T *pFileStream = NULL;
  char tmp[32];

  if(!pStream || !pStream->pFilter) {
    return -1;
  }
  //fprintf(stderr, "cbOnStreamAdd pStream:0x%x, pFilter: 0x%x, mediaT:%d\n", pStream, pStream->pFilter, pStream->pFilter->mediaType);

  for(idx = 0; idx < pAllStreams->maxStreams; idx++) {
    if(!pAllStreams->sp[idx].inuse) {
      pSp = &pAllStreams->sp[idx];
      break;
    }
  }

  if(pSp == NULL) {
    LOG(X_WARNING("Unable to add more than %d streams"), pAllStreams->maxStreams);
    return -1;
  } 

  //TODO: (re)init the pSp and its stream contents...
  if(pSp->pCapAction) {
    pSp->pCapAction->tmpFrameBuf.idx = 0;
    pktqueue_reset(pSp->pCapAction->pQueue, 0);
  }
  memset(&pSp->u, 0, sizeof(pSp->u));
  pSp->lastPktTs = 0;
  pSp->lastPktSeq = 0;
  pSp->islastPktLost = 0;
  pSp->frameHasError = 0;
  pSp->spFlags = 0;

  pSp->pStream = pStream;
  pStream->cbOnStreamEnd = capture_cbOnStreamEnd;

  if(pktHdr && IS_CAPTURE_FILTER_TRANSPORT_RTP(pStream->pFilter->transType)) {
    snprintf(tmp, sizeof(tmp), " ssrc:0x%x, pt:%d",  (pktHdr->key.ssrc), pktHdr->payloadType);
  } else {
    tmp[0] = '\0';
  }
  LOG(X_INFO("New input stream[%d] %s %s%s"), idx, pStream->strSrcDst,
       codecType_getCodecDescrStr(pStream->pFilter->mediaType), tmp);


  //
  // - if pCapAction is not set assume default behavior of recording 
  // - if cmd is CAPTURE_PKT_ACTION_QUEUE 
  //      then just put the contents into a queue and return
  // - if cmd is CAPTURE_PKT_ACTION_EXTRACTPES 
  //      nothing needs to be done, as any streamer is already preconfigured
  // - if cmd is CAPTURE_PKT_ACTION_QUEUEFRAME
  //      the default pkt handler callback needs to be set just as if recording
  //      the pkt callback will then queue any demuxed frame contents
  //

  // TODO: create m2t packet handler to start w/ sync byte + check continuity id

  if(pStream->pFilter->pCapAction) {

    if(pStream->pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_MP2TS) {
      pStream->clockHz = 90000;
    }

    pSp->pCapAction = pStream->pFilter->pCapAction;

    if(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUE) {

      pStream->cbOnPkt = cbOnPkt_queuePkt; 
      pStream->pCbUserData = pSp;
      pSp->inuse = 1; 
  
      return 0;

    } else if((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_EXTRACTPES) &&
              !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      LOG(X_ERROR("PES extract capture not implemented w/o using async queue"));
      return -1;
    }


  }

  switch(pStream->pFilter->mediaType) {

    case XC_CODEC_TYPE_VID_CONFERENCE:
    case XC_CODEC_TYPE_AUD_CONFERENCE:
      if(codectype_isVid(pStream->pFilter->mediaType)) {
        pStream->clockHz = pStream->pFilter->u_seqhdrs.vid.common.clockHz;
      } else {
        pStream->clockHz = pStream->pFilter->u_seqhdrs.aud.common.clockHz;
      }
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_H264:

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {

        pFileStream = &pSp->stream1;

      } else {
        if(pStream->pFilter->u_seqhdrs.vid.common.clockHz == 0) {
          LOG(X_ERROR("Input clock rate not set for H.264 stream"));
          return -1;
        }

      }

      pStream->clockHz = pStream->pFilter->u_seqhdrs.vid.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_h264;
      pSp->pCaptureFbArray = capture_create_fb_array(pAllStreams, pSp, pStream->clockHz);
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_MPEG4V:

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      } else {
        if(pStream->pFilter->u_seqhdrs.vid.common.clockHz == 0) {
          LOG(X_ERROR("Input clock rate not set for mpg4v stream"));
          return -1;
        }
      }
      pStream->clockHz = pStream->pFilter->u_seqhdrs.vid.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_mpg4v;
      pSp->pCaptureFbArray = capture_create_fb_array(pAllStreams, pSp, pStream->clockHz);
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_H263:
    case CAPTURE_FILTER_PROTOCOL_H263_PLUS:

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      } else {
        if(pStream->pFilter->u_seqhdrs.vid.common.clockHz == 0) {
          LOG(X_ERROR("Input clock rate not set for h263 stream"));
          return -1;
        }
      }
      pStream->clockHz = pStream->pFilter->u_seqhdrs.vid.common.clockHz;
      memset(&pSp->u.h263, 0, sizeof(pSp->u.h263));
      pSp->u.h263.codecType = pStream->pFilter->mediaType;
      pStream->cbOnPkt = cbOnPkt_h263;
      pSp->pCaptureFbArray = capture_create_fb_array(pAllStreams, pSp, pStream->clockHz);
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_VP8:

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      } else {
        if(pStream->pFilter->u_seqhdrs.vid.common.clockHz == 0) {
          LOG(X_ERROR("Input clock rate not set for vp8 stream"));
          return -1;
        }
      }
      pStream->clockHz = pStream->pFilter->u_seqhdrs.vid.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_vp8;
      pSp->pCaptureFbArray = capture_create_fb_array(pAllStreams, pSp, pStream->clockHz);
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_AAC:
      if(streamadd_aac(pSp) < 0) {
        return -1;
      }
      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      }
      pStream->clockHz = pStream->pFilter->u_seqhdrs.aud.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_aac;
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_SILK:

      if(streamadd_silk(pSp) < 0) {
        return -1;
      }

      //
      // We've been relying on in-band sequence headers to carry meta-data such as channel configuration,
      // here we try to set the xcode config based on what we read from any input filter setting or SDP
      //
      if(pAllStreams->pCfg->pStreamerCfg && pAllStreams->pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn == 0) {
        pAllStreams->pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn = pStream->pFilter->u_seqhdrs.aud.channels;
      }

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      }
      pStream->clockHz = pStream->pFilter->u_seqhdrs.aud.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_silk;
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_OPUS:

      if(streamadd_opus(pSp) < 0) {
        return -1;
      }

      //
      // We've been relying on in-band sequence headers to carry meta-data such as channel configuration,
      // here we try to set the xcode config based on what we read from any input filter setting or SDP
      //
      if(pAllStreams->pCfg->pStreamerCfg && pAllStreams->pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn == 0) {
        pAllStreams->pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn = pStream->pFilter->u_seqhdrs.aud.channels;
      }

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      }
      pStream->clockHz = pStream->pFilter->u_seqhdrs.aud.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_opus;
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_AMR:

      if(streamadd_amr(pSp) < 0) {
        return -1;
      }
      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      } 
      pStream->clockHz = pStream->pFilter->u_seqhdrs.aud.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_amr;
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_G711_MULAW:
    case CAPTURE_FILTER_PROTOCOL_G711_ALAW:
      pSp->u.pcm.bytesPerHz = 1;
      if(streamadd_pcm(pSp) < 0) {
        return -1;
      }
      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      } 
      pStream->clockHz = pStream->pFilter->u_seqhdrs.aud.common.clockHz;
      pStream->cbOnPkt = cbOnPkt_pcm;
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_MP2TS:

      // TODO: create m2t packet handler to start w/ sync byte + check continuity id
      pStream->clockHz = 90000;
      pStream->cbOnPkt = cbOnPkt_raw;
      pFileStream = &pSp->stream1;
      pSp->inuse = 1;

      break;

    case CAPTURE_FILTER_PROTOCOL_RTMP:

      //fprintf(stderr, "onStreamAdd - CAPTURE_FILTER_PROTOCOL_RTMP\n");
      if(rtmp_record_init(&pSp->u.rtmp) < 0) {
        return -1;
      }
      pStream->cbOnPkt = cbOnPkt_rtmp;
      pSp->u.rtmp.overWriteFile = pAllStreams->overWriteOutput;
      pSp->u.rtmp.outputDir = pAllStreams->outDir;
      pSp->u.rtmp.pDescr = pStream->strSrcDst;
      pSp->inuse = 1;

      break;

    case CAPTURE_FILTER_PROTOCOL_RAW:

      pStream->cbOnPkt = cbOnPkt_raw;
      pFileStream = &pSp->stream1;
      pSp->inuse = 1;
      break;

    case CAPTURE_FILTER_PROTOCOL_RAWV_BGRA32:
    case CAPTURE_FILTER_PROTOCOL_RAWV_RGBA32:
    case CAPTURE_FILTER_PROTOCOL_RAWV_BGR24:
    case CAPTURE_FILTER_PROTOCOL_RAWV_RGB24:
    case CAPTURE_FILTER_PROTOCOL_RAWV_BGR565:
    case CAPTURE_FILTER_PROTOCOL_RAWV_RGB565:
    case CAPTURE_FILTER_PROTOCOL_RAWV_YUV420P:
    case CAPTURE_FILTER_PROTOCOL_RAWV_NV12:
    case CAPTURE_FILTER_PROTOCOL_RAWV_NV21:

    case CAPTURE_FILTER_PROTOCOL_RAWA_PCM16LE:
    case CAPTURE_FILTER_PROTOCOL_RAWA_PCMULAW:
    case CAPTURE_FILTER_PROTOCOL_RAWA_PCMALAW:

      //if(!pSp->pCapAction || !(pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME)) {
      if(!pSp->pCapAction || 
         !((pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_QUEUEFRAME) ||
           (pSp->pCapAction->cmd & CAPTURE_PKT_ACTION_STUNONLY))) {
        pFileStream = &pSp->stream1;
      } 
      pStream->cbOnPkt = cbOnPkt_rawdev;
      pSp->inuse = 1;

      break;

    default:

      pSp->inuse = 0;
      LOG(X_ERROR("Unsupported filter media type: %u"), pStream->pFilter->mediaType);
      return -1;

  }

  if(pFileStream) {

    if(filepath) {
      snprintf(pFileStream->filename, sizeof(pFileStream->filename),
               "%s%s%s", (pAllStreams->outDir ? pAllStreams->outDir : ""),
                (pAllStreams->outDir ? DIR_DELIMETER_STR : ""),
                filepath);

    } else if(capture_createOutPath(pFileStream->filename,
                           sizeof(pFileStream->filename),
                           pAllStreams->outDir,
                           pStream->pFilter->outputFilePrfx,
                           &pStream->hdr,
                           pStream->pFilter->mediaType) < 0) {
      LOG(X_ERROR("Invalid output file name or directory."));
      pStream->cbOnPkt = NULL;
      return -1;
    }

    if(capture_openOutputFile(pFileStream, 
                              pAllStreams->overWriteOutput,
                              pAllStreams->outDir) < 0 ) {
      pStream->cbOnPkt = NULL;
      return -1;
    }
    LOG(X_INFO("Created '%s'"), pFileStream->filename);
  }

  pStream->pCbUserData = pSp;

  return 0;
}

int capture_cbWriteFile(void *pArg, const void *pData, uint32_t szData) {
  return WriteFileStream((FILE_STREAM_T *) pArg, pData, szData);
}

int capture_cbWriteBuf(void *pArg, const void *pData, uint32_t szData) {
  BYTE_STREAM_T *pStream = (BYTE_STREAM_T *) pArg;

  if(pStream->idx + szData > pStream->sz) {
    LOG(X_ERROR("Unable to store data frame contents %u -> [%u/%u]"), szData, pStream->idx, pStream->sz);
    return -1;
  }
  memcpy(&pStream->buf[pStream->idx], pData, szData);
  //fprintf(stderr, "storing %d [%d]\n", szData, pStream->idx);
  pStream->idx += szData;

  return szData;
}

static int capture_analyze_fbarray(const CAPTURE_FBARRAY_T *pCaptureFbArray) {
  int rc = 0;
  unsigned int idx;
  unsigned int idxElem;
  uint32_t tsDelta;
  float msDelta;
  float msDeltaHistory[10];
  unsigned int msDeltaHistoryIdx = 0;
  const CAPTURE_FBARRAY_ELEM_T *pElem;

  //LOG(X_DEBUG("ANALYZE FBARRAY...\n\n\n"));

  if(!pCaptureFbArray) {
    return 0;
  } else if(!pCaptureFbArray->pFbReq) {
    //
    // No point proceeding if we can't signal any feedback messages
    //
    return 0;
  }

  //fprintf(stderr, "analyze_fbarraycount:%d\n", pCaptureFbArray->countElem);

  idxElem = pCaptureFbArray->idxElem;

  for(idx = 0; idx < MIN(pCaptureFbArray->szElem, pCaptureFbArray->countElem); idx++) {

    pElem = &pCaptureFbArray->pElemArr[idxElem];

    //fprintf(stderr, "FB ANALYSIS [%d]/%d ts:%u flags: 0x%x, haveKey:%d\n", idxElem,pCaptureFbArray->szElem, pElem->ts, pElem->flags, pCaptureFbArray->havePriorKey);


    //
    // If we think the last frame was a keyframe and it was damaged, immediately request another
    //
    if((pElem->flags & CAPTURE_SP_FLAG_KEYFRAME)) {

      if(((pElem->flags & CAPTURE_SP_FLAG_PREVLOST) || (pElem->flags & CAPTURE_SP_FLAG_DROPFRAME ||
         pElem->flags & CAPTURE_SP_FLAG_DAMAGEDFRAME))) {
        LOG(X_DEBUG("Input keyframe damaged at ts %uHz.  Will request IDR"), pElem->ts);
        rc = 1;
        break;
      }

    } else {

      if(((pElem->flags & CAPTURE_SP_FLAG_PREVLOST) || (pElem->flags & CAPTURE_SP_FLAG_DROPFRAME ||
         pElem->flags & CAPTURE_SP_FLAG_DAMAGEDFRAME))) {

        if(pCaptureFbArray->havePriorKey && pCaptureFbArray->clockHz > 0) {

          tsDelta = RTP_TS_DELTA(pElem->ts, pCaptureFbArray->tsPriorKey);
          msDelta = (float) tsDelta / pCaptureFbArray->clockHz;
          //fprintf(stderr, "ts:%uHz, msDeltaGOP:%.3f, msDeltaHistoryIdx:%d\n", pElem->ts, msDelta, msDeltaHistoryIdx);

          //
          // TODO: this will break if we change H.264 RTP packetizer to utilize DTS (not PTS) B frame offsets
          // which are needed for QuickTime RTSP
          //
          if(msDeltaHistoryIdx < sizeof(msDeltaHistory) / sizeof(msDeltaHistory[0])) {
             msDeltaHistory[msDeltaHistoryIdx++] = msDelta;
          }

          if(msDeltaHistoryIdx >= 2) {
            LOG(X_DEBUG("Input GOP damaged at ts %uHz.  Last key frame %.3f ms ago.  Will request IDR"), pElem->ts, msDelta);
            rc = 1;
            break;
          }

        }
      }
    }

    //
    // Move to the next element
    //
    idxElem = FBARRAY_ELEM_PRIOR(pCaptureFbArray, idxElem);
  }

  //LOG(X_DEBUG("ANALYZE FBARRAY RC:%d\n"), rc);
  if(rc == 1) {
    capture_requestFB(pCaptureFbArray->pFbReq, ENCODER_FBREQ_TYPE_FIR, REQUEST_FB_FROM_CAPTURE);
    //fbarray_reset(pCaptureFbArray);
  }


  return rc;
}


static int capture_add_to_fbarray(CAPTURE_CBDATA_SP_T *pSp, uint32_t ts) {
  int rc = 0;

  //
  // If we got a complete keyframe, erase everything in the frame FeedBack history array that came in the prior GOP
  //
  if((pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME)) {
    fbarray_reset(pSp->pCaptureFbArray);
  }

  if((pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME) || (pSp->spFlags & CAPTURE_SP_FLAG_PREVLOST) ||
     (pSp->spFlags & CAPTURE_SP_FLAG_DROPFRAME) || (pSp->spFlags & CAPTURE_SP_FLAG_DAMAGEDFRAME)) {

    fbarray_add(pSp->pCaptureFbArray, ts, 0, (pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME) ? 1 : 0, pSp->spFlags);
    capture_analyze_fbarray(pSp->pCaptureFbArray);
    //fbarray_dump(pSp->pCaptureFbArray);
  }

  return rc;
}

int capture_addCompleteFrameToQ(CAPTURE_CBDATA_SP_T *pSp, uint32_t ts) {
  PKTQ_EXTRADATA_T xtra;
  int rc = 0;
  unsigned int numOverwritten;
  int idxRdDelta = 0;
  int64_t ptsDelta;
  uint32_t clockHz;

  clockHz = pSp->pStream->pFilter->u_seqhdrs.vid.common.clockHz;

  //
  // Add any capture statistics to the input feedback array which controls
  // sending requests for IDRs 
  //
  if(pSp->pCaptureFbArray) {
    capture_add_to_fbarray(pSp, ts);
  }

  if((pSp->spFlags & CAPTURE_SP_FLAG_DROPFRAME)) {

    LOG(X_WARNING("Dropping damaged frame ts: %u / %u"), ts, clockHz);
    pSp->spFlags &= ~CAPTURE_SP_FLAG_DROPFRAME;

  } else {

    xtra.tm.pts = (uint64_t) (90000 * ((double)ts / clockHz));
    ((CAPTURE_STREAM_T *)pSp->pStream)->ptsLastFrame = xtra.tm.pts;

    VSX_DEBUG_INFRAME(
      LOG(X_DEBUG("INFRAME  - capture-queue-frame [id:%d,rd:%d,wr:%d], len:%d, ts: %u/%uHz (%.3f), flags: 0x%x"), 
                   pSp->pCapAction->pQueue->cfg.id, pSp->pCapAction->pQueue->idxRd, 
                   pSp->pCapAction->pQueue->idxWr, pSp->pCapAction->tmpFrameBuf.idx, 
                   ts, clockHz, PTSF(xtra.tm.pts), pSp->spFlags););

    xtra.tm.dts = 0;
    //xtra.flags = 0;
    //if(pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME) {
    //  xtra.flags = CAPTURE_SP_FLAG_KEYFRAME;
    //}
    xtra.pQUserData = NULL;

    if(pSp->spFlags & CAPTURE_SP_FLAG_DAMAGEDFRAME) {
      LOG(X_DEBUGV("Queing damaged input frame ts: %u / %u"), ts, clockHz);
    }

//    if(pSp->pCapAction->pQueue->cfg.id==10){ 
//LOG(X_DEBUG("addCompleteFrameToQ[id:%d] rtp ts:%u %.3f clock:%uHz len:%d q{ rd:%d, wr:%d / %d} %s, flags:0x%x, complement: 0x%x "), pSp->pCapAction->pQueue->cfg.id, ts, (double)xtra.tm.pts/90000.0f,clockHz, pSp->pCapAction->tmpFrameBuf.idx, pSp->pCapAction->pQueue->idxRd, pSp->pCapAction->pQueue->idxWr, pSp->pCapAction->pQueue->cfg.maxPkts, (pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME) ? "KEYFRAME" : "", pSp->spFlags, pSp->pCapAction->pQueue->pQComplement); 
//}//avc_dumpHex(stderr, pSp->pCapAction->tmpFrameBuf.buf, pSp->pCapAction->tmpFrameBuf.idx > 16 ? 16 : pSp->pCapAction->tmpFrameBuf.idx, 0); }

#if 0
static unsigned int datalen;
int bufsz;
static char buf[1024];
static int g_fdh264_sz; if(g_fdh264_sz==0) g_fdh264_sz=open("test.h264.txt", O_RDWR | O_CREAT, 0660); 
bufsz=snprintf(buf, sizeof(buf), "file:0x%x framelen:%d 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x...\n", datalen, pSp->pCapAction->tmpFrameBuf.idx, pSp->pCapAction->tmpFrameBuf.buf[0], pSp->pCapAction->tmpFrameBuf.buf[1], pSp->pCapAction->tmpFrameBuf.buf[2], pSp->pCapAction->tmpFrameBuf.buf[3], pSp->pCapAction->tmpFrameBuf.buf[4], pSp->pCapAction->tmpFrameBuf.buf[5]);
write(g_fdh264_sz, buf, bufsz);
datalen += pSp->pCapAction->tmpFrameBuf.idx;
static int g_fdh264; if(g_fdh264==0) g_fdh264=open("test.h264", O_RDWR | O_CREAT, 0660); write(g_fdh264, pSp->pCapAction->tmpFrameBuf.buf, pSp->pCapAction->tmpFrameBuf.idx);
#endif // 0

    numOverwritten =  pSp->pCapAction->pQueue->numOverwritten;

    rc = pktqueue_addpkt(pSp->pCapAction->pQueue, pSp->pCapAction->tmpFrameBuf.buf,
                         pSp->pCapAction->tmpFrameBuf.idx, &xtra,
                         ((pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME) ? 1 : 0));

    if(pSp->pCapAction->pQueue->idxRd < pSp->pCapAction->pQueue->idxWr) {
      idxRdDelta = pSp->pCapAction->pQueue->idxWr - pSp->pCapAction->pQueue->idxRd;
    } else if(pSp->pCapAction->pQueue->idxRd > pSp->pCapAction->pQueue->idxWr) {
      idxRdDelta = pSp->pCapAction->pQueue->cfg.maxPkts - pSp->pCapAction->pQueue->idxRd + 
                   pSp->pCapAction->pQueue->idxWr;
    }
    ptsDelta = pSp->pCapAction->pQueue->ptsLastWr - 
               pSp->pCapAction->pQueue->pkts[pSp->pCapAction->pQueue->idxRd].xtra.tm.pts;

    if(pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_AUD_FRAMES &&
       pSp->pAllStreams->pCfg->pStreamerCfg &&
       pSp->pAllStreams->pCfg->pStreamerCfg->status.avs[0].offsetCur > 0) {
      // Adjust to ignore video encoder lag
      //LOG(X_DEBUG("decreasing ptsDelta for audio from %.3f"), PTSF(ptsDelta));
      ptsDelta -= pSp->pAllStreams->pCfg->pStreamerCfg->status.avs[0].offsetCur;
    }

    //LOG(X_DEBUG("queue id:%d, ptsDelta: %.3f, rd:%d/%d, wr:%d, delta:%d, offset0:%.3f, offsetCur:%.3f"), pSp->pCapAction->pQueue->cfg.id, PTSF(ptsDelta), pSp->pCapAction->pQueue->idxRd, pSp->pCapAction->pQueue->cfg.maxPkts, pSp->pCapAction->pQueue->idxWr, idxRdDelta, PTSF(pSp->pAllStreams->pCfg->pStreamerCfg->status.avs[0].offset0), PTSF(pSp->pAllStreams->pCfg->pStreamerCfg->status.avs[0].offsetCur));

    if(ptsDelta > 60000 && idxRdDelta != 0
       && pSp->pAllStreams->pCfg->pStreamerCfg && pSp->pAllStreams->pCfg->pStreamerCfg->avReaderType == VSX_STREAM_READER_TYPE_NOSYNC
       //(pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_VID_FRAMES && idxRdDelta > 20) ||
       //(pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_AUD_FRAMES && idxRdDelta > 60)
       ) {
//TODO: this should only warn for real-time pip avreader=3...
      if((timer_GetTime() - pSp->pCapAction->lagCtxt.tvLastQWarn) / TIME_VAL_MS > 1000) {

        LOG(X_WARNING("Capture %s queue lagging by %.3f, last pts: %.3f, %d / %d (rd:[%d], wr:[%d]"),
           (pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_VID_FRAMES ? "video" : "audio"), 
        PTSF(ptsDelta), PTSF(pSp->pCapAction->pQueue->ptsLastWr), idxRdDelta, 
        pSp->pCapAction->pQueue->cfg.maxPkts, pSp->pCapAction->pQueue->idxRd, pSp->pCapAction->pQueue->idxWr);
        pSp->pCapAction->lagCtxt.tvLastQWarn = timer_GetTime();
      }

    }

    //
    // If the video encoder appears to be lagging, attempt to reduce the output fps
    //
    if(pSp->pAllStreams->pCfg->pStreamerCfg && pSp->pAllStreams->pCfg->pStreamerCfg->abrSelfThrottle.active &&
       pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_VID_FRAMES &&
       (numOverwritten != pSp->pCapAction->pQueue->numOverwritten || ptsDelta > 90000)) {
      stream_abr_throttleFps(pSp->pAllStreams->pCfg->pStreamerCfg, .5f, 5);
      //stream_abr_throttleFps(pSp->pAllStreams->pCfg->pStreamerCfg, .1f, 2);
    }

    //
    // If the vid/aud queue is lagging beyond our real-time threshold, such as when the encoder is starved
    // for CPU, issue an FIR and upon receiving IDR, roll-up the queues to real-tiem again.
    //
    if(pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_VID_FRAMES && pSp->pCapAction->lagCtxt.doSyncToRT) {
      //TODO: this should only happen for real-time / conferencing mode
      if(pSp->pCapAction->lagCtxt.waitingOnIDR && (pSp->spFlags & CAPTURE_SP_FLAG_KEYFRAME)) {

        LOG(X_DEBUG("Flushing capture %s queue after lagging by %.3f, last pts: %.3f, %d / %d (rd:[%d], wr:[%d]"),
           (pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_VID_FRAMES ? "video" : "audio"), 
           PTSF(ptsDelta), PTSF(pSp->pCapAction->pQueue->ptsLastWr), idxRdDelta, 
          pSp->pCapAction->pQueue->cfg.maxPkts, pSp->pCapAction->pQueue->idxRd, pSp->pCapAction->pQueue->idxWr);

        pktqueue_syncToRT(pSp->pCapAction->pQueue);
        numOverwritten = pSp->pCapAction->pQueue->numOverwritten;
        pSp->pCapAction->lagCtxt.waitingOnIDR = 0;

      }  else if((numOverwritten != pSp->pCapAction->pQueue->numOverwritten || ptsDelta > 90000)) {
        pSp->pCapAction->lagCtxt.waitingOnIDR = 1;
      }
    }

    if(pSp->pCapAction->pQueue->cfg.id == STREAMER_QID_VID_FRAMES &&
       (pSp->pCapAction->lagCtxt.waitingOnIDR ||
        numOverwritten != pSp->pCapAction->pQueue->numOverwritten)) {

      if(pSp->pCaptureFbArray && pSp->pCaptureFbArray->pFbReq) {
        //LOG(X_DEBUG("Sending FIR..."));
        capture_requestFB(pSp->pCaptureFbArray->pFbReq, ENCODER_FBREQ_TYPE_FIR, REQUEST_FB_FROM_CAPTURE);
      }

    }

  }

  pSp->pCapAction->tmpFrameBuf.idx = 0;

  return rc;
}

void capture_initCbData(CAPTURE_CBDATA_T *pCbData, const char *outDir, int overWriteOutput) {
  unsigned int idx;

  memset(pCbData, 0, sizeof(CAPTURE_CBDATA_T));
  pCbData->overWriteOutput = overWriteOutput;
  pCbData->outDir = outDir;
  pCbData->maxStreams = sizeof(pCbData->sp) / sizeof(pCbData->sp[0]);

  for(idx = 0; idx < pCbData->maxStreams; idx++) {
    pCbData->sp[idx].pAllStreams = pCbData;
    pCbData->sp[idx].stream1.fp = FILEOPS_INVALID_FP;
  }

}

void capture_deleteCbData(CAPTURE_CBDATA_T *pCbData) {
  unsigned int idx;

  if(!pCbData) {
    return;
  }

  for(idx = 0; idx < pCbData->maxStreams; idx++) {

    if(pCbData->sp[idx].pCaptureFbArray) {
      fbarray_destroy(pCbData->sp[idx].pCaptureFbArray);
      pCbData->sp[idx].pCaptureFbArray = NULL;
    }

    if(pCbData->sp[idx].stream1.fp != FILEOPS_INVALID_FP) {
      fileops_Close(pCbData->sp[idx].stream1.fp);
      pCbData->sp[idx].stream1.fp = FILEOPS_INVALID_FP;
    }
  }

}

#endif // VSX_HAVE_CAPTURE
