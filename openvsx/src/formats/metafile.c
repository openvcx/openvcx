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
#include "formats/metafile.h"


#define METAFILE_FILE_CHK_SZMAX       128

#define METAFILE_KEY_FILE             "file"
#define METAFILE_KEY_HTTPLINK         "httplink"
#define METAFILE_KEY_DESCRIPTION      "description"
#define METAFILE_KEY_DEVICE           "device"
#define METAFILE_KEY_DIGESTAUTH       "digestauth"
#define METAFILE_KEY_ID               "id"
#define METAFILE_KEY_IGNORE           "ignore"
#define METAFILE_KEY_INPUT            "input"
#define METAFILE_KEY_METHODS          "methods"
#define METAFILE_KEY_PROFILE          "profile"
#define METAFILE_KEY_SHARED           "shared"
#define METAFILE_KEY_HTTP_PROXY       "httpproxy"
#define METAFILE_KEY_RTMP_PROXY       "rtmpproxy"
#define METAFILE_KEY_RTSP_PROXY       "rtspproxy"
#define METAFILE_KEY_XCODEARGS         SRV_CONF_KEY_XCODEARGS

#define PARSE_FLAG_HAVE_FILENAME          0x0001
#define PARSE_FLAG_HAVE_HTTPLINK          0x0002
#define PARSE_FLAG_HAVE_DESCRIPTION       0x0004
#define PARSE_FLAG_HAVE_DEVNAME           0x0008
#define PARSE_FLAG_HAVE_DIGESTAUTH        0x0010
#define PARSE_FLAG_HAVE_ID                0x0020
#define PARSE_FLAG_HAVE_IGNORE            0x0040
#define PARSE_FLAG_HAVE_INPUT             0x0080
#define PARSE_FLAG_HAVE_METHODS           0x0100
#define PARSE_FLAG_HAVE_PROFILE           0x0200
#define PARSE_FLAG_HAVE_SHARED            0x0400
#define PARSE_FLAG_HAVE_HTTP_PROXY        0x0800
#define PARSE_FLAG_HAVE_RTMP_PROXY        0x1000
#define PARSE_FLAG_HAVE_RTSP_PROXY        0x2000
#define PARSE_FLAG_HAVE_XCODEARGS         0x4000

#define MATCH_HAVE_FILENAME   0x01
#define MATCH_HAVE_XCODE      0x02
#define MATCH_HAVE_METHODS    0x04
#define MATCH_HAVE_DIGESTAUTH 0x08
#define MATCH_BY_PROFILE      0x10
#define MATCH_BY_DEVICE       0x20


typedef struct PARSE_ENTRY_DATA {
  char                filename[VSX_MAX_PATH_LEN];
  char                link[VSX_MAX_PATH_LEN];
  char                devname[STREAM_DEV_NAME_MAX];
  char                input[256];
  char                rtmpproxy[META_FILE_PROXY_STR_LEN];
  char                rtspproxy[META_FILE_PROXY_STR_LEN];
  char                httpproxy[META_FILE_PROXY_STR_LEN];
  char                xcodestr[META_FILE_XCODESTR_MAX];
  int                 methodBits;
  char                userpass[AUTH_ELEM_MAXLEN * 2 + 1];
  char                id[META_FILE_IDSTR_MAX];
  char                profile[META_FILE_PROFILESTR_MAX];
  int                 shared;
  int                 flags;
  char                ignore[FILE_LIST_ENTRY_NAME_LEN];
  char                description[META_FILE_DESCR_LEN];

  const char         *path;
  unsigned int        linenum;
} PARSE_ENTRY_DATA_T;

