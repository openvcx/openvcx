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


#ifndef __SERVER_AUTH_H__
#define __SERVER_AUTH_H__

#define AUTH_BUF_SZ          512
#define AUTH_NONCE_MAXLEN    64
#define AUTH_OPAQUE_MAXLEN   AUTH_NONCE_MAXLEN
#define IS_AUTH_CREDENTIALS_SET(pAuthStore) ((pAuthStore)->username[0] != '\0' || (pAuthStore)->pass[0] != '\0')

struct SRV_LISTENER_CFG;

const char *srv_check_authorized(const AUTH_CREDENTIALS_STORE_T *pAuthStore, const HTTP_REQ_T *pReq, 
                                 char *authbuf);

int srvauth_authenticate(const AUTH_CREDENTIALS_STORE_T *pAuth, const HTTP_REQ_T *pReq,
                         const char *nonce_srv, const char *opaque_srv, const char *realm_srv,
                         char *bufAuthHdr, unsigned int outBufSz);
int srv_check_authtoken(const struct SRV_LISTENER_CFG *pListenCfg, const HTTP_REQ_T *pReq, int allowcoma);
int srv_write_authtoken(char *buf, unsigned int szbuf, const char *pAuthTokenId, 
                        const char *authTokenFromUri, int querystr);


#endif // __SERVER_CMD_H__
