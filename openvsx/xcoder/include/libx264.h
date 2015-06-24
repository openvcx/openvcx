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

#ifndef __X264_EXT_H__
#define __X264_EXT_H__

#ifdef ENABLE_XCODE

#include "libavcodec/avcodec.h"
#include "x264.h"

typedef struct X264Context {
#if LIBAVCODEC_VERSION_MAJOR >= 53
    AVClass        *class;
#endif
    x264_param_t    params;
    x264_t         *enc;
    x264_picture_t  pic;
    uint8_t        *sei;
    int             sei_size;
    AVFrame         out_pic;
    // Added encoder frame lookahead configuration
    int             lookaheadmin1;
    int             i_slice_max_size;
} X264Context;


#endif // ENABLE_XCODE

#endif // __X264_EXT_H__
