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
#include "vsxlib_int.h"


int srv_ctrl_islegal_fpath(const char *path) {
  char tmpbuf[2][16];

  if(!path) {
    return 0;
  }

  snprintf(tmpbuf[0], sizeof(tmpbuf[0]), "%c..", DIR_DELIMETER);
  snprintf(tmpbuf[1], sizeof(tmpbuf[1]), "..%c", DIR_DELIMETER);

  if(strstr(path, tmpbuf[0]) || strstr(path, tmpbuf[1])) {
    return 0;
  }

  return 1;
}

static int listmediafileinfo(CLIENT_CONN_T *pConn, const char *file, const char *fidxfile,
                   char *pData, unsigned int *plen) {
  FILE_LIST_T fileList;
  FILE_LIST_ENTRY_T *pFileEntry = NULL;
  const char *filepart;
  size_t sz = 0;
  unsigned int len = 0;
  struct stat st;
  const char *str_fmt;

  memset(&fileList, 0, sizeof(fileList));
  file_list_read(fidxfile, &fileList);

  sz = mediadb_getdirlen(file);
  filepart = &file[sz];
  sz = strlen(filepart);

  if((pFileEntry = file_list_find(&fileList, filepart))) {
    //
    // Time needs to be in time_t (# of seconds since 1970) format
    // suitable for javascript Date.setTime call 
    //
    #define FMT_FILEENTRY "vc=%s&ac=%s&sz=%llu&dur=%.1f&tm="
    if(sizeof(st.st_ctime) == 8) {
      str_fmt = FMT_FILEENTRY"%"LL64"d&";
    } else {
      str_fmt = FMT_FILEENTRY"%ld&";
    }
  
    if((sz = snprintf(pData, *plen, str_fmt, 
                    pFileEntry->vstr, pFileEntry->astr, pFileEntry->size, 
                    pFileEntry->duration, pFileEntry->tm)) > 0) {
      len = sz; 
    }

  } else if(!strncasecmp(&filepart[sz - VSXTMP_EXT_LEN], VSXTMP_EXT, VSXTMP_EXT_LEN)) {

    if(fileops_stat(file, &st) == 0) {

      #define FMT_AVCTMP "sz=%llu&tm="
      if(sizeof(st.st_ctime) == 8) {
        str_fmt = FMT_AVCTMP"%lld&";
      } else {
        str_fmt = FMT_AVCTMP"%ld&";
      }

      if((sz = snprintf(pData, *plen, str_fmt, st.st_size, st.st_ctime)) > 0) {
        len = sz; 
      }

    } else {
      LOG(X_ERROR("Unable to stat %s"), file);
    }

  } else {
    LOG(X_DEBUG("Unable to find entry '%s' in '%s'"), filepart, fidxfile);
  }

  file_list_close(&fileList);

  *plen = len;

  return 0;
}

