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


#ifndef __BITS_H__
#define __BITS_H__

#include "unixcompat.h"

#define BIT_STREAM_INCR_BYTE(p) p->byteIdx++
#define BIT_STREAM_ENDOF_BYTE(p) if(p->bitIdx > 0) {p->byteIdx++;} p->bitIdx = 0;

typedef struct BYTE_STREAM {
  unsigned char          *buf;
  unsigned int            sz;
  unsigned int            idx;
} BYTE_STREAM_T;

typedef struct BIT_STREAM {
  unsigned char *buf;
  unsigned int sz;
  unsigned int byteIdx;
  unsigned int bitIdx;
  unsigned int emulPrevSeqLen; // escape sequence is only heeded when reading from bit stream
  unsigned char emulPrevSeq[4];
} BIT_STREAM_T;

typedef struct STREAM_CHUNK {
  BIT_STREAM_T bs;
  int startCodeMatch;
} STREAM_CHUNK_T;

int bits_read(BIT_STREAM_T *pStream, unsigned int numBits);
int bits_readExpGolomb(BIT_STREAM_T *pStream);
int bits_write(BIT_STREAM_T *pStream, uint32_t bits, unsigned int numBits);
int bits_writeExpGolomb(BIT_STREAM_T *pStream, unsigned int val);

#endif // __H264_BITS_H__
