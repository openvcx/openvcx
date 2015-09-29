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

#include "mgr/srvmgr.h"
#include "mgr/srvproxy.h"
#include "mgr/procdb.h"


#define HTTP_STATUS_STR_FORBIDDEN_LICENSE " 403 - Forbidden.  Your streaming time has expired.  Please upgrade your license." 


const char *srvmgr_action_tostr(MEDIA_ACTION_T action) {
  switch(action) {
    case MEDIA_ACTION_LIVE_STREAM:
      return "live_stream";
    case MEDIA_ACTION_CANNED_DIRECT:
      return "canned_direct";
    case MEDIA_ACTION_CANNED_INFLASH:
      return "canned_inflash";
    case MEDIA_ACTION_CANNED_WEBM:
      return "canned_webm";
    case MEDIA_ACTION_CANNED_PROCESS:
      return "canned_process";
    case MEDIA_ACTION_CANNED_HTML:
      return "canned_html";
    case MEDIA_ACTION_TMN:
      return "thumbnail";
    case MEDIA_ACTION_IMG:
      return "image";
    case MEDIA_ACTION_LIST:
      return "list";
    case MEDIA_ACTION_INDEX_FILELIST:
      return "index_filelist";
    case MEDIA_ACTION_CHECK_TYPE:
      return "check_type";
    case MEDIA_ACTION_CHECK_EXISTS:
      return "check_exists";
    case MEDIA_ACTION_NONE:
      return "none";
    default:
      return "unknown";
  }
}

static int is_shared_livemethod(const SRVMEDIA_RSRC_T *pMediaRsrc) {
  int rc = 0;

  //
  // Return 1 if the resource is using only share-able methods and no dedicated methods
  //
  if(pMediaRsrc->pmethodBits && 
      ((*pMediaRsrc->pmethodBits & ~((1 << STREAM_METHOD_HTTPLIVE) | (1 << STREAM_METHOD_DASH))) == 0)) {
    return 1;
  }

  return rc;
}

static int is_shared_resource(const SRVMEDIA_RSRC_T *pMediaRsrc,  MEDIA_ACTION_T action) {
  int shared = 1;

  //
  // Check if the child process stream processor can be shared among clients
  //
  if(pMediaRsrc->pshared && (*pMediaRsrc->pshared == 0 ||
//    !(*pMediaRsrc->pshared == -1 && action == MEDIA_ACTION_LIVE_STREAM && !is_shared_livemethod(pMediaRsrc))) {
    (*pMediaRsrc->pshared == -1 && action != MEDIA_ACTION_LIVE_STREAM))) {
    shared = 0;
  }

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - is_shared_resource '%s': pshared: 0x%x, *pshared: %d, returning %d"), 
                     pMediaRsrc->virtRsrc, pMediaRsrc->pshared, *pMediaRsrc->pshared, shared));

  return shared;
}

static int is_canned_process(const SRVMEDIA_RSRC_T *pMediaRsrc) {
  int rc = 0;

  if((pMediaRsrc->pXcodeStr && pMediaRsrc->pXcodeStr[0] != '\0') ||
     (pMediaRsrc->pmethodBits && *pMediaRsrc->pmethodBits & ~(1 << STREAM_METHOD_PROGDOWNLOAD))) {
    return 1;
  }

  return rc;
}

static int check_license(SRV_MGR_CONN_T *pConn, int sendresp) {
#if defined(VSX_HAVE_LICENSE)

  TIME_VAL tmnow = timer_GetTime();

 //fprintf(stderr, "CHECK_LICENSE FOR: '%s'\n", pConn->conn.httpReq.puri);


  //
  // Check if stream time is limited
  //
  if(!(pConn->cfg.plic->capabilities & LIC_CAP_STREAM_TIME_UNLIMITED)) {
    //fprintf(stderr, "NOW:%lld ST:%lld DLTA:%lld\n", tmnow, *pConn->cfg.ptmstart, (STREAM_LIMIT_SEC * TIME_VAL_US));
    if(tmnow > *pConn->cfg.ptmstart + (STREAM_LIMIT_SEC * TIME_VAL_US)) {
      LOG(X_INFO("Stream time limited.  Please upgrade your license."), 
               (int) (tmnow - *pConn->cfg.ptmstart) / TIME_VAL_US);

      if(sendresp) {
        http_resp_send(&pConn->conn.sd, &pConn->conn.httpReq, HTTP_STATUS_FORBIDDEN,
            (unsigned char *) HTTP_STATUS_STR_FORBIDDEN_LICENSE, 
            strlen(HTTP_STATUS_STR_FORBIDDEN_LICENSE));
      }

      return 0;
    }
  }

#endif // VSX_HAVE_LICENSE

  return 1;
}

static int parse_media_uri(const CLIENT_CONN_T *pConn, SRVMEDIA_RSRC_T *pRsrc) {

  size_t sz;
  size_t szprev, sztmp;
  char *p;
  char *pRsrcName = NULL;
  size_t szrsrcname;
  char uri[HTTP_URL_LEN];
  char tmp[HTTP_URL_LEN];
  int useRsrcExt = 0;

  //
  // The media request URI takes the form:
  // < > - Optional element
  // [ ] - Mandatory element
  //
  // /[streaming method]</[Resource leading path]></id_[unique id]>/[Resource name]</[Extended Resource Name]</prof_[Profile Designation]>
  //
  // [streaming method] - Eg., /httplive, /flv, /mkv, /rtmp
  // [Resource leading path] - Eg., /sub-dir/path-to/
  // [unique id] - Dedicated Stream Processor id Eg., id_af415093
  // [Resource name] - Eg., live.sdp, in.mp4
  // [Extended Resource name] - Eg., out.ts, multi.m3u8 
  // [Profile Designation] - Eg., prof_360p
  //

  if((sz = strlen(pConn->httpReq.puri)) >= sizeof(uri)) {
    sz = sizeof(uri) - 1;
  }
  memcpy(uri, pConn->httpReq.puri, sz + 1);

  //
  // Check for and strip any '/prof_[ profile name ]' profile quality indicator URI extension at the end
  //
  while(sz > 0 && uri[sz] != '/') {
    sz--;
  }

  if(!strncasecmp(&uri[sz], "/"VSX_URI_PROFILE_PARAM, 1 + strlen(VSX_URI_PROFILE_PARAM))) {
    strncpy(pRsrc->profile, &uri[sz + 6], sizeof(uri) - 1);
    uri[sz] = '\0';
  }

  //
  // Get the media resource name without any leading path
  //
  while(sz> 0 && uri[sz] != '/') {
    sz--;
  }
  szprev = sz;

  pRsrcName = &uri[sz];
  while(*pRsrcName == '/') {
    pRsrcName++;
  }
  szrsrcname = strlen(pRsrcName);
  
  //
  // Check if we should be checking for an 'rsrcExt' on the URI such as for /httplive or /dash if
  // the request should only be for a known media file extension or playlist file extension 
  //
  if(!strncasecmp(uri, VSX_HTTPLIVE_URL, strlen(VSX_HTTPLIVE_URL)) &&
      ((szrsrcname > (sztmp = strlen(HTTPLIVE_PL_NAME_EXT)) &&
       !strncasecmp(&pRsrcName[szrsrcname - sztmp], HTTPLIVE_PL_NAME_EXT, sztmp)) ||
       ((szrsrcname > (sztmp = 1 + strlen(HTTPLIVE_TS_NAME_EXT)) &&
      !strncasecmp(&pRsrcName[szrsrcname - sztmp], "."HTTPLIVE_TS_NAME_EXT, sztmp))))) {
    useRsrcExt = 1;
  } else 
  if(!strncasecmp(uri, VSX_DASH_URL, strlen(VSX_DASH_URL)) &&
      ((szrsrcname > (sztmp = strlen(DASH_MPD_EXT)) &&
       !strncasecmp(&pRsrcName[szrsrcname - sztmp], DASH_MPD_EXT, sztmp)) ||
       ((szrsrcname > (sztmp = 1 + strlen(DASH_DEFAULT_SUFFIX_M4S)) &&
      !strncasecmp(&pRsrcName[szrsrcname - sztmp], "."DASH_DEFAULT_SUFFIX_M4S, sztmp))))) {
    useRsrcExt = 1;
  }
  
  if(useRsrcExt) {

    strncpy(pRsrc->rsrcExt, pRsrcName, sizeof(pRsrc->rsrcExt));
    uri[sz] = '\0';
    while(sz> 0 && uri[sz] != '/') {
      sz--;
    }
    szprev = sz;
    pRsrcName = &uri[sz];
    while(*pRsrcName == '/') {
      pRsrcName++;
    }
    szrsrcname = strlen(pRsrcName);

  }

  //
  // Check for any '/id_[ instance id ]' streaming processor instance id and remove it from the uri
  //
  sztmp = sz;
  while(sztmp > 0 && uri[sztmp] == '/') {
    sztmp--;
  }
  while(sztmp > 0 && uri[sztmp] != '/') {
    sztmp--;
  }

  if(!strncasecmp(&uri[sztmp], "/"VSX_URI_ID_PARAM, 1 + strlen(VSX_URI_ID_PARAM)) && szprev >= 
      sztmp + 1 + strlen(VSX_URI_ID_PARAM)) {
    if((sz = (szprev - sztmp - 4)) >= sizeof(pRsrc->instanceId)) {
      sz = sizeof(pRsrc->instanceId) - 1;
    }
    memcpy(pRsrc->instanceId, &uri[sztmp + 4], sz);

    strncpy(tmp, pRsrcName, sizeof(tmp) - 1);
    strncpy(&uri[sztmp + 1], tmp, sizeof(uri) - sztmp -1);
    pRsrcName = &uri[sztmp + 1];
  }
    
  //
  // Get the filepath based on the media resource path in the URI
  //
  if(!(p = (char *) srv_ctrl_mediapath_fromuri(uri, pConn->pCfg->pMediaDb->mediaDir, pRsrc->filepath)) ||
     p[0] == '\0') {
    return -1;
  }

  strncpy(pRsrc->virtRsrc, p, sizeof(pRsrc->virtRsrc) - 1);
  sz = strlen(pRsrc->virtRsrc);
  if(szrsrcname > sz) {
    szrsrcname = sz;
  }
  pRsrc->pRsrcName = &pRsrc->virtRsrc[sz - szrsrcname];
  pRsrc->szvirtRsrcOnly = sz - szrsrcname;
  while(pRsrc->szvirtRsrcOnly > 0 && pRsrc->virtRsrc[pRsrc->szvirtRsrcOnly - 1] == '/') {
    pRsrc->szvirtRsrcOnly--;
  }

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - parse_media_uri: uri: '%s', virtRsrc: '%s', filepath: '%s', profile: '%s'"
                 ", instanceId: '%s', name: '%s', ext: '%s', len-virtrsrconly:%d"), 
                  pConn->httpReq.puri, pRsrc->virtRsrc, pRsrc->filepath, pRsrc->profile, 
                  pRsrc->instanceId, pRsrc->pRsrcName, pRsrc->rsrcExt, pRsrc->szvirtRsrcOnly));

  return 0;

}

int srvmgr_check_metafile(META_FILE_T *pMetaFile, 
                         SRVMEDIA_RSRC_T *pMediaRsrc, 
                         const char *devname, 
                         HTTP_STATUS_T *phttpStatus) {
  int rc = 0;
  int sz;
  struct stat st;
  char pathbuf[VSX_MAX_PATH_LEN];
  int loaddefault = 1;
  char *path = NULL;

  if(!pMetaFile || !pMediaRsrc) {
    return -1;
  }

  if(!(path = metafile_find(pMediaRsrc->filepath, pathbuf, sizeof(pathbuf), &loaddefault))) {
    return -1;
  }

  //
  // Set the metafile device and profile type input filters to extract the 
  // appropriate xcodestr
  //
  if(devname && devname[0] != '\0') {
    strncpy(pMetaFile->devicefilterstr, devname, sizeof(pMetaFile->devicefilterstr));
  }  else {
    pMetaFile->devicefilterstr[0] = '\0';
  }
  if(pMediaRsrc->profile[0] != '\0') {
    strncpy(pMetaFile->profilefilterstr, pMediaRsrc->profile, sizeof(pMetaFile->profilefilterstr));
  } else {
    pMetaFile->profilefilterstr[0] = '\0';
  }

  VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - srvmgr_check_metafile metafile: '%s' for resource: '%s', "
                             "meta-devicefilter: '%s', meta-profilefilter: '%s'"), 
                    pathbuf, pMediaRsrc->filepath, pMetaFile->devicefilterstr, pMetaFile->profilefilterstr) );

  if(metafile_open(path, pMetaFile, 0, 0) < 0) {
    return -1;
  } else if(!loaddefault) {

    //
    // If the requested resource is actually a metafile and it is missing the 
    // 'file=' argument, attempt to strip the .meta and find the accompanying media file 
    //
    if(pMetaFile->filestr[0] == '\0' && !strcmp(path, pMediaRsrc->filepath)) {
      strncpy(pMetaFile->filestr, pMediaRsrc->filepath, sizeof(pMetaFile->filestr));
      if((sz = strlen(pMetaFile->filestr)) > strlen(METAFILE_EXT)) {
        sz -= strlen(METAFILE_EXT);
        if(fileops_stat(pMetaFile->filestr, &st) != 0) {
          pMediaRsrc->filepath[sz] = '\0';
        }
      }
    }

    if(pMetaFile->filestr[0] != '\0') {

      if(fileops_stat(pMetaFile->filestr, &st) != 0) {
        LOG(X_ERROR("Metafile '%s' file entry '%s' does not exist"), path, pMetaFile->filestr);
        if(phttpStatus) {
          *phttpStatus = HTTP_STATUS_NOTFOUND;
        }
        return -1;
      }

      //if(path != pathdefault) {
       strncpy(pMediaRsrc->filepath, pMetaFile->filestr, sizeof(pMediaRsrc->filepath));
      //}

    } else if(!strcmp(path, pMediaRsrc->filepath)) {
      LOG(X_ERROR("Requested Metafile resource %s does not contain file location"), 
                  pMediaRsrc->filepath);
      return -1;
    }

  }

  if(pMetaFile->xcodestr[0] != '\0') {
    pMediaRsrc->pXcodeStr = pMetaFile->xcodestr;
  }

  if(pMetaFile->userpass[0] != '\0') {
    pMediaRsrc->pUserpass = pMetaFile->userpass;
  }

  pMediaRsrc->pmethodBits = &pMetaFile->methodBits;
  pMediaRsrc->pshared = &pMetaFile->shared;

  if(pMetaFile->linkstr[0] != '\0') {
    pMediaRsrc->pLinkStr = pMetaFile->linkstr;
  }
  if(pMetaFile->instr[0] != '\0') {
    pMediaRsrc->pInputStr = pMetaFile->instr;
  }

  if(pMetaFile->id[0] != '\0') {
    pMediaRsrc->pId = pMetaFile->id;
  }

  VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - srvmgr_check_metafile done xcodestr:'%s', file:'%s' ('%s'), id:'%s',"
                            " link:'%s', in:'%s', digestauth: '%s', methods: '%s', shared: %d"), 
                  pMediaRsrc->pXcodeStr, pMediaRsrc->filepath, pMetaFile->filestr, 
                  pMediaRsrc->pId, pMediaRsrc->pLinkStr, pMediaRsrc->pInputStr, pMediaRsrc->pUserpass,
                  devtype_dump_methods(pMetaFile->methodBits, pathbuf, sizeof(pathbuf)), pMetaFile->shared));


  return rc;
}

