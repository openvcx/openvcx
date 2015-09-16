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

#ifndef __STREAM_MONITOR_H__
#define __STREAM_MONITOR_H__


#include "util/throughput.h"
#include "server/srvdevtype.h"

#define STREAM_STATS_DUMP_MS          4000
#define STREAM_STATS_RANGE_MS_LOW     5000
#define STREAM_STATS_RANGE_MS_HIGH    -1


typedef struct STREAM_STATS_CTRS {

  unsigned int                  numRrTot;
  unsigned int                  ctr_rrRcvd;

  int32_t                       ctr_rrdelayMsTot;
  unsigned int                  ctr_rrdelaySamples;

  float                         ctr_fracLostTot;
  float                         ctr_fracLostSamples;

  uint32_t                      cumulativeLostPrev;
  int32_t                       ctr_cumulativeLost;

  uint32_t                      cumulativeSeqNum;
  int32_t                       ctr_cumulativeSeqNum;

  uint16_t                      seqhighestRrPrev;
  uint16_t                      ctr_countseqRr;

} STREAM_STATS_CTRS_T;

typedef struct STREAM_STATS {
  int                           active;
  STREAM_METHOD_T               method;
  struct sockaddr_storage       saRemote;
  struct STREAM_RTP_DEST       *pRtpDest;
  STREAM_MONITOR_ABR_TYPE_T     abrEnabled;
  struct timeval                tvstart;
  unsigned int                  numWr;
  unsigned int                  numRd;

  //
  // Real time running counters
  //
  STREAM_STATS_CTRS_T           ctr_rt;
  THROUGHPUT_STATS_T            throughput_rt[2];

  //
  // snapshot of running counters last time stream_stats_newinterval was called 
  //
  struct timeval                tv_last;
  STREAM_STATS_CTRS_T           ctr_last;
  THROUGHPUT_STATS_T            throughput_last[2];

  pthread_mutex_t               mtx;
  struct STREAM_STATS          *pnext;
  struct STREAM_STATS_MONITOR  *pMonitor;

} STREAM_STATS_T;

typedef struct STREAM_STATS_MONITOR {
  int                           active;
  unsigned int                  count;
  const char                   *statfilepath;
  unsigned int                  dumpIntervalMs;
  int                           runMonitor;
  struct STREAM_ABR            *pAbr;
  pthread_mutex_t               mtx;
  int                           rangeMs1;
  int                           rangeMs2;
  STREAM_STATS_T               *plist;
} STREAM_STATS_MONITOR_T;


int stream_stats_destroy(STREAM_STATS_T **ppStats, pthread_mutex_t *pmtx);
void stream_stats_addPktSample(STREAM_STATS_T *pStats, pthread_mutex_t *pmtx, unsigned int len, int rtp);
int stream_stats_addRTCPRR(STREAM_STATS_T *pStats, pthread_mutex_t *pmtx, const STREAM_RTCP_SR_T *pSr);
STREAM_STATS_T *stream_monitor_createattach(STREAM_STATS_MONITOR_T *pMonitor, 
                                            const struct sockaddr *psaRemote,
                                            STREAM_METHOD_T method,
                                            STREAM_MONITOR_ABR_TYPE_T abrEnabled);
int stream_monitor_dump_url(char *buf, unsigned int szbuf, STREAM_STATS_MONITOR_T *pMonitor);
int stream_monitor_start(STREAM_STATS_MONITOR_T *pMonitor, const char *statfilepath, 
                         unsigned int intervalMs);
int stream_monitor_stop(STREAM_STATS_MONITOR_T *pMonitor);

#endif // __STREAM_MONITOR_H__
