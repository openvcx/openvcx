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

#if defined(VSX_HAVE_SERVERMODE)


static int findsystempaths(const char *curdirArg, const char *homedirArg,
                           char *confpath, char *homepath, const char *defaultconffile,
                           const char *defaultconfpath) {
  struct stat st;
  char curdir[VSX_MAX_PATH_LEN];
  char path[VSX_MAX_PATH_LEN];
  const char *pconf = NULL;
  const char *phome = NULL;
  size_t sz;

  mediadb_prepend_dir(curdirArg, defaultconffile, path, sizeof(path));
  if(fileops_stat(path, &st) == 0) {
    pconf = path;
    phome = curdir;
  }

  if(!pconf && curdirArg) {
    sz = strlen(curdirArg);
    while(sz > 1 && curdirArg[sz - 1] == DIR_DELIMETER) {
      sz--;
    }
    curdir[sizeof(curdir) - 1] = '\0';
    strncpy(curdir, curdirArg, sizeof(curdir) - 1);
    curdir[sz] = '\0';
    sz = mediadb_getdirlen(curdir);
    curdir[sz] = '\0';
    mediadb_prepend_dir(curdir, defaultconfpath, path, sizeof(path));
    if(fileops_stat(path, &st) == 0) {
      pconf = path;
      phome = curdir;
    }
  }

  if(!pconf) {
    mediadb_prepend_dir(curdir, defaultconffile, path, sizeof(path));

    if(fileops_stat(path, &st) == 0) {
      pconf = path;
      phome = curdir;
    }
  }

  if(!pconf && homedirArg) {
    mediadb_prepend_dir(homedirArg, defaultconffile, path, sizeof(path));
    if(fileops_stat(path, &st) == 0) {
      pconf = path;
      phome = homedirArg;
    }
  }

  if(!pconf && homedirArg) {
    mediadb_prepend_dir(homedirArg, defaultconfpath, path, sizeof(path));
    if(fileops_stat(path, &st) == 0) {
      pconf = path;
      phome = homedirArg;
    }
  }

  if(pconf && confpath && strlen(pconf) < VSX_MAX_PATH_LEN - 1) {
    strncpy(confpath, pconf, VSX_MAX_PATH_LEN - 1);
  }
  if(homepath && (!phome || strlen(phome) < VSX_MAX_PATH_LEN - 1)) {
    if(!phome || phome[0] == '\0') {
#if defined(WIN32)
      // When using thumbnail creation (srvmediadb.c) "system" exec
      // of thumbnail program will setcwd into path of path thumbnail program,
      // which will break homepath references when homepath is "." 
      if(fileops_getcwd(homepath, VSX_MAX_PATH_LEN - 1) == NULL) {
        LOG(X_ERROR("Unable to get current directory path"));
        return -1;
      }
#else
      homepath[0] = '.'; 
      homepath[1] = '\0';
#endif // WIN32
      phome = homepath;

    } else {
      strncpy(homepath, phome, VSX_MAX_PATH_LEN - 1);
    }
  }

  if(pconf || phome) {
     return 0;
  } else {
    return -1;
  }
}

