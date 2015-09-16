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

#if defined(VSX_HAVE_SSL)

#undef ptrdiff_t
#include "openssl/ssl.h"
#include "openssl/err.h"

#if defined(__APPLE__) || defined(__linux__)
//
// Prevent SIGPIPE on socket write from SSL BIO
//
#include <signal.h>
#endif // (__APPLE__) || defined(__linux__)


static int g_ssl_init_srv;
static int g_ssl_init_cli;
static SSL_CTX *g_ssl_ctx_srv;
static SSL_CTX *g_ssl_ctx_cli;

static int netio_ssl_recvnb(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned int len, 
                            unsigned int mstmt);
static int netio_ssl_recv(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, 
                            unsigned char *buf, unsigned int len);
static int netio_ssl_recvnb_exact(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned int len,
                            unsigned int mstmt);
static int netio_ssl_recv_exact(NETIO_SOCK_T *pnesock, const struct sockaddr *psa,
                            unsigned char *buf, unsigned int len);

#endif // VSX_HAVE_SSL

extern int net_recvnb(SOCKET sock, unsigned char *buf, unsigned int len, unsigned int mstmt);
extern int net_recvnb_exact(SOCKET sock, unsigned char *buf, unsigned int len, unsigned int mstmt);
extern int net_recv_exact(SOCKET sock, const struct sockaddr *psa,
                   unsigned char *buf, unsigned int len);
extern int net_recv(SOCKET sock, const struct sockaddr *psa,
                    unsigned char *buf, unsigned int len);
extern int net_send(SOCKET sock, const struct sockaddr *psa, const unsigned char *pbuf,
                    unsigned int len);
extern int net_sendto(SOCKET sock, const struct sockaddr *psa, const unsigned char *pbuf,
                    unsigned int len, const char *descr);


int netio_peek(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned len) {
  if(!pnetsock) {
    return -1;
  }

  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)) {

#if defined(VSX_HAVE_SSL)

    if(PNETIOSOCK_FD(pnetsock)== INVALID_SOCKET || !pnetsock->ssl.pCtxt) {
      return -1;
    } else { 
      return SSL_peek(pnetsock->ssl.pCtxt, buf, len);
    }

#else // VSX_HAVE_SSL
    return -1;
#endif // VSX_HAVE_SSL

  } else {
    return recv(PNETIOSOCK_FD(pnetsock), buf, len, MSG_PEEK);
  }

}

int netio_recvnb(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned int len, unsigned int mstmt) {
  if(!pnetsock) {
    return -1;
  }

  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)) {

#if defined(VSX_HAVE_SSL)

//fprintf(stderr, "NETIO_RECVNB SSL this:0x%x, sslctxt:0x%x sock:%d len:%d, mstmt:%d\n", pnetsock, pnetsock->ssl.pctxt, PNETIOSOCK_FD(pnetsock), len, mstmt);
    
    return netio_ssl_recvnb(pnetsock, buf, len, mstmt);

#else // VSX_HAVE_SSL
    return -1;
#endif // VSX_HAVE_SSL

  } else {
    return net_recvnb(PNETIOSOCK_FD(pnetsock), buf, len, mstmt);
  }
}

int netio_recvnb_exact(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned int len,
                     unsigned int mstmt) {
  if(!pnetsock) {
    return -1;
  }
  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)) {

#if defined(VSX_HAVE_SSL)

    return netio_ssl_recvnb_exact(pnetsock, buf, len, mstmt);
    
#else // VSX_HAVE_SSL
    return -1;
#endif // VSX_HAVE_SSL

  } else {
    return net_recvnb_exact(PNETIOSOCK_FD(pnetsock), buf, len, mstmt);
  }

}

