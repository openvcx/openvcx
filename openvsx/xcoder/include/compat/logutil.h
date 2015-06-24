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

#ifndef __XLOGGER_H__
#define __XLOGGER_H__


#if defined(ANDROID_LOG)

#include <android/log.h>

#define logger_Init(path, name, maxf, maxsz, flags)
#define logger_SetFile(path, name, maxf, maxsz, flags)
#define logger_SetLevel(x)
#define logger_AddStderr(x, y)
#define LOG_TAG ""
#define LOG __android_log_print
#define X_CRITICAL(fmt) ANDROID_LOG_FATAL, LOG_TAG, fmt"\n"
#define X_ERROR(fmt) ANDROID_LOG_ERROR, LOG_TAG, fmt"\n"
#define X_WARNING(fmt) ANDROID_LOG_WARN, LOG_TAG, fmt"\n"
#define X_INFO(fmt) ANDROID_LOG_INFO, LOG_TAG, fmt"\n"
#define X_DEBUG(fmt) ANDROID_LOG_DEBUG, LOG_TAG, fmt"\n"
#define X_DEBUGV(fmt) ANDROID_LOG_VERBOSE, LOG_TAG,fmt"\n"

#else


#define LOG logmsg
#define logger_Init(path, name, maxf, maxsz, flags)
#define logger_SetLevel(x)
#define logger_AddStderr(x, y)
#define logger_SetFile(path, name, maxf, maxsz, flags)
#define LOG_FLAG_USESTDERR 1
#define LOG_FLAG_FLUSHOUTPUT 1
#define LOGGER_MAX_FILES 1
#define LOGGER_MAX_FILE_SZ 1
#define LOG_OUTPUT_DEFAULT 1
#define LOG_FLAG_USELOCKING 1

#define S_DEBUG 1

#define X_CRITICAL(fmt) 1, fmt"\n"
#define X_ERROR(fmt) 2, fmt"\n"
#define X_WARNING(fmt) 3, fmt"\n"
#define X_INFO(fmt) 4, fmt"\n"
#define X_DEBUG(fmt) 5, fmt"\n"
#define X_DEBUGV(fmt) 6, fmt"\n"

void logmsg(int sev, const char *fmt, ...);

#endif // ANDROID_LOG


#endif // __XLOGGER_H__
