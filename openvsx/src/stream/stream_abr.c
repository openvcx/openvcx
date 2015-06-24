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

#include "capture/capture_abr.h"

#define STREAM_STATS_ABR_RANGE_MS                       CAPTURE_STATS_ABR_RANGE_MS 
#define STREAM_ABR_CB_INTERVAL_MS                       STREAM_STATS_ABR_RANGE_MS
#define STREAM_ABR_XCODE_BPS_MIN                        50000
#define STREAM_ABR_XCODE_ADJUST_BPS_MIN                 50000
#define STREAM_ABR_RAISE_INTERVAL_MS_MIN                9500
#define STREAM_ABR_LOWER_INTERVAL_MS_MIN                7000
#define STREAM_ABR_RAISE_POSTLOWER_INTERVAL_MS_MIN      18000 


int stream_abr_monitor(STREAM_STATS_MONITOR_T *pMonitor,  STREAMER_CFG_T *pStreamerCfg) {
  int rc = 0;
  STREAM_ABR_T *pAbr = NULL;
  const IXCODE_VIDEO_OUT_T *pX;
  unsigned int rangeMs = STREAM_STATS_ABR_RANGE_MS;
  unsigned int outidx = 0;
  float fps;
  float fpsAbrMax;
  float fpsAbrMin;

  if(!pMonitor || !pStreamerCfg) {
    return -1;
  }

  pX = &pStreamerCfg->xcode.vid.out[outidx];

  if(!pStreamerCfg->xcode.vid.common.cfgDo_xcode) {
    LOG(X_WARNING("ABR can only be used for encoded video output"));
    return 0;
  }

  fps =  VID_GET_FPS(pStreamerCfg->xcode.vid.cfgOutClockHz, pStreamerCfg->xcode.vid.cfgOutFrameDeltaHz);
  fpsAbrMax =  VID_GET_FPS(pX->abr.cfgOutMaxClockHz, pX->abr.cfgOutMaxFrameDeltaHz);
  fpsAbrMin =  VID_GET_FPS(pX->abr.cfgOutMinClockHz, pX->abr.cfgOutMinFrameDeltaHz);

  if(((pX->abr.cfgBitRateOutMax == 0 && pX->abr.cfgBitRateOutMin == 0) || 
      (pX->abr.cfgBitRateOutMax == pX->cfgBitRateOut && pX->abr.cfgBitRateOutMin == pX->cfgBitRateOut)) &&
     ((pX->abr.cfgGopMsMax == 0 && pX->abr.cfgGopMsMin == 0) || 
      (pX->abr.cfgGopMsMax == pX->cfgGopMaxMs && pX->abr.cfgGopMsMin == pX->cfgGopMaxMs)) &&
      ((fpsAbrMax == 0 && fpsAbrMin == 0) || 
       (fpsAbrMax == fps && fpsAbrMin == fps))) {
    LOG(X_WARNING("ABR encoder configuration has not been set"));
    return 0;
  }

  xcode_dump_conf_video_abr(&pStreamerCfg->xcode.vid, 0, S_DEBUG);
  
  if(pMonitor->active) {
    LOG(X_ERROR("ABR shared stream stats monitor already active"));
    return -1;
  } else if(pMonitor->pAbr) {
    return -1;
  }

  if(!(pAbr = (STREAM_ABR_T *) calloc(1, sizeof(STREAM_ABR_T)))) {
    return -1;
  }

  pthread_mutex_init(&pAbr->mtx, NULL);

  //
  // Store the original encoder configuration
  //
  pAbr->xcodeCfgOrig.cfgBitRateOut = pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut;
  pAbr->xcodeCfgOrig.cfgBitRateTolOut = pStreamerCfg->xcode.vid.out[outidx].cfgBitRateTolOut;
  pAbr->xcodeCfgOrig.cfgVBVMaxRate = pStreamerCfg->xcode.vid.out[outidx].cfgVBVMaxRate;
  pAbr->xcodeCfgOrig.cfgVBVMinRate = pStreamerCfg->xcode.vid.out[outidx].cfgVBVMinRate;
  pAbr->xcodeCfgOrig.cfgVBVBufSize = pStreamerCfg->xcode.vid.out[outidx].cfgVBVBufSize;
  pAbr->xcodeCfgOrig.cfgGopMaxMs = pStreamerCfg->xcode.vid.out[outidx].cfgGopMaxMs;
  pAbr->xcodeCfgOrig.cfgGopMinMs = pStreamerCfg->xcode.vid.out[outidx].cfgGopMinMs;
  pAbr->xcodeCfgOrig.resOutClockHz = pStreamerCfg->xcode.vid.resOutClockHz;
  pAbr->xcodeCfgOrig.resOutFrameDeltaHz = pStreamerCfg->xcode.vid.resOutFrameDeltaHz;
  //pAbr->xcodeCfgOrig.resOutThrottledMaxClockHz = pStreamerCfg->xcode.vid.out[outidx].abr.resOutThrottledMaxClockHz;
  //pAbr->xcodeCfgOrig.resOutThrottledMaxFrameDeltaHz = pStreamerCfg->xcode.vid.out[outidx].abr.resOutThrottledMaxFrameDeltaHz;

  pAbr->pMonitor = pMonitor;
  pAbr->pStreamerCfg = pStreamerCfg;
  pMonitor->pAbr = pAbr;
  pMonitor->rangeMs1 = rangeMs;
  pMonitor->rangeMs2 = -1;

  if(pStreamerCfg->fbReq.nackRtpRetransmit) {
    burstmeter_init(&pAbr->rateNackRequest, rangeMs/10, rangeMs);
    burstmeter_init(&pAbr->rateNackRetransmitted, rangeMs/10, rangeMs);
    burstmeter_init(&pAbr->rateNackThrottled, rangeMs/10, rangeMs);
    burstmeter_init(&pAbr->rateNackAged, rangeMs/10, rangeMs);
  }

  if((rc = stream_monitor_start(pMonitor, NULL, rangeMs)) < 0) {
    if(pMonitor->pAbr) {
      stream_abr_close(pMonitor->pAbr);
      free(pMonitor->pAbr);
    }
  }

  return rc;
}

