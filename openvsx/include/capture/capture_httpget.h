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


#ifndef __CAPTURE_HTTPGET_H__
#define __CAPTURE_HTTPGET_H__

#include "capture.h"
#include "util/pktqueue.h"

enum CAP_HTTP_ACTION {
  CAP_HTTP_ACTION_UNKNOWN     = 0,
  CAP_HTTP_ACTION_TS          = 1,
  CAP_HTTP_ACTION_HTTPLIVE    = 2,
  CAP_HTTP_ACTION_FLV         = 3,
  CAP_HTTP_ACTION_MP4         = 4,
  CAP_HTTP_ACTION_DASH        = 5
};

typedef struct CAP_HTTP_COMMON {
  CAP_ASYNC_DESCR_T    *pCfg;
  unsigned char        *pdata;
  unsigned int          dataoffset;
  unsigned int          datasz;
} CAP_HTTP_COMMON_T;

typedef enum CONNECT_RETRY_RC {
  CONNECT_RETRY_RC_NORETRY            = -2, 
  CONNECT_RETRY_RC_ERROR              = -1, 
  CONNECT_RETRY_RC_OK                 = 0,
  CONNECT_RETRY_RC_CONNECTAGAIN       = 1
} CONNECT_RETRY_RC_T;

typedef CONNECT_RETRY_RC_T (* CONNECT_RETRY_ACTION_CB) (void *);

typedef struct CONNECT_RETRY_CTXT {
  CONNECT_RETRY_ACTION_CB   cbConnectedAction;
  void                      *pConnectedActionArg;
  HTTPCLI_AUTH_CTXT_T       *pAuthCliCtxt;
  NETIO_SOCK_T              *pnetsock;
  const struct sockaddr     *psa;
  uint64_t                  *pByteCtr;
  const char                *connectDescr;
  const int                 *pconnectretrycntminone;
  const int                 *prunning;
  unsigned int               tmtms;
} CONNECT_RETRY_CTXT_T;

/**
 * Connect to a remote endpoint.
 * Will attempt to retry connecting according to the retry configuration
 */
int connect_with_retry(CONNECT_RETRY_CTXT_T *pRetryCtxt);

unsigned char *http_read_net(CAP_HTTP_COMMON_T *pCommon,
                             NETIO_SOCK_T *pnetsock,
                             const struct sockaddr *psa,
                             unsigned int bytesToRead,
                             unsigned char *bufout,  
                             unsigned int szbuf,
                             unsigned int *pBytesRead);

int http_flv_recvloop(CAP_ASYNC_DESCR_T *pCfg,
                  unsigned int offset,
                  int sz,
                  FILE_OFFSET_T contentLen,
                  unsigned char *pbuf,
                  unsigned int szbuf,
                  CAPTURE_STREAM_T *pStream);

int http_gethttplive(CAP_ASYNC_DESCR_T *pCfg,
                         CAPTURE_CBDATA_T *pStreamsOut,
                         CAPTURE_STREAM_T *pStream,
                         const char *puri,
                         HTTP_RESP_T *pHttpResp,
                         HTTP_PARSE_CTXT_T *pHdrCtxt);

int http_recvloop(CAP_ASYNC_DESCR_T *pCfg,
                  NETIO_SOCK_T *pnetsock,
                  const struct sockaddr *psa,
                  CAPTURE_CBDATA_T *pStreamsOut,
                  CAPTURE_STREAM_T *pStream,
                  unsigned int offset,
                  int sz,
                  FILE_OFFSET_T contentLen,
                  unsigned char *pbuf,
                  unsigned int szbuf);

/*
unsigned char *http_get_contentlen_start(HTTP_RESP_T *pHttpResp,
                                    HTTP_PARSE_CTXT_T *pHdrCtxt,
                                    unsigned char *pbuf, unsigned int szbuf,
                                    int verifybufsz,
                                    unsigned int *pcontentLen);

const char *http_get_doc(CAP_ASYNC_DESCR_T *pCfg,
                         const char *puri,
                         HTTP_RESP_T *pHttpResp,
                         HTTP_PARSE_CTXT_T *pHdrCtxt,
                         unsigned char *pbuf, 
                         unsigned int szbuf);
*/

#define CAPTURE_HTTP_HOSTBUF_MAXLEN    128

int capture_httpget(CAP_ASYNC_DESCR_T *pCapCfg);

/**
 *  str      - Input URL  'rtsps://127.0.0.1/a/b
 *  psaIn    - Optional output initialized address struct
 *  ppuri    - Optional output pointer to URI
 *  hostbuf  - Optional output buffer to store hostname / IP Address 
 *  dfltPort - Default port if not given in input URL
 */
int capture_getdestFromStr(const char *str, struct sockaddr_storage *psa,
                           const char **ppuri, char *hostbuf, uint16_t dfltPort);


#endif // __CAPTURE_HTTPGET_H__