static char *listdir(CLIENT_CONN_T *pConn, 
                     const char *dir, 
                     const char *fidxdir,
                     const char *pargdir, 
                     unsigned int *plenOut, 
                     const char *searchfilter,
                     unsigned int startIdx, 
                     unsigned int countIdx, 
                     enum DIR_SORT sort) {

  DIR_ENTRY_LIST_T *pEntryList = NULL;
  DIR_ENTRY_T *pEntry = NULL;
  DIR_ENTRY_T *pEntryStart = NULL;
  unsigned int entryIdx = 0;
  char prfx;
  char filename[VSX_MAX_PATH_LEN * 2];
  char dispname[META_FILE_DESCR_LEN * 2];
  char buf[512];
  char tmp[64];
  int lenEncoded;
  int sz = 0;
  int rc;
  char *pBuf = NULL;
  unsigned int idxBuf = 0;
  unsigned int lenBuf = 0;
  unsigned int count = 0;
  unsigned int cntTotInDir = 0;

  *plenOut = 0;

  if((pEntryList = direntry_getentries(pConn->pCfg->pMediaDb, dir, 
                          fidxdir, searchfilter, 1, startIdx, countIdx, sort))) {
    pEntry = pEntryList->pHead;
    cntTotInDir = pEntryList->cntTotalInDir;
  }

  //TODO: silly not to have a count directly from the dirlist
  //while(pEntry && entryIdx < startIdx) {
  //  pEntry = pEntry->pnext;
  //  entryIdx++;
  //}

  pEntryStart = pEntry;
  entryIdx = 0;
  while(pEntry && (countIdx ==  0 || entryIdx++ < countIdx)) {

    lenEncoded = http_urlencode(pEntry->d_name, NULL, 0);
    sz = snprintf(buf, sizeof(buf), "%u", count);
    lenBuf += lenEncoded + 4 + sz; 

    if(pEntry->displayname && pEntry->displayname[0] != '\0') {
      lenEncoded = http_urlencode(pEntry->displayname, NULL, 0);
      lenBuf += lenEncoded + 4; 
    }

    if(!(pEntry->d_type & DT_DIR)) {
      // account for s_[idx]=[size]&t_[idx]=[duration]
      lenBuf += 44;
      lenBuf += 14; // time val
      if(pEntry->numTn > 0) {
        lenBuf += 14;
      }
    }

    pEntry = pEntry->pnext;
    count++;
  }

  lenBuf += (512 + 32);
  if(!(pBuf = (char *) avc_calloc(1, lenBuf))) {
    direntry_free(pEntryList);
    return NULL;
  }

  //
  // deprecated: Default UI display to windows style directory
  // do not alter the path which client sent to us
  //mediadb_fixdirdelims((char *) pargdir, '\\');
  if((sz = snprintf(pBuf, lenBuf, "cnt=%u&tot=%u&idx=%u&dir=%s&", 
                    count, cntTotInDir, startIdx, pargdir)) > 0) {
    idxBuf += sz; 
  }

  pEntry = pEntryStart;
  entryIdx = 0;
  while(pEntry && (countIdx == 0 || entryIdx < countIdx)) {

    lenEncoded = http_urlencode(pEntry->d_name, filename, sizeof(filename));
    sz = snprintf(buf, sizeof(buf), "%u", entryIdx);
    if(!(pEntry->d_type & DT_DIR)) {
      prfx = 'f';

      if(pEntry->numTn > 0) {
       // Number of thumbnails
        snprintf(tmp, sizeof(tmp), "t_%u=%d&", entryIdx, pEntry->numTn);
      } else {
        tmp[0] = '\0';
      }

      // File size, File duration 
      if((sz = snprintf(buf, sizeof(buf), "s_%u=%"LL64"u&d_%u=%.1f&c_%u=%ld&",
               entryIdx, pEntry->size, entryIdx, pEntry->duration, entryIdx, 
               pEntry->tm)) > 0) {
        if(tmp[0] != '\0') {
          if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s", tmp)) > 0) {
            sz += rc;
          }
        }
      }

      if(sz >0 && pEntry->displayname && pEntry->displayname[0] != '\0') {
        lenEncoded = http_urlencode(pEntry->displayname, dispname, sizeof(dispname));
        snprintf(&buf[sz], sizeof(buf) - sz, "n_%u=%s&",
                    entryIdx, dispname);
      }

    } else {
      prfx = 'd';
      buf[0] = '\0';
    }

    if((sz = snprintf(&pBuf[idxBuf], lenBuf- idxBuf, "%c_%u=%s&%s",
                    prfx, entryIdx, filename, buf)) >= 0) {
          idxBuf += sz;
    } else {
      LOG(X_WARNING("output buffer size [%u] / %u too small for '%s' ('%s')"),
           idxBuf, lenBuf, pEntry->d_name, filename);
      break;
    }

    entryIdx++;
    pEntry = pEntry->pnext;
  }


  direntry_free(pEntryList);

  *plenOut = idxBuf;

  return pBuf; 
}


static char *find_entry_description(ENTRY_META_DESCRIPTION_T *pDescList,
                                    const char *filename) {
  if(!filename) {
    return NULL;
  }

  //fprintf(stderr, "LOOK FOR DESC '%s'\n", filename);

  while(pDescList) {
    if(!strcasecmp(pDescList->filename, filename)) {
      return pDescList->description;
    }
    pDescList = pDescList->pnext;
  }
  return NULL;
}

