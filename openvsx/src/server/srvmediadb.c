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


#ifdef WIN32
#define DATE_CMD                   "date /t"
#else
#define DATE_CMD                   "date"
#endif // WIN32

#define FFMPEG_THUMB_NEW 1

int mediadb_checkfileext(const char *filepath, int includePl, int display) {

  static const char *extensions_media[] = { 
                                      ".mp4", ".m4a", ".m4v", "mov", "f6v",
                                      ".mov",
                                      ".3gp", ".3g2",
                                      ".flv", ".f4v", ".f4a", ".f4p", ".f4b",
                                      ".m2t", ".m2ts", ".ts",
                                      ".mkv", ".webm", 
                                      ".h264", 
                                      ".aac",
                                      ".sdp",
                                      NULL
                                    };
  static const char *extensions_playlist[] = { 
                                      ".m3u", ".m3u8", ".pl",
                                      NULL
                                     };
  //
  // .vsxtmp is appended to a file currently being recorded to prevent
  // inclusion into the file index database but to allowed to be displayed
  // in the client
  //
  static const char *extensions_display[] = { VSXTMP_EXT, NULL };
  const char **pext = NULL;
  size_t lenext;
  unsigned int idx;
  const unsigned int sizetot = 3; 
  size_t sz = strlen(filepath);

  for(idx = 0; idx < sizetot; idx++) {

    switch(idx) {
      case 0:
        pext = extensions_media;
        break;
      case 1:
        if(includePl) {
          pext = extensions_playlist;
          break;
        }
      case 2:
        if(display) {
          pext = extensions_display;
          break;
        }
      default:
        pext = NULL;
    }

    if(pext) {
      while(*pext) {
        lenext = strlen(*pext);
        if(sz > lenext) {
          if(!strncasecmp(&filepath[sz - lenext], *pext, lenext)) {
            return 1;
          } 
        }
        pext++;
      }
    }

  } // end of for

  return 0;
}

/*
const char *mediadb_getfilepathext(const char *mediaDir, const char *fullpath) {
  size_t sz;
  const char *pfilename = NULL;

  if(!mediaDir || !fullpath || (sz = strlen(mediaDir)) >= strlen(fullpath)) {
    return NULL;
  }
  pfilename = &fullpath[sz];
  while(*pfilename == '/' || *pfilename == '\\') {
    pfilename++;
  }
  return pfilename;
}
*/

int mediadb_getdirlen(const char *path) {
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
        return idx;
      default:
        break;
    }
  }
  //return sz;
  return 0;
}


int mediadb_prepend_dir(const char *mediaDir, const char *path,
                        char *out, unsigned int lenout) {
  size_t sz = 0; 
  size_t sz2;

  if(mediaDir && (sz = strlen(mediaDir)) >= lenout - 3) {
    return -1;
  }

  if(mediaDir == NULL || mediaDir[0] == '\0') {
    out[0] = '.';
    out[1] = '\0';
    sz = 1;
  } else {
    strncpy(out, mediaDir, lenout - 3);
  }

  if(out[sz - 1] != DIR_DELIMETER && path && path[0] != DIR_DELIMETER) {
    out[sz++] = DIR_DELIMETER;
    out[sz] = '\0';
  }

  if(path) {
    if((sz2 = strlen(path)) >= lenout - sz) {
      return -1;
    }
    strncpy(&out[sz], path, lenout - sz);
  }

  return 0;
}

int mediadb_prepend_dir2(const char *dir1, const char *dir2, const char *path,
                         char *out, unsigned int lenout) {
  int rc;
  char tmp[VSX_MAX_PATH_LEN];
  
  if((rc = mediadb_prepend_dir(dir1, dir2, tmp, sizeof(tmp)) >= 0)) {
     rc = mediadb_prepend_dir(tmp, path, out, lenout);
  }

  return rc;
}

int mediadb_fixdirdelims(char *path, char delim) {
  size_t len = strlen(path);
  size_t sz;
  int numreplaced = 0;
 
  for(sz = 0; sz < len; sz++) {
    if((path[sz] == '\\' || path[sz] == '/') && path[sz] != delim) {
      path[sz] = delim;
      numreplaced++;
    }
  }
  
  return numreplaced;
}