int metafile_isvalidFile(const char *path) {
  FILE_STREAM_T fs;
  int rc = 0;
  unsigned char buf[128];
  struct stat st;
  size_t sz;
  size_t idx;
  int is_nl = 0;
  FILE_OFFSET_T fSize;

  if(!path) {
    return 0;
  }

  if(fileops_stat(path, &st) != 0 || (st.st_mode & S_IFDIR)) {
    return 0;
  }

  if(OpenMediaReadOnly(&fs, path) != 0) {
    return 0;
  }

  if((fSize = fs.size) > METAFILE_FILE_CHK_SZMAX) {
    fSize = METAFILE_FILE_CHK_SZMAX;
  }

  while(fs.offset < fSize && rc != 1) {

    sz = sizeof(buf);
    if(fs.offset + sz > fSize) {
      sz = (size_t) (fSize - fs.offset);
    }
    if(ReadFileStream(&fs, buf, sz) != sz) {
      break;
    }
 
    for(idx = 0; idx < sz; idx++) {
      if(!avc_istextchar(buf[idx])) {
        CloseMediaFile(&fs);
        return 0;
      }
      if(buf[idx] == '\n') {
        is_nl = 1;
      } else {
        if(is_nl) {
          if(!memcmp(&buf[idx], METAFILE_KEY_FILE, strlen(METAFILE_KEY_FILE)) || 
             !memcmp(&buf[idx], METAFILE_KEY_DEVICE, strlen(METAFILE_KEY_DEVICE)) ||
             !memcmp(&buf[idx], METAFILE_KEY_DESCRIPTION, strlen(METAFILE_KEY_DESCRIPTION)) ||
             !memcmp(&buf[idx], METAFILE_KEY_IGNORE, strlen(METAFILE_KEY_IGNORE)) ||
             !memcmp(&buf[idx], METAFILE_KEY_RTMP_PROXY, strlen(METAFILE_KEY_RTMP_PROXY)) ||
             !memcmp(&buf[idx], METAFILE_KEY_RTSP_PROXY, strlen(METAFILE_KEY_RTSP_PROXY)) ||
             !memcmp(&buf[idx], METAFILE_KEY_HTTP_PROXY, strlen(METAFILE_KEY_HTTP_PROXY)) ||
             !memcmp(&buf[idx], METAFILE_KEY_METHODS, strlen(METAFILE_KEY_METHODS)) ||
             !memcmp(&buf[idx], METAFILE_KEY_XCODEARGS, strlen(METAFILE_KEY_XCODEARGS))) {
            rc = 1;
            break;
          } 
        }
        is_nl = 0;
      }
    }

  }

  CloseMediaFile(&fs);

  return rc;
}

int cbparse_methods_csv(void *pArg, const char *p) {
  PARSE_ENTRY_DATA_T *pEntryData = (PARSE_ENTRY_DATA_T *) pArg;
  STREAM_METHOD_T method;
  int rc = 0;

  if((method = devtype_methodfromstr(p)) > STREAM_METHOD_UNKNOWN && method < STREAM_METHOD_MAX) {
    pEntryData->methodBits |= (1 << method);
  }
  //LOG(X_DEBUG("PARSE_METHODS_CSV: %d, p:'%s', idx: %d"), method, p, pParseCtxt->idx);

  return rc;
}

static const char *store_parse_entry(const char *p, 
                               const char *key, 
                               int *pflags, 
                               int flag,  
                               char *buf, 
                               unsigned int bufsz) {

  if((p = strutil_skip_key(p, strlen(key))) && (p = avc_dequote(p, NULL, 0))) {

    if(buf && bufsz > 0) {
      strncpy(buf, p, bufsz - 1);
    }
    if(pflags && flag != 0) {
      *pflags |= flag;
    }
  }

  return p;
}

