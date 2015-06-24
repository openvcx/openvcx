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


inline static void incrementByte(BIT_STREAM_T *pStream) {
  int idx;
  int escape = 0;

  pStream->bitIdx = 0;
  BIT_STREAM_INCR_BYTE(pStream);

  if(pStream->emulPrevSeqLen > 0 && pStream->byteIdx >= pStream->emulPrevSeqLen - 1) {
    escape = 1;
    for(idx = 1 - ((int) pStream->emulPrevSeqLen); idx < 1; idx++) {
      if(pStream->buf[pStream->byteIdx + idx] != 
         pStream->emulPrevSeq[(int)pStream->emulPrevSeqLen + idx - 1]) {
        escape = 0;
        break;
      }
    }
  }

  if(escape) {
    pStream->byteIdx++;
  }

}

int bits_read(BIT_STREAM_T *pStream, unsigned int numBits) {
  unsigned int idx;
  unsigned int bitVal;
  int rc = 0;

  for(idx = 0; idx < numBits; idx++) {

    if(pStream->bitIdx > 7) {
      incrementByte(pStream);
    }

    if((bitVal = pStream->buf[pStream->byteIdx] & (1 << (7 - pStream->bitIdx)))) { 
      rc |= (1 << (numBits - idx -1));
    }

    pStream->bitIdx++;
  }

  if(pStream->bitIdx > 7) {
    incrementByte(pStream);
  }

  return rc;
}

//
// Reads a single integer value in H.264 Exponential Golomb encoding
//
int bits_readExpGolomb(BIT_STREAM_T *pStream) {

  unsigned int len = 0;
  unsigned int bitVal;

  do {

    if(pStream->bitIdx > 7) {
      pStream->bitIdx = 0;
      BIT_STREAM_INCR_BYTE(pStream);
    }

    if((bitVal = pStream->buf[pStream->byteIdx] & (1 << (7 - pStream->bitIdx))) == 0) {
      pStream->bitIdx++;
    }

    if(++len > 32) {
      return 0;
    }

  } while(bitVal == 0);

  if(pStream->bitIdx > 7) {
    pStream->bitIdx = 0;
    BIT_STREAM_INCR_BYTE(pStream);
  }

  return bits_read(pStream, len) - 1;
}

static int bits_writeFromStream(BIT_STREAM_T *pStreamDst, 
                         BIT_STREAM_T *pStreamSrc, 
                         unsigned int numBits) {
  unsigned int idx;
  unsigned int bitVal;
  int rc = 0;

  for(idx = 0; idx < numBits; idx++) {

    bitVal = bits_read(pStreamSrc, 1);

    if(pStreamDst->bitIdx > 7) {
      pStreamDst->bitIdx = 0;
      BIT_STREAM_INCR_BYTE(pStreamDst);
    }

    if(bitVal) {
      pStreamDst->buf[pStreamDst->byteIdx] |= (1 << (7 - pStreamDst->bitIdx));
    } else {
      pStreamDst->buf[pStreamDst->byteIdx] &= ~(1 << (7 - pStreamDst->bitIdx));
    }

    pStreamDst->bitIdx++;

  }

  if(pStreamDst->bitIdx > 7) {
    pStreamDst->bitIdx = 0;
    BIT_STREAM_INCR_BYTE(pStreamDst);
  }
  return rc;
}

int bits_write(BIT_STREAM_T *pStream, uint32_t bits, unsigned int numBits) {
  BIT_STREAM_T streamSrc;
  uint32_t val;

  if(numBits > 32) {
    numBits = 32;
  }
  val = htonl(bits);

  //TODO: should escape writing bad start code depending on context
  streamSrc.emulPrevSeqLen = 0;
  streamSrc.bitIdx = (8 - (numBits % 8));
  streamSrc.byteIdx = 3 - (numBits / 8);
  streamSrc.buf = (unsigned char *) &val;
  streamSrc.sz = 4;

  return bits_writeFromStream(pStream, &streamSrc, numBits);
}