int mediadb_mkdirfull(const char *root, const char *path) {

  size_t idxprev;
  struct stat st;
  size_t sz = 0;
  size_t len;
  char buf[VSX_MAX_PATH_LEN];

//  fprintf(stdout, "mkdirfull '%s' '%s'\n", root, path);

  if(!path) {
    return -1;
  }

  if(root) {
    sz = strlen(root);
  }

  buf[sizeof(buf) - 1] = '\0';
  idxprev = sz;
  len = strlen(path);
 
  for(; sz < len && sz < sizeof(buf); sz++) {
    if(path[sz] == DIR_DELIMETER)  {

      memset(buf, 0, sizeof(buf));
      strncpy(buf, path, sz);

//fprintf(stdout, "stat '%s'\n", buf);
      if(strlen(buf) > 0 && fileops_stat(buf, &st) != 0) {
        LOG(X_DEBUG("Creating dir '%s'"), buf);
        if(fileops_MkDir(buf, S_IRWXU | S_IRWXG | S_IRWXO ) < 0) {
          LOG(X_ERROR("Unable to create dir '%s'"), buf);
          return -1;
        }
      }

      while(path[sz] == DIR_DELIMETER) {
        sz++;
      }
      idxprev = sz;
//fprintf(stdout, "after'%s'\n", &path[sz]);
    }
  }
  

  if(sz > idxprev && fileops_stat(path, &st) != 0) {
    LOG(X_DEBUG("Creating dir '%s'"), path);
    if(fileops_MkDir(path, S_IRWXU | S_IRWXG | S_IRWXO ) < 0) {
      LOG(X_ERROR("Unable to create dir '%s'"), path);
      return -1;
    }
  }

  return 0;
}

int mediadb_parseprefixes(const char *prefixstr, const char *prefixOutArr[], 
                          unsigned int maxPrefix) {
  unsigned int prefixIdx = 0;
  const char *p, *p2;
  int inside = 0;

  //TODO: this should use strutil_parse_csv

  if(!prefixstr) {
    return prefixIdx;
  }

  p = p2 = prefixstr;

  while(*p2 != '\0') {


    if(!inside && *p2 == '"') {
      p = ++p2;
      inside = 1;
    }

    if(inside && *p2 == '"') {
      if(prefixIdx >= maxPrefix) {
        LOG(X_WARNING("Max number %d of prefixes reached"), maxPrefix);
        break;
      }
      prefixOutArr[prefixIdx++] = (char *) p;
      *((char *) p2) = '\0';
      inside = 0;
    }
    p2++;
       
  }

  return prefixIdx;
}

static int create_viddescrstr(const MEDIA_DESCRIPTION_T *pMediaDescr,
                              char *buf, unsigned int len) {
  int sz = 0;
  int rc = 0;
  unsigned int fps;

  if(!pMediaDescr->haveVid) {
    return 0;
  }

  if(pMediaDescr->vid.generic.resH > 0) {
    if((rc = snprintf(&buf[sz], len - sz, "%ux%u ", pMediaDescr->vid.generic.resH,
                       pMediaDescr->vid.generic.resV)) > 0) {
      sz += rc;
    }
  }
  if(pMediaDescr->vid.generic.fps > 0) {
    fps = (unsigned int) (pMediaDescr->vid.generic.fps * 1000);
    if((fps / 1000) * 1000 == fps) {
      rc = snprintf(&buf[sz], len - sz, "%u fps ", fps / 1000);
    } else {
      rc = snprintf(&buf[sz], len - sz, "%.2f fps ", pMediaDescr->vid.generic.fps);
    }
    if(rc > 0) {
      sz += rc;
    }
  }
  if((rc = snprintf(&buf[sz], len - sz, "%s ", pMediaDescr->vid.codec)) > 0) {
    sz += rc;
  }
  if(sz == 0) {
    // print non-emtpy line to designate presence of video to the parser
    sz = snprintf(buf, len, " ");
  }

  return sz;
}

