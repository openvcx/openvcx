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


#ifndef __FMT_HTTP_CLIENT_H__
#define __FMT_HTTP_CLIENT_H__

typedef struct HTTPCLI_AUTH_CTXT {
  int                               consecretry;
  int                               do_basic;
  int                               noncecount;
  char                              realm[AUTH_ELEM_MAXLEN];
  char                              opaque[AUTH_ELEM_MAXLEN];
  char                              cnonce[AUTH_ELEM_MAXLEN];
  char                              nonce[AUTH_ELEM_MAXLEN];
  char                              authorization[1024];
  enum HTTP_STATUS                  lastStatusCode;
  char                              lastMethod[32];
  const char                        uribuf[VSX_MAX_PATH_LEN];
  const char                        *puri;
  const AUTH_CREDENTIALS_STORE_T    *pcreds; 
} HTTPCLI_AUTH_CTXT_T;

int httpcli_gethdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_RESP_T *pHttPesp,
                     struct sockaddr_in *psain, const char *uri,
                     const char *connType, unsigned int range0,
                     unsigned int range1, const char *host,
                     HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt);

int httpcli_connect(NETIO_SOCK_T *pnetsock, struct sockaddr_in *psain, const char *descr);

int httpcli_req_queryhdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_RESP_T *pHttpResp,
                          struct sockaddr_in *psain, HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt, 
                          const char *descr);

/**
 * Process a HTTP/RTSP 401 Unauthorized response
 */
int httpcli_authenticate(HTTP_RESP_T *pHttpResp, HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt);

int httpcli_calcdigestauth(HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt);


#endif // _FMT_HTTP_CLIENT_H___
