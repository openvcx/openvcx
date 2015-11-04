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

static STREAM_DEVICE_T *g_devtypes;

static STREAM_DEVICE_T *create_device(const char *name, const char *strmatch,
                                      const char *strmatch2,
                                      enum STREAM_METHOD methods[], 
                                      enum STREAM_DEVICE_TYPE devtype) {
  STREAM_DEVICE_T *dev;

  if(!(dev = avc_calloc(1, sizeof(STREAM_DEVICE_T)))) {
    return NULL;
  }

  strncpy((char *) dev->name, name, sizeof(dev->name) - 1);
  strncpy((char *) dev->strmatch, strmatch, sizeof(dev->strmatch) - 1);
  if(strmatch2 && strmatch2[0] != '\0') {
    strncpy((char *) dev->strmatch2, strmatch2, sizeof(dev->strmatch2) - 1);
  } 
  memcpy(dev->methods, methods, sizeof(enum STREAM_METHOD) * STREAM_DEVICE_METHODS_MAX);
  dev->devtype = devtype;

  return dev;
}

static void devtype_free(STREAM_DEVICE_T *pdev) {
  STREAM_DEVICE_T *pdevnext;

  if(g_devtypes == pdev) {
    g_devtypes = NULL;
  }

  while(pdev) {
    pdevnext = pdev->pnext;
    free(pdev);
    pdev = pdevnext;
  }

}

void devtype_dump(STREAM_DEVICE_T *pdev) {

  while(pdev) {
    fprintf(stderr, "dev:'%s' match:'%s','%s', method:%d,%d,%d,%d, devtype:%d\n", pdev->name, pdev->strmatch, pdev->strmatch2, pdev->methods[0], pdev->methods[1], pdev->methods[2], pdev->methods[3], pdev->devtype);
    pdev = pdev->pnext;
  }
}

const STREAM_DEVICE_T *devtype_finddevice(const char *userAgent, int matchany) {
  const STREAM_DEVICE_T *pdev = NULL;

  if(!g_devtypes) {
    //devtype_init();
    LOG(X_ERROR("Device database not configured"));
    return NULL;
  }

  pdev = g_devtypes;
  while(pdev) {

    //
    // if matchany is set, any default empty match can be made (last fall-thru device)
    // match & match2 is an 'and' based match
    //
    if(matchany || pdev->strmatch[0] != '\0' || pdev->strmatch[1] != '\0') {

      if(pdev->strmatch[0] == '\0' || (userAgent && strcasestr(userAgent, pdev->strmatch))) {
        if(pdev->strmatch2[0] == '\0' || 
          (userAgent && strcasestr(userAgent, pdev->strmatch2))) {
          return pdev;
        }
      }
    }
    pdev = pdev->pnext;
  }

  if(matchany) {
    return pdev;
  } else {
    return NULL;
  }  
}


#define PARSE_FLAG_HAVE_DEVNAME          0x01
#define PARSE_FLAG_HAVE_METHOD           0x02
#define PARSE_FLAG_HAVE_DEVTYPE          0x04
#define PARSE_FLAG_HAVE_MATCH            0x08

#define PARSE_FLAG_HAVE_ALL  (PARSE_FLAG_HAVE_DEVNAME | PARSE_FLAG_HAVE_METHOD | \
                              PARSE_FLAG_HAVE_DEVTYPE | PARSE_FLAG_HAVE_MATCH)  

typedef struct PARSE_ENTRY_DATA {
  int                      flags;
  char                     devname[STREAM_DEV_NAME_MAX];
  char                     strmatch[64];
  char                     strmatch2[64];
  unsigned int             numMethods;
  enum STREAM_METHOD       methods[STREAM_DEVICE_METHODS_MAX];
  enum STREAM_DEVICE_TYPE  devtype;
} PARSE_ENTRY_DATA_T;

static enum STREAM_DEVICE_TYPE devtype_typefromstr(const char *str) {