static int create_auddescrstr(const MEDIA_DESCRIPTION_T *pMediaDescr,
                              char *buf, unsigned int len) {
  int sz = 0;
  int rc = 0;

  if(!pMediaDescr->haveAud) {
    return 0;
  }

  if(pMediaDescr->aud.generic.clockHz != 0) {
    if((pMediaDescr->aud.generic.clockHz/1000) * 1000 == pMediaDescr->aud.generic.clockHz) {
      rc = snprintf(&buf[sz], len - sz, "%u KHz ", pMediaDescr->aud.generic.clockHz/1000);
    } else if((pMediaDescr->aud.generic.clockHz/100)*100 == pMediaDescr->aud.generic.clockHz){
      rc = snprintf(&buf[sz], len - sz, "%.1f KHz ", (float)pMediaDescr->aud.generic.clockHz/1000.0f);
    } else if((pMediaDescr->aud.generic.clockHz/10)*10 == pMediaDescr->aud.generic.clockHz){
      rc = snprintf(&buf[sz], len - sz, "%.2f KHz ", (float)pMediaDescr->aud.generic.clockHz/1000.0f);
    } else {
      rc = snprintf(&buf[sz], len - sz, "%.3f KHz ", pMediaDescr->aud.generic.clockHz/1000.0f);
    }
    if(rc > 0) {
      sz += rc;
    }
  }
  if(pMediaDescr->aud.generic.channels == 1) {
    if((rc = snprintf(&buf[sz], len - sz, "mono ")) > 0) {
      sz += rc;
    }
  } else if(pMediaDescr->aud.generic.channels == 2) {
    if((rc = snprintf(&buf[sz], len - sz, "stereo ")) > 0) {
      sz += rc;
    }
  } else if(pMediaDescr->aud.generic.channels > 2) {
    if((rc = snprintf(&buf[sz], len - sz, "%d channels ", pMediaDescr->aud.generic.channels)) > 0) {
      sz += rc;
    }
  }
  if((rc = snprintf(&buf[sz], len - sz, "%s ", pMediaDescr->aud.codec)) > 0) {
    sz += rc;
  }
  if(sz == 0) {
    // print non-emtpy line to designate presence of audio to the parser
    sz = snprintf(buf, len, " ");
  }

  return sz;
}

static int update_fileentry(FILE_LIST_T *pFileList, const char *mediaName, 
                            const char *mediaPath, FILE_LIST_ENTRY_T **ppEntry) {
  MEDIA_DESCRIPTION_T mediaDescr;
  FILE_STREAM_T fileStream;
  FILE_LIST_ENTRY_T *pFileEntryCur = NULL;
  FILE_LIST_ENTRY_T *pFileEntryNew = NULL;
  FILE_LIST_ENTRY_T fileEntry;
  char buf[VSX_MAX_PATH_LEN];
  struct stat st;
  struct stat st2;
  int havestat = 0;
  size_t szt;
  int fileChanged = 1;

  memset(&fileEntry, 0, sizeof(fileEntry));
      
//fprintf(stdout, "looking for '%s'\n", mediaName);
  if((pFileEntryCur = file_list_find(pFileList, mediaName))) {

    //fprintf(stdout, "found entry '%s' numTn:%d\n", pFileEntryCur->name, pFileEntryCur->numTn);

    // mark the entry as current to avoid deletion
    pFileEntryCur->flag = 1;
        
    if((havestat = !fileops_stat(mediaPath, &st)) == 0) {
      LOG(X_ERROR("Unable to stat '%s'"), mediaPath);
    } else if(st.st_size == pFileEntryCur->size &&
              st.st_ctime == pFileEntryCur->tm) {

      fileChanged = 0;

      //
      // Special case to probe for recent creation .seek file
      // since the last time this entry was written
      //
      if(pFileEntryCur->duration == 0) {
        buf[sizeof(buf) - 1] = '\0';
        strncpy(buf, mediaPath, sizeof(buf) - 1);
        if((szt = strlen(buf))) {
          strncpy(&buf[szt], MP2TS_FILE_SEEKIDX_NAME, sizeof(buf) - szt - 1);
        }
        if(fileops_stat(buf, &st2) == 0) {
          fileChanged = 1;        
        }
      }

    }
    pFileEntryNew = pFileEntryCur;

  } else {

    // Check for invalid chars - ',' is used as a delimeter in .avcfidx
    if(strchr(mediaName, ',') || mediaName[0] == '#' || mediaName[0] == ' ' ||
              mediaName[0] == '=') {
      LOG(X_WARNING("Illegal filename '%s' not added to database"), mediaPath);
      pFileEntryNew = NULL;
      fileChanged = 0;
    } else {
      LOG(X_DEBUG("Could not find database entry '%s'"), mediaPath);
      pFileEntryNew = &fileEntry;
    }
  }

  if(fileChanged) {

    if(OpenMediaReadOnly(&fileStream, mediaPath) == 0) {

      memset(&mediaDescr, 0, sizeof(mediaDescr));

      if(filetype_getdescr(&fileStream, &mediaDescr, 1) == 0) {

         pFileEntryNew->flag = 1;
         pFileEntryNew->duration = mediaDescr.durationSec; 
         pFileEntryNew->size = mediaDescr.totSz;

         if(!havestat && (havestat = !fileops_stat(mediaPath, &st)) == 0) {
           LOG(X_ERROR("Unable to stat new entry '%s'"), mediaPath);
         }
         if(havestat) {
           pFileEntryNew->tm = st.st_ctime;
//fprintf(stderr, "-----------set tm:%ld %s\n", pFileEntryNew->tm, mediaPath);
         }

         if(mediaDescr.haveVid) {
           create_viddescrstr(&mediaDescr, pFileEntryNew->vstr, sizeof(pFileEntryNew->vstr));
         }
         if(mediaDescr.haveAud) {
           create_auddescrstr(&mediaDescr, pFileEntryNew->astr, sizeof(pFileEntryNew->astr));
           //fprintf(stdout, "aud descr:'%s'\n", pFileEntryNew->astr);
         }

       } else {
         pFileEntryNew->flag = 1;
         pFileEntryNew->size = fileStream.size;
       }

       CloseMediaFile(&fileStream);
    } // end of OpenMediaReadOnly

    if(pFileEntryCur == NULL) {
      pFileEntryNew->name[sizeof(pFileEntryNew->name) - 1] = '\0';
      strncpy(pFileEntryNew->name, mediaName, sizeof(pFileEntryNew->name) - 1);
      pFileEntryCur = file_list_addcopy(pFileList, pFileEntryNew);
    }
  } // end of if(fileChanged)

  if(ppEntry) {
    *ppEntry = pFileEntryCur;
  }

  return fileChanged; 
}

