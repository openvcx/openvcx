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

void mpdpl_free_entry(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry) {

  if(!pl || !pEntry) {
    return;
  }

  if(pEntry->path) {
    avc_free((void **) &pEntry->path);
  }

  if(pl->cbFreeEntry) {
    pl->cbFreeEntry(pEntry);
  }

  if(!pl->pEntriesBuf) {
    avc_free((void **) &pEntry);
  }

}

void mpdpl_free(PLAYLIST_MPD_T *pl) {
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;

  if(!pl) {
    return;
  }

  if(pl->initializer.path) {
    avc_free((void **) &pl->initializer.path);
  }

  while(pl->pEntriesHead) {
    pEntry = pl->pEntriesHead;
    pl->pEntriesHead = pEntry->pnext;

    mpdpl_free_entry(pl, pEntry);

  }

  pl->pEntriesTail = NULL;

  pl->numEntries = 0;
  if(pl->pEntriesBuf) {
    avc_free((void **) &pl->pEntriesBuf);
  }

  pthread_mutex_destroy(&pl->mtx);

}

void mpdpl_dump(const PLAYLIST_MPD_T *pl) {
  const PLAYLIST_MPD_ENTRY_T *pEntry = NULL;
  unsigned int idx = 0;

  pthread_mutex_lock(&((PLAYLIST_MPD_T *) pl)->mtx);

  if(pl->initializer.path) {
    fprintf(stderr, "mpd initializer '%s' state:%d\n", pl->initializer.path, pl->initializer.state); 
  }

  pEntry = pl->pEntriesHead;

  while(pEntry) {
    fprintf(stderr, "mpd[%d] '%s' state:%d\n", idx++, pEntry->path, pEntry->state); 
    pEntry = pEntry->pnext;
  }

  pthread_mutex_unlock(&((PLAYLIST_MPD_T *) pl)->mtx);

}

static PLAYLIST_MPD_ENTRY_T *mpdpl_find(PLAYLIST_MPD_T *pl, const char *path, int lock) {
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;

  if(lock) {
    pthread_mutex_lock(&pl->mtx);
  }

  pEntry = pl->pEntriesHead;

  while(pEntry) {

    if(!strcmp(pEntry->path, path)) {
      break;
    }
    pEntry = pEntry->pnext;
  }

  if(lock) {
    pthread_mutex_unlock(&pl->mtx);
  }

  return pEntry;
}


int mpdpl_create(PLAYLIST_MPD_T *pl, const char *buf) {
  unsigned int idx;

  pl->numEntries = 0;
  if(!(pl->pEntriesBuf = (PLAYLIST_MPD_ENTRY_T *) avc_calloc(3, sizeof(PLAYLIST_MPD_ENTRY_T)))) {
    return -1;
  }

  //pl->initializer.path = (char *) avc_calloc(1, 32);
  //snprintf(pl->initializer.path, 32, "2_out_init.mp4");

  for(idx = 0; idx < 3; idx++) {
    if(idx == 0) {
      pl->pEntriesHead = &pl->pEntriesBuf[0];
    } else {
      pl->pEntriesBuf[idx - 1].pnext = &pl->pEntriesBuf[idx];
    }
    pl->pEntriesTail = &pl->pEntriesBuf[idx];
    pl->pEntriesBuf[idx].path = (char *) avc_calloc(1, 32);
    pl->numEntries++;
    snprintf(pl->pEntriesBuf[idx].path, 32, "1out_%d.mp4", pl->numEntries);
  }

  pl->targetDuration = 5;
  pthread_mutex_init(&pl->mtx, NULL);

  return pl->numEntries;
}

static int mpdpl_attach_int(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry, int lock) {

  if(lock) {
    pthread_mutex_lock(&pl->mtx);
  }

  //
  // Attach to the end of the list
  //

  if(!pl->pEntriesHead) {
    pl->pEntriesHead = pEntry;
  } else {
    pl->pEntriesTail->pnext = pEntry;
  }

  pl->pEntriesTail = pEntry;
  pl->numEntries++;

  if(lock) {
    pthread_mutex_unlock(&pl->mtx);
  }

  return 0;
}

