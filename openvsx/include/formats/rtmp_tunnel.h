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


#ifndef __RTMP_TUNNEL_H__
#define __RTMP_TUNNEL_H__

#include "formats/http_parse.h"

typedef enum RTMPT_ICMD {
  RTMPT_ICMD_NONE     = 0,
  RTMPT_ICMD_IDENT    = 1,
  RTMPT_ICMD_OPEN     = 2,
  RTMPT_ICMD_IDLE     = 3,
  RTMPT_ICMD_SEND     = 4
} RTMPT_ICMD_T;

typedef struct RTMPT_CTXT {
  int                      isclient;
  unsigned int             httpTmtMs;
  uint64_t                 sessionId;
  unsigned int             seqnum;
  char                    *phosthdr;
  HTTP_REQ_T               httpReq;      // server http request parse context
  HTTP_PARSE_CTXT_T        httpCtxt;     // client http response parse context
  unsigned int             idxInHttpBuf;
  unsigned int             idxInContentLen;
  unsigned int             contentLen;
  unsigned int             lenFirstByte; // RTMPT body content-data prior to actual RTMP data

  int                      pendingResponse;
  unsigned int             contentLenResp;
  unsigned int             idxInResp;
  unsigned int             idxbuf0;

  unsigned char           *pbuftmp;
  unsigned int             szbuftmp;
  unsigned char           *pbufdyn;
  unsigned int             szbufdyn;
  unsigned int             idxbufdyn;
  unsigned int             szToPost;
  unsigned int             idxQueued;
  int                      doQueueOut;
  int                      doQueueIdle;
  int                      pendingRequests;
  RTMPT_ICMD_T             lastCmd;
  TIME_VAL                 tmLastIdleReq;
  TIME_VAL                 tmLastResp;
} RTMPT_CTXT_T;

struct RTMP_CTXT_CLIENT;

int rtmpt_istunneled(const unsigned char *pData, unsigned int len);
int rtmpt_setupclient(struct RTMP_CTXT_CLIENT *pClient, unsigned char *rtmptbufin, unsigned int szrtmptbufin,
                      unsigned char *rtmptbufout, unsigned int szrtmptbufout);
int rtmpt_setupserver(struct RTMP_CTXT *pRtmp, const unsigned char *prebufdata, unsigned int prebufdatasz);
int rtmpt_recv(struct RTMP_CTXT *pRtmp, unsigned char *pData, unsigned int len);
int rtmpt_send(struct RTMP_CTXT *pRtmp, const unsigned char *pData, unsigned int len, const char *descr);


#endif // __RTMP_TUNNEL_H__
