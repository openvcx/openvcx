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

static const char *set_uri(const char *url) {
  const char *puri = url;

  if(*puri == '/' || *puri == '\0') {
    return puri;
  }

  while((*puri >= 'a' && *puri <= 'z') || (*puri >= 'A' && *puri <= 'Z')) {
    puri++;
  }
  while(*puri == ':') { 
    puri++;
  }

  while(*puri == '/') { 
    puri++;
  }

  while(*puri != '/' && *puri != '\0') { 
    puri++;
  }

  return puri;
}

char *http_req_dump_uri(const HTTP_REQ_T *pReq, char *buf, unsigned int szbuf) {
  const KEYVAL_PAIR_T *pKv;
  int rc;
  unsigned int idx = 0; 

  buf[0] = '\0';
  pKv = pReq->uriPairs;
  while(pKv) {

    if(idx + 3 >= szbuf) {
      sprintf(&buf[MIN(szbuf - 3 - 1, idx)], "...");
      break;
    }

    if((pKv->key[0] != '\0' || pKv->val[0] != '\0') &&
       (rc = snprintf(&buf[idx], szbuf - idx - 3, "%s%s=%s", idx > 0 ? "&" : "?", pKv->key, pKv->val)) > 0) {
      // The return value may actually be the required length to fulfill the entire print
      idx += MIN(szbuf - idx - 3, rc);
    }
    pKv = pKv->pnext;
  }

  return buf;
}

static const char *parse_req_get_uri(const char *buf, unsigned int len, 
                                     HTTP_REQ_T *pReq) {

  const char *p = buf;
  const char *p2, *p3; 
  const char *puristart;
  int rc;
  unsigned int lenval;
  unsigned int idxpair;
  char buftmp[sizeof(pReq->reqPairs[0].val)];

  //fprintf(stderr, "srvhttp.c::parse_req_get_uri\n"); avc_dumpHex(stderr, buf, MIN(256, len), 1);

  for(idxpair = 0; 
      idxpair < sizeof(pReq->uriPairs) / sizeof(pReq->uriPairs[0]); 
      idxpair++) {
    pReq->uriPairs[idxpair].pnext = NULL;
  }
  idxpair = 0;

  while(p < (buf + len) && *p != '\0' && *p != ' ') {
    p++;
  }

  if((lenval = (p - buf)) > sizeof(pReq->method) - 1) {
    lenval = sizeof(pReq->method) - 1;
  }
  memcpy(pReq->method, buf, lenval);

  while(p < (buf + len) && *p == ' ') {
    p++;
  }

  p2 = p;

  while(p2 < (buf + len) && *p2 != ' ' && *p2 != '?' && *p2 != '\r' && *p2 != '\n') {
    p2++;
  }

  if((lenval = (p2 - p)) > sizeof(pReq->url) - 1) {
    lenval = sizeof(pReq->url) - 1;
  }
  memcpy(pReq->url, p, lenval);

  if((p3 = http_urldecode(pReq->url, buftmp, sizeof(buftmp))) != pReq->url) {
    pReq->url[sizeof(pReq->url) - 1] = '\0';
    strncpy(pReq->url, buftmp, sizeof(pReq->url) - 1);
  }

  //pReq->puri = pReq->url;
  pReq->puri = (char *) set_uri(pReq->url);

  while(p2 < (buf + len) && (*p2 == '?')) {
    p2++;
  }

  p = p2;

  while(p2 < (buf + len) && *p2 != '\0' && *p2 != ' ' && *p2 != '\r' && *p2 != '\n') {
    p2++;
  }

  //
  // Parse request key value pairs
  //
  p3 = puristart = p;
  while(p < p2) {
    while(p3 < p2 && *p3 != '\0' && *p3 != '\r' && *p3 != '\n' && *p3 != '&') {
      p3++;
    }

    if((rc = conf_parse_keyval(&pReq->uriPairs[idxpair], p, p3 - p, '=', 0)) >= 1) {

      if(rc == 2) {
        if((p = http_urldecode(pReq->uriPairs[idxpair].val, buftmp, sizeof(buftmp)))
           != pReq->uriPairs[idxpair].val) {
         //fprintf(stdout, "url param decoded '%s'->'%s'\n", pReq->uriPairs[idxpair].val, buftmp);
          pReq->uriPairs[idxpair].val[sizeof(pReq->uriPairs[idxpair].val) - 1] = '\0';
          strncpy(pReq->uriPairs[idxpair].val, buftmp, sizeof(pReq->uriPairs[idxpair].val) - 1);
        }
      } else if(rc == 1) {
        pReq->uriPairs[idxpair].val[0] = '\0';
      }

      if(idxpair > 0) {
        pReq->uriPairs[idxpair-1].pnext = &pReq->uriPairs[idxpair];
      }
      if(++idxpair >= sizeof(pReq->uriPairs) / sizeof(pReq->uriPairs[0])) {
        if(*p3 == '&') {
          LOG(X_WARNING("Failed to parse entire URL request after %d URI parameters at %c%c%c%c..."), idxpair, p3[0], p3[1], p3[2], p3[3]);
        }
        break;
      }
    }

    while(p3 < p2 && *p3 == '&') {
      p3++;
    }
    p = p3;
  }

  //avc_dumpHex(stderr, puristart, p3 - puristart, 1);
  //char bufd[16]; fprintf(stderr, "DUMP:'%s'\n", http_req_dump_uri(pReq, bufd, sizeof(bufd)));

  while(p2 < (buf + len) && *p2 == ' ') {
    p2++;
  }
  p = p2;
  while(p2 < (buf + len) && *p2 != '\0' && *p2 !='\r' && *p2 != '\n') {
    p2++;
  }
  if((lenval = (p2 - p)) > sizeof(pReq->version) - 1) {
    lenval = sizeof(pReq->version) - 1;
  }
  memcpy(pReq->version, p, lenval);

  return p;
}

