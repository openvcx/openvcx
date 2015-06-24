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


#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <pthread.h>


#include "vsx_common.h"

//static FILE *g_fpdelme;
#define LOGP(x...)  fprintf(stderr, ##x ); 
                    //if(g_fpdelme) fprintf(g_fpdelme, ##x);


#define MAX_PROXY_CONNECTIONS             10

typedef int (* CB_ONRCVDATA) (void *, int, const unsigned char *, unsigned int);

enum PROTO_TYPE {
  PROTO_TYPE_UNKNOWN    = 0,
  PROTO_TYPE_HTTP       = 1,
  PROTO_TYPE_RTSP       = 2,
  PROTO_TYPE_RTMP       = 3
};

struct ADDR_SET {
  int                fd;
  struct sockaddr_in sa;
};

struct CLIENT_INFO {
  enum PROTO_TYPE protoType;
  CB_ONRCVDATA cbOnRcvData;
  char userAgent[128];
  char mediaId[128];
};

struct HDR_STATE {
  int havehdr;
  int seqmatchidx;
  int xmithdr;
  unsigned char bufhdr[1024];
  unsigned int bufhdridx;

  unsigned int txbytes;
};

typedef void *(* CB_INIT_USERDATA) ();
typedef void (* CB_CLOSE_USERDATA) (void *);

struct PROXY_CONNECTION {
  int inUse;
  int id;
  enum PROTO_TYPE cfgProtoType;
  struct ADDR_SET addrRemote;

  CB_INIT_USERDATA    cbInitUserData;
  CB_CLOSE_USERDATA    cbCloseUserData;
  void *pUserData; 

  // everything after this point will be memset for each new request
  struct CLIENT_INFO client;
  struct ADDR_SET addrClient;
  struct HDR_STATE cli_header;
  struct HDR_STATE rem_header;

};


struct PROXY_RTMP_USERDATA {
  CAPTURE_CBDATA_SP_T ctxClient;
  CAPTURE_CBDATA_SP_T ctxRemote;
  char buf[0x7fff];
};

//static int g_fd_toclient;
//static int g_fd_toserver;
static int g_fd_toclient_offset;
static int g_fd_toserver_offset;
//static unsigned char g_handshake_client[3073];

/*
int test_handshake(char *p) {
  if(g_handshake_client[0] != RTMP_HANDSHAKE_HDR) {
    fprintf(stderr, "INVALID CLIENT HANDSHAKE HDR\n");
    return -1;
  }
  p[0] = RTMP_HANDSHAKE_HDR;
  *((uint32_t *) &p[1]) = htonl(96850050); // server uptime
  // server version
  p[5] = 3;
  p[6] = 0;
  p[7] = 1;
  p[8] = 1;

  if(fill_srv_handshake(&p[1], &g_handshake_client[1]) < 0) {
    fprintf(stderr, "FILL_SRV_HANDSHAKE failed\n");
    return -1;
  }

  return 0;
}
*/

static int sendbytes(struct ADDR_SET *pAddrDst, struct PROXY_CONNECTION *pProxy,
              const unsigned char *pBuf, unsigned int len);

static void *proxy_rtmp_init() {
  int rc;
  struct PROXY_RTMP_USERDATA *pCtx; 
  pCtx = (struct PROXY_RTMP_USERDATA *) calloc(1, sizeof(struct PROXY_RTMP_USERDATA));

  if((rc = rtmp_record_init(&pCtx->ctxClient.u.rtmp)) < 0) {
    free(pCtx);
    return NULL;
  }
  if((rc = rtmp_record_init(&pCtx->ctxRemote.u.rtmp)) < 0) {
    rtmp_record_close(&pCtx->ctxClient.u.rtmp); 
    free(pCtx);
    return NULL;
  }

  //g_fd_toclient = open("toclientrtmp_my.raw", O_RDWR | O_CREAT, 0666);
  //g_fd_toserver = open("toserverrtmp_my.raw", O_RDWR | O_CREAT, 0666);
  //g_fd_toclient = open("toclientrtmp_my.raw", O_RDONLY);
  //g_fd_toserver = open("toserverrtmp_my.raw", O_RDONLY);

  return pCtx;
}

