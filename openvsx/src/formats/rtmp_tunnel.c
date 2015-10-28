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


#include "vsx_common.h"

#define RTMPT_CLIENT_USER_AGENT   "Shockwave Flash"
#define RTMPT_URL_IDENT           "/fcs/ident2"
#define RTMPT_URL_OPEN            "/open/1"

static int create_post_hdrs(KEYVAL_PAIR_T kvs[], unsigned int szKvs) {
  unsigned int kvidx = 0;

  if(kvidx < szKvs) {
    memset(&kvs[kvidx], 0, sizeof(KEYVAL_PAIR_T));
    snprintf(kvs[kvidx].key, sizeof(kvs[kvidx].key), "%s", HTTP_HDR_CONTENT_TYPE);
    snprintf(kvs[kvidx].val, sizeof(kvs[kvidx].val), "%s", CONTENT_TYPE_RTMPTUNNELED);
  }

  if(kvidx < szKvs) {
    kvidx++;
    memset(&kvs[kvidx], 0, sizeof(KEYVAL_PAIR_T));
    kvs[kvidx - 1].pnext = &kvs[kvidx];
    snprintf(kvs[kvidx].key, sizeof(kvs[kvidx].key), "%s", HTTP_HDR_ACCEPT);
    snprintf(kvs[kvidx].val, sizeof(kvs[kvidx].val), "*/*");
  }

  return kvidx;
}

int rtmpt_sendheader(RTMP_CTXT_T *pRtmp, unsigned int contentLen) {
  int rc = 0;
  KEYVAL_PAIR_T kvs[5];
  char uribuf[64];
  const char *userAgent = RTMPT_CLIENT_USER_AGENT;

  memset(kvs, 0, sizeof(kvs));
  create_post_hdrs(kvs, sizeof(kvs) / sizeof(kvs[0]));
  snprintf(uribuf, sizeof(uribuf) - 1, "send/%lld/%d", pRtmp->tunnelSessionId, pRtmp->tunnelSeqnum++);

  rc = httpcli_req_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa,
                        HTTP_METHOD_POST, uribuf, http_getConnTypeStr(HTTP_CONN_TYPE_KEEPALIVE),
                        pRtmp->phosthdr, userAgent, NULL, kvs, NULL, contentLen);
               
  pRtmp->tunneldatasz += contentLen;

  return rc;
}

int rtmpt_recv(RTMP_CTXT_T *pRtmp,  unsigned char *pData, unsigned int len) {
  int rc = 0;
  unsigned int idxdata = 0;
  HTTP_RESP_T httpResp;
  unsigned int szcopy;

  if(!pRtmp || !pRtmp->ishttptunnel) {
    return -1;
  }

  memset(&httpResp, 0, sizeof(httpResp));

  do {

    //if(pRtmp->tunnelHttpCtxt.idxbuf == 0) {
    if(pRtmp->contentLen == 0) {
      if((rc = httpcli_req_queryhdrs(&pRtmp->tunnelHttpCtxt, &httpResp, 
                                     (const struct sockaddr *) &pRtmp->pSd->sa, NULL, "RTMPT")) < 0 ||
          !httpcli_get_contentlen_start(&httpResp, &pRtmp->tunnelHttpCtxt, pRtmp->tunnelHttpCtxt.pbuf, 
                                        pRtmp->tunnelHttpCtxt.szbuf, 0, &pRtmp->contentLen)) {
        return rc;
      }
      pRtmp->idxContent = 0;
      pRtmp->idxInHttpBuf = pRtmp->tunnelHttpCtxt.hdrslen;
    }

    szcopy = 0;
    if(pRtmp->idxInHttpBuf < pRtmp->tunnelHttpCtxt.idxbuf) {
      szcopy = MIN(pRtmp->tunnelHttpCtxt.idxbuf - pRtmp->idxInHttpBuf, pRtmp->contentLen - pRtmp->idxContent);
      szcopy = MIN(szcopy, len - idxdata);
    }
    if(szcopy > 0) {
      memcpy(&pData[idxdata], &pRtmp->tunnelHttpCtxt.pbuf[pRtmp->idxInHttpBuf], szcopy);
      idxdata += szcopy;
      pRtmp->idxContent += szcopy;
      pRtmp->idxInHttpBuf += szcopy;
    }

    if((szcopy = pRtmp->contentLen - pRtmp->idxContent) > 0) {
      if((rc = netio_recvnb_exact(&pRtmp->pSd->netsocket, &pData[idxdata], len, pRtmp->rcvTmtMs)) != len) {
        if(rc == 0) {
          return rc;
        }
        return -1;
      }
      pRtmp->idxContent += rc;
    }

  } while(idxdata < len);

  return rc;
}

