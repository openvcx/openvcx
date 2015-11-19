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


#ifndef __EXT_METAFILE_H__
#define __EXT_METAFILE_H__

#include "server/srvfidx.h"
#include "server/srvcmd.h"

#define METAFILE_EXT               ".meta"
#define METAFILE_DEFAULT          "default"METAFILE_EXT

#define META_FILE_XCODESTR_MAX     256
#define META_FILE_PROFILESTR_MAX   16
#define META_FILE_TAGSTR_MAX       16
#define META_FILE_IDSTR_MAX        16
#define META_FILE_DESCR_LEN        128 
#define META_FILE_PROXY_STR_LEN    128
#define META_FILE_TOKEN_LEN        128

typedef struct ENTRY_IGNORE {
  char                    filename[FILE_LIST_ENTRY_NAME_LEN];
  struct ENTRY_IGNORE    *pnext; 
} ENTRY_IGNORE_T;

typedef struct ENTRY_META_DESCRIPTION {
  char                           filename[FILE_LIST_ENTRY_NAME_LEN];
  char                           title[META_FILE_DESCR_LEN];
  struct ENTRY_META_DESCRIPTION *pnext; 
} ENTRY_META_DESCRIPTION_T;

typedef struct META_FILE {
  char                 devicefilterstr[STREAM_DEV_NAME_MAX];
  char                 profilefilterstr[META_FILE_PROFILESTR_MAX];

  // local file path
  char                 filestr[VSX_MAX_PATH_LEN];

  // remote http media link
  char                 linkstr[VSX_MAX_PATH_LEN];

  // child process custom input arguments 
  // (overridding default '--capture' commandline
  char                 instr[256];

  //char                 dimensionsstr[64]; 

  char                 rtmpproxy[META_FILE_PROXY_STR_LEN]; 
  char                 rtspproxy[META_FILE_PROXY_STR_LEN]; 
  char                 httpproxy[META_FILE_PROXY_STR_LEN]; 
  
  // xcode config
  char                 xcodestr[META_FILE_XCODESTR_MAX];

  // stream method types expressed as bitmask
  int                  methodBits;

  // Digest credentials separated by ':'
  char                userpass[AUTH_ELEM_MAXLEN * 2 + 1];

  // security token
  char                tokenId[META_FILE_TOKEN_LEN];

  // id forming unique child proces key 'media rsrc + id' 
  char                id[META_FILE_IDSTR_MAX];

  // boolean flag for shared resource using one common child processor
  int                 shared;

  // boolean flag indicating resource should only be available
  // through secure means, such as SSL/TLS
  int                 secure;

  // The following should only be found in a directory wide metafile
  ENTRY_IGNORE_T            *pignoreList;
  ENTRY_META_DESCRIPTION_T  *pDescriptionList;

  //TODO: methods not yet implemented... procdb check should check which method
  //enum STREAM_METHOD   methods[4]; 
} META_FILE_T;

int metafile_isvalidFile(const char *path);
int metafile_open(const char *path, META_FILE_T *pMetaFile, int readIgnores,
                  int readTitles);
int metafile_close(META_FILE_T *pMetaFile);
int metafile_findprofs(const char *path, const char *devname, 
                      char profs[][META_FILE_PROFILESTR_MAX], unsigned int max);
char *metafile_find(const char *pathmedia, char *pathbuf, 
                    unsigned int szpathbuf, int *ploaddefault);




#endif //__EXT_METAFILE_H__
