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


#ifndef __BURST_METER_H__
#define __BURST_METER_H__

#include "unixcompat.h"

#define THROUGHPUT_BYTES_IN_KILO_F          1024.0f
#define THROUGHPUT_BYTES_IN_MEG_F           (THROUGHPUT_BYTES_IN_KILO_F * THROUGHPUT_BYTES_IN_KILO_F) 

typedef struct BURSTMETER_METER_STATS {
  uint32_t                         bytes;
  uint32_t                         packets;
} BURSTMETER_METER_STATS_T;

typedef BURSTMETER_METER_STATS_T BURSTMETER_SAMPLE_T;

typedef struct BURSTMETER_METER {
  uint32_t                         periodMs;
  uint32_t                         rangeMs;
  BURSTMETER_METER_STATS_T            cur;
  BURSTMETER_METER_STATS_T            max;
} BURSTMETER_METER_T;

typedef struct BURSTMETER_SAMPLE_SET {
  unsigned int                     countPeriods;
  struct timeval                   tmLatestSample;
  uint16_t                         offsetBoundary;
  uint16_t                         padding;
  uint32_t                         idxLatest;
  BURSTMETER_SAMPLE_T             *samples;
  BURSTMETER_METER_T               meter;  // default meter of rangeMs duration 
  pthread_mutex_t                  mtx;
} BURSTMETER_SAMPLE_SET_T;

typedef struct BURSTMETER_DECODER_SET {
  unsigned int clockHz;
  unsigned int frameDeltaHz;
  BURSTMETER_SAMPLE_SET_T set;
} BURSTMETER_DECODER_SET_T;

int burstmeter_init(BURSTMETER_SAMPLE_SET_T *pSamples, unsigned int periodMs, unsigned int rangeMs);
void burstmeter_close(BURSTMETER_SAMPLE_SET_T *pSamples);
int burstmeter_reset(BURSTMETER_SAMPLE_SET_T *pSamples);
int burstmeter_AddSample(BURSTMETER_SAMPLE_SET_T *pSamples, 
                        unsigned int sampleLen,
                        const struct timeval *pTv);

int burstmeter_AddDecoderSample(BURSTMETER_DECODER_SET_T *pSet, unsigned int sampleLen,
                               unsigned int frameId);
int burstmeter_updateCounters(BURSTMETER_SAMPLE_SET_T *pSamples, const struct timeval *pTv);
float burstmeter_getBitrateBps(const BURSTMETER_SAMPLE_SET_T *pBurstSet);
float burstmeter_getPacketratePs(const BURSTMETER_SAMPLE_SET_T *pBurstSet);
int burstmeter_printBytes(char *buf, unsigned int szbuf, int64_t b);
char *burstmeter_printBytesStr(char *buf, unsigned int szbuf, int64_t b);
int burstmeter_printThroughput(char *buf, unsigned int szbuf, const BURSTMETER_SAMPLE_SET_T *pBurstSet);
char *burstmeter_printThroughputStr(char *buf, unsigned int szbuf, const BURSTMETER_SAMPLE_SET_T *pBurstSet);


#endif // __BURST_METER_H__
