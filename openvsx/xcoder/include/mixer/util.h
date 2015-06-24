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


#ifndef __UTIL_H__
#define __UTIL_H__

#include <netinet/in.h>
#include <pthread.h>

typedef void LOG_CTXT_T;

enum THREAD_FLAG {
  THREAD_FLAG_STOPPED        = 0,
  THREAD_FLAG_STARTING       = 1,
  THREAD_FLAG_DOEXIT         = 2,
  THREAD_FLAG_RUNNING        = 3,
};


#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

//
// Timestamp resolution to 1 us (micro-sec)
//
typedef uint64_t TIME_STAMP_T;

#define  TIME_VAL_US            1000000


#define TV_TO_TIME_STAMP(tv)   ((TIME_STAMP_T)tv.tv_sec * TIME_VAL_US) + tv.tv_usec;

TIME_STAMP_T time_getTime();

int mixer_cond_broadcast(pthread_cond_t *cond, pthread_mutex_t *mtx);
int mixer_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx);

#endif //__UTIL_H__
