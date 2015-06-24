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

#define IS_STREAM_PES_DATA_VID(p) ((p) == &(((STREAM_PES_T *) (p)->pPes)->vid) ? 1 : 0)
#define IS_STREAM_PES_DATA_AUD(p) ((p) == &(((STREAM_PES_T *) (p)->pPes)->aud) ? 1 : 0)

static int stream_net_av_reset(STREAM_PES_T *pPes) {
  STREAM_AV_DATA_T *pData = NULL;
  unsigned int idx;

  LOG(X_DEBUG("A/V input stream reset"));

  for(idx = 0; idx < 2; idx++) {

    if(idx == 0) {
      pData = &pPes->vid;
    } else {
      pData = &pPes->aud;
    }

    if(pData->pDataSrc) {
      pktqueue_reset((PKTQUEUE_T *) pData->pDataSrc->pCbData, 1);
    }

    xcode_resetdata(pData->pXcodeData);

    pData->numFrames = 0;
    memset(&pData->curPesTm, 0, sizeof(pData->curPesTm));
    memset(&pData->prevPesTm, 0, sizeof(pData->prevPesTm));
    pData->lastQWrIdx = 0;
    if(pData->pHaveSeqStart) {
      *pData->pHaveSeqStart = 0;
    }

  }

  return 0;
} 

static STREAM_AV_DATA_T *getComplement(STREAM_AV_DATA_T *pData) {
  STREAM_PES_T *pPes = (STREAM_PES_T *) pData->pPes;

  if(pData == &pPes->vid) {
    if(pPes->aud.pDataSrc) {
      return &pPes->aud;    
    }
  } else {    
    if(pPes->vid.pDataSrc) {
      return  &pPes->vid;
    }
  }
  return NULL;
}


static enum STREAM_NET_ADVFR_RC checktiming(STREAM_AV_DATA_T *pData) {
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  STREAM_AV_DATA_T *pDataComplement;
  int64_t ptsdelta;
  uint64_t tm0, tm1;

  //fprintf(stderr, "checktiming numF:%d, haveOffset:%d, %.3f\n", pData->numFrames, pData->pXcodeData->haveFrameTmStartOffset, PTSF(pData->pXcodeData->frameTmStartOffset));

  if(pData->numFrames == 0) {

    if(!pData->pXcodeData->haveFrameTmStartOffset) {

      //
      // Set the start of frame reception time
      //
      pData->pXcodeData->frameTmStartOffset = pData->curPesTm.qtm.dts ?
                 pData->curPesTm.qtm.dts : pData->curPesTm.qtm.pts;
      //fprintf(stderr, "cb pts:%.3f dts:%.3f\n", PTSF(pData->curPesTm.qtm.pts), PTSF(pData->curPesTm.qtm.dts));
      LOG(X_DEBUG("Setting av %s stream "MP2PES_STREAMTYPE_FMT_STR" pts start offset to %.3f"),
         IS_STREAM_PES_DATA_VID(pData) ? "video" : "audio",
         MP2PES_STREAMTYPE_FMT_ARGS(pData->pXcodeData->inStreamType), 
         PTSF(pData->pXcodeData->frameTmStartOffset));

      pData->pXcodeData->haveFrameTmStartOffset = 1;
    }

    if((pDataComplement = getComplement(pData))) {
      pktqueue_setrdr((PKTQUEUE_T *) pDataComplement->pDataSrc->pCbData, 0);

      if((pData->streamflags & VSX_STREAMFLAGS_AV_SAME_START_TM) &&
         !pDataComplement->pXcodeData->haveFrameTmStartOffset) {
        pDataComplement->pXcodeData->frameTmStartOffset = 
                                     pData->pXcodeData->frameTmStartOffset;
        LOG(X_DEBUG("Setting av %s complementary stream "MP2PES_STREAMTYPE_FMT_STR
                    " pts to matching start offset to %.3f"), 
              IS_STREAM_PES_DATA_VID(pData) ? "audio" : "video",
             MP2PES_STREAMTYPE_FMT_ARGS(pDataComplement->pXcodeData->inStreamType), 
              pDataComplement->pXcodeData->frameTmStartOffset);
        pDataComplement->pXcodeData->haveFrameTmStartOffset = 1;
      } 
    }

  } else {

    tm0 = pData->prevPesTm.qtm.dts ? pData->prevPesTm.qtm.dts :
          pData->prevPesTm.qtm.pts;
    tm1 = pData->curPesTm.qtm.dts ? pData->curPesTm.qtm.dts :
          pData->curPesTm.qtm.pts;
    ptsdelta = tm1 - tm0;

    if((ptsdelta > 0 && ptsdelta > 90000 * 5) ||
       (ptsdelta < 0 && ptsdelta < -90000 * 5)) {

      LOG(X_WARNING("Large pts %s jump pts:%.3f dts:%.3f -> "
        "pts:%.3f dts:%.3f (%"LL64"d %.3fsec)"), 
         IS_STREAM_PES_DATA_VID(pData) ? "video" : "audio",
          //(pData == &((STREAM_PES_T *) pData->pPes)->vid ? "vid" : "aud"),
          PTSF(pData->prevPesTm.qtm.pts), PTSF(pData->prevPesTm.qtm.dts), 
          PTSF(pData->curPesTm.qtm.pts), PTSF(pData->curPesTm.qtm.dts), 
          ptsdelta, PTSF(ptsdelta));

      rc = STREAM_NET_ADVFR_RC_RESET;

    //} else if(tm1 < tm0) {
    } else if(pData->curPesTm.qtm.pts < pData->prevPesTm.qtm.pts &&
              ((pData->curPesTm.qtm.dts == 0 && pData->prevPesTm.qtm.dts == 0) ||
              (pData->curPesTm.qtm.dts != 0 && pData->prevPesTm.qtm.dts != 0 &&
              pData->curPesTm.qtm.dts + 90000 < pData->prevPesTm.qtm.dts))) {

      LOG(X_WARNING("%s program time went backwards pts: %"LL64"uHz %.3f -> %"LL64"uHz %.3f"
                    ", dts: %.3f -> %.3f"), 
         IS_STREAM_PES_DATA_VID(pData) ? "video" : "audio",
         pData->prevPesTm.qtm.pts, PTSF(pData->prevPesTm.qtm.pts),
         pData->curPesTm.qtm.pts, PTSF(pData->curPesTm.qtm.pts),
         PTSF(pData->prevPesTm.qtm.dts), PTSF(pData->curPesTm.qtm.dts));

    }
  }

