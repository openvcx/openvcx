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


#ifndef __FMT_HTTP_H__
#define __FMT_HTTP_H__

#include "util/netutil.h"

#define HTTP_URL_LEN                 256
#define HTTP_MAX_URI_ELEMENTS        24
#define HTTP_MAX_HEADER_ELEMENTS     16 
#define HTTP_VERSION_FIELD_LENGTH    12

#define HTTP_PORT_DEFAULT      80

#define CONTENT_TYPE_OCTET_STREAM      "application/octet-stream"
#define CONTENT_TYPE_SDP               "application/sdp"
#define CONTENT_TYPE_M3U8              "application/x-mpegURL"
#define CONTENT_TYPE_VND_APPLE         "application/vnd.apple.mpegurl"
#define CONTENT_TYPE_SWF               "application/x-shockwave-flash"
#define CONTENT_TYPE_RTSPTUNNELED      "application/x-rtsp-tunnelled"
#define CONTENT_TYPE_DASH_MPD          "application/dash+xml"
#define CONTENT_TYPE_AAC               "audio/aac"
#define CONTENT_TYPE_M3U               "audio/x-mpegurl"
#define CONTENT_TYPE_XM4A              "audio/x-m4a"
#define CONTENT_TYPE_IMAGEGIF          "image/gif"
#define CONTENT_TYPE_IMAGEJPG          "image/jpeg"
#define CONTENT_TYPE_IMAGEPNG          "image/png"
#define CONTENT_TYPE_MP2               "video/mpeg"
#define CONTENT_TYPE_TS                "video/MP2T"
#define CONTENT_TYPE_MP4               "video/mp4"
#define CONTENT_TYPE_M4V               CONTENT_TYPE_MP4
#define CONTENT_TYPE_M4A               "audio/mp4"
#define CONTENT_TYPE_FLV               "video/x-flv"
#define CONTENT_TYPE_MATROSKA          "video/x-matroska"
#define CONTENT_TYPE_WEBM              "video/webm"
#define CONTENT_TYPE_MP2TS             "video/x-mpeg-ts"
#define CONTENT_TYPE_H264              "video/x-h264"
#define CONTENT_TYPE_XM4V              "video/x-m4v"
#define CONTENT_TYPE_TEXTHTML          "text/html; charset=UTF-8"
#define CONTENT_TYPE_TEXTPLAIN         "text/plain"

#define HTTP_HDR_ACCEPT                "Accept"
#define HTTP_HDR_ACCEPT_RANGES         "Accept-Ranges"
#define HTTP_HDR_AUTHORIZATION         "Authorization"
#define HTTP_HDR_CONNECTION            "Connection"
#define HTTP_HDR_CONTENT_BASE          "Content-Base"
#define HTTP_HDR_CONTENT_TYPE          "Content-Type"
#define HTTP_HDR_CONTENT_LEN           "Content-Length"
#define HTTP_HDR_CONTENT_RANGE         "Content-Range"
#define HTTP_HDR_DATE                  "Date"
#define HTTP_HDR_EXPIRES               "Expires"
#define HTTP_HDR_ETAG                  "ETag"
#define HTTP_HDR_IF_MOD_SINCE          "If-Modified-Since"
#define HTTP_HDR_IF_NONE_MATCH         "If-None-Match"
#define HTTP_HDR_LOCATION              "Location"
#define HTTP_HDR_RANGE                 "Range"
#define HTTP_HDR_REFERER               "Referer"
#define HTTP_HDR_SET_COOKIE            "Set-Cookie"
#define HTTP_HDR_USER_AGENT            "User-Agent"
#define HTTP_HDR_CACHE_CONTROL         "Cache-Control"
#define HTTP_HDR_WWW_AUTHENTICATE      "WWW-Authenticate"

#define HTTP_CACHE_CONTROL_NO_CACHE    "no-cache"

#define HTTP_METHOD_GET                "GET"
#define HTTP_METHOD_HEAD               "HEAD"
#define HTTP_METHOD_POST               "POST"

enum HTTP_CONN_TYPE {
  HTTP_CONN_TYPE_CLOSE            = 0,
  HTTP_CONN_TYPE_KEEPALIVE        = 1
};