int cbparse_entry_metafile(void *pArg, const char *p) {
  int rc = 0;
  PARSE_ENTRY_DATA_T *pEntryData = (PARSE_ENTRY_DATA_T *) pArg;
  
  if(!strncasecmp(p, METAFILE_KEY_FILE, strlen(METAFILE_KEY_FILE))) {

    store_parse_entry(p, METAFILE_KEY_FILE, &pEntryData->flags, PARSE_FLAG_HAVE_FILENAME,  
               pEntryData->filename, sizeof(pEntryData->filename));
  } else if(!strncasecmp(p, METAFILE_KEY_RTMP_PROXY, strlen(METAFILE_KEY_RTMP_PROXY))) {

    store_parse_entry(p, METAFILE_KEY_RTMP_PROXY, &pEntryData->flags, PARSE_FLAG_HAVE_RTMP_PROXY,  
               pEntryData->rtmpproxy, sizeof(pEntryData->rtmpproxy));

  } else if(!strncasecmp(p, METAFILE_KEY_RTSP_PROXY, strlen(METAFILE_KEY_RTSP_PROXY))) {

    store_parse_entry(p, METAFILE_KEY_RTSP_PROXY, &pEntryData->flags, PARSE_FLAG_HAVE_RTSP_PROXY,  
               pEntryData->rtspproxy, sizeof(pEntryData->rtspproxy));

  } else if(!strncasecmp(p, METAFILE_KEY_HTTP_PROXY, strlen(METAFILE_KEY_HTTP_PROXY))) {

    store_parse_entry(p, METAFILE_KEY_HTTP_PROXY, &pEntryData->flags, PARSE_FLAG_HAVE_HTTP_PROXY,  
               pEntryData->httpproxy, sizeof(pEntryData->httpproxy));

  } else if(!strncasecmp(p, METAFILE_KEY_HTTPLINK, strlen(METAFILE_KEY_HTTPLINK))) {

    store_parse_entry(p, METAFILE_KEY_HTTPLINK, &pEntryData->flags, PARSE_FLAG_HAVE_HTTPLINK,  
               pEntryData->link, sizeof(pEntryData->link));

  } else if(!strncasecmp(p, METAFILE_KEY_INPUT, strlen(METAFILE_KEY_INPUT))) {

    store_parse_entry(p, METAFILE_KEY_INPUT, &pEntryData->flags, PARSE_FLAG_HAVE_INPUT,  
               pEntryData->input, sizeof(pEntryData->input));

  } else if(!strncasecmp(p, METAFILE_KEY_DEVICE, strlen(METAFILE_KEY_DEVICE))) {

    store_parse_entry(p, METAFILE_KEY_DEVICE, &pEntryData->flags, PARSE_FLAG_HAVE_DEVNAME,  
               pEntryData->devname, sizeof(pEntryData->devname));

  } else if(!strncasecmp(p, METAFILE_KEY_PROFILE, strlen(METAFILE_KEY_PROFILE))) {

    store_parse_entry(p, METAFILE_KEY_PROFILE, &pEntryData->flags, PARSE_FLAG_HAVE_PROFILE,  
               pEntryData->profile, sizeof(pEntryData->profile));

  } else if(!strncasecmp(p, METAFILE_KEY_SHARED, strlen(METAFILE_KEY_SHARED))) {

    if((p = store_parse_entry(p, METAFILE_KEY_SHARED, &pEntryData->flags, PARSE_FLAG_HAVE_SHARED, NULL, 0))) {
      if(!strcasecmp(p, "false") || !strcmp(p, "0")) {
        pEntryData->shared = 0;
      } else if(!strcasecmp(p, "true") || !strcmp(p, "1")) {
        pEntryData->shared =1;
      }
    }

  } else if(!strncasecmp(p, METAFILE_KEY_XCODEARGS, strlen(METAFILE_KEY_XCODEARGS))) {

    store_parse_entry(p, METAFILE_KEY_XCODEARGS, &pEntryData->flags, PARSE_FLAG_HAVE_XCODEARGS,  
               pEntryData->xcodestr, sizeof(pEntryData->xcodestr));

  } else if(!strncasecmp(p, METAFILE_KEY_DIGESTAUTH, strlen(METAFILE_KEY_DIGESTAUTH))) {

    store_parse_entry(p, METAFILE_KEY_DIGESTAUTH, &pEntryData->flags, PARSE_FLAG_HAVE_DIGESTAUTH,  
               pEntryData->userpass, sizeof(pEntryData->userpass));
    //TODO: verify ascii chars delimited by colon...

  } else if(!strncasecmp(p, METAFILE_KEY_METHODS, strlen(METAFILE_KEY_METHODS))) {

    p = store_parse_entry(p, METAFILE_KEY_METHODS, &pEntryData->flags, PARSE_FLAG_HAVE_METHODS, NULL, 0);
    strutil_parse_csv(cbparse_methods_csv, pEntryData, p);

  } else if(!strncasecmp(p, METAFILE_KEY_ID, strlen(METAFILE_KEY_ID))) {

    store_parse_entry(p, METAFILE_KEY_ID, &pEntryData->flags, PARSE_FLAG_HAVE_ID,  
               pEntryData->id, sizeof(pEntryData->id));

  } else if(!strncasecmp(p, METAFILE_KEY_IGNORE, strlen(METAFILE_KEY_IGNORE))) {

    store_parse_entry(p, METAFILE_KEY_IGNORE, &pEntryData->flags, PARSE_FLAG_HAVE_IGNORE,  
               pEntryData->ignore, sizeof(pEntryData->ignore));

  } else if(!strncasecmp(p, METAFILE_KEY_DESCRIPTION, strlen(METAFILE_KEY_DESCRIPTION))) {

    store_parse_entry(p, METAFILE_KEY_DESCRIPTION, &pEntryData->flags, PARSE_FLAG_HAVE_DESCRIPTION,  
               pEntryData->description, sizeof(pEntryData->description));

  } else {

    LOG(X_WARNING("Unhandled metafile parameter '%s' in %s line:%d"), p, pEntryData->path, pEntryData->linenum);

  }

  return rc;
}

