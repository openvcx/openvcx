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


#ifndef __SERVER_DIRLIST_H__
#define __SERVER_DIRLIST_H__

#include "unixcompat.h"
#include "formats/metafile.h"

#ifndef WIN32

#endif // WIN32

#include "util/fileutil.h"
#include "server/srvmediadb.h"

#define DIR_ENTRY_LIST_BUFNUM        100


enum DIR_SORT {
  DIR_SORT_NONE            = 0,
  DIR_SORT_NAME_ASC        = 1,
  DIR_SORT_NAME_DESC       = 2,
  DIR_SORT_SIZE_ASC        = 3,
  DIR_SORT_SIZE_DESC       = 4,
  DIR_SORT_TIME_ASC        = 5,
  DIR_SORT_TIME_DESC       = 6
};


typedef struct DIR_ENTRY {
  char                      d_name[256];
  char                      displayname[META_FILE_DESCR_LEN];
  int                       d_type;
  FILE_OFFSET_T             size;
  time_t                    tm;
  int                       numTn;
  double                    duration;
  struct DIR_ENTRY         *pnext;
  struct DIR_ENTRY         *pprev;
} DIR_ENTRY_T;

typedef struct DIR_ENTRY_LIST {
  DIR_ENTRY_T              *pBuf;
  unsigned int              num;
  unsigned int              numAlloc;
  DIR_ENTRY_T              *pHead;
  struct DIR_ENTRY_LIST    *pnext;

  unsigned int              cntTotal;
  unsigned int              cntTotalInDir;
} DIR_ENTRY_LIST_T;



DIR_ENTRY_LIST_T *direntry_getentries(const MEDIADB_DESCR_T *pMediaDb, 
                                      const char *dir,
                                      const char *fidxdir, 
                                      const char *searchstr,
                                      int includedirs,
                                      unsigned int startidx,
                                      unsigned int max,
                                      enum DIR_SORT sort);
void direntry_free(DIR_ENTRY_LIST_T *pList);




#endif // __SERVER_DIRLIST_H__
