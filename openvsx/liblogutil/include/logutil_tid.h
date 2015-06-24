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

#ifndef __LOG_UTILTID_H__
#define __LOG_UTILTID_H__

#include "pthread_compat.h"

#define LOGUTIL_TAG_LENGTH    32

void logutil_tid_init();
void *logutil_tid_getContext();
int logutil_tid_setContext(void *pCtxt);
int logutil_tid_add(pthread_t tid, const char *tag);
int logutil_tid_remove(pthread_t tid);
const char *logutil_tid_lookup(pthread_t tid, int update);
void logutil_tid_dump();


#endif // __LOG_UTILTID_H__
