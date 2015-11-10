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


#ifndef __VSX_COMMON_H__
#define __VSX_COMMON_H__

#include "vsx_dbg.h"
#include "pcap_compat.h"

#if defined(WIN32)
#include <windows.h>
typedef uint32_t in_addr_t;
#endif // WIN32


#define IN_LOCALHOST(i)     (((u_int32_t)(i) & 0xff000000) == 0x7f000000)

#if defined(WIN32) && !defined(__MINGW32__)

#include <sys/stat.h>
#include <sys/types.h>
//#include <windows.h>
#include "unixcompat.h"
#include "pthread_compat.h"

#define ERRNO_FMT_STR      "error: %d" 
#define ERRNO_FMT_ARGS     WSAGetLastError() 

//
// netinet/in.h
//
#define IN_MULTICAST(i)     (((u_int32_t)(i) & 0xf0000000) == 0xe0000000)
#define INADDR_LOOPBACK      ((u_int32_t)0x7f000001)


#elif !defined(WIN32) && defined(__MINGW32__)

#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

#else // WIN32

#include <unistd.h>
#if !defined(__MINGW32__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#endif // __MINGW32__
#include <pthread.h>
#include <limits.h>
//#include <ctype.h>
#include "wincompat.h"

#define ERRNO_FMT_STR      "(errno: %d) %s" 
#define ERRNO_FMT_ARGS     errno, strerror(errno)

#endif // WIN32

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#define VSX_MAX_PATH_LEN                  512


#include "vsxconfig.h"

#include "ixcode_pip.h"
#include "vsxlib.h"

#include "logutil.h"
#include "commonutil.h"
#include "fileops.h"
#include "timers.h"
#include "version.h"
#include "license/license.h"
#include "util/util.h"
#include "codecs/codecs.h"
#include "formats/sdp.h"
#include "xcode/xcode_types.h"
#include "server/server.h"
#include "capture/capture.h"
#include "stream/stream.h"
#include "formats/formats.h"
#include "xcode/xcode.h"



#define AVC_RC_STR(rc)     ((rc) >= 0 ? "ok" : "error")
#define MAX(x,y)  ((x) > (y) ? (x) : (y))
#define MIN(x,y)  ((x) < (y) ? (x) : (y))

#define ABS(x)    (x < 0 ? (-1 * (x)) : (x))

#define LE32(x)   (x)
#define LE16(x)   (x)
#define BE32(x)   (htonl(x))
#define BE16(x)   (htons(x))

#define TOUPPER(c) (((c) >= 'a' && (c) <= 'z') ? (c - 32) : (c))
#define TOLOWER(c) (((c) >= 'A' && (c) <= 'Z') ? (c + 32) : (c))

#define PTS_HZ_SEC  90000
#define PTS_HZ_SECF 90000.0f
#define PTS_HZ_MS   90
#define PTS_HZ_MSF  90.0f
#define PTSF(pts)  ((double)(pts)/PTS_HZ_SECF)
#define PTSF_SEC(pts)  PTSF(pts)
#define PTSF_MS(pts)  ((double)(pts)/PTS_HZ_MSF)

#define PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED 1

extern int g_proc_exit;
extern int g_proc_exit_flags;
extern int g_verbosity;
extern const char *g_http_log_path;
extern TIME_VAL g_debug_ts;
extern int g_debug_flags;
extern unsigned int g_thread_stack_size;
extern const char *g_client_ssl_method;
extern const char *vsxlib_get_appnamestr( char *buf, unsigned int len);
extern const char *vsxlib_get_appnamewwwstr(char *buf, unsigned int len);
extern const char *vsxlib_get_useragentstr(char *buf, unsigned int len);

#if defined(VSX_HAVE_SSL)
extern pthread_mutex_t g_ssl_mtx;
#endif // VSX_HAVE_SSL

#if defined(__linux__) 
// On some systems strcasestr may just return an inappropriate pointer to the beginning of the hay stack!
#undef strcasestr
#define strcasestr avc_strcasestr
char *avc_strcasestr(const char *s, const char *find);
#endif // __linux__


#define PHTREAD_INIT_ATTR(pattr)  pthread_attr_init((pattr)); \
                                  pthread_attr_setdetachstate((pattr), PTHREAD_CREATE_DETACHED); \
                                  if(g_thread_stack_size > 0) { \
                                    pthread_attr_setstacksize((pattr), g_thread_stack_size); }



#endif // __VSX_COMMON_H__
