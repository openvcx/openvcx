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


#ifndef __STREAM__H__
#define __STREAM__H__


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

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
#include "../xcoder/include/mixer/mixer_api.h"
#endif //(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

#include "stream_srtp.h"
#include "stream_dtls.h"
#include "rtp.h"
#include "stream_abr.h"
#include "stream_net.h"
#include "stream_monitor.h"
#include "stream_fb.h"
#include "stream_rtcp.h"
#include "stream_stun.h"
#include "stream_turn.h"
#include "stream_outfmt.h"
#include "stream_rtp.h"
#include "stream_net_aac.h"
#include "stream_net_amr.h"
#include "stream_net_h264.h"
#include "stream_net_mpg4v.h"
#include "stream_net_vp8.h"
#include "stream_net_vorbis.h"
#include "stream_net_pes.h"
#include "stream_net_image.h"
#include "stream_net_vconference.h"
#include "stream_net_aconference.h"
#include "stream_net_av.h"
#include "stream_pktz.h"
#include "stream_pktz_mp2ts.h"
#include "stream_pktz_mpg4v.h"
#include "stream_pktz_h263.h"
#include "stream_pktz_h264.h"
#include "stream_pktz_vp8.h"
#include "stream_pktz_aac.h"
#include "stream_pktz_amr.h"
#include "stream_pktz_silk.h"
#include "stream_pktz_opus.h"
#include "stream_pktz_pcm.h"
#include "stream_pktz_frbuf.h"
#include "stream_av.h"
#include "stream_rtp_mp2tsraw.h"
#include "stream_rtsp.h"
#include "stream_pip.h"
#include "stream_pipconf.h"
#include "stream_piplayout.h"
#include "stream_xmit.h"
#include "streamer.h" // todo: deprecate
#include "streamer2.h"
#include "streamer_rtp.h"


#endif // __STREAM__H__
