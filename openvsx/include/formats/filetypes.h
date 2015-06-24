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


typedef enum MEDIA_FILE_TYPE {
  MEDIA_FILE_TYPE_UNKNOWN       = 0,

  //
  // video file types
  //
  MEDIA_FILE_TYPE_H264b         = 1,   // H.264 / MPEG-4/10 video
  MEDIA_FILE_TYPE_MP4V          = 2,   // MPEG-4/2 video
  MEDIA_FILE_TYPE_H263          = 3,   // H.263 video
  MEDIA_FILE_TYPE_H263_PLUS     = 4,   // H.263+ video
  MEDIA_FILE_TYPE_H262          = 5,   // H.262 / MPEG-2 video
  MEDIA_FILE_TYPE_VP6           = 6,   // On2 VP-6
  MEDIA_FILE_TYPE_VP6F          = 7,   // On2 VP-6 Flash
  MEDIA_FILE_TYPE_VP8           = 8,   // VP-8 
  MEDIA_FILE_TYPE_VID_GENERIC   = 101,  
  MEDIA_FILE_TYPE_VID_CONFERENCE = 102,

  //MEDIA_FILE_TYPE_VID_PASSTHRU  = 102,  

  //
  // audio file types
  //
  MEDIA_FILE_TYPE_AC3           = 20,    // A-52, AC-3, Dolby Digital Audio
  MEDIA_FILE_TYPE_AAC_ADTS      = 21,    // AAC
  MEDIA_FILE_TYPE_MPA_L2        = 22,    // MPEG-1/2 Audio Layer II (MP2) 
                                        // ISO/IEC 1172-3
  MEDIA_FILE_TYPE_MPA_L3        = 23,    // MPEG-1/2 Audio Layer III (MP3)
                                        // ISO/IEC 13818-3
  MEDIA_FILE_TYPE_AMRNB         = 24,    // AMR-NB 8KHz
  MEDIA_FILE_TYPE_VORBIS        = 25,    // VORBIS
  MEDIA_FILE_TYPE_SILK          = 26,    // Skype Audio
  MEDIA_FILE_TYPE_OPUS          = 27,    // OPUS 
  MEDIA_FILE_TYPE_AUD_GENERIC   = 121,  
  MEDIA_FILE_TYPE_AUD_CONFERENCE = 122, 

  //
  // Audio / Video container types
  //
  MEDIA_FILE_TYPE_MPEG2         = 40,   // MPEG-2 Program Stream
  MEDIA_FILE_TYPE_MP2TS         = 41,   // MPEG-2 Transport Stream
  MEDIA_FILE_TYPE_MP2TS_BDAV    = 42,   // MPEG-2 TS Blue Ray Disc format
  MEDIA_FILE_TYPE_MP4           = 43,   // mp4/mov/3gp format
  MEDIA_FILE_TYPE_FLV           = 44,   // Flash video format 
  MEDIA_FILE_TYPE_MKV           = 45,   // matroska 
  MEDIA_FILE_TYPE_WEBM          = 46,   // webm (matroska w/ vp8 & vorbis)
  MEDIA_FILE_TYPE_RTMPDUMP      = 47,   // raw rtmp bytestream format
  MEDIA_FILE_TYPE_RAWDUMP       = 48,   // raw generic bytestream format
  MEDIA_FILE_TYPE_M3U           = 49,   // m3u playlist format
  MEDIA_FILE_TYPE_SDP           = 50,   // Session Description format
  MEDIA_FILE_TYPE_META          = 51,   // Meta description file
  MEDIA_FILE_TYPE_DASHMPD       = 52,   // DASH MPD XML doc

  //
  // Audio container types
  //
  MEDIA_FILE_TYPE_WAV           = 60,

  //
  // Image container file types
  //
  MEDIA_FILE_TYPE_BMP           = 71,
  MEDIA_FILE_TYPE_PNG           = 72, 
  MEDIA_FILE_TYPE_JPG           = 73,

} MEDIA_FILE_TYPE_T;

