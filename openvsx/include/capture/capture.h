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



#ifndef __CAPTURE__H__
#define __CAPTURE__H__

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

#include "capture_fbarray.h"
#include "capture_abr.h"
#include "capture_types.h"
#include "capture_nettypes.h"
#include "capture_sockettypes.h"
#include "capture_strutil.h"
#include "capture_rtsp.h"
#include "capture_rtcp.h"
#include "capture_rtp.h"
#include "capture_tcp.h"
#include "capture_rtmp.h"
#include "capture_socket.h"
#include "capture_dummy.h"
#include "capture_pkt_rtmp.h"
#include "capture_appsp.h"
#include "capture_cbhandler.h"
#include "capture_filter.h"
#include "capture_pkthandler.h"
#include "capture_pcap.h"
#include "capture_file.h"
#include "capture_net.h"
#include "capture_pkt_aac.h"
#include "capture_pkt_amr.h"
#include "capture_pkt_pcm.h"
#include "capture_pkt_silk.h"
#include "capture_pkt_opus.h"
#include "capture_pkt_h264.h"
#include "capture_pkt_h263.h"
#include "capture_pkt_mpg4v.h"
#include "capture_pkt_vp8.h"
#include "capture_pkt_rawdev.h"
#include "capture_mp2ts.h"
#include "capture_httpget.h"
#include "capture_httpmp4.h"
#include "capture_dev.h"

#endif // __CAPTURE_H__
