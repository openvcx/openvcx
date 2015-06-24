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


unsigned char *ebml_parse_get_bytes(EBML_BYTE_STREAM_T *mbs, unsigned int len) {
  int rc;
  unsigned int lenread = 0;
  unsigned int idxbuf = 0;
  unsigned char *pdata = NULL;

  if(mbs->pfs) {

    if(len > mbs->bufsz / 2) {
      if(!mbs->nolog) {
        LOG(X_ERROR("EBML read request for length %d > max: %d"), len, mbs->bufsz / 2);
      }
      return NULL;
    }

    if(mbs->bs.idx == 0 || mbs->bs.sz == 0) {
      mbs->bs.idx = 0;
      mbs->bs.sz = 0;
      lenread = mbs->bufsz;
    } else if(mbs->bs.idx >= (mbs->bufsz / 2)) {
      lenread = mbs->bs.sz - (mbs->bufsz / 2);
      memcpy(mbs->bs.buf, &mbs->bs.buf[mbs->bufsz / 2], lenread);
      mbs->bs.idx -= (mbs->bufsz / 2);
      lenread = mbs->bufsz / 2;
      idxbuf = mbs->bufsz / 2;
    }

//fprintf(stderr, "\nBUFFER READ IDX[%d]\n", mbs->bs.idx); avc_dumpHex(stderr, mbs->bs.buf, mbs->bs.sz, 1);

    if(lenread > 0) {
//fprintf(stderr, "READING %d from disk into [%d]\n", lenread, idxbuf);
       if((rc = ReadFileStreamNoEof(mbs->pfs, &mbs->bs.buf[idxbuf], lenread)) < 0) {
        return NULL;
      }

      mbs->bs.sz = idxbuf + rc;
    }

  }

  if(len > mbs->bs.sz - mbs->bs.idx) {
    if(!mbs->pfs && !mbs->nolog) {
      LOG(X_ERROR("EBML read request for %d cannot be fulfilled without data source set"), len);
    }
    return NULL;
  }

  pdata = &mbs->bs.buf[mbs->bs.idx];
  mbs->bs.idx += len;
  mbs->offset += len;

//fprintf(stderr, "GOT %d bytes, file offset: %lld, buf idx[%d]/%d\n", len, mbs->offset, mbs->bs.idx, mbs->bs.sz);

  return pdata;
}

int ebml_parse_get_byte(EBML_BYTE_STREAM_T *mbs) {
  uint8_t *puc;

  if(!(puc = ebml_parse_get_bytes(mbs, 1))) {
    if(!mbs->nolog) {
      LOG(X_ERROR("EBML failed to read byte in %s at 0x%llx / 0x%llx"),
                mbs->pfs ? mbs->pfs->filename : "", mbs->offset, mbs->pfs ? mbs->pfs->offset : 0);
    }
  }
  return puc ? *puc : -1;
}

int ebml_parse_skip_bytes(EBML_BYTE_STREAM_T *mbs, unsigned int len) {
  unsigned char *puc;
  unsigned int skipped = 0;
  unsigned int skiplen;

  while(skipped < len) {
    if((skiplen = len - skipped) > (mbs->bufsz / 2)) {
      skiplen = mbs->bufsz / 2;
    }

    if(!(puc = ebml_parse_get_bytes(mbs, skiplen))) {
      return -1;
    }
    skipped += skiplen;
  }

  return 0;
}

int ebml_parse_get_num(EBML_BYTE_STREAM_T *mbs, unsigned int max, uint64_t *pval) {
  unsigned int idx;
  unsigned int len;
  int ival;

  if((ival = ebml_parse_get_byte(mbs)) < 0) {
    return -1;
  }

  ival = (uint8_t) ival;
  *pval = ival;
  idx = (unsigned int) math_log2_32((uint8_t) ival);

  if((len = 8 - idx) > max) {
    if(!mbs->nolog) {
      LOG(X_ERROR("EBML value contains invalid number size: 0x%x > max: 0x%x, in %s at 0x%llx"),
              len, max, mbs->pfs->filename, mbs->offset);
    }
    return -1;
  }

  //
  // Clear the leading bit of the first byte, which is the ebml byte length indicator
  //
  //*pval = (*pval) ^ (1 << idx);
  *pval &= ~(1 << idx);
  //fprintf(stderr, "SET 1 %d bits, ival:%d %lld\n", idx, ival, *pval);

  for(idx = 1; idx < len; idx++) {
    if((ival = ebml_parse_get_byte(mbs)) < 0) {
      return -1;
    }
    *pval = ((*pval) << 8) | ival;
  }

  return len;
}

