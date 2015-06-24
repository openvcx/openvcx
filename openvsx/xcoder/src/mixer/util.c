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


#include  <sys/time.h>
#include "mixer/util.h"


TIME_STAMP_T time_getTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return TV_TO_TIME_STAMP(tv);
}

int mixer_cond_broadcast(pthread_cond_t *cond, pthread_mutex_t *mtx) {
  int rc;

  pthread_mutex_lock(mtx);

  rc = pthread_cond_broadcast(cond);

  pthread_mutex_unlock(mtx);

  return rc;
}

int mixer_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx) {
  int rc;

  pthread_mutex_lock(mtx);

  rc = pthread_cond_wait(cond, mtx);

  pthread_mutex_unlock(mtx);

  return rc;
}
