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

#ifndef __DBG_PROFILE_H__
#define __DBG_PROFILE_H__


typedef struct PROFILER_TIMING {
  unsigned int lo0;
  unsigned int hi0;
  unsigned int lo1;
  unsigned int hi1;
  //double profiler_cpu_mhz;
} PROFILER_TIMING_T;


#ifdef ENABLE_PROFILING

#ifndef rdtsc
#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
#endif // rdtsc

/*
#ifndef rdtscl
#define rdtscl(low) \
     __asm__ __volatile__("rdtsc" : "=a" (low) : : "edx")
#endif // rdtscl
*/

double g_profiler_cpu_mhz;

#define _PROFILER_MEASURE(lo, hi)	rdtsc(lo, hi)

#define _PROFILER_TICKS(lo0, hi0, lo1, hi1) \
  		  (unsigned int) \
	          ((((uint64_t)hi1)<<32) | lo1) - ((((uint64_t)hi0) << 32) | lo0) \
		  - 32

#define _PROFILER_TICKS_TO_US(ticks)	(double) ((unsigned int)(ticks) / g_profiler_cpu_mhz)

#define PROFILER_INIT()			g_profiler_cpu_mhz = profiler_get_cpu_mhz();
#define PROFILER_MEASURE_START(pt)	_PROFILER_MEASURE((pt)->lo0, (pt)->hi0)
#define PROFILER_MEASURE_END(pt)		_PROFILER_MEASURE((pt)->lo1, (pt)->hi1)
#define PROFILER_CALC_US(pt)		\
		_PROFILER_TICKS_TO_US( \
		_PROFILER_TICKS((pt)->lo0, (pt)->hi0, (pt)->lo1, (pt)->hi1))


double profiler_get_cpu_mhz();

#else // ENABLE_PROFILING


#define PROFILER_INIT()
#define PROFILER_MEASURE_START(pt)
#define PROFILER_MEASURE_END(pt)
#define PROFILER_CALC_US(pt)	(double) 0


#endif // ENABLE_PROFILING

#endif // __DBG_PROFILE_H__