int netio_recv_exact(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, 
                   unsigned char *buf, unsigned int len) {

  if(!pnetsock) {
    return -1;
  }
  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)) {

#if defined(VSX_HAVE_SSL)

    return netio_ssl_recv_exact(pnetsock, psa, buf, len);

#else // VSX_HAVE_SSL
    return -1;
#endif // VSX_HAVE_SSL

  } else {
    return net_recv_exact(PNETIOSOCK_FD(pnetsock), psa, buf, len);
  }
}

int netio_recv(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, 
             unsigned char *buf, unsigned int len) {

  if(!pnetsock) {
    return -1;
  }
  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)) {

#if defined(VSX_HAVE_SSL)

    return netio_ssl_recv(pnetsock, psa, buf, len);

#else // VSX_HAVE_SSL
    return -1;
#endif // VSX_HAVE_SSL

  } else {
    return net_recv(PNETIOSOCK_FD(pnetsock), psa, buf, len);
  }
}

int netio_send(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, 
               const unsigned char *buf, unsigned int len) {
  if(!pnetsock) {
    return -1;
  }
  if((pnetsock->flags & NETIO_FLAG_SSL_TLS)) {

#if defined(VSX_HAVE_SSL)

  //fprintf(stderr, "NETIO_SEND SSL sslctxt:0x%x sock:%d len:%d\n", pnetsock->ssl.pCtxt, PNETIOSOCK_FD(pnetsock), len);
    
    if(!pnetsock->ssl.pCtxt) {
      return -1;
    }

//#if defined(__linux__) 
//    signal(SIGPIPE, SIG_IGN);
//#endif // __linux__

    return SSL_write(pnetsock->ssl.pCtxt, buf, len);

#else // VSX_HAVE_SSL
    return -1;
#endif // VSX_HAVE_SSL

  } else {
    return net_send(PNETIOSOCK_FD(pnetsock), psa, buf, len);
  }
}

int netio_sendto(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, 
               const unsigned char *buf, unsigned int len, const char *descr) {

  if(!pnetsock) {
    return -1;
  }

  return net_sendto(PNETIOSOCK_FD(pnetsock), psa, buf, len, descr);
}

void netio_closesocket(NETIO_SOCK_T *pnetsock) {

  if(!pnetsock) {
    return;
  }

#if defined(VSX_HAVE_SSL)

  //fprintf(stderr, "NETIO_CLOSESOCKET SSL this:0x%x, sslctxt:0x%x sock:%d\n", pnetsock, pnetsock->ssl.pCtxt, PNETIOSOCK_FD(pnetsock));

  if(pnetsock->ssl.pBioRd) {
    if(pnetsock->ssl.ownBioRd) {
      BIO_free(pnetsock->ssl.pBioRd);
      pnetsock->ssl.ownBioRd = 0;
    }
    pnetsock->ssl.pBioRd = NULL;
  }

  if(pnetsock->ssl.pBioWr) {
    if(pnetsock->ssl.ownBioWr) {
      BIO_free(pnetsock->ssl.pBioWr);
      pnetsock->ssl.ownBioWr = 0;
    }
    pnetsock->ssl.pBioWr = NULL;
  }

  if(pnetsock->ssl.pCtxt) {
    if(pnetsock->ssl.ownCtxt) {
      SSL_shutdown(pnetsock->ssl.pCtxt);
      SSL_free(pnetsock->ssl.pCtxt);
    }
    pnetsock->ssl.pCtxt = NULL;
  }

  pnetsock->ssl.pDtlsCtxt = NULL;
  memset(&pnetsock->ssl.dtlsKeysUpdateCtxt, 0, sizeof(pnetsock->ssl.dtlsKeysUpdateCtxt));

#endif // VSX_HAVE_SSL

  net_closesocket(&PNETIOSOCK_FD(pnetsock));

  pthread_mutex_destroy(&PSTUNSOCK(pnetsock)->mtxXmit);

}

