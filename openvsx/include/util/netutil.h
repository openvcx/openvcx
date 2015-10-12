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


#ifndef __NETUTIL_H__
#define __NETUTIL_H__

#ifdef WIN32

#include "unixcompat.h"

#else

#include <unistd.h>
#include <sys/socket.h>
#include "wincompat.h"


#endif // WIN32

#define DSCP_AF11       (10 << 2)
#define DSCP_AF12       (12 << 2)
#define DSCP_AF13       (14 << 2)
#define DSCP_AF21       (18 << 2)
#define DSCP_AF22       (20 << 2)
#define DSCP_AF23       (22 << 2)
#define DSCP_AF31       (26 << 2)
#define DSCP_AF32       (28 << 2)
#define DSCP_AF33       (30 << 2)
#define DSCP_AF34       (34 << 2)
#define DSCP_AF36       (36 << 2)
#define DSCP_AF38       (38 << 2)

#define SOCK_RCVBUFSZ_DEFAULT          0x2ffff
#define SOCK_RCVBUFSZ_UDP_DEFAULT      0x2ffff
#define SOCK_SNDBUFSZ_UDP_DEFAULT      0 

#define NET_BACKLOG_DEFAULT   5

//
// IPv6 compatibility macros
//
#define ADDR_LEN_IPV4               4
#define ADDR_LEN_IPV6               16

#define INET_NTOP(storage, buf, bufsz)  ((const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? \
                      inet_ntop(AF_INET6, &((const struct sockaddr_in6 *) &(storage))->sin6_addr, (buf), (bufsz)) : \
                      inet_ntop(AF_INET, &((const struct sockaddr_in *) &(storage))->sin_addr, (buf), (bufsz))

#define INET_PTON(ip, storage)  (((const const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? \
                      inet_pton(AF_INET6, ip, &((struct sockaddr_in6 *) &(storage))->sin6_addr) : \
                      inet_pton(AF_INET, ip, &((struct sockaddr_in *) &(storage))->sin_addr))
#define INET_PORT(storage) (((struct sockaddr_in *) &(storage))->sin_port)
#define PINET_PORT(pstorage) (((struct sockaddr_in *) (pstorage))->sin_port)

#define INET_SIZE(storage) (((const struct sockaddr *) &(storage))->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) : \
                            sizeof(struct sockaddr_in))

#define INET_IS_MULTICAST(storage) (((const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? \
                      IN6_IS_ADDR_MULTICAST( &((const struct sockaddr_in6 *)  &(storage))->sin6_addr) : \
                      IN_MULTICAST( htonl(((const struct sockaddr_in *) &(storage))->sin_addr.s_addr)))

#define INET_IS_SAMEADDR(s1, s2)  \
     ((((const struct sockaddr *)&(s1))->sa_family == AF_INET6 && (!memcmp(&((const struct sockaddr_in6 *) &(s1))->sin6_addr.s6_addr[0],  \
                                          &((const struct sockaddr_in6 *) &(s2))->sin6_addr.s6_addr[0], 16))) ||  \
      (((const struct sockaddr *)&(s1))->sa_family != AF_INET6 && ((const struct sockaddr_in *) &(s1))->sin_addr.s_addr == \
                                  ((const struct sockaddr_in *) &(s2))->sin_addr.s_addr))

#define INET_ADDR_VALID(storage)   (((const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? \
                                   !IN6_IS_ADDR_UNSPECIFIED( &((const struct sockaddr_in6 *) &(storage))->sin6_addr) : \
                                   IS_ADDR4_VALID(   ((const struct sockaddr_in *) &(storage))->sin_addr ))

#define INET_ADDR_LOCALHOST(storage)  (((const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? \
                                   IN6_IS_ADDR_LOOPBACK( &((const struct sockaddr_in6 *) &(storage))->sin6_addr) : \
                                   IN_LOCALHOST(htonl(((const struct sockaddr_in *) &(storage))->sin_addr.s_addr)))

#define INET_NTOP_ADDR_FMT_STR "%s%s%s"
#define INET_NTOP_ADDR_FMT_ARGS(storage, buf, bufsz) \
                            ((const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? "[" : "", \
                            INET_NTOP(storage, buf, bufsz), \
                            ((const struct sockaddr_storage *) &(storage))->ss_family == AF_INET6 ? "]" : ""

#define INET_NTOP_FMT_STR INET_NTOP_ADDR_FMT_STR":%d"
#define INET_NTOP_FMT_ARGS(storage, buf, bufsz)  INET_NTOP_ADDR_FMT_ARGS(storage, buf, bufsz), \
                                                 ntohs(INET_PORT(storage)) 
//
// Used to format an IPv4 or IPv6 address as a string
//
#define FORMAT_NETADDR(sa, buf, bufsz) strutil_format_netaddr((const struct sockaddr *) &(sa), (buf), (bufsz))


SOCKET net_opensocket(int socktype, unsigned int rcvbufsz, int sndbufsz, const struct sockaddr *psa);
int net_issockremotelyclosed(SOCKET sock, int writer);
SOCKET net_listen(const struct sockaddr *psa, int backlog);


void net_closesocket(SOCKET *psock);
int net_setsocknonblock(SOCKET sock, int on);
int net_connect(SOCKET sock, const struct sockaddr *psa);
//int net_bindlistener(SOCKET sock, struct sockaddr *psa);
int net_getlocalmediaport(int numpairs);
//int net_setqos4(SOCKET sock, const struct sockaddr *psa, uint8_t dscp);
int net_setqos(SOCKET sock, const struct sockaddr *psa , uint8_t dscp);
int net_resolvehost(const char *host, struct sockaddr_storage *pstorage);
int net_getaddress(const char *address, struct sockaddr_storage *pstorage);
in_addr_t net_getlocalip4();
const char *net_getlocalhostname();
int net_getlocalmac(const char *dev, unsigned char mac[]);
int net_setlocaliphost(const char *ipstr);

#define SAFE_INET_NTOA_LEN_MAX     32
#define SAFE_INET_NTOP_LEN_MAX    64 // > INET6_ADDRSTRLEN (46)
//char *net_inet_ntoa(struct in_addr in, char *buf);
//char *net_inet_ntop(const struct sockaddr *psa, char *buf);
int net_isipv6(const char *str);
int net_isipv4(const char *str);






#endif // __NETUTIL_H__
