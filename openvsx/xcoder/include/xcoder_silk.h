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

#ifndef __CODEC_SILK_H__
#define __CODEC_SILK_H__

#include "xcoder_wrap.h"

#define LIBSILK_NAME        "libSKP_SILK_SDK.so"
#define LIBSILK_PATH        "lib/"LIBSILK_NAME

void xcoder_silk_close_decoder(CODEC_WRAP_CTXT_T *pCtxt);
void xcoder_silk_close_encoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_silk_init_decoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_silk_init_encoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_silk_encode(CODEC_WRAP_CTXT_T *pCtxt,
                                   unsigned char *out_packets,
                                   unsigned int out_sz,
                                   const int16_t *samples,
                                   int64_t *pts);
int xcoder_silk_decode(CODEC_WRAP_CTXT_T *pCtxt, const unsigned char *, unsigned int, int16_t *,
                                   unsigned int);


#endif // __CODEC_SILK_H__
