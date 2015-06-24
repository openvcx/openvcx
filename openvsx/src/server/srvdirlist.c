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


typedef int (*COMPARE_DIR_ENTRY) (const DIR_ENTRY_T *, const DIR_ENTRY_T *);

static int direntry_compare_bynameasc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r);
static int direntry_compare_bynamedesc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r);
static int direntry_compare_bysizeasc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r);
static int direntry_compare_bysizedesc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r);
static int direntry_compare_bytimeasc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r);
static int direntry_compare_bytimedesc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r);


static COMPARE_DIR_ENTRY direntry_getcompfunc(enum DIR_SORT sort) {
  COMPARE_DIR_ENTRY compareFunc = NULL;

  switch(sort) {
    case DIR_SORT_NAME_ASC:
//fprintf(stderr, "compare name asc\n");
      compareFunc = direntry_compare_bynameasc;
      break;
    case DIR_SORT_NAME_DESC:
      compareFunc = direntry_compare_bynamedesc;
      break;
    case DIR_SORT_SIZE_ASC:
      compareFunc = direntry_compare_bysizeasc;
      break;
    case DIR_SORT_SIZE_DESC:
      compareFunc = direntry_compare_bysizedesc;
      break;
    case DIR_SORT_TIME_ASC:
      compareFunc = direntry_compare_bytimeasc;
      break;
    case DIR_SORT_TIME_DESC:
      compareFunc = direntry_compare_bytimedesc;
      break;
    case DIR_SORT_NONE:
//fprintf(stderr, "compare none\n");
    default:
      compareFunc = NULL;
    break;
  }

  return compareFunc;
}

static int direntry_compare_bynameasc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r) {
  const char *lstr, *rstr;

  if(l->displayname && l->displayname[0] != '\0') {
    lstr = l->displayname;
  } else {
    lstr = l->d_name;
  }

  if(r->displayname && r->displayname[0] != '\0') {
    rstr = r->displayname;
  } else {
    rstr = r->d_name;
  }

  return strcasecmp(lstr, rstr);
}

static int direntry_compare_bynamedesc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r) {
  return direntry_compare_bynameasc(r, l);
}

static int direntry_compare_bysizeasc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r) {
  if(l->size < r->size) {
    return -1;
  } else if(l->size > r->size) {
    return 1;
  } else {
    return 0;
  }
}

static int direntry_compare_bysizedesc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r) {
  return direntry_compare_bysizeasc(r, l);
}

static int direntry_compare_bytimeasc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r) {
  if(l->tm < r->tm) {
    return -1;
  } else if(l->tm > r->tm) {
    return 1;
  } else {
    return 0;
  }
}

static int direntry_compare_bytimedesc(const DIR_ENTRY_T *l, const DIR_ENTRY_T *r) {
  return direntry_compare_bytimeasc(r, l);
}



static DIR_ENTRY_T *direntry_add(DIR_ENTRY_LIST_T **ppListArg, const DIR_ENTRY_T *pEntry) {
  DIR_ENTRY_LIST_T *pListNode = *ppListArg;
  DIR_ENTRY_LIST_T *pListNodePrev = NULL;

  while(1) {

    while(pListNode) {

      if(pListNode->pBuf == NULL) {
        pListNode->pBuf = (DIR_ENTRY_T *) avc_calloc(DIR_ENTRY_LIST_BUFNUM, sizeof(DIR_ENTRY_T));
        pListNode->numAlloc = DIR_ENTRY_LIST_BUFNUM;
      }

      if(pListNode->num < pListNode->numAlloc) {
        memcpy(&pListNode->pBuf[pListNode->num], pEntry, sizeof(DIR_ENTRY_T));
        pListNode->num++;
        return &pListNode->pBuf[pListNode->num - 1];
      }

      pListNodePrev = pListNode;
      pListNode = pListNode->pnext;
    }

    pListNode = (DIR_ENTRY_LIST_T *) avc_calloc(1, sizeof(DIR_ENTRY_LIST_T));
    if(pListNodePrev) {
      pListNodePrev->pnext = pListNode;
    } else {
      *ppListArg = pListNode;
    }

  }

  return NULL;
}

