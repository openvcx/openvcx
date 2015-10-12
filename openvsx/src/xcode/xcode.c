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

uint64_t xcode_getFrameTm(const STREAM_XCODE_DATA_T *pData, int absolute, int64_t tmarg) {

  int64_t tm;

  if(tmarg != 0) {
    tm = tmarg;
  } else {
    if(pData->curFrame.pkt.xtra.tm.dts != 0) {    
      tm = pData->curFrame.pkt.xtra.tm.dts;
    } else {
      tm = pData->curFrame.pkt.xtra.tm.pts;
    }
  }

  if(!absolute) {
  
    if(tm >= pData->frameTmStartOffset) {
      tm -= pData->frameTmStartOffset;
    } else {
      LOG(X_WARNING("getFrameTm < 0 %.3f, start: %.3f"), PTSF(tm), PTSF(pData->frameTmStartOffset));
      tm = 0;
    }
  }

  VSX_DEBUG_XCODE( 
    LOG(X_DEBUGV("xcode_getFrameTm getFrameTm %.3f tmarg: %.3f, absolute: %d, (frameTmSt:%.3f) (pts:%.3f,dts:%.3f) (xout.tm.pts: %.3f) durationTotPrior: %.3f"), PTSF(tm), PTSF(tmarg), absolute, PTSF(pData->frameTmStartOffset), PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.pkt.xtra.tm.dts), PTSF(pData->curFrame.xout.tms[0].pts), PTSF(pData->curFrame.tm.durationTotPrior)); );

  return (uint64_t) tm;
}


int xcodectxt_allocbuf(STREAM_XCODE_CTXT_T *pXcodeCtxt, unsigned int vidSz, const PKTQUEUE_CONFIG_T *pPipFrQCfg) {
  int rc = 0;
  IXCODE_VIDEO_CTXT_T *pXcode = &pXcodeCtxt->vidData.piXcode->vid;

  if(!pXcodeCtxt) {
    return -1;
  }

  //
  // Although encoded output may be stored in the same queue slot as the input
  // vidUData.tmpFrame is used to store upsampled frame content
  // tmpFrame may also store output for large RGB/YUV output
  //
  if(vidSz == 0) {
    pXcodeCtxt->vidUData.tmpFrame.allocSz = (unsigned int) ((FRAMESZ_VIDQUEUE_SZFRAME_DFLT) *
                                            (FRAMESZ_GROW_MULTIPLIER));
  } else {
    LOG(X_DEBUG("Allocating encoded output buffer size %d"), vidSz);
    pXcodeCtxt->vidUData.tmpFrame.allocSz = vidSz;
    pXcodeCtxt->vidData.curFrame.useTmpFrameOut = 1;
  }

  //
  // If this is an active PIP
  //
  if(pPipFrQCfg && pPipFrQCfg->maxPktLen > 0) {
    if((pXcodeCtxt->vidUData.pPipFrQ = pktqueue_create(pPipFrQCfg->maxPkts, pPipFrQCfg->maxPktLen,
                                                       pPipFrQCfg->maxPkts, pPipFrQCfg->growMaxPktLen,
                                                       0, sizeof(OUTFMT_FRAME_DATA_T), 1))) {
      pktqueue_setrdr(pXcodeCtxt->vidUData.pPipFrQ, 0);
      pXcodeCtxt->vidUData.pPipFrQ->cfg.id = STREAMER_QID_PIPVID;
      pXcodeCtxt->vidUData.pPipFrQ->cfg.overwriteType = PKTQ_OVERWRITE_FIND_KEYFRAME;
      LOG(X_DEBUG("Created pip vid frameq[%d] %d x slotsz:%d, slotszmax:%d"),
          pXcodeCtxt->vidUData.pPipFrQ->cfg.id, pXcodeCtxt->vidUData.pPipFrQ->cfg.maxPktLen,
          pXcodeCtxt->vidUData.pPipFrQ->cfg.maxPktLen, pXcodeCtxt->vidUData.pPipFrQ->cfg.growMaxPktLen);
    }
  }

  if(pXcode && pXcode->out[0].passthru) {
    //
    // If useTmpFrameOut is not enabled, xcode encode puts output into same buffer as input
    //
    pXcodeCtxt->vidData.curFrame.useTmpFrameOut = 1;
  }

#if defined(XCODE_IPC)
  if(pXcodeCtxt->vidUData.tmpFrame.allocSz > IXCODE_SZFRAME_MAX) {
    LOG(X_ERROR("Output buffer size %d exceeds max ipc segment size %d"),
                pXcodeCtxt->vidUData.tmpFrame.allocSz, IXCODE_SZFRAME_MAX);
    return -1;
  }
#endif // XCODE_IPC

  if(!(pXcodeCtxt->vidUData.tmpFrame.pBuf =
       avc_calloc(1, FRAMESZ_VIDQUEUE_PREBUF_BYTES + 
                  pXcodeCtxt->vidUData.tmpFrame.allocSz))) {
    xcodectxt_close(pXcodeCtxt);
    return -1;
  }
  pXcodeCtxt->vidUData.tmpFrame.pData = pXcodeCtxt->vidUData.tmpFrame.pBuf +
                                        FRAMESZ_VIDQUEUE_PREBUF_BYTES;

  // 
  // Create tmpFrame.pBuf to hold any encoder input
  // This is created even if audio transcoding is not enabled to allow audio frames
  // to be treated transparently as transcoded frames by stream_rtp_mp2ts packetizer
  //
  pXcodeCtxt->audUData.tmpFrame.allocSz = 16384;
  if(!(pXcodeCtxt->audUData.tmpFrame.pBuf =
       avc_calloc(1, pXcodeCtxt->audUData.tmpFrame.allocSz))) {
    xcodectxt_close(pXcodeCtxt);
    return -1;
  }
  pXcodeCtxt->audUData.tmpFrame.pData = pXcodeCtxt->audUData.tmpFrame.pBuf;

  return rc;
}