const char *http_parse_req_uri(const char *buf, unsigned int len, HTTP_REQ_T *pReq,
                                 int verifyhttpmethod) {

  const char *p = NULL;

  if(len < 5 || !buf || !pReq) {
    return NULL; 
  }

  if(!verifyhttpmethod || !strncmp(buf, HTTP_METHOD_GET, 3) || 
                          !strncmp(buf, HTTP_METHOD_HEAD, 4) ||
                          !strncmp(buf, HTTP_METHOD_POST, 4)) {
    if((p = parse_req_get_uri(buf, len, pReq)) == NULL) {
      LOG(X_ERROR("Failed to parse HTTP client request")); 
    }
  } else {
    LOG(X_ERROR("HTTP method %c%c%c%c not supported"), buf[0], buf[1], buf[2], buf[3]);
  }

  return p;
}

#if 0

static int consume_form_prefix(HTTP_REQ_T *pReq, unsigned int idxLast) {
                   
  const char *pHdr;
  const char *p, *p2, *p3;
  unsigned int lentmp = 0;
  char tmpbuf[32];
  unsigned int contentLen = 0;
  const char *buf = &pReq->buf[pReq->postData.idxRead];
  unsigned int len = idxLast - pReq->postData.idxRead;

  if(!(pHdr = conf_find_keyval(pReq->reqPairs, HTTP_HDR_CONTENT_LEN))) {
    LOG(X_ERROR("No %s request header in form data"), HTTP_HDR_CONTENT_LEN);
    return -1;
  }
  contentLen = atoi(pHdr);
  fprintf(stdout, "http content-length:%d\n", contentLen);
  if(!(pHdr= conf_find_keyval(pReq->reqPairs, HTTP_HDR_CONTENT_TYPE))) {
    LOG(X_ERROR("No %s request header in form data"), HTTP_HDR_CONTENT_TYPE);
    return -1;
  }
  if((p = strstr(pHdr, "boundary="))) {
    p += 9;
    pReq->postData.boundary[sizeof(pReq->postData.boundary) - 1] = '\0';
    strncpy(pReq->postData.boundary, (char *) p, sizeof(pReq->postData.boundary) - 1);
    pReq->postData.lenBoundary = strlen(pReq->postData.boundary);
  } else {
    LOG(X_ERROR("No 'boundary=' found in Content-Type='%s'"), pHdr);
    return -1;
  }
//fprintf(stdout, "boundary:'%s' '%s' %d\n", p, boundary, lentmp);
//fprintf(stdout, "abc%sABC len:%d\n", buf, len);
  if(!(p = avc_binstrstr(buf, len, pReq->postData.boundary, 
                      pReq->postData.lenBoundary))) {
    LOG(X_ERROR("form boundary '%s' not found in form data"), pReq->postData.boundary);
fprintf(stdout, "len:%d\n", len);for(lentmp=0;lentmp<len;lentmp++)fprintf(stdout, "'%c' ", buf[lentmp]);
    return -1;
  }
  while(*p == '\r' || *p == '\n') {
    p++;
  }
  p2 = p;
  while((ptrdiff_t)(p2 - buf) < (ptrdiff_t) len && *p2 != '\r' && *p2 != '\n') {
    p2++;
  }

  if((p = avc_binstrstr(p, p2 - p,  "filename=", 9))) {
    if(*p == '"') {
      p++;
    }
    p3 = p;
    while(p3 < p2 && *p3 != '"' && *p3 != '\r' && *p3 != '\n') {
      p3++;
    }
    if((lentmp = (ptrdiff_t) (p3 - p)) >= sizeof(pReq->postData.filename)) {
      lentmp = sizeof(pReq->postData.filename)- 1; 
    }
    memcpy(pReq->postData.filename, p, lentmp);
  }
//fprintf(stdout, "--%s--\n", pReq->postData.filename);
  snprintf((char *) tmpbuf, sizeof(tmpbuf), "\r\n\r\n");
  if(!(p = avc_binstrstr(p2, len - (p2 - buf), tmpbuf, 4))) {
    LOG(X_ERROR("Unable to find start of form data"));
    return -1;
  }
//fprintf(stdout, "data: '%c', '%c' '%c' '%c' '%c' '%c'\n", p[0], p[1], p[2], p[3], p[4], p[5]);
//fprintf(stdout, "--%s-- %d\n", buf, len);

  pReq->postData.contentLen = contentLen - (p - buf); 
  pReq->postData.idxRead = (unsigned int) (ptrdiff_t) (p - pReq->buf);

  return 0;
}
#endif // 0

