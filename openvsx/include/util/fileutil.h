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


#ifndef __FILE_UTIL_H__
#define __FILE_UTIL_H__


#include "fileops.h"
#include "unixcompat.h"

typedef uint64_t        FILE_OFFSET_T;

typedef struct FILE_STREAM {
  FILE_HANDLE       fp;
  FILE_OFFSET_T     offset;
  FILE_OFFSET_T     size;
  char              filename[VSX_MAX_PATH_LEN];
} FILE_STREAM_T;


int OpenMediaReadOnly(FILE_STREAM_T *pStream, const char *path);
int CloseMediaFile(FILE_STREAM_T *pStream);
int ReadFileStream(FILE_STREAM_T *fs, void *buf, uint32_t size);
int ReadFileStreamNoEof(FILE_STREAM_T *fs, void *buf, uint32_t size);
int WriteFileStream(FILE_STREAM_T *fs, const void *buf, uint32_t size);
int SeekMediaFile(FILE_STREAM_T *fs, FILE_OFFSET_T offset, int whence);
int path_is_homedir(const char *path);
const char *path_homedirexpand(const char *path, char *buf, size_t szbuf);

#endif // __FILE_UTIL_H__