  pData->numFrames++;
  memcpy(&pData->prevPesTm.qtm, &pData->curPesTm.qtm, sizeof(pData->prevPesTm.qtm));

  return rc;
}

static int waitfordata(STREAM_AV_DATA_T *pData) {
  PKTQUEUE_T *pQ = (PKTQUEUE_T *) pData->pDataSrc->pCbData;
  STREAM_AV_DATA_T *pDataComplement = NULL;
  uint32_t qWrIdx; 
  uint32_t qWrIdxComplement = 0;
  int rc = 1;

  qWrIdx = pktqueue_havepkt(pQ);

  //fprintf(stderr, "waitfordata called for %s qwridx:%d/%d\n", pData == &((STREAM_PES_T *)pData->pPes)->vid ? "vid" : "aud", qWrIdx, pData->lastQWrIdx);

  if(qWrIdx != 0 && qWrIdx != pData->lastQWrIdx) {
    return 1;
  }

  pDataComplement = getComplement(pData);

  if(pDataComplement && ((PKTQUEUE_T *) pDataComplement->pDataSrc->pCbData)->haveRdr) {
    qWrIdxComplement = pktqueue_havepkt((PKTQUEUE_T *) pDataComplement->pDataSrc->pCbData); 
    //fprintf(stderr, "waitfordata complement %s qwridx:%d/%d\n", pDataComplement == &pPes->vid ? "vid" : "aud", qWrIdxComplement, pDataComplement->lastQWrIdx);
    if(qWrIdxComplement != 0 && qWrIdxComplement != pDataComplement->lastQWrIdx) {
      pDataComplement->lastQWrIdx = qWrIdxComplement;
      return 1;
    }
  }

  VSX_DEBUGLOG3("waiting for data %s qwridx:%d/%d, comp qwridx:%d\n", pData == &((STREAM_PES_T *)pData->pPes)->vid ? "vid" : "aud", qWrIdx, pData->lastQWrIdx, qWrIdxComplement);
  rc = pktqueue_waitforunreaddata(pQ);

  VSX_DEBUGLOG3("waiting for data %s done:%d\n", &((STREAM_PES_T *)pData->pPes)->vid ? "vid" : "aud", rc);

  return rc;
}

enum STREAM_NET_ADVFR_RC stream_net_av_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {

  STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pArg->pArgIn;
  STREAM_PES_T *pPes = (STREAM_PES_T *) pData->pPes;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  PKTQUEUE_T *pQ;
  const PKTQUEUE_PKT_T *pQPkt;

