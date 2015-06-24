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

#if defined(WIN32) && !defined(__MINGW32__)

#include <windows.h>
#include <stdio.h>
#include "unixcompat.h"
#include "pthread_compat.h"


int sched_get_priority_max(int policy) {
  return policy;
}

//pthread_t pthread_self() {
int pthread_self() {
  return GetCurrentThreadId();
}

int pthread_mutex_init(pthread_mutex_t  *mutex,  const pthread_mutexattr_t *mutexattr) {

  if((mutex->handle = CreateEvent(NULL, FALSE, TRUE, NULL)) == NULL) {
    return -1;
  }

  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {

  if(CloseHandle( mutex->handle)) {
    mutex->handle = NULL;
    return 0;
  } else {
    return -1;
  }
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {

  DWORD dw;

  if(mutex->handle == NULL && pthread_mutex_init(mutex, NULL) != 0)
    return -1;
  
  if((dw = WaitForSingleObject(mutex->handle, INFINITE)) == WAIT_OBJECT_0 ||
      dw == WAIT_ABANDONED) {
    return 0;
  }

  return -1;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {

  if(mutex->handle == NULL && pthread_mutex_init(mutex, NULL) != 0)
    return -1;

  if(SetEvent(mutex->handle) > 0)
    return 0;
  else
    return -1;
}


int pthread_attr_init(pthread_attr_t *attr) {
  attr->sched.sched_priority = SCHED_OTHER;
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
  return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
//PTHREAD_CREATE_JOINABLE
  return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy) {
  return 0;
}

int pthread_attr_setschedparam(pthread_attr_t   *attr,  const struct sched_param *param) {

  if(param->sched_priority < SCHED_OTHER || param->sched_priority > SCHED_RR)
    return -1;

  attr->sched.sched_priority = param->sched_priority;

  return 0;
}


int  pthread_create(pthread_t *thread, pthread_attr_t *attr, 
                    phtread_threadfunc entrypoint, void * arg) {

  //TODO: use _beginthreadex w/ __MINGW32__

  if((thread->handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) entrypoint, 
                                    arg, CREATE_SUSPENDED, (DWORD *) &thread->tid)) == NULL) {
    return -1;
  }

  switch(attr->sched.sched_priority) {
    case SCHED_OTHER:
      SetThreadPriority(thread->handle, THREAD_PRIORITY_NORMAL);
      break;
      case SCHED_FIFO:
      SetThreadPriority(thread->handle, THREAD_PRIORITY_ABOVE_NORMAL);
      break;
    case SCHED_RR:
      SetThreadPriority(thread->handle, THREAD_PRIORITY_HIGHEST);
      break;
  }


  ResumeThread(thread->handle);

  return 0;
}

void pthread_exit (void *value_ptr) {
  ExitThread(0);
}

int pthread_cancel(pthread_t thread) {
  if(TerminateThread(thread.handle, 0))
    return 0;
  else
    return -1;
}


int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *condattr) {

  if((cond->handle = CreateEvent(NULL, FALSE, TRUE, NULL)) == NULL) {
    return -1;
  }

  return 0;
}
int pthread_cond_destroy(pthread_cond_t *cond) {
  if(CloseHandle(cond->handle)) {
    cond->handle = NULL;
    return 0;
  } else {
    return -1;
  }
}
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {

  DWORD dw;

  if(cond->handle == NULL && pthread_cond_init(cond, NULL) != 0)
    return -1;
  
  if((dw = WaitForSingleObject(cond->handle, INFINITE)) == WAIT_OBJECT_0 ||
      dw == WAIT_ABANDONED) {
    return 0;
  }

  return -1;

}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *ts) {

  DWORD dw;
  struct timeval tv;
  DWORD ms;

  if(cond->handle == NULL && pthread_cond_init(cond, NULL) != 0)
    return -1;

  gettimeofday(&tv, NULL);

  ms = (DWORD) ((tv.tv_sec - ts->tv_sec) * 1000) + ((tv.tv_usec - (ts->tv_nsec/1000)) / 1000);
  
  if((dw = WaitForSingleObject(cond->handle, ms)) == WAIT_OBJECT_0 ||
      dw == WAIT_ABANDONED) {
    return 0;
  }

  return -1;

}

int pthread_cond_broadcast(pthread_cond_t *cond) {

  if(cond->handle == NULL && pthread_cond_init(cond, NULL) != 0)
    return -1;

  if(SetEvent(cond->handle) > 0) {
    return 0;
  }

  return -1;
}

int pthread_cond_signal(pthread_cond_t *cond) {
  return pthread_cond_broadcast(cond);
}

#endif // WIN32