int rtmpt_setupclient(RTMP_CTXT_CLIENT_T *pClient, unsigned char *rtmptbuf, unsigned int szrtmptbuf) {
  HTTP_RESP_T httpResp;
  unsigned char *pdata = NULL;
  const char *uri = NULL;
  char uribuf[64];
  const char *userAgent = RTMPT_CLIENT_USER_AGENT;
  unsigned char buf[4096];
  unsigned int szbuf;
  unsigned int tmtms = 0;
  unsigned char post[8];
  KEYVAL_PAIR_T kvs[5];
  int rc = 0;

  if(!pClient || !rtmptbuf || szrtmptbuf <= 0) {
    return -1;
  }

  memset(kvs, 0, sizeof(kvs));
  post[0] = '\0';
  pClient->ctxt.tunnelSessionId = 0;
  pClient->ctxt.tunnelSeqnum = 0;
  memset(&pClient->ctxt.tunnelHttpCtxt, 0, sizeof(pClient->ctxt.tunnelHttpCtxt));
  pClient->ctxt.tunnelHttpCtxt.pnetsock = &pClient->ctxt.pSd->netsocket;;
  pClient->ctxt.tunnelHttpCtxt.pbuf = (const char *) rtmptbuf;
  pClient->ctxt.tunnelHttpCtxt.szbuf = szrtmptbuf;
  pClient->ctxt.tunnelHttpCtxt.tmtms = tmtms;

  create_post_hdrs(kvs, sizeof(kvs) / sizeof(kvs[0]));

  uri = RTMPT_URL_IDENT;
  memset(&httpResp, 0, sizeof(httpResp));
  szbuf = sizeof(buf);
  pdata = httpcli_post(uri, buf, &szbuf, &pClient->ctxt.pSd->netsocket,
                       (const struct sockaddr *) &pClient->ctxt.pSd->sa,
                       tmtms, pClient->ctxt.phosthdr, userAgent, &httpResp, kvs, post, 1);

  if(httpResp.statusCode != HTTP_STATUS_NOTFOUND) {
    LOG(X_ERROR("RTMPT expecting %d to %s but got %d"), HTTP_STATUS_NOTFOUND, uri, httpResp.statusCode);
    return -1;
  }

  uri = RTMPT_URL_OPEN;
  memset(&httpResp, 0, sizeof(httpResp));
  szbuf = sizeof(buf);
  pdata = httpcli_post(uri, buf, &szbuf, &pClient->ctxt.pSd->netsocket,
                       (const struct sockaddr *) &pClient->ctxt.pSd->sa,
                       tmtms, pClient->ctxt.phosthdr, userAgent, &httpResp, kvs, post, 1);
  if(httpResp.statusCode != HTTP_STATUS_OK) {
    LOG(X_ERROR("RTMPT got %d response to %s"), httpResp.statusCode, uri);
    return -1;
  } else if((pClient->ctxt.tunnelSessionId = atoll((const char *) pdata)) == 0) {
    LOG(X_ERROR("RTMPT got invalid session id '%s', length: %d, to %s"), pdata, szbuf, uri);
    return -1;
  }

  snprintf(uribuf, sizeof(uribuf) - 1, "idle/%lld/%d", pClient->ctxt.tunnelSessionId, pClient->ctxt.tunnelSeqnum++);
  memset(&httpResp, 0, sizeof(httpResp));
  szbuf = sizeof(buf);
  pdata = httpcli_post(uribuf, buf, &szbuf, &pClient->ctxt.pSd->netsocket,
                       (const struct sockaddr *) &pClient->ctxt.pSd->sa,
                       tmtms, pClient->ctxt.phosthdr, userAgent, &httpResp, kvs, post, 1);
  if(httpResp.statusCode != HTTP_STATUS_OK) {
    LOG(X_ERROR("RTMPT got %d response to %s"), httpResp.statusCode, uribuf);
    return -1;
  } 

  return rc;
}
