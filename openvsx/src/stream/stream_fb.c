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

static int xcode_requestFB(IXCODE_VIDEO_CTXT_T *pXcodeV, unsigned int outidx,
                    enum ENCODER_FBREQ_TYPE fbReqType, unsigned int msDelay) {
  int rc = 0;
  TIME_VAL tm;
  STREAM_XCODE_VID_UDATA_T *pUData = NULL;

  if(!pXcodeV) {
    return -1;
  } else if(!pXcodeV->common.cfgDo_xcode || !pXcodeV->out[outidx].active ||
            pXcodeV->out[outidx].passthru) {
    return 0;
  }

  pUData = (STREAM_XCODE_VID_UDATA_T *) pXcodeV->pUserData;
  //fprintf(stderr, "xcode_requestFB\n");

  //if(!pUData->allowFIR) {
  //  return 0;
  //}

  tm = timer_GetTime() + (msDelay * TIME_VAL_MS);

  //fprintf(stderr, "FIR req outidx:%d tm:%llu now:%llu\n", outidx, pUData->out[outidx].fbReq.tmLastFIRRcvd/1000, tm/1000);

  if(fbReqType == ENCODER_FBREQ_TYPE_FIR) {

      pUData->out[outidx].fbReq.tmLastFIRRcvd = tm;

  } else if(fbReqType == ENCODER_FBREQ_TYPE_PLI) {

  }

  return rc;
}

int streamer_requestFB(STREAMER_CFG_T *pStreamerCfg, unsigned int outidx,
                     enum ENCODER_FBREQ_TYPE fbReqType, unsigned int msDelay,
                     enum REQUEST_FB_SRC requestSrc) {
  int rc = 0;
  unsigned int idx;

  if(!pStreamerCfg) {
    return -1;
  }

  //fprintf(stderr, "streamer_requestFB type:%d from:%d\n", fbReqType, requestSrc);

  if(!pStreamerCfg->xcode.vid.common.cfgDo_xcode ||
     !pStreamerCfg->xcode.vid.out[outidx].active ||
     pStreamerCfg->xcode.vid.out[outidx].passthru) {

    //
    // The stream is not being locally encoded, so request the feedback type to the remote input source
    // This flag will be processed by the RTP socket reader RTCP responder
    // Inform the input capture to request an IDR from the media source
    //
    capture_requestFB(&pStreamerCfg->fbReq, fbReqType, requestSrc);

    return 0;
  }

  if(!pStreamerCfg->fbReq.firCfg.fir_encoder) {
    return 0;
  }

  switch(requestSrc) {
    case REQUEST_FB_FROM_REMOTE:
      if(!pStreamerCfg->fbReq.firCfg.fir_recv_from_remote) {
        return 0;
      }
      break;
    case REQUEST_FB_FROM_LOCAL:
      if(!pStreamerCfg->fbReq.firCfg.fir_recv_from_connect) {
        return 0;
      }
      break;
    default:
      return 0;
  }

  //fprintf(stderr, "HTTPLIVE:%d MOOF:%D\n", pStreamerCfg->action.do_httplive, pStreamerCfg->action.do_moofliverecord);

  //
  // For key-frame synchronized output methods, such as HTTPLive / MPEG-DASH
  // request a keyframe for every available stream output
  //
  if(pStreamerCfg->action.do_httplive || pStreamerCfg->action.do_moofliverecord) {
    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      if(outidx == idx ||
         pStreamerCfg->pStorageBuf->httpLiveDatas[idx].active ||
         pStreamerCfg->pStorageBuf->moofRecordCtxts[idx].do_moof_segmentor) {
        rc = xcode_requestFB(&pStreamerCfg->xcode.vid, idx, fbReqType, msDelay);
      }
    }
    return rc;
  }

  //
  // Pass the Feedback Request to the local underlying encoder
  //
  rc = xcode_requestFB(&pStreamerCfg->xcode.vid, outidx, fbReqType, msDelay);

  return rc;
}

#endif // (VSX_HAVE_STREAMER)

#if defined(VSX_HAVE_CAPTURE)

int capture_requestFB(VID_ENCODER_FBREQUEST_T *pFbReq, enum ENCODER_FBREQ_TYPE fbReqType,
                      enum REQUEST_FB_SRC requestSrc) {
  int rc = 0;

  if(!pFbReq) {
    return -1;
  }

  //fprintf(stderr, "capture_requestFB type:%d from:%d\n", fbReqType, requestSrc);

  // TODO: this should not be FIR specific

  switch(requestSrc) {
    case REQUEST_FB_FROM_REMOTE:
      if(!pFbReq->firCfg.fir_send_from_remote) {
        return 0;
      }
      break;
    case REQUEST_FB_FROM_LOCAL:
      if(!pFbReq->firCfg.fir_send_from_local) {
        return 0;
      }
      break;
    case REQUEST_FB_FROM_CAPTURE:
      if(!pFbReq->firCfg.fir_send_from_capture) {
        return 0;
      }
      break;
    case REQUEST_FB_FROM_DECODER:
      if(!pFbReq->firCfg.fir_send_from_decoder) {
        return 0;
      }
      break;
    default:
      return 0;
  }

  //
  // Inform the input capture to request an IDR from the media source
  //
  pFbReq->tmLastFIRRcvd = timer_GetTime();

  return rc;
}

#endif // (VSX_HAVE_CAPTURE)