#define SRVMEDIA_MAX_LEVELS          10
#define SRVMEDIA_TN_MAX               8
#define SRVMEDIA_TN_INTERVAL_SEC      5

#define SM_THUMB_WIDTH      120
#define SM_THUMB_HEIGHT     66 
#define LG_THUMB_WIDTH      260 
#define LG_THUMB_HEIGHT     146


static int createTn(const MEDIADB_DESCR_T *pMediaDb,
                    FILE_LIST_ENTRY_T *pFileListEntry, 
                    const char *pathMedia, const char *pathTn,
                    int numTn, int maxTnInIter) {
  int rc = 0;
  int highestTn = 0;
  struct stat st;
  unsigned int idx;
  char buf[VSX_MAX_PATH_LEN];
  char cmd[VSX_MAX_PATH_LEN];
  char cmdnice[64];
  char tmpfile[VSX_MAX_PATH_LEN];
  int numTnCreated = 0;
  char avcthumbsize[32];
  unsigned int secOffset;
  const char *avcthumb = pMediaDb->avcthumb;
  const char *avcthumblog = pMediaDb->avcthumblog;
  const char *avcthumb_file = avcthumb;
  const char *avcthumblog2 = avcthumblog;
#ifdef WIN32
  char curdir[VSX_MAX_PATH_LEN];
  char avcthumblogpath[VSX_MAX_PATH_LEN];
  char avcthumblogpath2[VSX_MAX_PATH_LEN];
  char avcthumblogprefix[64];
  size_t sz;
#endif // WIN32
#if defined(FFMPEG_THUMB_NEW)
  const char thumbsizedelim = ':';
#else // (FFMPEG_THUMB_NEW)
  const char thumbsizedelim = 'x';
#endif //  (FFMPEG_THUMB_NEW)

  tmpfile[0] = '\0';
  cmdnice[0] = '\0';

  //fprintf(stdout, "createTn '%s' numTn:%d, maxTnIter:%d, TN_MAX:%d, g_proc_exit:%d (0x%x)\n", pathTn, numTn, maxTnInIter, SRVMEDIA_TN_MAX, g_proc_exit, &g_proc_exit);

  for(idx = 0; idx < SRVMEDIA_TN_MAX; idx++) {

    if(idx > 0) {
      secOffset = (idx - 1) * SRVMEDIA_TN_INTERVAL_SEC;
    } else {
      secOffset = 4;
    }


    if(pFileListEntry->duration == 0) {

      snprintf(tmpfile, sizeof(tmpfile), "%s%s"VSXTMP_EXT, pathTn, pMediaDb->tn_suffix);
      if(fileops_stat(tmpfile, &st) != 0) {
 
        if(numTn == 0 && idx == 0) {
//fprintf(stdout, "creating tmp file '%s'\n", tmpfile);
          //
          // Create tmpfile which should only exist during 
          // creation of thumbnails for source of unknown duration
          // Since the "tn=" parameter in the index file denotes
          // the current number of thumbnails available
          //
          if(fileops_touchfile(tmpfile) != 0) {
            LOG(X_ERROR("Failed to create %s"), tmpfile);
          }
        } else if((int) idx >= numTn) {
//fprintf(stdout, "breaking out, have all the tns %d > %d\n", idx, numTn);
          break;
        }

      } else if(idx == SRVMEDIA_TN_MAX - 1) {
//fprintf(stdout, "removing tmp file '%s'\n", tmpfile);
        fileops_DeleteFile(tmpfile);
      }

    } else if(pFileListEntry->duration > 0 && secOffset >= pFileListEntry->duration - 1) {
      break;
    }

    if(idx == 0) {
      snprintf(avcthumbsize, sizeof(avcthumbsize), "%d%c%d", 
               pMediaDb->smTnWidth, thumbsizedelim, pMediaDb->smTnHeight);
      snprintf(buf, sizeof(buf), "%s%s.jpg", pathTn, pMediaDb->tn_suffix);    
    } else {
      snprintf(avcthumbsize, sizeof(avcthumbsize), "%d%c%d", 
               pMediaDb->lgTnWidth, thumbsizedelim, pMediaDb->lgTnHeight);
      snprintf(buf, sizeof(buf), "%s%s%u.jpg", pathTn, pMediaDb->tn_suffix, idx - 1);    
    }

    if(fileops_stat(buf, &st) < 0  || st.st_size == 0) {

#ifdef WIN32

      //
      // cd into directory of avcthumb script
      //
      avcthumblogprefix[0] = '\0';
      curdir[0] = '\0';
      fileops_getcwd(curdir, sizeof(curdir));
      if((sz = mediadb_getdirlen(avcthumb)) > 0) {
        if(sz >= sizeof(tmpfile)) {
          sz = sizeof(tmpfile) - 1;
        }
        tmpfile[sizeof(tmpfile) - 1] = '\0';
        strncpy(tmpfile, avcthumb, sizeof(tmpfile));
        if(sz > 0) {
          tmpfile[sz] = '\0';
          fileops_setcwd(tmpfile);
          avcthumb_file = &avcthumb[sz];
          snprintf(avcthumblogprefix, sizeof(avcthumblogprefix), "..\\");
        }
      }
      if(avcthumblog) {
        snprintf(avcthumblogpath, sizeof(avcthumblogpath), "%s%s", 
                 avcthumblogprefix, avcthumblog);
        snprintf(avcthumblogpath2, sizeof(avcthumblogpath2), "%s%s.err", 
                 avcthumblogprefix, avcthumblog);
        avcthumblog2 = avcthumblogpath2;
        avcthumblog = avcthumblogpath;
      }

#else

      snprintf(cmdnice, sizeof(cmdnice), "nice -n19 ");

#endif // WIN32
      

#if defined(FFMPEG_THUMB_NEW)
      //
      //TODO: create multiple thumbs on one run... will speed thnigs up alot!
      // '-ss 4 -vf "thumbnail,scale=164:90,fps=1/4" -frames:v 8'
      //
      snprintf(cmd, sizeof(cmd), "%s%s -v warning -i \"%s\" -an -vf \"thumbnail,scale=%s\" "
               "-frames:v 1 -f image2 -vcodec mjpeg -ss %u \"%s\" %s%s %s%s", 
               cmdnice,
               avcthumb_file, pathMedia, avcthumbsize, secOffset, buf, 
               avcthumblog ? ">>" : "", avcthumblog ? avcthumblog : "",
               avcthumblog2 ? "2>>" : "", avcthumblog2 ? avcthumblog2 : "");

#else // (FFMPEG_THUMB_NEW)
      snprintf(cmd, sizeof(cmd), "%s%s -i \"%s\" -an -s \"%s\" "
               "-vframes 1 -f image2 -vcodec mjpeg -ss %u \"%s\" %s%s %s%s", 
               cmdnice,
               avcthumb_file, pathMedia, avcthumbsize, secOffset, buf, 
               avcthumblog ? ">>" : "", avcthumblog ? avcthumblog : "",
               avcthumblog2 ? "2>>" : "", avcthumblog2 ? avcthumblog2 : "");
#endif // (FFMPEG_THUMB_NEW)
    
//if(numcmd++ > 2) {
//  return 0;
//}
      LOG(X_DEBUG("Creating thumbnail '%s'"), cmd);
      //TODO: seems that we're not getting SIGINT in the parent here... need a way to spawn (async?) & wait/poll
      rc = system(cmd);

#ifdef WIN32
      if(curdir[0] != '\0') {
        fileops_setcwd(curdir);
      }
#endif // WIN3

      //LOG(X_DEBUG("DOING STAT ON '%s'"), buf);
      if(fileops_stat(buf, &st) == 0  && st.st_size > 0) {
        rc = 0;
        highestTn = idx + 1;
        if(maxTnInIter > 0 && ++numTnCreated >= maxTnInIter) {
          break;
        }
      } else {
        rc = -1;
        LOG(X_WARNING("Failed to generate thumbnail '%s'"), buf);

        if(pFileListEntry->duration == 0) {
          snprintf(tmpfile, sizeof(tmpfile), "%s%s"VSXTMP_EXT, pathTn, pMediaDb->tn_suffix);    

          if(fileops_stat(tmpfile, &st) == 0) {
            fileops_DeleteFile(tmpfile);
          }
        } else {
          snprintf(tmpfile, sizeof(tmpfile), "%s%s."NOTN_EXT, pathTn, pMediaDb->tn_suffix);    
          if(fileops_touchfile(tmpfile) != 0) {
            LOG(X_ERROR("Failed to create %s"), tmpfile);
          }
        }

        if(idx == 0) {
          return -1;
        }
        break;
      }

    } else {
      highestTn = idx + 1;
    }

  }
  
  return highestTn;
}

