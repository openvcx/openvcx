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

#ifndef __FILE_TYPES_H__
#define __FILE_TYPES_H__


typedef enum XC_CODEC_TYPE {
  XC_CODEC_TYPE_UNKNOWN     = 0,

  XC_CODEC_TYPE_AAC         = 21,
  XC_CODEC_TYPE_AC3         = 20,
  XC_CODEC_TYPE_MPEGA_L2    = 22,
  XC_CODEC_TYPE_MPEGA_L3    = 23,
  XC_CODEC_TYPE_AMRNB       = 24,
  XC_CODEC_TYPE_G711_MULAW  = 90,
  XC_CODEC_TYPE_G711_ALAW   = 91,

  XC_CODEC_TYPE_MPEG4V      = 2,
  XC_CODEC_TYPE_H264        = 1,
  XC_CODEC_TYPE_H263        = 3,
  XC_CODEC_TYPE_H263_PLUS   = 4,
  XC_CODEC_TYPE_H262        = 5,
  XC_CODEC_TYPE_VP6         = 6,
  XC_CODEC_TYPE_VP6F        = 7,

  XC_CODEC_TYPE_BMP         = 51,
  XC_CODEC_TYPE_PNG         = 52,
  XC_CODEC_TYPE_JPG         = 53,

  XC_CODEC_TYPE_RAWV_BGRA32     = 0x0100,
  XC_CODEC_TYPE_RAWV_RGBA32     = 0x0101,
  XC_CODEC_TYPE_RAWV_BGR24      = 0x0102,
  XC_CODEC_TYPE_RAWV_RGB24      = 0x0103,
  XC_CODEC_TYPE_RAWV_BGR565     = 0x0104,
  XC_CODEC_TYPE_RAWV_RGB565     = 0x0105,
  XC_CODEC_TYPE_RAWV_YUV420P    = 0x0106, // planar YUV 4:2:0 12bpp
  XC_CODEC_TYPE_RAWV_YUVA420P   = 0x0107, // planar YUV 4:2:0 20bpp (8bit alpha mask)
  XC_CODEC_TYPE_RAWV_NV12       = 0x0108, // semi-planar YUV:4:2:0, UVUVUVUV
  XC_CODEC_TYPE_RAWV_NV21       = 0x0109, // semi-planar YUV:4:2:0, VUVUVUVU


  XC_CODEC_TYPE_RAWA_PCM16LE    = 0x0300, // signed 16bit PCM little endian
  XC_CODEC_TYPE_RAWA_PCMULAW    = 0x0301,
  XC_CODEC_TYPE_RAWA_PCMALAW    = 0x0302

} XC_CODEC_TYPE_T;



#endif // __FILE_TYPES_H__
