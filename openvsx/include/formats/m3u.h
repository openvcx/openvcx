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


#ifndef __EXT_M3U_H__
#define __EXT_M3U_H__

#include "unixcompat.h"
#include "srvmediadb.h"

typedef struct PLAYLIST_M3U_ENTRY {
  struct PLAYLIST_M3U_ENTRY  *pnext;
  double                      durationSec;
  char                       *path;
  char                       *title;
} PLAYLIST_M3U_ENTRY_T;

typedef struct PLAYLIST_M3U {
  unsigned int                numEntries;
  PLAYLIST_M3U_ENTRY_T       *pEntries;
  char                       *dirPl;
  PLAYLIST_M3U_ENTRY_T       *pCur;
  unsigned int                mediaSequence;
  unsigned int                targetDuration;
} PLAYLIST_M3U_T;

int m3u_isvalidFile(const char *path, int extm3u);

PLAYLIST_M3U_T *m3u_create(const char *path);
PLAYLIST_M3U_T *m3u_create_fromdir(const MEDIADB_DESCR_T *pMediaDb, const char *filepath,
                                   int sort);
int m3u_create_buf(PLAYLIST_M3U_T *pl, const char *buf);
void m3u_free(PLAYLIST_M3U_T *pl, int dealloc);
void m3u_dump(const PLAYLIST_M3U_T *pl);



#endif //__EXT_M3U_H__
