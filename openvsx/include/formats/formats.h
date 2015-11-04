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


#ifndef __FORMATS__H__
#define __FORMATS__H__

#if defined(WIN32)
#include "unixcompat.h"
#include "pthread_compat.h"
#else
#include "wincompat.h"
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#endif // WIN32


#include "vsxconfig.h"

#include "formats/filetypes.h"
#include "formats/flv.h"
#include "formats/ebml.h"
#include "formats/mkv_types.h"
#include "formats/mkv.h"
#include "formats/httplive.h"
#include "formats/http_client.h"
#include "formats/mpdpl.h"
#include "formats/mpd.h"
#include "formats/m3u.h"
#include "formats/metafile.h"
#include "formats/mp2pes.h"
#include "formats/mp2ts.h"
#include "formats/mp4boxes.h"
#include "formats/mp4.h"
#include "formats/mp4track.h"
#include "formats/mp4boxes.h"
#include "formats/mp4creator.h"
#include "formats/mp4extractor.h"
#include "formats/mp4_moof.h"
#include "formats/bmp.h"
#include "formats/image.h"
#include "formats/codecfmt.h"
#include "formats/http_parse.h"
#include "formats/rtmp_auth.h"
#include "formats/rtmp_tunnel.h"
#include "formats/rtmp_parse.h"
#include "formats/flv_write.h"
#include "formats/mkv_write.h"
#include "formats/rtmp_pkt.h"
#include "formats/sdp.h"
#include "formats/sdputil.h"
#include "formats/stun.h"
#include "formats/ice.h"
#include "formats/rtsp_parse.h"
#include "formats/filetype.h"



#endif // __FORMATS_H__