void stream_abr_close(STREAM_ABR_T *pAbr) {

  if(pAbr) {
    burstmeter_close(&pAbr->rateNackRequest);
    burstmeter_close(&pAbr->rateNackRetransmitted);
    burstmeter_close(&pAbr->rateNackThrottled);
    burstmeter_close(&pAbr->rateNackAged);
    pthread_mutex_destroy(&pAbr->mtx);
  }

}

int stream_abr_notifyBitrate(STREAM_STATS_T *pStats, pthread_mutex_t *pmtx, STREAM_ABR_UPDATE_REASON_T reason, float fConfidence) {
  int rc = 0;
  STREAM_ABR_T *pAbr = NULL;

  if(pmtx) {
    pthread_mutex_lock(pmtx);
  }
  if(!pStats || !(pAbr = pStats->pMonitor->pAbr) || pStats->abrEnabled != STREAM_MONITOR_ABR_ENABLED) {
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    }
    return -1;
  }

  //TODO: what about non-NACK RTCP based reporting like receiver report indicated loss
  if(pAbr->pStreamerCfg->fbReq.nackRtpRetransmit <= 0) {
    if(pmtx) {
      pthread_mutex_unlock(pmtx);
    }
    return 0;
  }

  //LOG(X_DEBUG("stream_abr_notifyBitrate reason:%d"), reason);

/*
  pthread_mutex_lock(&pStats->mtx);

  uint32_t durationms;
  if(pStats->throughput_last[0].written.slots > 0 && pStats->tvstart.tv_sec > 0 && pStats->tv_last.tv_sec > 0) {
    durationms = TIME_TV_DIFF_MS(pStats->tv_last, pStats->tvstart);
  } else {
    durationms = 0;
  }
  //LOG(X_DEBUG("ABR_UPDATEBITRATE DURATION MS:%u, last:%u.%u, start:%u.%u"), durationms, pStats->tv_last.tv_sec, pStats->tv_last.tv_usec,pStats->tvstart.tv_sec, pStats->tvstart.tv_usec);
  if(pStats->throughput_last[0].bitratesWr[0].meter.rangeMs > 0) {
    LOG(X_DEBUG("0, 0ABR_UPDATEBITRATE reason:%d %.3f pkts/s, bitrate: %.3f b/s. NACK req: %.3f/s, aged: %.3f/s, throttled: %.3f/s, rexmit: %.3f"), reason, burstmeter_getPacketratePs(&pStats->throughput_last[0].bitratesWr[0]), burstmeter_getBitrateBps(&pStats->throughput_last[0].bitratesWr[0]), burstmeter_getPacketratePs(&pAbr->rateNackRequest), burstmeter_getPacketratePs(&pAbr->rateNackAged), burstmeter_getPacketratePs(&pAbr->rateNackThrottled), burstmeter_getPacketratePs(&pAbr->rateNackRetransmitted));

  }
  if(pStats->throughput_last[1].bitratesWr[0].meter.rangeMs > 0) {
    LOG(X_DEBUG("1,0 ABR_UPDATEBITRATE reason:%d %.3f pkts/s, bitrate: %.3f b/s"), reason, burstmeter_getPacketratePs(&pStats->throughput_last[1].bitratesWr[0]), burstmeter_getBitrateBps(&pStats->throughput_last[1].bitratesWr[0]));
  }

  pthread_mutex_unlock(&pStats->mtx);
*/

  pthread_mutex_lock(&pAbr->mtx);

  switch(reason) {

    //
    // We received a unique RTCP NACK asking a retransmission of a particular RTP sequence number.
    // Note:  if we receive one NACK regarding 17 missing packets then we should get this notification 17 times.
    //
    case STREAM_ABR_UPDATE_REASON_NACK_REQ:


      burstmeter_AddSample(&pAbr->rateNackRequest, 0, NULL);
      break;

    //
    // A NACKed RTP retransmission request cannot be honored because it's too far back in our RTP output
    // retransmission buffer
    //
    case STREAM_ABR_UPDATE_REASON_NACK_AGED:

      burstmeter_AddSample(&pAbr->rateNackAged, 0, NULL);
      break;

    //
    // We do not honor the NACK RTP retransmission request because of any RTP retransmission specific output
    // throttling we have configured
    //
    case STREAM_ABR_UPDATE_REASON_NACK_THROTTLED:
  
      burstmeter_AddSample(&pAbr->rateNackThrottled, 0, NULL);
      break;

    //
    // We just retransmitted an RTP packet which was NACKed 
    //
    case STREAM_ABR_UPDATE_REASON_NACK_RETRANSMITTED:

      burstmeter_AddSample(&pAbr->rateNackRetransmitted, 0, NULL);
      break;
 
    case STREAM_ABR_UPDATE_REASON_UNKNOWN:
    default:
      break;
  }

  pthread_mutex_unlock(&pAbr->mtx);

  if(pmtx) {
    pthread_mutex_unlock(pmtx);
  }

  return rc;
}

//
// Threshold definitions for stepping down frame rate according to target
// bitrate
//

typedef struct ABR_CONFIG {
  unsigned int kbps;    // Threshold bitrate, 
  float fps;            // target FPS to use, if at or below threshold bitrate
  int gopmax;           // TODO: for low frame rates, GOP will have greater influence on threshold
} ABR_CONFIG_T;

typedef struct ABR_CONFIG_LIST {
  int                  macroblocks;
  const ABR_CONFIG_T  *pcfg;
} ABR_CONFIG_LIST_T;

static const ABR_CONFIG_T s_abr_encoderCfg_320x240[] = { 
                                                 { 30, 1.0f, 1800 }, 
                                                 { 90, 3.0f, 1800 },
                                                 { 120, 5.0f, 1800 },
                                                 { 160, 10.0f, 1800 },
                                                 { 200, 15.0f, 1800 },
                                                 { 250, 15.0f, 1800 },
                                                 { 300, 24.0f, 1800 },
                                                 { 400, 30.0f, 1800 },
                                                 { 0, 0.0f, 0},
                                               };