static int check_metafile_default(char *filepath, char *pData, int len) {

  int rc;
  const char *parg;
  META_FILE_T metaFile;
  const char *pathMedia = NULL;
  char buf[VSX_MAX_PATH_LEN];

  //
  // Read dir wide metafile to get displayname
  //
  if((rc = mediadb_getdirlen(filepath)) > 0) {

    strncpy(buf, filepath, VSX_MAX_PATH_LEN);
    snprintf(&buf[rc], sizeof(buf) - rc, METAFILE_DEFAULT);

    memset(&metaFile, 0, sizeof(metaFile)); 
    if((rc = mediadb_getdirlen(filepath)) > 0) {
      pathMedia = &filepath[rc]; 
    }

    //
    // Check for a resource description in the metafile
    //
    rc = 0;
    if(metafile_open(buf, &metaFile, 0, 1) == 0 && 
      (parg = find_entry_description(metaFile.pDescriptionList, pathMedia))) {

      http_urlencode(parg, buf, sizeof(buf));

      // 
      // Include displayname 
      //
      rc = snprintf(pData, len, "n=%s&", buf);

    }
    metafile_close(&metaFile);
  }  

  return rc;
}


static int check_metafile(const CLIENT_CONN_T *pConn, char *filepath,
                          char *pData, unsigned int *plen) {
  int rc = 0;
  const char *userAgent;
  const char *pathMetaFile = NULL;
  const char *pathMedia;
  const char *profile = NULL;
  const char *parg;
  unsigned int idx;
  int len = 0;
  int chkprofile = 0;
  char *filepathorig = filepath;
  const STREAM_DEVICE_T *pdevtype = NULL;
  META_FILE_T metaFile;
  int numprofiles = 0;
  char profiles[12][META_FILE_PROFILESTR_MAX];
  char buf[VSX_MAX_PATH_LEN];
  char filepathbuf[VSX_MAX_PATH_LEN];

  pathMedia = filepath;

  if(!(pathMetaFile = metafile_find(filepath, buf, sizeof(buf), NULL))) {
    rc = -1;
  }

  if(rc >= 0) {
    if((userAgent = conf_find_keyval((const KEYVAL_PAIR_T *)
                        &pConn->httpReq.reqPairs, HTTP_HDR_USER_AGENT))) {
      pdevtype = devtype_finddevice(userAgent, 1);
    }

    if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs,
                                           "chkprofile"))) {
      chkprofile = atoi(parg);
    }

    if(chkprofile) {

      if(!(profile = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, 
                                       "profile"))) {
        profile = "";
      }

      memset(&metaFile, 0, sizeof(metaFile)); 
      if(pdevtype) {
        strncpy(metaFile.devicefilterstr, pdevtype->name, sizeof(metaFile.devicefilterstr) - 1);
      }
      strncpy(metaFile.profilefilterstr, profile, sizeof(metaFile.profilefilterstr) - 1);

     if(metafile_open(pathMetaFile, &metaFile, 0, 0) == 0 && metaFile.filestr[0] != '\0') {
        if(pathMetaFile == filepath) {
          strncpy(buf, filepath, VSX_MAX_PATH_LEN);
          pathMetaFile = buf;
        }
        LOG(X_DEBUG("Replacing metafile media path '%s' with '%s' according to '%s'"), 
                   filepath, metaFile.filestr, pathMetaFile);
        strncpy(filepathbuf, filepath,  VSX_MAX_PATH_LEN); 
        filepathorig = filepathbuf;
        strncpy(filepath, metaFile.filestr,  VSX_MAX_PATH_LEN); 
      }
      metafile_close(&metaFile);

      if((numprofiles = metafile_findprofs(pathMetaFile, pdevtype ? pdevtype->name : NULL, 
                                   profiles, 12)) > 0) {
        len = 0;
        for(idx = 0; idx < numprofiles; idx++) {
          if((rc = snprintf(&pData[len], *plen -  len, "prof_%d=%s&", idx, profiles[idx])) < 0) {
            break;
          }
          len += rc;
        }
        *plen -= len;

      }

    } // end of if(chkprofile)

  } // end of if(rc >= 0) 

  //
  // Check the default meta file for the resource display name
  // Note: we're passing filepathorig (nonreplaced according to prof_ arg
  // to enable description field to mach original resource name
  // not specific to current profile rendering
  //
  if((rc = check_metafile_default(filepathorig, &pData[len], *plen)) > 0) {
    len += rc;
    *plen -= len;
  }

  return len;
}