static void proxy_rtmp_close(void *pArg) {
  struct PROXY_RTMP_USERDATA *pCtx = (struct PROXY_RTMP_USERDATA *) pArg;
  rtmp_record_close(&pCtx->ctxClient.u.rtmp); 
  rtmp_record_close(&pCtx->ctxRemote.u.rtmp); 
}

static int proxy_rtmp_ondata(struct PROXY_CONNECTION *pProxy, int idxSrc,
                              const unsigned char *pData, unsigned int len) {

  int rc = 0;
  struct ADDR_SET *pAddrWr;
  struct PROXY_RTMP_USERDATA *pCtx;
  COLLECT_STREAM_PKTDATA_T pktdata;

  if(!pProxy || !pProxy->pUserData) {
    return - 1;
  }

  if(idxSrc == 0) {
    pAddrWr = &pProxy->addrRemote;
  } else {
    pAddrWr = &pProxy->addrClient;
  }


  pCtx = (struct PROXY_RTMP_USERDATA *) pProxy->pUserData;
  pktdata.payload.pData = (unsigned char *) pData;
  PKTCAPLEN(pktdata.payload) = len;

  if(idxSrc == 1) {

    rc = len;
    //fprintf(stderr, "reading %d [%d]\n", rc, g_fd_toclient_offset);
    //if(read(g_fd_toclient, pCtx->buf, rc) < 0) return -1;
    //write(g_fd_toclient, pData, len);
    g_fd_toclient_offset += len;

    if(g_fd_toclient_offset <= 3073) {

      //((unsigned char *)pData)[1001] = 0x01;
      //fprintf(stderr, "Handshake from server length: %d\n", len);
      //test_handshake(pCtx->buf);
      //fprintf(stderr, "HANDSHAKE OUT 0x%x\n", pCtx->buf[0]);
      //pData = pCtx->buf;

    } if(g_fd_toclient_offset > 3073) { 
      //pData = pCtx->buf; 
    }

    fprintf(stderr, "S --> --> --> --> --> --> C\n");
    cbOnPkt_rtmp(&pCtx->ctxClient, &pktdata);
  } else {

    if(g_fd_toserver_offset == 0) {
      //if(len != 1537) { fprintf(stderr, "INVALID HANDSHAKE RCV\n"); return -1; }
      //memcpy(g_handshake_client, pData, 1537);
      //avc_dumpHex(stderr, pData, len, 1);
    }

    //if(read(g_fd_toserver, &rc, 4) < 0) return -1;
    //fprintf(stderr, "reading %d [%d]\n", rc, g_fd_toserver_offset);
    //if(read(g_fd_toserver, pCtx->buf, rc) < 0) return -1;
    //write(g_fd_toserver, pData, len);
    g_fd_toserver_offset += len;

    fprintf(stderr, "S <-- <-- <-- <-- <-- <-- C\n");
    //fprintf(stderr, "\n\n\n\n");
    cbOnPkt_rtmp(&pCtx->ctxRemote, &pktdata);

  }

  //avc_dumpHex(stderr, pData, len, 1);

  if((rc = sendbytes(pAddrWr, pProxy, pData, len)) < 0) {
    return -1;
  }

  return 0;
}


static const char DESCR_CLIENT[] = "client";
static const char DESCR_REMOTE[] = "remote";


static int setsocketio(int fd, int nonblock) {
  int val;

  if((val = fcntl(fd, F_GETFL)) < 0) {
    perror("fcntl F_GETFL");
    return -1;
  }

  if(nonblock && (val & O_NONBLOCK) == 0) {
    val |= O_NONBLOCK;
  } else if(!nonblock && (val & O_NONBLOCK)) {
    val &= ~O_NONBLOCK;
  } else {
    return 0;
  }

  if(fcntl(fd, F_SETFL, val) < 0) {
    perror("fcntl F_SETFL");
    return -1;
  }

  return 0;
}