static const ABR_CONFIG_T s_abr_encoderCfg_640x480[] = { 
                                                 { 80, 1.0f, 1800 },    
                                                 { 150, 2.0f, 1800 },
                                                 { 320, 10.0f, 1800 },
                                                 { 500, 15.0f, 1800 },
                                                 { 900, 30.0f, 1800 },
                                                 { 0, 0.0f, 0},
                                               };

static const ABR_CONFIG_LIST_T s_abr_encoderCfgs[] = { 
                                                 { 4800, s_abr_encoderCfg_320x240 },
                                                 { 19200, s_abr_encoderCfg_640x480 },
                                                 { 0, NULL } };


const ABR_CONFIG_T *find_abr_config(int w, int h, int kbps) {
  const ABR_CONFIG_T *pConfig = NULL;
  const ABR_CONFIG_T *pConfigPrior = NULL;
  const ABR_CONFIG_LIST_T *pConfigList = s_abr_encoderCfgs;
  int macroblocks = (w * h) / 16;

  if(w <= 0 || h <= 0) {
    return NULL;
  }
 
  //
  // Find the resolution specific ABR_CONFIG_T element based on the requested resolution arguments
  //
  while(pConfigList->macroblocks > 0 && pConfigList->pcfg) {
    pConfig = pConfigList->pcfg;
    if(pConfigList->macroblocks >= macroblocks) {
      break; 
    }
    
    pConfigList++;
  }

  if(!pConfig) {
    return NULL;
  }

  //
  // Find the bitrate / fps threshold for the requested resolution
  //
  pConfigPrior = pConfig;
  while(pConfig->kbps > 0) {
    //fprintf(stderr, "bps %d check against config kbps: %d kb/s\n", kbps, pConfig->kbps);
    if(kbps <= pConfig->kbps) {
      break;
    }
    pConfigPrior = pConfig;
    pConfig++;
  }

  if(!pConfig || pConfig->kbps <= 0) {
    pConfig = pConfigPrior;
  }

  //fprintf(stderr, "found: %d kb/s\n", pConfig ? pConfig->kbps : -1);

  return pConfig; 
}

static int get_xcode_vbv(const STREAM_ABR_T *pAbr, ABR_VIDEO_OUT_RATE_T *pXcodeCfg, unsigned int bwBpsTarget) {
  int rc = 0;
  float f;

  //
  // Set any VBV encoder parameters with respect to the new target bitrate, maintaining the same ratio
  // of the initial VBV configuration 
  //
  if(pAbr->xcodeCfgOrig.cfgBitRateOut > 0) {

    if(pAbr->xcodeCfgOrig.cfgVBVBufSize > 0) {
      f = (float) pAbr->xcodeCfgOrig.cfgVBVBufSize /  pAbr->xcodeCfgOrig.cfgBitRateOut;
      pXcodeCfg->cfgVBVBufSize = bwBpsTarget * f; 
    }

    if(pAbr->xcodeCfgOrig.cfgVBVMaxRate > 0) {
      f = (float) pAbr->xcodeCfgOrig.cfgVBVMaxRate /  pAbr->xcodeCfgOrig.cfgBitRateOut;
      pXcodeCfg->cfgVBVMaxRate = bwBpsTarget * f; 
    }

    if(pAbr->xcodeCfgOrig.cfgVBVMinRate > 0) {
      f = (float) pAbr->xcodeCfgOrig.cfgVBVMinRate /  pAbr->xcodeCfgOrig.cfgBitRateOut;
      pXcodeCfg->cfgVBVMinRate = bwBpsTarget * f; 
    }

    if(pAbr->xcodeCfgOrig.cfgBitRateTolOut > 0) {
      f = (float) pAbr->xcodeCfgOrig.cfgBitRateTolOut / pAbr->xcodeCfgOrig.cfgBitRateOut;
      pXcodeCfg->cfgBitRateTolOut = bwBpsTarget * f; 
    }

  }

  return rc;
}

static int get_xcode_fps(const STREAM_ABR_T *pAbr, ABR_VIDEO_OUT_RATE_T *pXcodeCfg, 
                         unsigned int bwBpsTarget, unsigned int outidx, float *pfpsCur, float *pfpsTarget) {
  int rc = 0;
  float fpsOrig = 0;
  float fpsMax = 0;
  float fpsMin = 0;
  float fpsMaxThrottled = 0;
  unsigned int width = 0;
  unsigned int height = 0;
  const ABR_CONFIG_T *pAbrConfig = NULL;

  // vp8 320x240 vf=2
  // vfr=1,vb=30,vgmin=1200,vgmax=1800aa  vfr=1,vb=50,vgmin=800,vgmax=900
  // vfr=3,vb=90,vgmin=1200,vgmax=1800     vfr=3,vb=150,vgmin=800,vgmax=900
  // vfr=10,vb=160,vgmin=1200,vgmax=1800   vfr=10,vb=240,vgmin=800,vgmax=1800
  // vfr=15,vb=200,vgmin=1200,vgmax=1800   vfr=15,vb=240,vgmin=800,vgmax=1800
  // vfr=30,vb=300,vgmin=1200,vgmax=1800  

  fpsOrig = VID_GET_FPS(pAbr->xcodeCfgOrig.resOutClockHz, pAbr->xcodeCfgOrig.resOutFrameDeltaHz);


  fpsMax = VID_GET_FPS(pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.cfgOutMaxClockHz,
                       pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.cfgOutMaxFrameDeltaHz);

  fpsMin = VID_GET_FPS(pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.cfgOutMinClockHz,
                       pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.cfgOutMinFrameDeltaHz);

  //
  // resOutThrottledMax can be set by the self throttling mechanism when the encoder is beginning to fall
  // behind real-time too much under high system load or a single threaded encoder configuration running
  // out of CPU cycles.
  //
  fpsMaxThrottled = VID_GET_FPS(pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.resOutThrottledMaxClockHz,
                       pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.resOutThrottledMaxFrameDeltaHz);

  *pfpsCur = VID_GET_FPS(pAbr->pStreamerCfg->xcode.vid.resOutClockHz, pAbr->pStreamerCfg->xcode.vid.resOutFrameDeltaHz);

  *pfpsTarget = 0;

  if(pAbr->pStreamerCfg->xcode.vid.pip.active) {
    width = pAbr->pStreamerCfg->xcode.vid.out[outidx].cfgOutPipH;
    height = pAbr->pStreamerCfg->xcode.vid.out[outidx].cfgOutPipV;
  } else {
    width = pAbr->pStreamerCfg->xcode.vid.out[outidx].resOutH;
    height = pAbr->pStreamerCfg->xcode.vid.out[outidx].resOutV;
  }

  //
  // Lookup the ABR encoder config to see if we should alter the frame rate for the target bitrate 
  //
  if((pAbrConfig = find_abr_config(width, height, bwBpsTarget / 1000.0))) {

    if(fpsMax == 0) {
      fpsMax = fpsOrig;
    }

    //
    // If fpsThrottled has been set, do not exceed this self-cpu-throttling imposed limit
    //
    if(fpsMaxThrottled > 0 && fpsMax > fpsMaxThrottled) {
      fpsMax = fpsMaxThrottled;
    }

    if(fpsMin == 0) {
      fpsMin = fpsOrig;
    }

    if(fpsMax > 0 && fpsMin > 0) {

      *pfpsTarget = pAbrConfig->fps;

      //
      // Only exceed the original configured frame rate if target bitrate is above the original configured bitrate
      //
      if(fpsOrig > 0 && bwBpsTarget <= pAbr->xcodeCfgOrig.cfgBitRateOut && pAbrConfig->fps > fpsOrig) {
        *pfpsTarget = fpsOrig;
      }

      if(*pfpsTarget > fpsMax) {
        *pfpsTarget = fpsMax;
      }
      if(*pfpsTarget < fpsMin) {
        *pfpsTarget = fpsMin;
      }

      LOG(X_DEBUG("ABR found config level for %d x %d, %u b/s, threshold: %u b/s, target fps: %.3ffps, config fps: %.3ffps [ %.3f - %.3f ]"),
              width, height, bwBpsTarget, pAbrConfig->kbps * 1000, *pfpsTarget, pAbrConfig->fps, fpsMin, fpsMax);


      if(!(*pfpsTarget > *pfpsCur * 1.1 || *pfpsTarget < *pfpsCur / 1.1)) {
        *pfpsTarget = 0;
      } else if((rc = vid_convert_fps(*pfpsTarget, &pXcodeCfg->resOutClockHz, &pXcodeCfg->resOutFrameDeltaHz)) < 0) {

      }
    }

  }

  return rc;
}