int http_req_parsebuf(HTTP_REQ_T *pReq, const  char *buf, unsigned int len, int verifyhttpmethod) {
  unsigned int tmp;
  const char *p;

  memset(pReq, 0, sizeof(HTTP_REQ_T));

  //fprintf(stderr, "req_parsebuf... before parse_req_uri len:%d\n", len); avc_dumpHex(stderr, buf, len, 1);

  if((p = http_parse_req_uri(buf, len, pReq, verifyhttpmethod)) == NULL) {
    return -1; 
  }
  tmp = len - (p - buf);

  //fprintf(stderr, "req_parsebuf... after parse_erq_uri len:%d, tmp:%d\n", len, tmp); avc_dumpHex(stderr, p, tmp, 1);

  if((p = http_parse_headers(p, tmp, pReq->reqPairs,
                 sizeof(pReq->reqPairs) / sizeof(pReq->reqPairs[0]))) == NULL) {
    LOG(X_ERROR("Failed to parse request headers '%s' len:%d"), buf, len);
    return -1;
  }
  
  if((p = conf_find_keyval(pReq->reqPairs, HTTP_HDR_CONNECTION)) &&
    !strncasecmp(p, http_getConnTypeStr(HTTP_CONN_TYPE_KEEPALIVE), 10)) {
    pReq->connType = HTTP_CONN_TYPE_KEEPALIVE;
  }


 //TODO: handle POST submission of playlists / video file upload
/*
  if(!strncmp(pReq->method, "POST", 4)) {
    pReq->postData.lenInBuf = rc;
    pReq->postData.idxRead = (unsigned int) (ptrdiff_t) (p - buf);

fprintf(stdout, "POST data len:%d (%d - %d)\n", rc - pReq->postData.idxRead, rc, pReq->postData.idxRead);

if(rc == pReq->postData.idxRead) {
  fprintf(stdout, "ALL:----%s---done---\n", buf);
}
    if(consume_form_prefix(pReq, rc) < 0) {
      return -1;
    }
fprintf(stdout, "POST idx:%d contlen:%d file:'%s' '%s'%d\n", pReq->postData.idxRead, pReq->postData.contentLen, pReq->postData.filename, pReq->postData.boundary, pReq->postData.lenBoundary);

  }
*/

  //fprintf(stderr, "HTTP request rc:%d -%s-\n", rc, buf);

  return 0;
}


