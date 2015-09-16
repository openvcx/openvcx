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


static const char httpstrendhdr[] = "\r\n\r\n";

const char *http_getConnTypeStr(enum HTTP_CONN_TYPE type) {

  static const char *connTypes[] = { "Close", "Keep-Alive" };

  switch(type) {
    case HTTP_CONN_TYPE_KEEPALIVE:
      return connTypes[HTTP_CONN_TYPE_KEEPALIVE];
    default:
     return connTypes[HTTP_CONN_TYPE_CLOSE];
  }

}

struct STATUS_CODE_MAP {
  enum HTTP_STATUS      code;
  const char           *str;
};


const char *http_lookup_statuscode(enum HTTP_STATUS statusCode) {

  unsigned int idx = 0;

  static const struct STATUS_CODE_MAP statusCodes[] = {
                               {  HTTP_STATUS_SERVERERROR, "SERVER ERROR" },
                               {  HTTP_STATUS_OK, "OK" },
                               {  HTTP_STATUS_PARTIALCONTENT, "PARTIAL CONTENT" },
                               {  HTTP_STATUS_MOVED_PERMANENTLY, "MOVED PERMANENTLY" },
                               {  HTTP_STATUS_FOUND, "FOUND" },
                               {  HTTP_STATUS_NOTMODIFIED, "NOT MODIFIED" },
                               {  HTTP_STATUS_BADREQUEST, "BAD REQUEST" },
                               {  HTTP_STATUS_UNAUTHORIZED, "UNAUTHORIZED" },
                               {  HTTP_STATUS_FORBIDDEN, "FORBIDDEN" },
                               {  HTTP_STATUS_NOTFOUND, "NOT FOUND" },
                               {  HTTP_STATUS_METHODNOTALLOWED, "METHOD NOT ALLOWED" },
                               {  HTTP_STATUS_SERVICEUNAVAIL, "SERVICE UNAVAILABLE" },
                               {  0, NULL }
                               };

  while(statusCodes[idx].str) {
    if(statusCode == statusCodes[idx].code) {
      return statusCodes[idx].str;
    }
    idx++;
  }

  //
  // Return SERVER ERROR
  //
  return statusCodes[0].str;
}

static unsigned int getNibble(unsigned char nibble) {

  if(nibble >= '0' && nibble <= '9') {
    return nibble - '0';
  } else if(nibble >= 'a' && nibble <= 'f') {
    return nibble - 'a' + 0xa;
  } else if(nibble >= 'A' && nibble <= 'F') {
    return nibble - 'A' + 0xa;
  }
  return 0;
}

int http_urlencode(const char *in, char *out, unsigned int lenout) {
  unsigned int idxIn;
  unsigned int idxOut = 0;
  unsigned char c;
  size_t len;

  if(in == NULL) {
    return 0;
  }

  len = strlen(in);


  for(idxIn = 0; idxIn < len; idxIn++) {

    if((in[idxIn] >= '0' && in[idxIn] <= '9') ||
       (in[idxIn] >= 'A' && in[idxIn] <= 'Z') ||
       (in[idxIn] >= 'a' && in[idxIn] <= 'z') ||
       in[idxIn] == '_' || in[idxIn] == '-' || in[idxIn] == '+' ||
       in[idxIn] == ',' || in[idxIn] == '.') {

      if(out) {
        if(idxOut + 1 >= lenout) {
          break;
        }
        out[idxOut] = in[idxIn];
      }
      idxOut++;

    } else {
      if(out) {

        if(idxOut + 3 >= lenout) {
          break;
        }

        out[idxOut++] = '%';
        if((c =  (in[idxIn] / 0x10)) <= 9) {
          out[idxOut] = c + '0';
        } else {
          out[idxOut] = c - 10 + 'a';
        }

        idxOut++;
        if((c =  (in[idxIn] % 0x10)) <= 9) {
          out[idxOut] = c + '0';
        } else {
          out[idxOut] = c - 10 + 'a';
        }

        idxOut++;

      } else {
        idxOut += 3;
      }
    }
  }

  if(out) {
    out[idxOut] = '\0';
  }

  return idxOut;
}

const char *http_urldecode(const char *in, char *out, unsigned int lenout) {
  unsigned int idxIn;
  unsigned int idxOut = 0;
  unsigned int len;


  if(in == NULL || strchr(in, '%') == NULL) {
    return in;
  }

  if(!out) {
    return NULL;
  }

  len = strlen(in);
  for(idxIn = 0; idxIn < len; idxIn++) {

    if(in[idxIn] == '%') {
      out[idxOut] = getNibble(in[++idxIn]) << 4;
      out[idxOut++] |= getNibble(in[++idxIn]);
    } else {
      out[idxOut++] = in[idxIn];
    }

    if(idxOut >= lenout) {
      break;
    }
  }

  out[idxOut] = '\0';

  return out;
}

