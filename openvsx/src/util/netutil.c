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

static int add_multicast_group(SOCKET sock, struct in_addr *pinaddr) {

#if defined(WIN32)

  LOG(X_ERROR("Multicast not implemented on this system type"));
  return -1;

#else

  struct ip_mreq mcip;

  memset(&mcip, 0, sizeof(mcip));

  mcip.imr_multiaddr.s_addr = pinaddr->s_addr;
  mcip.imr_interface.s_addr = INADDR_ANY;

  if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mcip, 
     sizeof(mcip)) < 0) {

    LOG(X_ERROR("Unable to add multicast membership for %s"), inet_ntoa(*pinaddr));
    return -1;
  }

  LOG(X_DEBUG("Subscribed to multicast membership for %s"), inet_ntoa(*pinaddr));

  return 0;

#endif // WIN32

}

SOCKET net_opensocket(int socktype, unsigned int rcvbufsz, int sndbufsz,
                      struct sockaddr_in *psain) {
  int val;
  int sockbufsz = 0;
  SOCKET sock = INVALID_SOCKET;

  if(socktype != SOCK_STREAM && socktype != SOCK_DGRAM) {
    return INVALID_SOCKET;
  }

  if((sock = socket(PF_INET, socktype, 0)) == INVALID_SOCKET) {
    LOG(X_ERROR("Unable to create socket type %d"), socktype);
    return INVALID_SOCKET;
  }

  if(psain) {

    //
    // Handle IGMP multicast group subscription
    //
    if(socktype == SOCK_DGRAM && IN_MULTICAST(htonl(psain->sin_addr.s_addr))) {
      if(add_multicast_group(sock, &psain->sin_addr)  < 0) {
        closesocket(sock);
        return INVALID_SOCKET;
      }
    }


    val = 1;
    if(setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &val, sizeof (val)) != 0) {
      LOG(X_WARNING("setsockopt SOL_SOCKET SO_REUSEADDR fail "ERRNO_FMT_STR), 
              ERRNO_FMT_ARGS);
    }

#if defined(__APPLE__)
    if(socktype == SOCK_DGRAM) {
      val = 1;
      if(setsockopt (sock, SOL_SOCKET, SO_REUSEPORT,
                      (char *) &val, sizeof (val)) != 0) {
        LOG(X_WARNING("setsockopt SOL_SOCKET SO_REUSEPORT fail "ERRNO_FMT_STR), 
                ERRNO_FMT_ARGS);
      }
    }
#endif // __APPLE__


    //psain->sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr *) psain,
                          sizeof(struct sockaddr_in)) < 0) {
      LOG(X_ERROR("Unable to bind to local port %s:%d "ERRNO_FMT_STR),
                       inet_ntoa(psain->sin_addr), ntohs(psain->sin_port), ERRNO_FMT_ARGS);
      closesocket(sock);
      return INVALID_SOCKET;
    }

    //LOG(X_DEBUG("Bound listener to %s:%d"), inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));

  }

  if(rcvbufsz > 0) {

    if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
                  (char*) &rcvbufsz, sizeof(rcvbufsz)) != 0) {
      LOG(X_WARNING("Unable to setsockopt SOL_SOCKET SO_RCVBUF %u"), rcvbufsz);
    }

    sockbufsz = 0;
    val = sizeof(sockbufsz);
    if(getsockopt(sock, SOL_SOCKET, SO_RCVBUF,
                 (char*) &sockbufsz, (socklen_t *) &val) != 0) {
      LOG(X_ERROR("Unable to getsockopt SOL_SOCKET SO_RCVBUF %d"), val);
      closesocket(sock);
      return INVALID_SOCKET;
    }

    LOG(X_DEBUGV("socket receive buffer set to: %u"), sockbufsz);
  }

  if(sndbufsz > 0) {

    if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
                  (char*) &sndbufsz, sizeof(rcvbufsz)) != 0) {
      LOG(X_WARNING("Unable to setsockopt SOL_SOCKET SO_SNDBUF %u"), sndbufsz);
    }

    sockbufsz = 0;
    val = sizeof(sockbufsz);
    if(getsockopt(sock, SOL_SOCKET, SO_SNDBUF,
                 (char*) &sockbufsz, (socklen_t *) &val) != 0) {
      LOG(X_ERROR("Unable to getsockopt SOL_SOCKET SO_SNDBUF %d"), val);
      closesocket(sock);
      return INVALID_SOCKET;
    }

    LOG(X_DEBUGV("socket send buffer set to: %u"), sockbufsz);
  }

  return sock;
}