int http_req_readparse(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, unsigned int tmtsec, int verifyhttpmethod) {
  char buf[4096];
  HTTP_PARSE_CTXT_T hdrCtxt;

  if(!pSd || !pReq) {
    return -1;
  }

  buf[0] = '\0';
  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  hdrCtxt.pnetsock = &pSd->netsocket;
  hdrCtxt.pbuf = buf;
  hdrCtxt.szbuf = sizeof(buf);
  if(tmtsec == 0) {
    tmtsec = HTTP_REQUEST_TIMEOUT_SEC;
  }
  hdrCtxt.tmtms = tmtsec * 1000;

  return http_req_readpostparse(&hdrCtxt, pReq, verifyhttpmethod);
}

int http_req_readpostparse(HTTP_PARSE_CTXT_T *pHdrCtxt, HTTP_REQ_T *pReq, int verifyhttpmethod) {
  int rc;

  if(!pReq || !pHdrCtxt || !pHdrCtxt->pnetsock || PNETIOSOCK_FD(pHdrCtxt->pnetsock) == INVALID_SOCKET || 
     !pHdrCtxt->pbuf || pHdrCtxt->szbuf <= 0) {
    return -1;
  }

  //fprintf(stderr, "http_req_readpostparse\n");

  if((rc = http_readhdr(pHdrCtxt)) <= 0 || pHdrCtxt->hdrslen == 0) {
     //fprintf(stderr, "HTTP_READHDR:%d HDRSLEN:%d\n", rc, pHdrCtxt->hdrslen);

    // Check if an error occurred or of a persisent connection just timed out
    if(rc < 0) {
      LOG(X_ERROR("Failed to read client HTTP request headers (read:%d, recv rc:%d)"), 
          pHdrCtxt->idxbuf, rc);
      return rc;
    }
    // The request was not processed because the client has already disconnected,
    // likely after the prior request
    return 0;
  }

  VSX_DEBUG_HTTP( 
    LOG(X_DEBUG("HTTP - Read header %d bytes"), pHdrCtxt->hdrslen);
    LOGHEXT_DEBUG(pHdrCtxt->pbuf, pHdrCtxt->hdrslen) 
  );

  if(pHdrCtxt->tmtms > 0) {
    if(net_setsocknonblock(PNETIOSOCK_FD(pHdrCtxt->pnetsock), 0)  < 0) {
      return -1;
    }
  }

  if((rc = http_req_parsebuf(pReq, pHdrCtxt->pbuf, pHdrCtxt->hdrslen, verifyhttpmethod)) < 0) {
    return rc;
  }

  //fprintf(stderr, "HTTP HDRs: end:%d idx:%d\n", pHdrCtxt->hdrslen, pHdrCtxt->idxbuf); avc_dumpHex(stderr, buf, pHdrCtxt->hdrslen, 1);

  return 1;
}

static const char *getweekdaystr(int wkday) {
  switch(wkday) {
    case 0: 
      return "Sun";
    case 1: 
      return "Mon";
    case 2: 
      return "Tue";
    case 3: 
      return "Wed";
    case 4: 
      return "Thu";
    case 5: 
      return "Fri";
    default: 
      return "Sat";
  }
}
static const char *getmonthstr(int month) {
  switch(month) {
    case 0: 
      return "Jan";
    case 1: 
      return "Feb";
    case 2: 
      return "Mar";
    case 3: 
      return "Apr";
    case 4: 
      return "May";
    case 5: 
      return "Jun";
    case 6: 
      return "Jul";
    case 7: 
      return "Aug";
    case 8: 
      return "Sep";
    case 9: 
      return "Oct";
    case 10: 
      return "Nov";
    default: 
      return "Dec";
  }
}