int srv_ctrl_file_find_wext(char *filepath) {

  int rc;
  int append_len = 0;
  char buf[VSX_MAX_PATH_LEN];
  struct stat st;

  if(!filepath) {
    return -1;
  }

  //
  // Allow a resource to be referred without the filename extension
  //
  if((rc = strlen(filepath)) <= 4) {
    return 0;
  }

  if(!strchr(&filepath[rc], '.')) {
    //TODO: should be more generic check than just .mp4 , .sdp
    if((rc = fileops_stat(filepath, &st)) < 0) {
      snprintf(buf, sizeof(buf), "%s.mp4", filepath);

      if(rc < 0 && (rc = fileops_stat(buf, &st)) == 0) {
        append_len = 4; 
      } else {
        snprintf(buf, sizeof(buf), "%s.sdp", filepath);
      }
      if(rc < 0 && (rc = fileops_stat(buf, &st)) == 0) {
        append_len = 4; 
      } else {

      }

      if(append_len > 0) {
        strncpy(filepath, buf, VSX_MAX_PATH_LEN -1);
        return append_len;
      }
    }
  }

  return 0;
}


int srv_ctrl_listmedia(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus) {
  const char *pargdir = NULL;
  const char *pargfile = NULL;
  const char *searchstr = NULL;
  const char *parg = NULL;
  char entry[VSX_MAX_PATH_LEN];
  char fidxentry[VSX_MAX_PATH_LEN];
  size_t sz = 0;
  char *pbuf = NULL;
  int rc = 0;
  unsigned int lenbuf = 0;
  enum DIR_SORT sort = DIR_SORT_NONE;
  unsigned int startIdx = 0;
  unsigned int max = 0;
  SRV_PROPS_T srvProps;

  entry[0] = '\0';
  fidxentry[0] = '\0';

  //
  // If the media directory has not been set return cnt=-1
  //
  if(pConn->pCfg->pMediaDb->mediaDir == NULL) {
    snprintf(entry, sizeof(entry), "cnt=-1");
    if(http_resp_send(&pConn->sd, &pConn->httpReq,
       HTTP_STATUS_OK, (unsigned char *) entry, strlen(entry)) < 0) {
      return -1;
    }
    return 0;
  }

  if((pargfile = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, 
                                         "file"))) {
    parg = pargfile;
  } else if((pargdir = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, 
                                         "dir"))) {
    parg = pargdir;
    if(!strncmp(pargdir, "undefined", 9)) {
      parg = pargdir = "";
    }
  } else if(pargdir == NULL) {
    parg = pargdir = "";
  }

  if(pConn->pCfg->disable_root_dirlist && (parg == NULL || parg[0] == '\0')) {
    LOG(X_WARNING("Root directory listing disabled."), parg);
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      return -1;
    }
    rc = -1;
  } else if(!srv_ctrl_islegal_fpath(parg)) {
    LOG(X_WARNING("Directory listing contains illegal filepath '%s'"), parg);
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      return -1;
    }
    rc = -1;
  }

  mediadb_fixdirdelims((char *) parg, DIR_DELIMETER);

  if(rc == 0 && mediadb_prepend_dir(pConn->pCfg->pMediaDb->mediaDir, 
                                    parg, entry, sizeof(entry)) < 0) {
    rc = -1; 
  }
  if(rc == 0 && 
     mediadb_prepend_dir(pConn->pCfg->pMediaDb->dbDir, 
                         parg, fidxentry, sizeof(fidxentry)) < 0) {
    rc = -1; 
  }

  if(rc < 0) {
    LOG(X_ERROR("List media failed for '%s' %s, %s"), parg, pargfile ? pargfile : "", pargdir ? pargdir : ""); 
    if(http_resp_send(&pConn->sd, &pConn->httpReq, 
                             HTTP_STATUS_SERVERERROR, NULL, 0) < 0) {
      return -1;
    }
  }

  if(pargdir) {

    if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, "dirsort"))) {
      sort = atoi(parg);

      // Store the destination in the user config file
      if(pConn->pCfg->propFilePath) {
      
        if(srvprop_read(&srvProps, pConn->pCfg->propFilePath) == 0 && srvProps.dirsort != sort) {

          srvProps.dirsort = sort;
          srvprop_write(&srvProps, pConn->pCfg->propFilePath);
        }
      }

    }
    if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, 
                                         "start"))) {
      if((startIdx = atoi(parg)) < 0) {
        startIdx = 0;
      }
    }
    if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, 
                                         "max"))) {
      if((max = atoi(parg)) < 0) {
        max = 0;
      }
    }

    searchstr = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, 
                                         "search");

    //
    // Return a listing of the media contents of a directory
    //
    if((pbuf = listdir(pConn, entry, fidxentry, pargdir, 
                       &lenbuf, searchstr, startIdx, max, sort)) == NULL) {

    }

  } else if(pargfile) {

    fidxentry[mediadb_getdirlen(fidxentry)] = '\0';

    lenbuf = 1024;
    if(!(pbuf = (char *) avc_calloc(1, lenbuf))) {
      return -1;
    }

    //
    // Append any .mp4 , .sdp to the file path
    //
    srv_ctrl_file_find_wext(entry);

    sz = snprintf(pbuf, lenbuf, "file=%s&", pargfile);
    lenbuf -= sz;


    if((rc = check_metafile(pConn, entry, &pbuf[sz], &lenbuf)) > 0) {
      sz += rc;
      lenbuf -= sz;
    }

    listmediafileinfo(pConn, entry, fidxentry, &pbuf[sz], &lenbuf);

    lenbuf += sz;

  }

  if(http_resp_send(&pConn->sd, &pConn->httpReq, 
                           HTTP_STATUS_OK, (unsigned char *) pbuf, lenbuf) < 0) {
    if(pbuf) {
      free(pbuf);
    }
    return -1;
  }

  if(pbuf) {
    free(pbuf);
  }

  return 0;
}

