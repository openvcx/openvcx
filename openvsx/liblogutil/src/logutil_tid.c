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

#include "logutil_tid.h"
#include <string.h>
#include <stdio.h>

#ifdef WIN32

#include "unixcompat.h"
#include "pthread_compat.h"

#else // WIN32

#include <unistd.h>
#include <pthread.h>
#include "unixcompat.h"
#include "pthread_compat.h"

#endif // WIN32

#define TAG_COUNT     64

typedef struct LOGUTIL_THREAD {
  struct LOGUTIL_THREAD    *pnext;
  pthread_t                      tid;
  char                          tag[LOGUTIL_TAG_LENGTH];
} LOGUTIL_THREAD_T;

typedef struct LOGUTIL_THREAD_CTXT {
  LOGUTIL_THREAD_T    arr[TAG_COUNT];
  LOGUTIL_THREAD_T    *tids_free;
  LOGUTIL_THREAD_T    *tids;
  pthread_mutex_t     mtx;
} LOGUTIL_THREAD_CTXT_T;

static LOGUTIL_THREAD_CTXT_T _g_logutil_tidctxt;
static LOGUTIL_THREAD_CTXT_T *g_plogutil_tidctxt = &_g_logutil_tidctxt;


void *logutil_tid_getContext() {
  return g_plogutil_tidctxt;
}

int logutil_tid_setContext(void *pCtxt) {
  if(g_plogutil_tidctxt && g_plogutil_tidctxt->tids) {
    return -1;
  }
  g_plogutil_tidctxt = pCtxt;
  return 0;
}


void logutil_tid_init() {
  unsigned int idx;

  pthread_mutex_init(&g_plogutil_tidctxt->mtx, NULL);

  memset(g_plogutil_tidctxt->arr, 0, sizeof(g_plogutil_tidctxt->arr));

  for(idx = 1; idx < TAG_COUNT; idx++) {
    g_plogutil_tidctxt->arr[idx - 1].pnext = &g_plogutil_tidctxt->arr[idx];
  }

  g_plogutil_tidctxt->tids_free = &g_plogutil_tidctxt->arr[0];
  g_plogutil_tidctxt->tids = NULL;

}

int logutil_tid_add(pthread_t tid, const char *tag) {
  int rc = -1;
  LOGUTIL_THREAD_T *pTidCtxt;

  if(tid == 0 || !tag || tag[0] == '\0') {
    return -1;
  }

  pthread_mutex_lock(&g_plogutil_tidctxt->mtx);

  pTidCtxt = g_plogutil_tidctxt->tids;
  while(pTidCtxt) {
    if(tid == pTidCtxt->tid) {
      strncpy(pTidCtxt->tag, tag, sizeof(pTidCtxt->tag) - 1);
      rc = 0;
      break;
    }
    pTidCtxt = pTidCtxt->pnext;
  }

  if(rc != 0 && (pTidCtxt = g_plogutil_tidctxt->tids_free)) {
    g_plogutil_tidctxt->tids_free = g_plogutil_tidctxt->tids_free->pnext;

    if(g_plogutil_tidctxt->tids) {
      pTidCtxt->pnext = g_plogutil_tidctxt->tids;
    } else {
      pTidCtxt->pnext = NULL;
    }
    g_plogutil_tidctxt->tids = pTidCtxt;

    pTidCtxt->tid = tid;
    strncpy(pTidCtxt->tag, tag, sizeof(pTidCtxt->tag) - 1);
    rc = 0;
  } else {
    rc = -1;
  }

  pthread_mutex_unlock(&g_plogutil_tidctxt->mtx);

  return rc;
}

int logutil_tid_remove(pthread_t tid) {
  int rc = -1;
  LOGUTIL_THREAD_T *pTidCtxt, *pTidCtxtPrev = NULL;

  pthread_mutex_lock(&g_plogutil_tidctxt->mtx);

  pTidCtxt = g_plogutil_tidctxt->tids;
  while(pTidCtxt) {
    if(tid == pTidCtxt->tid) {

      if(pTidCtxtPrev) {
        pTidCtxtPrev->pnext = pTidCtxt->pnext;
      } else {
        g_plogutil_tidctxt->tids = pTidCtxt->pnext;
      }

      pTidCtxt->pnext = g_plogutil_tidctxt->tids_free;
      g_plogutil_tidctxt->tids_free = pTidCtxt;
      rc = 0;
      break;
    }
    pTidCtxtPrev = pTidCtxt;
    pTidCtxt = pTidCtxt->pnext;
  }

  pthread_mutex_unlock(&g_plogutil_tidctxt->mtx);

  return rc;
}

const char *logutil_tid_lookup(pthread_t tid, int update) {
  const char *tag = NULL;
  LOGUTIL_THREAD_T *pTidCtxt, *pTidCtxtPrev = NULL;

  pthread_mutex_lock(&g_plogutil_tidctxt->mtx);

  pTidCtxt = g_plogutil_tidctxt->tids;

  while(pTidCtxt) {

    if(tid == pTidCtxt->tid) {
      tag = pTidCtxt->tag;

      //
      // Move the node to the head of the list
      //
      if(update && pTidCtxtPrev) {
        pTidCtxtPrev->pnext = pTidCtxt->pnext;
        pTidCtxt->pnext = g_plogutil_tidctxt->tids;
        g_plogutil_tidctxt->tids = pTidCtxt;
      }
      break;
    }

    pTidCtxtPrev = pTidCtxt;
    pTidCtxt = pTidCtxt->pnext;
  }

  pthread_mutex_unlock(&g_plogutil_tidctxt->mtx);

  return tag ? tag : "";
}

#if 0
void logutil_tid_dump() {
  LOGUTIL_THREAD_T *pTidCtxt;

  pthread_mutex_lock(&g_plogutil_tidctxt->mtx);

  fprintf(stderr, "logutil_tid_dump...\n");
  pTidCtxt = g_plogutil_tidctxt->tids;
  while(pTidCtxt) {
    fprintf(stderr, "TAKEN: tid: %ld, tag:'%s'\n", pTidCtxt->tid, pTidCtxt->tag);
    pTidCtxt = pTidCtxt->pnext;
  }

  pTidCtxt = g_plogutil_tidctxt->tids_free;
  while(pTidCtxt) {
    fprintf(stderr, "FREE: tid: %ld, tag:'%s'\n", pTidCtxt->tid, pTidCtxt->tag);
    pTidCtxt = pTidCtxt->pnext;
  }

  pthread_mutex_unlock(&g_plogutil_tidctxt->mtx);
}
#endif // 0

#if 0

void logutil_tid_test() {

  logutil_tid_init();

  fprintf(stderr, "ADD %d\n", logutil_tid_add(1, "one"));
  fprintf(stderr, "ADD %d\n", logutil_tid_add(2, "two"));
  fprintf(stderr, "REMOVE%d\n", logutil_tid_remove(1));
  fprintf(stderr, "ADD %d\n", logutil_tid_add(3, "three"));
  fprintf(stderr, "ADD %d\n", logutil_tid_add(4, "four"));

  fprintf(stderr, "LOOKUP 4 '%s'\n",  logutil_tid_lookup(4));
  fprintf(stderr, "LOOKUP 3 '%s'\n",  logutil_tid_lookup(3));

  logutil_tid_dump();
}
#endif // 0