static int check_filename(const char *filename, 
                          const char *path,
                          META_FILE_T *pMetaFile) {
  int sz;
  struct stat st;

  if(fileops_stat(filename, &st) != 0 && (sz = mediadb_getdirlen(path)) > 0) {
    strncpy(pMetaFile->filestr, path, sizeof(pMetaFile->filestr));
    strncpy(&pMetaFile->filestr[sz], filename, sizeof(pMetaFile->filestr) - sz);
  } else {
    strncpy(pMetaFile->filestr, filename, sizeof(pMetaFile->filestr));
  }

  return 0;
}

static int handle_parsed_line(const PARSE_ENTRY_DATA_T *parseData, 
                              META_FILE_T *pMetaFile, 
                              const char *path, 
                              int *pmatch) {
  int rc = 0;
  int matchDev = 0; 
  int matchProf = 0;
  int match = 0;
  char buf[128];
 
  //
  // Check if we the key 'profile=' on this line item, and if so, attempt to match the
  // input profilefilterstr to the listed profile
  //
  if((parseData->flags & PARSE_FLAG_HAVE_PROFILE) && parseData->profile[0] != '\0') {
    if(strncasecmp(parseData->profile, pMetaFile->profilefilterstr, MAX(
          strlen(pMetaFile->profilefilterstr), strlen(parseData->profile)))) {
      matchProf = -1;
    } else {
      matchProf = 1;
    }
  }

  //
  // Check if we the key 'device=' on this line item, and if so, attempt to match the
  // input devicefilterstr to the listed profile
  //
  if((parseData->flags & PARSE_FLAG_HAVE_DEVNAME) && parseData->devname[0] != '\0') {
    if(strncasecmp(parseData->devname, pMetaFile->devicefilterstr, MAX(
          strlen(pMetaFile->devicefilterstr), strlen(parseData->devname)))) {
      matchDev = -1;
      match = 0;
    } else {
      matchDev = 1;
    }
  }

  //
  // If the device matched and the profile is matched or missing
  // If the profile matched and the device is matched or missing
  // Or if there was no attempt to match anything and there are no specified device/profile filters, and the 
  // line item is not a device/profile specific item
  //
  if((matchDev == 1 && matchProf != -1) || 
     (matchProf == 1 && matchDev != -1) ||
     (matchDev != -1 && matchProf != -1 && pMetaFile->devicefilterstr[0] == '\0' && 
      pMetaFile->profilefilterstr[0] == '\0' && parseData->devname[0] == '\0' && parseData->profile[0] == '\0')) {
    match = 1;
  }

  VSX_DEBUG_METAFILE( 

    LOG(X_DEBUG("META - handle_parsed_line flags:0x%x, file:'%s', devname:'%s', profile:'%s', methods:'%s', "
                "xcode: '%s', digestauth: '%s', matchProf: %d, matchDev: %d, match: %d, pmatch: 0x%x, "
                "meta-devicefilter:'%s', meta-profilefilter: '%s'"), 
       parseData->flags, parseData->filename, parseData->devname, parseData->profile, 
       devtype_dump_methods(parseData->methodBits, buf, sizeof(buf)), 
       parseData->xcodestr, parseData->userpass, matchProf, matchDev, match, *pmatch, pMetaFile->devicefilterstr, 
       pMetaFile->profilefilterstr));

  if((parseData->flags & PARSE_FLAG_HAVE_FILENAME) &&
     ((matchDev == 1 && (matchProf == 1 || parseData->profile[0] == '\0')) ||
      !(*pmatch & MATCH_HAVE_FILENAME))) {

    VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line mite set file '%s'..."), parseData->filename));

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {

      check_filename(parseData->filename, path, pMetaFile);

      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set filename: '%s', path: '%s', *pmatch: 0x%x"), 
                             parseData->filename, path, *pmatch));

      if(match && (matchProf == 1 || matchDev == 1)) {
        *pmatch |= MATCH_HAVE_FILENAME;
        VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set MATCH_HAVE_FILENAME")));
      }
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_RTMP_PROXY) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {
    
      strncpy(pMetaFile->rtmpproxy, parseData->rtmpproxy, sizeof(pMetaFile->rtmpproxy));
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_RTSP_PROXY) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {
    
      strncpy(pMetaFile->rtspproxy, parseData->rtspproxy, sizeof(pMetaFile->rtspproxy));
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_HTTP_PROXY) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {
    
      strncpy(pMetaFile->httpproxy, parseData->httpproxy, sizeof(pMetaFile->httpproxy));
    }
  }
  
  if((parseData->flags & PARSE_FLAG_HAVE_HTTPLINK) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {
    
      strncpy(pMetaFile->linkstr, parseData->link, sizeof(pMetaFile->linkstr));
      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set link: '%s',  *pmatch: 0x%x"), 
                            parseData->link, *pmatch));
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_ID) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {
    
      strncpy(pMetaFile->id, parseData->id, sizeof(pMetaFile->id));
      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set id: '%s',  *pmatch: 0x%x"), parseData->id, *pmatch));
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_SHARED) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 &&
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {

      pMetaFile->shared = parseData->shared;
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_INPUT) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {
    
      strncpy(pMetaFile->instr, parseData->input, sizeof(pMetaFile->instr));
      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set input: '%s',  *pmatch: 0x%x"), 
                          parseData->input, *pmatch));
    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_METHODS) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_METHODS))) {

    if(match || (matchDev >= 0 && matchProf >= 0 &&
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {

      pMetaFile->methodBits = parseData->methodBits;

      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set methods: '%s', *pmatch: 0x%x"),
              devtype_dump_methods(pMetaFile->methodBits, buf, sizeof(buf)), *pmatch));
      if(match) {
        *pmatch |= MATCH_HAVE_METHODS;
        VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set MATCH_HAVE_METHODS")));
      }

    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_DIGESTAUTH) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_DIGESTAUTH))) {

    if(match || (matchDev >= 0 && matchProf >= 0 &&
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {

      strncpy(pMetaFile->userpass, parseData->userpass, sizeof(pMetaFile->userpass));

      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set digestauth: '%s', *pmatch: 0x%x"),
                              pMetaFile->userpass, *pmatch));
      if(match) {
        *pmatch |= MATCH_HAVE_DIGESTAUTH;
        VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set MATCH_HAVE_DIGESTAUTH")));
      }

    }
  }

  if((parseData->flags & PARSE_FLAG_HAVE_XCODEARGS) &&
     ((matchDev == 1 && matchProf == 1) || !(*pmatch & MATCH_HAVE_XCODE))) {

    VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line mite set xcode: '%s', id: '%s', *pmatch: 0x%x"), 
                          parseData->xcodestr, parseData->id, *pmatch));

    if(match || (matchDev >= 0 && matchProf >= 0 && 
       !((*pmatch & MATCH_BY_PROFILE) || (*pmatch & MATCH_BY_DEVICE)))) {

      strncpy(pMetaFile->xcodestr, parseData->xcodestr, sizeof(pMetaFile->xcodestr));
      strncpy(pMetaFile->id, parseData->id, sizeof(pMetaFile->id));

      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set xcode: '%s', id: '%s', *pmatch: 0x%x"), 
                          parseData->xcodestr, parseData->id, *pmatch));

      if(match) {
        *pmatch |= MATCH_HAVE_XCODE;
        VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set MATCH_HAVE_XCODE")));
      }
    }
  }

  // 
  // exact match for specified profile and device name
  //
  if((matchProf == 1 && matchDev == 1) ||
     ((matchDev >= 0 && matchProf >= 0) && 
      ((matchProf == 1 && matchDev >= 0) || (matchDev == 1 && matchDev >= 0)))) {

    if(matchDev == 1) {
      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set MATCH_BY_DEVICE")));
      *pmatch |= MATCH_BY_DEVICE;
    }
    if(matchProf == 1) {
      VSX_DEBUG_METAFILE(LOG(X_DEBUG("META - handle_parsed_line set MATCH_BY_PROFILE")));
      *pmatch |= MATCH_BY_PROFILE;
    }
  } 

  return rc;
}

