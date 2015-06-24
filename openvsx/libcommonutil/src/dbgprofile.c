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

#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>
#include <time.h>
#include "dbgprofile.h"
//#include <asm/msr.h>

#ifdef ENABLE_PROFILING

/*
 *  Returns the system clock rate in MHz
 */
double profiler_get_cpu_mhz() {
  struct timeval tv0, tv1;
  unsigned int lo0, hi0 ;
  unsigned int lo1, hi1;
  unsigned int us, ticks, ticks_gtod;

  // 1st system call takes the most ticks
  gettimeofday(&tv0, NULL);

  rdtsc(lo0, hi0);
  gettimeofday(&tv0, NULL);
  rdtsc(lo1, hi1);
  ticks_gtod = (unsigned int)
               ((((uint64_t)hi1)<<32) | lo1) - ((((uint64_t)hi0) << 32) | lo0);

  rdtsc(lo0, hi0);
  usleep(100000);
  rdtsc(lo1, hi1);

  gettimeofday(&tv1, NULL);

  ticks = (unsigned int)
          ((((uint64_t)hi1)<<32) | lo1) - ((((uint64_t)hi0) << 32) | lo0);

  // add 32 to account for rdtsc instruction
  ticks += ticks_gtod + 32;

  us = (tv1.tv_sec - tv0.tv_sec) * 1000000 + (tv1.tv_usec - tv0.tv_usec);
  fprintf(stderr, "calibrated CPU:  %.2f Mhz (us: %u ticks: %u)\n",
                  (float)ticks/us, us, ticks);

  return (ticks / us);
}

#endif /// ENABLE_PROFILING

#if 0
int main(int argc, char *argv[]) {
  unsigned int lo0, hi0 ;
  unsigned int lo1, hi1;
  unsigned int ticks;
  double cpu_mhz;
  double ns_latency;
  struct timespec ts;

  ts.tv_sec = 0;
  ts.tv_nsec = 1;

  cpu_mhz = profiler_get_cpu_mhz();

  rdtsc(lo0, hi0);
  //usleep(1);
  //nanosleep(10050);
  nanosleep(&ts, NULL);
  rdtsc(lo1, hi1);

  ticks = (unsigned int)
          ((((uint64_t)hi1)<<32) | lo1) - ((((uint64_t)hi0) << 32) | lo0);

  ticks -= 32; // compensate for rdtsc instruction

  ns_latency = ticks / cpu_mhz ;

  fprintf(stderr, "latency:  %.2f ns (%u ticks)\n", ns_latency, ticks);

  return 0;
}
#endif // 0
