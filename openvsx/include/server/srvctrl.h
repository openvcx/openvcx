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


#ifndef __SERVER_CTRL_H__
#define __SERVER_CTRL_H__

#include "unixcompat.h"

#ifndef WIN32

#include <arpa/inet.h>

#endif // WIN32

#include "capture/capture.h"
#include "server/srvcmd.h"
#include "server/srvlistenstart.h"

#define VSX_ROOT_URL              "/"
#define VSX_LIST_URL              "/list"
#define VSX_MEDIA_URL             "/media"
#define VSX_CMD_URL               "/cmd"
#define VSX_TSLIVE_URL            "/tslive"
#define VSX_HTTPLIVE_URL          "/httplive"
#define VSX_DASH_URL              "/dash"
#define VSX_FLVLIVE_URL           "/flvlive"
#define VSX_FLV_URL               "/flv"
#define VSX_MKV_URL               "/mkv"
#define VSX_MKVLIVE_URL           "/mkvlive"
#define VSX_WEBM_URL              "/webm"
#define VSX_RTMP_URL              "/rtmp"
#define VSX_RTMPT_URL             "/rtmpt"
#define VSX_RSRC_URL              "/rsrc"
#define VSX_HTTP_URL              "/http"
#define VSX_RTSP_URL              "/rtsp"
#define VSX_RTSPLIVE_URL          "/rtsplive"
#define VSX_IMG_URL               "/img"
#define VSX_TMN_URL               "/tmn"
#define VSX_PROP_URL              "/prop"
#define VSX_LIVE_URL              "/live"
#define VSX_STATUS_URL            "/status"
#define VSX_PIP_URL               "/pip"
#define VSX_CONFIG_URL            "/config"
#define VSX_CROSSDOMAIN_XML_URL   "/crossdomain.xml"

#define VSX_CONF_FILE             "vsx.conf"
#define VSX_DEVICES_FILE          "devices.conf"

#define VSX_HTML_PATH             "html"
#define VSX_IMG_PATH              VSX_HTML_PATH DIR_DELIMETER_STR "img"
#define VSX_DB_PATH               ".vsxdb"
#define VSX_INDEX_HTML            VSX_HTML_PATH DIR_DELIMETER_STR "bcastctrl.html"
#define VSX_CONF_PATH             "etc" DIR_DELIMETER_STR VSX_CONF_FILE
#define VSX_DEVICESFILE_PATH      "etc" DIR_DELIMETER_STR VSX_DEVICES_FILE
#define VSX_HTTPLIVE_PATH         VSX_HTML_PATH DIR_DELIMETER_STR "/httplive"
#define VSX_MOOF_PATH             VSX_HTML_PATH DIR_DELIMETER_STR "/dash"
#define VSX_RSRC_HTML_PATH        VSX_HTML_PATH DIR_DELIMETER_STR "/rsrc"
#define VSX_RTMP_INDEX_HTML       VSX_RSRC_HTML_PATH DIR_DELIMETER_STR "/rtmp_embed.html"
#define VSX_FLASH_HTTP_INDEX_HTML VSX_RSRC_HTML_PATH DIR_DELIMETER_STR "/http_embed.html"
#define VSX_MKV_INDEX_HTML        VSX_RSRC_HTML_PATH DIR_DELIMETER_STR "mkv_embed.html"
#define VSX_HTTPLIVE_INDEX_HTML   VSX_RSRC_HTML_PATH DIR_DELIMETER_STR "httplive_embed.html"
#define VSX_DASH_INDEX_HTML       VSX_RSRC_HTML_PATH DIR_DELIMETER_STR "dash_embed.html"
#define VSX_RTSP_INDEX_HTML       VSX_RSRC_HTML_PATH DIR_DELIMETER_STR "rtsp_embed.html"

#define VSX_URI_PROFILE_PARAM     "prof_"
#define VSX_URI_ID_PARAM          "id_"
#define VSX_URI_TOKEN_QUERY_PARAM "tk"
#define VSX_URI_TOKEN_PARAM       VSX_URI_TOKEN_QUERY_PARAM"_"
#define VSX_URI_CHECK_PARAM       "check"

#if defined(VSX_HAVE_SERVERMODE) 

#define URL_CAP_BROADCAST (URL_CAP_FILEMEDIA | URL_CAP_CMD | URL_CAP_ROOTHTML | \
                           URL_CAP_LIST | URL_CAP_PROP | URL_CAP_IMG | URL_CAP_TMN)

int srv_start_conf(VSXLIB_STREAM_PARAMS_T *pParams,
                   const char *homeDir, const char *dbDir, int usedb, 
                   const char *curdir);

SRV_CONF_T *srv_init_conf(const char *listenArg, const char *mediaDirArg,
                          const char *homeDirArg, const char *confPathArg,
                          const char *dbDirArg, int usedb,
                          const char *curdir, 
                          unsigned int rtmphardlimit, 
                          unsigned int rtsphardlimit,
                          unsigned int httphardlimit,
                          SRV_START_CFG_T *pCfg,
                          AUTH_CREDENTIALS_STORE_T *ppAuthStores);

#endif // VSX_HAVE_SERVERMODE




#endif // __SERVER_CTRL_H__