int http_format_date(char *buf, unsigned int len, int logfmt) {
  time_t tmnow;
  struct tm *ptm;
  int rc;

  if(!buf || len < 0) {
    return -1;
  }

  tmnow = time(NULL);
  ptm = gmtime(&tmnow);

  if(logfmt) {
    rc = snprintf(buf, len, "%d/%s/%d:%02d:%02d:%02d GMT", 
           ptm->tm_mday, 
           getmonthstr(ptm->tm_mon), 
           ptm->tm_year + 1900,
           ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  } else {
    rc = snprintf(buf, len, "%s, %d %s %d %02d:%02d:%02d GMT",
           getweekdaystr(ptm->tm_wday),
           ptm->tm_mday,
           getmonthstr(ptm->tm_mon),
           ptm->tm_year + 1900,
           ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  }

  return rc;
}


int http_log_setfile(const char *path) {
  int rc = 0;

  g_http_log_path = path;

  return rc;
}

int http_log(const SOCKET_DESCR_T *pSd, const HTTP_REQ_T *pReq,
             enum HTTP_STATUS statusCode, FILE_OFFSET_T len) {
  int rc = 0;
  char dateStr[96];
  char tmp[128];
  char buf[1024];
  const char *userAgent = NULL;
  const char *referer = NULL;
  struct stat st;
  static pthread_mutex_t g_http_log_mtx;
  static int g_http_log_mtx_init;
  static FILE_HANDLE g_http_log_fp;

  if(!g_http_log_path ||  g_http_log_path[0] == '\0') {
    return 0;
  }

  if(!g_http_log_mtx_init) {
    g_http_log_mtx_init = 1;
    g_http_log_fp = FILEOPS_INVALID_FP;
    pthread_mutex_init(&g_http_log_mtx, NULL);
  }  

  if((rc = http_format_date(dateStr, sizeof(dateStr), 1)) >= 0) {

    if(!(userAgent = conf_find_keyval(pReq->reqPairs, HTTP_HDR_USER_AGENT)) ||
       userAgent[0] == '\0') {
      userAgent = "-";
    }

    if(!(referer = conf_find_keyval(pReq->reqPairs, HTTP_HDR_REFERER)) ||
       referer[0] == '\0') {
      referer = "-";
    }

    rc = snprintf(buf, sizeof(buf), "%s - - [%s] \"%s %s %s\" %d %lld \"%s\" \"%s\""EOL,
          FORMAT_NETADDR(pSd->sa, tmp, sizeof(tmp)), dateStr, pReq->method, pReq->url, pReq->version, 
          statusCode, len, referer, userAgent);

    pthread_mutex_lock(&g_http_log_mtx);

    if(fileops_stat(g_http_log_path, &st) != 0) {
      if(g_http_log_fp != FILEOPS_INVALID_FP) {
        fileops_Close(g_http_log_fp);
      }
      g_http_log_fp = FILEOPS_INVALID_FP;  
    }

    if(g_http_log_fp == FILEOPS_INVALID_FP) {
      //TODO: don't hardcode log dir relative path
      if((g_http_log_fp = fileops_Open(g_http_log_path, O_CREAT | O_APPEND | O_RDWR)) == 
         FILEOPS_INVALID_FP) {
        rc = -1;
      }
    }

    if(rc > 0) {
      if((rc = fileops_WriteBinary(g_http_log_fp, (unsigned char *) buf, rc)) < 0) {
        fileops_Close(g_http_log_fp);
        g_http_log_fp = FILEOPS_INVALID_FP;  
      }
    }

    pthread_mutex_unlock(&g_http_log_mtx);
  }

  return rc;
}

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
                      const char *auth) {
  int rc;
  size_t sz = 0;
  char buf[2048];
  char dateStr[96];
  char tmp[128];

  if(!pSd) {
    return -1;
  }

  if(contentType == NULL && len > 0) {
    contentType = CONTENT_TYPE_TEXTPLAIN;
  }

  if(http_format_date(dateStr, sizeof(dateStr), 0) < 0) {
    return -1;
  }

  if((rc = snprintf(buf, sizeof(buf), "%s %d %s\r\n"
                             "%s: %s\r\n"
                             "Server: %s\r\n",
           (httpVersion && httpVersion[0] != '\0') ? httpVersion : HTTP_VERSION_DEFAULT, 
           statusCode, http_lookup_statuscode(statusCode),
           HTTP_HDR_DATE, dateStr,
           vsxlib_get_appnamewwwstr(tmp, sizeof(tmp)))) < 0) {
   return -1;
  }
  sz += rc;

  if(location) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
             HTTP_HDR_LOCATION, location)) < 0) {
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

#if 1

    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, 
                //"Access-Control-Expose-Headers: Date\r\n"
                //"Access-Control-Allow-Headers: Overwrite, Destination, Content-Type, Depth, User-Agent, X-File-Size, X-Requested-With, If-Modified-Since, X-File-Name, Cache-Control, Range\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                //"Access-Control-Allow-Methods: PROPFIND, PROPPATCH, COPY, MOVE, DELETE, MKCOL, LOCK, UNLOCK, PUT, GETLIB, VERSION-CONTROL, CHECKIN, CHECKOUT, UNCHECKOUT, REPORT, UPDATE, CANCELUPLOAD, HEAD, OPTIONS, GET, POST\r\n"
                //"Access-Control-Allow-Credentials: true\r\n"
                //"Accept-Ranges: bytes\r\n"
                )) < 0) {
      return -1;
    }
    sz += rc;