static int check_mediafile(const SRVMEDIA_RSRC_T *pRsrc,
                           MEDIA_DESCRIPTION_T *pMediaDescr,
                           int introspect) {

  int rc = 0;
  FILE_STREAM_T fileStream;

  if(OpenMediaReadOnly(&fileStream, pRsrc->filepath) != 0) {
    return -1;
  }
 
  if(filetype_getdescr(&fileStream, pMediaDescr, introspect) < 0) {
    rc = -1;
  }

  if(rc == 0 && introspect && 
     (pMediaDescr->vid.generic.resH == 0  ||  pMediaDescr->vid.generic.resV == 0)) {

  }

  CloseMediaFile(&fileStream);

  return rc;
}

static int get_xcode_val_int(const char *xcodestr, const char *needle) {
  char tmp[16];
  const char *p = NULL;
  const char *p2;
  unsigned int sz;
  int val;

  while(!p) {

    if((p = strstr(xcodestr, needle))) {

      if(p > xcodestr) {
        //
        // Ensure that we dont' match "abc[needle]=val"
        //
        if(p[-1] != ' ' && p[-1] != '\t' &&  p[-1] != '"' &&  p[-1] != ',') {
          xcodestr = &p[2];
          p = NULL;
          continue;
        }
      }

      p += 2;
    } else {
      return -1;
    }

  }

  p2 = p;
  while(*p2 >= '0' && *p2 <= '9') {
    p2++;
  }

  sz = MIN(p2 - p, sizeof(tmp) - 1);
  memcpy(tmp, p, sz);
  tmp[sz] = '\0';
  val = atoi(tmp);

  return val;
}

static int check_mediaproperties(const SRVMEDIA_RSRC_T *pMediaRsrc,
                                 MEDIA_DESCRIPTION_T *pMediaDescr) {
  int rc = 0;
  int val;
  int width = 0;
  int height = 0;
  float aspectr = 0.0f;

  if(check_mediafile(pMediaRsrc, pMediaDescr, 1) == 0) { 
    rc = 1;

    //if(mediaDescr.haveVid) {
     //fprintf(stderr, "PROFILE:%d %d %s %dx%d\n", pMediaDescr->vid.generic.profile, pMediaDescr->vid.generic.mediaType, codecType_getCodecDescrStr(pMediaDescr->vid.generic.mediaType), pMediaDescr->vid.generic.resH, pMediaDescr->vid.generic.resV);
    //}
//fprintf(stderr, "mediatype:%s\n", codecType_getCodecDescrStr(mediaDescr.type));

  }

  //TODO: fill in resH, resV for sdp / live content

  //
  // Check any dimensions specified in the xcodestr
  //
  if(pMediaRsrc->pXcodeStr) {

    if((val = get_xcode_val_int(pMediaRsrc->pXcodeStr, "x=")) > 0 ||
       (val = get_xcode_val_int(pMediaRsrc->pXcodeStr, "videoWidth=")) > 0 ||
       (val = get_xcode_val_int(pMediaRsrc->pXcodeStr, "vx=")) > 0) {
      width = val;
    }
    if((val = get_xcode_val_int(pMediaRsrc->pXcodeStr, "y=")) > 0 ||
       (val = get_xcode_val_int(pMediaRsrc->pXcodeStr, "videoHeight=")) > 0 ||
       (val = get_xcode_val_int(pMediaRsrc->pXcodeStr, "vy=")) > 0) {
      height = val;
    }

    if((!width || !height) && pMediaDescr->vid.generic.resH > 0 && pMediaDescr->vid.generic.resV > 0) {
      aspectr = (float) pMediaDescr->vid.generic.resH / pMediaDescr->vid.generic.resV; 
      if(!width) {
        width = (int) (aspectr * (float) height); 
      }
      if(!height) {
        height = (int) ((float) width / aspectr); 
      }

    }

    if(width > 0 && height > 0) {
      pMediaDescr->vid.generic.resH = width;
      pMediaDescr->vid.generic.resV = height;
    }

  }

  //fprintf(stderr, "XCODE_STR:'%s' %d x %d\n", pMediaRsrc->pXcodeStr, pMediaDescr->vid.generic.resH, pMediaDescr->vid.generic.resV);

  return rc;
}

static MEDIA_ACTION_T get_media_action(const MEDIA_DESCRIPTION_T *pMediaDescr,
                                       const SRVMEDIA_RSRC_T *pMediaRsrc,
                                       CLIENT_CONN_T *pConn,
                                       const STREAM_DEVICE_T *pdevtype,
                                       STREAM_METHOD_T reqMethod,
                                       STREAM_METHOD_T methodAuto) {

  struct stat st;
  char path[VSX_MAX_PATH_LEN];
  MEDIA_ACTION_T action = MEDIA_ACTION_UNKNOWN;

  //
  // Check if the requested virtual resource actually corresponds to a download
  // file, such as a .swf, css, .jpg, 
  //
  if(methodAuto == STREAM_METHOD_RTMP || 
     methodAuto == STREAM_METHOD_FLVLIVE ||
     methodAuto == STREAM_METHOD_MKVLIVE ||
     methodAuto == STREAM_METHOD_RTSP ||
     methodAuto == STREAM_METHOD_RTSP_INTERLEAVED ||
     methodAuto == STREAM_METHOD_RTSP_HTTP) {

    if(is_canned_process(pMediaRsrc)) {

      //
      // If the resource is to be handled by a dedicated child stream processor 
      //
      return MEDIA_ACTION_CANNED_PROCESS;
     
    //
    // Construct a possible path of a resource in html/rsrc/
    //
    } else if(mediadb_prepend_dir2(pConn->pCfg->pMediaDb->homeDir,
          VSX_RSRC_HTML_PATH, pMediaRsrc->virtRsrc, (char *) path, sizeof(path)) < 0 ||
       !srv_ctrl_islegal_fpath(path)) {

      return MEDIA_ACTION_NONE;

    } else if(fileops_stat(path, &st) == 0) {

      //
      // If the request is for a resource which exists in html/rsrc
      //
      return MEDIA_ACTION_CANNED_HTML;
    }

  } else if(methodAuto == STREAM_METHOD_FLASHHTTP) {
    //TODO: what if the resource is supposed to be transcoded...
    return MEDIA_ACTION_CANNED_INFLASH;
  }


  if(fileops_stat(pMediaRsrc->filepath, &st) < 0) {
    action = MEDIA_ACTION_NONE;
  } else if(pMediaDescr->type == MEDIA_FILE_TYPE_SDP) {
    action = MEDIA_ACTION_LIVE_STREAM;
  } else if(reqMethod == STREAM_METHOD_PROGDOWNLOAD) {
    action = MEDIA_ACTION_CANNED_DIRECT;

  } else if(pMediaDescr->type == MEDIA_FILE_TYPE_MP4) {

    //
    // mp4 / 3gp / 3gpp / 3gp+
    //
    if(methodAuto == STREAM_METHOD_RTMP || methodAuto == STREAM_METHOD_FLVLIVE) {
      action = MEDIA_ACTION_CANNED_INFLASH;
    } else if(methodAuto == STREAM_METHOD_MKVLIVE) {
      action = MEDIA_ACTION_CANNED_WEBM;
    } else {
      action = MEDIA_ACTION_CANNED_DIRECT;
    }

  } else if(pMediaDescr->type == MEDIA_FILE_TYPE_FLV) {

    if(methodAuto == STREAM_METHOD_RTMP || methodAuto == STREAM_METHOD_FLVLIVE) {
      action = MEDIA_ACTION_CANNED_INFLASH;

    } else if(pdevtype->methods[0] != STREAM_METHOD_HTTPLIVE &&
              pdevtype->methods[0] != STREAM_METHOD_RTSP &&
              pdevtype->methods[0] != STREAM_METHOD_RTSP_INTERLEAVED &&
              pdevtype->methods[0] != STREAM_METHOD_RTSP_HTTP) {
      action = MEDIA_ACTION_CANNED_DIRECT;
    }

  } else if(pMediaDescr->type == MEDIA_FILE_TYPE_MKV || pMediaDescr->type == MEDIA_FILE_TYPE_WEBM) {

    action = MEDIA_ACTION_CANNED_WEBM;
  }

  if(action == MEDIA_ACTION_UNKNOWN) {

    action = MEDIA_ACTION_CANNED_PROCESS;
  }

  return action;
}

