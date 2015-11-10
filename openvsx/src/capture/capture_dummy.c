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

typedef struct CAP_SEND_DUMMY_FRAMES_CTXT {
  STREAMER_STATE_T      *prunning;
  CAP_ASYNC_DESCR_T     *pCfg;
  CAPTURE_STATE_T       *pState;
  pthread_mutex_t       *pmtx;
  char                   tid_tag[LOGUTIL_TAG_LENGTH];
} CAP_SEND_DUMMY_FRAMES_CTXT_T;

static CAPTURE_STREAM_T *findStream(const CAPTURE_STATE_T *pState, const CAPTURE_FILTER_T *pFilter) {
  unsigned int idx;

  for(idx = 0; idx < pState->maxStreams; idx++) {
    if(pState->pStreams[idx].pFilter &&
       pState->pStreams[idx].pFilter->mediaType == pFilter->mediaType) {
      return &pState->pStreams[idx];
    }
  }
  return NULL;
}

/*
static int capture_send_dummy_deinit(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_STATE_T *pState) {
  int rc = 0;
  unsigned int idx;

  for(idx = 0; idx < pState->numFilters; idx++) {

    if(codectype_isVid(pState->filters[idx].mediaType)) {
      pCfg->pStreamerCfg->xcode.vid.common.cfgFileTypeIn = pState->filters[idx].mediaType;
    } else if(codectype_isAud(pState->filters[idx].mediaType)) {
      pCfg->pStreamerCfg->xcode.aud.common.cfgFileTypeIn = pState->filters[idx].mediaType;
    }

  }

  return rc;
}
*/

static int init_filter(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_FILTER_T *pFilter, float *pfps, unsigned int *pframeSz) { 
  int rc = 0;

  //
  // bit of a brainless sleep here for streamer to finish init... because we're changing cfgFileTypeIn
  //
  while(pCfg->pStreamerCfg->running == STREAMER_STATE_STARTING ||
        pCfg->pStreamerCfg->running == STREAMER_STATE_STARTING_THREAD) {
    usleep(5000);
  }

  if(codectype_isVid(pFilter->mediaType)) {

    //pFilter->mediaType = XC_CODEC_TYPE_VID_CONFERENCE;
    pCfg->pStreamerCfg->xcode.vid.common.cfgFileTypeIn = XC_CODEC_TYPE_VID_CONFERENCE;

    if(pCfg->pStreamerCfg->xcode.vid.cfgOutFrameDeltaHz == 0 ||
       (*pfps = (float) pCfg->pStreamerCfg->xcode.vid.cfgOutClockHz /
                        pCfg->pStreamerCfg->xcode.vid.cfgOutFrameDeltaHz) == 0) {
      *pfps = 15;
    }

    *pframeSz = 1; 

    if(pCfg->pStreamerCfg->xcode.vid.pip.active &&
       (pCfg->pStreamerCfg->xcode.vid.out[0].cfgOutH == 0 ||
        pCfg->pStreamerCfg->xcode.vid.out[0].cfgOutV == 0)) {
      pCfg->pStreamerCfg->xcode.vid.out[0].cfgOutH = pCfg->pStreamerCfg->xcode.vid.pip.pXOverlay->out[0].cfgOutH;
      pCfg->pStreamerCfg->xcode.vid.out[0].cfgOutV = pCfg->pStreamerCfg->xcode.vid.pip.pXOverlay->out[0].cfgOutV;
    }

  } else if(codectype_isAud(pFilter->mediaType)) {

    //pFilter->mediaType = XC_CODEC_TYPE_AUD_CONFERENCE;
    pCfg->pStreamerCfg->xcode.aud.common.cfgFileTypeIn = XC_CODEC_TYPE_AUD_CONFERENCE;

    if(pCfg->pStreamerCfg->xcode.aud.cfgSampleRateIn == 0) {
      pCfg->pStreamerCfg->xcode.aud.cfgSampleRateIn = pCfg->pStreamerCfg->xcode.aud.cfgSampleRateOut;
    }
    if(pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn == 0) {
      pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn = pCfg->pStreamerCfg->xcode.aud.cfgChannelsOut;
    }
    if(pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn == 0) {
      pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn = 1;
    }

    *pfps = (float)1000 / STREAM_ACONFERENCE_PTIME;
    *pframeSz = pCfg->pStreamerCfg->xcode.aud.cfgSampleRateIn * pCfg->pStreamerCfg->xcode.aud.cfgChannelsIn / 
                *pfps * 2;

  } else {
    rc = -1;
  }

  return rc; 
}