SOCKET netio_opensocket(NETIO_SOCK_T *pnetsock, int socktype, unsigned int rcvbufsz, int sndbufsz,
                      const struct sockaddr *psa) {

  SOCKET sock = INVALID_SOCKET;

  if(!pnetsock) {
    return sock;
  }

  sock = net_opensocket(socktype, rcvbufsz, sndbufsz, psa);

  PNETIOSOCK_FD(pnetsock) = sock;
  PSTUNSOCK(pnetsock)->stunFlags = 0;
  PSTUNSOCK(pnetsock)->tmLastXmit = 0;
  PSTUNSOCK(pnetsock)->tmLastRcv = 0;
  memset(&PSTUNSOCK(pnetsock)->sainLastXmit, 0, sizeof(PSTUNSOCK(pnetsock)->sainLastXmit));
  memset(&PSTUNSOCK(pnetsock)->sainLastRcv, 0, sizeof(PSTUNSOCK(pnetsock)->sainLastRcv));
  PSTUNSOCK(pnetsock)->pXmitStats = NULL;
  
  pthread_mutex_init(&PSTUNSOCK(pnetsock)->mtxXmit, NULL);
  pnetsock->flags = 0; 
  memset(&pnetsock->ssl, 0, sizeof(pnetsock->ssl));

  return sock;
}

SOCKET netio_acceptssl(NETIO_SOCK_T *pnetsock) {

#if defined(VSX_HAVE_SSL)

  int rc;
  size_t sz;
  struct sockaddr_storage sa;
  char tmp[128];
  SSL *ssl_ctxt = NULL;

#endif // VSX_HAVE_SSL

  if(!pnetsock || PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET) {
    return -1;
  } else if(!(pnetsock->flags & NETIO_FLAG_SSL_TLS)) {
    return 0;
  }

#if defined(VSX_HAVE_SSL)

  if(g_ssl_init_srv == 0) {
    LOG(X_ERROR("SSL not initialized"));
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  } else if(g_ssl_init_srv < 0 || !g_ssl_ctx_srv) {
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  }


  if(!(ssl_ctxt = SSL_new(g_ssl_ctx_srv))) {
    LOG(X_ERROR("SSL_new failed"));
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  }

  if(SSL_set_fd(ssl_ctxt, PNETIOSOCK_FD(pnetsock)) <= 0) {
    LOG(X_ERROR("SSL_set_fd %d failed"), PNETIOSOCK_FD(pnetsock));
    SSL_free(ssl_ctxt);
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  }

#if defined(__APPLE__) || defined(__linux__)
  //
  // Prevent SIGPIPE on socket write from SSL BIO
  //
  signal(SIGPIPE, SIG_IGN);
#endif // (__APPLE__) || defined(__linux__)

  if((rc = SSL_accept(ssl_ctxt)) <= 0) {
    sz = sizeof(sa);
    getpeername(PNETIOSOCK_FD(pnetsock), (struct sockaddr *) &sa, (socklen_t *) &sz);
    rc = SSL_get_error(ssl_ctxt, rc);
    LOG(X_ERROR("SSL_accept failed from %s:%d, socket:%d. SSL Error: %d '%s'"), FORMAT_NETADDR(sa, tmp, sizeof(tmp)), 
                 htons(INET_PORT(sa)), PNETIOSOCK_FD(pnetsock), rc, ERR_reason_error_string(ERR_get_error()));

    SSL_free(ssl_ctxt);
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    ERR_print_errors_fp(stderr);
    return -1;
  }

  pnetsock->ssl.pCtxt = ssl_ctxt;

//fprintf(stderr, "NETIO_ACCEPT SSL this:0x%x, sslctxt:0x%x sock:%d\n", pnetsock, pnetsock->ssl.pCtxt, PNETIOSOCK_FD(pnetsock));


#else // VSX_HAVE_SSL

  net_closesocket(&PNETIOSOCK_FD(pnetsock));
  return -1;

#endif // VSX_HAVE_SSL

  return 0;
}

