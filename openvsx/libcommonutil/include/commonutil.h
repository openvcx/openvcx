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

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>

#include "logutil.h"

#define CHAR_PRINTABLE(c) ((c) >= 32 && (c) < 127)
#define CHAR_NUMERIC(c) ((c) >= '0' && (c) <= '9')

int avc_istextchar(const unsigned char c); // replacement for isascii
int avc_strip_nl(char *str, size_t sz, int stripws);
char *avc_dequote(const char *p, char *buf, unsigned int szbuf);
void *avc_calloc(size_t count, size_t size);
void *avc_realloc(void *porig, size_t size);
void avc_free(void **pp);
void avc_dumpHex(void *fp, const unsigned char *buf, unsigned int len, int ascii);
const char *avc_getPrintableDuration(unsigned long long duration, unsigned int timescale);
const unsigned char *avc_binstrstr(const unsigned char *buf,
                               unsigned int len,
                               const unsigned char *needle,
                               unsigned int lenneedle);
int avc_isnumeric(const char *s);



#endif // __COMMON_H__
