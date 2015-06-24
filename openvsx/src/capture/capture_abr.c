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

int capture_abr_init(CAPTURE_ABR_T *pAbr) {
  unsigned int rangeMs = CAPTURE_STATS_ABR_RANGE_MS;

  if(!pAbr) {
    return -1;
  }

  pAbr->tvLastAdjustment = 0;
  pAbr->tvLastAdjustmentDown = 0;
  pAbr->tvLastAdjustmentUp = 0;

  pAbr->bwPayloadBpsTargetLast = 0;
  pAbr->bwPayloadBpsTargetMax = 0;
  pAbr->bwPayloadBpsTargetMin = 0;
  //pAbr->forceAjustment = 0;
  
  if(pAbr->payloadBitrate.samples) {
    burstmeter_reset(&pAbr->payloadBitrate);
  } else {
    burstmeter_init(&pAbr->payloadBitrate, rangeMs / 10, rangeMs);
  }

  return 0;
}

void capture_abr_close(CAPTURE_ABR_T *pAbr) {

  burstmeter_close(&pAbr->payloadBitrate);

}

static double changeTargetBitrate(CAPTURE_ABR_T *pAbr, float factor, float fNackRate,
                                  double bwPayloadBpsTotal, double bwPayloadBpsCur) {
  double bwPayloadBpsTarget;

  if(factor == 0.0f) {
    VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - changeTargetBitrate factor:%.3f returning targetLast:%.1f"), factor, pAbr->bwPayloadBpsTargetLast));
    return pAbr->bwPayloadBpsTargetLast;
  }

  bwPayloadBpsTarget = pAbr->bwPayloadBpsTargetLast * factor; 

  VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - changeTargetBitrate factor:%.3f target:%.1f, targetLast:%.1f, min:%.1f, max:%.1f bps"), factor, bwPayloadBpsTarget, pAbr->bwPayloadBpsTargetLast, pAbr->bwPayloadBpsTargetMin, pAbr->bwPayloadBpsTargetMax));

  if(factor > 1.0f) {

    if(bwPayloadBpsTarget < pAbr->bwPayloadBpsTargetLast) {
      bwPayloadBpsTarget = pAbr->bwPayloadBpsTargetLast;
    } else if(bwPayloadBpsTarget > bwPayloadBpsCur * factor * 1.3) {
      bwPayloadBpsTarget = pAbr->bwPayloadBpsTargetLast;
    }

  } else if(factor > 0.0f && factor < 1.0f && bwPayloadBpsTarget > pAbr->bwPayloadBpsTargetLast) {
    bwPayloadBpsTarget = pAbr->bwPayloadBpsTargetLast;
  }

  if(bwPayloadBpsTarget > 0) {
    if(pAbr->bwPayloadBpsTargetMin > 0 && bwPayloadBpsTarget < pAbr->bwPayloadBpsTargetMin) {
      bwPayloadBpsTarget = pAbr->bwPayloadBpsTargetMin;
    } else if(pAbr->bwPayloadBpsTargetMax > 0 && bwPayloadBpsTarget > pAbr->bwPayloadBpsTargetMax) {
      bwPayloadBpsTarget = pAbr->bwPayloadBpsTargetMax;
    }
  }

  VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - changeTargetBitrate done with target:%.3f bps, last:%.3f, factor:%.3f"), bwPayloadBpsTarget, pAbr->bwPayloadBpsTargetLast, factor));

  if(pAbr->bwPayloadBpsTargetLast != bwPayloadBpsTarget) {
    LOG(X_DEBUG("REMB ABR %s bitrate adjustment notification %lld b/s -> target:%lld b/s, factor:%.2f, fNackRate: %.2f%%, total rate:%.3f b/s, cur rate: %.3f b/s"), factor > 1.0f ? "raising" : "lowering", (uint64_t) pAbr->bwPayloadBpsTargetLast, (uint64_t) bwPayloadBpsTarget, factor, fNackRate, bwPayloadBpsTotal, bwPayloadBpsCur);

    pAbr->bwPayloadBpsTargetLast = bwPayloadBpsTarget;
  }

  return bwPayloadBpsTarget;
}

