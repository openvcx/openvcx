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

#ifndef __CODEC_X264_H__
#define __CODEC_X264_H__

#include "xcoder_wrap.h"

#define LIBX264_NAME        "libx264.so"
#define LIBX264_PATH        "lib/"LIBX264_NAME


void xcoder_x264_close(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_x264_init(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx);
int xcoder_x264_encode(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx,
                             const struct AVFrame *raw, unsigned char *out, unsigned int outlen);



#endif // __CODEC_X264_H__