#if 0
int srv_ctrl_prop(CLIENT_CONN_T *pConn) {

  char buf[512];
  unsigned int idx;
  unsigned int lenresp = 0;
  int rc;
  const char *parg;
  SRV_PROPS_T props;
  int haveprops = 0;
  int havexcoder = 0;
  int ipodmode = 0;

  havexcoder = (xcode_enabled(0) >= XCODE_CFG_OK);

  // Set User-Agent specific flag back to client javascript context
  if((parg = conf_find_keyval(pConn->httpReq.reqPairs, HTTP_HDR_USER_AGENT))) {

    if(strnstr(parg, "iPod", 48) || strnstr(parg, "iPhone", 48)) {
      // set to non-0, this val is appended to button image src
      ipodmode = 4;
    }
  }

  if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) pConn->httpReq.uriPairs, 
                                         "dstaddr"))) {
    if(pConn->pCfg->propFilePath) {
      memset(&props, 0, sizeof(props));
      haveprops = !srvprop_read(&props, pConn->pCfg->propFilePath);
    }

    if(haveprops) {
      for(idx = 0; idx < SRV_PROP_DEST_MAX; idx++) {

        if(props.dstaddr[idx] != '\0') {
          if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                              "dstaddr%d=%s&", idx + 1, props.dstaddr[idx])) > 0) {
            lenresp += rc;
          }
        }
  
      }

      if(lenresp == 0) {
        lenresp = snprintf(&buf[lenresp], sizeof(buf) - lenresp, "dstaddr1=%s:%d&", 
                        inet_ntoa(pConn->sd.sain.sin_addr), 5004);
      }

      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                          "capaddr=%s&", props.capaddr)) > 0) {
        lenresp += rc;
      }
      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                          "loop=%d&", props.loop)) > 0) {
        lenresp += rc;
      }
      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                          "loop_pl=%d&", props.loop_pl)) > 0) {
        lenresp += rc;
      }
      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                          "live=%d&", props.tslive)) > 0) {
        lenresp += rc;
      }
      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                          "httplive=%d&", props.httplive)) > 0) {
        lenresp += rc;
      }
      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp,
                          "loop=%d&", props.loop)) > 0) {
        lenresp += rc;
      }
      // xcode profile radio option
      if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp,
                          "xcodeprofile=%d&", props.xcodeprofile)) > 0) {
        lenresp += rc;
      }


    } // haveprops

    // xcode profile radio option
    if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp,
                        "chchgr=%d&", pConn->pCfg->pchannelchgr ? 1 : 0)) > 0) {
      lenresp += rc;
    }


  } 

  if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                      "dirsort=%d&", props.dirsort)) > 0) {
   lenresp += rc;
  }

  if((rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                      "xcode=%d&", havexcoder)) > 0) {
    lenresp += rc;
  }

  if(ipodmode && (rc = snprintf(&buf[lenresp], sizeof(buf) - lenresp, 
                      "ipod=%d&", ipodmode)) > 0) {
    lenresp += rc;
  }

  if(http_resp_send(&pConn->sd, &pConn->httpReq,
                    HTTP_STATUS_OK,  (unsigned char *) buf, lenresp) < 0) {
    //LOG(X_ERROR("Failed to send %d bytes back to client"), 0);
    return -1;
  }

  return 0;
}
#endif // 0