#if 0
int net_bindlistener(SOCKET sock, struct sockaddr_in *psain)  {
  int rc;
  socklen_t sz;
  struct sockaddr_in satmp;

  if(sock == INVALID_SOCKET || !psain) {
    return -1;
  }

  if(psain->sin_port == 0) {
    memset(psain, 0, sizeof(psain));
    sz = sizeof(struct sockaddr_in);
    if((rc = getsockname(sock, (struct sockaddr *) psain, &sz)) < 0 ||
      psain->sin_port == 0) {
    
/*
      //
      // The socket may not have been used in a send operation
      //
      memset(&satmp, 0, sizeof(satmp));  
      satmp.sin_family = AF_INET;
      satmp.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
      satmp.sin_port = htons(random() % 50000 + 10000);
      rc = sendto(sock, (void *) "\0", 1, 0, (struct sockaddr *) &satmp, sizeof(satmp));
      fprintf(stderr, "send 1 to rc:%d %s:%d\n", rc, inet_ntoa(satmp.sin_addr), ntohs(satmp.sin_port));
*/

      memset(psain, 0, sizeof(psain));
      sz = sizeof(struct sockaddr_in);
      psain->sin_family = AF_INET;
      psain->sin_addr.s_addr = INADDR_ANY; 
      psain->sin_port = htons(random() % 50000 + 10000);
      if((rc = getsockname(sock, (struct sockaddr *) psain, &sz)) < 0 ||
        psain->sin_port == 0) {
        return -1;
      }
      fprintf(stderr, "now its cool rc:%d %s:%d\n", rc, inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
      

    }
  }

  psain->sin_family = AF_INET;
  if(bind(sock, (struct sockaddr *) psain, sizeof(struct sockaddr_in)) < 0) {
    LOG(X_ERROR("Unable to bind to %s:%d"),
                     inet_ntoa(psain->sin_addr), ntohs(psain->sin_port));
    return -1;
  }

  return 0;
}
#endif // 0


int net_recvnb(SOCKET sock, unsigned char *buf, unsigned int len, 
                      unsigned int mstmt) {
  struct timeval tv;
  fd_set fdsetRd;
  int rc;
  int sockflags = 0;

#if !defined(__APPLE__) && !defined(WIN32)
  sockflags = MSG_NOSIGNAL;
#endif // __APPLE__

  if(sock == INVALID_SOCKET) {
    return -1;
  }

  tv.tv_sec = (mstmt / 1000);
  tv.tv_usec = (mstmt % 1000) * 1000;

  FD_ZERO(&fdsetRd);
  FD_SET(sock, &fdsetRd);

  if((rc = select(sock + 1, &fdsetRd, NULL, NULL, &tv)) > 0) {

    if((rc = recv(sock, (void *) buf, len, sockflags)) < 0) {
      LOG(X_ERROR("recv nonblock %d returned %d "ERRNO_FMT_STR), 
          len, rc, ERRNO_FMT_ARGS);
    } else if(rc == 0) {
      LOG(X_WARNING("recv nonblock %d - connection has been closed."), len);
      rc = -1;
    }

  } else if(rc == 0) {
    //fprintf(stderr, "TMT EXPIRED sock:%d tv:%d,%d, mstmt:%d\n", sock, tv.tv_sec, tv.tv_usec, mstmt);
    // timeout expired
  } else {
    LOG(X_ERROR("select socket:%d returned: %d"), sock, rc);
  }

  return rc;
}

int net_connect(SOCKET sock, struct sockaddr_in *psa) {

  if(sock == INVALID_SOCKET || !psa) {
    return -1;
  }

  psa->sin_family = PF_INET;

  if(connect(sock, (struct sockaddr *) psa, (socklen_t) sizeof(struct sockaddr_in)) < 0) {

    LOG(X_ERROR("Failed to connect to %s:%d "ERRNO_FMT_STR),
            inet_ntoa(psa->sin_addr), ntohs(psa->sin_port), ERRNO_FMT_ARGS);
    return -1;
  }

  //LOG(X_DEBUG("Connected to %s:%d"), inet_ntoa(psa->sin_addr), ntohs(psa->sin_port));

  return 0;
}


int net_recvnb_exact(SOCKET sock, unsigned char *buf, unsigned int len, 
                            unsigned int mstmt) {
  unsigned int idx = 0;
  int rc;
  unsigned int ms;
  struct timeval tv0, tv1;
  
  gettimeofday(&tv0, NULL);
 
  do {

    if((rc = net_recvnb(sock, &buf[idx], len - idx, MIN(mstmt, 1000))) < 0) {
      return rc;
    } else if(rc > 0) {
      idx += rc;
    }

    if(mstmt > 0 && idx < len) {
      gettimeofday(&tv1, NULL);
      if((ms = TIME_TV_DIFF_MS(tv1, tv0)) > mstmt) {
        LOG(X_DEBUG("net_recvnb_exact aborting after %d/%dms, %u/%u bytes"), ms, mstmt, idx, len);
        return idx;
      }
    }

  } while(idx < len && !g_proc_exit);

  return idx;
}

int net_recv(SOCKET sock, const struct sockaddr_in *psa, 
                    unsigned char *buf, unsigned int len) {
  int rc = 0;
  int sockflags = 0;

#if !defined(__APPLE__) && !defined(WIN32)
  sockflags = MSG_NOSIGNAL;
#endif // __APPLE__

  while((rc = recv(sock, buf, len, sockflags)) < 0) {

#if defined(WIN32) && !defined(__MINGW32__)
    if(WSAGetLastError() == WSAEWOULDBLOCK) {
#else // WIN32
    if(errno == EAGAIN) {
#endif // WIN32
      return 0;
    } else {

      LOG(X_ERROR("recv failed (%d) for %u bytes from %s:%d "ERRNO_FMT_STR), rc, len, 
           psa ? inet_ntoa(psa->sin_addr) : "", psa ? ntohs(psa->sin_port) : 0, ERRNO_FMT_ARGS);
      return -1;
    }
  } 

  // client disconnected rc = 0

  return rc;
}

int net_recv_exact(SOCKET sock, const struct sockaddr_in *psa, 
                          unsigned char *buf, unsigned int len) {
  unsigned int idx = 0;
  int rc;
 
  do {

    if((rc = net_recv(sock, psa, &buf[idx], len - idx)) < 0) {
      return rc;
    } else if(rc > 0) {
      idx += rc;
    }

  } while(idx < len);

  return idx;
}


int net_send(SOCKET sock, const struct sockaddr_in *psa, 
                     const unsigned char *buf, unsigned int len) {
  int rc;
  int sockflags = 0;
  unsigned int idx = 0;

  if(len == 0) {
    return 0; 
  }

#if !defined(__APPLE__) && !defined(WIN32)
  sockflags = MSG_NOSIGNAL;
#endif // __APPLE__

  while(idx < len) {

    if((rc = send(sock, &buf[idx], len - idx, sockflags)) < 0) {
#if defined(__APPLE__) || defined(__linux__)
      //
      // send may return EAGAIN on a non-blocking socket when the local
      // xmit socket buffer becomes full
      //
      if(errno == EAGAIN) {
        // the proper thing to do here would be to call select 
        // on the write set
        usleep(1000);
        continue;
      }
#endif // __linux__
      LOG(X_ERROR("Failed to send %u bytes to %s:%d "ERRNO_FMT_STR), len, 
        psa ? inet_ntoa(psa->sin_addr) : 0, psa ? ntohs(psa->sin_port) : 0, ERRNO_FMT_ARGS);
      return -1;
    } else if(rc > 0) {
      idx += rc;
    }
  }

  return (int) idx;
}

void net_closesocket(SOCKET *psock) {
  if(psock && *psock != INVALID_SOCKET) {
    if(*psock != 0) {
      // Be careful not to be calling close on 0 (stdin), which could end up having nasty side
      // effects on linux/mac
      //LOG(X_DEBUG("NET_CLOSESOCKET FD:%d"), *psock);
      closesocket(*psock);
    }
    *psock = INVALID_SOCKET;
  }
}

int net_issockremotelyclosed(SOCKET sock, int writer) {
  struct timeval tv;
  fd_set fdset;

  if(sock == INVALID_SOCKET) {
    return 1;
  }

  tv.tv_sec = 1;
  tv.tv_usec = 0;

  FD_ZERO(&fdset);
  FD_SET(sock, &fdset);

  if(select(sock + 1, 
    (!writer ? &fdset : NULL), (writer ? &fdset : NULL), NULL, &tv) < 0) {
    return 1;
  }

  return 0;
}

SOCKET net_listen(struct sockaddr_in *psain, int backlog) {

  SOCKET socksrv;

  if(!psain) {
    return INVALID_SOCKET;
  }

  if((socksrv = net_opensocket(SOCK_STREAM, 0, 0, psain)) == INVALID_SOCKET) {
    return INVALID_SOCKET;
  }

  if(listen(socksrv, backlog) < 0) {
    LOG(X_ERROR("Unable to listen on local port %s:%d (backlog:%d"),
          inet_ntoa(psain->sin_addr), ntohs(psain->sin_port), backlog);
    closesocket(socksrv);
    return INVALID_SOCKET;
  }

  return socksrv;
}

in_addr_t net_resolvehost(const char *host) {
  struct hostent *pHost;
  struct in_addr addr;

  if(!host) {
    return INADDR_NONE;
  } else if((addr.s_addr = inet_addr(host)) == INADDR_NONE) {
    LOG(X_DEBUG("Resolving '%s'"), host);
    if((pHost = gethostbyname(host)) == NULL) {
      LOG(X_ERROR("Unable to resolve host %s"), host);
      return INADDR_NONE;
    } else if(pHost->h_addrtype == AF_INET) {
      if(pHost->h_length < 1) {
        LOG(X_ERROR("No IP returned for host %s"), host);
        return INADDR_NONE;
      }
      addr.s_addr = *(uint32_t *) pHost->h_addr_list[0];
      LOG(X_INFO("Resolved '%s' to %s"), host, inet_ntoa(addr));  
    } else {
      LOG(X_ERROR("Only ipv4 destination address supported in resolver"));
      return INADDR_NONE;
     }
  }

  return addr.s_addr;
}

int net_setsocknonblock(SOCKET sock, int on) {
#ifdef WIN32

  unsigned long val;

  val = on;
  if(ioctlsocket(sock, FIONBIO, &val) == SOCKET_ERROR) {
    LOG(X_ERROR("ioctlsocket FIOBIO failed for socket"));
    return -1;
  }

#else // WIN32
  int val;

  if((val=fcntl(sock, F_GETFL, 0)) == -1) {
    LOG(X_ERROR("fcntl F_GETFL failed for SOCK_STREAM socket")); 
    return -1;
  }
  if(on && !(val & O_NONBLOCK)) {
    val |= O_NONBLOCK;
  } else if(!on && (val & O_NONBLOCK)) {
    val &= ~O_NONBLOCK;
  } else {
    return 0; 
  }
  if(fcntl(sock, F_SETFL, val) == -1) {
    LOG(X_ERROR("fcntl F_SETFL %d failed for SOCK_STREAM socket"), val);
    return -1;
  }

#endif // WIN32

  return 0;
}

int net_setqos(SOCKET sock, const struct sockaddr_in *psain, uint8_t dscp) {
  int rc = -1;

  if(sock == INVALID_SOCKET) {
    return -1;
  }

#if defined(WIN32) && 0

  DWORD dw;
  QOS_VERSION	Version;
  QOS_TRAFFIC_TYPE windscp;
  HANDLE     	qosHandle = NULL;
  QOS_FLOWID  flowId = 0;

  Version.MajorVersion = 1;
  Version.MinorVersion = 0;

  // Get a handle to the QoS subsystem (required for tracking).
  if(QOSCreateHandle(&Version, &qosHandle) != TRUE) {
    return -1;
  }

  dw = dscp;
  if(QOSSetFlow(qosHandle, 1, QOSSetOutgoingRate, sizeof(dw), &dw, 0, NULL) != TRUE) {
    QOSCloseHandle(qosHandle);
    return -1;
  }

  if(dscp >= DSCP_AF38) {
    windscp = QOSTrafficTypeControl;
  } else if(dscp >= DSCP_AF32) {
    windscp = QOSTrafficTypeAudioVideo;
  } else if(dscp >= DSCP_AF21) {
    windscp = QOSTrafficTypeBackground;
  } else {
    windscp = QOSTrafficTypeBestEffort;
  }

  if(QOSAddSocketToFlow(qosHandle, sock, (SOCKADDR *) psain, windscp, 0, &flowId) != TRUE) {
    QOSCloseHandle(qosHandle);
    return -1;
  }


  rc = 0;






  //QOS qos;
  //WSABUF  wbuf;
  //char    cbuf[1024];

  //cbuf[0] = '\0';
  //wbuf.buf = cbuf;
  //wbuf.len = 1024;
  //wbuf.buf ="G711";
  //wbuf.len=2;

  //if(WSAGetQOSByName(sock, &wbuf, &qos) != TRUE) {
  //fprintf(stderr, "bad %d\n", WSAGetLastError());
  //}


#endif // WIN32

#if defined(__linux__)
  int opt = dscp;

  if((rc = setsockopt(sock, SOL_IP, IP_TOS, &opt, sizeof(opt))) < 0) {
    LOG(X_WARNING("Unable to set sockopt SOL_IP IP_TOS 0x%x"), opt);
  }
#endif // __linux__

  return rc;
}

static in_addr_t g_local_ip = INADDR_NONE;
static char g_local_ipstr[32];
static char g_local_hostname[128];

int net_setlocaliphost(const char *ipstr) {
  struct in_addr inaddr;
  char *p;

  if(!ipstr) {
    return -1;
  }
  memset(&inaddr, 0, sizeof(inaddr));

  if((inaddr.s_addr = inet_addr(ipstr)) != INADDR_NONE) {
    g_local_ip = inaddr.s_addr; 
    if((p = inet_ntoa(inaddr))) {
      strncpy(g_local_ipstr, p, sizeof(g_local_ipstr) - 1);
    }
  } else {
    strncpy(g_local_hostname, ipstr, sizeof(g_local_hostname) - 1);
  }
  return 0;
}

in_addr_t net_getlocalip() {
  in_addr_t addr;
  char *dev = NULL;

  if(g_local_ip != INADDR_NONE) {
    addr = g_local_ip;
  } else if(pktgen_GetLocalAdapterInfo(dev, NULL, &addr) < 0) {
    addr = INADDR_ANY;
  }

  return addr;
}

const char *net_getlocalhostname() {
  struct in_addr inaddr;
  char *p;

  if(g_local_hostname[0] != '\0') {
    return g_local_hostname;
  } else {
    memset(&inaddr, 0, sizeof(inaddr));
    inaddr.s_addr = net_getlocalip();
    if(g_local_ipstr[0] == '\0' && (p = inet_ntoa(inaddr))) {
      strncpy(g_local_ipstr, p, sizeof(g_local_ipstr) - 1);
    }
  } 

  return g_local_ipstr;
}

int net_getlocalmac(const char *devArg, unsigned char mac[]) {  
  char *dev = (char *) devArg;

  return pktgen_GetLocalAdapterInfo(dev, mac, NULL);
}

static int makelocalport(int even) {
  uint16_t port;
  port = random() % 50000 + 10000;
  if(even && (port % 2) == 1) {
    port++;
  }
  return port;
}

int net_getlocalmediaport(int numpairs) {
  uint16_t port;

  port = makelocalport(1);

  return (int) port;
}


#if !defined(WIN32)
static pthread_mutex_t g_inet_ntoa_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif // WIN32

char *net_inet_ntoa(struct in_addr in, char *buf) {
  char *p;

  if(!buf) {
    return NULL;
  }

#if !defined(WIN32)
  pthread_mutex_lock(&g_inet_ntoa_mtx);
#endif // WIN32

  if((p = inet_ntoa(in))) {
    strncpy(buf, p, SAFE_INET_NTOA_LEN_MAX - 1);
  } else {
    buf[0] = '\0';
  }

#if !defined(WIN32)
  pthread_mutex_unlock(&g_inet_ntoa_mtx);
#endif // WIN32

  return buf;
}

