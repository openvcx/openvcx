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

#ifndef __LOG_UTIL_H__
#define __LOG_UTIL_H__

#include "logutil_tid.h"

#define S_DEBUG_VVERBOSE 7
#define S_DEBUG_VERBOSE  6
#define S_DEBUG          5
#define S_INFO           4
#define S_WARNING        3
#define S_ERROR          2
#define S_CRITICAL       1


#define LOGGER_MAX_FILES                  5
#define LOGGER_MAX_FILE_SZ                1048576

#define LOG_FLAG_USESTDERR                  0x01
#define LOG_FLAG_USEFILEOUTPUT              0x02
#define LOG_FLAG_FLUSHOUTPUT                0x04
#define LOG_FLAG_USELOCKING                 0x08
#define LOG_FLAG_USESYSLOG                  0x10
#define LOG_FLAG_TRUNCATEFILE               0x20
#define LOG_FLAG_DEFAULT    (LOG_FLAG_USEFILEOUTPUT | LOG_FLAG_FLUSHOUTPUT | LOG_FLAG_USELOCKING)
#define LOG_FLAG_SYSLOG     (LOG_FLAG_USESYSLOG | LOG_FLAG_USELOCKING)

#define LOG_OUTPUT_PRINT_DATE               0x0100
#define LOG_OUTPUT_PRINT_SEV                0x0200
#define LOG_OUTPUT_PRINT_SEV_WARNING        0x0400
#define LOG_OUTPUT_PRINT_PID                0x0800
#define LOG_OUTPUT_PRINT_TID                0x1000
#define LOG_OUTPUT_PRINT_DATE_STDERR        0x2000
#define LOG_OUTPUT_PRINT_TAG                0x4000
#define LOG_OUTPUT_DEFAULT (LOG_OUTPUT_PRINT_DATE | LOG_OUTPUT_PRINT_SEV)
#define LOG_OUTPUT_DEFAULT_STDERR           0 

#if defined(ANDROID_LOG)

#include <android/log.h>

#define logger_Init(path, name, maxf, maxsz, flags)
#define logger_SetFile(path, name, maxf, maxsz, flags)
#define logger_SetLevel(x)
#define logger_AddStderr(x, x2)
#define LOG_TAG "vsx"
#define LOG __android_log_print
#define X_CRITICAL(fmt) ANDROID_LOG_FATAL, LOG_TAG, fmt"\n"
#define X_ERROR(fmt) ANDROID_LOG_ERROR, LOG_TAG, fmt"\n"
#define X_WARNING(fmt) ANDROID_LOG_WARN, LOG_TAG, fmt"\n"
#define X_INFO(fmt) ANDROID_LOG_INFO, LOG_TAG, fmt"\n"
#define X_DEBUG(fmt) ANDROID_LOG_DEBUG, LOG_TAG, fmt"\n"
#define X_DEBUGV(fmt) ANDROID_LOG_VERBOSE, LOG_TAG,fmt"\n"

#else


// The following macros are for backward compatibility
#define SET_LOG_LEVEL         logger_SetLevel
#define ADD_STDERR_LOGGING    logger_AddStderr

#ifdef LEGACY_LOGGING

#include <syslog.h>

/*
#define INIT_LOGGER(psv, pcode, pfile, pid) \
          logger_SetLevel(*psv); \
          if(*psv < 0) logger_AddStderr(); \
          logger_InitSyslog(pcode ? pcode : "", LOG_PID, LOG_LOCAL0);
*/

#else // LEGACY_LOGGING

#define INIT_LOGGER(name)     logger_Init("log", name, LOGGER_MAX_FILES, LOGGER_MAX_FILE_SZ, LOG_OUTPUT_DEFAULT)

#endif // LEGACY_LOGGING



int logger_Init(const char *filePath,
                const char *fileName,
                unsigned int maxFilesNum,
                unsigned int maxFileSz,
                unsigned int outputFlags);