int bits_writeExpGolomb(BIT_STREAM_T *pStream, unsigned int val) {

  unsigned int numBits = 0;
  unsigned int zero = 0;
  uint32_t valp1 = val + 1;
  BIT_STREAM_T streamSrc;

  do {
    valp1 = valp1 >> 1;
    numBits++;
  } while(valp1 > 0);

  //TODO: should escape writing bad start code based on context
  streamSrc.emulPrevSeqLen = 0;
  streamSrc.bitIdx = 0;
  streamSrc.byteIdx = 0;
  streamSrc.buf = (unsigned char *) &zero;
  streamSrc.sz = 4;
  bits_writeFromStream(pStream, &streamSrc, numBits - 1);

  valp1 = htonl(val + 1);
  streamSrc.emulPrevSeqLen = 0;
  streamSrc.bitIdx = (8 - (numBits % 8));
  streamSrc.byteIdx = 3 - (numBits / 8);
  streamSrc.buf = (unsigned char *) &valp1;
  streamSrc.sz = 4;
  bits_writeFromStream(pStream, &streamSrc, numBits);

  return 0;
}

#if 0
#include <string.h>
void testbits() {
  int i, idx;
  unsigned char buf[32];
  unsigned char buf2[32];
  BIT_STREAM_T stream;
  BIT_STREAM_T stream2;

  memset(&stream, 0, sizeof(stream));
  memset(&stream2, 0, sizeof(stream2));

  stream.emulPrevSeqLen = 3;
  stream.emulPrevSeq[2] = 0x03;
  stream.buf = buf;
  stream.sz = sizeof(buf);

  stream2.buf = buf2;
  stream2.sz = sizeof(buf);

  memset(buf, 0, sizeof(buf));
  memset(buf2, 0, sizeof(buf2));


  //bits_writeExpGolomb(&stream2, 8);
  //bits_writeExpGolomb(&stream2, 255);
  //bits_writeExpGolomb(&stream2, 37);

  //stream2.byteIdx = 0;
  //stream2.bitIdx = 0;
  //i = bits_readExpGolomb(&stream2);
  //i = bits_readExpGolomb(&stream2);
   //i = bits_readExpGolomb(&stream2);

  bits_write(&stream, 0x00, 8);
  bits_write(&stream, 0x00, 8);
  bits_write(&stream, 0x02, 8);
  bits_write(&stream, 0x00, 8);
  bits_write(&stream, 0x00, 8);
  bits_write(&stream, 0x00, 8);
  bits_write(&stream, 0x00, 8);
  bits_write(&stream, 0x00, 8);
  fprintf(stderr, "wrote 0x%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n", 
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);


  stream2.sz = 13;
  i = h264_escapeRawByteStream(&stream, &stream2);
  fprintf(stderr, "escapes:%d  0x%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n" 
                  "            0x%2.2x %2.2x %2.2x %2.2x\n",
        i, buf2[0], buf2[1], buf2[2], buf2[3], buf2[4], buf2[5], buf2[6], buf2[7],
           buf2[8], buf2[9], buf2[10], buf2[11]);
  fprintf(stderr, "%d %d\n", stream.byteIdx, stream2.byteIdx);

  stream2.byteIdx = 0;
  stream2.sz=8;
  stream2.emulPrevSeqLen = 3;
  stream2.emulPrevSeq[2] = 0x03;
  for(idx = 0; idx < 8; idx++) {
    i = bits_read(&stream2, 8);
    fprintf(stderr, "0x%2.2x ", i);
  }

  //stream.byteIdx = 0;
  //stream.bitIdx = 0;
  //i = bits_read(&stream, 32);
  //fprintf(stderr, "read%d 0x%2.2x %2.2x %2.2x %2.2x\n", 
  //        htonl(i), ((unsigned char *)&i)[0],((unsigned char *)&i)[1],
  //                  ((unsigned char *)&i)[2],((unsigned char *)&i)[3]);

}
#endif // 0
