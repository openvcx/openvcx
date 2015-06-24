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

int moofsrv_record_start(MOOFSRV_CTXT_T *pMoofCtxt, STREAMER_OUTFMT_T *pLiveFmt) {
  int rc = 0;
  unsigned int numQFull = 0;

  if(!pMoofCtxt || !pLiveFmt) {
    return -1;
  }

  //
  // Add a livefmt cb
  //
  if((pMoofCtxt->pRecordOutFmt = outfmt_setCb(pLiveFmt, moofsrv_addFrame, pMoofCtxt,
                                         &pLiveFmt->qCfg, NULL, 0, 0, &numQFull))) {

    if((rc = moofsrv_init(pMoofCtxt, pLiveFmt->qCfg.growMaxPktLen)) < 0) {
      return rc;
    }
    //pMoofCtxt->cbWrite = moofsrv_cbWriteDataFile;

    LOG(X_INFO("Starting dash recording[%d] %d/%d"), pMoofCtxt->pRecordOutFmt->cbCtxt.idx,
        numQFull + 1, pLiveFmt->max);

  } else {
    LOG(X_ERROR("No mooflive recording resource available (max:%d)"), (pLiveFmt ? pLiveFmt->max : 0));
    rc = -1;
  }

  return rc;
}

int moofsrv_record_stop(MOOFSRV_CTXT_T *pMoofCtxt) {
  int rc = 0;

  if(!pMoofCtxt) {
    return -1;
  }

  //
  // Remove the livefmt cb
  //
  if(pMoofCtxt->pRecordOutFmt) {
    outfmt_removeCb(pMoofCtxt->pRecordOutFmt);
    pMoofCtxt->pRecordOutFmt = NULL;
  }

  moofsrv_close(pMoofCtxt);

  return rc;
}

int moofsrv_init(MOOFSRV_CTXT_T *pMoof, unsigned int vidTmpFrameSz) {
  int rc = 0;

  pMoof->state = MOOFSRV_STATE_INVALID;

  memset(&pMoof->createCtxt, 0, sizeof(pMoof->createCtxt));

  //
  // useRandomAccessPoints attempts to break each mp4 on keyframes, which is only 
  // possible if the video stream is present
  //
  if(*pMoof->pnovid) {
    pMoof->moofInitCtxt.useRAP = 0;
  }

  if((rc = mp4moof_init(&pMoof->createCtxt, 
                        &pMoof->av, 
                        &pMoof->moofInitCtxt,
                        &pMoof->dashInitCtxt)) < 0) {
    return rc;
  }

  if(vidTmpFrameSz == 0) {
    vidTmpFrameSz = STREAMER_RTMPQ_SZSLOT_MAX;
  }
  if((codecfmt_init(&pMoof->av, vidTmpFrameSz, 0)) < 0) {
    moofsrv_close(pMoof);
    return -1;
  }

  //pFlv->cbWrite = moofsrv_cbWriteDataNet;

  return rc;
}

int moofsrv_close(MOOFSRV_CTXT_T *pMoof) {
  int rc = 0;

  if(!pMoof) {
    return -1;
  }

  rc = mp4moof_close(&pMoof->createCtxt);

  //fprintf(stderr, "MOOFSRV_CLOSE psps: 0x%x\n", pMoof->av.vid.codecCtxt.h264.avcc.psps);
  codecfmt_close(&pMoof->av);

  return rc;
}