  if(!strcasecmp(str, STREAM_DEVICE_TYPE_MOBILE_STR)) {
    return STREAM_DEVICE_TYPE_MOBILE;
  } else if(!strcasecmp(str, STREAM_DEVICE_TYPE_TABLET_STR)) {
    return STREAM_DEVICE_TYPE_TABLET;
  } else if(!strcasecmp(str, STREAM_DEVICE_TYPE_PC_STR)) {
    return STREAM_DEVICE_TYPE_PC;
  } else if(!strcasecmp(str, STREAM_DEVICE_TYPE_UNSUPPORTED_STR)) {
    return STREAM_DEVICE_TYPE_UNSUPPORTED;
  } else {
    return STREAM_DEVICE_TYPE_UNKNOWN;
  }

}

const char *devtype_methodstr(enum STREAM_METHOD method) {

  switch(method) {
    case STREAM_METHOD_HTTPLIVE:
      return STREAM_METHOD_HTTPLIVE_STR;
    case STREAM_METHOD_DASH:
      return STREAM_METHOD_DASH_STR;
    case STREAM_METHOD_FLVLIVE:
      return STREAM_METHOD_FLVLIVE_STR;
    case STREAM_METHOD_MKVLIVE:
      return STREAM_METHOD_MKVLIVE_STR;
    case STREAM_METHOD_TSLIVE:
      return STREAM_METHOD_TSLIVE_STR;
    case STREAM_METHOD_RTSP:
      return STREAM_METHOD_RTSP_STR;
    case STREAM_METHOD_RTSP_INTERLEAVED:
      return STREAM_METHOD_RTSP_INTERLEAVED_STR;
    case STREAM_METHOD_RTSP_HTTP:
      return STREAM_METHOD_RTSP_HTTP_STR;
    case STREAM_METHOD_RTMPT:
      return STREAM_METHOD_RTMPT_STR;
    case STREAM_METHOD_RTMP:
      return STREAM_METHOD_RTMP_STR;
    case STREAM_METHOD_PROGDOWNLOAD:
      return STREAM_METHOD_PROGDOWNLOAD_STR;
    case STREAM_METHOD_FLASHHTTP:
      return STREAM_METHOD_FLASHHTTP_STR;
    case STREAM_METHOD_UDP_RTP:
      return STREAM_METHOD_UDP_RTP_STR;
    case STREAM_METHOD_UDP:
      return STREAM_METHOD_UDP_STR;
    default:
      return ""; 
  }

}

STREAM_METHOD_T devtype_methodfromstr(const char *str) {

  if(!str) {
    return STREAM_METHOD_UNKNOWN;
  } else if(!strncasecmp(str, STREAM_METHOD_HTTPLIVE_STR, 
                  strlen(STREAM_METHOD_HTTPLIVE_STR) + 1)) {
    return STREAM_METHOD_HTTPLIVE;
  } else if(!strncasecmp(str, STREAM_METHOD_DASH_STR, 
                  strlen(STREAM_METHOD_DASH_STR) + 1)) {
    return STREAM_METHOD_DASH;
  } else if(!strncasecmp(str, STREAM_METHOD_TSLIVE_STR, 
                  strlen(STREAM_METHOD_TSLIVE_STR) + 1)) {
    return STREAM_METHOD_TSLIVE;
  } else if(!strncasecmp(str, STREAM_METHOD_RTSP_STR, 
                  strlen(STREAM_METHOD_RTSP_STR) + 1)) {
    return STREAM_METHOD_RTSP;
  } else if(!strncasecmp(str, STREAM_METHOD_RTSP_INTERLEAVED_STR, 
                  strlen(STREAM_METHOD_RTSP_INTERLEAVED_STR) + 1)) {
    return STREAM_METHOD_RTSP_INTERLEAVED;
  } else if(!strncasecmp(str, STREAM_METHOD_RTSP_HTTP_STR, 
                  strlen(STREAM_METHOD_RTSP_HTTP_STR) + 1)) {
    return STREAM_METHOD_RTSP_HTTP;
  } else if(!strncasecmp(str, STREAM_METHOD_RTMPT_STR, 
                  strlen(STREAM_METHOD_RTMPT_STR) + 1)) {
    return STREAM_METHOD_RTMPT;
  } else if(!strncasecmp(str, STREAM_METHOD_RTMP_STR, 
                  strlen(STREAM_METHOD_RTMP_STR) + 1)) {
    return STREAM_METHOD_RTMP;
  } else if(!strncasecmp(str, STREAM_METHOD_FLVLIVE_STR, 
                  strlen(STREAM_METHOD_FLVLIVE_STR) + 1)) {
    return STREAM_METHOD_FLVLIVE;
  } else if(!strncasecmp(str, STREAM_METHOD_MKVLIVE_STR, 
                  strlen(STREAM_METHOD_MKVLIVE_STR) + 1)) {
    return STREAM_METHOD_MKVLIVE;
  } else if(!strncasecmp(str, STREAM_METHOD_PROGDOWNLOAD_STR, 
                  strlen(STREAM_METHOD_PROGDOWNLOAD_STR) + 1)) {
    return STREAM_METHOD_PROGDOWNLOAD;
  } else if(!strncasecmp(str, STREAM_METHOD_FLASHHTTP_STR, 
                  strlen(STREAM_METHOD_FLASHHTTP_STR) + 1)) {
    return STREAM_METHOD_FLASHHTTP;
  } else if(!strncasecmp(str, STREAM_METHOD_UDP_RTP_STR, 
                  strlen(STREAM_METHOD_UDP_RTP_STR) + 1)) {
    return STREAM_METHOD_UDP_RTP;
  } else if(!strncasecmp(str, STREAM_METHOD_UDP_STR, 
                  strlen(STREAM_METHOD_UDP_STR) + 1)) {
    return STREAM_METHOD_UDP;
  } else {
    return STREAM_METHOD_UNKNOWN;
  }

}

