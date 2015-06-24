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


#define MAX_MALLOC_FILESZ             1024 
#define PLAYLIST_MAX_ENTRIES           384
#define PLAYLIST_FILE_CHK_SZMAX        128


int m3u_isvalidFile(const char *path, int extm3u) {
  FILE_STREAM_T fs;
  int rc = 0;
  unsigned char buf[256];
  size_t sz;
  size_t idx;
  FILE_OFFSET_T fSize;

  if(OpenMediaReadOnly(&fs, path) != 0) {
    return 0;
  }

  //fprintf(stdout, "m3u check for '%s'\n", path);

  if((fSize = fs.size) > PLAYLIST_FILE_CHK_SZMAX) {
    fSize = PLAYLIST_FILE_CHK_SZMAX;
  }

  while(fs.offset < fSize) {

    sz = sizeof(buf);
    if(fs.offset + sz > fSize) {
      sz = (size_t) (fSize - fs.offset);
    }
    if(ReadFileStream(&fs, buf, sz) != sz) {
      break;
    }

    //
    // Check if the playlist is an extended m3u type
    // 
    if(extm3u) {
      if(!strncasecmp((char *) buf, "#EXTM3U", 7)) {
        rc = 1;
      }
      break;
    }


    // Simply check if the file is a text file
    for(idx = 0; idx < sz; idx++) {
      //if(!isascii(buf[idx])) {
      if(!avc_istextchar(buf[idx])) {
        CloseMediaFile(&fs);
        return 0;
      }
    }

    if(fs.offset == fSize) {
      rc = 1;
    }

  }

  CloseMediaFile(&fs);

  return rc;
}

static int m3u_add(const char *line, PLAYLIST_M3U_T *pl, unsigned int *pidxEntry) {
  size_t sz;
  const char *p, *p2;
  char buf[64];

  //
  // Extended m3u format:
  // #EXTM3U
  // #EXTINF: [ Duration in seconds ], [ Description ]
  // [ http:// | c:\ | filename ]
  //

  if(!strncasecmp(line, "#EXTINF", 7)) {

    if(pl && pl->pEntries && pidxEntry) {

      if(*pidxEntry < pl->numEntries && pl->pEntries[*pidxEntry].path) {
        if( ++(*pidxEntry) >= pl->numEntries) {
          return -1;
        }
        pl->pEntries[(*pidxEntry)-1].pnext = &pl->pEntries[*pidxEntry];
      }

      sz = strlen(line);
      p = line;  
      while(p < line + sz && *p != ':') {
        p++;
      }
      while(p < line + sz && *p == ':') {
        p++;
      }
      p2 = p;
      while(p2 < line + sz && *p2 != ',' && *p2 != '\r' && *p2 != '\n') {
        p2++;
      }
      if((sz = p2 - p) > sizeof(buf) - 1) {
        sz = sizeof(buf) - 1;
      }
      memcpy(buf, p, p2 - p);
      buf[sz] = '\0';
      pl->pEntries[*pidxEntry].durationSec = atof(buf);
      if(*p2 == ',') {
        p2++;
        p = p2;
        if((sz = strlen(p)) > MAX_MALLOC_FILESZ) {
          sz = MAX_MALLOC_FILESZ;
        }
        if(!(pl->pEntries[*pidxEntry].title = (char *) avc_calloc(1, sz + 1))) {
          return -1;
        }
        memcpy(pl->pEntries[*pidxEntry].title, p, sz);
        avc_strip_nl(pl->pEntries[*pidxEntry].title, sz, 0);
      }

    }

  } else if(!strncasecmp(line, "#"HTTPLIVE_EXTX_MEDIA_SEQUENCE, 
                         strlen("#"HTTPLIVE_EXTX_MEDIA_SEQUENCE))) {
    if(pl) {
      p = line + strlen("#"HTTPLIVE_EXTX_MEDIA_SEQUENCE);
      if(*p == ':') {
        p++;
      }
      pl->mediaSequence = atoi(p);
    }

  } else if(!strncasecmp(line, "#"HTTPLIVE_EXTX_TARGET_DURATION, 
                         strlen("#"HTTPLIVE_EXTX_TARGET_DURATION))) {
    if(pl) {
      p = line + strlen("#"HTTPLIVE_EXTX_TARGET_DURATION);
      if(*p == ':') {
        p++;
      }
      pl->targetDuration = atoi(p);
    }

  } else if(line[0] != '#' && line[0] != '\0' && line[0] != '\r' && line[0] != '\n') {

    p = line;
    while(*p != '#' && *p != '\0' && *p != '\r' && *p != '\n') {
      p++;
    }
    if(*p == '#' && *p == '\0' && *p == '\r' && *p == '\n') {
      return 0;
    }

    if(pl && pl->pEntries && pidxEntry) {

      if(pl->pEntries[*pidxEntry].path != NULL) {
        if( ++(*pidxEntry) >= pl->numEntries) {
          return -1;
        }
        pl->pEntries[(*pidxEntry)-1].pnext = &pl->pEntries[*pidxEntry];
      }

      if((sz = strlen(line)) > MAX_MALLOC_FILESZ) {
        sz = MAX_MALLOC_FILESZ;
      }
      if(!(pl->pEntries[*pidxEntry].path = (char *) avc_calloc(1, sz + 1))) {
        return -1;
      }
      memcpy(pl->pEntries[*pidxEntry].path, line, sz);
      avc_strip_nl(pl->pEntries[*pidxEntry].path, sz, 0);

      //fprintf(stderr, "M3U_ADD entry: '%s'\n", line);
    }

    return 1;
  }

  return 0;
 
}

