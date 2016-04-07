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


#ifndef __VSX_DBG_H__
#define __VSX_DBG_H__


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
#define VSX_DEBUG_FLAG_PROXY            0x00800000

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
#define VSX_DEBUG_PROXY(x)      if((g_debug_flags & VSX_DEBUG_FLAG_PROXY)) { x; }

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



extern int g_verbosity;
extern int g_debug_flags;


#endif // __VSX_DBG_H__
