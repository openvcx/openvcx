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

FILE_LIST_ENTRY_T *file_list_addcopy(FILE_LIST_T *pFilelist, FILE_LIST_ENTRY_T *pEntryArg) {
  FILE_LIST_ENTRY_T *pEntry = NULL; 
  
  if(!pFilelist || !pEntryArg) {
    return NULL;
  }

  if(!(pEntry = (FILE_LIST_ENTRY_T *) avc_calloc(1, sizeof(FILE_LIST_ENTRY_T)))) {
    return NULL;
  }
  memcpy(pEntry, pEntryArg, sizeof(FILE_LIST_ENTRY_T));
  pEntry->pnext = NULL;
//fprintf(stdout, "added to mem flist '%s' numTn:%d\n",pEntry->name, pEntry->numTn);
  if(pFilelist->pentriesLast) {
    pFilelist->pentriesLast->pnext = pEntry;
  } else {
    pFilelist->pentries = pEntry;
  }
  pFilelist->pentriesLast = pEntry;

  return pEntry;
}

FILE_LIST_ENTRY_T *file_list_find(const FILE_LIST_T *pFilelist, const char *filename) {
  FILE_LIST_ENTRY_T *pEntry = NULL;

  if(!pFilelist || !filename) {
    return NULL;
  }

  pEntry = pFilelist->pentries; 
  while(pEntry) {
//fprintf(stdout, "compare '%s' '%s'\n", pEntry->name, filename);
    if(!strcasecmp(pEntry->name, filename)) {
      return pEntry; 
    }    
    pEntry = pEntry->pnext;
  }

  return NULL;
}
/*
int file_list_update(FILE_LIST_T *pFilelist, FILE_LIST_ENTRY_T *pEntryNew) {
  FILE_LIST_ENTRY_T *pEntry = NULL;

  if(!pFilelist || !pEntryNew) {
    return -1;
  }

  pEntry = pFilelist->pentries; 
  while(pEntry) {
    if(!strcmp(pEntry->name, pEntryNew->name)) {

      if(pEntry->size == pEntryNew->size) {
        return 0;
      }
      pEntry->size = pEntryNew->size; 
      pEntry->duration = pEntryNew->duration; 
      return 1; 
    }    
    pEntry = pEntry->pnext;
  }

  if(file_list_add(pFilelist, pEntryNew) == 0) {
    return 1;
  }

  return 0;
}
*/



