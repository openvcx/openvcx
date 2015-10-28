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

static int parseIpPortStr(const char *localAddrStr, 
                          struct sockaddr_storage *pLocalAddr,
                          uint16_t localPorts[], size_t maxLocalPorts, int requirePort) {

  size_t numPortsAdded = 0; 
  int rc = 0;
  char host[512];
  char ports[128];
  char uri[512];

  host[0] = '\0';
  ports[0] = '\0';
  uri[0] = '\0';
  memset(localPorts, 0, sizeof(uint16_t) * maxLocalPorts);

  if((rc = strutil_parseAddress(localAddrStr, host, sizeof(host), ports, sizeof(ports), uri, sizeof(uri)) < 0)) {
    return rc;
  }

  if(net_getaddress(host, pLocalAddr) != 0 ||
     (pLocalAddr->ss_family != AF_INET6 && ((struct sockaddr_in *) pLocalAddr)->sin_addr.s_addr == INADDR_NONE)) {
    LOG(X_ERROR("Invalid address specified in '%s'"), localAddrStr);
    return -1;
  } else if(!requirePort && ports[0] == '\0') {
    LOG(X_ERROR("No port specified in '%s'"), localAddrStr);
    return -1;
  }

  if((numPortsAdded = capture_parsePortStr(ports, localPorts, maxLocalPorts, 1)) < 0) {
    LOG(X_ERROR("Invalid port specified in '%s'"), localAddrStr);
    return numPortsAdded;
  }

  return numPortsAdded;
}

static int isDuplicatePort(unsigned short port,
                              const unsigned short ports[],
                              size_t numPorts) {
  size_t idx;
  for(idx = 0; idx < numPorts; idx++) {
    if(ports[idx] == port) {
      return 1;
    }
  }
  return 0;
}

static int addPorts(const char *str,
                    uint16_t ports[], 
                    size_t *pIdxStartPort,
                    size_t maxPorts,
                    int noDup) {
  const char *p2;
  unsigned short startPort;
  unsigned short endPort;
  size_t idxPort;
  size_t sz;
  size_t numPortsAdded = 0;
  char buf[256];

  if((p2 = strstr(str, "-"))) {
    if((sz = p2 - str) >= sizeof(buf)) {
      sz = sizeof(buf) - 1;
    }
    memcpy(buf, str, sz);
    buf[sz] = '\0';

    startPort = atoi(buf);
    if(startPort <= 0 || (endPort = atoi(p2 + 1)) < startPort) {
      return -1;
    }
    for(idxPort = 0; idxPort <= (size_t) (endPort - startPort) &&
                     *pIdxStartPort + numPortsAdded < maxPorts;
        idxPort++) {
      if(!noDup || !isDuplicatePort(startPort + idxPort, ports, *pIdxStartPort)) {
        ports[*pIdxStartPort + numPortsAdded] = startPort + idxPort;
        numPortsAdded++;
      }
    }
    (*pIdxStartPort) += numPortsAdded;

  } else {
    if((startPort = atoi(str)) > 0) {
      if(!noDup || !isDuplicatePort(startPort, ports, *pIdxStartPort)) {
        ports[(*pIdxStartPort)++] = startPort; 
        numPortsAdded++;
      }
    } else {
      return -1;
    }
  }
  return numPortsAdded;
}

int capture_parsePortStr(const char *str,
                 uint16_t localPorts[], 
                 size_t maxLocalPorts,
                 int noDup) {

  const char *p, *p2, *pdata;
  size_t numPortsAdded = 0; 
  char buf[64];
  int rc;

  p = str;

  while(p && numPortsAdded < maxLocalPorts) {

    if((p2 = strstr(p, ","))) {
      pdata = strutil_safe_copyToBuf(buf, sizeof(buf), p, p2);
    } else {
      pdata = p;
    }

    if((rc = addPorts(pdata, localPorts, &numPortsAdded, maxLocalPorts, noDup)) < 0) {
      LOG(X_ERROR("Invalid port specified in '%s'"), str);
      return rc;
    }

    if((p = p2)) {
      p++;
    }
  }

  return numPortsAdded;
}

static long getnum(const char *s) {
  int base = 10;

  if(s[0] == '0' && (s[1] == 'x' ||  s[1] == 'X')) {
    base = 16;
  }

  return strtol(s, NULL, base);
}

typedef struct PARSE_PAYLOADTYPES_CTXT {
  int isvid;
  int rc;
  int *vidPt;
  int *audPt;
} PARSE_PAYLOADTYPES_CTXT_T;

