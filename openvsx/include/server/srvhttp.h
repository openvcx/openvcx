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


#ifndef __SRV_HTTP_H__
#define __SRV_HTTP_H__

#include "unixcompat.h"
#include "capture/capture_sockettypes.h"
#include "formats/http_parse.h"

#define HTTP_ETAG_AUTO                   ""
#define HTTP_THROTTLERATE                1.2
#define HTTP_THROTTLEPREBUF              8.0

#define HTTP_REQUEST_TIMEOUT_SEC         30
#define HTTP_FOLLOW_SYMLINKS_DEFAULT     BOOL_DISABLED_DFLT 

typedef struct HTTP_MEDIA_STREAM {
  FILE_STREAM_T                 *pFileStream;
  const char                    *pContentType;
  MEDIA_DESCRIPTION_T           media;
} HTTP_MEDIA_STREAM_T;

typedef struct HTTP_RANGE_HDR {
  int                acceptRanges;   // Include "Accept-Ranges" response hdr
  int                contentRange;  // Include "Content-Range" response hdr
  FILE_OFFSET_T      start;
  FILE_OFFSET_T      end;
  FILE_OFFSET_T      total;
  int                unlimited;
} HTTP_RANGE_HDR_T;

struct CLIENT_CONN;

int http_resp_sendhdr(SOCKET_DESCR_T *pSd,
                      const char *httpVersion,
                      enum HTTP_STATUS statusCode,
                      FILE_OFFSET_T len,
                      const char *contentType,
                      const char *connType,
                      const char *cookie,
                      const HTTP_RANGE_HDR_T *pRange,
                      const char *etag,
                      const char *location,
                      const char *auth);

int http_parse_rangehdr(const char *rangestr, HTTP_RANGE_HDR_T *pRange);

int http_req_readparse(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, unsigned int tmtsec, int verifyhttpmethod);
int http_req_readpostparse(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_REQ_T *pReq, int verifyhttpmethod);
int http_req_parsebuf(HTTP_REQ_T *pReq, const  char *buf, unsigned int len, int verifyhttpmethod);
const char *http_parse_req_uri(const char *buf, unsigned int len, 
                               HTTP_REQ_T *pReq, int verifyhttpmethod);
char *http_req_dump_uri(const HTTP_REQ_T *pReq, char *buf, unsigned int szbuf);
int http_resp_send(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq,
                   enum HTTP_STATUS statusCode, 
                   unsigned char *pData, unsigned int len);
//int http_resp_send_unauthorized(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, const char *auth);
int http_resp_sendfile(struct CLIENT_CONN *pConn, HTTP_STATUS_T *pHttpStatus,
                       const char *path, const char *contentType, const char *etag);
int http_resp_sendmediafile(struct CLIENT_CONN *pConn, HTTP_STATUS_T *pHttpStatus,
                            HTTP_MEDIA_STREAM_T *pMedia, float throttlerate, float throttleprebuf);
int http_resp_sendtslive(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, PKTQUEUE_T *pQ, const char *contentType);
int http_resp_error(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, enum HTTP_STATUS statusCode, 
                    int close, const char *strResult, const char *auth);
int http_resp_moved(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, enum HTTP_STATUS statusCode, 
                    int close, const char *location);

const char *http_lookup_statuscode(enum HTTP_STATUS statusCode);
int http_format_date(char *buf, unsigned int len, int logfmt);
int http_log(const SOCKET_DESCR_T *pSd, const HTTP_REQ_T *pReq,
             enum HTTP_STATUS statusCode, FILE_OFFSET_T len);
int http_log_setfile(const char *p);
char *http_make_etag(const char *filepath, char *etagbuf, unsigned int szetagbuf);

int http_check_symlink(const struct CLIENT_CONN *pConn, const char *path);
//int http_check_pass(const struct CLIENT_CONN *pConn);

#endif // __SRV_HTTP_H__
