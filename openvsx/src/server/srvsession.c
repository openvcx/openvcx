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





#include "vsx_common.h"

#if 0
static int session_generateid(char *buf, unsigned int len) {
  int rc = 0;
  int i;
  //struct timeval tv;

  if(len < 32) {
    return -1;
  }

  //gettimeofday(&tv, NULL);
  //snprintf(buf, len, "%u%u", tv.tv_usec+999999, tv.tv_sec);

  // srandom should have been called from main.c

  for(i = 0; i < random() % 8; i++) {
    random(); 
  }

  rc = snprintf(buf, len, "%ld", random() % RAND_MAX);

  return rc;
}
#endif // 1


SESSION_DESCR_T *session_find(const SESSION_CACHE_T *pCache, const char *cookie) {
  SESSION_DESCR_T *pSession = NULL;

  //TODO: need a hash

  pSession = pCache->plist;
  while(pSession) {
    if(!strncmp(pSession->cookie, cookie, sizeof(pSession->cookie))) {
      break;
    }
    pSession = pSession->pnext;
  }

  if(pSession) {
    pSession->lastTm = time(NULL);
  }

  return pSession;
}

SESSION_DESCR_T *session_add(SESSION_CACHE_T *pCache, struct sockaddr *pSockAddr) {
  SESSION_DESCR_T *pSession = NULL;
  SESSION_DESCR_T *pSessionPrev = NULL;
  unsigned int numSessions = 0;
  char tmp[128];

  //TODO: need a hash

  while(pSession) {
    if(pSession->cookie[0] == '\0') {
      break;
    }
    numSessions++;
    pSessionPrev = pSession;
    pSession = pSession->pnext;
  }

  if(!pSession) {
    if(numSessions < SESSION_CACHE_MAX) {
      pSession = avc_calloc(1, sizeof(SESSION_DESCR_T)); 
      if(pSessionPrev) {
        pSessionPrev->pnext = pSession;
      } else {
        pCache->plist = pSession;
      }
    } else {
      LOG(X_WARNING("No available session for %s:%d"), 
           FORMAT_NETADDR(*pSockAddr, tmp, sizeof(tmp)),  ntohs(PINET_PORT(pSockAddr)));
      return NULL;
    }
  }

  pSession->lastTm = time(NULL);

  return pSession;
}