int srv_ctrl_initmediafile(HTTP_MEDIA_STREAM_T *pMediaStream, int introspect) {
  int rc = 0;
  //unsigned int len;
  const char *pext = NULL;

  if(!pMediaStream || !pMediaStream->pFileStream) {
    return -1;
  }

  if(filetype_getdescr(pMediaStream->pFileStream, &pMediaStream->media, introspect) != 0) {
    return -1;
  }

  //LOG(X_DEBUG("CTRL_INITMEDIAFILE - '%s', media.type: %d, introspect: %d, haveVid: %d, haveAud: %d"), pMediaStream->pFileStream->filename,  pMediaStream->media.type, introspect, pMediaStream->media.haveVid, pMediaStream->media.haveAud);

  if(!introspect) {
    pMediaStream->media.totSz = pMediaStream->pFileStream->size;
  }

  pMediaStream->pContentType = NULL;

  pext = strutil_getFileExtension(pMediaStream->pFileStream->filename);
/*
  if((len = strlen(pMediaStream->pFileStream->filename)) > 3) {
    pext = &pMediaStream->pFileStream->filename[len - 1];  
    while(pext > pMediaStream->pFileStream->filename + 1 && *(pext - 1) != '.') {
      pext--;
    }
  }
*/

  switch(pMediaStream->media.type) {

    case MEDIA_FILE_TYPE_MP4:

      //
      // iPod seems to blindy look at file extension to match w/ content type
      // regardless if file contains both aud / vid
      //
      if(pext) {
        if(!strncasecmp(pext, "m4a", 3)) {
          pMediaStream->pContentType = CONTENT_TYPE_XM4A;
        } else if(!strncasecmp(pext, "m4v", 3)) {
          pMediaStream->pContentType = CONTENT_TYPE_XM4V;
        }
      } 


      if(!pMediaStream->pContentType) {
        //
        // If introspect is turned off, we did not look into the contents of the file to discover whether 
        // it has both audio & video, so don't return track specific type
        //
        if(introspect && !pMediaStream->media.haveVid) {
          pMediaStream->pContentType = CONTENT_TYPE_XM4A;
        } else if(introspect && !pMediaStream->media.haveAud) {
          pMediaStream->pContentType = CONTENT_TYPE_XM4V;
        } else {
          pMediaStream->pContentType = CONTENT_TYPE_MP4;
        }
      }

      break;

    case MEDIA_FILE_TYPE_FLV:
      pMediaStream->pContentType = CONTENT_TYPE_FLV;
      break;

    case MEDIA_FILE_TYPE_MKV:
      pMediaStream->pContentType = CONTENT_TYPE_MATROSKA;
      break;

    case MEDIA_FILE_TYPE_WEBM:
      pMediaStream->pContentType = CONTENT_TYPE_WEBM;
      break;

    case MEDIA_FILE_TYPE_MP2TS:
    case MEDIA_FILE_TYPE_MP2TS_BDAV:
      if(pext) {
        if(!strncasecmp(pext, "ts", 2)) {
          pMediaStream->pContentType = CONTENT_TYPE_TS;
        } else {
          pMediaStream->pContentType = CONTENT_TYPE_MP2TS;
        }
      }
      break;

    case MEDIA_FILE_TYPE_H264b:
      pMediaStream->pContentType = CONTENT_TYPE_H264;
      break;

    case MEDIA_FILE_TYPE_AAC_ADTS:
      pMediaStream->pContentType = CONTENT_TYPE_AAC;
      break;

    case MEDIA_FILE_TYPE_M3U:
      pMediaStream->media.totSz = pMediaStream->pFileStream->size;
      if(pext) {
        if(!strncasecmp(pext, "m3u8", 4)) {
          pMediaStream->pContentType = CONTENT_TYPE_M3U8;
        } else {
          pMediaStream->pContentType = CONTENT_TYPE_TEXTPLAIN;
        }
      }
      break;

    default:
      LOG(X_ERROR("Unsupported file type %s in request"), pMediaStream->pFileStream->filename);
      rc = -1; 
  }

  return rc;
}