static int xcode_adjust(STREAMER_CFG_T *pStreamerCfg, unsigned int outidx, const ABR_VIDEO_OUT_RATE_T *pxcodeCfg) {
  IXCODE_CTXT_T xcodeCtxt;
  STREAM_XCODE_DATA_T xcodeData;
  float fpsCur;
  float fpsTarget;
  enum STREAM_NET_ADVFR_RC rcxcode;
  int rc = 0;

  if(!pStreamerCfg || outidx >= IXCODE_VIDEO_OUT_MAX || !pxcodeCfg) {
    return -1;
  }

  memset(&xcodeData, 0, sizeof(xcodeData));
  memset(&xcodeCtxt, 0, sizeof(xcodeCtxt));

  pthread_mutex_lock(&pStreamerCfg->mtxStrmr);

  fpsCur = VID_GET_FPS(pStreamerCfg->xcode.vid.resOutClockHz, pStreamerCfg->xcode.vid.resOutFrameDeltaHz);
  fpsTarget = VID_GET_FPS(pxcodeCfg->resOutClockHz, pxcodeCfg->resOutFrameDeltaHz);

  LOG(X_DEBUG("ABR setXcodeBitrate target xcode rate: %u b/s -> %u b/s, fps: %.2ffps -> %.2ffps (%d/%d)"), 
       pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut, pxcodeCfg->cfgBitRateOut, fpsCur, fpsTarget, 
       pxcodeCfg->resOutClockHz, pxcodeCfg->resOutFrameDeltaHz);


  memcpy(&xcodeCtxt, &pStreamerCfg->xcode, sizeof(xcodeCtxt));
  xcodeData.piXcode = &pStreamerCfg->xcode;

  //
  // Set new encoder output bitrate, and bitrate specific VBV settings
  //
  if(pxcodeCfg->cfgBitRateOut > 0) {
    xcodeCtxt.vid.out[outidx].cfgBitRateOut = pxcodeCfg->cfgBitRateOut;
  }
  if(pxcodeCfg->cfgBitRateTolOut > 0) {
    xcodeCtxt.vid.out[outidx].cfgBitRateTolOut = pxcodeCfg->cfgBitRateTolOut;
  }
  if(pxcodeCfg->cfgVBVBufSize > 0) {
    xcodeCtxt.vid.out[outidx].cfgVBVBufSize = pxcodeCfg->cfgVBVBufSize;
  }
  if(pxcodeCfg->cfgVBVMaxRate > 0) {
    xcodeCtxt.vid.out[outidx].cfgVBVMaxRate = pxcodeCfg->cfgVBVMaxRate;
  }
  if(pxcodeCfg->cfgVBVMinRate > 0) {
    xcodeCtxt.vid.out[outidx].cfgVBVMinRate = pxcodeCfg->cfgVBVMinRate;
  }

  if(pxcodeCfg->resOutClockHz > 0 && pxcodeCfg->resOutFrameDeltaHz > 0) {
    //
    // Set new output FPS adjustment
    //
    xcodeCtxt.vid.resOutClockHz = xcodeCtxt.vid.cfgOutClockHz = pxcodeCfg->resOutClockHz;
    xcodeCtxt.vid.resOutFrameDeltaHz = xcodeCtxt.vid.cfgOutFrameDeltaHz = pxcodeCfg->resOutFrameDeltaHz;
  }

  //
  // Update the running encoder configuration
  //
  if((rcxcode = xcode_vid_update(&xcodeData, &xcodeCtxt.vid)) != STREAM_NET_ADVFR_RC_OK) {
     rc = -1;
  }

  pthread_mutex_unlock(&pStreamerCfg->mtxStrmr);

  return rc;
}