SRV_CONF_T *srv_init_conf(const char *listenArg, const char *mediaDirArg, 
                          const char *homeDirArg, const char *confPathArg, 
                          const char *dbDirArg, int usedb, 
                          const char *curdir, 
                          unsigned int rtmphardlimit, 
                          unsigned int rtsphardlimit,
                          unsigned int httphardlimit,
                          SRV_START_CFG_T *pCfg,
                          AUTH_CREDENTIALS_STORE_T *ppAuthStores) {
  int rc = 0;
  //SOCKET_LIST_T sockList;
  const char *devcfgfile = NULL;
  SRV_CONF_T *pConf = NULL;
  const char *parg = NULL;
  int i;
  enum URL_CAPABILITY urlCapability;
  char path[VSX_MAX_PATH_LEN];
  const char *listenaddr[SRV_LISTENER_MAX];
  struct stat st;

  if(!pCfg || !pCfg->pParams) {
    return NULL;
  }

  pCfg->verbosity = pCfg->pParams->verbosity;
  pCfg->usedb = usedb;
  pCfg->cfgShared.throttlerate = HTTP_THROTTLERATE;
  pCfg->cfgShared.throttleprebuf = HTTP_THROTTLEPREBUF;
  urlCapability = URL_CAP_BROADCAST;

  rc = findsystempaths(curdir, homeDirArg, pCfg->confpath, pCfg->homepath, 
                       pCfg->conf_file_name, pCfg->conf_file_path);
  pCfg->pconfpath = pCfg->confpath;
  pCfg->phomedir = pCfg->homepath;

  if(confPathArg) {
    pCfg->pconfpath = confPathArg;
  } else if(rc == 0 && (!pCfg->pconfpath || pCfg->pconfpath[0] == '\0')) {
    pCfg->pconfpath = pCfg->conf_file_name;
  } else if(rc != 0) {
    LOG(X_ERROR("Unable to locate %s"), pCfg->conf_file_name);
    return NULL;
  }

  pCfg->pParams->confpath = pCfg->pconfpath;
  //
  // Parse the configuration file
  //
  if(!(pConf = vsxlib_loadconf(pCfg->pParams))) {
    if(!confPathArg) {
      LOG(X_WARNING("Use '--conf=' to specify configuration path"));
    }
    return NULL;
  }

  if(!pCfg->pParams->devconfpath ) {
    pCfg->pParams->devconfpath = VSX_DEVICESFILE_PATH;
  }

  //LOG(X_INFO("Read configuration from %s"), pCfg->pconfpath);

  memset(listenaddr, 0, sizeof(listenaddr));
  if(listenArg) {
    //TODO: this should accept up to SRV_LISTENER_MAX
    listenaddr[0] = listenArg;
  } else {
    conf_load_addr_multi(pConf, listenaddr, SRV_LISTENER_MAX, SRV_CONF_KEY_LISTEN);

    if(!listenaddr[0]) {
      LOG(X_ERROR("'%s=' not found in configuration"), SRV_CONF_KEY_LISTEN);
      conf_free(pConf);
      return NULL;
    }
  }

  if((rc = vsxlib_parse_listener((const char **) listenaddr, SRV_LISTENER_MAX,
                            pCfg->listenHttp, urlCapability, ppAuthStores)) < 0) {
    conf_free(pConf);
    return NULL;
  }

  if(mediaDirArg == NULL) {
    if((pCfg->pmediadir = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MEDIADIR)) == NULL) {

    }
  } else {
    pCfg->pmediadir = mediaDirArg; 
  }

  if(homeDirArg == NULL) {
    pCfg->phomedir = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_HOME);
    if(pCfg->phomedir == NULL) {
      pCfg->phomedir = pCfg->homepath;
    }
  } else {
    pCfg->phomedir = homeDirArg;
  }

  if(dbDirArg == NULL) {
    pCfg->pdbdir = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DBDIR);
  } else {
    pCfg->pdbdir = dbDirArg;
  }

  vsxlib_setsrvconflimits(pCfg->pParams, rtmphardlimit, rtsphardlimit, 
                          rtmphardlimit, rtmphardlimit, httphardlimit);

  pCfg->outiface = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_INTERFACE);
  pCfg->pavcthumb = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_AVCTHUMB);
  pCfg->pavcthumblog = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_AVCTHUMBLOG);
  if(pCfg->plogfile == NULL) {
    pCfg->plogfile = pCfg->pParams->logfile;
  }
  pCfg->pignorefileprfx = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_IGNOREFILEPRFX);
  pCfg->pignoredirprfx = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_IGNOREDIRPRFX);
  pCfg->phttplivedir = pCfg->pParams->httplivedir;
  pCfg->httpliveduration = pCfg->pParams->httplive_chunkduration;
  pCfg->pmoofdir = pCfg->pParams->moofdir;

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_THUMBSMALL))) {
    strutil_parseDimensions(parg, &pCfg->cfgShared.pMediaDb->smTnWidth, 
                            &pCfg->cfgShared.pMediaDb->smTnHeight);
  }
  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_THUMBLARGE))) {
    strutil_parseDimensions(parg, &pCfg->cfgShared.pMediaDb->lgTnWidth, 
                            &pCfg->cfgShared.pMediaDb->lgTnHeight);
  }

  if((pCfg->cfgShared.pchannelchgr = 
         conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_CHANNELCHANGER))) {
    if(pCfg->cfgShared.pchannelchgr[0] == '\0') {
      pCfg->cfgShared.pchannelchgr = NULL;
    }
  }
  if((pCfg->cfgShared.livepwd = pCfg->pParams->livepwd)) {
    if(pCfg->cfgShared.livepwd[0] == '\0') {
      pCfg->cfgShared.livepwd = NULL;
    }
  }
  if((pCfg->cfgShared.uipwd = 
          conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_UIPWD))) {
    if(pCfg->cfgShared.uipwd[0] == '\0') {
      pCfg->cfgShared.uipwd = NULL;
    }
  }

  if(pCfg->pParams->tslivemax > 0) {
    //pCfg->maxtslive = pCfg->pParams->tslivemax;
  }