int ebml_parse_get_id(EBML_BYTE_STREAM_T *mbs, uint32_t *pval) {
  int rc;
  uint64_t val;

  if((rc = ebml_parse_get_num(mbs, 4, &val)) < 0) {
    return rc;
  }

  //
  // (Re-)set the leading bit of the first byte, which is the ebml byte length indicator, 
  //  it is part of the full id field
  //
  *pval = val | 1 << (7 * rc);

  return rc;
}


int ebml_parse_get_length(EBML_BYTE_STREAM_T *mbs, uint64_t *pval) {
  int rc;

  if((rc = ebml_parse_get_num(mbs, 8, pval)) > 0 && 
     *pval + 1 == 1ULL << (7 * rc)) {
    *pval = EBML_MAX_NUMBER;
  }

  return rc;
}

int ebml_parse_get_uint(EBML_BYTE_STREAM_T *mbs, unsigned int len, uint64_t *pval) {
  unsigned int idx = 0;
  int ival;

  if(len > 8) {
    return -1;
  }

  *pval = 0;

  for(idx = 0; idx < len; idx++) {
    if((ival = ebml_parse_get_byte(mbs)) < 0) {
      return -1;
    }
    *pval = ((*pval) << 8) | ival;
  }

  return len;
}

int ebml_parse_get_double(EBML_BYTE_STREAM_T *mbs, unsigned int len, double *pval) {
  unsigned char *puc;
  float f;
  uint32_t ui32;
  uint64_t ui64;

  if(len == 0) {
    *pval = 0;
  } else if(len == 4) {

    if(!(puc = ebml_parse_get_bytes(mbs, 4))) {
      return -1;
    }
//avc_dumpHex(stderr, puc, 8, 1);
    f = *((float *) puc);
    *pval = (double) f;

  } else if(len == 8) {

    if(!(puc = ebml_parse_get_bytes(mbs, 4))) {
      return -1;
    }
    ui32 = htonl( *((uint32_t *) puc));
    ui64 = ((uint64_t) ui32) << 32;

    if(!(puc = ebml_parse_get_bytes(mbs, 4))) {
      return -1;
    }
    ui32 = htonl( *((uint32_t *) puc));
    ui64 |= ui32;

   *pval = *((double *) &ui64);

  } else {
    if(!mbs->nolog) {
      LOG(X_ERROR("EBML invalid double field length %d at offset 0x%llx"), len, mbs->offset);
    }
    return -1;
  }

  return len;
}

int ebml_parse_get_binary(EBML_BYTE_STREAM_T *mbs, unsigned int len, void **ppval) {
  unsigned char *puc;

  if(!(puc = ebml_parse_get_bytes(mbs, len))) {
    return -1;
  }

  *ppval = puc;

  return len;
}

int ebml_parse_get_string(EBML_BYTE_STREAM_T *mbs, unsigned int len, unsigned char **ppval) {

  unsigned char *puc;

  if(!(puc = ebml_parse_get_bytes(mbs, len))) {
    return -1;
  }

  *ppval = puc;

  return len;
}

void ebml_free_data(const EBML_SYNTAX_T *psyntax, void *pDataParent) {
  unsigned int syntaxIdx = 0;
  unsigned int elemIdx;
  EBML_ARR_T *pArr;
  EBML_BLOB_T *pBlob;
  void *pData;

  if(!psyntax) {
    return;
  }

  while(psyntax[syntaxIdx].id > EBML_TYPE_NONE) {

    pData = ((unsigned char *) pDataParent) + psyntax[syntaxIdx].offset;
  //fprintf(stderr, "EBML_FREE syntax[%d], syntax->offset:%d,type:%d, pData:0x%x\n", syntaxIdx, psyntax[syntaxIdx].offset, psyntax[syntaxIdx].type, pData);

    switch(psyntax[syntaxIdx].type) {

      case EBML_TYPE_SUBTYPES:
      case EBML_TYPE_PASS:

        if(psyntax[syntaxIdx].dynsize > 0) {
          if((pArr = (EBML_ARR_T *) pData)->parr) {
            for(elemIdx = 0; elemIdx < pArr->count; elemIdx++) {
              ebml_free_data(psyntax[syntaxIdx].pchild, ((unsigned char *) pArr->parr) +
                                                   psyntax[syntaxIdx].dynsize * elemIdx);
            }
//fprintf(stderr, "FREEING ELEMENT 0x%x\n", pArr->parr);
            if(!pArr->nondynalloc && pArr->parr) {
              avc_free(&pArr->parr);
            }
          }
        } else {
          ebml_free_data(psyntax[syntaxIdx].pchild, pData);
        }

        break;
      case EBML_TYPE_UTF8:

        break;
      case EBML_TYPE_BINARY:
        pBlob = (EBML_BLOB_T *) pData;
//fprintf(stderr, "FREEING BLOB SZ:%d 0x%x\n", pBlob->size, pBlob->p);
        if(!pBlob->nondynalloc && pBlob->p) {
          avc_free(&pBlob->p);
        }
        break;

      case EBML_TYPE_UINT32:
      case EBML_TYPE_UINT64:
//fprintf(stderr, "FREEING UINT64 %lld\n", *((uint64_t *)pData));
        break;

      default:
        break;
    }
    syntaxIdx++;
  }

}



