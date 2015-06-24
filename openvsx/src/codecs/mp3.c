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


#include "vsx_common.h"


static unsigned int getSampleRate(int rateidx, enum MPEG_AUDIO_VER ver) {
  unsigned int rateHz = 0;
 
  switch(rateidx) {
    case 0:
      switch(ver) {
        case MPEG_AUDIO_VER_2_5:
          rateHz = 11025;
          break;
        case MPEG_AUDIO_VER_2:
          rateHz = 22050;
          break;
        case MPEG_AUDIO_VER_1:
          rateHz = 44100;
          break;
        default:
          break;
      }
      break;
    case 0x01:
      switch(ver) {
        case MPEG_AUDIO_VER_2_5:
          rateHz = 12000;
          break;
        case MPEG_AUDIO_VER_2:
          rateHz = 24000;
          break;
        case MPEG_AUDIO_VER_1:
          rateHz = 48000;
          break;
        default:
          break;
      }
      break;
    case 0x02:
      switch(ver) {
        case MPEG_AUDIO_VER_2_5:
          rateHz = 8000;
          break;
        case MPEG_AUDIO_VER_2:
          rateHz = 16000;
          break;
        case MPEG_AUDIO_VER_1:
          rateHz = 32000;
          break;
        default:
          break;
      }
      break;
    default:
      break; // invalid 
  }

  return rateHz;
}


static unsigned int getNumChannels(int chmodeidx) {
  switch(chmodeidx) {
    case MPEG_AUDIO_CHANNEL_MODE_STEREO:
    case MPEG_AUDIO_CHANNEL_MODE_JOINT_STEREO:
    case MPEG_AUDIO_DUAL_MONO:
      return 2;
    case MPEG_AUDIO_CHANNEL_MODE_SINGLE:
      return 1;
    default:
      return 0;
  }
}

const char *mp3_layer(enum MPEG_AUDIO_LAYER layer) {
  static const char *strLayer[] = { "Reserved",
                                    "Layer 3",
                                    "Layer 2",
                                    "Layer 1",
                                    "Invalid Layer" };
  if(layer >= 0 && layer <= 0x03) {
    return strLayer[layer];
  } else {
    return strLayer[4];
  }
};


const char *mp3_version(enum MPEG_AUDIO_VER version) {
  static const char *strVersion[] = { "MPEG Audio v2.5",
                                      "MPEG Audio Reserved",
                                      "MPEG Audio Version 2",
                                      "MPEG Audio Version 1",
                                      "MPEG Audio Unknown Version" };
  if(version >= 0 && version <= 0x03) {
    return strVersion[version];
  } else {
    return strVersion[4];
  }
};

int mp3_parse_hdr(const unsigned char *pbuf, unsigned int len, MP3_DESCR_T *pMp3) {
  int rc = 0;
  int val;
  BIT_STREAM_T bs;

  if(len < 4) {
    return -1;
  }

  memset(&bs, 0, sizeof(bs));
  bs.buf = (unsigned char *) pbuf;
  bs.sz = len;

  val = bits_read(&bs, 11);    
  if(val != 0x7ff) {
    return -1;
  }

  pMp3->version = bits_read(&bs, 2);    
  pMp3->layer = bits_read(&bs, 2);    
  pMp3->hascrc = !bits_read(&bs, 1);    
  pMp3->btrtidx = bits_read(&bs, 4);    
  pMp3->rateidx = bits_read(&bs, 2);    
  pMp3->sampleRateHz = getSampleRate(pMp3->rateidx, pMp3->version);
  pMp3->haspadding = bits_read(&bs, 1);    
  bits_read(&bs, 1); // private bit   
  pMp3->chmodeidx = bits_read(&bs, 2);    
  pMp3->numChannels = getNumChannels(pMp3->chmodeidx);

  return rc;
}
