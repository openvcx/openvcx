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


#ifndef __MGR_NODE_H__
#define __MGR_NODE_H__

#include "formats/http_parse.h"

#define MGR_NODES_MAX           20

typedef struct MGR_NODE {
  int                           alive;
  char                          location[HTTP_URL_LEN];
  TIME_VAL                      tmLastFail;
  TIME_VAL                      tmLastOk;
  struct MGR_NODE              *pnext;
} MGR_NODE_T;

typedef struct MGR_NODE_LIST {
  unsigned int                  count;
  pthread_mutex_t               mtx;
  MGR_NODE_T                   *pHead;
  MGR_NODE_T                   *pLast;
  STREAMER_STATE_T              running;
  char                          tid_tag[LOGUTIL_TAG_LENGTH];
} MGR_NODE_LIST_T;

int mgrnode_init(const char *path, MGR_NODE_LIST_T *pNodes);
int mgrnode_close(MGR_NODE_LIST_T *pNodes);
const char *mgrnode_getlb(MGR_NODE_LIST_T *pNodes);


#endif // __MGR_NODE_H__