int mpdpl_attach(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry) {

  if(!pl || !pEntry) {
    return -1;
  }

  return mpdpl_attach_int(pl, pEntry, 1);
}

PLAYLIST_MPD_ENTRY_T *mpdpl_update(PLAYLIST_MPD_T *pl, const char *path, PLAYLIST_MPD_STATE_T state) {
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;

  if(!pl || !path || pl->pEntriesBuf) {
    return NULL;
  } 

  //
  // Check if the entry path already exists
  //
  pthread_mutex_lock(&pl->mtx);

  if((pEntry = mpdpl_find(pl, path, 0))) {
    pEntry->state = state;
  }

  pthread_mutex_unlock(&pl->mtx);

  return pEntry;
}

PLAYLIST_MPD_ENTRY_T *mpdpl_add(PLAYLIST_MPD_T *pl, const char *path, PLAYLIST_MPD_STATE_T state) {
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;
  size_t sz;

  if(!pl || !path || pl->pEntriesBuf) {
    return NULL;
  } 

  //
  // Check if the entry path already exists
  //
  pthread_mutex_lock(&pl->mtx);
  if((pEntry = mpdpl_find(pl, path, 0))) {
    pEntry->state = state;
    pthread_mutex_unlock(&pl->mtx);
    return pEntry;
  }
  pthread_mutex_unlock(&pl->mtx);

  sz = strlen(path) + 1;

  if(!(pEntry = (PLAYLIST_MPD_ENTRY_T *) avc_calloc(1, sizeof(PLAYLIST_MPD_ENTRY_T)))) {
    return NULL;
  }
 
  if(!(pEntry->path = avc_calloc(1, sz))) {
    free(pEntry);
    return NULL;
  }

  //fprintf(stderr, "CREATING MPD ENTRY FOR PL 0x%x ('%s')\n", pl, path);
  memcpy(pEntry->path, path, sz);
  pEntry->state = state;

  mpdpl_attach_int(pl, pEntry, 1);

  return pEntry;
}


static int mpdpl_pop(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry, PLAYLIST_MPD_ENTRY_T *pEntryPrior) {

  if(!pEntry) {
    return -1;
  }

  if(pEntryPrior) {
    pEntryPrior->pnext = pEntry->pnext;
  } else {
    pl->pEntriesHead = pEntry->pnext;
  }
  if(pEntry == pl->pEntriesTail) {
    pl->pEntriesTail = pEntryPrior;
  }

  if(pl->numEntries > 0) {
    pl->numEntries--;
  }

  return 0;
}

int mpdpl_peek_head(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry) {
  int rc = 0;

  if(!pl || !pEntry) {
    return -1;
  }

  pthread_mutex_lock(&pl->mtx);

  if(pl->pEntriesHead) {
    memcpy(pEntry, pl->pEntriesHead, sizeof(PLAYLIST_MPD_ENTRY_T));
    rc = 1;
  }

  pthread_mutex_unlock(&pl->mtx);

  return rc;
}

PLAYLIST_MPD_ENTRY_T *mpdpl_pop_head(PLAYLIST_MPD_T *pl) {
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;

  pthread_mutex_lock(&pl->mtx);

  pEntry = pl->pEntriesHead;

  if(mpdpl_pop(pl, pEntry, NULL) != 0) {
    pEntry = NULL;
  }

  pthread_mutex_unlock(&pl->mtx);

  return pEntry;
}

int mpdpl_remove(PLAYLIST_MPD_T *pl, const char *path) {
  int rc = 0;
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;
  PLAYLIST_MPD_ENTRY_T *pEntryPrior = NULL;

  if(!pl || !path) {
    return -1;
  }

  pthread_mutex_lock(&pl->mtx);

  pEntry = pl->pEntriesHead;

  while(pEntry) {

    if(!strcmp(pEntry->path, path)) {

      mpdpl_pop(pl, pEntry, pEntryPrior);

      mpdpl_free_entry(pl, pEntry);

      rc = 1;
      break;
    }

    pEntryPrior = pEntry;
    pEntry = pEntry->pnext;
  }

  pthread_mutex_unlock(&pl->mtx);

  return rc;
}
