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


static int bmp_bgra32_to_bgr24(const unsigned char *in, unsigned int inlen,
                              unsigned char *out, unsigned int lenout,
                              unsigned int linesize) {
  unsigned int idxout;
  unsigned int idxin = 0;
  unsigned int lineidx = 0;
  unsigned int rowidx = 0;

  if(!in || !out) {
     return -1;
  }

  idxout = lenout;
  lineidx = 0;
  while(idxout > 0) {

    idxin = (4 * rowidx * linesize) + ((linesize *4) - lineidx - 4);

    out[idxout - 3] = in[idxin];      // blue
    out[idxout - 2] = in[idxin + 1];  // green
    out[idxout - 1] = in[idxin + 2];  // red

    lineidx += 4;
    if(lineidx >= linesize * 4) {
      lineidx = 0;
      rowidx++;
    }
    idxout -= 3;
  }

  return lenout;
}

static int bmp_bgr565_to_bgr24(const unsigned char *in, unsigned int inlen,
                              unsigned char *out, unsigned int lenout,
                              unsigned int linesize) {
  unsigned int idxout;
  unsigned int idxin = 0;
  unsigned int lineidx = 0;
  unsigned int rowidx = 0;

  if(!in || !out) {
     return -1;
  }

  idxout = lenout;
  lineidx = 0;
  while(idxout > 0) {

    idxin = (2 * rowidx * linesize) + ((linesize *2) - lineidx - 2);

    out[idxout - 3] = in[idxin] >> 3;                     // blue
    out[idxout - 2] = ((in[idxin] & 0x07) << 5) |  (in[idxin + 1] >> 5);  // green
    out[idxout - 1] = in[idxin + 1] & 0x01f;              // red

    lineidx += 2;
    if(lineidx >= linesize * 4) {
      lineidx = 0;
      rowidx++;
    }
    idxout -= 3;
  }

  return lenout;
}

int bmp_create(BMP_FILE_T *pBmp, XC_CODEC_TYPE_T inFormatType,
               unsigned int width, unsigned int height,
               const unsigned char *pRawIn) {
  int rc = 0;
  unsigned int hdrLen = BMP_FILE_HDR_SZ + BITMAPINFOHEADER_SZ;
  unsigned int sz24;

  if(!pBmp || (width <= 0 || height <= 0) || !pRawIn) {
    return -1;
  }

  switch(inFormatType) {
    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_BGR565:
      break;
    default:
      LOG(X_ERROR("Unsupported BMP input data format %d"), inFormatType);
    return -1;
  }


  if((sz24 = width * height * 3) <= 0) {
    LOG(X_ERROR("Invalid BMP input format dimensions %x x %d"), width, height);
    return -1;
  }

  if(pBmp->pBuf && pBmp->bufLen < (2 + hdrLen + sz24)) {
    free(pBmp->pBuf);
    pBmp->pBuf = NULL;
    pBmp->bufLen = 0;
  }
  if(!pBmp->pBuf) {
    if(!(pBmp->pBuf = (unsigned char *) avc_calloc(1, 2 + hdrLen + sz24))) {
      return -1;
    }
    pBmp->bufLen = 2 + hdrLen + sz24;
  }

  pBmp->width = width;
  pBmp->height = height;
  pBmp->hdrLen = hdrLen;
  pBmp->dataLen = sz24;

  pBmp->pHdr = (BMP_HDR_T *) pBmp->pBuf;
  pBmp->pHdr->file.pad = 0;           // use 2 byte pad for 4 byte data alignment
  pBmp->pHdr->file.magic = LE16(0x4d42);
  pBmp->pHdr->file.filesz = LE32(pBmp->hdrLen + pBmp->dataLen);
  pBmp->pHdr->file.bmp_offset = LE32(pBmp->hdrLen);

  pBmp->pHdr->udib.bih.header_sz = LE32(BITMAPINFOHEADER_SZ);
  pBmp->pHdr->udib.bih.width = LE32(pBmp->width);
  pBmp->pHdr->udib.bih.height = LE32(pBmp->height);
  pBmp->pHdr->udib.bih.nplanes = LE16(1);
  pBmp->pHdr->udib.bih.bitspp = LE16(24);
  pBmp->pHdr->udib.bih.compress_type = LE32(BMP_COMPRESS_TYPE_RGB);
  pBmp->pHdr->udib.bih.bmp_bytesz = LE32(0);
  pBmp->pHdr->udib.bih.hres = LE32(0);
  pBmp->pHdr->udib.bih.vres = LE32(0);
  pBmp->pHdr->udib.bih.ncolors = LE32(0);
  pBmp->pHdr->udib.bih.nimpcolors = LE32(0);

  //
  // Creates 24 bpp output format as XC_CODEC_TYPE_RAW_BGR24
  //
  switch(inFormatType) {
    case XC_CODEC_TYPE_RAWV_BGRA32:
      if((rc = bmp_bgra32_to_bgr24(pRawIn, width * height * 4,
                              &pBmp->pBuf[2 + pBmp->hdrLen], 
                              pBmp->dataLen, width)) >= 0) {
        rc = pBmp->hdrLen + pBmp->dataLen;
      }
      break;
    case XC_CODEC_TYPE_RAWV_BGR565:
      if((rc = bmp_bgr565_to_bgr24(pRawIn, width * height * 2,
                                 &pBmp->pBuf[2 + pBmp->hdrLen], 
                                 pBmp->dataLen, width)) >= 0) {
        rc = pBmp->hdrLen + pBmp->dataLen;
      }
      break;
    default:
      break;
  }

  return rc;
}

int bmp_parse(BMP_FILE_T *pBmp) {
  int rc = 0;
  uint32_t hdrsz;

  if(!pBmp || !pBmp->pBuf || pBmp->bufLen < BMP_FILE_HDR_SZ + BITMAPINFOHEADER_SZ) {
    return -1;
  }

  pBmp->pHdr = (BMP_HDR_T *) (((unsigned char *) pBmp->pBuf) - 2);

  hdrsz = LE32(pBmp->pHdr->udib.bih.header_sz);

  switch(hdrsz) {
    case BITMAPINFOHEADER_SZ:
    case BITMAPINFOHEADER_OS2_SZ:
    case BITMAPINFOHEADER_WINV4_SZ:
    case BITMAPINFOHEADER_WINV5_SZ:
      pBmp->width = LE32(pBmp->pHdr->udib.bih.width); 
      pBmp->height = LE32(pBmp->pHdr->udib.bih.height); 
      break;
    default:
      break;
  }

  return rc;
}

void bmp_destroy(BMP_FILE_T *pBmp) {

  if(!pBmp) {
    return;
  }

  if(pBmp->pBuf) {
    free(pBmp->pBuf);
  }

}