int stream_abr_throttleFps(STREAMER_CFG_T *pStreamerCfg, float factor, float fpsMin) {
  int rc = 0; 
  IXCODE_VIDEO_OUT_T *pXOutV = NULL;
  unsigned int outidx = 0;
  float fpsCur = 0;
  float fpsTarget = 0;
  const float fpsMax = 30.0f;
  ABR_VIDEO_OUT_RATE_T xcodeCfg; 
  STREAM_ABR_T *pAbr = NULL;
  TIME_VAL tvNow;

  if(!pStreamerCfg) {
    return -1;
  } else if(!pStreamerCfg->abrSelfThrottle.active || !pStreamerCfg->xcode.vid.common.cfgDo_xcode ||
             pStreamerCfg->xcode.vid.common.encodeOutIdx < 10) {
    return 0;
  }

  pXOutV = &pStreamerCfg->xcode.vid.out[outidx];
  tvNow = timer_GetTime();

  if(pStreamerCfg->pMonitor && (pAbr = pStreamerCfg->pMonitor->pAbr)) {
    pthread_mutex_lock(&pAbr->mtx);
  }

  if((pStreamerCfg->abrSelfThrottle.tvLastAdjustment > 0 && 
     ((tvNow - pStreamerCfg->abrSelfThrottle.tvLastAdjustment) / TIME_VAL_MS < 12000)) ||
     (pAbr && (tvNow - pAbr->tvLastAdjustment) / TIME_VAL_MS < 2000)) {

    if(pAbr) {
      pthread_mutex_unlock(&pAbr->mtx);
    }
    return 0;
  }

  fpsCur = VID_GET_FPS(pStreamerCfg->xcode.vid.resOutClockHz, pStreamerCfg->xcode.vid.resOutFrameDeltaHz);

  if(fpsCur <= 1) {
    rc = -1; 
  }

  if(rc >= 0) {
    fpsTarget = fpsCur * factor;

    if(fpsMin == 0.0f) {
      fpsMin = 5.0f;
    } else if(fpsMin < 1.0f) {
      fpsMin = 1.0f;
    }

    if(fpsTarget < fpsMin) {
      fpsTarget = fpsMin;
    } else if(fpsTarget > fpsMax) {
      fpsTarget = fpsMax;
    }

    LOG(X_DEBUG("ABR throttleFPS %.3ffps -> %.3ffps, factor: %.3f, min: %.3ffps, enc[%u]"), 
        fpsCur, fpsTarget, factor, fpsMin, pStreamerCfg->xcode.vid.common.encodeOutIdx);

    if((int) fpsTarget == (int) fpsCur) {
      if(pAbr) {
        pthread_mutex_unlock(&pAbr->mtx);
      }
      return 0; 
    }

  }

  if(rc >= 0) {

    memset(&xcodeCfg, 0, sizeof(xcodeCfg));
    pStreamerCfg->abrSelfThrottle.tvLastAdjustment = tvNow;
    if(pAbr) {
      pAbr->tvLastAdjustment = tvNow;
      pAbr->tvLastAdjustmentDown = tvNow;
      pAbr->tvLastAdjustmentUp = tvNow;
    }

    if((rc = vid_convert_fps(fpsTarget, &xcodeCfg.resOutClockHz, &xcodeCfg.resOutFrameDeltaHz)) < 0) {
      rc = -1;
    }

  }

  if(rc >= 0) {

    if(pXOutV->abr.resOutThrottledMaxClockHz != xcodeCfg.resOutClockHz ||
       pXOutV->abr.resOutThrottledMaxFrameDeltaHz >xcodeCfg.resOutFrameDeltaHz) {

      pXOutV->abr.resOutThrottledMaxClockHz = xcodeCfg.resOutClockHz;
      pXOutV->abr.resOutThrottledMaxFrameDeltaHz = xcodeCfg.resOutFrameDeltaHz;
    }

    rc = xcode_adjust(pStreamerCfg, outidx, &xcodeCfg);

  }

  if(pAbr) {
    pthread_mutex_unlock(&pAbr->mtx);
  }

  return rc;
}

static int setXcodeBitrate(STREAM_ABR_T *pAbr, float bwBpsTarget, unsigned int outidx) {

  int rc;
  //enum STREAM_NET_ADVFR_RC rcxcode;
  float fpsCur = 0;
  float fpsTarget = 0;
  ABR_VIDEO_OUT_RATE_T xcodeCfg; 
  //STREAM_XCODE_DATA_T xcodeData;
  //IXCODE_CTXT_T xcodeCtxt;

  memset(&xcodeCfg, 0, sizeof(xcodeCfg));

  //
  // Update target FPS for the new target bitrate
  //
  get_xcode_fps(pAbr, &xcodeCfg, (unsigned int) bwBpsTarget, outidx, &fpsCur, &fpsTarget);

  //
  // Update any VBV / tolerance parameters for the new target bitrate
  //
  get_xcode_vbv(pAbr, &xcodeCfg, bwBpsTarget);

  //TODO: should do a GOP adjustment

  xcodeCfg.cfgBitRateOut = (unsigned int) bwBpsTarget;

  rc = xcode_adjust(pAbr->pStreamerCfg, outidx, &xcodeCfg);

/*
  LOG(X_DEBUG("ABR setXcodeBitrate target xcode rate: %d b/s -> %.0f b/s, fps: %.2ffps -> %.2ffps (%d/%d)"), 
       pAbr->pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut, bwBpsTarget, fpsCur, fpsTarget, 
       xcodeCfg.resOutClockHz, xcodeCfg.resOutFrameDeltaHz);

  memset(&xcodeData, 0, sizeof(xcodeData));
  memset(&xcodeCtxt, 0, sizeof(xcodeCtxt));

  pthread_mutex_lock(&pAbr->pStreamerCfg->mtxStrmr);

  memcpy(&xcodeCtxt, &pAbr->pStreamerCfg->xcode, sizeof(xcodeCtxt));
  xcodeData.piXcode = &pAbr->pStreamerCfg->xcode;

  //
  // Set new encoder output bitrate, and bitrate specific VBV settings
  //
  xcodeCtxt.vid.out[outidx].cfgBitRateOut = (unsigned int) bwBpsTarget;
  xcodeCtxt.vid.out[outidx].cfgBitRateTolOut = xcodeCfg.cfgBitRateTolOut;
  xcodeCtxt.vid.out[outidx].cfgVBVBufSize = xcodeCfg.cfgVBVBufSize;
  xcodeCtxt.vid.out[outidx].cfgVBVMaxRate = xcodeCfg.cfgVBVMaxRate;
  xcodeCtxt.vid.out[outidx].cfgVBVMinRate = xcodeCfg.cfgVBVMinRate;

  if(xcodeCfg.resOutClockHz > 0 && xcodeCfg.resOutFrameDeltaHz > 0) {
    //
    // Set new output FPS adjustment
    //
    xcodeCtxt.vid.resOutClockHz = xcodeCtxt.vid.cfgOutClockHz = xcodeCfg.resOutClockHz;
    xcodeCtxt.vid.resOutFrameDeltaHz = xcodeCtxt.vid.cfgOutFrameDeltaHz = xcodeCfg.resOutFrameDeltaHz;
  }

  //
  // Update the running encoder configuration
  //
  if((rcxcode = xcode_vid_update(&xcodeData, &xcodeCtxt.vid)) != STREAM_NET_ADVFR_RC_OK) {
     rc = -1;
  }

  pthread_mutex_unlock(&pAbr->pStreamerCfg->mtxStrmr);
*/
  return rc;
}