/*
  if(pCfg->pParams->flvlivemax > 0) {
    //pCfg->maxflvlive = pCfg->pParams->flvlivemax;
    pCfg->listenFlv[0].max = pCfg->pParams->flvlivemax;

    if(pCfg->pParams->flvliveaddr[0] && pCfg->pParams->flvliveaddr[0][0] != '\0') {
      if(capture_parseLocalAddr(pCfg->pParams->flvliveaddr[0], &sockList) <= 0) {
        LOG(X_ERROR("Invalid FLV listen address:port '%s'"), pCfg->pParams->flvliveaddr[0]);
        conf_free(pConf);
        return NULL;
       }
       //memcpy(&pCfg->saListenFlvLive, &sockList.salist[0], sizeof(pCfg->saListenFlvLive));
    }

  }
*/

  if(pCfg->pParams->rtplivemax > 0) {
    pCfg->maxrtp = pCfg->pParams->rtplivemax;
  }

  if(pCfg->pParams->rtmplivemax > 0) {
    //pCfg->listenRtmp[0].max = pCfg->pParams->rtmplivemax;

    if(pCfg->pParams->rtmpliveaddr[0] && pCfg->pParams->rtmpliveaddr[0][0] != '\0') {

      if(vsxlib_parse_listener(pCfg->pParams->rtmpliveaddr, 1, pCfg->listenHttp, URL_CAP_RTMPLIVE, NULL) < 0) {
        LOG(X_ERROR("Invalid RTMP listen address:port '%s'"), pCfg->pParams->rtmpliveaddr[0]);
        conf_free(pConf);
        return NULL;
       }
       //memcpy(&pCfg->saListenRtmp, &sockList.salist[0], sizeof(pCfg->saListenRtmp));
    }
  }

  if(pCfg->pParams->rtsplivemax > 0) {
    //pCfg->listenRtsp[0].max = pCfg->pParams->rtsplivemax;

    if(pCfg->pParams->rtspliveaddr[0] && pCfg->pParams->rtspliveaddr[0][0] != '\0') {

      if(vsxlib_parse_listener(pCfg->pParams->rtspliveaddr, 1, pCfg->listenHttp, URL_CAP_RTSPLIVE, NULL) < 0) {
        LOG(X_ERROR("Invalid RTSP listen address:port '%s'"), pCfg->pParams->rtspliveaddr[0]);
        conf_free(pConf);
        return NULL;
       }
       //memcpy(&pCfg->saListenRtsp, &sockList.salist[0], sizeof(pCfg->saListenRtsp));
       //pCfg->saListenRtsp = sockListTmp.salist[0];
       //pCfg->rtspliveport = ntohs(sockListTmp.salist[0].sin_port);
    }
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_MAXCONN))) {
    //if((pCfg->maxhttp = atoi(parg)) < 1) {
      //pCfg->maxhttp = VSX_CONNECTIONS_DEFAULT;
    //}
    //if(pCfg->maxhttp != VSX_CONNECTIONS_DEFAULT) {
    //  LOG(X_INFO("Max HTTP connection threads set to %u"), pCfg->maxhttp); 
    //}
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DISABLEDB))) {
    if(IS_CONF_VAL_TRUE(parg)) {
      pCfg->usedb = 0;
    }
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_DISABLEROOTLIST))) {
    if(IS_CONF_VAL_TRUE(parg)) {
      pCfg->cfgShared.disable_root_dirlist = 1;
    }
  }

  // Try to use [home dir]/[avc thumb script]
  if(pCfg->pavcthumb && pCfg->phomedir && pCfg->pavcthumb[0] != DIR_DELIMETER && 
     pCfg->pavcthumb[0] !='.') {
    mediadb_prepend_dir(pCfg->phomedir, pCfg->pavcthumb, pCfg->avcthumbpath, 
                       sizeof(pCfg->avcthumbpath));
    pCfg->pavcthumb = pCfg->avcthumbpath;
  }