#endif // 0

  if(cookie && cookie[0] != '\0') {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
             HTTP_HDR_SET_COOKIE, cookie)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(contentType) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %s\r\n",
           HTTP_HDR_CONTENT_TYPE, contentType)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(etag) {

    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: \"%s\"\r\n",
           HTTP_HDR_ETAG, etag)) < 0) {
      return -1;
    }
    sz += rc;

  } else {

    if(httpVersion && !strcmp(httpVersion, HTTP_VERSION_1_0)) {

      if((rc = snprintf(&buf[sz], sizeof(buf) - sz,   
                            "Pragma: no-cache\r\n"
                            HTTP_HDR_CACHE_CONTROL": no-store\r\n")) < 0) {
        return -1;
      }

    } else {

      if((rc = snprintf(&buf[sz], sizeof(buf) - sz,   
                            "Expires: -1\r\n"
                            HTTP_HDR_CACHE_CONTROL": private, max-age=0\r\n")) < 0) {
                            //HTTP_HDR_CACHE_CONTROL": no-store\r\n")) < 0) {
        return -1;
      }
    }

    sz += rc;

  }

  if(pRange) {

    if(pRange->acceptRanges) {
      if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: bytes\r\n",
             HTTP_HDR_ACCEPT_RANGES)) < 0) {
        return -1;
      }
      sz += rc;
    }

    if(pRange->contentRange) {
      if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: bytes %"LL64"u-%"LL64"u/%"LL64"u\r\n",
             HTTP_HDR_CONTENT_RANGE, pRange->start, pRange->end, pRange->total)) < 0) {
        return -1;
      }
      sz += rc;
    }
  }

  //
  // Write WWW-Authenticate header
  //
  if(auth) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s\r\n", auth)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if(len > 0) {
    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s: %"LL64"u\r\n",
             HTTP_HDR_CONTENT_LEN, len)) < 0) {
      return -1;
    }
    sz += rc;
  }

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "\r\n")) < 0) {
    return -1;
  }
  sz += rc;

  VSX_DEBUG_HTTP( 
    LOG(X_DEBUG("HTTP - response headers length: %d"), sz);
    LOGHEXT_DEBUG(buf, MIN(sz, 512));
  )

  if((rc = netio_send(&pSd->netsocket, (struct sockaddr *) &pSd->sa, (unsigned char *) buf, sz)) < 0) {
    LOG(X_ERROR("Failed to send HTTP header %d bytes"), sz);
  }

  return 0;
}

int http_resp_send(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, 
                   enum HTTP_STATUS statusCode,
                   unsigned char *pData, unsigned int len) {
  int rc = 0;

  if(!pData) {
    len = 0; 
  }

  http_log(pSd, pReq, statusCode, (FILE_OFFSET_T) len);

  if((rc = http_resp_sendhdr(pSd, pReq->version, statusCode,
                             (FILE_OFFSET_T) len, CONTENT_TYPE_TEXTHTML, 
                             http_getConnTypeStr(pReq->connType), 
                             pReq->cookie, NULL, NULL, NULL, NULL)) < 0) {
    return rc;
  }

  //
  // If the request was HEAD then just return not sending any content body
  //
  if(!strncmp(pReq->method, HTTP_METHOD_HEAD, 4)) {
    return rc;
  }

  if(pData && len > 0) {

    VSX_DEBUG_HTTP(
      LOG(X_DEBUG("HTTP - response body length: %d"), len);
      LOGHEXT_DEBUG(pData, MIN(len, 512));
    )

    if((rc = netio_send(&pSd->netsocket, (const struct sockaddr *) &pSd->sa, pData, len)) < 0) {
      LOG(X_ERROR("Failed to send HTTP content data %d bytes"), len);
    }
  }

  return rc;
}

static int http_resp_send_unauthorized(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, const char *auth) {
                   
  int rc = 0;
  enum HTTP_STATUS statusCode = HTTP_STATUS_UNAUTHORIZED;

  http_log(pSd, pReq, statusCode, (FILE_OFFSET_T) 0);

  if((rc = http_resp_sendhdr(pSd, pReq->version, statusCode,
                             (FILE_OFFSET_T) 0, CONTENT_TYPE_TEXTHTML, 
                             http_getConnTypeStr(pReq->connType), 
                             pReq->cookie, NULL, NULL, NULL, auth)) < 0) {
    return rc;
  }

  return rc;
}

