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


#ifndef __CAPTURE_HTTPGETMP4_H__
#define __CAPTURE_HTTPGETMP4_H__

#include "capture.h"
#include "formats/mpdpl.h"

typedef enum MP4_RCV_ERROR {
  MP4_RCV_ERROR_NONE         = 0,
  MP4_RCV_ERROR_UNKNOWN      = -1,
  MP4_RCV_ERROR_NO_MOOV      = -2,
  MP4_RCV_ERROR_RD_TMT       = -3,
  MP4_RCV_ERROR_FORCE_EXIT   = -10
} MP4_RECV_ERROR_T;


typedef struct CAP_HTTP_MP4_NEXT {
  uint32_t                   trackId;
  int                        count;
} CAP_HTTP_MP4_NEXT_T;

typedef struct CAP_HTTP_PLAYLIST_ENTRY {
  PLAYLIST_MPD_ENTRY_T      *plEntryUsed;
  int                        count;
  int                        active;
} CAP_HTTP_PLAYLIST_ENTRY_T;

typedef struct CAP_HTTP_MP4_STREAM {
  PLAYLIST_MPD_T             pl;
  CAP_HTTP_MP4_NEXT_T        next[2];
  CAP_HTTP_PLAYLIST_ENTRY_T  plUsed[2];
  int                        highestCount;
  MP4_RECV_ERROR_T           error;
} CAP_HTTP_MP4_STREAM_T;


int http_mp4_recv(CAP_ASYNC_DESCR_T *pCfg,
                  NETIO_SOCK_T *pNetSock,
                  const struct sockaddr *psa,
                  FILE_OFFSET_T contentLen,
                  HTTP_PARSE_CTXT_T *pHdrCtxt,
                  const char *outPath);

int http_gethttpdash(CAP_ASYNC_DESCR_T *pCfg,
                     const char *puri,
                     HTTP_RESP_T *pHttpResp,
                     HTTP_PARSE_CTXT_T *pHdrCtxt);


int cap_http_stream_mp4(CAP_ASYNC_DESCR_T *pCapCfg);



#endif // __CAPTURE_HTTPGETMP4_H__
