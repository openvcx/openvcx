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


#ifndef __SERVER_DEVTYPE_H__
#define __SERVER_DEVTYPE_H__

#include "unixcompat.h"

#define DEVFILE_KEY_DEVICE                  "device"
#define DEVFILE_KEY_METHODS                 "methods"
#define DEVFILE_KEY_METHOD                  "method"
#define DEVFILE_KEY_TYPE                    "type"
#define DEVFILE_KEY_MATCH                   "match"

#define STREAM_DEVICE_TYPE_UNKNOWN_STR      "unknown"
#define STREAM_DEVICE_TYPE_UNSUPPORTED_STR  "unsupported"
#define STREAM_DEVICE_TYPE_MOBILE_STR       "mobile"
#define STREAM_DEVICE_TYPE_TABLET_STR       "tablet"
#define STREAM_DEVICE_TYPE_PC_STR           "pc"

#define STREAM_METHOD_HTTPLIVE_STR          "httplive"
#define STREAM_METHOD_DASH_STR              "dash"
#define STREAM_METHOD_FLVLIVE_STR           "flvlive"
#define STREAM_METHOD_MKVLIVE_STR           "mkvlive"
#define STREAM_METHOD_TSLIVE_STR            "tslive"
#define STREAM_METHOD_RTSP_STR              "rtsp"
#define STREAM_METHOD_RTSP_INTERLEAVED_STR  "rtspi"
#define STREAM_METHOD_RTSP_HTTP_STR         "rtspt"
#define STREAM_METHOD_RTMP_STR              "rtmp"
#define STREAM_METHOD_PROGDOWNLOAD_STR      "http"
#define STREAM_METHOD_FLASHHTTP_STR         "flashhttp"
#define STREAM_METHOD_UDP_RTP_STR           "rtp"
#define STREAM_METHOD_UDP_STR               "udp"

#define STREAM_DEV_NAME_MAX       32

typedef enum STREAM_METHOD {
  STREAM_METHOD_INVALID            = -1,
  STREAM_METHOD_UNKNOWN            = 0,
  STREAM_METHOD_NONE               = 1,
  STREAM_METHOD_HTTPLIVE           = 2,
  STREAM_METHOD_DASH               = 3,
  STREAM_METHOD_TSLIVE             = 4,
  STREAM_METHOD_FLVLIVE            = 5,
  STREAM_METHOD_MKVLIVE            = 6,
  STREAM_METHOD_PROGDOWNLOAD       = 7,
  STREAM_METHOD_FLASHHTTP          = 8, 
  STREAM_METHOD_RTSP               = 9,
  STREAM_METHOD_RTSP_INTERLEAVED   = 10,
  STREAM_METHOD_RTSP_HTTP          = 11,
  STREAM_METHOD_RTMP               = 12,
  STREAM_METHOD_UDP_RTP            = 13,
  STREAM_METHOD_UDP                = 14,
  STREAM_METHOD_MAX                = 15 
} STREAM_METHOD_T;

typedef enum STREAM_DEVICE_TYPE {
  STREAM_DEVICE_TYPE_UNSUPPORTED   = -1,
  STREAM_DEVICE_TYPE_UNKNOWN       = 0,
  STREAM_DEVICE_TYPE_PC            = 1,
  STREAM_DEVICE_TYPE_MOBILE        = 2,
  STREAM_DEVICE_TYPE_TABLET        = 3
} STREAM_DEVICE_TYPE_T;

#define STREAM_DEVICE_METHODS_MAX   4

typedef struct STREAM_DEVICE {
  char                     name[STREAM_DEV_NAME_MAX];
  char                     strmatch[64]; 
  char                     strmatch2[64]; 
  enum STREAM_METHOD       methods[STREAM_DEVICE_METHODS_MAX];
  enum STREAM_DEVICE_TYPE  devtype;
  struct STREAM_DEVICE    *pnext;
} STREAM_DEVICE_T;


int devtype_findmethod(const STREAM_DEVICE_T *pdev, STREAM_METHOD_T method);
const char *devtype_methodstr(enum STREAM_METHOD method);
STREAM_METHOD_T devtype_methodfromstr(const char *str);
char *devtype_dump_methods(int methodBits, char *buf, unsigned int szbuf);
const STREAM_DEVICE_T *devtype_finddevice(const char *userAgent, int matchany);
int devtype_loadcfg(const char *path);
int devtype_defaultcfg();







#endif // __SERVER_DEVTYPE_H__
