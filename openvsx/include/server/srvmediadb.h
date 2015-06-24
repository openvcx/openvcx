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


#ifndef __SERVER_MEDIADB_H__
#define __SERVER_MEDIADB_H__

#include "unixcompat.h"

#define MEDIADB_PREFIXES_MAX    10

#define VSXTMP_EXT              ".vsxtmp"
#define VSXTMP_EXT_LEN          8 
#define NOTN_EXT                ".notn"
#define NOTN_EXT_LEN             5 

typedef struct MEDIADB_DESCR {
  const char *dbDir;
  const char *mediaDir;
  const char *homeDir;
  const char *avcthumb;
  const char *avcthumblog;
  const char *ignorefileprefixes[MEDIADB_PREFIXES_MAX];
  const char *ignoredirprefixes[MEDIADB_PREFIXES_MAX];
  int tmnDeltaSec;
  const char tn_suffix[8];
  int lgTnWidth;
  int lgTnHeight;
  int smTnWidth;
  int smTnHeight;
} MEDIADB_DESCR_T;

int mediadb_checkfileext(const char *filepath, int includePl, int display);
int mediadb_isvalidDirName(const MEDIADB_DESCR_T *pMediaDb, const char *d_name);
int mediadb_isvalidFileName(const MEDIADB_DESCR_T *pMediaDb, const char *d_name,
                            int includePl, int display);



int mediadb_prepend_dir(const char *pathdir, const char *path,
                       char *out, unsigned int lenout);
int mediadb_prepend_dir2(const char *pathdir, const char *pathdir2, const char *path,
                         char *out, unsigned int lenout);
int mediadb_mkdirfull(const char *root, const char *path);
int mediadb_fixdirdelims(char *path, char delim);
int mediadb_getdirlen(const char *path);
//const char *mediadb_getfilepathext(const char *mediaDir, const char *fullpath);
int mediadb_parseprefixes(const char *prefixstr, const char *prefixOutArr[], 
                          unsigned int maxPrefix);



void mediadb_proc(void *parg);





#endif // __SERVER_MEDIADB_H__
