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


#ifndef __SERVER_FILE_IDX_H__
#define __SERVER_FILE_IDX_H__

#include "unixcompat.h"
#include "util/fileutil.h"

#define FILE_LIST_IDXNAME         ".fidx.txt"

#define FILE_LIST_ENTRY_NAME_LEN     128


typedef struct FILE_LIST_ENTRY {
  char                           name[FILE_LIST_ENTRY_NAME_LEN]; 
  FILE_OFFSET_T                  size;
  double                         duration;
  char                           vstr[64]; 
  char                           astr[64]; 
  int                            flag;
  int                            numTn;
  time_t                         tm;
  struct FILE_LIST_ENTRY        *pnext;
} FILE_LIST_ENTRY_T;

typedef struct FILE_LIST_T {
  char                           pathmedia[VSX_MAX_PATH_LEN];
  unsigned int                   count;
  FILE_LIST_ENTRY_T             *pentries;
  FILE_LIST_ENTRY_T             *pentriesLast;
} FILE_LIST_T;


int file_list_read(const char *dir, FILE_LIST_T *pFilelist);
int file_list_write(const char *dir, const FILE_LIST_T *pFilelist);
void file_list_close(FILE_LIST_T *pFilelist);

FILE_LIST_ENTRY_T *file_list_find(const FILE_LIST_T *pFilelist, const char *filename);
FILE_LIST_ENTRY_T *file_list_addcopy(FILE_LIST_T *pFilelist, FILE_LIST_ENTRY_T *pEntry);
int file_list_removeunmarked(FILE_LIST_T *pFilelist);

//int file_list_getdirlen(const char *path);
//int file_list_read_file_entry(const char *path, FILE_LIST_ENTRY_T *pEntry);


#endif // __SERVER_FILE_IDX_H__