static int process_liveoverhttp(SRV_MGR_CONN_T *pMgrConn, 
                                const SRVMEDIA_RSRC_T *pMediaRsrc, 
                                const SYS_PROC_T *pProc, 
                                MEDIA_ACTION_T action, 
                                STREAM_METHOD_T method) {
  int rc = 0;
  int sz, sz2;
  SRV_CFG_T cfg;
  STREAMER_CFG_T streamerCfg;
  CLIENT_CONN_T *pConn = &pMgrConn->conn;
  SRV_CFG_T *pCfgOrig = pConn->pCfg;
  char rsrcurl[VSX_MAX_PATH_LEN];
  int is_remoteargfile = 0;
  const char *pvirtRsrc = pMediaRsrc->virtRsrc;
  MEDIA_DESCRIPTION_T mediaDescr;
  const MEDIA_DESCRIPTION_T *pMediaDescr;
  SRV_LISTENER_CFG_T listenRtmp;
  int appenduri = 1;

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - liveoverhttp action: %s (%d), method: %s (%d), rsrcName: '%s', "
                            "virtRsrc: '%s', filepath: '%s'"), 
                    srvmgr_action_tostr(action), action, devtype_methodstr(method), method, 
                    pMediaRsrc->pRsrcName, pMediaRsrc->virtRsrc, pMediaRsrc->filepath));

  if(!check_license(pMgrConn, 1)) {
    return -1;
  }

  memset(&listenRtmp, 0, sizeof(listenRtmp));
  rsrcurl[0] = '\0';
  pMediaDescr = &pProc->mediaDescr;
 
  if(action != MEDIA_ACTION_CANNED_HTML) {

    memcpy(&cfg, pConn->pCfg, sizeof(SRV_CFG_T));
    pConn->pCfg = &cfg;

    if(action == MEDIA_ACTION_CANNED_INFLASH || action == MEDIA_ACTION_CANNED_WEBM) {

      if(check_mediafile(pMediaRsrc, &mediaDescr, 1) == 0) {
        pMediaDescr = &mediaDescr;
      }
      // fprintf(stderr, "FLASH PROFILE:%d %d %s %dx%d\n", mediaDescr.vid.generic.profile, mediaDescr.vid.generic.mediaType, codecType_getCodecDescrStr(mediaDescr.vid.generic.mediaType), mediaDescr.vid.generic.resH, mediaDescr.vid.generic.resV);

      cfg.pListenRtmp = pConn->pListenCfg;

      //
      // The resource is located on a remote server
      //
      if(pMediaRsrc->pLinkStr) {

        if((sz = strlen(pMediaRsrc->pLinkStr)) > 8 && strstr(&pMediaRsrc->pLinkStr[8], "/")) {
          if((sz = snprintf(rsrcurl, sizeof(rsrcurl), "%s", pMediaRsrc->pLinkStr)) < 0) {
            rsrcurl[0] = '\0';
          }
          appenduri = 0;
          is_remoteargfile = 1;

        } else {

          if((sz = snprintf(rsrcurl, sizeof(rsrcurl), 
             "%s/"VSX_MEDIA_URL"/", pMediaRsrc->pLinkStr)) <= 0) {
            rsrcurl[0] = '\0';
          }

        }

      } else {

        //
        // Return a local URL
        //
        if((sz = snprintf(rsrcurl, sizeof(rsrcurl), ""VSX_MEDIA_URL"/")) <= 0) {
          rsrcurl[0] = '\0';
        }

      } 

      if(appenduri) {

        if((sz2 = strlen(pConn->pCfg->pMediaDb->mediaDir)) < strlen(pMediaRsrc->filepath)) {
          while(pMediaRsrc->filepath[sz2] == '/') {
            sz2++;
          }  
          http_urlencode(&pMediaRsrc->filepath[sz2], &rsrcurl[sz], sizeof(rsrcurl) - sz);
        } else {
          snprintf(rsrcurl, sizeof(rsrcurl) - sz, "%s", pMediaRsrc->filepath);
        }

      }

      VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - liveoverhttp created %s media link rsrcurl: '%s' for path: '%s'"), 
                            pMediaRsrc->pLinkStr ? "remote" : "local", rsrcurl, pMediaRsrc->filepath));

    } else if(method == STREAM_METHOD_FLVLIVE ||  method == STREAM_METHOD_MKVLIVE) {

      //
      // Check if the rtmpliveport is set - which should refer to the proxy
      //
      if(pMgrConn->cfg.pListenerHttpProxy[0].listenCfg.active && 
         INET_PORT(pMgrConn->cfg.pListenerHttpProxy[0].listenCfg.sa) != 0) { 
        memcpy(&listenRtmp, &pMgrConn->cfg.pListenerHttpProxy[0].listenCfg, sizeof(listenRtmp));
      } else {
        memcpy(&listenRtmp, pMgrConn->conn.pListenCfg, sizeof(listenRtmp));
        INET_PORT(listenRtmp.sa) = htons(MGR_GET_PORT_HTTP(pProc->startPort));
      }
      cfg.pListenRtmp = &listenRtmp; 

    } else {

      //
      // Check if the rtmpliveport is set - which should refer to the proxy
      //
      if(pMgrConn->cfg.pListenerRtmpProxy[0].listenCfg.active &&
         INET_PORT(pMgrConn->cfg.pListenerRtmpProxy[0].listenCfg.sa) != 0) {
        memcpy(&listenRtmp, &pMgrConn->cfg.pListenerRtmpProxy[0].listenCfg, sizeof(listenRtmp));
      } else {
        memcpy(&listenRtmp, pMgrConn->conn.pListenCfg, sizeof(listenRtmp));
        INET_PORT(listenRtmp.sa) = htons(MGR_GET_PORT_RTMP(pProc->startPort));
      }
      cfg.pListenRtmp = &listenRtmp; 

    }

    pvirtRsrc = NULL; 
  
    //
    // Set the video description including the resolution width x height
    //
    if(!pConn->pStreamerCfg0) {
      pConn->pStreamerCfg0 = &streamerCfg;
      //if(method == STREAM_METHOD_FLVLIVE) {
         //TODO:? why do_outfmt... fix srv_ctrl_flv( callign srv_ctrl_flvlive & setting up outfmt queues...
        //streamerCfg.action.liveFmts.out[STREAMER_OUTFMT_IDX_FLV].do_outfmt = 1;
        streamerCfg.action.do_flvlive = 1;
        streamerCfg.action.do_mkvlive = 1;
        streamerCfg.action.do_rtmplive = 1;
      //} else {
      //  streamerCfg.action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMP].do_outfmt = 1;
      //}

      memset(&streamerCfg.status, 0, sizeof(streamerCfg.status));
      memcpy(&streamerCfg.status.vidProps[0], &pMediaDescr->vid.generic, 
           sizeof(streamerCfg.status.vidProps[0]));
    }

  }

  if(action == MEDIA_ACTION_CANNED_WEBM) {

    //
    // Return the rsrc/mkv_embed.html with a link of a canned resource such as /media/
    //
    rc = srv_ctrl_mkv(pConn, rsrcurl, is_remoteargfile, cfg.pListenRtmp);

  } else if(action == MEDIA_ACTION_CANNED_INFLASH) {

    //
    // Return the rsrc/http_embed.html with a link of a canned resource such as /media/
    //
    rc = srv_ctrl_flv(pConn, rsrcurl, is_remoteargfile, cfg.pListenRtmp);

  } else if(method == STREAM_METHOD_MKVLIVE) {

    //
    // Return the rsrc/mkv_embed.html with a link of a live of the child stream processor
    //
    rc = srv_ctrl_mkv(pConn, pvirtRsrc, is_remoteargfile, cfg.pListenRtmp);

  } else if(method == STREAM_METHOD_FLVLIVE) {

    //
    // Return the rsrc/http_embed.html with a link of a live of the child stream processor
    //
    rc = srv_ctrl_flv(pConn, pvirtRsrc, is_remoteargfile, cfg.pListenRtmp);

  } else {
    rc = srv_ctrl_rtmp(pConn, pvirtRsrc, is_remoteargfile, rsrcurl, cfg.pListenRtmp, 0);
  }

  pConn->pCfg = pCfgOrig;

  if(pConn->pStreamerCfg0 == &streamerCfg) {
    pConn->pStreamerCfg0 = NULL;
  }

  return rc;
}

static int process_rtsplive(SRV_MGR_CONN_T *pMgrConn, const SRVMEDIA_RSRC_T *pMediaRsrc, 
                            const SYS_PROC_T *pProc, MEDIA_ACTION_T action,
                            int startProc) {
  int rc = 0;
  SRV_CFG_T cfg;
  char rsrcurl[VSX_MAX_PATH_LEN];
  CLIENT_CONN_T *pConn = &pMgrConn->conn;
  SRV_CFG_T *pCfgOrig = pConn->pCfg;
  const char *pvirtRsrc = pMediaRsrc->virtRsrc;
  //const struct SRV_LISTENER_CFG *pListenerCfg = NULL;
  SRV_LISTENER_CFG_T listenRtsp;

  if(!check_license(pMgrConn, 1)) {
    return -1;
  }

  memset(&listenRtsp, 0, sizeof(listenRtsp));
  rsrcurl[0] = '\0';

  if(action != MEDIA_ACTION_CANNED_HTML) {
    memcpy(&cfg, pConn->pCfg, sizeof(SRV_CFG_T));
    pConn->pCfg = &cfg;

    //pListenerCfg = &pConn->pCfg->pListenRtsp[0];

    //
    // Check if the rtspliveport is set - which should refer to the proxy
    //
    if(pMgrConn->cfg.pListenerRtspProxy[0].listenCfg.active &&
       INET_PORT(pMgrConn->cfg.pListenerRtspProxy[0].listenCfg.sa) != 0) {
      memcpy(&listenRtsp, &pMgrConn->cfg.pListenerRtspProxy[0].listenCfg, sizeof(listenRtsp));
    } else {
      INET_PORT(listenRtsp.sa) = htons(MGR_GET_PORT_RTSP(pProc->startPort));
    }
    cfg.pListenRtsp = &listenRtsp; 

    snprintf(rsrcurl, sizeof(rsrcurl), "rtsp://%s:%d/%s", net_getlocalhostname(),
           htons(INET_PORT(listenRtsp.sa)), pvirtRsrc ? pvirtRsrc: "live");

    pvirtRsrc = NULL; 
  }

  //
  // This is primitive, but sleep 1.5 sec when the child process was just started
  // for the first time - to give time for the RTSP listener to start
  //
  if(startProc) {
    usleep(1500000);
  }

  rc = srv_ctrl_rtsp(pConn, pvirtRsrc, 0, rsrcurl);

  pConn->pCfg = pCfgOrig;

  return rc;
}

static int process_tslive(SRV_MGR_CONN_T *pMgrConn, const SRVMEDIA_RSRC_T *pMediaRsrc, 
                            const SYS_PROC_T *pProc, MEDIA_ACTION_T action, 
                            int startProc) {
  int rc = 0;
  CLIENT_CONN_T *pConn = &pMgrConn->conn;
  const char *pvirtRsrc = pMediaRsrc->virtRsrc;
  SRV_LISTENER_CFG_T listenHttp;
  //const SRV_LISTENER_CFG_T *pListenerCfg = NULL;
  char rsrcurl[VSX_MAX_PATH_LEN];

  //pListenerCfg = srv_ctrl_findlistener(pConn->pCfg->pListenHttp, SRV_LISTENER_MAX_HTTP, URL_CAP_FLVLIVE);
  memset(&listenHttp, 0, sizeof(listenHttp));

  if(pMgrConn->cfg.pListenerHttpProxy[0].listenCfg.active &&
     INET_PORT(pMgrConn->cfg.pListenerHttpProxy[0].listenCfg.sa) != 0) {
    memcpy(&listenHttp, &pMgrConn->cfg.pListenerHttpProxy[0].listenCfg, sizeof(listenHttp));
  } else {
    INET_PORT(listenHttp.sa) = htons(MGR_GET_PORT_HTTP(pProc->startPort));
  }

  //
  // Send HTTP 30x redirect
  // 
  snprintf(rsrcurl, sizeof(rsrcurl), "http://%s:%d%s/%s", net_getlocalhostname(),
           htons(INET_PORT(listenHttp.sa)),
           VSX_TSLIVE_URL,
           pvirtRsrc ? pvirtRsrc: "live");

  rc = http_resp_moved(&pConn->sd, &pConn->httpReq, HTTP_STATUS_FOUND, 1, rsrcurl);

  return rc;
}

static int wait_for_file_created(const char *path) {
  int rc = 0;
  int ms, mswarn;
  int iter = 0;
  struct stat st;
  struct timeval tv0, tv1, tvwarn;
  const int mswaittm_warn = 13000;
  const int mswaittm_abort = 35000;

  gettimeofday(&tv0, NULL);
  memset(&tvwarn, 0, sizeof(tvwarn));

  do {

    gettimeofday(&tv1, NULL);
    ms = TIME_TV_DIFF_MS(tv1, tv0);

    if(fileops_stat(path, &st) == 0) {
      if(iter > 0) {
        LOG(X_DEBUG("File %s created after %d ms"), path, ms);
      }
      break;
    }

    if(ms >= mswaittm_warn) {

      if((mswarn = TIME_TV_DIFF_MS(tv1, tvwarn)) > 3000) {
        LOG(X_WARNING("File %s not created after %d ms"), path, ms);
        memcpy(&tvwarn, &tv1, sizeof(tvwarn));
      }

      if(ms >= mswaittm_abort) {
        LOG(X_ERROR("Aborting wait for file %s creation after %d ms"), path, ms);
        rc = -1;
        break;
      }
    }

    iter++;
    usleep(500000);

  } while(ms < mswaittm_abort);

  return rc;
}

static int get_segments_filepath(char *filepath, 
                                 const char *instanceId,
                                 const char *pRsrcName) {
  int rc = 0;
  rc = snprintf(filepath, VSX_MAX_PATH_LEN, "%s/%s", 
                instanceId, (pRsrcName && pRsrcName[0] !='\0') ? pRsrcName : "");

  return rc;
}

static int is_process_xcode_multi(const SRVMEDIA_RSRC_T *pMediaRsrc) {

  int rc = 0;
  IXCODE_CTXT_T xcodeCtxt;

  if(pMediaRsrc->pXcodeStr && pMediaRsrc->pXcodeStr[0] != '\0') {
    memset(&xcodeCtxt, 0, sizeof(xcodeCtxt));

    if((rc = xcode_parse_configstr(pMediaRsrc->pXcodeStr, &xcodeCtxt, 0, 0)) >= 0) {
      if(xcodeCtxt.vid.out[0].active && xcodeCtxt.vid.out[1].active) {
        return 1;
      }
    }
  }

  return rc >= 0 ? 0 : rc;
}