  if(!pData->pDataSrc || !pData->pDataSrc->pCbData) {
    if(pArg->plen) {
      *pArg->plen = 0;
    }
    return STREAM_NET_ADVFR_RC_NOCONTENT;
  }

  if(pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 0;
  }

  //fprintf(stderr, "av_advanceFrame called for %s stype:0x%x\n",  pData == &pPes->vid ? "vid" : "aud", pData->pXcodeData->inStreamType);

  waitfordata(pData);

  pQ = (PKTQUEUE_T *) pData->pDataSrc->pCbData;

  if(!pktqueue_havepkt(pQ) || !(pQPkt = pktqueue_readpktdirect(pQ))) {
    //fprintf(stderr, "ad_advanceFrame NOTAVAIL qid:%d haveData:%d wr-1:%d\n", pQ->cfg.id, pQ->haveData, pQ->uniqueWrIdx - 1);
    pktqueue_readpktdirect_done(pQ);
    return STREAM_NET_ADVFR_RC_NOTAVAIL;
  }

  //
  // Avoid memcpy of the frame data
  //
  if(pktqueue_swapreadslots(pQ, &pData->pXcodeData->curFrame.pSwappedSlot) < 0) {
    LOG(X_ERROR("Failed to swap slot in queue id:%d"), pQ->cfg.id);
    pktqueue_readpktdirect_done(pQ);
    return STREAM_NET_ADVFR_RC_ERROR;
  } else {
    pQPkt = pData->pXcodeData->curFrame.pSwappedSlot;
  }

  //fprintf(stderr, "stream_net_av advanceFr pQ[%d] len:%d\n", pQ->cfg.id, pQPkt->len);

  // Note this is just a shallow copy, not of the frame data contents
  memcpy(&pData->pXcodeData->curFrame.pkt, pQPkt, sizeof(pData->pXcodeData->curFrame.pkt));

  //fprintf(stderr, "ad_advanceFrame got fr len:%d wrIdx:%d pQ->userDataType:0x%x\n", pQPkt->len,pData->pXcodeData->curFrame.pkt.idx, pQ->cfg.userDataType); 

  pData->pXcodeData->curFrame.idxReadInFrame = 0;
  pData->pXcodeData->curFrame.idxReadFrame = pQ->idxRd;

  //fprintf(stderr, "AVREAD PKT FROM Q pts:%.3f dts:%.3f\n", PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.dts));

  pktqueue_readpktdirect_done(pQ);

#if 1
  //
  // If we're reading video & audio fromlive capture, and the input video sequence headers have not yet been set,
  // then just keep advancing the audio queue rdr position, otherwise, we keep on queing audio frames,
  // then when the vid seq start has been detected, the audio frames will be read, but by then it's possible
  // the audio queue has been reset, overwriting some 'to-be-played' audio frames
  //
  //fprintf(stderr, "TRY... vid:%d, xcodevid:%d, vid in-seq:%d, complement:0x%x\n", IS_STREAM_PES_DATA_VID(pData), pData->pXcodeData->piXcode->vid.common.cfgDo_xcode,  ((STREAM_XCODE_VID_UDATA_T *) pData->pXcodeData->piXcode->vid.pUserData)->haveSeqStart, getComplement(pData));
  STREAM_AV_DATA_T *pDataComplement;
  PKTQUEUE_T *pQComplement;
  