char *devtype_dump_methods(int methodBits,  char *buf, unsigned int szbuf) {
  int rc;
  unsigned int idxMethod;
  unsigned int idxbuf = 0;

  if(buf && szbuf > 0) {
    buf[0] = '\0';
  }

  for(idxMethod = 0; idxMethod < STREAM_METHOD_MAX; idxMethod++) {
    if(methodBits & (1 << idxMethod)) {
      if((rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "%s%s",
                        idxbuf > 0 ? "," : "", devtype_methodstr(idxMethod))) > 0) {
        idxbuf += rc;
      }
    }
  }

  return buf;
}

/*
int devtype_findmethod(const STREAM_DEVICE_T *pdev, STREAM_METHOD_T method) {
  unsigned int idx = 0; 
  for(idx = 0; idx < STREAM_DEVICE_METHODS_MAX; idx++) {
    if(pdev->methods[idx] == method) {
      return 1;
    }
  }

  return 0;
}
*/

int cbparse_entry_devtype_method(void *pArg, const char *p) {
  int rc = 0;
  PARSE_ENTRY_DATA_T *pEntryData = (PARSE_ENTRY_DATA_T *) pArg;

  if(pEntryData->numMethods < STREAM_DEVICE_METHODS_MAX) {
    pEntryData->methods[pEntryData->numMethods++] = devtype_methodfromstr(p);
    rc = 1;
  }

  return rc;
}