//
// Ensure to update http_parse.c::http_lookup_statuscode when 
// adding or updating members
//
typedef enum HTTP_STATUS {
  HTTP_STATUS_OK                 = 200,
  HTTP_STATUS_PARTIALCONTENT     = 206,
  HTTP_STATUS_MOVED_PERMANENTLY  = 301,
  HTTP_STATUS_FOUND              = 302,
  HTTP_STATUS_NOTMODIFIED        = 304,
  HTTP_STATUS_BADREQUEST         = 400,
  HTTP_STATUS_UNAUTHORIZED       = 401,
  HTTP_STATUS_FORBIDDEN          = 403,
  HTTP_STATUS_NOTFOUND           = 404,
  HTTP_STATUS_METHODNOTALLOWED   = 405,
  HTTP_STATUS_SERVERERROR        = 500,
  HTTP_STATUS_SERVICEUNAVAIL     = 503
} HTTP_STATUS_T;

#define IS_HTTP_STATUS_OK(s) ((s) >= 200 && (s) < 400)
#define IS_HTTP_STATUS_ERROR(s) ((s) >= 400)


typedef struct HTTP_POST_DATA {
  unsigned int        idxRead;
  unsigned int        lenInBuf;
  char                filename[128];
  char                boundary[128];
  unsigned int        lenBoundary;
  unsigned int        contentLen;
} HTTP_POST_DATA_T;

typedef struct HTTP_REQ {
  char                method[16];
  char                version[HTTP_VERSION_FIELD_LENGTH];
  char                url[HTTP_URL_LEN];   // does not include query string
  char               *puri;                // does not include server name, port
  KEYVAL_PAIR_T       uriPairs[HTTP_MAX_URI_ELEMENTS];
  KEYVAL_PAIR_T       reqPairs[HTTP_MAX_HEADER_ELEMENTS];
  enum HTTP_CONN_TYPE connType;
  char                cookie[128];
  HTTP_POST_DATA_T    postData;
} HTTP_REQ_T;

typedef struct HTTP_RESP {
  int                 statusCode;
  char                version[HTTP_VERSION_FIELD_LENGTH];
  KEYVAL_PAIR_T       hdrPairs[HTTP_MAX_HEADER_ELEMENTS];
} HTTP_RESP_T;



typedef struct HTTP_PARSE_CTXT {
  NETIO_SOCK_T      *pnetsock;
  const char        *pbuf;
  unsigned int       szbuf;
  unsigned int       tmtms;

  // set internally    buf[ endhdridx through idxbuf ] was read from socket 
  //                   and constitutes non header data
  unsigned int       hdrslen;
  unsigned int       idxbuf;
  unsigned int       termcharidx;
  int                rcvclosed;
} HTTP_PARSE_CTXT_T;


#define HTTP_STATUS_STR_FORBIDDEN     "403 - Forbidden"
#define HTTP_STATUS_STR_NOTFOUND      "404 - Not Found"
#define HTTP_STATUS_STR_SERVERERROR   "500 - Server Error"
#define HTTP_STATUS_STR_SERVERBUSY    "503 - Server Busy"

#define HTTP_VERSION_1_1         "HTTP/1.1"
#define HTTP_VERSION_1_0         "HTTP/1.0"
#define HTTP_VERSION_DEFAULT     HTTP_VERSION_1_1



int http_readhdr(HTTP_PARSE_CTXT_T *pCtxt);
const char *http_parse_headers(const char *buf, unsigned int len,
                               KEYVAL_PAIR_T *pKvs, unsigned int numKvs);
const char *http_parse_respline(const char *pBuf, unsigned int lenResp,
                                HTTP_RESP_T *pResp);

const char *http_lookup_statuscode(enum HTTP_STATUS statusCode);
const char *http_getConnTypeStr(enum HTTP_CONN_TYPE type);
const char *http_urldecode(const char *in, char *out, unsigned int lenout);
int http_urlencode(const char *in, char *out, unsigned int lenout);










#endif // _FMT_HTTP_H___