static int clearTn(const char *pathMedia, const char *pathDb, 
                   const FILE_LIST_T *pFileList) {

  DIR *pdir;
  struct dirent *direntry;
  char buf[VSX_MAX_PATH_LEN];
  char buf2[VSX_MAX_PATH_LEN];
  size_t sz,sz2;

  if(!(pdir = fileops_OpenDir(pathDb))) {
    return -1;
  }

  buf[sizeof(buf) - 1] = '\0';

  while((direntry = fileops_ReadDir(pdir))) {
    if(!(direntry->d_type & DT_DIR)) {

      // "_tn[idx].jpg"
      if((sz = strlen(direntry->d_name)) > 9 && 
         (!strncasecmp(&direntry->d_name[sz - 4], ".jpg", 4) ||
       !strncasecmp(&direntry->d_name[sz - VSXTMP_EXT_LEN], VSXTMP_EXT, VSXTMP_EXT_LEN) ||
         !strncasecmp(&direntry->d_name[sz - NOTN_EXT_LEN], NOTN_EXT, NOTN_EXT_LEN))) {

        strncpy(buf, direntry->d_name, sizeof(buf) - 1);
        for(sz2 = sz - 4; sz2 > sz - 12; sz2--) { 
          if(buf[sz2] == '_') {
            break;
          }
        }
        buf[sz2] = '\0';
//fprintf(stdout, "check '%s' '%s'\n", buf, direntry->d_name);

        if(file_list_find(pFileList, buf) == NULL) {
          mediadb_prepend_dir(pathDb, direntry->d_name, buf2, sizeof(buf));  
          LOG(X_DEBUG("Removing thumbnail '%s'\n"), buf2);
          if(fileops_DeleteFile(buf2) != 0) {
            LOG(X_ERROR("Failed to delete thumbnail %s"), buf2);
          }
        }

      }
    }
  }

  fileops_CloseDir(pdir);

  return 0;
}

