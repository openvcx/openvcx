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


#ifndef __FVL_WRITE_H__
#define __FLV_WRITE_H__

#include <stdio.h>
#include "util/bits.h"
#include "formats/rtmp_parse.h"


void flv_write_int24(BYTE_STREAM_T *bs, uint32_t val);
int flv_write_val_number(BYTE_STREAM_T *bs, double dbl);
int flv_write_string(BYTE_STREAM_T *bs, const char *str);
int flv_write_key_val_string(BYTE_STREAM_T *bs, const char *key, const char *val);
int flv_write_key_val(BYTE_STREAM_T *bs, const char *str, double dbl, int amfType);
int flv_write_objend(BYTE_STREAM_T *bs);
int flv_write_taghdr(BYTE_STREAM_T *bs, uint8_t tagType, uint32_t sz, uint32_t timestamp32);


int flv_write_filebody(FLV_BYTE_STREAM_T *fbs, int audio, int video);
int flv_write_onmeta(const CODEC_AV_CTXT_T *pAvCtxt, FLV_BYTE_STREAM_T *fbs);
int flv_write_vidstart(FLV_BYTE_STREAM_T *fbs, CODEC_VID_CTXT_T *pV);
int flv_write_audstart(FLV_BYTE_STREAM_T *fbs, CODEC_AUD_CTXT_T *pA);


#endif // __FLV_WRITE_H__
