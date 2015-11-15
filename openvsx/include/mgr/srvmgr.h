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


#ifndef __SRV_MGR_H__
#define __SRV_MGR_H__

#include "unixcompat.h"
#include "mgr/procdb.h"
#include "mgr/mgrnode.h"

#define MGR_CONF_FILE                      "vsxwp.conf"

#define SRV_CONF_KEY_LAUNCHMEDIA           "launchMedia"
#define SRV_CONF_KEY_PORTRANGE             "portRange"
#define SRV_CONF_KEY_MAX_MEDIA             "maxProcesses"
#define SRV_CONF_KEY_MAX_MEDIA_XCODE       "maxXcoders"
#define SRV_CONF_KEY_MAX_MBPS              "maxMacroBlocks"
#define SRV_CONF_KEY_EXPIRE_CHILD_SEC      "expireSec"
#define SRV_CONF_KEY_DISABLE_LISTING       "disableListing"
#define SRV_CONF_KEY_LBNODES_CONF          "LBNodesConfig"
#define SRV_CONF_KEY_MAX_CLIENTS_PER_PROC  "maxClientsPerProcess"
#define SRV_CONF_KEY_STATUS_MONITOR        "statusMonitor"
#define SRV_CONF_KEY_CPU_MONITOR           "cpuMonitor"
#define SRV_CONF_KEY_MEM_MONITOR           "memMonitor"

#define MGR_CONNECTIONS_MAX        1000 
#define MGR_CONNECTIONS_DEFAULT    50 


#if defined(WIN32)

#define MGR_CONF_PATH             "etc\\"MGR_CONF_FILE
#define MGR_LAUNCHCHILD           "bin\\vsxchild.sh"

#else

#define MGR_CONF_PATH             "etc/"MGR_CONF_FILE
#define MGR_LAUNCHCHILD           "bin/vsxchild.sh"

#endif // WIN32

//#define MGR_PORT_ALLOC_COUNT(ssl)         (2 + (ssl ? 1 : 0)) 
#define MGR_PORT_ALLOC_COUNT(ssl)         (1)
#define MGR_GET_PORT_HTTP(port, ssl)      ((port) + ((ssl) ? 0 : 0))
#define MGR_GET_PORT_RTMP(port, ssl)      ((port) + ((ssl) ? 0 : 0))
#define MGR_GET_PORT_RTSP(port, ssl)      ((port) + ((ssl) ? 0 : 0))
#define MGR_GET_PORT_STATUS(port)         ((port)+0)


typedef enum MEDIA_ACTION {
  MEDIA_ACTION_UNKNOWN           = 0,
  MEDIA_ACTION_NONE              = 1,
  MEDIA_ACTION_LIVE_STREAM       = 2,   // the resource is a live broadcast, such as an SDP
                                        // it must be handled by a dedicated vsx child proc
  MEDIA_ACTION_CANNED_DIRECT     = 3,   // direct download by browser 
  MEDIA_ACTION_CANNED_INFLASH    = 4,   // direct download within flash .swf container
  MEDIA_ACTION_CANNED_WEBM       = 5,   // direct download through html5 video tag
  MEDIA_ACTION_CANNED_PROCESS    = 6,   // treat the canned media file as a live broadcast
                                        // handled by a dedicated vsx child proc
  MEDIA_ACTION_CANNED_HTML       = 7,   // Non media resource, such as .swf, .css, .jpg
  MEDIA_ACTION_TMN               = 8,   // Media file thumbnail file (.jpg)
  MEDIA_ACTION_IMG               = 9,   // image directly from '/img' 
  MEDIA_ACTION_LIST              = 10,  // '/list' media directory web serivce
  MEDIA_ACTION_INDEX_FILELIST    = 11,  // '/' index html file for '/list'
  MEDIA_ACTION_CHECK_TYPE        = 12,  // web service call to query properties about resource
  MEDIA_ACTION_CHECK_EXISTS      = 13,
  MEDIA_ACTION_STATUS            = 14,
} MEDIA_ACTION_T;