static PLAYLIST_M3U_T *m3u_alloc(PLAYLIST_M3U_T *pl, unsigned int numEntries) {
  int dealloc = pl ? 1 : 0;

  if(!pl && !(pl = (PLAYLIST_M3U_T *) avc_calloc(1, sizeof(PLAYLIST_M3U_T)))) {
    return NULL;
  }

  if(!(pl->pEntries = (PLAYLIST_M3U_ENTRY_T *) avc_calloc(numEntries,
                                              sizeof(PLAYLIST_M3U_ENTRY_T)))) {
    m3u_free(pl, dealloc);
    return NULL;
  }
  pl->numEntries = numEntries;

  return pl;
}

PLAYLIST_M3U_T *m3u_create(const char *path) {
  PLAYLIST_M3U_T *pl = NULL;
  FILE_HANDLE fp;
  int rc;
  unsigned int lineidx = 0;
  unsigned int numEntries = 0;
  unsigned int idxEntry = 0;
  char buf[MAX_MALLOC_FILESZ];
  size_t sz;

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open playlist file for reading: %s"), path);
    return NULL;
  }

  sz = mediadb_getdirlen(path);

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {
    if((rc = m3u_add(buf, NULL, NULL)) > 0) {
      numEntries++;
    }
    if(numEntries >= PLAYLIST_MAX_ENTRIES) {
      LOG(X_WARNING("Reached maximum playlist content limit: %d"), numEntries);
      break;
    }
  }

  if(fileops_Fseek(fp, 0, SEEK_SET) != 0) {
    LOG(X_ERROR("Unable to start of file."));
    return NULL;
  }

  if(!(pl = m3u_alloc(NULL, numEntries))) {
    return pl;
  }

  if(sz > 0) {
    if(!(pl->dirPl = (char *) avc_calloc(1, sz + 2))) {
      m3u_free(pl, 1);
      return NULL;
    }
    memcpy(pl->dirPl, path, sz);
  }

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {
    lineidx++;
    if((rc = m3u_add(buf, pl, &idxEntry)) < 0) {
      LOG(X_ERROR("Unable to read playlist line %u '%s'"), lineidx, buf);
      break;
    }

    if(idxEntry >= numEntries) {
      LOG(X_WARNING("Playlist at %s capacity: %d"), path, idxEntry);
      break;
    }
  }

  fileops_Close(fp);

  return pl;
}

int m3u_create_buf(PLAYLIST_M3U_T *pl, const char *buf) {
  int rc = 0;
  const char *p, *p2;
  unsigned int sz;
  unsigned int idx;
  unsigned int lineidx = 0;
  unsigned int idxEntry = 0;
  unsigned int numEntries = 0;
  char line[MAX_MALLOC_FILESZ];

  if(!pl || !buf) {
    return -1;
  }

  for(idx = 0; idx < 2; idx++) {

    p2 = buf;

    while(*p2 != '\0') {

      p = p2;
      while(*p2 != '\0' && *p2 != '\r' && *p2 != '\n') {
        p2++;
      }   

      if((sz = (p2 - p)) > sizeof(line) - 1) {
        sz = sizeof(line) - 1;
      }
      memcpy(line, p, sz);
      line[sz] = '\0';

      if(idx == 0) {

        if((rc = m3u_add(line, NULL, NULL)) > 0) {
          numEntries++;
        }
        if(numEntries >= PLAYLIST_MAX_ENTRIES) {
          LOG(X_WARNING("Reached maximum playlist content limit: %d"), numEntries);
          break;
        }

      } else {

        lineidx++;
        if((rc = m3u_add(line, pl, &idxEntry)) < 0) {
          LOG(X_ERROR("Unable to read playlist line %u '%s'"), lineidx, buf);
          break;
        }

        if(idxEntry >= numEntries) {
          LOG(X_WARNING("Playlist at capacity: %d"), idxEntry);
          break;
        }


      }

      //fprintf(stderr, "LINE:'%c%c%c  %d\n", p[0], p[1], p[2], (p2 - p));
    
      while(*p2 == '\r' || *p2 == '\n') {
        p2++;
      }

    }

    if(idx == 0 && !m3u_alloc(pl, numEntries)) {
      return -1;
    }


  }

  if(rc >= 0) {
    rc = numEntries;
  }

  return rc;
}

