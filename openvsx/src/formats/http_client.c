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


int httpcli_req_send(NETIO_SOCK_T *pnetsock, 
                         const struct sockaddr *psa, 
                         const char *method,
                         const char *uri, 
                         const char *connType, 
                         const char *host, 
                         const char *userAgent, 
                         const char *authorization,
                         const KEYVAL_PAIR_T *pHdrs,
                         unsigned char *postData,
                         unsigned int szpost) {

  int rc;
  int sz = 0;
  char tmps[2][256];
  char buf[4096];
  const KEYVAL_PAIR_T *pHdr = NULL;

  if(!pnetsock || !psa || !uri || !method) {
    return -1;
  }

  if((rc = snprintf(buf, sizeof(buf),
          "%s %s "HTTP_VERSION_DEFAULT"\r\n"
          HTTP_HDR_HOST": %s\r\n"
          HTTP_HDR_USER_AGENT": %s\r\n",
          method,
          uri, 
          (host && host[0] != '\0') ? host : FORMAT_NETADDR(*psa, tmps[0], sizeof(tmps[0])),
          (userAgent && userAgent[0] != '\0') ? userAgent : vsxlib_get_useragentstr(tmps[1], sizeof(tmps[1]))
          )) < 0) {
    return -1;
  }
  sz += rc;

  pHdr = pHdrs;
  while(pHdr) {
    if(pHdr->key[0] != '\0' && pHdr->val[0] != '\0') {
      if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n", pHdr->key, pHdr->val)) < 0) {
        return -1;
      }
      sz += rc;
    }
    pHdr = pHdr->pnext;
  }

  if(szpost > 0) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %d\r\n",
             HTTP_HDR_CONTENT_LEN, szpost)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(authorization && authorization[0] != '\0') {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
             HTTP_HDR_AUTHORIZATION, authorization)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(connType) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
             HTTP_HDR_CONNECTION, connType)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "\r\n")) < 0) {
    return -1;
  }
  sz += rc;

  LOG(X_DEBUG("HTTP%s %s to %s:%d%s"), 
              (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "", method,
              FORMAT_NETADDR(*psa, tmps[0], sizeof(tmps[0])), htons(PINET_PORT(psa)), uri);

  VSX_DEBUG_HTTP(
    LOG(X_DEBUG("HTTP - Sending header %d bytes"), sz);
    LOGHEXT_DEBUG(buf, sz);
  );

  if((rc = netio_send(pnetsock, psa, (unsigned char *) buf, sz)) < 0) {
    LOG(X_ERROR("Failed to send HTTP%s request data %d bytes to %s:%d "ERRNO_FMT_STR),
          (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "",
          sz, FORMAT_NETADDR(*psa, tmps[0], sizeof(tmps[0])), ntohs(PINET_PORT(psa)), ERRNO_FMT_ARGS);
    return -1;
  }

  if(postData && szpost > 0) {
    VSX_DEBUG_HTTP(
      LOG(X_DEBUG("HTTP - Sending POST %d bytes"), szpost);
      LOGHEXT_DEBUG(postData, szpost);
    );
    if((rc = netio_send(pnetsock, psa, postData, szpost)) < 0) {
      LOG(X_ERROR("Failed to send HTTP%s request POST data %d bytes to %s:%d "ERRNO_FMT_STR),
          (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "",
          szpost, FORMAT_NETADDR(*psa, tmps[0], sizeof(tmps[0])), ntohs(PINET_PORT(psa)), ERRNO_FMT_ARGS);
      return -1;
    }
  }

  return 0;
}

int httpcli_req_queryhdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, 
                          HTTP_RESP_T *pHttpResp, 
                          const struct sockaddr *psa, 
                          HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt, 
                          const char *descr) {
  char tmp[128];
  int rc = 0;
  const char *p = NULL;

  if(!pHdrCtxt || !pHttpResp || !pHdrCtxt->pbuf) {
    return -1;
  }

  if((rc = http_readhdr(pHdrCtxt)) <= 0 || pHdrCtxt->hdrslen == 0) {
    LOG(X_ERROR("Failed to read %s%s response headers (read:%d, recv rc:%d) from %s:%d"),
          descr, (pHdrCtxt->pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "", pHdrCtxt->idxbuf, rc, 
          FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
    return -1;
  }

  VSX_DEBUG_HTTP(
    LOG(X_DEBUG("HTTP - Read header %d bytes from %s:%d"), 
                pHdrCtxt->hdrslen, FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
    LOGHEXT_DEBUG(pHdrCtxt->pbuf, pHdrCtxt->hdrslen)
  );

  // 
  // Check and parse the first respone line status code 
  //
  if(rc >= 0 &&
    !(p = http_parse_respline(pHdrCtxt->pbuf, pHdrCtxt->hdrslen, pHttpResp))) {
    rc = -1;
  }

  if(pAuthCliCtxt) {
    pAuthCliCtxt->lastStatusCode = pHttpResp->statusCode; 
  }

  //fprintf(stderr, "HTTP_QUERYHDRS - parse done hdrslen:%d, idxbuf:%d, termcharidx:%d, tmtms:%u, szbuf:%d\n", pHdrCtxt->hdrslen, pHdrCtxt->idxbuf, pHdrCtxt->termcharidx, pHdrCtxt->tmtms, pHdrCtxt->szbuf);

  if(pHttpResp->statusCode == HTTP_STATUS_UNAUTHORIZED) {
    rc = -1;
    LOG(X_WARNING("%s Unauthorized code %d %s received from %s:%d"),
           descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
           FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
  } else if(pHttpResp->statusCode < HTTP_STATUS_OK ||
            pHttpResp->statusCode >= HTTP_STATUS_BADREQUEST) {
    LOG(X_ERROR("%s Error Status code %d %s received from %s:%d"),
          descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
          FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
    rc = -1;
  } else if(pHttpResp->statusCode >= HTTP_STATUS_MOVED_PERMANENTLY) {
    LOG(X_ERROR("%s Unhandled Status code %d %s received from %s:%d"),
          descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
          FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
    rc = 1;
  } else {
    LOG(X_DEBUG("%s Status code %d %s received from %s:%d"),
          descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
          FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
  }

  //fprintf(stderr, "%d '%s'\n", pHttpResp->statusCode, pHttpResp->version);
  //avc_dumpHex(stderr, p, pHdrCtxt->hdrslen - (p - pHdrCtxt->pbuf), 1);

  if((rc >= 0 || pHttpResp->statusCode == HTTP_STATUS_UNAUTHORIZED) && 
     !(p = http_parse_headers(p, pHdrCtxt->hdrslen - (p - pHdrCtxt->pbuf),
      pHttpResp->hdrPairs, sizeof(pHttpResp->hdrPairs)/sizeof(pHttpResp->hdrPairs[0])))) {

  }

  return rc;
}

int httpcli_calcdigestauth(HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt) {
  int rc = 0;
  char tmpH1[KEYVAL_MAXLEN];
  char tmpH2[KEYVAL_MAXLEN];
  char tmpDigest[KEYVAL_MAXLEN];
  AUTH_LIST_T authList;

  if(!pAuthCliCtxt || pAuthCliCtxt->realm[0] == '\0' || pAuthCliCtxt->nonce[0] == '\0' || !pAuthCliCtxt->puri) {
    return -1;
  }

  if((rc = auth_digest_getH1(pAuthCliCtxt->pcreds->username, pAuthCliCtxt->realm, pAuthCliCtxt->pcreds->pass,
                             tmpH1, sizeof(tmpH1))) < 0) {
    return rc;
  }

  if((rc = auth_digest_getH2(pAuthCliCtxt->lastMethod, pAuthCliCtxt->puri, tmpH2, sizeof(tmpH2))) < 0) {
    return rc;
  }

  if((rc = auth_digest_getDigest(tmpH1, pAuthCliCtxt->nonce, tmpH2, NULL, NULL, NULL,
                                   tmpDigest, sizeof(tmpDigest))) < 0) {
    return rc;
  }

  memset(&authList, 0, sizeof(authList));
  auth_digest_additem(&authList, "username", pAuthCliCtxt->pcreds->username, 1);
  auth_digest_additem(&authList, "realm", pAuthCliCtxt->realm, 1);
  auth_digest_additem(&authList, "nonce", pAuthCliCtxt->nonce, 1);
  auth_digest_additem(&authList, "uri", pAuthCliCtxt->puri, 1);
  if(pAuthCliCtxt->opaque && pAuthCliCtxt->opaque[0] != '\0') {
    auth_digest_additem(&authList, "opaque", pAuthCliCtxt->puri, 1);
  }
  auth_digest_additem(&authList, "response", tmpDigest, 1);

  if((rc = auth_digest_write(&authList, pAuthCliCtxt->authorization,
                                 sizeof(pAuthCliCtxt->authorization))) < 0) {
    return rc;
  }

  return rc;
}

int httpcli_authenticate(HTTP_RESP_T *pHttpResp, HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt) {
  int rc = 0;
  int do_basic = 0;
  char tmps[3][KEYVAL_MAXLEN];
  const char *enc = NULL;
  const char *realm_srv = NULL;
  const char *nonce_srv = NULL;
  const char *opaque_srv = NULL;
  const char *pAuthHdr = NULL;
  AUTH_LIST_T authList;

  if(!pHttpResp || !pAuthCliCtxt || !pAuthCliCtxt->pcreds || !pAuthCliCtxt->puri) {
    return -1;
  }

  if(!IS_AUTH_CREDENTIALS_SET(pAuthCliCtxt->pcreds)) {
    LOG(X_ERROR("Server requires authentication but client credentials have not been set"));
    return -1;
  }

  if(!(pAuthHdr = conf_find_keyval((const KEYVAL_PAIR_T *) &pHttpResp->hdrPairs, HTTP_HDR_WWW_AUTHENTICATE))) {
    LOG(X_WARNING("Server %s response does not include "HTTP_HDR_WWW_AUTHENTICATE" header"), 
                  http_lookup_statuscode(pHttpResp->statusCode));
    rc = -1;
  }

  if(rc >= 0) {
    enc = pAuthHdr;
    MOVE_WHILE_SPACE(enc);
    if(!strncasecmp(AUTH_STR_BASIC, enc, strlen(AUTH_STR_BASIC))) {
      enc += strlen(AUTH_STR_BASIC);
      MOVE_WHILE_SPACE(enc);
      if(pAuthCliCtxt->pcreds->authtype == HTTP_AUTH_TYPE_ALLOW_BASIC ||
         pAuthCliCtxt->pcreds->authtype == HTTP_AUTH_TYPE_FORCE_BASIC) {
        do_basic = 1;
      } else {
        LOG(X_WARNING("Server response wants disabled Basic authentication"));
        rc = -1;
      }
    } else if(!strncasecmp(AUTH_STR_DIGEST, enc, strlen(AUTH_STR_DIGEST))) {
      enc += strlen(AUTH_STR_DIGEST);
      MOVE_WHILE_SPACE(enc);
      if(pAuthCliCtxt->pcreds->authtype == HTTP_AUTH_TYPE_FORCE_BASIC) {
        do_basic = 1;
      }
    } else {
      LOG(X_WARNING("Server response contains unsupported "HTTP_HDR_AUTHORIZATION" header type '%s'"), pAuthHdr);
      rc = -1;
    }
  }

  pAuthCliCtxt->do_basic = do_basic;
 
  if(rc >= 0 && do_basic) {

    if((rc = auth_basic_encode(pAuthCliCtxt->pcreds->username, pAuthCliCtxt->pcreds->pass, 
                               tmps[0], sizeof(tmps[0]))) < 0) {
      LOG(X_ERROR("Failed to encode basic auth"));
    }

    if((rc = snprintf(pAuthCliCtxt->authorization, sizeof(pAuthCliCtxt->authorization), 
                      "%s %s", AUTH_STR_BASIC, tmps[0])) < 0) {
      
    }

  } else if(rc >= 0) {

    //
    // Parse the Digest Authorization header
    //
    if(rc >= 0 && auth_digest_parse(enc, &authList) < 0) {
      LOG(X_WARNING("Failed to parse server "HTTP_HDR_AUTHORIZATION" '%s'"), pAuthHdr);
      rc = -1;
    } 
    if(rc >= 0 && 
       !(realm_srv = avc_dequote(conf_find_keyval(&authList.list[0], "realm"), tmps[0], sizeof(tmps[0])))) {
      LOG(X_WARNING("Server response header '"HTTP_HDR_AUTHORIZATION": %s' does not include realm"), pAuthHdr);
      rc = -1;
    }
    if(realm_srv) {
      strncpy(pAuthCliCtxt->realm, realm_srv, sizeof(pAuthCliCtxt->realm) - 1);
    }
    if(rc >= 0 && 
       !(nonce_srv = avc_dequote(conf_find_keyval(&authList.list[0], "nonce"), tmps[1], sizeof(tmps[1])))) {
      LOG(X_WARNING("Server response header '"HTTP_HDR_AUTHORIZATION": %s' does not include nonce"), pAuthHdr);
      rc = -1;
    }
    if(nonce_srv) {
      strncpy(pAuthCliCtxt->nonce, nonce_srv, sizeof(pAuthCliCtxt->nonce) - 1);
    }
    if(rc >= 0) {
       opaque_srv = avc_dequote(conf_find_keyval(&authList.list[0], "opaque"), tmps[2], sizeof(tmps[2]));
    }
    if(opaque_srv) {
      strncpy(pAuthCliCtxt->opaque, opaque_srv, sizeof(pAuthCliCtxt->opaque) - 1);
    }

    if(rc >= 0) {
      rc = httpcli_calcdigestauth(pAuthCliCtxt);
    }

  }

  return rc;
}
  
static int http_req_sendread(HTTP_PARSE_CTXT_T *pHdrCtxt, 
                             HTTP_RESP_T *pHttpResp,
                             const char *method,
                             const struct sockaddr *psa, 
                             const char *uri,
                             const char *connType,
                             const char *host,
                             const char *userAgent,
                             HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt, 
                             const KEYVAL_PAIR_T *pHdrs,
                             unsigned char *postData,
                             unsigned int szpost) {
  int rc = 0;
  unsigned int idxretry = 0; 

  if(!pHdrCtxt || !pHdrCtxt->pnetsock || !pHdrCtxt->pbuf || pHdrCtxt->szbuf <= 0 || !pHttpResp) {
    return -1;
  }

  do {

    if((rc = httpcli_req_send(pHdrCtxt->pnetsock, psa, method, uri, connType, 
                           host, userAgent, pAuthCliCtxt ? pAuthCliCtxt->authorization : NULL, 
                           pHdrs, postData, szpost) < 0)) {
      break;
    }

    if(pAuthCliCtxt) {
      strncpy(pAuthCliCtxt->lastMethod, HTTP_METHOD_GET, sizeof(pAuthCliCtxt->lastMethod) -1);
    }
    memset(pHttpResp, 0, sizeof(HTTP_RESP_T));

    //pHdrCtxt->tmtms = 0;
    HTTP_PARSE_CTXT_RESET(*pHdrCtxt);

    if((rc = httpcli_req_queryhdrs(pHdrCtxt, pHttpResp, psa, pAuthCliCtxt, "HTTP")) < 0) {
      //LOG(X_DEBUG("--REQ_QUERY_H rc:%d, idxretry:%d, statuscode:%d"), rc, idxretry, pHttpResp->statusCode);
      if(pHttpResp->statusCode == HTTP_STATUS_UNAUTHORIZED && pAuthCliCtxt) {

        httpcli_authenticate(pHttpResp, pAuthCliCtxt);

      } else {
        break;
      }
    }
    //LOG(X_DEBUG("REQ_QUERY_H rc:%d, idxretry:%d, statuscode:%d"), rc, idxretry, pHttpResp->statusCode);
  } while(++idxretry < 1);

  return rc;
}

int httpcli_connect(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, int tmtms, const char *descr) {
  char tmp[128];
  int rc = 0;

  if(!pnetsock || !psa) {
    return -1;
  }

  LOG(X_DEBUG("%s%sonnecting %sto %s:%d"), descr ? descr : "", descr ? " c" : "C",
         (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : "", FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), 
          ntohs(PINET_PORT(psa)));

  if(PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET &&
    (PNETIOSOCK_FD(pnetsock) = net_opensocket(SOCK_STREAM, SOCK_RCVBUFSZ_DEFAULT, 0, NULL)) == INVALID_SOCKET) {
    return -1;
  }

  if((rc = net_connect_tmt(PNETIOSOCK_FD(pnetsock), psa, tmtms)) < 0) {
    if(tmtms > 0) {
      //
      // Close a socket which may have been shutdown or may be trying to connect to a remote host
      // otherwise the socket can be reused
      //
      netio_closesocket(pnetsock);
    }
    return rc;
  }

  //
  // Handle SSL client connection
  //
  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)){
    if((rc = vsxlib_ssl_initclient(pnetsock)) < 0 || (rc = netio_connectssl(pnetsock)) < 0) {
      netio_closesocket(pnetsock);
      return rc;
    }
  }

  LOG(X_DEBUG("%s%sonnected %sto %s:%d"), descr ? descr : "", descr ? " c" : "C",
    (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : "", FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));

  return rc;
}

int httpcli_gethdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_RESP_T *pHttpResp,
                  const struct sockaddr *psa, const char *uri,
                  const char *connType, unsigned int range0,
                  unsigned int range1, const char *host,
                  HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt) {

  int rc = 0;
  unsigned int sz = 0;
  KEYVAL_PAIR_T *pkv = NULL;
  KEYVAL_PAIR_T kv;

  if(!pHdrCtxt || !pHdrCtxt->pnetsock || !pHdrCtxt->pbuf || pHdrCtxt->szbuf <= 0 || !pHttpResp) {
    return -1;
  }

  memset(&kv, 0, sizeof(kv));

  //
  // Construct any byte range header
  //
  if(range0 > 0 || range1 > 0) {
    snprintf(kv.key, sizeof(kv.key), "Range");
    if((rc = snprintf(kv.val, sizeof(kv.val), "bytes=%d-", range0)) > 0) {
      sz += rc;
      if(range1 > 0) {
        snprintf(&kv.val[sz], sizeof(kv.val) - sz, "%d", range1);
      }
    }
    pkv = &kv;
  }

  rc = http_req_sendread(pHdrCtxt, pHttpResp, HTTP_METHOD_GET, psa, uri, connType, 
                     host, NULL, pAuthCliCtxt, pkv, NULL, 0);
  return rc;
}

unsigned char *httpcli_get_contentlen_start(HTTP_RESP_T *pHttpResp,
                                    HTTP_PARSE_CTXT_T *pHdrCtxt,
                                    unsigned char *pbuf, unsigned int szbuf,
                                    int verifybufsz,
                                    unsigned int *pcontentLen) {
  const char *p;
  int contentLen = 0;
  unsigned int consumed = 0;
  unsigned char *pdata = NULL;

  if(!pHttpResp || !pHdrCtxt || !pbuf) {
    return NULL;
  }

  if((p = conf_find_keyval(pHttpResp->hdrPairs, HTTP_HDR_CONTENT_LEN))) {
    contentLen = atoi(p);
  }

  if(contentLen <= 0) {
    LOG(X_ERROR("%s not found in response"), HTTP_HDR_CONTENT_LEN);
  } else if(verifybufsz && (unsigned int) contentLen >= szbuf) {
    LOG(X_ERROR("Input buffer size too small %d / %d"), szbuf, contentLen);
  } else if(pHdrCtxt->idxbuf > pHdrCtxt->hdrslen) {

    if((consumed = pHdrCtxt->idxbuf - pHdrCtxt->hdrslen) < szbuf) {
      if((char *) pbuf != pHdrCtxt->pbuf) {
        memcpy(pbuf, &pHdrCtxt->pbuf[pHdrCtxt->hdrslen], consumed);
        pdata = pbuf;
      } else {
        pdata = &pbuf[pHdrCtxt->hdrslen];
      }
    } else {
      LOG(X_ERROR("Input buffer size too small %d / %d"), szbuf, contentLen);
    }

  } else if(pHdrCtxt->idxbuf == pHdrCtxt->hdrslen) {
    pdata = pbuf;
  }

  if(pdata && pcontentLen) {
    *pcontentLen = contentLen;
  }

  return pdata;
}

const unsigned char *httpcli_getpage(const char *location, const char *puri, 
                                     unsigned char *pbuf, unsigned int *pszbuf,
                                     HTTP_STATUS_T *phttpStatus, unsigned int tmtms) {
  int rc = 0;
  NETIO_SOCK_T netsock;
  struct sockaddr_storage sa;
  unsigned int szbufin;
  unsigned char *pdata = NULL;
  uint16_t port;
  char portstr[128]; 
  const char *strhost = NULL;
  char strhostonly[512];
  HTTP_RESP_T httpResp; 
  CAPTURE_FILTER_TRANSPORT_T transType = CAPTURE_FILTER_TRANSPORT_UNKNOWN;
  
  if(!(strhost = location) || !pbuf || !pszbuf || *pszbuf <= 0) {
    return NULL;
  } 
  if(phttpStatus) {
    *phttpStatus = 0;
  }

  szbufin = *pszbuf;
  *pszbuf = 0;

  if(!puri) {
    puri = "/";
  }

  if((transType = capture_parseTransportStr(&strhost)) == CAPTURE_FILTER_TRANSPORT_UNKNOWN) {
    transType = CAPTURE_FILTER_TRANSPORT_HTTPGET;
  }

  if(strutil_parseAddress(strhost, strhostonly, sizeof(strhostonly), portstr, sizeof(portstr), NULL, 0) < 0) {
    return NULL;
  } else if((port = atoi(portstr)) <= 0) {
    port = HTTP_PORT_DEFAULT;
  }

  memset(&sa, 0, sizeof(sa));
  if((rc = net_resolvehost(strhostonly, &sa)) < 0) {
  //if((rc = net_getaddress(strhostonly, &sa)) < 0) {
    return NULL;
  }
  INET_PORT(sa) = htons(port);

  memset(&netsock, 0, sizeof(netsock));
  NETIOSOCK_FD(netsock) = INVALID_SOCKET;
  if(IS_CAPTURE_FILTER_TRANSPORT_SSL(transType)) {
    netsock.flags |= NETIO_FLAG_SSL_TLS;  
  }

  if((rc = httpcli_connect(&netsock, (struct sockaddr *) &sa, tmtms, NULL)) < 0) {
    netio_closesocket(&netsock);
    return NULL;
  }

  memset(&httpResp, 0, sizeof(httpResp));

  if(!(pdata = httpcli_loadpagecontent(puri, pbuf, &szbufin, &netsock,
                                      (const struct sockaddr *) &sa, tmtms, &httpResp, NULL, location))) {
  } else {
    *pszbuf = szbufin;
  }

  if(phttpStatus) {
    *phttpStatus = httpResp.statusCode;
  }
  netio_closesocket(&netsock);

  return pdata;
}

static unsigned char *httpcli_getbody(HTTP_RESP_T *pHttpResp, HTTP_PARSE_CTXT_T *pHdrCtxt, unsigned int *plen,
                                      const char *puri, const struct sockaddr *psa, struct timeval *ptv0) {

  unsigned char *pdata = NULL;
  unsigned char *pbuf = NULL;
  NETIO_SOCK_T *pnetsock = NULL;
  unsigned int tmtms;
  unsigned int sz;
  struct timeval tv1;
  unsigned int consumed = 0;
  unsigned int contentLen = 0;
  char tmp[128];

  pbuf = (unsigned char *) pHdrCtxt->pbuf;
  pnetsock = pHdrCtxt->pnetsock;
  tmtms = pHdrCtxt->tmtms;

  if((pdata = httpcli_get_contentlen_start(pHttpResp, pHdrCtxt, pbuf, *plen, 1, &contentLen))) {
    consumed = pHdrCtxt->idxbuf - pHdrCtxt->hdrslen;
  }

  if(pdata && net_setsocknonblock(NETIOSOCK_FD(*pnetsock), 1) < 0) {
    pdata = NULL;
  }

  while(pdata && consumed < contentLen && !g_proc_exit) {

    if((sz = netio_recvnb(pnetsock, (unsigned char *) &pdata[consumed], contentLen - consumed, 500)) > 0) {
      VSX_DEBUG_HTTP(
        LOG(X_DEBUG("HTTP - Read %d body bytes at [%d]/%d bytes from %s:%d"), 
                  sz, consumed, contentLen, FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)));
        LOGHEXT_DEBUG(&pdata[consumed], sz)
      );
      consumed += sz;
    } else if(sz == 0 && PSTUNSOCK(pnetsock)->rcvclosed) {
      LOG(X_WARNING("HTTP %s:%d%s connection closed"),
           FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)), puri);
      pdata = NULL;
      break; 
    }
    gettimeofday(&tv1, NULL);
    if(tmtms > 0 && consumed < contentLen && TIME_TV_DIFF_MS(tv1, *ptv0) > (unsigned int) tmtms) {
      LOG(X_WARNING("HTTP %s:%d%s timeout %d ms exceeded"),
           FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), ntohs(PINET_PORT(psa)), puri, tmtms);
      pdata = NULL;
      break;
    }
  }

  if(pdata && contentLen > 0 && consumed >= contentLen) {
    pdata[consumed] = '\0';
    *plen = consumed;
  } else {
    pdata = NULL;
    *plen = 0;
  }

  return pdata;
}