typedef struct SRV_MGR_PARAMS {
  char                    *confpath;
  const char              *listenaddr[SRV_LISTENER_MAX];
  char                    *mediadir;
  char                    *homedir;
  char                    *dbdir;
  char                    *curdir;
  char                    *logfile;
  const char              *lbnodesfile;
  const char              *httpaccesslogfile;
  int                      nodb;
  int                      disable_root_dirlist;
  SRV_START_CFG_T         *pCfg;
  SYS_PROCLIST_T           procList;
} SRV_MGR_PARAMS_T;

typedef struct SRV_MGR_START_CFG {
  SRV_START_CFG_T              *pCfg;
  const char                   *plaunchpath;
  SYS_PROCLIST_T               *pProcList;
  TIME_VAL                     *ptmstart;
  MGR_NODE_LIST_T              *pLbNodes;
  STREAM_STATS_MONITOR_T       *pMonitor;
  LIC_INFO_T                   *plic;
  int                           enable_ssl_childlisteners;
  CPU_USAGE_PERCENT_T          *pCpuUsage;
  MEM_SNAPSHOT_T               *pMemUsage;

  struct SRV_MGR_LISTENER_CFG  *pListenerRtmpProxy;
  struct SRV_MGR_LISTENER_CFG  *pListenerRtspProxy;
  struct SRV_MGR_LISTENER_CFG  *pListenerHttpProxy;

} SRV_MGR_START_CFG_T;


typedef struct SRV_MGR_LISTENER_CFG {
  const SRV_MGR_START_CFG_T   *pStart;
  SRV_LISTENER_CFG_T           listenCfg;
  AUTH_CREDENTIALS_STORE_T     authStore;
} SRV_MGR_LISTENER_CFG_T;

typedef struct SRV_MGR_CONN {
  CLIENT_CONN_T               conn;
  const  SRV_MGR_START_CFG_T  *pMgrCfg;
} SRV_MGR_CONN_T;

typedef struct SRVMEDIA_RSRC {
  int              *pmethodBits;    // 'methods=' metafile bitmask value 
  int              *pshared;        // 'shared=' metafile value
  char             *pXcodeStr;      // 'xcodeargs=' metafile value
  char             *pUserpass;      // 'digestauth=' metafile value
  char             *pTokenId;       // 'token=' metafile value
  char             *pLinkStr;       // 'httplink=' metafile value
  char             *pInputStr;      // 'input=' metafile value
  char             *pId;            // 'id=' metafile value

  char              rsrcExt[64];
  char             *pRsrcName;      // Virtual resource name and extension without any leading path
                                    // within virtRsrc
  char              profile[16];    // 'prof_' URI value
  char              instanceId[SYS_PROC_INSTANCE_ID_LEN + 1]; // 'id_' URI value
  char              tokenId[META_FILE_TOKEN_LEN]; // 'tk_' URI value
  char              virtRsrc[VSX_MAX_PATH_LEN]; // Virtual resource path + name as extracted from the request URI
  size_t            szvirtRsrcOnly;             // length of virtual resource path excluding name
  char              filepath[VSX_MAX_PATH_LEN]; // Resource filepath on local storage
} SRVMEDIA_RSRC_T;




int srvmgr_start(SRV_MGR_PARAMS_T *pParams);
int srvmgr_check_metafile(META_FILE_T *pMetaFile, SRVMEDIA_RSRC_T *pMediaRsrc,
                          const char *devname, HTTP_STATUS_T *phttpStatus);
int srvmgr_check_start_proc(SRV_MGR_CONN_T *pConn, const SRVMEDIA_RSRC_T *pMediaRsrc,
                            MEDIA_DESCRIPTION_T *pMediaDescr, SYS_PROC_T *procArg,
                            const STREAM_DEVICE_T *pdevtype, MEDIA_ACTION_T action, 
                            STREAM_METHOD_T methodAuto, int isShared, int *pstartProc);
const char *srvmgr_action_tostr(MEDIA_ACTION_T action);
int srvmgr_is_shared_resource(const SRVMEDIA_RSRC_T *pMediaRsrc,  MEDIA_ACTION_T action);



#endif // __SRV_MGR_H__