static int ebml_number_size(uint64_t ui64) {
  unsigned int szBytes = 1;

  while((ui64 + 1) >> szBytes * 7) {
    szBytes++;
  }

  return szBytes;
}

static int ebml_id_size(uint32_t ui32) {
  return ((math_log2_32(ui32 + 1) -1) / 7) + 1;
}


static int ebml_sync(EBML_BYTE_STREAM_T *mbs, const unsigned char *pData, unsigned int lenData) {
  int rc = 0;
  unsigned int idxData = 0;
  unsigned int sz;
  unsigned int szFlushed = 0;

//fprintf(stderr, "FLUSH called idx[%d], lenData:%d\n", mbs->bs.idx, lenData);

  if((sz = lenData) > mbs->bs.sz - mbs->bs.idx) {
    sz = mbs->bs.sz - mbs->bs.idx;
  }

  if(pData && sz > 0) {
//fprintf(stderr, "FLUSH copying %d into mbs[%d]\n", sz, mbs->bs.idx);
    memcpy(&mbs->bs.buf[mbs->bs.idx], pData, sz);
    mbs->bs.idx += sz;
    idxData += sz;
  }

  if(idxData <= lenData) {

    if(mbs->pfs && mbs->bs.idx > 0 ) {
      if((rc = WriteFileStream(mbs->pfs, mbs->bs.buf, mbs->bs.idx)) != mbs->bs.idx) {
        return -1;
      }
      szFlushed += mbs->bs.idx;
      mbs->bs.idx = 0;
    }
//fprintf(stderr, "sz:%d, lenData:%d, idxData:%d mbs{0x%x} [%d]/%d, mbs->pfs:0x%x, pData: 0x%x 0x%x\n", sz, lenData, idxData, mbs, mbs->bs.idx, mbs->bs.sz, mbs->pfs, pData ? pData[0] : 0, pData ? pData[1] : 0);
    if(pData && (sz > 0 || (mbs->bs.idx == mbs->bs.sz))) {

      if((sz = (lenData - idxData)) > (mbs->bs.sz - mbs->bs.idx)) {
//fprintf(stderr, "FLUSH writing data buffer [%d] sz: %d\n", idxData, sz);
        if(!mbs->pfs) {
          if(!mbs->nolog) {
            LOG(X_ERROR("EBML sync no stream output set to flush %d"), sz);
          }
          return -1;
        }
        if((rc = WriteFileStream(mbs->pfs, &pData[idxData], sz)) != sz) {
          return -1;
        }
        mbs->bs.idx = 0;
        szFlushed += sz;
      } else if(sz > 0) {
//fprintf(stderr, "FLUSH queued sz: %d\n", sz);
        memcpy(mbs->bs.buf, &pData[idxData], sz); 
        mbs->bs.idx += sz;
      }
      idxData += sz;
    }

  }

  return rc;
}


int ebml_write_flush(EBML_BYTE_STREAM_T *mbs) {

  if(!mbs->pfs && mbs->bs.idx > 0) {
    if(!mbs->nolog) {
      LOG(X_ERROR("EBML flush no stream output set"));
    }
    return -1;
  }

  return ebml_sync(mbs, NULL, 0);
}


int ebml_write_num(EBML_BYTE_STREAM_T *mbs, uint64_t ui64, unsigned int numBytes) {
  unsigned int sz;
  unsigned char buf[16];
  unsigned int bufIdx = 0;

  if(ui64 > EBML_MAX_NUMBER) {
    return -1;
  } else if(ui64 == EBML_MAX_NUMBER) {
    sz = 8;
  } else { 
    sz = ebml_number_size(ui64);
  }

  if(numBytes == 0) {
    numBytes = sz;
  } else if(sz > numBytes || numBytes > 8) {
    return -1;
  }

  //
  // Set the leading bit of the first byte, which is the ebml byte length indicator
  //
  ui64 |= (uint64_t) 1 << (numBytes * 7);

  for(bufIdx = 0; bufIdx < numBytes; bufIdx++) {
    buf[bufIdx] = (uint8_t) (ui64 >> ((numBytes - bufIdx - 1) * 8));
  }
  if(ebml_sync(mbs, buf, bufIdx) < 0) {
    return -1;
  }

  return bufIdx;
}