int netio_connectssl(NETIO_SOCK_T *pnetsock) {

#if defined(VSX_HAVE_SSL)

  int rc;
  size_t sz;
  struct sockaddr_storage sa;
  char tmp[128];
  SSL *ssl_ctxt = NULL;

#endif // VSX_HAVE_SSL

  if(!pnetsock || PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET) {
    return -1;
  } else if(!(pnetsock->flags & NETIO_FLAG_SSL_TLS)) {
    return 0;
  }

#if defined(VSX_HAVE_SSL)

  if(g_ssl_init_cli == 0) {
    LOG(X_ERROR("SSL not initialized"));
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  } else if(g_ssl_init_cli < 0 || !g_ssl_ctx_cli) {
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  }

  if(!(ssl_ctxt = SSL_new(g_ssl_ctx_cli))) {
    LOG(X_ERROR("SSL_new failed"));
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  }

  if(SSL_set_fd(ssl_ctxt, PNETIOSOCK_FD(pnetsock)) <= 0) {
    LOG(X_ERROR("SSL_set_fd %d failed"), PNETIOSOCK_FD(pnetsock));
    SSL_free(ssl_ctxt);
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    return -1;
  }

#if defined(__APPLE__) || defined(__linux__)
  //
  // Prevent SIGPIPE on socket write from SSL BIO
  //
  signal(SIGPIPE, SIG_IGN);
#endif // (__APPLE__) || defined(__linux__)

  if((rc = SSL_connect(ssl_ctxt)) <= 0) {
    sz = sizeof(sa);
    getpeername(PNETIOSOCK_FD(pnetsock), (struct sockaddr *) &sa, (socklen_t *) &sz);
    rc = SSL_get_error(ssl_ctxt, rc);
    LOG(X_ERROR("SSL_connect failed to %s:%d, socket:%d. SSL Error: %d '%s'"), FORMAT_NETADDR(sa, tmp, sizeof(tmp)), 
                 htons(INET_PORT(sa)), PNETIOSOCK_FD(pnetsock), rc, ERR_reason_error_string(ERR_get_error()));

    SSL_free(ssl_ctxt);
    net_closesocket(&PNETIOSOCK_FD(pnetsock));
    ERR_print_errors_fp(stderr);
    return -1;
  }

  pnetsock->ssl.pCtxt = ssl_ctxt;

#else // VSX_HAVE_SSL

  net_closesocket(&PNETIOSOCK_FD(pnetsock));
  return -1;

#endif // VSX_HAVE_SSL

  return 0;

}


#if defined(VSX_HAVE_SSL)

