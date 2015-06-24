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




#ifndef __UTILS__H__
#define __UTILS__H__

#if defined(WIN32)
#include "unixcompat.h"
#include "pthread_compat.h"
#else
#include "wincompat.h"
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#endif // WIN32

#include "vsxconfig.h"

#include "util/math_common.h"
#include "util/pool.h"
#include "util/burstmeter.h"
#include "util/base64.h"
#include "util/bits.h"
#include "util/crc.h"
#include "util/conf.h"
#include "util/auth.h"
#include "util/sslutil.h"
#include "util/netutil.h"
#include "util/netio.h"
#include "util/sysutil.h"
#include "util/throughput.h"
#include "util/pktqueue.h"
#include "util/service.h"
#include "util/fileutil.h"
#include "util/vidutil.h"
#include "util/lexparse.h"
#include "util/strutil.h"
#include "util/listnode.h"
#include "util/md5.h"
#include "util/sha1.h"
#include "util/hmac.h"
#include "util/blowfish.h"
#include "license/license.h"
#include "util/getmacbyarp.h"

#endif // __UTILS_H__
