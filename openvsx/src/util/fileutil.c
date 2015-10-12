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

int OpenMediaReadOnly(FILE_STREAM_T *pStream, const char *path) {

  struct stat st;

  if(!pStream || !path) {
    return -1;
  } 

  memset(pStream, 0, sizeof(FILE_STREAM_T));

  if((pStream->fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for reading: '%s' "ERRNO_FMT_STR), path, ERRNO_FMT_ARGS);
    return -1;
  }

  if(fileops_stat(path, &st) != 0) {
    LOG(X_ERROR("Unable to get file size of '%s'."), path);
    //fileops_Close(pStream->fp);
    //pStream->fp = FILEOPS_INVALID_FP;
    CloseMediaFile(pStream);
    return -1;
  } else if(st.st_mode & S_IFDIR) {
    LOG(X_ERROR("Refusing to open directory '%s' for reading"), path);
    CloseMediaFile(pStream);
    return -1;
  }

  pStream->size = st.st_size;

  if(path != pStream->filename) {
    strncpy(pStream->filename, path, sizeof(pStream->filename) - 1);
  }

  return 0;
}

int CloseMediaFile(FILE_STREAM_T *pStream) {

  if(!pStream) {
    return -1;
  }

  if(pStream->fp != FILEOPS_INVALID_FP) {
    fileops_Close(pStream->fp);
    memset(pStream, 0, sizeof(FILE_STREAM_T));
    pStream->fp = FILEOPS_INVALID_FP;
  }

  return 0;
}

int ReadFileStream(FILE_STREAM_T *fs, void *buf, uint32_t size) {

  //LOG(X_DEBUGV("Reading %d"), size);
  VSX_DEBUGLOG3("Reading %d\n", size);

  if(fileops_Read(buf, 1, size, fs->fp) != size) {

    if(fileops_Feof(fs->fp)) {
      return -1;
    }

    LOG(X_ERROR("Failed to read %d from '%s' at file:0x%llx"), size, fs->filename, fs->offset);
    return -1;
  }
  fs->offset += size;

  return (int) size;
}

int ReadFileStreamNoEof(FILE_STREAM_T *fs, void *buf, uint32_t size) {
  unsigned int lenToRead = size;

  if((FILE_OFFSET_T) lenToRead >  (fs->size - fs->offset)) {
    lenToRead = (unsigned int) (fs->size - fs->offset);
  }

  if(ReadFileStream(fs, buf, lenToRead) != lenToRead) {
    return -1;
  }

  return lenToRead;
}

int WriteFileStream(FILE_STREAM_T *fs, const void *buf, uint32_t size) {

  if(fileops_WriteBinary(fs->fp, (unsigned char *) buf, size) != size) {
    LOG(X_ERROR("Failed to write %d to '%s'"), size, fs->filename);
    return -1;
  }

  fs->offset += size;
  return (int) size;
}

int SeekMediaFile(FILE_STREAM_T *fs, FILE_OFFSET_T offset, int whence) {

  if(!fs) {
    return -1;
  }

  if(fileops_Fseek(fs->fp, offset, whence) != 0) {
    LOG(X_ERROR("Unable to seek in file '%s' to offset %lld from %d."), 
          fs->filename, offset, whence);
    return -1;
  }

  //TODO: this should be an #ifdef
  if(sizeof(long) == 4) {
  
    if(whence == SEEK_SET) {
      fs->offset = offset;
    } else if(whence == SEEK_END) {
      fs->offset = fs->size;
    } else if(whence == SEEK_CUR) {
      fs->offset += offset;
    }

  } else {

    if((fs->offset = fileops_Ftell(fs->fp)) == -1) {
      LOG(X_ERROR("Unable to get file size."));
      return -1;
    }

  } 
  return 0;
}
