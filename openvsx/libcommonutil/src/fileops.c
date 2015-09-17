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

#if defined(WIN32) 

#include <sys/stat.h>
#include <windows.h>

#if !defined(__MINGW32__)

#include "unixcompat.h"
#include "pthread_compat.h"

#endif // __MINGW32__

#else  // WIN32

#define __USE_LARGEFILE64 1
#define __USE_FILE_OFFSET64  1

#if defined(__linux__)

#define _LARGEFILE_SOURCE   1
#define _LARGEFILE64_SOURCE  1
#define _FILE_OFFSET_BITS  64

#endif // (__linux__)


#if !defined(__MINGW32__)
#include <unistd.h>
#endif // __MINGW32__
#include <dirent.h>
#include <string.h>


#endif // WIN32


#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include "logutil.h"
#include "fileops.h"


//#if defined(WIN32) && !defined(__MINGW32__)
#if defined(WIN32) 

FILE_HANDLE fileops_Open(const char *path, int flags) {
  FILE_HANDLE fp;
  unsigned int accessMode;
  unsigned int createFlag;
  unsigned int shareFlag = 0;

  //
  // O_TRUNC is not implemented
  //

  if(path == NULL) {
    return FILEOPS_INVALID_FP;
  }

  if(flags & O_RDWR)
    accessMode = FILE_ALL_ACCESS;
  else if(flags & O_WRONLY)
    accessMode = GENERIC_WRITE;
  else {
    accessMode = GENERIC_READ;
    shareFlag = FILE_SHARE_READ | FILE_SHARE_WRITE;
  }
  //else
  //  return FILEOPS_INVALID_FP;

  if((flags & O_CREAT) && (flags & O_EXCL))
    createFlag = CREATE_NEW;
  else if(!(flags & O_CREAT))
    createFlag = OPEN_EXISTING;
  else if(flags & O_APPEND)
    createFlag = OPEN_ALWAYS;
  else
    createFlag = CREATE_ALWAYS;


  if((fp =  CreateFile(path, 
                       accessMode, 
                       shareFlag, 
                       NULL, 
                       createFlag, 
                       FILE_FLAG_BACKUP_SEMANTICS,//FILE_ATTRIBUTE_NORMAL, 
                       NULL)) ==  INVALID_HANDLE_VALUE) {

		return FILEOPS_INVALID_FP;
	}

  if(flags & O_APPEND) {
    if(SetFilePointer(fp, 0, 0, FILE_END) == INVALID_SET_FILE_POINTER) {
      CloseHandle(fp);
      return FILEOPS_INVALID_FP;
    }
  }

	return fp;
}

int fileops_isBeingWritten(const char *path) {
  FILE_HANDLE fp = INVALID_HANDLE_VALUE;
  int rc = -1;

  if((fp =  CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, 
                       OPEN_EXISTING, 0 , NULL)) ==  INVALID_HANDLE_VALUE) {

    if(GetLastError() == ERROR_SHARING_VIOLATION &&
       (fp =  CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                       NULL, OPEN_EXISTING, 0 , NULL)) !=  INVALID_HANDLE_VALUE) {
      rc = 1; 
    }

  } else {
    rc = 0;
  }

  if(fp != INVALID_HANDLE_VALUE) {
    CloseHandle(fp);
  }

  return rc;
}

FILE_HANDLE fileops_OpenSeek(const char *path, int flags, long long offset, int whence) {
  FILE_HANDLE fp;

  if((fp = fileops_Open(path, flags)) == FILEOPS_INVALID_FP) {
    return fp;
  }

  if(fileops_Fseek(fp, offset, whence) != 0) {
    LOG(X_ERROR("Unable to seek %d"), offset);
    fileops_Close(fp);
    return FILEOPS_INVALID_FP;
  }

  return fp;
}

int fileops_Close(FILE_HANDLE fp) {

  if(CloseHandle(fp))
    return 0;
  else
    return -1;
}

int fileops_WriteBinary(FILE_HANDLE fp, unsigned char *pData, unsigned int uiLen) {

  unsigned int ui;
  
  if(uiLen == 0) {
    return 0;
  } else if(pData == NULL || fp == 0) {
    return -1;
  }

  if(WriteFile(fp ,pData, uiLen, (DWORD *) &ui, NULL) == 0 || ui != uiLen) {
    return -1;
  }

  return (int) uiLen;
}

