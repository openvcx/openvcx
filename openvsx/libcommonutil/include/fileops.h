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

#ifndef __FILEOPS_H__
#define __FILEOPS_H__


#if defined(WIN32)

#include <fcntl.h>

#define FILE_HANDLE                         HANDLE
#define FILEOPS_INVALID_FP                  NULL

#endif // __MINGW32__


#if defined(WIN32)

#define DT_UNKNOWN       0
#define DT_FIFO          1
#define DT_CHR           2
#define DT_DIR           4
#define DT_BLK           6
#define DT_REG           8
#define DT_LNK          10
#define DT_SOCK         12
#define DT_WHT          14

#endif // WIN32


#if defined(WIN32) && !defined(__MINGW32__)


#define NAME_MAX  MAX_PATH

//from <sys/stat.h>
#define __S_ISUID       04000   /* Set user ID on execution.  */
#define __S_ISGID       02000   /* Set group ID on execution.  */
#define __S_ISVTX       01000   /* Save swapped text after use (sticky).  */
#define __S_IREAD       0400    /* Read by owner.  */
#define __S_IWRITE      0200    /* Write by owner.  */
#define __S_IEXEC       0100    /* Execute by owner.  */

#define S_IRUSR __S_IREAD       /* Read by owner.  */
#define S_IWUSR __S_IWRITE      /* Write by owner.  */
#define S_IXUSR __S_IEXEC       /* Execute by owner.  */
/* Read, write, and execute by owner.  */
#define S_IRWXU (__S_IREAD|__S_IWRITE|__S_IEXEC)

#define S_IRGRP (S_IRUSR >> 3)  /* Read by group.  */
#define S_IWGRP (S_IWUSR >> 3)  /* Write by group.  */
#define S_IXGRP (S_IXUSR >> 3)  /* Execute by group.  */
/* Read, write, and execute by group.  */
#define S_IRWXG (S_IRWXU >> 3)

#define S_IROTH (S_IRGRP >> 3)  /* Read by others.  */
#define S_IWOTH (S_IWGRP >> 3)  /* Write by others.  */
#define S_IXOTH (S_IXGRP >> 3)  /* Execute by others.  */
/* Read, write, and execute by others.  */
#define S_IRWXO (S_IRWXG >> 3)


//typedef struct _DIR {
//  HANDLE handle;
//  int haveData;
//  WIN32_FIND_DATA findData;
//} DIR;

struct dirent {
  unsigned char d_type;
  char          d_name[NAME_MAX];
};


//struct stat {
//  off_t st_size;
//};

#define   stat64   __stat64


//
// Treat stat as 64bit variant
//
#define stat   stat64  

int fileops_Write(FILE_HANDLE fp, char *fmt, ...);


#else // WIN32



#if defined(__linux__) 

#include <stdio.h>

#if !defined(__USE_LARGEFILE64)
#define __USE_LARGEFILE64
#endif // __USE_LARGEFILE64

#if !defined(__USE_FILE_OFFSET64)
#define __USE_FILE_OFFSET64
#endif // __USE_FILE_OFFSET64

#include <fcntl.h>

#include <dirent.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif // DT_DIR

#endif // __linux__

#if defined(__APPLE__)

#include <stdio.h>
#include <fcntl.h>

#endif // (__APPLE__)


#include <sys/types.h>
#include <sys/stat.h>
//#include <stdio.h>
#include <dirent.h>

#if !defined(__MINGW32__)
#include <unistd.h>
#endif // __MINGW32__


#if defined(__linux__)

#if !defined(USE_STAT_32)
//
// Treat stat as 64bit variant
//
#define stat   stat64  

#endif // USE_STAT_32

#endif // __linux__


#if !defined(__MINGW32__)

#define FILE_HANDLE                         FILE *
#define FILEOPS_INVALID_FP                  NULL
#define fileops_Write(fp, fmt, args...) fprintf(fp, fmt, ##args)

#endif // __MINGW32__


#endif // WIN32

#if defined(WIN32)

#define EOL                                 "\r\n"
#define DIR_DELIMETER                       '\\'
#define DIR_DELIMETER_STR                   "\\"

typedef struct _DIR_FILEOPS {
  HANDLE handle;
  int haveData;
  WIN32_FIND_DATA findData;
} DIR_FILEOPS;

#define DIR DIR_FILEOPS

#else // WIN32

#define EOL                                 "\n"
#define DIR_DELIMETER                       '/'
#define DIR_DELIMETER_STR                   "/"


#endif // WIN32

#if defined(__MINGW32__)

//#ifndef DT_DIR
//#define DT_DIR 4
//#endif // DT_DIR

//temporary fix for mingw compilation
#define d_type d_ino

//ensure stat st_size is 64bit type
#define stat _stati64

#endif // __MINGW32__


FILE_HANDLE fileops_Open(const char *path, int flags);
FILE_HANDLE fileops_OpenSeek(const char *path, int flags, long long offset, int whence);
int fileops_Close(FILE_HANDLE fp);
int fileops_WriteBinary(FILE_HANDLE fp, unsigned char *pData, unsigned int uiLen);
int fileops_WriteExt(FILE_HANDLE fp, char *buf, unsigned int szBuf, char *fmt, ...);
int fileops_Read(void *buf, int sz, int nmemb, FILE_HANDLE fp);
int fileops_Fseek(FILE_HANDLE fp, long long offset, int whence);
long long fileops_Ftell(FILE_HANDLE fp);
int fileops_Feof(FILE_HANDLE fp);
char *fileops_getcwd(char *buf, size_t size);
int fileops_setcwd(const char *buf);
int fileops_touchfile(const char *buf);
int fileops_isBeingWritten(const char *path);

int fileops_stat(const char *path, struct stat *buf);
int fileops_lstat(const char *path, struct stat *buf);
char *fileops_fgets(char *s, int n, FILE_HANDLE fp);


int fileops_Rename(const char *old, const char *new);
int fileops_CopyFile(const char *src, const char *dst);
int fileops_MoveFile(const char *src, const char *dst);
int fileops_DeleteFile(const char *path);

int fileops_MkDir(const char *path, int mode);


DIR *fileops_OpenDir(const char *name);
struct dirent *fileops_ReadDir(DIR *);
int fileops_CloseDir(DIR *);


#endif // __FILEOPS_H__