static int netio_ssl_recvnb(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned int len, 
                            unsigned int mstmt) {
  struct timeval tv, tv0;
  fd_set fdsetRd;
  int rc;
  int rcerr;
  int tryread = 0;
  unsigned int mselapsed = 0;  

  gettimeofday(&tv0, NULL);


//rc=fcntl(PNETIOSOCK_FD(pnetsock), F_GETFL, 0); fprintf(stderr, "SSL SOCK:%d rc:%d block:%d\n", PNETIOSOCK_FD(pnetsock), rc, (rc & O_NONBLOCK) ? 0 : 1);

  do {

    if(PNETIOSOCK_FD(pnetsock)== INVALID_SOCKET || !pnetsock->ssl.pCtxt) {
      return -1;
    }

    if(mselapsed > mstmt) {
      return 0;
    } else {
      mselapsed = mstmt - mselapsed; 
    }

    tv.tv_sec = (mselapsed / 1000);
    tv.tv_usec = (mselapsed % 1000) * 1000;

    FD_ZERO(&fdsetRd);
    FD_SET(PNETIOSOCK_FD(pnetsock), &fdsetRd);

    if(!tryread) {
      VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb SSL_read (before select) len:%d ... "), 0));
      rc = SSL_read(pnetsock->ssl.pCtxt, buf, 0);
      VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb SSL_read (before select) len:%d rc:%d"), 0, rc));
      if(rc < 0) {
        rcerr = SSL_get_error(pnetsock->ssl.pCtxt, rc);
        VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb SSL_get_error: (before select) rc:%d"),  rcerr));
      }
    }

    VSX_DEBUG_SSL(LOG(X_DEBUG("SSL - netio_ssl_recvnb len: %d select fd: %d ... "), len, PNETIOSOCK_FD(pnetsock)));

    if((!tryread && rc >= 0) || (rc = select(PNETIOSOCK_FD(pnetsock) + 1, &fdsetRd, NULL, NULL, &tv)) > 0) {

      VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb SSL_read len:%d ... "), len));
      rc = SSL_read(pnetsock->ssl.pCtxt, buf, len);
      VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb SSL_read len:%d rc:%d "), len, rc));

      if(rc < 0) {

        rcerr = SSL_get_error(pnetsock->ssl.pCtxt, rc);
        if(!(rcerr == SSL_ERROR_WANT_READ || rcerr == SSL_ERROR_WANT_WRITE)) {
          LOG(X_ERROR("SSL_read nonblock %d returned "));
          rc = -1;
          break;
        }

      } else if(rc == 0) {

        //
        // SSL connection has been shut down
        //

        rcerr = SSL_get_error(pnetsock->ssl.pCtxt, rc);
        rc = -1;
        break;

      } else {
        break;
      }

    } else if(rc == 0) {
      VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb select rc: %d"), rc));
      //fprintf(stderr, "TMT EXPIRED sock:%d tv:%d,%d, mstmt:%d\n", sock, tv.tv_sec, tv.tv_usec, mstmt);
      // timeout expired
    } else {
      LOG(X_ERROR("select socket:%d returned: %d"), pnetsock->ssl.pCtxt, rc);
      break;
    }

    gettimeofday(&tv, NULL);
    mselapsed = TIME_TV_DIFF_MS(tv, tv0);
    tryread = 1;

  } while(1);

  return rc;
}

static int netio_ssl_recv(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa, 
                          unsigned char *buf, unsigned int len) {
  char tmp[128];
  int rc = 0;

  if(PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET || !pnetsock->ssl.pCtxt) {
    return -1;
  }

  do {

    VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recv SSL_read len:%d ... "), len));

    if((rc = SSL_read(pnetsock->ssl.pCtxt, buf, len)) >= 0) {
      VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recv SSL_read len:%d rc: %d"), len, rc));
      break;
    }

    //if(errno == EAGAIN) {
    //  return 0;
    //} else {

      LOG(X_ERROR("SSL_read failed (%d) for %u bytes from %s:%d "ERRNO_FMT_STR), rc, len,
           psa ? FORMAT_NETADDR(*psa, tmp, sizeof(tmp)) : 0, psa ? ntohs(PINET_PORT(psa)) : 0, ERRNO_FMT_ARGS);
      return -1;
    //}
  } while(rc < 0);

  // client disconnected rc = 0

  return rc;
}

static int netio_ssl_recvnb_exact(NETIO_SOCK_T *pnetsock, unsigned char *buf, unsigned int len,
                     unsigned int mstmt) {

  int rc = 0;
  struct timeval tv, tv0;
  unsigned int mselapsed = 0;
  unsigned int idx = 0;

  if(PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET || !pnetsock->ssl.pCtxt) {
    return -1;
  }

  gettimeofday(&tv0, NULL);

  do {

    gettimeofday(&tv, NULL);

    mselapsed = TIME_TV_DIFF_MS(tv, tv0);

    if(mselapsed < mstmt) {
      mselapsed = mstmt - mselapsed;
    } else if(mselapsed > mstmt) {
      break;      
    }

    VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb_exact SSL_read len:%d [%d]/%d ... "), len - idx, idx, len));

    if((rc = netio_ssl_recvnb(pnetsock, &buf[idx], len - idx, mselapsed)) > 0) {
      idx += rc;
    } else {
      break;
    }

  } while(idx < len && !g_proc_exit);

  VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recvnb_exact len:%d, returning:%d "), len, idx));

  return (int) idx;
}