static DIR_ENTRY_T *direntry_addsorted(DIR_ENTRY_LIST_T **ppListArg, 
                                       const DIR_ENTRY_T *pEntry,
                                       COMPARE_DIR_ENTRY compareFunc) {
  DIR_ENTRY_T *pEntryInserted = NULL;
  DIR_ENTRY_T *pEntryTmpNext = NULL;
  DIR_ENTRY_T *pEntryTmpPrev = NULL;

//fprintf(stdout, "adding '%s'\n", pEntry->d_name);

  if(ppListArg && *ppListArg) {
    pEntryTmpNext = (*ppListArg)->pHead;
    while(pEntryTmpNext) {

      //fprintf(stdout, "comparing '%s' '%s' disp:'%s' '%s', tm:%u %u\n", pEntry->d_name, pEntryTmpNext->d_name, pEntry->displayname, pEntryTmpNext->displayname, pEntry->tm, pEntryTmpNext->tm);
      if(compareFunc && compareFunc(pEntry, pEntryTmpNext) <= 0) {
        //fprintf(stdout, "rc: break\n");
        break;
      }
//fprintf(stdout, "rc: %d\n", rc);
      pEntryTmpPrev = pEntryTmpNext;
      pEntryTmpNext = pEntryTmpNext->pnext;
    }
  }
  //fprintf(stdout, "no more... TmpNext: %s prev: %s\n", pEntryTmpNext ? pEntryTmpNext->d_name : "null", pEntryTmpPrev ? pEntryTmpPrev->d_name : "null");

  if((pEntryInserted = direntry_add(ppListArg, pEntry))) {

    if(pEntryTmpNext) {
      pEntryTmpNext->pprev = pEntryInserted;
      pEntryInserted->pnext = pEntryTmpNext;
    }

    if(pEntryTmpPrev) {
      pEntryTmpPrev->pnext = pEntryInserted;
      pEntryInserted->pprev = pEntryTmpPrev;
    }

    if(pEntryTmpPrev == NULL) {
      (*ppListArg)->pHead = pEntryInserted;
    }

  }

  return pEntryInserted;
}

void direntry_free(DIR_ENTRY_LIST_T *pList) {
  DIR_ENTRY_LIST_T *pListNext = NULL;

  while(pList) {
    pListNext = pList->pnext;
    if(pList->pBuf) {
      free(pList->pBuf);
    }
    free(pList);
    pList = pListNext;
  }
}

static int is_match_search(const char *filename, const char *dispname, 
                           const char *searchstr) {
  size_t szsearch = strlen(searchstr);
  size_t szhay = strlen(filename);

  if(searchstr[0] != '*' && szsearch <= 2) {

    if(TOUPPER(filename[0]) != TOUPPER(searchstr[0])) {
      return 0;
    }

    if(szsearch >= 2 && szhay >= 2) {

      if(TOUPPER(filename[1]) == TOUPPER(searchstr[1])) {
        return 1;
      }

    } else if(szsearch == 1) {
      return 1;
    }

  } else {
    while(*searchstr == '*') {
      searchstr++;
    }
    if(*searchstr == '\0' || strcasestr(filename, searchstr)) {
      return 1;
    }
  }

  return 0;
}

static int is_entry_ignored(ENTRY_IGNORE_T *pIgnoreList, const char *filename) {
  while(pIgnoreList) {
    if(!strcasecmp(pIgnoreList->filename, filename)) {
      return 1; 
    }
    pIgnoreList = pIgnoreList->pnext;
  }
  return 0;
}

static char *find_entry_description(ENTRY_META_DESCRIPTION_T *pDescList, 
                                    const char *filename) {
  while(pDescList) {
    if(!strcasecmp(pDescList->filename, filename)) {
      return pDescList->description; 
    }
    pDescList = pDescList->pnext;
  }
  return NULL;
}

