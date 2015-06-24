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


#ifndef __STREAM_XMIT__H__
#define __STREAM_XMIT__H__

int streamxmit_init(STREAM_RTP_DEST_T *pDest, const STREAM_DEST_CFG_T *pDestCfg);
int streamxmit_close(STREAM_RTP_DEST_T *pDest, int lock);
int streamxmit_async_stop(STREAM_RTP_MULTI_T *pRtp, int lock);
int streamxmit_sendto(struct STREAM_RTP_DEST *pDest, const unsigned char *data, unsigned int len, int rtcp, int keyframe, int drop);
int streamxmit_onRTCPNACK(STREAM_RTP_DEST_T *pDest, const RTCP_PKT_RTPFB_NACK_T *pHdr);

int streamxmit_sendpkt(struct STREAM_XMIT_NODE *pStream);
int streamxmit_writepkt(struct STREAM_XMIT_NODE *pStream);


#endif // __STREAM_XMIT__H__
