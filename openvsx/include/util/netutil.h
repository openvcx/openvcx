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



SOCKET net_opensocket(int socktype, unsigned int rcvbufsz, int sndbufsz, struct sockaddr_in *psain);
int net_issockremotelyclosed(SOCKET sock, int writer);
SOCKET net_listen(struct sockaddr_in *psain, int backlog);


void net_closesocket(SOCKET *psock);
int net_setsocknonblock(SOCKET sock, int on);
int net_connect(SOCKET sock, struct sockaddr_in *psa);
//int net_bindlistener(SOCKET sock, struct sockaddr_in *psain);
int net_getlocalmediaport(int numpairs);
int net_setqos(SOCKET sock, const struct sockaddr_in *psain, uint8_t dscp);
in_addr_t net_resolvehost(const char *host);
in_addr_t net_getlocalip();
const char *net_getlocalhostname();
int net_getlocalmac(const char *dev, unsigned char mac[]);
int net_setlocaliphost(const char *ipstr);

#define SAFE_INET_NTOA_LEN_MAX     32
char *net_inet_ntoa(struct in_addr in, char *buf);






#endif // __NETUTIL_H__
