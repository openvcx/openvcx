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

#include "mgr/mgrnode.h"

#define NODE_STATUS_REQ_TIMEOUT_MS          2000
#define NODE_POLL_ALIVE_MS     5000
#define NODE_POLL_DEAD_MS     15000
#define NODE_POLL_REPORT_MS   15000


typedef struct NODE_STATUS_RESP_DATA {
  int                       haveOk;
} NODE_STATUS_RESP_DATA_T;

int cbparse_nodestatus_resp(void *pArg, const char *p) {
  int rc = 0;
  NODE_STATUS_RESP_DATA_T *pResp = (NODE_STATUS_RESP_DATA_T *) pArg;
  KEYVAL_PAIR_T kv;

  memset(&kv, 0, sizeof(kv));
  if((rc = conf_parse_keyval(&kv, p, 0, '=', 0)) == 2) {
    if(!strncasecmp(kv.key, "status", 6) && !strncasecmp(kv.val, "OK", 2)) {
      pResp->haveOk = 1;
    }
  }

  return rc;
}

static int check_status(MGR_NODE_LIST_T *pNodes, MGR_NODE_T *pNode) {
  int rc = 0;
  HTTP_STATUS_T httpStatus;
  unsigned char buf[2048];
  unsigned int szdata = sizeof(buf);
  const char *page = NULL;
  int mark_dead = 0;
  NODE_STATUS_RESP_DATA_T status;
  TIME_VAL tm;

  VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - Sending node status check command: '%s' to: %s"),
                             VSX_STATUS_URL, pNode->location););

  memset(&status, 0, sizeof(status));

  if(!(page = (const char *) httpcli_getpage(pNode->location, VSX_STATUS_URL, (unsigned char *) buf, &szdata, 
                                             &httpStatus, NODE_STATUS_REQ_TIMEOUT_MS))) {
    LOG(X_ERROR("Failed to load "VSX_STATUS_URL" for %s (%d)"), pNode->location, httpStatus);
    rc = -1;
  }

  tm = timer_GetTime();

  if(rc >= 0) {
    strutil_parse_delimeted_str(cbparse_nodestatus_resp, &status, page, '&');
    if(status.haveOk) {
      if(!pNode->alive) {
        pthread_mutex_lock(&pNodes->mtx);
        pNode->alive = 1;
        pthread_mutex_unlock(&pNodes->mtx);
        LOG(X_INFO("Node %s status marked active"), pNode->location); 
      }
      pNode->tmLastOk = tm;
      VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - Node status Ok for %s"), pNode->location); );
    }
  }

  if(!status.haveOk) {
    if(pNode->alive) {
      mark_dead = 1;
      pthread_mutex_lock(&pNodes->mtx);
      pNode->alive = 0;
      pthread_mutex_unlock(&pNodes->mtx);
    }
    if(pNode->tmLastFail == 0 || mark_dead) {
      LOG(X_WARNING("Node %s status marked inactive"), pNode->location); 
    }
    pNode->tmLastFail = tm;
    LOG(X_DEBUG("Node status failed for %s"), pNode->location); 
  }

  return rc >= 0 ? status.haveOk : rc;
}

static int check_nodes(MGR_NODE_LIST_T *pNodes, TIME_VAL *ptmLastPrint) {
  int rc = 0;
  MGR_NODE_T *pNode = NULL;
  TIME_VAL tm;
  int state;
  int updated_state = 0;
  int count_alive = 0;

  tm = timer_GetTime();
  pNode = pNodes->pHead;
  while(pNode) {

    if((pNode->alive && (pNode->tmLastOk == 0 || pNode->tmLastOk + (NODE_POLL_ALIVE_MS * TIME_VAL_MS) < tm)) ||
       (!pNode->alive && (pNode->tmLastFail == 0 || pNode->tmLastFail + (NODE_POLL_DEAD_MS * TIME_VAL_MS) < tm))) {

      state = pNode->alive;
      check_status(pNodes, pNode);
      if(state != pNode->alive) {
        updated_state = 1; 
      }
    }

    pNode = pNode->pnext;
  }

  pNode = pNodes->pHead;
  while(pNode) {
    if(pNode->alive) {
      count_alive++;
    } 
    pNode = pNode->pnext;
  }

  if(updated_state || *ptmLastPrint + (NODE_POLL_REPORT_MS * TIME_VAL_MS) <= tm) {
    *ptmLastPrint = timer_GetTime();

    if(count_alive <= 0) {
      LOG(X_WARNING("No nodes in alive state!"));
    } else {
      LOG(X_DEBUG("%d / %d nodes in alive state"), count_alive, pNodes->count);
    }
  }

  return rc;
}

