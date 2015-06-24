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


#ifndef __SERVER_PROP_H__
#define __SERVER_PROP_H__

#include "unixcompat.h"

#ifndef WIN32

#include <arpa/inet.h>

#endif // WIN32

#include "server/srvhttp.h"
//#include "stream/stream_rtp.h" // for STREAM_DEST_MAX

#ifdef WIN32

#define SRV_USER_PROP_FILE             "etc\\user.conf"

#else

#define SRV_USER_PROP_FILE             "etc/user.conf"

#endif // WIN32

#define SRV_PROP_DEST_MAX                3

typedef struct SRV_PROPS {
  char                 dstaddr[SRV_PROP_DEST_MAX][128];
  char                 capaddr[64];
  int                  captranstype;
  int                  loop;
  int                  loop_pl;
  int                  tslive;   // stream output available via HTTP
  int                  httplive;
  int                  xcodeprofile;
  int                  dirsort;
} SRV_PROPS_T;

int srvprop_write(const SRV_PROPS_T *pProps, const char *path);
int srvprop_read(SRV_PROPS_T *pProps, const char *path);




#endif // __SERVER_PROP_H__