static int parse_pair_cb_payloadTypes(void *pArg, const char *pElement) {
  PARSE_PAYLOADTYPES_CTXT_T *pCtxt = (PARSE_PAYLOADTYPES_CTXT_T *) pArg;
  const char *p3;

  if((p3 = strchr(pElement, '='))) {
    if(TOUPPER(pElement[0]) == 'A') {
      pCtxt->isvid = 0;
    } else if(TOUPPER(pElement[0]) == 'V') {
      pCtxt->isvid = 1;
    }
    pElement = p3 + 1;
  }

  if(pCtxt->vidPt && pCtxt->isvid) {
    if(*pElement != '\0') {
      *pCtxt->vidPt = getnum(pElement);
      pCtxt->rc |= 0x01;
    }
    pCtxt->isvid = 0;
  } else if(pCtxt->audPt && !pCtxt->isvid) {
    if(*pElement != '\0') {
      *pCtxt->audPt = getnum(pElement);
      pCtxt->rc |= 0x02;
    }
    pCtxt->isvid = 1;
  }

  return 0;
}

int capture_parsePayloadTypes(const char *str, int *vidPt, int *audPt) {
  PARSE_PAYLOADTYPES_CTXT_T parseCtxt; 
  int rc = 0;

  parseCtxt.rc = 0;
  parseCtxt.isvid = 1;
  parseCtxt.vidPt = vidPt;
  parseCtxt.audPt = audPt;

  if((rc = strutil_parse_pair(str, (void *)  &parseCtxt, parse_pair_cb_payloadTypes)) < 0) {
    return rc;
  }

  return parseCtxt.rc;
}

int capture_parseLocalAddr(const char *localAddrStr, SOCKET_LIST_T *pSockList) {

  int numPorts = 0;
  struct sockaddr_storage localAddr;
  unsigned short localPorts[SOCKET_LIST_MAX];
  size_t idx;
  int rc = -1;

  if(!localAddrStr || !pSockList) {
    return -1; 
  }

  memset(pSockList, 0, sizeof(SOCKET_LIST_T));
  memset(&localAddr, 0, sizeof(localAddr));

  if((numPorts = parseIpPortStr(localAddrStr, &localAddr, localPorts, SOCKET_LIST_MAX, 1)) > 0) {

    for(idx = 0; idx < sizeof(pSockList->netsockets) / sizeof(pSockList->netsockets[0]); idx++) {

      memset(&pSockList->salist[idx], 0, sizeof(pSockList->salist[idx]));
      NETIOSOCK_FD(pSockList->netsockets[idx]) = INVALID_SOCKET;  

      if(idx < (size_t) numPorts) {
        memcpy(&pSockList->salist[idx], &localAddr, sizeof(pSockList->salist[idx]));
        INET_PORT(pSockList->salist[idx]) = htons(localPorts[idx]);
 
        //if(rtcp) {
        //  memcpy(&pSockList->salistRtcp[idx], &pSockList->salist[idx], sizeof(pSockList->salistRtcp[idx]));
        //  pSockList->salistRtcp[idx].sin_port =
        //                RTCP_PORT_FROM_RTP(htons(pSockList->salistRtcp[idx].sin_port));
        //}
      }
    }
    pSockList->numSockets = numPorts;

    rc = 1;
  } else if(numPorts == 0) {

    // The given localAddrStr string could be a name of a local interface
    rc = 0;
  } 

  return rc;
}

int capture_parseAuthUrl(const char **ppstr, AUTH_CREDENTIALS_STORE_T *pauth) {
  int rc = 0;
  const char *p;
  const char *pendcred = NULL;
  const char *pcolon = NULL;

  if(!ppstr || !(p = *ppstr)) {
    return -1;
  }

  //
  // Check if we have a username or pass
  //
  while(*p != '\0' && *p != '/' && *p != '@') {
    p++;
  }

  if(*p == '@') {
    pendcred = p;
    p = (*ppstr); 
    //
    // Check if we have a username and pass
    //
    while(*p != '\0' && *p != ':' && *p != '@') {
      p++;
    }
    if(*p == ':') {
      pcolon = p;
      p++;
      if(pauth) {
        if(pendcred - p >= sizeof(pauth->pass)) {
          LOG(X_ERROR("Auth password credentials too long in: %s"), *ppstr);
          return -1;
        }
        memcpy(pauth->pass, p, (pendcred - p));
        pauth->username[pendcred - p] = '\0';
      }
    } else {
      pcolon = pendcred;
    }

    if(pauth) {
      if(pcolon - (*ppstr) >= sizeof(pauth->username)) {
        LOG(X_ERROR("Auth username credentials too long in: %s"), *ppstr);
        return -1;
      }
      memcpy(pauth->username, *ppstr, (pcolon - *ppstr));
      pauth->username[pcolon - *ppstr] = '\0';
    }

  }

  if(pendcred) {
    (*ppstr) += (pendcred - *ppstr + 1);
  }

  return rc;
}