#define PROFILE_LIST_SZ   16

typedef struct PROFILE_LIST {
  char profiles[PROFILE_LIST_SZ][META_FILE_PROFILESTR_MAX];
  unsigned int count;
  int haveCatchAll;
} PROFILE_LIST_T;

typedef struct PROFILE_LIST_ALL {
  PROFILE_LIST_T       profs_nodevmatch;
  PROFILE_LIST_T       profs_devmatch;
} PROFILE_LIST_ALL_T;

static const char *find_prof(const char *prof,  
                            char profs[][META_FILE_PROFILESTR_MAX], unsigned int max) {
  unsigned int idx;

  for(idx = 0; idx < max; idx++) {
    if((profs[idx][0] == '\0' && prof[0] == '\0') || !strcasecmp(profs[idx], prof)) {
      return profs[idx];
    }
  }

  return NULL;
}

static int add_prof(const char *prof, PROFILE_LIST_T *pProfList) {
  int rc = 0;

  if(find_prof(prof, pProfList->profiles, pProfList->count)) {
    return 0;
  }

  if(pProfList->count < PROFILE_LIST_SZ) {
    if(prof[0] == '\0') {
      pProfList->haveCatchAll = 1;
    }
    strncpy(pProfList->profiles[pProfList->count], prof, META_FILE_PROFILESTR_MAX);
    pProfList->count++;
    rc = 1;
  } else {
    rc = -1;
  }

  return rc;
}