int http_readhdr(HTTP_PARSE_CTXT_T *pCtxt) {
  int rc = 0;
  int rcvd = 0;
  unsigned int sz;
  unsigned int idx;
  unsigned int startidxbuf;
  char tmp[128];
  struct sockaddr_storage sa;
  struct timeval tv0, tv1;

  if(!pCtxt || !pCtxt->pbuf || !pCtxt->pnetsock || PNETIOSOCK_FD(pCtxt->pnetsock) == INVALID_SOCKET || 
    pCtxt->idxbuf >= pCtxt->szbuf) {
    return -1;
  }

  //pCtxt->idxbuf = 0;
  startidxbuf = pCtxt->idxbuf;
  pCtxt->hdrslen = 0;
  pCtxt->termcharidx = 0;
  pCtxt->rcvclosed = 0;

  if(net_setsocknonblock(PNETIOSOCK_FD(pCtxt->pnetsock), (pCtxt->tmtms > 0 ? 1 : 0))  < 0) {
    return -1;
  }

  gettimeofday(&tv0, NULL);

  do {

    //fprintf(stderr, "http_readhdr... pCtxt->hdrslen:%d, tmtms:%d\n", pCtxt->hdrslen, pCtxt->tmtms);

    rcvd = 0;
    if(pCtxt->tmtms > 0) {
      //fprintf(stderr, "calling net_recvnb %d into buf[%d]\n", pCtxt->tmtms, pCtxt->idxbuf);
      if((rcvd = netio_recvnb(pCtxt->pnetsock, (unsigned char *) &pCtxt->pbuf[pCtxt->idxbuf], 
                            pCtxt->szbuf - pCtxt->idxbuf, pCtxt->tmtms)) < 0) {

        //fprintf(stderr, "netio_recvnb idxbuf:%d rcvd:%d\n", pCtxt->idxbuf, rcvd);

        //
        // Its possible that the first read will return 0 (connection closed) because
        // the client already disconnected after the prior request
        //
        if(startidxbuf == pCtxt->idxbuf) {
          rcvd = 0;
          pCtxt->rcvclosed = 1;
        } else {
          return -1;
        }

      } else if(rcvd == 0) {
        pCtxt->rcvclosed = 1;
        rc = sizeof(sa);
        getpeername(PNETIOSOCK_FD(pCtxt->pnetsock), (struct sockaddr *) &sa, (socklen_t *) &rc);
        LOG(X_DEBUG("Timeout reached while waiting for HTTP Headers %d / %d, %dms from %s:%d"),
            pCtxt->idxbuf, pCtxt->hdrslen, pCtxt->tmtms, 
            FORMAT_NETADDR(sa, tmp, sizeof(tmp)), htons(INET_PORT(sa))); 
      }
      //fprintf(stderr, "called net_recvnb tmt: %d rcvd:%d\n", pCtxt->tmtms, rcvd);

    } else {
      //fprintf(stderr, "calling recv\n");
      if((rcvd = netio_recv(pCtxt->pnetsock, NULL, (unsigned char *) &pCtxt->pbuf[pCtxt->idxbuf], 
                          pCtxt->szbuf - pCtxt->idxbuf)) < 0) {
        //fprintf(stderr, "bad %d errno:%d\n", rcvd, errno);
        return rcvd;
      }
      if(rcvd == 0) {
        pCtxt->rcvclosed = 1;
      }
    }

    sz = rcvd + pCtxt->idxbuf;

    //fprintf(stderr, "http_readhdr rcvd %d [%d], sz now:%d tmt:%d\n", rcvd,  pCtxt->idxbuf, sz, pCtxt->tmtms); //avc_dumpHex(stderr, pCtxt->pbuf, rcvd, 1);

    if(pCtxt->rcvclosed && sz == startidxbuf) {
      return 0;
    }

    rc = 0;

    for(idx = pCtxt->idxbuf; idx < sz; idx++) {
//if(idx<300) fprintf(stderr, "--%d--%c-- 0x%x termc:%u\n", idx, pCtxt->pbuf[idx], pCtxt->pbuf[idx], pCtxt->termcharidx);
      if(pCtxt->pbuf[idx] == httpstrendhdr[pCtxt->termcharidx]) {
        pCtxt->termcharidx++;
      } else if(pCtxt->termcharidx > 0) {
        pCtxt->termcharidx = 0;
      }
      if(pCtxt->termcharidx >= 4) {
        rc = pCtxt->hdrslen = idx + 1;
        break;
      }
    }

    //pCtxt->idxbuf += sz;
    pCtxt->idxbuf += rcvd;

    //fprintf(stderr, "request hdrs idxbuf now: %d /%d hdrslen:%d '%c%c%c%c...%c%c%c'\n", pCtxt->idxbuf, sz, pCtxt->hdrslen, pCtxt->pbuf[0], pCtxt->pbuf[1], pCtxt->pbuf[2], pCtxt->pbuf[3], pCtxt->pbuf[sz-3], pCtxt->pbuf[sz - 2], pCtxt->pbuf[sz-1]); avc_dumpHex(stderr, pCtxt->pbuf, sz, 1);

    if(pCtxt->tmtms > 0 && pCtxt->hdrslen == 0) {
      gettimeofday(&tv1, NULL);
      if(TIME_TV_DIFF_MS(tv1, tv0) > pCtxt->tmtms) {
        rc = sizeof(sa);
        getpeername(PNETIOSOCK_FD(pCtxt->pnetsock), (struct sockaddr *) &sa, (socklen_t *) &rc);
        LOG(X_ERROR("Timeout reached (2) while waiting for HTTP Headers %d / %d, %dms from %s:%d"),
            pCtxt->idxbuf, pCtxt->hdrslen, pCtxt->tmtms, 
            FORMAT_NETADDR(sa, tmp, sizeof(tmp)), htons(INET_PORT(sa))); 
        rc = -1;
        break;
      }
    }

  } while(pCtxt->idxbuf < pCtxt->szbuf && pCtxt->hdrslen == 0 && !pCtxt->rcvclosed);

  //fprintf(stderr, "http_readhdr returning %d idxbuf:%d szbuf:%d hdrslenrc:%d\n", rcvd, pCtxt->idxbuf, pCtxt->szbuf, pCtxt->hdrslen);

  return rc;
}