static int netio_ssl_recv_exact(NETIO_SOCK_T *pnetsock, const struct sockaddr *psa,
                            unsigned char *buf, unsigned int len) {
  int rc = 0;
  unsigned int idx = 0;

  if(PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET || !pnetsock->ssl.pCtxt) {
    return -1;
  }

  do {

    VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recv_exact SSL_read len:%d [%d]/%d ... "), len - idx, idx, len));

    if((rc = netio_ssl_recv(pnetsock, psa, &buf[idx], len - idx)) > 0) {
      idx += rc;
    } else {
      break;
    }

  } while(idx < len && !g_proc_exit);

  VSX_DEBUG_SSL( LOG(X_DEBUG("SSL - netio_ssl_recv_exact len:%d, returning:%d "), len, idx));

  return (int) idx;
}

int netio_ssl_enabled(int server) {

  if((server && g_ssl_init_srv > 0) ||
     (!server && g_ssl_init_cli > 0)) {
    return 1;
  }

  return 0;
}

int netio_ssl_close() {
  int rc = 0;

  if(g_ssl_ctx_srv) {
    SSL_CTX_free(g_ssl_ctx_srv);
    g_ssl_ctx_srv = NULL;
  }

  sslutil_close();
  //EVP_cleanup();

  g_ssl_init_srv = 0;

  return rc;
}

/*
static int netio_ssl_init() {

  SSL_load_error_strings();
  SSL_library_init();
  //ERR_load_BIO_strings();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();

  return 0;
}
*/

int netio_ssl_init_srv(const char *certPath, const char *privKeyPath) {
  int rc = 0;
  const SSL_METHOD *method = NULL;

  if(!certPath || !privKeyPath) {
    return -1;
  }

  //
  // g_ssl_mtx is defined as a system global in in vsxlib.c
  //
  pthread_mutex_lock(&g_ssl_mtx);

  if(g_ssl_init_srv > 0) {
    pthread_mutex_unlock(&g_ssl_mtx);
    return 0;
  }

  //if((rc = netio_ssl_init()) >= 0) {
  if((rc = sslutil_init()) >= 0) {

    //TODO: make this configurable

    //method = SSLv2_server_method();
    //method = SSLv23_server_method();
    method = TLSv1_server_method();
    //method = SSLv3_server_method();

  }

  if(rc >= 0 && method && !(g_ssl_ctx_srv = SSL_CTX_new(method))) {
    LOG(X_ERROR("SSL_CTX_new failed for TLS"));
    g_ssl_init_srv = -1;
    rc = -1;
  }

  //
  // openssl req -new -x509 -key privkey.pem -out cacert.pem -days 1095
  //
  // Dump the certificate
  // openssl x509 -text -in cacert.pem
  //
  // Get fingerprint
  // openssl x509 -noout -in cacert.pem -fingerprint -sha256
  //
  if(rc >= 0 && SSL_CTX_use_certificate_file(g_ssl_ctx_srv, certPath, SSL_FILETYPE_PEM) != 1) {
    LOG(X_ERROR("SSL_CTX_use_certificate_file '%s' failed. '%s'"), 
                certPath, ERR_reason_error_string(ERR_get_error()));
    g_ssl_init_srv = -1;
    rc = -1;
  }

  //SSL_CTX_set_default_passwd_cb_userdata(g_ssl_ctx_srv, "password");

  //
  // create private key w/o passphrase
  // openssl genrsa -out sslkey.pem 1024
  //
  // create private key w/ passphrase
  // openssl genrsa -des3 -out sslkey.pem -passout pass:password_here 1024
  //
  // View the contents of the private key
  // openssl rsa -noout -text -in sslkey.pem
  //
  // Output public key
  // openssl rsa -in sslkey.pem -pubout
  //
  if(rc >= 0 && SSL_CTX_use_PrivateKey_file(g_ssl_ctx_srv, privKeyPath, SSL_FILETYPE_PEM) != 1) {
    LOG(X_ERROR("SSL_CTX_use_PrivateKey_file '%s' failed. '%s'"), 
        privKeyPath, ERR_reason_error_string(ERR_get_error()));

    g_ssl_init_srv = -1;
    rc = -1;
  }

  if(rc >= 0 && SSL_CTX_check_private_key(g_ssl_ctx_srv) != 1) {
    LOG(X_ERROR("SSL_CTX_check_private_key failed. '%s'"), ERR_reason_error_string(ERR_get_error()));
    g_ssl_init_srv = -1;
    rc = -1;
  }

  if(rc >= 0) {
    g_ssl_init_srv = 1;
  }

  pthread_mutex_unlock(&g_ssl_mtx);

  return rc;
}