static int check_profs(const PARSE_ENTRY_DATA_T *parseData, const char *devname,  
                      PROFILE_LIST_ALL_T *pProfs, const char *path) {
           
  int rc = 0;
  int matchDev = 0; 
  PROFILE_LIST_T *pProfList;
  
  if((parseData->flags & PARSE_FLAG_HAVE_DEVNAME) && parseData->devname[0] != '\0') {
    if(!devname || strncasecmp(parseData->devname, devname, MAX(
          strlen(devname), strlen(parseData->devname)))) {
      matchDev = -1;
    } else {
      matchDev = 1;
    }
  }

  if(matchDev >= 0 && parseData->flags != 0) {

    if(matchDev == 1) {
      pProfList = &pProfs->profs_devmatch;
    } else {
      pProfList = &pProfs->profs_nodevmatch;
    }
    if(add_prof(parseData->profile, pProfList) < 0) {
      LOG(X_WARNING("Too many unique profiles found in %s"), path);
    }

    //fprintf(stderr, "PROFILE:'%s'\n", parseData->profile);
  }

  return rc;
}

static void reset_parsedata_ctxt(PARSE_ENTRY_DATA_T *parseData) {

  parseData->devname[0] = '\0';
  parseData->filename[0] = '\0';
  parseData->link[0] = '\0';
  parseData->profile[0] = '\0';
  parseData->xcodestr[0] = '\0';
  parseData->userpass[0] = '\0';
  parseData->id[0] = '\0';
  parseData->input[0] = '\0';
  parseData->rtmpproxy[0] = '\0';
  parseData->rtspproxy[0] = '\0';
  parseData->httpproxy[0] = '\0';
  parseData->methodBits = 0;
  parseData->flags = 0;
  parseData->shared = -1; // designates not set
  parseData->ignore[0] = '\0';
  parseData->description[0] = '\0';

}