  if(IS_STREAM_PES_DATA_VID(pData) && pData->pXcodeData->piXcode->vid.common.cfgDo_xcode &&
     !((STREAM_XCODE_VID_UDATA_T *) pData->pXcodeData->piXcode->vid.pUserData)->haveSeqStart &&
     (pDataComplement = getComplement(pData)) &&
     (pQComplement = (PKTQUEUE_T *) pDataComplement->pDataSrc->pCbData)) {

    // Only retain the last 'x' elements in the queue... to prevent any overwrite / reset

    //fprintf(stderr, "Setting av complement reader to catchup.  haveFrameTmOffset:%d %.3f\n", pDataComplement->pXcodeData->haveFrameTmStartOffset, PTSF(pDataComplement->pXcodeData->frameTmStartOffset));

    pktqueue_setrdr(pQComplement, 1);

    if(!pDataComplement->pXcodeData->haveFrameTmStartOffset) {

      pthread_mutex_lock(&pQComplement->mtx);

      if(pQComplement->idxRd != pQComplement->idxWr) {
        LOG(X_DEBUG("Setting av %s stream "MP2PES_STREAMTYPE_FMT_STR" pts start offset from %.3f to %.3f"),
           IS_STREAM_PES_DATA_VID(pData) ? "audio" : "video",
           MP2PES_STREAMTYPE_FMT_ARGS(pData->pXcodeData->inStreamType), 
           PTSF(pData->pXcodeData->frameTmStartOffset),
           PTSF(pQComplement->pkts[pQComplement->idxRd].xtra.tm.pts));

        LOG(X_DEBUG("Setting av complement to idxRd:%d, idxWr:%d, rd:%.3f, wr:%.3f, wr-1:%.3f"), pQComplement->idxRd, pQComplement->idxWr, PTSF(pQComplement->pkts[pQComplement->idxRd].xtra.tm.pts), PTSF(pQComplement->pkts[pQComplement->idxWr].xtra.tm.pts), PTSF(pQComplement->pkts[pQComplement->idxWr == 0 ? pQComplement->cfg.maxPkts-1 : pQComplement->idxWr -1].xtra.tm.pts));
        pDataComplement->pXcodeData->frameTmStartOffset = pQComplement->pkts[pQComplement->idxRd].xtra.tm.pts;
        pDataComplement->pXcodeData->haveFrameTmStartOffset = 1;
      }

      pthread_mutex_unlock(&pQComplement->mtx);

    }

    //pDataComplement->pXcodeData->frameTmStartOffset = pData->pXcodeData->frameTmStartOffset;
  }
#endif // 0

  VSX_DEBUGLOG3("lastQWrIdx now:%d\n", pData->lastQWrIdx);

  memcpy(&pData->curPesTm.qtm, &pData->pXcodeData->curFrame.pkt.xtra.tm, 
         sizeof(pData->curPesTm.qtm));

  // Do not use PKTQUEUE_T pkt contents directly to allow for any
  // prebuf contents such as SPS / PPS packaged w/ each I-frame    
  pData->pXcodeData->curFrame.pData = pData->pXcodeData->curFrame.pkt.pData;    
  pData->pXcodeData->curFrame.lenData = pData->pXcodeData->curFrame.pkt.len;
  pArg->isvid = (pData == &pPes->vid) ? 1 : 0;

  if(pArg->plen) {
    *pArg->plen = pData->pXcodeData->curFrame.lenData;
  }
  
  if((rc = checktiming(pData)) != STREAM_NET_ADVFR_RC_OK) {
    stream_net_av_reset(pData->pPes);
  }

  if(pArg->pPts) {
    *pArg->pPts = xcode_getFrameTm(pData->pXcodeData, 0, 0);
    //fprintf(stderr, "AV curF: pts:%.3f dts:%.3f start:%.3f, fr:%.3f (%llu)\n", PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.dts), PTSF(pData->pXcodeData->frameTmStartOffset), PTSF(*pArg->pPts), *pArg->pPts);
  }


  if(pArg->pkeyframeIn && (pData->pXcodeData->curFrame.pkt.flags & PKTQUEUE_FLAG_KEYFRAME)) {
    //k(pData->pXcodeData->curFrame.pkt.xtra.flags & CAPTURE_SP_FLAG_KEYFRAME)) {
    *pArg->pkeyframeIn = 1;
  } else {
    *pArg->pkeyframeIn = 0;
  }

  pArg->codecType = pQ->cfg.userDataType;

  //LOG(X_DEBUG("av_advanceFrame %s key:%d rc:%d  pts:%.3f (%.3f) (dts:%.3f) start:%.3f len:%u, Q[rd:%d,wr:%d/%d]"), pData == &pPes->vid ? "vid" : "aud", *pArg->pkeyframeIn, rc, PTSF(*pArg->pPts), PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.dts), PTSF(pData->pXcodeData->frameTmStartOffset), pData->pXcodeData->curFrame.lenData, pQ->idxRd, pQ->idxWr, pQ->cfg.maxPkts); 


  return rc;
}