int xcodectxt_init(STREAM_XCODE_CTXT_T *pXcodeCtxt, float vidFpsOut, int aud, int allowUpsample,
                   unsigned int fpsInNHint, unsigned int fpsInDHint) {
  int rc = 0;

  if(!pXcodeCtxt 
    || !pXcodeCtxt->vidUData.tmpFrame.pBuf 
    || !pXcodeCtxt->audUData.tmpFrame.pBuf
    ) {
    return -1;
  }

  pXcodeCtxt->vidData.pComplement = &pXcodeCtxt->audData;
  pXcodeCtxt->audData.pComplement = &pXcodeCtxt->vidData;

  if(fpsInNHint > 0 && fpsInDHint > 0) {
    pXcodeCtxt->vidData.piXcode->vid.inClockHz = fpsInNHint;
    pXcodeCtxt->vidData.piXcode->vid.inFrameDeltaHz = fpsInDHint;
  }

  //
  // Enable AUD NALs for httplive playback 
  //
  //pXcodeCtxt->vidUData.includeAUD = 1;

  //pXcodeCtxt->vidData.curFrame.maxLenData = pXcodeCtxt->vidUData.tmpFrame.allocSz;

  if(aud) {
    pXcodeCtxt->audUData.szEncodedBuf = sizeof(pXcodeCtxt->audUData.encodedBuf);

    //
    // Allocate the buffer to store full audio frames to be decoded
    // This is necessary for block based audio codecs such as ac3,
    // where entire input audio blocks can be chunked across PES packets
    //
    pXcodeCtxt->audUData.bufLen = pXcodeCtxt->audUData.tmpFrame.allocSz;
    pXcodeCtxt->audUData.pBuf = (unsigned char *) avc_calloc(1, pXcodeCtxt->audUData.bufLen);

    //
    // Clients such as quicktime (HTTP Live Streaming) appear to only play
    // the audio if the audio PES rate is > than the video frame rate
    // regardless of PCR pid
    //
    //if(vidFpsOut > 0) {
    //  pXcodeCtxt->audUData.minEncodedPesDeltaPts = (uint64_t) (1.1 * 90000.0f * vidFpsOut);
    //  if(pXcodeCtxt->audUData.minEncodedPesDeltaPts > (90000/20)) {
    //    pXcodeCtxt->audUData.minEncodedPesDeltaPts = (90000/20);
    //  }
    //}
  }

  return rc;
}

