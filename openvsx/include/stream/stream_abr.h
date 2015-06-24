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

#ifndef __STREAM_ABR_H__
#define __STREAM_ABR_H__

struct STREAMER_CFG;
struct STREAM_STATS_MONITOR;
struct STREAM_STATS;

typedef enum STREAM_MONITOR_ABR_TYPE {
  STREAM_MONITOR_ABR_NONE          = 0,
  STREAM_MONITOR_ABR_ENABLED       = 1,
  STREAM_MONITOR_ABR_ACCOUNT       = 2
} STREAM_MONITOR_ABR_TYPE_T;

typedef struct ABR_VIDEO_OUT_RATE {
  unsigned int                 cfgBitRateOut;
  unsigned int                 cfgBitRateTolOut;
  unsigned int                 cfgVBVMaxRate;
  unsigned int                 cfgVBVMinRate;
  unsigned int                 cfgVBVBufSize;
  int                          cfgGopMaxMs;
  int                          cfgGopMinMs;

  unsigned int                 resOutClockHz;
  unsigned int                 resOutFrameDeltaHz;

} ABR_VIDEO_OUT_RATE_T;

typedef struct STREAM_ABR {
  int                           active;
  struct STREAMER_CFG          *pStreamerCfg;
  struct STREAM_STATS_MONITOR   *pMonitor;
  pthread_mutex_t                mtx;

  TIME_VAL                      tvLastAdjustment;
  TIME_VAL                      tvLastAdjustmentDown;
  TIME_VAL                      tvLastAdjustmentUp;
  float                         bwBpsXcodeTargetLast;

  ABR_VIDEO_OUT_RATE_T           xcodeCfgOrig;

  BURSTMETER_SAMPLE_SET_T        rateNackRequest;
  BURSTMETER_SAMPLE_SET_T        rateNackAged;
  BURSTMETER_SAMPLE_SET_T        rateNackThrottled;
  BURSTMETER_SAMPLE_SET_T        rateNackRetransmitted;

} STREAM_ABR_T;

typedef struct STREAM_ABR_SELFTHROTTLE {
  int                           active;
  TIME_VAL                      tvLastAdjustment;
} STREAM_ABR_SELFTHROTTLE_T;

typedef enum STREAM_ABR_UPDATE_REASON {
  STREAM_ABR_UPDATE_REASON_UNKNOWN            = 0,
  STREAM_ABR_UPDATE_REASON_NACK_REQ           = 1,
  STREAM_ABR_UPDATE_REASON_NACK_AGED          = 2,
  STREAM_ABR_UPDATE_REASON_NACK_THROTTLED     = 3,
  STREAM_ABR_UPDATE_REASON_NACK_RETRANSMITTED = 4
} STREAM_ABR_UPDATE_REASON_T;

int stream_abr_monitor(struct STREAM_STATS_MONITOR *pMonitor, struct STREAMER_CFG *pStreamerCfg);
void stream_abr_close(STREAM_ABR_T *pAbr);
int stream_abr_cbOnStream(struct STREAM_STATS *pStreamStats);
int stream_abr_notifyBitrate(struct STREAM_STATS *pStreamStats, pthread_mutex_t *pmtx, 
                             STREAM_ABR_UPDATE_REASON_T reason, float fConfidence);

int stream_abr_throttleFps(struct STREAMER_CFG *pStreamerCfg, float factor, float fpsMin);


#endif // __STREAM_ABR_H__