static void mgrnodes_monitor_proc(void *pArg) {
  MGR_NODE_LIST_T *pNodes = (MGR_NODE_LIST_T *) pArg;
  TIME_VAL tmLastPrint = 0;

  pNodes->running = STREAMER_STATE_RUNNING;
  logutil_tid_add(pthread_self(), pNodes->tid_tag);

  LOG(X_DEBUG("MGR nodes monitor thread started"));

  while(pNodes->running == STREAMER_STATE_RUNNING  && !g_proc_exit) {
    check_nodes(pNodes, &tmLastPrint);
    usleep(100000);
  }

  pNodes->running = STREAMER_STATE_FINISHED;
  LOG(X_DEBUG("MGR nodes monitor thread ended"));
  logutil_tid_remove(pthread_self());


}
const char *mgrnode_getlb(MGR_NODE_LIST_T *pNodes) {
  //int rc = 0;
  MGR_NODE_T *pNode = NULL;
  MGR_NODE_T *pNode0 = NULL;

  if(!pNodes || !pNodes->pHead) {
    return NULL;
  }

  pthread_mutex_lock(&pNodes->mtx);

  pNode = pNodes->pLast;
  pNode0 = pNode;

  do {

    if(!pNode0 && pNode && !pNode->pnext) {
      // 1st time loop, at last node w/o match
      pNode = NULL;
      break;
    } else if(!pNode || !(pNode = pNode->pnext)) {
      pNode = pNodes->pHead;
    }

    if(pNode->alive) {
      break;
    }
    
  } while(pNode != pNode0);

  pNodes->pLast = pNode;

  pthread_mutex_unlock(&pNodes->mtx);

  return pNode ? pNode->location : NULL;
}

static int mgrnode_monitor_stop(MGR_NODE_LIST_T *pNodes) {
  int rc = 0;

  while(pNodes->running >= STREAMER_STATE_RUNNING) {

    pNodes->running = STREAMER_STATE_INTERRUPT;
    usleep(20000);
  }

  return rc;
}

static int mgrnode_monitor_start(MGR_NODE_LIST_T *pNodes) {
  int rc = 0 ;
  pthread_t ptdTsLive;
  pthread_attr_t attrTsLive;

  snprintf(pNodes->tid_tag, sizeof(pNodes->tid_tag), "nodes-monitor");
  pNodes->running = STREAMER_STATE_FINISHED;

  PHTREAD_INIT_ATTR(&attrTsLive);

  if(pthread_create(&ptdTsLive,
                    &attrTsLive,
                    (void *) mgrnodes_monitor_proc,
                    (void *) pNodes) != 0) {

    LOG(X_ERROR("Unable to create nodes monitor thread"));
    return -1;
  }

  return rc;
}

int mgrnode_close(MGR_NODE_LIST_T *pNodes) {
  int rc = 0;
  MGR_NODE_T *pNode;

  if(!pNodes) {
    return -1;
  }

  mgrnode_monitor_stop(pNodes);

  pthread_mutex_lock(&pNodes->mtx);

  while(pNodes->pHead) {
    pNode = pNodes->pHead->pnext;
    avc_free((void **) &pNodes->pHead);
    pNodes->pHead = pNode;
  }
 
  pNodes->count = 0;
  pNodes->pLast = NULL;

  pthread_mutex_unlock(&pNodes->mtx);

  pthread_mutex_destroy(&pNodes->mtx);

  return rc;
}

int mgrnode_init(const char *path, MGR_NODE_LIST_T *pNodes) {
  int rc = 0;
  SRV_CONF_T *pConf = NULL;
  unsigned int idx;
  const char *parg;
  MGR_NODE_T *pNode;
  char key[128];

  LOG(X_DEBUG("Reading active nodes from '%s"), path);

  if(!pNodes || pNodes->pHead) {
    return -1;
  }

  //
  // Parse the configuration file
  //
  if((pConf = conf_parse(path)) == NULL) {
    return -1;
  }

  pNodes->running = STREAMER_STATE_FINISHED;
  mgrnode_close(pNodes);

  memset(key, 0, sizeof(key));
  for(idx = 0; idx < MGR_NODES_MAX * 2; idx++) {

    snprintf(key, sizeof(key) - 1, "node%d.active", idx);
    if(!(parg = conf_find_keyval(pConf->pKeyvals, key)) || !IS_CONF_VAL_TRUE(parg)) {
      continue;
    }

    snprintf(key, sizeof(key) - 1, "node%d.location", idx);
    if((parg = conf_find_keyval(pConf->pKeyvals, key)) && strlen(parg) > 0) {
      
      if(!(pNode = avc_calloc(1, sizeof(MGR_NODE_T)))) {
        rc = -1;
        break;
      }
      strncpy(pNode->location, parg, sizeof(pNode->location) - 1);
      if(pNodes->pHead) {
        pNode->pnext = pNodes->pHead;
      }
      pNodes->pHead = pNode;
      LOG(X_DEBUG("Loaded active node '%s'"), pNode->location);
      if(++pNodes->count >= MGR_NODES_MAX) {
        break;
      }

    }
  }

  conf_free(pConf);

  if(rc >= 0) {
    pthread_mutex_init(&pNodes->mtx, NULL);
    rc = mgrnode_monitor_start(pNodes);
  }

  if(rc < 0) {
    mgrnode_close(pNodes);
  }

  return rc;
}
