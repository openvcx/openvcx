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


static int http_req_send(NETIO_SOCK_T *pnetsock, struct sockaddr_in *psain, const char *method,
                         const char *uri, const char *connType, unsigned int range0,
                         unsigned int range1, const char *host, const char *authorization) {

  int rc;
  int sz = 0;
  char tmp[256];
  char buf[2048];

  if(!pnetsock || !psain || !uri || !method) {
    return -1;
  }

  if((rc = snprintf(buf, sizeof(buf),
          "%s %s "HTTP_VERSION_DEFAULT"\r\n"
          "Host: %s\r\n"
          "User-Agent: %s\r\n",
          method,
          uri, 
          (host && host[0] != '\0') ? host : inet_ntoa(psain->sin_addr),
          vsxlib_get_appnamewwwstr(tmp, sizeof(tmp))
          )) < 0) {
    return -1;
  }
  sz += rc;

  if(range0 > 0 || range1 > 0) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "Range: bytes=%d-", range0)) < 0) {
      return -1;
    }
    sz += rc;
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%d", range1)) < 0) {
      return -1;
    }
    sz += rc;
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "\r\n")) < 0) {
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
              inet_ntoa(psain->sin_addr), htons(psain->sin_port), uri);

  VSX_DEBUG_HTTP(
    LOG(X_DEBUG("HTTP - Sending header %d bytes"), sz);
    LOGHEXT_DEBUG(buf, sz);
  );

  if((rc = netio_send(pnetsock, psain, (unsigned char *) buf, sz)) < 0) {
    LOG(X_ERROR("Failed to send HTTP%s request data %d bytes to %s:%d "ERRNO_FMT_STR),
          (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "",
          sz, inet_ntoa(psain->sin_addr), ntohs(psain->sin_port), ERRNO_FMT_ARGS);
    return -1;
  }

  return 0;
}

int httpcli_req_queryhdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_RESP_T *pHttpResp, 
                      struct sockaddr_in *psain, HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt, const char *descr) {
  int rc = 0;
  const char *p = NULL;

  if(!pHdrCtxt || !pHttpResp || !pHdrCtxt->pbuf) {
    return -1;
  }

  if((rc = http_readhdr(pHdrCtxt)) <= 0 || pHdrCtxt->hdrslen == 0) {
    LOG(X_ERROR("Failed to read %s%s response headers (read:%d, recv rc:%d) from %s:%d"),
          descr, (pHdrCtxt->pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "S" : "", pHdrCtxt->idxbuf, rc, 
          inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
    return -1;
  }

  VSX_DEBUG_HTTP(
    LOG(X_DEBUG("Read header %d bytes from %s:%d"), 
                pHdrCtxt->hdrslen, inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
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
           inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
  } else if(pHttpResp->statusCode < HTTP_STATUS_OK ||
            pHttpResp->statusCode >= HTTP_STATUS_BADREQUEST) {
    LOG(X_ERROR("%s Error Status code %d %s received from %s:%d"),
          descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
          inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
    rc = -1;
  } else if(pHttpResp->statusCode >= HTTP_STATUS_MOVED_PERMANENTLY) {
    LOG(X_ERROR("%s Unhandled Status code %d %s received from %s:%d"),
          descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
          inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
    rc = 1;
  } else {
    LOG(X_DEBUG("%s Status code %d %s received from %s:%d"),
          descr, pHttpResp->statusCode, http_lookup_statuscode(pHttpResp->statusCode), 
          inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
  }

  //fprintf(stderr, "%d '%s'\n", pHttpResp->statusCode, pHttpResp->version);
  //avc_dumpHex(stderr, p, pHdrCtxt->hdrslen - (p - pHdrCtxt->pbuf), 1);

  if((rc >= 0 || pHttpResp->statusCode == HTTP_STATUS_UNAUTHORIZED) && 
     !(p = http_parse_headers(p, pHdrCtxt->hdrslen - (p - pHdrCtxt->pbuf),
      pHttpResp->hdrPairs, sizeof(pHttpResp->hdrPairs)/sizeof(pHttpResp->hdrPairs[0])))) {

  }

  return rc;
}

static int req_queryhdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_RESP_T *pHttpResp, 
                              struct sockaddr_in *psain, HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt) {

  return httpcli_req_queryhdrs(pHdrCtxt, pHttpResp, psain, pAuthCliCtxt, "HTTP");
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
  
int httpcli_gethdrs(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_RESP_T *pHttpResp,
                  struct sockaddr_in *psain, const char *uri,
                  const char *connType, unsigned int range0,
                  unsigned int range1, const char *host,
                  HTTPCLI_AUTH_CTXT_T *pAuthCliCtxt) {
  int rc = 0;
  unsigned int idxretry = 0; 

  if(!pHdrCtxt || !pHdrCtxt->pnetsock || !pHdrCtxt->pbuf || pHdrCtxt->szbuf <= 0 || !pHttpResp) {
    return -1;
  }

  do {

    if((rc = http_req_send(pHdrCtxt->pnetsock, psain, HTTP_METHOD_GET, uri, connType, range0, 
                           range1, host, pAuthCliCtxt ? pAuthCliCtxt->authorization : NULL) < 0)) {
      break;
    }
    if(pAuthCliCtxt) {
      strncpy(pAuthCliCtxt->lastMethod, HTTP_METHOD_GET, sizeof(pAuthCliCtxt->lastMethod) -1);
    }
    memset(pHttpResp, 0, sizeof(HTTP_RESP_T));

    //pHdrCtxt->tmtms = 0;
    pHdrCtxt->hdrslen = 0;
    pHdrCtxt->idxbuf = 0;
    pHdrCtxt->termcharidx = 0;
    pHdrCtxt->rcvclosed = 0;

    if((rc = req_queryhdrs(pHdrCtxt, pHttpResp, psain, pAuthCliCtxt)) < 0) {
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

int httpcli_connect(NETIO_SOCK_T *pnetsock, struct sockaddr_in *psain, const char *descr) {
  int rc = 0;

  if(!pnetsock || !psain) {
    return -1;
  }

  LOG(X_DEBUG("%s%sonnecting %sto %s:%d"), descr ? descr : "", descr ? " c" : "C",
      (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : "", inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));

  if(PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET &&
    (PNETIOSOCK_FD(pnetsock) = net_opensocket(SOCK_STREAM, SOCK_RCVBUFSZ_DEFAULT, 0, NULL)) == INVALID_SOCKET) {
    return -1;
  }

  if((rc = net_connect(PNETIOSOCK_FD(pnetsock), psain)) < 0) {
    //netio_closesocket(pnetsock);
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
      (pnetsock->flags & NETIO_FLAG_SSL_TLS) ? "(SSL) " : "", inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));

  return rc;
}