static int srv_ctrl_is_uri_virtual(const char *puri) {

  //fprintf(stderr, "CHECK URI '%s'\n", puri);

  if(!strncasecmp(puri, VSX_RTSP_URL, strlen(VSX_RTSP_URL)) ||
     !strncasecmp(puri, VSX_RTMPT_URL, strlen(VSX_RTMPT_URL)) ||
     !strncasecmp(puri, VSX_RTMP_URL, strlen(VSX_RTMP_URL)) ||
     !strncasecmp(puri, VSX_HTTPLIVE_URL, strlen(VSX_HTTPLIVE_URL)) ||
     !strncasecmp(puri, VSX_DASH_URL, strlen(VSX_DASH_URL)) ||
     !strncasecmp(puri, VSX_MEDIA_URL, strlen(VSX_MEDIA_URL)) ||
     !strncasecmp(puri, VSX_FLV_URL, strlen(VSX_FLV_URL)) ||
     !strncasecmp(puri, VSX_MKV_URL, strlen(VSX_MKV_URL)) ||
     !strncasecmp(puri, VSX_WEBM_URL, strlen(VSX_WEBM_URL)) ||
     !strncasecmp(puri, VSX_LIVE_URL, strlen(VSX_LIVE_URL)) ||
     !strncasecmp(puri, VSX_TMN_URL, strlen(VSX_TMN_URL)) 
     //!strncasecmp(puri, VSX_LIST_URL, strlen(VSX_LIST_URL)) ||
     //!strncasecmp(puri, VSX_HTTP_URL, strlen(VSX_HTTP_URL))
    ) {
    return 1;
  }

  return 0;
}


const char *srv_ctrl_mediapath_fromuri(const char *puri, const char *mediaDir, char pathbuf[]) {
  const char *p;

  if(!puri) {
    return NULL;  
  }

  p = puri;

  while(*p == '/' && *p != '\0') {
    p++;
  }

  if(strchr(p, '/')) {

    if(srv_ctrl_is_uri_virtual(puri)) {

      while(*p != '/' && *p != '\0') {
        p++;
      }
      while(*p == '/' && *p != '\0') {
        p++;
      }

    }

  }

  if(*p == '\0') {
    return NULL;
  }
  //fprintf(stderr, "FROM URI '%s'\n", p);
  //http_urldecode(p, buf, sizeof(buf))

  mediadb_fixdirdelims((char *) p, DIR_DELIMETER);
  if(!srv_ctrl_islegal_fpath(p)) {
    return NULL;
  }

  if(mediadb_prepend_dir(mediaDir, p, pathbuf, VSX_MAX_PATH_LEN) < 0) {
    return NULL;
  }

  return p;
}

//
// This is currently used by vsxmgr
//
int srv_ctrl_loadmedia(CLIENT_CONN_T *pConn, const char *pargfile) {
  int rc = 0;
  //const char *pargfile = NULL;
  const char *parg = NULL;
  char pathbuf[VSX_MAX_PATH_LEN];
  char *path = pathbuf;
  FILE_STREAM_T fileStream;
  HTTP_MEDIA_STREAM_T mediaStream;
  int throttle = 1;

  if(pargfile) {
    path = (char *) pargfile;
  } else {
    if(!(pargfile = srv_ctrl_mediapath_fromuri(pConn->httpReq.puri, pConn->pCfg->pMediaDb->mediaDir, pathbuf))) {
      rc = -1;
    }
  }

  memset(&fileStream, 0, sizeof(fileStream));
  if(rc == 0 && OpenMediaReadOnly(&fileStream, path) < 0) {
    rc = -1;
  }

  if(rc != 0) {
    rc = snprintf(pathbuf, sizeof(pathbuf), "File not found");
    return http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 
                          (unsigned char *) pathbuf, rc);
  }

  memset(&mediaStream, 0, sizeof(mediaStream));
  mediaStream.pFileStream = &fileStream;

  if(srv_ctrl_initmediafile(&mediaStream, 1) < 0) {
    CloseMediaFile(&fileStream);
    rc = snprintf(pathbuf, sizeof(pathbuf), "Invalid media file type");
    return http_resp_send(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 
                          (unsigned char *) pathbuf, rc);
  }

  if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, "throttle")) &&
     !strncasecmp(parg, "off", 3)) {
    LOG(X_WARNING("Throttle turned off for '%s'"), pargfile);
    throttle = 0;
  }

  rc = http_resp_sendmediafile(pConn, NULL, &mediaStream, 
                               pConn->pCfg->throttlerate, pConn->pCfg->throttleprebuf);

  CloseMediaFile(&fileStream);

  return rc;
}