int xcodectxt_close(STREAM_XCODE_CTXT_T *pXcodeCtxt) {
  int rc = 0;

  if(!pXcodeCtxt) {
    return -1;
  }

  if(pXcodeCtxt->audUData.pBuf) {
    avc_free((void **) &pXcodeCtxt->audUData.pBuf);
  }

  if(pXcodeCtxt->vidUData.tmpFrame.pBuf) {
    avc_free((void **) &pXcodeCtxt->vidUData.tmpFrame.pBuf);
  }
  pXcodeCtxt->vidData.curFrame.useTmpFrameOut = 0;

  if(pXcodeCtxt->audUData.tmpFrame.pBuf) {
    avc_free((void **) &pXcodeCtxt->audUData.tmpFrame.pBuf);
  }

  //
  // Swapped queue slot may have been created via pktqueue_create
  // but when pktqueue_swapreadslots was called, became owned
  // by pXcodeCtxt
  //
  if(pXcodeCtxt->vidData.curFrame.pSwappedSlot &&
     pXcodeCtxt->vidData.curFrame.pSwappedSlot->pBuf) {
    avc_free((void **) &pXcodeCtxt->vidData.curFrame.pSwappedSlot->pBuf);
  }

  if(pXcodeCtxt->audData.curFrame.pSwappedSlot &&
     pXcodeCtxt->audData.curFrame.pSwappedSlot->pBuf) {
    avc_free((void **) &pXcodeCtxt->audData.curFrame.pSwappedSlot->pBuf);
  }

  if(pXcodeCtxt->vidUData.pPipFrQ) {
    pktqueue_destroy(pXcodeCtxt->vidUData.pPipFrQ);
    pXcodeCtxt->vidUData.pPipFrQ = NULL;
  }


  return rc;
}

enum XCODE_CFG_ENABLED xcode_enabled(int printerror) {
  enum XCODE_CFG_ENABLED xcode_enabled = XCODE_CFG_NOTCOMPILED;
#if defined(WIN32)
  HANDLE hSem;
#endif // WIN32

#if defined(ENABLE_XCODE)

  xcode_enabled = XCODE_CFG_COMPILED;

#if defined(XCODE_IPC)

  xcode_enabled = XCODE_CFG_IPC_NOTRUNNING;

  //
  // Check for existance of shared mem segment created by xcoder process
  //

#if defined(WIN32)

  if((hSem = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE,
                               FALSE, XCODE_SEM_CLI_NAME)) != NULL) {
    CloseHandle(hSem);
    xcode_enabled = XCODE_CFG_IPC_RUNNING;
  }

#else // WIN32

#if defined(__linux__)
  int flags = 0x180;
#else // __linux__
  int flags = SEM_R | SEM_A;
#endif // __linux__
  int shmid;
  if((shmid = shmget(XCODE_IPC_SHMKEY, 0, flags)) != -1) {
    xcode_enabled = XCODE_CFG_IPC_RUNNING;
  }

#endif // WIN32

#endif // XCODE_IPC

#endif // ENABLE_XCODE

  if(printerror) {
    switch(xcode_enabled) {
      case XCODE_CFG_IPC_NOTRUNNING:
        LOG(X_ERROR("Transcoding currently unavailable - vsxxcode is not running."));
        break;
      case XCODE_CFG_NOTCOMPILED:
        LOG(X_ERROR(VSX_XCODE_DISABLED_BANNER));
        LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
        break;
      default:
        break; 
    }
  }

  return xcode_enabled;
}