DIR_ENTRY_LIST_T *direntry_getentries(const MEDIADB_DESCR_T *pMediaDb, 
                                      const char *dir, 
                                      const char *fidxdir, 
                                      const char *searchstr,
                                      int includedirs,
                                      unsigned int startidx,
                                      unsigned int max,
                                      enum DIR_SORT sort) {
  DIR *pdir;
  int rc = 0;
  struct dirent *direntry;
  char path[VSX_MAX_PATH_LEN];
  struct stat st;
  int addEntry;
  FILE_LIST_T fileList;
  FILE_LIST_ENTRY_T *pFileEntry = NULL;
  META_FILE_T metaFile;
  DIR_ENTRY_LIST_T *pEntryList = NULL;
  DIR_ENTRY_T entry;
  DIR_ENTRY_T *pEntry;
  const char *pdispname;
  unsigned int idx = 0;
  unsigned int cnt = 0;
  unsigned int cntInDir = 0;
  COMPARE_DIR_ENTRY compareFunc = direntry_getcompfunc(sort);

  VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - direntry_getentries dir: '%s', fidxdir: '%s', searchstr: '%s', "
                             "includedirs: %d, startidx: %d, max: %d"), 
                            dir, fidxdir, searchstr, includedirs, startidx, max));

  if(!(pdir = fileops_OpenDir(dir))) {
    return NULL;
  }

  memset(&fileList, 0, sizeof(fileList));
  if(fidxdir) { 
    file_list_read(fidxdir, &fileList);
  }

  //
  // Read the directory wide metafile to get a list of 'ignore' entries
  //
  memset(&metaFile, 0, sizeof(metaFile));
  mediadb_prepend_dir(dir, METAFILE_DEFAULT, path, sizeof(path));
  if(fileops_stat(path, &st) == 0) {
    metafile_open(path, &metaFile, 1, 1);
  }

  while((direntry = fileops_ReadDir(pdir))) {

    VSX_DEBUG_MGR( LOG(X_DEBUGV("MGR - direntry_getentries d_name: '%s', isdir: %d"), 
           direntry->d_name, (direntry->d_type & DT_DIR)) );

    if(is_entry_ignored(metaFile.pignoreList, direntry->d_name)) {
      continue;
    }

    if(!(pdispname = find_entry_description(metaFile.pDescriptionList,
                                     direntry->d_name))) {
      pdispname = direntry->d_name;
    }

    if(searchstr && !is_match_search(pdispname, NULL, searchstr)) {
      continue;
    }

    memset(&entry, 0, sizeof(entry));
    strncpy(entry.d_name, direntry->d_name, sizeof(entry.d_name) - 1); 
    if(pdispname != direntry->d_name) {
      strncpy(entry.displayname, pdispname, sizeof(entry.displayname) - 1); 
    }
    entry.d_type = direntry->d_type;
    addEntry = 0;

    if(direntry->d_type & DT_DIR) {

      if(includedirs && mediadb_isvalidDirName(pMediaDb, direntry->d_name)) {
        addEntry = 1;
      }

    } else if(mediadb_isvalidFileName(pMediaDb, direntry->d_name, 1, 1)) {

      mediadb_prepend_dir(dir, direntry->d_name, path, sizeof(path));
      if(fileops_stat(path, &st) == 0) {
        entry.size = st.st_size;
        //entry.tm = st.st_mtime;
        entry.tm = st.st_ctime;

        if(fidxdir && (pFileEntry = file_list_find(&fileList, direntry->d_name))) {
          entry.numTn = pFileEntry->numTn; 
          entry.duration = pFileEntry->duration;
        }
        addEntry = 1;
      }

    }

    if(addEntry) {
      VSX_DEBUG_MGR( LOG(X_DEBUGV("MGR - direntry_getentries add d_name: '%s', isdir: %d"), 
           direntry->d_name, (direntry->d_type & DT_DIR)) );

      if(compareFunc || (idx >= startidx && (max == 0 || cnt < max))) {

        if(cnt >= DIR_ENTRY_LIST_BUFNUM) {
          LOG(X_WARNING("Not showing more than %d entries in %s"), cnt, dir);
          break;
        } else if(!(pEntry = direntry_addsorted(&pEntryList, &entry, compareFunc))) {
          LOG(X_ERROR("Failed to add directory entry '%s' to list"), direntry->d_name);
          rc = -1;
          break;
        }
        cnt++;
      }

      idx++;
      cntInDir++;
    }

  }

  //
  // Since when a sort is requested we have to sort every entry in the directory.  Now we can move the head pointer
  // to the first desired entry
  //
  if(pEntryList && compareFunc && startidx > 0) {
    pEntry = pEntryList->pHead;
    for(idx = 0; idx < startidx; idx++) {
      pEntry = pEntry->pnext;
      cnt--;
    }
    pEntryList->pHead = pEntry;
    if(cnt > max) {
      cnt = max;
    }
    //fprintf(stderr, "moved phead to %s, cnt:%d, cntInDir:%d\n", pEntryList->pHead ? pEntryList->pHead->d_name : NULL, cnt, cntInDir);
  }
  

  fileops_CloseDir(pdir);
  if(fidxdir) {
    file_list_close(&fileList);
  }
  metafile_close(&metaFile);

  if(rc == 0 && !pEntryList) {
    // If the user requested an index out of bounds, return an empty list with
    // a valid cntTotalInDir
    pEntryList = (DIR_ENTRY_LIST_T *) avc_calloc(1, sizeof(DIR_ENTRY_LIST_T));
  }

  if(pEntryList) {
    pEntryList->cntTotal = cnt; 
    pEntryList->cntTotalInDir = cntInDir; 
  }

  //if(pEntryList) fprintf(stderr, "DIR '%s' num:%d numAlloc:%d pnext:0x%x TOTAL:%d/%d\n", dir, pEntryList->num, pEntryList->numAlloc, pEntryList->pnext, pEntryList->cntTotal, pEntryList->cntTotalInDir);
 
  return pEntryList;
}
