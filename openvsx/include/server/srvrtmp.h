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


#ifndef __SRV_RTMP_H__
#define __SRV_RTMP_H__

#include "formats/rtmp_parse.h"

int rtmp_init(RTMP_CTXT_T *pRtmp, unsigned int vidTmpFrameSz);
void rtmp_close(RTMP_CTXT_T *pRtmp);
int rtmp_handle_conn(RTMP_CTXT_T *pRtmp);
int rtmp_addFrame(void *, const OUTFMT_FRAME_DATA_T *pFrame);




#endif // __SRV_RTMP_H__
