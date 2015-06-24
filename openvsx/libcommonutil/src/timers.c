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

#include "timers.h"
#include <stdio.h>
#include "logutil.h"

#ifdef WIN32

#include <windows.h>
#include "unixcompat.h"

static long long g_timerOffset = 0;

TIME_VAL timer_GetTime() {
  static LARGE_INTEGER freq;
  LARGE_INTEGER time;
  TIME_VAL return_val = 0;

  if(freq.QuadPart == 0) {
    QueryPerformanceFrequency(&freq);
    //LOG(X_DEBUG("Timer frequency is %lld "), freq.QuadPart);
  }

  if(freq.QuadPart > 0 &&
     QueryPerformanceCounter(&time) != 0) {
    return_val = (TIME_VAL)( (double)time.QuadPart*TIME_VAL_US/(double)freq.QuadPart);
  }

  return return_val;
}


int timer_calibrateTimers() {

  struct timeval stv0, stv1;
  LARGE_INTEGER perfFreq, perfCnt;
  long long usStart = 0, usEnd;
  unsigned long long usStartTv;
  unsigned int usGetTimeOfDayDelta;
  unsigned int usPerfCtrDelta;

  if(QueryPerformanceFrequency(&perfFreq) == 0) {
    return -1;
  }
  if(QueryPerformanceCounter(&perfCnt) == 0) {
    return -1;
  }

  gettimeofday(&stv0, NULL);

  //
  // Keep looping until gettimeofday returns a new time
  // At this point, check the value of queryperformancecounter
  // and continue looping until gettimeofday returns a new value again
  //
  while(1) {

    gettimeofday(&stv1, NULL);
    if(stv1.tv_usec == stv0.tv_usec) {
      continue;
    }

    QueryPerformanceCounter(&perfCnt);
    if(usStart == 0) {
      usStart = (long long) (((double)perfCnt.QuadPart * TIME_VAL_US) / (double)perfFreq.QuadPart);
      stv0.tv_usec = stv1.tv_usec;  
      stv0.tv_sec = stv1.tv_sec;
    } else {
      usEnd = (long long) (((double)perfCnt.QuadPart * TIME_VAL_US) / (double)perfFreq.QuadPart);
      break;
    }
      
  }

  //
  // The corresponding us offset obtained via gettimeofday and the performance counter
  // should be very similar
  //
  usGetTimeOfDayDelta = ((stv1.tv_sec - stv0.tv_sec) * TIME_VAL_US) + (stv1.tv_usec - stv0.tv_usec);
  usPerfCtrDelta = (unsigned int) (usEnd - usStart);

  usStartTv = ((unsigned long long)stv0.tv_sec * TIME_VAL_US) + stv0.tv_usec;

  //
  // Store the offset needed to derive a gettimeofday value from the performance counter value
  //
  g_timerOffset = usStartTv - usStart;

  //LOG(X_DEBUG("timer offset - gtd: %u, (%lld,%lld) perf ctr: %u, offset: %lld"), 
  //    usGetTimeOfDayDelta, perfCnt.QuadPart, perfFreq.QuadPart, usPerfCtrDelta, g_timerOffset);

  return 0;
}

int timer_getPreciseTime(struct timeval *ptv) {
  static LARGE_INTEGER perfFreq, perfCnt;
  long long usTm;

  if(!ptv) {
    return -1;
  }

  if(g_timerOffset == 0) {
    if(timer_calibrateTimers() != 0) {
      return -1;
    }
  }

  if(perfFreq.QuadPart == 0) {
    if(QueryPerformanceFrequency(&perfFreq) == 0) {
      return -1;
    }
  }

  if(QueryPerformanceCounter(&perfCnt) == 0) {
    return -1;
  }

  //
  // Derive a us value corresponding to gettimeofday based on the 
  // value of QueryPerformanceCounter using g_timerOffset compouted in calibrateTimers
  //
  usTm = (long long) (((double)perfCnt.QuadPart * TIME_VAL_US) / 
                      (double)perfFreq.QuadPart) + g_timerOffset;

  ptv->tv_sec = (long) (usTm / TIME_VAL_US);
  ptv->tv_usec = (long) (usTm % TIME_VAL_US);

  return 0;
}


#else
#include <sys/time.h>

TIME_VAL timer_GetTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((TIME_VAL)tv.tv_sec * TIME_VAL_US) + tv.tv_usec;
}

int timer_calibrateTimers() {
  return 0;
}
int timer_getPreciseTime(struct timeval *ptv) {
  return gettimeofday(ptv, NULL);
}

#endif