static float changeTargetBitrate(STREAM_ABR_T *pAbr, float factor, int raiseOk, float bwBpsCur, unsigned int outidx) {
  //float bwBpsTarget;
  float deltaBpsMin;
  float bwBpsXcodeDelta;
  float bwBpsXcodeCur;
  float bwBpsXcodeTarget;
  float bwBpsXcodeTargetMin;
  float bwBpsXcodeTargetMax;
  //IXCODE_VIDEO_OUT_T *pXOutV = &pAbr->pStreamerCfg->xcode.vid.out[outidx];

  //
  // Set a new target bitrate given the output adjustment factor
  //

  if(factor == 0.0f) {
    //return pAbr->bwBpsXcodeTargetLast;
    return 0;
  }

  //
  // Get the current xcode configured encoder bitrate, and the min and max limits
  //
  bwBpsXcodeCur = (float) pAbr->pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut;

  //
  // Minimum encoder bitrate should never reach 0
  //
  if((bwBpsXcodeTargetMin = (float) pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.cfgBitRateOutMin) == 0) {
    bwBpsXcodeTargetMin = STREAM_ABR_XCODE_BPS_MIN;
  }

  bwBpsXcodeTargetMax = (float) pAbr->pStreamerCfg->xcode.vid.out[outidx].abr.cfgBitRateOutMax;

  //
  // The target bitrate is the current target bitrate adjusted by the factor multiplier
  //
  //bwBpsTarget = pAbr->bwBpsTargetLast * factor;
  bwBpsXcodeTarget = pAbr->bwBpsXcodeTargetLast * factor;

  LOG(X_DEBUG("ABR changeTargetBitrate called factor: %.2f, raiseOk: %d, cur rate: %.0f b/s, last xcode rate: %.0f b/s, xcode target: %.0f b/s, [ %.0f - %.0f ] b/s"), factor, raiseOk, bwBpsCur, pAbr->bwBpsXcodeTargetLast, bwBpsXcodeTarget, bwBpsXcodeTargetMin, bwBpsXcodeTargetMax);

  //
  // Constrain the new target to the configured min/max limits
  //
  if(bwBpsXcodeTarget > 0) {

    //
    // Check if bitrate raising is limited only to the starting configured bitrate
    // since the conditions for raising bitrate are not below our good network watermark
    //
    if(raiseOk != 2 && bwBpsXcodeTarget > pAbr->xcodeCfgOrig.cfgBitRateOut) {
      LOG(X_DEBUG("ABR bitrate raise limited from %.0f b/s to original setting of %u b/s "), bwBpsXcodeTarget, pAbr->xcodeCfgOrig.cfgBitRateOut);
      bwBpsXcodeTarget = pAbr->xcodeCfgOrig.cfgBitRateOut;
    }

    if(bwBpsXcodeTargetMin > 0 && bwBpsXcodeTarget < bwBpsXcodeTargetMin) {
      bwBpsXcodeTarget = bwBpsXcodeTargetMin;
    } else if(bwBpsXcodeTargetMax > 0 && bwBpsXcodeTarget > bwBpsXcodeTargetMax) {
      bwBpsXcodeTarget = bwBpsXcodeTargetMax;
    }
  }

  if(factor > 1.0f) {
    //
    // Sanity check to make sure that if we are supposed to raise the bitrate, the new target is not below the current one
    //
    if(bwBpsXcodeTarget < pAbr->bwBpsXcodeTargetLast) {
      bwBpsXcodeTarget = pAbr->bwBpsXcodeTargetLast;
    }

  } else if(factor > 0.0f && factor < 1.0f && bwBpsXcodeTarget > pAbr->bwBpsXcodeTargetLast) {
    //
    // Sanity check to make sure that if we are supposed to lower the bitrate, the new target is not above above the current one
    //
    bwBpsXcodeTarget = pAbr->bwBpsXcodeTargetLast;
  }

  if(bwBpsXcodeTarget != bwBpsXcodeCur) {

    //
    // Set a minimum target adjustment delta to prevent too many small adjustments
    //
    if((deltaBpsMin = (bwBpsXcodeTargetMax - bwBpsXcodeTargetMin) / 10) < STREAM_ABR_XCODE_ADJUST_BPS_MIN) {
      deltaBpsMin = STREAM_ABR_XCODE_ADJUST_BPS_MIN;
    }

    if((bwBpsXcodeTargetMax > 0 && bwBpsXcodeTarget >= bwBpsXcodeTargetMax) ||
       (bwBpsXcodeTargetMin > 0 && bwBpsXcodeTarget <= bwBpsXcodeTargetMin) ||
       abs((bwBpsXcodeDelta = bwBpsXcodeTarget - pAbr->pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut)) >= deltaBpsMin) {

      LOG(X_DEBUG("ABR %s output bitrate factor: %.2f, xcode %lld b/s -> target: %lld b/s, current actual: %.0f b/s"), 
        factor > 1.0f ? "raising" : "lowering", factor, (uint64_t) pAbr->bwBpsXcodeTargetLast, (uint64_t) bwBpsXcodeTarget, bwBpsCur);

      setXcodeBitrate(pAbr, bwBpsXcodeTarget, outidx);

      pAbr->bwBpsXcodeTargetLast = bwBpsXcodeTarget;

      return bwBpsXcodeTarget;

    } else {

      LOG(X_DEBUG("ABR output bitrate factor: %.2f, xcode %lld b/s -> target: %lld b/s, delta: %.0f b/s is below threshold: %.0 b/s.  current actual: %.0f b/s"), 
        factor, (uint64_t) pAbr->bwBpsXcodeTargetLast, (uint64_t) bwBpsXcodeTarget, bwBpsXcodeDelta, deltaBpsMin, bwBpsCur);

      pAbr->bwBpsXcodeTargetLast = bwBpsXcodeTarget;
    }

  }

  return 0;
}