int netio_ssl_init_cli(const char *certPath, const char *privKeyPath) {
  int rc = 0;
  const SSL_METHOD *method = NULL;

  //if(!certPath || !privKeyPath) {
  //  return -1;
  //}

  //
  // g_ssl_mtx is defined as a system global in in vsxlib.c
  //
  pthread_mutex_lock(&g_ssl_mtx);

  if(g_ssl_init_cli > 0) {
    pthread_mutex_unlock(&g_ssl_mtx);
    return 0;
  }

  //if((rc = netio_ssl_init()) >= 0) {
  if((rc = sslutil_init()) >= 0) {

    //method = SSLv2_client_method();
    //method = SSLv23_client_method();
    method = TLSv1_client_method();
    //method = SSLv3_client_method();

  }

  if(rc >= 0 && method && !(g_ssl_ctx_cli = SSL_CTX_new(method))) {
    LOG(X_ERROR("SSL_CTX_new failed"));
    g_ssl_init_cli = -1;
    rc = -1;
  }

/*
  // openssl req -new -x509 -key privkey.pem -out cacert.pem -days 1095
  if(rc >= 0 && SSL_CTX_use_certificate_file(g_ssl_ctx_srv, certPath, SSL_FILETYPE_PEM) != 1) {
    LOG(X_ERROR("SSL_CTX_use_certificate_file '%s' failed. '%s'"), 
                certPath, ERR_reason_error_string(ERR_get_error()));
    g_ssl_init_srv = -1;
    rc = -1;
  }
  // openssl genrsa 1024
  if(rc >= 0 && SSL_CTX_use_PrivateKey_file(g_ssl_ctx_srv, privKeyPath, SSL_FILETYPE_PEM) != 1) {
    LOG(X_ERROR("SSL_CTX_use_PrivateKey_file '%s' failed. '%s'"), 
        privKeyPath, ERR_reason_error_string(ERR_get_error()));

    g_ssl_init_srv = -1;
    rc = -1;
  }

  if(rc >= 0 && SSL_CTX_check_private_key(g_ssl_ctx_srv) != 1) {
    LOG(X_ERROR("SSL_CTX_check_private_key failed. '%s'"), ERR_reason_error_string(ERR_get_error()));
    g_ssl_init_srv = -1;
    rc = -1;
  }
*/

  if(rc >= 0) {
    g_ssl_init_cli = 1;
  }

  pthread_mutex_unlock(&g_ssl_mtx);

  return rc;
}

#else // VSX_HAVE_SSL

int netio_ssl_enabled(int server) {
  return 0;
}

int netio_ssl_close() {
  return 0;
}

int netio_ssl_init_srv(const char *certPath, const char *privKeyPath) {
  LOG(X_ERROR("SSL not enabled"));
  return -1;
}

int netio_ssl_init_cli(const char *certPath, const char *privKeyPath) {
  LOG(X_ERROR("SSL not enabled"));
  return -1;
}

#endif // VSX_HAVE_SSL