unsigned char *httpcli_loadpagecontent(const char *puri, 
                                unsigned char *pbuf, 
                                unsigned int *plen,
                                NETIO_SOCK_T *pnetsock,
                                const struct sockaddr *psa,
                                unsigned int tmtms,
                                HTTP_RESP_T *pHttpResp, 
                                HTTP_PARSE_CTXT_T *pHdrCtxt, 
                                const char *hdrhost) {
  HTTP_RESP_T httpResp;
  HTTP_PARSE_CTXT_T hdrCtxt;
  struct timeval tv0;
  unsigned char *pdata = NULL;
  char tmp[128];

  if(!puri || !pbuf || !plen || *plen <= 0 || !psa || !pnetsock) {
    return NULL;
  }

  if(!pHttpResp) {
    memset(&httpResp, 0, sizeof(httpResp));
    pHttpResp = &httpResp;
  }
  if(!pHdrCtxt) {
    memset(&hdrCtxt, 0, sizeof(hdrCtxt));
    hdrCtxt.pnetsock = pnetsock;
    hdrCtxt.pbuf = (char *) pbuf;
    hdrCtxt.szbuf = *plen;
    hdrCtxt.tmtms = tmtms;
    pHdrCtxt = &hdrCtxt;
  }

  VSX_DEBUG_HTTP ( LOG(X_DEBUG("HTTP - Getting URL %s:%d/%s"), 
                  FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), htons(PINET_PORT(psa)), puri));

  gettimeofday(&tv0, NULL);

  if(pHdrCtxt->hdrslen == 0) {

    if((httpcli_gethdrs(pHdrCtxt, pHttpResp,  psa, puri,
          http_getConnTypeStr(HTTP_CONN_TYPE_CLOSE), 0, 0, hdrhost, NULL)) < 0) {
      return NULL;
    }

  }

  pdata = httpcli_getbody(pHttpResp, pHdrCtxt, plen, puri, psa, &tv0);

  return pdata;
}

