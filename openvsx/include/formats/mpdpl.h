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


#ifndef __EXT_MPD_PL_H__
#define __EXT_MPD_PL_H__

#include "unixcompat.h"

typedef enum PLAYLIST_MPD_STATE {
  PLAYLIST_MPD_STATE_INIT           = 0,
  PLAYLIST_MPD_STATE_DOWNLOADING    = 1,
  PLAYLIST_MPD_STATE_HAVEMDAT       = 2,
  PLAYLIST_MPD_STATE_DOWNLOADED     = 3,
  PLAYLIST_MPD_STATE_ERROR          = 4
} PLAYLIST_MPD_STATE_T;

typedef struct PLAYLIST_MPD_ENTRY {
  struct PLAYLIST_MPD_ENTRY       *pnext;
  char                            *path;
  PLAYLIST_MPD_STATE_T             state;
  void                            *pUserData;
} PLAYLIST_MPD_ENTRY_T;

typedef void (* MPD_CB_FREE)(PLAYLIST_MPD_ENTRY_T *);

typedef struct PLAYLIST_MPD_T {
  unsigned int                     numEntries;
  PLAYLIST_MPD_ENTRY_T            *pEntriesHead;
  PLAYLIST_MPD_ENTRY_T            *pEntriesTail;
  PLAYLIST_MPD_ENTRY_T            *pEntriesBuf;
  PLAYLIST_MPD_ENTRY_T             initializer;
  int                              endOfList;
  int                              targetDuration;
  pthread_mutex_t                  mtx;
  MPD_CB_FREE                      cbFreeEntry;
} PLAYLIST_MPD_T;

int mpdpl_create(PLAYLIST_MPD_T *pl, const char *buf);
void mpdpl_free(PLAYLIST_MPD_T *pl);
void mpdpl_free_entry(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry);
void mpdpl_dump(const PLAYLIST_MPD_T *pl);
PLAYLIST_MPD_ENTRY_T *mpdpl_add(PLAYLIST_MPD_T *pl, const char *path, PLAYLIST_MPD_STATE_T state);
PLAYLIST_MPD_ENTRY_T *mpdpl_update(PLAYLIST_MPD_T *pl, const char *path, PLAYLIST_MPD_STATE_T state);
int mpdpl_attach(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry);
int mpdpl_remove(PLAYLIST_MPD_T *pl, const char *path);
PLAYLIST_MPD_ENTRY_T *mpdpl_pop_head(PLAYLIST_MPD_T *pl);
int mpdpl_peek_head(PLAYLIST_MPD_T *pl, PLAYLIST_MPD_ENTRY_T *pEntry);

#endif //__EXT_MPD_PL_H__
