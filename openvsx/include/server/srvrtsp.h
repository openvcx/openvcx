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


#ifndef __SRV_RTSP_H__
#define __SRV_RTSP_H__

#include "formats/rtsp_parse.h"
#include "server/srvhttp.h"
#include "stream/stream_rtsptypes.h"



int rtsp_handle_conn(RTSP_REQ_CTXT_T *pRtsp);
int rtsp_addFrame(void *, const OUTFMT_FRAME_DATA_T *pFrame);
int rtsp_read_interleaved(NETIO_SOCK_T *pnetsock, HTTP_PARSE_CTXT_T *pHdrCtxt, 
                          unsigned char *buf, unsigned int szbuf, unsigned int tmtms);
int rtsp_handle_interleaved_msg(RTSP_REQ_CTXT_T *pRtsp, unsigned char *buf, unsigned int len);




#endif // __SRV_RTSP_H__