static void xtime_frame_reset(STREAM_XCODE_DATA_T *pData)  {

  //fprintf(stderr, "xtime_frame_reset\n");

  pData->curFrame.tm.waspaused = 1;
  pData->curFrame.tm.cfrInDriftAdj = 0;
  pData->curFrame.tm.cfrOutDriftAdj = 0;
  //pData->curFrame.tm.encDelay = 0;

  pData->curFrame.tm.haveseqstart = 0;
  pData->curFrame.tm.seqstartoffset = 0;
  pData->curFrame.tm.prevhaveseqstart = 0;
  ((STREAM_XCODE_VID_UDATA_T *)pData->piXcode->vid.pUserData)->haveSeqStart = 0;
  ((STREAM_XCODE_AUD_UDATA_T *)pData->piXcode->aud.pUserData)->haveSeqStart = 0;

  pData->curFrame.tm.frDecOutPeriod = 0;
  pData->curFrame.tm.frEncOutPeriod = 0;
  pData->curFrame.tm.frPrevDecOutPeriod = 0;
  pData->curFrame.tm.frPrevEncOutPeriod = 0;

}

int xcode_checkresetresume(STREAM_XCODE_DATA_T *pXcode) {
  int rc = 0;

  if(!pXcode->curFrame.tm.havestarttm0) {
    //pXcode->curFrame.tm.starttm0 = pXcode->curFrame.pkt.xtra.tm.pts;
    pXcode->curFrame.tm.starttmperiod = pXcode->curFrame.pkt.xtra.tm.pts;
    pXcode->curFrame.tm.havestarttm0 = 1;
    pXcode->curFrame.tm.starttv0 = timer_GetTime();
  }

  LOG(X_DEBUGV("xcode_checkresetresume waspaused: %d, starttv0: %llu"), 
       pXcode->curFrame.tm.waspaused, pXcode->curFrame.tm.starttv0);

  if(pXcode->curFrame.tm.waspaused) {
    pXcode->curFrame.tm.durationTotPrior = (uint64_t)
                 ((double)(timer_GetTime() - pXcode->curFrame.tm.starttv0) * .09f);
    LOG(X_DEBUG("xtime_frame was reset, durationTotPrior set to %.3f"),
                PTSF(pXcode->curFrame.tm.durationTotPrior));
    if(pXcode->pComplement) {
      pXcode->pComplement->curFrame.tm.durationTotPrior = pXcode->curFrame.tm.durationTotPrior;
    }
    pXcode->curFrame.tm.waspaused = 0;
    pXcode->curFrame.tm.starttmperiod = pXcode->curFrame.pkt.xtra.tm.pts;

    //pData->curFrame.pkt.xtra.tm.pts = 0; pData->curFrame.pkt.xtra.tm.dts = 0;
  }

  return rc;
}


