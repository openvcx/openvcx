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


#ifndef __SRV_PROXY_H__
#define __SRV_PROXY_H__

#include "unixcompat.h"

#define MGR_RTMPPROXY_MAX         100
#define MGR_RTSPPROXY_MAX         100

void srvmgr_rtmpproxy_proc(void *pArg);
void srvmgr_rtspproxy_proc(void *pArg);
void srvmgr_httpproxy_proc(void *pArg);

int srvmgr_http_handle_conn(SRV_MGR_CONN_T *pConn, const HTTP_PARSE_CTXT_T *pHdrCtxt);

#endif // __SRV_PROXY_H__