int capture_abr_notifyBitrate(const CAPTURE_STREAM_T *pStream, double *pbwPayloadBpsCur) {
  int rc = 0;
  const COLLECT_STREAM_HDR_T *pPktHdr;
  CAPTURE_ABR_T *pAbr;
  unsigned int tmp;
  double durationMs;
  double bwPayloadBpsTotal = 0;
  double bwPayloadBpsCur = 0;
  double bwPayloadBpsTarget = 0;
  float fNackRate = 0;
  float fLossRate = 0;
  float factor = 0.0f;
  int raiseOk = 1;
  TIME_VAL tvNow;

  VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate called")) );

  if(!pStream || !(pAbr = (CAPTURE_ABR_T *) pStream->pRembAbr)) {
    return -1;
  } else if(pStream->numPkts <= 0) {
    VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate numPkts:%lld"), pStream->numPkts) );
    return 0;
  }

  tvNow = timer_GetTime();

  if(pAbr->tvLastAdjustment > 0 && (tvNow - pAbr->tvLastAdjustment) / TIME_VAL_MS < 1000) {
    VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate last called %lld ms ago"), (tvNow - pAbr->tvLastAdjustment)/TIME_VAL_MS) );
    return 0;
  }

  pPktHdr = &pStream->hdr;

  durationMs = ((double)(pStream->lastTsChangeTv.tv_sec - pPktHdr->tvStart.tv_sec) * TIME_VAL_MS) +
               ((pStream->lastTsChangeTv.tv_usec - pPktHdr->tvStart.tv_usec) / TIME_VAL_MS);

  if(durationMs > TIME_VAL_MS) {
    bwPayloadBpsTotal = ((double) pStream->numBytesPayload * 8 / (durationMs / TIME_VAL_MS_FLOAT));
  }

  if(durationMs >= pAbr->payloadBitrate.meter.rangeMs) {
    bwPayloadBpsCur = burstmeter_getBitrateBps(&pAbr->payloadBitrate);
  }

  if(pStream->rtcpRR.pktdroptot > 0 || pStream->rtcpRR.pktdropcnt > 0) {

    tmp = MAX(pStream->rtcpRR.pktdroptot, pStream->rtcpRR.pktdropcnt);
    fLossRate = (float) tmp * 100.0f / pStream->numPkts;
  }

  if(pStream->numSeqnoNacked > 0 && durationMs >= pAbr->payloadBitrate.meter.rangeMs) {

    fNackRate = (float) pStream->numSeqnoNacked * 100.0f / pStream->numPkts;

    if(fNackRate >= 8.0f) {
      factor = 0.50f; 
    } else if(fNackRate > 5.0f) {
      factor = 0.70f; 
    } else if(fNackRate > 3.0f) {
      factor = 0.80f; 
    } else if(fNackRate > 1.0f) {
      raiseOk = 0;
    }

  }

  if(factor == 0 && raiseOk > 0 && durationMs >= pAbr->payloadBitrate.meter.rangeMs && pStream->numPkts > 250) {

    if(pAbr->bwPayloadBpsTargetLast == 0) {
      //
      // First time raise from default rate 
      //
      factor = 1.29f;
    } else if(pAbr->bwPayloadBpsTargetLast != 0 &&
       (pAbr->tvLastAdjustmentDown == 0 || (tvNow - (pAbr->tvLastAdjustmentDown) / TIME_VAL_MS > 15000))) {
      //
      // Subsequent raise after ok network state
      //
      factor = 1.31f;
    }

  }

  if(pAbr->forceAdjustment && factor == 0 && pAbr->bwPayloadBpsTargetLast == 0 && bwPayloadBpsCur > 0) {
    //LOG(X_DEBUG("SET FACTOR TO FORCE... cur:%.3f"), bwPayloadBpsCur);
    factor = 0.99f; 
  }

  VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate numPkts:%lld in %.2fms lossRate:%.3f, nackRate:%3f, bwTotal:%.1fbps, bwCur:%.1fbps, bwLast:%.1fbps, factor:%.3f, raiseOk:%d, force:%d"), pStream->numPkts, durationMs, fLossRate, fNackRate, bwPayloadBpsTotal, bwPayloadBpsCur, pAbr->bwPayloadBpsTargetLast, factor, raiseOk, pAbr->forceAdjustment));

  if(factor != 0.0f) {

    if(pAbr->bwPayloadBpsTargetLast == 0) {
      pAbr->bwPayloadBpsTargetLast = bwPayloadBpsCur * 1.01f;
    }

    pAbr->tvLastAdjustment = tvNow;
    rc = 1;

    if(factor > 0 && factor < 1.0f) {

      VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate want to step down, bwLast:%.3f, %llu ms ago"), pAbr->bwPayloadBpsTargetLast, (tvNow - pAbr->tvLastAdjustmentDown) / TIME_VAL_MS));

      //
      // lowering target bitrate
      //

      if(pAbr->tvLastAdjustmentDown > 0 && (tvNow - pAbr->tvLastAdjustmentDown) / TIME_VAL_MS < 7000) {
        //LOG(X_DEBUG("REMB too close to last down..."));
        return 0;
      }
      bwPayloadBpsTarget = changeTargetBitrate(pAbr, factor, fNackRate, bwPayloadBpsTotal, bwPayloadBpsCur);

      pAbr->tvLastAdjustmentDown = tvNow;

    } else if(factor > 1.0f) {

      VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate want to step up, bwLast:%.3f, %llu ms ago"), pAbr->bwPayloadBpsTargetLast, (tvNow - pAbr->tvLastAdjustmentUp) / TIME_VAL_MS));

      //
      // raising target bitrate
      //

      if(pAbr->tvLastAdjustmentUp > 0 && (tvNow - pAbr->tvLastAdjustmentUp) / TIME_VAL_MS < 7000) {
        //LOG(X_DEBUG("REMB too close to last up..."));
        return 0;
      }

      bwPayloadBpsTarget = changeTargetBitrate(pAbr, factor, fNackRate, bwPayloadBpsTotal, bwPayloadBpsCur);

      pAbr->tvLastAdjustmentUp = tvNow;

    }

    ((STREAM_RTCP_RR_T *) &pStream->rtcpRR)->rembBitrateBps = (uint64_t) pAbr->bwPayloadBpsTargetLast;

    VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate set bps: %llu, bwLast:%.3f"), ((STREAM_RTCP_RR_T *) &pStream->rtcpRR)->rembBitrateBps, pAbr->bwPayloadBpsTargetLast));

  } else if(pAbr->bwPayloadBpsTargetLast > 0 &&  bwPayloadBpsCur > 0) {

     //
     // Force a re-send of the prior RTCP REMB packet (in case of loss)
     //
     if(pAbr->bwPayloadBpsTargetLast > 1.3f * bwPayloadBpsCur || 1.3f * pAbr->bwPayloadBpsTargetLast < bwPayloadBpsCur) {
       VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate repeat prior indication  bwLast:%.3f, bwCur:%.3f (threshold test:%d, %d)"), pAbr->bwPayloadBpsTargetLast, bwPayloadBpsCur, (pAbr->bwPayloadBpsTargetLast > 1.3 * bwPayloadBpsCur) ? 1: 0 , (pAbr->bwPayloadBpsTargetLast < 1.3 * bwPayloadBpsCur) ? 1 : 0));
      rc = 1;
    }

  }

  if(rc > 0 && pbwPayloadBpsCur) {
    *pbwPayloadBpsCur = bwPayloadBpsCur;
  }

  VSX_DEBUG_REMB( LOG(X_DEBUG("REMB - capture_abr_notifyBitrate done with rc:%d, target:% llu (%.1f) bps"),  rc, ((STREAM_RTCP_RR_T *) &pStream->rtcpRR)->rembBitrateBps, pAbr->bwPayloadBpsTargetLast));

  return rc;
}


#endif // VSX_HAVE_CAPTURE