int xcode_resetdata(STREAM_XCODE_DATA_T *pXcode) {

  STREAM_XCODE_VID_UDATA_T *pUdataV;
  STREAM_XCODE_AUD_UDATA_T *pUdataA;

  if(!pXcode) {
    return -1;
  }

  pXcode->curFrame.idxReadInFrame = 0;
  pXcode->curFrame.idxReadFrame = 0;
  memset(&pXcode->curFrame.pkt, 0, sizeof(pXcode->curFrame.pkt));
  pXcode->curFrame.pData = NULL;
  pXcode->curFrame.lenData = 0;
#if 1
  memset(pXcode->curFrame.xout.outbuf.lens, 0, sizeof(pXcode->curFrame.xout.outbuf.lens));
#endif

  memset(&pXcode->curFrame.xout.keyframes, 0, sizeof(pXcode->curFrame.xout.keyframes));
  pXcode->curFrame.upsampleFrIdx = 0;
  pXcode->curFrame.numUpsampled = 0;
  //memset(&pXcode->curFrame.lastOutTm, 0, sizeof(pXcode->curFrame.lastOutTm));

  pXcode->curFrame.idxCompoundFrame = 0;
  pXcode->curFrame.numCompoundFrames = 0;
  memset(&pXcode->curFrame.compoundFrames, 0, sizeof(pXcode->curFrame.compoundFrames));

  pXcode->frameTmStartOffset = 0;
  pXcode->haveFrameTmStartOffset = 0;
  pXcode->numFramesOut = 0;

  pXcode->inPts90KhzAdj = 0;
  pXcode->consecErr = 0;

  xtime_frame_reset(pXcode);

  if(pXcode->piXcode) {
    pUdataV = (STREAM_XCODE_VID_UDATA_T *) pXcode->piXcode->vid.pUserData;
    pUdataA = (STREAM_XCODE_AUD_UDATA_T *) pXcode->piXcode->aud.pUserData;

    if(pXcode->piXcode->vid.pUserData) {
      //pXcode->curFrame.maxLenData = ((PESXCODE_VID_UDATA_T *) 
      //                         pXcode->piXcode->vid.pUserData)->tmpFrame.allocSz;
      //TODO: do  close/reset function for this stuff
      if(pXcode->piXcode->vid.common.cfgDo_xcode &&
         pXcode->piXcode->vid.common.cfgFileTypeIn == XC_CODEC_TYPE_MPEG4V) {
        memset(&pUdataV->uin.xmpg4v, 0, sizeof(pUdataV->uin.xmpg4v));
      }
    } 

    if(pUdataA) {
      //pXcode->curFrame.maxLenData = pUdataA->tmpFrame.allocSz;
      pUdataA->outHz = 0;
      pXcode->numFramesOut = 0;

      //TODO: do a close/reset function for this stuff
      if(pXcode->piXcode->aud.common.cfgDo_xcode) {
        pUdataA->bufIdx = 0;
        memset(&pUdataA->u, 0, sizeof(pUdataA->u));
      }
    }
  }

  return 0;
}

int xcode_update(STREAMER_CFG_T *pStreamerCfg, const char *strxcode, enum STREAM_XCODE_FLAGS flags) {
  int rc = 0;
  enum STREAM_NET_ADVFR_RC rcxcode;
  STREAM_XCODE_DATA_T xcodeData;
  IXCODE_CTXT_T xcodeCtxt;
  STREAM_XCODE_VID_UDATA_T *pUDataV;

  if(!pStreamerCfg) {
    return -1;
  }

//TODO: need to do xcode update frmo encoder thread... not from server thread

  memset(&xcodeData, 0, sizeof(xcodeData));
  memset(&xcodeCtxt, 0, sizeof(xcodeCtxt));

  pthread_mutex_lock(&pStreamerCfg->mtxStrmr);

  pUDataV = (STREAM_XCODE_VID_UDATA_T *) pStreamerCfg->xcode.vid.pUserData;
  memcpy(&xcodeCtxt, &pStreamerCfg->xcode, sizeof(xcodeCtxt));
  xcodeData.piXcode = &pStreamerCfg->xcode;

  if(rc >= 0 && pStreamerCfg->xcode.vid.common.cfgDo_xcode && strxcode) {
    if((rc = xcode_parse_configstr_vid(strxcode, &xcodeCtxt.vid, 1, 0)) > 0) {
      if(rc >= 0 && (rcxcode = xcode_vid_update(&xcodeData, &xcodeCtxt.vid)) != STREAM_NET_ADVFR_RC_OK) {
        rc = -1;
      }
    }
  }

  if(rc >= 0 && pStreamerCfg->xcode.aud.common.cfgDo_xcode && strxcode) {

    if((rc = xcode_parse_configstr_aud(strxcode, &xcodeCtxt.aud, 1, 0)) > 0) {
    
      if(rc >= 0 && (rcxcode = xcode_aud_update(&xcodeData, &xcodeCtxt.aud)) != STREAM_NET_ADVFR_RC_OK) {
        rc = -1;
      }

    }

  }

  if(rc >= 0 && pUDataV && (flags & STREAM_XCODE_FLAGS_RESET)) {
    pUDataV->flags |= (flags & STREAM_XCODE_FLAGS_RESET);
  }

  pthread_mutex_unlock(&pStreamerCfg->mtxStrmr);

  return rc;
}