int fileops_Write(FILE_HANDLE fp, char *fmt, ...) {
  int rc;
  char buf[4096];
  va_list vlist;

  va_start(vlist, fmt);

  rc = _vsnprintf(buf, sizeof(buf)-1, fmt,  vlist);
  va_end(vlist);

  if(rc > 0) {
    rc = fileops_WriteBinary(fp, (unsigned char *) buf, rc);
  }

  return rc;
}

int fileops_WriteExt(FILE_HANDLE fp, char *buf, unsigned int szBuf, char *fmt, ...) {
  int rc;
  va_list vlist;

  va_start(vlist, fmt);
  rc = _vsnprintf(buf, szBuf, fmt,  vlist);
  va_end(vlist);

  if(rc > 0) {
    rc = fileops_WriteBinary(fp, (unsigned char *) buf, rc);
  }

  return rc;
}

int fileops_Read(void *buf, int sz, int nmemb, FILE_HANDLE fp) {
  unsigned int numRead = 0;

  if(sz == 0 || nmemb == 0)
    return 0;

  if(ReadFile(fp, buf, nmemb * sz, (DWORD *) &numRead, NULL) == 0) {
    return -1;
  }

  return numRead / sz;
}

int fileops_Fseek(FILE_HANDLE fp, long long offset, int whence) {
  unsigned int moveMethod;
  //LONG distLo = (LONG) offset;
  //LONG distHi = (LONG) (offset >> 32);
  //LONG *pDistHi = NULL;
  LARGE_INTEGER lg;
  
  switch(whence) {
    case SEEK_SET:
      moveMethod = FILE_BEGIN;
      break;
    case SEEK_CUR:
      moveMethod = FILE_CURRENT;
      break;
    case SEEK_END:
      moveMethod = FILE_END;
      break;
    default:
      return -1;
  }

  //if(distHi != 0) {
  //  pDistHi = &distHi;
  //}
  lg.QuadPart = offset;

  //if(SetFilePointer(fp, distLo, pDistHi, moveMethod) == INVALID_SET_FILE_POINTER)
  //  return -1;
  if(SetFilePointerEx(fp, lg, NULL, moveMethod) == 0)
    return -1;

  return 0;
}

long long fileops_Ftell(FILE_HANDLE fp) {
  long long pos;
  //LARGE_INTEGER lg, lgOut;
  LONG dwHigh = 0;
  DWORD dwLow = 0;

  //lg.QuadPart = 0;

  if((dwLow = SetFilePointer(fp, 0, &dwHigh, FILE_CURRENT)) == INVALID_SET_FILE_POINTER)
   return -1;
  //if((pos = SetFilePointerEx(fp, lg, &lgOut, FILE_CURRENT)) == 0)
   //return -1;

  pos = ((long long) dwHigh << 32) | dwLow;

  return  pos;
  //return lg.QuadPart;
}

int fileops_Feof(FILE_HANDLE fp) {
  int rc;
  char buf[4];
  unsigned int numRead = 0;

  if((rc = ReadFile(fp, buf, 1, (DWORD *) &numRead, NULL)) != 0) {

    if(numRead == 0) {
      return 1; 
    } else {
      SetFilePointer(fp, -1, NULL, FILE_CURRENT);
      return 0;
    }
  } 

  return 0;
}

int fileops_CopyFile(const char *src, const char *dst) {

  if(CopyFile(src, dst, FALSE))
    return 0;

  return -1;
}

int fileops_MoveFile(const char *src, const char *dst) {

  if(MoveFileEx(src, dst, MOVEFILE_REPLACE_EXISTING))
    return 0;

  return -1;
}

int fileops_DeleteFile(const char *path) {

  if(DeleteFile(path))
    return 0;

  return -1;
}

int fileops_Rename(const char *old, const char *newname) {

  if(MoveFileEx(old, newname, 0))
    return 0;

  return -1;

}


int fileops_MkDir(const char *path, int mode) {

  if(CreateDirectory(path, NULL))
    return 0;

  return -1;
}

//#if !defined(__MINGW32__)