int cbparse_entry_devtype(void *pArg, const char *p) {
  int rc = 0;
  PARSE_ENTRY_DATA_T *pEntryData = (PARSE_ENTRY_DATA_T *) pArg;

  //fprintf(stderr, "entry '%s'\n", p);

  if(!strncasecmp(p, DEVFILE_KEY_DEVICE, strlen(DEVFILE_KEY_DEVICE))) {

    if((p = strutil_skip_key(p, strlen(DEVFILE_KEY_DEVICE)))) {
      p = avc_dequote(p, NULL, 0);
      strncpy(pEntryData->devname, p, sizeof(pEntryData->devname) - 1);
      pEntryData->flags |= PARSE_FLAG_HAVE_DEVNAME; 
    }

  } else if(!strncasecmp(p, DEVFILE_KEY_METHODS, strlen(DEVFILE_KEY_METHODS))) {

    if((p = strutil_skip_key(p, strlen(DEVFILE_KEY_METHODS)))) {
      p = avc_dequote(p, NULL, 0);

      if(strutil_parse_csv(cbparse_entry_devtype_method, pEntryData, p) < 0) {
        LOG(X_WARNING("Failed to parse device type method(s) '%s'"), p);
      }
      //fprintf(stderr, "%s %d %d %d %d\n", pEntryData->devname, pEntryData->methods[0], pEntryData->methods[1], pEntryData->methods[2], pEntryData->methods[3]);

      pEntryData->flags |= PARSE_FLAG_HAVE_METHOD; 
    }

  } else if(!strncasecmp(p, DEVFILE_KEY_METHOD, strlen(DEVFILE_KEY_METHOD))) {

    if((p = strutil_skip_key(p, strlen(DEVFILE_KEY_METHOD)))) {
      p = avc_dequote(p, NULL, 0);

      pEntryData->methods[0] = devtype_methodfromstr(p);
      pEntryData->flags |= PARSE_FLAG_HAVE_METHOD; 
    }

  } else if(!strncasecmp(p, DEVFILE_KEY_TYPE, strlen(DEVFILE_KEY_TYPE))) {

    if((p = strutil_skip_key(p, strlen(DEVFILE_KEY_TYPE)))) {
      p = avc_dequote(p, NULL, 0);
      pEntryData->devtype = devtype_typefromstr(p);
      pEntryData->flags |= PARSE_FLAG_HAVE_DEVTYPE; 
    }

  } else if(!strncasecmp(p, DEVFILE_KEY_MATCH, strlen(DEVFILE_KEY_MATCH))) {

    if((p = strutil_skip_key(p, strlen(DEVFILE_KEY_MATCH)))) {
      p = avc_dequote(p, NULL, 0);
      strncpy(pEntryData->strmatch, p, sizeof(pEntryData->strmatch) - 1);
      pEntryData->flags |= PARSE_FLAG_HAVE_MATCH; 
    }

  }

  return rc;
}

int devtype_loadcfg(const char *path) {
  STREAM_DEVICE_T *pdev = NULL;
  STREAM_DEVICE_T *pdevprev = NULL;
  FILE_HANDLE fp;
  char buf[1024];
  const char *p;
  int linenum = 1;
  int count = 0;
  PARSE_ENTRY_DATA_T parseData;

  if(!path) {
    return -1;
  }

  if(g_devtypes) {
    devtype_free(g_devtypes);
    g_devtypes = NULL;
  }

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open metafile for reading: %s"), path);
    return -1;
  }

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {

    p = buf;
    while(*p == ' ' || *p == '\t') {
      p++;
    }

    if(*p == '#') {
      continue;
    }

    memset(&parseData, 0, sizeof(parseData));
    if(strutil_parse_csv(cbparse_entry_devtype, &parseData, p) < 0) {
      LOG(X_ERROR("Failed to parse line %d in file %s"), linenum, path);
      devtype_free(g_devtypes);
      break;
    } else if((parseData.flags & PARSE_FLAG_HAVE_ALL)) { 
      if(!(pdev = create_device(parseData.devname, parseData.strmatch,
                           parseData.strmatch2, parseData.methods,
                           parseData.devtype))) {
        LOG(X_ERROR("Failed to create device config from line %d"), linenum);
        devtype_free(g_devtypes);
        break;
      } else if(pdevprev) {
        pdevprev->pnext = pdev;
      } else {
        g_devtypes = pdev;
      }
      pdevprev = pdev;
      count++;

    } else if(strlen(p) > 1) {
      LOG(X_WARNING("Incomplete line %d in file %s"), linenum, path);
    }

    

    linenum++;
  }

  fileops_Close(fp);

  LOG(X_DEBUG("Read %d device profiles from %s"), count, path);

  //devtype_dump(g_devtypes);

  return count;
}

int devtype_defaultcfg() {
  STREAM_DEVICE_T *pdev = NULL;
  unsigned int idx;
  enum STREAM_METHOD methods[STREAM_DEVICE_METHODS_MAX];

  if(g_devtypes) {
    devtype_free(g_devtypes);
    g_devtypes = NULL;
  }

  for(idx = 0; idx < STREAM_DEVICE_METHODS_MAX; idx++) {
    methods[idx] = STREAM_METHOD_UNKNOWN;
  }

  if(!(pdev = create_device("unknown", "", "", methods,
                             STREAM_DEVICE_TYPE_UNKNOWN))) {
    return -1;
  }

  g_devtypes = pdev;

  return 1;
}