unsigned char *httpcli_post(const char *puri, 
                            unsigned char *pbuf, 
                            unsigned int *plen,
                            NETIO_SOCK_T *pnetsock,
                            const struct sockaddr *psa,
                            unsigned int tmtms,
                            const char *hdrhost,
                            const char *hdrUserAgent,
                            HTTP_RESP_T *pHttpResp,
                            const KEYVAL_PAIR_T *pkvs,
                            unsigned char *postData,
                            unsigned int szpost) {
  HTTP_RESP_T httpResp;
  HTTP_PARSE_CTXT_T hdrCtxt;
  int rc = 0;
  struct timeval tv0;
  unsigned char *pdata = NULL;
  char tmp[128];
  HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt = NULL;

  if(!puri || !pbuf || !plen || *plen <= 0 || !psa || !pnetsock) {
    return NULL;
  }

  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = pnetsock;
  hdrCtxt.pbuf = (char *) pbuf;
  hdrCtxt.szbuf = *plen;
  hdrCtxt.tmtms = tmtms;

  if(!pHttpResp) {
    memset(&httpResp, 0, sizeof(httpResp));
    pHttpResp = &httpResp;
  }

  VSX_DEBUG_HTTP ( LOG(X_DEBUG("HTTP - Posting URL %s:%d/%s"), 
                  FORMAT_NETADDR(*psa, tmp, sizeof(tmp)), htons(PINET_PORT(psa)), puri));

  gettimeofday(&tv0, NULL);

  if((rc = http_req_sendread(&hdrCtxt, pHttpResp, HTTP_METHOD_POST, psa, puri, 
                     http_getConnTypeStr(HTTP_CONN_TYPE_KEEPALIVE), 
                     hdrhost, hdrUserAgent, pAuthCliCtxt, pkvs, postData, szpost)) >= 0) {
    pdata = httpcli_getbody(pHttpResp, &hdrCtxt, plen, puri, psa, &tv0);
  }

  return pdata;
}