static int connect_remote(struct PROXY_CONNECTION *pProxy) {

  if((pProxy->addrRemote.fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }

  LOGP("[%u] connecting to %s %s:%d\n", pProxy->id, DESCR_REMOTE,
  inet_ntoa(pProxy->addrRemote.sa.sin_addr), htons(pProxy->addrRemote.sa.sin_port));

  if(connect(pProxy->addrRemote.fd, (const struct sockaddr *) &pProxy->addrRemote.sa,
             sizeof(pProxy->addrRemote.sa)) < 0) {
    LOGP("[%u] failed to connect to %s %s:%d\n", pProxy->id, DESCR_REMOTE,
       inet_ntoa(pProxy->addrRemote.sa.sin_addr), htons(pProxy->addrRemote.sa.sin_port));
    perror("");
    close(pProxy->addrRemote.fd);
    pProxy->addrRemote.fd = 0;
    return -1;
  }

  if(setsocketio(pProxy->addrRemote.fd, 1) < 0) {
    close(pProxy->addrRemote.fd);
    pProxy->addrRemote.fd = 0;
    return -1;
  }

  LOGP("[%u] connected to %s %s:%d\n", pProxy->id, DESCR_REMOTE,
    inet_ntoa(pProxy->addrRemote.sa.sin_addr), htons(pProxy->addrRemote.sa.sin_port));

  return 0;
}

static int findHdrEnd(struct HDR_STATE *pHdr,
                      const unsigned char *pData, unsigned int len) {
  unsigned int idx;
  static unsigned char seqhdrend[] = "\r\n\r\n";

  for(idx = 0; idx < len; idx++) {

    if(pData[idx] == seqhdrend[pHdr->seqmatchidx]) {
      pHdr->seqmatchidx++;
    } else if(pHdr->seqmatchidx > 0) {
      pHdr->seqmatchidx = 0;
    }

    pHdr->bufhdr[pHdr->bufhdridx++] = pData[idx]; 

    if(pHdr->seqmatchidx >= 4) {
      return idx + 1;
    }

    if(pHdr->bufhdridx >= sizeof(pHdr->bufhdr)) {
      LOGP("No header end found after %d bytes\n", pHdr->bufhdridx);
      return -1;
    }

  }

  return 0;
}

static int onRcvHeaderHttp(struct HDR_STATE *pHdr,
                         unsigned char **ppData, unsigned int *plen) {

  int rc = 0;


  if((rc = findHdrEnd(pHdr, *ppData, *plen)) < 0) {
    return rc;
  } 

  if(rc == 0) {
    rc = *plen;
  } else {
    pHdr->havehdr = 1;
  }

  (*ppData) += rc;
  (*plen) -= rc;

   LOGP("header:%s\n", pHdr->bufhdr);

  return rc;
}

static int parseClientHttpUrl(struct CLIENT_INFO *pClient,
                              const char *pData, unsigned int len) {
  int rc = 0;
  char url[1024];
  unsigned int idx = 0;
  unsigned int idx2;

  // todo: use xxx_istextchar()
  while(idx < len && (pData[idx] >= 'A' && pData[idx] <= 'Z')) {
    idx++;
  }
  while(idx < len && pData[idx] == ' ') {
    idx++;
  }

  idx2 = idx;

  while(idx2 < len && pData[idx2] != ' ' && 
        pData[idx2] != '\r' && pData[idx2] != '\n') {
    idx2++;
  }

  idx2 = MIN(idx2 - idx, sizeof(url) - 1);
  memcpy(url, &pData[idx], idx2);
  url[idx2] = '\0';

  fprintf(stderr, "URL IS :'%s'\n", url);

  return rc;
}

static int parseClientHttpHdr(struct PROXY_CONNECTION *pProxy) {
  int rc = 0;
  unsigned int idx = 0, idx2, idx3, idx4;
  const char *pData = (const char *) pProxy->cli_header.bufhdr;
  unsigned int len = pProxy->cli_header.bufhdridx;

  //
  // Process the entire HTTP header
  //

  while(idx < len) {

    idx2 = idx;

    //
    // skip until ':' delimiting header key/value, or end of line
    //
    while(idx2 < len && pData[idx2] != ':' && pData[idx2]  != '\r' && 
         pData[idx2] != '\n') {
      idx2++;
    }
        
    idx3 = idx2;

    while(idx3 < len && pData[idx3] == ':') {
      idx3++;
    }

    //
    // idx3 is set to the header value start 
    //
    idx4 = idx3;

    while(idx4 < len && pData[idx4] != '\r' && pData[idx4] != '\n') {
      idx4++;
    }

    //if(idx == 0 && !strncasecmp(&pData[idx], "GET", 3)) {
    if(idx == 0) {
      parseClientHttpUrl(&pProxy->client, &pData[idx], idx4 - idx);
    } else if(!strncasecmp(&pData[idx], "User-Agent", 10)) {
      memcpy(pProxy->client.userAgent, &pData[idx3], 
            MIN(idx4 - idx3, sizeof(pProxy->client.userAgent) - 1));
    } 
    
    while(idx4 < len && (pData[idx4] == '\r' || pData[idx4] == '\n')) {
      idx4++;
    }

    idx = idx4;
  }

  return rc;
}

static int sendbytes(struct ADDR_SET *pAddrDst, struct PROXY_CONNECTION *pProxy,
              const unsigned char *pBuf, unsigned int len) {
  int rc;
  unsigned int idx = 0;
  struct HDR_STATE *pHdrXmit;
  const char *descr_wr;

  if(pAddrDst == &pProxy->addrClient) {
    pHdrXmit = &pProxy->rem_header;
    descr_wr = DESCR_CLIENT;
  } else {
    pHdrXmit = &pProxy->cli_header;
    descr_wr = DESCR_REMOTE;
  }

  setsocketio(pAddrDst->fd, 0);

  do {
    while((rc = send(pAddrDst->fd, &pBuf[idx], len - idx, 0)) < 0  && errno == EAGAIN) {
      //fprintf(stderr, "sleep\n");
      usleep(1000);
    }
    if(rc < 0) {
      break;
    }
    idx += len;
  } while(idx < len);

  if(idx != len) {
    LOGP("[%u] failed to send %d (%u) bytes to %s %s:%d (rc:%d errno:%d)\n", 
            pProxy->id, len, pHdrXmit->txbytes, descr_wr, 
            inet_ntoa(pAddrDst->sa.sin_addr), htons(pAddrDst->sa.sin_port), rc, errno);
    return -1;
  }
  pHdrXmit->txbytes += len;
  //LOGP("[%u] sent %d (%u) bytes to %s %s:%d\n",
  //        pProxy->id, len,pHdrXmit->txbytes,  descr_wr, 
  //        inet_ntoa(pAddrDst->sa.sin_addr), htons(pAddrDst->sa.sin_port));

  setsocketio(pAddrDst->fd, 1);

  return rc;
}

static int cbOnRcvDataRtsp(void *pArg, int idxSrc, 
                           const unsigned char *pData, unsigned int len) {

  LOGP("rtsp not implemented");

  return -1;
}


static int cbOnRcvDataRtmp(void *pArg, int idxSrc, 
                           const unsigned char *pData, unsigned int len) {

  struct PROXY_CONNECTION *pProxy = (struct PROXY_CONNECTION *) pArg;
  struct ADDR_SET *pAddrWr;
  int rc = 0;

  if(idxSrc == 0) {
    pAddrWr = &pProxy->addrRemote;
  } else {
    pAddrWr = &pProxy->addrClient;
  }

  if(idxSrc == 0 && pProxy->addrRemote.fd <= 0) {

    if(pProxy->addrRemote.sa.sin_addr.s_addr == INADDR_NONE ||
       htons(pProxy->addrRemote.sa.sin_port) <= 0) {
      LOGP("Remote destination not configured\n");
      return -1;
    }

    if((rc = connect_remote(pProxy)) < 0) {
      return -1;
    }
  }

  //if((rc = sendbytes(pAddrWr, pProxy, pData, len)) < 0) {
  //  return -1;
  //}

  if(proxy_rtmp_ondata(pProxy, idxSrc, pData, len) < 0) {
    return -1;
  }

  return rc;
}

static int cbOnRcvDataHttp(void *pArg, int idxSrc, 
                           const unsigned char *pDataArg, unsigned int len) {

  struct PROXY_CONNECTION *pProxy = (struct PROXY_CONNECTION *) pArg;
  unsigned char *pData = (unsigned char *) pDataArg;
  int rc;
  struct HDR_STATE *pHdrRd;
  const char *descrRd;
  struct ADDR_SET *pAddrRd; 
  struct ADDR_SET *pAddrWr;

  if(idxSrc == 0) {
    pAddrRd = &pProxy->addrClient;
    pAddrWr  = &pProxy->addrRemote;
    pHdrRd = &pProxy->cli_header;
    descrRd = DESCR_CLIENT;
  } else {
    pAddrRd = &pProxy->addrRemote;
    pAddrWr = &pProxy->addrClient;
    pHdrRd = &pProxy->rem_header;
    descrRd = DESCR_REMOTE;
  }

  LOGP("[%u] read %d bytes from %s %s:%d\n", pProxy->id, len, descrRd,
         inet_ntoa(pAddrRd->sa.sin_addr), htons(pAddrRd->sa.sin_port));


  if(!pHdrRd->havehdr) {

    if((rc = onRcvHeaderHttp(pHdrRd, &pData, &len)) < 0) {
      return -1;
    }

    if(pHdrRd->havehdr) {
      pHdrRd->xmithdr = 1;
      parseClientHttpHdr(pProxy);
    }

    if(pHdrRd->havehdr && idxSrc == 0 && pProxy->addrRemote.fd <= 0) {

      if(pProxy->addrRemote.sa.sin_addr.s_addr == INADDR_NONE ||
         htons(pProxy->addrRemote.sa.sin_port) <= 0) {
        LOGP("Remote destination not configured\n");
        return -1;
      }

      if((rc = connect_remote(pProxy)) < 0) {
        return -1;
      }
    }
  }


  if(pAddrWr != NULL && pAddrWr->fd > 0 && pHdrRd->havehdr) {

    if(pHdrRd->xmithdr && pHdrRd->bufhdridx > 0) {

      if(sendbytes(pAddrWr, pProxy, pHdrRd->bufhdr, pHdrRd->bufhdridx) < 0) {
        return -1;
      }

      pHdrRd->xmithdr = 0;
      pHdrRd->bufhdridx = 0;
    }

    if(len > 0 && sendbytes(pAddrWr, pProxy, pData, len) < 0) {
      return -1;
    }
  }

  return len;        
}

static int setCbRcvData(struct PROXY_CONNECTION *pProxy,
                        const unsigned char *pData, unsigned int len) {
  
  if((len >= 7 && !strncasecmp((char *) pData, "OPTIONS", 7)) ||
     (len >= 8 && !strncasecmp((char *) pData, "DESCRIBE", 8))) {

    //pProxy->client.cbOnRcvData = cbOnRcvDataRtsp;
    //pProxy->client.protoType = PROTO_TYPE_RTSP;
    pProxy->client.cbOnRcvData = cbOnRcvDataHttp;
    pProxy->client.protoType = PROTO_TYPE_HTTP;

  } else if((len >= 3 && !strncasecmp((char *) pData, "GET", 3)) || 
            (len >= 4 && !strncasecmp((char *) pData, "POST", 4)) || 
            (len >= 4 && !strncasecmp((char *) pData, "HEAD", 4))) {

    pProxy->client.cbOnRcvData = cbOnRcvDataHttp;
    pProxy->client.protoType = PROTO_TYPE_HTTP;
  } else {
    pProxy->client.cbOnRcvData = cbOnRcvDataHttp;
    pProxy->client.protoType = PROTO_TYPE_HTTP;

  }

  if(pProxy->client.cbOnRcvData != NULL) {
    return 0;
  }

  return -1;
}


static int proxyloop(struct PROXY_CONNECTION *pProxy) {

  int idx;
  int ircv;
  int val;
  fd_set rdset;
  fd_set wrset;
  struct timeval tv;
  unsigned char buf[16384];
  struct ADDR_SET *pAddrSet[2];

  pAddrSet[0] = &pProxy->addrClient;
  pAddrSet[1] = &pProxy->addrRemote;

  if(setsocketio(pAddrSet[0]->fd, 1) < 0) {
    return -1;
  }

  while(1) {

    FD_ZERO(&rdset);
    FD_SET(pAddrSet[0]->fd, &rdset);
    if(pAddrSet[1]->fd > 0) {
      FD_SET(pAddrSet[1]->fd, &rdset);
    }

    FD_ZERO(&wrset);
    FD_SET(pAddrSet[0]->fd, &wrset);
    if(pAddrSet[1]->fd > 0) {
      FD_SET(pAddrSet[1]->fd, &wrset);
    }

    if(pAddrSet[1]->fd > 0 && pAddrSet[1]->fd > pAddrSet[0]->fd) {
      idx = pAddrSet[1]->fd + 1;
    } else {
      idx = pAddrSet[0]->fd + 1;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    if((val = select(idx, &rdset, NULL, NULL, &tv)) > 0) {

      //fprintf(stderr, "[%u] select returned: %d\n", pProxy->id, val);
      for(idx = 0; idx < 2; idx++) {
        if(FD_ISSET(pAddrSet[idx]->fd, &rdset)) {

          //fprintf(stderr, "%d is set\n", pAddrSet[idx]->fd); 
          if((ircv = recv(pAddrSet[idx]->fd, buf, sizeof(buf), 0)) <= 0) {
            LOGP("[%u] failed to read from %s %s:%d\n", pProxy->id, 
              (idx == 0 ? DESCR_CLIENT : DESCR_REMOTE), 
              inet_ntoa(pAddrSet[idx]->sa.sin_addr), htons(pAddrSet[idx]->sa.sin_port));
            return -1;
          }

          if(pProxy->client.cbOnRcvData == NULL) {
            setCbRcvData(pProxy, buf, ircv);
          }

          if(pProxy->client.cbOnRcvData == NULL) {
            return -1;
          }

          if(pProxy->client.cbOnRcvData(pProxy, idx, buf, ircv) < 0) {
            return -1;
          }

        }
      }

    } else if(val == 0) {
      //fprintf(stderr, "[%u] select timeout returned: %d\n", pProxy->id, val);
    } else {
      LOGP("[%u] select returned: %d\n", pProxy->id, val);
      break;
    }

  }

  return 0;
}

static void closeProxyConn(struct PROXY_CONNECTION *pProxy) {

  LOGP("[%u] connection from %s:%d ended\n", pProxy->id,
    inet_ntoa(pProxy->addrClient.sa.sin_addr), htons(pProxy->addrClient.sa.sin_port));

  if(pProxy->addrClient.fd > 0) {
    close(pProxy->addrClient.fd);
    pProxy->addrClient.fd = -1;
  }
  if(pProxy->addrRemote.fd > 0) {
    close(pProxy->addrRemote.fd);
    pProxy->addrRemote.fd = -1;
  }

  if(pProxy->cbCloseUserData) {
    pProxy->cbCloseUserData(pProxy->pUserData);
  }

}



static void *proxyloop_proc(void *pArg) {
  struct PROXY_CONNECTION *pProxy = (struct PROXY_CONNECTION *) pArg;

  pProxy->id = pthread_self();

  proxyloop(pProxy);

  closeProxyConn(pProxy);
  pProxy->inUse = 0;

  return NULL;
}

static struct PROXY_CONNECTION *getAvailableProxy(struct PROXY_CONNECTION *pProxies,
                                                  unsigned int numProxies) {
  unsigned int idx;
 
  for(idx = 0; idx < numProxies; idx++) {
    if(!pProxies[idx].inUse) {

      pProxies[idx].inUse = 1;

      // partial memset here
      memset(&pProxies[idx].client, 0, sizeof(pProxies[0]) - 
             ((ptrdiff_t) &pProxies[idx].client - (ptrdiff_t) &pProxies[idx]));
      pProxies[idx].addrRemote.fd = 0;

      switch(pProxies[idx].cfgProtoType) {
        case PROTO_TYPE_RTMP:
          pProxies[idx].client.protoType = pProxies[idx].cfgProtoType;
          pProxies[idx].client.cbOnRcvData = cbOnRcvDataRtmp;
          break;
        case PROTO_TYPE_RTSP:
          pProxies[idx].client.protoType = pProxies[idx].cfgProtoType;
          pProxies[idx].client.cbOnRcvData = cbOnRcvDataRtsp;
          break;
        case PROTO_TYPE_HTTP:
          pProxies[idx].client.protoType = pProxies[idx].cfgProtoType;
          pProxies[idx].client.cbOnRcvData = cbOnRcvDataHttp;
          break;
        default:
          pProxies[idx].client.protoType = PROTO_TYPE_UNKNOWN;
          pProxies[idx].client.cbOnRcvData = NULL;
          break;
      }

      if(pProxies[idx].cbInitUserData) {
        pProxies[idx].pUserData = pProxies[idx].cbInitUserData();
      }

      return &pProxies[idx];
    }
  }

  return NULL;
}

static int proxy_start_loop(struct PROXY_CONNECTION *pProxies, 
                            unsigned int numProxies,
                            struct ADDR_SET *pAddrSrv) {

  struct ADDR_SET addrClient;
  int rc = 0;
  socklen_t slen;
  int i;
  struct PROXY_CONNECTION *pProxy;
  pthread_t ptd;
  pthread_attr_t ptdAttr;

  if(pAddrSrv == NULL || pProxies == NULL || 
     pAddrSrv->sa.sin_addr.s_addr == INADDR_NONE || pAddrSrv->sa.sin_port == 0) {
    return -1;
  }

  if((pAddrSrv->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
  
  i = 1;
  if(setsockopt(pAddrSrv->fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0) {
    LOGP("setsockopt SO_REUSEADDR failed\n");
  }

  LOGP("listening on %s:%d\n", inet_ntoa(pAddrSrv->sa.sin_addr), 
                                          htons(pAddrSrv->sa.sin_port));

  if(bind(pAddrSrv->fd, (struct sockaddr *) &pAddrSrv->sa, sizeof(pAddrSrv->sa)) < 0) {
    perror("bind failed");
    close(pAddrSrv->fd);
    return -1;
  }

  if(listen(pAddrSrv->fd, 8) < 0) {
    perror("listen failed");
    close(pAddrSrv->fd);
    return -1;
  } 

  do {

    slen = sizeof(addrClient.sa);
    if((addrClient.fd = accept(pAddrSrv->fd, 
             (struct sockaddr *) &addrClient.sa, &slen)) < 0) {
      perror("listen failed");
      close(pAddrSrv->fd);
      return -1;
    }
    LOGP("Received connection from %s:%d\n", 
         inet_ntoa(addrClient.sa.sin_addr), htons(addrClient.sa.sin_port));

    if((pProxy = getAvailableProxy(pProxies, numProxies)) != NULL) {

      memcpy(&pProxy->addrClient, &addrClient, sizeof(pProxy->addrClient));
      pthread_attr_init(&ptdAttr);
      if(pthread_create(&ptd, &ptdAttr, proxyloop_proc, pProxy) < 0) {
        LOGP("Failed to create proxy connection thread\n");
        closeProxyConn(pProxy);
        pProxy->inUse = 0;
      }
      pthread_attr_destroy(&ptdAttr);

    } else {
      LOGP("No connection resource available for %s:%d max: %d\n", 
        inet_ntoa(pProxy->addrClient.sa.sin_addr), 
        htons(pProxy->addrClient.sa.sin_port), numProxies);
      close(addrClient.fd);
    }

  } while(1);

  close(pAddrSrv->fd);

  return rc;
}


int proxy_start(struct PROXY_CONNECTION *pProxies, unsigned int numProxies,
                struct ADDR_SET *pAddrSrv, struct ADDR_SET *pAddrRemote,
                enum PROTO_TYPE protoType) {

  unsigned int idx;

  //g_fpdelme = fopen("test.dump", "w");

  for(idx = 0; idx < numProxies; idx++) {

    memset(&pProxies[idx].addrRemote.sa, 0, sizeof(pProxies[idx].addrRemote.sa));
    pProxies[idx].addrRemote.sa.sin_family = AF_INET;
    pProxies[idx].addrRemote.sa.sin_addr.s_addr = pAddrRemote->sa.sin_addr.s_addr;
    pProxies[idx].addrRemote.sa.sin_port = pAddrRemote->sa.sin_port;
    pProxies[idx].cfgProtoType = protoType;

    pProxies[idx].cbInitUserData = proxy_rtmp_init;
    pProxies[idx].cbCloseUserData = proxy_rtmp_close;
  }

  return proxy_start_loop(pProxies, numProxies, pAddrSrv);
}

int proxy_test(const char *listenHost, uint16_t listenPort,
               const char *remoteHost, uint16_t remotePort,
               const char *protoStr) {

  int rc = 0;
  struct ADDR_SET addrRemote;
  struct ADDR_SET addrLocal;
  enum PROTO_TYPE protoType = PROTO_TYPE_UNKNOWN;
  struct PROXY_CONNECTION *proxies = NULL;
  unsigned int proxiesCount = MAX_PROXY_CONNECTIONS;

  addrLocal.sa.sin_addr.s_addr = inet_addr(listenHost);
  addrLocal.sa.sin_port = htons(listenPort);

  addrRemote.sa.sin_addr.s_addr = inet_addr(remoteHost);
  addrRemote.sa.sin_port = htons(remotePort);


  if(protoStr) {
    if(!strncasecmp(protoStr, "rtmp", 4)) {
      protoType = PROTO_TYPE_RTMP; 
    } else {
      LOGP("Invalid protocol argument\n");
      return -1;
    }
  }

  proxies = (struct PROXY_CONNECTION *) calloc(proxiesCount, 
                                               sizeof(struct PROXY_CONNECTION));

  rc = proxy_start(proxies, proxiesCount, &addrLocal, &addrRemote, protoType);

  free(proxies);

  return rc;
}

#if 1

static struct PROXY_CONNECTION *g_proxies;
static unsigned int g_proxiesCount;
static struct ADDR_SET g_addrSrv;

void sighandler(int signum) {

  unsigned int idx;

  for(idx = 0; idx < g_proxiesCount; idx++) {

    if(g_proxies[idx].addrClient.fd > 0) {
      close(g_proxies[idx].addrClient.fd);
      g_proxies[idx].addrClient.fd = -1;
    }
    if(g_proxies[idx].addrRemote.fd > 0) {
      close(g_proxies[idx].addrRemote.fd);
      g_proxies[idx].addrRemote.fd = -1;
    }

  }

  if(g_addrSrv.fd > 0) {
    close(g_addrSrv.fd);
    g_addrSrv.fd = -1;
  }

  _exit(0);

}

int usage(int argc, const char *argv[] ) {

  fprintf(stdout, "Usage: %s <local ip> <local port> <dest ip> <dest port>\n", argv[0]);
  return 0;
}


int main(int argc, const char *argv[]) {

  struct ADDR_SET addrRemote;
  enum PROTO_TYPE protoType = PROTO_TYPE_UNKNOWN;

  signal(SIGINT, sighandler);
  signal(SIGQUIT, sighandler);

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, LOG_OUTPUT_DEFAULT_STDERR);



  if(argc < 5) {
    return usage(argc, argv);
  }

  g_addrSrv.sa.sin_addr.s_addr = inet_addr(argv[1]);
  g_addrSrv.sa.sin_port = htons(atoi(argv[2]));

  addrRemote.sa.sin_addr.s_addr = inet_addr(argv[3]);
  addrRemote.sa.sin_port = htons(atoi(argv[4]));


  if(argc >= 6) {
    if(!strncasecmp(argv[5], "rtmp", 4)) {
      protoType = PROTO_TYPE_RTMP; 
    } else if(!strncasecmp(argv[5], "http", 4)) {
      protoType = PROTO_TYPE_HTTP; 
    } else if(!strncasecmp(argv[5], "rtsp", 4)) {
      protoType = PROTO_TYPE_RTSP; 
    } else {
      fprintf(stderr, "Invalid protocol argument\n");
      return -1;
    }
  } 

  g_proxiesCount = MAX_PROXY_CONNECTIONS;
  g_proxies = (struct PROXY_CONNECTION *) calloc(g_proxiesCount, 
                                                 sizeof(struct PROXY_CONNECTION));

  proxy_start(g_proxies, g_proxiesCount, &g_addrSrv, &addrRemote, protoType);

  free(g_proxies);

  return 0;
}
#endif // 0