int mediadb_isvalidDirName(const MEDIADB_DESCR_T *pMediaDb, const char *d_name) {
  int idx;

  if(d_name[0] == '.' && (d_name[1] == '\0' || d_name[1] == '.')) {
    return 0;
  } else if(!strcasecmp(d_name, VSX_DB_PATH)) {
    return 0;
  }
  //LOG(X_DEBUG("STR STR '%s' '%s'"), pMediaDb->dbDir, d_name);
  //if(strstr(pMediaDb->dbDir, d_name)) {
  //  return 0;
  //}

  for(idx = 0; idx < MEDIADB_PREFIXES_MAX; idx++) {
    if(pMediaDb->ignoredirprefixes[idx] == NULL) {
      break; 
    } else if(!strncasecmp(d_name, pMediaDb->ignoredirprefixes[idx], 
              strlen(pMediaDb->ignoredirprefixes[idx]))) {
      return 0;
    }
  }

  return 1;
}

int mediadb_isvalidFileName(const MEDIADB_DESCR_T *pMediaDb, const char *d_name,
                            int includePl, int display) {
  int idx;
  
  if(mediadb_checkfileext(d_name, includePl, display) == 0) {
    return 0;
  }

  for(idx = 0; idx < MEDIADB_PREFIXES_MAX; idx++) {
    if(pMediaDb->ignorefileprefixes[idx] == NULL) {
      break; 
    } else if(!strncasecmp(d_name, pMediaDb->ignorefileprefixes[idx], 
              strlen(pMediaDb->ignorefileprefixes[idx]))) {
      return 0;
    }
  }

  return 1;
}