int moofsrv_addFrame(void *pArg, const OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;
  MOOFSRV_CTXT_T *pMoofCtxt = (MOOFSRV_CTXT_T *) pArg;
  OUTFMT_FRAME_DATA_T frame;
  int keyframe = 0;

  //LOG(X_DEBUG("MOOF - moofsrv_addFrame[0x%x,%d,%d] %s len:%d pts:%llu (%.3f) dts:%lld (%.3f) key:%d (vseq:%d, aseq:%d, sent:%d)"), pMoofCtxt, pMoofCtxt->requestOutIdx, OUTFMT_IDX(pFrame), pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), OUTFMT_LEN(pFrame), OUTFMT_PTS(pFrame), PTSF(OUTFMT_PTS(pFrame)), OUTFMT_DTS(pFrame), PTSF(OUTFMT_DTS(pFrame)), keyframe, pMoofCtxt->av.vid.haveSeqHdr, pMoofCtxt->av.aud.haveSeqHdr, pMoofCtxt->av.vid.sentSeqHdr);

  if((pFrame->isvid && *pMoofCtxt->pnovid) || (pFrame->isaud && *pMoofCtxt->pnoaud)) {
    return rc;
  }

  //LOG(X_DEBUG("MOOF [outidx:%d] %s vid.codecType: 0x%x aud.codecType: 0x%x"), pMoofCtxt->requestOutIdx, pFrame->isvid ? "video" : "audio", pMoofCtxt->av.vid.codecType, pMoofCtxt->av.aud.codecType);

  if(pFrame->isvid) {

    //
    // since pFrame is read-only, create a copy with the appropriate xcode out index
    // Be advised, avoid this for audio frames since only the 0th index data and timestamp info is set for audio
    //
    if(pMoofCtxt->requestOutIdx != pFrame->xout.outidx) {
      memcpy(&frame, pFrame, sizeof(frame));
      frame.xout.outidx = pMoofCtxt->requestOutIdx;
      pFrame = &frame;
    }

    keyframe = OUTFMT_KEYFRAME(pFrame);

  }
//return 0;

  //if(keyframe)fprintf(stderr, "\n\n\n\n\nKEYFRAME\n\n\n\n");
  //fprintf(stderr, "moofsrv_addFrame %s len:%d pts:%llu dts:%lld key:%d (vseq:%d, aseq:%d, sent:%d)\n", pFrame->isvid ? "vid" : (pFrame->isaud ? "aud" : "?"), OUTFMT_LEN(pFrame), OUTFMT_PTS(pFrame), OUTFMT_DTS(pFrame), keyframe, pMoofCtxt->av.vid.haveSeqHdr, pMoofCtxt->av.aud.haveSeqHdr, pMoofCtxt->av.vid.sentSeqHdr); //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), 48, 1);

  if(OUTFMT_DATA(pFrame) == NULL) {
    LOG(X_ERROR("MOOF Frame [outidx:%d] data not set"), pFrame->xout.outidx);
    return -1;
  }

  if(pFrame->isvid && !pMoofCtxt->av.vid.haveSeqHdr) {

    if((rc = flvsrv_check_vidseqhdr(&pMoofCtxt->av.vid, pFrame, &keyframe)) <= 0) {

      //
      // Request an IDR from the underling encoder
      //
      if(pMoofCtxt->av.vid.pStreamerCfg) {
        streamer_requestFB(pMoofCtxt->av.vid.pStreamerCfg, pFrame->xout.outidx, ENCODER_FBREQ_TYPE_FIR, 0, REQUEST_FB_FROM_LOCAL);
      }

      return rc;
    }

  } else if(pFrame->isaud && !pMoofCtxt->av.aud.haveSeqHdr) {

    if((rc = flvsrv_check_audseqhdr(&pMoofCtxt->av.aud, pFrame)) <= 0) {
      return rc;
    }

  }

  if(!pMoofCtxt->av.vid.sentSeqHdr && (*pMoofCtxt->pnovid || pMoofCtxt->av.vid.haveSeqHdr) &&
     (*pMoofCtxt->pnoaud || pMoofCtxt->av.aud.haveSeqHdr)) {

    if(rc >= 0) {
      pMoofCtxt->av.vid.sentSeqHdr = 1;
      LOG(X_DEBUG("dashplay got [outidx:%d] audio / video start - beginning transmission"), 
                  pMoofCtxt->moofInitCtxt.requestOutidx);
    }

  }

  if(rc >= 0) {

    if(!pMoofCtxt->av.vid.sentSeqHdr) {

      //
      // TODO: we're going to end up missing the first GOP in the case we get a vid keyframe
      // and then an audio start... because we don't write the FLV header start until we have  both
      //
      if(pMoofCtxt->pRecordOutFmt) {
        pMoofCtxt->av.vid.haveKeyFrame = 0;
      }
  

      //TODO: output queue prior skipped non-key video frames

      //fprintf(stderr, "TRY ABORT this is keyframe:%d, haveKeyFr:%d \n", keyframe, pFlvCtxt->av.vid.haveKeyFrame);
      return rc;
    }

    rc = 0;

    if(pFrame->isvid) {

      //
      // Only start sending video content beginning w/ a keyframe
      //
      if(!pMoofCtxt->av.vid.haveKeyFrame) {
        if(keyframe) {
          pMoofCtxt->av.vid.haveKeyFrame = 1;
          pMoofCtxt->av.vid.nonKeyFrames = 0;
        } else {
          codec_nonkeyframes_warn(&pMoofCtxt->av.vid, "MOOF");
        }
      }

      if(pMoofCtxt->av.vid.haveKeyFrame) {
        //fprintf(stderr, "MOOFSRV_SEND_VIDFRAME len:%d, pts:%.3f,key:%d, rc:%d\n", OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), OUTFMT_KEYFRAME(pFrame), rc); //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), MIN(16, OUTFMT_LEN(pFrame)), 1);
        if(pMoofCtxt->do_moof_segmentor && (rc = mp4moof_addFrame(&pMoofCtxt->createCtxt, pFrame)) < 0) {
          LOG(X_ERROR("mp4 moof addFrame video failed"));
        }
//rc=-1;

      }

    } else if(pFrame->isaud) {

      if(pMoofCtxt->av.aud.tm0 == 0) {
        pMoofCtxt->av.aud.tm0 = pMoofCtxt->av.aud.tmprev = OUTFMT_PTS(pFrame);
      }
      //fprintf(stderr, "MOOFSRV_SEND_AUDIO len:%d, pts:%.3f (%llu), rc:%d\n", OUTFMT_LEN(pFrame), PTSF(OUTFMT_PTS(pFrame)), OUTFMT_PTS(pFrame), rc); //avc_dumpHex(stderr, OUTFMT_DATA(pFrame), MIN(16, OUTFMT_LEN(pFrame)), 1);
      if(pMoofCtxt->do_moof_segmentor && (rc = mp4moof_addFrame(&pMoofCtxt->createCtxt, pFrame)) < 0) {
        LOG(X_ERROR("mp4 moof addFrame audio failed"));
      }
//rc=-1;
     
    }

  }

  if(rc < 0) {
    pMoofCtxt->state = MOOFSRV_STATE_ERROR;
  }

  return rc;
}