static int process_httplive(SRV_MGR_CONN_T *pMgrConn, 
                            const SRVMEDIA_RSRC_T *pMediaRsrc, 
                            const SYS_PROC_T *pProc,
                            HTTP_STATUS_T *pHttpStatus) {
  int rc = 0;
  size_t sz, sztmp;
  char tmp[VSX_MAX_PATH_LEN];
  char tmp2[VSX_MAX_PATH_LEN];
  char filepath[VSX_MAX_PATH_LEN];
  const char *fileExt = NULL;
  HTTPLIVE_DATA_T *pHttpLiveDataOrig = NULL;
  HTTPLIVE_DATA_T httpLiveDataTmp;
  CLIENT_CONN_T *pConn = &pMgrConn->conn;
  char *pRsrcName = NULL;

  if(!check_license(pMgrConn, 1)) {
    return -1;
  }

  //
  // The request should only be for a known media file '.ts.' extension or playlist file '.m3u8' extension 
  // otherwise we wish to return an index file with an embedded video tag of the resource playlist
  //
  sz = strlen(pMediaRsrc->pRsrcName);
  if((sz > (sztmp = strlen(HTTPLIVE_PL_NAME_EXT)) && 
      !strncasecmp(&pMediaRsrc->pRsrcName[sz - sztmp], HTTPLIVE_PL_NAME_EXT, sztmp)) ||
     ((sz > (sztmp = 1 + strlen(HTTPLIVE_TS_NAME_EXT)) && 
    !strncasecmp(&pMediaRsrc->pRsrcName[sz - sztmp], "."HTTPLIVE_TS_NAME_EXT, sztmp)))) {

    pRsrcName = pMediaRsrc->pRsrcName;
  }

  //
  // For any '/httplive' request, remap the media resource directory path to the 
  // 'html/httplive/<instance id>' directory used to store .ts / .m3u8 files
  //
  if((rc = get_segments_filepath(filepath, pProc->instanceId, pMediaRsrc->rsrcExt) < 0)) {
    return rc;
  }

  //
  // If we are transcoding output into multiple output representations, then set the count
  // of available representations > 1, which will be referenced by srv_ctrl_httplive to return 
  // multi.m3u8 instead of out.m3u8
  //
  if(pMediaRsrc->rsrcExt[0] == '\0' && is_process_xcode_multi(pMediaRsrc) > 0) {
    pHttpLiveDataOrig = pConn->pCfg->pHttpLiveDatas[0];
    pConn->pCfg->pHttpLiveDatas[0] = &httpLiveDataTmp;
    memcpy(&httpLiveDataTmp, pHttpLiveDataOrig, sizeof(httpLiveDataTmp));
    httpLiveDataTmp.count = 2;
  }

  //
  // Get the resource file extension to see if it's a playlist and if so, to wait until the playlist has
  // been created by the underlying stream media processor instance prior to returning the HTML document
  //
  fileExt = pMediaRsrc->rsrcExt;
  while(*fileExt != '\0' && *fileExt!= '.') {
    fileExt++;
  }

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - process_httplive url: '%s', proc instanceId: '%s', filepath: '%s', "
            "virtRsrc: '%s', rsrcName: '%s', rsrcExt: '%s' (compare ext: '%s', dir: '%s'), count: %d"), 
             pConn->httpReq.url, pProc->instanceId, filepath, 
             pMediaRsrc->virtRsrc, pRsrcName, pMediaRsrc->rsrcExt, fileExt, pConn->pCfg->pHttpLiveDatas[0]->dir, 
             pConn->pCfg->pHttpLiveDatas[0]->count));

  //
  // If the request is for the playlist file, sleep up to mswaittm giving
  // the file a chance to be initially created
  //
  if(fileExt[0] != '\0' && 
     (!strncmp(fileExt, HTTPLIVE_PL_NAME_EXT, strlen(HTTPLIVE_PL_NAME_EXT))) &&
     mediadb_prepend_dir(pConn->pCfg->pHttpLiveDatas[0]->dir, filepath, tmp2, sizeof(tmp2)) == 0) {

    VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - process_httplive checking path '%s'"), tmp2));

    if((rc = wait_for_file_created(tmp2)) < 0) {
      *pHttpStatus = HTTP_STATUS_SERVERERROR;
      rc = -1; 
    }
  }

  //
  // The URI should be in the form of: /httplive/leading-dir/id_xyz/pRsrcName/[ any input out.m3u8 ]
  //
  if(rc >= 0) {

    strncpy(tmp, pMediaRsrc->virtRsrc, sizeof(tmp) - 1);
    tmp[pMediaRsrc->szvirtRsrcOnly] = '\0';
    snprintf(tmp2, sizeof(tmp2), "%s/%s%s%s/%s/%s", VSX_HTTPLIVE_URL, tmp, 
        pMediaRsrc->instanceId[0] != '\0' ? "/"VSX_URI_ID_PARAM : "", pMediaRsrc->instanceId, 
        pMediaRsrc->pRsrcName, pMediaRsrc->rsrcExt);

    //
    // Retain the '/prof_' profile id in the URI, but not in the virtFilePath argument 
    //
    if(pMediaRsrc->profile[0] != '\0' && avc_isnumeric(pMediaRsrc->profile) != '\0') { 

      sztmp = strlen(tmp2);
      snprintf(tmp, sizeof(tmp), "%s/%s%s", tmp2,
        VSX_URI_PROFILE_PARAM, pMediaRsrc->profile);
    } else {
      strncpy(tmp, tmp2, sizeof(tmp));
    }

    VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - process_httplive calling srv_ctrl_httplive with uri: '%s' (was: '%s'), "
         "virtFilePath: '%s', filePath: '%s', media.instanceId: '%s', pRsrcName: '%s', xcode: '%s'"),
          tmp, pConn->httpReq.puri, tmp2, filepath, pMediaRsrc->instanceId, 
          pMediaRsrc->pRsrcName, pMediaRsrc->pXcodeStr));

    pConn->httpReq.puri = tmp;
    rc = srv_ctrl_httplive(pConn, VSX_HTTPLIVE_URL, tmp2, filepath, pHttpStatus);
  }

  //
  // Restore the original httplive data instance which may have been replaced to force returning the
  // multi.m3u8 file
  //
  if(pHttpLiveDataOrig) {
    pConn->pCfg->pHttpLiveDatas[0] = pHttpLiveDataOrig; 
  }

  return rc;
}

static int process_dash(SRV_MGR_CONN_T *pMgrConn, 
                            const SRVMEDIA_RSRC_T *pMediaRsrc, 
                            const SYS_PROC_T *pProc,
                            HTTP_STATUS_T *pHttpStatus) {
  int rc = 0;
  size_t sz, sztmp;
  char tmp[VSX_MAX_PATH_LEN];
  char tmp2[VSX_MAX_PATH_LEN];
  char filepath[VSX_MAX_PATH_LEN];
  CLIENT_CONN_T *pConn = &pMgrConn->conn;
  char *pRsrcName = NULL;
  const char *fileExt = NULL;

  if(!check_license(pMgrConn, 1)) {
    return -1;
  }

  //
  // The request should only be for a known media file extension (.m4s) or playlist file extension  (.mpd)
  // otherwise we wish to return an index file with an embedded video tag of the resource playlist
  //
  sz = strlen(pMediaRsrc->pRsrcName);
  if((sz > (sztmp = strlen(DASH_MPD_EXT)) && 
      !strncasecmp(&pMediaRsrc->pRsrcName[sz - sztmp], DASH_MPD_EXT, sztmp)) ||
     ((sz > (sztmp = 1 + strlen(DASH_DEFAULT_SUFFIX_M4S)) && 
    !strncasecmp(&pMediaRsrc->pRsrcName[sz - sztmp], "."DASH_DEFAULT_SUFFIX_M4S, sztmp)))) {

    pRsrcName = pMediaRsrc->pRsrcName;
  }

  //
  // For any '/dash' request, remap the media resource directory path to the 
  // 'html/dash/<instance id>' directory used to store .m4s / .mpd files
  //
  if((rc = get_segments_filepath(filepath, pProc->instanceId, pMediaRsrc->rsrcExt) < 0)) {
    return rc;
  }

  //
  // Get the resource file extension to see if it's a playlist and if so, to wait until the playlist has
  // been created by the underlying stream media processor instance prior to returning the HTML document
  //
  fileExt = pMediaRsrc->rsrcExt;
  while(*fileExt != '\0' && *fileExt!= '.') {
    fileExt++;
  }

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - process_dash url: '%s', proc instanceId: '%s', filepath: '%s', "
                            "virtRsrc: '%s', rsrcName: '%s', rsrcExt: '%s' (compare ext: '%s', dir: '%s')"), 
     pConn->httpReq.url, pProc->instanceId, filepath, 
     pMediaRsrc->virtRsrc, pRsrcName, pMediaRsrc->rsrcExt, fileExt, 
     pConn->pCfg->pMoofCtxts[0]->dashInitCtxt.outdir));

  //
  // If the request is for the playlist file, sleep up to mswaittm giving
  // the file a chance to be initially created
  //
  if(fileExt[0] != '\0' &&
     (!strncmp(fileExt, DASH_MPD_EXT, strlen(DASH_MPD_EXT))) &&
     mediadb_prepend_dir(pConn->pCfg->pMoofCtxts[0]->dashInitCtxt.outdir, filepath, tmp2, sizeof(tmp2)) == 0) {

    VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - process_dash checking path '%s'"), tmp2));

    if((rc = wait_for_file_created(tmp2)) < 0) {
      *pHttpStatus = HTTP_STATUS_SERVERERROR;
      rc = -1; 
    }
  }

  //
  // The URI should be in the form of: /dash/leading-dir/id_xyz/pRsrcName/[ any input out.m3u8 ]
  //
  if(rc >= 0) {

    strncpy(tmp, pMediaRsrc->virtRsrc, sizeof(tmp) - 1);
    tmp[pMediaRsrc->szvirtRsrcOnly] = '\0';
    snprintf(tmp2, sizeof(tmp2), "%s/%s%s%s/%s/%s", VSX_DASH_URL, tmp, 
             pMediaRsrc->instanceId[0] != '\0' ? "/"VSX_URI_ID_PARAM : "", pMediaRsrc->instanceId, 
           pMediaRsrc->pRsrcName, pMediaRsrc->rsrcExt);

    //
    // Retain the '/prof_' profile id in the URI, but not in the virtFilePath argument 
    //
    if(pMediaRsrc->profile[0] != '\0' && avc_isnumeric(pMediaRsrc->profile) != '\0') {

      sztmp = strlen(tmp2);
      snprintf(tmp, sizeof(tmp), "%s/%s%s", tmp2,
        VSX_URI_PROFILE_PARAM, pMediaRsrc->profile);
    } else {
      strncpy(tmp, tmp2, sizeof(tmp));
    }

    VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - process_dash calling srv_ctrl_mooflive with uri: '%s' (was: '%s'), "
         "virtFilePath: '%s', filePath: '%s', media.instanceId: '%s', pRsrcName: '%s', xcode: '%s'"),
          tmp, pConn->httpReq.puri, tmp2, filepath, pMediaRsrc->instanceId,
          pMediaRsrc->pRsrcName, pMediaRsrc->pXcodeStr));

    pConn->httpReq.puri = tmp;
    rc = srv_ctrl_mooflive(pConn, VSX_DASH_URL, tmp2, filepath, pHttpStatus);
  }

  return rc;
}

static int create_redirect_uri(STREAM_METHOD_T method, 
                               SRVMEDIA_RSRC_T *pMediaRsrc, 
                               char *buf, 
                               unsigned int szbuf) {
  int rc = 0;
  size_t szvirtRsrcOnly;
  const char *strmethod = "";

  //
  // Send HTTP 30x redirect
  // 
  if(pMediaRsrc->instanceId[0] == '\0') {

    switch(method) {
      case STREAM_METHOD_HTTPLIVE:
        strmethod = VSX_HTTPLIVE_URL;
        break; 
      case STREAM_METHOD_DASH:
        strmethod = VSX_DASH_URL;
        break; 
      default:
        return -1;
    }

    procdb_create_instanceId(pMediaRsrc->instanceId);

    rc = snprintf(buf, szbuf, "%s/%s", strmethod, pMediaRsrc->virtRsrc);
    if((szvirtRsrcOnly = strlen(strmethod) + 1 + pMediaRsrc->szvirtRsrcOnly) < szbuf) {
      rc = snprintf(&buf[szvirtRsrcOnly], szbuf - szvirtRsrcOnly, "/%s%s/%s%s%s",
               VSX_URI_ID_PARAM, pMediaRsrc->instanceId, pMediaRsrc->pRsrcName,
               pMediaRsrc->profile[0] != '\0' ? "/"VSX_URI_PROFILE_PARAM : "", pMediaRsrc->profile);
    } else {
      rc = -1;
    }
  }

  return rc;
}

static SYS_PROC_T *find_running_proc(SYS_PROCLIST_T *pProcs,  
                                     const SRVMEDIA_RSRC_T *pMediaRsrc,
                                     int lock) {
  SYS_PROC_T *pProc = NULL;

  if(pMediaRsrc->instanceId[0] != '\0') {
    pProc = procdb_findInstanceId(pProcs, pMediaRsrc->instanceId, lock);
  } else {
    pProc = procdb_findName(pProcs, pMediaRsrc->virtRsrc, pMediaRsrc->pId, lock);
  }

  return pProc;
}