DIR *fileops_OpenDir(const char *name) {
  DIR *pDir = NULL;
  char *pName = NULL;
  size_t sz;

  if(!name)
    return NULL;

  if((sz = strlen(name)) <= 0)
    return NULL;

  if((pName = (char *) calloc(1, sz + 3)) == NULL) {
    LOG(X_ERROR("Unable to allocate %d bytes"), sz + 3);
    return NULL;
  }

  strcpy(pName, name);
  if(pName[sz -1] != DIR_DELIMETER)
    pName[sz++] = DIR_DELIMETER;
  pName[sz++] = '*';
  pName[sz] = '\0';

  if((pDir = (DIR *) calloc(1, sizeof(DIR))) == NULL) {
    LOG(X_ERROR("Unable to allocate %d bytes"), sizeof(DIR));
    free(pName);
    return NULL;
  }

  if((pDir->handle = FindFirstFile(pName, &pDir->findData)) == INVALID_HANDLE_VALUE) {
    free(pName);
    free(pDir);
    return NULL;
  }

  pDir->haveData = 1;

  free(pName);

  return pDir;
}

struct dirent *fileops_ReadDir(DIR *pDir) {
  static struct dirent st;

  if(pDir->haveData == 0) {
    if(FindNextFile(pDir->handle, &pDir->findData) == FALSE) {
      return NULL;
    }
  }

  strncpy(st.d_name, pDir->findData.cFileName, sizeof(st.d_name) -1);
  st.d_name[sizeof(st.d_name) -1] = '\0';

  switch(pDir->findData.dwFileAttributes) {
    case FILE_ATTRIBUTE_DIRECTORY:
      st.d_type = DT_DIR;
      break;
    default:
      st.d_type = DT_REG;
      break;
  }

  pDir->haveData = 0;

  return &st;
}

int fileops_CloseDir(DIR *pDir) {
  int rc = -1;

  if(pDir) {
    rc = ! FindClose(pDir->handle);
    pDir->handle = INVALID_HANDLE_VALUE;
    free(pDir);
  }
  return rc;
}

//#endif // __MINGW32__

char *fileops_getcwd(char *buf, size_t size) {
  if(GetCurrentDirectory((DWORD) size, buf) != 0) {
    return buf;
  }
  return NULL;
}
int fileops_setcwd(const char *buf) {
  if(SetCurrentDirectory(buf) != 0) {
    return 0;
  }
  return -1;
}

static time_t fileTimeToTimeT(FILETIME *pFt) {
  LARGE_INTEGER jan1970FT = {{0, 0}};
  jan1970FT.QuadPart = 116444736000000000ll; // january 1st 1970

  return (time_t)  ((((LARGE_INTEGER *)pFt)->QuadPart - jan1970FT.QuadPart)/10000000);
}


int fileops_stat(const char *path, struct stat *buf) {
  FILE_HANDLE fp;
  LARGE_INTEGER sz;
  FILETIME ft[3];
  DWORD dw = 0;
  int rc = 0;

  if(path == NULL) {
    return -1;
  }

  memset(buf, 0, sizeof(struct stat));

  if((dw = GetFileAttributes(path)) == INVALID_FILE_ATTRIBUTES) {
    return -1;
  } else if(dw & FILE_ATTRIBUTE_DIRECTORY) {
    buf->st_mode = S_IFDIR;
    return 0;
  }

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    return -1;
  }

#if defined(__MINGW32__)
  if((buf->st_size = (LONGLONG) GetFileSize(fp, &dw)) != INVALID_FILE_SIZE) {
//fprintf(stderr, "path:'%s' sz: %I64d %d\n", path, buf->st_size, dw);
    if(dw != 0) {
      buf->st_size |= (((LONGLONG) dw) << 32);
    }
#else
  if(GetFileSizeEx(fp, &sz) != 0) {
    buf->st_size = sz.QuadPart;
#endif // __MINGW32__
    buf->st_mode = S_IFREG;
  } else {
    rc = -1;
  }

  if(rc == 0 && GetFileTime(fp, &ft[0],   // last create time
                                &ft[1],   // last access time
                                &ft[2]    // last write time
                                      ) != 0) {

    //buf->st_ctime = ft[2].dwLowDateTime | (((LONGLONG) ft[2].dwHighDateTime) << 32);
    buf->st_ctime = fileTimeToTimeT(&ft[2]);
    //buf->st_atime = ft[1].dwLowDateTime | (((LONGLONG) ft[1].dwHighDateTime) << 32);
    buf->st_atime = fileTimeToTimeT(&ft[1]);
    buf->st_mtime = buf->st_ctime;

  } else {
    rc = 1;
  }

  fileops_Close(fp);

  return rc;
}
#define fileops_lstat fileops_stat

