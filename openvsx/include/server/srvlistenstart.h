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


#ifndef __SERVER_LISTEN_START_H__
#define __SERVER_LISTEN_START_H__

#include "unixcompat.h"

#ifndef WIN32

#include <arpa/inet.h>

#endif // WIN32

#include "srvcmd.h"

/*
//
// Maximum unique (http|https://address:port) listeners 
//
#define SRV_LISTENER_MAX          4 
#define SRV_LISTENER_MAX_HTTP     SRV_LISTENER_MAX 
#define SRV_LISTENER_MAX_RTMP     SRV_LISTENER_MAX 
#define SRV_LISTENER_MAX_RTSP     SRV_LISTENER_MAX 
*/

typedef struct SRV_LISTENER_CFG {
  int                             active;
  pthread_mutex_t                 mtx;
  unsigned int                    idxCfg;   // idx of this listener [ 0 < SRV_LISTENER_MAX_xxx ] 
                                            // within the group of this type
  unsigned int                    max;      // max available client connections (from pool) within 
                                            // entire listener group of this type
  struct sockaddr_storage         sa;
  NETIO_FLAG_T                    netflags;
  NETIO_SOCK_T                   *pnetsockSrv;
  POOL_T                         *pConnPool;
  enum URL_CAPABILITY             urlCapabilities;
  struct AUTH_CREDENTIALS_STORE  *pAuthStore;
  char                            tid_tag[LOGUTIL_TAG_LENGTH];
  struct SRV_START_CFG           *pCfg;
} SRV_LISTENER_CFG_T;

typedef struct SRV_START_CFG {

  SRV_LISTENER_CFG_T    listenHttp[SRV_LISTENER_MAX_HTTP];
  SRV_LISTENER_CFG_T    listenRtmp[SRV_LISTENER_MAX_RTMP];
  SRV_LISTENER_CFG_T    listenRtsp[SRV_LISTENER_MAX_RTSP];

  unsigned int        maxrtp;
  const unsigned int *prtspsessiontimeout;
  const int          *prtsprefreshtimeoutviartcp;
  const char         *conf_file_name;
  const char         *conf_file_path;
  const char         *pmediadir;
  const char         *pconfpath;
  const char         *phomedir;
  const char         *plogdir;
  const char         *plogfile;
  int                 usedb;
  const char         *pdbdir;
  const char         *pavcthumb;
  const char         *pavcthumblog;
  const char         *outiface;
  const char         *pignorefileprfx;
  const char         *pignoredirprfx;
  const char         *phttplivedir;
  const char         *pmoofdir;
  float               httpliveduration;
  const char         *licfilepath;
  int                 verbosity;
  SRV_CFG_T           cfgShared;
  VSXLIB_STREAM_PARAMS_T *pParams;

  char                confpath[VSX_MAX_PATH_LEN];
  char                homepath[VSX_MAX_PATH_LEN];
  char                propfilepath[VSX_MAX_PATH_LEN];
  char                avcthumbpath[VSX_MAX_PATH_LEN];
  char                avcthumblogpath[VSX_MAX_PATH_LEN];
  char                logfilepath[VSX_MAX_PATH_LEN];

} SRV_START_CFG_T;


int srvlisten_startrtmplive(SRV_LISTENER_CFG_T *pListenCfg);
int srvlisten_startrtsplive(SRV_LISTENER_CFG_T *pListenCfg);
int srvlisten_starthttp(SRV_LISTENER_CFG_T *pListenCfg, int async);





#endif // __SERVER_LISTEN_START_H__