typedef enum XC_CODEC_TYPE {

  XC_CODEC_TYPE_UNKNOWN         = MEDIA_FILE_TYPE_UNKNOWN,

  XC_CODEC_TYPE_AAC             = MEDIA_FILE_TYPE_AAC_ADTS,
  XC_CODEC_TYPE_AC3             = MEDIA_FILE_TYPE_AC3,
  XC_CODEC_TYPE_MPEGA_L2        = MEDIA_FILE_TYPE_MPA_L2,
  XC_CODEC_TYPE_MPEGA_L3        = MEDIA_FILE_TYPE_MPA_L3,
  XC_CODEC_TYPE_AMRNB           = MEDIA_FILE_TYPE_AMRNB,
  XC_CODEC_TYPE_VORBIS          = MEDIA_FILE_TYPE_VORBIS,
  XC_CODEC_TYPE_SILK            = MEDIA_FILE_TYPE_SILK,
  XC_CODEC_TYPE_OPUS            = MEDIA_FILE_TYPE_OPUS,
  XC_CODEC_TYPE_AUD_GENERIC     = MEDIA_FILE_TYPE_AUD_GENERIC,
  XC_CODEC_TYPE_AUD_CONFERENCE  = MEDIA_FILE_TYPE_AUD_CONFERENCE, 

  XC_CODEC_TYPE_MPEG4V          = MEDIA_FILE_TYPE_MP4V,
  XC_CODEC_TYPE_H264            = MEDIA_FILE_TYPE_H264b,
  XC_CODEC_TYPE_H263            = MEDIA_FILE_TYPE_H263,
  XC_CODEC_TYPE_H263_PLUS       = MEDIA_FILE_TYPE_H263_PLUS,
  XC_CODEC_TYPE_H262            = MEDIA_FILE_TYPE_H262,
  XC_CODEC_TYPE_VP6             = MEDIA_FILE_TYPE_VP6,
  XC_CODEC_TYPE_VP6F            = MEDIA_FILE_TYPE_VP6F,
  XC_CODEC_TYPE_VP8             = MEDIA_FILE_TYPE_VP8,
  XC_CODEC_TYPE_VID_GENERIC     = MEDIA_FILE_TYPE_VID_GENERIC,
  XC_CODEC_TYPE_VID_CONFERENCE  = MEDIA_FILE_TYPE_VID_CONFERENCE, 

  //XC_CODEC_TYPE_VID_PASSTHRU    = MEDIA_FILE_TYPE_VID_PASSTHRU,

  XC_CODEC_TYPE_WAV             = MEDIA_FILE_TYPE_WAV,
  XC_CODEC_TYPE_BMP             = MEDIA_FILE_TYPE_BMP,
  XC_CODEC_TYPE_PNG             = MEDIA_FILE_TYPE_PNG,
  XC_CODEC_TYPE_JPG             = MEDIA_FILE_TYPE_JPG, 

  XC_CODEC_TYPE_G711_MULAW      = 90,
  XC_CODEC_TYPE_G711_ALAW       = 91,
  XC_CODEC_TYPE_TELEPHONE_EVENT = 92,    // RFC 2833

  // Ensure no other codecs are >= 0x100 (256)
  XC_CODEC_TYPE_RAWV_BGRA32     = 0x0100,
  XC_CODEC_TYPE_RAWV_RGBA32     = 0x0101,
  XC_CODEC_TYPE_RAWV_BGR24      = 0x0102,
  XC_CODEC_TYPE_RAWV_RGB24      = 0x0103,
  XC_CODEC_TYPE_RAWV_BGR565     = 0x0104,
  XC_CODEC_TYPE_RAWV_RGB565     = 0x0105,
  XC_CODEC_TYPE_RAWV_YUV420P    = 0x0106, // planar YUV 4:2:0 12bpp
  XC_CODEC_TYPE_RAWV_YUVA420P   = 0x0107, // planar YUVA 4:2:0 20bpp (8bit alpha mask)
  XC_CODEC_TYPE_RAWV_NV12       = 0x0108, // semi-planar YUV:4:2:0, UVUVUVUV
  XC_CODEC_TYPE_RAWV_NV21       = 0x0109, // semi-planar YUV:4:2:0, VUVUVUVU

  XC_CODEC_TYPE_RAWA_PCM16LE    = 0x0300, // signed 16bit PCM little endian
  XC_CODEC_TYPE_RAWA_PCMULAW    = 0x0301,
  XC_CODEC_TYPE_RAWA_PCMALAW    = 0x0302

} XC_CODEC_TYPE_T;

#define IS_XC_CODEC_TYPE_VID_RAW(x) (((x) & 0x0100) ? 1 : 0)
#define IS_XC_CODEC_TYPE_AUD_RAW(x) (((x) & 0x0300) ? 1 : 0)
#define IS_XC_CODEC_TYPE_RAW(x) (IS_XC_CODEC_TYPE_VID_RAW(x) || \
                                  IS_XC_CODEC_TYPE_AUD_RAW(x))
#define IS_XC_CODEC_TYPE_CONFERENCE(x) ((x) == XC_CODEC_TYPE_VID_CONFERENCE || \
                                        (x) == XC_CODEC_TYPE_AUD_CONFERENCE)


#define FILETYPE_CONFERENCE    "conference"

#endif // __FILE_TYPES_H__