char *fileops_fgets(char *s, int n, FILE_HANDLE fp) {
  unsigned int numRead = 0;
  unsigned int idx = 0;

  if(s == NULL || n == 0)
    return NULL;

  while(idx < (unsigned int) n -1) {
    numRead = 1;
    if(ReadFile(fp, &s[idx], 1, (DWORD *) &numRead, NULL) == 0) {
      return NULL;
    }
    
    if(numRead == 0 && idx == 0) {
      //eof
      return NULL;
    } else if(numRead == 0) {
      break;
    } else if(s[idx] == '\n') {
      idx += numRead;
      break;
    }
 
    idx += numRead;
  }
/*
  if(numRead > 0 && idx < (unsigned int) n -1) {
    idx++;

    if(s[idx] == '\r') {
      if(ReadFile(fp, &s[idx], 1, (DWORD *) &numRead, NULL) == 0) {
        return NULL;
      }
      if(numRead > 0) {
        idx++;
      }
    }
  }
*/
  s[idx] = '\0';

  return s;
}



#else // WIN32

//
//
// The following apply to non Win32 definitions

#define MAX_FILE_OPEN_FLAGS_LEN           8

FILE_HANDLE fileops_Open(const char *path, int flags) {

  char strflags[MAX_FILE_OPEN_FLAGS_LEN];
  struct stat st;

  //
  // O_TRUNC is not implemented
  //

  memset(strflags, 0, sizeof(strflags));

  if((flags & O_RDONLY) || (O_RDONLY == 0 && O_RDONLY == flags)) {

    strflags[0] = 'r';

  } else if(flags & O_CREAT) {

    if((flags & O_EXCL) && stat(path, &st) == 0) {
      return NULL;
    }

    if(flags & O_APPEND) {
      strflags[0] = 'a';
      if(!(flags & O_WRONLY)) {
        strflags[1] = '+';
      }
    } else {
      strflags[0] = 'w';
    }

  } else {
    strflags[0] = 'r';
    strflags[1] = '+';
  }


  return fopen(path, strflags);
}

int fileops_isBeingWritten(const char *path) {

  struct stat st, st2;

 if(stat(path, &st) != 0) {
   return -1;
 }

  usleep(500000);

  if(stat(path, &st2) != 0) {
    return -1;
  }

  if(st.st_size != st2.st_size) {
    return 1;
  } else {
    return 0;
  }

/*
  int fd;
  int rc = 0;

  if((fd = open(path, O_RDWR)) != 0) {

    if(flock(fd, LOCK_EX) == 0) {
      flock(fd, LOCK_UN);
    } else {
      rc = 1;
    }

    close(fd);
  }
  return rc;
*/

}

FILE_HANDLE fileops_OpenSeek(const char *path, int flags, long long offset, int whence) {
  FILE_HANDLE fp;
  int fd;

  //
  // For a file opened with O_APPEND, any subsequent write operation
  // is automatically prefixed by positioning the file pointer to the end of the file
  // To enable subsequent fseek / lseek functionality, the file is 
  // opened without O_APPEND and the desired file position is obtained
  // via fseek before returning to the caller
  //

  if(flags & (O_CREAT | O_APPEND)) {

    flags &= ~O_APPEND;

    if((fd = open(path, flags)) == -1) {
      return FILEOPS_INVALID_FP;
    }

    if((fp = fdopen(fd, "w")) == NULL) {
      close(fd);
      return FILEOPS_INVALID_FP;
    }

  } else if((fp = fileops_Open(path, flags)) == FILEOPS_INVALID_FP) {
    return fp;
  }

  if(fseek(fp, offset, whence) != 0) {
    LOG(X_ERROR("Unable to seek %lld"), offset);
    fclose(fp);
    return FILEOPS_INVALID_FP;
  }

  return fp;
}


int fileops_Close(FILE_HANDLE fp) {
  return fclose(fp);
}