int srvmgr_check_start_proc(SRV_MGR_CONN_T *pConn, 
                            const SRVMEDIA_RSRC_T *pMediaRsrc, 
                            MEDIA_DESCRIPTION_T *pMediaDescr, 
                            SYS_PROC_T *procArg, 
                            const STREAM_DEVICE_T *pdevtype,  
                            MEDIA_ACTION_T action, 
                            STREAM_METHOD_T methodAuto,
                            int *pstartProc) {
  int rc = 0;
  int haveFullMediaDescr = 0;
  SYS_PROC_T *pProc = NULL;
  int ssl = (pConn->conn.sd.netsocket.flags & NETIO_FLAG_SSL_TLS) ? 1 : 0;

  if(!srv_ctrl_islegal_fpath(pMediaRsrc->filepath)) {
    *pstartProc = 1;
    LOG(X_ERROR("Illegal file path: '%s' for resource: '%s'"), pMediaRsrc->filepath, pMediaRsrc->virtRsrc);
    return -1;
  } else  if(pMediaRsrc->pmethodBits && *pMediaRsrc->pmethodBits != 0 && 
            !( *pMediaRsrc->pmethodBits & (1 << methodAuto))) {
    LOG(X_ERROR("The media resource configuration for '%s' file path '%s' does not enable %s (%d), mask: 0x%x"), 
                pConn->conn.httpReq.puri, pMediaRsrc->filepath, devtype_methodstr(methodAuto), methodAuto, 
                *pMediaRsrc->pmethodBits);
    *pstartProc = 1;
    return -1;
  }

  *pstartProc = 0;

  if(!(pProc = find_running_proc(pConn->cfg.pProcList, pMediaRsrc, 1))) {
    haveFullMediaDescr = check_mediaproperties(pMediaRsrc, pMediaDescr);
  }

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - check_start_proc virtRsrc: '%s', id: '%s', instanceId: '%s' "
                            "found proc 0x%x, instanceIid: '%s', haveFullMedia: %d, shared: %d, methodbits: 0x%x"), 
         pMediaRsrc->virtRsrc, pMediaRsrc->pId, pMediaRsrc->instanceId, pProc, 
         pProc ? pProc->instanceId : "", haveFullMediaDescr, pMediaRsrc->pshared ? *pMediaRsrc->pshared : 0,
         pMediaRsrc->pmethodBits ? *pMediaRsrc->pmethodBits : 0));
 
  //
  // Lock the process list
  //
  pthread_mutex_lock(&pConn->cfg.pProcList->mtx);

  if(!(pProc = find_running_proc(pConn->cfg.pProcList, pMediaRsrc, 0))) {

    //
    // We are going to be creating a new child process, so check resource availability
    //
    if((pConn->cfg.pProcList->maxInstances > 0 &&
        pConn->cfg.pProcList->activeInstances >= pConn->cfg.pProcList->maxInstances)) {
      LOG(X_WARNING("Maximum child processes reached %d"), 
          pConn->cfg.pProcList->maxInstances);
      rc = -1;
    } else if((pConn->cfg.pProcList->maxXcodeInstances > 0 && pMediaRsrc->pXcodeStr &&
             pConn->cfg.pProcList->activeXcodeInstances >= pConn->cfg.pProcList->maxXcodeInstances)) {
      LOG(X_WARNING("Maximum child active transcoding processes reached %d"), 
                    pConn->cfg.pProcList->maxXcodeInstances);
      rc = -1;
    } else if((pConn->cfg.pProcList->maxMbbpsTot > 0 && pConn->cfg.pProcList->mbbpsTot + 
            procdb_getMbbps(pMediaDescr) > pConn->cfg.pProcList->maxMbbpsTot)) {
      LOG(X_WARNING("Maximum transcoding mbbps reached (active: %d + %d > %d)"), 
          pConn->cfg.pProcList->mbbpsTot, procdb_getMbbps(pMediaDescr), 
          pConn->cfg.pProcList->maxMbbpsTot);
      rc = -1;
    }

    if(rc != 0) {
      pthread_mutex_unlock(&pConn->cfg.pProcList->mtx);
      return -1;
    } else {

      //
      // Setup a new child process element configuration
      //
      if((pProc = procdb_setup(pConn->cfg.pProcList, pMediaRsrc->virtRsrc, 
                           pMediaRsrc->pId, (haveFullMediaDescr ? pMediaDescr : NULL), 
                           pMediaRsrc->pXcodeStr, pMediaRsrc->instanceId, 0))) {

        //if(mediaRsrc.pXcodeStr) {
        //  pProc->isXcoded = 1;
        //  pConn->cfg.pProcList->activeXcodeInstances++;
        //}
        //pConn->cfg.pProcList->activeInstances++;
        *pstartProc = 1;
      }

    }

  } else {

    VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - proc instanceId: '%s' already exists"), pProc->instanceId));

    //
    // Update last access time
    //
    gettimeofday(&pProc->tmLastAccess, NULL);
  }

  //
  // Store the new process context as an output variable
  //
  if(pProc) {
    memcpy(procArg, pProc, sizeof(SYS_PROC_T));
  }

  pthread_mutex_unlock(&pConn->cfg.pProcList->mtx);

  if(*pstartProc) {

    //
    // Start a child process to handle the media resource
    //
    if(procdb_start(pConn->cfg.pProcList, pProc, pMediaRsrc->filepath, pdevtype,
                    pConn->cfg.plaunchpath, pMediaRsrc->pXcodeStr, 
                    pMediaRsrc->pInputStr, pMediaRsrc->pUserpass, 
                    pMediaRsrc->pmethodBits ? *pMediaRsrc->pmethodBits : 0, ssl) < 0) {
      return -1;
    }

  }

  return rc;
}

static STREAM_METHOD_T get_method_fromurl(const CLIENT_CONN_T *pConn, 
                                          MEDIA_ACTION_T *pAction,  
                                          int *pliveAutoFind) {

  STREAM_METHOD_T method = STREAM_METHOD_NONE;

  if(!strncasecmp(pConn->httpReq.puri, VSX_RTSP_URL, strlen(VSX_RTSP_URL))) {
    method = STREAM_METHOD_RTSP;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_RTMP_URL, strlen(VSX_RTMP_URL))) {
    method = STREAM_METHOD_RTMP;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_HTTPLIVE_URL, strlen(VSX_HTTPLIVE_URL))) {
    method = STREAM_METHOD_HTTPLIVE;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_DASH_URL, strlen(VSX_DASH_URL))) {
    method = STREAM_METHOD_DASH;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_HTTP_URL, strlen(VSX_HTTP_URL))) {
    method = STREAM_METHOD_FLASHHTTP;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_MEDIA_URL, strlen(VSX_MEDIA_URL))) {
    method = STREAM_METHOD_PROGDOWNLOAD;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_FLV_URL, strlen(VSX_FLV_URL))) {
    method = STREAM_METHOD_FLVLIVE;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_MKV_URL, strlen(VSX_MKV_URL))) {
    method = STREAM_METHOD_MKVLIVE;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_LIVE_URL, strlen(VSX_LIVE_URL))) {
    method = STREAM_METHOD_UNKNOWN;
    *pliveAutoFind = 1;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_TMN_URL, strlen(VSX_TMN_URL))) {
    method = STREAM_METHOD_NONE;
    *pAction = MEDIA_ACTION_TMN;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_IMG_URL, strlen(VSX_IMG_URL))) {
    method = STREAM_METHOD_NONE;
    *pAction = MEDIA_ACTION_IMG;
  } else if(!strncasecmp(pConn->httpReq.puri, VSX_LIST_URL, strlen(VSX_LIST_URL))) {
    method = STREAM_METHOD_NONE;
    *pAction = MEDIA_ACTION_LIST;
  } else {
    method = STREAM_METHOD_NONE;
    *pAction = MEDIA_ACTION_INDEX_FILELIST;
  }

  return method;
}

STREAM_METHOD_T findBestMethod(const STREAM_DEVICE_T *pDev, const META_FILE_T *pMetaFile) {

  unsigned int idxMethod;
  if(pMetaFile->methodBits != 0) {

    for(idxMethod = 0; idxMethod < STREAM_DEVICE_METHODS_MAX; idxMethod++) {
      if(pDev->methods[idxMethod] > STREAM_METHOD_NONE && 
         ((1 << pDev->methods[idxMethod]) & pMetaFile->methodBits)) {
        return pDev->methods[idxMethod]; 
      }
    }
  } else {
    return pDev->methods[0];
  }

  return STREAM_METHOD_UNKNOWN;
}

const char *srvmgr_authenticate(SRV_MGR_CONN_T *pConn, 
                                const META_FILE_T *pMetaFile, 
                                char authbuf[], 
                                HTTP_STATUS_T *phttpStatus) {

  int rc = 0;
  const char *pauthbuf = NULL;
  AUTH_CREDENTIALS_STORE_T authStore;
  char authstr[AUTH_ELEM_MAXLEN * 2 + 1 + 1];
  const char *pauthstr = (const char *) authstr;
  AUTH_CREDENTIALS_STORE_T *pAuthStore = NULL;
   
  pAuthStore = pConn->conn.pListenCfg->pAuthStore;

  //
  // If the requested resource has defined digest credentials in the metafile using 'digestauth=' then
  // replace the main listener credentials with the resource specific ones, otherwise use any
  // credentials set on the http listener.
  //
  if(pMetaFile->userpass[0] != '\0') {
    memset(&authStore, 0, sizeof(authStore));
    snprintf(authstr, sizeof(authstr), "%s@", pMetaFile->userpass);
    if((rc = capture_parseAuthUrl(&pauthstr, &authStore)) < 0) {
      *phttpStatus = HTTP_STATUS_UNAUTHORIZED;
      return NULL;
    }
    pAuthStore = &authStore;
  }

  //
  // Validate any server authorization credentials 
  //
  if((pauthbuf = srv_check_authorized(pAuthStore, &pConn->conn.httpReq, authbuf))) {
    *phttpStatus = HTTP_STATUS_UNAUTHORIZED;
  }

  return pauthbuf;
}

