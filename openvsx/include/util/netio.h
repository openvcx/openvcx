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


#ifndef __NETIO_H__
#define __NETIO_H__

#include "netutil.h"
#include "timers.h"

#define NETIO_SSL_METHODSTR_SSLV2       "sslv2"
#define NETIO_SSL_METHODSTR_SSLV3       "sslv3"
#define NETIO_SSL_METHODSTR_SSLV23      "sslv23"
#define NETIO_SSL_METHODSTR_TLSV1       "tlsv1"
#define NETIO_SSL_METHODSTR_TLSV1_1     "tlsv1_1"
#define NETIO_SSL_METHODSTR_TLSV1_2     "tlsv1_2"

#define NETIO_SSL_METHODSTR_DEFAULT     NETIO_SSL_METHODSTR_TLSV1

#define NETIO_SSL_METHODS_STR  \
                 NETIO_SSL_METHODSTR_SSLV2"|" \
                 NETIO_SSL_METHODSTR_SSLV3"|" \
                 NETIO_SSL_METHODSTR_SSLV23"|" \
                 NETIO_SSL_METHODSTR_TLSV1"|" \
                 NETIO_SSL_METHODSTR_TLSV1_1"|" \
                 NETIO_SSL_METHODSTR_TLSV1_2 \

typedef enum NETIO_FLAG {
  NETIO_FLAG_NONE                  = 0x0000,
  NETIO_FLAG_PLAINTEXT             = 0x0001,
  NETIO_FLAG_SSL_TLS               = 0x0002,
  NETIO_FLAG_SSL_DTLS              = 0x0004,
  NETIO_FLAG_SRTP                  = 0x0008,
  NETIO_FLAG_SSL_DTLS_SRTP_OUTKEY  = 0x0010,
  NETIO_FLAG_SSL_DTLS_SERVER       = 0x0020
} NETIO_FLAG_T;

typedef enum SSL_SOCK_STATE {
  SSL_SOCK_STATE_ERROR                   = -1,
  SSL_SOCK_STATE_UNKNOWN                 = 0,
  SSL_SOCK_STATE_HANDSHAKE_INPROGRESS    = 2,
  SSL_SOCK_STATE_HANDSHAKE_COMPLETED     = 3
} SSL_SOCK_STATE_T;

typedef struct _STUN_SOCKET {
  SOCKET                             sockX;
  int                                stunFlags;
  TIME_VAL                           tmLastXmit;
  TIME_VAL                           tmLastRcv;

  struct sockaddr_storage            sainLastRcv;
  struct sockaddr_storage            sainLastXmit;

  //TODO: store last rcv,src addr pair, several versions for numerous ice candidates

  struct STREAM_STATS               *pXmitStats;
  pthread_mutex_t                    mtxXmit;
} STUN_SOCKET_T;

typedef STUN_SOCKET_T STUN_SOCKET;
struct NETIO_SOCK;

typedef int (* DTLS_NETSOCK_KEY_UPDATE)(void *, struct NETIO_SOCK *, const char *, int, int, const unsigned char *, unsigned int );

typedef struct DTLS_KEY_UPDATE_CTXT {
  void                       *pCbData;
  DTLS_NETSOCK_KEY_UPDATE     cbKeyUpdateFunc;
  int                         dtls_serverkey;
  int                         is_rtcp;
} DTLS_KEY_UPDATE_CTXT_T;

typedef struct SSL_SOCK {
  SSL_SOCK_STATE_T                 state;
  int                              ownBioRd;
  void                            *pBioRd;
  int                              ownBioWr;
  void                            *pBioWr;
  int                              ownCtxt;
  void                            *pCtxt;
  const struct STREAM_DTLS_CTXT   *pDtlsCtxt;
  DTLS_KEY_UPDATE_CTXT_T           dtlsKeysUpdateCtxt;
} SSL_SOCK_T;

typedef struct TURN_SOCK {
  int                        use_turn_indication_out;
  int                        use_turn_indication_in;
  int                        turn_channel;
  int                        have_permission;
  int                        have_error;
  TURN_POLICY_T              turnPolicy;
  struct sockaddr_storage    saPeerRelay;        // The peer's relay address:port assigned by the TURN server
                                                 // for which we create a permission for
  struct sockaddr_storage    saTurnSrv;          // The TURN server address:port used for relaying output data
} TURN_SOCK_T;

typedef struct NETIO_SOCK {
  STUN_SOCKET_T      stunSock;
  NETIO_FLAG_T       flags; 
  SSL_SOCK_T         ssl;
  TURN_SOCK_T        turn;
} NETIO_SOCK_T;


#define STUNSOCK_FD(ss) ((ss).sockX)
#define PSTUNSOCK_FD(pss) ((pss)->sockX)

#define STUNSOCK(ns) ((ns).stunSock)
#define PSTUNSOCK(pns) (&(pns)->stunSock)

#define NETIOSOCK_FD(ns)   STUNSOCK_FD(STUNSOCK(ns))
#define PNETIOSOCK_FD(pns) PSTUNSOCK_FD(PSTUNSOCK(pns))

#define NETIO_SET(to, from) NETIOSOCK_FD(to) = NETIOSOCK_FD(from); \
                            (to).stunSock.stunFlags = (from).stunSock.stunFlags; \
                            (to).stunSock.mtxXmit = (from).stunSock.mtxXmit; \
                            (to).flags = (from).flags; \
                            (to).ssl.pCtxt = (from).ssl.pCtxt; \
                            (to).ssl.pBioRd = (from).ssl.pBioRd; \
                            (to).ssl.pBioWr = (from).ssl.pBioWr;

#define IS_ADDR4_VALID(sin_addr) ((sin_addr).s_addr != INADDR_NONE && (sin_addr).s_addr != INADDR_ANY)

#define SSL_IDENTIFY_LEN_MIN                 5

int netio_recvnb(NETIO_SOCK_T *psock, unsigned char *buf, unsigned int len,
                 unsigned int mstmt);
int netio_recvnb_exact(NETIO_SOCK_T *psock, unsigned char *buf, unsigned int len,
                     unsigned int mstmt);
int netio_recv_exact(NETIO_SOCK_T *psock, const struct sockaddr *psa,
                   unsigned char *buf, unsigned int len);
int netio_recv(NETIO_SOCK_T *psock, const struct sockaddr *psa,
             unsigned char *buf, unsigned int len);
int netio_peek(NETIO_SOCK_T *psock, unsigned char *buf, unsigned int len);
int netio_send(NETIO_SOCK_T *psock, const struct sockaddr *psa,
               const unsigned char *buf, unsigned int len);
int netio_sendto(NETIO_SOCK_T *psock, const struct sockaddr *psa, 
                 const unsigned char *buf, unsigned int len, const char *descr);
void netio_closesocket(NETIO_SOCK_T *psock);
SOCKET netio_opensocket(NETIO_SOCK_T *psock, int socktype, unsigned int rcvbufsz, int sndbufsz,
                       const struct sockaddr *psa);
int netio_acceptssl(NETIO_SOCK_T *psock);
int netio_connectssl(NETIO_SOCK_T *psock);
int netio_ssl_init_srv(const char *certPath, const char *privKeyPath, const char *methodstr);
int netio_ssl_init_cli(const char *certPath, const char *privKeyPath, const char *methodstr);
int netio_ssl_close();
int netio_ssl_enabled(int server);
int netio_ssl_isssl(const unsigned char *pData, unsigned int len);



#endif // __NETIO_H__
