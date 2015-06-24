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


#ifndef __STRUTIL_H__
#define __STRUTIL_H__

#include "capture/capture.h"
#include "stream/streamer_rtp.h"

typedef int (*STRUTIL_PARSE_CSV_TOKEN)(void *, const char *);
typedef int (* PARSE_PAIR_CB) (void *, const char *);
#define PARSE_PAIR_BUF_SZ     VSX_MAX_PATH_LEN

#define safe_strncpy(s1, s2, n) strncpy((s1), (s2) ? (s2) : "\0", (n))

STREAM_PROTO_T strutil_convertProtoStr(const char *protoStr);
int strutil_convertDstStr(const char *dstArg, char dstHost[], size_t szHost,
                         uint16_t dstPorts[], size_t numDstPorts, char dstUri[], size_t szUri);
int path_getLenNonPrefixPart(const char *path);
int path_getLenDirPart(const char *path);
int get_port_range(const char *str, uint16_t *pstart, uint16_t *pend);
int strutil_parseDimensions(const char *str, int *px, int *py);
int strutil_parse_csv(STRUTIL_PARSE_CSV_TOKEN cbToken, void *pCbTokenArg,
                      const char *line);

int strutil_parse_pair(const char *str, void *pParseCtxt, PARSE_PAIR_CB cbParseElement);
const char *strutil_skip_key(const char *p, unsigned int len);
int strutil_read_rgb(const char *str, unsigned char RGB[3]);
int64_t strutil_read_numeric(const char *s, int bAllowbps, int bytesInK, int dflt);
int strutil_isTextFile(const char *path);
const char *strutil_getFileExtension(const char *filename);
const char *strutil_getFileName(const char *path);


#endif // __STRUTIL_H__