static CAPTURE_STREAM_T *on_new_stream(CAPTURE_STATE_T *pState, const CAPTURE_FILTER_T *pFilter, uint64_t *pts0) {
  unsigned int idxStream;
  CAPTURE_STREAM_T *pStream = NULL;

  //
  // The purpose here is to call cbOnStreamAdd for the new stream(s) if it hasn't already been init
  //

  for(idxStream = 0; idxStream < pState->maxStreams; idxStream++) {
    if(pState->pStreams[idxStream].pFilter && 
       pState->pStreams[idxStream].pFilter->mediaType == pFilter->mediaType) {
      // Already initialized
      *pts0 = pState->pStreams[idxStream].ptsLastFrame + 3000;
      return &pState->pStreams[idxStream]; 
    } else if(!pState->pStreams[idxStream].pFilter) {
      pStream = &pState->pStreams[idxStream];
      pStream->pFilter = pFilter;
      break;
    }
  }

  if(!pStream) {
    LOG(X_ERROR("Unable to find (sendonly) stream for filter media type: %d"), pFilter->mediaType);
    return NULL;
  } else if(pState->cbOnStreamAdd(pState->pCbUserData, pStream, NULL, NULL) < 0) {
    LOG(X_ERROR("Failed to initialize (sendonly) stream media type: %d"), pFilter->mediaType);
    return NULL;
  }

  //
  // Clear the RTCP BYE reception flag since some clients may have sent an RTCP BYE when going on hold.
  //
  //pStream->haveRtcpBye = 0;

  return pStream;
}

static void setNoDecode(CAPTURE_FILTER_T *pFilter, STREAMER_CFG_T *pStreamerCfg, int val) {

  if(codectype_isVid(pFilter->mediaType) && pStreamerCfg->xcode.vid.common.cfgNoDecode != val) {

    LOG(X_DEBUG("Setting PIP visibility off, decoding off"));

    pStreamerCfg->xcode.vid.common.cfgNoDecode = val;

    if(pStreamerCfg->xcode.vid.pip.pXOverlay) {
      pthread_mutex_lock(&pStreamerCfg->xcode.vid.pip.pXOverlay->overlay.mtx);
    }
    pStreamerCfg->xcode.vid.pip.visible = !val;
    if(pStreamerCfg->xcode.vid.pip.pXOverlay) {
      pthread_mutex_unlock(&pStreamerCfg->xcode.vid.pip.pXOverlay->overlay.mtx);
    }

  } else if(codectype_isAud(pFilter->mediaType) && pStreamerCfg->xcode.aud.common.cfgNoDecode != val) {
    pStreamerCfg->xcode.aud.common.cfgNoDecode = val;
  }
}