void srvmgr_client_proc(void *pfuncarg) {

  int rc = 0;
  SRV_MGR_CONN_T *pConn = (SRV_MGR_CONN_T *) pfuncarg;
  SYS_PROC_T proc;
  int startProc = 0;
  struct stat st;
  char tmpfile[VSX_MAX_PATH_LEN];
  char authbuf[AUTH_BUF_SZ];
  char tmp[512];
  const char *pauthbuf = NULL;
  const char *parg;
  const char *userAgent = NULL;
  const STREAM_DEVICE_T *pdevtype = NULL;
  MEDIA_ACTION_T action;
  STREAM_METHOD_T reqMethod; 
  STREAM_METHOD_T methodAuto;
  MEDIA_DESCRIPTION_T mediaDescr;
  MEDIA_ACTION_T checkAction;
  META_FILE_T metaFile;
  SRVMEDIA_RSRC_T mediaRsrc;
  HTTP_STATUS_T httpStatus;
  int liveAutoFind;
  int itmp;

  LOG(X_DEBUG("Handling %sconnection on port %d from %s:%d"),
              ((pConn->conn.sd.netsocket.flags & NETIO_FLAG_SSL_TLS) ? "SSL " : ""),
              ntohs(INET_PORT(pConn->conn.sd.sa)),
              FORMAT_NETADDR(pConn->conn.sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->conn.sd.sa)));

  do {

    VSX_DEBUG_HTTP( LOG(X_DEBUGV("HTTP - srvmgr_client_proc calling http_req_readparse...")));

    //
    // Get the HTTP request
    //
    if((rc = http_req_readparse(&pConn->conn.sd, &pConn->conn.httpReq, HTTP_REQUEST_TIMEOUT_SEC, 1)) <= 0) {
      if(rc < 0 && NETIOSOCK_FD(pConn->conn.sd.netsocket) != INVALID_SOCKET) {
        rc = http_resp_error(&pConn->conn.sd, &pConn->conn.httpReq, HTTP_STATUS_BADREQUEST, 1, NULL, NULL);
      }
      break;
    }
  
    if((g_debug_flags & (VSX_DEBUG_FLAG_HTTP | VSX_DEBUG_FLAG_MGR))) {
      LOG(X_DEBUG("HTTP - srvmgr_client_proc method: '%s', rc:%d, URI: %s%s"), 
                                pConn->conn.httpReq.method, rc, pConn->conn.httpReq.puri, 
                              http_req_dump_uri(&pConn->conn.httpReq, tmp, sizeof(tmp))); 

    }

    rc = 0;
    pauthbuf = NULL;
    httpStatus = HTTP_STATUS_OK;
    action = MEDIA_ACTION_UNKNOWN;
    reqMethod = STREAM_METHOD_INVALID;
    methodAuto = STREAM_METHOD_UNKNOWN;
    checkAction = MEDIA_ACTION_UNKNOWN;
    liveAutoFind = 0;
    memset(&mediaDescr, 0, sizeof(mediaDescr));
    memset(&mediaRsrc, 0, sizeof(mediaRsrc));
    userAgent = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->conn.httpReq.reqPairs, HTTP_HDR_USER_AGENT);

    if(httpStatus == HTTP_STATUS_OK) {

      //
      // Get the client's requested streaming method from the URL 
      //
      reqMethod = get_method_fromurl(&pConn->conn, &action, &liveAutoFind);

      if(reqMethod == STREAM_METHOD_INVALID || 
         ((action == MEDIA_ACTION_UNKNOWN || 
          (action == MEDIA_ACTION_INDEX_FILELIST && strcmp(pConn->conn.httpReq.puri, "/")))  && 
         (parse_media_uri(&pConn->conn, &mediaRsrc) < 0 || mediaRsrc.virtRsrc[0] == '\0'))) {
        LOG(X_WARNING("Invalid URI %s"), pConn->conn.httpReq.puri);
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }
    }

    if(httpStatus == HTTP_STATUS_OK) {
      //
      // Try to find a .meta file associated with the media file path
      //
      if(methodAuto != STREAM_METHOD_NONE) {
        memset(&metaFile, 0, sizeof(metaFile));
        srvmgr_check_metafile(&metaFile, &mediaRsrc, pdevtype->name, &httpStatus); 
        metafile_close(&metaFile);
      }


    }

    //
    // Validate any server authorization credentials required set on the http listeners
    //
    if((pauthbuf = srvmgr_authenticate(pConn, &metaFile, authbuf, &httpStatus)) || httpStatus != HTTP_STATUS_OK) {
      httpStatus = HTTP_STATUS_UNAUTHORIZED;
    }

    if(httpStatus == HTTP_STATUS_OK) {

      //
      // Lookup the client device type according to '/etc/devices.conf' User-Agent mapping
      //
      if(!(pdevtype = devtype_finddevice(userAgent, 1))) {
        LOG(X_ERROR("No matching device type found"));
        httpStatus = HTTP_STATUS_SERVERERROR;
      } else if(pdevtype->strmatch[0] == '\0') {
        LOG(X_DEBUG("Unmatched User-Agent: '%s' in media request '%s'"), userAgent, pConn->conn.httpReq.puri);
      }

    }

    if(httpStatus == HTTP_STATUS_OK) {

      //
      // The client is checking for the available play methods for the resoruce
      //
      if((parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->conn.httpReq.uriPairs, "check"))) {

        if((itmp = atoi(parg)) == MEDIA_ACTION_CHECK_EXISTS) {
          checkAction = itmp;
        } else if(itmp > 0) {
          checkAction = MEDIA_ACTION_CHECK_TYPE;
        }

      }
    }

    if(httpStatus == HTTP_STATUS_OK) {

      //
      // Allow a resource to be loaded w/o the full '/live/[resource name]
      //
      if((reqMethod == STREAM_METHOD_UNKNOWN || reqMethod == STREAM_METHOD_NONE) && 
          strlen(mediaRsrc.filepath) > strlen(pConn->conn.pCfg->pMediaDb->mediaDir)) {

        if(fileops_stat(mediaRsrc.filepath, &st) == 0) {
          reqMethod = STREAM_METHOD_UNKNOWN;
          action = MEDIA_ACTION_UNKNOWN;
          //fprintf(stderr, "YESSSSSS TO\n\n%u for '%s'\n",  pthread_self(), mediaRsrc.filepath);
        } else if(!liveAutoFind && action == MEDIA_ACTION_UNKNOWN) {
          //fprintf(stderr, "%u  CHANGING TO FILELIST\n",  pthread_self());
          action = MEDIA_ACTION_INDEX_FILELIST;
        }
      }

    }

    if(httpStatus == HTTP_STATUS_OK) {

      if(reqMethod == STREAM_METHOD_UNKNOWN) {

        //
        // Set the preferred streaming type according to 'etc/devices.conf' User-Agent lookup
        //
        if((methodAuto = findBestMethod(pdevtype, &metaFile)) == STREAM_METHOD_UNKNOWN) {
        //if((methodAuto = pdevtype->methods[0]) == STREAM_METHOD_UNKNOWN) {
          methodAuto = STREAM_METHOD_FLVLIVE;
          LOG(X_DEBUG("Unhandled "HTTP_HDR_USER_AGENT" '%s' default to %s"), 
                    userAgent, devtype_methodstr(methodAuto));
        }
      }

      if(methodAuto <= STREAM_METHOD_NONE) {
        methodAuto = reqMethod;
      }

    }

    if(httpStatus == HTTP_STATUS_OK) {

      //
      // Check and introspect the media resource only if for /media URL
      //
      if(reqMethod == STREAM_METHOD_UNKNOWN || methodAuto == STREAM_METHOD_PROGDOWNLOAD ||
        action == MEDIA_ACTION_CANNED_INFLASH) {
        check_mediafile(&mediaRsrc, &mediaDescr, 0); 
      }

      //
      // Decide how to serve the media resource
      //
      if(action == MEDIA_ACTION_UNKNOWN) {

        action = get_media_action(&mediaDescr, &mediaRsrc, &pConn->conn, pdevtype, reqMethod, methodAuto);

        LOG(X_DEBUG("MGR - get_media_action path: '%s', virt: '%s', got action: %s (%d) from unknown, ext:'%s', "
                    "reqMethod: %s (%d), methodAuto: %s (%d), media.type: %s (%d), profile:'%s'"), 
                     mediaRsrc.filepath, mediaRsrc.virtRsrc, srvmgr_action_tostr(action), action, 
                     mediaRsrc.pRsrcName, devtype_methodstr(reqMethod), reqMethod, 
                     devtype_methodstr(methodAuto), methodAuto, codecType_getCodecDescrStr(mediaDescr.type), 
                     mediaDescr.type, mediaRsrc.profile);
      }

      //
      // Ensure the media resource (media file or sdp) exists in the media dir
      //
      if(action == MEDIA_ACTION_NONE || action == MEDIA_ACTION_UNKNOWN) {
        LOG(X_WARNING("Resource does not exist '%s'"), pConn->conn.httpReq.puri);
        httpStatus = HTTP_STATUS_NOTFOUND;
      }

    } // end of httpStatus == HTTP_STATUS_OK

    if(httpStatus == HTTP_STATUS_OK) {

      if(mediaRsrc.instanceId[0] != '\0' && strlen(mediaRsrc.instanceId) != SYS_PROC_INSTANCE_ID_LEN) {
        LOG(X_WARNING("Invalid client instanceId: '%s'"), mediaRsrc.instanceId);
        mediaRsrc.instanceId[0] = '\0';
      }

      if(mediaRsrc.instanceId[0] == '\0' && 
         (methodAuto == STREAM_METHOD_HTTPLIVE || methodAuto == STREAM_METHOD_DASH) && 
         !is_shared_resource(&mediaRsrc, action)) {

        //
        // If the resource is a dedicated, non-shared resource then Send HTTP 30x redirect to 
        // a link unique for the client
        //
        // TODO: need to start live processor at this point.  Check if client requested unique id exists...
        // and if it does not, redirect to the  server generated non-shared resource id
        // 
        if((rc = create_redirect_uri(methodAuto, &mediaRsrc, tmp, sizeof(tmp))) > 0) {
          LOG(X_DEBUG("Redirecting non-sharable resource '%s' to instanceId: -> '%s'"), 
                        pConn->conn.httpReq.puri, tmp);
          rc = http_resp_moved(&pConn->conn.sd, &pConn->conn.httpReq, HTTP_STATUS_FOUND, 1, tmp);
          break;

        } else if(rc < 0) {
          httpStatus = HTTP_STATUS_SERVERERROR;
        }

      //} else {
      //  mediaRsrc.instanceId[0] = '\0';
      }
    }

    if(httpStatus == HTTP_STATUS_OK) {
      if(checkAction == MEDIA_ACTION_CHECK_TYPE || checkAction == MEDIA_ACTION_CHECK_EXISTS) {
        itmp = checkAction;
        checkAction = action;
        action = itmp;
      }
    }

    VSX_DEBUG_MGR(
       LOG(X_DEBUG("MGR - path: '%s', UA:'%s' devname:'%s', reqMethod: %s (%d), methodAuto: %s (%d), "
                   "action: %s (%d), checkAction: %s (%d), virtRsrc:'%s', pRsrcName:'%s', media.type: 0x%x, "
                   "media.profile: '%s', media.instanceId: '%s', pauthbuf: '%s'"), 
          mediaRsrc.filepath, userAgent, pdevtype ? pdevtype->name : "", devtype_methodstr(reqMethod), reqMethod, 
          devtype_methodstr(methodAuto), methodAuto, srvmgr_action_tostr(action), action, 
          srvmgr_action_tostr(checkAction), checkAction, mediaRsrc.virtRsrc, mediaRsrc.pRsrcName, 
          mediaDescr.type, mediaRsrc.profile, mediaRsrc.instanceId, pauthbuf));

    if(httpStatus == HTTP_STATUS_OK) {

      switch(action) {

        case MEDIA_ACTION_CANNED_DIRECT:

          // 
          // Directly handle the non-live canned media resource
          //
          rc = srv_ctrl_loadmedia(&pConn->conn, mediaRsrc.filepath);
          break;

        case MEDIA_ACTION_LIVE_STREAM:
        case MEDIA_ACTION_CANNED_PROCESS:

          memset(&proc, 0, sizeof(proc));
          startProc = 0;

          //
          // The media resource is processed by a dedicated vsx child instance
          //
          if((rc = srvmgr_check_start_proc(pConn, &mediaRsrc, &mediaDescr, &proc, pdevtype, 
                                           action, methodAuto, &startProc)) < 0) {
            if(startProc) {
              httpStatus = HTTP_STATUS_SERVERERROR;
              //rc = http_resp_error(&pConn->conn.sd, &pConn->conn.httpReq, HTTP_STATUS_SERVERERROR, 1, NULL, NULL);
            } else {
              httpStatus = HTTP_STATUS_SERVICEUNAVAIL;
              //rc = http_resp_error(&pConn->conn.sd, &pConn->conn.httpReq, HTTP_STATUS_SERVICEUNAVAIL, 1, HTTP_STATUS_STR_SERVERBUSY, NULL);
            }
            break;
          }
          //
          // Intentional fallt-through
          //

        case MEDIA_ACTION_CANNED_HTML:
        case MEDIA_ACTION_CANNED_INFLASH:
        case MEDIA_ACTION_CANNED_WEBM:

          if(methodAuto == STREAM_METHOD_HTTPLIVE) {

            rc = process_httplive(pConn, &mediaRsrc, &proc, &httpStatus);

          } else if(methodAuto == STREAM_METHOD_DASH) {

            rc = process_dash(pConn, &mediaRsrc, &proc, &httpStatus);

          } else if(methodAuto == STREAM_METHOD_RTMP || methodAuto == STREAM_METHOD_FLASHHTTP ||
                    methodAuto == STREAM_METHOD_FLVLIVE || methodAuto == STREAM_METHOD_MKVLIVE) {

            rc = process_liveoverhttp(pConn, &mediaRsrc, &proc, action, methodAuto);
  
          } else if(methodAuto == STREAM_METHOD_RTSP ||
                    methodAuto == STREAM_METHOD_RTSP_INTERLEAVED ||
                    methodAuto == STREAM_METHOD_RTSP_HTTP) {

            rc = process_rtsplive(pConn, &mediaRsrc, &proc, action, startProc);

          } else if(methodAuto == STREAM_METHOD_TSLIVE) {

            rc = process_tslive(pConn, &mediaRsrc, &proc, action, startProc);

          } else {

            //
            // Use the default handler
            //
            //rc = srv_ctrl_live(&pConn->conn, &forbidden, NULL);
            //forbidden = 1;
            httpStatus = HTTP_STATUS_FORBIDDEN;
          }

          break;

        case MEDIA_ACTION_TMN:

          if(pConn->conn.pListenCfg->urlCapabilities & URL_CAP_TMN) {
            rc = srv_ctrl_loadtmn(&pConn->conn);
          } else {
            httpStatus = HTTP_STATUS_FORBIDDEN;
          }

        break;

        case MEDIA_ACTION_IMG:

          if(pConn->conn.pListenCfg->urlCapabilities & URL_CAP_IMG) {
            rc = srv_ctrl_loadimg(&pConn->conn, &httpStatus);
          } else {
            httpStatus = HTTP_STATUS_FORBIDDEN;
          }

          break;
         
        case MEDIA_ACTION_LIST:

          if(pConn->conn.pListenCfg->urlCapabilities & URL_CAP_LIST) {
            rc = srv_ctrl_listmedia(&pConn->conn, &httpStatus);
          } else {
            httpStatus = HTTP_STATUS_FORBIDDEN;
          }
          break;

        case MEDIA_ACTION_INDEX_FILELIST:

          //
          // Send back a file from the '/html' directory
          //

          parg = pConn->conn.httpReq.puri;

          if(!strcmp(pConn->conn.httpReq.puri, "/")) {

            if((pConn->conn.pListenCfg->urlCapabilities & URL_CAP_DIRLIST)) { 

              if(pdevtype->devtype == STREAM_DEVICE_TYPE_MOBILE ||
                pdevtype->devtype == STREAM_DEVICE_TYPE_TABLET) {
                parg = "flist.html";
              } else if(pdevtype->devtype == STREAM_DEVICE_TYPE_UNSUPPORTED) {
                parg = "unsupported.html";
                LOG(X_WARNING("Request from unsupported device '%s'"), userAgent);
              } else {
                parg = "main.html";
              }

            } else {
              httpStatus = HTTP_STATUS_FORBIDDEN;
            }
          }

          if(httpStatus == HTTP_STATUS_OK) {
            mediadb_prepend_dir2(pConn->conn.pCfg->pMediaDb->homeDir,
                                 VSX_HTML_PATH, parg, tmpfile, sizeof(tmpfile));

            if(srv_ctrl_islegal_fpath(tmpfile) && fileops_stat(tmpfile, &st) == 0 && !(st.st_mode & S_IFDIR)) {
  
              rc = http_resp_sendfile(&pConn->conn, &httpStatus, tmpfile,
                                      CONTENT_TYPE_TEXTHTML, HTTP_ETAG_AUTO);
            } else {
              httpStatus = HTTP_STATUS_FORBIDDEN;
            }
          } // end of httpStatus = HTTP_STATUS_OK

          break;

        case MEDIA_ACTION_CHECK_TYPE:
          snprintf(tmpfile, sizeof(tmpfile), "action=%d&method=%d&methodAuto=%d", 
                   checkAction, reqMethod, methodAuto);
          rc = http_resp_send(&pConn->conn.sd, &pConn->conn.httpReq,
                    HTTP_STATUS_OK, (unsigned char *) tmpfile, strlen(tmpfile));

          break;

/*
        case MEDIA_ACTION_CHECK_EXISTS:

//TODO:  call procdb_findName to  remap parg to HTML/[TOKEN]/out.m3u
// return check if .m3u8 file exists yet or not
        parg = pConn->conn.httpReq.puri;
        mediadb_prepend_dir2(pConn->conn.pCfg->pMediaDb->homeDir,
                             VSX_HTML_PATH, parg, tmpfile, sizeof(tmpfile));
fprintf(stderr, "TMPFILE:'%s'\n", tmpfile);
        snprintf(tmpfile, sizeof(tmpfile), "action=%d&method=%d&methodAuto=%d", 
                 checkAction, reqMethod, methodAuto);
        rc = http_resp_send(&pConn->conn.sd, &pConn->conn.httpReq,
                  HTTP_STATUS_OK, (unsigned char *) tmpfile, strlen(tmpfile));


          break;
*/

        default:
          httpStatus = HTTP_STATUS_FORBIDDEN;
          break;
      } // end of switch(action...

    } // end of if(httpStatus == HTTP_STATUS_OK) {

    //if(forbidden || HTTP_STATUS_FORBIDDEN == httpStatus) {
    if(httpStatus != HTTP_STATUS_OK) {

      LOG(X_INFO("HTTP %s :%d%s %s from %s:%d"), pConn->conn.httpReq.method,
           ntohs(INET_PORT(pConn->conn.pListenCfg->sa)), pConn->conn.httpReq.puri,
           http_lookup_statuscode(httpStatus),
           FORMAT_NETADDR(pConn->conn.sd.sa, tmpfile, sizeof(tmpfile)), ntohs(INET_PORT(pConn->conn.sd.sa)));

      rc = http_resp_error(&pConn->conn.sd, &pConn->conn.httpReq, httpStatus, 1, NULL, pauthbuf); 
      break;
    }


    //
    // Respect Connection: keep-aliive
    //
    if(pConn->conn.httpReq.connType != HTTP_CONN_TYPE_KEEPALIVE) {
      break;
    }

  } while(rc >= 0);

  netio_closesocket(&pConn->conn.sd.netsocket);

  LOG(X_DEBUG("Mgr Connection ended %s:%d '%s'"), 
      FORMAT_NETADDR(pConn->conn.sd.sa, tmpfile, sizeof(tmpfile)), ntohs(INET_PORT(pConn->conn.sd.sa)), 
      pConn->conn.httpReq.url);

}