int ebml_write_id(EBML_BYTE_STREAM_T *mbs, uint32_t ui32) {
  unsigned int numBytes;
  unsigned int bufIdx;
  uint32_t tmp;
  unsigned char buf[8];

  if((numBytes = ebml_id_size(ui32)) > 4) {
    if(!mbs->nolog) {
      LOG(X_ERROR("EBML invalid id 0x%x size %d"), ui32, numBytes);
    }
    return -1;
  }

  tmp = ui32 | 1 << (numBytes * 7);

  //
  // Check to ensure that The id already has the leading length indicator field set 
  //
  if(tmp != ui32) {
    if(!mbs->nolog) {
      LOG(X_ERROR("EBML invalid id: 0x%x (should be 0x%x)"), ui32, tmp);
    }
    return -1;
  }

  for(bufIdx = 0; bufIdx < numBytes; bufIdx++) {
    buf[bufIdx] = (uint8_t) (ui32 >> ((numBytes - bufIdx - 1) * 8));
  }

  if(ebml_sync(mbs, buf, bufIdx) < 0) {
    return -1;
  }

  return bufIdx;
}

static int ebml_write_idhdr(EBML_BYTE_STREAM_T *mbs, uint32_t id, unsigned int length) {
  int rc;
  unsigned int written;

  if((rc = ebml_write_id(mbs, id)) < 0) {
    return rc;
  }

  written = rc;

  if((rc = ebml_write_num(mbs, length, 0)) < 0) {
    return rc;
  }

  return written + rc;
}

int ebml_write_uint(EBML_BYTE_STREAM_T *mbs, uint32_t id, uint64_t ui64) {
  int rc;
  unsigned int length = 1;
  uint64_t val;
  unsigned int idx;
  unsigned char buf[8];

  val = ui64;
  while((val >>= 8)) {
    length++;
  } 

  if((rc = ebml_write_idhdr(mbs, id, length)) < 0)  {
    return rc;
  }
//fprintf(stderr, "write_uint %d, length:%d\n", mbs->bs.idx, length);
  for(idx = 0; idx < length; idx++) {
    buf[idx] = ui64 >> ((length - idx - 1) * 8);
  }

  if(ebml_sync(mbs, buf, length) < 0) {
    return -1;
  }

  return rc + length;
}

int ebml_write_double(EBML_BYTE_STREAM_T *mbs, uint32_t id, double dbl) {
  int rc;
  uint64_t ui64;

  if((rc = ebml_write_idhdr(mbs, id, 8)) < 0)  {
    return rc;
  }

  ui64 = ((uint64_t) htonl(*((uint32_t *) &dbl ) )) << 32;
  ui64 |= ((uint64_t)  htonl( *((uint32_t *) &((unsigned char *) &dbl)[4])));

  if(ebml_sync(mbs, (unsigned char *) &ui64, 8) < 0) {
    return -1;
  }

  return rc + 8;
}

int ebml_write_binary(EBML_BYTE_STREAM_T *mbs, uint32_t id, const void *pval, unsigned int len) {
  int rc;

  if((rc = ebml_write_idhdr(mbs, id, len)) < 0)  {
    return rc;
  }

  if(ebml_sync(mbs, pval, len) < 0) {
    return -1;
  }

  return rc + len;
}

int ebml_write_string(EBML_BYTE_STREAM_T *mbs, uint32_t id, const char *pval) {
  if(!pval) {
    return -1;
  }
  return ebml_write_binary(mbs, id, pval, pval ? strlen(pval) : 0);
}

int ebml_write_void(EBML_BYTE_STREAM_T *mbs, unsigned int len) {
  int rc = 0;
  unsigned int sz;
  unsigned int written = 0;
  unsigned char buf[1024];
 
  if(len <= 2) {
    return -1;
  }

  if((rc = ebml_write_id(mbs, EBML_ID_VOID)) < 0) {
    return rc;
  }

  written = rc;

  if(len >= 10) {
    rc = ebml_write_num(mbs, len - 9, 8);
  } else {
    rc = ebml_write_num(mbs, len - 1, 0);
  }  

  if(rc < 0) {
    return rc;
  }
  written += rc;
  memset(buf, 0, sizeof(buf));

  while(written < len) {
    if((sz = (len - written) > sizeof(buf))) {
      sz = sizeof(buf);
    }
    if(ebml_sync(mbs, buf, sz) < 0) {
      return -1;
    }
    written += sz;
  }

  return written;
}
