int metafile_findprofs(const char *path, const char *devname, 
                      char profs[][META_FILE_PROFILESTR_MAX], unsigned int max) {
  int rc = 0;
  FILE_HANDLE fp;
  char *p;
  unsigned int idx = 0;
  PARSE_ENTRY_DATA_T parseData;
  char buf[2048];
  PROFILE_LIST_ALL_T foundProfs;

  if(!path || !profs) {
    return -1;
  }  

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open metafile for reading profiles: %s"), path);
    return -1;
  }

  VSX_DEBUG_METAFILE( LOG(X_DEBUG("META - metafile_findprofs: '%s', devname: '%s'"), path, devname));

  memset(&foundProfs, 0, sizeof(foundProfs));
  memset(&parseData, 0, sizeof(parseData));
  parseData.path = path;

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {

    parseData.linenum++;

    p = buf;
    while(*p == ' ' || *p == '\t') {
      p++;
    }

    if(*p == '#') {
      continue;
    } 

    //
    // Reset the parse context storage
    //
    reset_parsedata_ctxt(&parseData);

    strutil_parse_csv(cbparse_entry_metafile, &parseData, p);

    if(parseData.flags == 0) {
      continue;
    }

    check_profs(&parseData, devname, &foundProfs, path);

  }

  fileops_Close(fp);

  rc = 0;
  for(idx = 0; idx < foundProfs.profs_devmatch.count && idx < max; idx++) {
    if(!find_prof(foundProfs.profs_devmatch.profiles[idx], profs, rc)) { 
      strncpy(profs[rc], foundProfs.profs_devmatch.profiles[idx], 
            META_FILE_PROFILESTR_MAX);
//fprintf(stderr, "ADDING DEVMATCH[%d] '%s'\n", rc, profs[rc]);
      rc++;
    }
  }  

  if(foundProfs.profs_devmatch.count == 0 || !foundProfs.profs_devmatch.haveCatchAll) {
    for(idx = 0; idx < foundProfs.profs_nodevmatch.count && idx < max; idx++) {
      if(!find_prof(foundProfs.profs_nodevmatch.profiles[idx], profs, rc)) { 
        strncpy(profs[rc], foundProfs.profs_nodevmatch.profiles[idx], 
                META_FILE_PROFILESTR_MAX);
//fprintf(stderr, "ADDING NODEVMATCH[%d] '%s'\n", rc, profs[rc]);
        rc++;
      }  
    }
  }

  return rc;
}

int metafile_open(const char *path, 
                  META_FILE_T *pMetaFile, 
                  int readIgnores,
                  int readDescrs) {
 
  int rc = 0;
  FILE_HANDLE fp;
  char *p;
  int match = 0;
  PARSE_ENTRY_DATA_T parseData;
  ENTRY_IGNORE_T *pIgnore;
  ENTRY_IGNORE_T *pIgnorePrev = NULL;
  ENTRY_META_DESCRIPTION_T *pDesc;
  ENTRY_META_DESCRIPTION_T *pDescPrev = NULL;
  char buf[1024];

  if(!path || !pMetaFile) {
    return -1;
  }  

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open metafile for reading: %s (ignore list:%d)"), 
        path, readIgnores);
    return -1;
  }

  VSX_DEBUG_METAFILE( LOG(X_DEBUG("META - metafile_open: '%s', readIgnores: %d, readDescrs: %d"), 
                      path, readIgnores, readDescrs));

  pMetaFile->filestr[0] = '\0';
  pMetaFile->linkstr[0] = '\0';
  pMetaFile->instr[0] = '\0';
  pMetaFile->xcodestr[0] = '\0';
  pMetaFile->userpass[0] = '\0';
  pMetaFile->methodBits = 0;

  memset(&parseData, 0, sizeof(parseData));
  parseData.path = path;

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {

    parseData.linenum++;
    p = buf;
    while(*p == ' ' || *p == '\t') {
      p++;
    }

    if(*p == '#') {
      continue;
    } 

    //
    // Reset the parse context storage
    //
    reset_parsedata_ctxt(&parseData);

    rc = strutil_parse_csv(cbparse_entry_metafile, &parseData, p);

    if(parseData.flags == 0) {
      continue;
    }

    if(!(readIgnores || readDescrs)) {
      handle_parsed_line(&parseData, pMetaFile, path, &match);
    }

    //
    // We're only interested in reading the ignore list
    //
    if(readIgnores && parseData.ignore[0] != '\0') {
      if(!(pIgnore = (ENTRY_IGNORE_T *) avc_calloc(1, sizeof(ENTRY_IGNORE_T)))) {
        rc = -1;
        break;
      }
      strncpy(pIgnore->filename, parseData.ignore, sizeof(pIgnore->filename));

      if(pIgnorePrev) {
        pIgnorePrev->pnext = pIgnore;
      } else {
        pMetaFile->pignoreList = pIgnore;
      }
      pIgnorePrev = pIgnore;
    }

    //
    // We're only interested in reading the resource description 
    //
    if(readDescrs && parseData.description[0] != '\0' && parseData.filename[0] != '\0') {

      if(!(pDesc = (ENTRY_META_DESCRIPTION_T *) avc_calloc(1, sizeof(ENTRY_META_DESCRIPTION_T)))) {
        rc = -1;
        break;
      }

      strncpy(pDesc->description, parseData.description, sizeof(pDesc->description));
      strncpy(pDesc->filename, parseData.filename, sizeof(pDesc->filename));
      if(pDescPrev) {
        pDescPrev->pnext = pDesc;
      } else {
        pMetaFile->pDescriptionList = pDesc;
      }
      pDescPrev = pDesc;
    }

  }

  fileops_Close(fp);

  VSX_DEBUG_METAFILE( 
    LOG(X_DEBUG("META - metafile_open '%s', meta-devicefilter:'%s', meta-profilefilter: '%s' returning rc: %d, "
                "file: '%s', link: '%s', input: '%s', xcode: '%s', digestauth: '%s', methods: '%s', id: '%s'" ), 
                 path, pMetaFile->devicefilterstr, pMetaFile->profilefilterstr, rc, pMetaFile->filestr, 
                 pMetaFile->linkstr, pMetaFile->instr, pMetaFile->xcodestr, pMetaFile->userpass,
                 devtype_dump_methods(pMetaFile->methodBits, buf, sizeof(buf)), pMetaFile->id));

  return rc;
}