//
// This is currently used by vsxmgr
//
int srv_ctrl_loadimg(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus) {
  size_t sz, sz2;
  char path[VSX_MAX_PATH_LEN];
  int rc = 0;
  const char *contentType = CONTENT_TYPE_TEXTHTML;

  sz = strlen(VSX_IMG_URL);
  while(pConn->httpReq.puri[sz] == '/') {
    sz++;
  }

  mediadb_prepend_dir(pConn->pCfg->pMediaDb->homeDir, VSX_IMG_PATH, 
                      path, sizeof(path));
  sz = strlen(path);

  sz2 = strlen(VSX_IMG_URL);
  path[sizeof(path) - 1] = '\0';
  strncpy(&path[sz], &pConn->httpReq.puri[sz2], sizeof(path) - sz - 1);
  mediadb_fixdirdelims(path, DIR_DELIMETER);

  if(strstr(path, ".gif")) {
    contentType = CONTENT_TYPE_IMAGEGIF;
  } else {
    contentType = CONTENT_TYPE_IMAGEPNG;
  }

  if((rc = http_resp_sendfile(pConn, pHttpStatus, path, contentType, HTTP_ETAG_AUTO)) < 0) {
    *pHttpStatus = HTTP_STATUS_SERVERERROR;
  }

  return rc; 
}

//
// This is currently used by vsxmgr
//
int srv_ctrl_loadtmn(CLIENT_CONN_T *pConn) {
  size_t sz, sz2;
  char path[VSX_MAX_PATH_LEN];
  const char *contentType = CONTENT_TYPE_TEXTHTML;

  sz = strlen(VSX_TMN_URL);
  while(pConn->httpReq.puri[sz] == '/') {
    sz++;
  }

  sz = strlen(path);

  sz2 = strlen(VSX_TMN_URL);
  mediadb_prepend_dir(pConn->pCfg->pMediaDb->dbDir, &pConn->httpReq.puri[sz2], 
                      path, sizeof(path));
  mediadb_fixdirdelims(path, DIR_DELIMETER);

  if(strstr(path, ".jpg")) {
    contentType = CONTENT_TYPE_IMAGEJPG;
  } else {
    contentType = CONTENT_TYPE_IMAGEPNG;
 }
  
  return http_resp_sendfile(pConn, NULL, path, contentType, HTTP_ETAG_AUTO);
}


int srv_ctrl_submitdata(CLIENT_CONN_T *pConn) {

  FILE_HANDLE fp;
  char path[VSX_MAX_PATH_LEN];
  int rc = 0;
  unsigned char buf[1024];
  unsigned int idxRead = pConn->httpReq.postData.idxRead;
  unsigned int lenRead = pConn->httpReq.postData.lenInBuf - pConn->httpReq.postData.idxRead;
  unsigned int lenTot = pConn->httpReq.postData.contentLen + pConn->httpReq.postData.idxRead;
  unsigned int idxBuf = idxRead;

  if(pConn->httpReq.postData.filename[0] == '\0') {
    LOG(X_ERROR("No filename supplied in form data"));
    return -1;
  }
  
  mediadb_prepend_dir(pConn->pCfg->pMediaDb->mediaDir, pConn->httpReq.postData.filename, 
                      path, sizeof(path)); 

  if((fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for writing: %s"), path);
    return -1;
  }

  fprintf(stdout, "post submitdata got %d / %d , tot:%d\n", pConn->httpReq.postData.idxRead, pConn->httpReq.postData.lenInBuf, pConn->httpReq.postData.contentLen);

  while(idxRead < lenTot) {

    if(idxBuf == 0) {

      if((lenRead = (lenTot - idxRead)) > sizeof(buf)) {
        lenRead = sizeof(buf);
      }

      if((rc = netio_recv(&pConn->sd.netsocket, NULL, buf, lenRead)) < 0) {
        LOG(X_WARNING("recv form data failed with "ERRNO_FMT_STR), ERRNO_FMT_ARGS);
        break;
      } else if(rc == 0) {
        // client disconnected
        LOG(X_WARNING("recv form data - client disconnected"));
        break;
      }

    }
    
//fprintf(stdout, "write %d\n", lenRead);
    if(fileops_WriteBinary(fp, (unsigned char *) &buf[idxBuf], lenRead) != lenRead) {
      LOG(X_ERROR("Failed to write %d to %s"), lenRead, path);
      break;
    }
    idxBuf = 0;
    idxRead += lenRead;
  }

  fileops_Close(fp);

  return rc;
}

