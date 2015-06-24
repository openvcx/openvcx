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


#ifndef __POOL_H__
#define __POOL_H__

#include "unixcompat.h"
#include "pthread_compat.h"

typedef struct POOL_ELEMENT {
  int inuse;
  int id;
  struct POOL_ELEMENT *pPrev;
  struct POOL_ELEMENT *pNext;
} POOL_ELEMENT_T;

typedef struct POOL {
  unsigned int            szElement;
  unsigned int            numElements;
  int                     freeElements;
  POOL_ELEMENT_T         *pElements; 
  POOL_ELEMENT_T         *pInUse;
  POOL_ELEMENT_T         *pFree;
  int                     lock;
  pthread_mutex_t         mtx;
  int                     destroy_onempty;
  const char             *descr;
} POOL_T;

int pool_open(POOL_T *pPool, unsigned int numElements, unsigned int szElement,
              int lock);
POOL_ELEMENT_T *pool_get(POOL_T *pPool);
int pool_return(POOL_T *pPool, POOL_ELEMENT_T *pElement);
void pool_close(POOL_T *pPool, int wait_for_ret);


#endif // __POOL_H__
