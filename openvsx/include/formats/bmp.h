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


#ifndef __BMP_H__
#define __BMP_H__

#ifdef WIN32

#include <windows.h>
#include "unixcompat.h"

#else // WIN32

#include <unistd.h>
#include "wincompat.h"

#endif // WIN32

typedef enum BMP_COMPRESS_TYPE {
  BMP_COMPRESS_TYPE_RGB         = 0,
  BMP_COMPRESS_TYPE_RLE8        = 1,
  BMP_COMPRESS_TYPE_RLE4        = 2,
  BMP_COMPRESS_TYPE_BITFIELDS   = 3,
  BMP_COMPRESS_TYPE_JPEG        = 4,
  BMP_COMPRESS_TYPE_PNG         = 5
} BMP_COMPRESS_TYPE_T;

typedef struct {
  uint32_t              header_sz;
  int32_t               width;
  int32_t               height;
  uint16_t              nplanes;
  uint16_t              bitspp;
  uint32_t              compress_type;
  uint32_t              bmp_bytesz;
  int32_t               hres;
  int32_t               vres;
  uint32_t              ncolors;
  uint32_t              nimpcolors;
} BITMAPINFOHEADER_T;

#define  BITMAPINFOHEADER_SZ          40
#define  BITMAPINFOHEADER_OS2_SZ      64 
#define  BITMAPINFOHEADER_WINV4_SZ    108 
#define  BITMAPINFOHEADER_WINV5_SZ    124 


typedef struct BMP_FILE_HDR {
  uint16_t               pad;
  uint16_t               magic;
  uint32_t               filesz;
  uint16_t               creator1;
  uint16_t               creator2;
  uint32_t               bmp_offset;
} BMP_FILE_HDR_T;


typedef struct BMP_HDR {
  BMP_FILE_HDR_T         file;
  union {
    BITMAPINFOHEADER_T   bih;
  } udib;
} BMP_HDR_T;

#define  BMP_FILE_HDR_SZ   14

typedef struct BMP_FILE {
  unsigned char         *pBuf;
  unsigned int           bufLen;
  BMP_HDR_T             *pHdr;
  unsigned int           hdrLen;
  unsigned int           dataLen;
  unsigned int           width;
  unsigned int           height;
  XC_CODEC_TYPE_T        formatType;
} BMP_FILE_T;

#define BMP_GET_DATA(pBmp)     (&((pBmp)->pBuf[2]))
#define BMP_GET_LEN(pBmp)     ((pBmp)->hdrLen + (pBmp)->dataLen)

int bmp_create(BMP_FILE_T *pBmp, XC_CODEC_TYPE_T inFormatType,
               unsigned int width, unsigned int height,
               const unsigned char *pRawIn);

int bmp_parse(BMP_FILE_T *pBmp);
void bmp_destroy(BMP_FILE_T *pBmp);


#endif //__BMP_H__