int fileops_WriteBinary(FILE_HANDLE fp, unsigned char *pData, unsigned int uiLen) {
  int fd;
  ssize_t sz;

  if(uiLen == 0 || pData == NULL || fp == NULL) {
    return -1;
  }

  fd = fileno(fp);

  if((sz = write(fd, pData, uiLen)) != uiLen) {
    return -1;
  }

  return (int) sz;
}
int fileops_WriteExt(FILE_HANDLE fp, char *buf, unsigned int szBuf, char *fmt, ...) {
  int rc;
  va_list vlist;

  va_start(vlist, fmt);
  rc = vsnprintf(buf, szBuf, fmt,  vlist);
  va_end(vlist);

  if(rc > 0) {
    rc = fileops_WriteBinary(fp, (unsigned char *) buf, rc);
  }

  return rc;
}

int fileops_Read(void *buf, int sz, int nmemb, FILE_HANDLE fp) {
  return fread(buf, sz, nmemb, fp);
}

int fileops_Fseek(FILE_HANDLE fp, long long offset, int whence) {

  //int cntsk = 0;
  long long idx = 0;
  long long tmpoffset = 0;
  int whencetmp = whence;
  int rc = 0;

  //TODO: this should be an ifdef
  if(sizeof(long) == 4) {

    if(offset == 0) {
      return fseek(fp, offset, whence);
    } else if(whence == SEEK_END) {
      return -1;
    }

    while(idx < offset) {
      if((tmpoffset = (offset - idx)) > 0x7fffffff) {
        tmpoffset = 0x7fffffff;
      }
      //if(cntsk++ > 0) {
      //  fprintf(stderr, "seeking[%d] time %lld (orig:%lld)\n", cntsk, tmpoffset, offset);
      //}

      if((rc = fseek(fp, tmpoffset, whencetmp)) < 0) {
        return rc;
      }
      idx += tmpoffset;
      whencetmp = SEEK_CUR;
    }

    return rc;

  } else {
    return fseek(fp, offset, whence);
  }
}

long long fileops_Ftell(FILE_HANDLE fp) {

  //TODO: this should be an #ifdef
  if(sizeof(long) == 4) {
    fprintf(stderr, "ftell should not be used on 32bit systems\n");
  }

  return ftell(fp);
}

int fileops_Feof(FILE_HANDLE fp) {
  return feof(fp);
}

int fileops_CopyFile(const char *src, const char *dst) {
//#if defined(__MINGW32__)
//  if(CopyFile(src, dst, FALSE))
//    return 0;
//  return -1;
//#else // __MINGW32__
  return link(src, dst);
//#endif // __MINGW32__
}

int fileops_MoveFile(const char *src, const char *dst) {
  return rename(src, dst);
}

int fileops_DeleteFile(const char *path) {
  return unlink(path);
}

int fileops_Rename(const char *old, const char *newname) {
  return rename(old, newname);
}

int fileops_MkDir(const char *path, int mode) {
//#if defined(__MINGW32__)
//  return mkdir(path);
//#else // __MINGW32__
  return mkdir(path, mode);
//#endif // __MINGW32__
}

char *fileops_getcwd(char *buf, size_t size) {
  return getcwd(buf, size);
}
int fileops_setcwd(const char *buf) {
  return chdir(buf);
}
int fileops_stat(const char *path, struct stat *buf) {
#if defined(__linux__)
  return stat64(path, buf);
#else
  // __APPLE__
  return stat(path, buf);
#endif // __linux
}
int fileops_lstat(const char *path, struct stat *buf) {
#if defined(__linux__)
  return lstat64(path, buf);
#else
  // __APPLE__
  return lstat(path, buf);
#endif // __linux
}
char *fileops_fgets(char *s, int n, FILE_HANDLE fp) {
  return fgets(s, n, fp);
}

#endif  // WIN32


#if !defined(WIN32) 
DIR *fileops_OpenDir(const char *name) {
  if(!name) {
    return NULL;
  }
  return opendir(name);
}

struct dirent *fileops_ReadDir(DIR *dir) {
//#if defined(__MINGW32__)
//  struct stat st;
//  struct dirent *pdirent = readdir(dir);
  //if(pdirent && stat(path, buf) == 0
//#else // __MINGW32__
  return readdir(dir);
//#endif // __MINGW32__
}

int fileops_CloseDir(DIR *dir) {
  if(!dir) {
    return -1;
  }
  return closedir(dir);
}
#endif // WIN32


int fileops_touchfile(const char *path) {
  FILE_HANDLE fp;

  if((fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    return -1;
  }

  fileops_Close(fp);
  return 0;
}

