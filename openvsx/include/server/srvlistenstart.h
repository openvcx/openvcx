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

typedef enum SRV_ADDR_FILTER_TYPE {
  SRV_ADDR_FILTER_TYPE_ALLOW         = 0x01,
  SRV_ADDR_FILTER_TYPE_DENY          = 0x02,
  SRV_ADDR_FILTER_TYPE_GLOBAL        = 0x04,
  SRV_ADDR_FILTER_TYPE_ALLOWSTATUS   = 0x08 
} SRV_ADDR_FILTER_TYPE_T;

typedef struct SRV_ADDR_FILTER {
  SRV_ADDR_FILTER_TYPE_T          type;
  struct sockaddr_storage         sa;
  uint32_t                        mask;
  struct SRV_ADDR_FILTER         *pnext;
} SRV_ADDR_FILTER_T;

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
  const char                     *pAuthTokenId;
  SRV_ADDR_FILTER_T              *pfilters;
  char                            tid_tag[LOGUTIL_TAG_LENGTH];
  struct SRV_START_CFG           *pCfg;
} SRV_LISTENER_CFG_T;

typedef struct SRV_START_CFG {

  SRV_LISTENER_CFG_T    listenMedia[SRV_LISTENER_MAX];

  unsigned int       maxrtp;
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


int srv_rtmp_proc(CLIENT_CONN_T *pConn, const unsigned char *prebuf, unsigned int prebufsz, int istunneled);
int srv_rtsp_proc(CLIENT_CONN_T *pConn, const unsigned char *prebuf, unsigned int prebufsz);
int srvlisten_startmediasrv(SRV_LISTENER_CFG_T *pListenCfg, int async);





#endif // __SERVER_LISTEN_START_H__
