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


#define AC3_STARTCODE_16_BE        0x0b77
#define AC3_STARTCODE_16_LE        0x770b

#define AC3_STARTCODE_16           AC3_STARTCODE_16_LE 

struct AC3_RATE_INFO {
  int frmsizecod;
  int bitrate;
  int spf[3];    // 16 bit samples per syncframe at 32Khz, 441.KHz, 48Khz
};

static const struct AC3_RATE_INFO ac3_rateinfo[] = {
                                    { 0x00,  32000, { 64, 69, 96 } },
                                    { 0x01,  32000, { 64, 70, 96 } },
                                    { 0x02,  40000, { 80, 87, 120 } },
                                    { 0x03,  40000, { 80, 88, 120 } },
                                    { 0x04,  48000, { 96, 104, 144 } },
                                    { 0x05,  48000, { 96, 105, 144 } },
                                    { 0x06,  56000, { 112, 121, 168 } },
                                    { 0x07,  56000, { 112, 122, 168 } },
                                    { 0x08,  64000, { 128, 139, 192 } },
                                    { 0x09,  64000, { 128, 140, 192 } },
                                    { 0x0a,  80000, { 160, 174, 240 } },
                                    { 0x0b,  80000, { 160, 175, 240 } },
                                    { 0x0c,  96000, { 192, 208, 288 } },
                                    { 0x0d,  96000, { 192, 209, 288 } },
                                    { 0x0e, 112000, { 224, 243, 336 } },
                                    { 0x0f, 112000, { 224, 244, 336 } },
                                    { 0x10, 128000, { 256, 278, 384 } },
                                    { 0x11, 128000, { 256, 279, 384 } },
                                    { 0x12, 160000, { 320, 348, 480 } },
                                    { 0x13, 160000, { 320, 349, 480 } },
                                    { 0x14, 192000, { 384, 417, 576 } },
                                    { 0x15, 192000, { 384, 418, 576 } },
                                    { 0x16, 224000, { 448, 487, 672 } },
                                    { 0x17, 224000, { 448, 488, 672 } },
                                    { 0x18, 256000, { 512, 557, 768 } },
                                    { 0x19, 256000, { 512, 558, 768 } },
                                    { 0x1a, 320000, { 640, 696, 960 } },
                                    { 0x1b, 320000, { 640, 697, 960 } },
                                    { 0x1c, 384000, { 768, 835, 1152 } },
                                    { 0x1d, 384000, { 768, 836, 1152 } },
                                    { 0x1e, 448000, { 896, 975, 1344 } },
                                    { 0x1f, 448000, { 896, 976, 1344 } },
                                    { 0x20, 512000, { 1024, 1114, 1536 } },
                                    { 0x21, 512000, { 1024, 1114, 1536 } },
                                    { 0x22, 576000, { 1152, 1253, 1728 } },
                                    { 0x23, 576000, { 1152, 1254, 1728 } },
                                    { 0x24, 640000, { 1280, 1393, 1920 } },
                                    { 0x25, 640000, { 1280, 1394, 1920 } },
                                          };

int ac3_find_starthdr(const unsigned char *pbuf, unsigned int len) {
  unsigned int idx;

  // TODO: need to read priv data hdr instead of looking for start code
  // does not keep track of last start code boundary in prev pkt
  for(idx = 1; idx < len; idx += 1) {
    if(pbuf[idx - 1] == 0x0b && pbuf[idx] == 0x77) {
      return idx - 1;
    }
  }
  return -1;
}

int ac3_find_nexthdr(const unsigned char *pbuf, unsigned int len) {
  unsigned int idx;

  // TODO: need to read priv data hdr instead of looking for start code
  // does not keep track of last start code boundary in prev pkt
  for(idx = 0; idx < len; idx += 2) {
    if( *((uint16_t *) &pbuf[idx]) == AC3_STARTCODE_16) {
      return idx;
    }
  }
  return -1;
}

static unsigned int getSampleRate(int fscod) {
  switch(fscod) {
    case 0:
      return 48000;
    case 0x01:
      return 44100;
    case 0x02:
      return 32000; 
    default:
      return 0; // reserved
  }
}

static int getNumFrontChannels(int acmode) {
  switch(acmode) {
    case 0:
      return 2;  // Ch1, Ch2 
    case 1:
      return 1;  // Center
    case 2:
      return 2;  // L, R
    case 3:
      return 3;  // L, C, R
    case 4:
      return 3;  // L, R, S
    case 5:
      return 4;  // L, C, R, S
    case 6:
      return 4;  // L, C, SL, SR
    case 7:
      return 5;  // L, C, R SL, SR
    default:
      return -1;
  }

}

static int getLFEChannel(int acmode) {
  //TODO: get Low Freq Effech (.1) channel from dsurexmod field
  if(acmode == 6 || acmode == 7) {
    return 1;
  }
  return 0;
}

static unsigned int getFrameSize(unsigned int fscod, unsigned int frmsizecod) {

  if(frmsizecod >= sizeof(ac3_rateinfo) / sizeof(ac3_rateinfo[0])) {
    return 0;
  } else if(fscod >= 2) {
    return 0;
  }

  return ac3_rateinfo[frmsizecod].spf[fscod] * 2;

}

int ac3_parse_hdr(const unsigned char *pbuf, unsigned int len, AC3_DESCR_T *pAc3) {
  int rc = 0;
  int fscod;
  BIT_STREAM_T bs;

  if(len < 7) {
    return -1;
  }

  memset(&bs, 0, sizeof(bs));
  bs.buf = (unsigned char *) &pbuf[4];
  bs.sz = len - 4;

  // 2 bytes syncword 00x0b77
  // 2 bytes crc 
  // 1 byte frame size code (2 bits sample rate, 6 bytes frame size code)
  // BSI

  fscod = bits_read(&bs, 2);
  pAc3->sampleRateHz = getSampleRate(fscod);
  pAc3->frameSize = getFrameSize(fscod, bits_read(&bs, 6));

  bits_read(&bs, 5);       // bit straem id
  bits_read(&bs, 3);       // bit stream mode
  pAc3->acmode = bits_read(&bs, 3);       // audio coding mode

  //if((pAc3->acmode & 0x01) && pAc3->acmode != 0x01) {  // 3 front channels
  //  bits_read(&bs, 2);
  //}
  //if((pAc3->acmode & 0x4)) {  // if surround channel
  //  bits_read(&bs, 2);
  //}
  //if(pAc3->acmode == 0x02) {
  //  dsurmod = bits_read(&bs, 2);
  //}

  pAc3->numChannels = getNumFrontChannels(pAc3->acmode) + getLFEChannel(pAc3->acmode);

  return rc;
}
