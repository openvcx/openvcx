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

#define H263_START_CODE    0x20

static const uint16_t s_h263_dimensions[][2] = {
    { 0, 0 },
    { 128, 96 },
    { 176, 144 },
    { 352, 288 },
    { 704, 576 },
    { 1408, 1152 },
};

int h263_decodeHeader(BIT_STREAM_T *pStream, H263_DESCR_T *pH263, int rfc4629) {

  int fmt, i;
  int pic_start = 0;
  uint32_t startCode = 0;


  //avc_dumpHex(stderr, &pStream->buf[pStream->byteIdx], 16, 0); 
/*
  if(rfc4629) {

    int vrc = 0;
    int plen = 0;
    int pebit = 0;

    if(pStream->sz < 3) {
      return -1;
    }

    //     http://tools.ietf.org/html/rfc4629
    //
    //     0                   1
    //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //    |   RR    |P|V|   PLEN    |PEBIT|
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //
    if(bits_read(pStream, 5) != 0) {  // Reserved
      LOG(X_ERROR("Invalid H.263 RTP reserved field found in %d bytes"), pStream->sz);
      return -1;
    }
    pic_start = bits_read(pStream, 1);
    vrc = bits_read(pStream, 1);
    plen = bits_read(pStream, 6);
    pebit = bits_read(pStream, 3);
    if(vrc) {
        bits_read(pStream, 8);
    }
    if(plen > 0) {
      if(pStream->byteIdx + plen >= pStream->sz) {
        LOG(X_ERROR("Invalid H.263 RTP plen field: 0x%x found in %d bytes"), plen, pStream->sz);
        return -1;
      }
      pStream->byteIdx += plen;
    }

  } else {
    startCode = bits_read(pStream, 14); // start code
  }
*/

  if(rfc4629) {
    // First 2 bytes are RTP H.263+ packetization header
    pic_start = 1;
  } else {
    startCode = bits_read(pStream, 14); // start code
  }

  if(pStream->byteIdx + 8 >= pStream->sz) {
    LOG(X_ERROR("Invalid H.263 frame length %d"), pStream->sz);
    return -1;
  }

  do {

    if(pic_start) {
      //
      // First 2 bits are assumed to be 0
      // 
      startCode = bits_read(pStream, 6);
      pic_start = 0;
    } else {
      startCode = ((startCode << 8) | bits_read(pStream, 8)) & 0x003fffff;
    }
  
  } while(startCode != 0x20 && pStream->byteIdx < pStream->sz);

  if(startCode != H263_START_CODE) {
    LOG(X_ERROR("H.263 start code 0x%x not found in %d bytes"), H263_START_CODE, pStream->sz);
    return -1;
  }

  i = bits_read(pStream, 8); 
  if((pH263->frame.picNum & ~0xff) + i < pH263->frame.picNum) {
    i += 256;
  }
  pH263->frame.picNum = (pH263->frame.picNum & ~0xff) + i;

  if(bits_read(pStream, 1) != 1) {
    LOG(X_ERROR("Invalid H.263 marker bit in frame size %d"), pStream->sz);
    return -1;
  }
  if(bits_read(pStream, 1) != 0) {
    LOG(X_ERROR("Invalid H.263 id bit frame size %d"), pStream->sz);
    return -1;
  }

  bits_read(pStream, 3); // split screen off, camera screen off, freeze picture releaes screen off

  if((fmt = (bits_read(pStream, 3) & 0x07)) == 0) {
    LOG(X_ERROR("Invalid H.263 dimensions value"));
    return -1;
  } else if(fmt < 6) {

    pH263->plus.h263plus = 0;
    pH263->param.frameWidthPx = s_h263_dimensions[fmt][0];
    pH263->param.frameHeightPx = s_h263_dimensions[fmt][1];
   
    if(bits_read(pStream, 1)) {
      pH263->frame.frameType = H263_FRAME_TYPE_P; 
    } else {
      pH263->frame.frameType = H263_FRAME_TYPE_I; 
    }


  } else {
    //
    // H.263+
    //
    pH263->plus.h263plus = 1;
    if((i = bits_read(pStream, 3)) != 1) {
      LOG(X_ERROR("Invalid H.263+ ufep value 0x%x"), i);
      return -1;
    }
    fmt = bits_read(pStream, 3);
    bits_read(pStream, 1); // custom_pcf
    bits_read(pStream, 1); // umvplus 
    if(bits_read(pStream, 1)) {
      LOG(X_ERROR("H.263+ Syntax based arithmetic coding not supported"));
      return -1;
    }
    bits_read(pStream, 1); // obmc
    pH263->plus.advIntraCoding = bits_read(pStream, 1); // aic
    pH263->plus.deblocking = bits_read(pStream, 1); // loop filter
    pH263->plus.sliceStructured = bits_read(pStream, 1); // sliced structured
    if(bits_read(pStream, 1)) {
      LOG(X_ERROR("H.263+ Reference picture select not supported"));
      return -1;
    }
    if(bits_read(pStream, 1)) {
      LOG(X_ERROR("H.263+ Independent segment decoding  not supported"));
      return -1;
    }
    bits_read(pStream, 1); // inter VLC
    bits_read(pStream, 1); // modified quantizer 
    bits_read(pStream, 1); // prevent start code emulation
    bits_read(pStream, 3); // reseved
    i = bits_read(pStream, 3) & 0xff;
    switch(i) {
      case 0:
      case 7:
        pH263->frame.frameType = H263_FRAME_TYPE_I; 
        break;
      case 1:
      case 2:
        pH263->frame.frameType = H263_FRAME_TYPE_P; 
      case 3:
        break;
      default:
        LOG(X_ERROR("Invalid H.263+ frame type value 0x%x"), fmt);
        return -1;
    }
    bits_read(pStream, 2); // reseved
    bits_read(pStream, 1); // no rounding 
    bits_read(pStream, 4); // reseved
  
    //fprintf(stderr, "byte:%d, bit:%d\n", pStream->byteIdx, pStream->bitIdx);
    if(fmt == 6) {
      if(pStream->byteIdx + 4 >= pStream->sz) {
        LOG(X_ERROR("Invalid H.263+ frame length %d"), pStream->sz);
        return -1;
      }
      bits_read(pStream, 4); // aspect ratio
      pH263->param.frameWidthPx = 4 * (bits_read(pStream, 9) + 1);
      bits_read(pStream, 1);
      pH263->param.frameHeightPx = 4 * bits_read(pStream, 9);
    } else {
      pH263->param.frameWidthPx = s_h263_dimensions[fmt][0];
      pH263->param.frameHeightPx = s_h263_dimensions[fmt][1];
    }

  } 

  //LOG(X_DEBUG("H.263%s %dx%d %s %d(%d)"), pH263->h263plus ? "s" : "", pH263->param.frameWidthPx, pH263->param.frameHeightPx,  pH263->frame.frameType == H263_FRAME_TYPE_I ? "I" : pH263->frame.frameType == H263_FRAME_TYPE_B ? "B" : "P", i, pH263->frame.picNum);

  //pParam->clockHz = timebase_den;
  //pParam->frameDeltaHz = timebase_num;

  return 0;
}