int file_list_write(const char *dir, const FILE_LIST_T *pFilelist) {
  char idxpath[VSX_MAX_PATH_LEN];
  FILE_HANDLE fp;
  FILE_LIST_ENTRY_T *pEntry = NULL;

#define STR_FMT "file=\"%s\", size=%"LL64"u, duration=%.2f, vid=\"%s\", aud=\"%s\", tn=%d, tm="

  const char *str_fmt32 = STR_FMT"%ld"EOL;
  const char *str_fmt64 = STR_FMT"%"LL64"d"EOL;
  const char *str_fmt = str_fmt32;
  //const char *str_fmt = STR_FMT"%"LL64"d"EOL;

  if(!pFilelist) {
    return -1;
  }

  if(mediadb_prepend_dir(dir, FILE_LIST_IDXNAME, idxpath, sizeof(idxpath)) < 0) {
    return -1;
  }

  if((fp = fileops_Open(idxpath, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for writing"), idxpath);
    return -1;
  }

  fileops_Write(fp, "#vsx auto generated media directory index listing"EOL);
  fileops_Write(fp, "#mediadir=%s"EOL, pFilelist->pathmedia);

  if(sizeof(pEntry->tm) == 8) {
    str_fmt = str_fmt64;
  }

  pEntry = pFilelist->pentries;
  while(pEntry) {

    if(fileops_Write(fp, (char *) str_fmt,
                    pEntry->name, pEntry->size, pEntry->duration,
                    pEntry->vstr, pEntry->astr, pEntry->numTn, pEntry->tm) < 0) {
      LOG(X_ERROR("Failed to write file list entry"));
      fileops_Close(fp);
      return -1;
    }
  

    pEntry = pEntry->pnext;
  }


  fileops_Close(fp);

  return 0;
}

static int parse_line(const char *line, FILE_LIST_ENTRY_T *pEntry) {
  const char *p = line;
  const char *p2;
  KEYVAL_PAIR_T kv[7];
  unsigned int kvIdx = 0;
  size_t sz;

  while(*p==' ') {
    p++;
  }

  if(*p == '#') {
    return 0;
  }
//fprintf(stdout, "line:'%s'\n", line);

  memset(kv, 0, sizeof(kv));

  for(kvIdx = 0; kvIdx < sizeof(kv) / sizeof(kv[0]); kvIdx++ ) {

    p2 = p;
    while(*p2 != '\0' && *p2 != '\r' && *p2 != '\n' && *p2 != ',') {
      p2++;
    }

    if(conf_parse_keyval(&kv[kvIdx], p, p2 - p, '=', 0) < 0) {
      return -1;
    }
    if(kvIdx > 0) {
      kv[kvIdx - 1].pnext = &kv[kvIdx];
    } 
    p = p2; 
    if(*p == ',') {
      p++;
    }
  }

  if((p = conf_find_keyval(&kv[0], "file")) == NULL) {
    LOG(X_ERROR("Invalid index entry '%s'"), line);
    return -1;
  }
  if(*p == '"') {
     p++;
  }
  strncpy(pEntry->name, p, sizeof(pEntry->name) - 1);
  if((sz = strlen(pEntry->name)) > 0 && pEntry->name[sz -1]=='"') {
    pEntry->name[sz-1]='\0';
  }
//fprintf(stdout, "GOT NAME: '%s'\n", pEntry->name);  
  if((p = conf_find_keyval(&kv[0], "size")) == NULL) {
    LOG(X_ERROR("Invalid index entry '%s'"), line);
    return -1;
  }
  pEntry->size = atoll(p);

  if((p = conf_find_keyval(&kv[0], "duration")) == NULL) {
    LOG(X_ERROR("Invalid index entry '%s'"), line);
    return -1;
  }
  pEntry->duration = atof(p);

  if((p = conf_find_keyval(&kv[0], "vid"))) {
    if(*p == '"') {
       p++;
    }
    strncpy(pEntry->vstr, p, sizeof(pEntry->vstr) - 1);
    if((sz = strlen(pEntry->vstr)) > 0 && pEntry->vstr[sz -1]=='"') {
      pEntry->vstr[sz-1]='\0';
    }
  }

  if((p = conf_find_keyval(&kv[0], "aud"))) {
    if(*p == '"') {
       p++;
    }
    strncpy(pEntry->astr, p, sizeof(pEntry->astr) - 1);
    if((sz = strlen(pEntry->astr)) > 0 && pEntry->astr[sz -1]=='"') {
      pEntry->astr[sz-1]='\0';
    }
  }

  if((p = conf_find_keyval(&kv[0], "tm"))) {
    pEntry->tm = atoll(p);
  }

  if((p = conf_find_keyval(&kv[0], "tn"))) {
    pEntry->numTn = atol(p);
  }

  return 1;
}

int file_list_read(const char *dir, FILE_LIST_T *pFilelist) {
  char idxpath[VSX_MAX_PATH_LEN];
  char buf[1024];
  FILE_HANDLE fp;
  FILE_LIST_ENTRY_T fileEntry;

  if(!dir || !pFilelist) {
    return -1; 
  }

  file_list_close(pFilelist);

  if(mediadb_prepend_dir(dir, FILE_LIST_IDXNAME, idxpath, sizeof(idxpath)) < 0) {
    return -1;
  }

  if((fp = fileops_Open(idxpath, O_RDONLY)) == FILEOPS_INVALID_FP) {
//fprintf(stdout, "did not open '%s'\n", idxpath);
    return -1;
  }

//fprintf(stdout, "opened '%s'\n", idxpath);
  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {
    if(parse_line(buf, &fileEntry) > 0 && 
       file_list_addcopy(pFilelist, &fileEntry) != NULL) { 
      pFilelist->count++; 
    }
  }

  fileops_Close(fp);

  return pFilelist->count;
}

void file_list_close(FILE_LIST_T *pFilelist) {
  FILE_LIST_ENTRY_T *pEntry, *pEntryTmp;

  if(pFilelist) {
    pEntry = pFilelist->pentries;
    while(pEntry) {
      pEntryTmp = pEntry->pnext;
      free(pEntry);
      pEntry = pEntryTmp;
    }
    pFilelist->pentries = NULL;
    pFilelist->pentriesLast = NULL;
    pFilelist->count = 0;
  }
}

int file_list_removeunmarked(FILE_LIST_T *pFilelist) {
  FILE_LIST_ENTRY_T *pEntry, *pEntryNext;
  FILE_LIST_ENTRY_T *pEntryPrev = NULL;
  int cnt = 0;

  if(pFilelist) {
    pEntry = pFilelist->pentries;
    while(pEntry) {
      pEntryNext = pEntry->pnext;

      if(pEntry->flag == 0) {
//fprintf(stderr, "removing entry '%s'\n", pEntry->name);
        if(pFilelist->count > 0) {
          pFilelist->count--;
        }
        if(pEntryPrev) {
          pEntryPrev->pnext = pEntryNext; 
        } else {
          pFilelist->pentries = pEntryNext;
        }
        free(pEntry);
        
        cnt++;   

      } else {
        pEntryPrev = pEntry;
      }

      pEntry = pEntryNext;
    }
  }

  pFilelist->pentriesLast = pEntryPrev;

  return cnt;
}

/*
int file_list_getdirlen(const char *path) {
  size_t idx;
  size_t sz;

  if(!path) {
    return 0;
  }
  sz = strlen(path);

  for(idx = sz; idx > 0; idx--) {
    switch(path[idx - 1]) {
      case '\\':
      case '/':
        return sz;
      default:
        break;
    }
  }
  return sz;
}

int file_list_read_file_entry(const char *path, FILE_LIST_ENTRY_T *pEntryArg) {
  FILE_LIST_ENTRY_T *pEntry;
  FILE_LIST_T fileList;
  char dir[VSX_MAX_PATH_LEN];
  int sz;
  int rc = -1;

  if(!path || !pEntryArg) {
    return -1;
  }

  memset(&fileList, 0, sizeof(fileList));

  strncpy(dir, path, sizeof(dir));
  sz = file_list_getdirlen(dir);
  dir[sz] = '\0';
  //strncpy(&dir[sz], FILE_LIST_IDXNAME, sizeof(dir) - sz);
  
  if(file_list_read(dir, &fileList) < 0) {
    return -1;
  }

  if((pEntry = file_list_find(&fileList, &dir[sz+1]))) {
    memcpy(pEntryArg, pEntry, sizeof(FILE_LIST_ENTRY_T));
    pEntryArg->pnext = NULL;
    rc = 0;
  }

  file_list_close(&fileList);

  return rc;
}
*/

