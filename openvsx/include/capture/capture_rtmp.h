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


#ifndef __CAPTURE_RTMP_H__
#define __CAPTURE_RTMP_H__

#include "capture.h"
#include "formats/rtmp_auth.h"
#include "formats/rtmp_tunnel.h"
#include "formats/rtmp_parse.h"
#include "formats/rtmp_pkt.h"

#define RTMP_CLIENT_FP9         1

//#define RTMP_TMPFRAME_VID_SZ    0x80000
#define RTMP_TMPFRAME_VID_SZ    IXCODE_SZFRAME_MAX
#define RTMP_TMPFRAME_AUD_SZ    0x4000



typedef struct RTMP_CTXT_CLIENT {
  RTMP_CTXT_T                    ctxt;
  RTMP_CLIENT_PARAMS_T           client;
  RTMP_AUTH_STORE_T              auth;
  RTMP_AUTH_PERSIST_STORAGE_T   *pcachedAuth;

  const char                     *purl;
  const char                     *puri;
  const char                     *puridocname;
  const char                     *plocation;

  SOCKET_DESCR_T                 sd; 
  PKTQUEUE_T                     *pQVid;
  PKTQUEUE_T                     *pQAud;
  PKTQUEUE_T                     *pQTmpVid;
  struct STREAM_STATS            *pStreamStats;
  struct OUTFMT_CFG              *pOutFmt;
} RTMP_CTXT_CLIENT_T;

int capture_rtmp_client(CAP_ASYNC_DESCR_T *pCapCfg);
int rtmp_handle_vidpkt(RTMP_CTXT_CLIENT_T *pRtmp, unsigned char *pData,
                       unsigned int len, unsigned int ts);
int rtmp_handle_audpkt(RTMP_CTXT_CLIENT_T *pRtmp, unsigned char *pData,
                       unsigned int len, unsigned int ts);

int capture_rtmp_server(CAP_ASYNC_DESCR_T *pCapCfg);







#endif // __CAPTURE_RTMP_H__