char *metafile_find(const char *pathmedia, 
                    char *pathbuf, 
                    unsigned int szpathbuf,
                    int *ploaddefault) {
  int sz;
  struct stat st;
  int loaddefault = 0;

  if(!pathmedia) {
  }

  if(ploaddefault) {
    loaddefault = *ploaddefault;
    *ploaddefault = 0;
  }

  VSX_DEBUG_METAFILE( LOG(X_DEBUG("META - metafile_find pathmedia: '%s', loaddefault: %d"), 
                         pathmedia, loaddefault));

  //
  // Check if the media resource is actually a meta file
  //
  if(metafile_isvalidFile(pathmedia)) {
    return (char *) pathmedia;
  }

  //
  // Try to load the metafile for the media resource
  //
  snprintf(pathbuf, szpathbuf, "%s%s", pathmedia, METAFILE_EXT);
  if(fileops_stat(pathbuf, &st) == 0) {
    return pathbuf; 
  }

  //
  // Try to load the directory wide metafile
  //
  if(loaddefault && (sz = mediadb_getdirlen(pathmedia)) > 0) {
    strncpy(pathbuf, pathmedia, szpathbuf);
    strncpy(&pathbuf[sz], METAFILE_DEFAULT, szpathbuf - sz);
    if(fileops_stat(pathbuf, &st) == 0) {
      if(ploaddefault) {
        *ploaddefault = 1;
      }
      return pathbuf;
    }
  }

  return NULL;
}

int metafile_close(META_FILE_T *pMetaFile) {
  int rc = 0;
  ENTRY_IGNORE_T *pIgnore, *pIgnoreNext;
  ENTRY_META_DESCRIPTION_T *pDesc, *pDescNext;
  
  if(pMetaFile) {

    VSX_DEBUG_METAFILE( LOG(X_DEBUG("META - metafile_close for file: '%s'"), pMetaFile->filestr) );

    pIgnore = pMetaFile->pignoreList;
    while(pIgnore) {
      pIgnoreNext = pIgnore->pnext;
      free(pIgnore); 
      pIgnore = pIgnoreNext;
    }

    pDesc = pMetaFile->pDescriptionList;
    while(pDesc) {
      pDescNext = pDesc->pnext;
      free(pDesc); 
      pDesc = pDescNext;
    }

    pMetaFile->pignoreList = NULL;
    pMetaFile->pDescriptionList = NULL;
  }

  return rc;
}