static int iterate_subdirs(const MEDIADB_DESCR_T *pMediaDb, const char *pathDb, 
                           const char *pathMedia, int level, unsigned int iterIdx) {
  DIR *pdir;
  FILE_LIST_T fileList;
  FILE_LIST_ENTRY_T *pFileListEntry;
  struct dirent *direntry;
  char bufMedia[VSX_MAX_PATH_LEN];
  char bufDb[VSX_MAX_PATH_LEN];
  char tmpfile[VSX_MAX_PATH_LEN];
  int rc = 0;
  int rcsub = 0;
  int fileListChanged = 0;
  int numTnCreated = 0;
  int highestTn;
  struct stat st;

//fprintf(stdout, "%d dir:'%s' '%s'\n", level, pathDb, pathMedia);

  //
  // TODO: escape invalid chars in pathnames ' , & # $ '
  // 

  if(level >= SRVMEDIA_MAX_LEVELS) {
    LOG(X_WARNING("Path '%s' exceeded max number of subdirs under %s"), 
        pathMedia, pMediaDb->mediaDir);
    return -1;
  }

  if(!(pdir = fileops_OpenDir(pathMedia))) {
    return -1;
  }

  memset(&fileList, 0, sizeof(fileList));
  file_list_read(pathDb, &fileList);
  strncpy(fileList.pathmedia, pathMedia, sizeof(fileList.pathmedia) - 1);


  while((direntry = fileops_ReadDir(pdir)) && g_proc_exit == 0) {
//fprintf(stdout, "- %d %s\n", level, direntry->d_name);
    if(direntry->d_type & DT_DIR) {

      if(mediadb_isvalidDirName(pMediaDb, direntry->d_name)) {

        if(mediadb_prepend_dir(pathDb, direntry->d_name, bufDb, sizeof(bufDb)) < 0) {
          LOG(X_WARNING("Unable to concatenate subdir %s %s"), bufDb, direntry->d_name);
        } else if(mediadb_prepend_dir(pathMedia, direntry->d_name, bufMedia, sizeof(bufMedia)) < 0) {
          LOG(X_WARNING("Unable to concatenate subdir %s %s"), bufMedia, direntry->d_name);
        } else {

          if(numTnCreated > 1) {
            file_list_write(pathDb, &fileList);
          }

          if((rcsub = iterate_subdirs(pMediaDb, bufDb, bufMedia, 
                                      level+1, iterIdx)) < 0) {
            rc = rcsub;
          }
          if(g_proc_exit != 0) {
            break;
          }

        }

      } else if(direntry->d_name[0] != '.') {
        //fprintf(stderr, "skipping directory '%s'\n", direntry->d_name);
      }

    } else if(mediadb_isvalidFileName(pMediaDb, direntry->d_name, 1, 0)) {

//fprintf(stdout, "%d media file %s '%s'\n", level, pathMedia, direntry->d_name);

      if(fileops_stat(pathDb, &st) != 0 && mediadb_mkdirfull(pMediaDb->dbDir, pathDb) < 0) {
        continue;
      } 

      if(mediadb_prepend_dir(pathDb, direntry->d_name, bufDb, sizeof(bufDb)) < 0) {
        LOG(X_ERROR("Unable to concatenate subdir %s %s"), bufDb, direntry->d_name);
        continue;
      } else if(mediadb_prepend_dir(pathMedia, direntry->d_name, bufMedia, sizeof(bufMedia)) < 0) {
        LOG(X_ERROR("Unable to concatenate subdir %s %s"), bufMedia, direntry->d_name);
        continue;
      }

      if(update_fileentry(&fileList, direntry->d_name, bufMedia, &pFileListEntry) > 0) {

        LOG(X_DEBUG("'%s' changed ('%s')"), direntry->d_name, pFileListEntry->name);

        fileListChanged = 1;
        if(pFileListEntry->numTn < 0) {
          pFileListEntry->numTn = 0;
        }

      }

      if(pMediaDb->avcthumb && pFileListEntry && pFileListEntry->vstr[0] != '\0' &&
         pFileListEntry->numTn != -1) {

        //
        // On very first iteration, check all thumbnails
        //
        snprintf(tmpfile, sizeof(tmpfile), "%s%s."NOTN_EXT, bufDb, pMediaDb->tn_suffix);

        if(fileops_stat(tmpfile, &st) == 0 && iterIdx > 0) {
          //  
          // Ignore thumbnail creation if the .notn file has been
          // previously created on an error condition
          //

        } else 
        //
        // Update thumbnail info
        //
        if((highestTn = createTn(pMediaDb, pFileListEntry, bufMedia, bufDb, 
            pFileListEntry->numTn, 1)) > pFileListEntry->numTn || highestTn == -1) {

          pFileListEntry->numTn = highestTn;
          fileListChanged = 1;

          // Periodically update the db file
          if(numTnCreated > 10) {
            file_list_write(pathDb, &fileList);
            numTnCreated = 0;
          }

        }

      }

    } else {
//fprintf(stdout, "skipping %s '%s'\n", pathMedia, direntry->d_name);
    }

  }

  fileops_CloseDir(pdir);

  if(g_proc_exit == 0) {

    if(file_list_removeunmarked(&fileList) || fileListChanged) {

       LOG(X_DEBUG("Writing file index in %s"), pathDb);
//fprintf(stdout, "level:%u\n", level);
      file_list_write(pathDb, &fileList);
    }

    //
    //Remove thumbnails which are not included in the fileList
    //
    clearTn(pathMedia, pathDb, &fileList);
  }

  file_list_close(&fileList);

  return rc;
}

