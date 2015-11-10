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




#ifndef __RTSP_PARSE_H__
#define __RTSP_PARSE_H__

#include "http_parse.h"
#include "sdp.h"

#define RTSP_PORT_DEFAULT            554

#define RTSP_URL_LEN                 HTTP_URL_LEN

#define RTSP_INTERLEAVED_HEADER     '$'

#define RTSP_REQ_ANNOUNCE            "ANNOUNCE"
#define RTSP_REQ_OPTIONS             "OPTIONS"
#define RTSP_REQ_DESCRIBE            "DESCRIBE"
#define RTSP_REQ_SETUP               "SETUP"
#define RTSP_REQ_TEARDOWN            "TEARDOWN"
#define RTSP_REQ_PLAY                "PLAY"
#define RTSP_REQ_PAUSE               "PAUSE"

#define RTSP_URL_PREFIX              "rtsp://"
#define RTSP_VERSION_DEFAULT         "RTSP/1.0"

#define RTSP_HDR_CSEQ                "CSeq"
#define RTSP_HDR_SESSION             "Session"
#define RTSP_HDR_TRANSPORT           "Transport"
#define RTSP_HDR_SRVERROR            "x-Error"
#define RTSP_HDR_SESSIONCOOKIE       "x-sessioncookie"
#define RTSP_HDR_RTP_INFO            "RTP-Info"
#define RTSP_HDR_RANGE               HTTP_HDR_RANGE 
#define RTSP_HDR_AUTHORIZATION       HTTP_HDR_AUTHORIZATION
#define RTSP_HDR_CONNECTION          HTTP_HDR_CONNECTION 
#define RTSP_HDR_USER_AGENT          HTTP_HDR_USER_AGENT
#define RTSP_HDR_ACCEPT              HTTP_HDR_ACCEPT
#define RTSP_HDR_CONTENT_TYPE        HTTP_HDR_CONTENT_TYPE
#define RTSP_HDR_CONTENT_LEN         HTTP_HDR_CONTENT_LEN
#define RTSP_HDR_DATE                HTTP_HDR_DATE
#define RTSP_HDR_EXPIRES             HTTP_HDR_EXPIRES
#define RTSP_HDR_CONTENT_BASE        HTTP_HDR_CONTENT_BASE
#define RTSP_HDR_CACHE_CONTROL       HTTP_HDR_CACHE_CONTROL
#define RTSP_HDR_WWW_AUTHENTICATE    HTTP_HDR_WWW_AUTHENTICATE
#define RTSP_HDR_PUBLIC              "Public" 

#define RTSP_TRACKID                 SDP_ATTRIB_CONTROL_TRACKID 
#define RTSP_CLIENT_TIMEOUT          "timeout"
#define RTSP_CLIENT_PORT             "client_port"
#define RTSP_SERVER_PORT             "server_port"
#define RTSP_SOURCE                  "source"
#define RTSP_TRANS_UNICAST           "unicast"
#define RTSP_TRANS_MULTICAST         "multicast"
#define RTSP_TRANS_INTERLEAVED       "interleaved"

enum RTSP_STATUS {
  RTSP_STATUS_OK                 = HTTP_STATUS_OK,
  RTSP_STATUS_BADREQUEST         = HTTP_STATUS_BADREQUEST,
  RTSP_STATUS_UNAUTHORIZED       = HTTP_STATUS_UNAUTHORIZED,
  RTSP_STATUS_FORBIDDEN          = HTTP_STATUS_FORBIDDEN,
  RTSP_STATUS_NOTFOUND           = HTTP_STATUS_NOTFOUND,
  RTSP_STATUS_UNSUPPORTEDTRANS   = 461,
  RTSP_STATUS_SERVERERROR        = HTTP_STATUS_SERVERERROR,
  RTSP_STATUS_SERVICEUNAVAIL     = HTTP_STATUS_SERVICEUNAVAIL 
};


int rtsp_isrtsp(const unsigned char *pData, unsigned int len);

#endif // __RTSP_PARSE_H__
