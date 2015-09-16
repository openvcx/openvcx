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


#ifndef __SRV_HTTP_SESSION_H__
#define __SRV_HTTP_SESSION_H__

#include "unixcompat.h"

#ifndef WIN32

#include <netinet/in.h>
#include <arpa/inet.h>

#endif // WIN32

#include <time.h>


#define SESSION_CACHE_MAX          128

typedef struct SESSION_DESCR {
  char                     cookie[32];
  time_t                   lastTm;
  struct SESSION_DESCR    *pnext;
  struct SESSION_DESCR    *pprev;
} SESSION_DESCR_T;

typedef struct SESSION_CACHE {
  SESSION_DESCR_T         *plist;
} SESSION_CACHE_T;

SESSION_DESCR_T *session_add(SESSION_CACHE_T *pCache, struct sockaddr *pSocakAddr);
SESSION_DESCR_T *session_find(const SESSION_CACHE_T *pCache, const char *cookie);



#endif // __SRV_HTTP_SESSION_H__