const char *http_parse_headers(const char *buf, unsigned int len, 
                               KEYVAL_PAIR_T *pKvs, unsigned int numKvs) {
  const char *p = buf;
  const char *p2;
  int rc = 0;
  int newlidx = 0;
  unsigned int idxkv = 0;

  if(!buf || !pKvs) {
    return NULL;
  }

  for(idxkv = 0; idxkv < numKvs; idxkv++) {
    pKvs[idxkv].pnext = NULL;
  }
  idxkv = 0;
  p2 = p;

  //
  // Parse HTTP headers
  //
  while(p < (buf + len) && newlidx < 4) {

    //fprintf(stderr, "http_parse_headers len:%d/%d\n", p-buf, len );

    while(p2 < (buf + len) && *p2 != '\0' && *p2 != '\r' && *p2 != '\n') {
      p2++;
    }

    if(idxkv < numKvs) {
      if((rc = conf_parse_keyval(&pKvs[idxkv], p, p2 - p, ':', 0)) > 0) {
        if(idxkv > 0) {
          pKvs[idxkv - 1].pnext = &pKvs[idxkv];
        }
        VSX_DEBUG_HTTP( LOG(X_DEBUG("HTTP header[%d]: '%s'->'%s'"),  idxkv, pKvs[idxkv].key, pKvs[idxkv].val) );
        idxkv++;
      }
    }

    // Check for \r\n\r\n sequence marking end of HTTP headers
    newlidx = 0;
    while(p2 < (buf + len) && (*p2 == '\r' || *p2 == '\n')) {
      p2++;
      if(++newlidx == 4) {
        break;
      }
    }

    //
    // broken VLC may sometimes send multiple null bytes after string value
    //
    if(p2 < (buf + len) && newlidx < 4 && *p2 == '\0') {
      LOG(X_DEBUG("Client headers contain invalid NULL byte(s) at [%d]/%d"), (p2 - buf), len);
      //avc_dumpHex(stderr, buf, len, 1);
      while(p2 < (buf + len) && *p2 == '\0') {
         p2++; 
      }
    }

    p = p2;
  }

  return p2;
}

const char *http_parse_respline(const char *pBuf, unsigned int lenResp,
                                HTTP_RESP_T *pResp) {
  const char *p = pBuf;
  const char *p2;
  char tmp[16];
  unsigned int len;

  if(!pBuf || !pResp) {
    return NULL;
  }

  //
  // Get 'HTTP/1.x'
  //
  p2 = p;
  while(*p2 != ' ' && *p2 != '\r' && *p2 != '\n' && (ptrdiff_t) (p2 - p) < lenResp) {
    p2++;
  }
  if((len = (ptrdiff_t) (p2 - p)) >= sizeof(pResp->version)) {
    len = sizeof(pResp->version) - 1;
  }
  memcpy(pResp->version, p, len);
  while(*p2 == ' ' && (ptrdiff_t) (p2 - p) < lenResp) {
    p2++;
  }

  //
  // Get numeric status code
  //
  p = p2;
  while(*p2 != ' ' && *p2 != '\r' && *p2 != '\n' && (ptrdiff_t) (p2 - p) < lenResp) {
    p2++;
  }
  if((len = (ptrdiff_t) (p2 - p)) >= sizeof(tmp)) {
    len = sizeof(tmp) - 1;
  }
  memcpy(tmp, p, len);
  tmp[len] = '\0';
  pResp->statusCode = atoi(tmp);

  p = p2;
  while(*p2 != '\r' && *p2 != '\n' && (ptrdiff_t) (p2 - p) < lenResp) {
    p2++;
  }

  if(((ptrdiff_t)(p2 - p) +  2) <= lenResp &&
    (*p2 == '\r' || *p2 == '\n') && (*(p2 + 1) == '\r' || *(p2 + 1) == '\n')) {
    return (p2 + 2);
  }

  return NULL;
}