static int capture_send_dummy_frames(CAP_SEND_DUMMY_FRAMES_CTXT_T *pCtxt) {
  int rc = 0;
  CAPTURE_CBDATA_SP_T *pSp;
  CAPTURE_STREAM_T *pStream;
  PKTQ_EXTRADATA_T xtra;
  uint64_t pts;
  enum STREAM_NET_ADVFR_RC timeRc;
  unsigned int idx;
  unsigned char buf[16384];
  uint64_t pts0[CAPTURE_MAX_FILTERS];
  uint64_t frameIds[CAPTURE_MAX_FILTERS];
  unsigned int frameSz[CAPTURE_MAX_FILTERS];
  float fps[CAPTURE_MAX_FILTERS];
  TIME_VAL tvStart[CAPTURE_MAX_FILTERS];
  int is_init[CAPTURE_MAX_FILTERS];
  int haveSendOnly = 0;
  CAPTURE_FILTER_T *pFilters;
  unsigned int numFilters;
  CAP_ASYNC_DESCR_T *pCfg = pCtxt->pCfg;
  CAPTURE_STATE_T *pState = pCtxt->pState;

  buf[0] = 0x00;
  memset(frameIds, 0, sizeof(frameIds));
  memset(is_init, 0, sizeof(is_init));
  memset(fps, 0, sizeof(fps));
  memset(frameSz, 0, sizeof(frameSz));
  memset(pts0, 0, sizeof(pts0));
  memset(buf, 0, sizeof(buf));
  pFilters = pState->filt.filters;
  //pFilters = pCfg->pcommon->filters;
  numFilters = pState->filt.numFilters;

  LOG(X_DEBUG("Started dummy input frame processor"));

  while(pCfg->running == STREAMER_STATE_RUNNING  && g_proc_exit == 0 && 
        *pCtxt->prunning == STREAMER_STATE_RUNNING) {

    haveSendOnly = 0;

    for(idx = 0; idx < numFilters; idx++) {

      if(pCfg->pStreamerCfg->cfgrtp.xmitType != SDP_XMIT_TYPE_SENDONLY &&
         pFilters[idx].xmitType != SDP_XMIT_TYPE_SENDONLY) {

        //
        // Clear the xcode decode input flag
        //
        setNoDecode(&pFilters[idx], pCfg->pStreamerCfg, 0);

        if(is_init[idx]) {

          //
          // Restore the xcode input file type
          //
          if(codectype_isVid(pFilters[idx].mediaType)) {
            pCfg->pStreamerCfg->xcode.vid.common.cfgFileTypeIn = pFilters[idx].mediaType;
          } else if(codectype_isAud(pFilters[idx].mediaType)) {
            pCfg->pStreamerCfg->xcode.aud.common.cfgFileTypeIn = pFilters[idx].mediaType;
          }

          pthread_mutex_lock(pCtxt->pmtx);

          if((pStream = findStream(pState, &pFilters[idx]))) {
            //
            // Clear the RTCP BYE reception flag since some clients may have sent an RTCP BYE when going on hold.
            //
            //pStream->haveRtcpBye = 0;
            capture_delete_stream(pState, pStream);
          }

          pthread_mutex_unlock(pCtxt->pmtx);

          is_init[idx] = 0;
        }

        continue;

      } else if(!is_init[idx]) {

        init_filter(pCfg, &pFilters[idx], &fps[idx], &frameSz[idx]); 
        if(frameSz[idx] > sizeof(buf)) {
          LOG(X_ERROR("Capture dummy stream frame size %d exceeds %d"), frameSz[idx], sizeof(buf));
          return -1;
        }

        pthread_mutex_lock(pCtxt->pmtx);

        if(!(pStream = on_new_stream(pState, &pFilters[idx], &pts0[idx]))) {
          pthread_mutex_unlock(pCtxt->pmtx);
          return -1;
        }

        pthread_mutex_unlock(pCtxt->pmtx);

        is_init[idx] = 1;

      }

      haveSendOnly = 1;

      pthread_mutex_lock(pCtxt->pmtx);

      if(!(pStream = findStream(pState, &pFilters[idx])) || !(pSp = pStream->pCbUserData)) {
        LOG(X_ERROR("Unable to find dummy stream for filter media type: %d"), pFilters[idx].mediaType);
        pthread_mutex_unlock(pCtxt->pmtx);
        return -1;
      }

      pthread_mutex_unlock(pCtxt->pmtx);

      //
      // Set the xcode decode input flag
      //
      setNoDecode(&pFilters[idx], pCfg->pStreamerCfg, 1);

      //
      // Add the dummy frame to the input capture queue.  The dummy frame timing is used to 
      // drive the stream frame processor.
      //
      memset(&xtra, 0, sizeof(xtra));
      //xtra.flags = CAPTURE_SP_FLAG_KEYFRAME;

      if((timeRc = stream_net_check_time(&tvStart[idx], &frameIds[idx], fps[idx], 1, &pts, NULL)) ==
        STREAM_NET_ADVFR_RC_OK) {
        xtra.tm.pts = pts + pts0[idx];
        //fprintf(stderr, "ADD_DUMMY_FRAME [%d] fps:%.3f, clockHz:%d pts:%.3f (%llu) (relative pts:%.3f)\n", idx, fps[idx], pSp->pStream->clockHz, PTSF(xtra.tm.pts), xtra.tm.pts, PTSF(pts));
        rc = pktqueue_addpkt(pSp->pCapAction->pQueue, buf, frameSz[idx], &xtra, 1);
        //capture_addCompleteFrameToQ(pSp, ts);
      }

    }

    if(!haveSendOnly) {
      usleep(20000);
    }

  }

  LOG(X_DEBUG("Finished dummy input frame processor"));

  return 0;
}


static void capture_send_dummy_frames_proc(void *pArg) {
  CAP_SEND_DUMMY_FRAMES_CTXT_T *pCtxt = (CAP_SEND_DUMMY_FRAMES_CTXT_T *) pArg;  
  CAP_SEND_DUMMY_FRAMES_CTXT_T ctxt; 

  memcpy(&ctxt, pCtxt, sizeof(ctxt));
  *ctxt.prunning = STREAMER_STATE_RUNNING;

  logutil_tid_add(pthread_self(), ctxt.tid_tag);

  capture_send_dummy_frames(&ctxt);

  *ctxt.prunning = STREAMER_STATE_FINISHED;

  logutil_tid_remove(pthread_self());

}

int capture_send_dummy_frames_start(CAP_ASYNC_DESCR_T *pCfg, CAPTURE_STATE_T *pState, 
                                    STREAMER_STATE_T *prunning, pthread_mutex_t *pmtx) {
  CAP_SEND_DUMMY_FRAMES_CTXT_T args;
  const char *s;
  pthread_t ptd;
  pthread_attr_t attr;

  PHTREAD_INIT_ATTR(&attr);
  memset(&args, 0, sizeof(args));
  args.pCfg = pCfg;
  args.pState = pState;
  args.prunning = prunning;
  args.pmtx = pmtx;

  if((s = logutil_tid_lookup(pthread_self(), 0)) && s[0] != '\0') {
    snprintf(args.tid_tag, sizeof(args.tid_tag), "%s-capdm", s);
  }

  *prunning = STREAMER_STATE_STARTING_THREAD;

  if(pthread_create(&ptd, &attr,
                  (void *) capture_send_dummy_frames_proc,
                  (void *) &args) != 0) {
    *prunning = STREAMER_STATE_ERROR;
    return -1;
  }

  while(*args.prunning == STREAMER_STATE_STARTING_THREAD) {
    usleep(5000);
  }

  return 0;
}

#endif // (VSX_HAVE_CAPTURE)