static int resp_sendfile(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq,
                       const char *path, const char *contentType,
                       const char *etag) {
  FILE_STREAM_T fileStream;
  int rc = 0;
  FILE_OFFSET_T lentot;
  FILE_OFFSET_T idx = 0;
  unsigned int lenread;
  unsigned char buf[4096];
  char etagbuf[64];
  const char *pHdr;

  if(etag) {
    if(etag[0] == '\0') {
      if(!(etag = http_make_etag(path, etagbuf, sizeof(etagbuf)))) {
        LOG(X_WARNING("Failed to create etag for path '%s'"), path);
      }
    }
    if((pHdr = conf_find_keyval(pReq->reqPairs, HTTP_HDR_IF_NONE_MATCH)) &&
       (pHdr = avc_dequote(pHdr, NULL, 0)) &&
       (etag && !strcmp(pHdr, etag))) {

      LOG(X_DEBUG("'%s' Not modified for etag '%s'"), path, etag);

      http_log(pSd, pReq, HTTP_STATUS_NOTMODIFIED, 0);

      rc = http_resp_sendhdr(pSd, pReq->version, HTTP_STATUS_NOTMODIFIED,
                        0, NULL,
                        http_getConnTypeStr(pReq->connType), pReq->cookie,
                        NULL, etag, NULL, NULL);
      return rc;
    }
  }

  if(OpenMediaReadOnly(&fileStream, path) != 0) {
    http_resp_send(pSd, pReq, HTTP_STATUS_NOTFOUND, (unsigned char *) 
              HTTP_STATUS_STR_NOTFOUND, strlen(HTTP_STATUS_STR_NOTFOUND));
    return -1;
  }

  lentot = fileStream.size;

  http_log(pSd, pReq, HTTP_STATUS_OK, lentot);

  if((rc = http_resp_sendhdr(pSd, pReq->version, HTTP_STATUS_OK,
                  lentot, contentType, http_getConnTypeStr(pReq->connType), 
                  pReq->cookie, NULL, etag, NULL, NULL)) < 0) {
    CloseMediaFile(&fileStream);
    return rc;
  }

  //
  // If the request was HEAD then just return not sending any content body
  //
  if(!strncmp(pReq->method, HTTP_METHOD_HEAD, 4)) {
    CloseMediaFile(&fileStream);
    return rc;
  }

  while(idx < lentot) {

    if((lenread = (unsigned int) (lentot - idx)) > sizeof(buf)) {
      lenread = sizeof(buf);
    }

    if(ReadFileStream(&fileStream, buf, lenread) != lenread) {
      LOG(X_DEBUG("Failed to read %d bytes of '%s' at %d/%d"), lenread, path, idx, lentot);
      return -1;
    }

    VSX_DEBUG_HTTP(
      LOG(X_DEBUG("HTTP - response file-body length: %d [%llu]/%llu, path: '%s'"), lenread, idx, lentot, path);
      //LOGHEXT_DEBUG(buf, MIN(len, 16));
    )

    if((rc = netio_send(&pSd->netsocket, (const struct sockaddr *) &pSd->sa, buf, lenread)) < 0) {
      LOG(X_ERROR("Failed to send HTTP payload '%s' %u bytes (%llu/%llu)"), 
            path, lenread, idx, lentot);
      return -1;
    }
    idx += lenread;
  }

  CloseMediaFile(&fileStream);

  if(rc >= 0) {
    LOG(X_DEBUG("Sent file %"LL64"u bytes '%s'"), idx, path);
  }

  return rc;
}

int http_resp_sendfile(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus,
                       const char *path, const char *contentType, const char *etag) {
  int rc = 0;

  if(!pConn || !path) {
    return -1;
  } 

  if(!srv_ctrl_islegal_fpath(path)) {

    LOG(X_ERROR("Illegal file path '%s' referenced by URL '%s'"), path, pConn->httpReq.url);
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_NOTFOUND;
    } else {
      http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 1, NULL, NULL);
    }
    return -1;

  } else if(http_check_symlink(pConn, path) != 0) {

    LOG(X_ERROR("Loading %s is forbidden because following symbolic links is disabled"), path);
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    } else {
      http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_FORBIDDEN, 1, NULL, NULL);
    }
    return -1;

  }

  rc = resp_sendfile(&pConn->sd, &pConn->httpReq, path, contentType, etag);

  return rc;
}