static void srvmgr_main_proc(void *pArg) {
  NETIO_SOCK_T netsocksrv;  
  int rc;
  struct sockaddr_storage sa;
  int haveAuth = 0;
  char buf[SAFE_INET_NTOA_LEN_MAX];
  SRV_MGR_LISTENER_CFG_T *pSrvListen = (SRV_MGR_LISTENER_CFG_T *) pArg;

  pSrvListen->listenCfg.urlCapabilities = URL_CAP_LIVE | URL_CAP_FILEMEDIA | 
                                          URL_CAP_TSHTTPLIVE | URL_CAP_MOOFLIVE | URL_CAP_IMG; 

  if(!pSrvListen->pStart->disableDirListing) {
    pSrvListen->listenCfg.urlCapabilities |= (URL_CAP_TMN | URL_CAP_LIST | URL_CAP_DIRLIST);
  }

  memset(&netsocksrv, 0, sizeof(netsocksrv));
  memcpy(&sa, &pSrvListen->listenCfg.sa, sizeof(sa));
  netsocksrv.flags = pSrvListen->listenCfg.netflags;

  if((NETIOSOCK_FD(netsocksrv) = net_listen((const struct sockaddr *) &sa, 15)) == INVALID_SOCKET) {
    return;
  }

  pSrvListen->listenCfg.pnetsockSrv = &netsocksrv;

  if(pSrvListen->listenCfg.pAuthStore && IS_AUTH_CREDENTIALS_SET(pSrvListen->listenCfg.pAuthStore)) {
    haveAuth = 1;
  }

  LOG(X_INFO("Listening for requests on "URL_HTTP_FMT_STR" %s%s%s"),
              URL_HTTP_FMT_ARGS2(&pSrvListen->listenCfg, FORMAT_NETADDR(sa, buf, sizeof(buf))),
            (haveAuth ? " (Using auth)" : ""),
             pSrvListen->pStart->pCfg->cfgShared.uipwd ? " (Using password)" : "",
            (pSrvListen->pStart->disableDirListing ? " (Directory listing disabled)" : ""));

  //
  // Service any client connections on the http listening port
  //
  rc = srvlisten_loop(&pSrvListen->listenCfg, srvmgr_client_proc);

  LOG(X_WARNING("HTTP%s %s:%d listener thread exiting with code: %d"), 
               ((pSrvListen->listenCfg.netflags & NETIO_FLAG_SSL_TLS) ? "S" : ""),
               FORMAT_NETADDR(sa, buf, sizeof(buf)), ntohs(INET_PORT(sa)), rc);

  pSrvListen->listenCfg.pnetsockSrv = NULL;
  netio_closesocket(&netsocksrv);


}

typedef void (*THREAD_FUNC) (void *);

static int start_listener(SRV_MGR_LISTENER_CFG_T *pMgrListenCfg, THREAD_FUNC thread_func, int async) {
  pthread_t ptd;
  pthread_attr_t attr;
  int rc = 0;

  if(!pMgrListenCfg || !pMgrListenCfg->pStart ||  !pMgrListenCfg->listenCfg.pConnPool || 
     pMgrListenCfg->listenCfg.max <= 0 || !pMgrListenCfg->listenCfg.pCfg) {
    return -1;
  }


  if((pMgrListenCfg->listenCfg.netflags & NETIO_FLAG_SSL_TLS) && !netio_ssl_enabled(1)) {
    LOG(X_ERROR("SSL not enabled"));
    return -1;
  }

  if(async) {

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if(pthread_create(&ptd,
                      &attr,
                    (void *) thread_func,
                    (void *) pMgrListenCfg) != 0) {

      LOG(X_ERROR("Unable to create live listener thread"));
      return -1;
    }

  } else {
    thread_func(pMgrListenCfg);
    rc = 0;
  }

  return rc;

}

/*
int test() {
  META_FILE_T mf;
  char profs[8][META_FILE_PROFILESTR_MAX];
  int rc;

  memset(&mf, 0, sizeof(mf));
  sprintf(mf.devicefilterstr,"ipad");
  sprintf(mf.profilefilterstr,"3");
  rc = metafile_open("/opt/tmp/testmetafile", &mf, 0, 0);

  rc = metafile_findprofs("/opt/tmp/testmetafile", mf.devicefilterstr, profs, 8);

  return 0;  
}
*/

static int init_listener(POOL_T *pPool, 
                         SRV_MGR_START_CFG_T *pStart, 
                         SRV_MGR_LISTENER_CFG_T *pMgrListenerCfg,
                         unsigned int max,
                         SRV_LISTENER_CFG_T *pListenerSrc) {
  unsigned int idx;
  SRV_MGR_CONN_T *pConn;

  //
  // Create and init each available client connection instance
  //
  if(pool_open(pPool, max, sizeof(SRV_MGR_CONN_T), 1) != 0) {
    return -1;
  }

  pConn = (SRV_MGR_CONN_T *) pPool->pElements;
  while(pConn) {
    NETIOSOCK_FD(pConn->conn.sd.netsocket) = INVALID_SOCKET;
    pConn->conn.pCfg = &pStart->pCfg->cfgShared;
    memcpy(&pConn->cfg, pStart, sizeof(pConn->cfg));
    pConn = (SRV_MGR_CONN_T *) pConn->conn.pool.pNext;
  }

 
  for(idx = 0; idx < SRV_LISTENER_MAX; idx++) {
    pMgrListenerCfg[idx].pStart = pStart;
    memcpy(&pMgrListenerCfg[idx].listenCfg, &pListenerSrc[idx], sizeof(pMgrListenerCfg[idx].listenCfg));
    //memset(&pListenerSrc[idx], 0, sizeof(SRV_LISTENER_CFG_T));

    pMgrListenerCfg[idx].listenCfg.max = pStart->pCfg->pParams->httpmax;
    pMgrListenerCfg[idx].listenCfg.pConnPool = pPool;
    pMgrListenerCfg[idx].listenCfg.pCfg = pStart->pCfg;
  }

  return 0;
}

static int check_other_listeners(const SRV_LISTENER_CFG_T *pListenerThis,
                                 unsigned int max,
                                 const SRV_LISTENER_CFG_T *pListener1,
                                 const SRV_LISTENER_CFG_T *pListener2,
                                 const SRV_LISTENER_CFG_T *pListener3) {
  unsigned int idx;
  int rc;
  unsigned int max1 = SRV_LISTENER_MAX;

  if(!pListenerThis) {
    return -1;
  }

  for(idx = 0; idx < max; idx++) {

    if(!pListenerThis[idx].active) {
      continue;
    }

    if(pListener1 && (rc = vsxlib_check_prior_listeners(pListener1, max1,  
                             (const struct sockaddr *) &pListenerThis[idx].sa)) > 0) {
      return idx + 1;
    }

    if(pListener2 && (rc = vsxlib_check_prior_listeners(pListener2, max1,  
                             (const struct sockaddr *) &pListenerThis[idx].sa)) > 0) {
      return idx + 1;
    }

    if(pListener3 && (rc = vsxlib_check_prior_listeners(pListener3, max1,  
                             (const struct sockaddr *) &pListenerThis[idx].sa)) > 0) {
      return idx + 1;
    }

  }

  return 0;
}

int srvmgr_start(SRV_MGR_PARAMS_T *pParams) {
  MEDIADB_DESCR_T mediaDb;
  HTTPLIVE_DATA_T httpLiveDatas[IXCODE_VIDEO_OUT_MAX];
  MOOFSRV_CTXT_T  moofCtxts[IXCODE_VIDEO_OUT_MAX];
  SRV_CONF_T *pConf = NULL;
  SRV_START_CFG_T cfg;
  VSXLIB_STREAM_PARAMS_T params;
  unsigned int idx;
  char tmp[128];
  char tmp2[32];
  char buf[VSX_MAX_PATH_LEN];
  char launchpath[VSX_MAX_PATH_LEN];
  char mediadir[VSX_MAX_PATH_LEN];
  char dbdir[VSX_MAX_PATH_LEN];
  const char *p;
  struct stat st;
  pthread_t ptdMediaDb;
  pthread_attr_t attrMediaDb;
  LIC_INFO_T lic;
  SRV_MGR_CONN_T *pConn;
  SRV_MGR_START_CFG_T start;
  SRV_LISTENER_CFG_T listenerTmp[SRV_LISTENER_MAX_HTTP];
  SRV_MGR_LISTENER_CFG_T listenerHttp[SRV_LISTENER_MAX_HTTP];
  SRV_MGR_LISTENER_CFG_T listenerRtmpProxy[SRV_LISTENER_MAX_RTMP];
  SRV_MGR_LISTENER_CFG_T listenerRtspProxy[SRV_LISTENER_MAX_RTSP];
  SRV_MGR_LISTENER_CFG_T listenerHttpProxy[SRV_LISTENER_MAX_HTTP];
  POOL_T poolHttp;
  POOL_T poolRtmpProxy;
  POOL_T poolRtspProxy;
  POOL_T poolHttpProxy;
  AUTH_CREDENTIALS_STORE_T httpAuthStores[SRV_LISTENER_MAX];
  TIME_VAL tmstart = 0;
  unsigned int outidx;
  const char *log_tag = "main";
  int rc = 0;
  
  if(!pParams) {
    return -1;
  }

//return test();

  memset(&start, 0, sizeof(start));
  memset(&lic, 0, sizeof(lic));
  memset(&cfg, 0, sizeof(cfg));
  memset(&params, 0, sizeof(params));
  memset(&httpLiveDatas, 0, sizeof(httpLiveDatas));
  memset(&moofCtxts, 0, sizeof(moofCtxts));
  memset(&mediaDb, 0, sizeof(mediaDb));
  memset(listenerHttp, 0, sizeof(listenerHttp));
  memset(listenerRtmpProxy, 0, sizeof(listenerRtmpProxy));
  memset(listenerRtspProxy, 0, sizeof(listenerRtspProxy));
  memset(listenerHttpProxy, 0, sizeof(listenerHttpProxy));
  memset(&httpAuthStores, 0, sizeof(httpAuthStores));

#if defined(VSX_HAVE_SSL)
  //
  // TODO: This should be done in vsxlib_open, which is currently not called in mgr
  //
  pthread_mutex_init(&g_ssl_mtx, NULL);
#endif // VSX_HAVE_SSL

  params.httpmax = VSX_CONNECTIONS_DEFAULT;

  pParams->pCfg = &cfg;
  cfg.cfgShared.pMediaDb = &mediaDb;
  cfg.pParams = &params;
  snprintf((char *) mediaDb.tn_suffix, sizeof(mediaDb.tn_suffix), "_xn");
  cfg.conf_file_name = MGR_CONF_FILE;
  cfg.conf_file_path = MGR_CONF_PATH;
  cfg.plogfile = pParams->logfile;
  cfg.verbosity = params.verbosity = g_verbosity;

  start.pCfg = &cfg;
  start.pProcList = &pParams->procList;
  start.ptmstart = &tmstart;
  start.plic = &lic;
  start.pListenerRtmpProxy = listenerRtmpProxy;
  start.pListenerRtspProxy = listenerRtspProxy;
  start.pListenerHttpProxy = listenerHttpProxy;

  params.logmaxsz = LOGGER_MAX_FILE_SZ;
  params.logrollmax = LOGGER_MAX_FILES;
  params.disable_root_dirlist = cfg.cfgShared.disable_root_dirlist = pParams->disable_root_dirlist;

  //
  // Load properties which are common between vsx & vsxmgr
  //
  if(!(pConf = srv_init_conf(pParams->listenaddr[0], pParams->mediadir, 
                      pParams->homedir, pParams->confpath,
                      pParams->dbdir, !pParams->nodb, 
                      pParams->curdir, 
                      MGR_RTMPPROXY_MAX, 
                      MGR_RTSPPROXY_MAX, 
                      &cfg, 
                      httpAuthStores))) {
    return -1;
  }

  vsxlib_initlog(cfg.verbosity, cfg.plogfile, cfg.phomedir, params.logmaxsz, params.logrollmax, log_tag);

  //LOG(X_DEBUG("LISSSSTTT :%d :%d"), htons(INET_PORT(cfg.listenHttp[0].sa)), htons(INET_PORT(cfg.listenHttp[1].sa))); 

  LOG(X_INFO("Read configuration from %s"), cfg.pconfpath);

  if(cfg.usedb && cfg.pmediadir != NULL && cfg.pmediadir[0] != '\0' &&
    fileops_stat(cfg.pmediadir, &st) != 0 &&
    mediadb_prepend_dir(cfg.phomedir, cfg.pmediadir, mediadir, sizeof(mediadir)) >= 0 &&
    fileops_stat(mediadir, &st) == 0) {
    cfg.pmediadir = mediadir;
  }

  //
  // Get config entries specific to the mgr
  //

  if((cfg.cfgShared.enable_symlink = params.enable_symlink)) {
    LOG(X_DEBUG("Symlink following enabled."));
  }

  if((cfg.cfgShared.disable_root_dirlist)) {
    LOG(X_DEBUG("Root level directory listing disabled."));
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LAUNCHMEDIA)) && p[0] != '\0') {
    start.plaunchpath = p;
  } else {
    mediadb_prepend_dir(cfg.phomedir, MGR_LAUNCHCHILD, launchpath, sizeof(launchpath));
    start.plaunchpath = launchpath;
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_PORTRANGE)) &&
    get_port_range(p, &pParams->procList.minStartPort, &pParams->procList.maxStartPort) < 0) {

    LOG(X_ERROR("Invalid %s"), SRV_CONF_KEY_PORTRANGE);
    conf_free(pConf);
    return -1;
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DISABLE_LISTING)) && p[0] != '\0') {
    if((start.disableDirListing = atoi(p))) {
      LOG(X_DEBUG("Media directory listing and disabled"));
    }
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_MEDIA)) && p[0] != '\0') {
    start.pProcList->maxInstances = atoi(p); 
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_MEDIA_XCODE)) && p[0] != '\0') {
    start.pProcList->maxXcodeInstances = atoi(p); 
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAX_MBPS)) && p[0] != '\0') {
    start.pProcList->maxMbbpsTot = atoi(p); 
  }

  if((p = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_EXPIRE_CHILD_SEC)) && p[0] != '\0') {
    start.pProcList->procexpiresec = atoi(p); 
  }
  if(start.pProcList->procexpiresec <= 0) {
    start.pProcList->procexpiresec = PROCDB_EXPIRE_IDLE_SEC;
  }

  if(fileops_stat(start.plaunchpath, &st) != 0) {
    LOG(X_ERROR("Launch script %s does not exist"), start.plaunchpath);
    conf_free(pConf);
    return -1;
  }

  if(cfg.pdbdir == NULL) {
    mediadb_prepend_dir(cfg.pmediadir, VSX_DB_PATH, buf, sizeof(buf));
    cfg.pdbdir = buf;
  }

  //
  // create a 'unique' key for the mediaDir
  //
  idx = mp2ts_crc((const unsigned char *) cfg.pdbdir, strlen(cfg.pdbdir));
  snprintf(tmp2, sizeof(tmp2), "%2.2x%2.2x%2.2x%2.2x",
          (((unsigned char *)&idx)[0]),  (((unsigned char *)&idx)[1]),
          (((unsigned char *)&idx)[2]),  (((unsigned char *)&idx)[3]));
  mediadb_prepend_dir(cfg.pdbdir, tmp2, dbdir, sizeof(dbdir));
  cfg.pdbdir = dbdir;

  mediaDb.homeDir= cfg.phomedir;
  mediaDb.dbDir = cfg.pdbdir;
  mediaDb.mediaDir = cfg.pmediadir;
  mediaDb.avcthumb = cfg.pavcthumb;
  mediaDb.avcthumblog = cfg.pavcthumblog;
  mediadb_parseprefixes(cfg.pignoredirprfx, mediaDb.ignoredirprefixes,
                        MEDIADB_PREFIXES_MAX);
  mediadb_parseprefixes(cfg.pignorefileprfx, mediaDb.ignorefileprefixes,
                        MEDIADB_PREFIXES_MAX);
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //
    // Set HTTP Live properties
    //
    if(cfg.phttplivedir) {
      strncpy(httpLiveDatas[outidx].dir, cfg.phttplivedir, sizeof(httpLiveDatas[outidx].dir) - 1);
    } else {
      mediadb_prepend_dir(cfg.phomedir, VSX_HTTPLIVE_PATH,
                          httpLiveDatas[outidx].dir, sizeof(httpLiveDatas[outidx].dir));
    }
    httplive_format_path_prefix(httpLiveDatas[outidx].fileprefix,
                                sizeof(httpLiveDatas[outidx].fileprefix),
                                outidx, HTTPLIVE_TS_NAME_PRFX, NULL);

    cfg.cfgShared.pHttpLiveDatas[outidx] = &httpLiveDatas[outidx];

    //
    // Set DASH Live properties
    //
    if(cfg.pmoofdir) {
      strncpy((char *) moofCtxts[outidx].dashInitCtxt.outdir, cfg.pmoofdir, 
              sizeof(moofCtxts[outidx].dashInitCtxt.outdir) - 1);
    } else {
      mediadb_prepend_dir((char *) cfg.phomedir, VSX_MOOF_PATH, (char *) moofCtxts[outidx].dashInitCtxt.outdir, 
                        sizeof(moofCtxts[outidx].dashInitCtxt.outdir));
    }

    cfg.cfgShared.pMoofCtxts[outidx] = &moofCtxts[outidx];

  }

  VSX_DEBUG_MGR( LOG(X_DEBUG("MGR - homedir:'%s', mediaDir:'%s', dbDir:'%s'"),
                    cfg.phomedir, mediaDb.mediaDir, mediaDb.dbDir));



  if(procdb_create(&pParams->procList) < 0) {
    conf_free(pConf);
    return -1;
  }

  LOG(X_INFO("Home dir: '%s', Media dir: '%s', database %s, %d max connections"), 
             cfg.phomedir, cfg.pmediadir ? cfg.pmediadir : "<not set>",
             cfg.usedb ? cfg.pdbdir : "disabled", params.httpmax);

  if(start.pProcList->maxInstances > 0 || start.pProcList->maxXcodeInstances > 0 ||
     start.pProcList->maxMbbpsTot > 0) {
    LOG(X_INFO(SRV_CONF_KEY_MAX_MEDIA": %d, "SRV_CONF_KEY_MAX_MEDIA_XCODE": %d, "
               SRV_CONF_KEY_MAX_MBPS": %d"),
       start.pProcList->maxInstances, start.pProcList->maxXcodeInstances,
       start.pProcList->maxMbbpsTot);
  }

