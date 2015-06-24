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


#ifndef __CAPTURE_RTCP_H__
#define __CAPTURE_RTCP_H__


void capture_rtcprr_reset(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc, uint32_t localSSRC);
int capture_rtcp_create(STREAM_RTCP_RR_T *pRtcp, int do_rr, int do_fir, int do_pli, int do_nack,
                        int do_remb, unsigned char *pBuf, unsigned int lenbuf);
int capture_rtcp_set_local_ssrc(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc);
CAPTURE_STREAM_T *capture_rtcp_lookup(const CAPTURE_STATE_T *pState, 
                                      const COLLECT_STREAM_KEY_T *pKey);



#endif //__CAPTURE_RTCP_H__