int logger_InitSyslog(const char *ident, int syslogOpt, int syslogFacility);
void *logger_getContext();
int logger_setContext(void *pCtxt);

int logger_SetLevel(int sev);
void logger_AddStderr(int sev, int output_flags);
int logger_SetFile(const char *filePath,
                   const char *fileName,
                   unsigned int maxFilesNum,
                   unsigned int maxFileSz,
                   unsigned int outputFlags);
      
void logger_Log(int sev, const char *msg, ...);
void logger_LogHex(int sev, const void *buf, unsigned int len, int printAscii);
int logger_GetLogLevel();
int logger_SetHistory(unsigned int maxFiles, unsigned int maxFileBytes);




#define LOG logger_Log

#define LOG_FULL_FMT(fmt) "("__FILE__":%d) " fmt"\n", __LINE__

// These expansions are here because I cannot get a variable length macro
// to work in win32 SDK
#define X_CRITICALF(fmt) S_CRITICAL, LOG_FULL_FMT(fmt)
#define X_ERRORF(fmt) S_ERROR, LOG_FULL_FMT(fmt)
#define X_WARNINGF(fmt) S_WARNING, LOG_FULL_FMT(fmt)
#define X_INFOF(fmt) S_INFO, LOG_FULL_FMT(fmt)
#define X_DEBUGF(fmt) S_DEBUG, LOG_FULL_FMT(fmt)
#define X_DEBUGVF(fmt) S_WARNING, LOG_FULL_FMT(fmt)

#define X_CRITICAL(fmt) S_CRITICAL, fmt"\n"
#define X_ERROR(fmt) S_ERROR, fmt"\n"
#define X_WARNING(fmt) S_WARNING, fmt"\n"
#define X_INFO(fmt) S_INFO, fmt"\n"
#define X_DEBUG(fmt) S_DEBUG, fmt"\n"
#define X_DEBUGV(fmt) S_DEBUG_VERBOSE, fmt"\n"
#define X_DEBUGVV(fmt) S_DEBUG_VVERBOSE, fmt"\n"


#define LOGHEX_CRITICAL(data, len) logger_LogHex(S_CRITICAL, data, len, 0)
#define LOGHEXT_CRITICAL(data, len) logger_LogHex(S_CRITICAL, data, len, 1)
#define LOGHEX_ERROR(data, len) logger_LogHex(S_ERROR, data, len, 0)
#define LOGHEXT_ERROR(data, len) logger_LogHex(S_ERROR, data, len, 1)
#define LOGHEX_WARNING(data, len) logger_LogHex(S_WARNING, data, len, 0)
#define LOGHEXT_WARNING(data, len) logger_LogHex(S_WARNING, data, len, 1)
#define LOGHEX_INFO(data, len) logger_LogHex(S_INFO, data, len, 0)
#define LOGHEXT_INFO(data, len) logger_LogHex(S_INFO, data, len, 1)
#define LOGHEX_DEBUG(data, len) logger_LogHex(S_DEBUG, data, len, 0)
#define LOGHEXT_DEBUG(data, len) logger_LogHex(S_DEBUG, data, len, 1)
#define LOGHEX_DEBUGV(data, len) logger_LogHex(S_DEBUG_VERBOSE, data, len, 0)
#define LOGHEXT_DEBUGV(data, len) logger_LogHex(S_DEBUG_VERBOSE, data, len, 1)
#define LOGHEX_DEBUGVV(data, len) logger_LogHex(S_DEBUG_VVERBOSE, data, len, 0)
#define LOGHEXT_DEBUGVV(data, len) logger_LogHex(S_DEBUG_VVERBOSE, data, len, 1)

#ifndef WIN32

#define LOG_MSG(sv, fmt, args...) \
                if(sv == S_INFO) \
                  LOG(sv, fmt"\n", ##args); \
                else \
                  LOG(sv, "("__FILE__":%d) "fmt"\n", __LINE__, ## args)

#endif // WIN32


#endif // ANDROID

#endif // __LOG_UTIL_H__
