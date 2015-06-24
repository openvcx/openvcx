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

#ifndef __CODEC_OPUS_H__
#define __CODEC_OPUS_H__

#include "xcoder_wrap.h"


#if defined(__APPLE__)
#define LIBOPUS_NAME        "libopus.0.dylib"
#else
#define LIBOPUS_NAME        "libopus.so"
#endif // __APPLE__

#define LIBOPUS_PATH        "lib/"LIBOPUS_NAME

void xcoder_opus_close_decoder(CODEC_WRAP_CTXT_T *pCtxt);
void xcoder_opus_close_encoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_opus_init_decoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_opus_init_encoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_opus_encode(CODEC_WRAP_CTXT_T *pCtxt,
                                   unsigned char *out_packets,
                                   unsigned int out_sz,
                                   const int16_t *samples,
                                   int64_t *pts);
int xcoder_opus_decode(CODEC_WRAP_CTXT_T *pCtxt, const unsigned char *, unsigned int, int16_t *,
                                   unsigned int);


#endif // __CODEC_OPUS_H__
