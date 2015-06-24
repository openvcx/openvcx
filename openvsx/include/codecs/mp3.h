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


#ifndef __MP3_H__
#define __MP3_H__

#include "vsxconfig.h"

enum MPEG_AUDIO_VER {
  MPEG_AUDIO_VER_2_5         = 0x00,
  MPEG_AUDIO_VER_RESERVED    = 0x01,
  MPEG_AUDIO_VER_2           = 0x02,
  MPEG_AUDIO_VER_1           = 0x03
};

enum MPEG_AUDIO_LAYER {
  MPEG_AUDIO_LAYER_RESERVED    = 0x00,
  MPEG_AUDIO_LAYER_3           = 0x01,
  MPEG_AUDIO_LAYER_2           = 0x02,
  MPEG_AUDIO_LAYER_1           = 0x03
};

enum MPEG_AUDIO_CHANNEL_MODE {
  MPEG_AUDIO_CHANNEL_MODE_STEREO         = 0x00,
  MPEG_AUDIO_CHANNEL_MODE_JOINT_STEREO   = 0x01,
  MPEG_AUDIO_DUAL_MONO                   = 0x02,
  MPEG_AUDIO_CHANNEL_MODE_SINGLE         = 0x03
};

typedef struct MP3_DESCR {
  enum MPEG_AUDIO_VER        version;
  enum MPEG_AUDIO_LAYER      layer;
  int                        hascrc; 
  int                        btrtidx;
  int                        rateidx;
  int                        haspadding;
  int                        chmodeidx;

  unsigned int               sampleRateHz;
  unsigned int               numChannels;
} MP3_DESCR_T;

const char *mp3_layer(enum MPEG_AUDIO_LAYER layer);
const char *mp3_version(enum MPEG_AUDIO_VER version);


int mp3_parse_hdr(const unsigned char *pbuf, unsigned int len, MP3_DESCR_T *pMp3);





#endif // __MP3_H__