PLAYLIST_M3U_T *m3u_create_fromdir(const MEDIADB_DESCR_T *pMediaDb, const char *filepath,
                                   int sort) {
  PLAYLIST_M3U_T *pl = NULL;
  char dirpath[VSX_MAX_PATH_LEN];
  const char *filename;
  size_t sz;
  unsigned int numEntries = 0;
  DIR_ENTRY_LIST_T *pEntryList = NULL;
  DIR_ENTRY_T *pEntry = NULL;
  int rc = 0;

  if(!filepath) {
    return NULL;
  }

  memset(dirpath, 0, sizeof(dirpath));
  strncpy(dirpath, filepath, sizeof(dirpath) -1);
  sz = mediadb_getdirlen(dirpath);
  dirpath[sz] = '\0';
  filename = &filepath[sz];
  while(*filename == '\\' || *filename == '/') {
    filename++;
  }

  if((pEntryList = direntry_getentries(pMediaDb, dirpath, NULL, 
                                       NULL, 0, 0, 0, sort)) == NULL) {
    return NULL;
  }

  pEntry = pEntryList->pHead;
  while(pEntry) {
    if(!(pEntry->d_type & DT_DIR) && mediadb_checkfileext(pEntry->d_name, 0, 0)) {
      numEntries++;
    }
    pEntry = pEntry->pnext;
  }

  if(numEntries == 0) {
    rc = -1;
  }

  if(rc == 0 && !(pl = (PLAYLIST_M3U_T *) avc_calloc(1, sizeof(PLAYLIST_M3U_T)))) {
    rc = -1;
  }
  if(rc == 0 && !(pl->pEntries = (PLAYLIST_M3U_ENTRY_T *) avc_calloc(numEntries,
                                              sizeof(PLAYLIST_M3U_ENTRY_T)))) {
    rc = -1;
  }

  if(rc == 0 && !(pl->dirPl = (char *) avc_calloc(1, sz + 2))) {
    rc = -1;
  }
  strncpy(pl->dirPl, dirpath, sz +1);

  if(rc == 0) {
    pEntry = pEntryList->pHead;
  } else {
    pEntry = NULL; 
  }

  while(pEntry) {

    if(!(pEntry->d_type & DT_DIR) && mediadb_checkfileext(pEntry->d_name, 0, 0)) {

      if(pl->numEntries >= numEntries) {
        fprintf(stdout, "Playlist at capacity: %d\n", pl->numEntries);
        break;
      }
      sz = strlen(pEntry->d_name);
      if(!(pl->pEntries[pl->numEntries].path = (char *) avc_calloc(1, sz + 1))) {
        LOG(X_WARNING("Failed to alloc %d"), sz+1);
        m3u_free(pl, 1);
        pl = NULL;
        break;
      }
      memcpy(pl->pEntries[pl->numEntries].path, pEntry->d_name, sz);

      // Set the current play file
      if(!strncasecmp(pEntry->d_name, filename, sizeof(pEntry->d_name))) {
        pl->pCur = &pl->pEntries[pl->numEntries];
      }
      if(++pl->numEntries < numEntries) {
        pl->pEntries[pl->numEntries - 1].pnext = &pl->pEntries[pl->numEntries];
      }
    }

    pEntry = pEntry->pnext;
  }


  if(rc != 0) {
    if(pl) {
      m3u_free(pl, 1);
    }
    pl = NULL;
  }

  direntry_free(pEntryList);

  return pl;
}

void m3u_dump(const PLAYLIST_M3U_T *pl) {
  unsigned int idxEntry;

  if(pl && pl->pEntries) {

    fprintf(stdout, "entries: %d\n", pl->numEntries);
    if(pl->dirPl) {
      fprintf(stdout, "dir: '%s'\n", pl->dirPl);
    }
    fprintf(stdout, "media-sequence: %d\n", pl->mediaSequence);
    if(pl->targetDuration > 0) {
      fprintf(stdout, "targetduration: %d\n", pl->targetDuration);
    }
    for(idxEntry = 0; idxEntry < pl->numEntries; idxEntry++) {  
      fprintf(stdout, "[%u] '%s' %f '%s'\n", idxEntry, pl->pEntries[idxEntry].path,
              pl->pEntries[idxEntry].durationSec, pl->pEntries[idxEntry].title);
    }
  }

}

void m3u_free(PLAYLIST_M3U_T *pl, int dealloc) {
  unsigned int idxEntry;

  if(!pl) {
    return;
  }

  if(pl->pEntries) {
    for(idxEntry = 0; idxEntry < pl->numEntries; idxEntry++) {  
      if(pl->pEntries[idxEntry].path) {
        free(pl->pEntries[idxEntry].path);
      }
      if(pl->pEntries[idxEntry].title) {
        free(pl->pEntries[idxEntry].title);
      }
    }

    avc_free((void **) &pl->pEntries);
    //pl->pEntries = NULL;

    if(pl->dirPl) {
      avc_free((void **) &pl->dirPl);
      //pl->dirPl = NULL;
    }

  }

  pl->numEntries = 0; 
  if(dealloc) {
    free(pl);
  }
}