CAPTURE_FILTER_TRANSPORT_T capture_parseTransportStr(const char **ppstr) {
  CAPTURE_FILTER_TRANSPORT_T trans = CAPTURE_FILTER_TRANSPORT_UNKNOWN;

  if(ppstr && *ppstr) {
    if(!strncasecmp(*ppstr, "udp://", 6)) {
      trans = CAPTURE_FILTER_TRANSPORT_UDPRAW;
      (*ppstr) += 6;
    } else if(!strncasecmp(*ppstr, "rtp://", 6)) {
      trans = CAPTURE_FILTER_TRANSPORT_UDPRTP;
      (*ppstr) += 6;
    } else if(!strncasecmp(*ppstr, "srtp://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "dtls://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_UDPDTLS;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "dtlssrtp://", 11)) {
      trans = CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP;
      (*ppstr) += 11;
    } else if(!strncasecmp(*ppstr, "dtls-srtp://", 12)) {
      trans = CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP;
      (*ppstr) += 12;
    } else if(!strncasecmp(*ppstr, "tcp://", 6)) {
      trans = CAPTURE_FILTER_TRANSPORT_TCP;
      (*ppstr) += 6;
    } else if(!strncasecmp(*ppstr, "http://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPGET;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "https://", 8)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPSGET;
      (*ppstr) += 8;
    } else if(!strncasecmp(*ppstr, "rtsp://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_RTSP;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "rtsps://", 8)) {
      trans = CAPTURE_FILTER_TRANSPORT_RTSPS;
      (*ppstr) += 8;
    } else if(!strncasecmp(*ppstr, "rtmp://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_RTMP;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "rtmps://", 8)) {
      trans = CAPTURE_FILTER_TRANSPORT_RTMPS;
      (*ppstr) += 8;
    } else if(!strncasecmp(*ppstr, "rtmpt://", 8)) {
      trans = CAPTURE_FILTER_TRANSPORT_RTMPT;
      (*ppstr) += 8;
    } else if(!strncasecmp(*ppstr, "rtmpts://", 9)) {
      trans = CAPTURE_FILTER_TRANSPORT_RTMPTS;
      (*ppstr) += 8;
    } else if(!strncasecmp(*ppstr, "flv://", 6)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPFLV;
      (*ppstr) += 6;
    } else if(!strncasecmp(*ppstr, "flvs://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPSFLV;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "mp4://", 6)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPMP4;
      (*ppstr) += 6;
    } else if(!strncasecmp(*ppstr, "mp4s://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPSMP4;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "dash://", 7)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPDASH;
      (*ppstr) += 7;
    } else if(!strncasecmp(*ppstr, "dashs://", 8)) {
      trans = CAPTURE_FILTER_TRANSPORT_HTTPSDASH;
      (*ppstr) += 8;
    } else if(!strncasecmp(*ppstr, "/dev/", 5)) {
      trans = CAPTURE_FILTER_TRANSPORT_DEV;
    }
  }

  return trans;
}


int capture_getdestFromStr(const char *str,
                           struct sockaddr_storage *psa,
                           const char **ppuri,
                           char *hostbuf,
                           uint16_t dfltPort) {

  char buf[CAPTURE_HTTP_HOSTBUF_MAXLEN];
  unsigned int sz = 0;
  unsigned int idx;
  const char *p;
  const char *p2 = NULL;
  int have_onlyport = 1;
  int rc = 0;

  if(!str) {
    return -1;
  }

  if((p = strstr(str, "://"))) {
    str = p + 3; 
  }

  p = str;

  if((p2 = strstr(p, ":")) || (p2 = strstr(p, "/"))) {
    sz = p2 - p;
  } else {
    sz = strlen(str);
  }

  for(idx = 0; idx < sz; idx++) {
    if(p[idx] < '0' || p[idx] > '9') {
      have_onlyport = 0;
      break;
    }
  }
  if(have_onlyport) {
    sz = 0;
    p2 = p;
  }

  if(hostbuf) {
    hostbuf[0] = '\0';
  }

  if(psa && sz > 0) {

    if(sz >= sizeof(buf)) {
      sz = sizeof(buf) - 1;
    }
    memcpy(buf, p, sz);
    buf[sz] = '\0';

    if(hostbuf) {
      memcpy(hostbuf, buf, sz + 1);
    }

    memset(psa, 0, sizeof(struct sockaddr_in));
    if((rc = net_resolvehost(buf, psa)) != 0) {
      return rc;
    }
/*
    psa->sin_family = AF_INET;
    if((psa->sin_addr.s_addr = net_resolvehost4(buf)) == INADDR_NONE) {
      return -1;
    }
*/

  }

  if(p2 && (*p2 == ':' || have_onlyport)) {

    if(*p2 == ':') {
      p2++;
    }
    p = p2;
    if((p2 = strstr(p, "/"))) {
      sz = p2 - p;
    } else {
      sz = strlen(p);
    }

    if(psa) {
      if(sz >= sizeof(buf)) {
        sz = sizeof(buf) - 1;
      }
      memcpy(buf, p, sz);
      buf[sz] = '\0';

      if((PINET_PORT(psa)= htons(atoi(buf))) == 0) {
        LOG(X_ERROR("Invalid destination port '%s' in '%s'"), buf, str);
        return -1;
      }
    }
  } else {
    INET_PORT(*psa) = htons(dfltPort);
  }

  if(ppuri) {
    *ppuri = p2;
  }

  //LOG(X_DEBUG("GETDESTFROMSTR str: '%s', have_onlyport: %d, hostbuf: '%s', uri: '%s', inet_ntop: '%s', port: %d"), str, have_onlyport, hostbuf, ppuri ? *ppuri : "", psa ? INET_NTOP(*psa, buf, sizeof(buf)) : "", psa ? htons(PINET_PORT(psa)) : -1);
  return 0;
}

static char *log_format_pkt(char *buf, unsigned int sz, const struct sockaddr *pSaSrc, 
                    const struct sockaddr *pSaDst, int in) {
  int rc;
  char tmp[128];

  if(in) {
    if((rc = snprintf(buf, sz, "%s:%d -> :%d", 
               pSaSrc ? FORMAT_NETADDR(*pSaSrc, tmp, sizeof(tmp)) : "", 
               pSaSrc ? htons(PINET_PORT(pSaSrc)) : 0,
               pSaDst ? htons(PINET_PORT(pSaDst)) : 0)) <= 0 && sz > 0) {
      buf[0] = '\0';
    }
  } else {
    if((rc = snprintf(buf, sz, ":%d -> %s:%d", 
               pSaSrc ? htons(PINET_PORT(pSaSrc)) : 0,
               pSaSrc ? FORMAT_NETADDR(*pSaDst, tmp, sizeof(tmp)) : "", 
               pSaDst ? htons(PINET_PORT(pSaDst)) : 0)) <= 0 && sz > 0) {
      buf[0] = '\0';
    }

  }

  return buf;
}

static char *log_format_pkt_sock(char *buf, unsigned int sz, const NETIO_SOCK_T *pnetsock,
                    const struct sockaddr *pSaDst, int in) {
  struct sockaddr_storage saLocal;
  int sztmp;

  memset(&saLocal, 0, sizeof(saLocal));

  if(pnetsock) {
    sztmp = sizeof(saLocal);
    getsockname(PNETIOSOCK_FD(pnetsock), (struct sockaddr *) &saLocal,  (socklen_t *) &sztmp);
  }

  return log_format_pkt(buf, sz, (const struct sockaddr *) &saLocal, pSaDst, in);
}

char *capture_log_format_pkt(char *buf, unsigned int sz, const struct sockaddr *pSaSrc, 
                    const struct sockaddr *pSaDst) {
  return log_format_pkt(buf, sz, pSaSrc, pSaDst, 1);
}

char *stream_log_format_pkt(char *buf, unsigned int sz, const struct sockaddr *pSaSrc, 
                    const struct sockaddr *pSaDst) {
  return log_format_pkt(buf, sz, pSaSrc, pSaDst, 0);
}
char *stream_log_format_pkt_sock(char *buf, unsigned int sz, const NETIO_SOCK_T *pnetsock,
                    const struct sockaddr *pSaDst) {
  return log_format_pkt_sock(buf, sz, pnetsock, pSaDst, 0);
}
