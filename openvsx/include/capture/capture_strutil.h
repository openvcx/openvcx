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


#ifndef __CAPTURE_STRUTIL_H__
#define __CAPTURE_STRUTIL_H__

#include "capture_sockettypes.h"


int capture_parsePortStr(const char *str, uint16_t localPorts[], size_t maxLocalPorts, 
                         int noDup);
int capture_parsePayloadTypes(const char *str,  int *vidPt, int *audPt);
int capture_parseLocalAddr(const char *localAddr, SOCKET_LIST_T *pSockList);
//int capture_parseAddr(const char *localAddr, struct sockaddr *psa, int defaultPort);
CAPTURE_FILTER_TRANSPORT_T capture_parseTransportStr(const char **ppstr);
int capture_parseAuthUrl(const char **ppstr, AUTH_CREDENTIALS_STORE_T *pauth);
char *capture_safe_copyToBuf(char *buf, size_t szbuf, const char *pstart, const char *pend);

char *capture_log_format_pkt(char *buf, unsigned int sz, const struct sockaddr *pSaSrc, 
                             const struct sockaddr *pSaDst);
char *stream_log_format_pkt(char *buf, unsigned int sz, const struct sockaddr *pSaSrc, 
                            const struct sockaddr *pSaDst);
char *stream_log_format_pkt_sock(char *buf, unsigned int sz, const NETIO_SOCK_T *pnetsock,
                                 const struct sockaddr *pSaDst);


#endif // __CAPTURE_STRUTIL_H__
