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

#ifndef __CODEC_FAAC_H__
#define __CODEC_FAAC_H__

#include "xcoder_wrap.h"

#define LIBFAAC_NAME        "libfaac.so"
#define LIBFAAC_PATH        "lib/"LIBFAAC_NAME

void xcoder_faac_close_encoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_faac_init_encoder(CODEC_WRAP_CTXT_T *pCtxt);
int xcoder_faac_encode(CODEC_WRAP_CTXT_T *pCtxt,
                                   unsigned char *out_packets,
                                   unsigned int out_sz,
                                   const int16_t *samples,
                                   int64_t *pts);


#endif // __CODEC_FAAC_H__