void mediadb_proc(void *parg) {
  MEDIADB_DESCR_T *pMediaDb = (MEDIADB_DESCR_T *) parg;
  struct stat st;
  char buf[VSX_MAX_PATH_LEN];
  const char *avcthumb = NULL;
  size_t sz;
  int rc;
  unsigned int iterIdx = 0;

  if(!pMediaDb->mediaDir) {
    return;
  }

  memset(buf, 0, sizeof(buf));
  sz = mediadb_getdirlen(pMediaDb->dbDir);
  strncpy(buf,  pMediaDb->dbDir, sizeof(buf) - 1);
  buf[sz] = '\0';

  LOG(X_DEBUG("Database dir: '%s'"), pMediaDb->dbDir);

  if(fileops_stat(buf, &st) != 0) {
    //
    // Create database root dir
    //
    LOG(X_DEBUG("Creating dir '%s'"), buf);
    if(fileops_MkDir(buf, S_IRWXU | S_IRWXG | S_IRWXO ) < 0) {
      LOG(X_CRITICAL("Unable to create database dir '%s'"), buf);
      return;
    }
  }

  if(fileops_stat(pMediaDb->dbDir, &st) != 0) {
    //
    // Create database subdir specific for mediaDir
    //
    LOG(X_DEBUG("Creating dir '%s'"), pMediaDb->dbDir);
    if(fileops_MkDir(pMediaDb->dbDir, S_IRWXU | S_IRWXG | S_IRWXO ) < 0) {
      LOG(X_CRITICAL("Unable to create database dir '%s'"), pMediaDb->dbDir);
      return;
    }
  }

  //
  // Check thumbnail creation script
  //
  if(pMediaDb->avcthumb) {

    if(fileops_stat(pMediaDb->avcthumb, &st) == 0) {

      //
      // Truncate the avcthumblog 
      //
      if(pMediaDb->avcthumblog) {
        snprintf(buf, sizeof(buf), DATE_CMD" > %s", pMediaDb->avcthumblog);
        //LOG(X_DEBUG("Executing system command '%s'"), buf);
        rc = system(buf);
        if(fileops_stat(pMediaDb->avcthumblog, &st) != 0) {
          pMediaDb->avcthumblog = NULL;
        }

      }

      if(pMediaDb->lgTnWidth == 0 || pMediaDb->lgTnWidth == 0) {
        pMediaDb->lgTnWidth = LG_THUMB_WIDTH;
        pMediaDb->lgTnHeight = LG_THUMB_HEIGHT;
      }
      if(pMediaDb->smTnWidth == 0 || pMediaDb->smTnHeight == 0) {
        pMediaDb->smTnWidth = SM_THUMB_WIDTH;
        pMediaDb->smTnHeight = SM_THUMB_HEIGHT;
      }

      LOG(X_DEBUG("Using %s to generate thumbnails %dx%d %dx%d"), 
                  pMediaDb->avcthumb,
                  pMediaDb->smTnWidth, pMediaDb->smTnHeight,
                  pMediaDb->lgTnWidth, pMediaDb->lgTnHeight);

    } else {

      LOG(X_WARNING("Unable to find %s  Video thumbnails disabled."), pMediaDb->avcthumb);
      pMediaDb->avcthumb = NULL;
    } 

  } else {
    LOG(X_WARNING("Not using video thumbnails.  Please set '%s=' in the configuration file."),
        SRV_CONF_KEY_AVCTHUMB);
  }

  //
  // Set avchumb to null and do not create thumbnails on first 
  // iteration to allow all avcfidx files to be created quickly
  //
  avcthumb = pMediaDb->avcthumb; 
  pMediaDb->avcthumb = NULL; 
  if(g_proc_exit == 0) {
    iterate_subdirs(pMediaDb, pMediaDb->dbDir, pMediaDb->mediaDir, 1, 0);
    pMediaDb->avcthumb = avcthumb; 
    usleep(2000000);
  }

  while(g_proc_exit == 0) {
    iterate_subdirs(pMediaDb, pMediaDb->dbDir, pMediaDb->mediaDir, 1, iterIdx++);
    usleep(10000000);
  }

}