#if defined(VSX_HAVE_LICENSE)

  if(license_init(&lic, cfg.licfilepath, cfg.phomedir) < 0) {
    conf_free(pConf);
    return -1;
  } else if((lic.capabilities & LIC_CAP_NO_MGR)) {
    LOG(X_ERROR("VSXMgr not enabled in license"));
    return -1;
  } 

  tmstart = timer_GetTime();

#endif //VSX_HAVE_LICENSE


  //
  // Create and init each available http connection instance
  //
  if(init_listener(&poolHttp, &start, listenerHttp, params.httpmax, cfg.listenHttp) < 0) {
    return -1;
  }

  memset(cfg.listenHttp, 0, sizeof(cfg.listenHttp));
  for(idx = 0; idx < SRV_LISTENER_MAX_HTTP; idx++) {
    memcpy(&listenerTmp[idx], &listenerHttp[idx].listenCfg, sizeof(listenerTmp[idx]));
  }

  if(cfg.usedb) {

    if(!mediaDb.mediaDir) {
      LOG(X_CRITICAL("Media directory not set.  Please specify your media directory by modifying '%s' or by passing the '--media=' command line option"),
               MGR_CONF_PATH);
      conf_free(pConf);
      return -1;
    }

    //
    // Start the media database management thread
    //
    pthread_attr_init(&attrMediaDb);
    pthread_attr_setdetachstate(&attrMediaDb, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&ptdMediaDb,
                      &attrMediaDb,
                      (void *) mediadb_proc,
                      (void *) &mediaDb) != 0) {

      LOG(X_ERROR("Unable to create media database thread"));
      conf_free(pConf);
      return -1;
    }
  }

  //fprintf(stderr, "TRY RTMP %d '%s' 1:%s:%d\n", params.rtmplivemax ,params.rtmpliveaddr[0], INET_NTOP(cfg.listenRtmp[0].sa, tmp, sizeof(tmp)), ntohs(INET_PORT(cfg.listenRtmp[0].sa)));
  memset(cfg.listenRtmp, 0, sizeof(cfg.listenRtmp));
  if(params.rtmplivemax > 0 && params.rtmpliveaddr[0] &&
     (rc = vsxlib_parse_listener((const char **) params.rtmpliveaddr, SRV_LISTENER_MAX_RTMP,
                                  cfg.listenRtmp, URL_CAP_RTMPLIVE, NULL)) > 0) {

    //
    // Start the RTMP proxy
    //
    if((rc = check_other_listeners(cfg.listenRtmp, SRV_LISTENER_MAX_RTMP,
                                   listenerTmp, NULL, NULL)) > 0) {
      LOG(X_WARNING("RTMP server listener %s:%d cannot be shared with another protocol"),
          FORMAT_NETADDR(cfg.listenRtmp[rc - 1].sa, tmp, sizeof(tmp)), htons(INET_PORT(cfg.listenRtmp[rc - 1].sa)));
    } else {

      //
      // Create and init each available client connection instance
      //
      if(init_listener(&poolRtmpProxy, &start, listenerRtmpProxy, params.rtmplivemax, cfg.listenRtmp) < 0) {
        return -1;
      }

      //
      // Start the RTMP listener(s)
      //
      for(idx = 0; idx < sizeof(listenerRtmpProxy) / sizeof(listenerRtmpProxy[0]); idx++) {
        if(listenerRtmpProxy[idx].listenCfg.active &&
           ((rc = vsxlib_ssl_initserver(&params, &listenerRtmpProxy[idx].listenCfg)) < 0 ||
            (rc = start_listener(&listenerRtmpProxy[idx], srvmgr_rtmpproxy_proc, 1) < 0))) {
          return -1;
        }
      }

    }

  } else {
    //cfg.saListenRtmp.sin_port = 0;
  }

  memset(cfg.listenRtsp, 0, sizeof(cfg.listenRtsp));
  if(params.rtsplivemax > 0 && params.rtspliveaddr[0] &&
     (rc = vsxlib_parse_listener((const char **) params.rtspliveaddr, SRV_LISTENER_MAX_RTSP,
                                  cfg.listenRtsp, URL_CAP_RTSPLIVE, NULL)) > 0) {

    //
    // Start the RTSP proxy
    //
    if((rc = check_other_listeners(cfg.listenRtsp, SRV_LISTENER_MAX_RTSP,
                                   listenerTmp, cfg.listenRtmp,  NULL)) > 0) {
      LOG(X_WARNING("RTSP server listener %s:%d cannot be shared with another protocol"),
          FORMAT_NETADDR(cfg.listenRtsp[rc - 1].sa, tmp, sizeof(tmp)), htons(INET_PORT(cfg.listenRtsp[rc - 1].sa)));
    } else {

      //
      // Create and init each available client connection instance
      //
      if(init_listener(&poolRtspProxy, &start, listenerRtspProxy, params.rtsplivemax, cfg.listenRtsp) < 0) {
        return -1;
      }

      //
      // Start the RTSP listener(s)
      //
      for(idx = 0; idx < sizeof(listenerRtspProxy) / sizeof(listenerRtspProxy[0]); idx++) {
        if(listenerRtspProxy[idx].listenCfg.active &&
           ((rc = vsxlib_ssl_initserver(&params, &listenerRtspProxy[idx].listenCfg)) < 0 ||
            (rc = start_listener(&listenerRtspProxy[idx], srvmgr_rtspproxy_proc, 1) < 0))) {
          return -1;
        }
      }

    }

  } else {
    //cfg.saListenRtsp.sin_port = 0;
  }


  if(params.flvlivemax > 0 && params.flvliveaddr[0] &&
     (rc = vsxlib_parse_listener((const char **) params.flvliveaddr, SRV_LISTENER_MAX_HTTP,
                                  cfg.listenHttp, URL_CAP_FLVLIVE, NULL)) > 0) {

    //
    // Start the HTTP / FLV proxy
    //
    if((rc = check_other_listeners(cfg.listenHttp, SRV_LISTENER_MAX_HTTP,
                                   listenerTmp, cfg.listenRtmp, cfg.listenRtsp)) > 0) {
      LOG(X_WARNING("HTTP (proxy) server listener %s:%d cannot be shared with another protocol"),
          FORMAT_NETADDR(cfg.listenHttp[rc - 1].sa, tmp, sizeof(tmp)), htons(INET_PORT(cfg.listenHttp[rc - 1].sa)));
    } else {

      //
      // Create and init each available client connection instance
      //
      if(init_listener(&poolHttpProxy, &start, listenerHttpProxy, params.flvlivemax, cfg.listenHttp) < 0) {
        return -1;
      }

      //
      // Start the HTTP / FLV listener(s)
      //
      for(idx = 0; idx < sizeof(listenerHttpProxy) / sizeof(listenerHttpProxy[0]); idx++) {
        if(listenerHttpProxy[idx].listenCfg.active &&
           ((rc = vsxlib_ssl_initserver(&params, &listenerHttpProxy[idx].listenCfg)) < 0 ||
            (rc = start_listener(&listenerHttpProxy[idx], srvmgr_httpproxy_proc, 1) < 0))) {
          return -1;
        }
      }

    }

  } else {
  //  cfg.saListenFlvLive.sin_port = 0;
  }


  //
  // Start the HTTP listener(s)
  //
  for(idx = 0; idx < sizeof(listenerHttp) / sizeof(listenerHttp[0]); idx++) {
    if(idx > 0 && listenerHttp[idx].listenCfg.active) {

      if(idx > 0 && ((rc = vsxlib_ssl_initserver(&params, &listenerHttp[idx].listenCfg)) < 0 ||
         (rc = start_listener(&listenerHttp[idx], srvmgr_main_proc, 1) < 0))) {
        break;
      }
    }
  }

  if((rc = vsxlib_ssl_initserver(&params, &listenerHttp[0].listenCfg)) >= 0) {
     rc = start_listener(&listenerHttp[0], srvmgr_main_proc, 1);
  }

  if(rc >= 0) {
    while(g_proc_exit == 0) {
      usleep(500000);
    }
  }

  LOG(X_DEBUG("Cleaning up"));

  // TODO: on signal , create mgr_close which closes all sockets and calls pool_close
  pthread_mutex_lock(&poolHttp.mtx);
  pConn = (SRV_MGR_CONN_T *) poolHttp.pInUse;
  while(pConn) {
    netio_closesocket(&pConn->conn.sd.netsocket);
    pConn = (SRV_MGR_CONN_T *) pConn->conn.pool.pNext;
  }
  pthread_mutex_unlock(&poolHttp.mtx);
  pool_close(&poolHttp, 1);



  procdb_destroy(&pParams->procList);
  conf_free(pConf);

  return rc;
}

