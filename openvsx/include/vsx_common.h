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


#define VSX_DEBUG(x) if(g_verbosity > VSX_VERBOSITY_HIGH) { x; }
#define VSX_DEBUG2(x) if(g_verbosity > VSX_VERBOSITY_VERYHIGH) { x; }
#define VSX_DEBUG3(x) if(g_verbosity > VSX_VERBOSITY_HIGHEST) { x; }

#define VSX_DEBUG_FLAG_NET              0x00000001
#define VSX_DEBUG_FLAG_STREAMAV         0x00000002
#define VSX_DEBUG_FLAG_MKV              0x00000004
#define VSX_DEBUG_FLAG_RTSP             0x00000008
#define VSX_DEBUG_FLAG_RTMP             0x00000010
#define VSX_DEBUG_FLAG_RTMPT            0x00000020
#define VSX_DEBUG_FLAG_HTTP             0x00000040
#define VSX_DEBUG_FLAG_DASH             0x00000080
#define VSX_DEBUG_FLAG_AUTH             0x00000100
#define VSX_DEBUG_FLAG_RTP              0x00000200
#define VSX_DEBUG_FLAG_RTCP             0x00000400
#define VSX_DEBUG_FLAG_SSL              0x00000800
#define VSX_DEBUG_FLAG_SRTP             0x00001000
#define VSX_DEBUG_FLAG_DTLS             0x00002000
#define VSX_DEBUG_FLAG_REMB             0x00004000
#define VSX_DEBUG_FLAG_OUTFMT           0x00008000
#define VSX_DEBUG_FLAG_LIVE             0x00010000
#define VSX_DEBUG_FLAG_XCODE            0x00020000
#define VSX_DEBUG_FLAG_METAFILE         0x00040000
#define VSX_DEBUG_FLAG_MGR              0x00080000
#define VSX_DEBUG_FLAG_INFRAME          0x00100000
#define VSX_DEBUG_FLAG_CODEC            0x00200000
#define VSX_DEBUG_FLAG_MEM              0x00400000

#define VSX_DEBUG_NET(x)        if((g_debug_flags & VSX_DEBUG_FLAG_NET)) { x; }
#define VSX_DEBUG_STREAMAV(x)   if((g_debug_flags & VSX_DEBUG_FLAG_STREAMAV)) { x; }
#define VSX_DEBUG_MKV(x)        if((g_debug_flags & VSX_DEBUG_FLAG_MKV)) { x; }
#define VSX_DEBUG_RTSP(x)       if((g_debug_flags & VSX_DEBUG_FLAG_RTSP)) { x; }
#define VSX_DEBUG_RTMP(x)       if((g_debug_flags & VSX_DEBUG_FLAG_RTMP)) { x; }
#define VSX_DEBUG_RTMPT(x)      if((g_debug_flags & VSX_DEBUG_FLAG_RTMPT)) { x; }
#define VSX_DEBUG_HTTP(x)       if((g_debug_flags & VSX_DEBUG_FLAG_HTTP)) { x; }
#define VSX_DEBUG_DASH(x)       if((g_debug_flags & VSX_DEBUG_FLAG_DASH)) { x; }
#define VSX_DEBUG_AUTH(x)       if((g_debug_flags & VSX_DEBUG_FLAG_AUTH)) { x; }
#define VSX_DEBUG_RTP(x)        if((g_debug_flags & VSX_DEBUG_FLAG_RTP)) { x; }
#define VSX_DEBUG_RTCP(x)       if((g_debug_flags & VSX_DEBUG_FLAG_RTCP)) { x; }
#define VSX_DEBUG_SSL(x)        if((g_debug_flags & VSX_DEBUG_FLAG_SSL)) { x; }
#define VSX_DEBUG_SRTP(x)       if((g_debug_flags & VSX_DEBUG_FLAG_SRTP)) { x; }
#define VSX_DEBUG_DTLS(x)       if((g_debug_flags & VSX_DEBUG_FLAG_DTLS)) { x; }
#define VSX_DEBUG_REMB(x)       if((g_debug_flags & VSX_DEBUG_FLAG_REMB)) { x; }
#define VSX_DEBUG_OUTFMT(x)     if((g_debug_flags & VSX_DEBUG_FLAG_OUTFMT)) { x; }
#define VSX_DEBUG_LIVE(x)       if((g_debug_flags & VSX_DEBUG_FLAG_LIVE)) { x; }
#define VSX_DEBUG_XCODE(x)      if((g_debug_flags & VSX_DEBUG_FLAG_XCODE)) { x; }
#define VSX_DEBUG_METAFILE(x)   if((g_debug_flags & VSX_DEBUG_FLAG_METAFILE)) { x; }
#define VSX_DEBUG_MGR(x)        if((g_debug_flags & VSX_DEBUG_FLAG_MGR)) { x; }
#define VSX_DEBUG_INFRAME(x)    if((g_debug_flags & VSX_DEBUG_FLAG_INFRAME)) { x; }
#define VSX_DEBUG_CODEC(x)      if((g_debug_flags & VSX_DEBUG_FLAG_CODEC)) { x; }
#define VSX_DEBUG_MEM(x)        if((g_debug_flags & VSX_DEBUG_FLAG_MEM)) { x; }

#if defined(ANDROID_LOG)

//
// The following log macros are deprecated infavor of VSX_DEBUG_<function>
//
#define VSX_DEBUGLOG(x...) if(g_verbosity > VSX_VERBOSITY_HIGH) { \
                       __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, ##x ); }
#define VSX_DEBUGLOG2(x...) if(g_verbosity > VSX_VERBOSITY_VERYHIGH) { \
                       __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, ##x ); }
#define VSX_DEBUGLOG3(x...) if(g_verbosity > VSX_VERBOSITY_HIGHEST) { \
                       __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, ##x ); }

#elif defined(WIN32)

#define VSX_DEBUGLOG(x)
#define VSX_DEBUGLOG2(x)
#define VSX_DEBUGLOG3(x)

#else

#define VSX_DEBUGLOG_TIME g_debug_ts = timer_GetTime(); \
                           fprintf(stderr, "%"LL64"u.%"LL64"u ", g_debug_ts/TIME_VAL_US, g_debug_ts%TIME_VAL_US); 

//
// The following log macros are deprecated infavor of VSX_DEBUG_<function>
//
#define VSX_LOG(x...) if(g_verbosity > VSX_VERBOSITY_NORMAL) { \
                          VSX_DEBUGLOG_TIME \
                          fprintf(stderr, ##x  ); }
#define VSX_DEBUGLOG(x...) if(g_verbosity > VSX_VERBOSITY_HIGH) { \
                           VSX_DEBUGLOG_TIME \
                           fprintf(stderr, ##x  ); }
#define VSX_DEBUGLOG2(x...) if(g_verbosity > VSX_VERBOSITY_VERYHIGH) { \
                           VSX_DEBUGLOG_TIME \
                           fprintf(stderr, ##x  ); }
#define VSX_DEBUGLOG3(x...) if(g_verbosity > VSX_VERBOSITY_HIGHEST) { \
                           VSX_DEBUGLOG_TIME \
                           fprintf(stderr, ##x  ); }

#endif // ANDROID_LOG


#define PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED 1

extern int g_proc_exit;
extern int g_proc_exit_flags;
extern int g_verbosity;
extern const char *g_http_log_path;
extern TIME_VAL g_debug_ts;
extern int g_debug_flags;
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




#endif // __VSX_COMMON_H__
