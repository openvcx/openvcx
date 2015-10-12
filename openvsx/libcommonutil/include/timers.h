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

#ifndef __TIMER_H__
#define __TIMER_H__

#if defined(WIN32) && !defined(__MINGW32__)

typedef _int64 TIME_VAL;

#else // WIN32

typedef long long TIME_VAL;

#include <sys/time.h>

#endif // WIN32

#define  TIME_VAL_US            1000000
#define  TIME_VAL_US_FLOAT      1000000.0f
#define  TIME_VAL_MS            1000
#define  TIME_VAL_MS_FLOAT      1000.0f

#define TIME_TV_DIFF_MS(tv1, tv0)    ((((tv1).tv_sec - (tv0).tv_sec) * 1000) + \
                                      (((tv1).tv_usec - (tv0).tv_usec)/1000))
#define TIME_TV_SET(tvto, tvfrom)   (tvto).tv_sec = (tvfrom).tv_sec; \
                                    (tvto).tv_usec = (tvfrom).tv_usec; 
#define TV_FROM_TIMEVAL(tv, tm)     (tv).tv_sec = (tm) / TIME_VAL_US; \
                                     (tv).tv_usec = (tm) % TIME_VAL_US; 
#define TIME_FROM_TIMEVAL(tv)  (((tv).tv_sec * TIME_VAL_US) + (tv).tv_usec)
#define TV_INCREMENT_MS(tv, ms) (tv).tv_usec += (long) ((ms) * 1000); \
                                while((tv).tv_usec >= TIME_VAL_US) {\
                                  (tv).tv_sec++; (tv).tv_usec -= TIME_VAL_US; }


TIME_VAL timer_GetTime();
int timer_calibrateTimers();
int timer_getPreciseTime(struct timeval *ptv);

#endif // __TIMER_H__