#ifndef WIN32
  // On Win32, phomedir is expanded to the cwd path
  if(pCfg->pavcthumblog && pCfg->phomedir && pCfg->pavcthumblog[0] != DIR_DELIMETER && 
    pCfg->pavcthumblog[0] !='.') {
    mediadb_prepend_dir(pCfg->phomedir, pCfg->pavcthumblog, pCfg->avcthumblogpath, 
                        sizeof(pCfg->avcthumblogpath));
    pCfg->pavcthumblog = pCfg->avcthumblogpath;
  }
#endif // WIN32

  if((pCfg->cfgShared.propFilePath = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_PROPFILE))) {
    mediadb_prepend_dir(pCfg->phomedir, pCfg->cfgShared.propFilePath, pCfg->propfilepath, 
                        sizeof(pCfg->propfilepath));
    pCfg->cfgShared.propFilePath = pCfg->propfilepath;
  }

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_THROTTLERATE))) {
    pCfg->cfgShared.throttlerate = atof(parg);
  }
  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_THROTTLEPREBUF))) {
    pCfg->cfgShared.throttleprebuf = atof(parg);
  }

  //
  // Get the license contents
  //
  //if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_LICFILE))) {
  if(pCfg->pParams->licfilepath) {
    pCfg->licfilepath = pCfg->pParams->licfilepath;
  }

  //mediadb_prepend_dir(pCfg->phomedir, SRV_USER_PROP_FILE, pCfg->propfilepath, 
  //                      sizeof(pCfg->propfilepath));
  //pCfg->cfgShared.propFilePath = pCfg->propfilepath;

  if((parg = conf_find_keyval(pConf->pKeyvals, SRV_CONF_KEY_VERBOSE))) {
    if((i = atoi(parg)) > 0) {
      if(pCfg->verbosity == VSX_VERBOSITY_NORMAL) {
        logger_SetLevel(S_DEBUG);
        logger_AddStderr(S_DEBUG, LOG_OUTPUT_DEFAULT_STDERR);
        pCfg->verbosity++;
      } 
    }
  }

  //
  // Attempt alternate path relative to home dir
  //

  devcfgfile = pCfg->pParams->devconfpath;

  if(fileops_stat(devcfgfile, &st) < 0) {
    mediadb_prepend_dir(pCfg->phomedir, devcfgfile, path, sizeof(path));
    devcfgfile = path;
  }

  if(fileops_stat(devcfgfile, &st) < 0) {
    LOG(X_WARNING("Unable to find device profile configuration file %s"),
        devcfgfile);
    devtype_defaultcfg();
  } else {

    if(devtype_loadcfg(devcfgfile) < 0 ) {
      LOG(X_ERROR("Invalid device profile configuration file %s"), devcfgfile);
      conf_free(pConf);
      return NULL;
    }
  }

  return pConf;
}


#endif // VSX_HAVE_SERVERMODE