#define FACTORMIN(f, fnew)    (f) > 0.0f ? MIN((f), (fnew)) : (fnew)
#define RAISEOK(r, rnew) (rnew) == 0 || (r) == 0 ? 0 : MIN((r), (rnew)) 


int stream_abr_cbOnStream(STREAM_STATS_T *pStats) {
  int rc = 0;
  const unsigned int minPktsBeforeRaise = 250;
  uint32_t durationMs;
  float packetXmitRate;
  float factor = 0.0f;
  float fNackRequestRate = 0.0f;
  float fNackRetransmittedRate = 0.0f;
  float fNackAgedRate = 0.0f;
  float fNackThrottledRate = 0.0f;
  float bwRtpPayloadBpsCur = 0.0f;
  float bwRtpPayloadBpsCurRetransmitted = 0;
  float rtpPayloadPpsCurRetransmitted = 0;
  float bwRtcpPayloadBpsCur = 0.0f;
  float bwBpsCur = 0.0f;
  float bwBpsTarget = 0.0f;
  int raiseOk = 2;
  BURSTMETER_SAMPLE_SET_T *pBitrateRtp = NULL;
  BURSTMETER_SAMPLE_SET_T *pBitrateRtcp = NULL;
  unsigned int outidx = 0;
  STREAM_ABR_T *pAbr = NULL;
  TIME_VAL tvNow;
  struct timeval tv;

  if(!pStats || !(pAbr = pStats->pMonitor->pAbr) || pStats->abrEnabled != STREAM_MONITOR_ABR_ENABLED) {
    return -1;
  }

  tvNow = timer_GetTime();
  TV_FROM_TIMEVAL(tv, tvNow);

  //LOG(X_DEBUG("ABR stream_abr_cbOnStream")); 

  pthread_mutex_lock(&pAbr->mtx);

  if(pAbr->tvLastAdjustment > 0 && (tvNow - pAbr->tvLastAdjustment) / TIME_VAL_MS < 1000) {
    pthread_mutex_unlock(&pAbr->mtx);
    return 0;
  }


  if((pBitrateRtp = &pStats->throughput_last[0].bitratesWr[0])) {
    //
    // Roll-up any realtime rate based RTP counter stats
    //
    burstmeter_updateCounters(pBitrateRtp, &tv);
  }
  if((pBitrateRtcp = &pStats->throughput_last[1].bitratesWr[0])) {
    //
    // Roll-up any realtime rate based RTCP counter stats
    //
    burstmeter_updateCounters(pBitrateRtcp, &tv);
  }

  if(pStats->throughput_last[0].written.slots > 0 && pStats->tvstart.tv_sec > 0 && pStats->tv_last.tv_sec > 0) {
    durationMs = TIME_TV_DIFF_MS(pStats->tv_last, pStats->tvstart);
  } else {
    durationMs = 0;
  }

  if(pBitrateRtp && pBitrateRtp->meter.rangeMs > 0 && 
     (packetXmitRate = burstmeter_getPacketratePs(pBitrateRtp)) > 0) {

    burstmeter_updateCounters(&pStats->pRtpDest->asyncQ.retransmissionMeter, &tv);
    burstmeter_updateCounters(&pAbr->rateNackRequest, &tv);
    burstmeter_updateCounters(&pAbr->rateNackRetransmitted, &tv);
    burstmeter_updateCounters(&pAbr->rateNackAged, &tv);
    burstmeter_updateCounters(&pAbr->rateNackThrottled, &tv);

    bwRtpPayloadBpsCur = burstmeter_getBitrateBps(pBitrateRtp);
    if(pStats->pRtpDest) {
      //TODO: the period of retrnasmission meter is 1 sec, not the 5 second ABR analysis period because
      // this meter is supposed to be used for output throttling onl...
      bwRtpPayloadBpsCurRetransmitted = burstmeter_getBitrateBps(&pStats->pRtpDest->asyncQ.retransmissionMeter);
      rtpPayloadPpsCurRetransmitted = burstmeter_getPacketratePs(&pStats->pRtpDest->asyncQ.retransmissionMeter);
    }
    if(pBitrateRtcp && pBitrateRtcp->meter.rangeMs > 0) {
      bwRtcpPayloadBpsCur = burstmeter_getBitrateBps(pBitrateRtcp);
    }
    bwBpsCur = bwRtpPayloadBpsCur + bwRtpPayloadBpsCurRetransmitted + bwRtcpPayloadBpsCur;

    fNackRequestRate = burstmeter_getPacketratePs(&pAbr->rateNackRequest) * 100.0f / packetXmitRate;
    fNackRetransmittedRate = burstmeter_getPacketratePs(&pAbr->rateNackRetransmitted) * 100.0f / packetXmitRate;
    fNackAgedRate = burstmeter_getPacketratePs(&pAbr->rateNackAged) * 100.0f / packetXmitRate;
    fNackThrottledRate = burstmeter_getPacketratePs(&pAbr->rateNackThrottled) / packetXmitRate;

    LOG(X_DEBUG("ABR rtp: %.0f b/s (%.1 pkt/s), rtp-retransmitted (1sec): %.0f b/s (%.1 pkt/s), rtcp: %.0f b/s, total: %.0f b/s"),
        bwRtpPayloadBpsCur, packetXmitRate, bwRtpPayloadBpsCurRetransmitted, rtpPayloadPpsCurRetransmitted, bwRtcpPayloadBpsCur, bwBpsCur);
    LOG(X_DEBUG("ABR NACKRequestRate: %.3f%%, NACKRetransmitRate: %.3f%%, NACKAgedRate: %.3f%%, NACKThrottledRate: %.3f%%"),
         fNackRequestRate, fNackRetransmittedRate, fNackAgedRate, fNackThrottledRate);
 
      LOG(X_DEBUG("- ABR - 0,0 NACK abr_cbOnStream durationms:%u, %.3f pkts/s, bitrate: %.3f b/s. NACK req: %.3f/s, aged: %.3f/s, throttled: %.3f/s, rexmit: %.3fpkt/s, %.1f b/s"), durationMs, burstmeter_getPacketratePs(pBitrateRtp), burstmeter_getBitrateBps(pBitrateRtp), burstmeter_getPacketratePs(&pAbr->rateNackRequest), burstmeter_getPacketratePs(&pAbr->rateNackAged), burstmeter_getPacketratePs(&pAbr->rateNackThrottled), burstmeter_getPacketratePs(&pAbr->rateNackRetransmitted), burstmeter_getBitrateBps(&pStats->pRtpDest->asyncQ.retransmissionMeter));

    if(pBitrateRtcp && pBitrateRtcp->meter.rangeMs > 0) {
      LOG(X_DEBUG("- ABR - 1,0 NACK abr_cbOnStream  %.3f pkts/s, bitrate: %.3f b/s"), burstmeter_getPacketratePs(pBitrateRtcp), burstmeter_getBitrateBps(pBitrateRtcp));
    }

    //
    // Set the multiplier for lowering the current xcode set bitrate
    //

    if(fNackThrottledRate > 5.0f) {
      factor = FACTORMIN(factor, 0.5f);
    } else if(fNackThrottledRate > 3.0f) {
      factor = FACTORMIN(factor, 0.70f);
    } else if(fNackThrottledRate > 3.0f) {
      factor = FACTORMIN(factor, 0.80f);
    } else if(fNackThrottledRate > 0.1f) {
      raiseOk = RAISEOK(raiseOk, 0);
    } 

    if(fNackAgedRate > 5.0f) {
      factor = FACTORMIN(factor, 0.5f);
    } else if(fNackAgedRate > 3.0f) {
      factor = FACTORMIN(factor, 0.70f);
    } else if(fNackAgedRate > 3.0f) {
      factor = FACTORMIN(factor, 0.80f);
    } else if(fNackAgedRate > 0.1f) {
      raiseOk = RAISEOK(raiseOk, 0);
    }

    if(fNackRequestRate > 10.0f) {
      factor = FACTORMIN(factor, 0.5f);
    } else if(fNackRequestRate > 7.0f) {
      factor = FACTORMIN(factor, 0.70f);
    } else if(fNackRequestRate > 5.0f) {
      factor = FACTORMIN(factor, 0.80f);
    } else if(fNackRequestRate > 1.1f) {
      raiseOk = RAISEOK(raiseOk, 0);
    } else if(fNackRequestRate > 0.4f) {
      raiseOk = RAISEOK(raiseOk, 1);
    }

    if(fNackRetransmittedRate > 10.0f) {
      factor = FACTORMIN(factor, 0.5f);
    } else if(fNackRetransmittedRate > 7.0f) {
      factor = FACTORMIN(factor, 0.70f);
    } else if(fNackRetransmittedRate > 5.0f) {
      factor = FACTORMIN(factor, 0.80f);
    } else if(fNackRetransmittedRate > 1.1f) {
      raiseOk = RAISEOK(raiseOk, 0);
    } else if(fNackRetransmittedRate > 0.4f) {
      raiseOk = RAISEOK(raiseOk, 1);
    }

  }

  if(factor == 0 && raiseOk > 0 &&
     durationMs >= pBitrateRtp->meter.rangeMs && 
     pStats->throughput_last[0].written.slots >= minPktsBeforeRaise) {

    //
    // Set the multiplier for raising the current xcode set bitrate
    //

    if(pAbr->bwBpsXcodeTargetLast == 0) {
      factor = 1.29f;
    } else if(pAbr->bwBpsXcodeTargetLast != 0 &&
       (pAbr->tvLastAdjustmentDown == 0 || 
       (tvNow - (pAbr->tvLastAdjustmentDown) / TIME_VAL_MS > STREAM_ABR_RAISE_POSTLOWER_INTERVAL_MS_MIN))) {
      factor = 1.31f;
    }
  }

  if(factor != 0.0f) {

    if(pAbr->bwBpsXcodeTargetLast == 0) {
      //pAbr->bwBpsTargetLast = bwBpsCur * 1.01f;
      pAbr->bwBpsXcodeTargetLast = pAbr->pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut;
    }

    if(factor > 0 && factor < 1.0f) {

      //
      // lowering target bitrate
      //

      if(pAbr->tvLastAdjustmentDown > 0 && (tvNow - pAbr->tvLastAdjustmentDown) / TIME_VAL_MS < STREAM_ABR_LOWER_INTERVAL_MS_MIN) {
        pthread_mutex_unlock(&pAbr->mtx);
        return 0;
      }

      if((bwBpsTarget = changeTargetBitrate(pAbr, factor, raiseOk, bwBpsCur, outidx)) > 0) {
        pAbr->tvLastAdjustmentDown = tvNow;
      }

    } else if(factor > 1.0f) {

      //
      // raising target bitrate
      //

      if(pAbr->tvLastAdjustmentUp > 0 && (tvNow - pAbr->tvLastAdjustmentUp) / TIME_VAL_MS < STREAM_ABR_RAISE_INTERVAL_MS_MIN) {
        pthread_mutex_unlock(&pAbr->mtx);
        return 0;
      }

      if((bwBpsTarget = changeTargetBitrate(pAbr, factor, raiseOk, bwBpsCur, outidx)) > 0) {
        pAbr->tvLastAdjustmentUp = tvNow;
      }

    }

  }

  pthread_mutex_unlock(&pAbr->mtx);

  return rc;
}

#endif // VSX_HAVE_STREAMER
