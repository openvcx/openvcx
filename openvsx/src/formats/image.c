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

static int bmp_parse_hdr(IMAGE_GENERIC_DESCR_T *pImg) {
  int rc = 0;
  BMP_FILE_T bmp;

  memset(&bmp, 0, sizeof(bmp));
  bmp.pBuf = pImg->bs.buf;
  bmp.bufLen = pImg->bs.sz;

  if((rc = bmp_parse(&bmp)) == 0) {
    pImg->width = bmp.width;    
    pImg->height = bmp.height;    
  }

  return rc;
}

static int png_parse(IMAGE_GENERIC_DESCR_T *pImg) {
  int rc = 0;
  unsigned int idx = 8;
  unsigned int sz;
  const unsigned char png_hdr[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
  const unsigned char mng_hdr[] = { 0x8a, 0x4d, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

  if(pImg->bs.sz < 8 || 
     (memcmp(pImg->bs.buf, png_hdr, 8) && memcmp(pImg->bs.buf, mng_hdr, 8))) {
    return -1;
  }

  while(idx + 8 < pImg->bs.sz) {
    sz = htonl( *((uint32_t *) &pImg->bs.buf[idx]) );
    idx += 4;

    //fprintf(stderr, "TAG:%c%c%c%c len:%d\n", pImg->bs.buf[idx],pImg->bs.buf[idx+1], pImg->bs.buf[idx+2], pImg->bs.buf[idx+3], sz);

    if(pImg->bs.buf[idx] == 'I' && pImg->bs.buf[idx + 1] == 'H' &&
       pImg->bs.buf[idx + 2] == 'D' && pImg->bs.buf[idx + 3] == 'R') {

      if(sz != 13) {
        rc = -1;
        break;
      }
    
      pImg->width =  htonl( *((uint32_t *) &pImg->bs.buf[idx + 4]) );
      pImg->height =  htonl( *((uint32_t *) &pImg->bs.buf[idx + 8]) );
     
      break;

    } else if(pImg->bs.buf[idx] == 'I' && pImg->bs.buf[idx + 1] == 'D' &&
       pImg->bs.buf[idx + 2] == 'A' && pImg->bs.buf[idx + 3] == 'T') {

    }

    idx += (4 + sz + 4);

  }


  return rc;
}

int image_open(const char *path, IMAGE_GENERIC_DESCR_T *pImg, 
               enum MEDIA_FILE_TYPE mediaType) {
  int rc = 0;
  uint32_t sz;
  FILE_STREAM_T fs;

  if(!path || !pImg) {
    return -1;
  }

  pImg->mediaType = mediaType;

  memset(&fs, 0, sizeof(fs));
  if(OpenMediaReadOnly(&fs, path) < 0) {
    return -1;
  }

  if(fs.size > IMAGE_MAX_SIZE) {
    LOG(X_ERROR("File %s size %llu exceeds max %d"),
      fs.filename, fs.size, IMAGE_MAX_SIZE);
    rc = -1;
  } else if(fs.size < 4) {
    LOG(X_ERROR("File %s size is too small %llu"), fs.filename, fs.size);
    rc = -1;
  }

  if(rc >= 0) {
    memset(&pImg->bs, 0, sizeof(pImg->bs));
    pImg->bs.sz = (unsigned int) fs.size;
    if(!(pImg->bs.buf = avc_calloc(1, pImg->bs.sz))) {
      rc = -1;
    }
  }

  if(rc >= 0) {
    while(pImg->bs.idx < fs.size) {
      if((sz = (uint32_t) (fs.size - pImg->bs.idx)) > 4096) {
        sz = 4096;
      }
      if((rc = ReadFileStream(&fs, &pImg->bs.buf[pImg->bs.idx], sz)) != sz) {
        rc = -1;
        break;
      }
      pImg->bs.idx += rc;
      
    }
    if(rc >= 0) {
      rc = pImg->bs.idx;
    }
    pImg->bs.idx = 0;
  }

  CloseMediaFile(&fs);

  pImg->width = 0;
  pImg->height = 0;

  if(rc >= 0) {
    switch(mediaType) {
      case MEDIA_FILE_TYPE_PNG:
        png_parse(pImg);
        break;
      case MEDIA_FILE_TYPE_BMP:
        bmp_parse_hdr(pImg);
        break;
      default:
        break;
    }
  }

  return rc;
}

int image_close(IMAGE_GENERIC_DESCR_T *pImg) {
  int rc = 0;

  if(!pImg) {
    return -1;
  }

  if(pImg->bs.buf) {
    free(pImg->bs.buf);
    memset(&pImg->bs, 0, sizeof(pImg->bs));
  }

  return rc;
}

