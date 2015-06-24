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

#ifndef __PTHREAD_COMPAT_H__
#define __PTHREAD_COMPAT_H__

#if defined(WIN32) && !defined(__MINGW32__)

#define SCHED_OTHER     0
#define SCHED_FIFO      1
#define SCHED_RR        2

//#define PTHREAD_MUTEX_INITIALIZER                      (pthread_mutex_t) 0;       
//#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP         PTHREAD_MUTEX_INITIALIZER
//#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP        PTHREAD_MUTEX_INITIALIZER 

enum {
  PTHREAD_CREATE_JOINABLE,
#define PTHREAD_CREATE_JOINABLE PTHREAD_CREATE_JOINABLE
  PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_DETACHED PTHREAD_CREATE_DETACHED
};



typedef struct _pthread_mutex {
  void *handle;
} pthread_mutex_t;

typedef struct _pthread_t {
  void *handle;
  int tid;
} pthread_t;

typedef struct _pthread_cond_t {
  void *handle;
} pthread_cond_t;

typedef struct sched_param {
  int sched_priority;
} pthread_sched_param_t;

typedef struct _pthread_attr {
  struct sched_param sched;
} pthread_attr_t;

typedef struct _pthread_condattr {
  void *p;
} pthread_condattr_t;


typedef int pthread_mutexattr_t;

typedef void * (*phtread_threadfunc) (void *ptr) ;


int sched_get_priority_max(int policy);

//pthread_t pthread_self(void);
int pthread_self(void);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
int pthread_create(pthread_t *thread, pthread_attr_t *attr, phtread_threadfunc, void * arg);
void pthread_exit (void *);
int pthread_cancel(pthread_t thread);
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *condattr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *ts);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond);


#define PTHREAD_T_TOINT(ptd) ((long) (ptd).tid)
#define PTHREAD_SELF_TOINT(ptd) ((long) (ptd))

#elif defined(__MINGW32__)

#define PTHREAD_T_TOINT(ptd)   (long) ((ptd).p)
#define PTHREAD_SELF_TOINT(ptd)   (long) ((ptd).p)

#include <pthread.h>

#else // linux

#define PTHREAD_T_TOINT(ptd)   ((long) (ptd))
#define PTHREAD_SELF_TOINT(ptd)   ((long) (ptd))

#include <pthread.h>

#endif // WIN32

#endif // __PTHREAD_COMPAT_H__