int http_resp_error(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq, enum HTTP_STATUS statusCode, 
                    int close, const char *strResult, const char *auth) {
  int rc;
  const char *strStatusCode;

  if(!pReq || !pSd) {
    return -1;
  }

  if(close) {
    pReq->connType = HTTP_CONN_TYPE_CLOSE;
  }

  if(statusCode == HTTP_STATUS_UNAUTHORIZED && auth) {
    rc = http_resp_send_unauthorized(pSd, pReq, auth);
  } else {

    if(strResult) {
      strStatusCode = strResult;
    } else {
      strStatusCode = http_lookup_statuscode(statusCode);
    }

    rc = http_resp_send(pSd, pReq, statusCode, (unsigned char *) strStatusCode,  
                        strlen(strStatusCode));
  }

  return rc;
}

int http_resp_moved(SOCKET_DESCR_T *pSd, 
                    HTTP_REQ_T *pReq, 
                    enum HTTP_STATUS statusCode, 
                    int close, 
                    const char *location) {
  int rc;
  //const char *strStatusCode;
  unsigned int len = 0;

  if(!pReq || !pSd || !location) {
    return -1;
  }

  //strStatusCode = http_lookup_statuscode(statusCode);

  if(close) {
    pReq->connType = HTTP_CONN_TYPE_CLOSE;
  }

  http_log(pSd, pReq, statusCode, (FILE_OFFSET_T) len);

  if((rc = http_resp_sendhdr(pSd, pReq->version, statusCode,
                             (FILE_OFFSET_T) len, CONTENT_TYPE_TEXTHTML, 
                             http_getConnTypeStr(pReq->connType), 
                             pReq->cookie, NULL, NULL, location, NULL)) < 0) {
    return rc;
  }

  //if(pData && len > 0 && (rc = netio_send(&pSd->netsocket,  &pSd->sain, pData, len)) < 0) {
  //  LOG(X_ERROR("Failed to send HTTP content data %d bytes"), len);
  //}

  return rc;
}

int http_parse_rangehdr(const char *rangestr, HTTP_RANGE_HDR_T *pRange) {
  char buf[32];
  unsigned int sz;
  const char *p = rangestr;
  const char *p2;

  while(*p == ' ') {
    p++;
  }

  if(strncasecmp(p, "bytes=", 6)) {
    LOG(X_ERROR("Range header '%s' not supported"), rangestr);
    return -1;
  }

  p += 6;
  p2 = p;
  while(*p2 >= '0' && *p2 <= '9') {
    p2++;
  }

  memset(buf, 0, sizeof(buf));
  if((sz = p2 - p) >= sizeof(buf)) {
    sz = sizeof(buf) - 1;
  }
  memcpy(buf, p, sz);
  pRange->start = atoll(buf);

  while(*p2 == '-') {
    p2++;
  }
  p = p2;

  while(*p2 >= '0' && *p2 <= '9') {
    p2++;
  }

  if(p2 == p) {
    pRange->unlimited = 1;
  } else {
    memset(buf, 0, sizeof(buf));
    if((sz = p2 - p) >= sizeof(buf)) {
      sz = sizeof(buf) - 1;
    }
    memcpy(buf, p, sz);
    if((pRange->end = atoll(buf)) < pRange->start) {
      LOG(X_ERROR("Invalid range header value '%s'"), rangestr);
      return -1;
    }
  }

  return 0;
}

char *http_make_etag(const char *filepath, char *etagbuf, unsigned int szetagbuf) {
  unsigned int tag;
  unsigned int tm;
  struct stat st;
  FILE_OFFSET_T filesize;
 
  if(!filepath || !etagbuf) {
    return NULL;
  }

  if(fileops_stat(filepath, &st) != 0) {
    filesize = 0;
  } else {
    filesize = st.st_size;
  }
 
  if(filepath) {
    tag = mp2ts_crc((const unsigned char *) filepath, strlen(filepath));
  } else {
    tag = 0x01;
  }

  tm = st.st_mtime;

  if(snprintf(etagbuf, szetagbuf, "id-%2.2x%2.2x%2.2x%2.2x-%x-%llx", 
          (((unsigned char *)&tag)[0]),  (((unsigned char *)&tag)[1]),
          (((unsigned char *)&tag)[2]),  (((unsigned char *)&tag)[3]),
          tm, filesize) <= 0) {
    return NULL; 
  }

  return etagbuf;
}

