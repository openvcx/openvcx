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


#ifndef __MPEG_2_H__
#define __MPEG_2_H__

#include "vsxconfig.h"
#include "bits.h"
#include "fileutil.h"

#define MPEG2_HDR32_SEQ         0x000001b3
#define MPEG2_HDR32_PACK        0x000001ba
#define MPEG2_HDR32_SYSTEM      0x000001bb

#define MPEG2_HDR_SEQ           0xb3

typedef struct MPEG2_PACK_HDR {
  // 0x00 0x00 0x01 0xba
  int pad;
} MPEG2_PACK_HDR_T;

typedef struct MPEG2_SEQ_HDR {
  // 0x00 0x00 0x01 0xb3
  uint16_t hpx;
  uint16_t vpx;
  unsigned int aspratio;
  unsigned int frmrate;
  uint32_t bitrate;
  uint16_t vbufsz;
  uint16_t pad;
} MPEG2_SEQ_HDR_T;

typedef struct MPEG2_CONTAINER {
  FILE_STREAM_T *pStream;
} MPEG2_CONTAINER_T;

int mpg2_check_seq_hdr_start(const unsigned char *pbuf, unsigned int len);
int mpg2_parse_seq_hdr(const unsigned char *pbuf, unsigned int len, MPEG2_SEQ_HDR_T *pHdr);
float mpg2_get_fps(const MPEG2_SEQ_HDR_T *pHdr, unsigned int *pnum,
                   unsigned int *pden);
int mpg2_findStartCode(STREAM_CHUNK_T *pStream);


MPEG2_CONTAINER_T *mpg2_open(const char *path);
void mpg2_close(MPEG2_CONTAINER_T *pMpg2);



#endif // __MPEG_2_H__
