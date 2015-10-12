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


#ifndef __SERVER_FILESYS_H__
#define __SERVER_FILESYS_H__

#include "unixcompat.h"
#include "srvcmd.h"
#include "formats/http_parse.h"


//
//TODO: these http controlhandlers need to be unified
// rc < 0 the connetion loop will break
// any HTTP_STATUS_T which is an error will cause the srv_cmd_proc invoker to generate http_resp_error
//

int srv_ctrl_listmedia(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus);
int srv_ctrl_prop(CLIENT_CONN_T *pConn);
int srv_ctrl_pip(CLIENT_CONN_T *pConn);
int srv_ctrl_status(CLIENT_CONN_T *pConn);
int srv_ctrl_config(CLIENT_CONN_T *pConn);
int srv_ctrl_loadimg(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus);
int srv_ctrl_loadtmn(CLIENT_CONN_T *pConn);
int srv_ctrl_loadmedia(CLIENT_CONN_T *pConn, const char *filepath);
int srv_ctrl_tslive(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus);
int srv_ctrl_flvlive(CLIENT_CONN_T *pConn);
int srv_ctrl_mkvlive(CLIENT_CONN_T *pConn);
int srv_ctrl_live(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus, const char *accessUri);
int srv_ctrl_httplive(CLIENT_CONN_T *pConn, const char *uri, const char *virtFilePath, const char *filepath, 
                      HTTP_STATUS_T *pHttpStatus);
int srv_ctrl_mooflive(CLIENT_CONN_T *pConn, const char *uri, const char *virtFilePath, const char *filepath, 
                      HTTP_STATUS_T *pHttpStatus);
int srv_ctrl_rtmp(CLIENT_CONN_T *pConn, const char *uri, int is_remoteargfile, 
                  const char *rsrcUrl, const SRV_LISTENER_CFG_T *pListenHttp, unsigned int outidx);
int srv_ctrl_flv(CLIENT_CONN_T *pConn, const char *uri,  int is_remoteargfile, 
                 const SRV_LISTENER_CFG_T *pListenHttp);
int srv_ctrl_mkv(CLIENT_CONN_T *pConn, const char *uri, int is_remoteargfile, 
                 const SRV_LISTENER_CFG_T *pListenHttp);
int srv_ctrl_rtsp(CLIENT_CONN_T *pConn, const char *uri, int is_remoteargfile, 
                  const SRV_LISTENER_CFG_T *pListenRtsp);
int srv_ctrl_submitdata(CLIENT_CONN_T *pConn);
int srv_ctrl_islegal_fpath(const char *path);
int srv_ctrl_initmediafile(HTTP_MEDIA_STREAM_T *pMediaStream, int introspect);
const char *srv_ctrl_mediapath_fromuri(const char *uri, const char *mediaDir, char pathbuf[]);
int srv_ctrl_oncommand(CLIENT_CONN_T *pConn, enum HTTP_STATUS *pHttpStatus,
                       unsigned char *pout, unsigned int *plenout);
int srv_ctrl_file_find_wext(char *filepath);

const SRV_LISTENER_CFG_T *srv_ctrl_findlistener(const SRV_LISTENER_CFG_T *arrCfgs,
                                                unsigned int maxCfgs, enum URL_CAPABILITY urlCap,
                                                NETIO_FLAG_T flags, NETIO_FLAG_T flagsMask);

#define URL_RTSP_FMT2_STR                   "rtsp%s://%s"
#define URL_PROTO_FMT2_ARGS(ssl, location)    (ssl) ? "s" : "", (location) 

#define URL_RTSP_FMT_STR                   "rtsp%s://%s:%d"
#define URL_RTMP_FMT_STR                   "rtmp%s://%s:%d"
#define URL_HTTP_FMT_STR                   "http%s://%s:%d"

#define URL_HTTP_FMT_PROTO_HOST(sockflags, location) ((sockflags) & NETIO_FLAG_SSL_TLS) ? "s" : "", (location)
#define URL_HTTP_FMT_ARGS2(p, location)              URL_HTTP_FMT_PROTO_HOST( (p)->netflags, (location)), \
                                                     ntohs(INET_PORT((p)->sa))
#define URL_HTTP_FMT_ARGS(p)                         URL_HTTP_FMT_ARGS2(p, net_getlocalhostname())   
                                   









#endif // __SERVER_CTRL_H__