int64_t stream_net_av_frameDecodeHzOffset(void *pArg) {
  STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pArg;
  int64_t delta = 0;

  if(pData->pXcodeData->curFrame.pkt.xtra.tm.dts > 0) {
    delta = pData->pXcodeData->curFrame.pkt.xtra.tm.pts - 
            pData->pXcodeData->curFrame.pkt.xtra.tm.dts;
  }

  return delta;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_av_getNextSlice(void *pArg) {
  static MP4_MDAT_CONTENT_NODE_T content;
  // return dummy to avoid NULL
  return &content;
}

int stream_net_av_szNextSlice(void *pArg) {
  //fprintf(stderr, "szNextSlice\n");
  return 0;
}

int stream_net_av_haveMoreData(void *pArg) {
  //STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pArg;

  return 1;
}

int stream_net_av_haveDataNow(void *pArg) {
  STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pArg;
  PKTQUEUE_T *pQ = NULL;
  uint32_t qWrIdx; 

  if(!pData->pDataSrc || !pData->pDataSrc->pCbData) {
    //fprintf(stderr, "stream_net_av_haveDataNow NULL...\n");
    return 0;
  }

  pQ = (PKTQUEUE_T *) pData->pDataSrc->pCbData;

  qWrIdx = pktqueue_havepkt(pQ);

  //fprintf(stderr, "%llu.%llu stream_net_av_haveDataNow qid:%d, qWrIdx:%d, lastQWrIdx:%d\n", timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pQ->cfg.id, qWrIdx, pData->lastQWrIdx);

  if(qWrIdx != 0 && qWrIdx != pData->lastQWrIdx) {
    //fprintf(stderr, "stream_net_av_haveDataNow qid:%d, qWrIdx:%d, lastQWrIdx:%d returning 1\n", pQ->cfg.id, qWrIdx, pData->lastQWrIdx);
    return 1;
  }

  return 0;
}

int stream_net_av_haveNextSlice(void *pArg) {
  return 0;
}


int stream_net_av_getReadSliceBytes(void *pArg) {
  return 0;
}


int stream_net_av_getUnreadSliceBytes(void *pArg) {
  STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pArg;
  uint32_t szSlice;

  if(pData->numFrames < 1) {
    szSlice = 0;
  } else {
    szSlice = pData->pXcodeData->curFrame.lenData - pData->pXcodeData->curFrame.idxReadInFrame;
  }

  return szSlice;
}


int stream_net_av_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pArg;
  PKTQUEUE_T *pQ = (PKTQUEUE_T *) pData->pDataSrc->pCbData;
  int rc = 0;
  int szread = 0;

  // this is stupid... pktqueue read in advanceFrame should 'lease' the pktq node
  // and use it here

  pktqueue_lock(pQ, 1);

  // Check to make sure the contents being partially read have not been
  // overwritten by a subsequent pktqueue write operation
  if(pQ->pkts[pData->pXcodeData->curFrame.idxReadFrame].idx ==
                        pData->pXcodeData->curFrame.pkt.idx) {

    if((szread = pData->pXcodeData->curFrame.lenData -                 
       pData->pXcodeData->curFrame.idxReadInFrame) < 0) {
      LOG(X_ERROR("%s packet queue read invalid index [%d + %d / %d]"),
        IS_STREAM_PES_DATA_VID(pData) ? "video" : "audio",
        pData->pXcodeData->curFrame.idxReadInFrame, len, pData->pXcodeData->curFrame.lenData
);
      szread = -1;
      rc = -1;
    } else if((int) len > szread) {
      LOG(X_WARNING("%s packet queue read invalid length idx[%d + %d / %d]"),
                 IS_STREAM_PES_DATA_VID(pData) ? "video" : "audio",
                 pData->pXcodeData->curFrame.idxReadInFrame, len,
                 pData->pXcodeData->curFrame.lenData);
      len = szread;
    } else if(szread > (int) len) {
      szread = len;
    }
    if(rc == 0) {
      memcpy(pBuf, &pData->pXcodeData->curFrame.pData[pData->pXcodeData->curFrame.idxReadInFrame], szread);
      pData->pXcodeData->curFrame.idxReadInFrame += szread;
      if(pData->pXcodeData->curFrame.idxReadInFrame >= pData->pXcodeData->curFrame.lenData) 
{
      }
    }

  } else {
    LOG(X_WARNING("%s packet queue[id:%d] (sz:%d, rd:%d, wr:%d) read contents have been "
                  "overwritten during partial read"),
                  IS_STREAM_PES_DATA_VID(pData),
                  pQ->cfg.id, pQ->cfg.maxPkts, pQ->idxRd, pQ->idxWr);
    szread = -1;
  }

  pktqueue_lock(pQ, 0);

  return szread;
}



void stream_net_av_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_LIVE;
  pNet->cbAdvanceFrame = stream_net_av_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_av_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_av_getNextSlice;
  pNet->cbHaveMoreData = stream_net_av_haveMoreData;
  pNet->cbHaveDataNow = stream_net_av_haveDataNow;
  pNet->cbHaveNextFrameSlice = stream_net_av_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_av_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_av_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_av_getReadSliceBytes;
  pNet->cbCopyData = stream_net_av_getSliceBytes;
}

#endif // VSX_HAVE_STREAMER
